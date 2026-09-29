#pragma once
// ===========================================================================
//  Interface du pilote de ligne (docs/SPEC-PRODUIT.md 4.3) : pur
//
//  Le pilote traduit l'ecoute de D (et la lecture annexe) en evenements, et
//  un appui demande en trame (A, B) ou en contact (C). Il ne decide rien : ni
//  cible, ni enchainement de touches. Il porte les garde-fous electriques et
//  temporels (4.3), dont deux verrous de la regle d'or : il refuse "marche"
//  si son dernier etat lu dit moteur en marche ou s'il n'a rien lu, et tout
//  appui qui change le moteur avant delai_moteur_ms.
//
//  Realisations : PiloteSimule (pilote_simule.h), maintenant ; A, B ou C
//  apres l'avenant de l'etape 6.
// ===========================================================================
#include <stdint.h>

#include "hotte_etat.h"

namespace ligne {

using hotte::ResultatAppui;

// Codes d'anomalie de reception.
constexpr uint16_t kAnoIllisible = 1;    // trame illisible
constexpr uint16_t kAnoDebordement = 2;  // debordement de la reception
constexpr uint16_t kAnoTenueBasse = 3;   // ligne tenue basse

struct Evenement {
  enum Type : uint8_t { EtatLu, Appui, FinAppui, Anomalie, Service };
  Type type = EtatLu;
  uint32_t tMs = 0;
  hotte::Etat etat;                                   // EtatLu : trame de la carte (source Fil) ou lecture annexe
  hotte::Touche touche = hotte::Touche::Marche;       // Appui : touche vue sur le fil
  hotte::Origine origine = hotte::Origine::Inconnue;  // Appui : Panneau, ou Module (echo de notre trame)
  uint32_t idAppui = 0;                               // FinAppui
  ResultatAppui resultat = ResultatAppui::Ok;         // FinAppui
  uint16_t codeAnomalie = 0;                          // Anomalie
  bool enService = true;                              // Service : pilote hors service, puis revenu
};

// Trame decodee du fil, pour le diagnostic ('json trames 1', evenement
// trame_d) : origine carte, panneau ou module, 1 a 8 octets.
struct Trame {
  enum Origine : uint8_t { Carte, Panneau, Module };
  uint32_t tMs = 0;
  Origine origine = Carte;
  uint8_t n = 0;
  uint8_t octets[8] = {};
};

enum class Admission : uint8_t { Acceptee, Occupee, Refusee };

struct Diagnostic {
  uint32_t appuisEmis = 0, refusGarde = 0, collisions = 0, delais = 0;
  uint32_t etatsLus = 0, lecturesAnnexe = 0, anomalies = 0, appuisVus = 0, evenementsPerdus = 0;
};

// Admission refusee sans rien emettre, rendue a l'automate comme une fin
// d'appui : Occupee (rien emis) comme Delai, Refusee comme Garde.
inline ResultatAppui resultatAdmission(Admission a) {
  return a == Admission::Occupee ? ResultatAppui::Delai : ResultatAppui::Garde;
}

class Pilote {
 public:
  virtual ~Pilote() = default;
  virtual void demarrer(uint32_t nowMs) = 0;       // ecoute seulement
  virtual void poll(uint32_t nowMs) = 0;           // tache loop
  virtual bool evenement(Evenement *out) = 0;      // file, videe a chaque tour
  // Asynchrone : la fin arrive en FinAppui. Occupee : un appui est deja en
  // cours ; Refusee : un garde-fou l'interdit (l'automate en fait un Garde).
  virtual Admission appuyer(hotte::Touche t, uint32_t idAppui, uint32_t nowMs) = 0;
  virtual hotte::ModeEtat mode() const = 0;        // ce que porte D
  virtual bool annexe() const = 0;                 // lecture annexe en service
  virtual void diagnostic(Diagnostic *out) const = 0;
  // Trames decodees, bornees (les plus anciennes cedent) ; videe a chaque tour.
  virtual bool trame(Trame *out) = 0;
};

}  // namespace ligne
