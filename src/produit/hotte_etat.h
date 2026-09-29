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

// ---------------------------------------------------------------------------
//  Ordres, issues, actions
// ---------------------------------------------------------------------------

enum class CibleVentilo : uint8_t { Aucune, Eteint, V1, V2, V3, Derniere };
enum class Issue : uint8_t { Ok, Annulee, Echec, Abandon, Remplacee };
enum class Cause : uint8_t { Aucune, NonConfirme, SansLecture, Collision, Garde, Pilote, Duree };
constexpr uint8_t kNbCauses = 7;
enum class Canal : uint8_t { Matter, App };  // d'ou vient un ordre (evenement sequence)
enum class Sujet : uint8_t { Ventilo, Lampe };
enum class Evt : uint8_t { MarcheInconnue, ProlongeePerimee, LigneMuette, DeductionDementie, SondeVitesse };
const char *texte(CibleVentilo c);  // aucune, eteint, v1, v2, v3, derniere
const char *texte(Issue i);         // ok, annulee, echec, abandon, remplacee
const char *texte(Cause c);         // aucune, non_confirme, sans_lecture, collision, garde, pilote, duree
const char *texte(Canal c);         // matter, app
const char *texte(Sujet s);         // ventilo, lampe
const char *texte(Evt e);           // marche_inconnue, prolongee_perimee, ligne_muette, deduction_dementie, sonde_vitesse

struct Action {
  enum Type : uint8_t { Aucune, Appuyer, Publier, FinSequence, Changement, Evenement };
  Type type = Aucune;
  // Appuyer
  Touche touche = Touche::Marche;
  uint32_t idAppui = 0;
  // Publier : etat a publier. Changement : etat apres le changement.
  Etat etat;
  // Changement
  Etat avant;
  Origine origine = Origine::Inconnue;
  // FinSequence
  Sujet sujet = Sujet::Ventilo;
  uint32_t idOrdre = 0;
  Canal canal = Canal::Matter;
  Issue issue = Issue::Ok;
  Cause cause = Cause::Aucune;
  uint32_t dureeMs = 0;
  uint8_t appuis = 0;
  // Evenement
  Evt evt = Evt::MarcheInconnue;
};

struct Compteurs {
  uint32_t appuisPanneau = 0, appuisModule = 0;
  uint32_t reussies = 0, annulees = 0, remplacees = 0, abandons = 0;
  uint32_t echecs[kNbCauses] = {};  // par Cause ; [0] inutilise
  uint32_t nouveauxEssais = 0, transitionsInconnues = 0, anomalies = 0;
  uint32_t marcheInconnue = 0, prolongeePerimee = 0, lignesMuettes = 0, actionsPerdues = 0;
};

struct Diagnostic {
  Compteurs c;
  bool ventilo = false;  // ordre du ventilateur en cours
  bool cibleEteint = false;
  Moteur cibleMoteur = Moteur::Arret;
  uint32_t idVentilo = 0;
  Canal canalVentilo = Canal::Matter;
  bool lampe = false;  // ordre de la lampe en cours
  bool cibleLampe = false;
  uint32_t idLampe = 0;
  Canal canalLampe = Canal::Matter;
  bool enVol = false;  // appui injecte en cours (jusqu'a son verdict)
  Touche touche = Touche::Marche;
  uint8_t essai = 0;
  uint32_t volDepuisMs = 0;
  uint32_t delaiMoteurResteMs = 0;
  bool piloteEnService = true;
  bool marcheAutorisee = false;  // arret du moteur lu depuis le dernier appui (regle 3)
};

// ---------------------------------------------------------------------------
//  Automate
// ---------------------------------------------------------------------------

class Automate {
 public:
  explicit Automate(const Params &p = Params());
  void configurer(const Params &p);  // p deja valide (paramsValides)
  // Ce que porte la ligne, donne par le pilote (reel : l'avenant ; simule : 'simu mode').
  void configurerLigne(ModeEtat mode, bool annexe);
  // Etat initial (5.7). etatPublieNvs : dernier etat publie, relu en NVS
  // (cause Autre seulement ; nullptr s'il n'y en a pas). derniereVitesse :
  // NVS derniere_v (V1..V3 ; Arret : V2).
  void demarrer(Demarrage cause, const Etat *etatPublieNvs, Moteur derniereVitesse, uint32_t nowMs);

