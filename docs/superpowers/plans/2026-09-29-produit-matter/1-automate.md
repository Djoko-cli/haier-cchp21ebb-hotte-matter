# Partie 1 : logique pure (automate, hotte simulée, correspondance, surveillance)

Plan : [2026-09-29-produit-matter.md](../2026-09-29-produit-matter.md) (contraintes globales, écarts à la spec).

Tout ce qui décide est ici, sans Arduino ni pile Matter, testé sur le Mac par
`sh tools/tests/test_hote.sh` (clang++, `-Wall -Wextra -Werror`). Cycle de
chaque tâche : le test d'abord, qui échoue ; puis le code ; puis le test, qui
passe ; puis le commit, poussé.

### Tâche 1 : Automate, partie 1 : types, table des transitions, réglages

La table du §2.2 codée dans `appliquer()`, les réglages de l'automate
avec leurs bornes (§4.2), comme `inj::Params` de la sonde. Les lignes
« supposé identique en V1 et V2 » sont codées observées (§5.2, Q30).

**Fichiers :**
- Créer : `src/produit/hotte_etat.h`
- Créer : `src/produit/hotte_etat.cpp`
- Créer : `tools/tests/test_hotte_etat.cpp`
- Modifier : `tools/tests/test_hote.sh`

**Interfaces :**
- Produit (namespace `hotte`) : `Touche`, `Moteur`, `Marche`, `Confiance`, `Source`, `Origine`, `Demarrage`,
  `ModeEtat`, `ResultatAppui` ; `struct Etat` ; `bool memeEtat(const Etat&, const Etat&)` ;
  `Etat etatMiseSousTension(uint32_t)` ; `struct Transition { Etat apres; bool observee; }` ;
  `Transition appliquer(const Etat&, Touche)` ; `Moteur moteurDe(Touche)` ; `Touche toucheDe(Moteur)` ;
  `const char *texte(...)` pour chacune de ces énumérations ; `struct Params` (12 réglages du §4.2) ;
  `bool paramsValides(const Params&)` ; `kNbParams` ; `nomParam`, `valeurParam`, `bornesParam` ;
  `enum class Reglage` ; `Reglage reglerParam(Params*, const char*, uint32_t)` ;
  `bool chargerParams(const void*, size_t, Params*)`.

- [ ] **Étape 1 : Écrire le test (table du §2.2, textes, réglages)**

```cpp
// Tests hote de l'automate (src/produit/hotte_etat.*) : table des
// transitions (5.2), reglages et leurs bornes (4.2), confiance (5.1), et les
// entrees une a une, sans pilote (les suites completes : test_sequences.cpp).
// Lancer : sh tools/tests/test_hote.sh
#include <string.h>

#include "hotte_etat.h"
#include "verif.h"

using namespace hotte;

static Etat etat(Marche m, Moteur mo, bool lampe = false, Confiance c = Confiance::Confirme) {
  Etat e;
  e.marche = m;
  e.moteur = mo;
  e.lampe = lampe;
  e.lampeConnue = true;
  e.confiance = c;
  e.source = Source::Fil;
  return e;
}

static bool donne(const Etat &avant, Touche t, Marche m, Moteur mo, bool observee) {
  const Transition tr = appliquer(avant, t);
  return tr.apres.marche == m && tr.apres.moteur == mo && tr.observee == observee &&
         tr.apres.lampe == avant.lampe;
}

// Chaque ligne du 2.2 ; les lignes "suppose identique en V1 et V2" codees observees (Q30).
static void testTable() {
  const Moteur vitesses[] = {Moteur::V1, Moteur::V2, Moteur::V3};
  VERIF(donne(etat(Marche::Eteinte, Moteur::Arret), Touche::Marche, Marche::Armee, Moteur::Arret, true));
  VERIF(donne(etat(Marche::Armee, Moteur::Arret), Touche::Marche, Marche::Eteinte, Moteur::Arret, true));
  for (const Moteur x : vitesses) {
    const Touche tx = toucheDe(x);
    VERIF(donne(etat(Marche::Eteinte, Moteur::Arret), tx, Marche::Eteinte, Moteur::Arret, true));  // rien
    VERIF(donne(etat(Marche::Armee, Moteur::Arret), tx, Marche::Armee, x, true));
    VERIF(donne(etat(Marche::Armee, x), tx, Marche::Armee, Moteur::Arret, true));       // vitesse active
    VERIF(donne(etat(Marche::Armee, x), Touche::Marche, Marche::Prolongee, x, true));   // prolongee
    VERIF(donne(etat(Marche::Prolongee, x), tx, Marche::Armee, Moteur::Arret, true));   // 31,2 s
    VERIF(!appliquer(etat(Marche::Prolongee, x), Touche::Marche).observee);             // Q2
    VERIF(donne(etat(Marche::Inconnue, x), tx, Marche::Armee, Moteur::Arret, true));    // armee ou prolongee
    for (const Moteur y : vitesses) {
      if (y == x) continue;
      VERIF(donne(etat(Marche::Armee, x), toucheDe(y), Marche::Armee, y, true));
      VERIF(!appliquer(etat(Marche::Prolongee, x), toucheDe(y)).observee);  // Q2
      VERIF(!appliquer(etat(Marche::Inconnue, x), toucheDe(y)).observee);
    }
  }
  VERIF(!appliquer(etat(Marche::Inconnue, Moteur::Arret), Touche::V2).observee);
  VERIF(!appliquer(etat(Marche::Inconnue, Moteur::Arret), Touche::Marche).observee);
  // La lampe bascule dans tout etat, independante de la marche.
  const Marche marches[] = {Marche::Eteinte, Marche::Armee, Marche::Prolongee, Marche::Inconnue};
  for (const Marche m : marches)
    for (int l = 0; l <= 1; l++) {
      const Etat a = etat(m, m == Marche::Prolongee ? Moteur::V2 : Moteur::Arret, l);
      const Transition tr = appliquer(a, Touche::Lumiere);
      VERIF(tr.observee && tr.apres.lampe == !l && tr.apres.marche == m && tr.apres.moteur == a.moteur);
    }
  // Lampe inconnue : elle le reste.
  Etat inconnue = etat(Marche::Armee, Moteur::Arret);
  inconnue.lampeConnue = false;
  VERIF(!appliquer(inconnue, Touche::Lumiere).apres.lampeConnue);
  VERIF(moteurDe(Touche::V2) == Moteur::V2 && moteurDe(Touche::Marche) == Moteur::Arret);
  VERIF(toucheDe(Moteur::V3) == Touche::V3);
}

static void testTextes() {
  VERIF_EGAL_STR(texte(Touche::Lumiere), "lumiere");
  VERIF_EGAL_STR(texte(Moteur::V3), "v3");
  VERIF_EGAL_STR(texte(Marche::Prolongee), "prolongee");
  VERIF_EGAL_STR(texte(Confiance::Presume), "presume");
  VERIF_EGAL_STR(texte(Source::Annexe), "annexe");
  VERIF_EGAL_STR(texte(Origine::Panneau), "panneau");
  VERIF_EGAL_STR(texte(ModeEtat::AppuisSeuls), "appuis");
  VERIF_EGAL_STR(texte(ResultatAppui::Collision), "collision");
}

static void testParams() {
  const Params p;
  VERIF(paramsValides(p));
  VERIF(p.delaiMoteurMs == 3000 && p.entreAppuisMs == 500 && p.confirmationMs == 1000 && p.fraicheurMs == 2000);
  VERIF(p.lissageCalmeMs == 700 && p.lissagePlafondMs == 3000 && p.ordreCalmeMs == 150);
  VERIF(p.sequenceMaxMs == 15000 && p.attenteEtatMs == 5000 && p.prolongeeMaxMs == 1200000);
  VERIF(p.ignoreDemarrageMs == 2000 && p.sondeVitesse == 0);
  const char *noms[] = {"delai_moteur_ms",  "entre_appuis_ms",  "confirmation_ms", "fraicheur_ms",
                        "lissage_calme_ms", "lissage_plafond_ms", "ordre_calme_ms", "sequence_max_ms",
                        "attente_etat_ms",  "prolongee_max_ms", "ignore_demarrage_ms", "sonde_vitesse"};
  VERIF(kNbParams == 12);
  for (uint8_t i = 0; i < kNbParams; i++) VERIF_EGAL_STR(nomParam(i), noms[i]);
  VERIF(nomParam(kNbParams) == nullptr);
  VERIF(valeurParam(p, 0) == 3000 && valeurParam(p, 11) == 0);
  uint32_t lo = 0, hi = 0;
  VERIF(bornesParam("delai_moteur_ms", &lo, &hi) && lo == 1000 && hi == 60000);
  VERIF(bornesParam("prolongee_max_ms", &lo, &hi) && lo == 900000 && hi == 3600000);
  VERIF(bornesParam("ignore_demarrage_ms", &lo, &hi) && lo == 0 && hi == 10000);
  VERIF(bornesParam("sonde_vitesse", &lo, &hi) && lo == 0 && hi == 1);
  VERIF(!bornesParam("inconnu", &lo, &hi));
  // Chaque borne est atteinte, et pas au-dela.
  for (uint8_t i = 0; i < kNbParams; i++) {
    Params q;
    VERIF(bornesParam(nomParam(i), &lo, &hi));
    if (!strcmp(nomParam(i), "lissage_plafond_ms")) q.lissageCalmeMs = 300;  // calme <= plafond
    if (!strcmp(nomParam(i), "lissage_calme_ms")) q.lissagePlafondMs = 10000;
    VERIF(reglerParam(&q, nomParam(i), lo) == Reglage::Ok);
    VERIF(reglerParam(&q, nomParam(i), hi) == Reglage::Ok);
    if (lo > 0) VERIF(reglerParam(&q, nomParam(i), lo - 1) == Reglage::HorsBornes);
    VERIF(reglerParam(&q, nomParam(i), hi + 1) == Reglage::HorsBornes);
  }
  Params q;
  VERIF(reglerParam(&q, "vitesse", 3) == Reglage::NomInconnu);
  VERIF(reglerParam(&q, "confirmation_ms", 199) == Reglage::HorsBornes && q.confirmationMs == 1000);
  // Coherence : lissage_calme_ms <= lissage_plafond_ms.
  VERIF(reglerParam(&q, "lissage_plafond_ms", 1000) == Reglage::Ok);
  VERIF(reglerParam(&q, "lissage_calme_ms", 1001) == Reglage::HorsBornes && q.lissageCalmeMs == 700);
  // Image NVS : taille et bornes, sinon tous les defauts.
  Params lu;
  q.delaiMoteurMs = 4000;
  VERIF(chargerParams(&q, sizeof(q), &lu) && lu.delaiMoteurMs == 4000);
  Params mauvais;
  mauvais.fraicheurMs = 100;
  VERIF(!chargerParams(&mauvais, sizeof(mauvais), &lu) && lu.fraicheurMs == 2000 && lu.delaiMoteurMs == 3000);
  VERIF(!chargerParams(&q, sizeof(q) - 1, &lu) && lu.delaiMoteurMs == 3000);
  VERIF(!chargerParams(nullptr, 0, &lu));
}

int main() {
  testTable();
  testTextes();
  testParams();
  return bilan("test_hotte_etat");
}
```

- [ ] **Étape 2 : Annoncer le produit dans `tools/tests/test_hote.sh`**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
# Tests hote de la sonde (sans carte). A lancer depuis n'importe ou : sh tools/tests/test_hote.sh
```

par :

```sh
# Tests hote de la sonde et du produit (sans carte). A lancer depuis n'importe ou : sh tools/tests/test_hote.sh
```

- [ ] **Étape 3 : Brancher le test dans `tools/tests/test_hote.sh`**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
echo "tests hote : OK"
```

par :

```sh
# Produit (sous-projet 2) : modules purs de src/produit/.
CXXP="$CXX -Isrc/produit"
$CXXP src/produit/hotte_etat.cpp tools/tests/test_hotte_etat.cpp -o "$OUT/test_hotte_etat"
"$OUT/test_hotte_etat"
echo "tests hote : OK"
```

- [ ] **Étape 4 : Lancer les tests : ils échouent (module absent)**

```bash
sh tools/tests/test_hote.sh
```

Attendu : **échec**, avec :

```
clang++: error: no such file or directory: 'src/produit/hotte_etat.cpp'
```

- [ ] **Étape 5 : Écrire l'en-tête `src/produit/hotte_etat.h`**

```cpp
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
```

- [ ] **Étape 6 : Écrire `src/produit/hotte_etat.cpp`**

```cpp
// ===========================================================================
//  Automate de la hotte : voir hotte_etat.h. Pur (teste sur l'hote).
// ===========================================================================
#include "hotte_etat.h"

#include <string.h>

namespace hotte {

namespace {

struct Champ {
  const char *nom;
  uint32_t Params::*m;
  uint32_t lo, hi;
};
// Bornes du 4.2. Ordre : celui du tableau des parametres.
const Champ kChamps[kNbParams] = {
    {"delai_moteur_ms", &Params::delaiMoteurMs, 1000, 60000},
    {"entre_appuis_ms", &Params::entreAppuisMs, 100, 5000},
    {"confirmation_ms", &Params::confirmationMs, 200, 5000},
    {"fraicheur_ms", &Params::fraicheurMs, 200, 120000},
    {"lissage_calme_ms", &Params::lissageCalmeMs, 300, 2000},
    {"lissage_plafond_ms", &Params::lissagePlafondMs, 1000, 10000},
    {"ordre_calme_ms", &Params::ordreCalmeMs, 50, 1000},
    {"sequence_max_ms", &Params::sequenceMaxMs, 5000, 60000},
    {"attente_etat_ms", &Params::attenteEtatMs, 1000, 60000},
    {"prolongee_max_ms", &Params::prolongeeMaxMs, 900000, 3600000},
    {"ignore_demarrage_ms", &Params::ignoreDemarrageMs, 0, 10000},
    {"sonde_vitesse", &Params::sondeVitesse, 0, 1},
};

const Champ *champ(const char *nom) {
  if (!nom) return nullptr;
  for (const Champ &c : kChamps)
    if (!strcmp(c.nom, nom)) return &c;
  return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
//  Etat, table, textes
// ---------------------------------------------------------------------------

bool memeEtat(const Etat &a, const Etat &b) {
  return a.marche == b.marche && a.moteur == b.moteur && a.lampe == b.lampe && a.lampeConnue == b.lampeConnue;
}

Etat etatMiseSousTension(uint32_t nowMs) {
  Etat e;
  e.marche = Marche::Eteinte;
  e.moteur = Moteur::Arret;
  e.lampe = false;
  e.lampeConnue = true;
  e.confiance = Confiance::Deduit;
  e.source = Source::Deduit;
  e.depuisMs = nowMs;
  e.luMs = 0;
  return e;
}

Moteur moteurDe(Touche t) {
  switch (t) {
    case Touche::V1: return Moteur::V1;
    case Touche::V2: return Moteur::V2;
    case Touche::V3: return Moteur::V3;
    default: return Moteur::Arret;
  }
}

Touche toucheDe(Moteur m) {
  switch (m) {
    case Moteur::V1: return Touche::V1;
    case Moteur::V2: return Touche::V2;
    case Moteur::V3: return Touche::V3;
    default: return Touche::Marche;
  }
}

Transition appliquer(const Etat &avant, Touche t) {
  Transition r{avant, true};
  Etat &a = r.apres;
  if (t == Touche::Lumiere) {  // la lampe bascule, independante de la marche (etape -1)
    if (avant.lampeConnue) a.lampe = !avant.lampe;
    return r;
  }
  const Moteur m = avant.moteur;
  if (t == Touche::Marche) {
    switch (avant.marche) {
      case Marche::Eteinte:
        a.marche = Marche::Armee;
        return r;
      case Marche::Armee:
        // Moteur arrete : eteinte. Moteur en Vx : marche prolongee (observe en
        // V3 ; suppose identique en V1 et V2, code comme observe : Q30).
        a.marche = m == Moteur::Arret ? Marche::Eteinte : Marche::Prolongee;
        return r;
      case Marche::Prolongee:  // inconnu (Q2)
        a.marche = Marche::Inconnue;
        r.observee = false;
        return r;
      case Marche::Inconnue:
        r.observee = false;
        return r;
    }
    return r;
  }
  const Moteur v = moteurDe(t);
  switch (avant.marche) {
    case Marche::Eteinte:  // une vitesse sans marche ne fait rien (etape -1)
      return r;
    case Marche::Armee:
      // Vitesse active : armee, moteur arrete (observe en V3 ; Q30). Autre : Vy.
      a.moteur = m == v ? Moteur::Arret : v;
      return r;
    case Marche::Prolongee:
      if (m == v) {  // vitesse active : armee (observe, 31,2 s)
        a.marche = Marche::Armee;
        a.moteur = Moteur::Arret;
        return r;
      }
      a.marche = Marche::Inconnue;  // autre vitesse pendant la prolongation : inconnu (Q2)
      a.moteur = v;
      r.observee = false;
      return r;
    case Marche::Inconnue:
      if (m != Moteur::Arret && m == v) {  // armee comme prolongee vont a armee
        a.marche = Marche::Armee;
        a.moteur = Moteur::Arret;
        return r;
      }
      if (m != Moteur::Arret) a.moteur = v;  // armee : Vy ; prolongee : inconnu
      r.observee = false;
      return r;
  }
  return r;
}

const char *texte(Touche t) {
  static const char *const k[] = {"marche", "lumiere", "v1", "v2", "v3"};
  return (uint8_t)t < 5 ? k[(uint8_t)t] : "?";
}
const char *texte(Moteur m) {
  static const char *const k[] = {"arret", "v1", "v2", "v3"};
  return (uint8_t)m < 4 ? k[(uint8_t)m] : "?";
}
const char *texte(Marche m) {
  static const char *const k[] = {"eteinte", "armee", "prolongee", "inconnue"};
  return (uint8_t)m < 4 ? k[(uint8_t)m] : "?";
}
const char *texte(Confiance c) {
  static const char *const k[] = {"confirme", "deduit", "presume", "inconnu"};
  return (uint8_t)c < 4 ? k[(uint8_t)c] : "?";
}
const char *texte(Source s) {
  static const char *const k[] = {"fil", "annexe", "deduit", "nvs"};
  return (uint8_t)s < 4 ? k[(uint8_t)s] : "?";
}
const char *texte(Origine o) {
  static const char *const k[] = {"panneau", "module", "inconnue"};
  return (uint8_t)o < 3 ? k[(uint8_t)o] : "?";
}
const char *texte(ModeEtat m) {
  static const char *const k[] = {"repete", "changements", "appuis"};
  return (uint8_t)m < 3 ? k[(uint8_t)m] : "?";
}
const char *texte(ResultatAppui r) {
  static const char *const k[] = {"ok", "collision", "delai", "garde", "erreur"};
  return (uint8_t)r < 5 ? k[(uint8_t)r] : "?";
}
// ---------------------------------------------------------------------------
//  Reglages
// ---------------------------------------------------------------------------

bool paramsValides(const Params &p) {
  for (const Champ &c : kChamps)
    if (p.*c.m < c.lo || p.*c.m > c.hi) return false;
  return p.lissageCalmeMs <= p.lissagePlafondMs;
}

const char *nomParam(uint8_t i) { return i < kNbParams ? kChamps[i].nom : nullptr; }

uint32_t valeurParam(const Params &p, uint8_t i) { return i < kNbParams ? p.*kChamps[i].m : 0; }

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

}  // namespace hotte
```

