# Plan de la sonde, partie 3 : firmware-base

> En-tête, contraintes globales, carte des fichiers et interfaces : [../2026-09-27-reconnaissance-sonde.md](../2026-09-27-reconnaissance-sonde.md). Les tâches s'exécutent dans l'ordre des numéros (7 à 10).

### Tâche 7 : Projet PlatformIO, version, configuration, réglages NVS et bornes de capture

**Fichiers :**
- Créer : `platformio.ini`
- Créer : `src/fw_version.h` (adapté de la ScreenBar)
- Créer : `src/app_desc.c` (copie adaptée de la ScreenBar)
- Créer : `tools/git_rev.py` (copie de la ScreenBar)
- Créer : `src/config.h`
- Créer : `src/capture_model.h` (interface complète du contrat)
- Créer : `src/capture_model.cpp` (bornes seulement, la suite en tâche 9)
- Créer : `src/reglages.h`
- Créer : `src/reglages.cpp`
- Créer : `src/main.cpp`
- Créer : `tools/tests/verif.h`
- Créer : `tools/tests/test_capture.cpp`
- Créer : `tools/tests/test_hote.sh`
- Modifier : `docs/SPEC-RECONNAISSANCE.md` (lien d'en-tête figé sur `c58a506`, `rev` = 4 au §8.5, prérequis du §8.6 levé, ligne du §13 ; décisions de la relecture du plan : §1, §5 règle 2, §6 étapes 3a, 4, 5b et 7, §7.1, §7.2, §10, §12, §13)
- Tester : `tools/tests/test_capture.cpp` (par `sh tools/tests/test_hote.sh`), puis `~/.platformio/penv/bin/pio run -e sonde`

**Écart au découpage (assumé) :** `reglagesCharger` (cette tâche) appelle `capt::borner`, et `json_out` (tâche 8) utilise `capt::Partie`. L'en-tête `src/capture_model.h` est donc posé en entier dès cette tâche, et les quatre fonctions de bornes y sont implémentées et testées. `tools/tests/verif.h` et `tools/tests/test_capture.cpp` naissent ici, et non en tâches 8 et 9. Le découpeur et le mode changements restent en tâche 9.

**Interfaces :**
- Consomme : le dépôt `benq` au commit `c58a506`, en lecture seule (`src/fw_version.h`, `src/app_desc.c`, `tools/git_rev.py`, drapeaux `build_src_flags` de `platformio.ini`).
- Produit :
  - `src/config.h` du contrat, à la lettre (broches, bornes de capture, `kNvsEspace` = `"hotte"`, `kNomMdns`, `kPortUdp`).
  - `src/capture_model.h` du contrat, en entier. Implémentés ici : `bool capt::resolValide(uint32_t hz)` (1 000 000 ou 500 000), `bool capt::filtreValide(uint32_t us)` (0..3), `bool capt::silenceValide(uint32_t us, uint32_t resolHz)` (1 000 µs au moins, `us × resolHz ≤ 32 767 × 10⁶`), `uint8_t capt::borner(capt::Reglages *r)` (résolution d'abord, puis filtre, puis silence jugé à la résolution retenue ; rend le nombre de valeurs remplacées).
  - `src/reglages.h` du contrat : `struct ReglagesSonde { capt::Reglages capture; bool changements = false; };`, `uint8_t reglagesCharger(ReglagesSonde *r)`, `bool reglagesSauver(const ReglagesSonde &r)`. Clés `resol_hz` (u32), `filtre_us` (u8), `silence_us` (u32), `inverse` (bool), `chgt` (bool) dans l'espace `hotte`, ouvert en écriture même pour lire, `isKey()` avant chaque lecture (comme la ScreenBar : pas d'erreur NVS au premier démarrage).
  - `src/fw_version.h` : `FW_VERSION` = `"0.1.0"`, `FW_GIT_REV`, `FW_ENV`, `FW_VERSION_FULL` ; `esp_app_desc.project_name` = `"hotte-sonde"`, `esp_app_desc.version` = `FW_VERSION_FULL`.
  - `platformio.ini` : tout le commun dans `[env]` (plateforme, carte, drapeaux, `build_src_flags`), `[env:sonde]` vide et env par défaut. La tâche 14 ajoute `[env:generateur]` et les `build_src_filter`.
  - `tools/tests/verif.h` (texte de `01-taches.md`) : `VERIF(c)`, `VERIF_EGAL_STR(a, b)`, `static int bilan(const char *nom)`, qui affiche « `<nom> : N verifications, M echecs` ».
  - `tools/tests/test_hote.sh` : chaque test C++ y a deux lignes (compilation par `$CXX`, exécution), avant la ligne finale `echo "tests hote : OK"`.
  - `src/main.cpp` : `static ReglagesSonde sReglages;` ; `setup()` commence par `pinMode(kPinInjection, OUTPUT); digitalWrite(kPinInjection, LOW);`, puis `Serial.begin(115200);`, `reglagesCharger(&sReglages)` et la bannière `firmware <FW_VERSION_FULL> (<FW_ENV>)` ; `loop()` finit par `vTaskDelay(1);`.
  - `docs/SPEC-RECONNAISSANCE.md` à jour : le commit de référence `c58a506` (étape 9), puis les décisions prises à la relecture du plan (étape 10). Le banc n'a jamais de liaison avec la hotte, et le boîtier de mesure se détache du câble de sortie par une fiche JST XH 4 broches. Le niveau bas de `D` est jugé fonctionnellement : le FX2 ne mesure pas de tension, l'oscilloscope reste optionnel. S'y ajoutent la formulation de la règle 2, les seuils des étapes 3a et 4, l'ordre de sortie de l'étape 4 et le condensateur de 1 nF aux achats.

- [ ] **Étape 1 : Écrire le test qui échoue**

Le mini-cadre commun des tests C++, puis les tests des bornes, puis le script qui lance tous les tests hôte.

Créer `tools/tests/verif.h` :

```cpp
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
static int gEchecs = 0, gVerifs = 0;
#define VERIF(c) do { gVerifs++; if (!(c)) { gEchecs++; std::printf("ECHEC %s:%d : %s\n", __FILE__, __LINE__, #c); } } while (0)
#define VERIF_EGAL_STR(a, b) VERIF(std::strcmp((a), (b)) == 0)
static int bilan(const char *nom) {
  std::printf("%s : %d verifications, %d echecs\n", nom, gVerifs, gEchecs);
  return gEchecs ? 1 : 0;
}
```

Créer `tools/tests/test_capture.cpp` :

```cpp
// Tests hote de src/capture_model.* : bornes des reglages de capture.
// Lancer : sh tools/tests/test_hote.sh
#include "capture_model.h"
#include "config.h"
#include "verif.h"

using namespace capt;

static void testBornes() {
  VERIF(resolValide(1000000));
  VERIF(resolValide(500000));
  VERIF(!resolValide(0));
  VERIF(!resolValide(250000));
  VERIF(!resolValide(2000000));
  VERIF(filtreValide(0));
  VERIF(filtreValide(3));
  VERIF(!filtreValide(4));
  VERIF(!filtreValide(255));
  // Silence : 1000 us au moins, 32767 ticks au plus (32767 us a 1 MHz, 65534 us a 500 kHz).
  VERIF(silenceValide(1000, 1000000));
  VERIF(!silenceValide(999, 1000000));
  VERIF(silenceValide(32767, 1000000));
  VERIF(!silenceValide(32768, 1000000));
  VERIF(silenceValide(65534, 500000));
  VERIF(!silenceValide(65535, 500000));
  VERIF(!silenceValide(999, 500000));
  VERIF(!silenceValide(0, 1000000));
  VERIF(!silenceValide(4294967295u, 500000));  // produit sur 64 bits : pas de debordement
}

static void testBorner() {
  Reglages r;
  VERIF(r.resolHz == kResolDefautHz && r.filtreUs == kFiltreDefautUs && r.silenceUs == kSilenceDefautUs && r.inverse);
  VERIF(borner(&r) == 0);
  r.resolHz = kResolLenteHz;
  r.filtreUs = 3;
  r.silenceUs = 65534;
  r.inverse = false;
  VERIF(borner(&r) == 0 && r.resolHz == kResolLenteHz && r.filtreUs == 3 && r.silenceUs == 65534 && !r.inverse);
  // Silence valable a 500 kHz mais pas a 1 MHz : remplace par sa valeur par defaut.
  r.resolHz = kResolDefautHz;
  VERIF(borner(&r) == 1 && r.silenceUs == kSilenceDefautUs && r.filtreUs == 3);
  // Tout hors bornes (NVS corrompue) : trois valeurs remplacees.
  r.resolHz = 123;
  r.filtreUs = 200;
  r.silenceUs = 5;
  VERIF(borner(&r) == 3 && r.resolHz == kResolDefautHz && r.filtreUs == kFiltreDefautUs &&
        r.silenceUs == kSilenceDefautUs);
  // Resolution remplacee d'abord : le silence est juge avec la resolution par defaut.
  r.resolHz = 0;
  r.silenceUs = 40000;
  VERIF(borner(&r) == 2 && r.resolHz == kResolDefautHz && r.silenceUs == kSilenceDefautUs);
}

int main() {
  testBornes();
  testBorner();
  return bilan("test_capture");
}
```

Créer `tools/tests/test_hote.sh` :

```sh
#!/bin/sh
# Tests hote de la sonde (sans carte). A lancer depuis n'importe ou : sh tools/tests/test_hote.sh
set -e
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/hotte-tests"
mkdir -p "$OUT"
CXX="clang++ -std=c++17 -Wall -Wextra -Werror -Isrc"
# Une commande par ligne : sous 'set -e', un echec a gauche d'un '&&' passerait inapercu.
$CXX src/capture_model.cpp tools/tests/test_capture.cpp -o "$OUT/test_capture"
"$OUT/test_capture"
echo "tests hote : OK"
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`

Attendu : ÉCHEC avec « clang++: error: no such file or directory: 'src/capture_model.cpp' »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

`src/config.h` est celui du contrat, avec un en-tête de commentaire. `src/capture_model.h` est celui du contrat, à la lettre. `src/capture_model.cpp` n'implémente que les bornes.

Créer `src/config.h` :

```cpp
#pragma once
// ===========================================================================
//  Constantes de la sonde : broches du C6 SuperMini, valeurs par defaut et
//  bornes de la capture, noms de la NVS et du reseau (docs/SPEC-RECONNAISSANCE.md
//  sections 7.5, 8.2 et 8.5). Pur : inclus aussi par les tests hote.
// ===========================================================================
#include <stdint.h>

constexpr uint8_t kPinEcoute = 6;      // sortie de l'etage d'ecoute (voie 1)
constexpr uint8_t kPinInjection = 7;   // base de l'etage d'injection (voie 1)
constexpr uint8_t kPinGenerateur = 7;  // sortie du generateur (second C6)

constexpr uint32_t kResolDefautHz = 1000000;
constexpr uint32_t kResolLenteHz = 500000;
constexpr uint8_t kFiltreDefautUs = 1;
constexpr uint8_t kFiltreMaxUs = 3;
constexpr uint32_t kSilenceDefautUs = 5000;
constexpr uint32_t kSilenceMinUs = 1000;
constexpr uint32_t kSilenceMaxTicks = 32767;
constexpr uint32_t kTamponSymboles = 2048;
constexpr uint16_t kTramesParSeconde = 100;

constexpr const char *kNvsEspace = "hotte";
constexpr const char *kNomMdns = "hotte-sonde";
constexpr uint16_t kPortUdp = 5480;
```

Créer `src/capture_model.h` :

```cpp
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace capt {

constexpr uint16_t kDurMax = 110;  // durees par partie (ligne trame)

// Miroir pur de rmt_symbol_word_t : deux demi-symboles (duree en ticks sur 15 bits, niveau GPIO).
struct Sym {
  uint16_t d0;
  uint8_t l0;
  uint16_t d1;
  uint8_t l1;
};

struct Reglages {
  uint32_t resolHz = 1000000;  // 1000000 ou 500000
  uint8_t filtreUs = 1;        // 0..3
  uint32_t silenceUs = 5000;   // 1000 .. kSilenceMaxTicks ticks
  bool inverse = true;         // etage d'ecoute inverseur
};

bool resolValide(uint32_t hz);
bool filtreValide(uint32_t us);
bool silenceValide(uint32_t us, uint32_t resolHz);
// Reglages bornes : toute valeur hors bornes est remplacee par sa valeur par defaut.
// Rend le nombre de valeurs remplacees.
uint8_t borner(Reglages *r);

// Bloc brut depose par le rappel RMT (on_recv_done) dans le tampon circulaire.
struct Bloc {
  uint64_t tFinUs;       // esp_timer_get_time() dans le rappel
  bool dernier;          // edata->flags.is_last
  bool debordAvant;      // des symboles ont ete perdus juste avant ce bloc
  uint16_t nSym;
  const Sym *sym;
};

// Une partie publiee (ligne trame).
struct Partie {
  uint32_t num = 0;       // numero de reception (depuis le demarrage, a partir de 1)
  uint32_t part = 0;      // rang dans la reception (a partir de 0)
  bool fin = false;       // derniere partie de la reception
  uint64_t tUs = 0;       // debut de la partie
  bool niv0Haut = true;   // niveau DU BUS pendant dur[0]
  bool debord = false;    // perte juste avant cette partie
  uint16_t n = 0;         // nombre de durees
  uint32_t dur[kDurMax] = {};  // µs
};

typedef void (*EmetPartie)(const Partie &p, void *ctx);

// Transforme les blocs successifs en parties. Etat : reception en cours, rang, niveau courant.
class Decoupeur {
 public:
  void reset();
  // Convertit b (ticks -> µs selon r.resolHz, niveaux GPIO -> niveaux du bus si r.inverse),
  // retire les durees nulles, decoupe en parties de kDurMax durees au plus et appelle emit
  // pour chacune. tUs de la premiere partie d'un bloc = b.tFinUs - somme(durees du bloc)
  // - (b.dernier ? r.silenceUs : 0). Rend le nombre de parties emises.
  size_t traiter(const Bloc &b, const Reglages &r, EmetPartie emit, void *ctx);
  uint32_t receptions() const { return num_; }

 private:
  uint32_t num_ = 0, part_ = 0;
  bool enCours_ = false;
};

// |a-b| <= max(10, 5 % du plus grand).
bool procheDe(uint32_t a, uint32_t b);

// Mode changements : ne s'applique qu'aux receptions d'une seule partie (part 0 et fin).
class Changements {
 public:
  void reset();
  // true : p differe de la derniere reception emise (ou n'est pas comparable) et doit partir ;
  // false : identique, le compteur de repetitions avance.
  bool aEmettre(const Partie &p);
  // Repetitions a porter par la prochaine partie emise (puis remises a zero).
  uint32_t prendreRep();
  uint32_t repEnCours() const { return rep_; }

 private:
  bool a_ = false;
  bool niv0_ = true;
  uint16_t n_ = 0;
  uint32_t dur_[kDurMax] = {};
  uint32_t rep_ = 0;
};

}  // namespace capt
```

Créer `src/capture_model.cpp` :

```cpp
// ===========================================================================
//  Modele pur de la capture (docs/SPEC-RECONNAISSANCE.md section 8.2) : bornes
//  des reglages. Sans Arduino ni IDF : teste sur l'hote (tools/tests/test_capture.cpp).
// ===========================================================================
#include "capture_model.h"

#include "config.h"

namespace capt {

// ===========================================================================
//  Bornes
// ===========================================================================

bool resolValide(uint32_t hz) { return hz == kResolDefautHz || hz == kResolLenteHz; }

// Filtre du RMT : 255 cycles de 80 MHz au plus, soit 3,19 us.
bool filtreValide(uint32_t us) { return us <= kFiltreMaxUs; }

// Seuil de silence du RMT : kSilenceMaxTicks ticks au plus (registre sur 15 bits).
bool silenceValide(uint32_t us, uint32_t resolHz) {
  return us >= kSilenceMinUs && (uint64_t)us * resolHz <= (uint64_t)kSilenceMaxTicks * 1000000u;
}

uint8_t borner(Reglages *r) {
  const Reglages d;
  uint8_t n = 0;
  // La resolution d'abord : la borne du silence en depend.
  if (!resolValide(r->resolHz)) {
    r->resolHz = d.resolHz;
    n++;
  }
  if (!filtreValide(r->filtreUs)) {
    r->filtreUs = d.filtreUs;
    n++;
  }
  if (!silenceValide(r->silenceUs, r->resolHz)) {
    r->silenceUs = d.silenceUs;
    n++;
  }
  return n;
}

}  // namespace capt
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh`

Attendu :

```text
test_capture : 24 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 5 : Constater que le firmware ne compile pas encore**

Lancer : `~/.platformio/penv/bin/pio run -e sonde`

Attendu : ÉCHEC avec « NotPlatformIOProjectError: Not a PlatformIO project »

- [ ] **Étape 6 : Poser le projet et reprendre la version de la ScreenBar**

Le projet : un seul env pour l'instant, tout le commun dans `[env]`.

Créer `platformio.ini` :

```ini
; Hotte Haier CCHP21EBB : sonde de reconnaissance de la ligne D (sous-projet 1,
; docs/SPEC-RECONNAISSANCE.md), sur ESP32-C6 SuperMini.
;
; Meme plateforme que benq-screenbar-halo-matter (commit c58a506) : pioarduino
; 55.03.312-1, soit le coeur Arduino-ESP32 3.3.12 et IDF 5.5. PlatformIO ne
; garde qu'un coeur : les deux projets partagent le cache de paquets. Pas de
; Matter : table de partitions par defaut.

[platformio]
default_envs = sonde

[env]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312-1/platform-espressif32.zip
framework = arduino
monitor_speed = 115200
; log2file : ecrit toute la session serie dans platformio-device-monitor-*.log
; (ignore par git), pour relire une capture sans dependre du terminal.
monitor_filters = esp32_exception_decoder, time, log2file
; C6 SuperMini : pas de definition dans PlatformIO, on part de la DevKitC-1
; (meme puce) avec une flash de 4 Mo au lieu de 8.
board = esp32-c6-devkitc-1
board_upload.flash_size = 4MB
board_upload.maximum_size = 4194304
board_build.flash_size = 4MB
build_flags =
    -DCORE_DEBUG_LEVEL=1
    ; Serial sur l'USB Serial/JTAG materiel du C6, des le demarrage.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
; Version du firmware, pour src/ seulement (mecanisme de la ScreenBar) :
; FW_VERSION a la main, FW_GIT_REV par tools/git_rev.py (hash court, "-dirty"
; si l'arbre differe du commit), FW_ENV = nom de l'env ($PIOENV). Ensemble,
; "0.1.0-<commit>" : version affichee au demarrage, rapportee par hello.fw et
; par le descripteur d'application (src/app_desc.c, hello.fw_desc). Les envs
; qui ont leur propre build_src_flags doivent reprendre ${env.build_src_flags}.
build_src_flags =
    -DFW_VERSION='"0.1.0"'
    -DFW_ENV='"${PIOENV}"'
    !python3 tools/git_rev.py

; ---------------------------------------------------------------------------
; Sonde : ecoute de la ligne D sur GPIO6, injection sur GPIO7 (src/config.h).
; ---------------------------------------------------------------------------
[env:sonde]
```

`src/fw_version.h`, adapté (version 0.1.0 de la sonde, commentaires sans Matter) :

Créer `src/fw_version.h` :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/fw_version.h (adapte : version 0.1.0 de la sonde, commentaires)
#pragma once

// ===========================================================================
//  Version du firmware (C et C++ : app_desc.c l'inclut aussi)
//
//  FW_VERSION, FW_GIT_REV et FW_ENV viennent de platformio.ini
//  (build_src_flags, donc pour src/ seulement) : la version a la main, la
//  revision par tools/git_rev.py, le nom de l'env PlatformIO par $PIOENV. Les
//  valeurs ci-dessous ne servent qu'a une compilation hors PlatformIO.
//
//  0.1.0 : premiere sonde de la ligne D (protocole compagnon v1, profil hotte).
//
//  FW_VERSION_FULL ("0.1.0-903414e", "-dirty" si l'arbre differe du commit)
//  est la version affichee au demarrage et par 'info', et celle du descripteur
//  d'application (esp_app_desc.version, src/app_desc.c), que hello rapporte
//  en fw et fw_desc.
// ===========================================================================

#ifndef FW_VERSION
#define FW_VERSION "0.1.0"
#endif
#ifndef FW_GIT_REV
#define FW_GIT_REV "nogit"
#endif
#ifndef FW_ENV
#define FW_ENV "inconnu"  // env PlatformIO ("sonde", "generateur"), rapporte par hello.env
#endif
#define FW_VERSION_FULL FW_VERSION "-" FW_GIT_REV
```

`src/app_desc.c` et `tools/git_rev.py` : copies depuis le commit de référence, puis adaptations exactes.

```bash
git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:src/app_desc.c > src/app_desc.c
git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:tools/git_rev.py > tools/git_rev.py
```

Dans `src/app_desc.c`, remplacer :

```c
// ===========================================================================
//  Descripteur d'application (esp_app_desc_t) propre au projet
```

par :

```c
// Copie de benq-screenbar-halo-matter@c58a506 : src/app_desc.c (adapte : nom de projet hotte-sonde, commentaires)
// ===========================================================================
//  Descripteur d'application (esp_app_desc_t) propre au projet
```

Dans `src/app_desc.c`, remplacer :

```c
//  builder". Or la pile Matter rapporte esp_app_get_description()->version
//  comme SoftwareVersionString : c'est le "Programme interne" d'Apple Home.
```

par :

```c
//  builder". La sonde rapporte esp_app_get_description()->version dans
//  hello.fw_desc, qui doit etre egal a hello.fw (FW_VERSION_FULL).
```

Dans `src/app_desc.c`, remplacer :

```c
    .project_name = "benq-halo",
```

par :

```c
    .project_name = "hotte-sonde",
```

Dans `tools/git_rev.py`, remplacer :

```python
#!/usr/bin/env python3
```

par :

```python
#!/usr/bin/env python3
# Copie de benq-screenbar-halo-matter@c58a506 : tools/git_rev.py
```

Lancer : `diff <(git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:tools/git_rev.py) tools/git_rev.py`

Attendu :

```text
1a2
> # Copie de benq-screenbar-halo-matter@c58a506 : tools/git_rev.py
```

Lancer : `diff <(git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:src/app_desc.c) src/app_desc.c`

Attendu :

```text
0a1
> // Copie de benq-screenbar-halo-matter@c58a506 : src/app_desc.c (adapte : nom de projet hotte-sonde, commentaires)
6,7c7,8
< //  builder". Or la pile Matter rapporte esp_app_get_description()->version
< //  comme SoftwareVersionString : c'est le "Programme interne" d'Apple Home.
---
> //  builder". La sonde rapporte esp_app_get_description()->version dans
> //  hello.fw_desc, qui doit etre egal a hello.fw (FW_VERSION_FULL).
37c38
<     .project_name = "benq-halo",
---
>     .project_name = "hotte-sonde",
```

- [ ] **Étape 7 : Réglages en NVS et `setup()` minimal**

Créer `src/reglages.h` :

```cpp
#pragma once
#include "capture_model.h"

struct ReglagesSonde {
  capt::Reglages capture;
  bool changements = false;   // mode d'emission
};
// Lit l'espace NVS kNvsEspace ; applique capt::borner ; rend le nombre de valeurs remplacees.
uint8_t reglagesCharger(ReglagesSonde *r);
bool reglagesSauver(const ReglagesSonde &r);
```

Créer `src/reglages.cpp` :

```cpp
// ===========================================================================
//  Reglages de la sonde en NVS (espace kNvsEspace, "hotte")
//
//  Cles : resol_hz (u32), filtre_us (u8), silence_us (u32), inverse (bool),
//  chgt (bool, mode changements). Une cle absente vaut sa valeur par defaut ;
//  une valeur hors bornes est remplacee par sa valeur par defaut au chargement
//  (capt::borner), et l'appelant le signale (spec 8.2).
// ===========================================================================
#include "reglages.h"

#include <Preferences.h>

#include "config.h"

uint8_t reglagesCharger(ReglagesSonde *r) {
  *r = ReglagesSonde();
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
    p.end();
  }
  return capt::borner(&r->capture);
}

bool reglagesSauver(const ReglagesSonde &r) {
  Preferences p;
  if (!p.begin(kNvsEspace, false)) return false;
  bool ok = p.putUInt("resol_hz", r.capture.resolHz) == sizeof(uint32_t);
  ok &= p.putUChar("filtre_us", r.capture.filtreUs) == 1;
  ok &= p.putUInt("silence_us", r.capture.silenceUs) == sizeof(uint32_t);
  ok &= p.putBool("inverse", r.capture.inverse) == 1;
  ok &= p.putBool("chgt", r.changements) == 1;
  p.end();
  return ok;
}
```

Créer `src/main.cpp` :

```cpp
// ===========================================================================
//  Sonde de reconnaissance de la ligne D (docs/SPEC-RECONNAISSANCE.md)
// ===========================================================================
#include <Arduino.h>

#include "config.h"
#include "fw_version.h"
#include "reglages.h"

static ReglagesSonde sReglages;

void setup() {
  // TOUJOURS la premiere instruction (spec 8.4) : la base de l'etage
  // d'injection a l'etat bas avant tout le reste.
  pinMode(kPinInjection, OUTPUT);
  digitalWrite(kPinInjection, LOW);
  Serial.begin(115200);
  const uint8_t remplaces = reglagesCharger(&sReglages);
  Serial.printf("firmware %s (%s)\n", FW_VERSION_FULL, FW_ENV);
  if (remplaces) Serial.printf("[reglages] %u valeur(s) hors bornes en NVS : valeur(s) par defaut\n", remplaces);
}

void loop() {
  vTaskDelay(1);  // laisse tourner la tache IDLE
}
```

- [ ] **Étape 8 : Compiler et vérifier la version**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|SUCCESS|FAILED"`

Attendu : une seule ligne, « `========================= [SUCCESS] Took ... seconds =========================` » (aucun `warning`)

Lancer : `python3 tools/git_rev.py`

Attendu : « `-DFW_GIT_REV='"<hash court>-dirty"'` » (arbre pas encore committé)

Lancer : `~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-nm .pio/build/sonde/firmware.elf | grep " esp_app_desc$"`

Attendu : une ligne en `R` (définition forte de `src/app_desc.c`), par exemple « `42030020 R esp_app_desc` »

Lancer : `strings .pio/build/sonde/firmware.bin | grep -c hotte-sonde`

Attendu : `1`

- [ ] **Étape 9 : Mettre à jour la spec (commit de référence `c58a506`)**

Quatre remplacements exacts dans `docs/SPEC-RECONNAISSANCE.md`.

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
- Le protocole compagnon de la ScreenBar :
  [benq-screenbar-halo-matter/docs/PROTOCOLE-JSON.md](https://github.com/Djoko-cli/benq-screenbar-halo-matter/blob/main/docs/PROTOCOLE-JSON.md).
  La version de référence sera figée au prérequis du §8.6.
```

par :

```markdown
- Le protocole compagnon de la ScreenBar, dans sa version de référence, figée
  au commit `c58a506` (§8.6) :
  [benq-screenbar-halo-matter/docs/PROTOCOLE-JSON.md](https://github.com/Djoko-cli/benq-screenbar-halo-matter/blob/c58a506/docs/PROTOCOLE-JSON.md).
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
| `hello` `base` | **tous** les champs de la ScreenBar. `rev` = 2. `build` = `sonde` (nouvelle valeur). `reseau_build` = `aucun` (pas de réseau Matter). `session.transport` = `usb` ou `udp` |
```

par :

```markdown
| `hello` `base` | **tous** les champs de la ScreenBar. `rev` = 4, celle de la ScreenBar au commit `c58a506`. `build` = `sonde` (nouvelle valeur). `reseau_build` = `aucun` (pas de réseau Matter). `session.transport` = `usb` ou `udp` |
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
**Prérequis.** Le transport réseau de la ScreenBar n'est pas encore committé dans
`benq` : c'est le cas au 25/09 de `h1_proto`, `h1_crypto`, `net_udp`,
`tools/halo_udp.py` et `test_h1.cpp`, ainsi que des modifications de `json_out`,
`json_mode`, `cli`, `json_check` et `PROTOCOLE-JSON.md`. Il doit être
**committé et poussé** avant toute copie. Ce commit de référence est noté :
```

par :

```markdown
**Prérequis, levé le 27/09/2026.** Le transport réseau de la ScreenBar (`h1_proto`,
`h1_crypto`, `net_udp`, `tools/halo_udp.py`, `test_h1.cpp`, et les modifications
de `json_out`, `json_mode`, `cli`, `json_check` et `PROTOCOLE-JSON.md`) est
**committé et poussé** dans `benq`. Le commit de référence est **`c58a506`**.
Toute copie se fait depuis ce commit (`git show c58a506:<chemin>`). Il est noté :
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
| transport réseau de la ScreenBar pas encore committé | prérequis du §8.6, à lever avant toute copie de code |
```

par :

```markdown
| transport réseau de la ScreenBar pas encore committé | levé : commit de référence `c58a506` (§8.6) |
```

Lancer : `grep -c "c58a506" docs/SPEC-RECONNAISSANCE.md; grep -c "sera figée au prérequis\|pas encore committé dans" docs/SPEC-RECONNAISSANCE.md`

Attendu : `6` puis `0`

- [ ] **Étape 10 : Mettre à jour la spec (décisions de la relecture du plan)**

Décisions prises le 27/09/2026 à la relecture de ce plan : le banc n'a jamais de liaison avec la hotte (§10), et le boîtier de mesure se détache du câble de sortie par une fiche JST XH 4 broches (§7.1). Le niveau bas de `D` est jugé fonctionnellement, parce que le FX2 est numérique et ne mesure pas de tension ; une mesure chiffrée reste optionnelle, à l'oscilloscope (§1, étapes 5b et 7, §7.2, §10 critère 2, §13). S'y ajoutent la règle 2 (différentiel testé juste avant de poser un montage), l'étape 3a (entre 3 et 5 V, on en rediscute), l'étape 4 (polarité de +4,5 à +5,3 V ; couvercle et filtre avant l'étape 2b) et le condensateur de 1 nF du banc aux achats (§12). Treize remplacements exacts dans `docs/SPEC-RECONNAISSANCE.md`.

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
   - couche physique : tension de `+`, niveau de repos, niveau bas de chaque
     émetteur, drain ouvert ou push-pull, pull-up et son côté ;
```

par :

```markdown
   - couche physique : tension de `+`, niveau de repos, niveau bas de chaque
     émetteur (jugé fonctionnellement à l'étape 5b, chiffré seulement si un
     oscilloscope est disponible ; l'instrument est noté), drain ouvert ou
     push-pull, pull-up et son côté ;
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
2. **Avant la première mise sous tension d'un montage, on teste le différentiel :**
   1. hotte branchée, lumière allumée, on appuie sur le bouton T du différentiel ;
   2. **la lumière doit s'éteindre.** Sinon, la hotte n'est pas derrière ce
      différentiel : **ARRÊT** ;
   3. on réarme.
```

par :

```markdown
2. **On teste le différentiel juste avant de poser chaque nouveau montage**,
   la hotte étant encore dans son état précédent, déjà contrôlé :
   1. hotte branchée, lumière allumée, on appuie sur le bouton T du différentiel ;
   2. **la lumière doit s'éteindre.** Sinon, la hotte n'est pas derrière ce
      différentiel : **ARRÊT** ;
   3. on réarme, puis on débranche la fiche pour poser le montage.
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
**Attendu :** 3 V au plus, soit 0,3 mA au plus. **Au-delà de 5 V (0,5 mA), ou si
le différentiel déclenche : ARRÊT.**
```

par :

```markdown
**Attendu :** 3 V au plus, soit 0,3 mA au plus. **Au-delà de 5 V (0,5 mA), ou si
le différentiel déclenche : ARRÊT.** Entre 3 et 5 V, on le note, et on en
rediscute avant de relier un Mac ou l'analyseur.
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
   4. mesurer `TP+` par rapport à `TP−` : **de +4,7 à +5,0 V, positif**. Sinon,
      **ARRÊT** ;
```

par :

```markdown
   4. mesurer `TP+` par rapport à `TP−` : **positif, entre +4,5 et +5,3 V** (un
      port USB peut monter à 5,25 V). Sinon, **ARRÊT** ;
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
**Sortie, dans cet ordre :**
1. débrancher l'USB ;
2. ouvrir `J2` ;
3. remettre la fiche de l'adaptateur sur `CN3` ;
4. refaire l'étape 2b ;
5. fermer le couvercle et remettre le filtre.
```

par :

```markdown
**Sortie, dans cet ordre :**
1. débrancher l'USB ;
2. ouvrir `J2` ;
3. remettre la fiche de l'adaptateur sur `CN3` ;
4. fermer le couvercle et remettre le filtre, en vérifiant qu'ils ne pincent
   rien ;
5. refaire l'étape 2b, qui se fait couvercle fermé et filtre remis.
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
  - **voie 2 :** elle sert à relever le **niveau bas de `D` pendant les trames
    de chaque émetteur** et son temps de montée. Sa tolérance est ce temps de
    montée, puisque les deux seuils diffèrent.
- **Si un niveau bas dépasse 0,6 V,** on relève le seuil de l'étage d'écoute
  (§7.2), puis on refait l'étape 5.
```

par :

```markdown
  - **voie 2 :** elle montre `D` lui-même pendant les trames de chaque
    émetteur, et son temps de montée. Ses durées suivent celles de la voie 1
    à ce temps de montée près, puisque les deux seuils diffèrent.
- **Niveau bas de `D` : jugé fonctionnellement.** Le FX2 est numérique : il ne
  mesure pas de tension. Le niveau bas convient si la capture est propre (ni
  front parasite ni `debord`) et si la sonde et l'analyseur concordent sur la
  voie 1. Une mesure chiffrée est **optionnelle**, à l'oscilloscope si Majid en
  a un : sous 0,6 V. `RECONNAISSANCE.md` note l'instrument qui a donné les
  niveaux.
- **Si la capture n'est pas propre, ou si un niveau bas mesuré dépasse 0,6 V,**
  on relève le seuil de l'étage d'écoute (§7.2), puis on refait l'étape 5.
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
  4. **Pendant les premiers essais, voie 2 de l'analyseur sur `TP_Dp`.** Le
     niveau bas obtenu par l'injection doit rester sous 0,8 V.
```

par :

```markdown
  4. **Pendant les premiers essais, voie 2 de l'analyseur sur `TP_Dp`.** Le
     niveau bas obtenu par l'injection est jugé fonctionnellement : la carte
     réagit (bip, lumière, voyant) et la sonde relit la trame émise. Une
     mesure chiffrée est optionnelle, à l'oscilloscope : sous 0,8 V.
     L'instrument qui a donné les niveaux est noté.
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
- **Câble de sortie.** 4 fils (`−`, `+`, `D_panneau`, `D_carte`), conforme à la
  règle 5, le long du cheminement choisi à l'étape 0.
```

par :

```markdown
- **Câble de sortie.** 4 fils (`−`, `+`, `D_panneau`, `D_carte`), conforme à la
  règle 5, le long du cheminement choisi à l'étape 0. Il se termine par une
  **fiche JST XH 4 broches** (ordre `−`, `+`, `D_panneau`, `D_carte`, avec
  détrompeur), branchée sur l'embase assortie du boîtier de mesure : le
  boîtier se **détache** du câble, hotte débranchée.
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
  reste sous 0,6 V environ. On le vérifie à l'étape 5b. Sinon, on relève le seuil
  par la résistance entre base et émetteur, `R_be` :
```

par :

```markdown
  reste sous 0,6 V environ. On le vérifie à l'étape 5b : fonctionnellement
  (capture propre, concordance avec l'analyseur), et en chiffre seulement si un
  oscilloscope est disponible. Sinon, on relève le seuil par la résistance entre
  base et émetteur, `R_be` :
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
**Montage :**
- **Fil de masse** entre le `GND` du générateur et le `−` de l'étage d'écoute de la
  sonde.
```

par :

```markdown
**Montage :**
- **Aucune liaison avec la hotte.** Le banc se monte :
  - soit sur le boîtier de mesure **détaché** : hotte débranchée pendant toute
    la séance, câble de sortie déconnecté du boîtier (fiche XH 4 broches,
    §7.1), et contrôle hors tension avant tout USB, à l'ohmmètre : de `TP−`
    vers le contact de terre de la fiche de la hotte, puis vers la vis de
    terre de la carcasse, OL ;
  - soit sur une plaque d'essai du banc, avec son propre étage d'écoute (mêmes
    valeurs qu'au §7.2).
- **Fil de masse** entre le `GND` du générateur et le `−` de l'étage d'écoute de la
  sonde.
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
   - Dès l'arrivée de l'analyseur, on mesure ce décalage et l'asymétrie de
     l'étage : une voie sur la ligne à travers 47k/68k, une voie sur GPIO6.
```

par :

```markdown
   - Dès l'arrivée de l'analyseur, on mesure ce décalage et l'asymétrie de
     l'étage : une voie sur la ligne à travers 47k/68k, une voie sur GPIO6.
   - Le FX2 est numérique : il donne des instants, pas des tensions. Un relevé
     chiffré des niveaux de la ligne est optionnel, à l'oscilloscope, et
     l'instrument est noté.
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
| plaques à pastilles ; NPN BC547 ou 2N3904 (5 environ) ; résistances 100 k, 68 k, 47 k, 33 k, 15 k, 10 k, 4,7 k, 470 Ω, 220 Ω ; résistance 10 kΩ / 2 W ; barrettes et cavaliers 2,54 mm (`J1`, `J2`) ; barrettes femelles pour la sonde ; colliers | ~10 à 15 € selon le stock | tout de suite |
```

par :

```markdown
| plaques à pastilles ; NPN BC547 ou 2N3904 (5 environ) ; résistances 100 k, 68 k, 47 k, 33 k, 15 k, 10 k, 4,7 k, 470 Ω, 220 Ω ; résistance 10 kΩ / 2 W ; condensateur céramique 1 nF (banc, variante 2 du §10) ; barrettes et cavaliers 2,54 mm (`J1`, `J2`) ; barrettes femelles pour la sonde ; colliers | ~10 à 15 € selon le stock | tout de suite |
```

Dans `docs/SPEC-RECONNAISSANCE.md`, remplacer :

```markdown
| niveau bas de `D` au-dessus du seuil de l'étage | mesuré à l'étape 5b, puis seuil relevé (§7.2) |
```

par :

```markdown
| niveau bas de `D` au-dessus du seuil de l'étage | jugé à l'étape 5b (capture propre, concordance avec l'analyseur ; oscilloscope en option), puis seuil relevé (§7.2) |
```

Lancer : `grep -c "entre +4,5 et +5,3 V\|juste avant de poser chaque nouveau montage\|Entre 3 et 5 V\|jugé fonctionnellement\|fiche JST XH 4 broches\|Aucune liaison avec la hotte\|condensateur céramique 1 nF" docs/SPEC-RECONNAISSANCE.md; grep -c "de +4,7 à +5,0 V\|doit rester sous 0,8 V\|Avant la première mise sous tension d'un montage\|4. refaire l'étape 2b" docs/SPEC-RECONNAISSANCE.md`

Attendu : `9` puis `0`

- [ ] **Étape 11 : Commit**

```bash
git add platformio.ini src/fw_version.h src/app_desc.c src/config.h src/capture_model.h src/capture_model.cpp \
  src/reglages.h src/reglages.cpp src/main.cpp tools/git_rev.py tools/tests/verif.h tools/tests/test_capture.cpp \
  tools/tests/test_hote.sh docs/SPEC-RECONNAISSANCE.md
git commit -m "Poser le projet de la sonde : version, configuration, reglages et bornes de capture" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 8 : `json_out` scindé de la ScreenBar, `u64`, événements `trame` et `injection`

**Fichiers :**
- Créer : `src/json_out.h` (scindé de `c58a506:src/json_out.h`)
- Créer : `src/json_out.cpp` (scindé de `c58a506:src/json_out.cpp`)
- Créer : `tools/tests/test_json.cpp` (adapté de `c58a506:tools/host_tests/test_json.cpp`)
- Modifier : `tools/tests/test_hote.sh` (deux lignes pour `test_json`)
- Tester : `tools/tests/test_json.cpp`

**Interfaces :**
- Consomme : `capt::Partie`, `capt::kDurMax` (`src/capture_model.h`, tâche 7) ; `VERIF`, `VERIF_EGAL_STR`, `bilan` (`tools/tests/verif.h`, tâche 7).
- Produit (`namespace jsonp`, tout le contrat « ajouts à `src/json_out.h` », à la lettre) :
  - gardés de la ScreenBar, inchangés : `kVersion` (1), `kRev` (4), `kLineMax` (1024), `kBudget` (896), `kCmdMax` (127), `kCmdTextMax` (40), `kMsgMax` (120), `kLogTextMax` (191), `kStrMax`, `kRS`, `kCtrlU`, `kIdMax`, `kOrigins` (3), `kUsb` (0), `class Writer`, `heartbeat`, `sessionEnd`, `logLine`, `reply`, `class ReplyCache`, `parseIdPrefix`, `copyCmd`, `maskCmd`, `class LineAssembler`, `class RateCap`, `class Cadence`, `class Queue`, `struct Queued`, `kLateMs`, `leaseExpired`, `remoteRefusal` ;
  - `struct Reply` sans `hasTarget`, `target`, `dirty`, `version` ; `enum Suite : uint8_t { SuiteNone, SuiteInjection };` (JSON `"suite":"injection"`) ;
  - `void Writer::u64(const char *k, uint64_t v);` ;
  - `enum class Item : uint8_t { HelloBase, HelloId, Config, EtatBus, EtatCapture, EtatInjection, EtatSys, Compteurs, NetIp, Heartbeat, Reply };` ;
  - `void trame(Writer &w, uint32_t n, uint32_t ms, const capt::Partie &p, bool hasRep, uint32_t rep, uint32_t sautes);` (champs `v`, `t`, `n`, `ms`, `num`, `part`, `fin`, `t_us`, `niv0`, `dur_us`, `debord`, puis `rep` si `hasRep`, puis `sautes` si > 0) ;
  - `struct InjectionEv` du contrat et `void injection(Writer &w, uint32_t n, uint32_t ms, const InjectionEv &e);` (champs `v`, `t`, `n`, `ms`, `id` ou `null`, `cmd`, `resultat`, `niv0`, `dur_us`, `attente_us`, `relu_us`) ;
  - ajout hors contrat : `constexpr uint16_t kInjDurMax = 64;` (durées de `dur_us` écrites au plus). `relu_us` s'arrête avant de dépasser `kBudget`. À la borne maximale admise des réglages d'injection (`total_max_us` ≤ 200 000 µs, borne haute de la tâche 22 ; relues à `+tol_us`), une vraie commande `injecte` de 64 durées garde toutes ses relues (testé) ; seul un pire cas artificiel, une `cmd` de 40 caractères tous échappés qu'aucune commande acceptée ne contient, est coupé ;
  - `maskCmd` masque aussi le mot de passe du Wi-Fi : `wifi <ssid> <mdp>` devient `wifi <ssid>` dans `reponse.cmd` et le cache des réponses, même quand `copyCmd` a tronqué le mot de passe (la ScreenBar ne masquait que `json cle nouvelle`) ;
  - `remoteRefusal` du profil hotte : `nullptr` pour `json 1 [bail 10..120]`, `json 0|etat|hello|ping|trames ...|log ...`, `json periode` ≥ 2000, `json compteurs 0|≥ 1000`, `json reseau 0|≥ 10000`, `capture on|off|tout|changements` (sans rien derrière), `seuils <au moins une valeur>`, `injection on|off` (sans rien derrière), `injecte <au moins un mot>` ; sinon un `msg` (« `interdite a distance (10.5) : USB seulement` », ou le rappel de la borne) ;
  - `test_json [fichier]` écrit les messages réalistes (trame, injection, réponses, log, hb, fin) dans `fichier` : `test_hote.sh` produit `$OUT/test_json_lignes.txt`, que la tâche 11 passera à `json_check.py`.
- Retirés (dépendaient de `halo1_*`) : `lampsCode`, `kindCode`, `slotCode`, `verdictCode`, `relaunchCode`, `symptomCode`, `state`, `fields`, `rx`, `tx`, `relaunch`, `module`, `led`, `Issue`, `Delivery`, `delivery`, `LampSample`, `DeliveryWatch`, `kIdsMax`.

**Déroulé : quatre cycles courts**, chacun avec son échec constaté puis sa vérification. L'en-tête `src/json_out.h` est posé en entier au premier cycle, et les suivants n'y touchent plus. Chaque cycle remplace `main` de `tools/tests/test_json.cpp` par ses nouveaux tests suivis du `main` qui les appelle, et ajoute sa section à la fin de `src/json_out.cpp`, juste avant `}  // namespace jsonp` :
1. écrivain, `u64` et renumérotation (`setN`) : étapes 1 à 4 ;
2. messages (`trame`, `injection`, `reponse`, `log`, `hb`, `fin`) et pires cas : étapes 5 à 8 ;
3. commande affichée (`copyCmd`, `maskCmd`) et liste blanche (`remoteRefusal`) : étapes 9 à 12 ;
4. mécaniques de la session (préfixe `id=`, assemblage des lignes, débit, cadence, file, bail, cache des réponses) : étapes 13 à 16.

`src/json_out.cpp` range donc ses sections dans cet ordre, et non dans celui de la ScreenBar : `copyCmd` vient avant `maskCmd`, et le cache des réponses en dernier. Le code des fonctions reprises ne change pas, sauf `maskCmd` (mot de passe du Wi-Fi).

- [ ] **Étape 1 : Écrire le test qui échoue (écrivain, `u64`, renumérotation)**

Les tests de l'écrivain de la ScreenBar, passés sur `verif.h`, plus `u64` et `setN`. Avec un chemin en argument, `test_json` y écrit ses messages réalistes (aucun encore à ce cycle).

Créer `tools/tests/test_json.cpp` :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : tools/host_tests/test_json.cpp (adapte : sans halo1, verif.h, u64, trame, injection, liste blanche hotte, masque wifi, tests regroupes par cycle)
//
// Tests hote des briques du protocole compagnon (src/json_out.*), profil
// hotte : ecrivain (u64 compris), echappement, ordre des champs, tailles,
// renumerotation, evenements trame et injection, reponse, pires cas sous le
// budget de 896 octets, commande affichee (reponse.cmd, masquage), liste
// blanche a distance, lignes de l'hote, plafonds de debit, cadence, file des
// periodiques, bail, cache des reponses.
// Lancer : sh tools/tests/test_hote.sh
//
// Avec un chemin en argument, les messages realistes produits (evenements,
// reponses ; ni les essais de l'ecrivain, ni les pires cas artificiels) y sont
// ecrits (RS + JSON + LF), pour tools/json_check.py.
#include <stdio.h>
#include <string.h>

#include <initializer_list>
#include <string>

#include "capture_model.h"
#include "json_out.h"
#include "verif.h"

using namespace jsonp;

static FILE *gCapture = nullptr;
static bool gCaptureOn = false;  // seulement les messages realistes
static Writer gW;

// Ferme la ligne et la rend telle qu'elle partirait (RS ... LF).
static std::string finish(Writer &w, bool *ok = nullptr) {
  const bool good = w.finish();
  if (ok) *ok = good;
  std::string s((const char *)w.data(), w.size());
  if (good && gCapture && gCaptureOn) fwrite(s.data(), 1, s.size(), gCapture);
  return s;
}

static std::string framed(const char *json) { return std::string("\x1e") + json + "\n"; }

static void expectLine(Writer &w, const char *json, const char *what) {
  bool ok = false;
  const std::string got = finish(w, &ok);
  const std::string want = framed(json);
  if (!ok || got != want) printf("  %s :\n  obtenu  %s  attendu %s", what, got.c_str() + (got.empty() ? 0 : 1), want.c_str() + 1);
  VERIF(ok && got == want);
}

// ---------------------------------------------------------------------------
//  Ecrivain
// ---------------------------------------------------------------------------

static void testWriter() {
  gW.begin("x", 5, 7);
  expectLine(gW, "{\"v\":1,\"t\":\"x\",\"n\":5,\"ms\":7}", "enveloppe seule");

  // Ordre v, t, n, ms, puis bloc ; entiers extremes ; imbrication.
  gW.begin("etat", 4294967295u, 4294967295u);
  gW.str("bloc", "bus");
  gW.i32("neg", -2147483647 - 1);
  gW.i32("pos", 2147483647);
  gW.u32("zero", 0);
  gW.obj("o");
  gW.arr("a");
  gW.u32(nullptr, 1);
  gW.str(nullptr, "b");
  gW.obj(nullptr);
  gW.boolean("t", true);
  gW.null("z");
  gW.end();
  gW.end();
  gW.boolean("f", false);
  gW.end();
  gW.arr("vide");
  gW.end();
  expectLine(gW,
             "{\"v\":1,\"t\":\"etat\",\"n\":4294967295,\"ms\":4294967295,\"bloc\":\"bus\",\"neg\":-2147483648,"
             "\"pos\":2147483647,\"zero\":0,\"o\":{\"a\":[1,\"b\",{\"t\":true,\"z\":null}],\"f\":false},\"vide\":[]}",
             "imbrication et entiers extremes");

  // Echappement : '"' et '\', octets hors 0x20..0x7E -> '?', jamais de \u.
  gW.begin("log", 1, 2);
  gW.str("s", "a\"b\\c\x01\x1e\x7f\xc3\xa9 ~");
  gW.str("tronq", "abcdefgh", 3);
  gW.str("tronq_esc", "\"\"\"\"", 2);  // max compte les caracteres de la valeur, pas l'echappement
  gW.str("nul", nullptr);
  expectLine(gW,
             "{\"v\":1,\"t\":\"log\",\"n\":1,\"ms\":2,\"s\":\"a\\\"b\\\\c????? ~\",\"tronq\":\"abc\",\"tronq_esc\":"
             "\"\\\"\\\"\",\"nul\":null}",
             "echappement");

  // Hexa.
  const uint8_t b[3] = {0x0A, 0xFF, 0x2E};
  gW.begin("h", 0, 0);
  gW.hex("b", b, 3);
  gW.hex("vide", b, 0);
  gW.hexU32("boot", 0x3FA2C901, 8);
  gW.hexU32("pan", 0x1A2B, 4, true);
  gW.hexU32("petit", 0x5, 4, true);
  expectLine(gW,
             "{\"v\":1,\"t\":\"h\",\"n\":0,\"ms\":0,\"b\":\"0AFF2E\",\"vide\":\"\",\"boot\":\"3FA2C901\",\"pan\":\"0x1A2B\","
             "\"petit\":\"0x0005\"}",
             "hexa");

  // Mal ferme : jamais emis.
  bool ok = true;
  gW.begin("x", 0, 0);
  gW.obj("o");
  finish(gW, &ok);
  VERIF(!ok);
  gW.begin("x", 0, 0);
  gW.end();
  finish(gW, &ok);
  VERIF(!ok);

  // Taille : exactement 1024 octets (RS et LF compris) passe, 1025 non.
  for (int extra = 0; extra < 2; extra++) {
    gW.begin("x", 0, 0);
    // enveloppe : RS {"v":1,"t":"x","n":0,"ms":0 = 1 + 27 ; ,"p":"..." = 7 + len ; } LF = 2
    const size_t head = 1 + strlen("{\"v\":1,\"t\":\"x\",\"n\":0,\"ms\":0");
    const size_t pad = kLineMax - head - 7 - 2 + (size_t)extra;
    std::string s(pad, 'a');
    gW.str("p", s.c_str(), pad);
    const bool good = gW.finish();
    VERIF(good == (extra == 0) && (!good || gW.size() == kLineMax));
    VERIF(gW.size() <= kLineMax);
  }
  // Un depassement enorme reste borne et refuse.
  gW.begin("x", 0, 0);
  for (int i = 0; i < 300; i++) gW.u32("k", 4294967295u);
  VERIF(!gW.finish() && gW.overflow() && gW.size() <= kLineMax);
}

// Entier sur 64 bits (t_us) : decimal, sans zero de tete, jusqu'a 2^64-1.
static void testU64() {
  gW.begin("x", 0, 0);
  gW.u64("zero", 0);
  gW.u64("un", 1);
  gW.u64("u32_plus_1", 4294967296ull);
  gW.u64("sur_json", 9007199254740991ull);  // 2^53-1 : entier sur en JSON
  gW.u64("max", 18446744073709551615ull);
  gW.arr("a");
  gW.u64(nullptr, 1234567890123ull);
  gW.end();
  expectLine(gW,
             "{\"v\":1,\"t\":\"x\",\"n\":0,\"ms\":0,\"zero\":0,\"un\":1,\"u32_plus_1\":4294967296,"
             "\"sur_json\":9007199254740991,\"max\":18446744073709551615,\"a\":[1234567890123]}",
             "u64");
}

// Un evenement formate une fois, renumerote pour chaque session (setN).
static void testRenumerote() {
  gW.begin("trame", 0, 4242);
  gW.u32("num", 1);
  const bool fin = gW.finish();
  VERIF(fin && gW.setN(123456));
  VERIF(std::string((const char *)gW.data(), gW.size()) ==
        framed("{\"v\":1,\"t\":\"trame\",\"n\":123456,\"ms\":4242,\"num\":1}"));
  VERIF(gW.setN(7) && std::string((const char *)gW.data(), gW.size()) ==
                          framed("{\"v\":1,\"t\":\"trame\",\"n\":7,\"ms\":4242,\"num\":1}"));
  gW.begin("trame", 0, 1);
  VERIF(!gW.setN(3));  // ligne pas fermee
}

int main(int argc, char **argv) {
  if (argc > 1) gCapture = fopen(argv[1], "wb");
  testWriter();
  testU64();
  testRenumerote();
  if (gCapture) fclose(gCapture);
  return bilan("test_json");
}
```

Deux lignes ajoutées à `tools/tests/test_hote.sh`, avant la ligne finale :

Remplacer tout le contenu de `tools/tests/test_hote.sh` par :

```sh
#!/bin/sh
# Tests hote de la sonde (sans carte). A lancer depuis n'importe ou : sh tools/tests/test_hote.sh
set -e
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/hotte-tests"
mkdir -p "$OUT"
CXX="clang++ -std=c++17 -Wall -Wextra -Werror -Isrc"
# Une commande par ligne : sous 'set -e', un echec a gauche d'un '&&' passerait inapercu.
$CXX src/capture_model.cpp tools/tests/test_capture.cpp -o "$OUT/test_capture"
"$OUT/test_capture"
$CXX src/json_out.cpp tools/tests/test_json.cpp -o "$OUT/test_json"
"$OUT/test_json" "$OUT/test_json_lignes.txt"
echo "tests hote : OK"
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`

Attendu : « `test_capture : 24 verifications, 0 echecs` », puis ÉCHEC avec « clang++: error: no such file or directory: 'src/json_out.cpp' »

- [ ] **Étape 3 : Écrire l'implémentation minimale (en-tête complet, écrivain)**

Partir de `git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:src/json_out.h` et `...:src/json_out.cpp`. L'en-tête ci-dessous est la version scindée complète : tout ce qui touche à `halo1_*` est retiré, le reste est gardé mot pour mot (commentaires ajustés), puis `u64`, `trame`, `injection`, `Item`, `Reply::Suite` et `remoteRefusal` du profil hotte. `src/json_out.cpp` ne reçoit ici que l'écrivain ; les fonctions déclarées et pas encore définies ne servent à aucun test de ce cycle.

Créer `src/json_out.h` :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/json_out.h (adapte : scinde, sans halo1_*, u64, Item hotte, trame, injection, liste blanche hotte, masque wifi)
#pragma once
// ===========================================================================
//  Protocole compagnon v1, profil hotte (docs/PROTOCOLE-JSON.md) : briques pures
//
//  - Writer : une ligne machine, RS + objet JSON compact en ASCII + LF, 1024
//    octets au plus. Chaines echappees ('"' et '\'), tout octet hors
//    0x20..0x7E remplace par '?', jamais de \uXXXX. Au-dela de 1024 octets, la
//    ligne est marquee trop longue : jamais emise.
//  - Messages construits depuis des donnees simples : evenements trame et
//    injection de la sonde, reponse, battement, fin, log.
//  - Mecaniques de la session : prefixe id= des lignes de l'hote, assemblage
//    des lignes recues, plafonds de debit par type, cadence, file des lignes
//    periodiques, bail, liste blanche a distance.
//
//  Pur et sans Arduino : teste sur l'hote (tools/tests/test_json.cpp). La
//  session, l'ecriture sur le port serie et les instantanes sont dans
//  json_mode.cpp.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "capture_model.h"

namespace jsonp {

constexpr uint8_t kVersion = 1;         // v : version majeure
// hello.rev : revision mineure, celle de la ScreenBar au commit c58a506 (4) :
// le profil hotte part de la derniere revision du protocole compagnon.
constexpr uint8_t kRev = 4;
constexpr size_t kLineMax = 1024;       // RS et LF compris
constexpr size_t kBudget = 896;         // pire cas vise par message (marge de 128 pour les ajouts)
constexpr size_t kCmdMax = 127;         // ligne de l'hote, prefixe id= compris
constexpr size_t kCmdTextMax = 40;      // reponse.cmd, injection.cmd
constexpr size_t kMsgMax = 120;         // reponse.msg
constexpr size_t kLogTextMax = 191;     // log.txt
constexpr size_t kStrMax = 255;         // toute autre chaine
constexpr uint8_t kRS = 0x1E;
constexpr uint8_t kCtrlU = 0x15;        // vide la ligne en cours de saisie
constexpr uint32_t kIdMax = 999999999;  // id=<1..999999999>
constexpr uint16_t kInjDurMax = 64;     // injection.dur_us : durees ecrites au plus (inj::kDurMax)
// Origines (transports) : 0 = USB, 1..kOrigins-1 = sessions reseau etablies.
constexpr uint8_t kOrigins = 3;
constexpr uint8_t kUsb = 0;

// ---------------------------------------------------------------------------
//  Ecrivain d'une ligne machine
// ---------------------------------------------------------------------------

class Writer {
 public:
  // Ouvre la ligne : RS {"v":1,"t":type,"n":n,"ms":ms. Le champ suivant d'un
  // message en blocs est "bloc" (str("bloc", ...)).
  void begin(const char *type, uint32_t n, uint32_t ms);
  // Champs. k nul : element du tableau ouvert. Les cles sont des litteraux
  // ASCII du firmware, jamais echappees.
  void str(const char *k, const char *v, size_t max = kStrMax);  // v nul : null ; max caracteres de v
  void u32(const char *k, uint32_t v);
  // Writer : entier 64 bits (t_us), decimal sans zero de tete.
  void u64(const char *k, uint64_t v);
  void i32(const char *k, int32_t v);
  void boolean(const char *k, bool v);
  void null(const char *k);
  void hex(const char *k, const uint8_t *p, size_t n);  // "C5A5" (majuscules, sans 0x) ; n == 0 : ""
  void hexU32(const char *k, uint32_t v, uint8_t digits, bool prefix0x = false);  // "3FA2C901", "0x1A2B"
  void obj(const char *k);
  void arr(const char *k);
  void end();  // ferme l'objet ou le tableau ouvert
  // Ferme la ligne (} LF). false : plus de kLineMax octets, ou objets mal
  // fermes (bogue) ; la ligne ne doit pas etre emise.
  bool finish();
  // Ligne fermee (finish) : n remplace par celui d'un autre transport (meme
  // evenement pour chaque session). false : la ligne depasserait kLineMax.
  bool setN(uint32_t n);
  const uint8_t *data() const { return buf_; }
  size_t size() const { return len_; }
  bool overflow() const { return over_; }

 private:
  static constexpr uint8_t kDepth = 8;
  void put(char c);
  void puts(const char *s);
  void sep(const char *k);
  void num(uint32_t v, bool neg);
  void open(const char *k, char o, char c);
  uint8_t buf_[kLineMax];
  size_t len_ = 0;
  bool over_ = false, bad_ = false;
  uint8_t depth_ = 0;
  bool first_[kDepth] = {};
  char close_[kDepth] = {};
};

// ---------------------------------------------------------------------------
//  Messages d'evenement et de reponse
// ---------------------------------------------------------------------------

void heartbeat(Writer &w, uint32_t n, uint32_t ms, uint32_t boot, uint32_t upS, uint32_t lost);
void sessionEnd(Writer &w, uint32_t n, uint32_t ms, const char *cause);  // t "fin" : commande, bail
void logLine(Writer &w, uint32_t n, uint32_t ms, const char *src, const char *niv, const char *txt, uint32_t skipped);

// Evenement trame (profil hotte). rep : present si hasRep ; sautes : present si > 0.
void trame(Writer &w, uint32_t n, uint32_t ms, const capt::Partie &p, bool hasRep, uint32_t rep, uint32_t sautes);

struct InjectionEv {
  uint32_t id = 0;              // id de la commande (0 : sans id, champ null)
  const char *cmd = "";         // tronquee a kCmdTextMax
  const char *resultat = "ok";  // ok, collision, delai
  bool niv0Haut = false;        // niveau du bus pendant dur[0] (toujours bas pour 'injecte durees')
  const uint32_t *dur = nullptr;
  uint16_t n = 0;
  uint32_t attenteUs = 0;
  const uint32_t *relu = nullptr;
  uint16_t nRelu = 0;
};
// dur_us : kInjDurMax durees au plus. relu_us : coupe avant que la ligne ne
// depasse kBudget. A la borne maximale admise (total_max_us <= 200 000 us,
// relues a +tol_us), une vraie commande 'injecte' de 64 durees garde toutes
// ses relues ; seul un pire cas artificiel (cmd de 40 caracteres tous
// echappes, qu'une commande acceptee ne contient jamais) est coupe (teste).
void injection(Writer &w, uint32_t n, uint32_t ms, const InjectionEv &e);

// Message reponse.
struct Reply {
  enum Suite : uint8_t { SuiteNone, SuiteInjection };  // absent, injection
  uint32_t id = 0;
  bool fin = true;              // false : etape debut
  const char *cmd = "";         // la commande sans le prefixe, tronquee a kCmdTextMax
  bool ok = true;
  const char *code = "ok";
  const char *msg = nullptr;    // nul : absent ; tronque a kMsgMax
  uint32_t durMs = 0;           // fin seulement
  Suite suite = SuiteNone;
  bool hasLease = false;        // bail_s, up_s (json 1, json ping)
  uint32_t leaseS = 0, upS = 0;
  const char *key = nullptr;    // cle : 64 hexa, une seule fois ('json cle nouvelle', USB)
  bool hasKid = false;          // empreinte ('json cle ...') : 8 hexa, ou null sans cle
  const char *kid = nullptr;
};
void reply(Writer &w, uint32_t n, uint32_t ms, const Reply &r);

// Dernieres reponses fin d'une session reseau : un id repete (l'app renvoie
// une commande restee sans reponse) recoit la meme reponse, sans nouvelle
// execution. Ni msg, ni cle, ni empreinte ne sont gardes.
class ReplyCache {
 public:
  static constexpr uint8_t kN = 8;
  void clear();
  void put(const Reply &r);              // etape fin avec id ; remplace la plus ancienne
  const Reply *find(uint32_t id) const;  // r.cmd pointe dans le cache
 private:
  struct Entry {
    bool used = false;
    Reply r;
    char cmd[kCmdTextMax + 1] = {};
  };
  Entry e_[kN];
  uint8_t next_ = 0;
};

// ---------------------------------------------------------------------------
//  Lignes de l'hote
// ---------------------------------------------------------------------------

// Commande permise sur le transport reseau (liste blanche du profil hotte) ?
// nullptr : oui ; sinon le msg de la reponse 'interdite'. cmd : la commande
// sans le prefixe id=. Seules les bornes propres au reseau sont verifiees ici
// (bail 10..120, periodes minimales) ; le reste des arguments l'est par
// l'aiguillage (usage).
const char *remoteRefusal(const char *cmd);

// Prefixe "id=<n> " (n decimal 1..999999999, sans zero de tete superflu
// exige) en tete de ligne, espaces de tete ignores. true : *id rempli et
// *rest pointe sur la commande (espaces sautes). false : pas de prefixe
// valide, *rest = line.
bool parseIdPrefix(char *line, uint32_t *id, char **rest);
// Copie la commande pour reponse.cmd : kCmdTextMax caracteres au plus.
void copyCmd(char out[kCmdTextMax + 1], const char *cmd);
// reponse.cmd ne renvoie jamais un secret : 'json cle nouvelle <64 hexa>'
// (alea de l'app) devient 'json cle nouvelle', et 'wifi <ssid> <mdp>' devient
// 'wifi <ssid>' (le mot de passe, meme tronque, ne part jamais).
void maskCmd(char *shown);

// Assemblage des octets recus en lignes (cliPoll). Mode machine : octets hors
// 0x20..0x7E ignores, sauf LF, CR, Ctrl-U et retour arriere, pour qu'aucun RS
// ne revienne dans un message. Au-dela de kCmdMax caracteres, la ligne est
// marquee trop longue (refusee a son LF, rien n'est execute).
class LineAssembler {
 public:
  enum class Ev : uint8_t { None, Echo, Erase, Clear, Line };
  Ev feed(uint8_t c, bool machine);
  char *text();  // ligne terminee par 0 (apres Ev::Line)
  bool tooLong() const { return tooLong_; }
  uint8_t cleared() const { return cleared_; }  // caracteres effaces par le dernier Ctrl-U
  uint8_t length() const { return len_; }
  void reset() {
    len_ = 0;
    tooLong_ = false;
  }

 private:
  char buf_[kCmdMax + 1] = {};
  uint8_t len_ = 0, cleared_ = 0;
  bool tooLong_ = false;
};

// ---------------------------------------------------------------------------
//  Debit
// ---------------------------------------------------------------------------

// Plafond par fenetre d'une seconde. Au-dela, l'evenement n'est pas produit
// (aucun n consomme) et le suivant du meme type porte 'sautes'.
class RateCap {
 public:
  explicit RateCap(uint16_t perSecond) : limit_(perSecond) {}
  void setLimit(uint16_t perSecond) { limit_ = perSecond; }
  bool available(uint32_t now);  // place dans la fenetre en cours
  void take() { count_++; }
  void skip() { skipped_++; }
  uint32_t takeSkipped() {
    const uint32_t s = skipped_;
    skipped_ = 0;
    return s;
  }

 private:
  uint16_t limit_, count_ = 0;
  bool started_ = false;
  uint32_t winAt_ = 0, skipped_ = 0;
};

// Au plus kLines lignes acceptees par kWindowMs glissantes.
class Cadence {
 public:
  static constexpr uint8_t kLines = 20;
  static constexpr uint32_t kWindowMs = 1000;
  bool allow(uint32_t now);  // true : ligne acceptee et comptee

 private:
  uint32_t at_[kLines] = {};
  uint8_t idx_ = 0, n_ = 0;
};

// ---------------------------------------------------------------------------
//  File des lignes periodiques
// ---------------------------------------------------------------------------

constexpr uint32_t kLateMs = 500;  // ligne periodique perdue apres ce retard

enum class Item : uint8_t {
  HelloBase, HelloId, Config, EtatBus, EtatCapture, EtatInjection, EtatSys, Compteurs, NetIp, Heartbeat, Reply
};
struct Queued {
  Item item;
  uint8_t arg;   // Reply : place de la reponse differee
  bool session;  // periodique ou instantane de 'json 1' : retire a la fin du mode machine
  uint32_t at;   // mise en file (ou derniere demande explicite fondue dedans)
};

class Queue {
 public:
  static constexpr uint8_t kN = 24;
  // Ajoute en queue. Un element deja en file (hors Reply) n'est pas double ;
  // une demande explicite (session faux) fondue dedans le rend explicite et
  // repart de maintenant (son retard ne compte que depuis la demande).
  // false : file pleine.
  bool push(Item item, uint32_t now, bool session, uint8_t arg = 0);
  const Queued *front() const { return n_ ? &q_[head_] : nullptr; }
  void pop();
  uint8_t size() const { return n_; }
  bool has(Item item) const;
  // Retire de la tete les lignes en retard de plus de lateMs et rend leur
  // nombre (n consomme, json_perdus). Une reponse n'est jamais perdue pour
  // retard : elle arrete le balayage, les lignes derriere elle attendent.
  uint8_t dropLate(uint32_t now, uint32_t lateMs = kLateMs);
  // La tete peut-elle partir avec 'room' octets libres dans le tampon
  // d'emission ? Ligne periodique : 2 x kLineMax (elle, puis la place d'un
  // evenement). Reponse : kLineMax (elle tient, quelle qu'elle soit).
  bool frontReady(int room) const;
  // Retire les elements de session ; ceux qui restent gardent leur ordre.
  uint8_t dropSession();
  void clear() { head_ = n_ = 0; }

 private:
  Queued q_[kN] = {};
  uint8_t head_ = 0, n_ = 0;
};

// ---------------------------------------------------------------------------
//  Bail
// ---------------------------------------------------------------------------

// Le bail court depuis le plus recent : dernier octet recu, ou fin de la
// derniere commande. leaseS nul : sans bail, jamais expire.
bool leaseExpired(uint32_t now, uint32_t lastRx, uint32_t lastCmd, uint16_t leaseS);

}  // namespace jsonp
```

Créer `src/json_out.cpp` :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/json_out.cpp (adapte : scinde, sans halo1_*, u64, trame, injection, liste blanche hotte, masque wifi, sections reordonnees)
#include "json_out.h"

#include <string.h>

namespace jsonp {

// ===========================================================================
//  Writer
// ===========================================================================

void Writer::put(char c) {
  if (len_ < kLineMax) buf_[len_++] = (uint8_t)c;
  else over_ = true;
}

void Writer::puts(const char *s) {
  while (*s) put(*s++);
}

// Virgule avant tout champ sauf le premier de son niveau, puis la cle.
void Writer::sep(const char *k) {
  if (!depth_) {
    bad_ = true;
    return;
  }
  if (!first_[depth_ - 1]) put(',');
  first_[depth_ - 1] = false;
  if (k) {
    put('"');
    puts(k);
    put('"');
    put(':');
  }
}

void Writer::num(uint32_t v, bool neg) {
  char t[10];
  uint8_t i = 0;
  do {
    t[i++] = (char)('0' + v % 10);
    v /= 10;
  } while (v);
  if (neg) put('-');
  while (i) put(t[--i]);
}

void Writer::begin(const char *type, uint32_t n, uint32_t ms) {
  len_ = 0;
  over_ = bad_ = false;
  depth_ = 1;
  first_[0] = true;
  close_[0] = '}';
  put((char)kRS);
  put('{');
  u32("v", kVersion);
  str("t", type);
  u32("n", n);
  u32("ms", ms);
}

void Writer::str(const char *k, const char *v, size_t max) {
  sep(k);
  if (!v) {
    puts("null");
    return;
  }
  put('"');
  for (size_t i = 0; v[i] && i < max; i++) {
    const uint8_t c = (uint8_t)v[i];
    if (c == '"' || c == '\\') {
      put('\\');
      put((char)c);
    } else if (c < 0x20 || c > 0x7E) {
      put('?');  // jamais de \uXXXX, jamais d'octet hors ASCII imprimable
    } else {
      put((char)c);
    }
  }
  put('"');
}

void Writer::u32(const char *k, uint32_t v) {
  sep(k);
  num(v, false);
}

void Writer::u64(const char *k, uint64_t v) {
  sep(k);
  char t[20];  // 18446744073709551615 : 20 chiffres
  uint8_t i = 0;
  do {
    t[i++] = (char)('0' + v % 10);
    v /= 10;
  } while (v);
  while (i) put(t[--i]);
}

void Writer::i32(const char *k, int32_t v) {
  sep(k);
  if (v < 0) num((uint32_t)(-(int64_t)v), true);
  else num((uint32_t)v, false);
}

void Writer::boolean(const char *k, bool v) {
  sep(k);
  puts(v ? "true" : "false");
}

void Writer::null(const char *k) {
  sep(k);
  puts("null");
}

static const char kHexDigits[] = "0123456789ABCDEF";

void Writer::hex(const char *k, const uint8_t *p, size_t n) {
  sep(k);
  put('"');
  for (size_t i = 0; i < n; i++) {
    put(kHexDigits[p[i] >> 4]);
    put(kHexDigits[p[i] & 0x0F]);
  }
  put('"');
}

void Writer::hexU32(const char *k, uint32_t v, uint8_t digits, bool prefix0x) {
  sep(k);
  put('"');
  if (prefix0x) puts("0x");
  if (digits > 8) digits = 8;
  for (int8_t i = (int8_t)digits - 1; i >= 0; i--) put(kHexDigits[(v >> (4 * i)) & 0x0F]);
  put('"');
}

void Writer::open(const char *k, char o, char c) {
  sep(k);
  put(o);
  if (depth_ >= kDepth) {
    bad_ = true;
    return;
  }
  first_[depth_] = true;
  close_[depth_] = c;
  depth_++;
}

void Writer::obj(const char *k) { open(k, '{', '}'); }
void Writer::arr(const char *k) { open(k, '[', ']'); }

void Writer::end() {
  if (depth_ <= 1) {
    bad_ = true;
    return;
  }
  depth_--;
  put(close_[depth_]);
}

bool Writer::finish() {
  if (depth_ != 1) bad_ = true;
  put('}');
  put('\n');
  depth_ = 0;
  return !over_ && !bad_;
}

bool Writer::setN(uint32_t n) {
  if (over_ || bad_ || depth_) return false;
  // RS {"v":1,"t":"<type>","n":<chiffres>,... : le type est un litteral du
  // firmware, sans '"' ; le premier ,"n": est donc le bon.
  static const char kKey[] = ",\"n\":";
  const size_t kl = sizeof(kKey) - 1;
  size_t p = 0;
  while (p + kl <= len_ && memcmp(buf_ + p, kKey, kl)) p++;
  if (p + kl > len_) return false;
  p += kl;
  size_t q = p;
  while (q < len_ && buf_[q] >= '0' && buf_[q] <= '9') q++;
  char d[10];
  size_t dn = 0;
  do {
    d[dn++] = (char)('0' + n % 10);
    n /= 10;
  } while (n);
  const size_t newLen = len_ - (q - p) + dn;
  if (q == p || newLen > kLineMax) return false;
  memmove(buf_ + p + dn, buf_ + q, len_ - q);
  for (size_t i = 0; i < dn; i++) buf_[p + i] = (uint8_t)d[dn - 1 - i];
  len_ = newLen;
  return true;
}

}  // namespace jsonp
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh`

Attendu :

```text
test_capture : 24 verifications, 0 echecs
test_json : 16 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 5 : Écrire le test qui échoue (messages et pires cas)**

Les événements `trame` et `injection`, la réponse (dont celles de `json cle`), `log`, `hb` et `fin`, écrits dans le fichier des messages réalistes. Puis les pires cas, chacun sous le budget de 896 octets : la trame de 110 durées de 32 767 puis de 65 534 µs, compteurs au maximum, `t_us` = 2⁵³−1, `rep` et `sautes` au maximum ; l'événement `injection` construit à la borne maximale admise (`total_max_us` ≤ 200 000 µs : 15 durées de 10 000 et 49 de 1 000, le plus de chiffres possible ; relues à +500 µs, la borne haute de `tol_us`). Avec une `cmd` de 40 `"` échappés, `relu_us` est coupé ; avec une vraie commande de 40 caractères, les 64 relues restent.

Dans `tools/tests/test_json.cpp`, remplacer :

```cpp
int main(int argc, char **argv) {
  if (argc > 1) gCapture = fopen(argv[1], "wb");
  testWriter();
  testU64();
  testRenumerote();
  if (gCapture) fclose(gCapture);
  return bilan("test_json");
}
```

par :

```cpp
// ---------------------------------------------------------------------------
//  Evenements de la sonde
// ---------------------------------------------------------------------------

static capt::Partie partie(uint32_t num, uint32_t part, bool fin, uint64_t tUs, bool niv0Haut,
                           std::initializer_list<uint32_t> dur) {
  capt::Partie p;
  p.num = num;
  p.part = part;
  p.fin = fin;
  p.tUs = tUs;
  p.niv0Haut = niv0Haut;
  for (uint32_t d : dur) p.dur[p.n++] = d;
  return p;
}

static void testTrame() {
  // Une reception d'une partie, mode tout : ni rep, ni sautes.
  capt::Partie p = partie(12, 0, true, 81234567, false, {1500, 750, 750, 2250});
  trame(gW, 305, 81240, p, false, 0, 0);
  expectLine(gW,
             "{\"v\":1,\"t\":\"trame\",\"n\":305,\"ms\":81240,\"num\":12,\"part\":0,\"fin\":true,\"t_us\":81234567,"
             "\"niv0\":\"bas\",\"dur_us\":[1500,750,750,2250],\"debord\":false}",
             "trame simple");
  // Partie suivante d'une longue reception, apres une perte, mode changements.
  p = partie(13, 1, false, 5000000123ull, true, {100, 200});
  p.debord = true;
  trame(gW, 306, 81241, p, true, 7, 3);
  expectLine(gW,
             "{\"v\":1,\"t\":\"trame\",\"n\":306,\"ms\":81241,\"num\":13,\"part\":1,\"fin\":false,"
             "\"t_us\":5000000123,\"niv0\":\"haut\",\"dur_us\":[100,200],\"debord\":true,\"rep\":7,\"sautes\":3}",
             "trame avec rep et sautes");
  // Mode changements : rep present meme nul ; partie vide qui clot une reception.
  p = partie(13, 2, true, 5000000423ull, true, {});
  trame(gW, 307, 81242, p, true, 0, 0);
  expectLine(gW,
             "{\"v\":1,\"t\":\"trame\",\"n\":307,\"ms\":81242,\"num\":13,\"part\":2,\"fin\":true,"
             "\"t_us\":5000000423,\"niv0\":\"haut\",\"dur_us\":[],\"debord\":false,\"rep\":0}",
             "trame vide de fin, rep nul");
}

static void testInjection() {
  const uint32_t dur[4] = {750, 750, 750, 2250};
  const uint32_t relu[4] = {752, 748, 751, 2249};
  InjectionEv e;
  e.id = 42;
  e.cmd = "injecte durees 750 750 750 2250";
  e.dur = dur;
  e.n = 4;
  e.attenteUs = 20412;
  e.relu = relu;
  e.nRelu = 4;
  injection(gW, 900, 123456, e);
  expectLine(gW,
             "{\"v\":1,\"t\":\"injection\",\"n\":900,\"ms\":123456,\"id\":42,\"cmd\":\"injecte durees 750 750 750 2250\","
             "\"resultat\":\"ok\",\"niv0\":\"bas\",\"dur_us\":[750,750,750,2250],\"attente_us\":20412,"
             "\"relu_us\":[752,748,751,2249]}",
             "injection ok");
  // Sans id (console) : null ; le bus ne s'est pas tu : rien d'emis, rien de relu.
  InjectionEv d;
  d.cmd = "injecte durees 750";
  d.resultat = "delai";
  d.dur = dur;
  d.n = 1;
  d.attenteUs = 1000000;
  injection(gW, 901, 124456, d);
  expectLine(gW,
             "{\"v\":1,\"t\":\"injection\",\"n\":901,\"ms\":124456,\"id\":null,\"cmd\":\"injecte durees 750\","
             "\"resultat\":\"delai\",\"niv0\":\"bas\",\"dur_us\":[750],\"attente_us\":1000000,\"relu_us\":[]}",
             "injection delai sans id");
  // cmd tronquee a 40 caracteres (espace final compris) ; collision : relu plus court que l'emission.
  e.id = 43;
  e.cmd = "injecte durees 750 750 750 2250 750 750 750 2250";
  e.resultat = "collision";
  e.nRelu = 2;
  injection(gW, 902, 125000, e);
  expectLine(gW,
             "{\"v\":1,\"t\":\"injection\",\"n\":902,\"ms\":125000,\"id\":43,\"cmd\":\"injecte durees 750 750 750 2250 "
             "750 750 \",\"resultat\":\"collision\",\"niv0\":\"bas\",\"dur_us\":[750,750,750,2250],\"attente_us\":20412,"
             "\"relu_us\":[752,748]}",
             "injection collision, cmd tronquee");
}

static void testReply() {
  Reply r;
  r.id = 1;
  r.cmd = "json 1";
  r.durMs = 12;
  r.hasLease = true;
  r.leaseS = 30;
  r.upS = 83;
  reply(gW, 11, 83523, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":11,\"ms\":83523,\"id\":1,\"etape\":\"fin\",\"cmd\":\"json 1\",\"ok\":true,"
             "\"code\":\"ok\",\"duree_ms\":12,\"bail_s\":30,\"up_s\":83}",
             "reponse json 1");
  r = Reply();
  r.id = 5;
  r.cmd = "injecte durees 750 750";
  r.code = "accepte";
  r.durMs = 1;
  r.suite = Reply::SuiteInjection;
  reply(gW, 71, 95002, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":71,\"ms\":95002,\"id\":5,\"etape\":\"fin\",\"cmd\":\"injecte durees 750 750\","
             "\"ok\":true,\"code\":\"accepte\",\"duree_ms\":1,\"suite\":\"injection\"}",
             "reponse accepte, suite injection");
  r = Reply();
  r.id = 6;
  r.cmd = "injecte durees 750";
  r.ok = false;
  r.code = "refuse";
  r.msg = "non armee";
  reply(gW, 72, 95010, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":72,\"ms\":95010,\"id\":6,\"etape\":\"fin\",\"cmd\":\"injecte durees 750\","
             "\"ok\":false,\"code\":\"refuse\",\"msg\":\"non armee\",\"duree_ms\":0}",
             "reponse refus");
  r = Reply();
  r.id = 8;
  r.fin = false;
  r.cmd = "stats";
  r.code = "en_cours";
  reply(gW, 530, 180002, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":530,\"ms\":180002,\"id\":8,\"etape\":\"debut\",\"cmd\":\"stats\","
             "\"ok\":true,\"code\":\"en_cours\"}",
             "reponse debut");
  r.fin = true;
  r.code = "execute";
  r.durMs = 7;
  reply(gW, 531, 180009, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":531,\"ms\":180009,\"id\":8,\"etape\":\"fin\",\"cmd\":\"stats\","
             "\"ok\":true,\"code\":\"execute\",\"duree_ms\":7}",
             "reponse fin");
}

static void testEvents() {
  logLine(gW, 640, 200100, "capture", "notice", "[reglages] 1 valeur hors bornes en NVS : valeur par defaut", 0);
  expectLine(gW,
             "{\"v\":1,\"t\":\"log\",\"n\":640,\"ms\":200100,\"src\":\"capture\",\"niv\":\"notice\",\"txt\":\"[reglages] "
             "1 valeur hors bornes en NVS : valeur par defaut\"}",
             "log");
  heartbeat(gW, 641, 202000, 0x3FA2C901, 202, 0);
  expectLine(gW, "{\"v\":1,\"t\":\"hb\",\"n\":641,\"ms\":202000,\"boot\":\"3FA2C901\",\"up_s\":202,\"json_perdus\":0}", "hb");
  sessionEnd(gW, 662, 231400, "bail");
  expectLine(gW, "{\"v\":1,\"t\":\"fin\",\"n\":662,\"ms\":231400,\"cause\":\"bail\"}", "fin");
}

// Reponses de 'json cle' : cle (une fois) et empreinte.
static void testReponsesCle() {
  Reply k;
  k.id = 3;
  k.cmd = "json cle nouvelle";
  k.durMs = 4;
  k.key = "20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F";
  k.hasKid = true;
  k.kid = "630DCD29";
  reply(gW, 40, 5000, k);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":40,\"ms\":5000,\"id\":3,\"etape\":\"fin\",\"cmd\":"
             "\"json cle nouvelle\",\"ok\":true,\"code\":\"ok\",\"duree_ms\":4,"
             "\"cle\":\"20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F\",\"empreinte\":\"630DCD29\"}",
             "reponse json cle nouvelle");
  k = Reply();
  k.id = 4;
  k.cmd = "json cle";
  k.hasKid = true;  // sans cle : empreinte null
  reply(gW, 41, 5001, k);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":41,\"ms\":5001,\"id\":4,\"etape\":\"fin\",\"cmd\":\"json cle\","
             "\"ok\":true,\"code\":\"ok\",\"duree_ms\":0,\"empreinte\":null}",
             "reponse json cle sans cle");
}

// ---------------------------------------------------------------------------
//  Pires cas : chaque message tient dans le budget de 896 octets
// ---------------------------------------------------------------------------

static void checkBudget(const char *what, size_t *worst) {
  bool ok = false;
  const std::string s = finish(gW, &ok);
  if (!ok || s.size() > kBudget) printf("  %s : %zu octets (budget %zu)\n", what, s.size(), kBudget);
  VERIF(ok && s.size() <= kBudget);
  if (s.size() > *worst) *worst = s.size();
}

static void testWorstCases() {
  const uint32_t M = 4294967295u;
  size_t worst = 0;
  // trame : 110 durees de 32767 (1 MHz) puis de 65534 (500 kHz), compteurs au
  // maximum, t_us a 2^53-1, rep et sautes au maximum.
  capt::Partie p;
  p.num = p.part = M;
  p.fin = false;
  p.tUs = 9007199254740991ull;
  p.niv0Haut = true;
  p.debord = false;
  p.n = capt::kDurMax;
  for (uint32_t d : {32767u, 65534u}) {
    for (uint16_t i = 0; i < p.n; i++) p.dur[i] = d;
    trame(gW, M, M, p, true, M, M);
    checkBudget("trame", &worst);
  }
  printf("  pire trame : %zu octets (budget %zu)\n", worst, kBudget);
  // injection, a la borne maximale admise (total_max_us <= 200 000 us, tol_us
  // <= 500 us) : 64 durees de somme 199 000 us, 15 de 10000 et 49 de 1000 (le
  // plus de chiffres possible sous 200 000 us), relues a +500 us (+tol_us),
  // id, attente, n et ms au maximum.
  uint32_t dur[64], relu[64];
  uint32_t somme = 0;
  for (int i = 0; i < 64; i++) {
    dur[i] = i < 15 ? 10000 : 1000;
    relu[i] = dur[i] + 500;
    somme += dur[i];
  }
  VERIF(somme == 199000);
  // Pire cas artificiel : cmd de 40 '"', tous echappes (une commande 'injecte'
  // acceptee n'en contient jamais) ; relu_us est coupe pour tenir le budget.
  const std::string q(200, '"');
  InjectionEv e;
  e.id = kIdMax;
  e.cmd = q.c_str();
  e.resultat = "collision";
  e.niv0Haut = true;
  e.dur = dur;
  e.n = 64;
  e.attenteUs = M;
  e.relu = relu;
  e.nRelu = 64;
  injection(gW, M, M, e);
  checkBudget("injection", &worst);
  // Commande reelle de 40 caracteres, a la meme borne : relu_us n'est jamais
  // tronque (64 relues), et la ligne tient le budget.
  e.cmd = "injecte durees 10000 10000 10000 10000 10";
  injection(gW, M, M, e);
  {
    bool ok = false;
    const std::string s = finish(gW, &ok);
    size_t virgules = 0;
    for (size_t i = s.find("\"relu_us\""); i < s.size(); i++) virgules += s[i] == ',';
    VERIF(ok && s.size() <= kBudget && virgules + 1 == 64 && s.find(",10500,1500,") != std::string::npos);
  }
  // Durees absurdes (bogue de l'appelant) : relu_us tronque au budget, dur_us
  // borne a kInjDurMax, la ligne reste emise (1024 octets au plus).
  uint32_t big[100];
  for (uint32_t &d : big) d = M;
  e.dur = e.relu = big;
  e.n = e.nRelu = 100;
  injection(gW, M, M, e);
  bool ok = false;
  const std::string s = finish(gW, &ok);
  VERIF(ok && s.size() <= kLineMax && s.find("\"relu_us\":[]") != std::string::npos);
  // reponse : cmd et msg a leur longueur maximale, entierement echappes, tous les champs.
  const std::string bs(200, '\\');
  Reply rp;
  rp.id = kIdMax;
  rp.cmd = q.c_str();
  rp.ok = false;
  rp.code = "deja_traite";
  rp.msg = bs.c_str();
  rp.durMs = M;
  rp.suite = Reply::SuiteInjection;
  rp.hasLease = true;
  rp.leaseS = rp.upS = M;
  rp.key = "20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F";
  rp.hasKid = true;
  rp.kid = "630DCD29";
  reply(gW, M, M, rp);
  checkBudget("reponse", &worst);
  // log : 191 caracteres, tous echappes.
  const std::string lq(400, '\\');
  logLine(gW, M, M, "capture", "notice", lq.c_str(), M);
  checkBudget("log", &worst);
  heartbeat(gW, M, M, M, M, M);
  checkBudget("hb", &worst);
  sessionEnd(gW, M, M, "commande");
  checkBudget("fin", &worst);
  printf("  pire cas des evenements : %zu octets (budget %zu)\n", worst, kBudget);
}

int main(int argc, char **argv) {
  if (argc > 1) gCapture = fopen(argv[1], "wb");
  testWriter();
  testU64();
  testRenumerote();
  gCaptureOn = true;
  testTrame();
  testInjection();
  testReply();
  testEvents();
  testReponsesCle();
  gCaptureOn = false;
  testWorstCases();
  if (gCapture) fclose(gCapture);
  return bilan("test_json");
}
```

- [ ] **Étape 6 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`

Attendu : ÉCHEC à l'édition des liens de `test_json`, avec « Undefined symbols for architecture arm64 », « "jsonp::trame(jsonp::Writer&, unsigned int, unsigned int, capt::Partie const&, bool, unsigned int, unsigned int)", referenced from: » et « ld: symbol(s) not found for architecture arm64 »

- [ ] **Étape 7 : Écrire l'implémentation minimale (messages)**

Dans `src/json_out.cpp`, remplacer :

```cpp
}  // namespace jsonp
```

par :

```cpp
// ===========================================================================
//  Messages
// ===========================================================================

void heartbeat(Writer &w, uint32_t n, uint32_t ms, uint32_t boot, uint32_t upS, uint32_t lost) {
  w.begin("hb", n, ms);
  w.hexU32("boot", boot, 8);
  w.u32("up_s", upS);
  w.u32("json_perdus", lost);
}

void sessionEnd(Writer &w, uint32_t n, uint32_t ms, const char *cause) {
  w.begin("fin", n, ms);
  w.str("cause", cause);
}

void logLine(Writer &w, uint32_t n, uint32_t ms, const char *src, const char *niv, const char *txt,
             uint32_t skipped) {
  w.begin("log", n, ms);
  w.str("src", src);
  w.str("niv", niv);
  w.str("txt", txt, kLogTextMax);
  if (skipped) w.u32("sautes", skipped);
}

void trame(Writer &w, uint32_t n, uint32_t ms, const capt::Partie &p, bool hasRep, uint32_t rep, uint32_t sautes) {
  w.begin("trame", n, ms);
  w.u32("num", p.num);
  w.u32("part", p.part);
  w.boolean("fin", p.fin);
  w.u64("t_us", p.tUs);
  w.str("niv0", p.niv0Haut ? "haut" : "bas");
  w.arr("dur_us");
  for (uint16_t i = 0; i < p.n && i < capt::kDurMax; i++) w.u32(nullptr, p.dur[i]);
  w.end();
  w.boolean("debord", p.debord);
  if (hasRep) w.u32("rep", rep);
  if (sautes) w.u32("sautes", sautes);
}

void injection(Writer &w, uint32_t n, uint32_t ms, const InjectionEv &e) {
  w.begin("injection", n, ms);
  if (e.id) w.u32("id", e.id);
  else w.null("id");
  w.str("cmd", e.cmd ? e.cmd : "", kCmdTextMax);
  w.str("resultat", e.resultat ? e.resultat : "");
  w.str("niv0", e.niv0Haut ? "haut" : "bas");
  w.arr("dur_us");
  for (uint16_t i = 0; e.dur && i < e.n && i < kInjDurMax; i++) w.u32(nullptr, e.dur[i]);
  w.end();
  w.u32("attente_us", e.attenteUs);
  w.arr("relu_us");
  // Chaque duree relue coute 11 octets au plus (",4294967295"), puis "]}" LF.
  for (uint16_t i = 0; e.relu && i < e.nRelu && w.size() + 11 + 3 <= kBudget; i++) w.u32(nullptr, e.relu[i]);
  w.end();
}

void reply(Writer &w, uint32_t n, uint32_t ms, const Reply &r) {
  w.begin("reponse", n, ms);
  w.u32("id", r.id);
  w.str("etape", r.fin ? "fin" : "debut");
  w.str("cmd", r.cmd ? r.cmd : "", kCmdTextMax);
  w.boolean("ok", r.ok);
  w.str("code", r.code);
  if (r.msg) w.str("msg", r.msg, kMsgMax);
  if (r.fin) w.u32("duree_ms", r.durMs);
  if (r.suite == Reply::SuiteInjection) w.str("suite", "injection");
  if (r.hasLease) {
    w.u32("bail_s", r.leaseS);
    w.u32("up_s", r.upS);
  }
  if (r.key) w.str("cle", r.key, 64);
  if (r.hasKid) w.str("empreinte", r.kid, 8);
}

}  // namespace jsonp
```

- [ ] **Étape 8 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh`

Attendu :

```text
test_capture : 24 verifications, 0 echecs
  pire trame : 860 octets (budget 896)
  pire cas des evenements : 889 octets (budget 896)
test_json : 42 verifications, 0 echecs
tests hote : OK
```

La pire ligne `trame` fait 860 octets, sous le budget de 896 : 110 durées de 5 chiffres, `num`, `part`, `rep`, `sautes`, `n` et `ms` à 4 294 967 295, `t_us` à 9 007 199 254 740 991. Le pire cas des événements (889 octets) est l'événement `injection` artificiel, dont `relu_us` est coupé ; à la même borne, la vraie commande de 40 caractères garde ses 64 relues en 874 octets.

Lancer : `grep -c "" "${TMPDIR:-/tmp}/hotte-tests/test_json_lignes.txt"`

Attendu : `16` (lignes réalistes pour `json_check.py`, tâche 11)

- [ ] **Étape 9 : Écrire le test qui échoue (commande affichée et liste blanche)**

`reponse.cmd` est tronquée à 40 caractères et ne montre jamais un secret : ni l'aléa de `json cle nouvelle`, ni le mot de passe de `wifi <ssid> <mdp>`, même tronqué. Puis la liste blanche du profil hotte.

Dans `tools/tests/test_json.cpp`, remplacer :

```cpp
int main(int argc, char **argv) {
  if (argc > 1) gCapture = fopen(argv[1], "wb");
  testWriter();
  testU64();
  testRenumerote();
  gCaptureOn = true;
  testTrame();
  testInjection();
  testReply();
  testEvents();
  testReponsesCle();
  gCaptureOn = false;
  testWorstCases();
  if (gCapture) fclose(gCapture);
  return bilan("test_json");
}
```

par :

```cpp
// ---------------------------------------------------------------------------
//  Commande affichee (reponse.cmd) et liste blanche a distance
// ---------------------------------------------------------------------------

// reponse.cmd : tronquee a 40 caracteres, jamais un secret.
static void testCommandeAffichee() {
  char shown[kCmdTextMax + 1];
  copyCmd(shown, "injecte durees 750 750 750 2250 et encore des mots pour depasser");
  VERIF(strlen(shown) == kCmdTextMax && !strncmp(shown, "injecte durees 750 750 750 2250 et encor", kCmdTextMax));
  // Jamais l'alea de 'json cle nouvelle'.
  copyCmd(shown, "json cle nouvelle 000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "json cle nouvelle");
  copyCmd(shown, "json cle efface");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "json cle efface");
  // Jamais le mot de passe du Wi-Fi, meme tronque par copyCmd.
  copyCmd(shown, "wifi maison motdepasse-secret");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifi maison");
  copyCmd(shown, "wifi 0123456789ABCDEF0123456789ABCDEF motdepasse");
  VERIF_EGAL_STR(shown, "wifi 0123456789ABCDEF0123456789ABCDEF mo");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifi 0123456789ABCDEF0123456789ABCDEF");
  // Reseau ouvert (sans mdp), 'wifi' seul, autre commande : inchanges.
  copyCmd(shown, "wifi maison");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifi maison");
  copyCmd(shown, "wifi");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifi");
  copyCmd(shown, "wifis maison secret");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifis maison secret");
}

// Transport reseau : liste blanche du profil hotte.
static void testRemote() {
  struct Case {
    const char *cmd;
    bool ok;
  };
  static const Case kCases[] = {
      {"json 1", true},
      {"json 1 bail 30", true},
      {"json 1 bail 10", true},
      {"json 1 bail 120", true},
      {"json 1 bail 0", false},
      {"json 1 bail 9", false},
      {"json 1 bail 121", false},
      {"json 1 bail x", true},  // usage, dit par l'aiguillage
      {"json 1 bail 0000000000", false},
      {"json 1 bail 0000000600", false},
      {"json 1 bail 00000000030", true},
      {"json 1 bail 99999999999", false},
      {"json periode 0000000200", false},
      {"json compteurs 0000000200", false},
      {"json reseau 00000001000", false},
      {"json periode 00000002000", true},
      {"json 0", true},
      {"json etat", true},
      {"json hello", true},
      {"json ping", true},
      {"json periode 2000", true},
      {"json periode 1999", false},
      {"json periode 0", false},
      {"json compteurs 0", true},
      {"json compteurs 999", false},
      {"json compteurs 1000", true},
      {"json compteurs 60000", true},
      {"json reseau 0", true},
      {"json reseau 9999", false},
      {"json reseau 10000", true},
      {"json trames 1", true},
      {"json trames 0", true},
      {"json log 0", true},
      {"json", false},
      {"json cle", false},
      {"json cle efface", false},
      {"json cle nouvelle 00", false},
      {"capture on", true},
      {"capture off", true},
      {"capture tout", true},
      {"capture  changements", true},
      {"capture", false},
      {"capture on 2", false},
      {"capture pause", false},
      {"seuils 2 8000", true},
      {"seuils 0", true},
      {"seuils", false},
      {"injection on", true},
      {"injection off", true},
      {"injection", false},
      {"injection monte 1", false},
      {"injection on 1", false},
      {"injecte durees 750 750", true},
      {"injecte", false},
      {"wifi maison secret", false},
      {"info", false},
      {"bus", false},
      {"stats", false},
      {"help", false},
      {"reboot", false},
      {"lampe on", false},
      {"led test", false},
      {"", false},
      {"   ", false},
  };
  for (const Case &c : kCases) {
    const char *why = remoteRefusal(c.cmd);
    if ((why == nullptr) != c.ok) printf("  remoteRefusal('%s') : %s\n", c.cmd, why ? why : "permise");
    VERIF((why == nullptr) == c.ok);
    VERIF(!why || strlen(why) <= kMsgMax);
  }
}

int main(int argc, char **argv) {
  if (argc > 1) gCapture = fopen(argv[1], "wb");
  testWriter();
  testU64();
  testRenumerote();
  gCaptureOn = true;
  testTrame();
  testInjection();
  testReply();
  testEvents();
  testReponsesCle();
  gCaptureOn = false;
  testWorstCases();
  testCommandeAffichee();
  testRemote();
  if (gCapture) fclose(gCapture);
  return bilan("test_json");
}
```

- [ ] **Étape 10 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`

Attendu : ÉCHEC à l'édition des liens de `test_json`, avec « Undefined symbols for architecture arm64 », « "jsonp::remoteRefusal(char const*)", referenced from: », « "jsonp::maskCmd(char*)", referenced from: » et « ld: symbol(s) not found for architecture arm64 »

- [ ] **Étape 11 : Écrire l'implémentation minimale (commande affichée et liste blanche)**

`maskCmd` coupe `wifi <ssid> <mdp>` après le ssid : le reste de la ligne, mot de passe compris, ne part jamais.

Dans `src/json_out.cpp`, remplacer :

```cpp
}  // namespace jsonp
```

par :

```cpp
// ===========================================================================
//  Lignes de l'hote : commande affichee (reponse.cmd), liste blanche
// ===========================================================================