  // Entrees
  void ordreVentilo(CibleVentilo c, uint32_t idOrdre, Canal canal, uint32_t nowMs);
  void ordreLampe(bool allumee, uint32_t idOrdre, Canal canal, uint32_t nowMs);
  void surEtatLu(const Etat &lu, uint32_t nowMs);          // fil (complet) ou lecture annexe (moteur, lampe)
  void surAppui(Touche t, Origine o, uint32_t nowMs);      // appui vu sur le fil
  void surFinAppui(uint32_t idAppui, ResultatAppui r, uint32_t nowMs);
  void surAnomalie(uint16_t code, uint32_t nowMs);         // trame illisible, debordement, ligne tenue basse
  void surPilote(bool enService, uint32_t nowMs);

  // Sortie : une action par appel ; Action::Aucune quand il n'y a plus rien.
  Action suivante(uint32_t nowMs);

  const Etat &etat() const { return etat_; }
  Moteur derniereVitesse() const { return derniere_; }
  Diagnostic diagnostic(uint32_t nowMs) const;
  const Params &params() const { return p_; }
  ModeEtat mode() const { return mode_; }
  bool annexe() const { return annexe_; }

 private:
  struct Ordre {
    bool actif = false;
    uint32_t id = 0;
    Canal canal = Canal::Matter;
    uint32_t recuMs = 0;
    uint8_t appuis = 0;
    uint32_t bloqueMs = 0;  // debut de l'attente d'un etat utilisable (0 : pas bloque)
  };
  struct Vol {
    bool actif = false;
    bool orphelin = false;  // son ordre est parti : l'appui ne sert plus qu'a lire l'etat
    bool sonde = false;     // sonde de vitesse (regle 7)
    Sujet sujet = Sujet::Ventilo;
    Touche touche = Touche::Marche;
    uint32_t id = 0;
    Etat avant, attendu;
    uint8_t essai = 0;
    uint32_t debutMs = 0;
    bool fini = false;
    ResultatAppui res = ResultatAppui::Ok;
    uint32_t finMs = 0;
    bool echo = false;
    bool anomalie = false;
    uint32_t anomalieMs = 0;
    bool filLu = false;  // dernier etat complet lu apres la fin de l'appui
    Etat fil;
    uint32_t filMs = 0;
    bool annexeLue = false;  // derniere lecture annexe apres la fin de l'appui
    Etat annexeEtat;
    uint32_t annexeMs = 0;
  };
  struct Reprise {  // nouvel essai autorise pour ce sujet (regle 2)
    bool actif = false;
    Touche touche = Touche::Marche;
  };
  enum class Plan : uint8_t { Rien, Atteint, Presser, Attente, Bloque };
  enum class Verdict : uint8_t { Attendre, Reussi, NonPris, Autre, Echec };

  void pousser(const Action &a);
  void pousserPublier();
  void changer(const Etat &nouveau, Origine o, uint32_t nowMs);
  void finOrdre(Sujet s, Issue i, Cause c, uint32_t nowMs);
  void annulerTout(uint32_t nowMs);
  void verifierTemps(uint32_t nowMs);
  Verdict juger(uint32_t nowMs, Cause *cause) const;
  void appliquerVerdict(Verdict v, Cause c, uint32_t nowMs);
  Plan planVentilo(uint32_t nowMs, Touche *t, bool *sonde);
  Plan planLampe(Touche *t) const;
  bool delaiMoteurOk(uint32_t nowMs) const;
  bool planifier(uint32_t nowMs, Action *out);
  Action presser(Sujet s, Touche t, bool sonde, uint32_t nowMs);

  Params p_;
  ModeEtat mode_ = ModeEtat::Repete;
  bool annexe_ = false;
  Etat etat_;
  Moteur derniere_ = Moteur::V2;
  uint32_t demarrageMs_ = 0;
  bool arretLu_ = false;  // regle 3 : dernier etat lu = moteur arrete, aucun appui depuis
  Moteur moteurLu_ = Moteur::Arret;
  uint32_t changementMoteurMs_ = 0;  // dernier changement du moteur, lu ou deduit, toutes origines
  bool piloteOk_ = true;
  Ordre ov_, ol_;
  bool cibleEteint_ = false;
  Moteur cibleMoteur_ = Moteur::Arret;
  bool cibleLampe_ = false;
  Vol vol_;
  Reprise reprise_[2];
  uint32_t idAppui_ = 0;
  bool aEmis_ = false;
  uint32_t dernierFinMs_ = 0;  // fin du dernier appui emis (entre_appuis_ms)
  bool repriseImmediate_ = false;
  bool aPublier_ = false;
  bool muette_ = false;  // ligne muette deja signalee (mode repete)
  Compteurs c_;
  static constexpr uint8_t kFile = 16;
  Action file_[kFile];
  uint8_t tete_ = 0, nb_ = 0;
};

}  // namespace hotte
