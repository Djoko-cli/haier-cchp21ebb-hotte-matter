// Tests hote du pilote simule (src/produit/pilote_simule.*) : trois modes
// d'etat et lecture annexe, echo et fin d'un appui, reaction de la carte,
// garde-fous 6 et 7, pannes a la demande, marche prolongee, coupure.
// Lancer : sh tools/tests/test_hote.sh
#include <vector>

#include "pilote_simule.h"
#include "verif.h"

using namespace hotte;
using ligne::Evenement;

namespace {

struct Journal {
  std::vector<Evenement> ev;
  void lire(sim::PiloteSimule &p) {
    Evenement e;
    while (p.evenement(&e)) ev.push_back(e);
  }
  size_t nb(Evenement::Type t, Source s = Source::Deduit) const {
    size_t n = 0;
    for (const Evenement &e : ev)
      if (e.type == t && (t != Evenement::EtatLu || s == Source::Deduit || e.etat.source == s)) n++;
    return n;
  }
  // Le dernier evenement de ce type ; un evenement vide (tMs = 0xFFFFFFFF) s'il n'y en a pas.
  const Evenement &dernier(Evenement::Type t) const {
    static Evenement vide;
    vide.tMs = 0xFFFFFFFF;
    for (size_t i = ev.size(); i-- > 0;)
      if (ev[i].type == t) return ev[i];
    return vide;
  }
};

void avancer(sim::PiloteSimule &p, Journal &j, uint32_t &t, uint32_t ms) {
  for (uint32_t e = 0; e < ms; e += 10) {
    t += 10;
    p.poll(t);
    j.lire(p);
  }
}

sim::Reglages reglages(ModeEtat m, bool annexe = false) {
  sim::Reglages r;
  r.mode = m;
  r.annexe = annexe;
  return r;
}

}  // namespace

static void testModes() {
  {  // repete : l'etat toutes les 500 ms, et a chaque changement
    sim::PiloteSimule p(reglages(ModeEtat::Repete));
    Journal j;
    uint32_t t = 0;
    p.demarrer(t);
    avancer(p, j, t, 2000);
    VERIF(j.nb(Evenement::EtatLu, Source::Fil) == 4);
    const Evenement &e = j.dernier(Evenement::EtatLu);
    VERIF(e.etat.marche == Marche::Eteinte && e.etat.lampeConnue && e.etat.source == Source::Fil);
    p.appuiPanneau(Touche::Lumiere, t);
    avancer(p, j, t, 300);
    VERIF(j.nb(Evenement::Appui) == 1 && j.dernier(Evenement::Appui).origine == Origine::Panneau);
    VERIF(p.hotte().lampe && j.dernier(Evenement::EtatLu).etat.lampe);
  }
  {  // changements : rien au repos, l'etat a chaque changement
    sim::PiloteSimule p(reglages(ModeEtat::Changements));
    Journal j;
    uint32_t t = 0;
    p.demarrer(t);
    avancer(p, j, t, 5000);
    VERIF(j.ev.empty());
    p.appuiPanneau(Touche::Marche, t);
    avancer(p, j, t, 5000);
    VERIF(j.nb(Evenement::EtatLu) == 1 && j.dernier(Evenement::EtatLu).etat.marche == Marche::Armee);
    p.appuiPanneau(Touche::V1, t);  // armee -> V1 : un changement
    avancer(p, j, t, 1000);
    p.appuiPanneau(Touche::Marche, t);  // V1 -> prolongee
    avancer(p, j, t, 1000);
    VERIF(j.nb(Evenement::EtatLu) == 3 && p.hotte().marche == Marche::Prolongee);
  }
  {  // appuis seuls, avec lecture annexe : ni etat, des lectures annexes toutes les 200 ms
    sim::PiloteSimule p(reglages(ModeEtat::AppuisSeuls, true));
    Journal j;
    uint32_t t = 0;
    p.demarrer(t);
    p.appuiPanneau(Touche::Marche, t);
    avancer(p, j, t, 1000);
    VERIF(j.nb(Evenement::EtatLu, Source::Fil) == 0 && j.nb(Evenement::EtatLu, Source::Annexe) == 5);
    const Evenement &e = j.dernier(Evenement::EtatLu);
    VERIF(e.etat.marche == Marche::Inconnue && e.etat.moteur == Moteur::Arret && e.etat.lampeConnue);
    VERIF(j.nb(Evenement::Appui) == 1 && p.hotte().marche == Marche::Armee);
  }
}

