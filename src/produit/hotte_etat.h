#pragma once
// ===========================================================================
//  Automate de la hotte (docs/SPEC-PRODUIT.md, 4.2 et 5) : logique pure
//
//  - Etat de la hotte et sa confiance (5.1), table des transitions (5.2) ;
//  - Params : reglages de l'automate, bornes appliquees par 'hotte regle' et
//    au chargement de la NVS (comme inj::Params de la sonde) ;
//  - Automate : ordres (Maison, app) et evenements du pilote de ligne (etat
//    lu, appui vu sur le fil, fin d'un appui injecte, anomalie, pilote hors
//    service) en entree ; une Action a la fois en sortie (suivante) : appuyer,
//    publier, fin de sequence, changement d'etat, evenement.
//
//  Regle d'or (5.4, regle 3) : jamais "marche" sans un arret du moteur LU
//  (fil ou lecture annexe) posterieur au demarrage et au dernier appui.
//
//  Ni Arduino, ni pile Matter, ni horloge : le temps est passe en argument
//  (millis(), differences non signees). Teste sur l'hote
//  (tools/tests/test_hotte_etat.cpp, tools/tests/test_sequences.cpp).
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

namespace hotte {

enum class Touche : uint8_t { Marche, Lumiere, V1, V2, V3 };
enum class Moteur : uint8_t { Arret, V1, V2, V3 };
// Voyant marche : eteint, fixe (armee, moteur arrete ou en Vx), clignotant
// (marche prolongee, moteur en Vx), ou inconnu (la lecture annexe ne le lit pas).
enum class Marche : uint8_t { Eteinte, Armee, Prolongee, Inconnue };
enum class Confiance : uint8_t { Confirme, Deduit, Presume, Inconnu };
enum class Source : uint8_t { Fil, Annexe, Deduit, Nvs };
enum class Origine : uint8_t { Panneau, Module, Inconnue };
enum class Demarrage : uint8_t { MiseSousTension, Autre };
// Ce que porte la ligne D (etape 5, ajout c ; 5.4) : l'etat repete au repos,
// l'etat aux changements seulement, ou les appuis seulement.
enum class ModeEtat : uint8_t { Repete, Changements, AppuisSeuls };
// Resultat d'un appui injecte, rendu par le pilote (4.3).
enum class ResultatAppui : uint8_t { Ok, Collision, Delai, Garde, Erreur };

struct Etat {
  Marche marche = Marche::Inconnue;
  Moteur moteur = Moteur::Arret;
  bool lampe = false;
  bool lampeConnue = false;
  Confiance confiance = Confiance::Inconnu;
  Source source = Source::Deduit;
  uint32_t depuisMs = 0;  // heure du dernier etat lu ou deduit
  uint32_t luMs = 0;      // heure du dernier etat LU (fil ou annexe) ; 0 : aucun depuis le demarrage
};
// Ce que voient Maison et l'app : marche, moteur, lampe et lampeConnue.
bool memeEtat(const Etat &a, const Etat &b);
// Etat de depart d'une mise sous tension (5.7) : eteinte, lampe eteinte, Deduit.
Etat etatMiseSousTension(uint32_t nowMs);

// Table du 5.2. observee = faux : transition jamais observee (resultat
// inconnu : la confiance passera a Presume). Seuls marche, moteur, lampe et
// lampeConnue changent ; les autres champs sont copies.
struct Transition {
  Etat apres;
  bool observee;
};
Transition appliquer(const Etat &avant, Touche t);
Moteur moteurDe(Touche t);  // V1..V3 -> V1..V3 ; Marche, Lumiere -> Arret
Touche toucheDe(Moteur m);  // V1..V3 -> V1..V3 ; Arret -> Marche (sans objet)

// Textes du protocole et de la console (sans accents).
const char *texte(Touche t);     // marche, lumiere, v1, v2, v3
const char *texte(Moteur m);     // arret, v1, v2, v3
const char *texte(Marche m);     // eteinte, armee, prolongee, inconnue
const char *texte(Confiance c);  // confirme, deduit, presume, inconnu
const char *texte(Source s);     // fil, annexe, deduit, nvs
const char *texte(Origine o);    // panneau, module, inconnue
const char *texte(ModeEtat m);   // repete, changements, appuis
const char *texte(ResultatAppui r);  // ok, collision, delai, garde, erreur

// ---------------------------------------------------------------------------
//  Reglages (4.2) : 'hotte regle <nom> <valeur>', USB seulement
// ---------------------------------------------------------------------------

struct Params {
  uint32_t delaiMoteurMs = 3000, entreAppuisMs = 500, confirmationMs = 1000, fraicheurMs = 2000;
  uint32_t lissageCalmeMs = 700, lissagePlafondMs = 3000, ordreCalmeMs = 150;
  uint32_t sequenceMaxMs = 15000, attenteEtatMs = 5000, prolongeeMaxMs = 1200000;
  uint32_t ignoreDemarrageMs = 2000;
  uint32_t sondeVitesse = 0;  // 1 : sonde de vitesse permise (5.4, regle 7 ; Q6)
};
// Chaque valeur dans ses bornes, et lissage_calme_ms <= lissage_plafond_ms.
bool paramsValides(const Params &p);
constexpr uint8_t kNbParams = 12;
const char *nomParam(uint8_t i);  // i >= kNbParams : nullptr
uint32_t valeurParam(const Params &p, uint8_t i);
bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi);  // false : nom inconnu
enum class Reglage : uint8_t { Ok, NomInconnu, HorsBornes };
Reglage reglerParam(Params *p, const char *nom, uint32_t v);  // *p inchange si refus
// Image lue en NVS : acceptee si elle a la taille de Params et des valeurs
// valides (true) ; sinon *out recoit les valeurs par defaut, toutes.
bool chargerParams(const void *octets, size_t n, Params *out);

}  // namespace hotte