// Mot i (0, 1, ...) de s, separe par des espaces ; nullptr s'il manque.
static const char *word(const char *s, uint8_t i, size_t *len) {
  *len = 0;
  for (;;) {
    while (*s == ' ') s++;
    if (!*s) return nullptr;
    const char *w = s;
    while (*s && *s != ' ') s++;
    if (!i--) {
      *len = (size_t)(s - w);
      return w;
    }
  }
}

static bool wordIs(const char *w, size_t n, const char *k) { return w && strlen(k) == n && !strncmp(w, k, n); }

// Entier decimal, lu comme le lit l'aiguillage (strtoul : zeros de tete
// compris, au-dela de 4294967295 sature) ; false si le mot n'en est pas un
// (l'aiguillage dira usage). Sans cela, 'bail 0000000000' passerait la liste
// blanche puis vaudrait 0.
static bool wordNum(const char *w, size_t n, uint32_t *v) {
  if (!w || !n) return false;
  uint64_t x = 0;
  for (size_t i = 0; i < n; i++) {
    if (w[i] < '0' || w[i] > '9') return false;
    x = x * 10 + (uint64_t)(w[i] - '0');
    if (x > 0xFFFFFFFFull) x = 0xFFFFFFFFull + 1;  // sature, sans debordement
  }
  *v = x > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)x;
  return true;
}

