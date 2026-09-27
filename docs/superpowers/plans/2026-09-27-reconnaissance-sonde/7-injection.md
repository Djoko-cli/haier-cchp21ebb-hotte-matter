# Plan de la sonde, partie 7 : injection

> En-tête, contraintes globales, carte des fichiers et interfaces : [../2026-09-27-reconnaissance-sonde.md](../2026-09-27-reconnaissance-sonde.md). Les tâches s'exécutent dans l'ordre des numéros (22 à 25).

### Tâche 22 : Garde-fous de l'injection (`injection_regles`), réglages d'injection en NVS, commande `injection regle`

**Fichiers :**
- Créer : `src/injection_regles.h`, `src/injection_regles.cpp`
- Créer : `tools/tests/test_injection.cpp`
- Modifier : `tools/tests/test_hote.sh` (deux lignes `test_injection` avant `echo "tests hote : OK"`)
- Modifier : `src/reglages.h`, `src/reglages.cpp` (champs `injMontee` et `injection`, clés NVS `inj_monte` et `inj_params`, bornes au chargement)
- Modifier : `src/cli.cpp` (inclusion ; `afficherInjection`, `appliquerInjection`, `faireRegle`, `faireInjection` après `faireSeuils` ; `cmdInjection` après `cmdWifi` ; ligne `{"injection", ...}` en fin de `kCommandes`)
- Modifier : `docs/RECONNAISSANCE.md` (étape 7 : ligne « charger les valeurs de l'avenant : `injection regle <nom> <valeur>` », lignes suivantes renumérotées)
- Tester : `tools/tests/test_injection.cpp` (par `sh tools/tests/test_hote.sh`), `~/.platformio/penv/bin/pio run -e sonde`

**Interfaces :**
- Consomme :
  - `src/json_out.h` (tâche 8) : `const char *jsonp::remoteRefusal(const char *cmd);` (liste blanche : `injection on|off` et `injecte ...` seulement), `jsonp::kMsgMax` (120).
  - `src/capture_model.h` (tâche 9) : `uint8_t capt::borner(capt::Reglages *r);`.
  - `src/reglages.h` (tâche 7) : `struct ReglagesSonde { capt::Reglages capture; bool changements = false; };`, `uint8_t reglagesCharger(ReglagesSonde *r);`, `bool reglagesSauver(const ReglagesSonde &r);` ; `src/config.h` : `kNvsEspace`, `kPinInjection`.
  - `src/sonde.h` (tâche 10) : `ReglagesSonde &sondeReglages();`, `bool sondeAppliquer(const ReglagesSonde &r);` (NVS, puis `jsonConfigChanged()`).
  - `src/cli.cpp` (tâches 10 et 11) : `static char *splitWord(char *s)`, `static bool lireU32(const char *s, uint32_t *v)`, `struct Resultat { bool ok; const char *code; const char *msg; }`, `static Resultat faireSeuils(char *args)`, `static void cmdWifi(char *args)`, `static const Commande kCommandes[]`.
  - `tools/tests/verif.h` (tâche 8) : `VERIF`, `VERIF_EGAL_STR`, `bilan`.
- Produit :
  - `src/injection_regles.h` : l'API du contrat (`inj::kDurMax`, `inj::Params`, `paramsValides`, `inj::Refus`, `refusTexte`, `inj::Demande`, `analyser`, `inj::Etat`, `niveauAttendu`, `frontAnormal`), à la lettre, plus ces ajouts :
    ```cpp
    constexpr uint8_t kNbParams = 7;
    const char *nomParam(uint8_t i);                                // i >= kNbParams : nullptr
    bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi);  // false : nom inconnu
    enum class Reglage : uint8_t { Ok, NomInconnu, HorsBornes };
    Reglage reglerParam(Params *p, const char *nom, uint32_t v);    // *p inchange si refus
    bool chargerParams(const void *octets, size_t n, Params *out);  // image NVS ; false : defauts
    uint32_t totalUs(const Demande &d);
    bool frontProgramme(const Demande &d, uint32_t tRelUs, uint32_t tolUs);
    struct Sym { uint16_t d0; uint8_t l0; uint16_t d1; uint8_t l1; };
    constexpr size_t kSymMax = 48;
    size_t versSymboles(const Demande &d, Sym *out, size_t cap);
    ```
  - `src/reglages.h` : `ReglagesSonde` gagne `bool injMontee = false;` et `inj::Params injection;` (clés NVS `inj_monte`, `inj_params`).
  - Console USB : `injection` (état et valeurs, en texte) et `injection regle <nom> <valeur>` ; `static bool appliquerInjection(const ReglagesSonde &r)` et `static Resultat faireInjection(char *args)` dans `cli.cpp`, que la tâche 23 étend.

**Décisions de cette tâche (à connaître pour les suivantes) :**
- **Bornes des valeurs d'injection** (l'enveloppe de sécurité que l'avenant de l'étape 6 ne pourra pas dépasser ; les défauts du contrat sont dedans) : `bas_max_us` 10..20 000 ; `total_max_us` 100..200 000, et au moins `bas_max_us` (borne haute égale à la valeur par défaut : à cette borne, `relu_us` n'est jamais tronqué, tâche 8) ; `silence_min_us` 1 000..1 000 000 ; `attente_max_ms` 10..10 000 ; `delai_min_ms` 1 000..60 000 ; `arme_max_s` 10..600 (jamais plus des 10 min de la spec §8.3) ; `tol_us` 1..500. Elles vivent dans un seul tableau, `kChamps` de `injection_regles.cpp` ; `json_check.py` en garde une copie, comparée par un test (tâche 23).
- **`analyser`** : premier mot `durees`, puis des entiers décimaux stricts (chiffres seuls, 1 à 4 294 967 295), séparés par des espaces. Ordre des refus : `Syntaxe` (mot-clé, nombre mal formé ou nul, aucune durée), puis `TropDeDurees` (plus de 64), `BasTropLong` (une durée de rang pair au-delà de `basMaxUs`), `TropLong` (somme au-delà de `totalMaxUs`). `*out` n'est écrit que si la demande est acceptée.
- **`Etat`** : différences d'horloge en `uint32_t` (justes quand `millis()` repasse par 0) ; `resteS` arrondi au-dessus (armée depuis 0,1 s : 600) ; `admettre` refuse dans l'ordre `NonMontee`, `NonArmee`, `Delai`.
- **`frontProgramme`** (ajout) : le test « un front programmé à ±tol » de `frontAnormal`, sorti en fonction ; la surveillance de la tâche 23 s'en sert aussi. `frontAnormal` reste exactement celle du contrat.
- **`versSymboles`** : GPIO7 haut (1) = bus tiré bas (rang pair), GPIO7 bas (0) = bus relâché ; une durée de plus de 32 767 µs est coupée en demi-symboles de même niveau ; un nombre impair de demi-symboles est complété par 1 µs GPIO bas (une durée nulle arrêterait l'émission). Pire cas des bornes (64 durées, 200 ms) : 35 symboles, sous `kSymMax` = 48, une mémoire de canal RMT.
- **IRAM** : `niveauAttendu`, `frontAnormal` et `frontProgramme` seront appelées par l'interruption des fronts (tâche 23). Le fichier reste pur : `IRAM_ATTR` vient de `<esp_attr.h>` s'il existe (`__has_include`), sinon il ne fait rien (hôte).
- **NVS** : `inj_params` est l'image de `inj::Params` (7 × `uint32_t`, 28 octets). Une image d'une autre taille, ou hors bornes, est remplacée **tout entière** par les valeurs par défaut, et comptée dans le nombre rendu par `reglagesCharger` : le message existant de `setup()` (`[reglages] N valeur(s) hors bornes en NVS`) la signale.
- **`injection regle <nom> <valeur>`** : USB seulement (`remoteRefusal` refuse déjà tout `injection` autre que `on`/`off`), valeurs vérifiées **avant** la NVS, puis `sondeAppliquer` (NVS, `config` réémise). Avec un `id`, `injection ...` reste ici une commande à texte (`reponse` `debut`, texte, `fin`) ; la tâche 23 la rend sans texte. C'est par elle que la préparation de l'étape 7 du journal charge les valeurs de l'avenant (`docs/RECONNAISSANCE.md`, complété plus bas dans cette tâche).

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_injection.cpp` :

```cpp
// Tests hote des garde-fous de l'injection (src/injection_regles.*) : bornes
// des parametres (reglage par nom, image lue en NVS), analyse de
// 'injecte durees ...', etat (montee, armement, desarmement seul apres
// arme_max_s, delai minimal), niveau attendu et front anormal pendant une
// emission, symboles RMT de l'etage d'injection ; liste blanche a distance
// pour la commande 'injection'.
// Lancer : sh tools/tests/test_hote.sh
#include <string.h>

#include <string>

#include "injection_regles.h"
#include "json_out.h"
#include "verif.h"

using namespace inj;

static void testParams() {
  const Params p;
  VERIF(p.basMaxUs == 3000 && p.totalMaxUs == 200000 && p.silenceMinUs == 20000);
  VERIF(p.attenteMaxMs == 1000 && p.delaiMinMs == 3000 && p.armeMaxS == 600 && p.tolUs == 20);
  VERIF(paramsValides(p));  // les defauts d'avant l'avenant sont dans les bornes
  // Noms : ceux des champs de config.injection, dans cet ordre.
  const char *noms[] = {"bas_max_us", "total_max_us", "silence_min_us", "attente_max_ms",
                        "delai_min_ms", "arme_max_s", "tol_us"};
  VERIF(kNbParams == 7);
  for (uint8_t i = 0; i < kNbParams; i++) VERIF_EGAL_STR(nomParam(i), noms[i]);
  VERIF(nomParam(kNbParams) == nullptr);
  uint32_t lo = 0, hi = 0;
  VERIF(bornesParam("delai_min_ms", &lo, &hi) && lo == 1000 && hi == 60000);
  VERIF(bornesParam("arme_max_s", &lo, &hi) && lo == 10 && hi == 600);  // jamais plus de 10 min (spec 8.3)
  VERIF(bornesParam("tol_us", &lo, &hi) && lo == 1 && hi == 500);
  // Borne haute de total_max_us egale au defaut : relu_us n'est jamais tronque (json_out.h).
  VERIF(bornesParam("total_max_us", &lo, &hi) && lo == 100 && hi == 200000);
  VERIF(!bornesParam("inconnu", &lo, &hi));
  // Chaque borne est atteinte, et pas au-dela.
  for (uint8_t i = 0; i < kNbParams; i++) {
    Params q;
    VERIF(bornesParam(nomParam(i), &lo, &hi));
    if (!strcmp(nomParam(i), "total_max_us")) q.basMaxUs = lo;  // bas_max_us <= total_max_us
    VERIF(reglerParam(&q, nomParam(i), lo) == Reglage::Ok);
    VERIF(reglerParam(&q, nomParam(i), hi) == Reglage::Ok);
    VERIF(reglerParam(&q, nomParam(i), lo - 1) == Reglage::HorsBornes);
    VERIF(reglerParam(&q, nomParam(i), hi + 1) == Reglage::HorsBornes);
  }
}

static void testReglerParam() {
  Params p;
  VERIF(reglerParam(&p, "tol_us", 30) == Reglage::Ok && p.tolUs == 30);
  VERIF(reglerParam(&p, "tol_us", 0) == Reglage::HorsBornes && p.tolUs == 30);  // inchange si refus
  VERIF(reglerParam(&p, "vitesse", 3) == Reglage::NomInconnu);
  VERIF(reglerParam(&p, "arme_max_s", 601) == Reglage::HorsBornes && p.armeMaxS == 600);
  VERIF(reglerParam(&p, "delai_min_ms", 999) == Reglage::HorsBornes && p.delaiMinMs == 3000);
  // Coherence : bas_max_us <= total_max_us.
  VERIF(reglerParam(&p, "total_max_us", 2000) == Reglage::HorsBornes && p.totalMaxUs == 200000);
  VERIF(reglerParam(&p, "bas_max_us", 2000) == Reglage::Ok);
  VERIF(reglerParam(&p, "total_max_us", 2000) == Reglage::Ok && p.totalMaxUs == 2000);
  VERIF(reglerParam(&p, "bas_max_us", 2001) == Reglage::HorsBornes && p.basMaxUs == 2000);
}

static void testChargerParams() {
  Params lu;
  lu.tolUs = 35;
  lu.delaiMinMs = 5000;
  Params out;
  VERIF(chargerParams(&lu, sizeof(lu), &out) && out.tolUs == 35 && out.delaiMinMs == 5000);
  // Image d'une autre taille (autre version du firmware) : valeurs par defaut.
  out.tolUs = 99;
  VERIF(!chargerParams(&lu, sizeof(lu) - 4, &out) && out.tolUs == 20 && out.delaiMinMs == 3000);
  VERIF(!chargerParams(nullptr, 0, &out) && out.tolUs == 20);
  // Valeur hors bornes en NVS : tout revient aux valeurs par defaut.
  Params mauvais;
  mauvais.basMaxUs = 50000;
  mauvais.tolUs = 40;
  VERIF(!chargerParams(&mauvais, sizeof(mauvais), &out) && out.basMaxUs == 3000 && out.tolUs == 20);
  mauvais = Params();
  mauvais.armeMaxS = 3600;
  VERIF(!chargerParams(&mauvais, sizeof(mauvais), &out) && out.armeMaxS == 600);
}

static void testRefusTexte() {
  VERIF_EGAL_STR(refusTexte(Refus::NonMontee), "non montee");
  VERIF_EGAL_STR(refusTexte(Refus::NonArmee), "non armee");
  VERIF_EGAL_STR(refusTexte(Refus::Delai), "delai minimal");
  VERIF_EGAL_STR(refusTexte(Refus::Syntaxe), "syntaxe");
  VERIF_EGAL_STR(refusTexte(Refus::BasTropLong), "duree basse trop longue");
  VERIF_EGAL_STR(refusTexte(Refus::TropLong), "trame trop longue");
  VERIF_EGAL_STR(refusTexte(Refus::TropDeDurees), "trop de durees");
  VERIF_EGAL_STR(refusTexte(Refus::Aucun), "aucun");
}

static void testAnalyser() {
  const Params p;
  Demande d;
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  VERIF(d.n == 4 && d.dur[0] == 750 && d.dur[1] == 750 && d.dur[2] == 750 && d.dur[3] == 2250);
  VERIF(totalUs(d) == 4500);
  VERIF(analyser("  durees   1500  750 ", p, &d) == Refus::Aucun && d.n == 2 && d.dur[0] == 1500 && d.dur[1] == 750);
  // Syntaxe : mot-cle, entiers stricts > 0, au moins une duree.
  Demande garde;
  garde.n = 7;
  VERIF(analyser("durees", p, &garde) == Refus::Syntaxe && garde.n == 7);  // *out inchange si refus
  VERIF(analyser("", p, &d) == Refus::Syntaxe);
  VERIF(analyser(nullptr, p, &d) == Refus::Syntaxe);
  VERIF(analyser("touche lumiere", p, &d) == Refus::Syntaxe);  // syntaxe nommee : avec l'avenant
  VERIF(analyser("dureesx 750", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 0 750", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 -5", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 +5", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 7a", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 4294967296", p, &d) == Refus::Syntaxe);
  // Duree basse (rang pair) : bas_max_us au plus ; une duree haute peut depasser.
  VERIF(analyser("durees 3000 5000 3000", p, &d) == Refus::Aucun);
  VERIF(analyser("durees 3001", p, &d) == Refus::BasTropLong);
  VERIF(analyser("durees 750 750 3001", p, &d) == Refus::BasTropLong);
  // Somme : total_max_us au plus.
  VERIF(analyser("durees 1000 99000 1000 99000", p, &d) == Refus::Aucun && totalUs(d) == 200000);
  VERIF(analyser("durees 1000 99000 1000 99001", p, &d) == Refus::TropLong);
  // 64 durees au plus.
  std::string s = "durees";
  for (int i = 0; i < kDurMax; i++) s += " 1";
  VERIF(analyser(s.c_str(), p, &d) == Refus::Aucun && d.n == kDurMax);
  s += " 1";
  VERIF(analyser(s.c_str(), p, &d) == Refus::TropDeDurees);
  // Les bornes suivent les parametres en vigueur.
  Params q;
  q.basMaxUs = 500;
  VERIF(analyser("durees 750", q, &d) == Refus::BasTropLong);
}

static void testEtat() {
  Params p;
  Etat e;
  VERIF(!e.montee() && !e.armee(0, p) && e.resteS(0, p) == 0);
  VERIF(e.admettre(1000, p) == Refus::NonMontee);
  e.setMontee(true);
  VERIF(e.admettre(1000, p) == Refus::NonArmee);
  e.armer(1000);
  VERIF(e.armee(1000, p) && e.resteS(1000, p) == 600);
  VERIF(e.resteS(1000 + 599001, p) == 1);                           // arrondi au-dessus
  VERIF(e.armee(1000 + 599999, p) && !e.armee(1000 + 600000, p));   // desarmee seule apres 600 s
  VERIF(e.resteS(1000 + 600000, p) == 0);
  VERIF(e.admettre(1000 + 600000, p) == Refus::NonArmee);
  // Rearmee ; le delai minimal court depuis la derniere emission.
  e.armer(700000);
  VERIF(e.admettre(700000, p) == Refus::Aucun);  // aucune emission encore
  e.noterEmission(701000);
  VERIF(e.admettre(701000 + 2999, p) == Refus::Delai);
  VERIF(e.admettre(701000 + 3000, p) == Refus::Aucun);
  // Ordre des refus : montee, puis armee, puis delai.
  e.setMontee(false);
  VERIF(e.admettre(701500, p) == Refus::NonMontee);
  e.setMontee(true);
  e.desarmer();
  VERIF(e.admettre(701500, p) == Refus::NonArmee && !e.armee(701500, p));
  // arme_max_s regle plus court : desarme plus tot.
  p.armeMaxS = 30;
  e.armer(800000);
  VERIF(e.armee(800000 + 29999, p) && !e.armee(800000 + 30000, p));
  // millis() qui repasse par 0 (49,7 jours) : les ecarts restent justes.
  e.armer(0xFFFFFF00u);
  VERIF(e.armee(0x100u, p) && e.resteS(0x100u, p) == 30);
}

static void testNiveauEtFront() {
  const Params p;
  Demande d;
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  // Fronts programmes : 0 (vers le bas), 750, 1500, 2250, 4500 (fin).
  VERIF(!niveauAttendu(d, 0) && !niveauAttendu(d, 749));
  VERIF(niveauAttendu(d, 750) && niveauAttendu(d, 1499));
  VERIF(!niveauAttendu(d, 1500) && niveauAttendu(d, 2250) && niveauAttendu(d, 4499));
  VERIF(niveauAttendu(d, 4500) && niveauAttendu(d, 100000));  // apres la fin : bus relache, haut
  // Front pres d'un front programme (+-tol) : normal, quel que soit son sens.
  VERIF(!frontAnormal(d, 752, true, 20));
  VERIF(!frontAnormal(d, 730, true, 20) && !frontAnormal(d, 770, false, 20));
  VERIF(!frontAnormal(d, 4520, true, 20));
  // Loin d'un front programme : anormal seulement si le niveau differe de l'attendu.
  VERIF(frontAnormal(d, 1000, false, 20));   // on relache (haut attendu), quelqu'un tire la ligne
  VERIF(!frontAnormal(d, 1000, true, 20));
  VERIF(frontAnormal(d, 771, false, 20) && !frontAnormal(d, 770, false, 20));
  VERIF(frontAnormal(d, 1521, true, 20) && !frontAnormal(d, 1521, false, 20));
  VERIF(frontAnormal(d, 3000, false, 20));
  VERIF(frontAnormal(d, 5000, false, 20));   // apres la fin (la surveillance s'arrete avant)
  VERIF(!frontAnormal(d, 750, false, 0));    // tolerance nulle : exactement au front
  VERIF(frontAnormal(d, 751, false, 0));
  // Front programme a +-tol : 0, 750, 1500, 2250, 4500.
  VERIF(frontProgramme(d, 0, 20) && frontProgramme(d, 752, 20) && frontProgramme(d, 4515, 20));
  VERIF(!frontProgramme(d, 1100, 20) && !frontProgramme(d, 771, 20) && !frontProgramme(d, 4521, 20));
  // Nombre impair : la derniere duree est basse, le relachement final est un front programme.
  VERIF(analyser("durees 1500 750 750", p, &d) == Refus::Aucun);
  VERIF(!niveauAttendu(d, 2999) && niveauAttendu(d, 3000));
  VERIF(!frontAnormal(d, 3010, true, 20));
}

static void testSymboles() {
  Sym y[kSymMax] = {};
  const Params p;
  Demande d;
  // Etage inverseur : GPIO7 haut (1) = bus tire bas ; rang pair = bas.
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  VERIF(versSymboles(d, y, kSymMax) == 2);
  VERIF(y[0].d0 == 750 && y[0].l0 == 1 && y[0].d1 == 750 && y[0].l1 == 0);
  VERIF(y[1].d0 == 750 && y[1].l0 == 1 && y[1].d1 == 2250 && y[1].l1 == 0);
  // Nombre impair de demi-symboles : complete par 1 us GPIO bas (bus relache).
  VERIF(analyser("durees 1500 750 750", p, &d) == Refus::Aucun);
  VERIF(versSymboles(d, y, kSymMax) == 2);
  VERIF(y[0].d0 == 1500 && y[0].l0 == 1 && y[0].d1 == 750 && y[0].l1 == 0);
  VERIF(y[1].d0 == 750 && y[1].l0 == 1 && y[1].d1 == 1 && y[1].l1 == 0);
  // Duree haute de plus de 32 767 us : decoupee en demi-symboles de meme niveau.
  VERIF(analyser("durees 100 70000", p, &d) == Refus::Aucun);
  VERIF(versSymboles(d, y, kSymMax) == 2);
  VERIF(y[0].d0 == 100 && y[0].l0 == 1 && y[0].d1 == 32767 && y[0].l1 == 0);
  VERIF(y[1].d0 == 32767 && y[1].l0 == 0 && y[1].d1 == 4466 && y[1].l1 == 0);
  // Pire cas des bornes : 64 durees, somme de 200 000 us (borne haute de
  // total_max_us, le defaut) : 63 demi-symboles de 10 us, puis 199 370 us
  // haut en 7 demi-symboles, soit 35 symboles, sous kSymMax.
  std::string s = "durees";
  for (int i = 0; i < 63; i++) s += " 10";
  s += " 199370";
  VERIF(analyser(s.c_str(), p, &d) == Refus::Aucun && totalUs(d) == 200000);
  VERIF(versSymboles(d, y, kSymMax) == 35);
  // Capacite trop petite, demande vide : 0.
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  VERIF(versSymboles(d, y, 1) == 0);
  VERIF(versSymboles(Demande(), y, kSymMax) == 0);
}

// Liste blanche a distance (jsonp::remoteRefusal) : 'injection on|off' et
// 'injecte ...' seulement ; 'injection regle' et 'injection monte' : USB.
static void testDistance() {
  VERIF(jsonp::remoteRefusal("injection on") == nullptr);
  VERIF(jsonp::remoteRefusal("injection off") == nullptr);
  VERIF(jsonp::remoteRefusal("injecte durees 750 750") == nullptr);
  VERIF(jsonp::remoteRefusal("injection regle tol_us 30") != nullptr);
  VERIF(jsonp::remoteRefusal("injection monte 1") != nullptr);
  VERIF(jsonp::remoteRefusal("injection") != nullptr);
}

int main() {
  testParams();
  testReglerParam();
  testChargerParams();
  testRefusTexte();
  testAnalyser();
  testEtat();
  testNiveauEtFront();
  testSymboles();
  testDistance();
  return bilan("test_injection");
}
```

Dans `tools/tests/test_hote.sh` (les deux lignes de `test_injection`, avant la ligne `echo` finale) :

Après :

```sh
# Enveloppe H1 : crypto de CommonCrypto (macOS, dans libSystem : rien a lier) ; mbedTLS sur la carte.
$CXX src/h1_proto.cpp tools/tests/test_h1.cpp -o "$OUT/test_h1"
"$OUT/test_h1"
```

insérer :

```sh
$CXX src/injection_regles.cpp src/json_out.cpp tools/tests/test_injection.cpp -o "$OUT/test_injection"
"$OUT/test_injection"
```

`json_out.cpp` est lié au test pour la liste blanche (`remoteRefusal`) ; il n'appelle aucune fonction de `capture_model.cpp`.

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`
Attendu : ÉCHEC après `test_h1 : 121 verification(s), 0 echec(s)`, avec « clang++: error: no such file or directory: 'src/injection_regles.cpp' ».

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `src/injection_regles.h` :

```cpp
#pragma once
// ===========================================================================
//  Garde-fous de l'injection (docs/SPEC-RECONNAISSANCE.md 8.3 et 8.4) : pur
//
//  - Params : valeurs de l'avenant de l'etape 6 (defauts d'avant l'avenant),
//    bornes appliquees par 'injection regle' et au chargement de la NVS ;
//  - analyser : 'injecte durees <d1> <d2> ...', la seule syntaxe avant
//    l'avenant (durees en us, alternees, la premiere au niveau BAS du bus) ;
//  - Etat : etage declare monte, armement (desarme seul apres armeMaxS),
//    delai minimal entre deux emissions ;
//  - niveauAttendu, frontAnormal, frontProgramme : surveillance des fronts
//    pendant une emission (appelees par l'interruption des fronts : en IRAM
//    sur la carte) ;
//  - versSymboles : durees -> symboles RMT de l'etage d'injection.
//
//  Pur et sans Arduino : teste sur l'hote (tools/tests/test_injection.cpp).
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

namespace inj {

constexpr uint16_t kDurMax = 64;  // durees par demande

struct Params {
  uint32_t basMaxUs = 3000, totalMaxUs = 200000, silenceMinUs = 20000;
  uint32_t attenteMaxMs = 1000, delaiMinMs = 3000, armeMaxS = 600, tolUs = 20;
};
// Chaque valeur dans ses bornes (bornesParam), et basMaxUs <= totalMaxUs.
bool paramsValides(const Params &p);

// Ajouts : reglage par nom ('injection regle <nom> <valeur>', USB seulement)
// et chargement de la NVS. Noms : ceux des champs de config.injection.
constexpr uint8_t kNbParams = 7;
const char *nomParam(uint8_t i);                                // i >= kNbParams : nullptr
bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi);  // false : nom inconnu
enum class Reglage : uint8_t { Ok, NomInconnu, HorsBornes };
Reglage reglerParam(Params *p, const char *nom, uint32_t v);    // *p inchange si refus
// Image lue en NVS (cle inj_params) : acceptee si elle a la taille de Params
// et des valeurs valides (true) ; sinon *out recoit les valeurs par defaut.
bool chargerParams(const void *octets, size_t n, Params *out);

enum class Refus : uint8_t { Aucun, NonMontee, NonArmee, Delai, Syntaxe, BasTropLong, TropLong, TropDeDurees };
const char *refusTexte(Refus r);  // "non montee", "non armee", "delai minimal", "syntaxe", ...

struct Demande {
  uint16_t n = 0;
  uint32_t dur[kDurMax] = {};  // alternees, dur[0] au niveau BAS du bus
};
// args : ce qui suit 'injecte' (ex. "durees 750 750 750 2250"). Bornes : chaque duree basse
// (rang pair) <= basMaxUs, somme <= totalMaxUs, 1..kDurMax durees, entiers > 0.
// *out n'est ecrit que si la demande est acceptee (Refus::Aucun).
Refus analyser(const char *args, const Params &p, Demande *out);
uint32_t totalUs(const Demande &d);  // ajout : somme des durees

class Etat {
 public:
  void setMontee(bool m) { montee_ = m; }
  bool montee() const { return montee_; }
  void armer(uint32_t nowMs);
  void desarmer() { arme_ = false; }
  bool armee(uint32_t nowMs, const Params &p) const;       // faux apres armeMaxS
  uint32_t resteS(uint32_t nowMs, const Params &p) const;  // 0 si non armee
  Refus admettre(uint32_t nowMs, const Params &p) const;   // montee, armee, delai depuis la derniere emission
  void noterEmission(uint32_t nowMs);

 private:
  bool montee_ = false, arme_ = false, aEmis_ = false;
  uint32_t armeAt_ = 0, emisAt_ = 0;
};

// Niveau du bus attendu a tRelUs depuis le debut de l'emission (true = haut ; apres la fin : haut).
bool niveauAttendu(const Demande &d, uint32_t tRelUs);
// Front observe vers le niveau niv a tRelUs : anormal si aucun front programme a +-tolUs
// et si niv differe du niveau attendu.
bool frontAnormal(const Demande &d, uint32_t tRelUs, bool niv, uint32_t tolUs);
// Ajout : un front programme (0, puis la fin de chaque duree) a +-tolUs de tRelUs ?
bool frontProgramme(const Demande &d, uint32_t tRelUs, uint32_t tolUs);

// Ajout : symboles RMT (miroir de rmt_symbol_word_t) de l'etage d'injection,
// niveau GPIO7 : 1 = bus tire bas (rang pair), 0 = bus relache. Une duree de
// plus de 32 767 us est decoupee en demi-symboles de meme niveau ; un nombre
// impair de demi-symboles est complete par 1 us GPIO bas (bus relache). Rend
// le nombre de symboles, 0 si la demande est vide ou si cap est trop petit.
struct Sym {
  uint16_t d0;
  uint8_t l0;
  uint16_t d1;
  uint8_t l1;
};
constexpr size_t kSymMax = 48;  // une memoire de canal RMT ; le pire cas des bornes en prend 35
size_t versSymboles(const Demande &d, Sym *out, size_t cap);

}  // namespace inj
```

Créer `src/injection_regles.cpp` :

```cpp
// ===========================================================================
//  Garde-fous de l'injection : voir injection_regles.h. Pur (teste sur l'hote).
// ===========================================================================
#include "injection_regles.h"

#include <string.h>

// Sur la carte, les fonctions appelees par l'interruption des fronts
// (bord.cpp) sont en IRAM ; sur l'hote, IRAM_ATTR ne fait rien.
#if defined(__has_include)
#if __has_include(<esp_attr.h>)
#include <esp_attr.h>
#endif
#endif
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

namespace inj {

namespace {

// Bornes des parametres (enveloppe de securite de l'avenant). Ordre : celui de config.injection.
struct Champ {
  const char *nom;
  uint32_t Params::*m;
  uint32_t lo, hi;
};
const Champ kChamps[kNbParams] = {
    {"bas_max_us", &Params::basMaxUs, 10, 20000},
    {"total_max_us", &Params::totalMaxUs, 100, 200000},
    {"silence_min_us", &Params::silenceMinUs, 1000, 1000000},
    {"attente_max_ms", &Params::attenteMaxMs, 10, 10000},
    {"delai_min_ms", &Params::delaiMinMs, 1000, 60000},
    {"arme_max_s", &Params::armeMaxS, 10, 600},
    {"tol_us", &Params::tolUs, 1, 500},
};

const Champ *champ(const char *nom) {
  if (!nom) return nullptr;
  for (const Champ &c : kChamps)
    if (!strcmp(c.nom, nom)) return &c;
  return nullptr;
}

const char *espaces(const char *s) {
  while (*s == ' ') s++;
  return s;
}

}  // namespace

bool paramsValides(const Params &p) {
  for (const Champ &c : kChamps)
    if (p.*c.m < c.lo || p.*c.m > c.hi) return false;
  return p.basMaxUs <= p.totalMaxUs;
}

const char *nomParam(uint8_t i) { return i < kNbParams ? kChamps[i].nom : nullptr; }

bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi) {
  const Champ *c = champ(nom);
  if (!c) return false;
  *lo = c->lo;
  *hi = c->hi;
  return true;
}

Reglage reglerParam(Params *p, const char *nom, uint32_t v) {
  const Champ *c = champ(nom);
  if (!c) return Reglage::NomInconnu;
  Params q = *p;
  q.*c->m = v;
  if (!paramsValides(q)) return Reglage::HorsBornes;
  *p = q;
  return Reglage::Ok;
}

bool chargerParams(const void *octets, size_t n, Params *out) {
  Params p;
  if (octets && n == sizeof(Params)) memcpy(&p, octets, sizeof(Params));
  const bool ok = octets && n == sizeof(Params) && paramsValides(p);
  *out = ok ? p : Params();
  return ok;
}

const char *refusTexte(Refus r) {
  switch (r) {
    case Refus::Aucun: return "aucun";
    case Refus::NonMontee: return "non montee";
    case Refus::NonArmee: return "non armee";
    case Refus::Delai: return "delai minimal";
    case Refus::Syntaxe: return "syntaxe";
    case Refus::BasTropLong: return "duree basse trop longue";
    case Refus::TropLong: return "trame trop longue";
    case Refus::TropDeDurees: return "trop de durees";
  }
  return "?";
}

Refus analyser(const char *args, const Params &p, Demande *out) {
  const char *s = espaces(args ? args : "");
  if (strncmp(s, "durees", 6) || (s[6] && s[6] != ' ')) return Refus::Syntaxe;
  s += 6;
  Demande d;
  uint32_t n = 0;
  uint64_t somme = 0;
  bool basTropLong = false;
  for (s = espaces(s); *s; s = espaces(s)) {
    uint64_t v = 0;
    const char *debut = s;
    for (; *s >= '0' && *s <= '9'; s++) {
      v = v * 10 + (uint64_t)(*s - '0');
      if (v > 0xFFFFFFFFull) return Refus::Syntaxe;
    }
    if (s == debut || (*s && *s != ' ') || !v) return Refus::Syntaxe;
    if (n < kDurMax) d.dur[n] = (uint32_t)v;
    if (n % 2 == 0 && v > p.basMaxUs) basTropLong = true;
    somme += v;
    n++;
  }
  if (!n) return Refus::Syntaxe;
  if (n > kDurMax) return Refus::TropDeDurees;
  if (basTropLong) return Refus::BasTropLong;
  if (somme > p.totalMaxUs) return Refus::TropLong;
  d.n = (uint16_t)n;
  *out = d;
  return Refus::Aucun;
}

uint32_t totalUs(const Demande &d) {
  uint64_t s = 0;
  for (uint16_t i = 0; i < d.n && i < kDurMax; i++) s += d.dur[i];
  return s > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)s;
}

void Etat::armer(uint32_t nowMs) {
  arme_ = true;
  armeAt_ = nowMs;
}

bool Etat::armee(uint32_t nowMs, const Params &p) const {
  return arme_ && (uint64_t)(uint32_t)(nowMs - armeAt_) < (uint64_t)p.armeMaxS * 1000u;
}

uint32_t Etat::resteS(uint32_t nowMs, const Params &p) const {
  if (!armee(nowMs, p)) return 0;
  const uint64_t reste = (uint64_t)p.armeMaxS * 1000u - (uint32_t)(nowMs - armeAt_);
  return (uint32_t)((reste + 999) / 1000);
}

Refus Etat::admettre(uint32_t nowMs, const Params &p) const {
  if (!montee_) return Refus::NonMontee;
  if (!armee(nowMs, p)) return Refus::NonArmee;
  if (aEmis_ && (uint32_t)(nowMs - emisAt_) < p.delaiMinMs) return Refus::Delai;
  return Refus::Aucun;
}

void Etat::noterEmission(uint32_t nowMs) {
  aEmis_ = true;
  emisAt_ = nowMs;
}

bool IRAM_ATTR niveauAttendu(const Demande &d, uint32_t tRelUs) {
  uint32_t fin = 0;
  for (uint16_t i = 0; i < d.n && i < kDurMax; i++) {
    fin += d.dur[i];
    if (tRelUs < fin) return i % 2 == 1;  // rang pair : bus tire bas
  }
  return true;  // apres la fin : bus relache
}

bool IRAM_ATTR frontProgramme(const Demande &d, uint32_t tRelUs, uint32_t tolUs) {
  // Fronts programmes : 0, puis la fin de chaque duree (la derniere comprise).
  uint32_t front = 0;
  for (uint16_t i = 0; i <= d.n && i <= kDurMax; i++) {
    const uint32_t ecart = tRelUs > front ? tRelUs - front : front - tRelUs;
    if (ecart <= tolUs) return true;
    if (i < d.n && i < kDurMax) front += d.dur[i];
  }
  return false;
}

bool IRAM_ATTR frontAnormal(const Demande &d, uint32_t tRelUs, bool niv, uint32_t tolUs) {
  return !frontProgramme(d, tRelUs, tolUs) && niv != niveauAttendu(d, tRelUs);
}

size_t versSymboles(const Demande &d, Sym *out, size_t cap) {
  size_t k = 0;  // demi-symboles poses
  for (uint16_t i = 0; i < d.n && i < kDurMax; i++) {
    const uint8_t niv = i % 2 == 0 ? 1 : 0;  // GPIO7 haut = bus bas
    for (uint32_t reste = d.dur[i]; reste;) {
      const uint16_t t = reste > 32767 ? 32767 : (uint16_t)reste;
      if (k / 2 >= cap) return 0;
      Sym &y = out[k / 2];
      if (k % 2 == 0) {
        y = Sym{t, niv, 0, 0};
      } else {
        y.d1 = t;
        y.l1 = niv;
      }
      k++;
      reste -= t;
    }
  }
  if (!k) return 0;
  if (k % 2) {
    out[k / 2].d1 = 1;  // 1 us bus relache : une duree nulle arreterait l'emission trop tot
    out[k / 2].l1 = 0;
    k++;
  }
  return k / 2;
}

}  // namespace inj
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh | tail -3`
Attendu :
```
test_h1 : 121 verification(s), 0 echec(s)
test_injection : 155 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 5 : Ranger les valeurs d'injection en NVS et ajouter `injection regle`**

Remplacer tout le contenu de `src/reglages.h` (deux champs, et l'inclusion de `injection_regles.h`) par :

```cpp
#pragma once
#include "capture_model.h"
#include "injection_regles.h"

struct ReglagesSonde {
  capt::Reglages capture;
  bool changements = false;   // mode d'emission
  bool injMontee = false;     // etage d'injection declare monte ('injection monte 0|1', cle inj_monte)
  inj::Params injection;      // valeurs de l'avenant ('injection regle', cle inj_params)
};
// Lit l'espace NVS kNvsEspace ; applique capt::borner et inj::chargerParams ;
// rend le nombre de valeurs remplacees (un jeu inj_params refuse compte pour une).
uint8_t reglagesCharger(ReglagesSonde *r);
bool reglagesSauver(const ReglagesSonde &r);
```

Remplacer tout le contenu de `src/reglages.cpp` (clés `inj_monte` et `inj_params`) par :

```cpp
// ===========================================================================
//  Reglages de la sonde en NVS (espace kNvsEspace, "hotte")
//
//  Cles : resol_hz (u32), filtre_us (u8), silence_us (u32), inverse (bool),
//  chgt (bool, mode changements), inj_monte (bool, etage d'injection monte),
//  inj_params (octets : inj::Params, valeurs de l'avenant). Une cle absente
//  vaut sa valeur par defaut ; une valeur hors bornes est remplacee par sa
//  valeur par defaut au chargement (capt::borner ; inj::chargerParams remet
//  tout le jeu d'injection aux defauts), et l'appelant le signale (spec 8.2).
// ===========================================================================
#include "reglages.h"

#include <Preferences.h>

#include "config.h"

uint8_t reglagesCharger(ReglagesSonde *r) {
  *r = ReglagesSonde();
  uint8_t remplaces = 0;
  Preferences p;
  // Ouverture en ecriture meme pour lire : en lecture seule, un espace de noms
  // absent fait loguer une erreur NVS au premier demarrage.
  if (p.begin(kNvsEspace, false)) {
    // isKey() d'abord : interroger une cle absente logue une erreur NVS.
    if (p.isKey("resol_hz")) r->capture.resolHz = p.getUInt("resol_hz", r->capture.resolHz);
    if (p.isKey("filtre_us")) r->capture.filtreUs = p.getUChar("filtre_us", r->capture.filtreUs);
    if (p.isKey("silence_us")) r->capture.silenceUs = p.getUInt("silence_us", r->capture.silenceUs);
    if (p.isKey("inverse")) r->capture.inverse = p.getBool("inverse", r->capture.inverse);
    if (p.isKey("chgt")) r->changements = p.getBool("chgt", r->changements);
    if (p.isKey("inj_monte")) r->injMontee = p.getBool("inj_monte", false);
    if (p.isKey("inj_params")) {
      uint8_t image[sizeof(inj::Params)] = {};
      const size_t n = p.getBytesLength("inj_params");
      const bool lu = n == sizeof(image) && p.getBytes("inj_params", image, sizeof(image)) == sizeof(image);
      if (!inj::chargerParams(lu ? image : nullptr, lu ? n : 0, &r->injection)) remplaces++;
    }
    p.end();
  }
  return (uint8_t)(remplaces + capt::borner(&r->capture));
}

bool reglagesSauver(const ReglagesSonde &r) {
  Preferences p;
  if (!p.begin(kNvsEspace, false)) return false;
  bool ok = p.putUInt("resol_hz", r.capture.resolHz) == sizeof(uint32_t);
  ok &= p.putUChar("filtre_us", r.capture.filtreUs) == 1;
  ok &= p.putUInt("silence_us", r.capture.silenceUs) == sizeof(uint32_t);
  ok &= p.putBool("inverse", r.capture.inverse) == 1;
  ok &= p.putBool("chgt", r.changements) == 1;
  ok &= p.putBool("inj_monte", r.injMontee) == 1;
  ok &= p.putBytes("inj_params", &r.injection, sizeof(r.injection)) == sizeof(r.injection);
  p.end();
  return ok;
}
```

Dans `src/cli.cpp` (inclusion, fonctions de réglage après `faireSeuils`, `cmdInjection` après `cmdWifi`, ligne de la table) :

Après :

```cpp
#include "capture_rmt.h"
#include "config.h"
#include "fw_version.h"
```

insérer :

```cpp
#include "injection_regles.h"
```

Remplacer :

```cpp
  return {true, "ok", nullptr};
}

// ---------------------------------------------------------------------------
//  Commandes
// ---------------------------------------------------------------------------
```

par :

```cpp
  return {true, "ok", nullptr};
}

static const char kUsageInjection[] = "usage : injection [regle <nom> <valeur>]";

static void afficherInjection() {
  const ReglagesSonde &r = sondeReglages();
  const inj::Params &p = r.injection;
  Serial.printf("injection : etage %s (GPIO%u)\n", r.injMontee ? "declare monte" : "non monte", (unsigned)kPinInjection);
  Serial.printf("  bas_max_us %lu, total_max_us %lu, silence_min_us %lu, attente_max_ms %lu, delai_min_ms %lu, "
                "arme_max_s %lu, tol_us %lu\n",
                (unsigned long)p.basMaxUs, (unsigned long)p.totalMaxUs, (unsigned long)p.silenceMinUs,
                (unsigned long)p.attenteMaxMs, (unsigned long)p.delaiMinMs, (unsigned long)p.armeMaxS,
                (unsigned long)p.tolUs);
}

// Reglages d'injection changes : NVS, config reemise.
static bool appliquerInjection(const ReglagesSonde &r) { return sondeAppliquer(r); }

// injection regle <nom> <valeur> : verifie avant d'ecrire en NVS (bornes : inj::bornesParam).
static Resultat faireRegle(char *args) {
  static char msg[jsonp::kMsgMax + 1];
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  uint32_t v = 0;
  if (!*args || !lireU32(valeur, &v) || *reste)
    return {false, "usage", "usage : injection regle <nom> <valeur> ('injection' : noms et valeurs)"};
  ReglagesSonde r = sondeReglages();
  uint32_t lo = 0, hi = 0;
  switch (inj::reglerParam(&r.injection, args, v)) {
    case inj::Reglage::NomInconnu:
      snprintf(msg, sizeof(msg), "injection regle : parametre %s inconnu ('injection' : noms)", args);
      return {false, "usage", msg};
    case inj::Reglage::HorsBornes:
      inj::bornesParam(args, &lo, &hi);
      snprintf(msg, sizeof(msg), "injection regle : %s %lu hors bornes (%lu..%lu, et bas_max_us <= total_max_us)",
               args, (unsigned long)v, (unsigned long)lo, (unsigned long)hi);
      return {false, "usage", msg};
    case inj::Reglage::Ok: break;
  }
  if (!appliquerInjection(r)) return {false, "refuse", "injection regle : echec de l'ecriture en NVS"};
  return {true, "ok", nullptr};
}

// injection [regle <nom> <valeur>] ; args vide : rien a faire.
static Resultat faireInjection(char *args) {
  char *reste = splitWord(args);
  if (!strcmp(args, "regle")) return faireRegle(reste);
  if (!*args) return {true, "ok", nullptr};
  return {false, "usage", kUsageInjection};
}

// ---------------------------------------------------------------------------
//  Commandes
// ---------------------------------------------------------------------------
```

Après :

```cpp
  Serial.printf("wifi : identifiants enregistres, connexion a %s ('info' pour suivre)\n", args);
}

```

insérer :

```cpp
// injection [regle <nom> <valeur>] : USB seulement (liste blanche).
static void cmdInjection(char *args) {
  if (*args) {
    const Resultat r = faireInjection(args);
    if (r.msg) Serial.println(r.msg);
    if (!r.ok) return;
  }
  afficherInjection();
}

```

Après :

```cpp
  {"reboot", cmdReboot, "redemarrage"},
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log] : mode machine"},
  {"wifi", cmdWifi, "wifi <ssid> <mdp>"},
