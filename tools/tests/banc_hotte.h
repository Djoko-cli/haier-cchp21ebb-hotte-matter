#pragma once
// Banc des tests de l'automate : l'automate (hotte_etat) relie au pilote
// simule (pilote_simule), comme dans loop() de main_produit.cpp ; le temps
// avance par pas de 10 ms. A chaque appui "marche" injecte, le banc verifie
// la regle d'or avec sa propre tenue des evenements (independante de
// l'automate) : un etat lu (fil ou annexe) moteur arrete depuis le dernier
// appui, et le moteur reel de la hotte simulee arrete.
#include <vector>

#include "hotte_etat.h"
#include "pilote_simule.h"

struct BancHotte {
  hotte::Automate a;
  sim::PiloteSimule p;
  uint32_t t = 1000;
  std::vector<hotte::Action> fins, publis, changements, evts;
  std::vector<hotte::Touche> appuis;  // touches injectees, dans l'ordre
  std::vector<uint32_t> appuisT;      // et leur heure
  bool arretLuDepuisAppui = false;
  int violations = 0;   // "marche" sans arret lu depuis le dernier appui, ou moteur reel tournant
  int refusPilote = 0;  // garde-fou du pilote declenche (Refusee)

  explicit BancHotte(hotte::ModeEtat mode, bool annexe = false, const hotte::Params &pa = hotte::Params()) : a(pa) {
    sim::Reglages r;
    r.mode = mode;
    r.annexe = annexe;
    r.delaiMoteurMs = pa.delaiMoteurMs;
    p.regler(r);
    a.configurerLigne(mode, annexe);
  }

  void demarrer(hotte::Demarrage c = hotte::Demarrage::MiseSousTension, const hotte::Etat *nvs = nullptr,
                hotte::Moteur derniere = hotte::Moteur::V2) {
    p.demarrer(t);
    a.demarrer(c, nvs, derniere, t);
    arretLuDepuisAppui = false;
    vider();
  }

  void vider() {
    ligne::Evenement ev;
    while (p.evenement(&ev)) {
      switch (ev.type) {
        case ligne::Evenement::EtatLu:
          arretLuDepuisAppui = ev.etat.moteur == hotte::Moteur::Arret;
          a.surEtatLu(ev.etat, t);
          break;
        case ligne::Evenement::Appui:
          arretLuDepuisAppui = false;
          a.surAppui(ev.touche, ev.origine, t);
          break;
        case ligne::Evenement::FinAppui: a.surFinAppui(ev.idAppui, ev.resultat, t); break;
        case ligne::Evenement::Anomalie: a.surAnomalie(ev.codeAnomalie, t); break;
        case ligne::Evenement::Service: a.surPilote(ev.enService, t); break;
      }
    }
    for (;;) {
      const hotte::Action x = a.suivante(t);
      if (x.type == hotte::Action::Aucune) break;
      switch (x.type) {
        case hotte::Action::Appuyer: {
          if (x.touche == hotte::Touche::Marche &&
              (!arretLuDepuisAppui || p.hotte().moteur != hotte::Moteur::Arret))
            violations++;
          appuis.push_back(x.touche);
          appuisT.push_back(t);
          arretLuDepuisAppui = false;
          const ligne::Admission ad = p.appuyer(x.touche, x.idAppui, t);
          if (ad != ligne::Admission::Acceptee) {
            if (ad == ligne::Admission::Refusee) refusPilote++;
            a.surFinAppui(x.idAppui, ligne::resultatAdmission(ad), t);
          }
          break;
        }
        case hotte::Action::FinSequence: fins.push_back(x); break;
        case hotte::Action::Publier: publis.push_back(x); break;
        case hotte::Action::Changement: changements.push_back(x); break;
        case hotte::Action::Evenement: evts.push_back(x); break;
        default: break;
      }
    }
  }

  void pas(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i += 10) {
      t += 10;
      p.poll(t);
      vider();
    }
  }

  // Avance jusqu'a la fin de sequence suivante (au plus maxMs) ; nullptr sinon.
  const hotte::Action *attendreFin(uint32_t maxMs = 30000) {
    const size_t n = fins.size();
    for (uint32_t e = 0; e < maxMs && fins.size() == n; e += 10) pas(10);
    return fins.size() > n ? &fins[n] : nullptr;
  }

  void panneau(hotte::Touche tc) {
    p.appuiPanneau(tc, t);
    pas(10);
  }

  // Mene la hotte a un etat par des appuis du panneau (depuis eteinte, lampe
  // eteinte), en laissant passer le delai moteur entre deux changements.
  void mener(hotte::Marche m, hotte::Moteur mo, bool lampe = false) {
    if (m != hotte::Marche::Eteinte) {
      panneau(hotte::Touche::Marche);
      pas(1000);
    }
    if (mo != hotte::Moteur::Arret) {
      pas(3000);
      panneau(hotte::toucheDe(mo));
      pas(1000);
    }
    if (m == hotte::Marche::Prolongee) {
      panneau(hotte::Touche::Marche);
      pas(1000);
    }
    if (lampe) {
      panneau(hotte::Touche::Lumiere);
      pas(1000);
    }
    pas(3500);  // delai moteur ecoule depuis le dernier changement
  }

  bool hotteEst(hotte::Marche m, hotte::Moteur mo) const {
    return p.hotte().marche == m && p.hotte().moteur == mo;
  }

  size_t nbAppuis(hotte::Touche tc) const {
    size_t n = 0;
    for (hotte::Touche x : appuis) n += x == tc;
    return n;
  }
};