void copyCmd(char out[kCmdTextMax + 1], const char *cmd) {
  size_t i = 0;
  for (; cmd && cmd[i] && i < kCmdTextMax; i++) out[i] = cmd[i];
  out[i] = 0;
}

void maskCmd(char *shown) {
  size_t n0, n1, n2;
  const char *w0 = word(shown, 0, &n0), *w1 = word(shown, 1, &n1), *w2 = word(shown, 2, &n2);
  if (wordIs(w0, n0, "json") && wordIs(w1, n1, "cle") && wordIs(w2, n2, "nouvelle")) strcpy(shown, "json cle nouvelle");
  // 'wifi <ssid> <mdp>' : coupe apres le ssid (reponse.cmd, cache des reponses).
  if (wordIs(w0, n0, "wifi") && w1) shown[(size_t)(w1 - shown) + n1] = 0;
}

const char *remoteRefusal(const char *cmd) {
  static const char kDenied[] = "interdite a distance (10.5) : USB seulement";
  size_t n0, n1, n2, n3, n4;
  const char *w0 = word(cmd, 0, &n0), *w1 = word(cmd, 1, &n1), *w2 = word(cmd, 2, &n2);
  const char *w3 = word(cmd, 3, &n3), *w4 = word(cmd, 4, &n4);
  uint32_t v = 0;
  if (wordIs(w0, n0, "json")) {
    if (wordIs(w1, n1, "1")) {
      // Jamais 'bail 0' a distance : la sonde emettrait pour un client parti
      // jusqu'a l'oubli de la session.
      if (wordIs(w2, n2, "bail") && wordNum(w3, n3, &v) && !w4 && (v < 10 || v > 120))
        return "json 1 : bail de 10 a 120 s a distance";
      return nullptr;
    }
    if (wordIs(w1, n1, "0") || wordIs(w1, n1, "etat") || wordIs(w1, n1, "hello") || wordIs(w1, n1, "ping") ||
        wordIs(w1, n1, "trames") || wordIs(w1, n1, "log"))
      return nullptr;
    if (wordIs(w1, n1, "periode"))
      return wordNum(w2, n2, &v) && v < 2000 ? "json periode : 2000..60000 ms a distance" : nullptr;
    if (wordIs(w1, n1, "compteurs"))
      return wordNum(w2, n2, &v) && v && v < 1000 ? "json compteurs : 0 ou 1000..60000 ms a distance" : nullptr;
    if (wordIs(w1, n1, "reseau"))
      return wordNum(w2, n2, &v) && v && v < 10000 ? "json reseau : 0 ou 10000..60000 ms a distance" : nullptr;
    return kDenied;  // 'json' seul (texte humain), 'json cle ...'
  }
  if (wordIs(w0, n0, "capture") && !w2 &&
      (wordIs(w1, n1, "on") || wordIs(w1, n1, "off") || wordIs(w1, n1, "tout") || wordIs(w1, n1, "changements")))
    return nullptr;
  // 'seuils' seul affiche du texte humain ; avec des valeurs, l'aiguillage borne.
  if (wordIs(w0, n0, "seuils") && w1) return nullptr;
  if (wordIs(w0, n0, "injection") && !w2 && (wordIs(w1, n1, "on") || wordIs(w1, n1, "off"))) return nullptr;
  if (wordIs(w0, n0, "injecte") && w1) return nullptr;
  return kDenied;  // wifi, json cle, injection monte, reboot, bus, stats, info, help...
}

}  // namespace jsonp
```

- [ ] **Étape 12 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh`