- [ ] **Étape 7 : Lancer les tests : ils passent**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
test_hotte_etat : 153 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 8 : Committer**

```bash
git add src/produit/hotte_etat.h src/produit/hotte_etat.cpp tools/tests/test_hotte_etat.cpp tools/tests/test_hote.sh
git commit -m "$(cat <<'FIN'
Ajouter la table des transitions et les reglages de l'automate de la hotte

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 2 : Automate, partie 2 : ordres, confiance, séquences, règle d'or

**Fichiers :**
- Modifier : `src/produit/hotte_etat.h`
- Modifier : `src/produit/hotte_etat.cpp`
- Modifier : `tools/tests/test_hotte_etat.cpp`

**Interfaces :**
- Consomme (tâche 1) : les types, `appliquer`, `Params`.
- Produit : `CibleVentilo`, `Issue`, `Cause` (`kNbCauses`), `Canal`, `Sujet`, `Evt` et leurs `texte()` ;
  `struct Action` (types `Appuyer`, `Publier`, `FinSequence`, `Changement`, `Evenement`) ;
  `struct Compteurs` ; `struct Diagnostic` ; `class Automate` avec
  `configurer(const Params&)`, `configurerLigne(ModeEtat, bool annexe)`,
  `demarrer(Demarrage, const Etat *etatPublieNvs, Moteur derniereVitesse, uint32_t now)`,
  `ordreVentilo(CibleVentilo, uint32_t idOrdre, Canal, uint32_t now)`,
  `ordreLampe(bool, uint32_t idOrdre, Canal, uint32_t now)`, `surEtatLu(const Etat&, uint32_t)`,
  `surAppui(Touche, Origine, uint32_t)`, `surFinAppui(uint32_t idAppui, ResultatAppui, uint32_t)`,
  `surAnomalie(uint16_t, uint32_t)`, `surPilote(bool, uint32_t)`, `Action suivante(uint32_t now)`,
  `etat()`, `derniereVitesse()`, `diagnostic(uint32_t)`, `params()`, `mode()`, `annexe()`.

- [ ] **Étape 1 : Écrire les tests de l'automate (confiance, ordres, pilote hors service)**

Ils remplacent le `main` de la tâche 1.

Dans `tools/tests/test_hotte_etat.cpp`, remplacer :

```cpp
int main() {
  testTable();
  testTextes();
  testParams();
  return bilan("test_hotte_etat");
}
```

par :

```cpp
// Vide l'automate ; rend le nombre d'actions du type demande, *derniere recoit la derniere.
static int vider(Automate &a, uint32_t t, Action::Type type, Action *derniere = nullptr) {
  int n = 0;
  for (Action x = a.suivante(t); x.type != Action::Aucune; x = a.suivante(t))
    if (x.type == type) {
      n++;
      if (derniere) *derniere = x;
    }
  return n;
}

static void testConfiance() {
  {  // mise sous tension : eteinte, Deduit, publiee
    Automate a;
    a.configurerLigne(ModeEtat::Changements, false);
    a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::Arret, 1000);
    VERIF(a.etat().marche == Marche::Eteinte && a.etat().confiance == Confiance::Deduit && a.etat().lampeConnue);
    VERIF(a.derniereVitesse() == Moteur::V2);
    VERIF(vider(a, 1000, Action::Publier) == 1);
    // Etat complet lu : Confirme. Sans evenement, il le reste des heures
    // (mode changements) : un etat ne vieillit pas vers Inconnu.
    a.surEtatLu(etat(Marche::Armee, Moteur::Arret), 2000);
    VERIF(a.etat().confiance == Confiance::Confirme && a.diagnostic(2000).marcheAutorisee);
    VERIF(vider(a, 2000, Action::Changement) == 1);
    vider(a, 2000 + 3600000u, Action::Aucune);
    VERIF(a.etat().confiance == Confiance::Confirme);
    // Anomalie de reception : Presume, valeurs gardees, "marche" interdite.
    a.surAnomalie(1, 3700000);
    VERIF(a.etat().confiance == Confiance::Presume && a.etat().marche == Marche::Armee);
    VERIF(!a.diagnostic(3700000).marcheAutorisee && a.diagnostic(3700000).c.anomalies == 1);
  }
  {  // mode repete : ligne muette au-dela de fraicheur_ms
    Automate a;
    a.configurerLigne(ModeEtat::Repete, false);
    a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::V2, 0);
    a.surEtatLu(etat(Marche::Eteinte, Moteur::Arret), 100);
    vider(a, 100, Action::Aucune);
    VERIF(a.etat().confiance == Confiance::Confirme);
    VERIF(vider(a, 2099, Action::Evenement) == 0);
    Action e;
    VERIF(vider(a, 2100, Action::Evenement, &e) == 1 && e.evt == Evt::LigneMuette);
    VERIF(a.etat().confiance == Confiance::Presume && a.diagnostic(2100).c.lignesMuettes == 1);
    VERIF(vider(a, 5000, Action::Evenement) == 0);  // signalee une fois
    a.surEtatLu(etat(Marche::Eteinte, Moteur::Arret), 5000);
    VERIF(a.etat().confiance == Confiance::Confirme);
  }
  {  // autre cause : dernier etat publie relu en NVS, Presume ; sans NVS, Inconnu et rien de publie
    Automate a;
    a.configurerLigne(ModeEtat::Changements, false);
    const Etat nvs = etat(Marche::Armee, Moteur::V3, true);
    a.demarrer(Demarrage::Autre, &nvs, Moteur::V3, 0);
    VERIF(a.etat().confiance == Confiance::Presume && a.etat().source == Source::Nvs);
    VERIF(a.etat().moteur == Moteur::V3 && a.etat().lampe);
    Action pub;
    VERIF(vider(a, 0, Action::Publier, &pub) == 1 && pub.etat.moteur == Moteur::V3);
    Automate b;
    b.configurerLigne(ModeEtat::Changements, false);
    b.demarrer(Demarrage::Autre, nullptr, Moteur::V2, 0);
    VERIF(b.etat().confiance == Confiance::Inconnu && vider(b, 0, Action::Publier) == 0);
  }
  {  // lecture annexe : moteur et lampe lus, voyant marche deduit ou inconnu
    Automate a;
    a.configurerLigne(ModeEtat::AppuisSeuls, true);
    a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::V2, 0);
    Etat an;
    an.marche = Marche::Inconnue;
    an.moteur = Moteur::Arret;
    an.lampe = false;
    an.lampeConnue = true;
    an.source = Source::Annexe;
    a.surEtatLu(an, 100);
    VERIF(a.etat().marche == Marche::Eteinte && a.etat().confiance == Confiance::Deduit);
    VERIF(a.diagnostic(100).marcheAutorisee);
    an.moteur = Moteur::V2;  // moteur en marche, voyant deduit eteint : incoherent
    a.surEtatLu(an, 200);
    VERIF(a.etat().marche == Marche::Inconnue && a.etat().moteur == Moteur::V2);
  }
  {  // appui du panneau sur la vitesse active en V1 : Deduit (ligne supposee, codee observee, Q30)
    Automate a;
    a.configurerLigne(ModeEtat::Changements, false);
    a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::V2, 0);
    a.surEtatLu(etat(Marche::Armee, Moteur::V1), 100);
    vider(a, 100, Action::Aucune);
    a.surAppui(Touche::V1, Origine::Panneau, 200);
    VERIF(a.etat().moteur == Moteur::Arret && a.etat().confiance == Confiance::Deduit);
    Action c;
    VERIF(vider(a, 200, Action::Changement, &c) == 1 && c.origine == Origine::Panneau);
    VERIF(a.diagnostic(200).c.appuisPanneau == 1);
    // Transition inconnue (autre vitesse pendant la prolongation) : Presume.
    a.surEtatLu(etat(Marche::Prolongee, Moteur::V2), 300);
    a.surAppui(Touche::V3, Origine::Panneau, 400);
    VERIF(a.etat().confiance == Confiance::Presume && a.diagnostic(400).c.transitionsInconnues == 1);
  }
}

static void testTextesOrdres() {
  VERIF_EGAL_STR(texte(CibleVentilo::Derniere), "derniere");
  VERIF_EGAL_STR(texte(Issue::Remplacee), "remplacee");
  VERIF_EGAL_STR(texte(Cause::NonConfirme), "non_confirme");
  VERIF_EGAL_STR(texte(Cause::SansLecture), "sans_lecture");
  VERIF_EGAL_STR(texte(Canal::App), "app");
  VERIF_EGAL_STR(texte(Sujet::Lampe), "lampe");
  VERIF_EGAL_STR(texte(Evt::ProlongeePerimee), "prolongee_perimee");
}

// Ordre egal a l'etat : publication, aucun appui. Pilote hors service : Echec
// (cause pilote) si un appui etait en vol, puis attente et Abandon.
static void testOrdresUnitaires() {
  Automate a;
  a.configurerLigne(ModeEtat::Changements, false);
  a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::V2, 0);
  a.surEtatLu(etat(Marche::Armee, Moteur::V2), 100);
  vider(a, 100, Action::Aucune);
  a.ordreVentilo(CibleVentilo::V2, 5, Canal::App, 200);
  Action f;
  int appuis = 0, publis = 0, fins = 0;
  for (Action x = a.suivante(200); x.type != Action::Aucune; x = a.suivante(200)) {
    appuis += x.type == Action::Appuyer;
    publis += x.type == Action::Publier;
    if (x.type == Action::FinSequence) {
      fins++;
      f = x;
    }
  }
  VERIF(appuis == 0 && publis == 1 && fins == 1 && f.issue == Issue::Ok && f.idOrdre == 5 && f.appuis == 0);
  // Un appui en vol, puis le pilote rend Erreur.
  a.ordreLampe(true, 6, Canal::Matter, 5000);
  Action x = a.suivante(5000);
  VERIF(x.type == Action::Appuyer && x.touche == Touche::Lumiere);
  a.surFinAppui(x.idAppui, ResultatAppui::Erreur, 5060);
  VERIF(vider(a, 5060, Action::FinSequence, &f) == 1 && f.issue == Issue::Echec && f.cause == Cause::Pilote);
  VERIF(!a.diagnostic(5060).piloteEnService);
  a.ordreLampe(true, 7, Canal::Matter, 6000);
  VERIF(vider(a, 6000, Action::Appuyer) == 0);
  VERIF(vider(a, 10999, Action::FinSequence) == 0);
  VERIF(vider(a, 11000, Action::FinSequence, &f) == 1 && f.issue == Issue::Abandon && f.cause == Cause::Pilote);
  a.surPilote(true, 12000);
  a.ordreLampe(true, 8, Canal::Matter, 12000);
  VERIF(vider(a, 12000, Action::Appuyer) == 1);
}

int main() {
  testTable();
  testTextes();
  testParams();
  testTextesOrdres();
  testConfiance();
  testOrdresUnitaires();
  return bilan("test_hotte_etat");
}
```

- [ ] **Étape 2 : Lancer les tests : ils échouent (automate absent)**

```bash
sh tools/tests/test_hote.sh
```

Attendu : **échec**, avec :

```
tools/tests/test_hotte_etat.cpp:128:18: error: unknown type name 'Automate'
tools/tests/test_hotte_etat.cpp:128:43: error: use of undeclared identifier 'Action'
tools/tests/test_hotte_etat.cpp:128:62: error: unknown type name 'Action'
tools/tests/test_hotte_etat.cpp:130:8: error: unknown type name 'Action'
tools/tests/test_hotte_etat.cpp:130:44: error: use of undeclared identifier 'Action'
tools/tests/test_hotte_etat.cpp:140:5: error: unknown type name 'Automate'
tools/tests/test_hotte_etat.cpp:145:26: error: use of undeclared identifier 'Action'
tools/tests/test_hotte_etat.cpp:150:26: error: use of undeclared identifier 'Action'
tools/tests/test_hotte_etat.cpp:151:31: error: use of undeclared identifier 'Action'
tools/tests/test_hotte_etat.cpp:159:5: error: unknown type name 'Automate'
tools/tests/test_hotte_etat.cpp:163:19: error: use of undeclared identifier 'Action'
tools/tests/test_hotte_etat.cpp:165:26: error: use of undeclared identifier 'Action'
```

- [ ] **Étape 3 : Déclarer l'automate dans `src/produit/hotte_etat.h`**

Le bloc s'insère avant la fermeture du namespace.

Dans `src/produit/hotte_etat.h`, remplacer :

```cpp
}  // namespace hotte
```

par :

```cpp
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
```

- [ ] **Étape 4 : Écrire l'automate dans `src/produit/hotte_etat.cpp`**

Règles tenues ici : un appui à la fois, confirmation par mode (tableau
du §5.4), un nouvel essai seulement si l'échec est prouvé, jamais « marche »
sans un arrêt du moteur **lu** depuis le dernier appui (`arretLu_`), délai
moteur toutes origines, annulation par le panneau, sonde de vitesse si elle
est permise, marche prolongée périmée, ligne muette.

Dans `src/produit/hotte_etat.cpp`, remplacer :

```cpp
}  // namespace hotte
```

par :

```cpp
// ===========================================================================
//  Automate (5.1 a 5.9)
// ===========================================================================

namespace {

bool vitesse(Touche t) { return t == Touche::V1 || t == Touche::V2 || t == Touche::V3; }

bool fiable(Confiance c) { return c == Confiance::Confirme || c == Confiance::Deduit; }

// Confiance apres une deduction par la table (appui vu sur le fil).
Confiance deduite(Confiance avant, bool observee) {
  if (avant == Confiance::Inconnu) return Confiance::Inconnu;
  if (!observee || avant == Confiance::Presume) return Confiance::Presume;
  return Confiance::Deduit;
}

// Un etat complet lu s'accorde avec e sur ce que les deux connaissent.
bool accordFil(const Etat &lu, const Etat &e) {
  if (lu.moteur != e.moteur) return false;
  if (lu.marche != Marche::Inconnue && e.marche != Marche::Inconnue && lu.marche != e.marche) return false;
  return !lu.lampeConnue || !e.lampeConnue || lu.lampe == e.lampe;
}

// Une lecture annexe ne juge que ce que la touche change : la lampe ou le moteur.
bool accordAnnexe(const Etat &lu, const Etat &e, Touche t) {
  if (t == Touche::Lumiere) return lu.lampeConnue && e.lampeConnue && lu.lampe == e.lampe;
  return lu.moteur == e.moteur;
}

}  // namespace

const char *texte(CibleVentilo c) {
  static const char *const k[] = {"aucune", "eteint", "v1", "v2", "v3", "derniere"};
  return (uint8_t)c < 6 ? k[(uint8_t)c] : "?";
}
const char *texte(Issue i) {
  static const char *const k[] = {"ok", "annulee", "echec", "abandon", "remplacee"};
  return (uint8_t)i < 5 ? k[(uint8_t)i] : "?";
}
const char *texte(Cause c) {
  static const char *const k[] = {"aucune", "non_confirme", "sans_lecture", "collision", "garde", "pilote", "duree"};
  return (uint8_t)c < kNbCauses ? k[(uint8_t)c] : "?";
}
const char *texte(Canal c) { return c == Canal::App ? "app" : "matter"; }
const char *texte(Sujet s) { return s == Sujet::Lampe ? "lampe" : "ventilo"; }
const char *texte(Evt e) {
  static const char *const k[] = {"marche_inconnue", "prolongee_perimee", "ligne_muette", "deduction_dementie",
                                  "sonde_vitesse"};
  return (uint8_t)e < 5 ? k[(uint8_t)e] : "?";
}

// ---------------------------------------------------------------------------
//  Automate : entrees
// ---------------------------------------------------------------------------

Automate::Automate(const Params &p) : p_(p) {}

void Automate::configurer(const Params &p) { p_ = p; }

void Automate::configurerLigne(ModeEtat mode, bool annexe) {
  mode_ = mode;
  annexe_ = annexe;
  muette_ = false;
}

void Automate::demarrer(Demarrage cause, const Etat *etatPublieNvs, Moteur derniereVitesse, uint32_t nowMs) {
  const Params p = p_;
  const ModeEtat m = mode_;
  const bool a = annexe_;
  *this = Automate(p);  // tout repart de zero, sauf les reglages et la ligne
  mode_ = m;
  annexe_ = a;
  demarrageMs_ = nowMs;
  derniere_ = derniereVitesse == Moteur::Arret ? Moteur::V2 : derniereVitesse;
  if (cause == Demarrage::MiseSousTension) {
    // H-coupure (5.7, Q20) : la hotte revient toujours eteinte. Rien n'est
    // lu : "marche" attend un arret du moteur lu (regle 3).
    etat_ = etatMiseSousTension(nowMs);
  } else if (etatPublieNvs) {
    etat_ = *etatPublieNvs;  // la hotte a pu continuer a tourner
    etat_.confiance = Confiance::Presume;
    etat_.source = Source::Nvs;
    etat_.depuisMs = nowMs;
    etat_.luMs = 0;
  } else {
    etat_ = Etat();
    etat_.depuisMs = nowMs;
  }
  moteurLu_ = etat_.moteur;
  changementMoteurMs_ = nowMs;  // prudence : un changement du moteur a pu preceder le demarrage
  aPublier_ = etat_.confiance != Confiance::Inconnu;
}