```

insérer :

```cpp
  {"injection", cmdInjection, "injection [regle <nom> <valeur>] : garde-fous de l'injection"},
```

- [ ] **Étape 6 : Charger l'avenant par `injection regle` (journal de l'étape 7)**

La préparation de l'étape 7 du journal vérifie déjà dans `config` que les valeurs de l'avenant sont chargées ; elle dit maintenant comment les charger. Une ligne de plus dans son tableau, les suivantes renumérotées.

Dans `docs/RECONNAISSANCE.md`, remplacer :

```markdown
| 3 | USB branché, `injection monte 1` envoyé | — | réponse `ok` | | |
| 4 | `config` : valeurs de l'avenant chargées | — | égales à celles de l'avenant | | sinon on corrige avant d'aller plus loin |
| 5 | USB débranché, sonde passée sur batterie | visuel | | | |
| 6 | contrôles de la règle 7 à jour (étape 2b refaite si le boîtier a été ouvert) ; Mac sur batterie, sans aucun autre câble | — | | | |
| 7 | hotte branchée ; `injection on` envoyé par `hotte_udp.py`, depuis un terminal, après la confirmation que Majid tape lui-même : « Majid devant la hotte » (jamais un script ni un agent) | — | injection armée | | |
| 8 | niveau bas obtenu par l'injection, pendant les premiers essais | — | jugement fonctionnel : la carte réagit (bip), et l'impulsion se retrouve sur la voie 2 de l'analyseur et dans `relu_us` de l'événement `injection` ; si mesuré (oscilloscope) : sous 0,8 V | | sinon : on arrête, `injection off`, hotte débranchée, puis 470 Ω remplacées par 220 Ω |

Instrument qui a donné le niveau de la ligne 8 : ☐ jugement fonctionnel
```

par :

```markdown
| 3 | USB branché, `injection monte 1` envoyé | — | réponse `ok` | | |
| 4 | charger les valeurs de l'avenant : `injection regle <nom> <valeur>` (USB, hotte débranchée), une commande par valeur | — | chacune acceptée | | refusée (hors bornes) : l'avenant sort de l'enveloppe de sécurité du firmware, on le revoit |
| 5 | `config` : valeurs de l'avenant chargées | — | égales à celles de l'avenant | | sinon on corrige avant d'aller plus loin |
| 6 | USB débranché, sonde passée sur batterie | visuel | | | |
| 7 | contrôles de la règle 7 à jour (étape 2b refaite si le boîtier a été ouvert) ; Mac sur batterie, sans aucun autre câble | — | | | |
| 8 | hotte branchée ; `injection on` envoyé par `hotte_udp.py`, depuis un terminal, après la confirmation que Majid tape lui-même : « Majid devant la hotte » (jamais un script ni un agent) | — | injection armée | | |
| 9 | niveau bas obtenu par l'injection, pendant les premiers essais | — | jugement fonctionnel : la carte réagit (bip), et l'impulsion se retrouve sur la voie 2 de l'analyseur et dans `relu_us` de l'événement `injection` ; si mesuré (oscilloscope) : sous 0,8 V | | sinon : on arrête, `injection off`, hotte débranchée, puis 470 Ω remplacées par 220 Ω |

Instrument qui a donné le niveau de la ligne 9 : ☐ jugement fonctionnel
```

Lancer : `grep -c "charger les valeurs de l'avenant : " docs/RECONNAISSANCE.md ; grep -c "niveau de la ligne 9" docs/RECONNAISSANCE.md`
Attendu : `1`, puis `1`.

- [ ] **Étape 7 : Compiler et relancer les tests**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|RAM:|Flash:|SUCCESS|FAILED"`
Attendu : aucune ligne `warning` ni `error` ; `RAM:   [===       ]  30.5% (used 99812 bytes from 327680 bytes)`, `Flash: [========  ]  81.7% ...`, puis `sonde          SUCCESS`.
Lancer : `sh tools/tests/test_hote.sh | tail -1` puis `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « tests hote : OK » ; « Ran 153 tests », « OK » (aucun test Python ne change dans cette tâche).

- [ ] **Étape 8 : Commit**

```bash
git add src/injection_regles.h src/injection_regles.cpp tools/tests/test_injection.cpp tools/tests/test_hote.sh src/reglages.h src/reglages.cpp src/cli.cpp docs/RECONNAISSANCE.md
git commit -m "Ajouter les garde-fous de l'injection et leurs reglages en NVS" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 23 : Injection sur GPIO7 (RMT en émission, silence, surveillance, arrêt sur collision), commandes `injection` et `injecte`, blocs `etat.injection` et `config.injection`

**Fichiers :**
- Modifier : `src/injection_regles.h`, `src/injection_regles.cpp` (ajout de `inj::Surveillance` et de `inj::kDepartMaxUs`)
- Modifier : `tools/tests/test_injection.cpp` (`testSurveillance`)
- Modifier : `tools/tests/test_json_check.py` (messages valides `config` et `etat/injection`, trois tests, classe `BornesInjection`)
- Modifier : `tools/json_check.py` (`INJ_BORNES`, `RESULTAT`, objet `config.injection`, bloc `etat/injection`, résultat `erreur`, cohérences)
- Modifier (contenu entier) : `src/bord.h`, `src/bord.cpp` (surveillance dans l'interruption)
- Créer : `src/injection.h`, `src/injection.cpp`
- Modifier : `src/json_out.h` (commentaire de `InjectionEv::resultat` : `erreur`)
- Modifier : `src/json_mode.h` (`jsonInjection`), `src/json_mode.cpp` (`config.injection`, `etatInjection`, `pushState`, `jsonInjection`)
- Modifier : `src/cli.cpp` (`injection monte|on|off`, `injecte`, aiguillage sans texte de `runLine`)
- Modifier : `src/main.cpp` (`injectionBegin` en fin de `setup()`, `injectionPoll` dans `loop()`)
- Modifier : `docs/PROTOCOLE-JSON.md` (sections 1, 3, 4.2, 4.3, 5, 5.1, 6, 6.2, 8.1, 8.3, 8.6, 9.4)
- Tester : `sh tools/tests/test_hote.sh`, `python3 -m unittest discover -s tools/tests`, `~/.platformio/penv/bin/pio run -e sonde`

**Interfaces :**
- Consomme :
  - tâche 22 : `inj::Params`, `inj::paramsValides`, `inj::Demande`, `inj::analyser`, `inj::Refus`, `inj::refusTexte`, `inj::Etat`, `inj::frontAnormal`, `inj::frontProgramme`, `inj::totalUs`, `inj::Sym`, `inj::kSymMax`, `inj::versSymboles` ; `ReglagesSonde::injMontee`, `ReglagesSonde::injection` ; dans `cli.cpp` : `afficherInjection`, `appliquerInjection`, `faireInjection`, `cmdInjection`.
  - `src/bord.h` et `src/bord.cpp` (tâche 10) : `bordBegin`, `bordDernierUs`, `bordBusHaut`, `bordFronts` ; `src/capture_rmt.h` : `const capt::Reglages &captureReglages();`.
  - `src/json_out.h` (tâche 8) : `struct jsonp::InjectionEv`, `void jsonp::injection(Writer &w, uint32_t n, uint32_t ms, const InjectionEv &e);`, `jsonp::Reply::SuiteInjection`, `jsonp::copyCmd`, `jsonp::kInjDurMax` (64), `jsonp::Item::EtatInjection`, `jsonp::kCmdTextMax`, `jsonp::kLineMax`.
  - `src/json_mode.h` (tâches 11 et 19) : `jsonOrigin`, `jsonSetOrigin`, `jsonMachine`, `jsonLog`, `jsonReply` ; dans `json_mode.cpp` : `sSinks`, `sOrigin`, `claim`, `send`, `push`, `bootUp`, `produce`, `pushState`, `config`.
  - `src/cli.cpp` (tâches 10, 11, 19) : `runLine` (branche `capture`/`seuils` sans texte), `faireCapture`, `faireSeuils`, `firstWordIs`, `afterWord`.
  - IDF 5.5 : `driver/rmt_tx.h` (`rmt_new_tx_channel`, `rmt_tx_register_event_callbacks`, `rmt_enable`, `rmt_transmit`, `rmt_disable`, `rmt_del_channel`, `rmt_new_copy_encoder`), `driver/gpio.h` (`gpio_get_level`, en IRAM ; `gpio_set_level`, `gpio_set_direction`).
- Produit :
  - `src/injection.h` : l'API du contrat, plus `inj::Refus injectionArmer(uint32_t nowMs);` et `void injectionDesarmer();`.
  - `src/bord.h` : `bordSurveiller(const inj::Demande *d, uint64_t t0Us, uint32_t tolUs)`, `bordFinSurveillance()`, `bordCollision()`, `uint16_t bordRelu(uint32_t *out, uint16_t cap)`, plus `bool bordActif();`.
  - `src/injection_regles.h` : `constexpr uint32_t kDepartMaxUs = 500;`, `class Surveillance { debut, front, fin, active, ancree, collision, relu }`.
  - `src/json_mode.h` : `void jsonInjection(const jsonp::InjectionEv &e);` (signature du contrat).
  - Protocole : `config.injection` (8 champs), `etat` bloc `injection` (`montee`, `armee`, `arme_reste_s`, `derniere`), événement `injection` (résultats `ok`, `collision`, `delai`, `erreur`), commandes `injection monte 0|1 | on | off | regle <nom> <valeur>` et `injecte durees <d1> <d2> ...`.

**Décisions de cette tâche (à connaître pour les suivantes) :**
- **Un canal RMT d'émission par injection.** Il est créé juste avant l'émission (GPIO7, 1 MHz, 48 symboles, niveau initial et de fin GPIO bas), puis désactivé et supprimé. Entre deux émissions, GPIO7 est une sortie simple à l'état bas, comme après la première instruction de `setup()`. `brocheBasse()` (`gpio_set_level` à 0, puis `gpio_set_direction` en sortie) rend la broche au GPIO simple : dans IDF 5.5, `gpio_output_enable` rebranche la sortie de la matrice sur le registre du GPIO (`gpio_hal_matrix_out_default`). L'encodeur copie est créé une fois (`injectionBegin`). Le groupe RMT du C6 a 2 canaux d'émission et 2 de réception : la capture (réception, tâche 10) continue pendant l'injection et rend la trame injectée comme toute trame.
- **Attente active pendant l'émission** (la durée de la demande, plus 0,5 ms et `tol_us` ; 20 ms de marge au-delà, résultat `erreur`). `rmt_disable` n'est pas permis en interruption (il passe par des files FreeRTOS non ISR) : l'interruption des fronts pose un drapeau, et la boucle d'attente arrête le canal quelques µs plus tard. La tâche `loop` reste bloquée 200 ms au plus (borne haute de `total_max_us`, tâche 22) ; l'USB, le Wi-Fi et la capture (interruptions) continuent.
- **Origine de la surveillance** : la latence de `rmt_transmit` n'est pas connue au µs près. Le premier front vers le bas dans les 500 µs qui suivent l'appel (`kDepartMaxUs`) fixe donc l'origine ; tout autre premier front est une collision.
- **Front masqué (ajout à la règle du contrat).** `frontAnormal` ne voit pas le cas où un autre émetteur commence à tirer la ligne pendant un palier bas de la sonde : le relâchement de la sonde disparaît, et le front suivant (la fin de l'impulsion de l'autre) va vers le niveau attendu. `Surveillance::front` juge donc anormal **tout** front loin d'un front programmé (`frontAnormal`, ou `!frontProgramme`) ; `frontAnormal` reste à la lettre du contrat. Sans cet ajout, la collision provoquée au banc (tâche 24) passerait inaperçue une fois sur trente environ.
- **Silence** : aucun front depuis `silence_min_us` (`bordDernierUs`) **et** bus haut (`bordBusHaut`) ; au-delà de `attente_max_ms`, résultat `delai`.
- **Délai minimal** : compté dès l'acceptation (une seule injection à la fois : une demande pendant l'attente du silence est refusée `delai minimal`), puis de nouveau à la fin de l'émission.
- **Résultat `erreur`** (ajout au contrat, qui ne prévoit que `ok`, `collision`, `delai`) : rien n'est émis ou l'émission n'a pas fini (RMT indisponible, interruption des fronts absente, injection désarmée ou étage démonté pendant l'attente). Un `log` `injection` en donne la cause. `json_check.py` l'accepte ; `docs/PROTOCOLE-JSON.md` le documente.
- **Événement vers les sessions** : `injection.cpp` pose l'origine de la commande (`jsonSetOrigin`) le temps de `jsonInjection`. Cette origine reçoit l'`id`, même hors mode machine si la commande en portait un (comme la `livraison` de la ScreenBar §7.3) ; les autres sessions en mode machine voient `id` null. Une commande de la console, sans `id` et en mode humain, reçoit une ligne de texte `injection : <resultat>, attente ... ; emises ... ; relues ...`.
- **`config.injection` est facultatif pour `json_check.py`** (toujours émis par ce firmware) : les captures des tâches 16 et 21, faites avant l'injection, restent conformes. `test_serie_enregistre.py` simule d'ailleurs une sonde de cette époque.
- **Commandes avec `id`** : `injection <valeurs>` et `injecte ...` répondent sans texte, comme `capture` et `seuils <valeurs>` ; `injecte` acceptée répond `accepte` avec `suite` = `injection`.