Attendu :

```text
test_capture : 24 verifications, 0 echecs
  pire trame : 860 octets (budget 896)
  pire cas des evenements : 889 octets (budget 896)
test_json : 179 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 13 : Écrire le test qui échoue (mécaniques de la session)**

Préfixe `id=`, assemblage des lignes reçues, plafonds de débit, cadence, file des périodiques, bail et cache des réponses, tels que la ScreenBar les teste.

Dans `tools/tests/test_json.cpp`, remplacer :

```cpp
int main(int argc, char **argv) {
  if (argc > 1) gCapture = fopen(argv[1], "wb");
  testWriter();
  testU64();
  testRenumerote();
  gCaptureOn = true;
  testTrame();
  testInjection();
  testReply();
  testEvents();
  testReponsesCle();
  gCaptureOn = false;
  testWorstCases();
  testCommandeAffichee();
  testRemote();
  if (gCapture) fclose(gCapture);
  return bilan("test_json");
}
```

par :

```cpp
// ---------------------------------------------------------------------------
//  Lignes de l'hote
// ---------------------------------------------------------------------------

static bool idOf(const char *in, uint32_t *id, std::string *rest) {
  char buf[160];
  snprintf(buf, sizeof(buf), "%s", in);
  char *r = nullptr;
  const bool ok = parseIdPrefix(buf, id, &r);
  *rest = r;
  return ok;
}