void Automate::ordreVentilo(CibleVentilo c, uint32_t idOrdre, Canal canal, uint32_t nowMs) {
  if (c == CibleVentilo::Aucune) return;
  const bool eteint = c == CibleVentilo::Eteint;
  Moteur m = Moteur::Arret;
  if (c == CibleVentilo::Derniere) m = derniere_;
  else if (c == CibleVentilo::V1) m = Moteur::V1;
  else if (c == CibleVentilo::V2) m = Moteur::V2;
  else if (c == CibleVentilo::V3) m = Moteur::V3;
  if (ov_.actif && eteint == cibleEteint_ && m == cibleMoteur_) {
    // Meme cible (5.5) : rien ne repart ; le nouvel ordre reprend la sequence.
    Action a;
    a.type = Action::FinSequence;
    a.sujet = Sujet::Ventilo;
    a.idOrdre = ov_.id;
    a.canal = ov_.canal;
    a.issue = Issue::Remplacee;
    a.dureeMs = nowMs - ov_.recuMs;
    a.appuis = ov_.appuis;
    pousser(a);
    c_.remplacees++;
    ov_.id = idOrdre;
    ov_.canal = canal;
    ov_.recuMs = nowMs;
    ov_.appuis = 0;
    return;
  }
  finOrdre(Sujet::Ventilo, Issue::Remplacee, Cause::Aucune, nowMs);
  ov_ = Ordre();
  ov_.actif = true;
  ov_.id = idOrdre;
  ov_.canal = canal;
  ov_.recuMs = nowMs;
  cibleEteint_ = eteint;
  cibleMoteur_ = m;
  reprise_[(int)Sujet::Ventilo] = Reprise();
}

void Automate::ordreLampe(bool allumee, uint32_t idOrdre, Canal canal, uint32_t nowMs) {
  if (ol_.actif && allumee == cibleLampe_) {
    Action a;
    a.type = Action::FinSequence;
    a.sujet = Sujet::Lampe;
    a.idOrdre = ol_.id;
    a.canal = ol_.canal;
    a.issue = Issue::Remplacee;
    a.dureeMs = nowMs - ol_.recuMs;
    a.appuis = ol_.appuis;
    pousser(a);
    c_.remplacees++;
    ol_.id = idOrdre;
    ol_.canal = canal;
    ol_.recuMs = nowMs;
    ol_.appuis = 0;
    return;
  }
  finOrdre(Sujet::Lampe, Issue::Remplacee, Cause::Aucune, nowMs);
  ol_ = Ordre();
  ol_.actif = true;
  ol_.id = idOrdre;
  ol_.canal = canal;
  ol_.recuMs = nowMs;
  cibleLampe_ = allumee;
  reprise_[(int)Sujet::Lampe] = Reprise();
}

void Automate::surEtatLu(const Etat &lu, uint32_t nowMs) {
  const bool sur = fiable(etat_.confiance);
  Etat n = etat_;
  n.moteur = lu.moteur;
  if (lu.marche != Marche::Inconnue) {
    n.marche = lu.marche;
  } else if (!sur || (lu.moteur == Moteur::Arret && etat_.marche == Marche::Prolongee) ||
             (lu.moteur != Moteur::Arret && etat_.marche == Marche::Eteinte)) {
    // Lecture annexe : le voyant marche n'est pas lu, et la deduction ne tient plus.
    n.marche = Marche::Inconnue;
  }
  if (lu.lampeConnue) {
    n.lampe = lu.lampe;
    n.lampeConnue = true;
  } else if (!sur) {
    n.lampeConnue = false;
  }
  // Confirme : etat complet lu, ou lecture partielle qui s'accorde avec un
  // etat deja confirme sans appui ni anomalie depuis. Sinon, Deduit : le
  // moteur et la lampe sont lus, le voyant marche vient de la table.
  const bool complet = lu.marche != Marche::Inconnue && lu.lampeConnue;
  const bool garde = etat_.confiance == Confiance::Confirme && memeEtat(n, etat_);
  n.confiance = complet || garde ? Confiance::Confirme : Confiance::Deduit;
  n.source = lu.source;
  n.depuisMs = nowMs;
  n.luMs = nowMs;
  if (lu.moteur != moteurLu_) {
    changementMoteurMs_ = nowMs;
    moteurLu_ = lu.moteur;
  }
  arretLu_ = lu.moteur == Moteur::Arret;  // lu apres le dernier appui : un appui remet a faux
  muette_ = false;
  if (vol_.actif && vol_.fini) {
    if (lu.source == Source::Fil) {
      vol_.filLu = true;
      vol_.fil = lu;
      vol_.filMs = nowMs;
    } else {
      vol_.annexeLue = true;
      vol_.annexeEtat = lu;
      vol_.annexeMs = nowMs;
    }
  }
  const bool notre = vol_.actif && accordFil(lu, vol_.attendu) && !accordFil(lu, vol_.avant);
  changer(n, notre ? Origine::Module : Origine::Inconnue, nowMs);
}

void Automate::surAppui(Touche t, Origine o, uint32_t nowMs) {
  arretLu_ = false;
  const Transition tr = appliquer(etat_, t);
  if (o == Origine::Module) {  // echo de notre propre trame
    if (vol_.actif && !vol_.echo && t == vol_.touche) vol_.echo = true;
    // Appuis seuls : le voyant marche ne se connait que par l'echo et la
    // table. Ailleurs, l'etat lu tranchera ; le moteur et la lampe viennent
    // toujours d'une lecture.
    if (mode_ != ModeEtat::AppuisSeuls || tr.apres.marche == etat_.marche) return;
    Etat n = etat_;
    n.marche = tr.apres.marche;
    n.confiance = deduite(etat_.confiance, tr.observee);
    n.source = Source::Deduit;
    n.depuisMs = nowMs;
    if (!tr.observee) c_.transitionsInconnues++;
    changer(n, Origine::Module, nowMs);
    return;
  }
  c_.appuisPanneau++;
  Etat n = tr.apres;
  n.confiance = deduite(etat_.confiance, tr.observee);
  n.source = Source::Deduit;
  n.depuisMs = nowMs;
  if (!tr.observee) c_.transitionsInconnues++;
  changer(n, Origine::Panneau, nowMs);
  annulerTout(nowMs);  // le panneau est prioritaire (5.6)
  pousserPublier();    // publie tout de suite (C2)
}

void Automate::surFinAppui(uint32_t idAppui, ResultatAppui r, uint32_t nowMs) {
  if (!vol_.actif || vol_.fini || vol_.id != idAppui) return;
  vol_.fini = true;
  vol_.res = r;
  vol_.finMs = nowMs;
  if (r == ResultatAppui::Erreur) piloteOk_ = false;
}

void Automate::surAnomalie(uint16_t, uint32_t nowMs) {
  c_.anomalies++;
  arretLu_ = false;
  if (fiable(etat_.confiance)) etat_.confiance = Confiance::Presume;  // valeurs publiees inchangees
  if (vol_.actif) {
    vol_.anomalie = true;
    vol_.anomalieMs = nowMs;
  }
}

void Automate::surPilote(bool enService, uint32_t) { piloteOk_ = enService; }

// ---------------------------------------------------------------------------
//  Automate : sorties
// ---------------------------------------------------------------------------

void Automate::pousser(const Action &a) {
  if (nb_ == kFile) {
    c_.actionsPerdues++;
    return;
  }
  file_[(tete_ + nb_) % kFile] = a;
  nb_++;
}

void Automate::pousserPublier() {
  aPublier_ = false;
  if (etat_.confiance == Confiance::Inconnu) return;  // rien de nouveau n'est publie (5.1)
  for (uint8_t i = 0; i < nb_; i++)
    if (file_[(tete_ + i) % kFile].type == Action::Publier) return;  // il publiera l'etat du moment
  Action a;
  a.type = Action::Publier;
  pousser(a);
}

void Automate::changer(const Etat &n, Origine o, uint32_t nowMs) {
  const Etat avant = etat_;
  etat_ = n;
  if (n.moteur != Moteur::Arret && fiable(n.confiance)) derniere_ = n.moteur;  // toutes origines
  if (n.moteur != avant.moteur) changementMoteurMs_ = nowMs;
  if (memeEtat(avant, n)) return;
  Action a;
  a.type = Action::Changement;
  a.avant = avant;
  a.etat = n;
  a.origine = o;
  pousser(a);
  aPublier_ = true;
}

void Automate::finOrdre(Sujet s, Issue i, Cause c, uint32_t nowMs) {
  Ordre &o = s == Sujet::Ventilo ? ov_ : ol_;
  if (!o.actif) return;
  Action a;
  a.type = Action::FinSequence;
  a.sujet = s;
  a.idOrdre = o.id;
  a.canal = o.canal;
  a.issue = i;
  a.cause = c;
  a.dureeMs = nowMs - o.recuMs;
  a.appuis = o.appuis;
  pousser(a);
  switch (i) {
    case Issue::Ok: c_.reussies++; break;
    case Issue::Annulee: c_.annulees++; break;
    case Issue::Remplacee: c_.remplacees++; break;
    case Issue::Abandon: c_.abandons++; break;
    case Issue::Echec: c_.echecs[(uint8_t)c < kNbCauses ? (uint8_t)c : 0]++; break;
  }
  o = Ordre();
  if (vol_.actif && vol_.sujet == s) vol_.orphelin = true;  // l'appui en vol finit, puis plus rien
  reprise_[(int)s] = Reprise();
  if (i != Issue::Remplacee) pousserPublier();  // l'etat reel, en fin de sequence
}

void Automate::annulerTout(uint32_t nowMs) {
  finOrdre(Sujet::Ventilo, Issue::Annulee, Cause::Aucune, nowMs);
  finOrdre(Sujet::Lampe, Issue::Annulee, Cause::Aucune, nowMs);
}

void Automate::verifierTemps(uint32_t nowMs) {
  // Ligne muette, en mode repete seulement : aucune repetition depuis fraicheur_ms.
  if (mode_ == ModeEtat::Repete && fiable(etat_.confiance) && !muette_) {
    const uint32_t ref = etat_.luMs ? etat_.luMs : demarrageMs_;
    if (nowMs - ref >= p_.fraicheurMs) {
      etat_.confiance = Confiance::Presume;
      muette_ = true;
      arretLu_ = false;
      c_.lignesMuettes++;
      c_.anomalies++;
      Action a;
      a.type = Action::Evenement;
      a.evt = Evt::LigneMuette;
      pousser(a);
    }
  }
  // Marche prolongee sans lecture depuis prolongee_max_ms : l'etat publie ne
  // change pas, mais rien ne se planifie plus sur lui (5.9).
  if (etat_.marche == Marche::Prolongee && fiable(etat_.confiance)) {
    const uint32_t ref = etat_.luMs ? etat_.luMs : etat_.depuisMs;
    if (nowMs - ref >= p_.prolongeeMaxMs) {
      etat_.confiance = Confiance::Presume;
      c_.prolongeePerimee++;
      Action a;
      a.type = Action::Evenement;
      a.evt = Evt::ProlongeePerimee;
      pousser(a);
    }
  }
  if (vol_.actif && vol_.fini) {
    Cause c = Cause::Aucune;
    const Verdict v = juger(nowMs, &c);
    if (v != Verdict::Attendre) appliquerVerdict(v, c, nowMs);
  }
  const Sujet sujets[2] = {Sujet::Ventilo, Sujet::Lampe};
  for (const Sujet s : sujets) {
    Ordre &o = s == Sujet::Ventilo ? ov_ : ol_;
    if (!o.actif) continue;
    if (nowMs - o.recuMs >= p_.sequenceMaxMs) {
      finOrdre(s, Issue::Abandon, Cause::Duree, nowMs);
    } else if (o.bloqueMs && nowMs - o.bloqueMs >= p_.attenteEtatMs) {
      finOrdre(s, Issue::Abandon, piloteOk_ ? Cause::SansLecture : Cause::Pilote, nowMs);
    }
  }
}

Automate::Verdict Automate::juger(uint32_t nowMs, Cause *cause) const {
  const Vol &v = vol_;
  *cause = Cause::Aucune;
  if (v.res == ResultatAppui::Delai) return Verdict::NonPris;  // rien emis
  if (v.res == ResultatAppui::Garde) {
    *cause = Cause::Garde;
    return Verdict::Echec;
  }
  if (v.res == ResultatAppui::Erreur) {
    *cause = Cause::Pilote;
    return Verdict::Echec;
  }
  const bool collision = v.res == ResultatAppui::Collision;
  const uint32_t conf = p_.confirmationMs;
  const bool marche = v.touche == Touche::Marche;
  // 1. Etat complet lu sur le fil apres la fin de l'appui.
  if (v.filLu) {
    if (accordFil(v.fil, v.attendu)) return Verdict::Reussi;
    if (!accordFil(v.fil, v.avant)) return Verdict::Autre;  // un appui du panneau
    // Inchange, lu au-dela du delai de reponse de la carte, apres toute anomalie.
    if (v.filMs - v.finMs >= conf && (!v.anomalie || (int32_t)(v.filMs - v.anomalieMs) > 0))
      return Verdict::NonPris;
  }
  // 2. Lecture annexe apres la fin de l'appui (moteur ou lampe).
  if (v.annexeLue && !marche) {
    if (accordAnnexe(v.annexeEtat, v.attendu, v.touche)) return Verdict::Reussi;
    if (!accordAnnexe(v.annexeEtat, v.avant, v.touche)) return Verdict::Autre;
    if (v.annexeMs - v.finMs >= conf) return Verdict::NonPris;
  }
  // 3. Sans lecture decisive.
  if (mode_ == ModeEtat::AppuisSeuls && marche) {
    // Le voyant marche ne se lit pas : l'echo de notre trame et la table.
    // Jamais de nouvel essai pour "marche" dans ce mode.
    if (!collision && v.echo) return Verdict::Reussi;
    if (nowMs - v.finMs < conf) return Verdict::Attendre;
    *cause = collision ? Cause::Collision : Cause::SansLecture;
    return Verdict::Echec;
  }
  if (mode_ == ModeEtat::Changements && !collision && v.echo && !v.filLu && !v.anomalie &&
      nowMs - v.finMs >= conf)
    return Verdict::NonPris;  // notre trame est passee, la carte n'a rien emis
  // La lecture suivante doit venir : la repetition (repete) ou la lecture
  // annexe (moteur, lampe). Au-dela, echec.
  uint32_t limite = conf;
  if (mode_ == ModeEtat::Repete) limite += p_.fraicheurMs;
  else if (annexe_ && !marche) limite += conf;
  if (nowMs - v.finMs < limite) return Verdict::Attendre;
  *cause = collision ? Cause::Collision : (v.filLu || v.annexeLue) ? Cause::NonConfirme : Cause::SansLecture;
  return Verdict::Echec;
}

void Automate::appliquerVerdict(Verdict v, Cause c, uint32_t nowMs) {
  const Vol vol = vol_;
  vol_ = Vol();
  if (vol.res != ResultatAppui::Delai) {
    aEmis_ = true;
    dernierFinMs_ = vol.finMs;
  }
  const int s = (int)vol.sujet;
  switch (v) {
    case Verdict::Reussi:
      reprise_[s] = Reprise();
      return;
    case Verdict::Autre:  // un autre etat : un appui du panneau (5.6)
      annulerTout(nowMs);
      return;
    case Verdict::Echec:
      if (!vol.orphelin) finOrdre(vol.sujet, Issue::Echec, c, nowMs);
      return;
    case Verdict::NonPris:
      break;
    case Verdict::Attendre:
      return;
  }
  if (vol.orphelin) return;
  if (mode_ == ModeEtat::AppuisSeuls && etat_.marche != vol.avant.marche) {
    Etat n = etat_;  // la deduction de l'echo n'a pas eu lieu
    n.marche = vol.avant.marche;
    changer(n, Origine::Module, nowMs);
  }
  const bool delai = vol.res == ResultatAppui::Delai;
  if (vol.sonde && !delai) {
    // Sonde de vitesse sans effet : la hotte etait eteinte (une vitesse sans
    // marche ne fait rien). La suite : marche, puis la vitesse.
    Etat n = etat_;
    n.marche = Marche::Eteinte;
    n.source = Source::Deduit;
    n.depuisMs = nowMs;
    changer(n, Origine::Module, nowMs);
    Action a;
    a.type = Action::Evenement;
    a.evt = Evt::SondeVitesse;
    pousser(a);
    return;
  }
  const bool permis = delai || !(mode_ == ModeEtat::AppuisSeuls && vol.touche == Touche::Marche);
  if (vol.essai == 0 && permis) {  // un nouvel essai, un seul, l'echec etant prouve (regle 2)
    reprise_[s].actif = true;
    reprise_[s].touche = vol.touche;
    repriseImmediate_ = delai;
    c_.nouveauxEssais++;
    return;
  }
  // Deuxieme echec prouve. Une vitesse depuis "armee" deduite qui ne prend
  // pas : la deduction du voyant marche est dementie.
  if (vitesse(vol.touche) && vol.avant.marche == Marche::Armee && vol.avant.moteur == Moteur::Arret &&
      etat_.confiance != Confiance::Confirme) {
    Etat n = etat_;
    n.marche = Marche::Inconnue;
    changer(n, Origine::Inconnue, nowMs);
    Action a;
    a.type = Action::Evenement;
    a.evt = Evt::DeductionDementie;
    pousser(a);
  }
  finOrdre(vol.sujet, Issue::Echec, Cause::NonConfirme, nowMs);
}

bool Automate::delaiMoteurOk(uint32_t nowMs) const { return nowMs - changementMoteurMs_ >= p_.delaiMoteurMs; }

