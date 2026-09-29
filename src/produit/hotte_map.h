#pragma once
// ===========================================================================
//  Correspondance Matter et boite d'intentions (docs/SPEC-PRODUIT.md 3.2,
//  3.3, 4.4 ; 5.4, regle 6) : pur
//
//  - palier, pourcent, fanMode : l'etat de la hotte vu par Maison (armee
//    compte comme eteinte), et les ecritures de Maison vues comme des cibles ;
//  - RegroupementEp1 : FanMode et PercentSetting dans une seule fenetre ;
//    RegroupementEp2 : OnOff de la lampe, sa propre fenetre ;
//  - ignoreeAuDemarrage : ordres Matter ignores juste apres le demarrage ;
//  - EcritureDifferee : les ecritures NVS 10 s apres le dernier changement ;
//  - versNvs, depuisNvs : l'etat publie en NVS (etat_pub).
//
//  matter_hotte.cpp s'en sert sous le verrou de la boite (les rappels de la
//  tache CHIP y deposent, la tache loop vide). Teste sur l'hote
//  (tools/tests/test_hotte_map.cpp).
// ===========================================================================
#include <stdint.h>

#include "hotte_etat.h"

namespace hotte {

// FanMode (cluster FanControl), valeurs brutes de la specification Matter.
constexpr uint8_t kFanOff = 0, kFanLow = 1, kFanMedium = 2, kFanHigh = 3, kFanOn = 4, kFanAuto = 5, kFanSmart = 6;

Moteur palier(uint8_t pourcent);  // 0 : Arret ; 1..33 : V1 ; 34..66 : V2 ; 67..100 (et au-dela) : V3
uint8_t pourcent(Moteur m);       // Arret 0 ; V1 33 ; V2 66 ; V3 100
uint8_t fanMode(Moteur m);        // Arret Off ; V1 Low ; V2 Medium ; V3 High
CibleVentilo cibleDe(Moteur m);   // Arret : Eteint ; Vx : Vx
// Off : Eteint ; Low, Medium, High : V1, V2, V3 ; On : Derniere ; Auto, Smart : Aucune.
CibleVentilo cibleDeMode(uint8_t fanMode);

// Ce que le noeud publie pour un etat (3.2) : l'arret publie toujours
// FanMode Off et 0 % ; un palier, sa valeur canonique.
struct Reflet {
  uint8_t fanMode;
  uint8_t pourcent;  // PercentSetting et PercentCurrent
  bool lampe;
};
Reflet reflet(const Etat &e);

// Regroupement d'EP1 (5.4, regle 6). A la fermeture (calmeMs sans nouvelle
// ecriture, ou plafondMs apres la premiere), la cible vient de la derniere
// ecriture de la fenetre, dans l'ordre d'arrivee ; un FanMode Low, Medium,
// High ou On cede devant un PercentSetting > 0 de la meme fenetre, dont on
// prend la derniere valeur.
class RegroupementEp1 {
 public:
  void ecrireMode(uint8_t fanMode, uint32_t nowMs);
  void ecrirePourcent(uint8_t pourcent, uint32_t nowMs);
  bool ouvert() const { return ouvert_; }
  // Vrai a la fermeture, qui vide la fenetre : *cible (Aucune si rien
  // d'utile : Auto, Smart), *premierMs (premiere ecriture de la fenetre).
  bool pret(uint32_t nowMs, uint32_t calmeMs, uint32_t plafondMs, CibleVentilo *cible, uint32_t *premierMs = nullptr);

 private:
  void noter(uint32_t nowMs);
  bool ouvert_ = false, aMode_ = false, aPourcent_ = false;
  uint8_t mode_ = 0, pourcent_ = 0;
  uint32_t seq_ = 0, seqMode_ = 0, seqPourcent_ = 0;
  uint32_t premierMs_ = 0, dernierMs_ = 0;
};

// Regroupement d'EP2 : la derniere valeur de la fenetre gagne.
class RegroupementEp2 {
 public:
  void ecrire(bool allumee, uint32_t nowMs);
  bool ouvert() const { return ouvert_; }
  bool pret(uint32_t nowMs, uint32_t calmeMs, uint32_t plafondMs, bool *allumee, uint32_t *premierMs = nullptr);

 private:
  bool ouvert_ = false, valeur_ = false;
  uint32_t premierMs_ = 0, dernierMs_ = 0;
};

// Garde-fou de demarrage (4.4, repris de benq) : une fenetre dont la
// premiere ecriture tombe moins de ignoreMs apres le demarrage est ignoree.
// Difference signee : une ecriture deposee pendant Matter.begin() precede
// l'heure du demarrage.
bool ignoreeAuDemarrage(uint32_t premierMs, uint32_t demarrageMs, uint32_t ignoreMs);

// Ecriture NVS differee (4.4) : delaiMs apres le dernier changement, et
// seulement si la valeur differe de celle ecrite. 'reboot' et
// 'decommission' ecrivent d'abord ce qui attend (enAttente, valeur).
class EcritureDifferee {
 public:
  explicit EcritureDifferee(uint32_t delaiMs = 10000) : delaiMs_(delaiMs) {}
  void ecrite(uint32_t valeur);  // relue au demarrage, ou ecrite avec succes
  void noter(uint32_t valeur, uint32_t nowMs);
  bool echue(uint32_t nowMs) const { return attente_ && nowMs - depuisMs_ >= delaiMs_; }
  bool enAttente() const { return attente_; }
  uint32_t valeur() const { return valeur_; }

 private:
  uint32_t delaiMs_;
  bool connue_ = false, attente_ = false;
  uint32_t ecrite_ = 0, valeur_ = 0, depuisMs_ = 0;
};

// Etat publie compacte pour la NVS (etat_pub) : marche, moteur, lampe et
// lampeConnue, sous une marque. depuisNvs rend faux pour toute valeur mal
// formee (la NVS n'a rien d'utile).
uint16_t versNvs(const Etat &e);
bool depuisNvs(uint16_t v, Etat *out);

}  // namespace hotte