static void testIdPrefix() {
  uint32_t id = 0;
  std::string rest;
  VERIF(idOf("id=17 capture tout", &id, &rest) && id == 17 && rest == "capture tout");
  VERIF(idOf("  id=1   json 1", &id, &rest) && id == 1 && rest == "json 1");
  VERIF(idOf("id=999999999 json ping", &id, &rest) && id == 999999999 && rest == "json ping");
  VERIF(idOf("id=5", &id, &rest) && id == 5 && rest.empty());
  VERIF(idOf("id=007 x", &id, &rest) && id == 7);
  VERIF(!idOf("id=0 json 1", &id, &rest) && rest == "id=0 json 1");
  VERIF(!idOf("id=1000000000 json 1", &id, &rest));
  VERIF(!idOf("id=0000000001 x", &id, &rest));
  VERIF(!idOf("id= 5 x", &id, &rest));
  VERIF(!idOf("id=17capture", &id, &rest));
  VERIF(!idOf("id=-3 x", &id, &rest));
  VERIF(!idOf("capture id=3", &id, &rest));
  VERIF(!idOf("ID=3 x", &id, &rest));
}

static std::string feedAll(LineAssembler &a, const char *bytes, size_t n, bool machine, int *lines,
                           std::string *echo) {
  std::string last;
  for (size_t i = 0; i < n; i++) {
    const LineAssembler::Ev e = a.feed((uint8_t)bytes[i], machine);
    if (e == LineAssembler::Ev::Echo && echo) *echo += bytes[i];
    if (e == LineAssembler::Ev::Line) {
      last = a.text();
      last += a.tooLong() ? "|trop long" : "";
      (*lines)++;
      a.reset();
    }
  }
  return last;
}

static void testAssembler() {
  LineAssembler a;
  int lines = 0;
  std::string echo;
  const char in1[] = "id=1 json 1\r\n";
  VERIF(feedAll(a, in1, sizeof(in1) - 1, true, &lines, &echo) == "id=1 json 1" && lines == 1);
  // Mode machine : RS, echappements et octets hauts ignores.
  lines = 0;
  const char in2[] = "bu\x1e" "s\x1b\x01\xff\x80 x\n";
  VERIF(feedAll(a, in2, sizeof(in2) - 1, true, &lines, nullptr) == "bus x" && lines == 1);
  // Mode humain : tout octet est garde (sauf CR, LF, Ctrl-U, retour arriere).
  lines = 0;
  const char in3[] = "a\x01" "b\n";
  VERIF(feedAll(a, in3, sizeof(in3) - 1, false, &lines, nullptr) == std::string("a\x01" "b") && lines == 1);
  // Ctrl-U vide la ligne ; retour arriere.
  lines = 0;
  const char in4[] = "reste d'une session\x15" "\nid=2 json pinh\x08g\n";
  a.reset();
  std::string last = feedAll(a, in4, sizeof(in4) - 1, true, &lines, nullptr);
  VERIF(lines == 2 && last == "id=2 json ping");
  a.reset();
  a.feed('a', false);
  a.feed('b', false);
  VERIF(a.feed(kCtrlU, false) == LineAssembler::Ev::Clear && a.cleared() == 2 && a.length() == 0);
  VERIF(a.feed(8, false) == LineAssembler::Ev::None);
  // 127 caracteres passent, 128 marquent la ligne trop longue ; la suivante repart.
  for (int extra = 0; extra < 2; extra++) {
    a.reset();
    lines = 0;
    std::string s(kCmdMax + (size_t)extra, 'x');
    s += "\n";
    last = feedAll(a, s.c_str(), s.size(), true, &lines, nullptr);
    VERIF(lines == 1 && (extra ? last.size() == kCmdMax + strlen("|trop long") : last.size() == kCmdMax));
  }
  lines = 0;
  last = feedAll(a, "ok\n", 3, true, &lines, nullptr);
  VERIF(last == "ok");
  // Ctrl-U efface aussi le depassement.
  a.reset();
  std::string s(200, 'y');
  s += "\x15" "json\n";
  lines = 0;
  last = feedAll(a, s.c_str(), s.size(), true, &lines, nullptr);
  VERIF(last == "json");
}

// ---------------------------------------------------------------------------
//  Debit, cadence, file
// ---------------------------------------------------------------------------

static void testRate() {
  RateCap cap(50);
  uint32_t t = 0u - 300;  // a travers le retour a zero de millis()
  unsigned passed = 0;
  for (int i = 0; i < 80; i++) {
    if (cap.available(t)) {
      cap.take();
      passed++;
    } else {
      cap.skip();
    }
    t += 5;  // 80 evenements en 400 ms
  }
  VERIF(passed == 50 && cap.takeSkipped() == 30 && cap.takeSkipped() == 0);
  t += 1000;
  VERIF(cap.available(t));
  // Cadence : 20 lignes par seconde glissante.
  Cadence c;
  uint32_t now = 1000;
  int ok = 0;
  for (int i = 0; i < 25; i++) ok += c.allow(now + (uint32_t)i * 10);  // 25 lignes en 250 ms
  VERIF(ok == 20);
  VERIF(!c.allow(now + 999));
  VERIF(c.allow(now + 1000));
  VERIF(!c.allow(now + 1001));
  VERIF(c.allow(now + 1010));
}

static void testQueue() {
  Queue q;
  VERIF(q.push(Item::EtatBus, 0, true) && q.push(Item::EtatSys, 1, true) && q.size() == 2);
  VERIF(q.push(Item::EtatBus, 2, true) && q.size() == 2);  // doublon ignore
  VERIF(q.push(Item::Reply, 3, false, 1) && q.push(Item::Reply, 3, false, 2) && q.size() == 4);  // reponses jamais fondues
  VERIF(q.push(Item::EtatSys, 4, false) && q.size() == 4);  // doublon explicite
  VERIF(q.dropSession() == 1 && q.size() == 3);
  const Queued *f = q.front();
  VERIF(f && f->item == Item::EtatSys && !f->session && f->at == 4);  // promue, gardee a sa place
  q.pop();
  f = q.front();
  VERIF(f && f->item == Item::Reply && f->arg == 1);
  q.clear();
  int pushed = 0;
  for (int i = 0; i < 40; i++) pushed += q.push(Item::Reply, (uint32_t)i, false, (uint8_t)i);
  VERIF(pushed == Queue::kN && q.size() == Queue::kN);
  VERIF(!q.push(Item::Config, 50, false));
  for (int i = 0; i < 5; i++) q.pop();
  VERIF(q.push(Item::Config, 51, false) && q.has(Item::Config));
  uint8_t prev = 4;
  bool order = true;
  while (const Queued *e = q.front()) {
    if (e->item == Item::Reply) {
      order &= e->arg == prev + 1;
      prev = e->arg;
    }
    q.pop();
  }
  VERIF(order);  // FIFO a travers le tour de l'anneau
}

// Retard (500 ms) et place libre : les deux gardes de l'envoi des periodiques.
static void testQueueDrain() {
  Queue q;
  VERIF(!q.frontReady(4096) && q.dropLate(1000) == 0);
  q.push(Item::EtatBus, 0, true);
  q.push(Item::EtatSys, 100, true);
  q.push(Item::Reply, 200, false, 0);
  q.push(Item::Compteurs, 250, true);
  VERIF(q.dropLate(500) == 0 && q.size() == 4);
  VERIF(q.dropLate(700) == 2 && q.front()->item == Item::Reply);
  VERIF(q.dropLate(1000000) == 0 && q.size() == 2);  // une reponse n'est jamais perdue pour retard
  VERIF(!q.frontReady(kLineMax - 1) && q.frontReady(kLineMax));
  VERIF(!q.frontReady(-1));
  q.pop();
  VERIF(q.front()->item == Item::Compteurs && q.dropLate(1000000) == 1 && !q.size());
  q.push(Item::NetIp, 2000, true);
  VERIF(!q.frontReady(2 * kLineMax - 1) && q.frontReady(2 * kLineMax));
  q.clear();
  // Fusion : une demande explicite repart de maintenant, pas une periodique.
  q.push(Item::EtatSys, 0, true);
  q.push(Item::EtatSys, 450, false);
  VERIF(q.size() == 1 && !q.front()->session && q.front()->at == 450);
  VERIF(q.dropLate(950) == 0 && q.dropLate(951) == 1);
  q.push(Item::EtatBus, 0, true);
  q.push(Item::EtatBus, 400, true);
  VERIF(q.front()->at == 0 && q.dropLate(501) == 1);
  q.clear();
  q.push(Item::HelloBase, 0, false);
  q.push(Item::HelloBase, 300, true);
  VERIF(!q.front()->session && q.front()->at == 0);
}

static void testLease() {
  VERIF(!leaseExpired(1000000, 0, 0, 0));
  VERIF(!leaseExpired(30999, 1000, 500, 30) && leaseExpired(31000, 1000, 500, 30));
  VERIF(!leaseExpired(90000, 1000, 70000, 30) && leaseExpired(100000, 1000, 70000, 30));
  const uint32_t rx = 0xFFFFF000u;
  VERIF(!leaseExpired(rx + 29999u, rx, rx - 0x1000u, 30) && leaseExpired(rx + 30000u, rx, rx - 0x1000u, 30));
  VERIF(!leaseExpired(9999, 0, 0, 10) && leaseExpired(600000, 0, 0, 600));
}

// ---------------------------------------------------------------------------
//  Cache des reponses (sessions reseau)
// ---------------------------------------------------------------------------