static void testAppui() {
  sim::PiloteSimule p(reglages(ModeEtat::Changements));
  Journal j;
  uint32_t t = 0;
  p.demarrer(t);
  // Garde-fou 6 : rien lu depuis le demarrage, "marche" refusee.
  VERIF(p.appuyer(Touche::Marche, 1, t) == ligne::Admission::Refusee);
  p.appuiPanneau(Touche::Marche, t);  // armee, un etat est lu
  avancer(p, j, t, 500);
  VERIF(p.appuyer(Touche::V2, 2, t) == ligne::Admission::Acceptee);
  VERIF(p.appuyer(Touche::V3, 3, t) == ligne::Admission::Occupee);  // un appui a la fois
  avancer(p, j, t, 60);  // fin de la trame : echo et fin d'appui
  const Evenement &a = j.dernier(Evenement::Appui);
  const Evenement &f = j.dernier(Evenement::FinAppui);
  VERIF(a.origine == Origine::Module && a.touche == Touche::V2);
  VERIF(f.idAppui == 2 && f.resultat == ResultatAppui::Ok);
  VERIF(p.hotte().moteur == Moteur::Arret);  // latence de la carte : 150 ms
  avancer(p, j, t, 150);
  VERIF(p.hotte().moteur == Moteur::V2 && j.dernier(Evenement::EtatLu).etat.moteur == Moteur::V2);
  // Trames simulees : l'etat armee du panneau, l'echo de V2 (0xA0 + touche), l'etat V2.
  ligne::Trame tr;
  VERIF(p.trame(&tr) && tr.origine == ligne::Trame::Panneau && tr.octets[0] == 0xA0);  // marche du panneau
  VERIF(p.trame(&tr) && tr.origine == ligne::Trame::Carte && tr.octets[0] == 0x10);    // armee
  VERIF(p.trame(&tr) && tr.origine == ligne::Trame::Module && tr.octets[0] == 0xA3);   // V2
  VERIF(p.trame(&tr) && tr.origine == ligne::Trame::Carte && tr.octets[0] == 0x18);    // armee, V2
  VERIF(!p.trame(&tr));
  // Garde-fou 6 : moteur en marche, "marche" refusee. Garde-fou 7 : delai moteur.
  VERIF(p.appuyer(Touche::Marche, 4, t) == ligne::Admission::Refusee);
  VERIF(p.appuyer(Touche::V3, 5, t) == ligne::Admission::Refusee);
  VERIF(p.appuyer(Touche::Lumiere, 6, t) == ligne::Admission::Acceptee);  // la lampe n'a pas de delai
  avancer(p, j, t, 3000);
  VERIF(p.appuyer(Touche::V3, 7, t) == ligne::Admission::Acceptee);
  ligne::Diagnostic d;
  p.diagnostic(&d);
  VERIF(d.refusGarde == 3 && d.appuisEmis == 3);
}

static void testPannes() {
  sim::PiloteSimule p(reglages(ModeEtat::Repete));
  Journal j;
  uint32_t t = 0;
  p.demarrer(t);
  p.appuiPanneau(Touche::Marche, t);
  avancer(p, j, t, 1000);
  // Trame d'etat illisible.
  p.illisibles(1);
  avancer(p, j, t, 500);
  VERIF(j.dernier(Evenement::Anomalie).codeAnomalie == ligne::kAnoIllisible);
  // Sans effet : echo vu, carte sourde.
  p.sansEffet(1);
  j.ev.clear();
  VERIF(p.appuyer(Touche::V1, 1, t) == ligne::Admission::Acceptee);
  avancer(p, j, t, 1000);
  VERIF(j.nb(Evenement::Appui) == 1 && p.hotte().moteur == Moteur::Arret);
  // Perdu : ni echo ni effet, fin Ok.
  p.perdus(1);
  j.ev.clear();
  p.appuyer(Touche::V1, 2, t);
  avancer(p, j, t, 1000);
  VERIF(j.nb(Evenement::Appui) == 0 && j.dernier(Evenement::FinAppui).resultat == ResultatAppui::Ok);
  VERIF(p.hotte().moteur == Moteur::Arret);
  // Collision qui prend : fin Collision, pas d'echo, la carte agit.
  p.collisions(1, true);
  j.ev.clear();
  p.appuyer(Touche::V1, 3, t);
  avancer(p, j, t, 1000);
  VERIF(j.nb(Evenement::Appui) == 0 && j.dernier(Evenement::FinAppui).resultat == ResultatAppui::Collision);
  VERIF(p.hotte().moteur == Moteur::V1);
  avancer(p, j, t, 3000);
  // Collision qui ne prend pas.
  p.collisions(1, false);
  p.appuyer(Touche::V2, 4, t);
  avancer(p, j, t, 1000);
  VERIF(p.hotte().moteur == Moteur::V1);
  // Pas de silence : fin Delai, rien emis.
  p.delais(1);
  j.ev.clear();
  p.appuyer(Touche::Lumiere, 5, t);
  avancer(p, j, t, 100);
  VERIF(j.dernier(Evenement::FinAppui).resultat == ResultatAppui::Delai && j.nb(Evenement::Appui) == 0);
  // Carte qui refuse tout (B3).
  p.refuser(true);
  p.appuyer(Touche::Lumiere, 6, t);
  avancer(p, j, t, 1000);
  VERIF(!p.hotte().lampe);
  p.refuser(false);
  // Hors service : evenement Service, fin Erreur, puis retour.
  j.ev.clear();
  p.horsService(true, t);
  p.appuyer(Touche::Lumiere, 7, t);
  avancer(p, j, t, 100);
  VERIF(!j.dernier(Evenement::Service).enService);
  VERIF(j.dernier(Evenement::FinAppui).resultat == ResultatAppui::Erreur);
  p.horsService(false, t);
  avancer(p, j, t, 10);
  VERIF(j.dernier(Evenement::Service).enService);
}

