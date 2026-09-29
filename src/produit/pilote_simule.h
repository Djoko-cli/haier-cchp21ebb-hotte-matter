#pragma once
// ===========================================================================
//  Pilote de ligne simule (docs/SPEC-PRODUIT.md 4.3) : pur
//
//  Une hotte modele derriere l'interface ligne::Pilote. Elle suit la table du
//  5.2 : latence de la carte, trois modes d'etat (etat repete au repos, etat
//  aux changements, appuis seuls), lecture annexe (moteur et lampe), marche
//  prolongee de 15 min, transitions inconnues reglables. Pannes a la demande :
//  trame d'etat illisible, appui sans effet, appui perdu sur le fil,
//  collision, pas de silence, carte qui refuse tout (B3), pilote hors
//  service. Il porte les garde-fous 6 et 7 du pilote reel.
//
//  Aucune broche : le build simule ne lie pas le code d'injection. Sur la
//  carte, main_produit.cpp garde l'etat de la hotte simulee a travers un
//  redemarrage (RTC_NOINIT) et la commande par la console ('simu ...').
//  Teste sur l'hote (tools/tests/test_pilote_simule.cpp).
// ===========================================================================
#include <stdint.h>

#include "hotte_etat.h"
#include "pilote_ligne.h"

namespace sim {

// Transitions de la prolongation jamais observees (Q2) : la carte ne fait
// rien, ou elle agit directement (marche : eteinte ; autre vitesse : Vy).
enum class Inconnues : uint8_t { Rien, Direct };

struct Reglages {
  hotte::ModeEtat mode = hotte::ModeEtat::Repete;
  bool annexe = false;
  uint32_t latenceMs = 150;       // reaction de la carte a une trame d'appui
  uint32_t periodeMs = 500;       // mode repete : periode de l'etat au repos
  uint32_t trameMs = 60;          // duree d'une trame d'appui sur le fil
  uint32_t annexeMs = 200;        // periode de la lecture annexe
  uint32_t prolongeeMs = 900000;  // duree de la marche prolongee (15 min, etape -1)
  bool finArmee = false;          // fin de la prolongation : armee au lieu d'eteinte (Q2)
  Inconnues inconnues = Inconnues::Rien;
  uint32_t delaiMoteurMs = 3000;  // garde-fou 7 : celui de l'automate
};

// Bornes ('simu ...', et image lue en NVS) : latence 0..5000 ms, periode
// 100..60000 ms, trame 10..1000 ms, annexe 50..5000 ms, prolongee
// 60000..3600000 ms, delai moteur 1000..60000 ms, modes et choix connus.
bool reglagesValides(const Reglages &r);

struct EtatHotte {
  hotte::Marche marche = hotte::Marche::Eteinte;
  hotte::Moteur moteur = hotte::Moteur::Arret;
  bool lampe = false;
};
bool etatHotteValide(const EtatHotte &e);  // marche connue, jamais eteinte moteur tournant
const char *texte(Inconnues i);            // rien, direct
// Etat de la hotte simulee compacte pour la NVS (simu_dem) et la memoire RTC.
uint16_t versMot(const EtatHotte &e);
bool depuisMot(uint16_t v, EtatHotte *out);  // faux si mal forme ou invalide

class PiloteSimule : public ligne::Pilote {
 public:
  explicit PiloteSimule(const Reglages &r = Reglages());
  void regler(const Reglages &r);
  const Reglages &reglages() const { return r_; }
  // Etat reel de la hotte simulee. poserHotte : sans evenement ni trame
  // (demarrage, 'simu apres_demarrage'). hotte() : ce que montre le panneau.
  void poserHotte(const EtatHotte &e, uint32_t nowMs);
  EtatHotte hotte() const { return h_; }

  // ligne::Pilote
  void demarrer(uint32_t nowMs) override;
  void poll(uint32_t nowMs) override;
  bool evenement(ligne::Evenement *out) override;
  ligne::Admission appuyer(hotte::Touche t, uint32_t idAppui, uint32_t nowMs) override;
  hotte::ModeEtat mode() const override { return r_.mode; }
  bool annexe() const override { return r_.annexe; }
  void diagnostic(ligne::Diagnostic *out) const override { *out = d_; }
  // Trames simulees : un appui, 0xA0 + touche ; un etat de la carte,
  // marche << 4 | moteur << 2 | lampe (codage du banc, pas celui de la carte).
  bool trame(ligne::Trame *out) override;

  // Commandes du banc et des tests ('simu ...').
  void appuiPanneau(hotte::Touche t, uint32_t nowMs);
  void coupure(uint32_t nowMs);  // coupure secteur de la hotte seule : eteinte, lampe eteinte
  void illisibles(uint8_t n) { illisibles_ = n; }  // prochaines trames d'etat de la carte
  void sansEffet(uint8_t n) { sansEffet_ = n; }    // prochains appuis du module : echo vu, carte sourde
  void perdus(uint8_t n) { perdus_ = n; }          // ... : ni echo ni effet
  void collisions(uint8_t n, bool prend) {         // ... : collision ; prend : la carte l'a recu
    collisions_ = n;
    collisionPrend_ = prend;
  }
  void delais(uint8_t n) { delais_ = n; }  // ... : pas de silence, rien emis
  void refuser(bool on) { refuse_ = on; }  // la carte ignore tout appui du module (B3)
  bool refuse() const { return refuse_; }
  void horsService(bool on, uint32_t nowMs);
  bool horsService() const { return hs_; }

 private:
  enum : uint8_t { kEcho, kFin, kCarte };
  struct Tache {
    bool actif = false;
    uint8_t type = kEcho;
    uint32_t t = 0;
    hotte::Touche touche = hotte::Touche::Marche;
    hotte::Origine origine = hotte::Origine::Inconnue;
    uint32_t id = 0;
    hotte::ResultatAppui res = hotte::ResultatAppui::Ok;
  };
  void planifier(const Tache &t);
  void pousser(const ligne::Evenement &e);
  void pousserTrame(ligne::Trame::Origine o, uint8_t octet, uint32_t nowMs);
  void carte(hotte::Touche t, uint32_t nowMs);  // la carte recoit une trame d'appui
  void changement(uint32_t nowMs);              // l'etat de la carte vient de changer
  void emettreEtat(uint32_t nowMs);
  void emettreAnnexe(uint32_t nowMs);
  void noterLu(const hotte::Etat &e, uint32_t nowMs);
  bool changeMoteur(hotte::Touche t) const;

  Reglages r_;
  EtatHotte h_;
  uint32_t debutProlMs_ = 0;
  uint32_t dernierEtatMs_ = 0, derniereAnnexeMs_ = 0;
  bool enCours_ = false;
  bool luUnEtat_ = false;  // un etat lu depuis demarrer() (garde-fou 6)
  hotte::Etat lu_;
  bool changementVu_ = false;    // un changement du moteur vu depuis demarrer()
  uint32_t changementLuMs_ = 0;  // et son heure (garde-fou 7)
  uint8_t illisibles_ = 0, sansEffet_ = 0, perdus_ = 0, collisions_ = 0, delais_ = 0;
  bool collisionPrend_ = false, refuse_ = false, hs_ = false;
  static constexpr uint8_t kTaches = 16;
  Tache taches_[kTaches];
  static constexpr uint8_t kFile = 32;
  ligne::Evenement file_[kFile];
  uint8_t tete_ = 0, nb_ = 0;
  ligne::Diagnostic d_;
  static constexpr uint8_t kTrames = 16;
  ligne::Trame trames_[kTrames];
  uint8_t teteT_ = 0, nbT_ = 0;
};

}  // namespace sim