// Un id repete recoit la meme reponse ; ni msg ni cle.
static void testCache() {
  ReplyCache cache;
  VERIF(!cache.find(1));
  Reply r;
  r.id = 5;
  r.cmd = "injecte durees 750 750";
  r.code = "accepte";
  r.msg = "attente du silence";
  r.suite = Reply::SuiteInjection;
  cache.put(r);
  const Reply *got = cache.find(5);
  VERIF(got && got->suite == Reply::SuiteInjection && !got->msg && !strcmp(got->cmd, r.cmd) && got->cmd != r.cmd);
  Reply debut = r;
  debut.id = 6;
  debut.fin = false;
  cache.put(debut);
  VERIF(!cache.find(6));  // etape debut jamais gardee
  for (uint32_t id = 10; id < 10 + ReplyCache::kN; id++) {
    r.id = id;
    cache.put(r);
  }
  VERIF(!cache.find(5) && cache.find(10) && cache.find(10 + ReplyCache::kN - 1));
  r.id = 12;
  r.code = "ok";
  cache.put(r);
  VERIF(cache.find(12) && !strcmp(cache.find(12)->code, "ok"));
  cache.clear();
  VERIF(!cache.find(12));
}

int main(int argc, char **argv) {
  if (argc > 1) gCapture = fopen(argv[1], "wb");
  testWriter();
  testU64();
  testRenumerote();
  gCaptureOn = true;
  testTrame();
  testInjection();
  testReply();
  testEvents();
  testReponsesCle();
  gCaptureOn = false;
  testWorstCases();
  testCommandeAffichee();
  testRemote();
  testIdPrefix();
  testAssembler();
  testRate();
  testQueue();
  testQueueDrain();
  testLease();
  testCache();
  if (gCapture) fclose(gCapture);
  return bilan("test_json");
}
```

- [ ] **Étape 14 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`

Attendu : ÉCHEC à l'édition des liens de `test_json`, avec « Undefined symbols for architecture arm64 », entre autres « "jsonp::parseIdPrefix(char*, unsigned int*, char**)", referenced from: », « "jsonp::ReplyCache::put(jsonp::Reply const&)", referenced from: » et « ld: symbol(s) not found for architecture arm64 »

- [ ] **Étape 15 : Écrire l'implémentation minimale (mécaniques de la session)**

Dans `src/json_out.cpp`, remplacer :

```cpp
}  // namespace jsonp
```

par :

```cpp
// ===========================================================================
//  Lignes de l'hote : prefixe id=, assemblage
// ===========================================================================

bool parseIdPrefix(char *line, uint32_t *id, char **rest) {
  *rest = line;
  char *p = line;
  while (*p == ' ') p++;
  if (strncmp(p, "id=", 3)) return false;
  p += 3;
  uint32_t v = 0;
  uint8_t digits = 0;
  while (*p >= '0' && *p <= '9') {
    if (++digits > 9) return false;
    v = v * 10 + (uint32_t)(*p++ - '0');
  }
  if (!digits || v < 1 || v > kIdMax || (*p && *p != ' ')) return false;
  while (*p == ' ') p++;
  *id = v;
  *rest = p;
  return true;
}

LineAssembler::Ev LineAssembler::feed(uint8_t c, bool machine) {
  if (c == '\r') return Ev::None;  // CRLF de l'hote : le CR est ignore
  if (c == '\n') {
    buf_[len_] = 0;
    return Ev::Line;
  }
  if (c == kCtrlU) {
    cleared_ = len_;
    len_ = 0;
    tooLong_ = false;
    return Ev::Clear;
  }
  if (c == 8 || c == 127) {  // retour arriere
    if (!len_) return Ev::None;
    len_--;
    return Ev::Erase;
  }
  if (machine && (c < 0x20 || c > 0x7E)) return Ev::None;
  if (len_ >= kCmdMax) {
    tooLong_ = true;
    return Ev::None;
  }
  buf_[len_++] = (char)c;
  return Ev::Echo;
}

char *LineAssembler::text() {
  buf_[len_] = 0;
  return buf_;
}

// ===========================================================================
//  Debit
// ===========================================================================

bool RateCap::available(uint32_t now) {
  if (!started_ || now - winAt_ >= 1000) {
    started_ = true;
    winAt_ = now;
    count_ = 0;
  }
  return count_ < limit_;
}

bool Cadence::allow(uint32_t now) {
  // at_[idx_] : la plus ancienne des kLines dernieres lignes acceptees.
  if (n_ >= kLines && now - at_[idx_] < kWindowMs) return false;
  at_[idx_] = now;
  idx_ = (uint8_t)((idx_ + 1) % kLines);
  if (n_ < kLines) n_++;
  return true;
}

// ===========================================================================
//  File
// ===========================================================================

bool Queue::has(Item item) const {
  for (uint8_t i = 0; i < n_; i++)
    if (q_[(head_ + i) % kN].item == item) return true;
  return false;
}

bool Queue::push(Item item, uint32_t now, bool session, uint8_t arg) {
  if (item != Item::Reply) {
    for (uint8_t i = 0; i < n_; i++) {
      Queued &q = q_[(head_ + i) % kN];
      if (q.item != item) continue;
      // Demande explicite : survit a la fin du mode machine, et son retard
      // part d'elle (la ligne est formatee a l'envoi : rien n'est perime).
      if (!session) {
        q.session = false;
        q.at = now;
      }
      return true;
    }
  }
  if (n_ >= kN) return false;
  q_[(head_ + n_) % kN] = Queued{item, arg, session, now};
  n_++;
  return true;
}

void Queue::pop() {
  if (!n_) return;
  head_ = (uint8_t)((head_ + 1) % kN);
  n_--;
}

uint8_t Queue::dropLate(uint32_t now, uint32_t lateMs) {
  uint8_t dropped = 0;
  while (n_) {
    const Queued &q = q_[head_];
    if (q.item == Item::Reply || now - q.at <= lateMs) break;
    pop();
    dropped++;
  }
  return dropped;
}

bool Queue::frontReady(int room) const {
  if (!n_) return false;
  const size_t need = q_[head_].item == Item::Reply ? kLineMax : 2 * kLineMax;
  return room >= 0 && (size_t)room >= need;
}

uint8_t Queue::dropSession() {
  Queued keep[kN];
  uint8_t k = 0, dropped = 0;
  for (uint8_t i = 0; i < n_; i++) {
    const Queued &q = q_[(head_ + i) % kN];
    if (q.session) dropped++;
    else keep[k++] = q;
  }
  for (uint8_t i = 0; i < k; i++) q_[i] = keep[i];
  head_ = 0;
  n_ = k;
  return dropped;
}

// ===========================================================================
//  Bail
// ===========================================================================

bool leaseExpired(uint32_t now, uint32_t lastRx, uint32_t lastCmd, uint16_t leaseS) {
  if (!leaseS) return false;
  const uint32_t last = (int32_t)(lastRx - lastCmd) > 0 ? lastRx : lastCmd;
  return now - last >= (uint32_t)leaseS * 1000u;
}

// ===========================================================================
//  Cache des reponses
// ===========================================================================

void ReplyCache::clear() {
  // Sur place : un objet temporaire coute pres d'un Ko de pile a la tache loop.
  for (Entry &e : e_) e.used = false;
  next_ = 0;
}

void ReplyCache::put(const Reply &r) {
  if (!r.fin || !r.id) return;
  // Une seule entree par id : la plus recente (un id repete ne s'execute pas,
  // mais une reponse differee peut suivre une reponse immediate du meme id).
  for (Entry &e : e_)
    if (e.used && e.r.id == r.id) e.used = false;
  Entry &e = e_[next_];
  next_ = (uint8_t)((next_ + 1) % kN);
  e.used = true;
  e.r = r;
  copyCmd(e.cmd, r.cmd ? r.cmd : "");
  e.r.cmd = e.cmd;
  e.r.msg = nullptr;
  e.r.key = nullptr;
  e.r.hasKid = false;
  e.r.kid = nullptr;
}

const Reply *ReplyCache::find(uint32_t id) const {
  if (!id) return nullptr;
  for (const Entry &e : e_)
    if (e.used && e.r.id == id) return &e.r;
  return nullptr;
}

}  // namespace jsonp
```

- [ ] **Étape 16 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh`

Attendu :

```text
test_capture : 24 verifications, 0 echecs
  pire trame : 860 octets (budget 896)
  pire cas des evenements : 889 octets (budget 896)
test_json : 243 verifications, 0 echecs
tests hote : OK
```

Lancer : `grep -c "" "${TMPDIR:-/tmp}/hotte-tests/test_json_lignes.txt"`

Attendu : `16` (lignes réalistes pour `json_check.py`, tâche 11)

- [ ] **Étape 17 : Vérifier que le firmware compile toujours**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|SUCCESS|FAILED"`

Attendu : une seule ligne `[SUCCESS]`, aucun `warning`

- [ ] **Étape 18 : Commit**

```bash
git add src/json_out.h src/json_out.cpp tools/tests/test_json.cpp tools/tests/test_hote.sh
git commit -m "Scinder json_out de la ScreenBar : u64, evenements trame et injection, liste blanche hotte" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 9 : `capture_model` : symboles RMT → parties, découpe à 110, mode changements

**Fichiers :**
- Modifier : `src/capture_model.cpp` (découpeur, `procheDe`, mode changements ; fichier entier ci-dessous)
- Modifier : `tools/tests/test_capture.cpp` (tests du découpeur et du mode changements ; fichier entier ci-dessous)
- Tester : `tools/tests/test_capture.cpp`

**Interfaces :**
- Consomme : `src/capture_model.h` (tâche 7, inchangé) ; les bornes de la tâche 7.
- Produit (déclarations du contrat, comportement précisé ici) :
  - `void capt::Decoupeur::reset();` : clôt la réception en cours sans rien émettre ; la numérotation continue (`num` depuis le démarrage).
  - `size_t capt::Decoupeur::traiter(const capt::Bloc &b, const capt::Reglages &r, capt::EmetPartie emit, void *ctx);` :
    - ticks → µs (`× 10⁶ / r.resolHz`), niveau du bus = niveau GPIO inversé si `r.inverse`, durées nulles retirées, deux voisines de même niveau fondues (les niveaux d'une partie restent alternés) ;
    - parties de `kDurMax` (110) durées au plus ; `tUs` de la première = `b.tFinUs − somme − (b.dernier ? r.silenceUs : 0)`, borné à 0 ; chaque partie suivante commence à la fin de la précédente ;
    - `num` à partir de 1, `part` à partir de 0 dans la réception, `fin` sur la dernière partie d'un bloc `dernier` ;
    - `b.debordAvant` : nouvelle réception (la précédente reste sans `fin`), `debord` sur sa première partie ;
    - bloc sans durée : une partie vide (`n` = 0) seulement s'il clôt une réception en cours ou s'il porte `debordAvant`, sinon rien.
  - `bool capt::procheDe(uint32_t a, uint32_t b);` : `|a−b| ≤ 10` ou `|a−b| × 20 ≤ max(a, b)`.
  - `capt::Changements` : comparable = `part == 0 && fin` ; `aEmettre` rend `false` (et `rep` avance) si la partie est comparable, sans `debord`, et proche, durée par durée, de la dernière réception ÉMISE (même `niv0Haut`, même `n`) ; toute partie émise devient la référence si elle est comparable, sinon la référence est oubliée. `prendreRep()` rend puis remet à zéro ; `repEnCours()` ; `reset()` oublie tout.

- [ ] **Étape 1 : Écrire le test qui échoue**

Les tests des bornes de la tâche 7 sont gardés tels quels ; s'y ajoutent ceux du découpeur et du mode changements.

Remplacer tout le contenu de `tools/tests/test_capture.cpp` par :

```cpp
// Tests hote de src/capture_model.* : bornes des reglages de capture, symboles
// RMT -> parties (conversion, inversion, durees nulles, decoupe a 110, t_us,
// numerotation, pertes), mode changements.
// Lancer : sh tools/tests/test_hote.sh
#include <initializer_list>

#include "capture_model.h"
#include "config.h"
#include "verif.h"

using namespace capt;

static void testBornes() {
  VERIF(resolValide(1000000));
  VERIF(resolValide(500000));
  VERIF(!resolValide(0));
  VERIF(!resolValide(250000));
  VERIF(!resolValide(2000000));
  VERIF(filtreValide(0));
  VERIF(filtreValide(3));
  VERIF(!filtreValide(4));
  VERIF(!filtreValide(255));
  // Silence : 1000 us au moins, 32767 ticks au plus (32767 us a 1 MHz, 65534 us a 500 kHz).
  VERIF(silenceValide(1000, 1000000));
  VERIF(!silenceValide(999, 1000000));
  VERIF(silenceValide(32767, 1000000));
  VERIF(!silenceValide(32768, 1000000));
  VERIF(silenceValide(65534, 500000));
  VERIF(!silenceValide(65535, 500000));
  VERIF(!silenceValide(999, 500000));
  VERIF(!silenceValide(0, 1000000));
  VERIF(!silenceValide(4294967295u, 500000));  // produit sur 64 bits : pas de debordement
}

static void testBorner() {
  Reglages r;
  VERIF(r.resolHz == kResolDefautHz && r.filtreUs == kFiltreDefautUs && r.silenceUs == kSilenceDefautUs && r.inverse);
  VERIF(borner(&r) == 0);
  r.resolHz = kResolLenteHz;
  r.filtreUs = 3;
  r.silenceUs = 65534;
  r.inverse = false;
  VERIF(borner(&r) == 0 && r.resolHz == kResolLenteHz && r.filtreUs == 3 && r.silenceUs == 65534 && !r.inverse);
  // Silence valable a 500 kHz mais pas a 1 MHz : remplace par sa valeur par defaut.
  r.resolHz = kResolDefautHz;
  VERIF(borner(&r) == 1 && r.silenceUs == kSilenceDefautUs && r.filtreUs == 3);
  // Tout hors bornes (NVS corrompue) : trois valeurs remplacees.
  r.resolHz = 123;
  r.filtreUs = 200;
  r.silenceUs = 5;
  VERIF(borner(&r) == 3 && r.resolHz == kResolDefautHz && r.filtreUs == kFiltreDefautUs &&
        r.silenceUs == kSilenceDefautUs);
  // Resolution remplacee d'abord : le silence est juge avec la resolution par defaut.
  r.resolHz = 0;
  r.silenceUs = 40000;
  VERIF(borner(&r) == 2 && r.resolHz == kResolDefautHz && r.silenceUs == kSilenceDefautUs);
}

// ---------------------------------------------------------------------------
//  Decoupeur : symboles RMT -> parties
// ---------------------------------------------------------------------------

static Partie gParts[8];
static size_t gN = 0;

static void collecte(const Partie &p, void *ctx) {
  VERIF(ctx == &gN);
  if (gN < 8) gParts[gN] = p;
  gN++;
}

static size_t traite(Decoupeur &d, const Bloc &b, const Reglages &r) {
  gN = 0;
  const size_t n = d.traiter(b, r, collecte, &gN);
  VERIF(n == gN);
  return n;
}

// Somme des durees d'une partie.
static uint64_t somme(const Partie &p) {
  uint64_t s = 0;
  for (uint16_t i = 0; i < p.n; i++) s += p.dur[i];
  return s;
}

// Une trame courte : 750 750 750 2250 100, fin de reception (demi-symbole final nul).
// Niveaux GPIO : 0 pendant d0, 1 pendant d1 ; etage inverseur : GPIO bas = bus haut.
static const Sym kCourte[3] = {{750, 0, 750, 1}, {750, 0, 2250, 1}, {100, 0, 0, 1}};

static void testConversion() {
  Decoupeur d;
  Reglages r;  // 1 MHz, inverse, silence 5000 us
  VERIF(traite(d, Bloc{1000000, true, false, 3, kCourte}, r) == 1);
  const Partie &p = gParts[0];
  VERIF(p.num == 1 && p.part == 0 && p.fin && !p.debord && p.niv0Haut && p.n == 5);
  VERIF(p.dur[0] == 750 && p.dur[1] == 750 && p.dur[2] == 750 && p.dur[3] == 2250 && p.dur[4] == 100);
  VERIF(p.tUs == 1000000u - 4600u - 5000u);  // fin - somme - silence
  VERIF(d.receptions() == 1);
  // 500 kHz : un tick vaut 2 us.
  r.resolHz = kResolLenteHz;
  r.silenceUs = 40000;
  VERIF(traite(d, Bloc{2000000, true, false, 3, kCourte}, r) == 1);
  VERIF(gParts[0].num == 2 && gParts[0].n == 5 && gParts[0].dur[0] == 1500 && gParts[0].dur[3] == 4500 &&
        gParts[0].dur[4] == 200);
  VERIF(gParts[0].tUs == 2000000u - 9200u - 40000u);
  // Etage non inverseur : GPIO bas = bus bas.
  r = Reglages();
  r.inverse = false;
  VERIF(traite(d, Bloc{3000000, true, false, 3, kCourte}, r) == 1);
  VERIF(!gParts[0].niv0Haut && gParts[0].n == 5);
}

static void testDureesNulles() {
  Decoupeur d;
  const Reglages r;
  // Nulle en tete : la premiere duree est celle de d1 (GPIO 1 : bus bas).
  const Sym tete[2] = {{0, 0, 300, 1}, {400, 0, 0, 1}};
  VERIF(traite(d, Bloc{1000000, true, false, 2, tete}, r) == 1);
  VERIF(gParts[0].n == 2 && !gParts[0].niv0Haut && gParts[0].dur[0] == 300 && gParts[0].dur[1] == 400);
  // Nulle au milieu : les deux durees voisines de meme niveau sont fondues (les niveaux restent alternes).
  const Sym milieu[3] = {{100, 0, 200, 1}, {0, 0, 50, 1}, {80, 0, 0, 1}};
  VERIF(traite(d, Bloc{1000000, true, false, 3, milieu}, r) == 1);
  VERIF(gParts[0].n == 3 && gParts[0].dur[0] == 100 && gParts[0].dur[1] == 250 && gParts[0].dur[2] == 80);
  VERIF(gParts[0].tUs == 1000000u - 430u - 5000u);
}

static void testDecoupe() {
  Decoupeur d;
  const Reglages r;
  // 150 symboles de 10 + 20 us, le dernier sans d1 : 299 durees -> 110 + 110 + 79.
  static Sym s[150];
  for (Sym &x : s) x = Sym{10, 0, 20, 1};
  s[149].d1 = 0;
  const uint64_t fin = 50000000;
  VERIF(traite(d, Bloc{fin, true, false, 150, s}, r) == 3);
  VERIF(gParts[0].n == 110 && gParts[1].n == 110 && gParts[2].n == 79);
  VERIF(somme(gParts[0]) == 1650 && somme(gParts[1]) == 1650 && somme(gParts[2]) == 1180);
  // t_us successifs : chaque partie commence ou la precedente finit.
  VERIF(gParts[0].tUs == fin - 4480 - 5000);
  VERIF(gParts[1].tUs == gParts[0].tUs + 1650 && gParts[2].tUs == gParts[1].tUs + 1650);
  // Niveaux alternes d'une partie a la suivante : la premiere duree de la
  // partie k+1 a le niveau oppose de la derniere de la partie k.
  for (int k = 0; k < 2; k++) {
    const Partie &a = gParts[k];
    const bool dernierHaut = a.niv0Haut != ((a.n - 1) % 2 == 1);
    VERIF(gParts[k + 1].niv0Haut == !dernierHaut);
  }
  VERIF(gParts[0].niv0Haut && gParts[1].dur[0] == 10 && gParts[2].dur[78] == 10);
  // Une seule reception : meme num, rangs 0..2, fin sur la derniere seulement.
  for (int k = 0; k < 3; k++) VERIF(gParts[k].num == 1 && gParts[k].part == (uint32_t)k && !gParts[k].debord);
  VERIF(!gParts[0].fin && !gParts[1].fin && gParts[2].fin);
  // Exactement 110 durees : une seule partie, pas de partie vide derriere.
  static Sym t[55];
  for (Sym &x : t) x = Sym{10, 0, 20, 1};
  VERIF(traite(d, Bloc{fin, true, false, 55, t}, r) == 1 && gParts[0].n == 110 && gParts[0].fin);
}

static void testReceptions() {
  Decoupeur d;
  const Reglages r;
  const Sym a[3] = {{100, 0, 200, 1}, {100, 0, 200, 1}, {100, 0, 200, 1}};
  const Sym b[2] = {{100, 0, 50, 1}, {70, 0, 0, 1}};
  // Reception en deux blocs : le premier sans fin (tampon du pilote plein), sans silence retranche.
  VERIF(traite(d, Bloc{2000000, false, false, 3, a}, r) == 1);
  VERIF(gParts[0].num == 1 && gParts[0].part == 0 && !gParts[0].fin && gParts[0].tUs == 2000000u - 900u);
  VERIF(traite(d, Bloc{2005300, true, false, 2, b}, r) == 1);
  VERIF(gParts[0].num == 1 && gParts[0].part == 1 && gParts[0].fin && gParts[0].tUs == 2005300u - 220u - 5000u);
  // La suivante porte le numero 2.
  VERIF(traite(d, Bloc{3000000, true, false, 2, b}, r) == 1 && gParts[0].num == 2 && gParts[0].part == 0);
  VERIF(d.receptions() == 2);
  // Perte avant un bloc : la reception en cours est abandonnee (sans fin), une nouvelle commence.
  VERIF(traite(d, Bloc{4000000, false, false, 3, a}, r) == 1 && gParts[0].num == 3);
  VERIF(traite(d, Bloc{4100000, true, true, 2, b}, r) == 1);
  VERIF(gParts[0].num == 4 && gParts[0].part == 0 && gParts[0].fin && gParts[0].debord);
  // reset() : la reception en cours est close, la numerotation continue.
  VERIF(traite(d, Bloc{5000000, false, false, 3, a}, r) == 1 && gParts[0].num == 5);
  d.reset();
  VERIF(traite(d, Bloc{5100000, true, false, 2, b}, r) == 1 && gParts[0].num == 6 && gParts[0].part == 0);
  VERIF(d.receptions() == 6);
}

static void testBlocsVides() {
  Decoupeur d;
  const Reglages r;
  const Sym a[1] = {{100, 0, 200, 1}};
  // Bloc final vide qui clot une reception en cours : partie vide, fin.
  VERIF(traite(d, Bloc{1000000, false, false, 1, a}, r) == 1);
  VERIF(traite(d, Bloc{1010000, true, false, 0, nullptr}, r) == 1);
  VERIF(gParts[0].num == 1 && gParts[0].part == 1 && gParts[0].fin && gParts[0].n == 0);
  VERIF(gParts[0].tUs == 1010000u - 5000u);
  // Bloc vide hors reception, ou seulement des durees nulles : rien, aucune reception comptee.
  const Sym zero[1] = {{0, 0, 0, 1}};
  VERIF(traite(d, Bloc{1020000, true, false, 0, nullptr}, r) == 0);
  VERIF(traite(d, Bloc{1030000, true, false, 1, zero}, r) == 0);
  VERIF(traite(d, Bloc{1040000, false, false, 1, zero}, r) == 0);
  VERIF(d.receptions() == 1);
  // Bloc vide apres une perte : une partie vide porte debord.
  VERIF(traite(d, Bloc{1050000, true, true, 0, nullptr}, r) == 1);
  VERIF(gParts[0].num == 2 && gParts[0].debord && gParts[0].fin && gParts[0].n == 0);
  // Horloge plus courte que la reception (impossible en service) : t_us borne a 0.
  VERIF(traite(d, Bloc{1000, true, false, 1, a}, r) == 1 && gParts[0].tUs == 0);
}

// ---------------------------------------------------------------------------
//  Mode changements
// ---------------------------------------------------------------------------