Automate::Plan Automate::planVentilo(uint32_t nowMs, Touche *t, bool *sonde) {
  *sonde = false;
  if (!ov_.actif) return Plan::Rien;
  const Etat &e = etat_;
  if (!fiable(e.confiance) || !piloteOk_) return Plan::Bloque;
  if (cibleEteint_) {
    if (e.moteur != Moteur::Arret) {  // vitesse active, arret du moteur lu, puis marche
      *t = toucheDe(e.moteur);
      return delaiMoteurOk(nowMs) ? Plan::Presser : Plan::Attente;
    }
    switch (e.marche) {
      case Marche::Eteinte:
        return Plan::Atteint;
      case Marche::Inconnue: {  // rien sur le voyant marche ; C1 tient (moteur et lampe)
        c_.marcheInconnue++;
        Action a;
        a.type = Action::Evenement;
        a.evt = Evt::MarcheInconnue;
        pousser(a);
        return Plan::Atteint;
      }
      case Marche::Armee:
        if (!arretLu_) return Plan::Bloque;  // regle 3
        *t = Touche::Marche;
        return Plan::Presser;
      case Marche::Prolongee:  // prolongee moteur arrete : incoherent, on attend une lecture
        return Plan::Bloque;
    }
    return Plan::Bloque;
  }
  const Moteur y = cibleMoteur_;
  if (e.moteur == y) return Plan::Atteint;  // prolongee-Vy comprise : elle continue (Q38)
  if (e.moteur != Moteur::Arret) {
    // Prolongee-Vx : vitesse active d'abord (Vx, armee, puis Vy) ; sinon Vy directement.
    *t = e.marche == Marche::Prolongee ? toucheDe(e.moteur) : toucheDe(y);
    return delaiMoteurOk(nowMs) ? Plan::Presser : Plan::Attente;
  }
  switch (e.marche) {
    case Marche::Armee:
      *t = toucheDe(y);
      return delaiMoteurOk(nowMs) ? Plan::Presser : Plan::Attente;
    case Marche::Eteinte:
      if (!arretLu_) return Plan::Bloque;  // regle 3, y compris apres une mise sous tension
      *t = Touche::Marche;
      return Plan::Presser;
    case Marche::Inconnue:  // sonde de vitesse (regle 7), si elle est permise (Q6)
      if (!p_.sondeVitesse || !arretLu_) return Plan::Bloque;
      *t = toucheDe(y);
      *sonde = true;
      return delaiMoteurOk(nowMs) ? Plan::Presser : Plan::Attente;
    case Marche::Prolongee:
      return Plan::Bloque;
  }
  return Plan::Bloque;
}

Automate::Plan Automate::planLampe(Touche *t) const {
  if (!ol_.actif) return Plan::Rien;
  const Etat &e = etat_;
  // Une bascule appuyee a l'aveugle ferait l'inverse une fois sur deux.
  if (!fiable(e.confiance) || !e.lampeConnue || !piloteOk_) return Plan::Bloque;
  if (e.lampe == cibleLampe_) return Plan::Atteint;
  *t = Touche::Lumiere;
  return Plan::Presser;
}

Action Automate::presser(Sujet s, Touche t, bool sonde, uint32_t nowMs) {
  Ordre &o = s == Sujet::Ventilo ? ov_ : ol_;
  Reprise &r = reprise_[(int)s];
  vol_ = Vol();
  vol_.actif = true;
  vol_.sujet = s;
  vol_.touche = t;
  if (++idAppui_ == 0) idAppui_ = 1;
  vol_.id = idAppui_;
  vol_.debutMs = nowMs;
  vol_.avant = etat_;
  if (sonde) {  // hypothese a confirmer : la hotte etait armee
    vol_.sonde = true;
    vol_.attendu = etat_;
    vol_.attendu.marche = Marche::Armee;
    vol_.attendu.moteur = moteurDe(t);
  } else {
    vol_.attendu = appliquer(etat_, t).apres;
  }
  vol_.essai = r.actif && r.touche == t ? 1 : 0;
  r = Reprise();
  repriseImmediate_ = false;
  arretLu_ = false;
  o.appuis++;
  c_.appuisModule++;
  Action a;
  a.type = Action::Appuyer;
  a.touche = t;
  a.idAppui = vol_.id;
  return a;
}

bool Automate::planifier(uint32_t nowMs, Action *out) {
  const Sujet sujets[2] = {Sujet::Ventilo, Sujet::Lampe};  // le ventilateur d'abord
  for (const Sujet s : sujets) {
    Ordre &o = s == Sujet::Ventilo ? ov_ : ol_;
    if (!o.actif) continue;
    if (vol_.actif && !vol_.orphelin && vol_.sujet == s) continue;  // son verdict d'abord
    Touche t = Touche::Marche;
    bool sonde = false;
    const Plan pl = s == Sujet::Ventilo ? planVentilo(nowMs, &t, &sonde) : planLampe(&t);
    if (pl != Plan::Bloque) o.bloqueMs = 0;
    else if (!o.bloqueMs) o.bloqueMs = nowMs ? nowMs : 1;
    if (pl == Plan::Atteint) {
      finOrdre(s, Issue::Ok, Cause::Aucune, nowMs);
      continue;
    }
    if (pl != Plan::Presser || vol_.actif) continue;  // un appui a la fois
    if (aEmis_ && !repriseImmediate_ && nowMs - dernierFinMs_ < p_.entreAppuisMs) continue;
    *out = presser(s, t, sonde, nowMs);
    return true;
  }
  return false;
}

Action Automate::suivante(uint32_t nowMs) {
  if (!nb_) verifierTemps(nowMs);
  if (!nb_) {
    Action a;
    if (planifier(nowMs, &a)) return a;
  }
  if (!nb_ && aPublier_ && !ov_.actif && !ol_.actif) pousserPublier();
  if (!nb_) return Action();
  Action a = file_[tete_];
  tete_ = (uint8_t)((tete_ + 1) % kFile);
  nb_--;
  if (a.type == Action::Publier) a.etat = etat_;  // l'etat du moment
  return a;
}

Diagnostic Automate::diagnostic(uint32_t nowMs) const {
  Diagnostic d;
  d.c = c_;
  d.ventilo = ov_.actif;
  d.cibleEteint = cibleEteint_;
  d.cibleMoteur = cibleMoteur_;
  d.idVentilo = ov_.id;
  d.canalVentilo = ov_.canal;
  d.lampe = ol_.actif;
  d.cibleLampe = cibleLampe_;
  d.idLampe = ol_.id;
  d.canalLampe = ol_.canal;
  d.enVol = vol_.actif;
  d.touche = vol_.touche;
  d.essai = vol_.essai;
  d.volDepuisMs = vol_.debutMs;
  const uint32_t ecoule = nowMs - changementMoteurMs_;
  d.delaiMoteurResteMs = ecoule >= p_.delaiMoteurMs ? 0 : p_.delaiMoteurMs - ecoule;
  d.piloteEnService = piloteOk_;
  d.marcheAutorisee = arretLu_;
  return d;
}

}  // namespace hotte
```

- [ ] **Étape 5 : Lancer les tests : ils passent**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
test_hotte_etat : 193 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 6 : Committer**

```bash
git add src/produit/hotte_etat.h src/produit/hotte_etat.cpp tools/tests/test_hotte_etat.cpp
git commit -m "$(cat <<'FIN'
Ecrire l'automate de la hotte : ordres, confiance, sequences, regle d'or

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 3 : Pilote de ligne et hotte simulée

Le pilote ne décide rien : il traduit la ligne en événements et un appui
en trame. La hotte simulée suit la table du §5.2 derrière la même interface ;
elle porte les garde-fous 6 et 7 du pilote réel (deux verrous de la règle
d'or, indépendants de l'automate).

**Fichiers :**
- Créer : `src/produit/pilote_ligne.h`
- Créer : `src/produit/pilote_simule.h`
- Créer : `src/produit/pilote_simule.cpp`
- Créer : `tools/tests/test_pilote_simule.cpp`
- Modifier : `tools/tests/test_hote.sh`

**Interfaces :**
- Consomme : `hotte::Etat`, `Touche`, `Origine`, `ModeEtat`, `ResultatAppui`, `appliquer`, `moteurDe`.
- Produit (namespace `ligne`) : `struct Evenement` (`EtatLu`, `Appui`, `FinAppui`, `Anomalie`, `Service`),
  `struct Trame`, `enum class Admission`, `struct Diagnostic`, `resultatAdmission(Admission)`,
  `class Pilote` (`demarrer`, `poll`, `evenement`, `appuyer`, `mode`, `annexe`, `diagnostic`, `trame`),
  codes `kAnoIllisible`, `kAnoDebordement`, `kAnoTenueBasse`.
- Produit (namespace `sim`) : `Inconnues`, `struct Reglages`, `bool reglagesValides(const Reglages&)`,
  `struct EtatHotte`, `etatHotteValide`, `versMot`, `depuisMot`, `class PiloteSimule : public ligne::Pilote`
  (`regler`, `reglages`, `poserHotte`, `hotte`, `appuiPanneau`, `coupure`, `illisibles`, `sansEffet`,
  `perdus`, `collisions`, `delais`, `refuser`, `refuse`, `horsService`).