static void testProlongee() {
  sim::Reglages r = reglages(ModeEtat::Changements);
  sim::PiloteSimule p(r);
  Journal j;
  uint32_t t = 0;
  p.demarrer(t);
  sim::EtatHotte h;
  h.marche = Marche::Prolongee;
  h.moteur = Moteur::V2;
  VERIF(sim::etatHotteValide(h));
  p.poserHotte(h, t);  // sans evenement
  avancer(p, j, t, 100);
  VERIF(j.ev.empty());
  avancer(p, j, t, 900000);
  VERIF(p.hotte().marche == Marche::Eteinte && p.hotte().moteur == Moteur::Arret);  // Q2 : eteinte par defaut
  VERIF(j.nb(Evenement::EtatLu) == 1);
  // Fin en armee ; transitions inconnues directes.
  r.finArmee = true;
  r.inconnues = sim::Inconnues::Direct;
  p.regler(r);
  p.poserHotte(h, t);
  p.appuiPanneau(Touche::V3, t);
  avancer(p, j, t, 500);
  VERIF(p.hotte().marche == Marche::Prolongee && p.hotte().moteur == Moteur::V3);
  avancer(p, j, t, 900000);
  VERIF(p.hotte().marche == Marche::Armee && p.hotte().moteur == Moteur::Arret);
  r.inconnues = sim::Inconnues::Rien;
  p.regler(r);
  p.poserHotte(h, t);
  p.appuiPanneau(Touche::Marche, t);
  avancer(p, j, t, 500);
  VERIF(p.hotte().marche == Marche::Prolongee);  // la carte ne fait rien
  // Coupure : eteinte, lampe eteinte.
  p.appuiPanneau(Touche::Lumiere, t);
  avancer(p, j, t, 500);
  p.coupure(t);
  VERIF(p.hotte().marche == Marche::Eteinte && !p.hotte().lampe);
  sim::EtatHotte faux;
  faux.marche = Marche::Eteinte;
  faux.moteur = Moteur::V1;
  VERIF(!sim::etatHotteValide(faux));
  VERIF_EGAL_STR(sim::texte(sim::Inconnues::Direct), "direct");
}

static void testReglages() {
  sim::Reglages r;
  VERIF(sim::reglagesValides(r));
  r.periodeMs = 99;
  VERIF(!sim::reglagesValides(r));
  r = sim::Reglages();
  r.mode = (ModeEtat)3;
  VERIF(!sim::reglagesValides(r));
  r = sim::Reglages();
  r.prolongeeMs = 3600001;
  VERIF(!sim::reglagesValides(r));
  sim::EtatHotte e;
  e.marche = Marche::Prolongee;
  e.moteur = Moteur::V3;
  e.lampe = true;
  sim::EtatHotte lu;
  VERIF(sim::depuisMot(sim::versMot(e), &lu) && lu.marche == e.marche && lu.moteur == e.moteur && lu.lampe);
  VERIF(!sim::depuisMot(0, &lu) && !sim::depuisMot(0x5304, &lu));  // eteinte moteur V1 : invalide
}

int main() {
  testModes();
  testAppui();
  testPannes();
  testProlongee();
  testReglages();
  return bilan("test_pilote_simule");
}