static void testProcheDe() {
  VERIF(procheDe(100, 100));
  VERIF(procheDe(100, 110) && procheDe(110, 100));  // 10 us au moins
  VERIF(!procheDe(100, 111));
  VERIF(procheDe(0, 10) && !procheDe(0, 11));
  VERIF(procheDe(1000, 1050) && !procheDe(1000, 1053));  // 5 % du plus grand
  VERIF(procheDe(1000000, 1050000) && !procheDe(1000000, 1060000));
  VERIF(procheDe(4294967295u, 4294967294u) && !procheDe(0, 4294967295u));  // sans debordement
}

static Partie simple(std::initializer_list<uint32_t> dur, bool niv0Haut = true) {
  Partie p;
  p.num = 1;
  p.fin = true;
  p.niv0Haut = niv0Haut;
  for (uint32_t x : dur) p.dur[p.n++] = x;
  return p;
}

static void testChangements() {
  Changements c;
  const Partie p1 = simple({750, 750, 750, 2250});
  VERIF(c.aEmettre(p1) && c.prendreRep() == 0);
  VERIF(!c.aEmettre(p1) && c.repEnCours() == 1);
  VERIF(!c.aEmettre(p1) && c.repEnCours() == 2);
  // A 5 % pres : identique.
  VERIF(!c.aEmettre(simple({760, 745, 750, 2300})) && c.repEnCours() == 3);
  // Une duree hors tolerance : emise, elle porte les 3 repetitions.
  const Partie p2 = simple({750, 800, 750, 2250});
  VERIF(c.aEmettre(p2) && c.prendreRep() == 3 && c.repEnCours() == 0 && c.prendreRep() == 0);
  // Nombre de durees ou niveau de depart differents : emise.
  VERIF(c.aEmettre(simple({750, 800, 750})));
  VERIF(c.aEmettre(simple({750, 800, 750}, false)));
  // Comparaison a la derniere EMISE : une derive lente finit par partir.
  const Partie q0 = simple({1000}), q1 = simple({1040}), q2 = simple({1080});
  VERIF(c.aEmettre(q0));
  VERIF(!c.aEmettre(q1));  // 40 us de 1000 : identique
  VERIF(c.aEmettre(q2));   // 80 us de 1000 : differente (40 us de 1040 seulement)
  VERIF(c.prendreRep() == 1);
  // Reception en plusieurs parties : pas comparable, toujours emise ; elle
  // devient la derniere emise, la reception simple suivante part aussi.
  Partie longue = simple({1080});
  longue.fin = false;
  VERIF(c.aEmettre(longue));
  longue.part = 1;
  longue.fin = true;
  VERIF(c.aEmettre(longue));
  VERIF(c.aEmettre(q2) && !c.aEmettre(q2));
  // Une perte (debord) part toujours, meme identique.
  Partie perte = q2;
  perte.debord = true;
  VERIF(c.aEmettre(perte) && c.prendreRep() == 1);
  // reset : plus de reference, plus de repetitions.
  VERIF(!c.aEmettre(q2) && c.repEnCours() == 1);
  c.reset();
  VERIF(c.repEnCours() == 0 && c.aEmettre(q2));
}

int main() {
  testBornes();
  testBorner();
  testConversion();
  testDureesNulles();
  testDecoupe();
  testReceptions();
  testBlocsVides();
  testProcheDe();
  testChangements();
  return bilan("test_capture");
}
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`

Attendu : ÉCHEC à l'édition des liens avec « "capt::Decoupeur::traiter(capt::Bloc const&, capt::Reglages const&, void (*)(capt::Partie const&, void*), void*)", referenced from » et « ld: symbol(s) not found »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Les bornes de la tâche 7 sont inchangées ; s'y ajoutent le découpeur et le mode changements.

Remplacer tout le contenu de `src/capture_model.cpp` par :

```cpp
// ===========================================================================
//  Modele pur de la capture (docs/SPEC-RECONNAISSANCE.md section 8.2) : bornes
//  des reglages, symboles RMT -> parties publiees (lignes trame), mode
//  changements. Sans Arduino ni IDF : teste sur l'hote (tools/tests/test_capture.cpp).
// ===========================================================================
#include "capture_model.h"

#include "config.h"

namespace capt {

// ===========================================================================
//  Bornes
// ===========================================================================

bool resolValide(uint32_t hz) { return hz == kResolDefautHz || hz == kResolLenteHz; }

// Filtre du RMT : 255 cycles de 80 MHz au plus, soit 3,19 us.
bool filtreValide(uint32_t us) { return us <= kFiltreMaxUs; }

// Seuil de silence du RMT : kSilenceMaxTicks ticks au plus (registre sur 15 bits).
bool silenceValide(uint32_t us, uint32_t resolHz) {
  return us >= kSilenceMinUs && (uint64_t)us * resolHz <= (uint64_t)kSilenceMaxTicks * 1000000u;
}

uint8_t borner(Reglages *r) {
  const Reglages d;
  uint8_t n = 0;
  // La resolution d'abord : la borne du silence en depend.
  if (!resolValide(r->resolHz)) {
    r->resolHz = d.resolHz;
    n++;
  }
  if (!filtreValide(r->filtreUs)) {
    r->filtreUs = d.filtreUs;
    n++;
  }
  if (!silenceValide(r->silenceUs, r->resolHz)) {
    r->silenceUs = d.silenceUs;
    n++;
  }
  return n;
}

// ===========================================================================
//  Decoupeur
//
//  Une reception va du premier front au silence (bloc dernier). Le pilote la
//  rend en un bloc, ou en plusieurs si elle depasse son tampon (blocs non
//  derniers) : ses parties se suivent (part 0, 1, ...), la derniere porte fin.
//  Un bloc marque debordAvant ouvre une nouvelle reception : la precedente
//  reste sans fin, la continuite est perdue.
//
//  reset() clot la reception en cours sans rien emettre ; la numerotation
//  continue (num : depuis le demarrage).
//
//  Le pilote alterne les niveaux d'un demi-symbole au suivant ; si une duree
//  nulle retiree en laissait deux voisines de meme niveau, elles seraient
//  fondues, pour que les niveaux d'une partie restent alternes (niv0Haut dit
//  celui de dur[0]).
// ===========================================================================

void Decoupeur::reset() {
  part_ = 0;
  enCours_ = false;
}

// Ticks -> us : exact a 1 MHz (x1) et a 500 kHz (x2).
static uint32_t versUs(uint16_t ticks, uint32_t hz) {
  return hz ? (uint32_t)((uint64_t)ticks * 1000000u / hz) : ticks;
}

size_t Decoupeur::traiter(const Bloc &b, const Reglages &r, EmetPartie emit, void *ctx) {
  if (b.debordAvant) enCours_ = false;  // continuite perdue : nouvelle reception
  uint64_t total = 0;
  for (uint16_t i = 0; i < b.nSym; i++) total += versUs(b.sym[i].d0, r.resolHz) + versUs(b.sym[i].d1, r.resolHz);
  // Rien a publier, sauf pour clore une reception en cours ou signaler une perte.
  if (!total && !b.debordAvant && !(b.dernier && enCours_)) {
    if (b.dernier) enCours_ = false;
    return 0;
  }
  if (!enCours_) {
    num_++;
    part_ = 0;
    enCours_ = true;
  }
  const uint64_t retrait = total + (b.dernier ? r.silenceUs : 0);
  Partie p;
  p.num = num_;
  p.part = part_;
  p.tUs = b.tFinUs > retrait ? b.tFinUs - retrait : 0;
  p.debord = b.debordAvant;
  uint64_t dansPartie = 0;  // somme des durees de p
  bool dernierHaut = false;
  size_t emises = 0;
  for (uint16_t i = 0; i < b.nSym; i++) {
    for (uint8_t moitie = 0; moitie < 2; moitie++) {
      const uint32_t us = versUs(moitie ? b.sym[i].d1 : b.sym[i].d0, r.resolHz);
      if (!us) continue;
      const bool gpioHaut = (moitie ? b.sym[i].l1 : b.sym[i].l0) != 0;
      const bool haut = r.inverse ? !gpioHaut : gpioHaut;
      if (p.n && haut == dernierHaut) {  // voisine de meme niveau : fondue
        p.dur[p.n - 1] += us;
        dansPartie += us;
        continue;
      }
      if (p.n == kDurMax) {  // partie pleine : elle part, la suivante commence a sa fin
        emit(p, ctx);
        emises++;
        part_++;
        p.part = part_;
        p.tUs += dansPartie;
        p.debord = false;
        p.n = 0;
        dansPartie = 0;
      }
      if (!p.n) p.niv0Haut = haut;
      p.dur[p.n++] = us;
      dansPartie += us;
      dernierHaut = haut;
    }
  }
  p.fin = b.dernier;
  emit(p, ctx);
  emises++;
  part_++;
  if (b.dernier) enCours_ = false;
  return emises;
}

// ===========================================================================
//  Mode changements
// ===========================================================================

bool procheDe(uint32_t a, uint32_t b) {
  const uint32_t haut = a > b ? a : b, ecart = a > b ? a - b : b - a;
  return ecart <= 10 || (uint64_t)ecart * 20 <= haut;  // 5 % = 1/20
}

void Changements::reset() {
  a_ = false;
  n_ = 0;
  rep_ = 0;
}

bool Changements::aEmettre(const Partie &p) {
  // Comparable : une reception d'une seule partie. Une perte (debord) part
  // toujours, pour que la marque arrive.
  const bool comparable = p.part == 0 && p.fin;
  if (comparable && !p.debord && a_ && p.niv0Haut == niv0_ && p.n == n_) {
    bool pareil = true;
    for (uint16_t i = 0; i < n_ && pareil; i++) pareil = procheDe(p.dur[i], dur_[i]);
    if (pareil) {
      rep_++;
      return false;
    }
  }
  // Emise : elle devient la derniere reception emise (sans reference si elle
  // n'est pas comparable : la suivante partira).
  a_ = comparable;
  if (comparable) {
    niv0_ = p.niv0Haut;
    n_ = p.n;
    for (uint16_t i = 0; i < n_; i++) dur_[i] = p.dur[i];
  }
  return true;
}

uint32_t Changements::prendreRep() {
  const uint32_t r = rep_;
  rep_ = 0;
  return r;
}

}  // namespace capt
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh`

Attendu :

```text
test_capture : 140 verifications, 0 echecs
  pire trame : 860 octets (budget 896)
  pire cas des evenements : 889 octets (budget 896)
test_json : 243 verifications, 0 echecs
tests hote : OK
```

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|SUCCESS|FAILED"`

Attendu : une seule ligne `[SUCCESS]`, aucun `warning`

- [ ] **Étape 5 : Commit**

```bash
git add src/capture_model.cpp tools/tests/test_capture.cpp
git commit -m "Ecrire le decoupeur de la capture : parties de 110 durees, horodatage, mode changements" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 10 : Capture RMT continue, fronts GPIO et console USB

**Fichiers :**
- Créer : `src/capture_rmt.h`
- Créer : `src/capture_rmt.cpp`
- Créer : `src/bord.h`
- Créer : `src/bord.cpp`
- Créer : `src/sonde.h`
- Créer : `src/cli.h`
- Créer : `src/cli.cpp`
- Modifier : `src/main.cpp` (fichier entier ci-dessous)
- Tester : `~/.platformio/penv/bin/pio run -e sonde` ; `sh tools/tests/test_hote.sh` (inchangé). Pas de test sur carte ici : c'est la tâche 16.

**Comportement du pilote RMT (lu dans `framework-espidf/components/esp_driver_rmt/src/rmt_rx.c`, IDF 5.5, et `rmt_rx.h` / `rmt_types.h` du cœur 3.3.12) :**
- sur C6, un canal RX a 48 symboles de mémoire (`SOC_RMT_MEM_WORDS_PER_CHANNEL`) ; sans DMA, le pilote la vide par moitiés de 24 (ping-pong, interruption de seuil) dans le tampon passé à `rmt_receive` ;
- avec `flags.en_partial_rx = 1`, le rappel `on_recv_done` est appelé avec `is_last = false` quand ce tampon est plein et qu'une moitié de plus arrive : `received_symbols` est le tampon entier, réécrit dès le retour du rappel. Il est appelé avec `is_last = true` au silence (seuil d'inactivité) : c'est la fin de la réception, éventuellement vide ;
- après `is_last`, le canal est arrêté (état « enable ») : il faut rappeler `rmt_receive`, ce qui est permis en interruption ; on le fait dans le rappel ;
- l'interruption n'est pas allouée en IRAM (`CONFIG_RMT_RX_ISR_CACHE_SAFE` absent du cœur) : elle attend la fin d'une écriture en flash, et `rmt_receive`, qui est en flash, peut être appelé du rappel ;
- horloge `RMT_CLK_SRC_DEFAULT` = PLL 80 MHz, diviseur de groupe 1 : le filtre compte à 80 MHz (255 cycles au plus, soit 3,19 µs), le seuil de silence en ticks du canal (32 767 au plus) ;
- conséquence pour l'horodatage du §8.2 (`tFinUs − somme`) : un bloc non final est rendu après la moitié suivante (24 symboles), ou après les derniers symboles et le silence s'il sort à la fin de réception. Son `t_us` est en retard d'autant. Seules les réceptions de plus de 240 symboles sont touchées (limite écrite en tête de `capture_rmt.cpp`, à mesurer au banc).

**Interfaces :**
- Consomme : `capt::Reglages`, `capt::borner`, `capt::Sym`, `capt::Bloc`, `capt::Decoupeur`, `capt::Changements`, `capt::EmetPartie` (tâches 7 et 9) ; `ReglagesSonde`, `reglagesCharger`, `reglagesSauver` (tâche 7) ; `jsonp::LineAssembler`, `jsonp::RateCap`, `jsonp::kCmdMax` (tâche 8) ; `src/config.h`.
- Produit :
  - `src/capture_rmt.h` du contrat, à la lettre. `captureBegin` borne ses réglages, abandonne les blocs non lus, remet le découpeur à zéro (`reset`, la numérotation continue), crée le canal RX sur `kPinEcoute` (tampon du pilote de 240 symboles), l'arme ; `false` et `log_e` si IDF échoue. `captureEnd` arrête et libère le canal. `capturePoll` vide l'anneau (2 048 symboles, 64 en-têtes) bloc par bloc sans dépasser `maxParties`, sauf pour le premier bloc d'un appel. `CaptureStats` : `receptions` (découpeur), `parties` (émises), `blocs` et `symboles` (rendus par le pilote, perdus compris), `debord` (blocs perdus, anneau plein). `captureReglages()` : réglages en vigueur, bornés.
  - `src/bord.h` du contrat, à la lettre. `gpio_config` (entrée, pull-up, `GPIO_INTR_ANYEDGE`), `gpio_install_isr_service(0)` (`ESP_ERR_INVALID_STATE` toléré), `gpio_isr_handler_add` ; rappel en IRAM ; `bordBusHaut()` applique `captureReglages().inverse` ; `bordDernierUs()` rend 0 avant le premier front.
  - `src/sonde.h` : `ReglagesSonde &sondeReglages();`, `bool sondeAppliquer(const ReglagesSonde &r);` (définis dans `main.cpp`). `sondeAppliquer` relance la capture (`captureBegin`, qui fait `captureEnd` d'abord) seulement si elle tourne et que ses réglages changent, remet `sChangements` à zéro si le mode change, puis appelle `reglagesSauver`. La tâche 11 y ajoute `jsonConfigChanged()`.
  - `src/cli.h` : `void cliBegin();`, `void cliPoll();` (la tâche 19 ajoute `cliRunRemote`).
  - `src/cli.cpp`, points d'accroche de `01-taches.md` : `typedef void (*Handler)(char *args);`, `struct Commande { const char *nom; Handler h; const char *aide; };`, `static const Commande kCommandes[]` (7 lignes : `help`, `info`, `capture`, `seuils`, `bus`, `stats`, `reboot`), `static bool executer(char *line)`, `static void runLine(char *line)`, `static jsonp::LineAssembler sLine`. Les handlers sont définis au-dessus de la table, sauf `cmdHelp` (déclaré au-dessus, défini après, car il parcourt la table). Outils statiques réutilisables : `splitWord`, `lireU32`, `silenceMaxUs`, `afficherReglages(const ReglagesSonde &r)`. `cliPoll` passe `machine = false` à `sLine.feed` ; la tâche 11 y met `jsonMachine()` et `jsonNoteRx()`.
  - `src/main.cpp` : `static ReglagesSonde sReglages;`, `static capt::Changements sChangements;`, `static jsonp::RateCap sTexte(20);`, `static void afficherTrame(const capt::Partie &p, bool hasRep, uint32_t rep)` (format `trame <num>.<part>[ fin] t=<t_us> <haut|bas> <d1> <d2> ...`, suivi de ` debord`, ` rep=<n>`, ` (<n> lignes sautees)` s'il y a lieu ; jamais d'attente : sans 1 024 octets libres dans le tampon d'émission de l'USB, `Serial.availableForWrite()`, la ligne est perdue et comptée avec les lignes sautées), `static void surPartie(const capt::Partie &p, void *)` (mode changements : `aEmettre`, puis `hasRep = true`, `rep = prendreRep()`). `loop()` : `cliPoll(); capturePoll(surPartie, nullptr, 8); vTaskDelay(1);`. La tâche 11 ajoute `jsonBegin()` après `Serial.begin`, `jsonTrame(p, hasRep, rep)` dans `surPartie` (et n'y affiche le texte qu'hors mode machine) et `jsonPoll()` après `capturePoll`.

**Écarts (assumés) :** `seuils` prend un troisième argument facultatif `resol_hz` (1 000 000 ou 500 000). Sinon, la résolution de 500 kHz prévue au §8.2 ne serait réglable par aucune commande. L'aide de la table devient donc `seuils [filtre_us 0..3] [silence_us] [resol_hz]`.

**Texte humain sans attente (§8.7, « la sonde n'attend jamais le client ») :** `HWCDC::write` attend jusqu'à 2 s (20 fois 100 ms) quand le Mac reste branché sans lire, par exemple après la fermeture du moniteur. Pendant ce temps, la boucle ne viderait plus l'anneau de la capture. `afficherTrame` n'écrit donc une ligne que si le tampon d'émission a la place d'une ligne entière (1 024 octets, `Serial.availableForWrite()`) ; sinon la ligne est perdue et comptée avec les lignes sautées du plafond, signalées sur la suivante. Le tampon d'émission passe de 256 octets à 8 Ko dès cette tâche (`Serial.setTxBufferSize(8192)` avant `Serial.begin`) : à 256 octets, aucune ligne de plus de 256 octets n'aurait la place. La tâche 11 garde ce tampon pour les lignes machine.

- [ ] **Étape 1 : Écrire le test qui échoue**

Le « test » de cette tâche est la compilation du firmware complet. On écrit d'abord `src/main.cpp` tel qu'il doit être : il appelle les modules pas encore écrits.

Remplacer tout le contenu de `src/main.cpp` par :

```cpp
// ===========================================================================
//  Sonde de reconnaissance de la ligne D (docs/SPEC-RECONNAISSANCE.md)
//
//  setup() : GPIO7 bas en premier, reglages NVS, interruption des fronts,
//  capture RMT, console. loop() : console, capture (parties -> texte ou
//  lignes trame), puis la tache IDLE.
// ===========================================================================
#include <Arduino.h>

#include "bord.h"
#include "capture_rmt.h"
#include "cli.h"
#include "config.h"
#include "fw_version.h"
#include "json_out.h"
#include "reglages.h"
#include "sonde.h"

static ReglagesSonde sReglages;
static capt::Changements sChangements;  // mode changements : receptions d'une partie
static jsonp::RateCap sTexte(20);       // lignes trame affichees par seconde (mode humain)
static char sLigne[1024];

ReglagesSonde &sondeReglages() { return sReglages; }

static bool memeCapture(const capt::Reglages &a, const capt::Reglages &b) {
  return a.resolHz == b.resolHz && a.filtreUs == b.filtreUs && a.silenceUs == b.silenceUs && a.inverse == b.inverse;
}

bool sondeAppliquer(const ReglagesSonde &r) {
  ReglagesSonde n = r;
  capt::borner(&n.capture);  // deja fait par l'appelant ; jamais de valeur hors bornes au RMT
  const bool relance = captureActive() && !memeCapture(n.capture, sReglages.capture);
  if (n.changements != sReglages.changements) sChangements.reset();
  sReglages = n;
  bool ok = true;
  if (relance) ok = captureBegin(sReglages.capture);  // captureEnd() d'abord
  if (!reglagesSauver(sReglages)) ok = false;
  return ok;
}

// Mode humain : 'trame <num>.<part>[ fin] t=<t_us> <haut|bas> <d1> <d2> ...',
// 20 lignes par seconde au plus. Jamais d'attente (spec 8.7) : sans la place
// d'une ligne entiere dans le tampon d'emission de l'USB (Mac qui ne lit
// plus), la ligne est perdue, car Serial.write bloquerait la boucle jusqu'a
// 2 s. Lignes sautees (plafond) et perdues (tampon plein) sont comptees
// ensemble et signalees sur la suivante.
static void afficherTrame(const capt::Partie &p, bool hasRep, uint32_t rep) {
  if (!sTexte.available(millis()) || Serial.availableForWrite() < (int)sizeof(sLigne)) {
    sTexte.skip();
    return;
  }
  sTexte.take();
  const size_t cap = sizeof(sLigne) - 64;  // place pour les suffixes et le saut de ligne
  size_t k = (size_t)snprintf(sLigne, sizeof(sLigne), "trame %lu.%lu%s t=%llu %s", (unsigned long)p.num,
                              (unsigned long)p.part, p.fin ? " fin" : "", (unsigned long long)p.tUs,
                              p.niv0Haut ? "haut" : "bas");
  for (uint16_t i = 0; i < p.n && k < cap; i++)
    k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " %lu", (unsigned long)p.dur[i]);
  if (p.debord) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " debord");
  if (hasRep) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " rep=%lu", (unsigned long)rep);
  const uint32_t sautees = sTexte.takeSkipped();
  if (sautees) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " (%lu lignes sautees)", (unsigned long)sautees);
  sLigne[k++] = '\n';
  Serial.write((const uint8_t *)sLigne, k);
}

// Chaque partie produite par la capture (capturePoll).
static void surPartie(const capt::Partie &p, void *) {
  bool hasRep = false;
  uint32_t rep = 0;
  if (sReglages.changements) {
    if (!sChangements.aEmettre(p)) return;  // identique a la derniere emise : comptee
    hasRep = true;
    rep = sChangements.prendreRep();
  }
  afficherTrame(p, hasRep, rep);
}

void setup() {
  // TOUJOURS la premiere instruction (spec 8.4) : la base de l'etage
  // d'injection a l'etat bas avant tout le reste.
  pinMode(kPinInjection, OUTPUT);
  digitalWrite(kPinInjection, LOW);
  // Tampon d'emission de l'USB (HWCDC) : 256 octets par defaut, moins qu'une
  // ligne trame de 110 durees. 8 Ko : afficherTrame n'ecrit qu'une ligne qui
  // tient entiere, et une dizaine de lignes tiennent pendant que le Mac lit.
  Serial.setTxBufferSize(8192);
  Serial.begin(115200);
  const uint8_t remplaces = reglagesCharger(&sReglages);
  Serial.printf("firmware %s (%s)\n", FW_VERSION_FULL, FW_ENV);
  if (remplaces) Serial.printf("[reglages] %u valeur(s) hors bornes en NVS : valeur(s) par defaut\n", remplaces);
  if (!bordBegin(kPinEcoute)) Serial.println("[bord] interruption des fronts indisponible");
  if (!captureBegin(sReglages.capture)) Serial.println("[capture] echec du RMT ('capture on' pour reessayer)");
  cliBegin();
}

