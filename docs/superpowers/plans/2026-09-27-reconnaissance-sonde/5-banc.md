# Plan de la sonde, partie 5 : banc

> En-tête, contraintes globales, carte des fichiers et interfaces : [../2026-09-27-reconnaissance-sonde.md](../2026-09-27-reconnaissance-sonde.md). Les tâches s'exécutent dans l'ordre des numéros (13 à 16b).

### Tâche 13 : Motifs du banc en C++ (`src/motifs.{h,cpp}`), croisés avec `signaux.py`

Le générateur (tâche 14) émet les mêmes motifs que ceux que `tools/signaux.py`
(tâche 2) fabrique pour les tests : même contenu, mêmes segments au µs près.
`src/motifs.cpp` est pur (ni Arduino ni IDF) ; `tools/tests/dump_motifs.cpp`
l'imprime en JSON et `tools/tests/test_motifs.py` le compile avec `clang++`, puis
compare chaque motif à `signaux.trame_motif` et `signaux.octets_motif`, pour les
index 0 à 9 (plus 255, 256 et 999, pour le passage de l'index à 8 bits). La
rafale n'est comparée que par sa longueur, sa somme et ses 20 premiers segments.

Conventions reprises de la tâche 2, à l'identique : frontières des bits UART à
`(k * 1000000 + bauds / 2) / bauds` µs du début de l'octet ; bit de stop du
dernier octet compris dans la trame ; `wtc` = trame, 4T haut, réponse (5T haut
après fusion) ; `octets(Wtc)` rend 3 octets (16 bits de la trame, 8 bits de la
réponse) ; `rafale` : 0 octet, 100 000 segments de 100 µs, le premier haut.
Aucune divergence entre `signaux.py` et le contrat n'a été trouvée.

**Fichiers :**
- Créer : `src/motifs.h`
- Créer : `src/motifs.cpp`
- Créer : `tools/tests/dump_motifs.cpp`
- Tester : `tools/tests/test_motifs.py`

**Interfaces :**
- Consomme : `signaux.MOTIFS`, `signaux.octets_motif(nom: str, index: int) -> bytes`, `signaux.trame_motif(nom: str, index: int) -> list[Segment]`, `signaux.pause_motif_us(nom: str) -> int`, `signaux.repos_haut_motif(nom: str) -> bool` (tâche 2) ; `clang++` (Xcode Command Line Tools), avec les drapeaux de `test_hote.sh` (`-std=c++17 -Wall -Wextra -Werror -Isrc`).
- Produit :
  - `src/motifs.h` du contrat, à la lettre : `enum class motif::Id : uint8_t { Uart500, Uart500Inv, Uart2400, Uart2400Inv, Uart9600, Uart9600Inv, Wtc, Krona, Rafale }`, `struct motif::Seg { bool haut; uint32_t us; }`, `const char *motif::nom(Id)`, `bool motif::depuisNom(const char *, Id *)`, `bool motif::reposHaut(Id)`, `uint32_t motif::pauseUs(Id)`, `size_t motif::octets(Id, uint32_t index, uint8_t *out, size_t cap)` (0 si `cap` est trop petit), `size_t motif::trame(Id, uint32_t index, Seg *out, size_t cap)` (0 si `cap` est trop petit, jamais une trame tronquée) ;
  - trois constantes en plus (écart assumé, pour les tampons du générateur) : `constexpr size_t motif::kSegMax = 128;` (segments d'une trame, rafale exceptée ; le pire cas mesuré est 52, pour `wtc`), `constexpr uint32_t motif::kRafaleSegs = 100000;`, `constexpr uint32_t motif::kRafaleUs = 100;` ;
  - un `Id` hors de l'énumération rend `"?"`, `true`, `0`, `0`, `0`.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_motifs.py` :

```python
#!/usr/bin/env python3
"""Croisement src/motifs.cpp / tools/signaux.py : compile tools/tests/dump_motifs.cpp
avec clang++, puis compare ses segments et ses octets a signaux.trame_motif et
signaux.octets_motif, motif par motif.
python3 -m unittest discover -s tools/tests -p test_motifs.py -v"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.normpath(os.path.join(ICI, "..", ".."))
sys.path.insert(0, os.path.join(ICI, ".."))
import signaux  # noqa: E402

INDICES = list(range(10)) + [255, 256, 999]  # dump_motifs.cpp : kIndices


def compiler_et_lancer() -> dict:
    """Compile dump_motifs.cpp avec src/motifs.cpp (drapeaux de test_hote.sh) et rend son JSON."""
    cxx = shutil.which("clang++")
    if cxx is None:
        raise AssertionError("clang++ introuvable (Xcode Command Line Tools)")
    with tempfile.TemporaryDirectory() as dossier:
        exe = os.path.join(dossier, "dump_motifs")
        r = subprocess.run([cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I" + os.path.join(RACINE, "src"),
                            os.path.join(RACINE, "src", "motifs.cpp"), os.path.join(ICI, "dump_motifs.cpp"),
                            "-o", exe], capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("compilation de dump_motifs.cpp :\n" + r.stderr)
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode:
            raise AssertionError(f"dump_motifs a echoue ({r.returncode}) :\n{r.stderr}")
    return json.loads(r.stdout)


def segs(liste) -> list[tuple[bool, int]]:
    return [(bool(h), int(d)) for h, d in liste]


class Motifs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dump = compiler_et_lancer()
        cls.par_nom = {m["nom"]: m for m in cls.dump["motifs"]}

    def test_noms_dans_l_ordre_de_signaux(self):
        self.assertEqual([m["nom"] for m in self.dump["motifs"]], list(signaux.MOTIFS))
        for m in self.dump["motifs"]:
            self.assertTrue(m["retrouve"], m["nom"])  # depuisNom(nom(id)) == id
        self.assertFalse(self.dump["inconnu"])        # depuisNom("inconnu")

    def test_repos_et_pause(self):
        for nom in signaux.MOTIFS:
            m = self.par_nom[nom]
            self.assertEqual(m["repos_haut"], signaux.repos_haut_motif(nom), nom)
            self.assertEqual(m["pause_us"], signaux.pause_motif_us(nom), nom)

    def test_octets(self):
        for nom in signaux.MOTIFS:
            trames = self.par_nom[nom]["trames"]
            self.assertEqual([t["index"] for t in trames], INDICES)
            for t in trames:
                self.assertEqual(bytes(t["octets"]), signaux.octets_motif(nom, t["index"]), (nom, t["index"]))

    def test_segments(self):
        for nom in signaux.MOTIFS:
            if nom == "rafale":
                continue
            for t in self.par_nom[nom]["trames"]:
                attendu = signaux.trame_motif(nom, t["index"])
                self.assertEqual(segs(t["segments"]), attendu, (nom, t["index"]))
                self.assertEqual(t["n"], len(attendu), (nom, t["index"]))
                self.assertEqual(t["somme_us"], sum(d for _, d in attendu), (nom, t["index"]))

    def test_rafale_longueur_et_debut(self):
        attendu = signaux.trame_motif("rafale", 0)
        for t in self.par_nom["rafale"]["trames"]:  # index ignore
            self.assertEqual(t["n"], len(attendu))
            self.assertEqual(t["n"], self.dump["rafale_segs"])
            self.assertEqual(segs(t["segments"]), attendu[:20])
            self.assertEqual(t["somme_us"], sum(d for _, d in attendu))
            self.assertEqual(t["octets"], [])

    def test_tampon_du_generateur_suffit(self):
        # gen_main.cpp alloue kSegMax segments par trame (rafale exceptee) ;
        # max_segments est le maximum sur les index 0..1023 (tous les restes mod 256).
        for nom in signaux.MOTIFS:
            if nom == "rafale":
                continue
            m = self.par_nom[nom]
            self.assertLessEqual(m["max_segments"], self.dump["seg_max"], nom)
            pire = max(len(signaux.trame_motif(nom, i)) for i in range(1024))
            self.assertEqual(m["max_segments"], pire, nom)

    def test_capacite_trop_petite(self):
        self.assertEqual(self.dump["cap_trop_petite"], {"uart500": 0, "rafale": 0, "octets": 0})


if __name__ == "__main__":
    unittest.main()
```

Créer `tools/tests/dump_motifs.cpp` (programme de test, compilé et lancé par `test_motifs.py`) :

```cpp
// Imprime en JSON les motifs du banc de src/motifs.cpp (noms, repos, pauses,
// octets et segments des trames), pour tools/tests/test_motifs.py qui les
// compare a tools/signaux.py. Rafale : longueur, somme et 20 premiers segments.
// Compile et lance par test_motifs.py (clang++ -std=c++17 -Wall -Wextra -Werror -Isrc).
#include <cstdio>
#include <vector>

#include "motifs.h"

using namespace motif;

static const uint32_t kIndices[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255, 256, 999};

static const char *b(bool v) { return v ? "true" : "false"; }

static void segments(const Seg *s, size_t n) {
  std::printf("[");
  for (size_t i = 0; i < n; i++) std::printf("%s[%s,%u]", i ? "," : "", b(s[i].haut), (unsigned)s[i].us);
  std::printf("]");
}

int main() {
  std::vector<Seg> seg(kRafaleSegs);
  uint8_t oct[16];
  std::printf("{\"motifs\":[");
  for (uint8_t k = 0; k <= (uint8_t)Id::Rafale; k++) {
    const Id id = (Id)k;
    Id retrouve = Id::Rafale;
    const bool ok = depuisNom(nom(id), &retrouve) && retrouve == id;
    std::printf("%s{\"nom\":\"%s\",\"retrouve\":%s,\"repos_haut\":%s,\"pause_us\":%u,\"trames\":[", k ? "," : "",
                nom(id), b(ok), b(reposHaut(id)), (unsigned)pauseUs(id));
    for (size_t j = 0; j < sizeof(kIndices) / sizeof(kIndices[0]); j++) {
      const uint32_t index = kIndices[j];
      const size_t no = octets(id, index, oct, sizeof(oct));
      const size_t ns = trame(id, index, seg.data(), seg.size());
      unsigned long long somme = 0;
      for (size_t i = 0; i < ns; i++) somme += seg[i].us;
      std::printf("%s{\"index\":%u,\"octets\":[", j ? "," : "", (unsigned)index);
      for (size_t i = 0; i < no; i++) std::printf("%s%u", i ? "," : "", (unsigned)oct[i]);
      std::printf("],\"n\":%u,\"somme_us\":%llu,\"segments\":", (unsigned)ns, somme);
      segments(seg.data(), id == Id::Rafale && ns > 20 ? 20 : ns);
      std::printf("}");
    }
    // Pire nombre de segments sur les index 0..1023 (tous les restes modulo 256 et 8).
    size_t pire = 0;
    for (uint32_t index = 0; index < 1024; index++) {
      const size_t ns = trame(id, index, seg.data(), seg.size());
      if (ns > pire) pire = ns;
    }
    std::printf("],\"max_segments\":%u}", (unsigned)pire);
  }
  Id inutile = Id::Rafale;
  std::printf("],\"seg_max\":%u,\"rafale_segs\":%u,\"inconnu\":%s", (unsigned)kSegMax, (unsigned)kRafaleSegs,
              b(depuisNom("inconnu", &inutile)));
  // Capacite trop petite : 0, jamais une trame tronquee.
  std::printf(",\"cap_trop_petite\":{\"uart500\":%u,\"rafale\":%u,\"octets\":%u}}\n",
              (unsigned)trame(Id::Uart500, 0, seg.data(), 3), (unsigned)trame(Id::Rafale, 0, seg.data(), kRafaleSegs - 1),
              (unsigned)octets(Id::Uart500, 0, oct, 5));
  return 0;
}
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_motifs.py -v`

Attendu : ÉCHEC avec « ERROR: setUpClass (test_motifs.Motifs) », puis « AssertionError: compilation de dump_motifs.cpp : » et « clang++: error: no such file or directory: '<racine du dépôt>/src/motifs.cpp' »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `src/motifs.h` :

```cpp
#pragma once
// Motifs du banc de validation (spec section 10) : segments de LIGNE et contenus
// connus, identiques a tools/signaux.py (croisement : tools/tests/test_motifs.py).
// Pur : compile sur l'hote et dans le firmware generateur (src/gen_main.cpp).
#include <stddef.h>
#include <stdint.h>
namespace motif {
enum class Id : uint8_t { Uart500, Uart500Inv, Uart2400, Uart2400Inv, Uart9600, Uart9600Inv, Wtc, Krona, Rafale };
struct Seg { bool haut; uint32_t us; };   // niveau de LIGNE
const char *nom(Id id);                    // "uart500", "uart500inv", ..., "wtc", "krona", "rafale"
bool depuisNom(const char *s, Id *out);
bool reposHaut(Id id);
uint32_t pauseUs(Id id);                   // repos apres chaque trame
// Contenu connu de la trame index (octets, ou bits pour wtc emballes MSB d'abord).
size_t octets(Id id, uint32_t index, uint8_t *out, size_t cap);
// Segments de la trame index (sans le repos final). Rend le nombre de segments, 0 si cap trop petit.
size_t trame(Id id, uint32_t index, Seg *out, size_t cap);

// Ajouts au contrat (tampons du generateur) :
constexpr size_t kSegMax = 128;             // segments d'une trame, rafale exceptee (verifie par test_motifs.py)
constexpr uint32_t kRafaleSegs = 100000;    // rafale : segments alternes, le premier haut
constexpr uint32_t kRafaleUs = 100;         // rafale : duree de chaque segment
}
```

Créer `src/motifs.cpp` :

```cpp
// ===========================================================================
//  Motifs du banc de validation (docs/SPEC-RECONNAISSANCE.md section 10)
//
//  Segments de LIGNE (niveau haut ?, duree en us), sans le repos qui precede
//  ni la pause qui suit ; les segments consecutifs de meme niveau sont fondus.
//  Memes contenus et memes segments que tools/signaux.py :
//   - UART 8N1, LSB d'abord ; frontieres des bits d'un octet a
//     (k * 1000000 + bauds / 2) / bauds us de son debut (k = 0..10) ; le bit
//     de stop du dernier octet fait partie de la trame ;
//   - wtc : trame (depart 2T bas, 16 bits, 1T haut), 4T haut, reponse
//     (depart, 8 bits, 1T haut) : 5T haut entre les deux apres fusion ;
//   - krona : UART 500 bauds inverse, 4 000 us de repos entre deux octets ;
//   - rafale : kRafaleSegs segments alternes de kRafaleUs, le premier haut.
//  Pur : compile sur l'hote (tools/tests/dump_motifs.cpp) et dans le
//  firmware generateur.
// ===========================================================================
#include "motifs.h"

#include <string.h>

namespace motif {
namespace {

struct Def {
  const char *nom;
  bool reposHaut;
  uint32_t pauseUs;  // repos apres la trame
  uint32_t bauds;    // UART (krona compris) ; 0 sinon
};

// Dans l'ordre de Id.
const Def kDefs[] = {
    {"uart500", true, 20000, 500},   {"uart500inv", false, 20000, 500}, {"uart2400", true, 20000, 2400},
    {"uart2400inv", false, 20000, 2400}, {"uart9600", true, 20000, 9600}, {"uart9600inv", false, 20000, 9600},
    {"wtc", true, 6000, 0},          {"krona", false, 18000, 500},      {"rafale", true, 1000000, 0},
};
constexpr uint8_t kNombre = sizeof(kDefs) / sizeof(kDefs[0]);
constexpr uint32_t kTWtcUs = 750;
constexpr uint32_t kPauseOctetKronaUs = 4000;

const Def *def(Id id) { return (uint8_t)id < kNombre ? &kDefs[(uint8_t)id] : nullptr; }

// Ajoute des segments en fondant les niveaux egaux ; un depassement de
// capacite est retenu et fin() rend alors 0.
class Ecrivain {
 public:
  Ecrivain(Seg *out, size_t cap) : out_(out), cap_(cap) {}
  void ajouter(bool haut, uint32_t us) {
    if (!us) return;
    if (n_ && out_[n_ - 1].haut == haut) {
      out_[n_ - 1].us += us;
      return;
    }
    if (n_ == cap_) {
      plein_ = true;
      return;
    }
    out_[n_++] = Seg{haut, us};
  }
  size_t fin() const { return plein_ ? 0 : n_; }

 private:
  Seg *out_;
  size_t cap_;
  size_t n_ = 0;
  bool plein_ = false;
};

void uart(Ecrivain &e, const uint8_t *o, size_t n, uint32_t bauds, bool reposHaut, uint32_t pauseOctetUs) {
  uint32_t bornes[11];
  for (uint32_t k = 0; k < 11; k++) bornes[k] = (k * 1000000u + bauds / 2) / bauds;
  for (size_t i = 0; i < n; i++) {
    if (i) e.ajouter(reposHaut, pauseOctetUs);
    for (uint32_t k = 0; k < 10; k++) {  // depart, 8 bits de donnees, stop
      const bool bit = k == 0 ? false : k == 9 ? true : ((o[i] >> (k - 1)) & 1) != 0;
      e.ajouter(bit == reposHaut, bornes[k + 1] - bornes[k]);  // 1 logique = niveau du repos
    }
  }
}

// Distance d'impulsion (type WTC6534) : depart 2T bas, bits MSB d'abord
// (0 = 1T haut + 1T bas, 1 = 1T haut + 3T bas), fin 1T haut.
void impulsions(Ecrivain &e, uint32_t valeur, uint8_t nBits) {
  e.ajouter(false, 2 * kTWtcUs);
  for (int k = nBits - 1; k >= 0; k--) {
    e.ajouter(true, kTWtcUs);
    e.ajouter(false, ((valeur >> k) & 1) ? 3 * kTWtcUs : kTWtcUs);
  }
  e.ajouter(true, kTWtcUs);
}

uint32_t valeurWtc(uint32_t index) { return (0x0100u << (index % 8)) & 0xFFFFu; }

}  // namespace

const char *nom(Id id) {
  const Def *d = def(id);
  return d ? d->nom : "?";
}

bool depuisNom(const char *s, Id *out) {
  for (uint8_t k = 0; k < kNombre; k++) {
    if (!strcmp(s, kDefs[k].nom)) {
      *out = (Id)k;
      return true;
    }
  }
  return false;
}

bool reposHaut(Id id) {
  const Def *d = def(id);
  return d ? d->reposHaut : true;
}

uint32_t pauseUs(Id id) {
  const Def *d = def(id);
  return d ? d->pauseUs : 0;
}

size_t octets(Id id, uint32_t index, uint8_t *out, size_t cap) {
  uint8_t c[6];
  size_t n = 0;
  switch (id) {
    case Id::Uart500: case Id::Uart500Inv: case Id::Uart2400: case Id::Uart2400Inv:
    case Id::Uart9600: case Id::Uart9600Inv: {
      c[0] = 0xA5;
      c[1] = 0x5A;
      c[2] = 0x00;
      c[3] = 0xFF;
      c[4] = (uint8_t)(index & 0xFF);
      c[5] = (uint8_t)((0xA5 + 0x5A + 0x00 + 0xFF + (index & 0xFF)) & 0xFF);
      n = 6;
      break;
    }
    case Id::Wtc: {
      const uint32_t v = valeurWtc(index);
      c[0] = (uint8_t)(v >> 8);
      c[1] = (uint8_t)(v & 0xFF);
      c[2] = (uint8_t)(index & 0xFF);
      n = 3;
      break;
    }
    case Id::Krona: {
      const uint8_t k = (uint8_t)(index % 8);
      c[0] = 0x55;
      c[1] = k;
      c[2] = (uint8_t)(~k & 0xFF);
      c[3] = 0x00;
      c[4] = (uint8_t)((c[0] + c[1] + c[2] + c[3]) & 0xFF);
      n = 5;
      break;
    }
    case Id::Rafale: return 0;
  }
  if (n > cap) return 0;
  memcpy(out, c, n);
  return n;
}

size_t trame(Id id, uint32_t index, Seg *out, size_t cap) {
  const Def *d = def(id);
  if (!d) return 0;
  Ecrivain e(out, cap);
  if (id == Id::Rafale) {
    if (cap < kRafaleSegs) return 0;
    for (uint32_t k = 0; k < kRafaleSegs; k++) out[k] = Seg{k % 2 == 0, kRafaleUs};
    return kRafaleSegs;
  }
  if (id == Id::Wtc) {
    impulsions(e, valeurWtc(index), 16);
    e.ajouter(true, 4 * kTWtcUs);  // reponse simulee de la carte 4T apres la fin
    impulsions(e, index & 0xFF, 8);
    return e.fin();
  }
  uint8_t o[6];
  const size_t n = octets(id, index, o, sizeof(o));
  uart(e, o, n, d->bauds, d->reposHaut, id == Id::Krona ? kPauseOctetKronaUs : 0);
  return e.fin();
}

}  // namespace motif
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_motifs.py -v`

Attendu : les 7 tests à `ok` (`test_capacite_trop_petite`, `test_noms_dans_l_ordre_de_signaux`, `test_octets`, `test_rafale_longueur_et_debut`, `test_repos_et_pause`, `test_segments`, `test_tampon_du_generateur_suffit`), puis « Ran 7 tests » et « OK ».

Lancer : `python3 -m unittest discover -s tools/tests` puis `sh tools/tests/test_hote.sh | tail -1`

Attendu : « OK » (toute la suite Python) ; « tests hote : OK » (tests hôte inchangés).

Vérification facultative que le croisement détecte un écart : remplacer `kPauseOctetKronaUs = 4000` par `4001` dans `src/motifs.cpp`, relancer `test_motifs.py` : « FAIL: test_segments » (`6001` au lieu de `6000`) ; remettre `4000`.

- [ ] **Étape 5 : Commit**

```bash
git add src/motifs.h src/motifs.cpp tools/tests/dump_motifs.cpp tools/tests/test_motifs.py
git commit -m "Ajouter les motifs du banc en C++, croises avec signaux.py" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 14 : Firmware du générateur (env `generateur`)

Le second C6 imite la ligne D (bus de 5 V à drain ouvert) : il émet les motifs de
`motifs.cpp` sur GPIO7, à travers le NPN du banc (Q3, WIRING §10 : GPIO haut =
ligne basse), par le **pilote RMT d'IDF en émission** (`driver/rmt_tx.h`,
encodeur copie, 1 MHz, donc des durées au tick près). La conversion des
segments de ligne en symboles RMT est un en-tête pur, `src/gen_symboles.h`,
testé sur l'hôte ; `src/gen_main.cpp` n'est vérifié ici que par la compilation
(essai sur carte : tâche 16).

**Comportement du pilote RMT d'émission** (lu dans
`framework-espidf/components/esp_driver_rmt/src/rmt_tx.c`, IDF 5.5, et dans
`rmt_tx.h`, `rmt_encoder.h` et `soc_caps.h` du cœur 3.3.12) :
- `rmt_tx_channel_config_t.flags.init_level` fixe le niveau de la broche à la création du canal ; après chaque transaction, la broche reste à `rmt_transmit_config_t.flags.eot_level` (`rmt_ll_tx_fix_idle_level`) : c'est ainsi que la ligne reste au repos du motif entre deux trames ;
- une durée nulle dans un symbole arrête l'émission : il faut un nombre pair de demi-symboles, chacun de 32 767 ticks au plus ;
- `mem_block_symbols` est un multiple de 48 (`SOC_RMT_MEM_WORDS_PER_CHANNEL`) ; avec 96, le canal TX 0 prend aussi le bloc du canal 1, libre dans le générateur ;
- `loop_count` (boucle matérielle, `SOC_RMT_SUPPORT_TX_LOOP_COUNT`) : les symboles et le symbole de fin doivent tenir dans la mémoire du canal ; au-delà de 1 023 tours (`RMT_LL_MAX_LOOP_COUNT_PER_BATCH`), le pilote relance la boucle depuis son interruption, ce qui laisserait un trou : la rafale fait donc 1 000 tours de 50 symboles, en un seul lot. Le rappel `on_trans_done` est aussi appelé à la fin d'une boucle ;
- sur C6, `rmt_disable` arrête l'émission tout de suite (`SOC_RMT_SUPPORT_ASYNC_STOP`) et recycle la transaction en cours ; le générateur n'a jamais plus d'une transaction en vol, rien d'autre ne repart au `rmt_enable` suivant.

**Fichiers :**
- Modifier : `platformio.ini` (`build_src_filter` de `[env:sonde]`, nouvel `[env:generateur]`)
- Modifier : `tools/tests/test_hote.sh` (deux lignes avant la dernière)
- Créer : `src/gen_symboles.h`
- Créer : `src/gen_main.cpp`
- Tester : `tools/tests/test_generateur.cpp` ; `~/.platformio/penv/bin/pio run -e generateur` ; `~/.platformio/penv/bin/pio run -e sonde`

**Interfaces :**
- Consomme : `motif::Id`, `motif::Seg`, `motif::nom`, `motif::depuisNom`, `motif::reposHaut`, `motif::pauseUs`, `motif::trame`, `motif::kSegMax`, `motif::kRafaleSegs`, `motif::kRafaleUs` (tâche 13) ; `kPinGenerateur` (`src/config.h`, tâche 7) ; `FW_VERSION_FULL`, `FW_ENV` (`src/fw_version.h`) et `src/app_desc.c` (tâche 7) ; `tools/tests/verif.h` (`VERIF`, `bilan`, tâche 8).
- Produit :
  - env `generateur` : `build_src_filter = -<*> +<gen_main.cpp> +<motifs.cpp> +<app_desc.c>` ; env `sonde` : `build_src_filter = +<*> -<gen_main.cpp>` ;
  - `src/gen_symboles.h` (pur, en-tête seul) : `namespace gen { constexpr uint16_t kTicksMax = 32767; constexpr size_t kSymMax = motif::kSegMax; constexpr size_t kMemSymboles = 96; constexpr size_t kRafaleBoucle = 50; constexpr uint32_t kRafaleTours = 1000; constexpr uint32_t kToursParLot = 1023; struct Sym { uint16_t d0; uint8_t l0; uint16_t d1; uint8_t l1; }; uint8_t niveauGpio(bool ligneHaute); size_t symboles(const motif::Seg *s, size_t n, Sym *out, size_t cap); void rafale(Sym *out); }` ;
  - console USB du générateur (115200) : `motif <nom> [n] [pause_ms]` (n : 1 000 par défaut, 1 pour `rafale`, 0 = sans fin ; `pause_ms` 0..60 000, défaut `motif::pauseUs`), `stop` (arrête et relâche la ligne : GPIO7 bas), `etat`, `help`. Lignes produites : `motif <nom> : <n> trames, pause <p> us, repos haut (GPIO7 bas)|bas (GPIO7 haut)` au lancement (`: sans fin` si n = 0), `motif <nom> trame <index>` à chaque trame (index à partir de 0), `motif <nom> fini : <n> trames, ligne au repos ...` à la fin, `stop : ligne relachee (GPIO7 bas)`. Les tâches 16, 21 et 24 s'en servent ;
  - déroulé d'un motif : prise du repos (2 µs au niveau du repos, qui y laissent la ligne), pause, trame 0, pause, trame 1, etc. Chaque trame est une transaction RMT ; la pause est comptée par la tâche `loop` depuis le rappel de fin, avec une attente active sur ses 2 dernières ms (valeur demandée plus quelques dizaines de µs). Après la dernière trame, la ligne reste au repos du motif.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_generateur.cpp` :

```cpp
// Tests hote de src/gen_symboles.h : segments de LIGNE -> symboles RMT du
// generateur (niveaux GPIO inverses, demi-symboles de 32767 ticks au plus,
// nombre pair de demi-symboles), sur des cas simples et sur tous les motifs.
// Lancer : sh tools/tests/test_hote.sh
#include <vector>

#include "gen_symboles.h"
#include "motifs.h"
#include "verif.h"

using motif::Seg;

// Re-deplie les symboles en segments de LIGNE (niveaux egaux fondus) ; false si une duree est nulle.
static bool deplier(const gen::Sym *y, size_t n, std::vector<Seg> *out) {
  out->clear();
  for (size_t i = 0; i < n; i++) {
    const uint16_t d[2] = {y[i].d0, y[i].d1};
    const uint8_t l[2] = {y[i].l0, y[i].l1};
    for (int m = 0; m < 2; m++) {
      if (!d[m] || d[m] > gen::kTicksMax) return false;
      const bool haut = l[m] == 0;  // GPIO bas = ligne haute (NPN bloque)
      if (!out->empty() && out->back().haut == haut) out->back().us += d[m];
      else out->push_back(Seg{haut, d[m]});
    }
  }
  return true;
}

static void testNiveaux() {
  VERIF(gen::niveauGpio(true) == 0);   // ligne haute : GPIO bas, NPN bloque
  VERIF(gen::niveauGpio(false) == 1);  // ligne basse : GPIO haut, NPN sature
}

static void testCasSimples() {
  gen::Sym y[4] = {};
  // Trois segments : nombre impair de demi-symboles, le dernier est coupe en deux.
  const Seg a[] = {{true, 100}, {false, 200}, {true, 301}};
  VERIF(gen::symboles(a, 3, y, 4) == 2);
  VERIF(y[0].d0 == 100 && y[0].l0 == 0 && y[0].d1 == 200 && y[0].l1 == 1);
  VERIF(y[1].d0 == 150 && y[1].l0 == 0 && y[1].d1 == 151 && y[1].l1 == 0);
  // Nombre pair : rien a couper.
  const Seg b[] = {{false, 750}, {true, 2250}};
  VERIF(gen::symboles(b, 2, y, 4) == 1);
  VERIF(y[0].d0 == 750 && y[0].l0 == 1 && y[0].d1 == 2250 && y[0].l1 == 0);
  // Segment plus long que 32767 ticks : decoupe en demi-symboles de meme niveau.
  const Seg c[] = {{false, 70000}};
  VERIF(gen::symboles(c, 1, y, 4) == 2);
  VERIF(y[0].d0 == 32767 && y[0].d1 == 32767 && y[0].l0 == 1 && y[0].l1 == 1);
  VERIF(y[1].d0 == 2233 && y[1].d1 == 2233 && y[1].l0 == 1 && y[1].l1 == 1);
  // Capacite trop petite : 0.
  VERIF(gen::symboles(a, 3, y, 1) == 0);
  VERIF(gen::symboles(c, 1, y, 1) == 0);
  // Dernier demi-symbole impair de 1 us : impossible a couper.
  const Seg d[] = {{true, 1}};
  VERIF(gen::symboles(d, 1, y, 4) == 0);
  // Rien a emettre : 0.
  VERIF(gen::symboles(a, 0, y, 4) == 0);
}

// Tous les motifs (rafale exceptee), index 0..1023 : les symboles redonnent les segments.
static void testTousLesMotifs() {
  Seg s[motif::kSegMax];
  gen::Sym y[gen::kSymMax];
  std::vector<Seg> v;
  bool ok = true;
  size_t pire = 0;
  for (uint8_t k = 0; k < (uint8_t)motif::Id::Rafale; k++) {
    for (uint32_t index = 0; index < 1024; index++) {
      const size_t ns = motif::trame((motif::Id)k, index, s, motif::kSegMax);
      const size_t ny = gen::symboles(s, ns, y, gen::kSymMax);
      if (!ns || !ny || !deplier(y, ny, &v) || v.size() != ns) {
        ok = false;
        continue;
      }
      for (size_t i = 0; i < ns; i++)
        if (v[i].haut != s[i].haut || v[i].us != s[i].us) ok = false;
      if (ny > pire) pire = ny;
    }
  }
  VERIF(ok);
  VERIF(pire > 0 && pire <= gen::kSymMax);
  std::printf("  pire trame : %u symboles (tampon de %u)\n", (unsigned)pire, (unsigned)gen::kSymMax);
}

static void testRafale() {
  // Motif de la boucle du generateur : kRafaleBoucle symboles {haut, bas} de kRafaleUs,
  // rejoues kRafaleTours fois : kRafaleSegs segments en tout, le premier haut.
  VERIF(gen::kRafaleBoucle * 2 * gen::kRafaleTours == motif::kRafaleSegs);
  VERIF(gen::kRafaleBoucle + 1 <= gen::kMemSymboles);  // plus le symbole de fin
  VERIF(gen::kRafaleTours <= gen::kToursParLot);        // un seul lot : pas de relance, pas de trou
  gen::Sym y[gen::kRafaleBoucle];
  gen::rafale(y);
  bool ok = true;
  for (size_t i = 0; i < gen::kRafaleBoucle; i++)
    if (y[i].d0 != motif::kRafaleUs || y[i].l0 != 0 || y[i].d1 != motif::kRafaleUs || y[i].l1 != 1) ok = false;
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

Dans `tools/tests/test_hote.sh`, insérer avant la dernière ligne, `echo "tests hote : OK"` :

```sh
$CXX src/motifs.cpp tools/tests/test_generateur.cpp -o "$OUT/test_generateur"
"$OUT/test_generateur"
```

Dans `platformio.ini`, la section `[env:sonde]` est vide depuis la tâche 7 et termine le fichier :

```ini
; ---------------------------------------------------------------------------
; Sonde : ecoute de la ligne D sur GPIO6, injection sur GPIO7 (src/config.h).
; ---------------------------------------------------------------------------
[env:sonde]
```

Remplacer sa dernière ligne, `[env:sonde]`, par :

```ini
[env:sonde]
; gen_main.cpp est le firmware du generateur (env generateur) : pas dans la sonde.
build_src_filter = +<*> -<gen_main.cpp>

; ---------------------------------------------------------------------------
; Generateur du banc (spec section 10) : second C6 qui imite la ligne D avec
; des motifs a contenu connu, sur GPIO7 (src/config.h : kPinGenerateur), par
; le RMT en emission. Le NPN du banc inverse : GPIO haut = ligne basse.
; Sources : gen_main.cpp, motifs.cpp (partage avec les tests hote) et
; app_desc.c (version, comme la sonde).
; ---------------------------------------------------------------------------
[env:generateur]
build_src_filter = -<*> +<gen_main.cpp> +<motifs.cpp> +<app_desc.c>
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`

Attendu : ÉCHEC avec « tools/tests/test_generateur.cpp:7:10: fatal error: 'gen_symboles.h' file not found »

Lancer : `~/.platformio/penv/bin/pio run -e generateur 2>&1 | grep -E "undefined|FAILED"`

Attendu : ÉCHEC avec « undefined reference to `setup()' », « undefined reference to `loop()' » et « [FAILED] »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `src/gen_symboles.h` :

```cpp
#pragma once
// ===========================================================================
//  Generateur du banc : segments de LIGNE -> symboles RMT. Pur, en-tete seul
//  (inclus par gen_main.cpp et par tools/tests/test_generateur.cpp).
//
//  Le NPN du banc inverse : GPIO haut = ligne basse. Un symbole RMT porte deux
//  demi-symboles (duree en ticks sur 15 bits, niveau GPIO). Une duree nulle
//  arrete l'emission : le nombre de demi-symboles doit donc etre pair ; s'il
//  est impair, le dernier est coupe en deux moities de meme niveau. Un segment
//  de plus de kTicksMax ticks est decoupe. Canal a 1 MHz : 1 tick = 1 us.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "motifs.h"

namespace gen {

constexpr uint16_t kTicksMax = 32767;       // duree d'un demi-symbole (15 bits)
constexpr size_t kSymMax = motif::kSegMax;  // symboles d'une trame, rafale exceptee
// Memoire du canal TX : deux blocs de 48 mots (SOC_RMT_MEM_WORDS_PER_CHANNEL),
// le second emprunte au canal 1, libre dans le generateur.
constexpr size_t kMemSymboles = 96;
// Rafale : kRafaleBoucle symboles {haut, bas} rejoues kRafaleTours fois par la
// boucle materielle du RMT. La boucle et son symbole de fin tiennent dans la
// memoire du canal, et kRafaleTours ne depasse pas le lot maximal du C6
// (RMT_LL_MAX_LOOP_COUNT_PER_BATCH = 1023) : une seule passe, sans relance
// par l'interruption du pilote, donc sans trou.
constexpr size_t kRafaleBoucle = 50;
constexpr uint32_t kRafaleTours = motif::kRafaleSegs / (2 * kRafaleBoucle);
constexpr uint32_t kToursParLot = 1023;
static_assert(kRafaleTours * 2 * kRafaleBoucle == motif::kRafaleSegs, "rafale : nombre de segments");
static_assert(kRafaleBoucle + 1 <= kMemSymboles && kRafaleTours <= kToursParLot, "rafale : boucle materielle");

// Miroir de rmt_symbol_word_t, niveaux GPIO.
struct Sym {
  uint16_t d0;
  uint8_t l0;
  uint16_t d1;
  uint8_t l1;
};

inline uint8_t niveauGpio(bool ligneHaute) { return ligneHaute ? 0 : 1; }

// Symboles des segments s[0..n) ; rend leur nombre, 0 si n == 0, si cap est
// trop petit ou si le dernier demi-symbole, impair, dure 1 tick.
inline size_t symboles(const motif::Seg *s, size_t n, Sym *out, size_t cap) {
  size_t k = 0;  // demi-symboles poses
  for (size_t i = 0; i < n; i++) {
    const uint8_t niv = niveauGpio(s[i].haut);
    uint32_t reste = s[i].us;
    while (reste) {
      const uint16_t d = reste > kTicksMax ? kTicksMax : (uint16_t)reste;
      if (k / 2 >= cap) return 0;
      Sym &y = out[k / 2];
      if (k % 2 == 0) {
        y = Sym{d, niv, 0, niv};
      } else {
        y.d1 = d;
        y.l1 = niv;
      }
      k++;
      reste -= d;
    }
  }
  if (!k) return 0;
  if (k % 2) {
    Sym &y = out[k / 2];
    if (y.d0 < 2) return 0;
    y.d1 = (uint16_t)(y.d0 - y.d0 / 2);
    y.d0 = (uint16_t)(y.d0 / 2);
    y.l1 = y.l0;
    k++;
  }
  return k / 2;
}

// Les kRafaleBoucle symboles de la boucle de la rafale : ligne haute puis basse, kRafaleUs chacune.
inline void rafale(Sym *out) {
  for (size_t i = 0; i < kRafaleBoucle; i++)
    out[i] = Sym{(uint16_t)motif::kRafaleUs, niveauGpio(true), (uint16_t)motif::kRafaleUs, niveauGpio(false)};
}

}  // namespace gen
```

Créer `src/gen_main.cpp` :

```cpp
// ===========================================================================
//  Generateur du banc de validation (docs/SPEC-RECONNAISSANCE.md section 10)
//
//  Second C6 (env generateur) : imite la ligne D, un bus de 5 V a drain
//  ouvert, avec les motifs a contenu connu de motifs.cpp. Sortie sur GPIO7
//  (kPinGenerateur) par le pilote RMT d'IDF en emission (driver/rmt_tx.h,
//  encodeur copie, 1 MHz). Montage : docs/WIRING.md section 10 (GPIO7 ->
//  4,7k -> base du NPN, 10k base-emetteur, collecteur sur la ligne, pull-up
//  vers le 5V). Logique inversee : GPIO haut = ligne basse, GPIO bas = ligne
//  relachee (haute).
//
//  Console USB (115200) :
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
//  trame est UNE transaction RMT (durees au tick pres). Pendant la pause, la
//  ligne reste au repos ; la tache loop la compte depuis la fin de la
//  transaction (rappel on_trans_done) et attend activement ses 2 dernieres
//  ms : la pause vaut la valeur demandee plus quelques dizaines de us. Apres
//  la derniere trame, la ligne reste au repos du motif ; 'stop' la relache.
//
//  Rafale : 100 000 segments de 100 us (10 s) en une transaction en boucle
//  materielle (gen::kRafaleBoucle symboles rejoues gen::kRafaleTours fois,
//  en un seul lot : aucune relance par l'interruption, donc aucun trou).
// ===========================================================================
#include <Arduino.h>
#include <driver/rmt_tx.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include <string.h>

#include "config.h"
#include "fw_version.h"
#include "gen_symboles.h"
#include "motifs.h"

namespace {

constexpr uint32_t kNDefaut = 1000;       // critere 1 du banc : 1 000 trames par motif
constexpr uint32_t kPauseMaxMs = 60000;
constexpr uint32_t kAttenteActiveUs = 2000;
constexpr size_t kLigneMax = 80;

rmt_channel_handle_t sCanal = nullptr;
rmt_encoder_handle_t sCopie = nullptr;
// Tampons lus par le pilote pendant l'emission : jamais modifies avant la fin.
rmt_symbol_word_t sSym[gen::kSymMax];
rmt_symbol_word_t sRafale[gen::kRafaleBoucle];
rmt_symbol_word_t sBref[1];

// Ecrits par le rappel (interruption), lus par la tache loop.
volatile bool sOccupe = false;   // une transaction est en cours
volatile uint32_t sFinUs = 0;    // fin de la derniere transaction (esp_timer, 32 bits bas)

enum class Etape : uint8_t { Arret, Emission, Pause, Fini };
struct Course {
  motif::Id id = motif::Id::Uart500;
  uint32_t n = 0;         // trames demandees (0 : sans fin)
  uint32_t pauseUs = 0;
  uint32_t index = 0;     // prochaine trame
  Etape etape = Etape::Arret;
  bool reposHaut = true;  // niveau de la ligne entre deux trames
};
Course sC;
motif::Seg sSeg[motif::kSegMax];
gen::Sym sY[gen::kSymMax];
char sLigne[kLigneMax + 1];
size_t sLong = 0;

bool IRAM_ATTR finEmission(rmt_channel_handle_t, const rmt_tx_done_event_data_t *, void *) {
  sFinUs = (uint32_t)esp_timer_get_time();
  sOccupe = false;
  return false;
}

rmt_symbol_word_t mot(const gen::Sym &y) {
  rmt_symbol_word_t w;
  w.duration0 = y.d0;
  w.level0 = y.l0;
  w.duration1 = y.d1;
  w.level1 = y.l1;
  return w;
}

const char *texteRepos(bool haut) { return haut ? "haut (GPIO7 bas)" : "bas (GPIO7 haut)"; }

bool canalOuvrir() {
  rmt_tx_channel_config_t c = {};
  c.gpio_num = (gpio_num_t)kPinGenerateur;
  c.clk_src = RMT_CLK_SRC_DEFAULT;
  c.resolution_hz = 1000000;
  c.mem_block_symbols = gen::kMemSymboles;  // deux blocs : la boucle de la rafale et sa fin y tiennent
  c.trans_queue_depth = 2;   // une seule transaction en vol a la fois
  c.flags.init_level = 0;    // ligne relachee
  esp_err_t err = rmt_new_tx_channel(&c, &sCanal);
  if (err == ESP_OK) {
    rmt_tx_event_callbacks_t cb = {};
    cb.on_trans_done = finEmission;
    err = rmt_tx_register_event_callbacks(sCanal, &cb, nullptr);
  }
  if (err == ESP_OK) {
    rmt_copy_encoder_config_t e = {};
    err = rmt_new_copy_encoder(&e, &sCopie);
  }
  if (err == ESP_OK) err = rmt_enable(sCanal);
  if (err != ESP_OK) {
    Serial.printf("[rmt] %s : generateur inutilisable (GPIO7 reste bas)\n", esp_err_to_name(err));
    sCanal = nullptr;
    return false;
  }
  return true;
}

bool envoyer(const rmt_symbol_word_t *s, size_t n, bool ligneHauteApres, int boucles) {
  rmt_transmit_config_t t = {};
  t.loop_count = boucles;
  t.flags.eot_level = gen::niveauGpio(ligneHauteApres);
  sOccupe = true;
  const esp_err_t err = rmt_transmit(sCanal, sCopie, s, n * sizeof(rmt_symbol_word_t), &t);
  if (err != ESP_OK) {
    sOccupe = false;
    Serial.printf("[rmt] rmt_transmit : %s\n", esp_err_to_name(err));
    return false;
  }
  return true;
}

// Deux us au niveau voulu, qui y laissent la ligne (eot_level).
bool poserNiveau(bool ligneHaute) {
  const uint8_t g = gen::niveauGpio(ligneHaute);
  sBref[0] = mot(gen::Sym{1, g, 1, g});
  return envoyer(sBref, 1, ligneHaute, 0);
}

// Interrompt la transaction en cours (arret asynchrone du C6) ; la ligne reste au dernier niveau de repos.
void interrompre() {
  rmt_disable(sCanal);
  sOccupe = false;
  rmt_enable(sCanal);
}

void arreter() {
  interrompre();
  if (poserNiveau(true)) rmt_tx_wait_all_done(sCanal, 100);
  sC.etape = Etape::Arret;
}

void demarrer(motif::Id id, uint32_t n, uint32_t pauseUs) {
  if (sC.etape == Etape::Emission || sC.etape == Etape::Pause) interrompre();
  sC = Course{};
  sC.id = id;
  sC.n = n;
  sC.pauseUs = pauseUs;
  sC.reposHaut = motif::reposHaut(id);
  if (!poserNiveau(sC.reposHaut)) return;  // etape Arret
  sC.etape = Etape::Emission;              // prise du repos, puis pause, puis trame 0
  if (n) Serial.printf("motif %s : %lu trames", motif::nom(id), (unsigned long)n);
  else Serial.printf("motif %s : sans fin", motif::nom(id));
  Serial.printf(", pause %lu us, repos %s\n", (unsigned long)pauseUs, texteRepos(sC.reposHaut));
}

void avancer() {
  if (sC.etape == Etape::Emission && !sOccupe) sC.etape = Etape::Pause;
  if (sC.etape != Etape::Pause) return;
  const uint32_t ecoule = (uint32_t)esp_timer_get_time() - sFinUs;
  if (ecoule < sC.pauseUs) {
    const uint32_t reste = sC.pauseUs - ecoule;
    if (reste > kAttenteActiveUs) return;  // tour suivant de loop (tic de 1 ms)
    delayMicroseconds(reste);
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
    const size_t ny = gen::symboles(sSeg, ns, sY, gen::kSymMax);
    for (size_t i = 0; i < ny; i++) sSym[i] = mot(sY[i]);
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

// Entier decimal strict : chiffres seulement, 4294967295 au plus.
bool lireU32(const char *s, uint32_t *v) {
  if (!*s) return false;
  uint64_t x = 0;
  for (const char *p = s; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    x = x * 10 + (uint64_t)(*p - '0');
    if (x > 0xFFFFFFFFull) return false;
  }
  *v = (uint32_t)x;
  return true;
}

void aide() {
  Serial.println("=== Generateur du banc ===");
  Serial.println("  motif <nom> [n] [pause_ms]  n trames (defaut 1000, rafale 1 ; 0 : sans fin), pause apres chaque trame");
  Serial.println("  stop                        arrete et relache la ligne (GPIO7 bas)");
  Serial.println("  etat                        motif, trame, pause, niveau de la ligne");
  Serial.println("motifs (repos, pause par defaut) :");
  for (uint8_t k = 0; k <= (uint8_t)motif::Id::Rafale; k++) {
    const motif::Id id = (motif::Id)k;
    Serial.printf("  %-12s repos %-4s %7lu us\n", motif::nom(id), motif::reposHaut(id) ? "haut" : "bas",
                  (unsigned long)motif::pauseUs(id));
  }
}

void etat() {
  if (!sCanal) {
    Serial.println("etat : RMT indisponible, GPIO7 bas");
    return;
  }
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
  Serial.printf(" (%s), pause %lu us, repos %s\n", sC.etape == Etape::Emission ? "emission" : "pause",
                (unsigned long)sC.pauseUs, texteRepos(sC.reposHaut));
}

void usageMotif() { Serial.println("usage : motif <nom> [n] [pause_ms 0..60000] ('help' : noms des motifs)"); }

void executer(char *ligne) {
  char *mots[5];
  int n = 0;
  char *reste = nullptr;
  for (char *m = strtok_r(ligne, " ", &reste); m; m = strtok_r(nullptr, " ", &reste)) {
    if (n == 5) {
      Serial.println("trop d'arguments");
      return;
    }
    mots[n++] = m;
  }
  if (!n) return;
  if (!strcmp(mots[0], "help")) {
    aide();
  } else if (!strcmp(mots[0], "etat")) {
    etat();
  } else if (!sCanal) {
    Serial.println("RMT indisponible : rien a faire (voir le message de demarrage)");
  } else if (!strcmp(mots[0], "stop")) {
    arreter();
    Serial.println("stop : ligne relachee (GPIO7 bas)");
  } else if (!strcmp(mots[0], "motif")) {
    motif::Id id;
    if (n < 2 || n > 4 || !motif::depuisNom(mots[1], &id)) return usageMotif();
    uint32_t nb = id == motif::Id::Rafale ? 1 : kNDefaut;
    uint32_t pauseUs = motif::pauseUs(id);
    uint32_t pauseMs = 0;
    if (n >= 3 && !lireU32(mots[2], &nb)) return usageMotif();
    if (n == 4) {
      if (!lireU32(mots[3], &pauseMs) || pauseMs > kPauseMaxMs) return usageMotif();
      pauseUs = pauseMs * 1000;
    }
    demarrer(id, nb, pauseUs);
  } else {
    Serial.printf("commande inconnue : %s ('help')\n", mots[0]);
  }
}

// Echo, effacement, une commande par ligne (CR ou LF ; les lignes vides sont ignorees).
void lireConsole() {
  while (Serial.available()) {
    const int c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (!sLong) continue;
      Serial.println();
      sLigne[sLong] = 0;
      sLong = 0;
      executer(sLigne);
      Serial.print("> ");
    } else if (c == 8 || c == 127) {
      if (sLong) {
        sLong--;
        Serial.print("\b \b");
      }
    } else if (c >= 32 && c < 127 && sLong < kLigneMax) {
      sLigne[sLong++] = (char)c;
      Serial.print((char)c);
    }
  }
}

}  // namespace

void setup() {
  // TOUJOURS la premiere instruction : GPIO7 bas, NPN bloque, ligne relachee.
  pinMode(kPinGenerateur, OUTPUT);
  digitalWrite(kPinGenerateur, LOW);
  Serial.begin(115200);
  Serial.printf("generateur %s (%s) : GPIO%u -> NPN -> ligne (GPIO haut = ligne basse)\n", FW_VERSION_FULL, FW_ENV,
                (unsigned)kPinGenerateur);
  gen::Sym y[gen::kRafaleBoucle];
  gen::rafale(y);
  for (size_t i = 0; i < gen::kRafaleBoucle; i++) sRafale[i] = mot(y[i]);
  canalOuvrir();
  aide();
  Serial.print("> ");
}

void loop() {
  lireConsole();
  if (sCanal) avancer();
  vTaskDelay(1);  // laisse tourner la tache IDLE
}
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh | tail -3`

Attendu :

```text
  pire trame : 26 symboles (tampon de 128)
test_generateur : 20 verifications, 0 echecs
tests hote : OK
```

Lancer : `~/.platformio/penv/bin/pio run -e generateur 2>&1 | grep -E "warning|RAM:|Flash:|SUCCESS|FAILED"`

Attendu (aucun `warning`) :

```text
RAM:   [=         ]   5.6% (used 18208 bytes from 327680 bytes)
Flash: [==        ]  21.4% (used 2800xx bytes from 1310720 bytes)
========================= [SUCCESS] Took ... seconds =========================
```

Lancer : `~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-nm -C .pio/build/generateur/firmware.elf | grep finEmission`

Attendu : un symbole en IRAM (adresse en `0x408…`), par exemple « `408002dc t (anonymous namespace)::finEmission(rmt_channel_t*, rmt_tx_done_event_data_t const*, void*)` »

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|SUCCESS|FAILED"`

Attendu : « [SUCCESS] », aucun `warning` (la sonde ne compile pas `gen_main.cpp` ; `motifs.cpp`, compilé mais inutilisé, disparaît à l'édition des liens).

- [ ] **Étape 5 : Commit**

```bash
git add platformio.ini tools/tests/test_hote.sh src/gen_symboles.h src/gen_main.cpp tools/tests/test_generateur.cpp
git commit -m "Ajouter le firmware du generateur du banc (RMT en emission sur GPIO7)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 15 : Verdicts du banc (`tools/banc.py`) et procédure (`docs/BANC.md`)

`banc.py` juge une capture de la sonde contre le motif que le générateur
émettait. **Critère 1** : les trames sont décodées par `decodeurs.auto` sans
forcer le décodeur ; leurs contenus sont mis bout à bout et on y reconnaît les
trames du motif (index retrouvé par le contenu, modulo 256, ou 8 pour `krona`,
et déroulé dans l'ordre), ce qui recolle les trames `krona` que la sonde coupe
entre deux octets. **Critère 2** : chaque trame de la capture qui porte seule
une trame reconnue est comparée palier par palier à `signaux.trame_motif`,
après correction de l'asymétrie `--decalage-us` (hauts − N, bas + N).
**Critère 3** (rafale) : complétude, `debord`, réceptions incomplètes et hausse
des compteurs de la sonde. `docs/BANC.md` porte la procédure du §10 de la spec,
les réglages par motif (vérifiés par un test contre `signaux.py`) et les
tableaux de résultats vides. Son §2 fixe le montage **sans aucune liaison avec
la hotte** (la règle de SECURITE.md et de WIRING §10) : la sonde sur le boîtier
de mesure détaché (hotte débranchée toute la séance, fiche XH 4 broches du
câble de sortie retirée, `TP−` vers la terre de la fiche de la hotte et vers la
vis de la carcasse à OL avant tout USB), ou sinon sur une plaque d'essai du
banc avec son propre étage d'écoute. Il décrit aussi, pour les tâches à venir,
les voies de l'analyseur (critère 2 à l'analyseur, tâche 16b) et les deux
façons de mesurer le courant de la sonde (critère 6, tâches 16 et 21) : une
tâche **[BANC avec Majid]** n'écrit que des résultats.

`banc.py` traite `krona` sur le flux d'octets : pour `index % 8` égal à 0 ou 1,
la sonde coupe la trame entre deux octets (22 ou 20 ms de repos dans la trame,
tâche 2), et la trame est reconnue à cheval sur deux réceptions ; elle n'entre
simplement pas dans la comparaison des durées. Le seuil `seuils 1 19000` de
`uart500`, `uart500inv` et `krona` (octet 0x00 : 18 ms hors repos) est imposé
par le tableau du §4 et vérifié par `test_reglages_par_motif`.

**Fichiers :**
- Créer : `tools/banc.py`
- Créer : `docs/BANC.md`
- Tester : `tools/tests/test_banc.py`

**Interfaces :**
- Consomme : `analyse.charger(chemin, repos, silence_us) -> (trames datées, repos_haut, receptions)`, `analyse.libelle`, `analyse.resume` (tâche 6) ; `capture_fmt.lire` (tâche 3) ; `decodeurs.auto`, `decodeurs.appliquer`, `decodeurs.Decodage` (tâches 4 et 5) ; `signaux.MOTIFS`, `signaux.octets_motif`, `signaux.trame_motif`, `signaux.pause_motif_us`, `signaux.repos_haut_motif`, `signaux.fusionner`, `signaux.simuler_sonde` (tâche 2) ; format `.jsonl` (contrat) ; message `compteurs` bloc `sonde` (`debord`, `lignes_perdues`, `sautes` : tâche 11) ; `tools/serie_enregistre.py <port> <scénario> [commande ...] --duree s` (tâche 12, cité par `BANC.md`) ; console du générateur (tâche 14).
- Produit :
  - `python3 tools/banc.py <capture> <motif> [--decalage-us N] [--n N] [--silence us] [--tol-us T]` (contrat, plus trois options) ; code de sortie 0 si les critères sont remplis, 1 sinon ou sans trame, 2 si la capture est illisible ou les arguments faux ;
  - `banc.evaluer(chemin, nom: str, n: int | None = None, decalage_us: float = 0.0, silence_us: int | None = None, tol_us: float = 2.0) -> banc.Verdict` ; `Verdict` : `reconnues` (index reconnus dans 0..n−1), `manquantes`, `taux`, `hypothese`, `hypothese_ok`, `comparees`, `durees`, `formes_differentes`, `ecart_moy_haut`, `ecart_max_haut`, `ecart_moy_bas`, `ecart_max_bas`, `ecart_max`, `decalage_suggere`, `compteurs` (hausses de `debord`, `lignes_perdues`, `sautes`, ou `None`), `pertes` (`"aucune"`, `"signalee"`, `"non signalee"`), `ok1`, `ok2`, `ok3`, `ok` ; `banc.n_defaut(nom) -> int` (1 000, rafale 1) ; `banc.reconnaitre(unites, nom) -> list[tuple[int, int | None]]` ;
  - lignes de sortie citées par la tâche 16 : `critere 1 : <k> / <n> trames decodees au bit pres (<x.x> %) : OK|ECHEC`, `  manquantes : ...`, `decodeur (auto, sans forcer) : ... ; attendu : ... : OK|INATTENDU`, `  hautes : ecart moyen ... ; basses : ... ; decalage suggere <+x.x> us`, `critere 2 : ecart max <e> us <= <t> us : OK|ECHEC`, `critere 3 : <k> / <n> rafales completes ; ...`, `compteurs de la sonde pendant la capture : debord +a, lignes_perdues +b, sautes +c`, `verdict : OK|ECHEC` ;
  - `docs/BANC.md`, sections `## 1. Matériel` à `## 7. Résultats` ; le §2 en quatre parties, reprises par les tâches 16, 16b, 21 et 24 : `### 2.1 Où monter la sonde` (boîtier détaché et ses deux contrôles OL, ou plaque d'essai), `### 2.2 Ligne et générateur`, `### 2.3 Analyseur FX2 (critère 2 à l'analyseur)`, `### 2.4 Courant de la sonde (critère 6)` ; le tableau du §4 (une ligne par motif : nom, `seuils 1 <silence>`, `--duree`, `motif <nom>`, durée d'émission) est lu par `test_banc.py` : garder ce format de ligne ; au §7, les tableaux vides, dont `### Critère 2 à l'analyseur (tâche 16b)` et sa ligne `tol_us` ; les tâches 16, 16b, 21 et 24 remplissent le §7 ;
  - lignes de `docs/BANC.md` prises pour ancres par la tâche 23b (ne pas les retoucher ici) : la fin du §1 (de `- pull-up de ligne` à `## 2. Montage`), les lignes `Console du générateur` et `Enregistrement de la sonde` du §3, les lignes 2 à 6 du tableau du §6 suivies de `## 7. Résultats`, le tableau « Critère 5 » du §7 ; la tâche 21 ajoute son §8 après la dernière ligne, `- Limites constatées :`.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_banc.py` :

```python
#!/usr/bin/env python3
"""Tests de tools/banc.py sur des captures synthetiques (signaux.simuler_sonde),
dont une capture volontairement abimee.
python3 -m unittest discover -s tools/tests -p test_banc.py -v"""
import contextlib
import io
import json
import os
import re
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import banc  # noqa: E402
import signaux  # noqa: E402


def objets_motif(nom, indices, silence_us, t0_us=1000000):
    """Objets 'trame' de la sonde pour les trames indices du motif, chacune suivie de sa pause."""
    rh = signaux.repos_haut_motif(nom)
    segs = []
    for i in indices:
        segs += signaux.trame_motif(nom, i) + [(rh, signaux.pause_motif_us(nom))]
    return signaux.simuler_sonde(segs, t0_us, silence_us, rh)


def compteurs(n, **champs):
    c = {"v": 1, "t": "compteurs", "n": n, "ms": 0, "bloc": "sonde", "receptions": 0, "parties": 0, "blocs": 0,
         "symboles": 0, "debord": 0, "rep": 0, "lignes_perdues": 0, "sautes": 0, "rejets": 0}
    c.update(champs)
    return c


def niveaux(o):
    """Niveau (haut ?) de chaque duree d'un objet trame."""
    h = o["niv0"] == "haut"
    return [h if k % 2 == 0 else not h for k in range(len(o["dur_us"]))]


class Capture(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def jsonl(self, nom, objets, debut=None, fin=None):
        """Capture .jsonl : hello, compteurs de debut, objets, compteurs de fin."""
        tete = [{"v": 1, "t": "hello", "n": 0, "ms": 0, "bloc": "base"}] + ([debut] if debut else [])
        p = os.path.join(self.dossier.name, nom + ".jsonl")
        with open(p, "w") as f:
            for k, o in enumerate(tete + objets + ([fin] if fin else [])):
                f.write(json.dumps({"rx_ms": 1790000000000 + k, "de": "usb", "l": o}) + "\n")
        return p

    def lancer(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            code = banc.main(list(argv))
        return code, out.getvalue()


class Contenus(Capture):
    def test_uart500_parfait(self):
        p = self.jsonl("uart500", objets_motif("uart500", range(20), 19000))
        v = banc.evaluer(p, "uart500", n=20)
        self.assertEqual((v.hypothese.nom, v.hypothese.params, v.hypothese_ok), ("uart", {"bauds": 500, "inverse": False}, True))
        self.assertEqual(v.reconnues, list(range(20)))
        self.assertEqual((v.taux, v.comparees, v.formes_differentes, v.ecart_max), (1.0, 20, 0, 0))
        self.assertTrue(v.ok1 and v.ok2 and v.ok)
        code, sortie = self.lancer(p, "uart500", "--n", "20")
        self.assertEqual(code, 0)
        self.assertIn("critere 1 : 20 / 20 trames decodees au bit pres (100.0 %) : OK", sortie)
        self.assertIn("critere 2 : ecart max 0 us <= 2 us : OK", sortie)

    def test_uart9600inv_index_au_dela_de_255(self):
        p = self.jsonl("uart9600inv", objets_motif("uart9600inv", range(300), 5000))
        v = banc.evaluer(p, "uart9600inv", n=300)
        self.assertEqual(v.hypothese.params, {"bauds": 9600, "inverse": True})
        self.assertEqual(v.reconnues, list(range(300)))
        self.assertTrue(v.ok)

    def test_wtc_trame_et_reponse(self):
        p = self.jsonl("wtc", objets_motif("wtc", range(12), 5000))
        v = banc.evaluer(p, "wtc", n=12)
        self.assertEqual((v.hypothese.nom, v.hypothese_ok), ("distance_impulsion", True))
        self.assertEqual(v.reconnues, list(range(12)))
        self.assertEqual((v.comparees, v.ecart_max), (12, 0))
        self.assertTrue(v.ok)

    def test_krona_trames_coupees_par_la_sonde(self):
        # index % 8 = 0 ou 1 : 20 ms de repos ou plus dans la trame, la sonde la coupe en deux
        p = self.jsonl("krona", objets_motif("krona", range(16), 19000))
        v = banc.evaluer(p, "krona", n=16)
        self.assertEqual(v.receptions, 20)
        self.assertEqual(v.reconnues, list(range(16)))
        self.assertEqual(v.comparees, 12)  # les 4 trames coupees ne sont pas comparees en durees
        self.assertTrue(v.ok)

    def test_capture_serie_brute(self):
        p = os.path.join(self.dossier.name, "uart2400.log")
        with open(p, "wb") as f:
            f.write(b"firmware 0.1.0-abc1234 (sonde)\r\n")
            for o in objets_motif("uart2400", range(10), 5000):
                f.write(b"\x1e" + json.dumps(o, separators=(",", ":")).encode() + b"\n")
        v = banc.evaluer(p, "uart2400", n=10)
        self.assertEqual(v.reconnues, list(range(10)))
        self.assertTrue(v.ok)

    def test_mauvais_motif(self):
        p = self.jsonl("uart9600", objets_motif("uart9600", range(10), 5000))
        v = banc.evaluer(p, "wtc", n=10)
        self.assertFalse(v.hypothese_ok)
        self.assertEqual(v.reconnues, [])
        self.assertFalse(v.ok)


class Abimee(Capture):
    def test_capture_abimee(self):
        objs = objets_motif("uart2400", range(50), 5000)
        # index 10 perdu (reception num 11) ; index 20 : un palier allonge de 300 us (decodage faux) ;
        # index 30 : un palier allonge de 3 us (decodage juste, ecart de duree hors tolerance).
        objs = [o for o in objs if o["num"] != 11]
        vingt = next(o for o in objs if o["num"] == 21)
        vingt["dur_us"][3] += 300
        trente = next(o for o in objs if o["num"] == 31)
        trente["dur_us"][5] += 3
        p = self.jsonl("abimee", objs)
        v = banc.evaluer(p, "uart2400", n=50)
        self.assertEqual(v.manquantes, [10, 20])
        self.assertEqual(len(v.reconnues), 48)
        self.assertEqual(v.ecart_max, 3)
        self.assertFalse(v.ok1)
        self.assertFalse(v.ok2)
        code, sortie = self.lancer(p, "uart2400", "--n", "50")
        self.assertEqual(code, 1)
        self.assertIn("critere 1 : 48 / 50 trames decodees au bit pres (96.0 %) : ECHEC", sortie)
        self.assertIn("manquantes : 10, 20", sortie)
        self.assertIn("critere 2 : ecart max 3 us <= 2 us : ECHEC", sortie)

    def test_decalage_corrige_l_asymetrie(self):
        objs = objets_motif("uart9600", range(20), 5000)
        for o in objs:  # etage asymetrique : hauts +3 us, bas -3 us
            o["dur_us"] = [d + 3 if h else d - 3 for d, h in zip(o["dur_us"], niveaux(o))]
        p = self.jsonl("asym", objs)
        v = banc.evaluer(p, "uart9600", n=20)
        self.assertEqual(v.reconnues, list(range(20)))
        self.assertEqual((v.ecart_moy_haut, v.ecart_moy_bas, v.ecart_max, v.decalage_suggere), (3, -3, 3, 3))
        self.assertFalse(v.ok2)
        v = banc.evaluer(p, "uart9600", n=20, decalage_us=3)
        self.assertEqual((v.ecart_max, v.decalage_suggere), (0, 3))
        self.assertTrue(v.ok)
        code, sortie = self.lancer(p, "uart9600", "--n", "20", "--decalage-us", "3")
        self.assertEqual(code, 0)
        self.assertIn("decalage suggere +3.0 us", sortie)


class Rafale(Capture):
    def test_rafale_complete(self):
        objs = objets_motif("rafale", [0], 5000)
        self.assertEqual(len(objs), 910)  # 99 999 durees, 110 par partie
        p = self.jsonl("rafale", objs, compteurs(1, debord=4), compteurs(2, debord=4))
        v = banc.evaluer(p, "rafale")
        self.assertEqual((v.n, v.reconnues, v.durees, v.ecart_max), (1, [0], 99999, 0))
        self.assertEqual(v.pertes, "aucune")
        self.assertEqual(v.compteurs, {"debord": 0, "lignes_perdues": 0, "sautes": 0})
        self.assertTrue(v.ok3 and v.ok)
        code, sortie = self.lancer(p, "rafale")
        self.assertEqual(code, 0)
        self.assertIn("critere 3 : 1 / 1 rafales completes ; aucune perte : OK (10000 fronts/s tenus)", sortie)

    def test_rafale_perte_signalee_par_debord(self):
        objs = [o for o in objets_motif("rafale", [0], 5000) if o["part"] != 500]
        next(o for o in objs if o["part"] == 501)["debord"] = True
        v = banc.evaluer(self.jsonl("rafale", objs), "rafale")
        self.assertEqual((v.reconnues, v.incompletes, v.pertes), ([], 2, "signalee"))
        self.assertTrue(v.ok3)

    def test_rafale_perte_non_signalee(self):
        objs = [o for o in objets_motif("rafale", [0], 5000) if o["part"] != 500]
        p = self.jsonl("rafale", objs)
        v = banc.evaluer(p, "rafale")
        self.assertEqual(v.pertes, "non signalee")
        self.assertFalse(v.ok3 or v.ok)
        code, sortie = self.lancer(p, "rafale")
        self.assertEqual(code, 1)
        self.assertIn("critere 3 : 0 / 1 rafales completes ; perte non signalee (reception incomplete sans debord) : ECHEC",
                      sortie)


class Ligne(Capture):
    def test_compteurs_de_la_sonde(self):
        p = self.jsonl("krona", objets_motif("krona", range(8), 19000), compteurs(1, sautes=2), compteurs(2, sautes=3))
        code, sortie = self.lancer(p, "krona", "--n", "8")
        self.assertEqual(code, 0)
        self.assertIn("compteurs de la sonde pendant la capture : debord +0, lignes_perdues +0, sautes +1", sortie)

    def test_codes_de_sortie(self):
        self.assertEqual(self.lancer(os.path.join(self.dossier.name, "absent.jsonl"), "krona")[0], 2)
        vide = self.jsonl("vide", [])
        code, sortie = self.lancer(vide, "krona")
        self.assertEqual(code, 1)
        self.assertIn("aucune trame", sortie)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
            banc.main([vide, "inconnu"])
        self.assertEqual(e.exception.code, 2)


class Documentation(unittest.TestCase):
    """docs/BANC.md : le tableau des reglages par motif est coherent avec les motifs."""

    def test_reglages_par_motif(self):
        chemin = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "BANC.md")
        with open(chemin, encoding="utf-8") as f:
            lignes = [re.match(r"^\| `(\w+)` \| `seuils 1 (\d+)` \| (\d+) \| `motif \1` \|", l) for l in f]
        reglages = {m.group(1): (int(m.group(2)), int(m.group(3))) for m in lignes if m}
        self.assertEqual(sorted(reglages), sorted(signaux.MOTIFS))
        for nom, (silence, duree) in reglages.items():
            rh = signaux.repos_haut_motif(nom)
            n = banc.n_defaut(nom)
            trames = [signaux.trame_motif(nom, i) for i in range(min(n, 256))]
            # Aucun palier hors repos ne clot la reception ; le repos apres la trame la clot.
            hors_repos = max(d for t in trames for niv, d in t if niv != rh)
            apres = min((t[-1][1] if t[-1][0] == rh else 0) + signaux.pause_motif_us(nom) for t in trames)
            self.assertTrue(1000 <= silence <= 32767, nom)
            self.assertLess(hors_repos, silence, nom)
            self.assertGreater(apres, silence, nom)
            # L'enregistrement couvre l'emission des n trames, plus 20 s pour lancer le motif.
            emission = sum(sum(d for _, d in signaux.trame_motif(nom, i)) + signaux.pause_motif_us(nom)
                           for i in range(n)) / 1e6
            self.assertGreaterEqual(duree, emission + 20, nom)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_banc.py -v`

Attendu : ÉCHEC avec « ERROR: test_banc (unittest.loader._FailedTest.test_banc) » et « ModuleNotFoundError: No module named 'banc' »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `tools/banc.py` :

```python
#!/usr/bin/env python3
"""Verdicts du banc de validation (docs/SPEC-RECONNAISSANCE.md section 10,
procedure : docs/BANC.md) : une capture de la sonde, prise pendant que le
generateur emettait un motif, contre le contenu connu de ce motif
(tools/signaux.py, identique a src/motifs.cpp).

Usage :
  python3 tools/banc.py <capture> <motif> [--decalage-us N] [--n N] [--silence us] [--tol-us T]

<capture> : fichier .jsonl (serie_enregistre.py, hotte_udp.py enregistre) ou
capture serie brute. <motif> : un nom de signaux.MOTIFS, lance par
'motif <nom>' sur le generateur, qui numerote ses trames a partir de 0.

Critere 1, trames decodees au bit pres : les trames de la capture (une par
reception de la sonde, ou redecoupees a chaque repos >= --silence us) sont
decodees par decodeurs.auto, SANS forcer le decodeur. Leurs contenus (octets
pour l'UART, messages de bits pour la distance d'impulsion) sont mis bout a
bout, dans l'ordre ; une trame en erreur coupe la suite. On y reconnait les
trames du motif : l'index d'une trame est retrouve par son contenu (modulo
256, ou 8 pour krona) et deroule dans l'ordre. Une trame que la sonde a coupee
en deux (krona) est reconnue a cheval. Taux = index reconnus dans 0..n-1 / n
(n = --n, defaut 1000, rafale 1).

Critere 2, durees : chaque trame de la capture qui porte seule une trame
reconnue est comparee palier par palier a signaux.trame_motif, sans les
paliers de repos de tete et de queue (fondus dans le repos). Les durees
hautes mesurees sont corrigees de -N us et les basses de +N us (N =
--decalage-us : asymetrie de la chaine generateur, ligne, etage d'ecoute ;
positif si les niveaux hauts sont allonges). Ecart = duree corrigee -
nominale : moyenne et maximum en valeur absolue, par niveau. OK si l'ecart
maximal est <= --tol-us (2 us) et si chaque trame comparee a les paliers
attendus. Decalage suggere = N + (moyenne des hauts - moyenne des bas) / 2.

Rafale (critere 3) : pas de contenu ; une rafale est complete si sa reception
a les kRafaleSegs - 1 paliers attendus (le premier, haut, se fond dans le
repos). Pertes : receptions incompletes (partie manquante), parties marquees
debord, et hausses de debord, lignes_perdues et sautes entre le premier et le
dernier message 'compteurs' de la sonde dans la capture. Aucune perte, ou une
perte signalee (debord ou compteurs) : OK ; perte non signalee : ECHEC.

Code de sortie : 0 si les criteres sont remplis (1 et 2 ; rafale : 3, et 2
s'il y a une rafale complete) ; 1 sinon, ou si la capture n'a aucune trame ;
2 si la capture est illisible ou les arguments faux.
"""
import argparse
import sys
from dataclasses import dataclass, field

import analyse
import capture_fmt
import decodeurs
import signaux

TOL_US = 2.0
N_DEFAUT = 1000
COMPTEURS_PERTES = ("debord", "lignes_perdues", "sautes")


def n_defaut(nom: str) -> int:
    return 1 if nom == "rafale" else N_DEFAUT


def decodeur_attendu(nom: str) -> tuple[str, dict | None]:
    """(nom du decodeur, parametres) ; parametres None : seul le nom compte (T estime)."""
    if nom == "wtc":
        return "distance_impulsion", None
    if nom == "krona":
        return "uart", {"bauds": 500, "inverse": True}
    if nom.startswith("uart"):
        return "uart", {"bauds": int(nom[4:].removesuffix("inv")), "inverse": nom.endswith("inv")}
    return "", None  # rafale : pas de decodage


def jetons_attendus(nom: str, index: int) -> tuple:
    """Contenu de la trame index dans l'unite du decodeur : octets, ou messages de bits (wtc)."""
    o = signaux.octets_motif(nom, index)
    if nom == "wtc":
        return (format((o[0] << 8) | o[1], "016b"), format(o[2], "08b"))
    return tuple(o)


def jetons(d: decodeurs.Decodage) -> list | None:
    """Contenu d'une trame decodee ; None si son decodage a des erreurs."""
    if d.erreurs:
        return None
    return list(d.octets) if d.nom == "uart" else d.bits.split()


def reconnaitre(unites: list, nom: str) -> list[tuple[int, int | None]]:
    """(index, unite) des trames du motif reconnues, dans l'ordre.

    unites : jetons de chaque trame de la capture (None : decodage en erreur,
    coupe la suite). unite : rang de la trame de la capture qui porte seule
    la trame reconnue, None si elle est a cheval ou partagee."""
    per = 8 if nom == "krona" else 256
    attendus = {jetons_attendus(nom, r): r for r in range(per)}
    lg = len(jetons_attendus(nom, 0))
    flux: list = []
    taille = []
    for u, jts in enumerate(unites):
        taille.append(len(jts) if jts is not None else 0)
        if jts is None:
            flux.append(None)
        else:
            flux.extend((j, u) for j in jts)
    out = []
    dernier = -1
    p = 0
    while p + lg <= len(flux):
        fen = flux[p:p + lg]
        r = None if None in fen else attendus.get(tuple(j for j, _ in fen))
        if r is None:
            p += 1
            continue
        index = dernier + 1 + (r - dernier - 1) % per
        us = {u for _, u in fen}
        seule = us.pop() if len(us) == 1 else None
        out.append((index, seule if seule is not None and taille[seule] == lg else None))
        dernier = index
        p += lg
    return out


def sans_repos(segs, repos_haut: bool) -> list[tuple[bool, int]]:
    """Segments fusionnes, sans le palier de repos de tete ni celui de queue."""
    s = signaux.fusionner(list(segs))
    if s and s[0][0] == repos_haut:
        s = s[1:]
    if s and s[-1][0] == repos_haut:
        s = s[:-1]
    return s


def compteurs_sonde(objets) -> dict | None:
    """Hausse de debord, lignes_perdues et sautes entre le premier et le dernier
    message compteurs (bloc sonde) ; None s'il y en a moins de deux."""
    cs = [o for o in objets if o.get("t") == "compteurs" and o.get("bloc") == "sonde"]
    if len(cs) < 2:
        return None
    return {k: int(cs[-1].get(k, 0)) - int(cs[0].get(k, 0)) for k in COMPTEURS_PERTES}


@dataclass
class Verdict:
    motif: str
    n: int
    tol_us: float
    decalage_us: float
    receptions: int = 0
    incompletes: int = 0
    avec_debord: int = 0
    trames: int = 0                                # trames de la capture analysees
    hypothese: decodeurs.Decodage | None = None    # meilleure hypothese de auto (None : rafale)
    hypothese_ok: bool = True
    reconnues: list[int] = field(default_factory=list)  # index reconnus dans 0..n-1, tries
    comparees: int = 0
    durees: int = 0
    formes_differentes: int = 0
    ecart_moy_haut: float | None = None
    ecart_max_haut: float | None = None
    ecart_moy_bas: float | None = None
    ecart_max_bas: float | None = None
    decalage_suggere: float | None = None
    compteurs: dict | None = None

    @property
    def taux(self) -> float:
        return len(self.reconnues) / self.n

    @property
    def manquantes(self) -> list[int]:
        vues = set(self.reconnues)
        return [i for i in range(self.n) if i not in vues]

    @property
    def ecart_max(self) -> float | None:
        m = [x for x in (self.ecart_max_haut, self.ecart_max_bas) if x is not None]
        return max(m) if m else None

    @property
    def pertes(self) -> str:
        """'aucune', 'signalee' (debord ou compteurs) ou 'non signalee'."""
        hausse = bool(self.compteurs) and any(v > 0 for v in self.compteurs.values())
        if self.avec_debord or hausse:
            return "signalee"
        return "non signalee" if self.incompletes else "aucune"

    @property
    def ok1(self) -> bool:
        return self.hypothese_ok and len(self.reconnues) == self.n

    @property
    def ok2(self) -> bool:
        return self.comparees > 0 and self.formes_differentes == 0 and self.ecart_max <= self.tol_us

    @property
    def ok3(self) -> bool:
        return self.pertes == "signalee" or (self.pertes == "aucune" and len(self.reconnues) == self.n)

    @property
    def ok(self) -> bool:
        if self.motif == "rafale":
            return self.ok3 and (self.ok2 or self.comparees == 0)
        return self.ok1 and self.ok2


def evaluer(chemin, nom: str, n: int | None = None, decalage_us: float = 0.0, silence_us: int | None = None,
            tol_us: float = TOL_US) -> Verdict:
    """Verdict d'une capture pour le motif nom (OSError si elle est illisible)."""
    repos_haut = signaux.repos_haut_motif(nom)
    v = Verdict(motif=nom, n=n or n_defaut(nom), tol_us=tol_us, decalage_us=decalage_us)
    trames, _, recs = analyse.charger(chemin, "haut" if repos_haut else "bas", silence_us)
    v.compteurs = compteurs_sonde(capture_fmt.lire(chemin))
    v.receptions = len(recs)
    v.incompletes = sum(1 for r in recs if not r.complete)
    v.avec_debord = sum(1 for r in recs if r.debord)
    segs = [t for _, t in trames]
    v.trames = len(segs)

    if nom == "rafale":
        attendu = sans_repos(signaux.trame_motif(nom, 0), repos_haut)
        paires = []
        for u, s in enumerate(segs):
            if len(s) == len(attendu) and s[0][0] == attendu[0][0]:
                paires.append((len(paires), u))
    else:
        if not segs:
            return v
        hyp = decodeurs.auto(segs)[0]
        v.hypothese = hyp
        nom_att, params_att = decodeur_attendu(nom)
        v.hypothese_ok = hyp.nom == nom_att and (params_att is None or hyp.params == params_att)
        paires = reconnaitre([jetons(d) for d in decodeurs.appliquer(hyp, segs)], nom)
    v.reconnues = sorted({i for i, _ in paires if i < v.n})

    somme = {True: 0.0, False: 0.0}
    nombre = {True: 0, False: 0}
    pire = {True: 0.0, False: 0.0}
    for index, u in paires:
        if u is None or index >= v.n:
            continue
        attendu = sans_repos(signaux.trame_motif(nom, index), repos_haut)
        mesure = segs[u]
        if len(mesure) != len(attendu) or any(a[0] != b[0] for a, b in zip(mesure, attendu)):
            v.formes_differentes += 1
            continue
        v.comparees += 1
        for (haut, d), (_, nominal) in zip(mesure, attendu):
            e = (d - decalage_us if haut else d + decalage_us) - nominal
            somme[haut] += e
            nombre[haut] += 1
            pire[haut] = max(pire[haut], abs(e))
    v.durees = nombre[True] + nombre[False]
    if nombre[True]:
        v.ecart_moy_haut, v.ecart_max_haut = somme[True] / nombre[True], pire[True]
    if nombre[False]:
        v.ecart_moy_bas, v.ecart_max_bas = somme[False] / nombre[False], pire[False]
    if nombre[True] and nombre[False]:
        v.decalage_suggere = decalage_us + (v.ecart_moy_haut - v.ecart_moy_bas) / 2
    return v


def fmt(x: float) -> str:
    return f"{x:g}"


def ok(b: bool) -> str:
    return "OK" if b else "ECHEC"


def afficher(v: Verdict, chemin) -> None:
    print(f"capture {chemin} : {v.receptions} receptions ({v.incompletes} incompletes, {v.avec_debord} avec debord), "
          f"{v.trames} trames analysees")
    repos = "haut" if signaux.repos_haut_motif(v.motif) else "bas"
    print(f"motif {v.motif} : repos {repos}, {v.n} trames attendues")
    if v.motif == "rafale":
        completes = f"critere 3 : {len(v.reconnues)} / {v.n} rafales completes"
        debit = 1000000 // signaux.trame_motif("rafale", 0)[0][1]
        if v.pertes == "aucune" and v.ok3:
            print(f"{completes} ; aucune perte : OK ({debit} fronts/s tenus)")
        elif v.pertes == "aucune":
            print(f"{completes} ; aucune perte signalee, mais rafale absente ou deformee : ECHEC")
        elif v.pertes == "signalee":
            print(f"{completes} ; perte signalee (debord ou compteurs) : OK, debit maximal < {debit} fronts/s")
        else:
            print(f"{completes} ; perte non signalee (reception incomplete sans debord) : ECHEC")
    else:
        nom_att, params_att = decodeur_attendu(v.motif)
        att = analyse.libelle(decodeurs.Decodage(nom_att, params_att, b"", "", 0, 0)) if params_att else nom_att
        print(f"decodeur (auto, sans forcer) : {analyse.resume(v.hypothese)} ; attendu : {att} : "
              + ("OK" if v.hypothese_ok else "INATTENDU"))
        print(f"critere 1 : {len(v.reconnues)} / {v.n} trames decodees au bit pres ({100 * v.taux:.1f} %) : {ok(v.ok1)}")
        manq = v.manquantes
        if manq:
            suite = f" ... (+{len(manq) - 20})" if len(manq) > 20 else ""
            print("  manquantes : " + ", ".join(str(i) for i in manq[:20]) + suite)
    print(f"durees : {v.comparees} trames comparees ({v.durees} durees), decalage {fmt(v.decalage_us)} us, "
          f"{v.formes_differentes} formes differentes")
    if v.durees:
        parts = []
        for nom, moy, pire in (("hautes", v.ecart_moy_haut, v.ecart_max_haut), ("basses", v.ecart_moy_bas, v.ecart_max_bas)):
            if moy is not None:
                parts.append(f"{nom} : ecart moyen {moy:+.2f} us, max {fmt(pire)} us")
        sugg = f" ; decalage suggere {v.decalage_suggere:+.1f} us" if v.decalage_suggere is not None else ""
        print("  " + " ; ".join(parts) + sugg)
        print(f"critere 2 : ecart max {fmt(v.ecart_max)} us <= {fmt(v.tol_us)} us : {ok(v.ok2)}")
    else:
        print("critere 2 : aucune trame comparee : ECHEC")
    if v.compteurs is None:
        print("compteurs de la sonde : moins de deux messages dans la capture, pertes cote sonde inconnues")
    else:
        print("compteurs de la sonde pendant la capture : "
              + ", ".join(f"{k} {v.compteurs[k]:+d}" for k in COMPTEURS_PERTES))
    print(f"verdict : {ok(v.ok)}")


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="banc.py", description="Verdicts du banc : capture de la sonde contre un motif.")
    p.add_argument("capture")
    p.add_argument("motif", choices=signaux.MOTIFS)
    p.add_argument("--decalage-us", type=float, default=0.0,
                   help="asymetrie a corriger : hauts - N us, bas + N us (defaut 0)")
    p.add_argument("--n", type=int, help="trames emises par le generateur (defaut 1000, rafale 1)")
    p.add_argument("--silence", type=int, help="redecoupe a chaque repos >= silence us (defaut : une trame par reception)")
    p.add_argument("--tol-us", type=float, default=TOL_US, help="ecart maximal admis (defaut 2 us)")
    a = p.parse_args(argv)
    if (a.n is not None and a.n < 1) or (a.silence is not None and a.silence < 1) or a.tol_us < 0:
        p.error("--n et --silence doivent etre >= 1, --tol-us >= 0")
    try:
        v = evaluer(a.capture, a.motif, a.n, a.decalage_us, a.silence, a.tol_us)
    except OSError as e:
        print(f"banc.py : {e}", file=sys.stderr)
        return 2
    if not v.trames:
        print(f"aucune trame dans {a.capture}")
        return 1
    afficher(v, a.capture)
    return 0 if v.ok else 1


if __name__ == "__main__":
    sys.exit(main())
```

Créer `docs/BANC.md` :

````markdown
# Banc de validation

Avant la hotte, on vérifie la sonde sur un bus imité (§10 de
[SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md)). Un second ESP32-C6, avec le
firmware `generateur`, émet sur une ligne de 5 V à drain ouvert des motifs dont
on connaît le contenu au bit près. La sonde les capture. `tools/banc.py` compare
chaque capture au motif émis. **Aucun secteur au banc, et aucune liaison avec
la hotte** (§2) : les deux C6 sont sur l'USB du Mac.

Montage détaillé : [WIRING.md §10](WIRING.md#10-générateur-du-banc). Ce fichier
donne la procédure, les commandes, les critères et les résultats. Il est versé
dans un **dépôt public** : ni SSID, ni mot de passe, ni clé.

## 1. Matériel

- la sonde (C6 SuperMini, firmware `sonde`) et un étage d'écoute (WIRING §5) :
  celui du boîtier de mesure **détaché de la hotte**, ou celui d'une plaque
  d'essai du banc (§2.1) ;
- pour la plaque d'essai : plaque sans soudure ; NPN (BC547 ou 2N3904),
  100k × 2 (série et `R_be`), 10k (collecteur) ;
- le générateur (second C6 SuperMini, firmware `generateur`) ;
- Q3 (BC547 ou 2N3904), 4,7k (base), 10k (base-émetteur) ;
- 47k et 68k : diviseur de la voie D0 de l'analyseur (§2.3) ;
- testeur USB, s'il y en a un (critère 6, §2.4) ;
- pull-up de ligne : 10k (variante 1) ; 4,7k et 1 nF (variante 2) ;
- fils Dupont, un fil de masse ;
- multimètre (contrôles, critère 6) ;
- plus tard : l'analyseur FX2 (critère 2 complet) et l'étage d'injection (critère 5).

## 2. Montage

**Le banc n'a jamais de liaison avec la hotte** : ni le générateur, ni la
sonde, ni le Mac, ni l'analyseur
([SECURITE.md](SECURITE.md#banc-de-validation--aucune-liaison-avec-la-hotte),
[WIRING.md §10](WIRING.md#10-générateur-du-banc)). Tout se câble **les câbles
USB débranchés** : ceux de la sonde, du générateur, et de l'analyseur s'il est
là.

### 2.1 Où monter la sonde

Au choix, noté au §7 (« Montage et firmwares ») :

- **Sur le boîtier de mesure, détaché de la hotte**, seulement si :
  1. la hotte reste **débranchée pendant toute la séance** (sa fiche hors de
     la prise, à vue) ;
  2. le **câble de sortie est déconnecté du boîtier** : fiche XH 4 broches
     retirée de l'embase, hotte débranchée ;
  3. **avant de brancher le moindre USB**, contrôle hors tension, multimètre
     au calibre le plus élevé : `TP−` vers la terre de la fiche de la hotte
     (le trou qui reçoit la broche de terre), puis `TP−` vers la vis de la
     carcasse : **OL** les deux fois. Sinon, on ne branche rien : une liaison
     reste, on la cherche.

  `J2` reste ouvert, `TP+` libre. Le boîtier ne retourne sur la hotte qu'une
  fois le banc démonté (étage d'injection du critère 5 compris), après ses
  contrôles ([WIRING.md §12](WIRING.md#12-contrôles-avant-pose), lignes 11 à
  17), fiche XH 4 broches remise hotte débranchée.
- **Sinon, sur une plaque d'essai du banc**, avec son propre étage d'écoute,
  aux valeurs de WIRING §5 : 100k de la LIGNE à la base d'un NPN, `R_be` de
  100k entre base et émetteur, 10k du `3V3` de la sonde au collecteur,
  collecteur sur GPIO6, émetteur au `GND` de la sonde.

Dans la suite, `TP_Dp` est l'entrée de l'étage d'écoute (son 100k côté LIGNE)
et `TP−` le `GND` de la sonde, sur le boîtier comme sur la plaque d'essai.

### 2.2 Ligne et générateur

```
  5V (générateur) ── R_pu ──┬────────────────────────► LIGNE ──► TP_Dp de la sonde
                            │                                    (entrée 100k de l'étage d'écoute)
                            ├── 1 nF ── GND  (variante 2 seulement)
                            └── collecteur de Q3
  GPIO7 (générateur) ── 4,7k ──┬── base de Q3
                              10k
  GND (générateur) ────────────┴── émetteur de Q3 ──────────► TP− de la sonde (fil de masse)

  R_pu : 10k (variante 1), ou 4,7k avec 1 nF vers la masse (variante 2).
```

1. **Fil de masse** entre le `GND` du générateur et `TP−` (le `GND` de la sonde).
2. **Pull-up** entre le `5V` du générateur et la LIGNE : 10k pour la variante 1 ;
   4,7k, plus 1 nF entre la LIGNE et la masse, pour la variante 2 (le filtre
   supposé de la carte).
3. **Q3** : base par 4,7k sur GPIO7 du générateur, 10k entre base et émetteur,
   émetteur à la masse, collecteur sur la LIGNE. GPIO7 haut = ligne basse.
4. **La LIGNE sur `TP_Dp`**, l'entrée de l'étage d'écoute de la sonde.
5. L'étage d'injection n'est pas monté (sauf pour le critère 5) ; sur le
   boîtier, `J2` ouvert, `TP+` libre.
6. **Contrôles hors tension, au multimètre** : LIGNE vers masse, plus de 10 kΩ
   (pas de court-circuit) ; GPIO7 du générateur n'est relié qu'au 4,7k ; GPIO6 de
   la sonde n'est relié qu'au collecteur de l'étage d'écoute et à son 10k ; sur
   le boîtier, les deux **OL** du §2.1, avant tout USB.

### 2.3 Analyseur FX2 (critère 2 à l'analyseur)

Il mesure le délai et l'asymétrie de l'étage d'écoute seul. Il se pose les
câbles USB débranchés, **masse de l'analyseur d'abord**, sur `TP−`. La LIGNE
passe par le diviseur 47k/68k de WIRING §8 ; jamais la LIGNE en direct sur une
voie.

```
  LIGNE ── 47k ──┬────────────► D0   (LIGNE × 0,59)
                 │
                68k
                 │
  TP− ───────────┴────────────► GND de l'analyseur (posé en premier)

  GPIO6 (sonde) ──────────────► D1   (sortie de l'étage d'écoute, 3,3 V)
  GPIO7 (générateur) ─────────► D2   (facultatif : commande de Q3, 3,3 V)
```

- D1 va sur GPIO6 **de la sonde**, D2 sur GPIO7 **du générateur** : aucune
  voie sur GPIO7 de la sonde.
- Chargée par le diviseur, la LIGNE au repos descend vers 4,0 à 4,7 V.
- PulseView, pilote `fx2lafw` : 12 MHz (24 MHz si la capture passe sans
  erreur), au moins 1,5 s de capture (20 M échantillons à 12 MHz, 40 M à
  24 MHz), déclenchement sur un front descendant de D0.
- Délais au décodeur « Jitter » de PulseView, une rangée par sens (Clock : la
  voie de départ ; Resulting signal : la voie d'arrivée) :
  - front descendant de la LIGNE : Clock D0 `falling`, Resulting signal D1
    `rising` (l'étage inverse) ;
  - front montant de la LIGNE : Clock D0 `rising`, Resulting signal D1
    `falling` ;
  - avec D2 (Q3) : Clock D2 `rising`, Resulting signal D0 `falling`, puis
    Clock D2 `falling`, Resulting signal D0 `rising`.

  À défaut, les deux curseurs de PulseView, front par front.
- Asymétrie = délai moyen des fronts descendants − délai moyen des fronts
  montants (fronts de la LIGNE). Positive, elle allonge les niveaux hauts
  capturés, comme un `--decalage-us` positif : sur toute la chaîne, `banc.py`
  trouve N ≈ asymétrie de l'étage + asymétrie de Q3.
- Le FX2 est numérique : il donne des instants, pas des tensions. Un relevé
  chiffré des niveaux de la LIGNE est optionnel, à l'oscilloscope, et
  l'instrument est noté au §7.
- En variante 2, la LIGNE monte lentement (4,7k et 1 nF : environ 5 µs). D0
  franchit son seuil (vers 2,5 V sur la LIGNE) après l'étage (vers 1,2 V) :
  le délai montant y paraît plus court de 2 à 3 µs, parfois négatif.
  L'étage seul se lit en variante 1.

### 2.4 Courant de la sonde (critère 6)

Deux méthodes ; l'instrument est noté au §7.

- **Testeur USB**, s'il y en a un, entre la source et le câble USB de la
  sonde : le port du Mac (sans Wi-Fi) ou la batterie (Wi-Fi actif). On lit le
  courant affiché.
- **Sinon, multimètre en série sur VBUS**, vers la broche `5V` de la sonde :
  1. tous les USB débranchés ; **l'USB-C de la sonde reste débranché pendant
     toute la mesure** : jamais deux sources sur sa broche `5V` ;
  2. multimètre en courant continu, **calibre 200 mA** : borne mA sur le `5V`
     du générateur (le VBUS de son USB), COM sur la broche `5V` de la sonde ;
     la masse est déjà commune (fil de masse) ; sur le boîtier, `J2` reste
     ouvert ;
  3. brancher l'USB du générateur seul : la sonde démarre par sa broche `5V`,
     capture active, et rejoint seule le Wi-Fi s'il est configuré. Lire le
     courant. S'il saute sans cesse, ou si la session Wi-Fi tombe, la chute
     de tension dans le calibre est trop forte : passer sur l'entrée 10 A ;
  4. à la fin, débrancher l'USB du générateur et retirer le multimètre : plus
     rien ne relie les deux `5V`.

L'afficheur donne une moyenne ; les pointes d'émission du Wi-Fi (350 mA au
plus, fiche du C6) ne s'y lisent pas. Ordres de grandeur : 20 à 50 mA sans
Wi-Fi, de l'ordre de 100 mA Wi-Fi actif (spec §8.7).

## 3. Logiciels et ports

- **Ports.** Brancher la sonde seule, `ls /dev/cu.usbmodem*`, noter son port
  (`PORT_SONDE`). Brancher ensuite le générateur, relancer la commande, noter le
  nouveau port (`PORT_GEN`). Toujours les mêmes prises du Mac.
- **Flash** (depuis la racine du dépôt) :
  - `~/.platformio/penv/bin/pio run -e sonde -t upload --upload-port $PORT_SONDE`
  - `~/.platformio/penv/bin/pio run -e generateur -t upload --upload-port $PORT_GEN`
- **Console du générateur**, dans un terminal à part, ouvert pendant tout le banc :
  `~/.platformio/penv/bin/pio device monitor -e generateur -p $PORT_GEN`.
  Commandes : `motif <nom> [n] [pause_ms]` (1 000 trames par défaut, 1 pour la
  rafale ; 0 : sans fin), `stop`, `etat`, `help`. Il affiche
  `motif <nom> trame <index>` à chaque trame, puis `motif <nom> fini : <n> trames`.
- **Enregistrement de la sonde** (mode machine par l'USB, fichier
  `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl`) :
  `python3 tools/serie_enregistre.py $PORT_SONDE <scénario> "capture tout" "seuils 1 <silence>" --duree <s>`.
  Aucun terminal ne doit tenir `PORT_SONDE` pendant ce temps.
- **Verdicts** :
  - `python3 tools/banc.py <capture> <motif> [--decalage-us N]` : critères 1 à 3 ;
  - `python3 tools/analyse.py auto <capture> --max 3` : la même capture vue par
    l'outil de l'étape 6, sans connaître le motif ;
  - `python3 tools/json_check.py -q <capture>` : conformité des lignes machine.

## 4. Réglages par motif

Le seuil de silence de la sonde clôt une réception : il doit dépasser le plus long
palier **hors repos** d'une trame (18 ms pour l'octet 0x00 à 500 bauds), et rester
sous le repos qui suit la trame (bit de stop et pause). Pour `krona`, les octets
0xFF et 0xFE suivis de la pause de 4 ms font 20 à 22 ms de repos **dans** la
trame : la sonde coupe alors la trame entre deux octets, sans rien perdre, et
`banc.py` la recolle. `--duree` couvre l'émission des trames, plus 20 s pour
lancer le motif.

| Motif | Seuils de la sonde | `--duree` (s) | Générateur | Émission (s) |
|---|---|---|---|---|
| `uart500` | `seuils 1 19000` | 170 | `motif uart500` | 140,0 |
| `uart500inv` | `seuils 1 19000` | 170 | `motif uart500inv` | 140,0 |
| `uart2400` | `seuils 1 5000` | 70 | `motif uart2400` | 45,0 |
| `uart2400inv` | `seuils 1 5000` | 70 | `motif uart2400inv` | 45,0 |
| `uart9600` | `seuils 1 5000` | 50 | `motif uart9600` | 26,3 |
| `uart9600inv` | `seuils 1 5000` | 50 | `motif uart9600inv` | 26,3 |
| `wtc` | `seuils 1 5000` | 80 | `motif wtc` | 56,9 |
| `krona` | `seuils 1 19000` | 160 | `motif krona` | 134,0 |
| `rafale` | `seuils 1 5000` | 40 | `motif rafale` | 11,0 |

Contenus (identiques dans `src/motifs.cpp` et `tools/signaux.py`) :
- `uartNNN` : 8N1 à NNN bauds, repos haut (`inv` : repos bas, niveaux inversés),
  octets `A5 5A 00 FF i s` (i = index & 0xFF, s = somme & 0xFF), pause de 20 ms ;
- `wtc` : distance d'impulsion, T = 750 µs, départ 2T bas, « 0 » = 1T haut +
  1T bas, « 1 » = 1T haut + 3T bas, 16 bits `0x0100 << (index % 8)`, puis une
  réponse simulée de la carte 4T après : 8 bits `index & 0xFF` ; pause de 6 ms ;
- `krona` : `55 k ~k 00 s` (k = index % 8), 500 bauds 8N1 inversé (repos bas),
  4 ms entre deux octets, 18 ms après la trame ;
- `rafale` : 100 000 paliers de 100 µs (un front toutes les 100 µs pendant 10 s),
  une seule trame, pause de 1 s.

## 5. Déroulé d'un motif

1. L'agent lance l'enregistrement (réglages du §4) :
   `python3 tools/serie_enregistre.py $PORT_SONDE banc-v<variante>-<motif> "capture tout" "seuils 1 <silence>" --duree <s>`.
2. Dès que le terminal affiche la réponse `ok` de `seuils`, Majid tape
   `motif <motif>` dans la console du générateur.
3. Majid regarde défiler `motif <motif> trame <index>` jusqu'à
   `motif <motif> fini : 1000 trames` ; l'enregistrement s'arrête seul.
4. L'agent lance `banc.py`, `analyse.py auto` et `json_check.py` sur le fichier
   écrit, et note les résultats au §7.

## 6. Critères (§10 de la spec)

| # | Critère | Comment on le vérifie | Tâche |
|---|---|---|---|
| 1 | Pour chaque motif, 1 000 trames décodées **au bit près** par `analyse.py auto`, sans forcer le décodeur | `banc.py` : `critere 1 : 1000 / 1000` et décodeur attendu ; `analyse.py auto` : l'hypothèse 1 est ce décodeur, 0 erreur | 16 |
| 2 | Durées à ±2 µs du nominal, une fois corrigé le décalage de l'étage ; asymétrie inférieure à 5 µs, ou documentée et compensée | sans FX2 : `banc.py` donne le `decalage suggere` (asymétrie de toute la chaîne), puis `--decalage-us` et `critere 2 : ... OK`. Avec le FX2 : une voie sur la LIGNE à travers 47k/68k, une voie sur GPIO6, délai et asymétrie de l'étage mesurés | 16 (sans FX2), puis à l'arrivée du FX2 |
| 3 | Rafale : aucune perte, ou une perte signalée par `debord`, et le débit maximal documenté | `banc.py <capture> rafale` : `critere 3`, compteurs de la sonde (`debord`, `lignes_perdues`, `sautes`) | 16 |
| 4 | 10 min de `krona` par le Wi-Fi, mode `tout`, sonde à son emplacement de test : zéro perte côté sonde, trous de `n` côté Mac sous 0,1 %, avec le RSSI | `hotte_udp.py enregistre`, `json_check.py --jsonl`, compteurs | 21 |
| 5 | Collision détectée et émission arrêtée ; à défaut, la limite est documentée | étage d'injection monté au banc, le générateur émet pendant une injection | 24 |
| 6 | Consommation de la sonde relevée, Wi-Fi actif | USB d'abord (tâche 16), Wi-Fi actif ensuite | 16 (USB), 21 (Wi-Fi) |

## 7. Résultats

### Montage et firmwares

Sonde sur : `boîtier` (détaché de la hotte, §2.1) ou `plaque` (plaque d'essai
du banc). Contrôle `TP−` vers la terre de la fiche et vers la vis de la
carcasse : boîtier seulement, avant tout USB, une ligne par séance.

| Date | Tâche | Variante (R_pu) | Sonde sur | `TP−` → terre / vis (boîtier) | `fw` de la sonde | `fw` du générateur | `PORT_SONDE` | `PORT_GEN` | LIGNE au repos (V) |
|---|---|---|---|---|---|---|---|---|---|
| | 16 | 1 (10k) | | | | | | | |
| | 16 | 2 (4,7k + 1 nF) | | | | | | | |
| | 16b | 1 (10k) | | | | | | | |
| | 16b | 2 (4,7k + 1 nF) | | | | | | | |

### Critères 1 et 2, variante 1 (10k)

| Motif | Fichier `logs/` | Trames reconnues / 1000 | Décodeur de `auto` | Écart moyen hauts / bas (µs) | Écart max (µs) | Décalage suggéré (µs) | Verdict |
|---|---|---|---|---|---|---|---|
| `uart500` | | | | | | | |
| `uart500inv` | | | | | | | |
| `uart2400` | | | | | | | |
| `uart2400inv` | | | | | | | |
| `uart9600` | | | | | | | |
| `uart9600inv` | | | | | | | |
| `wtc` | | | | | | | |
| `krona` | | | | | | | |

### Critères 1 et 2, variante 2 (4,7k + 1 nF)

| Motif | Fichier `logs/` | Trames reconnues / 1000 | Décodeur de `auto` | Écart moyen hauts / bas (µs) | Écart max (µs) | Décalage suggéré (µs) | Verdict |
|---|---|---|---|---|---|---|---|
| `uart500` | | | | | | | |
| `uart500inv` | | | | | | | |
| `uart2400` | | | | | | | |
| `uart2400inv` | | | | | | | |
| `uart9600` | | | | | | | |
| `uart9600inv` | | | | | | | |
| `wtc` | | | | | | | |
| `krona` | | | | | | | |

### Décalage retenu (critère 2)

| Variante | Décalage retenu (`--decalage-us`) | Écart max après correction (µs) | Remarque |
|---|---|---|---|
| 1 (10k) | | | |
| 2 (4,7k + 1 nF) | | | |

### Critère 2 à l'analyseur (tâche 16b)

Analyseur et fréquence d'échantillonnage : ____. Instrument des niveaux de la
LIGNE : ☐ aucun (le FX2 ne donne que des instants) ☐ oscilloscope : ____.

Délais sur 20 fronts de chaque sens, moyenne / max, en µs (§2.3) : étage =
LIGNE (D0) vers GPIO6 (D1) ; Q3 = GPIO7 du générateur (D2) vers la LIGNE,
facultatif. Asymétrie = descendant − montant (fronts de la LIGNE).

| Variante | Motif | Fichiers `logs/` (`.sr`, `.jsonl`) | Étage : descendant | Étage : montant | Asymétrie de l'étage | Q3 (D2) : asymétrie | N retenu (tâche 16) | `banc.py`, même capture : trames, décalage suggéré | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| 1 (10k) | `uart9600` | | | | | | | | |
| 1 (10k) | `wtc` | | | | | | | | |
| 2 (4,7k + 1 nF) | `uart9600` | | | | | | | | |
| 2 (4,7k + 1 nF) | `wtc` | | | | | | | | |

Asymétrie de l'étage sous 5 µs : ☐ oui ☐ non (elle est alors documentée ici
et compensée par `--decalage-us`).

**Pour l'injection (`tol_us`)** : plus grand délai mesuré (étage, plus Q3 si
D2 est posé) + 10 µs de marge = ____ µs ; on garde 20 µs, la valeur par
défaut, si le calcul donne moins. Valeur reportée dans l'avenant de l'étape 6
(`RECONNAISSANCE.md`) et réglée avant les essais d'injection du banc
(`injection regle tol_us <valeur>`).

### Critère 3 : rafale

| Variante | Fichier `logs/` | Rafales complètes | Parties avec `debord` | Réceptions incomplètes | `debord` / `lignes_perdues` / `sautes` (hausse) | Débit tenu | Verdict |
|---|---|---|---|---|---|---|---|
| 1 (10k) | | | | | | | |
| 2 (4,7k + 1 nF) | | | | | | | |

### Critère 4 : Wi-Fi (tâche 21)

| Date | Emplacement | RSSI (dBm) | Durée | `lignes_perdues` / `debord` / `sautes` | Trous de `n` (Mac) | Verdict |
|---|---|---|---|---|---|---|
| | | | | | | |

### Critère 5 : collision (tâche 24)

| Date | Essai | Résultat de `injection` | Émission arrêtée ? | Remarque |
|---|---|---|---|---|
| | | | | |

### Critère 6 : consommation de la sonde

| Date | Alimentation, instrument (§2.4) | Situation | Courant (mA) |
|---|---|---|---|
| | USB, sans Wi-Fi ; testeur USB ou multimètre : ____ | capture active, bus au repos | |
| | USB, sans Wi-Fi ; testeur USB ou multimètre : ____ | capture active, motif `krona` | |
| | Wi-Fi actif (tâche 21) | session UDP, motif `krona` | |

### Conclusions

- Débit maximal de la capture :
- Décalage et asymétrie retenus :
- Délai de l'étage à l'analyseur, et `tol_us` proposé (tâche 16b) :
- Limites constatées :
````

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_banc.py -v`

Attendu : 14 tests à `ok` (`Abimee` 2, `Contenus` 6, `Documentation` 1, `Ligne` 2, `Rafale` 3), « Ran 14 tests », « OK ».

Lancer : `python3 -m unittest discover -s tools/tests` puis `sh tools/tests/test_hote.sh | tail -1`

Attendu : « OK » (toute la suite : 140 tests à la rédaction, tâches 2 à 15) ; « tests hote : OK ».

Contrôle de durée (facultatif) : sur des captures synthétiques de 1 000 trames, `banc.evaluer` prend moins d'une seconde par motif (0,6 s pour `krona`, 0,8 s pour `wtc` à la rédaction).

Contrôle du texte de `docs/BANC.md` (montage sans liaison avec la hotte, analyseur, courant) :

```bash
for m in "jamais de liaison avec la hotte" "retirée de l'embase, hotte débranchée" "carcasse : **OL** les deux fois" "masse de l'analyseur d'abord" "calibre 200 mA" "USB-C de la sonde reste débranché" "### Critère 2 à l'analyseur (tâche 16b)" "Pour l'injection"; do grep -q -F -- "$m" docs/BANC.md && echo "ok : $m" || echo "ABSENT : $m"; done
```

Attendu : huit lignes « ok : ... », aucune « ABSENT : ... ».

- [ ] **Étape 5 : Commit**

```bash
git add tools/banc.py tools/tests/test_banc.py docs/BANC.md
git commit -m "Ajouter les verdicts du banc et sa procedure" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 16 : [BANC avec Majid] Validation de la sonde par l'USB

Exécutée par l'agent principal avec Majid, jamais par un sous-agent. Aucun
secteur, et **aucune liaison avec la hotte** (`docs/BANC.md` §2.1) : les deux C6
sont sur l'USB du Mac. Critères du §10 de la spec vérifiés ici : **1**, **2 sans
FX2**, **3** et **6** (consommation par l'USB ; celle avec le Wi-Fi viendra à la
tâche 21). Le critère 2 à l'analyseur (délai et asymétrie de l'étage seul) est
la tâche 16b, à l'arrivée du FX2 ; le critère 4 est la tâche 21, le critère 5 la
tâche 24. Tout résultat va dans `docs/BANC.md` §7. Cette tâche n'écrit que des
résultats : la procédure est celle de `docs/BANC.md` (tâche 15).

**Fichiers :**
- Modifier : `docs/BANC.md` (§7 : tableaux de résultats et conclusions)
- Tester : captures `logs/*-banc-v1-*.jsonl` et `logs/*-banc-v2-*.jsonl` (ignorées par git), `tools/banc.py`, `tools/analyse.py auto`, `tools/json_check.py`

**Interfaces :**
- Consomme : firmware `sonde` en mode machine par l'USB (tâches 7 à 11 : `json 1`, `capture tout`, `seuils 1 <silence>`, lignes `trame` et `compteurs`) ; `tools/serie_enregistre.py <port> <scénario> [commande ...] --duree s` (tâche 12) ; firmware `generateur` et sa console (tâche 14) ; `tools/banc.py` et `docs/BANC.md` §2 à §5 (tâche 15 : §2.1 où monter la sonde, §2.2 ligne et générateur, §2.4 courant) ; `tools/analyse.py auto` (tâche 6) ; `tools/json_check.py -q` (tâche 11).
- Produit : `docs/BANC.md` §7 rempli pour les variantes 1 et 2 ; le **décalage retenu** `N` (µs) par variante, repris à l'étape 5b et comparé, en tâche 16b, à l'asymétrie mesurée à l'analyseur ; le débit tenu par la capture (rafale) ; la consommation de la sonde par l'USB.

**Ce que l'agent fait, ce que Majid fait.** L'agent lance les commandes du Mac
(flash, enregistrement, verdicts) après l'accord de Majid pour chaque flash.
Majid câble, mesure au multimètre, tient la console du générateur et y tape les
commandes `motif`. Dans les commandes ci-dessous, `$PORT_SONDE` et `$PORT_GEN`
sont les ports relevés à l'étape 2.

- [ ] **Étape 1 : Montage, variante 1 (10k), hors tension, sans liaison avec la hotte**

Les deux câbles USB débranchés, Majid choisit où monter la sonde
(`docs/BANC.md` §2.1) :
- **boîtier de mesure détaché**, seulement si la hotte reste débranchée pendant
  toute la séance et si la fiche XH 4 broches du câble de sortie est retirée de
  l'embase du boîtier (hotte débranchée) ; `J2` ouvert, `TP+` libre ;
- sinon, **plaque d'essai du banc** : Majid y câble l'étage d'écoute de la sonde
  (100k de la LIGNE à la base d'un NPN, `R_be` de 100k entre base et émetteur,
  10k du `3V3` de la sonde au collecteur, collecteur sur GPIO6, émetteur au
  `GND` de la sonde).

Puis il câble selon `docs/BANC.md` §2.2 (schéma de WIRING §10) : fil de masse
`GND` du générateur → `TP−` ; pull-up de 10k entre le `5V` du générateur et la
LIGNE ; Q3 (4,7k en base depuis GPIO7 du générateur, 10k base-émetteur,
émetteur à la masse, collecteur sur la LIGNE) ; LIGNE sur `TP_Dp`. Pas d'étage
d'injection.

Contrôles au multimètre, toujours hors tension, **avant de brancher le moindre
USB**, notés au §7 (« Montage et firmwares », ligne `16`) :

| Mesure | Attendu | Sinon |
|---|---|---|
| boîtier seulement : `TP−` vers la terre de la fiche de la hotte (le trou qui reçoit la broche de terre), calibre le plus élevé | OL | une liaison avec la hotte reste (fiche XH encore enfichée ?) : ne rien brancher, la chercher |
| boîtier seulement : `TP−` vers la vis de la carcasse, calibre le plus élevé | OL | idem |
| LIGNE vers `GND` (ohmmètre) | plus de 10 kΩ | court-circuit : revoir le câblage avant tout USB |
| GPIO7 du générateur vers LIGNE | pas de liaison directe (seulement par Q3) | revoir le câblage |
| GPIO6 de la sonde | relié seulement au collecteur de l'étage d'écoute (Q1 sur le boîtier) et à son 10k | ne jamais relier GPIO6 à GPIO7 (WIRING §6) |

- [ ] **Étape 2 : Ports et flash (l'agent, avec l'accord de Majid)**

1. Brancher **la sonde seule** sur l'USB du Mac, puis :

   ```bash
   ls /dev/cu.usbmodem*
   ```

   Noter le port : `PORT_SONDE=/dev/cu.usbmodemXXXX`.

2. Flasher la sonde :

   ```bash
   ~/.platformio/penv/bin/pio run -e sonde -t upload --upload-port $PORT_SONDE
   ```

   Attendu : « [SUCCESS] ».

3. Brancher le générateur (autre prise du Mac), relancer `ls /dev/cu.usbmodem*`,
   noter le nouveau port : `PORT_GEN=/dev/cu.usbmodemYYYY`. Flasher :

   ```bash
   ~/.platformio/penv/bin/pio run -e generateur -t upload --upload-port $PORT_GEN
   ```

   Attendu : « [SUCCESS] ».

- [ ] **Étape 3 : Contrôle sous tension, générateur à l'arrêt**

1. Majid ouvre la console du générateur dans un terminal à part, et la garde
   ouverte jusqu'à la fin du banc :

   ```bash
   ~/.platformio/penv/bin/pio device monitor -e generateur -p $PORT_GEN
   ```

   Il tape `etat`. Attendu : « etat : arret, ligne relachee (GPIO7 bas) ». Puis
   `help` : la liste des 9 motifs avec leur repos et leur pause. S'il voit
   « [rmt] ... : generateur inutilisable », on s'arrête et on analyse (pilote
   RMT).
2. Majid mesure, COM sur `TP−` : la LIGNE entre 4,2 et 5,0 V (le `5V` du
   générateur, 4,7 à 5,0 V selon la carte, moins la chute dans le pull-up due au
   courant de l'étage d'écoute) ; GPIO7 du générateur à 0 V ; GPIO6 de la sonde
   près de 0 V (étage inverseur : ligne haute, Q1 saturé). Il note la tension de
   la LIGNE au §7.
3. Essai court de toute la chaîne. L'agent lance :

   ```bash
   python3 tools/serie_enregistre.py $PORT_SONDE banc-v1-essai "capture tout" "seuils 1 5000" --duree 20
   ```

   Dès que le terminal affiche « id=3 seuils 1 5000 : ok », Majid tape
   `motif uart9600 20` dans la console du générateur, et voit défiler
   « motif uart9600 trame 0 » à « motif uart9600 trame 19 », puis
   « motif uart9600 fini : 20 trames, ligne au repos haut (GPIO7 bas) ».
   À la fin de l'enregistrement, l'agent lance :

   ```bash
   F=$(ls -t logs/*-banc-v1-essai.jsonl | head -1)
   python3 tools/banc.py "$F" uart9600 --n 20
   ```

   Attendu : « critere 1 : 20 / 20 trames decodees au bit pres (100.0 %) : OK ».
   Sinon : pas de trame du tout → vérifier la masse commune, la LIGNE sur
   `TP_Dp`, la tension de GPIO6 pendant `motif uart9600 0` (elle doit bouger au
   multimètre) ; décodage faux → noter la sortie de `banc.py` et analyser avant
   d'aller plus loin (on ne touche ni aux tolérances ni aux motifs).

- [ ] **Étape 4 : Critères 1 et 2, variante 1 : les 8 motifs à contenu**

Pour chaque ligne du tableau du §4 de `docs/BANC.md`, sauf `rafale`, dans cet
ordre : `uart500`, `uart500inv`, `uart2400`, `uart2400inv`, `uart9600`,
`uart9600inv`, `wtc`, `krona`. Le seuil de silence est celui du §4 :
`seuils 1 19000` pour `uart500`, `uart500inv` et `krona`, `seuils 1 5000` pour
les autres. Exemple complet pour `uart500` (seuil 19000, 170 s) :

1. L'agent lance l'enregistrement (aucun autre programme ne doit tenir
   `$PORT_SONDE`) :

   ```bash
   python3 tools/serie_enregistre.py $PORT_SONDE banc-v1-uart500 "capture tout" "seuils 1 19000" --duree 170
   ```

   (Dans un autre terminal, `tail -f logs/live.log` montre le résumé toutes les
   5 s.)
2. Dès « id=3 seuils 1 19000 : ok », Majid tape `motif uart500`. Il regarde
   défiler « motif uart500 trame <index> » jusqu'à
   « motif uart500 fini : 1000 trames, ligne au repos haut (GPIO7 bas) » (environ
   140 s ; 45 s pour 2 400 bauds, 26 s pour 9 600, 57 s pour `wtc`, 134 s pour
   `krona`). Si le générateur redémarre en cours de route (sa bannière
   « generateur ... » réapparaît), on refait le motif avec un nouvel
   enregistrement.
3. À la fin de l'enregistrement, l'agent lance :

   ```bash
   F=$(ls -t logs/*-banc-v1-uart500.jsonl | head -1)
   python3 tools/banc.py "$F" uart500
   python3 tools/analyse.py auto "$F" --max 3
   python3 tools/json_check.py -q "$F"
   ```

   Attendu :
   - `banc.py` : « decodeur (auto, sans forcer) : uart 500 bauds : 0 erreurs / 6000 symboles (0.0 %) ; attendu : uart 500 bauds : OK », « critere 1 : 1000 / 1000 trames decodees au bit pres (100.0 %) : OK », « compteurs de la sonde pendant la capture : debord +0, lignes_perdues +0, sautes +0 » ; la ligne `critere 2` peut être en ÉCHEC tant que le décalage n'est pas corrigé (étape 5) : noter l'écart moyen des hauts et des bas, l'écart max et le « decalage suggere » ;
   - `analyse.py auto` : l'hypothèse « 1. » est la même (`uart 500 bauds`), avec 0 erreur ;
   - `json_check.py` : 0 erreur, et 0 trou de `n`.
4. L'agent recopie au §7 (« Critères 1 et 2, variante 1 ») : fichier, trames
   reconnues, décodeur, écarts, décalage suggéré, verdict du critère 1.

Décodeurs attendus : `uart500` → `uart 500 bauds` ; `uart500inv` →
`uart 500 bauds inverse` ; de même à 2 400 et 9 600 bauds ; `wtc` →
`distance_impulsion T=<environ 750> us` (T est estimé) ; `krona` →
`uart 500 bauds inverse`. Pour `krona`, la sonde coupe les trames d'index 0, 1,
8, 9... entre deux octets (22 ou 20 ms de repos dans la trame, plus que le
seuil de 19 ms) : `banc.py` compte donc plus de réceptions que de trames, les
reconnaît quand même toutes sur le flux d'octets (`critere 1 : 1000 / 1000`) et
n'en compare que les trois quarts en durées : c'est attendu.

En cas d'ÉCHEC du critère 1 : noter les `manquantes`, les compteurs et les
réceptions incomplètes ; refaire une fois le même motif ; si l'échec revient, on
s'arrête et on cherche la cause (capture, câblage, générateur) avant tout autre
motif.

- [ ] **Étape 5 : Critère 2 sans FX2 : décalage retenu**

1. Prendre la médiane des 8 « decalage suggere » de l'étape 4, arrondie à
   0,5 µs : c'est `N`, l'asymétrie de toute la chaîne (NPN du générateur, ligne,
   étage d'écoute).
2. Relancer `banc.py` sur les 8 captures avec la correction :

   ```bash
   for m in uart500 uart500inv uart2400 uart2400inv uart9600 uart9600inv wtc krona; do
     F=$(ls -t logs/*-banc-v1-$m.jsonl | head -1)
     echo "== $m"; python3 tools/banc.py "$F" $m --decalage-us N | grep -E "critere|verdict"
   done
   ```

   (remplacer `N` par la valeur trouvée). Attendu pour chaque motif :
   « critere 2 : ecart max <e> us <= 2 us : OK » et « verdict : OK ».
3. Noter `N` et l'écart max après correction au §7 (« Décalage retenu »). Si
   `|N|` atteint 5 µs, l'asymétrie est **documentée et compensée** (critère 2 de
   la spec) : le noter en conclusion. Si un motif garde un écart max au-delà de
   2 µs après correction, noter lequel et ses écarts par niveau : c'est une limite
   de la capture à analyser, pas un réglage à forcer.

- [ ] **Étape 6 : Critère 3, rafale, variante 1**

1. L'agent lance :

   ```bash
   python3 tools/serie_enregistre.py $PORT_SONDE banc-v1-rafale "capture tout" "seuils 1 5000" --duree 40
   ```

2. Dès « id=3 seuils 1 5000 : ok », Majid tape `motif rafale` : « motif rafale
   trame 0 », puis, environ 11 s plus tard, « motif rafale fini : 1 trames ... ».
3. L'agent lance :

   ```bash
   F=$(ls -t logs/*-banc-v1-rafale.jsonl | head -1)
   python3 tools/banc.py "$F" rafale --decalage-us N
   ```

   Attendu : « critere 3 : 1 / 1 rafales completes ; aucune perte : OK (10000
   fronts/s tenus) », « durees : 1 trames comparees (99999 durees) »,
   « critere 2 : ... : OK », hausses des compteurs à +0.
   Autres issues :
   - « perte signalee (debord ou compteurs) : OK, debit maximal < 10000 fronts/s » : la spec l'accepte ; noter quels compteurs ont bougé (`debord` : anneau de la capture plein ; `sautes` : plafond de 100 lignes `trame` par seconde ; `lignes_perdues` : sortie USB saturée) ;
   - « perte non signalee ... : ECHEC » : une partie manque sans que rien ne le dise ; on s'arrête et on cherche la cause ;
   - un écart max de plus de 2 µs sur un seul palier toutes les 50 périodes trahirait un trou de la boucle matérielle du générateur, pas de la sonde : le noter.
4. Noter la ligne au §7 (« Critère 3 : rafale »).

- [ ] **Étape 7 : Variante 2 (4,7k + 1 nF)**

1. Débrancher les deux câbles USB (Majid ferme d'abord la console du générateur).
2. Majid remplace le pull-up de 10k par 4,7k et ajoute 1 nF entre la LIGNE et la
   masse ; mêmes contrôles hors tension qu'à l'étape 1 (la hotte reste
   débranchée si la sonde est sur le boîtier).
3. Rebrancher les deux USB (mêmes prises : mêmes ports), rouvrir la console du
   générateur, mesurer la LIGNE au repos (entre 4,2 et 5,0 V).
4. Refaire les étapes 4, 5 et 6 avec des scénarios `banc-v2-<motif>` au lieu de
   `banc-v1-<motif>`, et noter dans les tableaux « variante 2 ». Le décalage
   suggéré peut changer avec le 1 nF (montée plus lente de la ligne) : on retient
   un `N` par variante.

- [ ] **Étape 8 : Critère 6, consommation de la sonde par l'USB**

Méthode de `docs/BANC.md` §2.4, la même qu'à la tâche 21 (Wi-Fi actif) :
- **Testeur USB** (si Majid en a un) entre le port du Mac et le câble USB de la
  sonde : lire le courant de la sonde, générateur à l'arrêt (`stop`), puis
  pendant `motif krona 0` (sans fin), puis taper `stop`.
- **Sinon, multimètre en série sur VBUS**, vers la broche `5V` de la sonde :
  1. débrancher les deux USB ; **l'USB-C de la sonde reste débranché pendant
     toute la mesure** (jamais deux sources sur sa broche `5V`) ;
  2. multimètre en courant continu, **calibre 200 mA** : borne mA sur le `5V`
     du générateur (le VBUS de son USB), COM sur la broche `5V` de la sonde ; la
     masse est déjà commune (fil de masse) ; sur le boîtier, `J2` reste ouvert ;
  3. brancher l'USB du générateur seul : la sonde démarre par sa broche `5V`,
     capture active ; lire le courant au repos, puis pendant `motif krona 0`,
     puis `stop` ; si le courant saute sans cesse, passer sur l'entrée 10 A ;
  4. débrancher l'USB du générateur, retirer le multimètre : plus rien ne relie
     les deux `5V`.

Attendu : quelques dizaines de mA, sans Wi-Fi (ordre de grandeur 20 à 50 mA).
Noter au §7 (« Critère 6 ») avec l'instrument utilisé. La ligne « Wi-Fi actif »
reste vide : tâche 21.

- [ ] **Étape 9 : Conclusions dans `docs/BANC.md`**

Remplir au §7 : « Montage et firmwares » (lignes `16` : dates, où est la sonde,
contrôles OL s'il s'agit du boîtier, `fw` de chaque carte, lu dans le `hello`
d'une capture :
`grep -o '"fw":"[^"]*"' "$F" | head -1`,
et dans la bannière du générateur), les tableaux des critères 1, 2 et 3 pour les
deux variantes, le décalage retenu, la consommation, et les lignes de
« Conclusions » : débit maximal de la capture, décalage et asymétrie retenus,
limites constatées (la ligne de l'analyseur attend la tâche 16b). Rien de
secret : le dépôt est public.

Puis vérifier que le tableau du §4 n'a pas été abîmé :

```bash
python3 -m unittest discover -s tools/tests -p test_banc.py
```

Attendu : « OK ».

- [ ] **Étape 10 : Fin de séance**

Débrancher les USB (Majid ferme d'abord la console du générateur). Si la sonde
était sur le boîtier de mesure et que le boîtier retourne sur la hotte avant la
prochaine séance du banc : banc démonté, contrôles du boîtier (WIRING §12,
lignes 11 à 17), puis fiche XH 4 broches remise, hotte débranchée
(`docs/BANC.md` §2.1). Sinon, le boîtier reste détaché.

- [ ] **Étape 11 : Commit**

```bash
git add docs/BANC.md
git commit -m "Consigner les resultats du banc par l'USB" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 16b : [BANC avec Majid] Critère 2 à l'analyseur

Exécutée par l'agent principal avec Majid, jamais par un sous-agent, **à
l'arrivée du FX2**, après la tâche 16 (elle en reprend le montage et le décalage
retenu `N` de chaque variante) ; elle peut venir après les tâches 21 et 24. Aucun
secteur, aucune liaison avec la hotte (`docs/BANC.md` §2.1). Elle mesure ce que
la tâche 16 ne pouvait pas séparer : le délai et l'asymétrie de l'étage d'écoute
seul (critère 2 du §10 de la spec), une voie de l'analyseur sur la LIGNE à
travers 47k/68k, une sur GPIO6. Le FX2 est numérique : il donne des instants,
pas des tensions ; un relevé chiffré des niveaux reste optionnel, à
l'oscilloscope. Cette tâche n'écrit que des résultats, dans le tableau
« Critère 2 à l'analyseur (tâche 16b) » que la tâche 15 a préparé.

**Fichiers :**
- Modifier : `docs/BANC.md` (§7 : « Montage et firmwares », lignes `16b` ; « Critère 2 à l'analyseur (tâche 16b) » et sa ligne `tol_us` ; « Conclusions ») ; `docs/RECONNAISSANCE.md` (avenant de l'étape 6 : `tol_us`, seulement s'il est déjà écrit)
- Tester : sessions PulseView `logs/*-banc-v1-fx2-*.sr` et `logs/*-banc-v2-fx2-*.sr`, captures de la sonde `logs/*-banc-v1-fx2-*.jsonl` et `logs/*-banc-v2-fx2-*.jsonl` (ignorées par git), `tools/banc.py`

**Interfaces :**
- Consomme : montage et firmwares de la tâche 16 (`docs/BANC.md` §2.1, §2.2 ; aucun flash ici) ; `docs/BANC.md` §2.3 (voies, diviseur, réglages de PulseView, décodeur « Jitter ») et le tableau « Critère 2 à l'analyseur (tâche 16b) » du §7 (tâche 15) ; `N` de chaque variante (§7, « Décalage retenu », tâche 16) ; `tools/serie_enregistre.py` (tâche 12) ; `tools/banc.py <capture> <motif> --n 20 --decalage-us N` (tâche 15) ; console du générateur (`motif uart9600 20`, `motif wtc 20`, `stop`, `etat` : tâche 14) ; analyseur FX2 et PulseView (pilote `fx2lafw`).
- Produit : délais montant et descendant de l'étage d'écoute et leur asymétrie, par variante ; la comparaison avec `N` ; `tol_us` proposé pour l'injection (délai maximal mesuré + marge), dans `docs/BANC.md` §7 et dans l'avenant de l'étape 6.

**Ce que l'agent fait, ce que Majid fait.** Majid câble, règle PulseView, tape
les commandes du générateur et lit les délais dans PulseView. L'agent lance les
enregistrements de la sonde et `banc.py`, calcule moyennes, maxima et
asymétries à partir des valeurs que Majid lui donne, et remplit `docs/BANC.md`.

- [ ] **Étape 1 : Montage, variante 1, analyseur posé, hors tension**

1. Tous les USB débranchés (sonde, générateur, analyseur). Montage de la tâche
   16, étape 1 (`docs/BANC.md` §2.1 et §2.2), variante 1 (10k). Sonde sur le
   boîtier : hotte débranchée pendant toute la séance, fiche XH 4 broches
   retirée, et les deux contrôles **OL** (`TP−` vers la terre de la fiche de la
   hotte, puis vers la vis de la carcasse), avant tout USB.
2. Analyseur, selon `docs/BANC.md` §2.3 : **sa masse d'abord**, sur `TP−` ;
   puis 47k de la LIGNE à un point libre, 68k de ce point à `TP−`, et D0 sur ce
   point ; D1 sur GPIO6 de la sonde ; D2, facultatif, sur GPIO7 du
   générateur.

Contrôles, toujours hors tension, avant tout USB :

| Contrôle | Attendu | Sinon |
|---|---|---|
| D0 (visuel) | sur le point milieu du diviseur : 47k vers la LIGNE, 68k vers `TP−` | jamais la LIGNE en direct sur une voie : revoir le diviseur |
| D1 (visuel) | sur GPIO6 de la sonde, et sur rien d'autre | revoir le câblage |
| D2, s'il est posé (visuel) | sur GPIO7 du générateur ; aucune voie sur GPIO7 de la sonde | revoir le câblage |
| LIGNE vers `GND` (ohmmètre) | plus de 10 kΩ | court-circuit : revoir le câblage avant tout USB |

- [ ] **Étape 2 : Brancher et régler**

1. Brancher la sonde, le générateur, puis l'analyseur (mêmes prises qu'à la
   tâche 16 : mêmes ports). Pas de flash : les firmwares de la tâche 16.
2. Majid ouvre la console du générateur
   (`~/.platformio/penv/bin/pio device monitor -e generateur -p $PORT_GEN`),
   tape `etat` : « etat : arret, ligne relachee (GPIO7 bas) ».
3. Majid mesure la LIGNE au repos, COM sur `TP−` : entre 4,0 et 4,7 V (le
   diviseur la charge). Il la note au §7 (« Montage et firmwares », ligne
   `16b`, variante 1).
4. PulseView : pilote `fx2lafw`, voies D0, D1 (et D2), 12 MHz (24 MHz si la
   capture passe sans erreur), 20 M échantillons à 12 MHz (40 M à 24 MHz),
   déclenchement sur un front descendant de D0.

- [ ] **Étape 3 : `uart9600`, variante 1**

1. L'agent lance l'enregistrement de la sonde :

   ```bash
   python3 tools/serie_enregistre.py $PORT_SONDE banc-v1-fx2-uart9600 "capture tout" "seuils 1 5000" --duree 20
   ```

2. Dès « id=3 seuils 1 5000 : ok », Majid lance la capture de PulseView (elle
   attend le déclenchement), puis tape `motif uart9600 20` dans la console du
   générateur. La capture s'arrête seule. Il l'enregistre sous
   `logs/AAAA-MM-JJ-banc-v1-fx2-uart9600.sr`.
3. Majid ajoute les décodeurs « Jitter » du §2.3 (front descendant de la LIGNE :
   Clock D0 `falling`, Resulting signal D1 `rising` ; front montant : D0
   `rising`, D1 `falling` ; avec D2, les deux rangées de Q3) et donne à l'agent
   les 20 premières valeurs de chaque rangée (à défaut, 20 mesures aux
   curseurs, de chaque sens).
4. L'agent calcule, pour chaque sens, la moyenne et le maximum des 20 délais,
   puis l'asymétrie de l'étage = moyenne descendante − moyenne montante (et
   celle de Q3, avec D2). Attendu : des délais de l'ordre de 1 à 3 µs (WIRING
   §5 : l'étage commute en 1 à 2 µs), une asymétrie inférieure à 5 µs.
5. À la fin de l'enregistrement, l'agent lance (`N1` : le décalage retenu en
   variante 1, tâche 16) :

   ```bash
   F=$(ls -t logs/*-banc-v1-fx2-uart9600.jsonl | head -1)
   python3 tools/banc.py "$F" uart9600 --n 20 --decalage-us N1
   ```

   Attendu : « critere 1 : 20 / 20 trames decodees au bit pres (100.0 %) : OK »
   (le diviseur ne gêne pas la capture) et un « decalage suggere » à 1 µs près
   de celui que la tâche 16 a noté pour ce motif (§7, « Critères 1 et 2,
   variante 1 »). Sinon : noter les deux valeurs, et chercher ce qui charge la
   LIGNE.

- [ ] **Étape 4 : `wtc`, variante 1**

Comme l'étape 3, avec le scénario `banc-v1-fx2-wtc`, `seuils 1 5000`,
`--duree 20`, la commande `motif wtc 20` et la session
`logs/AAAA-MM-JJ-banc-v1-fx2-wtc.sr` :

```bash
python3 tools/serie_enregistre.py $PORT_SONDE banc-v1-fx2-wtc "capture tout" "seuils 1 5000" --duree 20
```

puis, à la fin de l'enregistrement :

```bash
F=$(ls -t logs/*-banc-v1-fx2-wtc.jsonl | head -1)
python3 tools/banc.py "$F" wtc --n 20 --decalage-us N1
```

Attendu : « critere 1 : 20 / 20 trames decodees au bit pres (100.0 %) : OK »,
des délais du même ordre qu'avec `uart9600`.

- [ ] **Étape 5 : Variante 2 (4,7k + 1 nF)**

1. Majid ferme la console du générateur et arrête PulseView ; débrancher tous
   les USB.
2. Majid remplace le pull-up de 10k par 4,7k et ajoute 1 nF entre la LIGNE et
   la masse ; mêmes contrôles hors tension qu'à l'étape 1 (la hotte reste
   débranchée si la sonde est sur le boîtier).
3. Refaire les étapes 2 à 4 avec les scénarios `banc-v2-fx2-uart9600` et
   `banc-v2-fx2-wtc`, les sessions `...-banc-v2-fx2-*.sr` et `N2`, le décalage
   retenu en variante 2. LIGNE au repos : entre 4,0 et 4,7 V. En variante 2, le
   délai montant peut paraître plus court de 2 à 3 µs, parfois négatif : D0
   franchit son seuil après l'étage sur une montée lente (`docs/BANC.md` §2.3).
   C'est attendu : l'étage seul se lit en variante 1.

- [ ] **Étape 6 : Verdicts et `tol_us`**

1. **Asymétrie** de l'étage, par variante : inférieure à 5 µs → critère 2
   rempli sur ce point ; sinon, elle est documentée au §7 et compensée par
   `--decalage-us` (critère 2 de la spec).
2. **Comparaison avec `N`** (tâche 16, toute la chaîne) : `N` ≈ asymétrie de
   l'étage + asymétrie de Q3 (`docs/BANC.md` §2.3). Avec D2, l'égalité doit
   tenir à 1 µs près (le quantum de la capture) ; sans D2, `N` et l'asymétrie de
   l'étage sont du même ordre. Un écart plus grand est noté en « Remarque » et
   dans les limites constatées.
3. **`tol_us`** : plus grand délai mesuré (les deux sens, les deux motifs, les
   deux variantes ; plus le plus grand délai de Q3 si D2 est posé, image de
   l'étage d'injection) + 10 µs de marge (étage d'injection, latence de
   l'interruption de la sonde), arrondi au µs supérieur. S'il reste sous
   20 µs, on garde les 20 µs par défaut.

- [ ] **Étape 7 : Noter les résultats**

1. Dans `docs/BANC.md` §7 :
   - « Montage et firmwares » : les deux lignes `16b` (date, où est la sonde,
     contrôles OL s'il s'agit du boîtier, `fw` inchangés, LIGNE au repos) ;
   - « Critère 2 à l'analyseur (tâche 16b) » : analyseur et fréquence
     d'échantillonnage, instrument des niveaux (« aucun » sans oscilloscope),
     les quatre lignes du tableau, la case de l'asymétrie, la ligne
     **Pour l'injection (`tol_us`)** ;
   - « Conclusions » : la ligne « Délai de l'étage à l'analyseur, et `tol_us`
     proposé (tâche 16b) ».
2. Si l'avenant de l'étape 6 est déjà écrit (`docs/RECONNAISSANCE.md`), y
   reporter `tol_us` ; sinon, il le reprendra du §7.
3. Vérifier que le tableau du §4 n'a pas été abîmé :

   ```bash
   python3 -m unittest discover -s tools/tests -p test_banc.py
   ```

   Attendu : « OK ».

- [ ] **Étape 8 : Fin de séance**

Majid ferme la console du générateur et PulseView. Débrancher les USB
(analyseur, générateur, sonde), puis retirer les pinces de l'analyseur et le
diviseur. Si la sonde était sur le boîtier de mesure et qu'il retourne sur la
hotte : banc démonté, contrôles du boîtier (WIRING §12, lignes 11 à 17), puis
fiche XH 4 broches remise, hotte débranchée.

- [ ] **Étape 9 : Commit**

```bash
git add docs/BANC.md docs/RECONNAISSANCE.md
git commit -m "Consigner le critere 2 du banc a l'analyseur" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