- [ ] **Étape 1 : Écrire le test (modes d'état, appui, garde-fous, pannes, prolongation, réglages)**

```cpp
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
```

- [ ] **Étape 2 : Brancher le test**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
echo "tests hote : OK"
```

par :

```sh
$CXXP src/produit/hotte_etat.cpp src/produit/pilote_simule.cpp tools/tests/test_pilote_simule.cpp -o "$OUT/test_pilote_simule"
"$OUT/test_pilote_simule"
echo "tests hote : OK"
```

- [ ] **Étape 3 : Lancer les tests : ils échouent**

```bash
sh tools/tests/test_hote.sh
```

Attendu : **échec**, avec :

```
clang++: error: no such file or directory: 'src/produit/pilote_simule.cpp'
```

- [ ] **Étape 4 : Écrire l'interface `src/produit/pilote_ligne.h` (§4.3)**

```cpp
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
```

- [ ] **Étape 5 : Écrire `src/produit/pilote_simule.h`**

```cpp
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
```

- [ ] **Étape 6 : Écrire `src/produit/pilote_simule.cpp`**

```cpp
// ===========================================================================
//  Pilote de ligne simule : voir pilote_simule.h. Pur (teste sur l'hote).
// ===========================================================================
#include "pilote_simule.h"

namespace sim {

using hotte::Marche;
using hotte::Moteur;
using hotte::Touche;

bool etatHotteValide(const EtatHotte &e) {
  if (e.marche == Marche::Inconnue) return false;
  if (e.marche == Marche::Eteinte && e.moteur != Moteur::Arret) return false;
  return !(e.marche == Marche::Prolongee && e.moteur == Moteur::Arret);
}

const char *texte(Inconnues i) { return i == Inconnues::Direct ? "direct" : "rien"; }

bool reglagesValides(const Reglages &r) {
  return (uint8_t)r.mode <= (uint8_t)hotte::ModeEtat::AppuisSeuls && r.latenceMs <= 5000 && r.periodeMs >= 100 &&
         r.periodeMs <= 60000 && r.trameMs >= 10 && r.trameMs <= 1000 && r.annexeMs >= 50 && r.annexeMs <= 5000 &&
         r.prolongeeMs >= 60000 && r.prolongeeMs <= 3600000 && (uint8_t)r.inconnues <= (uint8_t)Inconnues::Direct &&
         r.delaiMoteurMs >= 1000 && r.delaiMoteurMs <= 60000;
}

uint16_t versMot(const EtatHotte &e) {
  return (uint16_t)(0x5300 | (uint16_t)e.marche | (uint16_t)e.moteur << 2 | (uint16_t)(e.lampe ? 1 : 0) << 4);
}

bool depuisMot(uint16_t v, EtatHotte *out) {
  if ((v & 0xFFE0) != 0x5300) return false;
  EtatHotte e;
  e.marche = (Marche)(v & 3);
  e.moteur = (Moteur)(v >> 2 & 3);
  e.lampe = v >> 4 & 1;
  if (!etatHotteValide(e)) return false;
  *out = e;
  return true;
}

PiloteSimule::PiloteSimule(const Reglages &r) : r_(r) {}

void PiloteSimule::regler(const Reglages &r) { r_ = r; }

void PiloteSimule::poserHotte(const EtatHotte &e, uint32_t nowMs) {
  h_ = e;
  if (h_.marche == Marche::Prolongee) debutProlMs_ = nowMs;
}

void PiloteSimule::demarrer(uint32_t nowMs) {
  for (Tache &t : taches_) t.actif = false;
  tete_ = nb_ = 0;
  enCours_ = false;
  luUnEtat_ = false;
  lu_ = hotte::Etat();
  changementVu_ = false;
  changementLuMs_ = nowMs;
  dernierEtatMs_ = derniereAnnexeMs_ = nowMs;
}

void PiloteSimule::pousser(const ligne::Evenement &e) {
  if (nb_ == kFile) {
    d_.evenementsPerdus++;
    return;
  }
  file_[(tete_ + nb_) % kFile] = e;
  nb_++;
}

void PiloteSimule::pousserTrame(ligne::Trame::Origine o, uint8_t octet, uint32_t nowMs) {
  if (nbT_ == kTrames) {  // la plus ancienne cede
    teteT_ = (uint8_t)((teteT_ + 1) % kTrames);
    nbT_--;
  }
  ligne::Trame &t = trames_[(teteT_ + nbT_) % kTrames];
  t.tMs = nowMs;
  t.origine = o;
  t.n = 1;
  t.octets[0] = octet;
  nbT_++;
}

bool PiloteSimule::trame(ligne::Trame *out) {
  if (!nbT_) return false;
  *out = trames_[teteT_];
  teteT_ = (uint8_t)((teteT_ + 1) % kTrames);
  nbT_--;
  return true;
}

bool PiloteSimule::evenement(ligne::Evenement *out) {
  if (!nb_) return false;
  *out = file_[tete_];
  tete_ = (uint8_t)((tete_ + 1) % kFile);
  nb_--;
  return true;
}

void PiloteSimule::planifier(const Tache &t) {
  for (Tache &s : taches_)
    if (!s.actif) {
      s = t;
      s.actif = true;
      return;
    }
  d_.evenementsPerdus++;  // agenda plein : n'arrive pas avec un appui a la fois
}

void PiloteSimule::noterLu(const hotte::Etat &e, uint32_t nowMs) {
  if (luUnEtat_ && e.moteur != lu_.moteur) {
    changementVu_ = true;
    changementLuMs_ = nowMs;
  }
  lu_ = e;
  luUnEtat_ = true;
}

void PiloteSimule::emettreEtat(uint32_t nowMs) {
  dernierEtatMs_ = nowMs;
  ligne::Evenement ev;
  ev.tMs = nowMs;
  if (illisibles_) {
    illisibles_--;
    ev.type = ligne::Evenement::Anomalie;
    ev.codeAnomalie = ligne::kAnoIllisible;
    d_.anomalies++;
    pousser(ev);
    return;
  }
  pousserTrame(ligne::Trame::Carte,
               (uint8_t)((uint8_t)h_.marche << 4 | (uint8_t)h_.moteur << 2 | (h_.lampe ? 1 : 0)), nowMs);
  ev.type = ligne::Evenement::EtatLu;
  ev.etat.marche = h_.marche;
  ev.etat.moteur = h_.moteur;
  ev.etat.lampe = h_.lampe;
  ev.etat.lampeConnue = true;
  ev.etat.confiance = hotte::Confiance::Confirme;
  ev.etat.source = hotte::Source::Fil;
  ev.etat.depuisMs = ev.etat.luMs = nowMs;
  noterLu(ev.etat, nowMs);
  d_.etatsLus++;
  pousser(ev);
}

void PiloteSimule::emettreAnnexe(uint32_t nowMs) {
  derniereAnnexeMs_ = nowMs;
  ligne::Evenement ev;
  ev.type = ligne::Evenement::EtatLu;
  ev.tMs = nowMs;
  ev.etat.marche = Marche::Inconnue;  // la lecture annexe ne lit pas le voyant marche
  ev.etat.moteur = h_.moteur;
  ev.etat.lampe = h_.lampe;
  ev.etat.lampeConnue = true;
  ev.etat.confiance = hotte::Confiance::Deduit;
  ev.etat.source = hotte::Source::Annexe;
  ev.etat.depuisMs = ev.etat.luMs = nowMs;
  noterLu(ev.etat, nowMs);
  d_.lecturesAnnexe++;
  pousser(ev);
}

void PiloteSimule::changement(uint32_t nowMs) {
  if (r_.mode != hotte::ModeEtat::AppuisSeuls) emettreEtat(nowMs);
}

void PiloteSimule::carte(Touche t, uint32_t nowMs) {
  const EtatHotte avant = h_;
  if (t == Touche::Lumiere) {
    h_.lampe = !h_.lampe;
  } else if (t == Touche::Marche) {
    switch (h_.marche) {
      case Marche::Eteinte: h_.marche = Marche::Armee; break;
      case Marche::Armee:
        if (h_.moteur == Moteur::Arret) {
          h_.marche = Marche::Eteinte;
        } else {
          h_.marche = Marche::Prolongee;
          debutProlMs_ = nowMs;
        }
        break;
      case Marche::Prolongee:
        if (r_.inconnues == Inconnues::Direct) {
          h_.marche = Marche::Eteinte;
          h_.moteur = Moteur::Arret;
        }
        break;
      case Marche::Inconnue: break;
    }
  } else {
    const Moteur v = hotte::moteurDe(t);
    switch (h_.marche) {
      case Marche::Eteinte: break;  // une vitesse sans marche ne fait rien
      case Marche::Armee: h_.moteur = h_.moteur == v ? Moteur::Arret : v; break;
      case Marche::Prolongee:
        if (h_.moteur == v) {
          h_.marche = Marche::Armee;
          h_.moteur = Moteur::Arret;
        } else if (r_.inconnues == Inconnues::Direct) {
          h_.moteur = v;
        }
        break;
      case Marche::Inconnue: break;
    }
  }
  if (h_.marche != avant.marche || h_.moteur != avant.moteur || h_.lampe != avant.lampe) changement(nowMs);
}

bool PiloteSimule::changeMoteur(Touche t) const {
  if (t == Touche::Marche || t == Touche::Lumiere) return false;
  const hotte::Transition tr = hotte::appliquer(lu_, t);
  return !tr.observee || tr.apres.moteur != lu_.moteur;  // dans le doute, oui
}

ligne::Admission PiloteSimule::appuyer(Touche t, uint32_t idAppui, uint32_t nowMs) {
  Tache fin;
  fin.type = kFin;
  fin.id = idAppui;
  if (hs_) {
    fin.t = nowMs;
    fin.res = hotte::ResultatAppui::Erreur;
    planifier(fin);
    return ligne::Admission::Acceptee;
  }
  if (enCours_) return ligne::Admission::Occupee;
  // Garde-fou 6 : jamais "marche" moteur tournant, ni sans etat lu.
  if (t == Touche::Marche && (!luUnEtat_ || lu_.moteur != Moteur::Arret)) {
    d_.refusGarde++;
    return ligne::Admission::Refusee;
  }
  // Garde-fou 7 : delai entre deux changements du moteur vus sur le fil.
  if (changeMoteur(t) && changementVu_ && nowMs - changementLuMs_ < r_.delaiMoteurMs) {
    d_.refusGarde++;
    return ligne::Admission::Refusee;
  }
  enCours_ = true;
  if (delais_) {  // pas de silence dans attente_max_ms : rien emis
    delais_--;
    d_.delais++;
    fin.t = nowMs + 1;
    fin.res = hotte::ResultatAppui::Delai;
    planifier(fin);
    return ligne::Admission::Acceptee;
  }
  d_.appuisEmis++;
  fin.t = nowMs + r_.trameMs;
  bool recue = true, echo = true;
  if (perdus_) {
    perdus_--;
    recue = echo = false;
  } else if (collisions_) {
    collisions_--;
    d_.collisions++;
    fin.res = hotte::ResultatAppui::Collision;
    echo = false;  // trame coupee : pas d'echo lisible
    recue = collisionPrend_;
  }
  if (sansEffet_) {
    sansEffet_--;
    recue = false;
  }
  if (refuse_) recue = false;
  if (echo) {
    Tache e;
    e.type = kEcho;
    e.t = fin.t;
    e.touche = t;
    e.origine = hotte::Origine::Module;
    planifier(e);
  }
  planifier(fin);
  if (recue) {
    Tache c;
    c.type = kCarte;
    c.t = fin.t + r_.latenceMs;
    c.touche = t;
    planifier(c);
  }
  return ligne::Admission::Acceptee;
}

void PiloteSimule::appuiPanneau(Touche t, uint32_t nowMs) {
  Tache e;
  e.type = kEcho;
  e.t = nowMs + r_.trameMs;
  e.touche = t;
  e.origine = hotte::Origine::Panneau;
  planifier(e);
  Tache c;
  c.type = kCarte;
  c.t = e.t + r_.latenceMs;
  c.touche = t;
  planifier(c);
}

void PiloteSimule::coupure(uint32_t nowMs) {
  for (Tache &t : taches_)
    if (t.actif && t.type == kCarte) t.actif = false;  // la carte repart de zero
  h_ = EtatHotte();
  changement(nowMs);
}

void PiloteSimule::horsService(bool on, uint32_t nowMs) {
  if (hs_ == on) return;
  hs_ = on;
  ligne::Evenement ev;
  ev.type = ligne::Evenement::Service;
  ev.tMs = nowMs;
  ev.enService = !on;
  pousser(ev);
}

void PiloteSimule::poll(uint32_t nowMs) {
  // Taches echues, dans l'ordre du temps.
  for (;;) {
    Tache *prochaine = nullptr;
    for (Tache &t : taches_)
      if (t.actif && (int32_t)(nowMs - t.t) >= 0 && (!prochaine || (int32_t)(t.t - prochaine->t) < 0))
        prochaine = &t;
    if (!prochaine) break;
    const Tache t = *prochaine;
    prochaine->actif = false;
    if (t.type == kCarte) {
      carte(t.touche, t.t);
    } else if (t.type == kEcho) {
      pousserTrame(t.origine == hotte::Origine::Module ? ligne::Trame::Module : ligne::Trame::Panneau,
                   (uint8_t)(0xA0 | (uint8_t)t.touche), t.t);
      ligne::Evenement ev;
      ev.type = ligne::Evenement::Appui;
      ev.tMs = t.t;
      ev.touche = t.touche;
      ev.origine = t.origine;
      d_.appuisVus++;
      pousser(ev);
    } else {
      enCours_ = false;
      ligne::Evenement ev;
      ev.type = ligne::Evenement::FinAppui;
      ev.tMs = t.t;
      ev.idAppui = t.id;
      ev.resultat = t.res;
      pousser(ev);
    }
  }
  // Fin de la marche prolongee (15 min) : eteinte, ou armee (Q2).
  if (h_.marche == Marche::Prolongee && nowMs - debutProlMs_ >= r_.prolongeeMs) {
    h_.marche = r_.finArmee ? Marche::Armee : Marche::Eteinte;
    h_.moteur = Moteur::Arret;
    changement(nowMs);
  }
  if (r_.mode == hotte::ModeEtat::Repete && nowMs - dernierEtatMs_ >= r_.periodeMs) emettreEtat(nowMs);
  if (r_.annexe && nowMs - derniereAnnexeMs_ >= r_.annexeMs) emettreAnnexe(nowMs);
}

}  // namespace sim
```

- [ ] **Étape 7 : Lancer les tests : ils passent**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
test_pilote_simule : 56 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 8 : Committer**

```bash
git add src/produit/pilote_ligne.h src/produit/pilote_simule.h src/produit/pilote_simule.cpp tools/tests/test_pilote_simule.cpp tools/tests/test_hote.sh
git commit -m "$(cat <<'FIN'
Ajouter l'interface du pilote de ligne et la hotte simulee

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 4 : Tests de bout en bout de l'automate

**Fichiers :**
- Créer : `tools/tests/banc_hotte.h`
- Créer : `tools/tests/test_sequences.cpp`
- Modifier : `tools/tests/test_hote.sh`

**Interfaces :**
- Consomme : `hotte::Automate`, `sim::PiloteSimule`, `ligne::resultatAdmission`.
- Produit : `struct BancHotte` (`tools/tests/banc_hotte.h`) : l'automate relié à la hotte simulée comme
  dans `loop()`, pas de 10 ms, vérification indépendante de la règle d'or à chaque « marche » ;
  `demarrer`, `pas`, `attendreFin`, `panneau`, `mener`, `hotteEst`, `nbAppuis`.

- [ ] **Étape 1 : Écrire le banc de test `tools/tests/banc_hotte.h`**

```cpp
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
```

- [ ] **Étape 2 : Écrire `tools/tests/test_sequences.cpp`**

Chaque couple (état, cible) du §5.3 dans les cinq configurations de ligne,
les cas imposés du §9.1, l'annulation par le panneau à chaque étape, la règle
d'or sur 60 suites générées, les délais, la sonde de vitesse, le démarrage et
la marche prolongée.

```cpp
// Tests de bout en bout de l'automate avec le pilote simule (spec produit
// 9.1) : suites d'appuis de chaque couple (etat, cible) du 5.3 dans les trois
// modes d'etat, avec et sans lecture annexe ; cas imposes ; regle d'or sur
// des suites generees ; annulation par le panneau a chaque etape ; delais ;
// confirmation et nouvel essai ; sonde de vitesse ; demarrage.
// Lancer : sh tools/tests/test_hote.sh
#include <stdio.h>

#include "banc_hotte.h"
#include "verif.h"

using namespace hotte;

namespace {

struct Ligne {
  const char *nom;
  ModeEtat mode;
  bool annexe;
};
const Ligne kLignes[] = {
    {"repete", ModeEtat::Repete, false},
    {"repete+annexe", ModeEtat::Repete, true},
    {"changements", ModeEtat::Changements, false},
    {"changements+annexe", ModeEtat::Changements, true},
    {"appuis+annexe", ModeEtat::AppuisSeuls, true},
};

struct Depart {
  const char *nom;
  Marche m;
  Moteur mo;
};
const Depart kDeparts[] = {
    {"eteinte", Marche::Eteinte, Moteur::Arret}, {"armee", Marche::Armee, Moteur::Arret},
    {"v1", Marche::Armee, Moteur::V1},           {"v2", Marche::Armee, Moteur::V2},
    {"v3", Marche::Armee, Moteur::V3},           {"prol-v1", Marche::Prolongee, Moteur::V1},
    {"prol-v2", Marche::Prolongee, Moteur::V2},  {"prol-v3", Marche::Prolongee, Moteur::V3},
};

const CibleVentilo kCibles[] = {CibleVentilo::Eteint, CibleVentilo::V1, CibleVentilo::V2, CibleVentilo::V3};

Moteur moteurCible(CibleVentilo c) {
  return c == CibleVentilo::V1 ? Moteur::V1 : c == CibleVentilo::V2 ? Moteur::V2 : c == CibleVentilo::V3 ? Moteur::V3
                                                                                                          : Moteur::Arret;
}

void echec(const char *quoi, const Ligne &l, const Depart &d, CibleVentilo c) {
  printf("  -> %s : ligne %s, depart %s, cible %s\n", quoi, l.nom, d.nom, texte(c));
}

}  // namespace

// Chaque couple (etat, cible) du 5.3, dans chaque mode : issue, etat final
// de la hotte, regle d'or, aucun refus du pilote.
static void testMatrice() {
  for (const Ligne &l : kLignes)
    for (const Depart &d : kDeparts)
      for (const CibleVentilo c : kCibles) {
        BancHotte b(l.mode, l.annexe);
        b.demarrer();
        b.mener(d.m, d.mo);
        if (!b.hotteEst(d.m, d.mo)) {
          echec("depart non atteint", l, d, c);
          VERIF(false);
          continue;
        }
        b.fins.clear();
        b.a.ordreVentilo(c, 7, Canal::App, b.t);
        const Action *f = b.attendreFin();
        const Action fin0 = f ? *f : Action();
        f = f ? &fin0 : nullptr;
        b.pas(1000);  // appuis seuls : la fin vient de l'echo, avant la reaction de la carte
        const bool sansLecture = l.mode == ModeEtat::Changements && !l.annexe && d.m == Marche::Eteinte;
        const Moteur y = moteurCible(c);
        if (sansLecture && c != CibleVentilo::Eteint) {
          // Aucun etat lu depuis la mise sous tension : "marche" attend un
          // arret lu, qui ne vient pas. Abandon, jamais "marche".
          VERIF(f && f->issue == Issue::Abandon && f->cause == Cause::SansLecture);
          VERIF(b.nbAppuis(Touche::Marche) == 0);
        } else {
          const bool ok = f && f->issue == Issue::Ok && f->idOrdre == 7 && f->canal == Canal::App;
          if (!ok) echec(f ? texte(f->issue) : "pas de fin", l, d, c);
          VERIF(ok);
          bool fin;
          if (c == CibleVentilo::Eteint) fin = b.hotteEst(Marche::Eteinte, Moteur::Arret);
          else if (d.m == Marche::Prolongee && d.mo == y) fin = b.hotteEst(Marche::Prolongee, y);  // Q38
          else fin = b.hotteEst(Marche::Armee, y);
          if (!fin) echec("etat final", l, d, c);
          VERIF(fin);
        }
        if (b.violations || b.refusPilote) echec("regle d'or ou refus du pilote", l, d, c);
        VERIF(b.violations == 0);
        VERIF(b.refusPilote == 0);
      }
}

// Cas imposes du 9.1, dans les trois modes d'etat.
static void testCasImposes() {
  const Ligne rep = kLignes[0], chg = kLignes[3], app = kLignes[4];
  // Eteindre depuis V2, carte qui repond : Ok partout ; appuis seuls : marche final Deduit.
  for (const Ligne &l : {rep, chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V2);
    b.appuis.clear();
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::Matter, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && f->appuis == 2);
    VERIF(b.appuis.size() == 2 && b.appuis[0] == Touche::V2 && b.appuis[1] == Touche::Marche);
    if (l.mode == ModeEtat::AppuisSeuls) VERIF(b.a.etat().confiance == Confiance::Deduit);
    b.pas(1000);
    VERIF(b.hotteEst(Marche::Eteinte, Moteur::Arret));
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
  // Idem, premier appui V2 sans effet : un nouvel essai, puis Ok.
  for (const Ligne &l : {rep, chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V2);
    b.appuis.clear();
    b.p.sansEffet(1);
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::Matter, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok);
    VERIF(b.nbAppuis(Touche::V2) == 2 && b.nbAppuis(Touche::Marche) == 1);
    VERIF(b.a.diagnostic(b.t).c.nouveauxEssais == 1);
    b.pas(1000);
    VERIF(b.hotteEst(Marche::Eteinte, Moteur::Arret));
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
  // Lampe, trame de la carte illisible (repete) : Presume, puis la
  // repetition suivante tranche. Lampe changee : Ok, un seul appui.
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.pas(1000);
    b.p.illisibles(1);
    b.appuis.clear();
    b.a.ordreLampe(true, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Lumiere) == 1);
    VERIF(b.p.hotte().lampe);
  }
  // Lampe inchangee au-dela de confirmation_ms, apres une trame illisible : nouvel essai.
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.pas(1000);
    b.p.sansEffet(1);
    b.p.illisibles(1);
    b.appuis.clear();
    b.a.ordreLampe(true, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Lumiere) == 2);
    VERIF(b.p.hotte().lampe);
  }
  // Changements et appuis seuls : la lecture annexe tranche pour la lampe.
  for (const Ligne &l : {chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.pas(1000);
    b.p.sansEffet(1);
    b.p.illisibles(1);
    b.appuis.clear();
    b.a.ordreLampe(true, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Lumiere) == 2);
    VERIF(b.p.hotte().lampe);
  }
  // "Marche" (armee vers eteinte) sans effet. Repete et changements : nouvel
  // essai, puis Ok. Appuis seuls : jamais de nouvel essai pour "marche".
  for (const Ligne &l : {rep, chg}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.appuis.clear();
    b.p.sansEffet(1);
    b.a.ordreVentilo(CibleVentilo::Eteint, 3, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Marche) == 2);
    b.pas(1000);
    VERIF(b.hotteEst(Marche::Eteinte, Moteur::Arret));
  }
  {
    // Appuis seuls, trame perdue (ni echo ni effet) : Echec, un seul appui.
    BancHotte b(ModeEtat::AppuisSeuls, true);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.appuis.clear();
    b.p.perdus(1);
    b.a.ordreVentilo(CibleVentilo::Eteint, 3, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Echec && f->cause == Cause::SansLecture && b.nbAppuis(Touche::Marche) == 1);
    // Echo vu, carte sourde : le voyant marche ne se lit pas ; la table
    // conclut a tort (ecart assume, note au plan). Jamais de nouvel essai.
    BancHotte c(ModeEtat::AppuisSeuls, true);
    c.demarrer();
    c.mener(Marche::Armee, Moteur::Arret);
    c.appuis.clear();
    c.p.sansEffet(1);
    c.a.ordreVentilo(CibleVentilo::Eteint, 3, Canal::App, c.t);
    f = c.attendreFin();
    VERIF(f && f->issue == Issue::Ok && c.nbAppuis(Touche::Marche) == 1);
  }
  // Collision sur V2 (arret) qui a pris, puis moteur arrete lu : etape
  // reussie, puis "marche".
  for (const Ligne &l : {rep, chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V2);
    b.appuis.clear();
    b.p.collisions(1, true);
    b.a.ordreVentilo(CibleVentilo::Eteint, 4, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::V2) == 1 && b.nbAppuis(Touche::Marche) == 1);
    VERIF(b.violations == 0);
  }
  // Collision sur "marche", aucun etat utile lu : changements (sans trame
  // d'etat) et appuis seuls : Echec, cause collision.
  for (const Ligne &l : {chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.p.collisions(1, false);
    b.a.ordreVentilo(CibleVentilo::Eteint, 5, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Echec && f->cause == Cause::Collision);
  }
  // Collision, etat inchange lu au-dela du delai (repete) : nouvel essai.
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.appuis.clear();
    b.p.collisions(1, false);
    b.a.ordreVentilo(CibleVentilo::Eteint, 5, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Marche) == 2);
  }
  // Mise sous tension, hotte reellement en V2 (module redemarre seul), ordre
  // V3 : etat lu V2, puis V3, jamais "marche". Sans lecture annexe en mode
  // changements : Abandon au bout de attente_etat_ms, jamais "marche".
  for (const Ligne &l : kLignes) {
    BancHotte b(l.mode, l.annexe);
    sim::EtatHotte h;
    h.marche = Marche::Armee;
    h.moteur = Moteur::V2;
    b.p.poserHotte(h, b.t);
    b.demarrer(Demarrage::MiseSousTension);
    b.a.ordreVentilo(CibleVentilo::V3, 6, Canal::Matter, b.t);
    const Action *f = b.attendreFin();
    if (l.mode == ModeEtat::Changements && !l.annexe) {
      VERIF(f && f->issue == Issue::Abandon && f->cause == Cause::SansLecture);
      VERIF(b.appuis.empty());
    } else {
      VERIF(f && f->issue == Issue::Ok);
      VERIF(b.appuis.size() == 1 && b.appuis[0] == Touche::V3);
      VERIF(b.hotteEst(Marche::Armee, Moteur::V3));
    }
    VERIF(b.nbAppuis(Touche::Marche) == 0);
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
}

// La lampe : un appui si l'etat lu differe, rien sinon ; independante du ventilateur.
static void testLampe() {
  for (const Ligne &l : kLignes) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V1, true);
    b.appuis.clear();
    b.a.ordreLampe(true, 1, Canal::App, b.t);  // deja allumee
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && f->appuis == 0 && b.appuis.empty());
    b.a.ordreLampe(false, 2, Canal::App, b.t);
    f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && f->appuis == 1);
    b.pas(1000);
    VERIF(!b.p.hotte().lampe && b.hotteEst(Marche::Armee, Moteur::V1));
  }
}

// Un appui du panneau a chaque etape de chaque sequence : Annulee, etat
// publie tout de suite, plus aucun appui apres celui en vol.
static void testAnnulation() {
  for (const Ligne &l : kLignes)
    for (const Depart &d : kDeparts)
      for (const CibleVentilo c : kCibles) {
        for (size_t k = 0; k < 3; k++) {
          BancHotte b(l.mode, l.annexe);
          b.demarrer();
          b.mener(d.m, d.mo);
          b.appuis.clear();
          b.a.ordreVentilo(c, 9, Canal::App, b.t);
          for (uint32_t e = 0; e < 20000 && b.appuis.size() <= k && b.fins.empty(); e += 10) b.pas(10);
          if (b.appuis.size() <= k) break;  // sequence de moins de k + 1 appuis
          const size_t nPublis = b.publis.size();
          b.p.appuiPanneau(Touche::Lumiere, b.t);
          const Action *f = b.attendreFin();
          VERIF(f && f->issue == Issue::Annulee && f->idOrdre == 9);
          VERIF(b.publis.size() > nPublis);
          b.pas(8000);
          if (b.appuis.size() != k + 1) echec("appui apres annulation", l, d, c);
          VERIF(b.appuis.size() == k + 1);
          VERIF(b.violations == 0 && b.refusPilote == 0);
        }
      }
}

// Generateur pseudo-aleatoire deterministe (LCG de Numerical Recipes).
struct Alea {
  uint32_t x;
  uint32_t operator()(uint32_t n) {
    x = x * 1664525u + 1013904223u;
    return (x >> 8) % n;
  }
};

// Regle d'or, propriete : sur des suites generees (appuis du panneau, ordres
// de Maison et de l'app, pannes, attentes), jamais "marche" sans un arret du
// moteur lu depuis le dernier appui, ni moteur reel tournant ; et jamais un
// refus du pilote (l'automate attend lui-meme ses delais).
static void testRegleDor() {
  const Touche touches[] = {Touche::Marche, Touche::Lumiere, Touche::V1, Touche::V2, Touche::V3};
  const CibleVentilo cibles[] = {CibleVentilo::Eteint, CibleVentilo::V1, CibleVentilo::V2, CibleVentilo::V3,
                                 CibleVentilo::Derniere};
  uint32_t marches = 0;
  for (const Ligne &l : kLignes)
    for (uint32_t graine = 1; graine <= 12; graine++) {
      Params pa;
      pa.sondeVitesse = graine % 2;
      BancHotte b(l.mode, l.annexe, pa);
      Alea r{graine * 7919u + (uint32_t)l.mode * 104729u + (l.annexe ? 3u : 0u)};
      b.demarrer(graine % 3 ? Demarrage::MiseSousTension : Demarrage::Autre);
      for (int pasN = 0; pasN < 250; pasN++) {
        switch (r(10)) {
          case 0: b.p.appuiPanneau(touches[r(5)], b.t); break;
          case 1:
          case 2: b.a.ordreVentilo(cibles[r(5)], r(1000), r(2) ? Canal::App : Canal::Matter, b.t); break;
          case 3: b.a.ordreLampe(r(2), r(1000), Canal::Matter, b.t); break;
          case 4:
            switch (r(5)) {
              case 0: b.p.illisibles(1); break;
              case 1: b.p.sansEffet(1); break;
              case 2: b.p.collisions(1, r(2)); break;
              case 3: b.p.perdus(1); break;
              default: b.p.delais(1); break;
            }
            break;
          default: break;
        }
        b.pas(10 * (1 + r(400)));
      }
      b.pas(30000);
      marches += (uint32_t)b.nbAppuis(Touche::Marche);
      if (b.violations || b.refusPilote) printf("  -> ligne %s, graine %u\n", l.nom, graine);
      VERIF(b.violations == 0);
      VERIF(b.refusPilote == 0);
    }
  VERIF(marches > 50);  // la propriete a bien ete mise a l'epreuve
}

// Delais : delai moteur toutes origines, entre deux appuis, entrelacement
// de la lampe pendant l'attente du moteur, sequence_max_ms, attente_etat_ms.
static void testDelais() {
  {  // un appui du panneau change le moteur : l'ordre suivant attend delai_moteur_ms
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.panneau(Touche::V2);
    b.pas(300);  // la carte a reagi, l'etat est lu
    const uint32_t lu = b.t;
    b.appuis.clear();
    b.appuisT.clear();
    b.a.ordreVentilo(CibleVentilo::V3, 1, Canal::Matter, b.t);
    b.a.ordreLampe(true, 2, Canal::Matter, b.t);
    b.pas(6000);
    VERIF(b.appuis.size() == 2 && b.appuis[0] == Touche::Lumiere && b.appuis[1] == Touche::V3);
    VERIF(b.appuisT.size() == 2 && b.appuisT[1] - lu >= 2700);  // delai compte depuis l'etat lu (300 ms avant)
    VERIF(b.appuisT[1] - b.appuisT[0] >= 500 + 60);                // entre_appuis_ms apres la fin du premier
    VERIF(b.hotteEst(Marche::Armee, Moteur::V3) && b.p.hotte().lampe);
    VERIF(b.refusPilote == 0);
  }
  {  // attente_etat_ms : aucun etat utilisable, abandon au bout de 5 s
    BancHotte b(ModeEtat::Changements);
    b.demarrer();
    b.a.ordreVentilo(CibleVentilo::V2, 3, Canal::App, b.t);
    const uint32_t t0 = b.t;
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Abandon && f->cause == Cause::SansLecture);
    VERIF(b.t - t0 >= 5000 && b.t - t0 <= 5100);
    VERIF(!b.publis.empty());  // l'etat reel est republie
  }
  {  // sequence_max_ms : plus court que l'attente d'un etat
    Params pa;
    pa.attenteEtatMs = 60000;
    pa.sequenceMaxMs = 5000;
    BancHotte b(ModeEtat::Changements, false, pa);
    b.demarrer();
    b.a.ordreVentilo(CibleVentilo::V2, 3, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Abandon && f->cause == Cause::Duree && f->dureeMs >= 5000);
  }
  {  // le pilote plus strict que l'automate : Garde, Echec, etat republie
    Params pa;
    BancHotte b(ModeEtat::Repete, false, pa);
    sim::Reglages r = b.p.reglages();
    r.delaiMoteurMs = 20000;
    b.p.regler(r);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V1);
    const size_t nPublis = b.publis.size();
    b.a.ordreVentilo(CibleVentilo::V2, 4, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Echec && f->cause == Cause::Garde);
    VERIF(b.refusPilote == 1 && b.publis.size() > nPublis);
  }
}

// Remplacement, idempotence, et "derniere vitesse".
static void testOrdres() {
  {  // meme cible pendant sa propre sequence : rien ne repart
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V2);
    b.appuis.clear();
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::App, b.t);
    b.pas(200);
    b.a.ordreVentilo(CibleVentilo::Eteint, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->idOrdre == 1 && f->issue == Issue::Remplacee);
    f = b.attendreFin();
    VERIF(f && f->idOrdre == 2 && f->issue == Issue::Ok);
    VERIF(b.appuis.size() == 2);  // V2 puis marche, une seule fois
  }
  {  // nouvelle cible : l'ancien ordre est remplace, la sequence repart de l'etat lu
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.a.ordreVentilo(CibleVentilo::V3, 1, Canal::Matter, b.t);
    b.pas(20);
    b.a.ordreVentilo(CibleVentilo::V1, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->idOrdre == 1 && f->issue == Issue::Remplacee && f->canal == Canal::Matter);
    f = b.attendreFin();
    VERIF(f && f->idOrdre == 2 && f->issue == Issue::Ok);
    b.pas(500);
    VERIF(b.hotteEst(Marche::Armee, Moteur::V1));
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
  {  // derniere vitesse : V2 la toute premiere fois, puis le dernier palier atteint, toutes origines
    BancHotte b(ModeEtat::Repete);
    b.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::Arret);
    VERIF(b.a.derniereVitesse() == Moteur::V2);
    b.mener(Marche::Armee, Moteur::V3);
    VERIF(b.a.derniereVitesse() == Moteur::V3);
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::App, b.t);
    b.attendreFin();
    b.pas(4000);
    b.a.ordreVentilo(CibleVentilo::Derniere, 2, Canal::Matter, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok);
    b.pas(500);
    VERIF(b.hotteEst(Marche::Armee, Moteur::V3));
  }
}

// Sonde de vitesse (regle 7) : voyant marche inconnu, moteur arrete lu.
static void testSondeVitesse() {
  sim::EtatHotte eteinte;  // la verite : eteinte
  Etat nvs;                // ce que croit le module : armee (dernier etat publie)
  nvs.marche = Marche::Armee;
  nvs.moteur = Moteur::Arret;
  nvs.lampe = false;
  nvs.lampeConnue = true;
  for (uint32_t permise = 0; permise <= 1; permise++) {
    Params pa;
    pa.sondeVitesse = permise;
    BancHotte b(ModeEtat::Changements, true, pa);
    b.p.poserHotte(eteinte, b.t);
    b.demarrer(Demarrage::Autre, &nvs);
    b.pas(500);  // premiere lecture annexe : moteur arrete, voyant marche inconnu
    VERIF(b.a.etat().marche == Marche::Inconnue && b.a.etat().confiance == Confiance::Deduit);
    b.pas(3000);
    b.a.ordreVentilo(CibleVentilo::V2, 1, Canal::App, b.t);
    const Action *f = b.attendreFin();
    if (permise) {
      VERIF(f && f->issue == Issue::Ok);
      VERIF(b.appuis.size() == 3 && b.appuis[0] == Touche::V2 && b.appuis[1] == Touche::Marche &&
            b.appuis[2] == Touche::V2);
      bool sonde = false;
      for (const Action &e : b.evts) sonde |= e.evt == Evt::SondeVitesse;
      VERIF(sonde);
      b.pas(500);
      VERIF(b.hotteEst(Marche::Armee, Moteur::V2));
    } else {
      VERIF(f && f->issue == Issue::Abandon && b.appuis.empty());
    }
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
}

// Demarrage (5.7) : aucun appui injecte ; etat initial et premiere publication.
static void testDemarrage() {
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    VERIF(!b.publis.empty() && b.publis[0].etat.marche == Marche::Eteinte &&
          b.publis[0].etat.confiance == Confiance::Deduit);
    b.pas(10000);
    VERIF(b.appuis.empty());
    VERIF(b.a.etat().confiance == Confiance::Confirme);  // la repetition a confirme
  }
  {  // autre cause, sans etat en NVS : Inconnu, rien de publie jusqu'a la premiere lecture
    BancHotte b(ModeEtat::Changements);
    b.demarrer(Demarrage::Autre, nullptr);
    VERIF(b.publis.empty() && b.a.etat().confiance == Confiance::Inconnu);
    b.pas(10000);
    VERIF(b.publis.empty() && b.appuis.empty());
    b.panneau(Touche::Marche);
    b.pas(500);
    VERIF(!b.publis.empty() && b.a.etat().confiance == Confiance::Confirme);
  }
}

// Marche prolongee : la fin se lit (5.9) ; sans lecture au-dela de
// prolongee_max_ms, Presume, etat publie inchange.
static void testProlongee() {
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Prolongee, Moteur::V2);
    b.pas(900000);
    b.pas(2000);
    VERIF(b.hotteEst(Marche::Eteinte, Moteur::Arret));
    VERIF(!b.publis.empty() && b.publis.back().etat.marche == Marche::Eteinte &&
          b.publis.back().etat.moteur == Moteur::Arret);
    VERIF(!b.changements.empty() && b.changements.back().origine == Origine::Inconnue);
  }
  {
    BancHotte b(ModeEtat::Changements);
    sim::Reglages r = b.p.reglages();
    r.prolongeeMs = 3600000;  // la hotte simulee ne finit pas : rien ne se lit
    b.p.regler(r);
    b.demarrer();
    b.mener(Marche::Prolongee, Moteur::V2);
    const size_t n = b.publis.size();
    b.pas(1200000);
    bool perimee = false;
    for (const Action &e : b.evts) perimee |= e.evt == Evt::ProlongeePerimee;
    VERIF(perimee && b.a.etat().confiance == Confiance::Presume);
    VERIF(b.publis.size() == n);  // etat publie inchange
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Abandon && b.appuis.empty());  // rien planifie sur un etat perime
  }
}

int main() {
  testMatrice();
  testCasImposes();
  testLampe();
  testAnnulation();
  testRegleDor();
  testDelais();
  testOrdres();
  testSondeVitesse();
  testDemarrage();
  testProlongee();
  return bilan("test_sequences");
}
```

- [ ] **Étape 3 : Brancher le test**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
echo "tests hote : OK"
```

par :

```sh
$CXXP src/produit/hotte_etat.cpp src/produit/pilote_simule.cpp tools/tests/test_sequences.cpp -o "$OUT/test_sequences"
"$OUT/test_sequences"
echo "tests hote : OK"
```

- [ ] **Étape 4 : Lancer les tests : ils passent (l'automate des tâches 1 et 2 est complet)**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
test_sequences : 1670 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 5 : Vérifier que les tests mordent : sans la règle 3, ils échouent**

Le mutant appuie « marche » depuis « éteinte » sans arrêt lu : la propriété et la matrice le voient. Le nombre d'échecs doit être non nul. Rien n'est modifié dans le dépôt.

```bash
cp src/produit/hotte_etat.cpp "$TMPDIR/mutant.cpp"
sed -i '' '/if (!arretLu_) return Plan::Bloque;  \/\/ regle 3, y compris/d' "$TMPDIR/mutant.cpp"
clang++ -std=c++17 -Wall -Wextra -Werror -Isrc -Isrc/produit "$TMPDIR/mutant.cpp" src/produit/pilote_simule.cpp tools/tests/test_sequences.cpp -o "$TMPDIR/mutant"
"$TMPDIR/mutant" | tail -1
```

Attendu :

```
test_sequences : 1682 verifications, 87 echecs
```

- [ ] **Étape 6 : Committer**

```bash
git add tools/tests/banc_hotte.h tools/tests/test_sequences.cpp tools/tests/test_hote.sh
git commit -m "$(cat <<'FIN'
Tester l'automate de bout en bout avec la hotte simulee

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 5 : Correspondance Matter et boîte d'intentions

**Fichiers :**
- Créer : `src/produit/hotte_map.h`
- Créer : `src/produit/hotte_map.cpp`
- Créer : `tools/tests/test_hotte_map.cpp`
- Modifier : `tools/tests/test_sequences.cpp`
- Modifier : `tools/tests/test_hote.sh`

**Interfaces :**
- Consomme : `hotte::Moteur`, `CibleVentilo`, `Etat`, `memeEtat`.
- Produit (namespace `hotte`) : `kFanOff`..`kFanSmart` ; `Moteur palier(uint8_t)` ; `uint8_t pourcent(Moteur)` ;
  `uint8_t fanMode(Moteur)` ; `CibleVentilo cibleDe(Moteur)` ; `CibleVentilo cibleDeMode(uint8_t)` ;
  `struct Reflet { uint8_t fanMode; uint8_t pourcent; bool lampe; }` ; `Reflet reflet(const Etat&)` ;
  `class RegroupementEp1` (`ecrireMode`, `ecrirePourcent`, `ouvert`,
  `pret(now, calmeMs, plafondMs, CibleVentilo*, uint32_t *premierMs)`) ; `class RegroupementEp2`
  (`ecrire`, `ouvert`, `pret(now, calmeMs, plafondMs, bool*, uint32_t*)`) ;
  `bool ignoreeAuDemarrage(premierMs, demarrageMs, ignoreMs)` ; `class EcritureDifferee`
  (`ecrite`, `noter`, `echue`, `enAttente`, `valeur`) ; `uint16_t versNvs(const Etat&)` ;
  `bool depuisNvs(uint16_t, Etat*)`.

- [ ] **Étape 1 : Écrire le test (paliers, reflet, priorités de la règle 6, rafales, démarrage, NVS)**

```cpp
// Tests hote de la correspondance Matter (src/produit/hotte_map.*) : paliers
// et pourcentages (3.2, 3.3), reflet d'un etat, regroupement des ecritures
// d'EP1 et d'EP2 (5.4, regle 6), garde-fou de demarrage, ecritures NVS
// differees et etat publie en NVS (4.4).
// Lancer : sh tools/tests/test_hote.sh
#include "hotte_map.h"
#include "verif.h"

using namespace hotte;

static void testCorrespondance() {
  VERIF(palier(0) == Moteur::Arret);
  for (int p = 1; p <= 100; p++) {
    const Moteur m = palier((uint8_t)p);
    VERIF(m == (p <= 33 ? Moteur::V1 : p <= 66 ? Moteur::V2 : Moteur::V3));
    // Aller et retour : un curseur pose a p revient a la valeur canonique de son palier.
    VERIF(palier(pourcent(m)) == m);
  }
  VERIF(palier(101) == Moteur::V3 && palier(255) == Moteur::V3);
  VERIF(pourcent(Moteur::Arret) == 0 && pourcent(Moteur::V1) == 33 && pourcent(Moteur::V2) == 66 &&
        pourcent(Moteur::V3) == 100);
  VERIF(fanMode(Moteur::Arret) == kFanOff && fanMode(Moteur::V1) == kFanLow && fanMode(Moteur::V2) == kFanMedium &&
        fanMode(Moteur::V3) == kFanHigh);
  VERIF(cibleDeMode(kFanOff) == CibleVentilo::Eteint && cibleDeMode(kFanLow) == CibleVentilo::V1 &&
        cibleDeMode(kFanMedium) == CibleVentilo::V2 && cibleDeMode(kFanHigh) == CibleVentilo::V3 &&
        cibleDeMode(kFanOn) == CibleVentilo::Derniere);
  VERIF(cibleDeMode(kFanAuto) == CibleVentilo::Aucune && cibleDeMode(kFanSmart) == CibleVentilo::Aucune);
  VERIF(cibleDe(Moteur::Arret) == CibleVentilo::Eteint && cibleDe(Moteur::V2) == CibleVentilo::V2);
  // "Armee" compte comme eteinte ; la prolongee comme son palier.
  Etat e;
  e.marche = Marche::Armee;
  e.moteur = Moteur::Arret;
  e.lampe = true;
  e.lampeConnue = true;
  Reflet r = reflet(e);
  VERIF(r.fanMode == kFanOff && r.pourcent == 0 && r.lampe);
  e.marche = Marche::Prolongee;
  e.moteur = Moteur::V2;
  e.lampeConnue = false;
  r = reflet(e);
  VERIF(r.fanMode == kFanMedium && r.pourcent == 66 && !r.lampe);
}

static CibleVentilo fermer(RegroupementEp1 &g, uint32_t t) {
  CibleVentilo c = CibleVentilo::Aucune;
  VERIF(g.pret(t, 700, 3000, &c));
  VERIF(!g.ouvert());
  return c;
}

// Priorites de la regle 6.
static void testRegroupementEp1() {
  RegroupementEp1 g;
  CibleVentilo c = CibleVentilo::Aucune;
  VERIF(!g.pret(0, 700, 3000, &c));  // rien d'ouvert
  g.ecrireMode(kFanOn, 1000);
  VERIF(!g.pret(1699, 700, 3000, &c));  // pas encore le calme
  VERIF(fermer(g, 1700) == CibleVentilo::Derniere);  // On seul : derniere vitesse
  g.ecrireMode(kFanOn, 0);
  g.ecrirePourcent(40, 10);
  VERIF(fermer(g, 800) == CibleVentilo::V2);  // On et 40 % : V2
  g.ecrirePourcent(40, 0);
  g.ecrireMode(kFanOn, 10);
  VERIF(fermer(g, 800) == CibleVentilo::V2);  // 40 % et On : V2 aussi
  g.ecrirePourcent(40, 0);
  g.ecrireMode(kFanOff, 10);
  VERIF(fermer(g, 800) == CibleVentilo::Eteint);  // 40 % puis Off : eteindre
  g.ecrirePourcent(0, 0);
  g.ecrirePourcent(50, 10);
  VERIF(fermer(g, 800) == CibleVentilo::V2);  // 0 puis 50 % : V2
  g.ecrireMode(kFanOff, 0);
  g.ecrirePourcent(0, 0);  // cascade du serveur CHIP
  VERIF(fermer(g, 800) == CibleVentilo::Eteint);
  g.ecrirePourcent(0, 0);
  g.ecrireMode(kFanOff, 0);
  VERIF(fermer(g, 800) == CibleVentilo::Eteint);
  g.ecrireMode(kFanLow, 0);
  VERIF(fermer(g, 800) == CibleVentilo::V1);
  g.ecrireMode(kFanHigh, 0);
  g.ecrirePourcent(0, 5);  // un 0 plus ancien ne fait pas ceder High
  g.ecrireMode(kFanHigh, 10);
  VERIF(fermer(g, 800) == CibleVentilo::V3);
  g.ecrirePourcent(100, 0);
  VERIF(fermer(g, 800) == CibleVentilo::V3);  // bouton de la tuile a 100 % : V3 (3.3, regle 4)
  g.ecrireMode(kFanAuto, 0);
  VERIF(fermer(g, 800) == CibleVentilo::Aucune);  // hors sequence : rien
  // Premiere ecriture de la fenetre rendue ; plafond a 3 s depuis elle.
  uint32_t premier = 0;
  g.ecrirePourcent(10, 5000);
  for (uint32_t t = 5000; t < 8000; t += 500) g.ecrirePourcent(20, t);
  VERIF(!g.pret(7999, 700, 3000, &c, &premier));
  VERIF(g.pret(8000, 700, 3000, &c, &premier) && premier == 5000 && c == CibleVentilo::V1);
}

// Rafales : ecritures plus rapprochees que le calme pendant 8 s, puis plus espacees.
static void testRafales() {
  RegroupementEp1 g;
  CibleVentilo derniere = CibleVentilo::Aucune;
  int ordres = 0;
  uint8_t ecrit = 0;
  for (uint32_t t = 0; t <= 12000; t += 10) {
    if (t < 8000 && t % 500 == 0) {
      ecrit = (uint8_t)(1 + (t / 500 * 37) % 100);
      g.ecrirePourcent(ecrit, t);
    }
    CibleVentilo c;
    if (g.pret(t, 700, 3000, &c)) {
      ordres++;
      derniere = c;
    }
  }
  VERIF(ordres <= 8000 / 3000 + 1 + 1);  // au plus ceil(8 s / plafond) + 1
  VERIF(derniere == cibleDe(palier(ecrit)));
  // Plus espacees que le calme : un ordre chacune.
  RegroupementEp1 h;
  ordres = 0;
  for (uint32_t t = 0; t <= 9000; t += 10) {
    if (t < 8000 && t % 1000 == 0) h.ecrireMode(t / 1000 % 2 ? kFanHigh : kFanLow, t);
    CibleVentilo c;
    if (h.pret(t, 700, 3000, &c)) ordres++;
  }
  VERIF(ordres == 8);
}

static void testRegroupementEp2() {
  RegroupementEp2 g;
  bool on = false;
  uint32_t premier = 0;
  g.ecrire(true, 100);
  g.ecrire(false, 200);
  g.ecrire(true, 300);
  VERIF(!g.pret(449, 150, 3000, &on));
  VERIF(g.pret(450, 150, 3000, &on, &premier) && on && premier == 100 && !g.ouvert());
}

static void testDemarrage() {
  VERIF(ignoreeAuDemarrage(1000, 1000, 2000));
  VERIF(ignoreeAuDemarrage(2999, 1000, 2000));
  VERIF(!ignoreeAuDemarrage(3000, 1000, 2000));
  VERIF(ignoreeAuDemarrage(990, 1000, 2000));  // deposee pendant Matter.begin()
  VERIF(!ignoreeAuDemarrage(1000, 1000, 0));
}

static void testEcritureDifferee() {
  EcritureDifferee d;
  d.ecrite(2);  // relue au demarrage
  d.noter(2, 0);
  VERIF(!d.enAttente());  // egale a l'ecrite : rien
  d.noter(3, 1000);
  VERIF(d.enAttente() && !d.echue(10999) && d.echue(11000) && d.valeur() == 3);
  d.noter(1, 5000);  // un nouveau changement repousse l'echeance
  VERIF(!d.echue(14999) && d.echue(15000) && d.valeur() == 1);
  d.ecrite(1);
  VERIF(!d.enAttente());
  d.noter(3, 20000);
  d.noter(1, 21000);  // revenue a la valeur ecrite : plus rien a ecrire
  VERIF(!d.enAttente());
  EcritureDifferee neuve;  // NVS vide : la premiere valeur s'ecrit
  neuve.noter(2, 0);
  VERIF(neuve.enAttente() && neuve.echue(10000));
}

static void testEtatNvs() {
  const Marche marches[] = {Marche::Eteinte, Marche::Armee, Marche::Prolongee, Marche::Inconnue};
  const Moteur moteurs[] = {Moteur::Arret, Moteur::V1, Moteur::V2, Moteur::V3};
  for (const Marche m : marches)
    for (const Moteur mo : moteurs)
      for (int l = 0; l < 4; l++) {
        Etat e;
        e.marche = m;
        e.moteur = mo;
        e.lampe = l & 1;
        e.lampeConnue = l & 2;
        Etat r;
        VERIF(depuisNvs(versNvs(e), &r) && memeEtat(r, e));
      }
  Etat r;
  VERIF(!depuisNvs(0, &r) && !depuisNvs(0xFFFF, &r) && !depuisNvs(0xA5C0, &r));
}

int main() {
  testCorrespondance();
  testRegroupementEp1();
  testRafales();
  testRegroupementEp2();
  testDemarrage();
  testEcritureDifferee();
  testEtatNvs();
  return bilan("test_hotte_map");
}
```

- [ ] **Étape 2 : Inclure la correspondance dans `test_sequences.cpp`**

Dans `tools/tests/test_sequences.cpp`, remplacer :

```cpp
#include "banc_hotte.h"
```

par :

```cpp
#include "banc_hotte.h"
#include "hotte_map.h"
```

- [ ] **Étape 3 : Ajouter le test du regroupement et du délai moteur**

Écritures d'EP1 espacées, par le regroupement, jusqu'à la hotte simulée : au plus un changement du moteur par `delai_moteur_ms`.

Dans `tools/tests/test_sequences.cpp`, remplacer :

```cpp
int main() {
```

par :

```cpp
// Ecritures d'EP1 plus espacees que le calme (un ordre chacune), alternant
// V1 et V3 pendant 8 s : au plus un changement du moteur par delai_moteur_ms,
// et la hotte finit sur la derniere ecriture.
static void testRegroupementMoteur() {
  BancHotte b(ModeEtat::Repete);
  b.demarrer();
  b.mener(Marche::Armee, Moteur::Arret);
  RegroupementEp1 g;
  const Params &pa = b.a.params();
  std::vector<uint32_t> changements;
  Moteur avant = b.p.hotte().moteur;
  const uint32_t t0 = b.t;
  uint8_t dernier = 0;
  while (b.t - t0 < 20000) {
    const uint32_t e = b.t - t0;
    if (e < 8000 && e % 1000 == 0) {
      dernier = e / 1000 % 2 ? 100 : 20;
      g.ecrirePourcent(dernier, b.t);
    }
    CibleVentilo c;
    if (g.pret(b.t, pa.lissageCalmeMs, pa.lissagePlafondMs, &c)) b.a.ordreVentilo(c, 0, Canal::Matter, b.t);
    b.pas(10);
    if (b.p.hotte().moteur != avant) {
      changements.push_back(b.t);
      avant = b.p.hotte().moteur;
    }
  }
  VERIF(changements.size() >= 2);
  for (size_t i = 1; i < changements.size(); i++) VERIF(changements[i] - changements[i - 1] >= pa.delaiMoteurMs);
  VERIF(b.p.hotte().moteur == palier(dernier));
  VERIF(b.violations == 0 && b.refusPilote == 0);
}

int main() {
```

- [ ] **Étape 4 : L'appeler depuis `main`**

Dans `tools/tests/test_sequences.cpp`, remplacer :

```cpp
  testProlongee();
```

par :

```cpp
  testProlongee();
  testRegroupementMoteur();
```

- [ ] **Étape 5 : Lier `hotte_map.cpp` au test de bout en bout**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
$CXXP src/produit/hotte_etat.cpp src/produit/pilote_simule.cpp tools/tests/test_sequences.cpp -o "$OUT/test_sequences"
```

par :

```sh
$CXXP src/produit/hotte_etat.cpp src/produit/hotte_map.cpp src/produit/pilote_simule.cpp tools/tests/test_sequences.cpp -o "$OUT/test_sequences"
```

- [ ] **Étape 6 : Brancher le test de la correspondance**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
echo "tests hote : OK"
```

par :

```sh
$CXXP src/produit/hotte_etat.cpp src/produit/hotte_map.cpp tools/tests/test_hotte_map.cpp -o "$OUT/test_hotte_map"
"$OUT/test_hotte_map"
echo "tests hote : OK"
```

- [ ] **Étape 7 : Lancer les tests : ils échouent**

```bash
sh tools/tests/test_hote.sh
```

Attendu : **échec**, avec :

```
clang++: error: no such file or directory: 'src/produit/hotte_map.cpp'
```

- [ ] **Étape 8 : Écrire `src/produit/hotte_map.h`**

```cpp
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
```

- [ ] **Étape 9 : Écrire `src/produit/hotte_map.cpp`**

```cpp
// ===========================================================================
//  Correspondance Matter et boite d'intentions : voir hotte_map.h. Pur.
// ===========================================================================
#include "hotte_map.h"

namespace hotte {

Moteur palier(uint8_t p) {
  if (p == 0) return Moteur::Arret;
  if (p <= 33) return Moteur::V1;
  if (p <= 66) return Moteur::V2;
  return Moteur::V3;
}

uint8_t pourcent(Moteur m) {
  switch (m) {
    case Moteur::V1: return 33;
    case Moteur::V2: return 66;
    case Moteur::V3: return 100;
    default: return 0;
  }
}

uint8_t fanMode(Moteur m) {
  switch (m) {
    case Moteur::V1: return kFanLow;
    case Moteur::V2: return kFanMedium;
    case Moteur::V3: return kFanHigh;
    default: return kFanOff;
  }
}

CibleVentilo cibleDe(Moteur m) {
  switch (m) {
    case Moteur::V1: return CibleVentilo::V1;
    case Moteur::V2: return CibleVentilo::V2;
    case Moteur::V3: return CibleVentilo::V3;
    default: return CibleVentilo::Eteint;
  }
}

CibleVentilo cibleDeMode(uint8_t m) {
  switch (m) {
    case kFanOff: return CibleVentilo::Eteint;
    case kFanLow: return CibleVentilo::V1;
    case kFanMedium: return CibleVentilo::V2;
    case kFanHigh: return CibleVentilo::V3;
    case kFanOn: return CibleVentilo::Derniere;
    default: return CibleVentilo::Aucune;  // Auto, Smart : hors de la sequence OffLowMedHigh
  }
}

Reflet reflet(const Etat &e) {
  Reflet r;
  r.fanMode = fanMode(e.moteur);
  r.pourcent = pourcent(e.moteur);
  r.lampe = e.lampeConnue && e.lampe;
  return r;
}

// ---------------------------------------------------------------------------

void RegroupementEp1::noter(uint32_t nowMs) {
  if (!ouvert_) {
    ouvert_ = true;
    premierMs_ = nowMs;
  }
  dernierMs_ = nowMs;
}

void RegroupementEp1::ecrireMode(uint8_t m, uint32_t nowMs) {
  noter(nowMs);
  aMode_ = true;
  mode_ = m;
  seqMode_ = ++seq_;
}

void RegroupementEp1::ecrirePourcent(uint8_t p, uint32_t nowMs) {
  noter(nowMs);
  aPourcent_ = true;
  pourcent_ = p;
  seqPourcent_ = ++seq_;
}

bool RegroupementEp1::pret(uint32_t nowMs, uint32_t calmeMs, uint32_t plafondMs, CibleVentilo *cible,
                           uint32_t *premierMs) {
  if (!ouvert_) return false;
  // Differences signees : une ecriture deposee apres le millis() de l'appelant.
  if ((int32_t)(nowMs - dernierMs_) < (int32_t)calmeMs && (int32_t)(nowMs - premierMs_) < (int32_t)plafondMs)
    return false;
  CibleVentilo c = CibleVentilo::Aucune;
  if (aPourcent_ && (!aMode_ || seqPourcent_ > seqMode_)) {  // derniere ecriture : PercentSetting
    c = pourcent_ == 0 ? CibleVentilo::Eteint : cibleDe(palier(pourcent_));
  } else if (mode_ == kFanOff) {
    c = CibleVentilo::Eteint;
  } else if (cibleDeMode(mode_) != CibleVentilo::Aucune) {  // Low, Medium, High, On
    c = aPourcent_ && pourcent_ > 0 ? cibleDe(palier(pourcent_)) : cibleDeMode(mode_);
  }
  *cible = c;
  if (premierMs) *premierMs = premierMs_;
  *this = RegroupementEp1();
  return true;
}

void RegroupementEp2::ecrire(bool allumee, uint32_t nowMs) {
  if (!ouvert_) {
    ouvert_ = true;
    premierMs_ = nowMs;
  }
  dernierMs_ = nowMs;
  valeur_ = allumee;
}

bool RegroupementEp2::pret(uint32_t nowMs, uint32_t calmeMs, uint32_t plafondMs, bool *allumee, uint32_t *premierMs) {
  if (!ouvert_) return false;
  if ((int32_t)(nowMs - dernierMs_) < (int32_t)calmeMs && (int32_t)(nowMs - premierMs_) < (int32_t)plafondMs)
    return false;
  *allumee = valeur_;
  if (premierMs) *premierMs = premierMs_;
  *this = RegroupementEp2();
  return true;
}

bool ignoreeAuDemarrage(uint32_t premierMs, uint32_t demarrageMs, uint32_t ignoreMs) {
  return (int32_t)(premierMs - demarrageMs) < (int32_t)ignoreMs;
}

// ---------------------------------------------------------------------------

void EcritureDifferee::ecrite(uint32_t valeur) {
  connue_ = true;
  ecrite_ = valeur;
  if (attente_ && valeur_ == valeur) attente_ = false;
}

void EcritureDifferee::noter(uint32_t valeur, uint32_t nowMs) {
  if (attente_ && valeur == valeur_) return;  // deja en attente : le delai court depuis le changement
  valeur_ = valeur;
  depuisMs_ = nowMs;
  attente_ = !connue_ || valeur != ecrite_;
}

// ---------------------------------------------------------------------------

namespace {
constexpr uint16_t kMarque = 0xA500, kMasqueMarque = 0xFF00;
}

uint16_t versNvs(const Etat &e) {
  return (uint16_t)(kMarque | (uint16_t)e.marche | (uint16_t)e.moteur << 2 | (uint16_t)(e.lampe ? 1 : 0) << 4 |
                    (uint16_t)(e.lampeConnue ? 1 : 0) << 5);
}

bool depuisNvs(uint16_t v, Etat *out) {
  if ((v & kMasqueMarque) != kMarque || (v & 0xC0)) return false;
  Etat e;
  e.marche = (Marche)(v & 3);
  e.moteur = (Moteur)(v >> 2 & 3);
  e.lampe = v >> 4 & 1;
  e.lampeConnue = v >> 5 & 1;
  *out = e;
  return true;
}

}  // namespace hotte
```

- [ ] **Étape 10 : Lancer les tests : ils passent**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
test_sequences : 1676 verifications, 0 echecs
test_hotte_map : 327 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 11 : Committer**

```bash
git add src/produit/hotte_map.h src/produit/hotte_map.cpp tools/tests/test_hotte_map.cpp tools/tests/test_sequences.cpp tools/tests/test_hote.sh
git commit -m "$(cat <<'FIN'
Ajouter la correspondance Matter et le regroupement des ecritures d'EP1

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 6 : Surveillance : température de la puce et tensions

Filet de sécurité de l'amendement du 28/09 : la température de la puce
(alerte au-delà de 70 °C, retour sous 65 °C : Q34), et les deux tensions de
l'alimentation. **Aucune action sur la hotte.** `alim_alertes` vaut 0 tant que
le pont de mesure n'est pas monté (au banc, GPIO2 et GPIO3 sont en l'air).

**Fichiers :**
- Créer : `src/produit/surveillance.h`
- Créer : `src/produit/surveillance.cpp`
- Créer : `tools/tests/test_surveillance.cpp`
- Modifier : `tools/tests/test_hote.sh`

**Interfaces :**
- Produit (namespace `surv`) : `struct Params` (`temp_alerte_c`, `temp_hyst_c`, `alim_hotte_min_mv`,
  `alim_module_min_mv`, `alim_hyst_mv`, `alim_alertes`) et ses fonctions de bornes (comme `hotte::Params`) ;
  `enum class Sujet` ; `struct Alerte` ; `struct Voie` ; `class Surveillance` (`configurer`, `params`,
  `temperature(bool valide, int32_t dixiemes, uint32_t now)`, `tensions(hotteMv, hotteMinMv, moduleMv, moduleMinMv)`,
  `alerte(Alerte*)`, `tempConnue`, `tempDixiemes`, `tempMaxDixiemes`, `alerteTemperature`, `alertesTemperature`,
  `lecturesRatees`, `hotte`, `module`, `maxPoseLu`, `maxPose`, `maxPoseAEcrire`, `maxPoseEcrit`, `maxPoseRaz`,
  `kAucun`).

- [ ] **Étape 1 : Écrire le test (bornes, seuils et hystérésis, lecture en échec, maximum depuis la pose)**

```cpp
// Tests hote de la surveillance (src/produit/surveillance.*) : bornes des
// reglages, seuils et hysteresis de la temperature et des deux tensions,
// lecture en echec, maximum depuis la pose (spec produit 4.2, 7.4, 9.1).
// Lancer : sh tools/tests/test_hote.sh
#include <string.h>

#include "surveillance.h"
#include "verif.h"

using namespace surv;

static void testParams() {
  const Params p;
  VERIF(paramsValides(p) && p.tempAlerteC == 70 && p.tempHystC == 5 && p.alimHotteMinMv == 4500 &&
        p.alimModuleMinMv == 3700 && p.alimHystMv == 100 && p.alimAlertes == 0);
  const char *noms[] = {"temp_alerte_c", "temp_hyst_c", "alim_hotte_min_mv", "alim_module_min_mv", "alim_hyst_mv",
                        "alim_alertes"};
  for (uint8_t i = 0; i < kNbParams; i++) VERIF_EGAL_STR(nomParam(i), noms[i]);
  VERIF(nomParam(kNbParams) == nullptr && valeurParam(p, 0) == 70);
  uint32_t lo = 0, hi = 0;
  VERIF(bornesParam("temp_alerte_c", &lo, &hi) && lo == 20 && hi == 85);
  VERIF(bornesParam("alim_module_min_mv", &lo, &hi) && lo == 3000 && hi == 5000);
  for (uint8_t i = 0; i < kNbParams; i++) {
    Params q;
    VERIF(bornesParam(nomParam(i), &lo, &hi));
    if (!strcmp(nomParam(i), "temp_alerte_c")) q.tempHystC = 1;
    VERIF(reglerParam(&q, nomParam(i), lo) == Reglage::Ok);
    VERIF(reglerParam(&q, nomParam(i), hi) == Reglage::Ok);
    if (lo) VERIF(reglerParam(&q, nomParam(i), lo - 1) == Reglage::HorsBornes);
    VERIF(reglerParam(&q, nomParam(i), hi + 1) == Reglage::HorsBornes);
  }
  Params q;
  VERIF(reglerParam(&q, "temp_alerte_c", 25) == Reglage::Ok);  // B15 : seuil abaisse au banc
  VERIF(reglerParam(&q, "temp_hyst_c", 10) == Reglage::Ok);
  VERIF(reglerParam(&q, "inconnu", 1) == Reglage::NomInconnu);
  Params lu;
  VERIF(chargerParams(&q, sizeof(q), &lu) && lu.tempAlerteC == 25);
  q.alimHystMv = 1;
  VERIF(!chargerParams(&q, sizeof(q), &lu) && lu.tempAlerteC == 70);
  VERIF_EGAL_STR(texte(Sujet::LectureTemperature), "lecture_temperature");
  VERIF_EGAL_STR(texte(Sujet::AlimModule), "alim_module");
}

static void testTemperature() {
  Surveillance s;
  Alerte a;
  s.temperature(true, 415, 0);
  VERIF(s.tempConnue() && s.tempDixiemes() == 415 && s.tempMaxDixiemes() == 415 && !s.alerte(&a));
  s.temperature(true, 700, 10000);  // au seuil : pas encore
  VERIF(!s.alerteTemperature() && !s.alerte(&a));
  s.temperature(true, 701, 20000);
  VERIF(s.alerteTemperature() && s.alerte(&a) && a.sujet == Sujet::Temperature && a.debut && a.valeur == 701 &&
        a.seuil == 700);
  s.temperature(true, 660, 30000);  // hysteresis : retour sous 65
  VERIF(s.alerteTemperature() && !s.alerte(&a));
  s.temperature(true, 649, 40000);
  VERIF(!s.alerteTemperature() && s.alerte(&a) && !a.debut && s.alertesTemperature() == 1);
  VERIF(s.tempMaxDixiemes() == 701);
  // Lecture en echec (NAN) : alerte, puis fin a la lecture suivante.
  s.temperature(false, 0, 50000);
  VERIF(s.alerteTemperature() && s.alerte(&a) && a.sujet == Sujet::LectureTemperature && a.debut);
  s.temperature(false, 0, 60000);
  VERIF(!s.alerte(&a) && s.lecturesRatees() == 2);
  s.temperature(true, 420, 70000);
  VERIF(!s.alerteTemperature() && s.alerte(&a) && a.sujet == Sujet::LectureTemperature && !a.debut);
}

static void testTensions() {
  Params p;
  Surveillance s(p);  // alertes de tension coupees (banc) : passages comptes, aucune alerte
  Alerte a;
  s.tensions(20, 10, 15, 5);
  VERIF(s.hotte().connue && s.hotte().minSecondeMv == 10 && s.hotte().passages == 1 && !s.alerte(&a));
  p.alimAlertes = 1;
  s.configurer(p);
  s.tensions(5000, 4990, 4400, 4380);
  VERIF(!s.alerte(&a) && s.hotte().minDemarrageMv == 10);
  s.tensions(4600, 4499, 4400, 3699);  // les deux voies passent sous leur seuil
  VERIF(s.alerte(&a) && a.sujet == Sujet::AlimHotte && a.debut && a.valeur == 4499 && a.seuil == 4500);
  VERIF(s.alerte(&a) && a.sujet == Sujet::AlimModule && a.debut && a.valeur == 3699 && a.seuil == 3700);
  s.tensions(4600, 4550, 4400, 3790);  // hysteresis de 100 mV
  VERIF(!s.alerte(&a));
  s.tensions(4700, 4601, 4400, 3801);
  VERIF(s.alerte(&a) && a.sujet == Sujet::AlimHotte && !a.debut);
  VERIF(s.alerte(&a) && a.sujet == Sujet::AlimModule && !a.debut);
  VERIF(s.hotte().passages == 2 && s.module().passages == 2 && s.module().minDemarrageMv == 5);
}

static void testMaxPose() {
  Surveillance s;
  s.maxPoseLu(Surveillance::kAucun);  // NVS vide
  VERIF(!s.maxPoseAEcrire(0));
  s.temperature(true, 400, 0);
  VERIF(s.maxPose() == 400 && s.maxPoseAEcrire(0));
  s.maxPoseEcrit(0);
  s.temperature(true, 409, 70000);  // moins d'un degre de plus : rien
  VERIF(!s.maxPoseAEcrire(70000));
  s.temperature(true, 410, 80000);
  VERIF(s.maxPoseAEcrire(80000));
  s.maxPoseEcrit(80000);
  s.temperature(true, 500, 100000);  // moins d'une minute apres la derniere ecriture
  VERIF(!s.maxPoseAEcrire(100000) && s.maxPoseAEcrire(140000));
  Surveillance t;
  t.maxPoseLu(550);  // relu : une temperature plus basse ne l'ecrase pas
  t.temperature(true, 450, 0);
  VERIF(t.maxPose() == 550 && !t.maxPoseAEcrire(0));
  t.maxPoseRaz();
  VERIF(t.maxPose() == Surveillance::kAucun);
}

int main() {
  testParams();
  testTemperature();
  testTensions();
  testMaxPose();
  return bilan("test_surveillance");
}
```

- [ ] **Étape 2 : Brancher le test**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
echo "tests hote : OK"
```

par :

```sh
$CXXP src/produit/surveillance.cpp tools/tests/test_surveillance.cpp -o "$OUT/test_surveillance"
"$OUT/test_surveillance"
echo "tests hote : OK"
```

- [ ] **Étape 3 : Lancer les tests : ils échouent**

```bash
sh tools/tests/test_hote.sh
```

Attendu : **échec**, avec :

```
clang++: error: no such file or directory: 'src/produit/surveillance.cpp'
```

- [ ] **Étape 4 : Écrire `src/produit/surveillance.h`**

```cpp
#pragma once
// ===========================================================================
//  Surveillance (docs/SPEC-PRODUIT.md 4.2, 7.4) : pur
//
//  Temperature de la puce (lue toutes les 10 s) et tensions des deux voies
//  d'alimentation (minimum de chaque seconde, en mV) : seuils avec
//  hysteresis, alertes de debut et de fin, maxima, passages sous les seuils,
//  maximum depuis la pose (NVS temp_max). AUCUNE action sur la hotte : les
//  alertes ne sont que des messages.
//
//  Temperatures en dixiemes de degre (41,5 C -> 415), tensions en mV.
//  Teste sur l'hote (tools/tests/test_surveillance.cpp).
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

namespace surv {

struct Params {
  uint32_t tempAlerteC = 70, tempHystC = 5;  // Q34 : alerte au-dela de 70, retour sous 65
  uint32_t alimHotteMinMv = 4500, alimModuleMinMv = 3700, alimHystMv = 100;
  // 1 : alertes de tension en service (pont de mesure monte). 0 au banc,
  // ou GPIO2 et GPIO3 sont en l'air : les tensions sont publiees, sans alerte.
  uint32_t alimAlertes = 0;
};
// Chaque valeur dans ses bornes, et temp_hyst_c < temp_alerte_c.
bool paramsValides(const Params &p);
constexpr uint8_t kNbParams = 6;
const char *nomParam(uint8_t i);  // temp_alerte_c, temp_hyst_c, alim_hotte_min_mv, alim_module_min_mv, alim_hyst_mv, alim_alertes
uint32_t valeurParam(const Params &p, uint8_t i);
bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi);
enum class Reglage : uint8_t { Ok, NomInconnu, HorsBornes };
Reglage reglerParam(Params *p, const char *nom, uint32_t v);
bool chargerParams(const void *octets, size_t n, Params *out);

enum class Sujet : uint8_t { Temperature, LectureTemperature, AlimHotte, AlimModule };
const char *texte(Sujet s);  // temperature, lecture_temperature, alim_hotte, alim_module

struct Alerte {
  Sujet sujet = Sujet::Temperature;
  bool debut = true;  // faux : fin
  int32_t valeur = 0;  // dixiemes de degre, ou mV ; lecture en echec : 0
  int32_t seuil = 0;
};

// Une voie d'alimentation : minimum de la derniere seconde, depuis le
// demarrage, alerte avec hysteresis, passages sous le seuil.
struct Voie {
  bool connue = false;
  uint32_t actuelMv = 0, minSecondeMv = 0, minDemarrageMv = 0;
  bool sous = false;    // sous le seuil (sortie au-dela du seuil + hysteresis)
  bool alerte = false;  // sous le seuil, alertes de tension en service
  uint32_t passages = 0;
};

class Surveillance {
 public:
  explicit Surveillance(const Params &p = Params()) : p_(p) {}
  void configurer(const Params &p);
  const Params &params() const { return p_; }

  // Lecture de la temperature : valide faux si elle a echoue (NAN).
  void temperature(bool valide, int32_t dixiemes, uint32_t nowMs);
  // Tensions : actuelle et minimum de la seconde ecoulee, pour chaque voie.
  void tensions(uint32_t hotteMv, uint32_t hotteMinMv, uint32_t moduleMv, uint32_t moduleMinMv);
  // File des alertes (debut, fin), videe par la tache loop.
  bool alerte(Alerte *out);

  bool tempConnue() const { return tempConnue_; }
  int32_t tempDixiemes() const { return temp_; }
  int32_t tempMaxDixiemes() const { return tempMax_; }  // depuis le demarrage
  bool alerteTemperature() const { return alerteTemp_ || alerteLecture_; }
  uint32_t alertesTemperature() const { return alertesTemp_; }
  uint32_t lecturesRatees() const { return lecturesRatees_; }
  const Voie &hotte() const { return hotte_; }
  const Voie &module() const { return module_; }

  // Maximum depuis la pose (NVS temp_max). maxPoseLu : relu au demarrage
  // (absent : -1000). A ecrire au plus une fois par minute, et seulement
  // s'il depasse l'ecrit d'au moins 1 degre ; maxPoseEcrit apres succes.
  void maxPoseLu(int32_t dixiemes) { maxPose_ = maxPoseEcrit_ = dixiemes; }
  int32_t maxPose() const { return maxPose_; }
  bool maxPoseAEcrire(uint32_t nowMs) const;
  void maxPoseEcrit(uint32_t nowMs);
  void maxPoseRaz() { maxPose_ = maxPoseEcrit_ = kAucun; }  // a la pose, par l'USB

  static constexpr int32_t kAucun = -1000;

 private:
  void pousser(Sujet s, bool debut, int32_t valeur, int32_t seuil);
  void voie(Voie &v, Sujet s, uint32_t mv, uint32_t minMv, uint32_t seuilMv);

  Params p_;
  bool tempConnue_ = false, alerteTemp_ = false, alerteLecture_ = false;
  int32_t temp_ = 0, tempMax_ = kAucun;
  uint32_t alertesTemp_ = 0, lecturesRatees_ = 0;
  int32_t maxPose_ = kAucun, maxPoseEcrit_ = kAucun;
  bool maxPoseEcritUneFois_ = false;
  uint32_t maxPoseEcritMs_ = 0;
  Voie hotte_, module_;
  static constexpr uint8_t kFile = 8;
  Alerte file_[kFile];
  uint8_t tete_ = 0, nb_ = 0;
};

}  // namespace surv
```

- [ ] **Étape 5 : Écrire `src/produit/surveillance.cpp`**

```cpp
// ===========================================================================
//  Surveillance : voir surveillance.h. Pur (teste sur l'hote).
// ===========================================================================
#include "surveillance.h"

#include <string.h>

namespace surv {

namespace {

struct Champ {
  const char *nom;
  uint32_t Params::*m;
  uint32_t lo, hi;
};
const Champ kChamps[kNbParams] = {
    {"temp_alerte_c", &Params::tempAlerteC, 20, 85},
    {"temp_hyst_c", &Params::tempHystC, 1, 10},
    {"alim_hotte_min_mv", &Params::alimHotteMinMv, 3000, 6000},
    {"alim_module_min_mv", &Params::alimModuleMinMv, 3000, 5000},
    {"alim_hyst_mv", &Params::alimHystMv, 20, 500},
    {"alim_alertes", &Params::alimAlertes, 0, 1},
};

const Champ *champ(const char *nom) {
  if (!nom) return nullptr;
  for (const Champ &c : kChamps)
    if (!strcmp(c.nom, nom)) return &c;
  return nullptr;
}

}  // namespace

bool paramsValides(const Params &p) {
  for (const Champ &c : kChamps)
    if (p.*c.m < c.lo || p.*c.m > c.hi) return false;
  return p.tempHystC < p.tempAlerteC;
}

const char *nomParam(uint8_t i) { return i < kNbParams ? kChamps[i].nom : nullptr; }

uint32_t valeurParam(const Params &p, uint8_t i) { return i < kNbParams ? p.*kChamps[i].m : 0; }

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

const char *texte(Sujet s) {
  static const char *const k[] = {"temperature", "lecture_temperature", "alim_hotte", "alim_module"};
  return (uint8_t)s < 4 ? k[(uint8_t)s] : "?";
}

void Surveillance::configurer(const Params &p) { p_ = p; }

void Surveillance::pousser(Sujet s, bool debut, int32_t valeur, int32_t seuil) {
  if (nb_ == kFile) {  // la plus ancienne cede : la file n'est jamais pleine en pratique
    tete_ = (uint8_t)((tete_ + 1) % kFile);
    nb_--;
  }
  Alerte &a = file_[(tete_ + nb_) % kFile];
  a.sujet = s;
  a.debut = debut;
  a.valeur = valeur;
  a.seuil = seuil;
  nb_++;
}

bool Surveillance::alerte(Alerte *out) {
  if (!nb_) return false;
  *out = file_[tete_];
  tete_ = (uint8_t)((tete_ + 1) % kFile);
  nb_--;
  return true;
}

void Surveillance::temperature(bool valide, int32_t d, uint32_t) {
  if (!valide) {  // une lecture en echec est une alerte (7.4)
    lecturesRatees_++;
    if (!alerteLecture_) {
      alerteLecture_ = true;
      pousser(Sujet::LectureTemperature, true, 0, 0);
    }
    return;
  }
  if (alerteLecture_) {
    alerteLecture_ = false;
    pousser(Sujet::LectureTemperature, false, d, 0);
  }
  tempConnue_ = true;
  temp_ = d;
  if (d > tempMax_) tempMax_ = d;
  if (d > maxPose_) maxPose_ = d;
  const int32_t seuil = (int32_t)p_.tempAlerteC * 10;
  const int32_t retour = seuil - (int32_t)p_.tempHystC * 10;
  if (!alerteTemp_ && d > seuil) {
    alerteTemp_ = true;
    alertesTemp_++;
    pousser(Sujet::Temperature, true, d, seuil);
  } else if (alerteTemp_ && d < retour) {
    alerteTemp_ = false;
    pousser(Sujet::Temperature, false, d, seuil);
  }
}

void Surveillance::voie(Voie &v, Sujet s, uint32_t mv, uint32_t minMv, uint32_t seuilMv) {
  v.actuelMv = mv;
  v.minSecondeMv = minMv;
  if (!v.connue || minMv < v.minDemarrageMv) v.minDemarrageMv = minMv;
  v.connue = true;
  if (!v.sous && minMv < seuilMv) {
    v.sous = true;
    v.passages++;  // compte meme sans alerte : l'essai de 24 h le lit
  } else if (v.sous && minMv > seuilMv + p_.alimHystMv) {
    v.sous = false;
  }
  const bool alerte = v.sous && p_.alimAlertes;
  if (alerte != v.alerte) {
    v.alerte = alerte;
    pousser(s, alerte, (int32_t)minMv, (int32_t)seuilMv);
  }
}

void Surveillance::tensions(uint32_t hotteMv, uint32_t hotteMinMv, uint32_t moduleMv, uint32_t moduleMinMv) {
  voie(hotte_, Sujet::AlimHotte, hotteMv, hotteMinMv, p_.alimHotteMinMv);
  voie(module_, Sujet::AlimModule, moduleMv, moduleMinMv, p_.alimModuleMinMv);
}

bool Surveillance::maxPoseAEcrire(uint32_t nowMs) const {
  if (maxPose_ == kAucun || maxPose_ < maxPoseEcrit_ + 10) return false;
  return !maxPoseEcritUneFois_ || nowMs - maxPoseEcritMs_ >= 60000;
}

void Surveillance::maxPoseEcrit(uint32_t nowMs) {
  maxPoseEcrit_ = maxPose_;
  maxPoseEcritUneFois_ = true;
  maxPoseEcritMs_ = nowMs;
}

}  // namespace surv
```

- [ ] **Étape 6 : Lancer les tests : ils passent**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
test_surveillance : 70 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 7 : Committer**

```bash
git add src/produit/surveillance.h src/produit/surveillance.cpp tools/tests/test_surveillance.cpp tools/tests/test_hote.sh
git commit -m "$(cat <<'FIN'
Ajouter la surveillance de la temperature de la puce et des tensions

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