void loop() {
  cliPoll();
  capturePoll(surPartie, nullptr, 8);
  vTaskDelay(1);  // laisse tourner la tache IDLE
}
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `~/.platformio/penv/bin/pio run -e sonde`

Attendu : ÉCHEC avec « src/main.cpp:10:10: fatal error: bord.h: No such file or directory »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Capture RMT (en-tête du contrat, à la lettre) :

Créer `src/capture_rmt.h` :

```cpp
#pragma once
#include "capture_model.h"

bool captureBegin(const capt::Reglages &r);  // GPIO kPinEcoute ; false : echec IDF (log)
void captureEnd();
bool captureActive();
// Tache loop : vide le tampon circulaire, au plus maxParties parties par appel.
size_t capturePoll(capt::EmetPartie emit, void *ctx, size_t maxParties);
struct CaptureStats {
  uint32_t receptions, parties, blocs, symboles, debord;
};
CaptureStats captureStats();
const capt::Reglages &captureReglages();
```

Créer `src/capture_rmt.cpp` :

```cpp
// ===========================================================================
//  Capture continue de la ligne D par le pilote RMT d'IDF (spec 8.2)
//
//  Canal RX sur GPIO6 (kPinEcoute), une memoire de 48 symboles (le pilote la
//  vide par moities de 24, en ping-pong), reception partielle
//  (flags.en_partial_rx). Le pilote copie chaque moitie pleine dans sRecu et
//  appelle le rappel on_recv_done (esp_driver_rmt/src/rmt_rx.c, IDF 5.5) :
//   - is_last = false quand sRecu est plein et qu'une moitie de plus arrive
//     (reception de plus de kSymRecus symboles) : edata rend sRecu entier,
//     que le pilote reecrit des le retour du rappel ;
//   - is_last = true au silence (seuil d'inactivite du canal) : edata rend la
//     fin de la reception, eventuellement vide. Le canal est alors arrete ; le
//     rappel le rearme aussitot (rmt_receive est permis en interruption).
//  Le rappel (en IRAM) horodate le bloc (esp_timer_get_time) et le copie dans
//  l'anneau sAnneau de kTamponSymboles symboles, avec son en-tete, sous une
//  section critique (sMux). La tache loop vide l'anneau (capturePoll) a
//  travers le decoupeur (capture_model). Anneau ou en-tetes pleins : le bloc
//  est perdu et compte (debord), le suivant porte debordAvant.
//
//  L'interruption du RMT n'est pas sure pendant une ecriture en flash
//  (CONFIG_RMT_RX_ISR_CACHE_SAFE absent du coeur) : elle attend la fin de
//  l'ecriture, et le rappel peut appeler rmt_receive, qui est en flash.
//
//  Limites connues :
//   - un bloc non final est rendu apres la moitie suivante (24 symboles), ou,
//     s'il est rendu par la fin de reception, apres les derniers symboles et
//     le silence : son t_us (tFinUs - somme, spec 8.2) est en retard d'autant.
//     Seules les receptions de plus de kSymRecus symboles (480 durees) sont
//     concernees ;
//   - pendant une ecriture en NVS (commandes seuils, capture tout|changements),
//     plus de 24 symboles d'affilee ecraseraient la memoire du canal ;
//   - captureBegin() abandonne les blocs pas encore lus.
// ===========================================================================
#include "capture_rmt.h"

#include <Arduino.h>
#include <driver/rmt_rx.h>
#include <esp_attr.h>
#include <esp_timer.h>

#include "config.h"

namespace {

constexpr size_t kMemCanal = 48;   // SOC_RMT_MEM_WORDS_PER_CHANNEL : un bloc, ping-pong de 24
constexpr size_t kSymRecus = 240;  // tampon du pilote : 10 moities
constexpr uint32_t kEntetes = 64;  // blocs en attente dans l'anneau (puissance de 2)

struct Entete {
  uint64_t tFinUs;
  uint32_t debut;  // indice du premier symbole dans sAnneau
  uint16_t n;
  bool dernier, debordAvant;
};

rmt_channel_handle_t sCanal = nullptr;
rmt_receive_config_t sRecv = {};
capt::Reglages sReglages;
capt::Decoupeur sDecoupeur;
volatile bool sActif = false;  // faux : le rappel ne rearme plus le canal

// Tampon du pilote, lu dans le rappel seulement.
rmt_symbol_word_t sRecu[kSymRecus];

// Anneau : ecrit par le rappel, lu par la tache loop. Compteurs libres (indice
// = compteur modulo la taille, puissances de 2), sous sMux.
portMUX_TYPE sMux = portMUX_INITIALIZER_UNLOCKED;
rmt_symbol_word_t sAnneau[kTamponSymboles];
Entete sEntetes[kEntetes];
uint32_t sSymEcrits = 0, sSymLus = 0, sEntEcrits = 0, sEntLus = 0;
bool sPerte = false;
uint32_t sBlocs = 0, sSymboles = 0, sDebord = 0;

// Tache loop seulement.
uint32_t sParties = 0;
capt::Sym sSym[kSymRecus];  // bloc en cours de decoupage

bool IRAM_ATTR surReception(rmt_channel_handle_t canal, const rmt_rx_done_event_data_t *e, void *) {
  const uint64_t t = esp_timer_get_time();
  const uint32_t n = (uint32_t)e->num_symbols;
  portENTER_CRITICAL_ISR(&sMux);
  sBlocs++;
  sSymboles += n;
  if (n > kTamponSymboles - (sSymEcrits - sSymLus) || sEntEcrits - sEntLus >= kEntetes) {
    sPerte = true;  // bloc perdu : le suivant porte debordAvant
    sDebord++;
  } else {
    Entete &h = sEntetes[sEntEcrits % kEntetes];
    h.tFinUs = t;
    h.debut = sSymEcrits % kTamponSymboles;
    h.n = (uint16_t)n;
    h.dernier = e->flags.is_last;
    h.debordAvant = sPerte;
    for (uint32_t i = 0; i < n; i++) sAnneau[(sSymEcrits + i) % kTamponSymboles] = e->received_symbols[i];
    sSymEcrits += n;
    sEntEcrits++;
    sPerte = false;
  }
  portEXIT_CRITICAL_ISR(&sMux);
  // Fin de reception : le pilote a arrete le canal ; on le rearme tout de suite.
  if (e->flags.is_last && sActif) rmt_receive(canal, sRecu, sizeof(sRecu), &sRecv);
  return false;
}

bool echec(const char *quoi, esp_err_t err) {
  log_e("[capture] %s : %s", quoi, esp_err_to_name(err));
  captureEnd();
  return false;
}

}  // namespace

bool captureBegin(const capt::Reglages &r) {
  captureEnd();
  sReglages = r;
  capt::borner(&sReglages);
  portENTER_CRITICAL(&sMux);
  sSymEcrits = sSymLus = sEntEcrits = sEntLus = 0;
  sPerte = false;
  portEXIT_CRITICAL(&sMux);
  sDecoupeur.reset();

  rmt_rx_channel_config_t c = {};
  c.gpio_num = (gpio_num_t)kPinEcoute;
  c.clk_src = RMT_CLK_SRC_DEFAULT;  // PLL 80 MHz : filtre jusqu'a 255 cycles (3,19 us)
  c.resolution_hz = sReglages.resolHz;
  c.mem_block_symbols = kMemCanal;
  esp_err_t err = rmt_new_rx_channel(&c, &sCanal);
  if (err != ESP_OK) {
    sCanal = nullptr;
    return echec("rmt_new_rx_channel", err);
  }
  rmt_rx_event_callbacks_t cb = {};
  cb.on_recv_done = surReception;
  err = rmt_rx_register_event_callbacks(sCanal, &cb, nullptr);
  if (err != ESP_OK) return echec("rmt_rx_register_event_callbacks", err);
  err = rmt_enable(sCanal);
  if (err != ESP_OK) return echec("rmt_enable", err);
  sRecv = {};
  sRecv.signal_range_min_ns = (uint32_t)sReglages.filtreUs * 1000u;  // 0 : filtre coupe
  sRecv.signal_range_max_ns = sReglages.silenceUs * 1000u;           // silence : fin de reception
  sRecv.flags.en_partial_rx = 1;
  sActif = true;
  err = rmt_receive(sCanal, sRecu, sizeof(sRecu), &sRecv);
  if (err != ESP_OK) return echec("rmt_receive", err);
  return true;
}

void captureEnd() {
  sActif = false;
  if (!sCanal) return;
  rmt_disable(sCanal);  // arrete le canal et son interruption
  rmt_del_channel(sCanal);
  sCanal = nullptr;
}

bool captureActive() { return sActif && sCanal; }

size_t capturePoll(capt::EmetPartie emit, void *ctx, size_t maxParties) {
  size_t emises = 0;
  while (emises < maxParties) {
    Entete h{};
    portENTER_CRITICAL(&sMux);
    const bool vide = sEntLus == sEntEcrits;
    if (!vide) h = sEntetes[sEntLus % kEntetes];
    portEXIT_CRITICAL(&sMux);
    if (vide) break;
    // Parties que ce bloc peut donner ; au-dela de maxParties, il attend le tour suivant.
    const size_t besoin = h.n ? (2u * h.n + capt::kDurMax - 1) / capt::kDurMax : 1;
    if (emises && emises + besoin > maxParties) break;
    // Hors section critique : le rappel n'ecrit jamais dans un bloc pas encore lu.
    for (uint16_t i = 0; i < h.n; i++) {
      const rmt_symbol_word_t &s = sAnneau[(h.debut + i) % kTamponSymboles];
      sSym[i] = capt::Sym{(uint16_t)s.duration0, (uint8_t)s.level0, (uint16_t)s.duration1, (uint8_t)s.level1};
    }
    portENTER_CRITICAL(&sMux);
    sSymLus += h.n;
    sEntLus++;
    portEXIT_CRITICAL(&sMux);
    const capt::Bloc b{h.tFinUs, h.dernier, h.debordAvant, h.n, sSym};
    const size_t n = sDecoupeur.traiter(b, sReglages, emit, ctx);
    sParties += n;
    emises += n;
  }
  return emises;
}

CaptureStats captureStats() {
  CaptureStats s;
  portENTER_CRITICAL(&sMux);
  s.blocs = sBlocs;
  s.symboles = sSymboles;
  s.debord = sDebord;
  portEXIT_CRITICAL(&sMux);
  s.receptions = sDecoupeur.receptions();
  s.parties = sParties;
  return s;
}

const capt::Reglages &captureReglages() { return sReglages; }
```

Fronts de la ligne (en-tête du contrat, à la lettre) :

Créer `src/bord.h` :

```cpp
#pragma once
#include <stdint.h>

bool bordBegin(uint8_t pin);   // service ISR d'IDF (ESP_ERR_INVALID_STATE tolere), deux fronts
uint64_t bordDernierUs();      // esp_timer du dernier front vu
bool bordBusHaut();            // niveau du bus maintenant (inversion de l'etage comprise)
uint32_t bordFronts();         // fronts vus depuis le demarrage
```

Créer `src/bord.cpp` :

```cpp
// ===========================================================================
//  Fronts de la ligne D (spec 8.4) : interruption GPIO sur les deux fronts de
//  la broche d'ecoute, par le service ISR d'IDF (pas d'attachInterrupt
//  d'Arduino). Elle tient l'heure du dernier front (silence avant une
//  injection) et le nombre de fronts ; elle coexiste avec le canal RMT sur la
//  meme broche. Le niveau du bus se lit sur la broche, remis dans le sens du
//  bus selon l'etage (captureReglages().inverse).
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
uint64_t sDernierUs = 0;  // sous sMux : deux mots de 32 bits
volatile uint32_t sFronts = 0;

void IRAM_ATTR surFront(void *) {
  const uint64_t t = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&sMux);
  sDernierUs = t;
  sFronts = sFronts + 1;
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
```

Accès de la console aux réglages en vigueur :

Créer `src/sonde.h` :

```cpp
#pragma once
#include "reglages.h"
ReglagesSonde &sondeReglages();          // reglages en vigueur (main.cpp)
// Nouveaux reglages (deja bornes par l'appelant) : capture relancee (captureEnd
// + captureBegin) si elle tourne et que ses reglages changent, puis
// reglagesSauver ; jsonConfigChanged (tache 11). false : echec du RMT ou de la NVS.
bool sondeAppliquer(const ReglagesSonde &r);
```

Console :

Créer `src/cli.h` :

```cpp
#pragma once
#include <stdint.h>
void cliBegin();                                  // tache 10
void cliPoll();                                   // tache 10 : lit l'USB, execute les lignes
```

Créer `src/cli.cpp` :

```cpp
// ===========================================================================
//  Console de la sonde sur l'USB (spec 8.3) : texte humain, 'help' liste tout.
//
//  Les commandes sont dans kCommandes : les taches suivantes y AJOUTENT des
//  lignes (et leurs handlers au-dessus), sans rien renommer. Les commandes
//  interdites a distance le sont par jsonp::remoteRefusal (json_out), pas par
//  la table.
// ===========================================================================
#include "cli.h"

#include <Arduino.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <string.h>

#include "bord.h"
#include "capture_rmt.h"
#include "config.h"
#include "fw_version.h"
#include "json_out.h"
#include "sonde.h"

typedef void (*Handler)(char *args);   // args : ce qui suit le mot-cle (peut etre "")
struct Commande {
  const char *nom;      // mot-cle
  Handler h;
  const char *aide;     // une ligne pour 'help'
};

// ---------------------------------------------------------------------------
//  Outils
// ---------------------------------------------------------------------------

// Detache le premier mot de s et rend le reste (jamais nul, espaces de tete sautes).
static char *splitWord(char *s) {
  char *sp = strchr(s, ' ');
  if (!sp) return s + strlen(s);
  *sp++ = 0;
  while (*sp == ' ') sp++;
  return sp;
}

// Entier decimal strict : chiffres seulement, 4294967295 au plus.
static bool lireU32(const char *s, uint32_t *v) {
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

// Silence maximal a une resolution : kSilenceMaxTicks ticks.
static uint32_t silenceMaxUs(uint32_t resolHz) {
  return (uint32_t)((uint64_t)kSilenceMaxTicks * 1000000u / resolHz);
}

static void afficherReglages(const ReglagesSonde &r) {
  Serial.printf("capture %s, mode %s : GPIO%u, %lu Hz, filtre %u us, silence %lu us, etage %s\n",
                captureActive() ? "active" : "arretee", r.changements ? "changements" : "tout", (unsigned)kPinEcoute,
                (unsigned long)r.capture.resolHz, (unsigned)r.capture.filtreUs, (unsigned long)r.capture.silenceUs,
                r.capture.inverse ? "inverseur" : "direct");
}

// ---------------------------------------------------------------------------
//  Commandes
// ---------------------------------------------------------------------------

static void cmdHelp(char *args);  // apres la table, qu'elle parcourt

static void cmdInfo(char *) {
  uint8_t mac[8] = {};  // 8 octets : esp_read_mac peut ecrire une EUI-64
  Serial.printf("firmware %s (env %s), IDF %s, Arduino %s\n", FW_VERSION_FULL, FW_ENV, esp_get_idf_version(),
                ESP_ARDUINO_VERSION_STR);
  if (esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK)
    Serial.printf("MAC %02X:%02X:%02X:%02X:%02X:%02X, serie HOTTE-%02X%02X%02X%02X%02X%02X\n", mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  else Serial.println("MAC illisible");
  Serial.printf("ecoute GPIO%u, injection GPIO%u (tenue basse)\n", (unsigned)kPinEcoute, (unsigned)kPinInjection);
  afficherReglages(sondeReglages());
}

static void cmdCapture(char *args) {
  ReglagesSonde r = sondeReglages();
  if (!strcmp(args, "on")) {
    if (!captureActive() && !captureBegin(r.capture)) {
      Serial.println("capture : echec du demarrage du RMT");
      return;
    }
  } else if (!strcmp(args, "off")) {
    captureEnd();
  } else if (!strcmp(args, "tout") || !strcmp(args, "changements")) {
    r.changements = !strcmp(args, "changements");
    if (!sondeAppliquer(r)) Serial.println("capture : mode non enregistre en NVS");
  } else if (*args) {
    Serial.println("usage : capture on|off|tout|changements");
    return;
  }
  afficherReglages(sondeReglages());
}

// seuils [filtre_us] [silence_us] [resol_hz] : tout est verifie avant d'ecrire (spec 8.2).
static void cmdSeuils(char *args) {
  ReglagesSonde r = sondeReglages();
  uint32_t v[3] = {};
  uint8_t n = 0;
  for (char *w = args; *w;) {
    char *reste = splitWord(w);
    if (n == 3 || !lireU32(w, &v[n])) {
      Serial.println("usage : seuils [filtre_us 0..3] [silence_us] [resol_hz 1000000|500000]");
      return;
    }
    n++;
    w = reste;
  }
  if (!n) {
    afficherReglages(r);
    return;
  }
  capt::Reglages c = r.capture;
  if (!capt::filtreValide(v[0])) {
    Serial.printf("seuils : filtre %lu us hors bornes (0..%u)\n", (unsigned long)v[0], (unsigned)kFiltreMaxUs);
    return;
  }
  c.filtreUs = (uint8_t)v[0];
  if (n >= 3) {
    if (!capt::resolValide(v[2])) {
      Serial.printf("seuils : resolution %lu Hz refusee (1000000 ou 500000)\n", (unsigned long)v[2]);
      return;
    }
    c.resolHz = v[2];
  }
  if (n >= 2) c.silenceUs = v[1];
  if (!capt::silenceValide(c.silenceUs, c.resolHz)) {
    Serial.printf("seuils : silence %lu us hors bornes a %lu Hz (%lu..%lu)\n", (unsigned long)c.silenceUs,
                  (unsigned long)c.resolHz, (unsigned long)kSilenceMinUs, (unsigned long)silenceMaxUs(c.resolHz));
    return;
  }
  r.capture = c;
  if (!sondeAppliquer(r)) Serial.println("seuils : echec du RMT ou de la NVS");
  afficherReglages(sondeReglages());
}

static void cmdBus(char *) {
  const bool haut = bordBusHaut();
  const uint64_t dernier = bordDernierUs();
  Serial.printf("bus %s, %lu fronts depuis le demarrage", haut ? "haut" : "bas", (unsigned long)bordFronts());
  if (!dernier) {
    Serial.println(", aucun encore");
    return;
  }
  const uint64_t age = (uint64_t)esp_timer_get_time() - dernier;
  Serial.printf(", dernier il y a %llu ms", (unsigned long long)(age / 1000));
  if (age >= sondeReglages().capture.silenceUs) Serial.printf(" : repos %s\n", haut ? "haut" : "bas");
  else Serial.println(" : reception en cours");
}

static void cmdStats(char *) {
  const CaptureStats s = captureStats();
  Serial.printf("receptions %lu, parties %lu, blocs %lu, symboles %lu, debord %lu (blocs perdus, tampon plein)\n",
                (unsigned long)s.receptions, (unsigned long)s.parties, (unsigned long)s.blocs,
                (unsigned long)s.symboles, (unsigned long)s.debord);
}

static void cmdReboot(char *) {
  Serial.println("redemarrage");
  Serial.flush();
  delay(100);
  ESP.restart();
}

// Table des commandes : les taches suivantes AJOUTENT des lignes a cette table
// (et leurs handlers au-dessus), sans rien renommer.
static const Commande kCommandes[] = {
  {"help", cmdHelp, "liste des commandes"},
  {"info", cmdInfo, "version, MAC, reglages de capture"},
  {"capture", cmdCapture, "capture on|off|tout|changements"},
  {"seuils", cmdSeuils, "seuils [filtre_us 0..3] [silence_us] [resol_hz]"},
  {"bus", cmdBus, "niveau du bus, repos, fronts"},
  {"stats", cmdStats, "compteurs de capture"},
  {"reboot", cmdReboot, "redemarrage"},
};

static void cmdHelp(char *) {
  Serial.println("=== Commandes ===");
  for (const Commande &c : kCommandes) Serial.printf("  %-10s %s\n", c.nom, c.aide);
}

// Decoupe le mot-cle, cherche dans kCommandes ; false : inconnue.
static bool executer(char *line) {
  while (*line == ' ') line++;
  size_t n = strlen(line);
  while (n && line[n - 1] == ' ') line[--n] = 0;
  if (!*line) return true;
  char *args = splitWord(line);
  for (const Commande &c : kCommandes) {
    if (!strcmp(line, c.nom)) {
      c.h(args);
      return true;
    }
  }
  return false;
}

// Une ligne de l'hote (tache 11 : prefixe id=, cadence, reponse).
static void runLine(char *line) {
  while (*line == ' ') line++;
  if (!executer(line)) Serial.printf("commande inconnue : %s ('help')\n", line);
}

// ---------------------------------------------------------------------------

static jsonp::LineAssembler sLine;  // 127 caracteres au plus

void cliBegin() {
  Serial.println("Tape 'help' pour la liste des commandes.");
  Serial.print("> ");
}

// Echo, effacement, Ctrl-U (vide la ligne), commande, invite.
void cliPoll() {
  while (Serial.available()) {
    const uint8_t c = (uint8_t)Serial.read();
    switch (sLine.feed(c, false)) {
      case jsonp::LineAssembler::Ev::Echo: Serial.print((char)c); break;
      case jsonp::LineAssembler::Ev::Erase: Serial.print("\b \b"); break;
      case jsonp::LineAssembler::Ev::Clear:
        for (uint8_t i = 0; i < sLine.cleared(); i++) Serial.print("\b \b");
        break;
      case jsonp::LineAssembler::Ev::Line:
        Serial.println();
        if (sLine.tooLong()) Serial.printf("ligne de plus de %u caracteres : ignoree\n", (unsigned)jsonp::kCmdMax);
        else runLine(sLine.text());
        sLine.reset();
        Serial.flush();  // l'USB CDC du C6 perd des octets si on enchaine trop vite
        Serial.print("> ");
        break;
      case jsonp::LineAssembler::Ev::None: break;
    }
  }
}
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|RAM:|Flash:|SUCCESS|FAILED"`

Attendu :

```text
RAM:   [=         ]   9.0% (used 29384 bytes from 327680 bytes)
Flash: [==        ]  22.8% (used 2986xx bytes from 1310720 bytes)
========================= [SUCCESS] Took ... seconds =========================
```
Aucun `warning`. Les octets de flash varient un peu avec le hash de version.

Lancer : `~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-nm -C .pio/build/sonde/firmware.elf | grep -E "surReception|surFront"`

Attendu : deux symboles à une adresse en `0x408…` (IRAM), par exemple « `40800316 t (anonymous namespace)::surReception(rmt_channel_t*, rmt_rx_done_event_data_t const*, void*)` » et « `408002dc t (anonymous namespace)::surFront(void*)` »

Lancer : `sh tools/tests/test_hote.sh | tail -1`

Attendu : « `tests hote : OK` » (tests hôte inchangés)

- [ ] **Étape 5 : Commit**

```bash
git add src/capture_rmt.h src/capture_rmt.cpp src/bord.h src/bord.cpp src/sonde.h src/cli.h src/cli.cpp src/main.cpp
git commit -m "Brancher la capture RMT continue, les fronts de GPIO6 et la console USB" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