- [ ] **Étape 1 : Écrire les tests qui échouent**

Dans `tools/tests/test_injection.cpp` (`testSurveillance`, avant `testDistance`, et son appel) :

Après :

```cpp
  VERIF(versSymboles(Demande(), y, kSymMax) == 0);
}

```

insérer :

```cpp
// Surveillance d'une emission (tache 23) : fronts passes par l'interruption
// (temps esp_timer, niveau du bus), origine au premier front vers le bas.
static void testSurveillance() {
  const Params p;
  Demande d;
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  uint32_t r[kDurMax] = {};
  Surveillance s;
  VERIF(!s.active() && !s.collision() && s.relu(r, kDurMax) == 0);
  s.front(5000, false);  // inactive : ignore
  VERIF(!s.collision());

  // Emission normale : l'emission commence 30 us apres t0 (latence du RMT).
  const uint64_t t0 = 1000000, a = t0 + 30;
  s.debut(&d, t0, 20);
  VERIF(s.active() && !s.ancree());
  s.front(a, false);  // origine
  VERIF(s.ancree() && !s.collision());
  s.front(a + 752, true);
  s.front(a + 1500, false);
  s.front(a + 2251, true);  // puis 2250 us haut : pas de front, le bus reste relache
  VERIF(!s.collision());
  s.front(a + 4600, false);  // apres la fenetre (4500 + 20) : trafic d'un autre, ignore
  VERIF(!s.collision());
  VERIF(s.relu(r, kDurMax) == 3 && r[0] == 752 && r[1] == 748 && r[2] == 751);
  VERIF(s.relu(r, 2) == 2);
  s.fin();
  VERIF(!s.active());
  s.front(a + 4700, true);
  VERIF(!s.collision() && s.relu(r, kDurMax) == 3);

  // Collision : quelqu'un tire la ligne pendant un palier haut ; la suite est ignoree.
  s.debut(&d, t0, 20);
  s.front(t0 + 10, false);
  s.front(t0 + 10 + 752, true);
  VERIF(!s.collision());
  s.front(t0 + 10 + 1000, false);
  VERIF(s.collision());
  s.front(t0 + 10 + 1100, true);
  VERIF(s.relu(r, kDurMax) == 2 && r[0] == 752 && r[1] == 248);

  // Front masque : un autre emetteur tient la ligne basse pendant notre
  // relachement a 750 us, puis la relache a 1100 us. Ce front va vers le
  // niveau emis (haut), mais loin de tout front programme : collision.
  VERIF(!frontAnormal(d, 1100, true, 20));  // a lui seul, frontAnormal ne le voit pas
  s.debut(&d, t0, 20);
  s.front(t0 + 10, false);
  s.front(t0 + 10 + 1100, true);
  VERIF(s.collision());
  VERIF(s.relu(r, kDurMax) == 1 && r[0] == 1100);

  // Premier front vers le haut (le bus etait tenu bas par un autre) : collision.
  s.debut(&d, t0, 20);
  VERIF(!s.collision());
  s.front(t0 + 40, true);
  VERIF(s.collision() && !s.ancree());
  // Premier front trop tard apres t0 (au-dela de kDepartMaxUs) : collision.
  s.debut(&d, t0, 20);
  s.front(t0 + kDepartMaxUs + 1, false);
  VERIF(s.collision());
  s.debut(&d, t0, 20);
  s.front(t0 + kDepartMaxUs, false);
  VERIF(!s.collision() && s.ancree());

  // Nombre impair de durees : le relachement final est relu.
  VERIF(analyser("durees 1500 750 750", p, &d) == Refus::Aucun);
  s.debut(&d, t0, 20);
  s.front(t0 + 20, false);
  s.front(t0 + 20 + 1502, true);
  s.front(t0 + 20 + 2250, false);
  s.front(t0 + 20 + 3004, true);
  VERIF(!s.collision());
  VERIF(s.relu(r, kDurMax) == 3 && r[0] == 1502 && r[1] == 748 && r[2] == 754);

  // debut() remet tout a zero.
  s.debut(&d, t0, 20);
  VERIF(s.active() && !s.ancree() && !s.collision() && s.relu(r, kDurMax) == 0);
}

```

Après :

```cpp
  testEtat();
  testNiveauEtFront();
  testSymboles();
```

insérer :

```cpp
  testSurveillance();
```

Dans `tools/tests/test_json_check.py` (import de `re`, messages valides, trois tests avant `test_hello`, classe `BornesInjection` avant `Fichiers`) :

Après :

```python
import io
import json
import os
```

insérer :

```python
import re
```

Remplacer :

```python
        appareil="hotte", caps=["sonde", "injection", "trames", "log"]),
    ("config", None): msg(
        "config", capture={"gpio": 6, "resol_hz": 1000000, "filtre_us": 1, "silence_us": 5000, "mode": "tout",
                           "inverse": True}),
    ("etat", "bus"): msg("etat", "bus", boot="3FA2C901", up_s=12, repos="haut", fronts=1520, derniere_ms=3,
                         receptions_s=17),
    ("etat", "capture"): msg("etat", "capture", boot="3FA2C901", up_s=12, active=True, mode="tout", debord=0,
                             rep_en_cours=0),
    ("etat", "sys"): msg(
        "etat", "sys", boot="3FA2C901", up_s=12,
        sys={"heap": 250000, "heap_min": 240000, "heap_bloc": 110000, "pile_boucle": 5200, "boucle_max_ms": 2,
```

par :

```python
        appareil="hotte", caps=["sonde", "injection", "trames", "log"]),
    ("config", None): msg(
        "config", capture={"gpio": 6, "resol_hz": 1000000, "filtre_us": 1, "silence_us": 5000, "mode": "tout",
                           "inverse": True},
        injection={"gpio": 7, "bas_max_us": 3000, "total_max_us": 200000, "silence_min_us": 20000,
                   "attente_max_ms": 1000, "delai_min_ms": 3000, "arme_max_s": 600, "tol_us": 20}),
    ("etat", "bus"): msg("etat", "bus", boot="3FA2C901", up_s=12, repos="haut", fronts=1520, derniere_ms=3,
                         receptions_s=17),
    ("etat", "capture"): msg("etat", "capture", boot="3FA2C901", up_s=12, active=True, mode="tout", debord=0,
                             rep_en_cours=0),
    ("etat", "injection"): msg("etat", "injection", boot="3FA2C901", up_s=12, montee=True, armee=True,
                               arme_reste_s=587, derniere={"id": 42, "resultat": "ok"}),
    ("etat", "sys"): msg(
        "etat", "sys", boot="3FA2C901", up_s=12,
        sys={"heap": 250000, "heap_min": 240000, "heap_bloc": 110000, "pile_boucle": 5200, "boucle_max_ms": 2,
```

Après :

```python
        self.assertTrue(self.modifie(cle, capture=dict(capture, filtre_us=4))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, mode="rafale"))[0])

```

insérer :

```python
    def test_config_injection(self):
        cle = ("config", None)
        inj = VALIDES[cle]["injection"]
        self.assertEqual(self.modifie(cle, sans_injection=None), ([], []))  # captures d'avant la tache 23
        self.assertTrue(self.modifie(cle, injection=dict(inj, tol_us=0))[0])
        self.assertTrue(self.modifie(cle, injection=dict(inj, arme_max_s=601))[0])
        self.assertTrue(self.modifie(cle, injection=dict(inj, delai_min_ms=999))[0])
        self.assertTrue(self.modifie(cle, injection=dict(inj, bas_max_us=5000, total_max_us=4000))[0])
        self.assertTrue(self.modifie(cle, injection=dict(inj, total_max_us=200001))[0])  # borne haute : le defaut
        self.assertEqual(self.modifie(cle, injection=dict(inj, bas_max_us=20000, total_max_us=200000))[0], [])

    def test_etat_injection(self):
        cle = ("etat", "injection")
        self.assertEqual(self.modifie(cle, armee=False, arme_reste_s=0, derniere=None), ([], []))
        self.assertEqual(self.modifie(cle, derniere={"id": None, "resultat": "delai"}), ([], []))
        self.assertEqual(self.modifie(cle, derniere={"id": 7, "resultat": "erreur"}), ([], []))
        self.assertTrue(self.modifie(cle, montee=False)[0])       # armee sans etage monte
        self.assertTrue(self.modifie(cle, armee=False)[0])        # desarmee avec un reste
        self.assertTrue(self.modifie(cle, arme_reste_s=0)[0])     # armee sans reste
        self.assertTrue(self.modifie(cle, arme_reste_s=601)[0])   # 10 min au plus
        self.assertTrue(self.modifie(cle, derniere={"id": 42, "resultat": "annulee"})[0])
        self.assertTrue(self.modifie(cle, sans_derniere=None)[0])

    def test_injection_evenement(self):
        cle = ("injection", None)
        self.assertEqual(self.modifie(cle, resultat="erreur", relu_us=[]), ([], []))
        self.assertEqual(self.modifie(cle, resultat="collision", id=None), ([], []))
        self.assertTrue(self.modifie(cle, resultat="annulee")[0])
        self.assertTrue(self.modifie(cle, dur_us=[1] * 65)[0])
        self.assertTrue(self.modifie(cle, relu_us=[1] * 65)[0])
        self.assertTrue(self.modifie(cle, dur_us=[])[0])          # une demande a au moins une duree
        self.assertTrue(self.modifie(cle, id=0)[0])               # sans id : null

```

Après :

```python
        self.assertEqual(verifier(obj)[0], [])
        _, errs, _ = json_check.check_line(b"\x1e" + json.dumps(VALIDES[("fin", None)]).encode())
        self.assertTrue(any("non compacte" in e for e in errs), errs)
```

insérer :

```python


class BornesInjection(unittest.TestCase):
    def test_memes_bornes_que_le_firmware(self):
        # kChamps de src/injection_regles.cpp : nom, champ, borne basse, borne haute.
        with open(os.path.join(ICI, "..", "..", "src", "injection_regles.cpp"), encoding="utf-8") as f:
            texte = f.read()
        c = {m[0]: (int(m[1]), int(m[2])) for m in re.findall(r'\{"(\w+)", &Params::\w+, (\d+), (\d+)\}', texte)}
        self.assertEqual(len(c), 7)
        self.assertEqual(c, json_check.INJ_BORNES)
```

- [ ] **Étape 2 : Lancer les tests et vérifier qu'ils échouent**

Lancer : `sh tools/tests/test_hote.sh`
Attendu : ÉCHEC à la compilation de `test_injection`, avec « tools/tests/test_injection.cpp:237:3: error: use of undeclared identifier 'Surveillance'; did you mean 'testSurveillance'? ».
Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | grep -E "^(FAIL|ERROR):|^Ran|^FAILED"`
Attendu :
```
ERROR: test_memes_bornes_que_le_firmware (test_json_check.BornesInjection.test_memes_bornes_que_le_firmware)
FAIL: test_config_injection (test_json_check.Schemas.test_config_injection)
FAIL: test_etat_injection (test_json_check.Schemas.test_etat_injection)
FAIL: test_injection_evenement (test_json_check.Schemas.test_injection_evenement)
FAIL: test_messages_valides (test_json_check.Schemas.test_messages_valides)
Ran 157 tests in ...
FAILED (failures=4, errors=1)
```

- [ ] **Étape 3 : Écrire la surveillance (pure)**

Dans `src/injection_regles.h` (`Surveillance`, avant la fin de l'espace de noms) :

Après :

```cpp
constexpr size_t kSymMax = 48;  // une memoire de canal RMT ; le pire cas des bornes en prend 35
size_t versSymboles(const Demande &d, Sym *out, size_t cap);

```

insérer :

```cpp
// Ajout (tache 23) : surveillance d'une emission. L'interruption des fronts
// de la ligne (bord.cpp) passe chaque front a front(), sous sa section
// critique. Le premier front, vers le bas, dans les kDepartMaxUs qui suivent
// t0Us (heure d'appel de rmt_transmit), fixe l'origine : le debut reel de
// l'emission. Tout autre premier front est une collision (un autre emetteur).
// Ensuite, chaque front jusqu'a la fin de la demande plus tolUs est juge :
// loin de tout front programme, il est anormal, qu'il aille vers un autre
// niveau que l'emis (frontAnormal) ou vers le niveau emis (le front
// precedent manquait : masque par un autre emetteur qui tenait la ligne).
// Les fronts plus tardifs sont ignores (trafic d'un autre emetteur apres
// l'emission). relu : durees entre fronts successifs depuis
// l'origine (n - 1 pour un nombre pair de durees : le dernier palier haut n'a
// pas de front de fin ; n pour un nombre impair). Apres une collision, plus
// rien n'est compte.
constexpr uint32_t kDepartMaxUs = 500;
class Surveillance {
 public:
  void debut(const Demande *d, uint64_t t0Us, uint32_t tolUs);
  void front(uint64_t tUs, bool busHaut);
  void fin() { actif_ = false; }
  bool active() const { return actif_; }
  bool ancree() const { return ancre_; }
  bool collision() const { return coll_; }
  uint16_t relu(uint32_t *out, uint16_t cap) const;

 private:
  const Demande *d_ = nullptr;
  bool actif_ = false, ancre_ = false, coll_ = false;
  uint64_t t0_ = 0, prec_ = 0;
  uint32_t tol_ = 0, finRel_ = 0;
  uint16_t n_ = 0;
  uint32_t relu_[kDurMax] = {};
};

```

Dans `src/injection_regles.cpp` (méthodes de `Surveillance`, avant la fin de l'espace de noms) :

Après :

```cpp
  return k / 2;
}

```

insérer :

```cpp
void Surveillance::debut(const Demande *d, uint64_t t0Us, uint32_t tolUs) {
  d_ = d;
  t0_ = prec_ = t0Us;
  tol_ = tolUs;
  finRel_ = totalUs(*d) + tolUs;
  n_ = 0;
  ancre_ = coll_ = false;
  actif_ = true;
}

void IRAM_ATTR Surveillance::front(uint64_t tUs, bool busHaut) {
  if (!actif_ || coll_) return;
  if (!ancre_) {
    if (!busHaut && tUs >= t0_ && tUs - t0_ <= kDepartMaxUs) {
      ancre_ = true;
      t0_ = prec_ = tUs;
    } else {
      coll_ = true;  // premier front inattendu : un autre emetteur
    }
    return;
  }
  if (tUs < t0_ || tUs - t0_ > finRel_) return;  // apres la fenetre : trafic d'un autre
  if (n_ < kDurMax) relu_[n_++] = (uint32_t)(tUs - prec_);
  prec_ = tUs;
  const uint32_t rel = (uint32_t)(tUs - t0_);
  // Vers un autre niveau que l'emis (frontAnormal), ou vers le niveau emis
  // loin de tout front programme : le front precedent a ete masque.
  if (frontAnormal(*d_, rel, busHaut, tol_) || !frontProgramme(*d_, rel, tol_)) coll_ = true;
}

uint16_t Surveillance::relu(uint32_t *out, uint16_t cap) const {
  const uint16_t n = n_ < cap ? n_ : cap;
  for (uint16_t i = 0; i < n; i++) out[i] = relu_[i];
  return n;
}

```

Lancer : `sh tools/tests/test_hote.sh | tail -2`
Attendu : « test_injection : 180 verifications, 0 echecs », « tests hote : OK ».

- [ ] **Étape 4 : Étendre le vérificateur**

Dans `tools/json_check.py` :

Remplacer :

```python
    etat, compteurs, reseau ; entiers seulement (t_us jusqu'a 2^53 - 1) ;
  - contenu : champs obligatoires, types, bornes, enumerations, tailles des
    chaines, coherences simples (etape et code d'une reponse, reglages de
    capture, trame vide...) ; un champ inconnu est un avertissement ;
  - continuite : trous de n (pertes, avec leur taux), n qui recule.

Usage :
```

par :

```python
    etat, compteurs, reseau ; entiers seulement (t_us jusqu'a 2^53 - 1) ;
  - contenu : champs obligatoires, types, bornes, enumerations, tailles des
    chaines, coherences simples (etape et code d'une reponse, reglages de
    capture et d'injection, etat de l'injection, trame vide...) ; un champ
    inconnu est un avertissement ;
  - continuite : trous de n (pertes, avec leur taux), n qui recule.

Usage :
```

Après :

```python
SILENCE_MAX_TICKS = 32767
DUR_MAX = 110       # durees par ligne trame
INJ_DUR_MAX = 64    # durees par injection
```

insérer :

```python
# Bornes des valeurs d'injection (kChamps de src/injection_regles.cpp ; test_json_check le verifie).
INJ_BORNES = {
    "bas_max_us": (10, 20000),
    "total_max_us": (100, 200000),
    "silence_min_us": (1000, 1000000),
    "attente_max_ms": (10, 10000),
    "delai_min_ms": (1000, 60000),
    "arme_max_s": (10, 600),
    "tol_us": (1, 500),
}
```

Après :

```python
    "mise_sous_tension", "broche", "logiciel", "panique", "chien_int", "chien_tache", "chien",
    "baisse_tension", "usb", "inconnue",
)
```

insérer :

```python
RESULTAT = Enum("ok", "collision", "delai", "erreur")  # evenement injection, etat.injection.derniere
```

Après :

```python
                    "inverse": BOOL,
                }
            ),
```

insérer :

```python
            # Absent des captures faites avant l'injection (taches 16 et 21) : facultatif.
            "injection": Opt(Obj(dict({"gpio": U8}, **{k: U32 for k in INJ_BORNES}))),
```

Après :

```python
            "mode": MODE,
            "debord": U32,
            "rep_en_cours": U32,
```

insérer :

```python
        }
    ),
    ("etat", "injection"): Obj(
        {
            "boot": BOOT,
            "up_s": U32,
            "montee": BOOL,
            "armee": BOOL,
            "arme_reste_s": Int(0, 600),
            "derniere": Null(Obj({"id": Null(Int(1, 999999999)), "resultat": RESULTAT})),
```

Remplacer :

```python
        {
            "id": Null(Int(1, 999999999)),
            "cmd": Str(40),
            "resultat": Enum("ok", "collision", "delai"),
            "niv0": NIVEAU,
            "dur_us": Arr(Int(1, U32_MAX), INJ_DUR_MAX),
            "attente_us": U32,
```

par :

```python
        {
            "id": Null(Int(1, 999999999)),
            "cmd": Str(40),
            "resultat": RESULTAT,
            "niv0": NIVEAU,
            "dur_us": Arr(Int(1, U32_MAX), INJ_DUR_MAX),
            "attente_us": U32,
```

Après :

```python
            elif isinstance(hz, int) and isinstance(sil, int) and not isinstance(sil, bool):
                if sil < SILENCE_MIN_US or sil * hz > SILENCE_MAX_TICKS * 1000000:
                    errs.append(f"config : silence_us {sil} hors bornes a {hz} Hz")
```

insérer :

```python
        i = obj.get("injection")
        if isinstance(i, dict):
            for k, (lo, hi) in INJ_BORNES.items():
                v = i.get(k)
                if isinstance(v, int) and not isinstance(v, bool) and not lo <= v <= hi:
                    errs.append(f"config : injection.{k} {v} hors bornes ({lo}..{hi})")
            b, t = i.get("bas_max_us"), i.get("total_max_us")
            if isinstance(b, int) and isinstance(t, int) and b > t:
                errs.append(f"config : injection.bas_max_us {b} au-dela de total_max_us {t}")
    elif t == "etat" and obj.get("bloc") == "injection":
        armee, reste = obj.get("armee"), obj.get("arme_reste_s")
        if armee is True and obj.get("montee") is not True:
            errs.append("etat/injection : armee sans etage monte")
        if armee is True and reste == 0:
            errs.append("etat/injection : armee avec arme_reste_s 0")
        if armee is False and isinstance(reste, int) and reste:
            errs.append(f"etat/injection : desarmee avec arme_reste_s {reste}")
    elif t == "injection":
        if obj.get("dur_us") == []:
            errs.append("injection : dur_us vide (une demande a au moins une duree)")
```

Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « Ran 157 tests », « OK ».

- [ ] **Étape 5a : Surveillance dans l'interruption des fronts (`bord`)**

Remplacer tout le contenu de `src/bord.h` (surveillance d'une injection) par :

```cpp
#pragma once
#include <stdint.h>

#include "injection_regles.h"

bool bordBegin(uint8_t pin);   // service ISR d'IDF (ESP_ERR_INVALID_STATE tolere), deux fronts
uint64_t bordDernierUs();      // esp_timer du dernier front vu
bool bordBusHaut();            // niveau du bus maintenant (inversion de l'etage comprise)
uint32_t bordFronts();         // fronts vus depuis le demarrage

// Tache 23 : surveillance d'une injection. L'interruption passe chaque front,
// avec le niveau du bus lu sur la broche, a une inj::Surveillance
// (injection_regles.h) : origine au premier front vers le bas qui suit t0Us,
// collision sur un front anormal, durees relues. d doit rester valide jusqu'a
// bordFinSurveillance().
void bordSurveiller(const inj::Demande *d, uint64_t t0Us, uint32_t tolUs);
void bordFinSurveillance();
bool bordCollision();                             // front anormal vu depuis bordSurveiller
uint16_t bordRelu(uint32_t *out, uint16_t cap);   // durees relues ; rend leur nombre
bool bordActif();                                 // ajout : interruption installee (bordBegin reussi)
```

Remplacer tout le contenu de `src/bord.cpp` (niveau lu et surveillance dans l'interruption) par :

```cpp
// ===========================================================================
//  Fronts de la ligne D (spec 8.4) : interruption GPIO sur les deux fronts de
//  la broche d'ecoute, par le service ISR d'IDF (pas d'attachInterrupt
//  d'Arduino). Elle tient l'heure du dernier front (silence avant une
//  injection) et le nombre de fronts ; elle coexiste avec le canal RMT sur la
//  meme broche. Le niveau du bus se lit sur la broche, remis dans le sens du
//  bus selon l'etage (captureReglages().inverse).
//
//  Pendant une injection (tache 23), elle lit aussi le niveau a chaque front
//  et le passe a la surveillance (inj::Surveillance) : collision, durees
//  relues. Tout se passe sous la section critique sMux, en IRAM
//  (gpio_get_level, esp_timer_get_time et les fonctions de surveillance y
//  sont) ; la tache loop relit les resultats sous la meme section.
// ===========================================================================
#include "bord.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_timer.h>

#include "capture_rmt.h"
#include "config.h"

namespace {

portMUX_TYPE sMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t sPin = kPinEcoute;
bool sActif = false;
uint64_t sDernierUs = 0;  // sous sMux : deux mots de 32 bits
volatile uint32_t sFronts = 0;
inj::Surveillance sSurv;  // sous sMux
bool sInverse = true;     // etage d'ecoute inverseur (fixe par bordSurveiller)

void IRAM_ATTR surFront(void *) {
  const uint64_t t = esp_timer_get_time();
  const bool gpioHaut = gpio_get_level((gpio_num_t)sPin) != 0;
  portENTER_CRITICAL_ISR(&sMux);
  sDernierUs = t;
  sFronts = sFronts + 1;
  if (sSurv.active()) sSurv.front(t, sInverse ? !gpioHaut : gpioHaut);
  portEXIT_CRITICAL_ISR(&sMux);
}

}  // namespace

bool bordBegin(uint8_t pin) {
  gpio_config_t c = {};
  c.pin_bit_mask = 1ULL << pin;
  c.mode = GPIO_MODE_INPUT;
  c.pull_up_en = GPIO_PULLUP_ENABLE;  // comme le pilote RMT : sans effet avec le 10 k de collecteur
  c.pull_down_en = GPIO_PULLDOWN_DISABLE;
  c.intr_type = GPIO_INTR_ANYEDGE;
  esp_err_t err = gpio_config(&c);
  if (err == ESP_OK) {
    err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) err = ESP_OK;  // deja installe (Arduino ou un autre module)
  }
  if (err == ESP_OK) err = gpio_isr_handler_add((gpio_num_t)pin, surFront, nullptr);
  if (err != ESP_OK) {
    log_e("[bord] GPIO%u : %s", (unsigned)pin, esp_err_to_name(err));
    return false;
  }
  sPin = pin;
  sActif = true;
  return true;
}

uint64_t bordDernierUs() {
  portENTER_CRITICAL(&sMux);
  const uint64_t t = sDernierUs;
  portEXIT_CRITICAL(&sMux);
  return t;
}

bool bordBusHaut() {
  const bool gpioHaut = gpio_get_level((gpio_num_t)sPin) != 0;
  return captureReglages().inverse ? !gpioHaut : gpioHaut;
}

uint32_t bordFronts() { return sFronts; }

bool bordActif() { return sActif; }

void bordSurveiller(const inj::Demande *d, uint64_t t0Us, uint32_t tolUs) {
  const bool inverse = captureReglages().inverse;
  portENTER_CRITICAL(&sMux);
  sInverse = inverse;
  sSurv.debut(d, t0Us, tolUs);
  portEXIT_CRITICAL(&sMux);
}

void bordFinSurveillance() {
  portENTER_CRITICAL(&sMux);
  sSurv.fin();
  portEXIT_CRITICAL(&sMux);
}

bool bordCollision() {
  portENTER_CRITICAL(&sMux);
  const bool c = sSurv.collision();
  portEXIT_CRITICAL(&sMux);
  return c;
}

uint16_t bordRelu(uint32_t *out, uint16_t cap) {
  portENTER_CRITICAL(&sMux);
  const uint16_t n = sSurv.relu(out, cap);
  portEXIT_CRITICAL(&sMux);
  return n;
}
```

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|SUCCESS|FAILED"`
Attendu : aucune ligne `warning` ni `error` ; `sonde          SUCCESS` (rien n'appelle encore la surveillance).

- [ ] **Étape 5b : Module d'injection (`injection.{h,cpp}`)**

`injection.cpp` rend chaque injection par `jsonInjection` : sa déclaration vient ici, avec le module, et sa définition à l'étape 5c. Le résultat `erreur` (décisions ci-dessus) entre aussi dans le commentaire de `jsonp::InjectionEv`.

Dans `src/json_out.h` :

Remplacer :

```cpp
  const char *resultat = "ok";  // ok, collision, delai
```

par :

```cpp
  const char *resultat = "ok";  // ok, collision, delai, erreur
```

Dans `src/json_mode.h` :

Remplacer :

```cpp
//  Mode machine : protocole compagnon v1, profil hotte (docs/PROTOCOLE-JSON.md)
//
//  Session ('json 1', bail, 'json 0'), lignes periodiques (etat, compteurs,
//  battement) par la file des periodiques, evenements trame et log, reponses
//  aux lignes portant un id.
//
//  Une session par transport (origine : jsonp::kUsb, puis une par session
//  reseau etablie, net_udp_wifi.cpp) : ses reglages, son n, sa file, ses
```

par :

```cpp
//  Mode machine : protocole compagnon v1, profil hotte (docs/PROTOCOLE-JSON.md)
//
//  Session ('json 1', bail, 'json 0'), lignes periodiques (etat, compteurs,
//  battement) par la file des periodiques, evenements trame, injection et log,
//  reponses aux lignes portant un id.
//
//  Une session par transport (origine : jsonp::kUsb, puis une par session
//  reseau etablie, net_udp_wifi.cpp) : ses reglages, son n, sa file, ses
```

Après :

```cpp
// 100 lignes par seconde et par session au plus ; les parties d'une reception
// sont produites ou sautees ensemble ; la suivante produite porte 'sautes'.
void jsonTrame(const capt::Partie &p, bool hasRep, uint32_t rep);
```

insérer :

```cpp
// Fin d'une injection (tache 23, injection.cpp) : ligne injection vers chaque
// session en mode machine, sans plafond. L'id n'a de sens que dans la session
// qui a envoye la commande : l'origine courante (jsonOrigin(), que
// injection.cpp pose sur celle de la commande) le recoit, meme hors mode
// machine si la commande portait un id ; les autres voient id null.
void jsonInjection(const jsonp::InjectionEv &e);
```

Créer `src/injection.h` :

```cpp
#pragma once
// ===========================================================================
//  Injection sur la ligne D, voie 1 (docs/SPEC-RECONNAISSANCE.md 8.4)
//
//  RMT en emission sur GPIO7 (etage d'injection, docs/WIRING.md section 6 :
//  GPIO7 haut = bus tire bas), attente du silence, surveillance des fronts,
//  arret sur collision, evenement 'injection'. Garde-fous purs :
//  injection_regles.h. Tout se passe dans la tache loop.
// ===========================================================================
#include "injection_regles.h"
#include "json_out.h"

// Parametres de l'avenant et etage declare monte (NVS, reglages.h). Premier
// appel : encodeur RMT cree (false : injection impossible). Appels suivants
// ('injection monte', 'injection regle') : valeurs mises a jour ; etage non
// monte : injection desarmee. Une demande deja acceptee garde ses valeurs.
bool injectionBegin(const inj::Params &p, bool montee);
// Demande analysee (inj::analyser) : Refus::Aucun si elle est acceptee (elle
// part des que le bus se tait, evenement 'injection' ensuite, vers l'origine
// jsonOrigin() de la commande avec son id) ; sinon le refus (non montee, non
// armee, delai minimal ; une injection deja en attente compte comme delai).
inj::Refus injectionDemander(const inj::Demande &d, uint32_t id, const char *cmd, uint32_t nowMs);
void injectionPoll();                      // attente de silence, emission, surveillance, evenement
void injectionJson(jsonp::Writer &w, uint32_t nowMs);   // bloc etat.injection
inj::Etat &injectionEtat();
const inj::Params &injectionParams();
// Ajouts : armement ('injection on' : refuse si l'etage n'est pas declare
// monte) et desarmement ('injection off') ; le desarmement seul apres
// arme_max_s est annonce en log 'injection'.
inj::Refus injectionArmer(uint32_t nowMs);
void injectionDesarmer();
```

Créer `src/injection.cpp` :

```cpp
// ===========================================================================
//  Injection sur la ligne D, voie 1 (docs/SPEC-RECONNAISSANCE.md 8.4)
//
//  Etage (docs/WIRING.md section 6) : GPIO7 -> 4,7k -> base de Q2, collecteur
//  par 470 ohms sur D. GPIO7 haut = bus tire bas ; GPIO7 bas = bus relache
//  (drain ouvert : jamais de 3,3 V pousse sur D). Hors emission, GPIO7 est une
//  sortie simple a l'etat bas (premiere instruction de setup(), puis
//  brocheBasse() apres chaque emission).
//
//  Une demande acceptee (injectionDemander) :
//   1. attend le silence : aucun front depuis silence_min_us (bordDernierUs)
//      et bus haut. Au-dela de attente_max_ms : resultat 'delai', rien n'est
//      emis ;
//   2. part en une seule transaction du pilote RMT d'IDF (driver/rmt_tx.h) :
//      canal cree pour cette emission sur GPIO7, 1 MHz, encodeur copie,
//      niveau initial et de fin GPIO bas. La surveillance des fronts
//      (bord.cpp) est armee juste avant rmt_transmit ;
//   3. la tache loop attend activement la fin de la transaction (la duree de
//      la demande : 200 ms au plus, borne haute de total_max_us), ou
//      une collision signalee par l'interruption des fronts : rmt_disable
//      arrete alors l'emission tout de suite (arret asynchrone du C6) ;
//   4. canal supprime, GPIO7 rendue a une sortie simple a l'etat bas, delai
//      minimal compte depuis la fin de l'emission, evenement 'injection'
//      (ok, collision, delai, erreur) avec les durees relues ; en texte si la
//      commande venait de la console en mode humain.
//  Une seule demande a la fois. L'interruption ne fait que poser des
//  drapeaux ; seule la tache loop produit des lignes (spec 8.7).
// ===========================================================================
#include "injection.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rmt_tx.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include <string.h>

#include "bord.h"
#include "config.h"
#include "json_mode.h"

static_assert(jsonp::kInjDurMax == inj::kDurMax, "injection.dur_us : autant de durees que inj::kDurMax");

namespace {

constexpr uint32_t kMargeUs = 20000;  // transaction pas finie 20 ms apres la fin prevue : abandonnee (jamais vu)

enum class Resultat : uint8_t { Ok, Collision, Delai, Erreur };

const char *texte(Resultat r) {
  switch (r) {
    case Resultat::Ok: return "ok";
    case Resultat::Collision: return "collision";
    case Resultat::Delai: return "delai";
    case Resultat::Erreur: return "erreur";
  }
  return "erreur";
}

// Demande acceptee, en attente du silence.
struct EnAttente {
  bool active = false;
  inj::Demande d;
  inj::Params p;  // valeurs au moment de l'acceptation
  uint32_t id = 0;
  uint8_t origine = jsonp::kUsb;
  char cmd[jsonp::kCmdTextMax + 1] = {};
  uint64_t accepteUs = 0;
};

inj::Params sParams;
inj::Etat sEtat;
bool sArmee = false;  // armee au tour precedent : annonce du desarmement seul
EnAttente sE;
rmt_encoder_handle_t sCopie = nullptr;
rmt_symbol_word_t sSym[inj::kSymMax];  // lu par le pilote pendant l'emission
size_t sNSym = 0;
volatile bool sFini = false;  // rappel on_trans_done
uint32_t sRelu[inj::kDurMax];
bool sDerniere = false;  // etat.injection.derniere
uint32_t sDerniereId = 0;
Resultat sDerniereRes = Resultat::Ok;
char sTexte[jsonp::kLineMax];

bool IRAM_ATTR finEmission(rmt_channel_handle_t, const rmt_tx_done_event_data_t *, void *) {
  sFini = true;
  return false;
}

// Annonce : log 'injection' en mode 'json log 1', sinon texte sur l'USB.
void annonce(const char *txt) {
  if (!jsonLog("injection", "notice", txt)) Serial.println(txt);
}

// GPIO7 : sortie simple a l'etat bas (bus relache, Q2 bloque).
void brocheBasse() {
  gpio_set_level((gpio_num_t)kPinInjection, 0);
  gpio_set_direction((gpio_num_t)kPinInjection, GPIO_MODE_OUTPUT);
}

// Emet sE.d (tache loop, attente active). *nRelu : durees relues.
Resultat emettre(uint16_t *nRelu) {
  *nRelu = 0;
  rmt_channel_handle_t canal = nullptr;
  rmt_tx_channel_config_t c = {};
  c.gpio_num = (gpio_num_t)kPinInjection;
  c.clk_src = RMT_CLK_SRC_DEFAULT;
  c.resolution_hz = 1000000;  // 1 tick = 1 us
  c.mem_block_symbols = inj::kSymMax;
  c.trans_queue_depth = 1;
  c.flags.init_level = 0;  // bus relache
  esp_err_t err = rmt_new_tx_channel(&c, &canal);
  if (err == ESP_OK) {
    rmt_tx_event_callbacks_t cb = {};
    cb.on_trans_done = finEmission;
    err = rmt_tx_register_event_callbacks(canal, &cb, nullptr);
  }
  if (err == ESP_OK) err = rmt_enable(canal);
  if (err != ESP_OK) {
    if (canal) rmt_del_channel(canal);
    brocheBasse();
    snprintf(sTexte, sizeof(sTexte), "[injection] canal RMT : %s, rien n'est emis", esp_err_to_name(err));
    annonce(sTexte);
    return Resultat::Erreur;
  }
  rmt_transmit_config_t t = {};
  t.flags.eot_level = 0;  // fin : bus relache
  sFini = false;
  const uint64_t t0 = esp_timer_get_time();
  const uint64_t finFenetre = t0 + inj::kDepartMaxUs + inj::totalUs(sE.d) + sE.p.tolUs;
  bordSurveiller(&sE.d, t0, sE.p.tolUs);
  Resultat res = Resultat::Ok;
  bool arrete = false;
  err = rmt_transmit(canal, sCopie, sSym, sNSym * sizeof(rmt_symbol_word_t), &t);
  if (err != ESP_OK) {
    res = Resultat::Erreur;
  } else {
    for (;;) {
      if (bordCollision()) {
        rmt_disable(canal);  // arret immediat de l'emission
        arrete = true;
        res = Resultat::Collision;
        break;
      }
      const uint64_t maintenant = esp_timer_get_time();
      if (sFini && maintenant >= finFenetre) break;  // fin, plus la tolerance du dernier front
      if (maintenant >= finFenetre + kMargeUs) {
        res = Resultat::Erreur;
        break;
      }
    }
  }
  bordFinSurveillance();
  *nRelu = bordRelu(sRelu, inj::kDurMax);
  if (!arrete) rmt_disable(canal);
  rmt_del_channel(canal);
  brocheBasse();
  if (res == Resultat::Erreur) {
    snprintf(sTexte, sizeof(sTexte), "[injection] emission : %s",
             err != ESP_OK ? esp_err_to_name(err) : "transaction jamais finie, arretee");
    annonce(sTexte);
  }
  return res;
}

// Fin d'une demande : evenement 'injection', etat, texte.
void terminer(Resultat r, uint32_t attenteUs, uint16_t nRelu) {
  sE.active = false;
  jsonp::InjectionEv e;
  e.id = sE.id;
  e.cmd = sE.cmd;
  e.resultat = texte(r);
  e.niv0Haut = false;  // 'injecte durees' : la premiere duree est basse
  e.dur = sE.d.dur;
  e.n = sE.d.n;
  e.attenteUs = attenteUs;
  e.relu = sRelu;
  e.nRelu = nRelu;
  // L'evenement est la suite de la commande : son id va a la session d'origine.
  const uint8_t avant = jsonOrigin();
  jsonSetOrigin(sE.origine);
  jsonInjection(e);
  jsonSetOrigin(avant);
  sDerniere = true;
  sDerniereId = sE.id;
  sDerniereRes = r;
  if (r == Resultat::Ok && !nRelu) annonce("[injection] aucun front relu : etage d'injection, ligne ou etage d'ecoute ?");
  if (sE.origine != jsonp::kUsb || jsonMachine() || sE.id) return;
  // Console en mode humain, commande sans id : 'injection : <resultat>, attente <us> us ; emises ... ; relues ...'.
  const size_t cap = sizeof(sTexte) - 16;
  size_t k = (size_t)snprintf(sTexte, sizeof(sTexte), "injection : %s, attente %lu us ; emises", texte(r),
                              (unsigned long)attenteUs);
  for (uint16_t i = 0; i < sE.d.n && k < cap; i++)
    k += (size_t)snprintf(sTexte + k, sizeof(sTexte) - k, " %lu", (unsigned long)sE.d.dur[i]);
  if (k < cap) k += (size_t)snprintf(sTexte + k, sizeof(sTexte) - k, " ; relues%s", nRelu ? "" : " aucune");
  for (uint16_t i = 0; i < nRelu && k < cap; i++)
    k += (size_t)snprintf(sTexte + k, sizeof(sTexte) - k, " %lu", (unsigned long)sRelu[i]);
  Serial.println(sTexte);
}

}  // namespace

bool injectionBegin(const inj::Params &p, bool montee) {
  sParams = inj::paramsValides(p) ? p : inj::Params();
  sEtat.setMontee(montee);
  if (!montee) {
    sEtat.desarmer();
    sArmee = false;
  }
  if (sCopie) return true;
  rmt_copy_encoder_config_t e = {};
  const esp_err_t err = rmt_new_copy_encoder(&e, &sCopie);
  if (err != ESP_OK) {
    sCopie = nullptr;
    log_e("[injection] encodeur RMT : %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

inj::Refus injectionDemander(const inj::Demande &d, uint32_t id, const char *cmd, uint32_t nowMs) {
  if (sE.active) return inj::Refus::Delai;  // une demande deja en attente du silence
  const inj::Refus r = sEtat.admettre(nowMs, sParams);
  if (r != inj::Refus::Aucun) return r;
  inj::Sym y[inj::kSymMax];
  const size_t n = inj::versSymboles(d, y, inj::kSymMax);
  if (!n) return inj::Refus::Syntaxe;  // demande vide (inj::analyser n'en rend jamais)
  for (size_t i = 0; i < n; i++) {
    sSym[i].duration0 = y[i].d0;
    sSym[i].level0 = y[i].l0;
    sSym[i].duration1 = y[i].d1;
    sSym[i].level1 = y[i].l1;
  }
  sNSym = n;
  sE.d = d;
  sE.p = sParams;
  sE.id = id;
  sE.origine = jsonOrigin();
  jsonp::copyCmd(sE.cmd, cmd ? cmd : "");
  sE.accepteUs = esp_timer_get_time();
  sE.active = true;
  sEtat.noterEmission(nowMs);  // une seule injection a la fois ; le delai repart a la fin de l'emission
  return inj::Refus::Aucun;
}

void injectionPoll() {
  const uint32_t nowMs = millis();
  const bool armee = sEtat.armee(nowMs, sParams);
  if (sArmee && !armee) {
    sEtat.desarmer();
    snprintf(sTexte, sizeof(sTexte), "[injection] desarmee seule (%lu s ecoulees)", (unsigned long)sParams.armeMaxS);
    annonce(sTexte);
  }
  sArmee = armee;
  if (!sE.active) return;
  const uint64_t maintenant = esp_timer_get_time();
  const uint32_t attente = (uint32_t)(maintenant - sE.accepteUs);
  if (!sEtat.montee() || !armee) {
    annonce("[injection] desarmee ou etage non monte avant l'emission : rien n'est emis");
    terminer(Resultat::Erreur, attente, 0);
    return;
  }
  if (!bordActif() || !sCopie) {
    annonce("[injection] interruption des fronts ou encodeur RMT indisponible : rien n'est emis");
    terminer(Resultat::Erreur, attente, 0);
    return;
  }
  const uint64_t dernier = bordDernierUs();
  const bool silence = (!dernier || maintenant - dernier >= sE.p.silenceMinUs) && bordBusHaut();
  if (!silence) {
    if (attente >= (uint64_t)sE.p.attenteMaxMs * 1000u) terminer(Resultat::Delai, attente, 0);
    return;
  }
  uint16_t nRelu = 0;
  const Resultat r = emettre(&nRelu);
  if (r != Resultat::Erreur) sEtat.noterEmission(millis());  // le delai minimal court depuis la fin
  terminer(r, attente, nRelu);
}

void injectionJson(jsonp::Writer &w, uint32_t nowMs) {
  w.boolean("montee", sEtat.montee());
  w.boolean("armee", sEtat.armee(nowMs, sParams));
  w.u32("arme_reste_s", sEtat.resteS(nowMs, sParams));
  if (!sDerniere) {
    w.null("derniere");
    return;
  }
  w.obj("derniere");
  if (sDerniereId) w.u32("id", sDerniereId);
  else w.null("id");
  w.str("resultat", texte(sDerniereRes));
  w.end();
}

inj::Etat &injectionEtat() { return sEtat; }

const inj::Params &injectionParams() { return sParams; }

inj::Refus injectionArmer(uint32_t nowMs) {
  if (!sEtat.montee()) return inj::Refus::NonMontee;
  sEtat.armer(nowMs);
  sArmee = true;
  return inj::Refus::Aucun;
}

void injectionDesarmer() {
  sEtat.desarmer();
  sArmee = false;
}
```

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|SUCCESS|FAILED"`
Attendu : aucune ligne `warning` ni `error` ; `sonde          SUCCESS`. Rien n'appelle encore le module : l'édition de liens écarte ses fonctions, et avec elles l'appel à `jsonInjection`, défini à l'étape 5c.

- [ ] **Étape 5c : Blocs `config.injection` et `etat.injection`, événement `injection` (`json_mode.cpp`)**

Dans `src/json_mode.cpp` (inclusion, objet `injection` de `config`, bloc `etat.injection`, `jsonInjection` ; la branche `default` de `produce` disparaît, tous les `Item` étant traités) :

Après :

```cpp
#include "config.h"
#include "fw_version.h"
#include "h1_proto.h"
```

insérer :

```cpp
#include "injection.h"
```

Après :

```cpp
  sW.str("mode", r.changements ? "changements" : "tout");
  sW.boolean("inverse", r.capture.inverse);
  sW.end();
```

insérer :

```cpp
  // Valeurs de l'avenant en vigueur (NVS, 'injection regle') : injectionParams().
  const inj::Params &p = injectionParams();
  sW.obj("injection");
  sW.u32("gpio", kPinInjection);
  sW.u32("bas_max_us", p.basMaxUs);
  sW.u32("total_max_us", p.totalMaxUs);
  sW.u32("silence_min_us", p.silenceMinUs);
  sW.u32("attente_max_ms", p.attenteMaxMs);
  sW.u32("delai_min_ms", p.delaiMinMs);
  sW.u32("arme_max_s", p.armeMaxS);
  sW.u32("tol_us", p.tolUs);
  sW.end();
```

Après :

```cpp
  sW.str("mode", sondeReglages().changements ? "changements" : "tout");
  sW.u32("debord", captureStats().debord);
  sW.u32("rep_en_cours", sondeRepEnCours());
```

insérer :

```cpp
}

static void etatInjection(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "injection");
  bootUp();
  injectionJson(sW, now);
```

Après :

```cpp
    case Item::Config: config(o, now); break;
    case Item::EtatBus: etatBus(o, now); break;
    case Item::EtatCapture: etatCapture(o, now); break;
```

insérer :

```cpp
    case Item::EtatInjection: etatInjection(o, now); break;
```

Remplacer :

```cpp
      isReply = p.cache;
      break;
    }
    default:  // bloc absent de ce build (etat.injection : tache 23)
      sBusy = false;
      return;
  }
  // Le maximum n'est remis a 0 que s'il est parti : perdue, la ligne suivante le porte.
  if (send(o) && q.item == Item::EtatSys) k.loopMaxMs = 0;
```

par :

```cpp
      isReply = p.cache;
      break;
    }
  }
  // Le maximum n'est remis a 0 que s'il est parti : perdue, la ligne suivante le porte.
  if (send(o) && q.item == Item::EtatSys) k.loopMaxMs = 0;
```

Après :

```cpp
static void pushState(uint8_t o, uint32_t now, bool session) {
  push(o, Item::EtatBus, now, session);
  push(o, Item::EtatCapture, now, session);
```

insérer :

```cpp
  push(o, Item::EtatInjection, now, session);
```

Remplacer :

```cpp
  }
}

bool jsonLog(const char *src, const char *niv, const char *txt) {
  const uint32_t now = millis();
  bool usbTaken = false;  // le texte n'est plus ecrit sur l'USB
```

par :

```cpp
  }
}

void jsonInjection(const InjectionEv &e) {
  const uint32_t now = millis();
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    // L'origine attend l'evenement de son id, meme hors mode machine (comme la
    // livraison de la ScreenBar, 7.3) ; les autres le recoivent en mode machine.
    const bool attendu = o == sOrigin && e.id;
    if ((!k.machine && !attendu) || !claim()) continue;
    InjectionEv mine = e;
    if (o != sOrigin) mine.id = 0;  // id d'une autre session : null
    injection(sW, k.n, now, mine);
    send(o);
  }
}

bool jsonLog(const char *src, const char *niv, const char *txt) {
  const uint32_t now = millis();
  bool usbTaken = false;  // le texte n'est plus ecrit sur l'USB
```

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|SUCCESS|FAILED"`
Attendu : aucune ligne `warning` ni `error` ; `sonde          SUCCESS`.

- [ ] **Étape 5d : Commandes `injection` et `injecte` (`cli.cpp`), appels dans `main.cpp`**

Dans `src/cli.cpp` :

Remplacer :

```cpp
//  la table.
//
//  Lignes de l'hote (docs/PROTOCOLE-JSON.md) : un prefixe id=<n> fait la
//  semantique (runLine). Avec id, 'json ...', 'capture ...' et 'seuils <v>'
//  repondent sans texte (reponse ok, usage ou refuse) ; les autres commandes
//  gardent leur texte, entre une reponse debut et une reponse fin.
// ===========================================================================
#include "cli.h"

```

par :

```cpp
//  la table.
//
//  Lignes de l'hote (docs/PROTOCOLE-JSON.md) : un prefixe id=<n> fait la
//  semantique (runLine). Avec id, 'json ...', 'capture ...', 'seuils <v>',
//  'injection <v>' et 'injecte ...' repondent sans texte (reponse ok,
//  accepte, usage ou refuse) ; les autres commandes gardent leur texte, entre
//  une reponse debut et une reponse fin.
// ===========================================================================
#include "cli.h"

```

Après :

```cpp
#include "capture_rmt.h"
#include "config.h"
#include "fw_version.h"
```

insérer :

```cpp
#include "injection.h"
```

Remplacer :

```cpp
  return {true, "ok", nullptr};
}

static const char kUsageInjection[] = "usage : injection [regle <nom> <valeur>]";

static void afficherInjection() {
  const ReglagesSonde &r = sondeReglages();
  const inj::Params &p = r.injection;
  Serial.printf("injection : etage %s (GPIO%u)\n", r.injMontee ? "declare monte" : "non monte", (unsigned)kPinInjection);
  Serial.printf("  bas_max_us %lu, total_max_us %lu, silence_min_us %lu, attente_max_ms %lu, delai_min_ms %lu, "
                "arme_max_s %lu, tol_us %lu\n",
                (unsigned long)p.basMaxUs, (unsigned long)p.totalMaxUs, (unsigned long)p.silenceMinUs,
```

par :

```cpp
  return {true, "ok", nullptr};
}

static const char kUsageInjection[] = "usage : injection [monte 0|1 | on | off | regle <nom> <valeur>]";
static const char kUsageInjecte[] = "usage : injecte durees <d1> <d2> ... (us, alternees, la premiere basse)";

static void afficherInjection() {
  const inj::Params &p = injectionParams();
  const inj::Etat &e = injectionEtat();
  const uint32_t now = millis();
  Serial.printf("injection : etage %s (GPIO%u), ", e.montee() ? "declare monte" : "non monte", (unsigned)kPinInjection);
  if (e.armee(now, p)) Serial.printf("armee (reste %lu s)\n", (unsigned long)e.resteS(now, p));
  else Serial.println("desarmee");
  Serial.printf("  bas_max_us %lu, total_max_us %lu, silence_min_us %lu, attente_max_ms %lu, delai_min_ms %lu, "
                "arme_max_s %lu, tol_us %lu\n",
                (unsigned long)p.basMaxUs, (unsigned long)p.totalMaxUs, (unsigned long)p.silenceMinUs,
```

Remplacer :

```cpp
                (unsigned long)p.tolUs);
}

// Reglages d'injection changes : NVS, config reemise.
static bool appliquerInjection(const ReglagesSonde &r) { return sondeAppliquer(r); }

// injection regle <nom> <valeur> : verifie avant d'ecrire en NVS (bornes : inj::bornesParam).
static Resultat faireRegle(char *args) {
```

par :

```cpp
                (unsigned long)p.tolUs);
}

// Reglages d'injection changes : NVS, config reemise, module d'injection a jour.
static bool appliquerInjection(const ReglagesSonde &r) {
  const bool ok = sondeAppliquer(r);
  injectionBegin(sondeReglages().injection, sondeReglages().injMontee);
  return ok;
}

// injection regle <nom> <valeur> : verifie avant d'ecrire en NVS (bornes : inj::bornesParam).
static Resultat faireRegle(char *args) {
```

Remplacer :

```cpp
  return {true, "ok", nullptr};
}

// injection [regle <nom> <valeur>] ; args vide : rien a faire.
static Resultat faireInjection(char *args) {
  char *reste = splitWord(args);
  if (!strcmp(args, "regle")) return faireRegle(reste);
  if (!*args) return {true, "ok", nullptr};
  return {false, "usage", kUsageInjection};
}

// ---------------------------------------------------------------------------
```

par :

```cpp
  return {true, "ok", nullptr};
}

// injection monte 0|1 | on | off | regle <nom> <valeur> ; args vide : rien a faire.
static Resultat faireInjection(char *args) {
  static char msg[jsonp::kMsgMax + 1];
  char *reste = splitWord(args);
  if (!strcmp(args, "regle")) return faireRegle(reste);
  if (!strcmp(args, "monte")) {
    if (strcmp(reste, "0") && strcmp(reste, "1")) return {false, "usage", "usage : injection monte 0|1"};
    ReglagesSonde r = sondeReglages();
    r.injMontee = !strcmp(reste, "1");
    if (!appliquerInjection(r)) return {false, "refuse", "injection monte : echec de l'ecriture en NVS"};
    return {true, "ok", nullptr};
  }
  if (*reste) return {false, "usage", kUsageInjection};
  if (!strcmp(args, "on")) {
    if (injectionArmer(millis()) != inj::Refus::Aucun)
      return {false, "refuse", "injection : non montee ('injection monte 1' par l'USB, etage monte)"};
    snprintf(msg, sizeof(msg), "injection armee pour %lu s ('injection off' pour desarmer)",
             (unsigned long)injectionParams().armeMaxS);
    return {true, "ok", msg};
  }
  if (!strcmp(args, "off")) {
    injectionDesarmer();
    return {true, "ok", "injection desarmee"};
  }
  if (!*args) return {true, "ok", nullptr};
  return {false, "usage", kUsageInjection};
}

// injecte durees <d1> <d2> ... : analysee (bornes en vigueur), puis demandee.
// Acceptee : l'evenement injection suit (asynchrone). shown : la commande
// telle que l'evenement la montre.
static Resultat faireInjecte(const char *args, uint32_t id, const char *shown) {
  static char msg[jsonp::kMsgMax + 1];
  const inj::Params &p = injectionParams();
  inj::Demande d;
  const inj::Refus a = inj::analyser(args, p, &d);
  switch (a) {
    case inj::Refus::Aucun: break;
    case inj::Refus::TropDeDurees:
      snprintf(msg, sizeof(msg), "injecte : trop de durees (%u au plus)", (unsigned)inj::kDurMax);
      return {false, "usage", msg};
    case inj::Refus::BasTropLong:
      snprintf(msg, sizeof(msg), "injecte : duree basse trop longue (bas_max_us %lu)", (unsigned long)p.basMaxUs);
      return {false, "usage", msg};
    case inj::Refus::TropLong:
      snprintf(msg, sizeof(msg), "injecte : trame trop longue (total_max_us %lu)", (unsigned long)p.totalMaxUs);
      return {false, "usage", msg};
    default: return {false, "usage", kUsageInjecte};
  }
  const inj::Refus r = injectionDemander(d, id, shown, millis());
  switch (r) {
    case inj::Refus::Aucun: return {true, "accepte", nullptr};
    case inj::Refus::NonMontee: return {false, "refuse", "injecte : non montee ('injection monte 1' par l'USB)"};
    case inj::Refus::NonArmee: return {false, "refuse", "injecte : non armee ('injection on')"};
    case inj::Refus::Delai:
      snprintf(msg, sizeof(msg), "injecte : delai minimal (%lu ms entre deux injections)",
               (unsigned long)p.delaiMinMs);
      return {false, "refuse", msg};
    default:
      snprintf(msg, sizeof(msg), "injecte : %s", inj::refusTexte(r));
      return {false, "refuse", msg};
  }
}

// ---------------------------------------------------------------------------
```

Remplacer :

```cpp
  Serial.printf("wifi : identifiants enregistres, connexion a %s ('info' pour suivre)\n", args);
}

// injection [regle <nom> <valeur>] : USB seulement (liste blanche).
static void cmdInjection(char *args) {
  if (*args) {
    const Resultat r = faireInjection(args);
```

par :

```cpp
  Serial.printf("wifi : identifiants enregistres, connexion a %s ('info' pour suivre)\n", args);
}

// injection [monte 0|1 | on | off | regle <nom> <valeur>] : 'on' et 'off'
// aussi a distance (liste blanche).
static void cmdInjection(char *args) {
  if (*args) {
    const Resultat r = faireInjection(args);
```

Après :

```cpp
    if (!r.ok) return;
  }
  afficherInjection();
```

insérer :

```cpp
}

// injecte durees <d1> <d2> ... : le resultat suit en texte ('injection : ...').
static void cmdInjecte(char *args) {
  char shown[jsonp::kCmdTextMax + 1];
  snprintf(shown, sizeof(shown), "injecte %s", args);
  const Resultat r = faireInjecte(args, 0, shown);
  Serial.println(r.ok ? "injecte : acceptee, attente du silence" : r.msg);
```

Remplacer :

```cpp
  {"reboot", cmdReboot, "redemarrage"},
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log] : mode machine"},
  {"wifi", cmdWifi, "wifi <ssid> <mdp>"},
  {"injection", cmdInjection, "injection [regle <nom> <valeur>] : garde-fous de l'injection"},
};

static void cmdHelp(char *) {
```

par :

```cpp
  {"reboot", cmdReboot, "redemarrage"},
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log] : mode machine"},
  {"wifi", cmdWifi, "wifi <ssid> <mdp>"},
  {"injection", cmdInjection, "injection [monte 0|1 | on | off | regle <nom> <valeur>]"},
  {"injecte", cmdInjecte, "injecte durees <d1> <d2> ... (us, la premiere basse)"},
};

static void cmdHelp(char *) {
```

Remplacer :

```cpp
// Une ligne de l'hote : prefixe id=<n> retire avant l'aiguillage, refus sans
// execution (trop longue, cadence, interdite a distance), puis :
//  - sans id : comme toujours (texte) ;
//  - avec id : famille json (reponse seule) ; capture et seuils <valeurs>
//    sans texte (reponse ok, usage ou refuse ; spec 8.3 : a distance, aucune
//    commande ne produit de texte) ; sinon commande a texte entre reponse
//    debut et reponse fin.
// A distance, jsonRemoteAdmit passe avant tout refus (id deja vu).
static void runLine(char *line, bool tooLong) {
  const uint32_t t0 = millis();
```

par :

```cpp
// Une ligne de l'hote : prefixe id=<n> retire avant l'aiguillage, refus sans
// execution (trop longue, cadence, interdite a distance), puis :
//  - sans id : comme toujours (texte) ;
//  - avec id : famille json (reponse seule) ; capture, seuils <valeurs>,
//    injection <valeurs> et injecte sans texte (reponse ok, accepte, usage ou
//    refuse ; spec 8.3 : a distance, aucune commande ne produit de texte) ;
//    sinon commande a texte entre reponse debut et reponse fin.
// A distance, jsonRemoteAdmit passe avant tout refus (id deja vu).
static void runLine(char *line, bool tooLong) {
  const uint32_t t0 = millis();
```

Remplacer :

```cpp
  }
  if (firstWordIs(cmd, "json")) {
    jsonCommand(afterWord(cmd), c);
  } else if (firstWordIs(cmd, "capture") || (firstWordIs(cmd, "seuils") && *afterWord(cmd))) {
    const Resultat res = firstWordIs(cmd, "capture") ? faireCapture(afterWord(cmd)) : faireSeuils(afterWord(cmd));
    r.ok = res.ok;
    r.code = res.code;
    r.msg = res.msg;
    r.durMs = millis() - t0;
    jsonReply(r);
  } else {
```

par :

```cpp
  }
  if (firstWordIs(cmd, "json")) {
    jsonCommand(afterWord(cmd), c);
  } else if (firstWordIs(cmd, "capture") || firstWordIs(cmd, "injecte") ||
             ((firstWordIs(cmd, "seuils") || firstWordIs(cmd, "injection")) && *afterWord(cmd))) {
    Resultat res{};
    if (firstWordIs(cmd, "capture")) res = faireCapture(afterWord(cmd));
    else if (firstWordIs(cmd, "seuils")) res = faireSeuils(afterWord(cmd));
    else if (firstWordIs(cmd, "injection")) res = faireInjection(afterWord(cmd));
    else res = faireInjecte(afterWord(cmd), id, shown);
    r.ok = res.ok;
    r.code = res.code;
    r.msg = res.msg;
    // 'injecte' acceptee : l'evenement injection suivra, avec cet id.
    if (res.ok && firstWordIs(cmd, "injecte")) r.suite = jsonp::Reply::SuiteInjection;
    r.durMs = millis() - t0;
    jsonReply(r);
  } else {
```

Dans `src/main.cpp` :

Remplacer :

```cpp
//  Sonde de reconnaissance de la ligne D (docs/SPEC-RECONNAISSANCE.md)
//
//  setup() : GPIO7 bas en premier, port USB, identifiant de demarrage,
//  reglages NVS, interruption des fronts, capture RMT, console, Wi-Fi, UDP.
//  loop() : console, capture (parties -> lignes trame du mode machine, ou
//  texte), mode machine (bail, lignes periodiques), Wi-Fi, UDP (commandes
//  recues, datagrammes emis), puis la tache IDLE.
// ===========================================================================
#include <Arduino.h>

```

par :

```cpp
//  Sonde de reconnaissance de la ligne D (docs/SPEC-RECONNAISSANCE.md)
//
//  setup() : GPIO7 bas en premier, port USB, identifiant de demarrage,
//  reglages NVS, interruption des fronts, capture RMT, console, Wi-Fi, UDP,
//  injection (desarmee).
//  loop() : console, capture (parties -> lignes trame du mode machine, ou
//  texte), mode machine (bail, lignes periodiques), Wi-Fi, UDP (commandes
//  recues, datagrammes emis), injection (attente du silence, emission,
//  evenement), puis la tache IDLE.
// ===========================================================================
#include <Arduino.h>

```

Après :

```cpp
#include "cli.h"
#include "config.h"
#include "fw_version.h"
```

insérer :

```cpp
#include "injection.h"
```

Après :

```cpp
  cliBegin();
  netWifiBegin();  // identifiants en NVS : station, mDNS ; sans eux, radio eteinte
  netUdpBegin();   // cle H1 en NVS (hotte/cle) : socket UDP 5480 des que le Wi-Fi a une adresse
```

insérer :

```cpp
  // Valeurs de l'avenant et etage declare monte (NVS) ; toujours desarmee au demarrage.
  if (!injectionBegin(sReglages.injection, sReglages.injMontee))
    Serial.println("[injection] encodeur RMT indisponible : injection impossible");
```

Après :

```cpp
  jsonPoll();
  netWifiPoll();
  netUdpPoll();
```

insérer :

```cpp
  injectionPoll();
```

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|SUCCESS|FAILED"`
Attendu : aucune ligne `warning` ni `error` ; `sonde          SUCCESS`. L'étape 6 relève la mémoire et vérifie l'IRAM.

- [ ] **Étape 6 : Compiler et vérifier l'IRAM**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|RAM:|Flash:|SUCCESS|FAILED"`
Attendu : aucune ligne `warning` ni `error` ; `RAM:   [===       ]  31.2% (used 102252 bytes from 327680 bytes)`, `Flash: [========  ]  82.3% ...`, `sonde          SUCCESS`.
Lancer : `$(ls ~/.platformio/packages/toolchain-riscv32-esp/bin/*-objdump | head -1) -t .pio/build/sonde/firmware.elf | grep -E "frontProgramme|frontAnormal|niveauAttendu|Surveillance5front|surFront|finEmission" | awk '{print $4, $NF}'`
Attendu : les six fonctions en `.iram0.text` (l'interruption des fronts ne lit rien en flash).

- [ ] **Étape 7 : Documenter l'injection dans le protocole (exemples vérifiés)**

Dans `docs/PROTOCOLE-JSON.md` :

Remplacer :

```markdown
- Code : `src/json_out.*` (briques pures, testées sur l'hôte),
  `src/json_mode.*` (sessions, instantanés, événements), `src/cli.cpp`
  (lignes de l'hôte), `src/net_wifi.*` et `src/net_udp_wifi.*` (transport
  réseau), `src/h1_proto.*` (enveloppe H1, copiée de la ScreenBar).
- Vérification : `tools/json_check.py` contrôle toute ligne machine contre ce
  profil (capture série brute, ou `.jsonl` avec `--jsonl`). Les exemples des
  sections 8 et 9 sont vérifiés par
  `python3 tools/json_check.py --strict --exemples docs/PROTOCOLE-JSON.md`,
  lancé par `sh tools/tests/test_hote.sh`.
- État : transports USB et UDP sur le Wi-Fi (section 9). L'injection vient
  ensuite ; ce document s'étendra avec elle.

## 1. Ce qui ne change pas

```

par :

```markdown
- Code : `src/json_out.*` (briques pures, testées sur l'hôte),
  `src/json_mode.*` (sessions, instantanés, événements), `src/cli.cpp`
  (lignes de l'hôte), `src/net_wifi.*` et `src/net_udp_wifi.*` (transport
  réseau), `src/h1_proto.*` (enveloppe H1, copiée de la ScreenBar),
  `src/injection_regles.*` (garde-fous de l'injection, testés sur l'hôte) et
  `src/injection.*` (émission).
- Vérification : `tools/json_check.py` contrôle toute ligne machine contre ce
  profil (capture série brute, ou `.jsonl` avec `--jsonl`). Les exemples des
  sections 8 et 9 sont vérifiés par
  `python3 tools/json_check.py --strict --exemples docs/PROTOCOLE-JSON.md`,
  lancé par `sh tools/tests/test_hote.sh`.
- État : transports USB et UDP sur le Wi-Fi (section 9) ; injection bornée
  de l'étape 7 (sections 5.1 et 6.2), avec la seule syntaxe d'avant
  l'avenant de l'étape 6 : `injecte durees ...`.

## 1. Ce qui ne change pas

```

Remplacer :

```markdown
| `json` | état de la session, en texte | |
| `json 1 [bail <s>]` | mode machine ; `hello`, `config` et l'instantané complet par la file des périodiques, puis la `reponse` | bail 0 (aucun) ou 10..600 s, défaut 30 |
| `json 0` | retour au mode humain : `fin`, puis l'invite | |
| `json etat` | instantané : `etat` (3 blocs) et `compteurs` | |
| `json hello` | `hello` (2 blocs) et `config` | |
| `json ping` | renouvelle le bail ; la `reponse` porte `bail_s` et `up_s` | |
| `json periode <ms>` | période des `etat` | 0 ou 200..60000, défaut **1 000** |
```

par :

```markdown
| `json` | état de la session, en texte | |
| `json 1 [bail <s>]` | mode machine ; `hello`, `config` et l'instantané complet par la file des périodiques, puis la `reponse` | bail 0 (aucun) ou 10..600 s, défaut 30 |
| `json 0` | retour au mode humain : `fin`, puis l'invite | |
| `json etat` | instantané : `etat` (4 blocs) et `compteurs` | |
| `json hello` | `hello` (2 blocs) et `config` | |
| `json ping` | renouvelle le bail ; la `reponse` porte `bail_s` et `up_s` | |
| `json periode <ms>` | période des `etat` | 0 ou 200..60000, défaut **1 000** |
```

Remplacer :

```markdown
### 4.2 `config`

Émise avec `hello`, et de nouveau après chaque `capture tout`,
`capture changements` ou `seuils <valeurs>` accepté, même si les valeurs ne
changent pas (`sondeAppliquer`). `capture on` et `capture off` ne changent pas
la `config` : l'état se lit dans `etat.capture.active`.

| Champ | Type | Sens |
|---|---|---|
```

par :

```markdown
### 4.2 `config`

Émise avec `hello`, et de nouveau après chaque `capture tout`,
`capture changements`, `seuils <valeurs>`, `injection monte 0|1` ou
`injection regle <nom> <valeur>` accepté, même si les valeurs ne changent pas
(`sondeAppliquer`). `capture on` et `capture off` ne changent pas la `config` :
l'état se lit dans `etat.capture.active` ; de même, `injection on` et
`injection off` se lisent dans `etat.injection.armee`.

| Champ | Type | Sens |
|---|---|---|
```

Remplacer :

```markdown
| `capture.mode` | `tout` ou `changements` | mode d'émission (spec §8.2) |
| `capture.inverse` | booléen | étage d'écoute inverseur : les niveaux publiés sont ceux **du bus** |

### 4.3 `etat`

Période `periode_ms` (1 000 ms sur l'USB). Trois blocs.

**Bloc `bus`** :

```

par :

```markdown
| `capture.mode` | `tout` ou `changements` | mode d'émission (spec §8.2) |
| `capture.inverse` | booléen | étage d'écoute inverseur : les niveaux publiés sont ceux **du bus** |

Objet `injection` : les valeurs de l'avenant de l'étape 6 (spec §6), et avant
lui les valeurs par défaut ci-dessous. Elles se règlent par l'USB seulement
(`injection regle <nom> <valeur>`, section 5.1), sont vérifiées avant d'être
écrites en NVS (espace `hotte`, clé `inj_params`), et de nouveau au démarrage :
un jeu hors bornes y est remplacé tout entier par les valeurs par défaut.

| Champ | Bornes | Défaut | Sens |
|---|---|---|---|
| `injection.gpio` | | 7 | base de l'étage d'injection |
| `injection.bas_max_us` | 10..20 000 | 3 000 | durée basse maximale d'une impulsion |
| `injection.total_max_us` | 100..200 000, et au moins `bas_max_us` | 200 000 | durée totale maximale d'une trame injectée (la borne haute est le défaut : `relu_us` n'est jamais tronqué, section 6.2) |
| `injection.silence_min_us` | 1 000..1 000 000 | 20 000 | silence exigé avant d'émettre : aucun front, bus haut |
| `injection.attente_max_ms` | 10..10 000 | 1 000 | attente maximale de ce silence, puis résultat `delai` |
| `injection.delai_min_ms` | 1 000..60 000 | 3 000 | délai minimal entre deux injections (toutes, avant l'avenant) |
| `injection.arme_max_s` | 10..600 | 600 | désarmement automatique après `injection on` |
| `injection.tol_us` | 1..500 | 20 | tolérance de la surveillance des fronts (délai des étages, mesuré au banc) |

### 4.3 `etat`

Période `periode_ms` (1 000 ms sur l'USB). Quatre blocs.

**Bloc `bus`** :

```

Après :

```markdown
| `mode` | `tout` ou `changements` | |
| `debord` | entier | blocs perdus depuis le démarrage, tampon circulaire plein |
| `rep_en_cours` | entier | mode `changements` : réceptions identiques depuis la dernière émise |
```

insérer :

```markdown

**Bloc `injection`** :

| Champ | Type | Sens |
|---|---|---|
| `montee` | booléen | étage d'injection déclaré monté (`injection monte 1`, NVS) |
| `armee` | booléen | injection armée (`injection on`) ; fausse au démarrage, et d'elle-même après `arme_max_s` |
| `arme_reste_s` | entier 0..600 | secondes avant le désarmement automatique ; 0 si désarmée |
| `derniere` | objet ou null | dernière injection terminée depuis le démarrage : `id` (celui de la commande dans sa session, ou null) et `resultat` (section 6.2) ; null avant la première |
```

Remplacer :

```markdown
|---|---|---|
| famille `json` | comme la ScreenBar | lignes produites, puis `reponse` `fin` |
| `capture on\|off\|tout\|changements`, `seuils <filtre_us> [silence_us] [resol_hz]` | **sans texte** : la réponse porte le résultat | `reponse` `fin` : `ok` (avec un `msg` si le mode n'a pas pu être écrit en NVS), `usage` (mot inconnu ou valeur hors bornes, avec `msg`) ou `refuse` (RMT ou NVS en échec) ; puis `config` après `tout`, `changements` et `seuils` |
| toute autre commande (`info`, `bus`, `stats`, `help`, `seuils` seul...) | inchangée : texte humain | `reponse` `debut`, le texte, puis `reponse` `fin` `execute` (ou `inconnue`) |

Sans `id`, rien ne change : texte humain, aucune `reponse`. `seuils` vérifie
toutes les valeurs **avant** d'écrire en NVS (spec §8.2).

## 6. Événements

```

par :

```markdown
|---|---|---|
| famille `json` | comme la ScreenBar | lignes produites, puis `reponse` `fin` |
| `capture on\|off\|tout\|changements`, `seuils <filtre_us> [silence_us] [resol_hz]` | **sans texte** : la réponse porte le résultat | `reponse` `fin` : `ok` (avec un `msg` si le mode n'a pas pu être écrit en NVS), `usage` (mot inconnu ou valeur hors bornes, avec `msg`) ou `refuse` (RMT ou NVS en échec) ; puis `config` après `tout`, `changements` et `seuils` |
| `injection on\|off`, `injection monte 0\|1`, `injection regle <nom> <valeur>` | **sans texte** (section 5.1) | `reponse` `fin` : `ok` (`on` porte un `msg`), `usage` (avec `msg`) ou `refuse` (`on` sans étage monté, NVS en échec) ; puis `config` après `monte` et `regle` |
| `injecte durees <d1> <d2> ...` | **asynchrone**, sans texte (section 5.1) | `reponse` `fin` tout de suite : `accepte` avec `suite` = `injection`, ou `refuse` (`msg` : `non montee`, `non armee`, `delai minimal`), ou `usage` (syntaxe, bornes) ; puis, si acceptée, l'événement `injection` avec le même `id` (section 6.2) |
| toute autre commande (`info`, `bus`, `stats`, `help`, `seuils` seul, `injection` seul...) | inchangée : texte humain | `reponse` `debut`, le texte, puis `reponse` `fin` `execute` (ou `inconnue`) |

Sans `id`, rien ne change : texte humain, aucune `reponse`. `seuils` vérifie
toutes les valeurs **avant** d'écrire en NVS (spec §8.2).

### 5.1 Injection

Spec §8.3 et §8.4. Tous les garde-fous sont dans le firmware.

- **`injecte durees <d1> <d2> ...`** : la seule syntaxe avant l'avenant de
  l'étape 6. Durées en µs, entiers > 0, alternées, la première au niveau
  **bas** du bus ; 64 au plus (et la ligne fait 127 caractères au plus) ;
  chaque durée basse (rang pair) au plus `bas_max_us`, la somme au plus
  `total_max_us`. Sinon `usage`, avec un `msg`.
- **Refus**, dans cet ordre : `non montee` (`injection monte 1` jamais
  envoyé), `non armee`, `delai minimal` (moins de `delai_min_ms` depuis la fin
  de la dernière émission ; une injection encore en attente du silence compte
  aussi).
- **Acceptée** : elle attend le silence (`silence_min_us` sans front, bus
  haut), part en une seule transaction RMT sur GPIO7 (GPIO7 haut = bus tiré
  bas, par Q2), sous la surveillance des fronts de GPIO6, puis l'événement
  `injection` donne le résultat (section 6.2).
- **`injection on`** n'arme que si l'étage est déclaré monté. L'injection est
  désarmée au démarrage, par `injection off`, par `injection monte 0`, et
  d'elle-même après `arme_max_s` (annonce `log` `injection`). Une injection
  en attente du silence n'est pas émise si l'injection est désarmée entre-temps
  (résultat `erreur`).
- **`injection monte 0|1`** et **`injection regle <nom> <valeur>`** (noms :
  les champs de `config.injection`, sauf `gpio`) : USB seulement, vérifiés avant
  la NVS. `injection` seul affiche l'état et les valeurs en texte.
- À distance, seuls `injection on|off` et `injecte ...` passent (section 9.4) ;
  `tools/hotte_udp.py` n'envoie `injection on` qu'après une confirmation tapée
  dans un terminal, la phrase exacte « Majid devant la hotte » (règle 11 de la
  spec) ; sans terminal (script, agent), il refuse.

## 6. Événements

```

Remplacer :

```markdown
|---|---|---|
| `trame` | 100 par seconde | `json trames 1` (défaut) |
| `log` | 20 par seconde | `json log 1` |
| `injection` | aucun | toujours |

### 6.1 `trame` : une partie de réception

```

par :

```markdown
|---|---|---|
| `trame` | 100 par seconde | `json trames 1` (défaut) |
| `log` | 20 par seconde | `json log 1` |
| `injection` | aucun | toujours ; `id` pour la seule session qui a envoyé la commande |

### 6.1 `trame` : une partie de réception

```

Remplacer :

```markdown

### 6.2 `injection` : fin d'une injection

Les commandes `injection` et `injecte` (spec §8.3 et §8.4) arrivent avec
l'étage d'injection ; le format de leur événement est fixé :
`id` (celui de la commande, ou null), `cmd` (40 caractères au plus),
`resultat` (`ok`, `collision`, `delai`), `niv0`, `dur_us` (durées émises, 64
au plus), `attente_us` (attente du silence), `relu_us` (durées relues pendant
l'émission).

### 6.3 `log` : annonces du firmware

```

par :

```markdown

### 6.2 `injection` : fin d'une injection

Suit la `reponse` `accepte` d'un `injecte` : une ligne par demande acceptée,
vers chaque session en mode machine. Seule la session qui a envoyé la commande
reçoit son `id`, même hors mode machine si la commande en portait un ; les
autres voient `id` null (mêmes règles que la `livraison` de la ScreenBar §7.3).
Une commande de la console sans `id`, en mode humain, reçoit le résultat en
texte : `injection : ok, attente 412 us ; emises 750 750 750 2250 ; relues 752 748 751`.

| Champ | Type | Sens |
|---|---|---|
| `id` | entier ou null | id de la commande dans la session qui l'a envoyée ; null dans les autres sessions, et pour une commande de la console sans id |
| `cmd` | chaîne, 40 caractères au plus | la commande |
| `resultat` | `ok`, `collision`, `delai`, `erreur` | voir ci-dessous |
| `niv0` | `bas` | niveau du bus pendant `dur_us[0]` (toujours `bas` avec `injecte durees`) |
| `dur_us` | tableau de 1 à 64 entiers | durées demandées, en µs : émises en entier si `ok`, jusqu'à l'arrêt si `collision`, pas du tout si `delai` ou `erreur` |
| `attente_us` | entier | de l'acceptation au début de l'émission (ou à l'abandon) |
| `relu_us` | tableau de 64 entiers au plus | durées entre fronts successifs lus sur GPIO6 pendant l'émission, depuis son premier front : n − 1 valeurs pour n durées en nombre pair (le dernier palier haut se fond dans le repos), n en nombre impair ; vide si rien n'a été émis |

Résultats :
- **`ok`** : émise en entier, sans front anormal. Sans aucune durée relue, un
  `log` `injection` le signale (étage d'injection, ligne ou étage d'écoute à
  vérifier).
- **`collision`** : un front anormal pendant l'émission (le bus change de
  niveau loin de tout front programmé, à `tol_us` près, vers un autre niveau
  que celui émis), ou un premier front qui n'est pas celui de l'émission (vers
  le haut, ou plus de 500 µs après le départ). L'émission est arrêtée tout de
  suite (`rmt_disable`) et GPIO7 revient à l'état bas ; `relu_us` s'arrête au
  front anormal.
- **`delai`** : le bus ne s'est pas tu dans `attente_max_ms` ; rien n'est émis.
- **`erreur`** : rien n'est émis, ou l'émission n'a pas pu finir (RMT
  indisponible, interruption des fronts absente, injection désarmée ou étage
  déclaré démonté pendant l'attente du silence) ; un `log` `injection` en
  donne la cause.

La trame injectée est aussi capturée comme toute trame du bus (`trame`,
section 6.1) : c'est ainsi qu'on « retrouve la trame sur le bus » (spec §6,
étape 7). La ligne tient le budget de 896 octets : à la borne haute de
`total_max_us` (200 000 µs), une commande `injecte` de 64 durées garde toutes
ses durées relues (test hôte `test_json`). `relu_us` ne serait coupé qu'au-delà
du budget, ce qu'aucune commande acceptée n'atteint.

### 6.3 `log` : annonces du firmware

```

Remplacer :

````markdown
```
<RS>{"v":1,"t":"hello","n":0,"ms":12031,"bloc":"base","rev":4,"fw":"0.1.0-1a2b3c4","fw_desc":"0.1.0-1a2b3c4","date":"Sep 27 2026","heure":"14:02:11","env":"sonde","build":"sonde","reseau_build":"aucun","puce":"esp32c6","idf":"v5.5.5","arduino":"3.3.12","boot":"3FA2C901","reset":"mise_sous_tension","reset_n":1,"up_s":12,"session":{"transport":"usb","periode_ms":1000,"compteurs_ms":1000,"reseau_ms":5000,"bail_s":30,"trames":true,"log":false},"limites":{"ligne_max":1024,"cmd_max":127}}
<RS>{"v":1,"t":"hello","n":1,"ms":12032,"bloc":"identite","boot":"3FA2C901","mac":"F0F5BD012345","id":{"fabricant":"Djoko-CLI","produit":"Sonde hotte Haier","serie":"HOTTE-F0F5BD012345","nom":"Sonde hotte","hw":1,"hw_txt":"C6 SuperMini, etages v1"},"appareil":"hotte","caps":["sonde","injection","trames","log","udp","cle","mdns"]}
<RS>{"v":1,"t":"config","n":2,"ms":12033,"capture":{"gpio":6,"resol_hz":1000000,"filtre_us":1,"silence_us":5000,"mode":"tout","inverse":true}}
<RS>{"v":1,"t":"etat","n":3,"ms":12034,"bloc":"bus","boot":"3FA2C901","up_s":12,"repos":"haut","fronts":1520,"derniere_ms":3,"receptions_s":10}
<RS>{"v":1,"t":"etat","n":4,"ms":12035,"bloc":"capture","boot":"3FA2C901","up_s":12,"active":true,"mode":"tout","debord":0,"rep_en_cours":0}
<RS>{"v":1,"t":"etat","n":5,"ms":12036,"bloc":"sys","boot":"3FA2C901","up_s":12,"sys":{"heap":247812,"heap_min":241600,"heap_bloc":110580,"pile_boucle":5316,"boucle_max_ms":2,"json_perdus":0,"json_trop_longs":0,"rejets":0}}
<RS>{"v":1,"t":"compteurs","n":6,"ms":12037,"bloc":"sonde","receptions":118,"parties":118,"blocs":118,"symboles":1947,"debord":0,"rep":0,"lignes_perdues":0,"sautes":0,"rejets":0}
<RS>{"v":1,"t":"reseau","n":7,"ms":12038,"bloc":"ip","frais_ms":0,"srp":null,"adresses":[{"adr":"192.168.1.42","type":"autre","pref":true}],"udp":{"port":5480,"ouvert":true,"empreinte":"630DCD29","sessions":0,"provisoire":false,"rx":0,"rejets":0,"rx_perdus":0,"defis":0,"tx":0,"tx_perdus":0,"tx_erreurs":0,"tampons_libres":null,"tampons_min":null},"mdns":{"nom":"hotte-sonde.local"},"wifi":{"connecte":true,"rssi_dbm":-58,"ip":"192.168.1.42","pertes":0}}
<RS>{"v":1,"t":"reponse","n":8,"ms":12039,"id":1,"etape":"fin","cmd":"json 1","ok":true,"code":"ok","duree_ms":8,"bail_s":30,"up_s":12}
```

Puis `etat` et `compteurs` chaque seconde, `reseau` toutes les 5 s, et les
````

par :

````markdown
```
<RS>{"v":1,"t":"hello","n":0,"ms":12031,"bloc":"base","rev":4,"fw":"0.1.0-1a2b3c4","fw_desc":"0.1.0-1a2b3c4","date":"Sep 27 2026","heure":"14:02:11","env":"sonde","build":"sonde","reseau_build":"aucun","puce":"esp32c6","idf":"v5.5.5","arduino":"3.3.12","boot":"3FA2C901","reset":"mise_sous_tension","reset_n":1,"up_s":12,"session":{"transport":"usb","periode_ms":1000,"compteurs_ms":1000,"reseau_ms":5000,"bail_s":30,"trames":true,"log":false},"limites":{"ligne_max":1024,"cmd_max":127}}
<RS>{"v":1,"t":"hello","n":1,"ms":12032,"bloc":"identite","boot":"3FA2C901","mac":"F0F5BD012345","id":{"fabricant":"Djoko-CLI","produit":"Sonde hotte Haier","serie":"HOTTE-F0F5BD012345","nom":"Sonde hotte","hw":1,"hw_txt":"C6 SuperMini, etages v1"},"appareil":"hotte","caps":["sonde","injection","trames","log","udp","cle","mdns"]}
<RS>{"v":1,"t":"config","n":2,"ms":12033,"capture":{"gpio":6,"resol_hz":1000000,"filtre_us":1,"silence_us":5000,"mode":"tout","inverse":true},"injection":{"gpio":7,"bas_max_us":3000,"total_max_us":200000,"silence_min_us":20000,"attente_max_ms":1000,"delai_min_ms":3000,"arme_max_s":600,"tol_us":20}}
<RS>{"v":1,"t":"etat","n":3,"ms":12034,"bloc":"bus","boot":"3FA2C901","up_s":12,"repos":"haut","fronts":1520,"derniere_ms":3,"receptions_s":10}
<RS>{"v":1,"t":"etat","n":4,"ms":12035,"bloc":"capture","boot":"3FA2C901","up_s":12,"active":true,"mode":"tout","debord":0,"rep_en_cours":0}
<RS>{"v":1,"t":"etat","n":5,"ms":12036,"bloc":"injection","boot":"3FA2C901","up_s":12,"montee":false,"armee":false,"arme_reste_s":0,"derniere":null}
<RS>{"v":1,"t":"etat","n":6,"ms":12037,"bloc":"sys","boot":"3FA2C901","up_s":12,"sys":{"heap":247812,"heap_min":241600,"heap_bloc":110580,"pile_boucle":5316,"boucle_max_ms":2,"json_perdus":0,"json_trop_longs":0,"rejets":0}}
<RS>{"v":1,"t":"compteurs","n":7,"ms":12038,"bloc":"sonde","receptions":118,"parties":118,"blocs":118,"symboles":1947,"debord":0,"rep":0,"lignes_perdues":0,"sautes":0,"rejets":0}
<RS>{"v":1,"t":"reseau","n":8,"ms":12039,"bloc":"ip","frais_ms":0,"srp":null,"adresses":[{"adr":"192.168.1.42","type":"autre","pref":true}],"udp":{"port":5480,"ouvert":true,"empreinte":"630DCD29","sessions":0,"provisoire":false,"rx":0,"rejets":0,"rx_perdus":0,"defis":0,"tx":0,"tx_perdus":0,"tx_erreurs":0,"tampons_libres":null,"tampons_min":null},"mdns":{"nom":"hotte-sonde.local"},"wifi":{"connecte":true,"rssi_dbm":-58,"ip":"192.168.1.42","pertes":0}}
<RS>{"v":1,"t":"reponse","n":9,"ms":12040,"id":1,"etape":"fin","cmd":"json 1","ok":true,"code":"ok","duree_ms":9,"bail_s":30,"up_s":12}
```

Puis `etat` et `compteurs` chaque seconde, `reseau` toutes les 5 s, et les
````

Remplacer :

````markdown

```
<RS>{"v":1,"t":"reponse","n":502,"ms":40002,"id":2,"etape":"fin","cmd":"capture changements","ok":true,"code":"ok","duree_ms":14}
<RS>{"v":1,"t":"config","n":503,"ms":40003,"capture":{"gpio":6,"resol_hz":1000000,"filtre_us":1,"silence_us":5000,"mode":"changements","inverse":true}}
<RS>{"v":1,"t":"reponse","n":510,"ms":40410,"id":3,"etape":"fin","cmd":"seuils 2 40000","ok":false,"code":"usage","msg":"seuils : silence 40000 us hors bornes a 1000000 Hz (1000..32767)","duree_ms":0}
```

````

par :

````markdown

```
<RS>{"v":1,"t":"reponse","n":502,"ms":40002,"id":2,"etape":"fin","cmd":"capture changements","ok":true,"code":"ok","duree_ms":14}
<RS>{"v":1,"t":"config","n":503,"ms":40003,"capture":{"gpio":6,"resol_hz":1000000,"filtre_us":1,"silence_us":5000,"mode":"changements","inverse":true},"injection":{"gpio":7,"bas_max_us":3000,"total_max_us":200000,"silence_min_us":20000,"attente_max_ms":1000,"delai_min_ms":3000,"arme_max_s":600,"tol_us":20}}
<RS>{"v":1,"t":"reponse","n":510,"ms":40410,"id":3,"etape":"fin","cmd":"seuils 2 40000","ok":false,"code":"usage","msg":"seuils : silence 40000 us hors bornes a 1000000 Hz (1000..32767)","duree_ms":0}
```

````

Remplacer :

````markdown
>
```

### 8.6 Événement `injection`

Format seulement (les commandes viennent avec l'étage d'injection) :

```
<RS>{"v":1,"t":"reponse","n":900,"ms":123001,"id":42,"etape":"fin","cmd":"injecte durees 750 750 750 2250","ok":true,"code":"accepte","duree_ms":1,"suite":"injection"}
<RS>{"v":1,"t":"injection","n":901,"ms":123456,"id":42,"cmd":"injecte durees 750 750 750 2250","resultat":"ok","niv0":"bas","dur_us":[750,750,750,2250],"attente_us":20412,"relu_us":[752,748,751,2249]}
```

## 9. Transport réseau : UDP sur le Wi-Fi
````

par :

````markdown
>
```

### 8.6 Injection

Par l'USB, étage monté (au banc, tâche 24) ; `injection monte 1` a été envoyé
une fois (NVS) :

```
id=40 injection on
id=41 injecte durees 750 750 750 2250
id=42 injecte durees 750
```

L'injection est armée pour 600 s. La première demande est acceptée ; le bus est
au repos, elle part aussitôt. Ses trois premiers paliers sont relus (le dernier,
haut, se fond dans le repos), puis la capture rend la même trame. La seconde
arrive dans les 3 s : délai minimal.

```
<RS>{"v":1,"t":"reponse","n":898,"ms":122500,"id":40,"etape":"fin","cmd":"injection on","ok":true,"code":"ok","msg":"injection armee pour 600 s ('injection off' pour desarmer)","duree_ms":0}
<RS>{"v":1,"t":"reponse","n":900,"ms":123001,"id":41,"etape":"fin","cmd":"injecte durees 750 750 750 2250","ok":true,"code":"accepte","duree_ms":0,"suite":"injection"}
<RS>{"v":1,"t":"injection","n":901,"ms":123006,"id":41,"cmd":"injecte durees 750 750 750 2250","resultat":"ok","niv0":"bas","dur_us":[750,750,750,2250],"attente_us":412,"relu_us":[752,748,751]}
<RS>{"v":1,"t":"trame","n":902,"ms":123011,"num":2204,"part":0,"fin":true,"t_us":123001455,"niv0":"bas","dur_us":[751,749,751],"debord":false}
<RS>{"v":1,"t":"etat","n":903,"ms":124000,"bloc":"injection","boot":"3FA2C901","up_s":124,"montee":true,"armee":true,"arme_reste_s":599,"derniere":{"id":41,"resultat":"ok"}}
<RS>{"v":1,"t":"reponse","n":905,"ms":124100,"id":42,"etape":"fin","cmd":"injecte durees 750","ok":false,"code":"refuse","msg":"injecte : delai minimal (3000 ms entre deux injections)","duree_ms":0}
```

Collision (critère 5 du banc) : le générateur tire la ligne 500 µs toutes les
50 ms ; son impulsion tombe dans le second palier haut de 20 ms. L'émission
s'arrête au front anormal : la dernière impulsion basse n'est jamais émise.
Puis une demande pendant un trafic continu (motif `wtc`, pauses de 6 ms) :
aucun silence de 20 ms en 1 s. Une autre session, ouverte en même temps, voit
la même injection sans son `id`.

```
<RS>{"v":1,"t":"injection","n":1210,"ms":161052,"id":43,"cmd":"injecte durees 500 20000 500 20000 500","resultat":"collision","niv0":"bas","dur_us":[500,20000,500,20000,500],"attente_us":19630,"relu_us":[502,19998,503,8012]}
<RS>{"v":1,"t":"injection","n":1388,"ms":170230,"id":44,"cmd":"injecte durees 1000","resultat":"delai","niv0":"bas","dur_us":[1000],"attente_us":1000204,"relu_us":[]}
<RS>{"v":1,"t":"injection","n":77,"ms":161052,"id":null,"cmd":"injecte durees 500 20000 500 20000 500","resultat":"collision","niv0":"bas","dur_us":[500,20000,500,20000,500],"attente_us":19630,"relu_us":[502,19998,503,8012]}
```

Refus : sans `injection on`, et `injection regle` à distance (USB seulement) :

```
<RS>{"v":1,"t":"reponse","n":52,"ms":95012,"id":8,"etape":"fin","cmd":"injecte durees 750 750","ok":false,"code":"refuse","msg":"injecte : non armee ('injection on')","duree_ms":0}
<RS>{"v":1,"t":"reponse","n":53,"ms":95230,"id":9,"etape":"fin","cmd":"injection regle tol_us 30","ok":false,"code":"interdite","msg":"interdite a distance (10.5) : USB seulement","duree_ms":0}
```

## 9. Transport réseau : UDP sur le Wi-Fi
````

Remplacer :

```markdown
- `json periode 2000..60000`, `json compteurs 0|1000..60000`,
  `json reseau 0|10000..60000`, `json trames 0|1`, `json log 0|1` ;
- `capture on|off|tout|changements`, `seuils <valeurs>` (bornés, sans texte) ;
- `injection on|off`, `injecte ...` (avec l'étage d'injection).

Tout le reste reçoit la `reponse` `interdite`, et rien n'est exécuté : en
particulier `json` seul, `json cle ...`, `wifi`, `injection monte`, `reboot`,
`bus`, `stats`, `info` et `help`.

### 9.5 Profil distant

```

par :

```markdown
- `json periode 2000..60000`, `json compteurs 0|1000..60000`,
  `json reseau 0|10000..60000`, `json trames 0|1`, `json log 0|1` ;
- `capture on|off|tout|changements`, `seuils <valeurs>` (bornés, sans texte) ;
- `injection on|off`, `injecte ...` (section 5.1).

Tout le reste reçoit la `reponse` `interdite`, et rien n'est exécuté : en
particulier `json` seul, `json cle ...`, `wifi`, `injection` seul,
`injection monte`, `injection regle`, `reboot`, `bus`, `stats`, `info` et
`help`.

### 9.5 Profil distant

```

- [ ] **Étape 8 : Relancer toutes les vérifications**

Lancer : `sh tools/tests/test_hote.sh | grep -A2 "^40 ligne"` puis `sh tools/tests/test_hote.sh | tail -1`
Attendu :
```
40 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
plus longue ligne : 526 octets (docs/PROTOCOLE-JSON.md exemple 12)
par type : compteurs/sonde 1, config 2, etat/bus 1, etat/capture 1, etat/injection 2, etat/sys 1, fin 1, hb 1, hello/base 2, hello/identite 1, injection 4, log 2, reponse 15, reseau/ip 1, trame 5
tests hote : OK
```
Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | tail -3` ; `~/.platformio/penv/bin/pio run -e sonde 2>&1 | tail -3`
Attendu : « Ran 157 tests », « OK » ; « sonde          SUCCESS ».

- [ ] **Étape 9 : Commit**

```bash
git add src/injection_regles.h src/injection_regles.cpp tools/tests/test_injection.cpp tools/json_check.py tools/tests/test_json_check.py src/bord.h src/bord.cpp src/json_out.h src/injection.h src/injection.cpp src/json_mode.h src/json_mode.cpp src/cli.cpp src/main.cpp docs/PROTOCOLE-JSON.md
git commit -m "Ajouter l'injection sur GPIO7 avec arret sur collision" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 23b : Commande `impulsions` du générateur et procédure du critère 5 (`docs/BANC.md` §9)

La tâche 24 est un banc avec Majid : elle n'écrit que des résultats. Le code
dont elle a besoin (la commande `impulsions` du générateur) et sa procédure
(`docs/BANC.md` §9) s'écrivent ici, et partent dans un commit avant le banc :
les deux firmwares y seront flashés depuis un arbre propre. **Le banc n'a
jamais de liaison avec la hotte** (`docs/BANC.md` §2.1) : le §9 le rappelle,
et l'étage d'injection monté pour le critère 5 est démonté à la fin de la
séance, contrôle au multimètre compris.

**Fichiers :**
- Modifier : `tools/tests/test_generateur.cpp` (`testImpulsions`)
- Modifier : `src/gen_symboles.h` (`kImpulsionMinUs`, `kImpulsionMaxUs`, `kPeriodeMaxMs`, `impulsionValide`)
- Modifier : `src/gen_main.cpp` (commande `impulsions <bas_us> <periode_ms> [n]`)
- Modifier : `docs/BANC.md` (§1, §3, §6 ; ligne `24` de « Montage et firmwares » et tableau « Critère 5 » du §7 ; §9 ajouté en fin de fichier)
- Tester : `sh tools/tests/test_hote.sh`, `~/.platformio/penv/bin/pio run -e generateur`, contrôle du texte par `grep`, `python3 -m unittest discover -s tools/tests`

**Interfaces :**
- Consomme : générateur de la tâche 14 (`Course`, `avancer`, `envoyer`, `poserNiveau`, `interrompre`, `arreter`, `lireU32`, `gen::symboles`, `motif wtc 0`, `stop`) et son test `tools/tests/test_generateur.cpp` ; le firmware `sonde` de la tâche 23 (`injection monte|on|off|regle`, `injecte durees ...`, événement `injection`, `trame`) ; `tools/serie_enregistre.py <port> <scénario> [commande ...] --duree s` (tâche 12 ; il affiche `id=<n> <commande> : <code> (<msg>)` à chaque réponse `fin`) ; `tools/hotte_udp.py session` (tâche 20) ; `tools/json_check.py --jsonl` (tâches 11 et 23) ; `docs/BANC.md` (tâche 15 : §1, §2.1, §3, §6, §7 ; tâche 16b : `tol_us` proposé au §7 ; tâche 20b : ligne `21` de « Montage et firmwares », dernier paragraphe du §8).
- Produit : la commande `impulsions` du générateur ; `docs/BANC.md` §9 (9.1 à 9.6), suivi à la lettre par la tâche 24, et les tableaux du §7 qu'elle remplit (ligne `24` de « Montage et firmwares », « Critère 5 : collision (tâche 24) »).

**Pourquoi une commande de plus au générateur.** Aucun motif ne garantit la collision : il faut des silences d'au moins 20 ms (sinon la sonde attend puis rend `delai`), et une activité certaine pendant les 40 ms que dure l'injection d'essai. `impulsions 500 50` tire la ligne 500 µs toutes les 50 ms. La sonde part 20 ms au moins après une impulsion, et la suivante tombe forcément dans ses deux premiers paliers. La dernière impulsion basse de la sonde, 2 500 µs à 41 ms du départ, n'est donc jamais atteinte avant la collision. Si on la retrouve dans la capture, l'émission ne s'est pas arrêtée (`docs/BANC.md` §9.3).

**Décisions :**
- **Sans liaison avec la hotte** : sonde sur le boîtier détaché (hotte débranchée toute la séance, fiche XH 4 broches retirée, deux **OL** avant tout USB) ou sur la plaque d'essai du banc (`docs/BANC.md` §2.1). L'étage d'injection se pose sur l'un ou l'autre, les deux USB débranchés.
- **Démontage à la fin** (§9.6) : `injection monte 0`, puis, les deux USB débranchés, Q2, le 4,7k, le 10k et les 470 Ω retirés (ou au moins le collecteur de Q2 dessoudé de la LIGNE) ; contrôle `TP_Dp` vers la broche 7 des barrettes, dans les deux sens : plus de 100 kΩ. Le tableau du critère 5 a sa ligne « étage d'injection démonté : oui / non ».
- **`tol_us`** : si la tâche 16b en propose un au-dessus de 20 µs, il est réglé au §9.1, avant la première injection.

- [ ] **Étape 1 : Écrire le test qui échoue**

Dans `tools/tests/test_generateur.cpp` :

Remplacer :

```cpp
  VERIF(ok);
}

int main() {
  testNiveaux();
  testCasSimples();
  testTousLesMotifs();
  testRafale();
  return bilan("test_generateur");
}
```

par :

```cpp
  VERIF(ok);
}

// Commande 'impulsions' (critere 5 du banc) : bornes, et une impulsion basse
// devient un symbole de deux moities GPIO haut (ligne basse).
static void testImpulsions() {
  VERIF(gen::impulsionValide(500, 50));
  VERIF(gen::impulsionValide(gen::kImpulsionMinUs, 2));
  VERIF(!gen::impulsionValide(gen::kImpulsionMinUs - 1, 50));
  VERIF(gen::impulsionValide(gen::kImpulsionMaxUs, 31));
  VERIF(!gen::impulsionValide(gen::kImpulsionMaxUs + 1, 60));
  VERIF(!gen::impulsionValide(gen::kImpulsionMaxUs, 30));  // au moins 1 ms de ligne haute entre deux
  VERIF(!gen::impulsionValide(500, 1));
  VERIF(gen::impulsionValide(500, gen::kPeriodeMaxMs));
  VERIF(!gen::impulsionValide(500, gen::kPeriodeMaxMs + 1));
  VERIF(!gen::impulsionValide(500, 0));
  gen::Sym y[2] = {};
  const Seg s[] = {{false, 500}};
  VERIF(gen::symboles(s, 1, y, 2) == 1);
  VERIF(y[0].d0 == 250 && y[0].l0 == 1 && y[0].d1 == 250 && y[0].l1 == 1);
  const Seg l[] = {{false, gen::kImpulsionMaxUs}};
  VERIF(gen::symboles(l, 1, y, 2) == 1);
  VERIF(y[0].d0 + y[0].d1 == gen::kImpulsionMaxUs && y[0].l0 == 1 && y[0].l1 == 1);
}

int main() {
  testNiveaux();
  testCasSimples();
  testTousLesMotifs();
  testRafale();
  testImpulsions();
  return bilan("test_generateur");
}
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`
Attendu : ÉCHEC à la compilation de `test_generateur`, avec « tools/tests/test_generateur.cpp:102:14: error: no member named 'impulsionValide' in namespace 'gen' ».

- [ ] **Étape 3 : Écrire la commande `impulsions`**

Dans `src/gen_symboles.h` :

Après :

```cpp
  return k / 2;
}

```

insérer :

```cpp
// Commande 'impulsions <bas_us> <periode_ms> [n]' (critere 5 du banc : une
// collision pendant une injection de la sonde) : une impulsion basse de
// bas_us, repetee toutes les periode_ms, ligne haute entre deux. Bornes :
// 10..30000 us, 60 s de periode au plus, et au moins 1 ms de ligne haute
// entre deux impulsions.
constexpr uint32_t kImpulsionMinUs = 10;
constexpr uint32_t kImpulsionMaxUs = 30000;
constexpr uint32_t kPeriodeMaxMs = 60000;
inline bool impulsionValide(uint32_t basUs, uint32_t periodeMs) {
  return basUs >= kImpulsionMinUs && basUs <= kImpulsionMaxUs && periodeMs <= kPeriodeMaxMs &&
         (uint64_t)periodeMs * 1000u >= (uint64_t)basUs + 1000u;
}

```

Dans `src/gen_main.cpp` :

Remplacer :

```cpp
//    motif <nom> [n] [pause_ms]   n trames (defaut 1000, rafale 1 ; 0 : sans
//                                 fin), pause apres chaque trame (defaut :
//                                 celle du motif, motif::pauseUs)
//    stop                         arrete et relache la ligne (GPIO7 bas)
//    etat                         motif, trame, pause, niveau de la ligne
//    help                         commandes et motifs
//  A chaque trame emise : 'motif <nom> trame <index>' (index depuis 0).
//
//  Deroulement : prise du repos (un symbole bref au niveau du repos, qui y
//  laisse la ligne : eot_level), pause, trame 0, pause, trame 1, ... Chaque
```

par :

```cpp
//    motif <nom> [n] [pause_ms]   n trames (defaut 1000, rafale 1 ; 0 : sans
//                                 fin), pause apres chaque trame (defaut :
//                                 celle du motif, motif::pauseUs)
//    impulsions <bas_us> <periode_ms> [n]
//                                 impulsion basse de bas_us toutes les
//                                 periode_ms, n fois (defaut 0 : sans fin) ;
//                                 collision du critere 5 (docs/BANC.md 9)
//    stop                         arrete et relache la ligne (GPIO7 bas)
//    etat                         motif, trame, pause, niveau de la ligne
//    help                         commandes et motifs
//  A chaque trame emise : 'motif <nom> trame <index>' (index depuis 0) ;
//  a chaque impulsion : 'impulsion <index>'.
//
//  Deroulement : prise du repos (un symbole bref au niveau du repos, qui y
//  laisse la ligne : eot_level), pause, trame 0, pause, trame 1, ... Chaque
```

Après :

```cpp
enum class Etape : uint8_t { Arret, Emission, Pause, Fini };
struct Course {
  motif::Id id = motif::Id::Uart500;
```

insérer :

```cpp
  bool impulsions = false;  // commande 'impulsions' au lieu d'un motif
  uint32_t basUs = 0;       // impulsions : duree basse
```

Après :

```cpp
  Serial.printf(", pause %lu us, repos %s\n", (unsigned long)pauseUs, texteRepos(sC.reposHaut));
}

```

insérer :

```cpp
// 'impulsions' : pause comptee depuis la fin de chaque impulsion, soit une
// periode de bas_us + pause (plus quelques dizaines de us).
void demarrerImpulsions(uint32_t basUs, uint32_t periodeMs, uint32_t n) {
  if (sC.etape == Etape::Emission || sC.etape == Etape::Pause) interrompre();
  sC = Course{};
  sC.impulsions = true;
  sC.basUs = basUs;
  sC.n = n;
  sC.pauseUs = periodeMs * 1000 - basUs;
  sC.reposHaut = true;
  if (!poserNiveau(true)) return;  // etape Arret
  sC.etape = Etape::Emission;
  if (n) Serial.printf("impulsions : %lu", (unsigned long)n);
  else Serial.print("impulsions : sans fin");
  Serial.printf(", %lu us basses toutes les %lu ms, ligne haute entre deux\n", (unsigned long)basUs,
                (unsigned long)periodeMs);
}

```

Remplacer :

```cpp
  }
  if (sC.n && sC.index >= sC.n) {
    sC.etape = Etape::Fini;
    Serial.printf("motif %s fini : %lu trames, ligne au repos %s\n", motif::nom(sC.id), (unsigned long)sC.n,
                  texteRepos(sC.reposHaut));
    return;
  }
  bool ok;
  if (sC.id == motif::Id::Rafale) {
    ok = envoyer(sRafale, gen::kRafaleBoucle, true, (int)gen::kRafaleTours);
  } else {
    const size_t ns = motif::trame(sC.id, sC.index, sSeg, motif::kSegMax);
```

par :

```cpp
  }
  if (sC.n && sC.index >= sC.n) {
    sC.etape = Etape::Fini;
    if (sC.impulsions) Serial.printf("impulsions finies : %lu, ligne haute\n", (unsigned long)sC.n);
    else
      Serial.printf("motif %s fini : %lu trames, ligne au repos %s\n", motif::nom(sC.id), (unsigned long)sC.n,
                    texteRepos(sC.reposHaut));
    return;
  }
  bool ok;
  if (sC.impulsions) {
    const motif::Seg s{false, sC.basUs};
    const size_t ny = gen::symboles(&s, 1, sY, gen::kSymMax);
    for (size_t i = 0; i < ny; i++) sSym[i] = mot(sY[i]);
    ok = ny && envoyer(sSym, ny, true, 0);
  } else if (sC.id == motif::Id::Rafale) {
    ok = envoyer(sRafale, gen::kRafaleBoucle, true, (int)gen::kRafaleTours);
  } else {
    const size_t ns = motif::trame(sC.id, sC.index, sSeg, motif::kSegMax);
```

Remplacer :

```cpp
    ok = ny && envoyer(sSym, ny, sC.reposHaut, 0);
  }
  if (!ok) {
    Serial.printf("motif %s : trame %lu impossible, arret\n", motif::nom(sC.id), (unsigned long)sC.index);
    arreter();
    return;
  }
  sC.etape = Etape::Emission;
  Serial.printf("motif %s trame %lu\n", motif::nom(sC.id), (unsigned long)sC.index);
  sC.index++;
}

```

par :

```cpp
    ok = ny && envoyer(sSym, ny, sC.reposHaut, 0);
  }
  if (!ok) {
    if (sC.impulsions) Serial.printf("impulsion %lu impossible, arret\n", (unsigned long)sC.index);
    else Serial.printf("motif %s : trame %lu impossible, arret\n", motif::nom(sC.id), (unsigned long)sC.index);
    arreter();
    return;
  }
  sC.etape = Etape::Emission;
  if (sC.impulsions) Serial.printf("impulsion %lu\n", (unsigned long)sC.index);
  else Serial.printf("motif %s trame %lu\n", motif::nom(sC.id), (unsigned long)sC.index);
  sC.index++;
}

```

Après :

```cpp
void aide() {
  Serial.println("=== Generateur du banc ===");
  Serial.println("  motif <nom> [n] [pause_ms]  n trames (defaut 1000, rafale 1 ; 0 : sans fin), pause apres chaque trame");
```

insérer :

```cpp
  Serial.println("  impulsions <bas_us> <periode_ms> [n]  impulsion basse repetee (n : defaut 0, sans fin), critere 5");
```

Remplacer :

```cpp
  switch (sC.etape) {
    case Etape::Arret: Serial.println("etat : arret, ligne relachee (GPIO7 bas)"); return;
    case Etape::Fini:
      Serial.printf("etat : fini, motif %s, %lu trames, ligne au repos %s\n", motif::nom(sC.id), (unsigned long)sC.n,
                    texteRepos(sC.reposHaut));
      return;
    case Etape::Emission:
    case Etape::Pause: break;
  }
  Serial.printf("etat : motif %s, %lu trames emises", motif::nom(sC.id), (unsigned long)sC.index);
  if (sC.n) Serial.printf(" sur %lu", (unsigned long)sC.n);
```

par :

```cpp
  switch (sC.etape) {
    case Etape::Arret: Serial.println("etat : arret, ligne relachee (GPIO7 bas)"); return;
    case Etape::Fini:
      if (sC.impulsions) Serial.printf("etat : fini, %lu impulsions, ligne haute\n", (unsigned long)sC.n);
      else
        Serial.printf("etat : fini, motif %s, %lu trames, ligne au repos %s\n", motif::nom(sC.id),
                      (unsigned long)sC.n, texteRepos(sC.reposHaut));
      return;
    case Etape::Emission:
    case Etape::Pause: break;
  }
  if (sC.impulsions) {
    Serial.printf("etat : impulsions de %lu us, %lu emises", (unsigned long)sC.basUs, (unsigned long)sC.index);
    if (sC.n) Serial.printf(" sur %lu", (unsigned long)sC.n);
    Serial.printf(", periode %lu us\n", (unsigned long)(sC.basUs + sC.pauseUs));
    return;
  }
  Serial.printf("etat : motif %s, %lu trames emises", motif::nom(sC.id), (unsigned long)sC.index);
  if (sC.n) Serial.printf(" sur %lu", (unsigned long)sC.n);
```

Après :

```cpp
}

void usageMotif() { Serial.println("usage : motif <nom> [n] [pause_ms 0..60000] ('help' : noms des motifs)"); }
```

insérer :

```cpp

void usageImpulsions() {
  Serial.println("usage : impulsions <bas_us 10..30000> <periode_ms 1..60000> [n] (1 ms de ligne haute au moins entre deux)");
}
```

Après :

```cpp
      pauseUs = pauseMs * 1000;
    }
    demarrer(id, nb, pauseUs);
```

insérer :

```cpp
  } else if (!strcmp(mots[0], "impulsions")) {
    uint32_t bas = 0, periode = 0, nb = 0;
    if (n < 3 || n > 4 || !lireU32(mots[1], &bas) || !lireU32(mots[2], &periode) ||
        (n == 4 && !lireU32(mots[3], &nb)) || !gen::impulsionValide(bas, periode))
      return usageImpulsions();
    demarrerImpulsions(bas, periode, nb);
```

- [ ] **Étape 4 : Lancer les tests et compiler le générateur**

Lancer : `sh tools/tests/test_hote.sh | grep -E "test_generateur|tests hote"`
Attendu : « test_generateur : 34 verifications, 0 echecs », « tests hote : OK ».
Lancer : `~/.platformio/penv/bin/pio run -e generateur 2>&1 | grep -E "warning|error|RAM:|Flash:|SUCCESS|FAILED"`
Attendu : aucune ligne `warning` ni `error` ; `RAM:   [=         ]   5.6% (used 18216 bytes from 327680 bytes)`, `Flash: [==        ]  21.5% ...`, `generateur     SUCCESS`.

- [ ] **Étape 5 : Écrire la procédure du critère 5 dans `docs/BANC.md`**

Dans `docs/BANC.md` (matériel, commandes du générateur, critère 5 du §6, deux tableaux du §7, puis le §9 ajouté en fin de fichier) :

Remplacer :

```markdown
- pull-up de ligne : 10k (variante 1) ; 4,7k et 1 nF (variante 2) ;
- fils Dupont, un fil de masse ;
- multimètre (contrôles, critère 6) ;
- plus tard : l'analyseur FX2 (critère 2 complet) et l'étage d'injection (critère 5).

## 2. Montage

```

par :

```markdown
- pull-up de ligne : 10k (variante 1) ; 4,7k et 1 nF (variante 2) ;
- fils Dupont, un fil de masse ;
- multimètre (contrôles, critère 6) ;
- plus tard : l'analyseur FX2 (critère 2 complet) ; l'étage d'injection de la
  sonde (critère 5, §9) : Q2 (BC547 ou 2N3904), 4,7k, 10k, 470 Ω.

## 2. Montage

```

Remplacer :

```markdown
- **Console du générateur**, dans un terminal à part, ouvert pendant tout le banc :
  `~/.platformio/penv/bin/pio device monitor -e generateur -p $PORT_GEN`.
  Commandes : `motif <nom> [n] [pause_ms]` (1 000 trames par défaut, 1 pour la
  rafale ; 0 : sans fin), `stop`, `etat`, `help`. Il affiche
  `motif <nom> trame <index>` à chaque trame, puis `motif <nom> fini : <n> trames`.
- **Enregistrement de la sonde** (mode machine par l'USB, fichier
  `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl`) :
  `python3 tools/serie_enregistre.py $PORT_SONDE <scénario> "capture tout" "seuils 1 <silence>" --duree <s>`.
```

par :

```markdown
- **Console du générateur**, dans un terminal à part, ouvert pendant tout le banc :
  `~/.platformio/penv/bin/pio device monitor -e generateur -p $PORT_GEN`.
  Commandes : `motif <nom> [n] [pause_ms]` (1 000 trames par défaut, 1 pour la
  rafale ; 0 : sans fin), `impulsions <bas_us> <periode_ms> [n]` (critère 5,
  §9 ; 0 ou rien : sans fin), `stop`, `etat`, `help`. Il affiche
  `motif <nom> trame <index>` à chaque trame, puis `motif <nom> fini : <n> trames`
  (`impulsion <index>` à chaque impulsion).
- **Enregistrement de la sonde** (mode machine par l'USB, fichier
  `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl`) :
  `python3 tools/serie_enregistre.py $PORT_SONDE <scénario> "capture tout" "seuils 1 <silence>" --duree <s>`.
```

Remplacer :

```markdown
| 2 | Durées à ±2 µs du nominal, une fois corrigé le décalage de l'étage ; asymétrie inférieure à 5 µs, ou documentée et compensée | sans FX2 : `banc.py` donne le `decalage suggere` (asymétrie de toute la chaîne), puis `--decalage-us` et `critere 2 : ... OK`. Avec le FX2 : une voie sur la LIGNE à travers 47k/68k, une voie sur GPIO6, délai et asymétrie de l'étage mesurés | 16 (sans FX2), puis à l'arrivée du FX2 |
| 3 | Rafale : aucune perte, ou une perte signalée par `debord`, et le débit maximal documenté | `banc.py <capture> rafale` : `critere 3`, compteurs de la sonde (`debord`, `lignes_perdues`, `sautes`) | 16 |
| 4 | 10 min de `krona` par le Wi-Fi, mode `tout`, sonde à son emplacement de test : zéro perte côté sonde, trous de `n` côté Mac sous 0,1 %, avec le RSSI | `hotte_udp.py enregistre`, `json_check.py --jsonl`, compteurs | 21 |
| 5 | Collision détectée et émission arrêtée ; à défaut, la limite est documentée | étage d'injection monté au banc, le générateur émet pendant une injection | 24 |
| 6 | Consommation de la sonde relevée, Wi-Fi actif | USB d'abord (tâche 16), Wi-Fi actif ensuite | 16 (USB), 21 (Wi-Fi) |

## 7. Résultats
```

par :

```markdown
| 2 | Durées à ±2 µs du nominal, une fois corrigé le décalage de l'étage ; asymétrie inférieure à 5 µs, ou documentée et compensée | sans FX2 : `banc.py` donne le `decalage suggere` (asymétrie de toute la chaîne), puis `--decalage-us` et `critere 2 : ... OK`. Avec le FX2 : une voie sur la LIGNE à travers 47k/68k, une voie sur GPIO6, délai et asymétrie de l'étage mesurés | 16 (sans FX2), puis à l'arrivée du FX2 |
| 3 | Rafale : aucune perte, ou une perte signalée par `debord`, et le débit maximal documenté | `banc.py <capture> rafale` : `critere 3`, compteurs de la sonde (`debord`, `lignes_perdues`, `sautes`) | 16 |
| 4 | 10 min de `krona` par le Wi-Fi, mode `tout`, sonde à son emplacement de test : zéro perte côté sonde, trous de `n` côté Mac sous 0,1 %, avec le RSSI | `hotte_udp.py enregistre`, `json_check.py --jsonl`, compteurs | 21 |
| 5 | Collision détectée et émission arrêtée ; à défaut, la limite est documentée | étage d'injection monté au banc (§9) ; `impulsions 500 50` du générateur pendant une injection : résultat `collision`, et la trame capturée s'arrête à l'impulsion du générateur | 24 |
| 6 | Consommation de la sonde relevée, Wi-Fi actif | USB d'abord (tâche 16), Wi-Fi actif ensuite | 16 (USB), 21 (Wi-Fi) |

## 7. Résultats
```

Après :

```markdown
| | 21 | 1 (10k) | | | | | | | |
```

insérer :

```markdown
| | 24 | 1 (10k) | | | | | | | |
```

Remplacer :

```markdown

### Critère 5 : collision (tâche 24)

| Date | Essai | Résultat de `injection` | Émission arrêtée ? | Remarque |
|---|---|---|---|---|
| | | | | |

### Critère 6 : consommation de la sonde

```

par :

```markdown

### Critère 5 : collision (tâche 24)

| Date | Essai (§9) | Résultat de `injection` | Émission arrêtée ? | Remarque |
|---|---|---|---|---|
| | 9.1 refus : sans montage, sans armement | `non montee`, `non armee` | — | |
| | 9.2 injection simple × 10 | `ok` : __ / 10 ; écart max relu − émis : __ µs | — | trame capturée identique (±2 µs après décalage) : __ / 10 ; `delai minimal` vu : oui / non |
| | 9.3 collision × 10 | `collision` : __ / 10 | bas le plus long sous 2 000 µs : __ / 10 | |
| | 9.4 bus jamais silencieux | `delai`, attente __ µs | `relu_us` vide : oui / non | |
| | 9.5 désarmement seul (30 s) | annonce reçue : oui / non ; puis `non armee` : oui / non | — | |
| | 9.6 fin | GPIO7 au repos : __ V | — | `injection monte 0` envoyé : oui / non |
| | 9.6 démontage, USB débranchés | étage d'injection démonté : oui / non | — | `TP_Dp` → broche 7, deux sens : __ / __ kΩ (plus de 100 kΩ) |

### Critère 6 : consommation de la sonde

```

Après :

```markdown
Si le critère échoue : noter le RSSI et `wifi.pertes`, rapprocher la sonde du
point d'accès et recommencer ; sinon, reprendre le risque « Wi-Fi trop
faible » du §13 de la spec (enregistrement local, relu par l'USB).
```

insérer :

````markdown

## 9. Injection et collision (critère 5)

Même montage (§2, variante 1), **toujours sans aucune liaison avec la hotte**
(§2.1) : sonde sur le boîtier détaché (hotte débranchée pendant toute la
séance, fiche XH 4 broches retirée de l'embase, les deux **OL** relevés avant
tout USB), ou sur la plaque d'essai du banc. On y ajoute l'**étage d'injection
de la sonde**
([WIRING.md §6](WIRING.md#6-étage-dinjection-voie-1--gpio7-vers-d_panneau-étape-7-seulement)),
son collecteur sur la LIGNE : sur le boîtier, à la place prévue (WIRING §4) ;
sur la plaque d'essai, à côté de l'étage d'écoute. Il n'est monté que pour ce
paragraphe et se démonte à la fin (§9.6). Tout se câble **les deux câbles USB
débranchés** :

```
  GPIO7 (sonde) ── 4,7k ──┬── base de Q2
                         10k
  TP− (GND sonde) ────────┴── émetteur de Q2
  LIGNE ── 470 Ω ──────────── collecteur de Q2      (GPIO7 haut → LIGNE tirée bas)
```

Contrôles hors tension, au multimètre, **avant tout USB** : GPIO7 de la sonde
n'est relié qu'au 4,7k ; GPIO6 n'est relié qu'au collecteur de Q1 et à son 10k
(**jamais GPIO6 et GPIO7 intervertis**) ; LIGNE vers masse, plus de 10 kΩ ; sur
le boîtier, les deux **OL** du §2.1 (`TP−` vers la terre de la fiche de la
hotte, puis vers la vis de la carcasse). Aucun secteur : les deux C6 sont sur
l'USB du Mac.

La sonde parle au Mac soit par `tools/serie_enregistre.py` (mode machine,
fichier `.jsonl`), soit par une console humaine
(`~/.platformio/penv/bin/pio device monitor -p $PORT_SONDE -b 115200`), jamais
les deux à la fois. Résumé des injections d'un ou de plusieurs
enregistrements : chaque événement `injection`, chaque trame capturée de plus
d'une durée, et le plus long palier bas capturé :

```
python3 -c "
import json, os, sys
for f in sys.argv[1:]:
    n = os.path.basename(f)
    ms = [json.loads(l)['l'] for l in open(f)]
    for m in ms:
        if m['t'] == 'injection':
            print(n, 'injection', m['id'], m['resultat'], 'attente', m['attente_us'], 'emises', m['dur_us'], 'relues', m['relu_us'])
        elif m['t'] == 'trame' and len(m['dur_us']) > 1:
            print(n, 'trame', m['niv0'], m['dur_us'])
    bas = [d for m in ms if m['t'] == 'trame' for i, d in enumerate(m['dur_us']) if (i % 2 == 0) == (m['niv0'] == 'bas')]
    print(n, 'bas le plus long :', max(bas) if bas else None, 'us')
" logs/<fichier>.jsonl [...]
```

### 9.1 Au repos, puis refus

Console humaine de la sonde, générateur arrêté (`stop`).

1. `info` : `ecoute GPIO6, injection GPIO7 (tenue basse)`. Au multimètre :
   GPIO7 à 0 V, LIGNE à 5 V environ.
2. `injection` : `injection : etage non monte (GPIO7), desarmee`, puis les
   valeurs (`bas_max_us 3000, total_max_us 200000, ...`).
3. `injecte durees 1000` : `injecte : non montee ('injection monte 1' par l'USB)`.
4. `injection on` : `injection : non montee ('injection monte 1' par l'USB, etage monte)`.
5. `injection monte 1` : `injection : etage declare monte (GPIO7), desarmee`.
6. `injecte durees 1000` : `injecte : non armee ('injection on')`.
7. Si le §7 (« Critère 2 à l'analyseur ») propose un `tol_us` au-dessus de 20 :
   `injection regle tol_us <valeur>` ; les valeurs affichées le montrent.
8. Fermer la console (Ctrl-C).

### 9.2 Injection simple, relue par la voie 1

Générateur arrêté (`stop`) : la LIGNE est au repos haut.

1. `python3 tools/serie_enregistre.py $PORT_SONDE banc-injection "injection on" "injecte durees 1500 750 750 750 750 2250 750" "injecte durees 1000" --duree 6`.
   Attendu : `id=2 injection on : ok (injection armee pour 600 s ('injection off' pour desarmer))`,
   `id=3 injecte durees 1500 750 750 750 750 2250 750 : accepte`,
   `id=4 injecte durees 1000 : refuse (injecte : delai minimal (3000 ms entre deux injections))`.
2. Résumé (commande plus haut) sur le fichier écrit. Attendu :
   - `injection 3 ok attente <a> emises [1500, 750, 750, 750, 750, 2250, 750] relues [...]` :
     7 durées relues, chacune à 20 µs (`tol_us`) au plus de la durée émise ;
   - `trame bas [...]` : la même trame, capturée par la voie 1, 7 durées à
     ±2 µs des émises une fois corrigé le décalage retenu au §7.
3. `python3 tools/json_check.py --jsonl logs/<fichier>.jsonl` : 0 erreur.
4. Recommencer 10 fois (le délai minimal de 3 s est tenu : chaque lancement
   dure plus de 6 s) ; noter les `ok` et le plus grand écart relu − émis.

Si une injection simple donne `collision` : le délai des étages dépasse
`tol_us`. Noter l'écart, puis, par la console, `injection regle tol_us <écart + 10>`,
et recommencer ; la valeur retenue ira dans l'avenant de l'étape 6.

### 9.3 Collision provoquée

La sonde émet `500 20000 500 20000 2500` : deux impulsions basses de 500 µs
séparées de 20 ms, puis, 20 ms plus loin, une impulsion basse de **2 500 µs**,
plus longue que tout ce que la LIGNE porte d'autre. Le générateur tire la ligne
500 µs toutes les 50 ms. La sonde attend 20 ms de silence après une impulsion :
la suivante tombe donc toujours avant la fin du second palier haut. Si
l'émission s'arrête à la collision, le palier bas de 2 500 µs ne passe jamais
sur la LIGNE.

1. Générateur : `impulsions 500 50` (sans fin). Il affiche `impulsion 0`,
   `impulsion 1`...
2. Sonde, dix fois :
   `for i in 1 2 3 4 5 6 7 8 9 10; do python3 tools/serie_enregistre.py $PORT_SONDE banc-collision-$i "seuils 1 30000" "injection on" "injecte durees 500 20000 500 20000 2500" --duree 4; done`.
   Le silence de capture de 30 ms réunit l'injection et l'impulsion qui la
   heurte en une seule réception ; les impulsions suivantes, 49,5 ms plus
   loin, font chacune la leur.
3. Générateur : `stop`.
4. Résumé sur les dix fichiers (`logs/*-banc-collision-*.jsonl`). Attendu pour
   chacun :
   - `injection 4 collision ... relues [...]` (`id` 4 : `injecte` est la
     troisième commande donnée, après `json 1 bail 0` qui prend l'`id` 1) :
     de 1 à 4 durées relues, la dernière finissant au front où la collision a
     été vue (l'impulsion du générateur, ou sa fin si elle a commencé pendant
     un palier bas de la sonde) ;
   - `bas le plus long : <500 à 1 000> us`, **sous 2 000 µs** : l'impulsion de
     2 500 µs n'est jamais partie, **l'émission s'est arrêtée**. Sans arrêt,
     on lirait 2 500.

### 9.4 Bus jamais silencieux

1. Générateur : `motif wtc 0` (sans fin : pauses de 6 ms, jamais 20 ms de
   silence).
2. Sonde : `python3 tools/serie_enregistre.py $PORT_SONDE banc-delai "injection on" "injecte durees 1000" --duree 4`.
3. Résumé. Attendu : `injection 3 delai attente <environ 1000000> emises [1000] relues []` :
   rien n'est émis.
4. Générateur : `stop`.

### 9.5 Désarmement seul

Console humaine de la sonde.

1. `injection regle arme_max_s 30`, puis `injection on` :
   `injection armee pour 30 s ('injection off' pour desarmer)`.
2. Attendre 35 s : `[injection] desarmee seule (30 s ecoulees)` s'affiche.
3. `injecte durees 1000` : `injecte : non armee ('injection on')`.
4. `injection regle arme_max_s 600`.

### 9.6 Fin

1. Console : `injection monte 0` (la sonde retournera sur la hotte sans étage
   d'injection jusqu'à l'étape 7), `seuils 1 5000`, puis `injection` :
   `injection : etage non monte (GPIO7), desarmee`.
2. Au multimètre : GPIO7 à 0 V, LIGNE à 5 V environ.
3. Si le Wi-Fi est en place (tâche 21) :
   `python3 tools/hotte_udp.py session hotte-sonde.local "injection regle tol_us 30" "injection monte 1" --duree 5`.
   Attendu : deux `reponse` `interdite` (USB seulement).
4. **Les deux câbles USB débranchés, démonter l'étage d'injection** : Q2, le
   4,7k, le 10k et les 470 Ω retirés, ou au moins le collecteur de Q2 dessoudé
   de la LIGNE. `injection monte 0` ne suffit pas : le boîtier ne retourne sur
   la hotte que sans étage d'injection (WIRING.md §6 : étape 7 seulement).
5. Contrôle hors tension, au multimètre, calibre le plus élevé : `TP_Dp` vers
   la broche 7 des barrettes (GPIO7 de la sonde ; sur la plaque d'essai, la
   broche GPIO7), dans les deux sens : **plus de 100 kΩ**. Sinon, l'étage
   n'est pas démonté : on finit de le retirer, puis on refait la mesure.
6. Noter les résultats au §7 (critère 5), démontage compris.

Si une collision n'est pas vue (résultat `ok` au §9.3), ou si l'émission ne
s'arrête pas (palier bas de 2 500 µs capturé) : on le note au §7 avec les durées
relues, et la limite part dans l'avenant de l'étape 6 (spec §8.4 : la collision
n'est alors constatée qu'après coup, par relecture).
````

- [ ] **Étape 6 : Vérifier la procédure et relancer les tests**

Lancer :

```bash
for m in "## 9. Injection et collision (critère 5)" "### 9.6 Fin" "**Les deux câbles USB débranchés, démonter l'étage d'injection**" "| | 24 | 1 (10k) | | | | | | | |" "étage d'injection démonté : oui / non" "impulsions <bas_us> <periode_ms> [n]" "au-dessus de 20 :"; do grep -q -F -- "$m" docs/BANC.md && echo "ok : $m" || echo "ABSENT : $m"; done
```

Attendu : sept lignes « ok : ... ».
Lancer : `grep -c '^### 9\.' docs/BANC.md` ; `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : `6` ; « Ran 157 tests », « OK ».

- [ ] **Étape 7 : Commit**

```bash
git add src/gen_symboles.h src/gen_main.cpp tools/tests/test_generateur.cpp docs/BANC.md
git commit -m "Ajouter la commande impulsions du generateur et la procedure du critere 5" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 24 [BANC avec Majid] : critère 5, injection relue et collision provoquée

Exécutée par l'agent principal avec Majid, jamais par un sous-agent. Montage de départ : celui de la tâche 16 (`docs/BANC.md` §2, variante 1), validée d'abord (décalage retenu au §7), plus l'étage d'injection du §9. La procédure est celle de `docs/BANC.md` §9 (tâche 23b) : cette tâche n'écrit que des résultats. Le §9.6, point 3, suppose le Wi-Fi et la clé de la tâche 21 ; sans eux, on le saute.

**Fichiers :**
- Modifier : `docs/BANC.md` (§7 seulement : ligne `24` de « Montage et firmwares », « Critère 5 : collision (tâche 24) », « Conclusions »)
- Tester : le banc ; `tools/serie_enregistre.py`, le résumé du §9, `tools/json_check.py --jsonl`

**Interfaces :**
- Consomme : le firmware `sonde` de la tâche 23 (`injection monte|on|off|regle`, `injecte durees ...`, événement `injection`, `trame`) ; le générateur de la tâche 23b (`impulsions <bas_us> <periode_ms> [n]`, `motif wtc 0`, `stop`) ; `tools/serie_enregistre.py <port> <scénario> [commande ...] --duree s` (tâche 12) ; `tools/hotte_udp.py session` (tâche 20) ; `tools/json_check.py --jsonl` (tâches 11 et 23) ; `docs/BANC.md` §2, §3, §7 et §9 (tâches 15, 16b, 20b et 23b).
- Produit : les résultats du critère 5 dans `docs/BANC.md` §7 ; la valeur de `tol_us` à reprendre dans l'avenant de l'étape 6.

**Règles pour cette tâche :**
- **Le banc n'a jamais de liaison avec la hotte** (`docs/BANC.md` §2.1) : la sonde n'est sur le boîtier de mesure détaché que si la hotte reste débranchée pendant toute la séance et si le câble de sortie est déconnecté du boîtier (fiche XH 4 broches retirée de l'embase) ; avant tout USB, `TP−` vers la terre de la fiche de la hotte puis vers la vis de la carcasse : **OL**. Sinon, plaque d'essai du banc avec son propre étage d'écoute (mêmes valeurs). Rien ne touche au secteur : les deux C6 sont sur l'USB du Mac.
- Tout se câble les deux câbles USB débranchés ; **jamais GPIO6 et GPIO7 intervertis** (WIRING.md §6).
- Aucun code ni document à écrire : seulement des résultats, dans les tableaux prévus par la tâche 23b. Les deux firmwares sont flashés depuis l'arbre propre de la tâche 23b.
- À la fin : `injection monte 0`, puis, les deux USB débranchés, l'étage d'injection **démonté** et contrôlé au multimètre (§9.6) : le boîtier ne retourne sur la hotte (étapes 5 et 5b) que sans étage d'injection.

- [ ] **Étape 1 : Vérifier le logiciel avant le banc (l'agent)**

Lancer : `git status --short`, `sh tools/tests/test_hote.sh | tail -1`, `python3 -m unittest discover -s tools/tests 2>&1 | tail -1`, `~/.platformio/penv/bin/pio run -e sonde -e generateur 2>&1 | tail -3`, `grep -c '^### 9\.' docs/BANC.md`
Attendu : rien (arbre propre, au commit de la tâche 23b), « tests hote : OK », « OK », `sonde          SUCCESS` et `generateur     SUCCESS`, `6` (§9.1 à §9.6). Si l'un échoue : on s'arrête, pas de banc.

- [ ] **Étape 2 : Montage, hors tension, sans liaison avec la hotte (Majid)**

Tous les câbles USB débranchés. Le montage de la tâche 16, variante 1 (`docs/BANC.md` §2.1 et §2.2), plus l'étage d'injection du §9 (Q2 : 4,7k de GPIO7 de la sonde à la base, 10k base-émetteur, émetteur au GND de la sonde, collecteur par 470 Ω sur la LIGNE). Contrôles au multimètre, hors tension, **avant de brancher le moindre USB**, notés au §7 (« Montage et firmwares », ligne `24`) :

| Mesure | Attendu | Sinon |
|---|---|---|
| boîtier seulement : hotte et câble de sortie | hotte débranchée (fiche hors de la prise, à vue) pour toute la séance ; fiche XH 4 broches du câble de sortie retirée de l'embase du boîtier | ne rien brancher |
| boîtier seulement : `TP−` vers la terre de la fiche de la hotte (le trou qui reçoit la broche de terre), calibre le plus élevé | OL | une liaison avec la hotte reste : ne rien brancher, la chercher |
| boîtier seulement : `TP−` vers la vis de la carcasse, calibre le plus élevé | OL | idem |
| LIGNE vers `GND` (ohmmètre) | plus de 10 kΩ | court-circuit : revoir le câblage avant tout USB |
| GPIO7 de la sonde | relié seulement au 4,7k de base de Q2 | revoir le câblage |
| GPIO6 de la sonde | relié seulement au collecteur de l'étage d'écoute et à son 10k | GPIO6 et GPIO7 intervertis ? revoir le câblage |

- [ ] **Étape 3 : Flasher la sonde (l'agent, avec l'accord de Majid)**

Montage de l'étape 2 en place, sonde seule sur l'USB du Mac (`PORT_SONDE`, `docs/BANC.md` §3). Pendant le flashage, le 10k base-émetteur tient Q2 bloqué (WIRING.md §6) : la LIGNE reste relâchée.
Lancer : `~/.platformio/penv/bin/pio run -e sonde -t upload --upload-port $PORT_SONDE`
Attendu : `SUCCESS`. Le `fw` de la sonde porte le commit de la tâche 23b (`git rev-parse --short HEAD`) : `info` le montrera à l'étape 5.

- [ ] **Étape 4 : Flasher le générateur (l'agent, avec l'accord de Majid)**

Majid branche ensuite le générateur (`PORT_GEN`, `docs/BANC.md` §3).
Lancer : `~/.platformio/penv/bin/pio run -e generateur -t upload --upload-port $PORT_GEN`
Attendu : `SUCCESS` ; même commit, noté au §7 pour les deux cartes. Majid ouvre la console du générateur (`~/.platformio/penv/bin/pio device monitor -e generateur -p $PORT_GEN`) et tape `help` : la ligne `impulsions <bas_us> <periode_ms> [n]  impulsion basse repetee (n : defaut 0, sans fin), critere 5` apparaît. Puis `etat` : `etat : arret, ligne relachee (GPIO7 bas)`.

- [ ] **Étape 5 : Repos et refus, §9.1 (Majid au clavier, l'agent lit)**

Majid ouvre une console sur la sonde (`~/.platformio/penv/bin/pio device monitor -p $PORT_SONDE -b 115200`) et tape, dans l'ordre :
- `info` : attendu, entre autres, `firmware 0.1.0-<commit de la tâche 23b> (env sonde)` et `ecoute GPIO6, injection GPIO7 (tenue basse)`. Au multimètre : GPIO7 à 0 V, LIGNE à 5 V environ (noté au §7, ligne `24`).
- `injection` : `injection : etage non monte (GPIO7), desarmee`, puis `  bas_max_us 3000, total_max_us 200000, silence_min_us 20000, attente_max_ms 1000, delai_min_ms 3000, arme_max_s 600, tol_us 20`.
- `injecte durees 1000` : `injecte : non montee ('injection monte 1' par l'USB)`.
- `injection on` : `injection : non montee ('injection monte 1' par l'USB, etage monte)`.
- `injection monte 1` : `injection : etage declare monte (GPIO7), desarmee`.
- `injecte durees 1000` : `injecte : non armee ('injection on')`.
- si le §7 (« Critère 2 à l'analyseur », tâche 16b) propose un `tol_us` au-dessus de 20 : `injection regle tol_us <valeur>`, et la ligne des valeurs le montre.
Il ferme la console (Ctrl-C) : le port doit être libre.

- [ ] **Étape 6 : Injection simple, relue par la voie 1, §9.2 (l'agent)**

Générateur : Majid tape `stop`.
Lancer : `python3 tools/serie_enregistre.py $PORT_SONDE banc-injection "injection on" "injecte durees 1500 750 750 750 750 2250 750" "injecte durees 1000" --duree 6`
Attendu : `id=2 injection on : ok (injection armee pour 600 s ('injection off' pour desarmer))`, `id=3 injecte durees 1500 750 750 750 750 2250 750 : accepte`, `id=4 injecte durees 1000 : refuse (injecte : delai minimal (3000 ms entre deux injections))`.
Lancer le résumé de `docs/BANC.md` §9 sur le fichier écrit (`logs/AAAA-MM-JJ-hhmm-banc-injection.jsonl`).
Attendu : `injection 3 ok attente <a> emises [1500, 750, 750, 750, 750, 2250, 750] relues [<7 durées>]`, chaque relue à `tol_us` (20 µs par défaut) au plus de l'émise ; `trame bas [<7 durées>]`, à ±2 µs des émises une fois corrigé le décalage du §7.
Lancer : `python3 tools/json_check.py --jsonl logs/<fichier>.jsonl | tail -3`
Attendu : `... ligne(s) machine, 0 erreur(s) ...`.
Recommencer neuf fois la première commande (chaque lancement dure 6 s : le délai minimal est tenu) ; noter le nombre de `ok` et le plus grand écart relu − émis. Si une injection simple rend `collision` : `docs/BANC.md` §9.2, dernier paragraphe (`injection regle tol_us <écart + 10>` par la console).

- [ ] **Étape 7 : Collision provoquée, §9.3 (Majid, puis l'agent)**

1. Majid tape `impulsions 500 50` dans la console du générateur. Attendu : `impulsions : sans fin, 500 us basses toutes les 50 ms, ligne haute entre deux`, puis `impulsion 0`, `impulsion 1`...
2. L'agent lance : `for i in 1 2 3 4 5 6 7 8 9 10; do python3 tools/serie_enregistre.py $PORT_SONDE banc-collision-$i "seuils 1 30000" "injection on" "injecte durees 500 20000 500 20000 2500" --duree 4; done`
   Attendu, à chaque tour : `id=2 seuils 1 30000 : ok`, `id=3 injection on : ok (...)`, `id=4 injecte durees 500 20000 500 20000 2500 : accepte`.
3. Majid tape `stop` dans la console du générateur.
4. L'agent lance le résumé de `docs/BANC.md` §9 sur `logs/*-banc-collision-*.jsonl`.
   Attendu, pour chacun des dix fichiers : `injection 4 collision attente <a> emises [500, 20000, 500, 20000, 2500] relues [<1 à 4 durées>]`, puis `bas le plus long : <500 à 1000> us`. **Sous 2 000 µs, l'impulsion de 2 500 µs n'est jamais partie : l'émission s'est arrêtée.** À 2 500 ou plus, elle ne s'est pas arrêtée : critère 5 en échec, on garde les fichiers.

- [ ] **Étape 8 : Bus jamais silencieux, §9.4 (Majid, puis l'agent)**

Majid tape `motif wtc 0` dans la console du générateur.
Lancer : `python3 tools/serie_enregistre.py $PORT_SONDE banc-delai "injection on" "injecte durees 1000" --duree 4`
Attendu : `id=3 injecte durees 1000 : accepte` ; le résumé donne `injection 3 delai attente <environ 1000000> emises [1000] relues []`.
Majid tape `stop`.

- [ ] **Étape 9 : Désarmement seul, §9.5 (Majid au clavier)**

Console de la sonde. `injection regle arme_max_s 30`, puis `injection on` : `injection armee pour 30 s ('injection off' pour desarmer)`. Après 35 s : `[injection] desarmee seule (30 s ecoulees)`. Puis `injecte durees 1000` : `injecte : non armee ('injection on')`. Puis `injection regle arme_max_s 600`.

- [ ] **Étape 10 : Fin, §9.6 (Majid, puis l'agent)**

Console de la sonde : `injection monte 0`, `seuils 1 5000`, puis `injection` : `injection : etage non monte (GPIO7), desarmee`. Au multimètre : GPIO7 à 0 V, LIGNE à 5 V environ. Majid ferme la console.
Si le Wi-Fi et la clé de la tâche 21 sont en place, l'agent lance : `python3 tools/hotte_udp.py session hotte-sonde.local "injection regle tol_us 30" "injection monte 1" --duree 5`
Attendu : `reponse id=2 fin interdite « injection regle tol_us 30 » : interdite a distance (10.5) : USB seulement` et la même ligne pour `id=3` et `injection monte 1`.

- [ ] **Étape 11 : Démontage de l'étage d'injection, §9.6 (Majid)**

Les deux câbles USB débranchés (sonde et générateur), Majid démonte l'étage d'injection : Q2, le 4,7k, le 10k et les 470 Ω retirés, ou au moins le collecteur de Q2 dessoudé de la LIGNE. Puis, hors tension, multimètre au calibre le plus élevé : `TP_Dp` vers la broche 7 des barrettes (GPIO7 de la sonde ; sur la plaque d'essai, la broche GPIO7), dans les deux sens.
Attendu : **plus de 100 kΩ** les deux fois. Sinon l'étage n'est pas démonté : Majid finit de le retirer et refait la mesure. Il donne à l'agent « démonté : oui / non » et les deux lectures. `injection monte 0` (étape 10) reste envoyé : les deux ensemble, avant tout retour du boîtier sur la hotte.

- [ ] **Étape 12 : Noter les résultats (l'agent)**

Dans `docs/BANC.md` §7, remplir :
- « Montage et firmwares », ligne `24` : date, où est la sonde (`boîtier` ou `plaque`), les deux **OL** (boîtier), `fw` de la sonde et du générateur (`0.1.0-<commit de la tâche 23b>`), ports, LIGNE au repos ;
- « Critère 5 : collision (tâche 24) », ligne par ligne : date, nombres sur 10, écart max relu − émis, attente du `delai`, GPIO7 au repos, et la ligne « 9.6 démontage » (étage démonté : oui / non ; les deux lectures `TP_Dp` → broche 7) ;
- « Conclusions » : une ligne `- Collision (critère 5) : détectée et émission arrêtée 10 / 10, tol_us retenu : <valeur>`, ou la limite constatée.

**Critère 5 rempli** si les 10 collisions sont vues et si aucun palier bas de 2 500 µs n'est capturé. Sinon, la limite part dans l'avenant (spec §8.4). Dans tous les cas, l'étage d'injection est démonté (étape 11) avant que le boîtier ne retourne sur la hotte. Relire le diff avant le commit : `git diff docs/BANC.md`.

- [ ] **Étape 13 : Commit**

```bash
git add docs/BANC.md
git commit -m "Consigner le critere 5 du banc (injection et collision)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 25 : Clôture : README, renvois de la spec, vérification finale

**Fichiers :**
- Créer : `tools/tests/test_liens.py`
- Modifier (contenu entier) : `README.md` (statut, sonde, outils, documents)
- Modifier : `docs/SPEC-RECONNAISSANCE.md` (documents liés ; renvois des §5, §7, §8.3, §8.4, §8.5, §10 ; `BANC.md` dans l'arborescence du §11)
- Modifier : `src/json_mode.cpp` (retrait de `anyMachine`, définie en tâche 11 et jamais appelée)
- Tester : `python3 -m unittest discover -s tools/tests -v`, `sh tools/tests/test_hote.sh`, `~/.platformio/penv/bin/pio run -e sonde` et `-e generateur`

**Interfaces :**
- Consomme : tous les documents des tâches 1 à 24 (`docs/SECURITE.md`, `docs/RECONNAISSANCE.md`, `docs/WIRING.md`, `docs/BANC.md`, `docs/PROTOCOLE-JSON.md`, `captures/README.md` de la tâche 12b) et leurs titres (ancres) ; l'usage réel des outils (docstrings de `analyse.py`, `banc.py`, `hotte_udp.py`, `serie_enregistre.py`, `verse_capture.py`) ; `src/json_mode.cpp` (tâches 11, 19 et 23).
- Produit : un README à jour, une spec qui renvoie aux documents de terrain, et `tools/tests/test_liens.py`, qui garde tous les liens relatifs de `README.md` et de `docs/*.md` justes (fichier et ancre).

**Décisions :**
- Les ancres sont calculées comme GitHub : minuscules, ponctuation retirée (accents gardés), espaces en tirets. `test_liens.py` vérifie tous les liens relatifs, pas seulement ceux de cette tâche. Ceux des tâches 1 à 24 passent déjà.
- `docs/PROTOCOL.md` n'existe qu'à l'étape 6 : le README le cite sans lien.
- La spec gagne une ligne pour `injection regle <nom> <valeur>` au §8.3, et la syntaxe d'avant l'avenant (`injecte durees ...`), avec renvoi vers `PROTOCOLE-JSON.md` §5.1.
- Le README cite aussi `captures/` (lien vers `captures/README.md`) et `tools/verse_capture.py` (tâche 12b) ; `test_liens.py` le vérifie.
- `anyMachine()` de `src/json_mode.cpp` (tâche 11) n'est appelée nulle part, ni après les tâches 19 et 23 : elle est retirée ici.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_liens.py` :

```python
#!/usr/bin/env python3
"""Liens de la documentation : python3 -m unittest discover -s tools/tests -p test_liens.py -v

Chaque lien relatif de README.md et de docs/*.md mene a un fichier present et,
s'il porte une ancre, a un titre de ce fichier (ancre calculee comme GitHub :
minuscules, ponctuation retiree, espaces en tirets). README.md et la spec
renvoient aux documents de terrain, du banc et du protocole ; le README cite
aussi captures/ et tools/verse_capture.py.
"""
import os
import re
import sys
import unittest

sys.dont_write_bytecode = True

RACINE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def ancre(titre):
    return re.sub(r"[^\w\- ]", "", titre.strip().lower()).replace(" ", "-")


def ancres(chemin):
    with open(chemin, encoding="utf-8") as f:
        return {ancre(l.lstrip("#")) for l in f if l.startswith("#")}


def documents():
    docs = os.path.join(RACINE, "docs")
    return ["README.md"] + ["docs/" + n for n in sorted(os.listdir(docs)) if n.endswith(".md")]


def lire(relatif):
    with open(os.path.join(RACINE, relatif), encoding="utf-8") as f:
        return f.read()


class Liens(unittest.TestCase):
    def test_liens_relatifs(self):
        erreurs = []
        for doc in documents():
            chemin = os.path.join(RACINE, doc)
            for cible in re.findall(r"\]\(([^)\s]+)\)", lire(doc)):
                if cible.startswith(("http://", "https://")):
                    continue
                fichier, _, a = cible.partition("#")
                vise = os.path.normpath(os.path.join(os.path.dirname(chemin), fichier)) if fichier else chemin
                if not os.path.exists(vise):
                    erreurs.append(f"{doc} : {cible} : fichier absent")
                elif a and vise.endswith(".md") and a not in ancres(vise):
                    erreurs.append(f"{doc} : {cible} : ancre absente")
        self.assertEqual(erreurs, [])

    def test_renvois(self):
        readme = lire("README.md")
        for n in ("SPEC-RECONNAISSANCE.md", "SECURITE.md", "RECONNAISSANCE.md", "WIRING.md", "BANC.md",
                  "PROTOCOLE-JSON.md", "BRIEF-RECHERCHE.md"):
            self.assertIn(f"](docs/{n}", readme, n)
        self.assertIn("](captures/README.md)", readme)  # captures de reference (tache 12b)
        self.assertIn("tools/verse_capture.py", readme)
        spec = lire("docs/SPEC-RECONNAISSANCE.md")
        for n in ("SECURITE.md", "RECONNAISSANCE.md", "WIRING.md", "BANC.md", "PROTOCOLE-JSON.md"):
            self.assertIn(f"]({n}", spec, n)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_liens.py 2>&1 | grep -E "^(FAIL|AssertionError|Ran|FAILED)" | cut -c1-80`
Attendu :
```
FAIL: test_renvois (test_liens.Liens.test_renvois)
AssertionError: '](docs/SECURITE.md' not found in "# Hotte Haier CCHP21EBB
Ran 2 tests in ...
FAILED (failures=1)
```
(`test_liens_relatifs` passe déjà.)

- [ ] **Étape 3 : Écrire le README et les renvois de la spec**

Remplacer tout le contenu de `README.md` (statut, sonde, outils, documents) par :

````markdown
# Hotte Haier CCHP21EBB → Matter sur Thread

Piloter une hotte **Haier CCHP21EBB** (90 cm, 3 vitesses, éclairage LED)
depuis Apple Home ou tout autre contrôleur Matter, avec un **ESP32-C6** en
Matter sur Thread. La hotte garde son panneau tactile d'origine, dont les
voyants restent synchronisés avec l'app.

> **Statut : sous-projet 1, la reconnaissance.** Les fiches de terrain, le
> firmware de la sonde, celui du générateur de banc et les outils Mac sont
> écrits et testés sur l'hôte ; le banc de validation consigne ses résultats
> dans [docs/BANC.md](docs/BANC.md). Restent les étapes sur la hotte (−1 à 7,
> [journal](docs/RECONNAISSANCE.md)), puis le protocole décodé
> (`docs/PROTOCOL.md`, à l'étape 6) et le choix de l'architecture.

## Principe visé

Le panneau tactile parle à la carte de puissance (`XB_DYB_V4`) par **un seul
fil de données**, sur le connecteur `CN3 « - D + »`. L'ESP32 se branche sur ce
fil par un adaptateur réversible, **sans jamais toucher au secteur**. Il
« appuie » sur les touches en émettant les mêmes trames que le panneau. La
carte d'origine reste maître : relais, bips et voyants restent cohérents.

Avant d'écrire le produit, il faut connaître ce protocole. C'est l'objet de la
reconnaissance.

## ⚠️ Sécurité

La carte de puissance est **alimentée en 230 V**. Seul le coin des connecteurs
basse tension (`CN1`, `CN2`, `CN3`) est concerné par ce projet, et on n'y
touche **que hotte débranchée**. Les règles sont dans
[docs/SPEC-RECONNAISSANCE.md §5](docs/SPEC-RECONNAISSANCE.md#5-règles-de-sécurité),
en fiche à garder sous les yeux dans [docs/SECURITE.md](docs/SECURITE.md).
Si tu reproduis ce projet, tu le fais sous ta propre responsabilité.

## La sonde

Un ESP32-C6 SuperMini sur le boîtier de mesure, hors de la hotte
([docs/WIRING.md](docs/WIRING.md)) :

- **écoute** de la ligne `D` sur GPIO6, par un étage à transistor (niveaux
  remis dans le sens du bus), avec le pilote RMT d'IDF en réception continue :
  des parties horodatées de durées consécutives, découpées en trames sur le
  Mac ;
- **protocole compagnon v1** de la ScreenBar, profil « hotte »
  ([docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md)) : par l'USB, ou en UDP
  sur le Wi-Fi (port 5480, enveloppe H1 authentifiée, `hotte-sonde.local`) ;
- **injection bornée** de l'étape 7 sur GPIO7 (étage en drain ouvert), avec
  les garde-fous du §8.4 de la spec : étage déclaré monté, armement limité à
  10 min, délai minimal, attente du silence, arrêt sur collision.

Le second C6, avec le firmware `generateur`, imite la ligne au banc avec des
motifs à contenu connu ([docs/BANC.md](docs/BANC.md)).

Construire (PlatformIO, même plateforme que la ScreenBar : pioarduino
55.03.312-1, Arduino-ESP32 3.3.12, IDF 5.5) :

```
~/.platformio/penv/bin/pio run -e sonde
~/.platformio/penv/bin/pio run -e generateur
```

On ne flashe la sonde que **hotte débranchée** ou au banc
(`-t upload --upload-port <port>`). Sur la console USB (115200 bauds), `help`
liste les commandes.

## Outils Mac

Python 3.11, bibliothèque standard seulement (le port série s'ouvre par
`termios`, sans redémarrer le C6).

| Outil | Rôle |
|---|---|
| `tools/serie_enregistre.py <port> <scénario> [commande ...]` | session machine par l'USB, enregistrée dans `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl` |
| `tools/hotte_udp.py cle <port>` | clé H1 posée par l'USB, rangée dans `~/.config/hotte-sonde/cle` (jamais affichée) |
| `tools/hotte_udp.py session <hôte> [commande ...]` | session réseau, résumé lisible ; `injection on` ne part qu'après la phrase « Majid devant la hotte », tapée dans un terminal (règle 11) |
| `tools/hotte_udp.py enregistre <hôte> <scénario> [commande ...]` | même session, enregistrée en `.jsonl`, résumé dans `logs/live.log` |
| `tools/analyse.py histo\|trames\|auto\|uart\|diff <capture>` | histogrammes, redécoupage en trames, décodage UART, distance d'impulsion ou Manchester, différences entre scénarios (étape 6) |
| `tools/banc.py <capture> <motif>` | verdicts du banc : trames décodées au bit près, écarts de durées |
| `tools/json_check.py [--jsonl] <capture>` | conformité des lignes machine au profil hotte |
| `tools/verse_capture.py <capture.jsonl> <nom>` | verse une capture de référence (étapes 5 et 5b) de `logs/` dans `captures/`, avec sa provenance ; refuse toute clé, tout SSID ou mot de passe |

Tests sans carte, depuis la racine du dépôt :

```
sh tools/tests/test_hote.sh                     # C++ (clang++) : capture, protocole, motifs, H1, injection
python3 -m unittest discover -s tools/tests -v  # outils Python
```

## Documents

| Document | Contenu |
|---|---|
| [docs/SPEC-RECONNAISSANCE.md](docs/SPEC-RECONNAISSANCE.md) | sous-projet 1 : objectif, sécurité, étapes, adaptateur, firmware « sonde », outils, banc de validation |
| [docs/SECURITE.md](docs/SECURITE.md) | les 13 règles en fiche, tests d'isolation et de fuite, conduite en cas d'anomalie |
| [docs/RECONNAISSANCE.md](docs/RECONNAISSANCE.md) | journal des étapes −1 à 7 : mesures, photos, décisions |
| [docs/WIRING.md](docs/WIRING.md) | adaptateur, câble de sortie, boîtier de mesure, étages, brochage, nomenclature |
| [docs/BANC.md](docs/BANC.md) | banc de validation : procédure, critères 1 à 6, résultats |
| [docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md) | profil hotte du protocole compagnon, par différence avec la ScreenBar |
| [docs/BRIEF-RECHERCHE.md](docs/BRIEF-RECHERCHE.md) | recherche du 24/09/2026 : identification de la carte, protocoles plausibles, Matter et Apple Home, 57 sources |
| [docs/photos/](docs/photos/) | photos de la carte et de l'étiquette (numéro de série masqué) |
| [captures/](captures/README.md) | captures de référence des étapes 5 et 5b, versées par `tools/verse_capture.py` : règles et provenance |

## Les trois sous-projets

1. **Reconnaissance** (en cours) : décoder la ligne `D`, choisir l'architecture,
   prouver l'injection.
2. **Produit** : interface définitive et firmware Matter (ventilateur 3 vitesses,
   lumière, arrêt différé).
3. **Bibliothèque commune et app compagnon** multi-appareils, partagées avec le
   projet frère.

## Projet frère

[benq-screenbar-halo-matter](https://github.com/Djoko-cli/benq-screenbar-halo-matter) :
une BenQ ScreenBar Halo pilotée en Matter sur Thread par un ESP32-C6. Même
plateforme (Arduino-ESP32 3.3.12 via pioarduino), même protocole compagnon. Le
code repris ici vient de son commit
[`c58a506`](https://github.com/Djoko-cli/benq-screenbar-halo-matter/tree/c58a506).
````

Dans `docs/SPEC-RECONNAISSANCE.md` :

Après :

```markdown

**Documents liés :**
- [BRIEF-RECHERCHE.md](BRIEF-RECHERCHE.md) : faits, niveaux de preuve et 57 sources.
```

insérer :

```markdown
- [SECURITE.md](SECURITE.md) : les règles du §5 en fiche, et les tests
  d'isolation et de fuite pas à pas.
- [RECONNAISSANCE.md](RECONNAISSANCE.md) : le journal des étapes −1 à 7 (§6).
- [WIRING.md](WIRING.md) : le matériel d'interface du §7 en détail.
- [BANC.md](BANC.md) : la procédure et les résultats du banc de validation (§10).
- [PROTOCOLE-JSON.md](PROTOCOLE-JSON.md) : le profil « hotte » du protocole
  compagnon (§8.5).
```

Remplacer :

```markdown

## 5. Règles de sécurité

Elles valent pour toutes les étapes. `docs/SECURITE.md` les reprend en fiche
à garder sous les yeux.

**Débrancher, attendre, ne pas passer la main**

```

par :

```markdown

## 5. Règles de sécurité

Elles valent pour toutes les étapes. [SECURITE.md](SECURITE.md) les reprend en
fiche à garder sous les yeux, avec les procédures des étapes 1, 2b et 3a.

**Débrancher, attendre, ne pas passer la main**

```

Après :

```markdown

## 7. Matériel d'interface

```

insérer :

```markdown
Le détail (schémas, valeurs, brochage du C6, nomenclature, contrôles avant
pose) est dans [WIRING.md](WIRING.md).

```

Remplacer :

```markdown
| `json ...` | session machine (§8.5) | selon la liste blanche |
| `injection monte 0\|1` | déclare l'étage d'injection monté (NVS) | non |
| `injection on\|off` | arme ou désarme l'injection. Désarmée au démarrage, et désarmée seule après 10 min | oui |
| `injecte ...` | syntaxe fixée par l'avenant de l'étape 6 | oui |
| `reboot` | redémarrage | non |

**À distance, aucune commande ne produit de texte humain.** Il partirait sur
```

par :

```markdown
| `json ...` | session machine (§8.5) | selon la liste blanche |
| `injection monte 0\|1` | déclare l'étage d'injection monté (NVS) | non |
| `injection on\|off` | arme ou désarme l'injection. Désarmée au démarrage, et désarmée seule après 10 min | oui |
| `injection regle <nom> <valeur>` | valeurs de l'avenant (§8.4), vérifiées avant la NVS ([PROTOCOLE-JSON.md §5.1](PROTOCOLE-JSON.md#51-injection)) | non |
| `injecte ...` | syntaxe fixée par l'avenant de l'étape 6 ; avant lui, `injecte durees <d1> <d2> ...` ([PROTOCOLE-JSON.md §5.1](PROTOCOLE-JSON.md#51-injection)) | oui |
| `reboot` | redémarrage | non |

**À distance, aucune commande ne produit de texte humain.** Il partirait sur
```

Remplacer :

```markdown
  niveau émis, avec une tolérance égale au délai de l'étage mesuré au banc. En
  cas d'écart, l'émission est arrêtée : on désactive le canal, la broche est
  libérée et le 10 k bloque Q2. Résultat `collision`.
  - Cette détection se valide au banc (§10).
  - Si elle s'avère impossible, on l'écrit dans l'avenant, et la collision est
    seulement constatée après coup, par relecture.

```

par :

```markdown
  niveau émis, avec une tolérance égale au délai de l'étage mesuré au banc. En
  cas d'écart, l'émission est arrêtée : on désactive le canal, la broche est
  libérée et le 10 k bloque Q2. Résultat `collision`.
  - Cette détection se valide au banc (§10, [BANC.md §9](BANC.md#9-injection-et-collision-critère-5)).
  - Si elle s'avère impossible, on l'écrit dans l'avenant, et la collision est
    seulement constatée après coup, par relecture.

```

Remplacer :

```markdown
- les champs `v`, `t`, `n`, `ms` viennent en tête, puis `bloc` ;
- `json 1` ouvre la session, avec un bail.

Le profil est documenté dans `docs/PROTOCOLE-JSON.md` de ce dépôt, **par
différence** avec celui de la ScreenBar.

**Messages**

```

par :

```markdown
- les champs `v`, `t`, `n`, `ms` viennent en tête, puis `bloc` ;
- `json 1` ouvre la session, avec un bail.

Le profil est documenté dans [PROTOCOLE-JSON.md](PROTOCOLE-JSON.md) de ce
dépôt, **par différence** avec celui de la ScreenBar.

**Messages**

```

Après :

```markdown
## 10. Banc de validation, avant la hotte

Le second C6, avec le firmware `generateur`, imite un bus de 5 V à drain ouvert.
```

insérer :

```markdown
Procédure pas à pas, commandes et résultats : [BANC.md](BANC.md).
```

Après :

```markdown
    PROTOCOL.md                  le protocole D décodé
    PROTOCOLE-JSON.md            profil hotte du protocole compagnon (par différence)
    WIRING.md                    adaptateur, boîtier de mesure, étages, brochage
```

insérer :

```markdown
    BANC.md                      banc de validation (§10) : procédure et résultats
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_liens.py -v 2>&1 | tail -4`
Attendu : `test_renvois ... ok`, « Ran 2 tests », « OK ».

- [ ] **Étape 5 : Retirer `anyMachine`, jamais appelée**

Lancer : `grep -rn anyMachine src/`
Attendu : une seule ligne, `src/json_mode.cpp:109:static bool anyMachine() {` (la définition ; aucun appel).

Dans `src/json_mode.cpp`, remplacer :

```cpp
static bool remote(uint8_t o) { return o != kUsb; }

static bool anyMachine() {
  for (const Sink &s : sSinks)
    if (s.machine) return true;
  return false;
}

```

par :

```cpp
static bool remote(uint8_t o) { return o != kUsb; }

```

Lancer : `grep -rn anyMachine src/ ; ~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|SUCCESS|FAILED"`
Attendu : rien pour `grep` ; aucune ligne `warning` ni `error`, puis `sonde          SUCCESS`.

- [ ] **Étape 6 : Vérification finale complète**

Lancer : `sh tools/tests/test_hote.sh`
Attendu (le chemin de `$TMPDIR` varie) :
```
test_capture : 140 verifications, 0 echecs
  pire trame : 860 octets (budget 896)
  pire cas des evenements : 889 octets (budget 896)
test_json : 243 verifications, 0 echecs
16 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
plus longue ligne : 218 octets ($TMPDIR/hotte-tests/test_json_lignes.txt:15)
par type : fin 1, hb 1, injection 3, log 1, reponse 7, trame 3
40 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
plus longue ligne : 526 octets (docs/PROTOCOLE-JSON.md exemple 12)
par type : compteurs/sonde 1, config 2, etat/bus 1, etat/capture 1, etat/injection 2, etat/sys 1, fin 1, hb 1, hello/base 2, hello/identite 1, injection 4, log 2, reponse 15, reseau/ip 1, trame 5
  pire trame : 26 symboles (tampon de 128)
test_generateur : 34 verifications, 0 echecs
test_wifi : 19 verifications, 0 echecs
test_h1 : 121 verification(s), 0 echec(s)
test_injection : 180 verifications, 0 echecs
tests hote : OK
```
Lancer : `python3 -m unittest discover -s tools/tests -v 2>&1 | tail -3`
Attendu : « Ran 159 tests in ... », « OK ».
Lancer : `~/.platformio/penv/bin/pio run -e sonde -t clean && ~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -ci warning`
Attendu : « 0 » (compilation complète, aucun avertissement).
Lancer : `~/.platformio/penv/bin/pio run -e sonde -e generateur 2>&1 | tail -5`
Attendu (les durées varient) :
```
Environment    Status    Duration
-------------  --------  ------------
sonde          SUCCESS   00:00:03.304
generateur     SUCCESS   00:00:02.413
========================= 2 succeeded in 00:00:05.717 =========================
```
Lancer : `git status --short`
Attendu : ` M README.md`, ` M docs/SPEC-RECONNAISSANCE.md`, ` M src/json_mode.cpp`, `?? tools/tests/test_liens.py`, et rien d'autre.

- [ ] **Étape 7 : Commit**

```bash
git add README.md docs/SPEC-RECONNAISSANCE.md src/json_mode.cpp tools/tests/test_liens.py
git commit -m "Mettre a jour le README et les renvois de la spec, retirer anyMachine" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
