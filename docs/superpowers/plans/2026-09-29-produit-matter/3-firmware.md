# Partie 3 : firmware du produit et documents

Plan : [2026-09-29-produit-matter.md](../2026-09-29-produit-matter.md) (contraintes globales, écarts à la spec).

Le firmware sur la carte : son test est la compilation des trois
environnements. Les documents : procédure du banc Matter, README, réponse à
Q22.

### Tâche 10 : Firmware du produit : environnement `produit`, Matter, compagnon, build

Le firmware n'a pas de test sur l'hôte : son test est la compilation
(tâche entière), puis le banc Matter (tâches 12 à 18). Les modules purs des
tâches 1 à 7 portent toute la logique ; ceux-ci les relient à la pile Matter,
à la console USB et au transport UDP sur Thread. Les copies passent par des
scripts qui vérifient chaque texte remplacé.

**Fichiers :**
- Modifier : `platformio.ini`
- Créer : `src/produit/config_produit.h`
- Créer : `src/produit/produit.h`
- Créer : `src/produit/reglages_produit.h`
- Créer : `src/produit/reglages_produit.cpp`
- Créer : `src/produit/alim.h`
- Créer : `src/produit/alim.cpp`
- Créer : `src/produit/matter_hotte.h`
- Créer : `src/produit/matter_hotte.cpp`
- Créer : `src/produit/net_udp_thread.h`
- Créer : `src/produit/net_udp_thread.cpp`
- Créer : `src/produit/json_mode_produit.h`
- Créer : `src/produit/json_mode_produit.cpp`
- Créer : `src/produit/cli_produit.h`
- Créer : `src/produit/cli_produit.cpp`
- Créer : `src/produit/main_produit.cpp`
- Modifier : `tools/tests/test_hote.sh`

**Interfaces :**
- Consomme : tout ce qui précède (`hotte::Automate`, `sim::PiloteSimule`, `hotte_map`, `surv::Surveillance`,
  `json_out_produit`), `src/h1_proto.*`, `src/h1_crypto.cpp`, `src/app_desc.c`, `src/fw_version.h`.
- Produit : l'environnement PlatformIO `produit` (pilote simulé) ; `config_produit.h` (broches, identité,
  codes d'appairage `kDiscriminateur` = 0xFA1 et `kCodeAppairage` = 45924130, vérifiés à la compilation) ;
  `produit.h` (`produitAutomate()`, `produitPilote()`, `produitSurveillance()`, `produitReglerAutomate`,
  `produitReglerSurveillance`, `produitEcrireEnAttente`, `produitEssai`, `produitReglerEssai`, `produitSimu`,
  `produitReglerSimu`, `produitSimuApresDemarrage`, `produitSimuAuto`) ; `matter_hotte.h`
  (`matterHotteBegin`, `matterHottePoll`, `matterHottePublier`, `matterHotteDerniere`,
  `matterHotteDecommission`, `matterReglerMed|Tx|Maxint|Derniere`, `matterOtTryLock`, ...) ;
  `net_udp_thread.h` (transport de la ScreenBar, plus `netUdpRafale`) ; `json_mode_produit.h`
  (`jsonOrdreApp`, `jsonSequence`, `jsonHotte`, `jsonAlerte`, `jsonTrameD`, ...) ; `cli_produit.h`.

- [ ] **Étape 1 : Annoncer le produit en tête de `platformio.ini`**

Dans `platformio.ini`, remplacer :

```ini
; Hotte Haier CCHP21EBB : sonde de reconnaissance de la ligne D (sous-projet 1,
; docs/SPEC-RECONNAISSANCE.md), sur ESP32-C6 SuperMini.
```

par :

```ini
; Hotte Haier CCHP21EBB, sur ESP32-C6 SuperMini : sonde de reconnaissance de la
; ligne D (sous-projet 1, docs/SPEC-RECONNAISSANCE.md) et son generateur de
; banc ; module produit Matter sur Thread (sous-projet 2, docs/SPEC-PRODUIT.md).
```

- [ ] **Étape 2 : Écarter `src/produit/` de la sonde**

Dans `platformio.ini`, remplacer :

```ini
; gen_main.cpp est le firmware du generateur (env generateur) : pas dans la sonde.
build_src_filter = +<*> -<gen_main.cpp>
```

par :

```ini
; gen_main.cpp est le firmware du generateur (env generateur) : pas dans la
; sonde ; src/produit/ est le module du produit (env produit).
build_src_filter = +<*> -<gen_main.cpp> -<produit/>
```

- [ ] **Étape 3 : Ajouter l'environnement `produit`**

Les quatre lignes du rôle Thread vont ensemble (spec 4.7) ; `MATTER_THREAD_MED=0` : éligible routeur par défaut.

Dans `platformio.ini`, remplacer :

```ini
build_src_filter = -<*> +<gen_main.cpp> +<motifs.cpp> +<app_desc.c>
```

par :

```ini
build_src_filter = -<*> +<gen_main.cpp> +<motifs.cpp> +<app_desc.c>

; ---------------------------------------------------------------------------
; Produit (sous-projet 2, docs/SPEC-PRODUIT.md 4.7) : noeud Matter sur Thread,
; EP1 Ventilateur, EP2 Lumiere, canal compagnon profil "hotte". Pilote de ligne
; SIMULE tant que l'etape 6 n'a pas choisi A, B ou C (second plan) : aucune
; broche pilotee, GPIO1 tenue basse. Image Matter : partitions huge_app
; (3 Mo d'application, pas d'OTA). Premier flash (Thread) : -t erase, puis
; -t upload.
; ---------------------------------------------------------------------------
[env:produit]
board_build.partitions = huge_app.csv
; Modules du produit, plus les modules purs partages avec la sonde.
build_src_filter = -<*> +<produit/> +<h1_proto.cpp> +<h1_crypto.cpp> +<app_desc.c>
build_flags =
    ${env.build_flags}
    -DPILOTE_SIMULE=1
    -DPILOTE_LIGNE_TEXTE='"simule"'
    ; Les quatre lignes du role Thread vont ensemble (4.7). MATTER_THREAD_MED=0 :
    ; eligible routeur quand la NVS est vide ('matter med 1' : MED des l'init).
    ; L'enveloppe et l'option de l'editeur de liens : l'une sans l'autre ne lie pas.
    -DMATTER_NET_THREAD=1
    -DMATTER_THREAD_MED=0
    -DHALO_WRAP_THREAD_DEVTYPE=1
    -Wl,--wrap=_ZN4chip11DeviceLayer8Internal40GenericThreadStackManagerImpl_OpenThreadINS0_22ThreadStackManagerImplEE20_SetThreadDeviceTypeENS0_19ConnectivityManager16ThreadDeviceTypeE
```

- [ ] **Étape 4 : Écrire `src/produit/config_produit.h`**

Codes d'appairage tirés au hasard le 29/09 (discriminateur de 12 bits
différent de 0xF00 ; code hors de la liste interdite, lue dans la pile :
`chip::PayloadContents::IsValidSetupPIN` de `libespressif__esp_matter.a`).
Les `static_assert` les vérifient à chaque compilation (Q32).

```cpp
#pragma once
// ===========================================================================
//  Constantes du produit (docs/SPEC-PRODUIT.md 6.2, 7, 8) : broches de la
//  SuperMini violette, identite Matter, codes d'appairage, NVS, reseau. Pur :
//  inclus aussi par les tests hote (les static_assert y sont verifies).
// ===========================================================================
#include <stdint.h>

// --- Broches (6.2) ----------------------------------------------------------
// Ecoute et injection cote USB, loin de l'antenne ; aucune n'a de tirage au reset.
constexpr uint8_t kPinEcouteD = 0;     // ecoute de D (voie 1) : pilotes A, B, C (second plan)
constexpr uint8_t kPinInjectionD = 1;  // injection (voie 1) : a l'etat bas des la premiere instruction
constexpr uint8_t kPinAlimHotte = 2;   // ADC1 canal 2 : + de CN3, pont 100 k / 100 k
constexpr uint8_t kPinAlimModule = 3;  // ADC1 canal 3 : apres diode et fusible, au 470 uF
constexpr uint8_t kPinLedRgb = 8;      // WS2812 de la carte : mise au noir au demarrage (8.6)
constexpr uint32_t kPontRapport = 2;   // tension = 2 x tension a l'ADC (pont 100 k / 100 k)

// --- NVS et reseau -----------------------------------------------------------
constexpr const char *kNvsProduit = "produit";  // espace de noms du produit (4.4)
constexpr uint16_t kPortUdp = 5480;             // UDP sur Thread, comme la ScreenBar (7.2)

// --- Identite (8.1) ----------------------------------------------------------
#define MATTER_VENDOR_NAME "Djoko-CLI"
#define MATTER_PRODUCT_NAME "Module hotte Haier"
#define MATTER_NODE_LABEL "Hotte"
#define MATTER_SERIAL_PREFIX "HOTTE-"
#define MATTER_HW_VERSION 1
#ifndef PILOTE_LIGNE_TEXTE
#define PILOTE_LIGNE_TEXTE "simule"
#endif
#define MATTER_HW_VERSION_STRING "C6 SuperMini + pilote " PILOTE_LIGNE_TEXTE

// --- Codes d'appairage (8.2) -------------------------------------------------
// Tires au hasard le 29/09/2026 (module secrets de Python), distincts de ceux
// de la ScreenBar (0xF00, 20202021). Publics, comme les siens : ils ne servent
// que pendant la fenetre de mise en service. JAMAIS changes apres la premiere
// mise en service.
constexpr uint16_t kDiscriminateur = 0xFA1;
constexpr uint32_t kCodeAppairage = 45924130;

// Regle de la pile (chip::PayloadContents::IsValidSetupPIN, esp_matter 1.5.1,
// lue dans libespressif__esp_matter.a le 29/09) : 1..99999998, sauf 11111111
// a 88888888, 12345678 et 87654321.
constexpr bool codeAppairageValide(uint32_t c) {
  return c >= 1 && c <= 99999998 && c != 11111111 && c != 22222222 && c != 33333333 && c != 44444444 &&
         c != 55555555 && c != 66666666 && c != 77777777 && c != 88888888 && c != 12345678 && c != 87654321;
}
static_assert(codeAppairageValide(kCodeAppairage), "code d'appairage refuse par la pile");
static_assert(kCodeAppairage != 20202021, "code d'appairage de la ScreenBar");
static_assert(kDiscriminateur <= 0xFFF && kDiscriminateur != 0xF00, "discriminateur : 12 bits, pas celui de la ScreenBar");
```

- [ ] **Étape 5 : Vérifier les codes d'appairage dans `tools/tests/test_hote.sh`**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
echo "tests hote : OK"
```

par :

```sh
# Constantes du produit : les static_assert des codes d'appairage (spec 8.2, Q32).
echo '#include "config_produit.h"' | $CXXP -fsyntax-only -x c++ -
echo "tests hote : OK"
```

- [ ] **Étape 6 : Lancer les tests hôte**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
tests hote : OK
```

- [ ] **Étape 7 : Lancer le build : il échoue (ni `setup` ni `loop`)**

```bash
~/.platformio/penv/bin/pio run -e produit
```

Attendu : **échec**, avec :

```
========================== [FAILED] Took 6.16 seconds ==========================
produit        FAILED    00:00:06.156
```

- [ ] **Étape 8 : Écrire `src/produit/produit.h`**

```cpp
#pragma once
// ===========================================================================
//  Coeur du produit, tenu par main_produit.cpp (tache loop seulement) :
//  automate, pilote de ligne, surveillance, reglages en NVS.
// ===========================================================================
#include "hotte_etat.h"
#include "pilote_ligne.h"
#include "surveillance.h"
#if PILOTE_SIMULE
#include "pilote_simule.h"
#endif

hotte::Automate &produitAutomate();
ligne::Pilote &produitPilote();
surv::Surveillance &produitSurveillance();
// Jeux de reglages hors bornes lus en NVS au demarrage, remplaces par les
// defauts : annonces a chaque ouverture de session machine.
uint8_t produitHorsBornes();
// 'hotte regle' : reglages deja valides, appliques puis enregistres en NVS.
// false : NVS en echec (appliques quand meme).
bool produitReglerAutomate(const hotte::Params &p);
bool produitReglerSurveillance(const surv::Params &p);
// Ecrit tout de suite ce qui attend en NVS (derniere_v, etat_pub, temp_max) :
// avant 'reboot' et 'decommission' (4.4).
void produitEcrireEnAttente();
// Essai de 24 h ('essai alim on|off', USB) : autorise 'radio rafale' a
// distance ; s'efface seul apres 72 h de fonctionnement cumule.
bool produitEssai();
bool produitReglerEssai(bool on);
uint32_t produitEssaiResteMin();
#if PILOTE_SIMULE
sim::PiloteSimule &produitSimu();
// Reglages de la hotte simulee : appliques au pilote et a l'automate (mode,
// lecture annexe, delai moteur), enregistres en NVS (simu).
bool produitReglerSimu(const sim::Reglages &r);
// Etat de la hotte simulee au prochain demarrage, une fois (NVS simu_dem).
bool produitSimuApresDemarrage(const sim::EtatHotte &e);
// Endurance (banc B11) : un appui du panneau toutes les periodeS secondes,
// dans un cycle qui revient a eteinte, lampe eteinte ; 0 : arret. Pas en NVS.
void produitSimuAuto(uint32_t periodeS);
uint32_t produitSimuAutoS();
#endif
```

- [ ] **Étape 9 : Écrire `src/produit/reglages_produit.h` (NVS, espace `produit`)**

```cpp
#pragma once
// ===========================================================================
//  NVS du produit, espace "produit" (docs/SPEC-PRODUIT.md 4.4) : reglages de
//  l'automate et de la surveillance (bornes appliquees au chargement),
//  derniere vitesse, dernier etat publie, maximum de temperature depuis la
//  pose, essai de 24 h, et, dans le build simule, la hotte simulee. Les
//  reglages Matter (med, tx_dbm, maxint, derniere) sont dans matter_hotte.cpp,
//  la cle H1 dans net_udp_thread.cpp. Tache loop seulement.
// ===========================================================================
#include "hotte_etat.h"
#include "surveillance.h"
#if PILOTE_SIMULE
#include "pilote_simule.h"
#endif

// Jeu hors bornes (ou de taille inattendue) : tous ses defauts. Rend le
// nombre de jeux remplaces (0, 1 ou 2).
uint8_t nvsChargerParams(hotte::Params *a, surv::Params *s);
bool nvsSauverParams(const hotte::Params &a);
bool nvsSauverSurveillance(const surv::Params &s);
bool nvsLireDerniere(hotte::Moteur *m);  // faux : absente (V2 par defaut)
bool nvsEcrireDerniere(hotte::Moteur m);
bool nvsLireEtatPub(hotte::Etat *e);     // faux : absent ou mal forme
bool nvsEcrireEtatPub(const hotte::Etat &e);
bool nvsLireTempMax(int32_t *dixiemes);  // faux : absent (aucune pose)
bool nvsEcrireTempMax(int32_t dixiemes);
bool nvsEffacerTempMax();
uint16_t nvsLireEssaiMin();              // minutes restantes ; 0 : pas d'essai
bool nvsEcrireEssaiMin(uint16_t min);    // 0 : efface
#if PILOTE_SIMULE
bool nvsLireSimu(sim::Reglages *r);      // faux : absent ou hors bornes (defauts)
bool nvsSauverSimu(const sim::Reglages &r);
bool nvsPrendreSimuDem(sim::EtatHotte *e);  // lu PUIS efface (une fois)
bool nvsEcrireSimuDem(const sim::EtatHotte &e);
#endif
```

- [ ] **Étape 10 : Écrire `src/produit/reglages_produit.cpp`**

```cpp
// ===========================================================================
//  NVS du produit : voir reglages_produit.h.
// ===========================================================================
#include "reglages_produit.h"

#include <Preferences.h>

#include "config_produit.h"
#include "hotte_map.h"

namespace {

const char *const kParams = "params", *const kParamsS = "params_s", *const kDerniere = "derniere_v";
const char *const kEtatPub = "etat_pub", *const kTempMax = "temp_max", *const kEssai = "essai";
const char *const kSimu = "simu", *const kSimuDem = "simu_dem";

// Ouvert en ecriture meme pour lire (comme benq) : en lecture seule, un espace
// absent fait loguer une erreur NVS au premier demarrage. isKey() avant toute
// lecture : interroger une cle absente logue aussi une erreur.
template <class F>
bool avec(F f) {
  Preferences p;
  if (!p.begin(kNvsProduit, false)) return false;
  const bool ok = f(p);
  p.end();
  return ok;
}

}  // namespace

uint8_t nvsChargerParams(hotte::Params *a, surv::Params *s) {
  uint8_t remplaces = 0;
  hotte::Params pa;
  surv::Params ps;
  avec([&](Preferences &p) {
    if (p.isKey(kParams)) {
      const size_t n = p.getBytesLength(kParams);
      uint8_t buf[sizeof(hotte::Params) + 8];
      const size_t lu = n <= sizeof(buf) ? p.getBytes(kParams, buf, n) : 0;
      if (!hotte::chargerParams(buf, lu, &pa)) remplaces++;
    }
    if (p.isKey(kParamsS)) {
      const size_t n = p.getBytesLength(kParamsS);
      uint8_t buf[sizeof(surv::Params) + 8];
      const size_t lu = n <= sizeof(buf) ? p.getBytes(kParamsS, buf, n) : 0;
      if (!surv::chargerParams(buf, lu, &ps)) remplaces++;
    }
    return true;
  });
  *a = pa;
  *s = ps;
  return remplaces;
}

bool nvsSauverParams(const hotte::Params &a) {
  return avec([&](Preferences &p) { return p.putBytes(kParams, &a, sizeof(a)) == sizeof(a); });
}

bool nvsSauverSurveillance(const surv::Params &s) {
  return avec([&](Preferences &p) { return p.putBytes(kParamsS, &s, sizeof(s)) == sizeof(s); });
}

bool nvsLireDerniere(hotte::Moteur *m) {
  uint8_t v = 0;
  const bool ok = avec([&](Preferences &p) {
    if (!p.isKey(kDerniere)) return false;
    v = p.getUChar(kDerniere, 0);
    return v >= 1 && v <= 3;
  });
  if (ok) *m = (hotte::Moteur)v;
  return ok;
}

bool nvsEcrireDerniere(hotte::Moteur m) {
  return avec([&](Preferences &p) { return p.putUChar(kDerniere, (uint8_t)m) == 1; });
}

bool nvsLireEtatPub(hotte::Etat *e) {
  uint16_t v = 0;
  const bool lu = avec([&](Preferences &p) {
    if (!p.isKey(kEtatPub)) return false;
    v = p.getUShort(kEtatPub, 0);
    return true;
  });
  return lu && hotte::depuisNvs(v, e);
}

bool nvsEcrireEtatPub(const hotte::Etat &e) {
  return avec([&](Preferences &p) { return p.putUShort(kEtatPub, hotte::versNvs(e)) == 2; });
}

bool nvsLireTempMax(int32_t *d) {
  return avec([&](Preferences &p) {
    if (!p.isKey(kTempMax)) return false;
    *d = p.getInt(kTempMax, 0);
    return true;
  });
}

bool nvsEcrireTempMax(int32_t d) {
  return avec([&](Preferences &p) { return p.putInt(kTempMax, d) == 4; });
}

bool nvsEffacerTempMax() {
  return avec([&](Preferences &p) { return !p.isKey(kTempMax) || p.remove(kTempMax); });
}

uint16_t nvsLireEssaiMin() {
  uint16_t v = 0;
  avec([&](Preferences &p) {
    if (p.isKey(kEssai)) v = p.getUShort(kEssai, 0);
    return true;
  });
  return v > 4320 ? 4320 : v;  // 72 h au plus
}

bool nvsEcrireEssaiMin(uint16_t min) {
  return avec([&](Preferences &p) {
    if (!min) return !p.isKey(kEssai) || p.remove(kEssai);
    return p.putUShort(kEssai, min) == 2;
  });
}

#if PILOTE_SIMULE
bool nvsLireSimu(sim::Reglages *r) {
  sim::Reglages lu;
  const bool ok = avec([&](Preferences &p) {
    return p.isKey(kSimu) && p.getBytesLength(kSimu) == sizeof(lu) && p.getBytes(kSimu, &lu, sizeof(lu)) == sizeof(lu);
  });
  if (!ok || !sim::reglagesValides(lu)) return false;
  *r = lu;
  return true;
}

bool nvsSauverSimu(const sim::Reglages &r) {
  return avec([&](Preferences &p) { return p.putBytes(kSimu, &r, sizeof(r)) == sizeof(r); });
}

bool nvsPrendreSimuDem(sim::EtatHotte *e) {
  uint16_t v = 0;
  const bool lu = avec([&](Preferences &p) {
    if (!p.isKey(kSimuDem)) return false;
    v = p.getUShort(kSimuDem, 0);
    p.remove(kSimuDem);  // une fois
    return true;
  });
  return lu && sim::depuisMot(v, e);
}

bool nvsEcrireSimuDem(const sim::EtatHotte &e) {
  return avec([&](Preferences &p) { return p.putUShort(kSimuDem, sim::versMot(e)) == 2; });
}
#endif
```

- [ ] **Étape 11 : Écrire `src/produit/alim.h`**

```cpp
#pragma once
// ===========================================================================
//  Mesures du module (docs/SPEC-PRODUIT.md 6.5, 7.4)
//
//  - Tensions des deux voies : ADC1 en mode continu (DMA), GPIO2 (+ de CN3)
//    et GPIO3 (apres diode et fusible), environ 1 kHz chacune ; le minimum de
//    chaque seconde et la derniere valeur vont a la surveillance. Un creux
//    plus court que la periode d'echantillonnage peut echapper (6.5).
//  - Temperature de la puce : temperatureRead() toutes les 10 s.
//
//  Au banc, sans pont, GPIO2 et GPIO3 sont en l'air : les tensions lues ne
//  veulent rien dire, et les alertes de tension sont coupees (alim_alertes 0).
//  Tache loop seulement.
// ===========================================================================
#include <Arduino.h>

#include "surveillance.h"

bool alimBegin();  // setup() : ADC continu demarre ; false : ADC indisponible (temperature seule)
void alimPoll(surv::Surveillance &s, uint32_t now);
// Banc B15 (Q42) : l'ADC continu s'arrete, la temperature se lit seule ; puis reprise.
void alimArreter();
bool alimReprendre();
void alimStatut(Print &out, const surv::Surveillance &s);
```

- [ ] **Étape 12 : Écrire `src/produit/alim.cpp`**

ADC1 en mode continu par le pilote d'IDF (pas l'enveloppe Arduino, qui moyenne et cacherait les creux) ; température de la puce toutes les 10 s.

```cpp
// ===========================================================================
//  Mesures du module : voir alim.h.
// ===========================================================================
#include "alim.h"

#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_continuous.h>
#include <math.h>

#include "config_produit.h"

namespace {

constexpr uint32_t kFreqHz = 2000;          // les deux voies se partagent l'echantillonneur
constexpr uint32_t kTrameOctets = 256;      // 64 resultats (4 octets chacun sur le C6)
constexpr uint32_t kReserveOctets = 1024;   // 4 trames d'avance
constexpr uint32_t kTemperatureMs = 10000;  // 7.4
constexpr uint8_t kLecturesParTour = 4;     // au plus 4 x 64 resultats par tour de loop()

adc_continuous_handle_t sAdc = nullptr;
adc_cali_handle_t sCali[2] = {nullptr, nullptr};
adc_channel_t sCanal[2] = {};
bool sEnService = false;

struct Voie {
  bool vu = false;        // un echantillon dans la seconde en cours
  uint32_t minBrut = 0;   // minimum brut de la seconde
  uint32_t dernierBrut = 0;
};
Voie sVoies[2];
uint32_t sSecondeMs = 0, sTemperatureMs = 0;
bool sTemperatureLue = false;
uint32_t sEchantillons = 0, sErreurs = 0, sSecondes = 0;
float sDerniereTemperature = NAN;

uint32_t versMv(uint8_t v, uint32_t brut) {
  int mv = 0;
  if (!sCali[v] || adc_cali_raw_to_voltage(sCali[v], (int)brut, &mv) != ESP_OK) return 0;
  return (uint32_t)mv * kPontRapport;  // pont 100 k / 100 k
}

}  // namespace

bool alimBegin() {
  adc_unit_t u;
  if (adc_continuous_io_to_channel(kPinAlimHotte, &u, &sCanal[0]) != ESP_OK ||
      adc_continuous_io_to_channel(kPinAlimModule, &u, &sCanal[1]) != ESP_OK)
    return false;
  adc_continuous_handle_cfg_t cfg = {};
  cfg.max_store_buf_size = kReserveOctets;
  cfg.conv_frame_size = kTrameOctets;
  if (adc_continuous_new_handle(&cfg, &sAdc) != ESP_OK) return false;
  adc_digi_pattern_config_t motif[2] = {};
  for (uint8_t v = 0; v < 2; v++) {
    motif[v].atten = ADC_ATTEN_DB_12;  // 0 a ~3,1 V a l'ADC : 6,2 V au + de CN3
    motif[v].channel = (uint8_t)sCanal[v];
    motif[v].unit = ADC_UNIT_1;
    motif[v].bit_width = ADC_BITWIDTH_12;
    adc_cali_curve_fitting_config_t cc = {};
    cc.unit_id = ADC_UNIT_1;
    cc.chan = sCanal[v];
    cc.atten = ADC_ATTEN_DB_12;
    cc.bitwidth = ADC_BITWIDTH_12;
    if (adc_cali_create_scheme_curve_fitting(&cc, &sCali[v]) != ESP_OK) sCali[v] = nullptr;
  }
  adc_continuous_config_t conf = {};
  conf.pattern_num = 2;
  conf.adc_pattern = motif;
  conf.sample_freq_hz = kFreqHz;
  conf.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  conf.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
  if (adc_continuous_config(sAdc, &conf) != ESP_OK) return false;
  sEnService = adc_continuous_start(sAdc) == ESP_OK;
  return sEnService;
}

void alimArreter() {
  if (sAdc && sEnService) adc_continuous_stop(sAdc);
  sEnService = false;
}

bool alimReprendre() {
  if (!sAdc) return false;
  if (!sEnService) sEnService = adc_continuous_start(sAdc) == ESP_OK;
  return sEnService;
}

void alimPoll(surv::Surveillance &s, uint32_t now) {
  if (sEnService) {
    adc_continuous_data_t lu[kTrameOctets / SOC_ADC_DIGI_RESULT_BYTES];
    for (uint8_t k = 0; k < kLecturesParTour; k++) {
      uint32_t n = 0;
      const esp_err_t e = adc_continuous_read_parse(sAdc, lu, sizeof(lu) / sizeof(lu[0]), &n, 0);
      if (e == ESP_ERR_TIMEOUT || !n) break;  // plus rien d'echantillonne
      if (e != ESP_OK) {
        sErreurs++;
        break;
      }
      for (uint32_t i = 0; i < n; i++) {
        if (!lu[i].valid) continue;
        const uint8_t v = lu[i].channel == sCanal[0] ? 0 : lu[i].channel == sCanal[1] ? 1 : 2;
        if (v > 1) continue;
        Voie &x = sVoies[v];
        if (!x.vu || lu[i].raw_data < x.minBrut) x.minBrut = lu[i].raw_data;
        x.dernierBrut = lu[i].raw_data;
        x.vu = true;
        sEchantillons++;
      }
    }
  }
  if (now - sSecondeMs >= 1000) {
    sSecondeMs = now;
    if (sVoies[0].vu && sVoies[1].vu) {
      s.tensions(versMv(0, sVoies[0].dernierBrut), versMv(0, sVoies[0].minBrut), versMv(1, sVoies[1].dernierBrut),
                 versMv(1, sVoies[1].minBrut));
      sSecondes++;
    }
    sVoies[0].vu = sVoies[1].vu = false;
  }
  if (!sTemperatureLue || now - sTemperatureMs >= kTemperatureMs) {
    sTemperatureLue = true;
    sTemperatureMs = now;
    const float t = temperatureRead();  // NAN si hors de -40..125 C (IDF change seul de plage)
    sDerniereTemperature = t;
    s.temperature(!isnan(t), isnan(t) ? 0 : (int32_t)lroundf(t * 10.0f), now);
  }
}

void alimStatut(Print &out, const surv::Surveillance &s) {
  out.printf("alim : ADC %s, %lu echantillons, %lu secondes, %lu erreurs\n", sEnService ? "en service" : "arrete",
             (unsigned long)sEchantillons, (unsigned long)sSecondes, (unsigned long)sErreurs);
  const surv::Voie &h = s.hotte(), &m = s.module();
  if (h.connue)
    out.printf("  hotte  (GPIO%u) : %lu mV, min seconde %lu, min demarrage %lu, passages %lu%s\n",
               (unsigned)kPinAlimHotte, (unsigned long)h.actuelMv, (unsigned long)h.minSecondeMv,
               (unsigned long)h.minDemarrageMv, (unsigned long)h.passages, h.alerte ? ", ALERTE" : "");
  if (m.connue)
    out.printf("  module (GPIO%u) : %lu mV, min seconde %lu, min demarrage %lu, passages %lu%s\n",
               (unsigned)kPinAlimModule, (unsigned long)m.actuelMv, (unsigned long)m.minSecondeMv,
               (unsigned long)m.minDemarrageMv, (unsigned long)m.passages, m.alerte ? ", ALERTE" : "");
  out.printf("  alertes de tension %s ; puce %.1f C (max %.1f, depuis la pose %.1f), seuil %lu C%s\n",
             s.params().alimAlertes ? "en service" : "coupees (pont non monte)", (double)sDerniereTemperature,
             s.tempMaxDixiemes() / 10.0, s.maxPose() / 10.0, (unsigned long)s.params().tempAlerteC,
             s.alerteTemperature() ? ", ALERTE" : "");
}
```

- [ ] **Étape 13 : Écrire `src/produit/matter_hotte.h`**

```cpp
#pragma once
// ===========================================================================
//  Couche Matter du produit (docs/SPEC-PRODUIT.md 3, 4.4, 8)
//
//  Un noeud sans pont : EP1 "Hotte" (Fan 0x002B, OffLowMedHigh, sans Auto,
//  sous-classe MatterFanHotte), EP2 "Eclairage hotte" (On/Off Light).
//  Recettes reprises de benq (src/matter_bridge.cpp au commit c58a506) :
//  boite d'intentions remplie par les rappels (tache CHIP) et videe par la
//  tache loop, echo propre ecarte, reflets par updateAttributeVal sous
//  TryLockChipStack, ordres ignores au demarrage, identite posee a chaque
//  demarrage, Thread seul, role et plafond des abonnements en NVS.
//
//  Toutes les fonctions sont pour la tache loop.
// ===========================================================================
#include <Arduino.h>

#include "hotte_etat.h"
#include "json_out_produit.h"

// Cree les endpoints et demarre la pile. etatInitial : l'etat initial de
// l'automate (5.7), publie par le premier reflet (parade du piege de la NVS).
void matterHotteBegin(const hotte::Etat &etatInitial, hotte::Moteur derniereVitesse);
// Vide la boite d'intentions : ordres regroupes vers l'automate (ignores
// pendant ignore_demarrage_ms), puis reflet de l'etat publie s'il attend.
void matterHottePoll(hotte::Automate &a, uint32_t now);
// Etat a publier (Action::Publier de l'automate) : reflete au prochain tour
// ou la boite est vide et le verrou de la pile libre.
void matterHottePublier(const hotte::Etat &e);
// Derniere vitesse de l'automate : copie atomique lue par la tache CHIP pour
// la substitution de On (3.3, regle 3).
void matterHotteDerniere(hotte::Moteur m);

bool matterHotteMisEnService();
void matterHotteDecommission();  // efface la cle H1 d'abord (7.7)
void matterHotteStatut(Print &out);
void matterHotteEtat(jsonp::MatterEtat *out, uint32_t now);
void matterHotteRadio(jsonp::RadioCompteurs *out);
// Journal des ecritures brutes de Maison (banc Matter, B2) : 'matter journal'.
void matterHotteJournal(Print &out);

// Reglages (NVS produit), USB seulement ; au prochain demarrage sauf tx et derniere.
bool matterReglerMed(uint32_t v, bool *saved);     // 0 routeur (defaut), 1 MED des l'init
bool matterReglerTx(uint32_t dbm, bool *saved);    // 8..20, applique tout de suite
bool matterReglerMaxint(uint32_t s, bool *saved);  // 0, ou 10..3600
bool matterReglerDerniere(bool on, bool *saved);   // substitution de On (banc B2)

// Verrou OpenThread pour le transport reseau (net_udp_thread.cpp) : false si
// la pile n'est pas demarree ou si le verrou n'est pas libre dans totalMs
// (0 : sans attente). Sous ce verrou, AUCUN appel Matter/CHIP.
bool matterOtTryLock(uint32_t totalMs);
void matterOtUnlock();
```

- [ ] **Étape 14 : Écrire `src/produit/matter_hotte.cpp`**

Recettes de benq (`src/matter_bridge.cpp` au commit `c58a506`) : boîte
d'intentions sous verrou court, écho propre écarté, reflets par
`updateAttributeVal` sous `TryLockChipStack`, ordres ignorés au démarrage,
identité posée à chaque démarrage, enveloppe du rôle Thread, plafond des
abonnements, clé H1 effacée au départ du dernier contrôleur. `MatterFanHotte`
voit chaque écriture avant le serveur CHIP et remplace On par le `FanMode` de
la dernière vitesse (§3.3, règle 3 ; `matter derniere 0` la coupe au banc).

```cpp
// Recettes reprises de benq-screenbar-halo-matter@c58a506 : src/matter_bridge.cpp
// (boite d'intentions, echo propre, reflets sous TryLockChipStack, garde-fou de
// demarrage, identite, type Thread des l'init, plafond des abonnements, cle
// effacee au depart du dernier controleur), reecrites pour la hotte.
#include "matter_hotte.h"

#include <Matter.h>
#include <Preferences.h>
#include <app/InteractionModelEngine.h>
#include <app/ReadHandler.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage-null-handling.h>
#include <esp_mac.h>
#include <esp_openthread.h>
#include <esp_openthread_lock.h>
#include <openthread/link.h>
#include <openthread/platform/radio.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <stdarg.h>

#include "config_produit.h"
#include "hotte_map.h"
#include "json_mode_produit.h"
#include "net_udp_thread.h"

using namespace chip::app::Clusters;

#ifndef MATTER_THREAD_MED
#define MATTER_THREAD_MED 0
#endif
#ifndef HALO_WRAP_THREAD_DEVTYPE
#define HALO_WRAP_THREAD_DEVTYPE 0
#endif

// ===========================================================================
//  Boite d'intentions : remplie par les rappels (tache CHIP), videe par la
//  tache loop. Les ecritures d'EP1 vont dans un seul regroupement (5.4, regle
//  6), celles d'EP2 dans le leur ; la derniere valeur gagne.
// ===========================================================================

static TaskHandle_t sLoopTask = nullptr;  // pris dans matterHotteBegin() (setup = tache loop)
static portMUX_TYPE sBoiteMux = portMUX_INITIALIZER_UNLOCKED;
static hotte::RegroupementEp1 sEp1;
static hotte::RegroupementEp2 sEp2;
// Reglages lus par la tache CHIP : copies atomiques ecrites par la tache loop.
static volatile uint8_t sModeDerniere = hotte::kFanMedium;  // FanMode de la derniere vitesse
static volatile bool sSubstitution = true;                  // 'matter derniere 0|1'
static volatile uint16_t sMaxIntCap = 20;                   // plafond des abonnements neufs (s)

// Journal des ecritures brutes (banc Matter, B2) : rempli sous sBoiteMux.
struct Brute {
  uint32_t ms;
  uint32_t attr;
  uint8_t valeur;
  bool nulle, substituee;
};
static constexpr uint8_t kJournal = 16;
static Brute sJournal[kJournal];
static uint8_t sJournalN = 0;  // ecritures depuis le demarrage, modulo 256 (place = n % kJournal)

// Nos reflets repassent par les rappels, mais toujours dans la tache loop ;
// les ordres des controleurs arrivent dans la tache CHIP.
static inline bool ownEcho() { return xTaskGetCurrentTaskHandle() == sLoopTask; }

static void noterBrute(uint32_t attr, uint8_t v, bool nulle, bool substituee) {
  Brute &b = sJournal[sJournalN % kJournal];
  b.ms = millis();
  b.attr = attr;
  b.valeur = v;
  b.nulle = nulle;
  b.substituee = substituee;
  sJournalN++;
}

// EP1 : sous-classe de MatterFan (4.4). Elle recoit chaque ecriture AVANT le
// serveur CHIP, valeur brute : depot dans la boite, cache de MatterFan mis a
// jour, jamais MatterFan::attributeChangeCB pour un ordre (il publierait
// PercentCurrent = la cible, MatterFan.cpp l. 110 a 118). FanMode = On :
// intention "derniere vitesse", puis la valeur est remplacee par le FanMode
// de la derniere vitesse (la pile ne la convertit plus en High, et Maison
// recoit Success).
class MatterFanHotte : public MatterFan {
 public:
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                         esp_matter_attr_val_t *val) override {
    if (!val || endpoint_id != getEndPointId() || cluster_id != FanControl::Id || ownEcho())
      return MatterFan::attributeChangeCB(endpoint_id, cluster_id, attribute_id, val);
    const uint32_t now = millis();
    switch (attribute_id) {
      case FanControl::Attributes::FanMode::Id: {
        uint8_t m = val->val.u8;
        const bool subst = m == hotte::kFanOn && sSubstitution;
        portENTER_CRITICAL(&sBoiteMux);
        noterBrute(attribute_id, m, false, subst);
        sEp1.ecrireMode(m, now);  // la valeur brute, On compris
        portEXIT_CRITICAL(&sBoiteMux);
        if (subst) {
          m = sModeDerniere;
          val->val.u8 = m;  // la pile ecrit ce mode en base, sans conversion en High
        }
        if (m <= hotte::kFanHigh) currentFanMode = (FanMode_t)m;
        return true;
      }
      case FanControl::Attributes::PercentSetting::Id: {
        const bool nulle = val->type == ESP_MATTER_VAL_TYPE_NULLABLE_UINT8 &&
                           chip::app::NumericAttributeTraits<uint8_t>::IsNullValue(val->val.u8);
        portENTER_CRITICAL(&sBoiteMux);
        noterBrute(attribute_id, val->val.u8, nulle, false);
        if (!nulle && val->val.u8 <= 100) sEp1.ecrirePourcent(val->val.u8, now);
        portEXIT_CRITICAL(&sBoiteMux);
        if (!nulle && val->val.u8 <= 100) currentPercent = val->val.u8;
        return true;
      }
      case FanControl::Attributes::PercentCurrent::Id:  // cascade du serveur (Off) : le cache suit
        if (val->val.u8 <= 100) currentPercent = val->val.u8;
        return true;
      default:
        return MatterFan::attributeChangeCB(endpoint_id, cluster_id, attribute_id, val);
    }
  }
};

static MatterFanHotte sFan;
static MatterOnOffLight sLampe;

// Un rappel par attribut, jamais onChange() : celui-ci transmet aussi les
// valeurs en cache des autres attributs, qui passeraient pour des ordres.
static bool onLampe(bool on) {
  if (ownEcho()) return true;
  const uint32_t now = millis();
  portENTER_CRITICAL(&sBoiteMux);
  noterBrute(OnOff::Attributes::OnOff::Id, on, false, false);
  sEp2.ecrire(on, now);
  portEXIT_CRITICAL(&sBoiteMux);
  return true;
}

static volatile uint32_t sIdentify = 0;  // demandes Identify (aucun effet, 3.3)
static bool onIdentify(bool active) {
  if (active) __atomic_fetch_add(&sIdentify, 1, __ATOMIC_RELAXED);
  return true;
}

// ===========================================================================
//  Etat du pont (tache loop)
// ===========================================================================

static bool sDemarre = false;  // pile Thread demarree (verrou OT utilisable)
static uint32_t sBootMs = 0;
static bool sGardeDemarrage = true;
static bool sAPublier = false;  // reflet en attente
// Demarrage sans etat connu (confiance Inconnu, 5.1) : rien de nouveau n'est
// publie, mais les pourcentages suivent le FanMode restaure par la pile (piege
// de la NVS : jamais "High a 0 %").
static bool sRefletBase = false;
static uint32_t sIgnoreMs = 2000;  // ignore_demarrage_ms en vigueur (json : ignore_ms)
static hotte::Reflet sVoulu = {hotte::kFanOff, 0, false};
static struct {
  uint32_t fenetres, ignores, reflets, ecritures, echecs, verrouOccupe, identifies;
} sStats = {};

static void journal(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void journal(const char *fmt, ...) {
  char ligne[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ligne, sizeof(ligne), fmt, ap);
  va_end(ap);
  if (!jsonLog("matter", "notice", ligne)) jsonTexteUsb(ligne);
}

// --- Reglages (NVS produit) --------------------------------------------------

static const char *const kNvsMed = "med";
static const char *const kNvsTx = "tx_dbm";
static const char *const kNvsMaxInt = "maxint";
static const char *const kNvsDerniere = "derniere";
static uint8_t sMedProchain = MATTER_THREAD_MED ? 1 : 0;  // prochain demarrage
static uint8_t sMedDemarrage = MATTER_THREAD_MED ? 1 : 0;  // ce demarrage
static bool sTxPose = false;  // tx_dbm present en NVS
static int8_t sTxDbm = 20;

static void chargerReglages() {
  Preferences p;
  if (!p.begin(kNvsProduit, false)) return;
  if (p.isKey(kNvsMed)) sMedProchain = p.getUChar(kNvsMed, sMedProchain) ? 1 : 0;
  if (p.isKey(kNvsTx)) {
    const int8_t v = p.getChar(kNvsTx, 20);
    if (v >= 8 && v <= 20) {
      sTxPose = true;
      sTxDbm = v;
    }
  }
  if (p.isKey(kNvsMaxInt)) {
    const uint16_t v = p.getUShort(kNvsMaxInt, 20);
    if (v == 0 || (v >= 10 && v <= 3600)) sMaxIntCap = v;
  }
  if (p.isKey(kNvsDerniere)) sSubstitution = p.getUChar(kNvsDerniere, 1) != 0;
  p.end();
  sMedDemarrage = sMedProchain;
}

template <class F>
static bool ecrireNvs(F ecrire) {
  Preferences p;
  if (!p.begin(kNvsProduit, false)) return false;
  const bool ok = ecrire(p);
  p.end();
  return ok;
}

bool matterReglerMed(uint32_t v, bool *saved) {
  if (v > 1) return false;
  sMedProchain = (uint8_t)v;
  *saved = ecrireNvs([&](Preferences &p) { return p.putUChar(kNvsMed, (uint8_t)v) == 1; });
  return true;
}

// Puissance d'emission 802.15.4 (6.1, Q13) : sous le verrou OpenThread.
static bool appliquerTx(int8_t dbm) {
  if (!sDemarre || !esp_openthread_lock_acquire(pdMS_TO_TICKS(50))) return false;
  const otError e = otPlatRadioSetTransmitPower(esp_openthread_get_instance(), dbm);
  esp_openthread_lock_release();
  return e == OT_ERROR_NONE;
}

bool matterReglerTx(uint32_t dbm, bool *saved) {
  if (dbm < 8 || dbm > 20) return false;
  sTxPose = true;
  sTxDbm = (int8_t)dbm;
  const bool applique = appliquerTx(sTxDbm);
  *saved = ecrireNvs([&](Preferences &p) { return p.putChar(kNvsTx, (int8_t)dbm) == 1; });
  if (!applique) journal("[matter] tx %lu dBm enregistre, pas encore applique (pile occupee)", (unsigned long)dbm);
  return true;
}

bool matterReglerMaxint(uint32_t s, bool *saved) {
  if (s != 0 && (s < 10 || s > 3600)) return false;
  sMaxIntCap = (uint16_t)s;
  *saved = ecrireNvs([&](Preferences &p) { return p.putUShort(kNvsMaxInt, (uint16_t)s) == 2; });
  return true;
}

bool matterReglerDerniere(bool on, bool *saved) {
  sSubstitution = on;
  *saved = ecrireNvs([&](Preferences &p) { return p.putUChar(kNvsDerniere, on ? 1 : 0) == 1; });
  return true;
}

// --- Type Thread des l'init (benq, section b) ---------------------------------
//  esp_matter::start demande le role routeur (_SetThreadDeviceType) ;
//  l'enveloppe le remplace par MED quand 'matter med 1' est pose. Editeur de
//  liens : -Wl,--wrap=<symbole> et HALO_WRAP_THREAD_DEVTYPE vont ensemble.
#if HALO_WRAP_THREAD_DEVTYPE
using ThreadDeviceType = chip::DeviceLayer::ConnectivityManager::ThreadDeviceType;
extern "C" CHIP_ERROR
__real__ZN4chip11DeviceLayer8Internal40GenericThreadStackManagerImpl_OpenThreadINS0_22ThreadStackManagerImplEE20_SetThreadDeviceTypeENS0_19ConnectivityManager16ThreadDeviceTypeE(
    void *self, ThreadDeviceType type);

extern "C" CHIP_ERROR
__wrap__ZN4chip11DeviceLayer8Internal40GenericThreadStackManagerImpl_OpenThreadINS0_22ThreadStackManagerImplEE20_SetThreadDeviceTypeENS0_19ConnectivityManager16ThreadDeviceTypeE(
    void *self, ThreadDeviceType type) {
  if (sMedDemarrage && type == chip::DeviceLayer::ConnectivityManager::kThreadDeviceType_Router)
    type = chip::DeviceLayer::ConnectivityManager::kThreadDeviceType_MinimalEndDevice;
  return __real__ZN4chip11DeviceLayer8Internal40GenericThreadStackManagerImpl_OpenThreadINS0_22ThreadStackManagerImplEE20_SetThreadDeviceTypeENS0_19ConnectivityManager16ThreadDeviceTypeE(
      self, type);
}
#endif

// --- Plafond de l'intervalle maximal des abonnements neufs (benq, section c) --

// Ecrits par la tache CHIP, lus par la tache loop : operations atomiques.
static uint32_t sAbonnes = 0, sAbonnementsDemandes = 0, sAbonnementsPlafonnes = 0;

class SurveillanceAbonnements : public chip::app::ReadHandler::ApplicationCallback {
 public:
  CHIP_ERROR OnSubscriptionRequested(chip::app::ReadHandler &rh, chip::Transport::SecureSession &) override {
    uint16_t plancher = 0, max = 0;
    rh.GetReportingIntervals(plancher, max);
    const uint16_t cap = sMaxIntCap;
    __atomic_fetch_add(&sAbonnementsDemandes, 1, __ATOMIC_RELAXED);
    if (cap) {
      const uint16_t voulu = cap < plancher ? plancher : cap;
      if (voulu < max && rh.SetMaxReportingInterval(voulu) == CHIP_NO_ERROR)
        __atomic_fetch_add(&sAbonnementsPlafonnes, 1, __ATOMIC_RELAXED);
    }
    return CHIP_NO_ERROR;
  }
  void OnSubscriptionEstablished(chip::app::ReadHandler &) override {
    __atomic_fetch_add(&sAbonnes, 1, __ATOMIC_RELAXED);
  }
  void OnSubscriptionTerminated(chip::app::ReadHandler &) override {
    uint32_t n = __atomic_load_n(&sAbonnes, __ATOMIC_RELAXED);
    while (n && !__atomic_compare_exchange_n(&sAbonnes, &n, n - 1, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
    }
  }
};
static SurveillanceAbonnements sAbonnements;

// ===========================================================================
//  Identite du noeud (8.1), posee a chaque demarrage avant Matter.begin()
// ===========================================================================

static char sSerie[sizeof(MATTER_SERIAL_PREFIX) + 12] = {};

static void poserIdentite() {
  // esp_read_mac(ESP_MAC_BASE) : la MAC-48 d'usine, propre a la carte ;
  // tampon de 8 par prudence (le C6 peut rendre une EUI-64 ailleurs).
  uint8_t mac[8] = {};
  const bool macOk = esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK;
  if (macOk)
    snprintf(sSerie, sizeof(sSerie), "%s%02X%02X%02X%02X%02X%02X", MATTER_SERIAL_PREFIX, mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
  // Evaluees dans l'ordre, toutes meme apres un refus.
  const bool ok[] = {
      Matter.setVendorName(MATTER_VENDOR_NAME),
      Matter.setProductName(MATTER_PRODUCT_NAME),
      Matter.setHardwareVersion(MATTER_HW_VERSION),
      Matter.setHardwareVersionString(MATTER_HW_VERSION_STRING),
      macOk && Matter.setSerialNumber(sSerie),  // MAC illisible : numero d'usine de la pile
      Matter.setDeviceName(MATTER_NODE_LABEL),
      Matter.setSetupDiscriminator(kDiscriminateur),
      Matter.setSetupPasscode(kCodeAppairage),
  };
  static const char *const kQuoi[] = {"fabricant", "produit", "version materielle", "materiel (texte)",
                                      "numero de serie", "nom du noeud", "discriminateur", "code d'appairage"};
  for (size_t i = 0; i < sizeof(ok); i++)
    if (!ok[i]) {
      char ligne[96];
      snprintf(ligne, sizeof(ligne), "!! identite Matter : %s refuse, valeur d'usine de la pile gardee", kQuoi[i]);
      jsonTexteUsb(ligne);
    }
}

// ===========================================================================
//  Cycle de vie
// ===========================================================================

bool matterOtTryLock(uint32_t totalMs) {
  return sDemarre && esp_openthread_lock_acquire(pdMS_TO_TICKS(totalMs / 2));
}
void matterOtUnlock() { esp_openthread_lock_release(); }

void matterHotteDerniere(hotte::Moteur m) { sModeDerniere = hotte::fanMode(m == hotte::Moteur::Arret ? hotte::Moteur::V2 : m); }

void matterHotteBegin(const hotte::Etat &etatInitial, hotte::Moteur derniereVitesse) {
  sLoopTask = xTaskGetCurrentTaskHandle();
  chargerReglages();
  matterHotteDerniere(derniereVitesse);
  // Avant le premier begin() d'endpoint : c'est lui qui cree le noeud (4.4).
  if (!Matter.selectNetwork(MATTER_NETWORK_THREAD))
    jsonTexteUsb("!! selectNetwork(THREAD) refuse : le noeud resterait en Wi-Fi");
  const hotte::Reflet r = hotte::reflet(etatInitial);
  sFan.begin(r.pourcent, (MatterFan::FanMode_t)r.fanMode, MatterFan::FAN_MODE_SEQ_OFF_LOW_MED_HIGH);
  sLampe.begin(r.lampe);
  sLampe.onChangeOnOff(onLampe);
  sFan.onIdentify(onIdentify);
  sLampe.onIdentify(onIdentify);
  poserIdentite();
  Matter.begin();
  // Matter.begin() ne rend rien : l'instance OpenThread n'existe qu'apres
  // esp_openthread_init, qui cree le verrou.
  sDemarre = chip::DeviceLayer::ThreadStackMgrImpl().OTInstance() != nullptr;
  if (!sDemarre) {
    jsonTexteUsb("!! pile Thread absente");
  } else {
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    chip::app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&sAbonnements);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    if (sTxPose && !appliquerTx(sTxDbm)) jsonTexteUsb("!! puissance d'emission non appliquee au demarrage");
  }
  // Les ecritures de demarrage de la pile passent par les rappels : le delai
  // du garde-fou part d'ici. Premier reflet force : la pile a restaure FanMode
  // et OnOff depuis sa NVS, pas les pourcentages (parade du piege, 4.4).
  sBootMs = millis();
  sVoulu = r;
  sRefletBase = etatInitial.confiance == hotte::Confiance::Inconnu;
  sAPublier = true;
}

void matterHottePublier(const hotte::Etat &e) {
  if (e.confiance == hotte::Confiance::Inconnu) return;  // rien de nouveau n'est publie (5.1)
  sVoulu = hotte::reflet(e);
  sRefletBase = false;
  sAPublier = true;
}

// Valeur d'un attribut uint8 ou booleen, nullable ou non.
static bool lire(MatterEndPoint &ep, uint32_t cl, uint32_t attr, esp_matter_attr_val_t *v, uint8_t *out) {
  *v = esp_matter_invalid(nullptr);
  if (!ep.getAttributeVal(cl, attr, v)) return false;
  switch ((int)v->type & ~ESP_MATTER_VAL_NULLABLE_BASE) {
    case ESP_MATTER_VAL_TYPE_BOOLEAN: *out = v->val.b ? 1 : 0; return true;
    case ESP_MATTER_VAL_TYPE_UINT8:
    case ESP_MATTER_VAL_TYPE_ENUM8: *out = v->val.u8; return true;
    default: return false;
  }
}

// Ecrit la valeur voulue si l'attribut (la base, pas le cache) differe.
static void aligner(MatterEndPoint &ep, uint32_t cl, uint32_t attr, uint8_t voulu) {
  esp_matter_attr_val_t v;
  uint8_t cur = 0;
  if (!lire(ep, cl, attr, &v, &cur)) {
    sStats.echecs++;
    return;
  }
  if (cur == voulu && !(((int)v.type & ESP_MATTER_VAL_NULLABLE_BASE) &&
                        chip::app::NumericAttributeTraits<uint8_t>::IsNullValue(v.val.u8)))
    return;
  if (((int)v.type & ~ESP_MATTER_VAL_NULLABLE_BASE) == ESP_MATTER_VAL_TYPE_BOOLEAN) v.val.b = voulu != 0;
  else v.val.u8 = voulu;
  if (ep.updateAttributeVal(cl, attr, &v)) sStats.ecritures++;
  else sStats.echecs++;
}

// Sous le verrou de la pile, sans attendre : occupe, nouvel essai au tour
// suivant. Jamais de reflet tant que la boite n'est pas vide.
static bool refleter() {
  if (!chip::DeviceLayer::PlatformMgr().TryLockChipStack()) {
    sStats.verrouOccupe++;
    return false;
  }
  portENTER_CRITICAL(&sBoiteMux);
  const bool attente = sEp1.ouvert() || sEp2.ouvert();
  portEXIT_CRITICAL(&sBoiteMux);
  if (attente) {
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    return false;
  }
  if (sRefletBase) {  // la base telle que la pile l'a restauree, pourcentages alignes
    esp_matter_attr_val_t v;
    uint8_t mode = hotte::kFanOff, on = 0;
    lire(sFan, FanControl::Id, FanControl::Attributes::FanMode::Id, &v, &mode);
    lire(sLampe, OnOff::Id, OnOff::Attributes::OnOff::Id, &v, &on);
    const hotte::Moteur m = mode == hotte::kFanLow      ? hotte::Moteur::V1
                            : mode == hotte::kFanMedium ? hotte::Moteur::V2
                            : mode == hotte::kFanHigh   ? hotte::Moteur::V3
                                                        : hotte::Moteur::Arret;
    sVoulu.fanMode = hotte::fanMode(m);
    sVoulu.pourcent = hotte::pourcent(m);
    sVoulu.lampe = on != 0;
    sRefletBase = false;
  }
  // L'arret publie toujours FanMode Off ET 0 % (3.2) ; un palier, sa valeur canonique.
  aligner(sFan, FanControl::Id, FanControl::Attributes::FanMode::Id, sVoulu.fanMode);
  aligner(sFan, FanControl::Id, FanControl::Attributes::PercentSetting::Id, sVoulu.pourcent);
  aligner(sFan, FanControl::Id, FanControl::Attributes::PercentCurrent::Id, sVoulu.pourcent);
  aligner(sLampe, OnOff::Id, OnOff::Attributes::OnOff::Id, sVoulu.lampe ? 1 : 0);
  chip::DeviceLayer::PlatformMgr().UnlockChipStack();
  sStats.reflets++;
  return true;
}

// Dernier controleur parti sans remise a zero (accessoire retire de Maison) :
// la cle de l'app part aussi (7.7). Seul un passage observe compte.
static void surveillerProprietaire(uint32_t now) {
  static int8_t sVu = -1;
  static uint32_t sAt = 0;
  if (!sDemarre || (sVu >= 0 && now - sAt < 500)) return;
  sAt = now;
  const bool enService = Matter.isDeviceCommissioned();
  char kid[9];
  if (sVu == 1 && !enService && netUdpKid(kid)) {
    const bool ok = netUdpKeyErase();
    journal(ok ? "[matter] plus aucun controleur : cle H1 effacee" : "[matter] plus aucun controleur : effacement NVS de la cle en echec");
  }
  sVu = enService ? 1 : 0;
}

void matterHottePoll(hotte::Automate &a, uint32_t now) {
  surveillerProprietaire(now);
  const hotte::Params &p = a.params();
  sIgnoreMs = p.ignoreDemarrageMs;
  hotte::CibleVentilo cible = hotte::CibleVentilo::Aucune;
  bool lampe = false, ep1 = false, ep2 = false;
  uint32_t premier1 = 0, premier2 = 0;
  portENTER_CRITICAL(&sBoiteMux);
  ep1 = sEp1.pret(now, p.lissageCalmeMs, p.lissagePlafondMs, &cible, &premier1);
  ep2 = sEp2.pret(now, p.ordreCalmeMs, p.lissagePlafondMs, &lampe, &premier2);
  portEXIT_CRITICAL(&sBoiteMux);
  if (ep1 || ep2) {
    sStats.fenetres++;
    const uint32_t premier = ep1 && ep2 ? ((int32_t)(premier1 - premier2) < 0 ? premier1 : premier2)
                                        : ep1 ? premier1 : premier2;
    if (sGardeDemarrage && hotte::ignoreeAuDemarrage(premier, sBootMs, p.ignoreDemarrageMs)) {
      // Rien n'est injecte au demarrage (5.7) ; l'etat reel est republie.
      sStats.ignores++;
      journal("[matter] ordres ignores au demarrage :%s%s", ep1 ? " ventilateur" : "", ep2 ? " lampe" : "");
      sAPublier = true;
    } else {
      if (ep1 && cible != hotte::CibleVentilo::Aucune) a.ordreVentilo(cible, 0, hotte::Canal::Matter, now);
      if (ep2) a.ordreLampe(lampe, 0, hotte::Canal::Matter, now);
      if (ep1 && cible == hotte::CibleVentilo::Aucune) sAPublier = true;  // Auto, Smart : realigner
    }
  }
  if (sGardeDemarrage && now - sBootMs >= p.ignoreDemarrageMs + p.lissagePlafondMs) sGardeDemarrage = false;
  if (sAPublier && refleter()) sAPublier = false;
  const uint32_t id = __atomic_load_n(&sIdentify, __ATOMIC_RELAXED);
  if (id != sStats.identifies) {
    sStats.identifies = id;
    journal("[matter] Identify demande : aucun effet (3.3)");
  }
}

// ===========================================================================
//  Statut
// ===========================================================================

bool matterHotteMisEnService() { return Matter.isDeviceCommissioned(); }

void matterHotteDecommission() {
  // La cle de l'app part d'abord (7.7) ; deux essais : un echec la laisserait
  // revenir au demarrage suivant.
  if (!netUdpKeyErase() && !netUdpKeyErase()) journal("[matter] cle H1 : effacement NVS en echec");
  Matter.decommission();
}

void matterHotteEtat(jsonp::MatterEtat *m, uint32_t now) {
  static uint8_t sFabriques = 0;
  m->demarre = sDemarre;
  m->misEnService = Matter.isDeviceCommissioned();
  if (sDemarre && chip::DeviceLayer::PlatformMgr().TryLockChipStack()) {
    sFabriques = chip::Server::GetInstance().GetFabricTable().FabricCount();
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
  }
  m->fabriques = sFabriques;
  m->abonnements = (uint16_t)__atomic_load_n(&sAbonnes, __ATOMIC_RELAXED);
  const uint32_t fin = sBootMs + sIgnoreMs;
  m->ignoreResteMs = sGardeDemarrage && (int32_t)(fin - now) > 0 ? fin - now : 0;
  m->maxintS = sMaxIntCap;
  m->med = sMedProchain;
  m->txConnu = false;
  if (sDemarre && esp_openthread_lock_acquire(0)) {
    int8_t dbm = 0;
    m->txConnu = otPlatRadioGetTransmitPower(esp_openthread_get_instance(), &dbm) == OT_ERROR_NONE;
    m->txDbm = dbm;
    esp_openthread_lock_release();
  }
  m->derniere = sSubstitution;
  m->ecritures = sStats.ecritures;
  m->reflets = sStats.reflets;
  m->verrouOccupe = sStats.verrouOccupe;
  m->ignores = sStats.ignores;
}

void matterHotteRadio(jsonp::RadioCompteurs *r) {
  static jsonp::RadioCompteurs sDernier;
  if (!sDemarre || !esp_openthread_lock_acquire(0)) {  // occupe : la derniere lecture
    *r = sDernier;
    return;
  }
  otInstance *ot = esp_openthread_get_instance();
  jsonp::RadioCompteurs n;
  n.connu = true;
  const otDeviceRole role = otThreadGetDeviceRole(ot);
  n.role = otThreadDeviceRoleToString(role);
  int8_t dbm = 0;
  n.txConnu = otPlatRadioGetTransmitPower(ot, &dbm) == OT_ERROR_NONE;
  n.txDbm = dbm;
  n.sensibiliteDbm = otPlatRadioGetReceiveSensitivity(ot);
  if (role == OT_DEVICE_ROLE_CHILD) {  // MED : le lien avec le parent
    otRouterInfo parent;
    int8_t rssi = 0;
    if (otThreadGetParentInfo(ot, &parent) == OT_ERROR_NONE && otThreadGetParentAverageRssi(ot, &rssi) == OT_ERROR_NONE) {
      n.lien = n.lienParent = true;
      n.rssiMoyenDbm = rssi;
      n.lqIn = parent.mLinkQualityIn;
      n.lqOut = parent.mLinkQualityOut;
    }
  } else if (role == OT_DEVICE_ROLE_ROUTER || role == OT_DEVICE_ROLE_LEADER) {
    // Routeur : le voisin routeur de meilleur RSSI moyen, et sa qualite de lien.
    otNeighborInfoIterator it = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    otNeighborInfo nb;
    bool trouve = false;
    uint16_t rloc = 0;
    while (otThreadGetNextNeighborInfo(ot, &it, &nb) == OT_ERROR_NONE) {
      if (nb.mIsChild || (trouve && nb.mAverageRssi <= n.rssiMoyenDbm)) continue;
      trouve = true;
      n.rssiMoyenDbm = nb.mAverageRssi;
      rloc = nb.mRloc16;
    }
    otRouterInfo ri;
    if (trouve && otThreadGetRouterInfo(ot, (uint16_t)(rloc >> 10), &ri) == OT_ERROR_NONE) {
      n.lien = true;
      n.lqIn = ri.mLinkQualityIn;
      n.lqOut = ri.mLinkQualityOut;
    }
  }
  if (const otMacCounters *mc = otLinkGetCounters(ot)) {
    n.txTotal = mc->mTxTotal;
    n.txRetry = mc->mTxRetry;
    n.txEchecs = mc->mTxDirectMaxRetryExpiry;
  }
  if (const otMleCounters *ml = otThreadGetMleCounters(ot)) {
    n.changementsParent = ml->mParentChanges;
    n.changementsRole = (uint32_t)ml->mDetachedRole + ml->mChildRole + ml->mRouterRole + ml->mLeaderRole;
  }
  esp_openthread_lock_release();
  sDernier = n;
  *r = n;
}

void matterHotteJournal(Print &out) {
  portENTER_CRITICAL(&sBoiteMux);
  Brute copie[kJournal];
  const uint8_t n = sJournalN;
  for (uint8_t i = 0; i < kJournal; i++) copie[i] = sJournal[i];
  portEXIT_CRITICAL(&sBoiteMux);
  const uint8_t nb = n < kJournal ? n : kJournal;
  out.printf("ecritures brutes (%u dernieres) :\n", nb);
  for (uint8_t k = 0; k < nb; k++) {
    const Brute &b = copie[(uint8_t)(n - nb + k) % kJournal];
    const char *quoi = b.attr == FanControl::Attributes::FanMode::Id          ? "FanMode"
                       : b.attr == FanControl::Attributes::PercentSetting::Id ? "PercentSetting"
                                                                               : "OnOff";
    out.printf("  %8lu ms  %-14s %s%s\n", (unsigned long)b.ms, quoi, b.nulle ? "null" : String(b.valeur).c_str(),
               b.substituee ? " (On -> derniere vitesse)" : "");
  }
}

void matterHotteStatut(Print &out) {
  out.printf("matter : %s, %s, identite %s / %s / n/s %s\n", sDemarre ? "pile demarree" : "PILE ABSENTE",
             Matter.isDeviceCommissioned() ? "mis en service" : "pas encore mis en service", MATTER_VENDOR_NAME,
             MATTER_PRODUCT_NAME, sSerie[0] ? sSerie : "?");
  if (!Matter.isDeviceCommissioned()) {
    out.printf("  code manuel     : %s\n", Matter.getManualPairingCode().c_str());
    out.printf("  QR              : %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
  out.printf("  role            : %s au prochain demarrage (%s a celui-ci)\n", sMedProchain ? "MED" : "routeur",
             sMedDemarrage ? "MED" : "routeur");
  out.printf("  tx              : %s\n", sTxPose ? String(String(sTxDbm) + " dBm (NVS)").c_str() : "valeur de la pile");
  out.printf("  maxint          : %u s%s\n", (unsigned)sMaxIntCap, sMaxIntCap ? "" : " (celui du controleur)");
  out.printf("  derniere        : substitution de On %s\n", sSubstitution ? "active" : "coupee (banc B2)");
  out.printf("  abonnements     : %lu actifs, %lu demandes, %lu plafonnes\n",
             (unsigned long)__atomic_load_n(&sAbonnes, __ATOMIC_RELAXED),
             (unsigned long)__atomic_load_n(&sAbonnementsDemandes, __ATOMIC_RELAXED),
             (unsigned long)__atomic_load_n(&sAbonnementsPlafonnes, __ATOMIC_RELAXED));
  out.printf("  fenetres %lu, ignorees %lu, reflets %lu, ecritures %lu (echecs %lu), verrou occupe %lu, Identify %lu\n",
             (unsigned long)sStats.fenetres, (unsigned long)sStats.ignores, (unsigned long)sStats.reflets,
             (unsigned long)sStats.ecritures, (unsigned long)sStats.echecs, (unsigned long)sStats.verrouOccupe,
             (unsigned long)sStats.identifies);
}
```

- [ ] **Étape 15 : Copier le transport UDP de benq (commit `c58a506`) et l'adapter**

Le dépôt de benq (`/Users/Majid/Documents/Dev/esp32/benq`) est lu, jamais modifié.

Depuis la racine du dépôt :

```bash
python3 - <<'EOF'
#!/usr/bin/env python3
"""Copie src/net_udp.{h,cpp} de benq-screenbar-halo-matter au commit c58a506
(depot local /Users/Majid/Documents/Dev/esp32/benq) vers
src/produit/net_udp_thread.{h,cpp}, et l'adapte au produit : NVS produit/cle,
port kPortUdp, modules du produit, build Thread toujours, rafale de l'essai de
24 h (spec produit 7.5). Chaque texte remplace doit se trouver exactement une fois."""
import subprocess
import sys

BENQ = "/Users/Majid/Documents/Dev/esp32/benq"


def show(chemin):
    return subprocess.run(["git", "-C", BENQ, "show", f"c58a506:{chemin}"], check=True, capture_output=True, text=True).stdout


def remplacer(t, avant, apres, f):
    n = t.count(avant)
    if n != 1:
        sys.exit(f"{f} : {n} occurrence(s) de {avant[:70]!r} (1 attendue)")
    return t.replace(avant, apres)


# --- En-tete -----------------------------------------------------------------
h = show("src/net_udp.h")
for a, b in [
    (
        "#pragma once\n",
        "// Copie de benq-screenbar-halo-matter@c58a506 : src/net_udp.h (adapte : NVS\n"
        "// produit/cle, port kPortUdp, modules du produit, toujours Thread, rafale\n"
        "// de l'essai de 24 h)\n"
        "#pragma once\n",
    ),
    ("//  Transport reseau du protocole JSON (docs/PROTOCOLE-JSON.md, section 10)\n",
     "//  Transport reseau du protocole compagnon du produit (docs/PROTOCOLE-JSON-PRODUIT.md ;\n"
     "//  ScreenBar, docs/PROTOCOLE-JSON.fr.md, section 10)\n"),
    ("//  Build Thread seulement (MATTER_NET_THREAD) : socket UDP d'OpenThread sur\n"
     "//  un port fixe, enveloppe H1 (h1_proto.h), cle partagee en NVS (halo1/cle).\n",
     "//  Socket UDP d'OpenThread sur le port kPortUdp (5480), enveloppe H1\n"
     "//  (h1_proto.h), cle partagee en NVS (produit/cle).\n"),
    ("//    (cli.cpp : cliRunRemote), formatage des lignes, HMAC ;\n", "//    (cli_produit.cpp : cliRunRemote), formatage des lignes, HMAC ;\n"),
    ("//    de matter_bridge.cpp), verrou occupe : au tour suivant. Sous ce verrou,\n",
     "//    de matter_hotte.cpp), verrou occupe : au tour suivant. Sous ce verrou,\n"),
    ('#include "json_out.h"\n', '#include "json_out_produit.h"\n'),
    ("// Port UDP fixe, sous la plage ephemere d'OpenThread (49152..65535).\n"
     "constexpr uint16_t kHaloUdpPort = 5480;\n\n#if MATTER_NET_THREAD\n\n", ""),
    ("// setup(), apres matterBridgeBegin() : cle lue en NVS, file de reception.\n",
     "// setup(), apres matterHotteBegin() : cle lue en NVS, file de reception.\n"),
    ("// a zero Matter (matterDecommissionNow). false : NVS en echec (la cle est\n",
     "// a zero Matter (matterHotteDecommission). false : NVS en echec (la cle est\n"),
]:
    h = remplacer(h, a, b, "net_udp_thread.h")
h = remplacer(h, "bool netUdpKid(char kid[9]);  // false : aucune cle\n\n#endif\n",
              "bool netUdpKid(char kid[9]);  // false : aucune cle\n\n"
              "// 'radio rafale <s>' (essai de 24 h, 7.5) : datagrammes pleins, non\n"
              "// authentifies, vers l'adresse de la session slot, pendant s secondes (1 a\n"
              "// 30), hors plafond de debit mais dans la reserve de tampons d'OpenThread.\n"
              "// false : session absente ou duree hors bornes.\n"
              "bool netUdpRafale(uint8_t slot, uint32_t secondes);\n", "net_udp_thread.h")
open("src/produit/net_udp_thread.h", "w").write(h)

# --- Source ------------------------------------------------------------------
c = show("src/net_udp.cpp")
for a, b in [
    ('#include "net_udp.h"\n\n#if MATTER_NET_THREAD\n\n',
     "// Copie de benq-screenbar-halo-matter@c58a506 : src/net_udp.cpp (adapte : NVS\n"
     "// produit/cle, port kPortUdp, modules du produit, toujours Thread, rafale\n"
     "// de l'essai de 24 h)\n"
     '#include "net_udp_thread.h"\n\n'),
    ('#include "cli.h"\n#include "h1_proto.h"\n#include "json_mode.h"\n#include "matter_bridge.h"\n',
     '#include "cli_produit.h"\n#include "config_produit.h"\n#include "h1_proto.h"\n#include "json_mode_produit.h"\n#include "matter_hotte.h"\n'),
    ("// Une origine JSON par session etablie, apres l'USB (json_mode.cpp).\n",
     "// Une origine JSON par session etablie, apres l'USB (json_mode_produit.cpp).\n"),
    ('static const char *const kNvsNs = "halo1";\n', "static const char *const kNvsNs = kNvsProduit;\n"),
    ("// Debit moyen plafonne : chaque datagramme fait des trames 802.15.4 a quelques\n"
     "// cm du BM5602 (et partage le canal avec Matter). 3000 octets/s, ~10 % du canal ;\n",
     "// Debit moyen plafonne : chaque datagramme partage le canal 802.15.4 avec\n"
     "// Matter. 3000 octets/s, ~10 % du canal ;\n"),
    ("static constexpr uint8_t kRawSlot = 0xFF;\n",
     "static constexpr uint8_t kRawSlot = 0xFF;     // DEFI d'une poignee de main\n"
     "static constexpr uint8_t kRafaleSlot = 0xFE;  // rafale de l'essai de 24 h\n"),
    ("  uint32_t rx, rejected, defis, tx, txLost, txErr;\n  uint16_t bufMin;\n} sSt = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFFFF};\n",
     "  uint32_t rx, rejected, defis, tx, txLost, txErr, rafale;\n  uint16_t bufMin;\n} sSt = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFFFF};\n\n"
     "// Rafale en cours ('radio rafale') : jusqu'a sRafaleFin, vers la session sRafaleSlot.\n"
     "static bool sRafale = false;\n"
     "static uint8_t sRafaleSlot = 0;\n"
     "static uint32_t sRafaleFin = 0;\n"),
    ("  auto paced = [](const TxItem &t) { return t.slot != kRawSlot && sTxCredit < t.len; };\n",
     "  auto paced = [](const TxItem &t) { return t.slot < h1::kSlots && sTxCredit < t.len; };\n"),
    ("    mi.mSockPort = kHaloUdpPort;\n", "    mi.mSockPort = kPortUdp;\n"),
    ("      if (t.slot != kRawSlot) sTxCredit -= t.len;\n",
     "      if (t.slot < h1::kSlots) sTxCredit -= t.len;\n      if (t.slot == kRafaleSlot) sSt.rafale++;\n"),
    ("      a.mPort = kHaloUdpPort;\n", "      a.mPort = kPortUdp;\n"),
    ("//  Cle (NVS halo1/cle)\n", "//  Cle (NVS produit/cle)\n"),
    ("  w.u32(\"port\", kHaloUdpPort);\n", "  w.u32(\"port\", kPortUdp);\n"),
    ("  if (sSt.bufMin != 0xFFFF) w.u32(\"tampons_min\", sSt.bufMin);\n  else w.null(\"tampons_min\");\n  w.end();\n}\n\n#endif  // MATTER_NET_THREAD\n",
     "  if (sSt.bufMin != 0xFFFF) w.u32(\"tampons_min\", sSt.bufMin);\n  else w.null(\"tampons_min\");\n"
     "  w.u32(\"rafale_tx\", sSt.rafale);\n  w.end();\n}\n"),
]:
    c = remplacer(c, a, b, "net_udp_thread.cpp")

# Rafale : remplissage de la file a chaque tour, avant l'emission.
c = remplacer(c, "  txFlush();\n  // Meme sans cle : l'app lit nom SRP et adresses par l'USB avant d'en regler une.\n",
              "  rafalePoll(now);\n  txFlush();\n  // Meme sans cle : l'app lit nom SRP et adresses par l'USB avant d'en regler une.\n",
              "net_udp_thread.cpp")
c = remplacer(c, "// ===========================================================================\n//  Cycle de vie\n",
              """// ===========================================================================
//  Rafale de l'essai de 24 h (spec produit 7.5, 9.4)
// ===========================================================================

bool netUdpRafale(uint8_t slot, uint32_t secondes) {
  if (slot >= h1::kSlots || !sTable.slot(slot).used || secondes < 1 || secondes > 30) return false;
  sRafale = true;
  sRafaleSlot = slot;
  sRafaleFin = millis() + secondes * 1000;
  return true;
}

// Datagrammes pleins, non authentifies (l'app les rejette), en queue de file :
// deux places restent aux lignes de la session. Hors plafond de debit ; la
// reserve de tampons d'OpenThread s'applique (txFlush).
static void rafalePoll(uint32_t now) {
  if (!sRafale) return;
  const h1::Session &s = sTable.slot(sRafaleSlot);
  if (!s.used || (int32_t)(now - sRafaleFin) >= 0 || !sOpen) {
    sRafale = false;
    return;
  }
  while (sTxN + 2 < kTxN) {
    TxItem *t = txTail();
    t->at = now;
    t->errCounted = false;
    t->slot = kRafaleSlot;
    t->len = (uint16_t)kTxMax;
    t->port = s.peer.port;
    memcpy(t->peer, s.peer.ip, 16);
    memcpy(t->local, s.peer.local, 16);
    memset(t->data, 0x52, kTxMax);  // 'R' : ni un SALUT ni une donnee H1
    sTxN++;
  }
}

// ===========================================================================
//  Cycle de vie
""", "net_udp_thread.cpp")
for reste in ("halo1", "kHaloUdpPort", "MATTER_NET_THREAD", "matter_bridge"):
    if reste in c or reste in h:
        sys.exit(f"net_udp_thread : reste de benq : {reste}")
open("src/produit/net_udp_thread.cpp", "w").write(c)
print("net_udp_thread.{h,cpp} ecrits")
EOF
```

Sortie attendue :

```
net_udp_thread.{h,cpp} ecrits
```

- [ ] **Étape 16 : Écrire `src/produit/json_mode_produit.h`**

```cpp
// Copie de src/json_mode.h de la sonde (commit 1cb2c7c), elle-meme copiee de
// benq-screenbar-halo-matter@c58a506 (adapte : profil du produit, evenements
// hotte, sequence, alerte et trame_d, sessions reseau sur UDP/Thread)
#pragma once
// ===========================================================================
//  Mode machine : protocole compagnon v1, profil hotte du produit
//  (docs/PROTOCOLE-JSON-PRODUIT.md)
//
//  Session ('json 1', bail, 'json 0'), lignes periodiques (etat, compteurs,
//  reseau, battement) par la file des periodiques, evenements hotte,
//  sequence, alerte, trame_d et log, reponses aux lignes portant un id.
//
//  Une session par transport (origine : jsonp::kUsb, puis une par session
//  reseau etablie, net_udp_thread.cpp) : ses reglages, son n, sa file, ses
//  pertes. Les evenements partent vers chaque session en mode machine,
//  formates pour elle ; une reponse, vers l'origine de sa commande seulement.
//
//  Regles tenues ici (spec produit 4.8) :
//  - un seul producteur, la tache loop ; un seul tampon de formatage ;
//  - jamais d'attente : une ligne qui ne tient pas dans le tampon d'emission
//    est perdue et comptee (json_perdus), jamais ecrite a moitie ;
//  - rien n'est persiste : chaque demarrage repart en mode humain.
// ===========================================================================
#include <Arduino.h>

#include "json_out_produit.h"

// Debut de setup(), avant toute radio : identifiant de ce demarrage (hello.boot).
void jsonBegin();
// A chaque tour de loop() : bail, lignes periodiques.
void jsonPoll();

uint32_t jsonBootId();
bool jsonMachine();  // mode machine en cours sur l'USB
void jsonNoteRx();   // un octet recu de l'hote USB (bail)

// --- Console (cli_produit.cpp) ------------------------------------------------

struct JsonCmd {
  bool hasId;
  uint32_t id;
  const char *cmd;  // commande sans le prefixe, tronquee (reponse.cmd)
  uint32_t t0;      // debut de l'execution (duree_ms)
};
void jsonCommand(char *arg, const JsonCmd &c);
bool jsonCadenceOk(uint32_t now);
void jsonRefuse(const JsonCmd &c, const char *code, const char *msg);
void jsonReply(const jsonp::Reply &r);
void jsonReplyEnd(const jsonp::Reply &r);
void jsonAfterCommand();
// Un reglage a change ('hotte regle', 'simu ...', 'matter ...') : config
// reemise a chaque session en mode machine au prochain jsonAfterCommand().
void jsonConfigChanged();

// --- Evenements du produit (vers chaque session en mode machine) ------------

// Ordre de l'app ('hotte ventilo|lampe') : numero d'ordre pour l'automate,
// retenu avec l'origine et l'id de la commande, pour que l'evenement sequence
// porte cet id dans la session qui l'a envoyee (null ailleurs).
uint32_t jsonOrdreApp(uint32_t idCmd);
void jsonSequence(const hotte::Action &fin);
void jsonHotte(const hotte::Etat &avant, const hotte::Etat &apres, hotte::Origine o);
// Alerte : vers CHAQUE session (meme hors mode machine pour l'USB, sans
// abonnement), doublee d'un log notice, meme sans 'json log 1' (7.4).
void jsonAlerte(const surv::Alerte &a);
// Trame decodee du fil : sessions avec 'json trames 1' ; 10 par seconde au
// plus ; coupees seules apres 60 s a distance.
void jsonTrameD(const jsonp::TrameD &t);
// Mode 'json log 1' : la ligne part en message log (src produit|matter|hotte|
// reseau|simu, niv notice|trace), 20 par seconde au plus ; true si l'USB l'a
// prise (emise, plafonnee ou perdue), false pour l'afficher en texte.
bool jsonLog(const char *src, const char *niv, const char *txt);
// Annonce en texte sur l'USB, quand jsonLog ne l'a pas prise : la ligne
// entiere ou rien, jamais d'attente ; perdue, elle est comptee.
void jsonTexteUsb(const char *txt);

// --- Transport reseau (net_udp_thread.cpp, cli_produit.cpp) -------------------

void jsonSetOrigin(uint8_t origin);
uint8_t jsonOrigin();
void jsonRemoteReset(uint8_t origin);
void jsonNoteRemoteRx(uint8_t origin);
bool jsonRemoteAdmit(uint32_t id, const char *shown);
void jsonCountRejected();
```

- [ ] **Étape 17 : Copier la session JSON de la sonde (commit `1cb2c7c`) et l'adapter**

Depuis la racine du dépôt :

```bash
python3 - <<'EOF'
#!/usr/bin/env python3
"""Copie src/json_mode.cpp de la sonde (commit 1cb2c7c) vers
src/produit/json_mode_produit.cpp et l'adapte au profil du produit (spec
produit 7). Chaque texte remplace doit se trouver exactement une fois."""
import subprocess
import sys

c = subprocess.run(["git", "show", "1cb2c7c:src/json_mode.cpp"], check=True, capture_output=True, text=True).stdout


def remplacer(avant, apres):
    global c
    n = c.count(avant)
    if n != 1:
        sys.exit(f"json_mode_produit.cpp : {n} occurrence(s) de {avant[:70]!r} (1 attendue)")
    c = c.replace(avant, apres)


def entre(debut, fin, apres):
    """Remplace de 'debut' (compris) jusqu'a 'fin' (exclu)."""
    global c
    if c.count(debut) != 1 or c.count(fin) != 1:
        sys.exit(f"json_mode_produit.cpp : bornes introuvables ou doubles : {debut[:50]!r} / {fin[:50]!r}")
    i, j = c.index(debut), c.index(fin)
    if j <= i:
        sys.exit(f"json_mode_produit.cpp : bornes dans le mauvais ordre : {debut[:50]!r}")
    c = c[:i] + apres + c[j:]


# --- En-tete et inclusions ---------------------------------------------------
remplacer(
    "// Copie de benq-screenbar-halo-matter@c58a506 : src/json_mode.cpp (adapte : profil hotte, instantanes de la sonde, evenements trame et log ; sans lampe, LED, Matter ni livraison ; sessions reseau sur UDP/Wi-Fi)\n"
    '#include "json_mode.h"\n',
    "// Copie de src/json_mode.cpp de la sonde (commit 1cb2c7c), elle-meme copiee de\n"
    "// benq-screenbar-halo-matter@c58a506 (adapte : profil du produit, instantanes\n"
    "// et evenements du produit, sessions reseau sur UDP/Thread)\n"
    '#include "json_mode_produit.h"\n',
)
remplacer(
    '#include "bord.h"\n#include "capture_rmt.h"\n#include "config.h"\n#include "fw_version.h"\n#include "h1_proto.h"\n'
    '#include "injection.h"\n#include "net_udp_wifi.h"\n#include "sonde.h"\n',
    '#include "alim.h"\n#include "config_produit.h"\n#include "fw_version.h"\n#include "h1_proto.h"\n'
    '#include "matter_hotte.h"\n#include "net_udp_thread.h"\n#include "produit.h"\n',
)

# --- Identite, profils, etat de session -------------------------------------
entre(
    "// Identite de la sonde (spec 8.5, hello.identite).\n",
    "static Writer sW;           // le seul tampon de formatage (1024 octets)\n",
    """// Identite du module (spec produit 7.3, hello.identite).
static constexpr char kFabricant[] = "Djoko-CLI";
static constexpr char kProduit[] = "Module hotte Haier";
static constexpr char kSeriePrefixe[] = "HOTTE-";
static constexpr char kNom[] = "Hotte";
static constexpr uint32_t kHw = 1;
static constexpr char kHwTxt[] = "C6 SuperMini, pilote " PILOTE_LIGNE_TEXTE;
static constexpr char kAppareil[] = "hotte";

static constexpr uint32_t kHbMs = 2000;  // battement quand les etat sont coupes ou lents
// Profil USB : etat 1 s, compteurs 1 s, reseau 5 s.
static constexpr uint32_t kPeriodDefault = 1000, kCountersDefault = 1000, kNetDefault = 5000;
static constexpr uint16_t kLeaseDefault = 30;
static constexpr uint8_t kReplies = 4;          // reponses differees (apres un instantane)
static constexpr uint32_t kHeapBlocMs = 10000;  // plus grand bloc du tas relu au plus toutes les 10 s
// Profil distant (7.6), comme la ScreenBar : etat 2 s, compteurs coupes (les
// essais les demandent : 'json compteurs 10000'), reseau 30 s ; trame_d et log
// coupes au depart. Retard admis dans la file (debit plafonne a 3000 o/s).
static constexpr uint32_t kRemotePeriod = 2000, kRemoteCounters = 0, kRemoteNet = 30000;
static constexpr uint32_t kRemoteLateMs = 6000;
static constexpr uint16_t kLogCap = 20;         // lignes log par seconde et par session
static constexpr uint16_t kTramesDCap = 10;     // lignes trame_d par seconde et par session (7.5)
static constexpr uint32_t kTramesDistantMs = 60000;  // trame_d coupees seules apres 60 s a distance

""",
)
remplacer(
    "// (net_udp_wifi.cpp : jsonRemoteReset a chaque changement).\n",
    "// (net_udp_thread.cpp : jsonRemoteReset a chaque changement).\n",
)
remplacer(
    "  bool frames = true, log = false;\n",
    "  bool frames = false, log = false;  // trame_d et log coupes au depart\n"
    "  uint32_t framesAt = 0;             // 'json trames 1' a distance : coupees seules apres 60 s\n",
)
remplacer(
    "  uint32_t trameNum = 0;             // reception dont les parties sont en cours (0 : aucune)\n"
    "  bool trameSautee = false;          // ses parties sont sautees (plafond)\n",
    "",
)
remplacer("  RateCap trameCap{kTramesParSeconde}, logCap{kLogCap};\n", "  RateCap trameCap{kTramesDCap}, logCap{kLogCap};\n")
remplacer(
    "// Compteurs de la sonde depuis le demarrage, toutes sessions (compteurs.sonde).\n",
    "// Compteurs du module depuis le demarrage, toutes sessions ('json').\n",
)
remplacer(
    "// Receptions par seconde (etat.bus), sur la derniere seconde complete.\n"
    "static uint32_t sRecAt = 0, sRecPrev = 0, sRecParS = 0;\n"
    "// Niveau de repos echantillonne hors reception (0 : jamais, 1 : haut, 2 : bas).\n"
    "static uint8_t sRepos = 0;\n",
    "// Ordres de l'app en cours (jsonOrdreApp) : l'evenement sequence porte l'id\n"
    "// de la commande dans la session qui l'a envoyee, null ailleurs.\n"
    "struct OrdreApp {\n"
    "  uint32_t numero = 0;  // numero d'ordre donne a l'automate (0 : place libre)\n"
    "  uint32_t idCmd = 0;\n"
    "  uint8_t origine = kUsb;\n"
    "};\n"
    "static constexpr uint8_t kOrdresApp = 8;\n"
    "static OrdreApp sOrdres[kOrdresApp];\n"
    "static uint32_t sNumeroOrdre = 0;\n",
)
remplacer(
    "// Evenement frequent (trame, log) vers une session reseau : seulement s'il\n",
    "// Evenement frequent (trame_d, log) vers une session reseau : seulement s'il\n",
)

# --- hello, config, etats, compteurs -----------------------------------------
remplacer('  sW.str("build", "sonde");\n  sW.str("reseau_build", "aucun");\n', '  sW.str("build", "produit");\n  sW.str("reseau_build", "thread");\n')
remplacer(
    '  sW.arr("caps");\n  sW.str(nullptr, "sonde");\n  sW.str(nullptr, "injection");\n  sW.str(nullptr, "trames");\n'
    '  sW.str(nullptr, "log");\n  sW.str(nullptr, "udp");\n  sW.str(nullptr, "cle");\n  sW.str(nullptr, "mdns");\n  sW.end();\n',
    '  sW.arr("caps");  // jamais "injection" (7.3)\n'
    '  for (const char *cap : {"hotte", "matter", "udp", "cle", "log", "trames_d", "alim", "thermique", "essai"})\n'
    "    sW.str(nullptr, cap);\n"
    "#if PILOTE_SIMULE\n"
    '  sW.str(nullptr, "simule");\n'
    "#endif\n"
    "  sW.end();\n"
    '  sW.obj("thermique");\n'
    "  const int32_t pose = produitSurveillance().maxPose();\n"
    '  if (pose != surv::Surveillance::kAucun) sW.i32("temp_max_pose_c", degres(pose));\n'
    '  else sW.null("temp_max_pose_c");\n'
    "  sW.end();\n",
)
entre(
    "static void config(uint8_t o, uint32_t now) {\n",
    "static void etatSys(uint8_t o, uint32_t now) {\n",
    """static void config(uint8_t o, uint32_t now) {
  const hotte::Params &a = produitAutomate().params();
  const surv::Params &s = produitSurveillance().params();
  sW.begin("config", sSinks[o].n, now);
  sW.obj("automate");  // noms de 'hotte regle' (4.2)
  for (uint8_t i = 0; i < hotte::kNbParams; i++) sW.u32(hotte::nomParam(i), hotte::valeurParam(a, i));
  sW.end();
  sW.obj("surveillance");
  for (uint8_t i = 0; i < surv::kNbParams; i++) sW.u32(surv::nomParam(i), surv::valeurParam(s, i));
  sW.end();
  sW.obj("ligne");
  sW.str("pilote", PILOTE_LIGNE_TEXTE);
  sW.str("mode", hotte::texte(produitPilote().mode()));
  sW.boolean("annexe", produitPilote().annexe());
  sW.end();
#if PILOTE_SIMULE
  const sim::Reglages &r = produitSimu().reglages();
  sW.obj("simu");
  sW.u32("latence_ms", r.latenceMs);
  sW.u32("periode_ms", r.periodeMs);
  sW.u32("trame_ms", r.trameMs);
  sW.u32("annexe_ms", r.annexeMs);
  sW.u32("prolongee_ms", r.prolongeeMs);
  sW.str("fin", r.finArmee ? "armee" : "eteinte");
  sW.str("inconnues", sim::texte(r.inconnues));
  sW.boolean("refuse", produitSimu().refuse());
  sW.boolean("hors_service", produitSimu().horsService());
  sW.end();
#else
  sW.null("simu");
#endif
}

static void etatHotteBloc(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "hotte");
  bootUp();
  etatHotte(sW, produitAutomate().etat(), now);
}

static void etatAutomateBloc(uint8_t o, uint32_t now) {
  const hotte::Automate &a = produitAutomate();
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "automate");
  bootUp();
  etatAutomate(sW, a.diagnostic(now), a.mode(), a.annexe(), now);
}

static void etatMatterBloc(uint8_t o, uint32_t now) {
  MatterEtat m;
  matterHotteEtat(&m, now);  // verrous pris sans attente et rendus avant le formatage
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "matter");
  bootUp();
  etatMatter(sW, m);
}

static void etatThermiqueBloc(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "thermique");
  bootUp();
  etatThermique(sW, produitSurveillance());
}

static void etatAlimBloc(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "alim");
  bootUp();
  etatAlim(sW, produitSurveillance());
}

""",
)
entre(
    "static void compteurs(uint8_t o, uint32_t now) {\n",
    "// Une reponse vient de partir vers une origine reseau : gardee pour un id repete.\n",
    """static void compteursHotteBloc(uint8_t o, uint32_t now) {
  sW.begin("compteurs", sSinks[o].n, now);
  sW.str("bloc", "hotte");
  compteursHotte(sW, produitAutomate().diagnostic(now).c);
}

static void compteursAlimBloc(uint8_t o, uint32_t now) {
  sW.begin("compteurs", sSinks[o].n, now);
  sW.str("bloc", "alim");
  compteursAlim(sW, produitSurveillance());
}

static void compteursRadioBloc(uint8_t o, uint32_t now) {
  RadioCompteurs r;
  matterHotteRadio(&r);  // verrou OpenThread pris sans attente, rendu avant le formatage
  sW.begin("compteurs", sSinks[o].n, now);
  sW.str("bloc", "radio");
  compteursRadio(sW, r);
}

""",
)
remplacer(
    "    case Item::EtatBus: etatBus(o, now); break;\n"
    "    case Item::EtatCapture: etatCapture(o, now); break;\n"
    "    case Item::EtatInjection: etatInjection(o, now); break;\n"
    "    case Item::EtatSys: etatSys(o, now); break;\n"
    "    case Item::Compteurs: compteurs(o, now); break;\n",
    "    case Item::EtatHotte: etatHotteBloc(o, now); break;\n"
    "    case Item::EtatAutomate: etatAutomateBloc(o, now); break;\n"
    "    case Item::EtatMatter: etatMatterBloc(o, now); break;\n"
    "    case Item::EtatThermique: etatThermiqueBloc(o, now); break;\n"
    "    case Item::EtatAlim: etatAlimBloc(o, now); break;\n"
    "    case Item::EtatSys: etatSys(o, now); break;\n"
    "    case Item::CompteursHotte: compteursHotteBloc(o, now); break;\n"
    "    case Item::CompteursAlim: compteursAlimBloc(o, now); break;\n"
    "    case Item::CompteursRadio: compteursRadioBloc(o, now); break;\n",
)
remplacer(
    "// Valeurs hors bornes lues en NVS au demarrage (spec 8.2) : un log a chaque\n",
    "// Reglages hors bornes lus en NVS au demarrage (4.2) : un log a chaque\n",
)
remplacer(
    '  snprintf(txt, sizeof(txt), "%u valeur(s) hors bornes en NVS : valeurs par defaut", (unsigned)sondeHorsBornes());\n'
    '  logLine(sW, k.n, now, "sonde", "notice", txt, 0);\n',
    '  snprintf(txt, sizeof(txt), "%u jeu(x) de reglages hors bornes en NVS : valeurs par defaut",\n'
    "           (unsigned)produitHorsBornes());\n"
    '  logLine(sW, k.n, now, "produit", "notice", txt, 0);\n',
)
remplacer(
    "static void pushState(uint8_t o, uint32_t now, bool session) {\n"
    "  push(o, Item::EtatBus, now, session);\n"
    "  push(o, Item::EtatCapture, now, session);\n"
    "  push(o, Item::EtatInjection, now, session);\n"
    "  push(o, Item::EtatSys, now, session);\n"
    "}\n\n"
    "static void pushCounters(uint8_t o, uint32_t now, bool session) { push(o, Item::Compteurs, now, session); }\n",
    "static void pushState(uint8_t o, uint32_t now, bool session) {\n"
    "  push(o, Item::EtatHotte, now, session);\n"
    "  push(o, Item::EtatAutomate, now, session);\n"
    "  push(o, Item::EtatMatter, now, session);\n"
    "  push(o, Item::EtatThermique, now, session);\n"
    "  push(o, Item::EtatAlim, now, session);\n"
    "  push(o, Item::EtatSys, now, session);\n"
    "}\n\n"
    "static void pushCounters(uint8_t o, uint32_t now, bool session) {\n"
    "  push(o, Item::CompteursHotte, now, session);\n"
    "  push(o, Item::CompteursAlim, now, session);\n"
    "  push(o, Item::CompteursRadio, now, session);\n"
    "}\n",
)

# --- Session -----------------------------------------------------------------
remplacer(
    "  k.frames = true;  // USB et reseau : les trames sont la raison d'etre de la sonde\n"
    "  k.trameNum = 0;\n"
    "  k.trameSautee = false;\n",
    "  k.frames = false;  // trame_d : diagnostic, a la demande ('json trames 1')\n",
)
remplacer(
    "  k.nvsLog = sondeHorsBornes() > 0;  // apres l'instantane (drain), meme sans 'json log 1'\n",
    "  k.nvsLog = produitHorsBornes() > 0;  // apres l'instantane (drain), meme sans 'json log 1'\n",
)
remplacer(
    "  k.frames = true;\n  k.log = false;\n  k.nextEtat = k.nextCpt = k.nextNet = k.nextHb = 0;\n",
    "  k.frames = false;\n  k.log = false;\n  k.nextEtat = k.nextCpt = k.nextNet = k.nextHb = 0;\n",
)
remplacer("  k.topId = 0;\n  k.trameNum = 0;\n  k.trameSautee = false;\n  k.q.clear();\n", "  k.topId = 0;\n  k.q.clear();\n")
remplacer("  k.trameCap = RateCap(kTramesParSeconde);\n", "  k.trameCap = RateCap(kTramesDCap);\n")
remplacer(
    '  out.printf("  sonde    : %lu ligne(s) perdue(s), %lu trame(s) sautee(s), %lu rejet(s), toutes sessions\\n",\n',
    '  out.printf("  module   : %lu ligne(s) perdue(s), %lu trame(s) sautee(s), %lu rejet(s), toutes sessions\\n",\n',
)
remplacer(
    '  out.println("  protocole : docs/PROTOCOLE-JSON.md (v1, profil hotte)");\n',
    '  out.println("  protocole : docs/PROTOCOLE-JSON-PRODUIT.md (v1, profil hotte du produit)");\n',
)
remplacer(
    "      const bool frames = !strcmp(sub, \"trames\");\n"
    "      (frames ? k.frames : k.log) = on;\n"
    "      if (frames) {\n"
    "        k.trameNum = 0;  // la reception en cours est jugee de nouveau\n"
    "        k.trameSautee = false;\n"
    "      }\n",
    "      const bool frames = !strcmp(sub, \"trames\");\n"
    "      (frames ? k.frames : k.log) = on;\n"
    "      if (frames) k.framesAt = now;  // a distance : coupees seules 60 s plus tard\n",
)
remplacer(
    "  // Reseau : bornes propres (liste blanche, spec 8.5), verifiees ici aussi.\n",
    "  // Reseau : bornes propres (liste blanche, spec produit 7.5), verifiees ici aussi.\n",
)

# --- Evenements --------------------------------------------------------------
entre(
    "void jsonTrame(const capt::Partie &p, bool hasRep, uint32_t rep) {\n",
    "bool jsonLog(const char *src, const char *niv, const char *txt) {\n",
    """uint32_t jsonOrdreApp(uint32_t idCmd) {
  if (++sNumeroOrdre == 0) sNumeroOrdre = 1;
  OrdreApp &o = sOrdres[sNumeroOrdre % kOrdresApp];  // le plus ancien cede sa place
  o.numero = sNumeroOrdre;
  o.idCmd = idCmd;
  o.origine = sOrigin;
  return sNumeroOrdre;
}

void jsonSequence(const hotte::Action &fin) {
  const uint32_t now = millis();
  OrdreApp ordre;
  if (fin.canal == hotte::Canal::App && fin.idOrdre)
    for (OrdreApp &o : sOrdres)
      if (o.numero == fin.idOrdre) {
        ordre = o;
        o.numero = 0;  // un seul evenement par ordre
      }
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    // L'origine attend l'evenement de son id, meme hors mode machine (comme
    // l'injection de la sonde) ; les autres le recoivent en mode machine.
    const bool attendu = ordre.numero && o == ordre.origine && ordre.idCmd;
    if ((!k.machine && !attendu) || !claim()) continue;
    evtSequence(sW, k.n, now, fin, o == ordre.origine ? ordre.idCmd : 0);
    send(o);
  }
}

void jsonHotte(const hotte::Etat &avant, const hotte::Etat &apres, hotte::Origine origine) {
  const uint32_t now = millis();
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || !claim()) continue;
    evtHotte(sW, k.n, now, avant, apres, origine);
    send(o);
  }
}

void jsonAlerte(const surv::Alerte &a) {
  const uint32_t now = millis();
  // Toujours emise vers chaque session, sans abonnement ; l'USB la recoit
  // aussi hors mode machine, en texte (7.4). Aucune action sur la hotte.
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || !claim()) continue;
    evtAlerte(sW, k.n, now, a);
    send(o);
  }
  char txt[96];
  if (a.sujet == surv::Sujet::Temperature || a.sujet == surv::Sujet::LectureTemperature)
    snprintf(txt, sizeof(txt), "alerte %s %s : %ld C (seuil %ld C)", surv::texte(a.sujet),
             a.debut ? "debut" : "fin", (long)degres(a.valeur), (long)degres(a.seuil));
  else
    snprintf(txt, sizeof(txt), "alerte %s %s : %ld mV (seuil %ld mV)", surv::texte(a.sujet),
             a.debut ? "debut" : "fin", (long)a.valeur, (long)a.seuil);
  // Doublee d'un log notice, meme sans 'json log 1'.
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || k.log || !claim()) continue;  // 'json log 1' : jsonLog l'emet ci-dessous
    logLine(sW, k.n, now, "produit", "notice", txt, 0);
    send(o);
  }
  if (!jsonLog("produit", "notice", txt) && !sSinks[kUsb].machine) jsonTexteUsb(txt);
}

void jsonTrameD(const TrameD &t) {
  const uint32_t now = millis();
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || !k.frames) continue;
    if (!k.trameCap.available(now)) {
      k.trameCap.skip();  // aucun n consomme ; la suivante produite porte 'sautes'
      sSautes++;
      continue;
    }
    if (!eventRoom(o) || !claim()) continue;
    k.trameCap.take();
    evtTrameD(sW, k.n, now, t, k.trameCap.takeSkipped());
    send(o);
  }
}

""",
)

# --- Cycle de vie ------------------------------------------------------------
remplacer("  sBoot = esp_random();\n  bootloader_random_disable();\n  sRecAt = millis();\n", "  sBoot = esp_random();\n  bootloader_random_disable();\n")
remplacer(
    "  if (now - sRecAt >= 1000) {\n"
    "    const uint32_t r = captureStats().receptions;\n"
    "    sRecParS = r - sRecPrev;\n"
    "    sRecPrev = r;\n"
    "    sRecAt = now;\n"
    "  }\n\n",
    "",
)
remplacer(
    "    if (k.machine && leaseExpired(now, k.lastRx, k.lastCmd, k.leaseS)) leaveMachine(o, true, now);\n",
    "    if (k.machine && leaseExpired(now, k.lastRx, k.lastCmd, k.leaseS)) leaveMachine(o, true, now);\n"
    "    // trame_d a distance : coupees seules apres 60 s (7.5) ; hello.base le dit.\n"
    "    if (remote(o) && k.frames && now - k.framesAt >= kTramesDistantMs) {\n"
    "      k.frames = false;\n"
    "      if (k.machine) push(o, Item::HelloBase, now, true);\n"
    "    }\n",
)

for reste in ("capt::", "sondeReglages", "injectionJson", "bordBusHaut", "captureStats", "kTramesParSeconde", "net_udp_wifi"):
    if reste in c:
        sys.exit(f"json_mode_produit.cpp : reste de la sonde : {reste}")
open("src/produit/json_mode_produit.cpp", "w").write(c)
print("json_mode_produit.cpp ecrit")
EOF
```

Sortie attendue :

```
json_mode_produit.cpp ecrit
```

- [ ] **Étape 18 : Écrire `src/produit/cli_produit.h`**

```cpp
// Copie de src/cli.h de la sonde (commit 1cb2c7c) (adapte : produit)
#pragma once
#include <stdint.h>

void cliBegin();
void cliPoll();  // lignes de l'USB ; redemarrage differe ('reboot')
// Ligne recue d'une session reseau etablie (net_udp_thread.cpp) : meme
// aiguillage que l'USB, liste blanche du produit comprise (7.5).
void cliRunRemote(uint8_t origin, char *line, bool tooLong);
```

- [ ] **Étape 19 : Écrire `src/produit/cli_produit.cpp`**

```cpp
// Copie partielle de src/cli.cpp de la sonde (commit 1cb2c7c), elle-meme reprise
// de benq-screenbar-halo-matter@c58a506 (adapte : runLine, cliPoll et
// cliRunRemote repris ; commandes du produit)
// ===========================================================================
//  Console du produit sur l'USB (docs/SPEC-PRODUIT.md 7.5) : texte humain,
//  'help' liste tout. Les commandes interdites a distance le sont par
//  jsonp::remoteRefusal (json_out_produit), pas par la table.
//
//  Lignes de l'hote (docs/PROTOCOLE-JSON-PRODUIT.md) : un prefixe id=<n> fait
//  la semantique (runLine). Avec id, 'json ...', 'hotte ventilo|lampe|regle',
//  'simu <reglage>', 'matter <reglage>', 'essai ...', 'radio rafale' et
//  'reboot' repondent sans texte (reponse ok, accepte, usage ou refuse) ; les
//  autres commandes gardent leur texte, entre une reponse debut et une
//  reponse fin.
// ===========================================================================
#include "cli_produit.h"

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <esp_mac.h>
#include <string.h>

#include "alim.h"
#include "config_produit.h"
#include "fw_version.h"
#include "json_mode_produit.h"
#include "json_out_produit.h"
#include "matter_hotte.h"
#include "net_udp_thread.h"
#include "produit.h"

typedef void (*Handler)(char *args);  // args : ce qui suit le mot-cle (peut etre "")
struct Commande {
  const char *nom;
  Handler h;
  const char *aide;
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

// Resultat d'une commande sans texte : reponse.code et reponse.msg avec un
// id, texte humain sans id.
struct Resultat {
  bool ok;
  const char *code;  // ok, accepte, usage, refuse
  const char *msg;   // nul : rien a dire ; 120 caracteres au plus (reponse.msg)
};

static uint32_t sRebootAt = 0;  // redemarrage differe (0 : aucun) : la reponse part d'abord

// ---------------------------------------------------------------------------
//  hotte ventilo | lampe | regle
// ---------------------------------------------------------------------------

static const char kUsageHotte[] = "usage : hotte [ventilo off|v1|v2|v3|derniere | lampe on|off | regle <nom> <valeur>]";

// Ordre a l'automate (7.5) : asynchrone. Accepte : l'evenement sequence suit,
// avec l'id de la commande.
static Resultat faireOrdre(char *args, uint32_t id) {
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  if (*reste) return {false, "usage", kUsageHotte};
  const uint32_t now = millis();
  if (!strcmp(args, "ventilo")) {
    hotte::CibleVentilo c = hotte::CibleVentilo::Aucune;
    if (!strcmp(valeur, "off")) c = hotte::CibleVentilo::Eteint;
    else if (!strcmp(valeur, "v1")) c = hotte::CibleVentilo::V1;
    else if (!strcmp(valeur, "v2")) c = hotte::CibleVentilo::V2;
    else if (!strcmp(valeur, "v3")) c = hotte::CibleVentilo::V3;
    else if (!strcmp(valeur, "derniere")) c = hotte::CibleVentilo::Derniere;
    else return {false, "usage", "usage : hotte ventilo off|v1|v2|v3|derniere"};
    produitAutomate().ordreVentilo(c, jsonOrdreApp(id), hotte::Canal::App, now);
    return {true, "accepte", nullptr};
  }
  if (!strcmp(args, "lampe")) {
    if (strcmp(valeur, "on") && strcmp(valeur, "off")) return {false, "usage", "usage : hotte lampe on|off"};
    produitAutomate().ordreLampe(!strcmp(valeur, "on"), jsonOrdreApp(id), hotte::Canal::App, now);
    return {true, "accepte", nullptr};
  }
  return {false, "usage", kUsageHotte};
}

// hotte regle <nom> <valeur> : automate (4.2) ou surveillance ; verifie avant la NVS.
static Resultat faireRegle(char *args) {
  static char msg[jsonp::kMsgMax + 1];
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  uint32_t v = 0;
  if (!*args || !lireU32(valeur, &v) || *reste)
    return {false, "usage", "usage : hotte regle <nom> <valeur> ('hotte' : noms et valeurs)"};
  uint32_t lo = 0, hi = 0;
  hotte::Params a = produitAutomate().params();
  switch (hotte::reglerParam(&a, args, v)) {
    case hotte::Reglage::Ok:
      if (!produitReglerAutomate(a)) return {false, "refuse", "hotte regle : applique mais pas enregistre (NVS)"};
      jsonConfigChanged();
      return {true, "ok", nullptr};
    case hotte::Reglage::HorsBornes:
      hotte::bornesParam(args, &lo, &hi);
      snprintf(msg, sizeof(msg), "hotte regle : %s %lu hors bornes (%lu..%lu ; calme <= plafond)", args,
               (unsigned long)v, (unsigned long)lo, (unsigned long)hi);
      return {false, "usage", msg};
    case hotte::Reglage::NomInconnu: break;
  }
  surv::Params s = produitSurveillance().params();
  switch (surv::reglerParam(&s, args, v)) {
    case surv::Reglage::Ok:
      if (!produitReglerSurveillance(s)) return {false, "refuse", "hotte regle : applique mais pas enregistre (NVS)"};
      jsonConfigChanged();
      return {true, "ok", nullptr};
    case surv::Reglage::HorsBornes:
      surv::bornesParam(args, &lo, &hi);
      snprintf(msg, sizeof(msg), "hotte regle : %s %lu hors bornes (%lu..%lu ; hysteresis < seuil)", args,
               (unsigned long)v, (unsigned long)lo, (unsigned long)hi);
      return {false, "usage", msg};
    case surv::Reglage::NomInconnu: break;
  }
  snprintf(msg, sizeof(msg), "hotte regle : parametre %s inconnu ('hotte' : noms)", args);
  return {false, "usage", msg};
}

static void afficherHotte() {
  const uint32_t now = millis();
  const hotte::Automate &a = produitAutomate();
  const hotte::Etat &e = a.etat();
  const hotte::Diagnostic d = a.diagnostic(now);
  Serial.printf("hotte : marche %s, moteur %s, lampe %s ; confiance %s, source %s", hotte::texte(e.marche),
                hotte::texte(e.moteur), e.lampeConnue ? (e.lampe ? "allumee" : "eteinte") : "inconnue",
                hotte::texte(e.confiance), hotte::texte(e.source));
  if (e.luMs) Serial.printf(", lu il y a %lu ms\n", (unsigned long)(now - e.luMs));
  else Serial.println(", rien lu depuis le demarrage");
  Serial.printf("  ligne %s, lecture annexe %s, pilote %s ; derniere vitesse %s ; marche %s\n",
                hotte::texte(a.mode()), a.annexe() ? "oui" : "non", d.piloteEnService ? "en service" : "HORS SERVICE",
                hotte::texte(a.derniereVitesse()), d.marcheAutorisee ? "permise (arret lu)" : "attend un arret lu");
  if (d.ventilo) Serial.printf("  ordre ventilo : %s\n", d.cibleEteint ? "eteint" : hotte::texte(d.cibleMoteur));
  if (d.lampe) Serial.printf("  ordre lampe : %s\n", d.cibleLampe ? "on" : "off");
  if (d.enVol) Serial.printf("  appui en vol : %s, essai %u\n", hotte::texte(d.touche), (unsigned)d.essai);
  const hotte::Compteurs &c = d.c;
  Serial.printf("  appuis panneau %lu, module %lu ; sequences ok %lu, annulees %lu, remplacees %lu, abandons %lu, "
                "nouveaux essais %lu\n",
                (unsigned long)c.appuisPanneau, (unsigned long)c.appuisModule, (unsigned long)c.reussies,
                (unsigned long)c.annulees, (unsigned long)c.remplacees, (unsigned long)c.abandons,
                (unsigned long)c.nouveauxEssais);
  Serial.print("  echecs :");
  for (uint8_t i = 1; i < hotte::kNbCauses; i++)
    Serial.printf(" %s %lu", hotte::texte((hotte::Cause)i), (unsigned long)c.echecs[i]);
  Serial.println();
  Serial.print("  reglages :");
  for (uint8_t i = 0; i < hotte::kNbParams; i++)
    Serial.printf(" %s %lu", hotte::nomParam(i), (unsigned long)hotte::valeurParam(a.params(), i));
  const surv::Params &s = produitSurveillance().params();
  for (uint8_t i = 0; i < surv::kNbParams; i++)
    Serial.printf(" %s %lu", surv::nomParam(i), (unsigned long)surv::valeurParam(s, i));
  Serial.println();
}

// ---------------------------------------------------------------------------
//  simu ... (build simule, USB seulement)
// ---------------------------------------------------------------------------

#if PILOTE_SIMULE
static bool lireTouche(const char *s, hotte::Touche *t) {
  static const hotte::Touche kTouches[] = {hotte::Touche::Marche, hotte::Touche::Lumiere, hotte::Touche::V1,
                                           hotte::Touche::V2, hotte::Touche::V3};
  for (hotte::Touche x : kTouches)
    if (!strcmp(s, hotte::texte(x))) {
      *t = x;
      return true;
    }
  return false;
}

// eteinte | armee | v1..v3 | prol-v1..prol-v3, puis [lampe]
static bool lireEtatHotte(char *s, sim::EtatHotte *e) {
  char *lampe = splitWord(s);
  char *reste = splitWord(lampe);
  if (*reste || (*lampe && strcmp(lampe, "lampe"))) return false;
  sim::EtatHotte x;
  x.lampe = *lampe != 0;
  const bool prol = !strncmp(s, "prol-", 5);
  const char *v = prol ? s + 5 : s;
  if (!strcmp(s, "eteinte")) {
    x.marche = hotte::Marche::Eteinte;
  } else if (!strcmp(s, "armee")) {
    x.marche = hotte::Marche::Armee;
  } else if (!strcmp(v, "v1") || !strcmp(v, "v2") || !strcmp(v, "v3")) {
    x.marche = prol ? hotte::Marche::Prolongee : hotte::Marche::Armee;
    x.moteur = (hotte::Moteur)(v[1] - '0');
  } else {
    return false;
  }
  *e = x;
  return sim::etatHotteValide(x);
}

static const char kUsageSimu[] =
    "usage : simu [mode repete|changements|appuis | annexe 0|1 | latence|periode|prolongee <ms> | fin eteinte|armee"
    " | inconnues rien|direct | appui <touche> | coupure | illisible|sans_effet|perdu|delai <n> | collision <n> [prend]"
    " | refuse 0|1 | hs 0|1 | apres_demarrage <etat> [lampe] | auto <s>]";

static Resultat faireSimu(char *args) {
  static char msg[jsonp::kMsgMax + 1];
  sim::PiloteSimule &p = produitSimu();
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  const uint32_t now = millis();
  sim::Reglages r = p.reglages();
  uint32_t n = 0;
  if (!strcmp(args, "appui") && !*reste) {
    hotte::Touche t;
    if (!lireTouche(valeur, &t)) return {false, "usage", "usage : simu appui marche|lumiere|v1|v2|v3"};
    p.appuiPanneau(t, now);
    return {true, "ok", nullptr};
  }
  if (!strcmp(args, "auto") && !*reste && lireU32(valeur, &n) && n <= 3600) {
    produitSimuAuto(n);  // banc B11 : 0 arrete
    if (!n) return {true, "ok", "endurance arretee"};
    snprintf(msg, sizeof(msg), "endurance : un appui du panneau toutes les %lu s", (unsigned long)n);
    return {true, "ok", msg};
  }
  if (!strcmp(args, "coupure") && !*valeur) {
    p.coupure(now);
    return {true, "ok", "hotte simulee : coupure secteur, revenue eteinte"};
  }
  if (!strcmp(args, "apres_demarrage")) {
    sim::EtatHotte e;
    char etat[40];
    snprintf(etat, sizeof(etat), "%s%s%s", valeur, *reste ? " " : "", reste);
    if (!lireEtatHotte(etat, &e)) return {false, "usage", "usage : simu apres_demarrage eteinte|armee|v1..v3|prol-v1..v3 [lampe]"};
    if (!produitSimuApresDemarrage(e)) return {false, "refuse", "simu apres_demarrage : echec de l'ecriture en NVS"};
    return {true, "ok", "hotte simulee : cet etat au prochain demarrage, une fois"};
  }
  if (*reste && strcmp(args, "collision")) return {false, "usage", kUsageSimu};
  if (!strcmp(args, "mode")) {
    if (!strcmp(valeur, "repete")) r.mode = hotte::ModeEtat::Repete;
    else if (!strcmp(valeur, "changements")) r.mode = hotte::ModeEtat::Changements;
    else if (!strcmp(valeur, "appuis")) r.mode = hotte::ModeEtat::AppuisSeuls;
    else return {false, "usage", "usage : simu mode repete|changements|appuis"};
  } else if (!strcmp(args, "annexe") && (!strcmp(valeur, "0") || !strcmp(valeur, "1"))) {
    r.annexe = !strcmp(valeur, "1");
  } else if (!strcmp(args, "latence") && lireU32(valeur, &n)) {
    r.latenceMs = n;
  } else if (!strcmp(args, "periode") && lireU32(valeur, &n)) {
    r.periodeMs = n;
  } else if (!strcmp(args, "prolongee") && lireU32(valeur, &n)) {
    r.prolongeeMs = n;
  } else if (!strcmp(args, "fin") && (!strcmp(valeur, "eteinte") || !strcmp(valeur, "armee"))) {
    r.finArmee = !strcmp(valeur, "armee");
  } else if (!strcmp(args, "inconnues") && (!strcmp(valeur, "rien") || !strcmp(valeur, "direct"))) {
    r.inconnues = !strcmp(valeur, "direct") ? sim::Inconnues::Direct : sim::Inconnues::Rien;
  } else if ((!strcmp(args, "illisible") || !strcmp(args, "sans_effet") || !strcmp(args, "perdu") ||
              !strcmp(args, "delai") || !strcmp(args, "collision")) &&
             lireU32(valeur, &n) && n <= 255) {
    const bool prend = !strcmp(reste, "prend");
    if (*reste && !prend) return {false, "usage", "usage : simu collision <n> [prend]"};
    if (!strcmp(args, "illisible")) p.illisibles((uint8_t)n);
    else if (!strcmp(args, "sans_effet")) p.sansEffet((uint8_t)n);
    else if (!strcmp(args, "perdu")) p.perdus((uint8_t)n);
    else if (!strcmp(args, "delai")) p.delais((uint8_t)n);
    else p.collisions((uint8_t)n, prend);
    return {true, "ok", nullptr};
  } else if (!strcmp(args, "refuse") && (!strcmp(valeur, "0") || !strcmp(valeur, "1"))) {
    p.refuser(!strcmp(valeur, "1"));
    jsonConfigChanged();
    return {true, "ok", nullptr};
  } else if (!strcmp(args, "hs") && (!strcmp(valeur, "0") || !strcmp(valeur, "1"))) {
    p.horsService(!strcmp(valeur, "1"), now);
    jsonConfigChanged();
    return {true, "ok", nullptr};
  } else {
    return {false, "usage", kUsageSimu};
  }
  if (!sim::reglagesValides(r)) {
    snprintf(msg, sizeof(msg), "simu %s : hors bornes (latence 0..5000, periode 100..60000, prolongee 60000..3600000)",
             args);
    return {false, "usage", msg};
  }
  if (!produitReglerSimu(r)) return {false, "refuse", "simu : applique mais pas enregistre (NVS)"};
  return {true, "ok", nullptr};
}

static void afficherSimu() {
  const sim::PiloteSimule &p = produitSimu();
  const sim::EtatHotte h = p.hotte();
  const sim::Reglages &r = p.reglages();
  Serial.printf("hotte simulee : marche %s, moteur %s, lampe %s%s%s", hotte::texte(h.marche), hotte::texte(h.moteur),
                h.lampe ? "allumee" : "eteinte", p.refuse() ? ", REFUSE les appuis du module" : "",
                p.horsService() ? ", pilote HORS SERVICE" : "");
  if (produitSimuAutoS()) Serial.printf(", endurance : un appui toutes les %lu s", (unsigned long)produitSimuAutoS());
  Serial.println();
  Serial.printf("  mode %s, annexe %s, latence %lu ms, periode %lu ms, trame %lu ms, prolongee %lu ms (fin %s), "
                "inconnues %s\n",
                hotte::texte(r.mode), r.annexe ? "oui" : "non", (unsigned long)r.latenceMs, (unsigned long)r.periodeMs,
                (unsigned long)r.trameMs, (unsigned long)r.prolongeeMs, r.finArmee ? "armee" : "eteinte",
                sim::texte(r.inconnues));
  ligne::Diagnostic d;
  p.diagnostic(&d);
  Serial.printf("  emis %lu, refus %lu, collisions %lu, delais %lu, etats lus %lu, annexe %lu, anomalies %lu, "
                "appuis vus %lu, perdus %lu\n",
                (unsigned long)d.appuisEmis, (unsigned long)d.refusGarde, (unsigned long)d.collisions,
                (unsigned long)d.delais, (unsigned long)d.etatsLus, (unsigned long)d.lecturesAnnexe,
                (unsigned long)d.anomalies, (unsigned long)d.appuisVus, (unsigned long)d.evenementsPerdus);
}
#endif

// ---------------------------------------------------------------------------
//  matter <reglage>, essai, radio rafale
// ---------------------------------------------------------------------------

static const char kUsageMatter[] = "usage : matter [med 0|1 | tx 8..20 | maxint 0|10..3600 | derniere 0|1 | journal]";

static Resultat faireMatter(char *args) {
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  uint32_t v = 0;
  bool saved = false;
  if (*reste || !lireU32(valeur, &v)) return {false, "usage", kUsageMatter};
  bool ok = false;
  const char *apres = nullptr;
  if (!strcmp(args, "med")) {
    ok = matterReglerMed(v, &saved);
    apres = "role applique au prochain demarrage ('reboot')";
  } else if (!strcmp(args, "tx")) {
    ok = matterReglerTx(v, &saved);
  } else if (!strcmp(args, "maxint")) {
    ok = matterReglerMaxint(v, &saved);
    apres = "pour les abonnements neufs";
  } else if (!strcmp(args, "derniere")) {
    ok = v <= 1 && matterReglerDerniere(v == 1, &saved);
  }
  if (!ok) return {false, "usage", kUsageMatter};
  jsonConfigChanged();
  if (!saved) return {false, "refuse", "matter : applique mais pas enregistre (NVS)"};
  return {true, "ok", apres};
}

static Resultat faireEssai(char *args) {
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  if (strcmp(args, "alim") || (strcmp(valeur, "on") && strcmp(valeur, "off")) || *reste)
    return {false, "usage", "usage : essai alim on|off"};
  if (!produitReglerEssai(!strcmp(valeur, "on"))) return {false, "refuse", "essai : echec de l'ecriture en NVS"};
  return {true, "ok", strcmp(valeur, "on") ? "essai de 24 h desarme" : "essai de 24 h arme pour 72 h : 'radio rafale' permise a distance"};
}

static Resultat faireRafale(char *args) {
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  uint32_t s = 0;
  if (strcmp(args, "rafale") || !lireU32(valeur, &s) || *reste || s < 1 || s > 30)
    return {false, "usage", "usage : radio rafale <1..30 s>"};
  if (!produitEssai()) return {false, "refuse", "radio rafale : seulement pendant l'essai de 24 h ('essai alim on')"};
  const uint8_t o = jsonOrigin();
  if (o == jsonp::kUsb || !netUdpRafale((uint8_t)(o - 1), s))
    return {false, "refuse", "radio rafale : depuis une session reseau etablie seulement"};
  return {true, "ok", nullptr};
}

// ---------------------------------------------------------------------------
//  Commandes
// ---------------------------------------------------------------------------

static void imprimer(const Resultat &r) {
  if (r.msg) Serial.println(r.msg);
  else Serial.println(r.ok ? "ok" : r.code);
}

static void cmdHelp(char *args);  // apres la table, qu'elle parcourt

static void cmdInfo(char *) {
  uint8_t mac[8] = {};
  Serial.printf("firmware %s (env %s), IDF %s, Arduino %s\n", FW_VERSION_FULL, FW_ENV, esp_get_idf_version(),
                ESP_ARDUINO_VERSION_STR);
  if (esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK)
    Serial.printf("MAC %02X:%02X:%02X:%02X:%02X:%02X, serie HOTTE-%02X%02X%02X%02X%02X%02X\n", mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("pilote %s ; GPIO%u (injection) tenue basse ; ecoute GPIO%u\n", PILOTE_LIGNE_TEXTE,
                (unsigned)kPinInjectionD, (unsigned)kPinEcouteD);
  char kid[9];
  if (netUdpKid(kid)) Serial.printf("udp : port %u, cle %s\n", (unsigned)kPortUdp, kid);
  else Serial.printf("udp : port %u ferme, aucune cle ('python3 tools/hotte_udp.py cle <port>')\n", (unsigned)kPortUdp);
  Serial.printf("essai de 24 h : %s\n", produitEssai() ? "arme" : "non");
  matterHotteStatut(Serial);
}

static void cmdHotte(char *args) {
  if (!*args) {
    afficherHotte();
    return;
  }
  char *reste = splitWord(args);
  if (!strcmp(args, "regle")) {
    imprimer(faireRegle(reste));
    return;
  }
  // Remet le mot detache : faireOrdre relit "ventilo v2".
  char ligne[48];
  snprintf(ligne, sizeof(ligne), "%s %s", args, reste);
  const Resultat r = faireOrdre(ligne, 0);
  if (!r.ok) Serial.println(r.msg);
  else Serial.println("hotte : ordre accepte, la sequence suit");
}

#if PILOTE_SIMULE
static void cmdSimu(char *args) {
  if (*args) {
    const Resultat r = faireSimu(args);
    if (r.msg) Serial.println(r.msg);
    if (!r.ok) return;
  }
  afficherSimu();
}
#endif

static void cmdMatter(char *args) {
  if (!*args) {
    matterHotteStatut(Serial);
    return;
  }
  if (!strcmp(args, "journal")) {
    matterHotteJournal(Serial);
    return;
  }
  imprimer(faireMatter(args));
}

static void cmdAlim(char *args) {
  if (!strcmp(args, "arret")) alimArreter();
  else if (!strcmp(args, "reprise") && !alimReprendre()) Serial.println("alim : reprise de l'ADC en echec");
  else if (*args && strcmp(args, "reprise")) Serial.println("usage : alim [arret | reprise]");
  alimStatut(Serial, produitSurveillance());
}

static void cmdThermique(char *args) {
  if (strcmp(args, "raz")) {
    Serial.println("usage : thermique raz   (a la pose : maximum depuis la pose remis a zero)");
    return;
  }
  produitSurveillance().maxPoseRaz();
  Serial.println("thermique : maximum depuis la pose remis a zero");
}

static void cmdEssai(char *args) { imprimer(faireEssai(args)); }

static void cmdRadio(char *args) { imprimer(faireRafale(args)); }

static void cmdJson(char *args) { jsonCommand(args, JsonCmd{false, 0, "", millis()}); }

static void cmdDecommission(char *) {
  Serial.println("decommission : cle H1 effacee, noeud retire de toutes ses fabriques, redemarrage");
  produitEcrireEnAttente();
  matterHotteDecommission();
}

static void cmdReboot(char *) {
  Serial.println("redemarrage (valeurs NVS en attente ecrites d'abord)");
  sRebootAt = millis() + 200;
}

static const Commande kCommandes[] = {
  {"help", cmdHelp, "liste des commandes"},
  {"info", cmdInfo, "version, MAC, pilote, cle, Matter"},
  {"hotte", cmdHotte, "hotte [ventilo off|v1|v2|v3|derniere | lampe on|off | regle <nom> <valeur>]"},
#if PILOTE_SIMULE
  {"simu", cmdSimu, "simu [reglage | appui | coupure | panne | apres_demarrage] : hotte simulee"},
#endif
  {"matter", cmdMatter, "matter [med 0|1 | tx 8..20 | maxint s | derniere 0|1 | journal]"},
  {"alim", cmdAlim, "alim [arret | reprise] : tensions et temperature de la puce"},
  {"thermique", cmdThermique, "thermique raz : maximum depuis la pose remis a zero"},
  {"essai", cmdEssai, "essai alim on|off : essai de 24 h (radio rafale permise a distance)"},
  {"radio", cmdRadio, "radio rafale <s> : rafale d'emission (essai de 24 h, session reseau)"},
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log|cle] : mode machine"},
  {"decommission", cmdDecommission, "remise a zero Matter et effacement de la cle H1 (USB)"},
  {"reboot", cmdReboot, "redemarrage (valeurs NVS en attente ecrites d'abord)"},
};

static void cmdHelp(char *) {
  Serial.println("=== Commandes ===");
  for (const Commande &c : kCommandes) Serial.printf("  %-12s %s\n", c.nom, c.aide);
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

// ---------------------------------------------------------------------------
//  Lignes de l'hote. runLine et cliPoll sont repris de src/cli.cpp de la sonde.
// ---------------------------------------------------------------------------

static bool firstWordIs(const char *s, const char *w) {
  const size_t n = strlen(w);
  return !strncmp(s, w, n) && (s[n] == ' ' || !s[n]);
}

static char *afterWord(char *s) {
  while (*s && *s != ' ') s++;
  while (*s == ' ') s++;
  return s;
}

// Commande a reponse seule (sans texte), avec id : rend vrai et remplit r.
static bool sansTexte(char *cmd, uint32_t id, jsonp::Reply *r) {
  Resultat res{};
  if (firstWordIs(cmd, "hotte") && *afterWord(cmd)) {
    char *args = afterWord(cmd);
    if (firstWordIs(args, "regle")) res = faireRegle(afterWord(args));
    else res = faireOrdre(args, id);
#if PILOTE_SIMULE
  } else if (firstWordIs(cmd, "simu") && *afterWord(cmd)) {
    res = faireSimu(afterWord(cmd));
#endif
  } else if (firstWordIs(cmd, "matter") && *afterWord(cmd) && !firstWordIs(afterWord(cmd), "journal")) {
    res = faireMatter(afterWord(cmd));
  } else if (firstWordIs(cmd, "essai")) {
    res = faireEssai(afterWord(cmd));
  } else if (firstWordIs(cmd, "radio")) {
    res = faireRafale(afterWord(cmd));
  } else if (firstWordIs(cmd, "reboot") && !*afterWord(cmd)) {
    sRebootAt = millis() + 500;  // la reponse part d'abord (datagramme compris)
    res = {true, "ok", "redemarrage dans 500 ms"};
  } else {
    return false;
  }
  r->ok = res.ok;
  r->code = res.code;
  r->msg = res.msg;
  // Ordre accepte : l'evenement sequence suivra, avec cet id.
  if (res.ok && !strcmp(res.code, "accepte")) r->suite = jsonp::Reply::SuiteSequence;
  return true;
}

// Une ligne de l'hote : prefixe id=<n> retire avant l'aiguillage, refus sans
// execution (trop longue, cadence, interdite a distance), puis :
//  - sans id : comme toujours (texte) ;
//  - avec id : famille json (reponse seule) ; commandes sans texte (reponse
//    ok, accepte, usage ou refuse) ; sinon commande a texte entre reponse
//    debut et reponse fin.
// A distance, jsonRemoteAdmit passe avant tout refus (id deja vu).
static void runLine(char *line, bool tooLong) {
  const uint32_t t0 = millis();
  const bool remote = jsonOrigin() != jsonp::kUsb;
  uint32_t id = 0;
  char *cmd = line;
  const bool hasId = jsonp::parseIdPrefix(line, &id, &cmd);
  while (*cmd == ' ') cmd++;
  size_t len = strlen(cmd);
  while (len && cmd[len - 1] == ' ') cmd[--len] = 0;
  if (remote && !hasId) {
    jsonCountRejected();  // a distance, toujours un id : sans lui, aucune reponse possible
    return;
  }
  if (!hasId && !*cmd && !tooLong) return;  // ligne vide (Ctrl-U compris) : rien
  char shown[jsonp::kCmdTextMax + 1];
  jsonp::copyCmd(shown, cmd);  // avant que l'aiguillage ne coupe la ligne en mots
  jsonp::maskCmd(shown);       // jamais l'alea de 'json cle nouvelle' dans la reponse
  const JsonCmd c{hasId, id, shown, t0};
  if (remote && jsonRemoteAdmit(id, shown)) return;
  if (tooLong) {
    jsonRefuse(c, "trop_long", "ligne de plus de 127 octets : rien n'est execute");
    return;
  }
  if ((hasId || jsonMachine() || remote) && !jsonCadenceOk(t0)) {
    jsonRefuse(c, "cadence", "plus de 20 lignes par seconde : rien n'est execute");
    return;
  }
  if (remote) {
    if (const char *why = jsonp::remoteRefusal(cmd, produitEssai())) {
      jsonRefuse(c, "interdite", why);
      jsonAfterCommand();
      return;
    }
  }
  if (!hasId) {
    if (!executer(cmd)) Serial.printf("commande inconnue : %s ('help')\n", cmd);
    jsonAfterCommand();
    return;
  }
  jsonp::Reply r;
  r.id = id;
  r.cmd = shown;
  if (!*cmd) {
    r.ok = false;
    r.code = "inconnue";
    r.msg = "commande vide";
    jsonReply(r);
    return;
  }
  if (firstWordIs(cmd, "json")) {
    jsonCommand(afterWord(cmd), c);
  } else if (sansTexte(cmd, id, &r)) {
    r.durMs = millis() - t0;
    jsonReply(r);
  } else {
    r.fin = false;
    r.code = "en_cours";
    jsonReply(r);  // la commande va ecrire son texte
    const bool known = executer(cmd);
    if (!known) Serial.printf("commande inconnue : %s ('help')\n", cmd);
    r.fin = true;
    r.ok = known;
    r.code = known ? "execute" : "inconnue";
    r.durMs = millis() - t0;
    jsonReplyEnd(r);
  }
  jsonAfterCommand();
}

void cliRunRemote(uint8_t origin, char *line, bool tooLong) {
  // Jamais l'USB par ce chemin (il echappe a la liste blanche).
  if (origin == jsonp::kUsb || origin >= jsonp::kOrigins) return;
  const uint8_t prev = jsonOrigin();
  jsonSetOrigin(origin);
  if (jsonOrigin() == origin) runLine(line, tooLong);
  jsonSetOrigin(prev);
}

static jsonp::LineAssembler sLine;  // 127 caracteres au plus, prefixe id= compris

void cliBegin() {
  Serial.println("Tape 'help' pour la liste des commandes.");
  Serial.print("> ");
}

void cliPoll() {
  if (sRebootAt && (int32_t)(millis() - sRebootAt) >= 0) {
    produitEcrireEnAttente();  // 4.4 : ce qui attend en NVS, d'abord
    ESP.restart();
  }
  while (Serial.available()) {
    const uint8_t c = (uint8_t)Serial.read();
    jsonNoteRx();
    const bool machine = jsonMachine();
    switch (sLine.feed(c, machine)) {
      case jsonp::LineAssembler::Ev::Echo:
        if (!machine) Serial.print((char)c);
        break;
      case jsonp::LineAssembler::Ev::Erase:
        if (!machine) Serial.print("\b \b");
        break;
      case jsonp::LineAssembler::Ev::Clear:
        if (!machine)
          for (uint8_t i = 0; i < sLine.cleared(); i++) Serial.print("\b \b");
        break;
      case jsonp::LineAssembler::Ev::Line: {
        if (!machine) Serial.println();
        const bool tooLong = sLine.tooLong();
        runLine(sLine.text(), tooLong);
        sLine.reset();
        if (!machine && !jsonMachine()) {
          Serial.flush();  // l'USB CDC du C6 perd des octets si on enchaine trop vite
          Serial.print("> ");
        }
        break;
      }
      case jsonp::LineAssembler::Ev::None: break;
    }
  }
}
```

- [ ] **Étape 20 : Écrire `src/produit/main_produit.cpp`**

`setup()` suit le §5.7 : GPIO1 basse en toute première instruction, `enableLoopWDT()` en dernier. Aucun appui n'est injecté au démarrage.

```cpp
// ===========================================================================
//  Module de la hotte Haier CCHP21EBB (docs/SPEC-PRODUIT.md) : noeud Matter
//  sur Thread, EP1 Ventilateur, EP2 Lumiere, canal compagnon profil "hotte".
//
//  setup() (5.7) : GPIO1 bas en premier, console, NVS, pilote (ecoute
//  seulement), etat initial, Matter (identite, Thread, EP1, EP2), transport
//  reseau, mesures, chien de garde en dernier. Rien n'est injecte au demarrage.
//  loop() (4.1) : pilote -> automate ; boite d'intentions Matter -> automate ;
//  console et reseau -> automate ; actions de l'automate (appuis, reflets,
//  evenements) ; mode machine ; mesures et surveillance ; ecritures NVS
//  differees ; delay(1). Aucune attente bloquante.
// ===========================================================================
#include <Arduino.h>
#include <esp_log.h>
#include <esp_system.h>
#include <lib/support/logging/CHIPLogging.h>
#include <stdarg.h>

#include "alim.h"
#include "cli_produit.h"
#include "config_produit.h"
#include "fw_version.h"
#include "hotte_map.h"
#include "json_mode_produit.h"
#include "matter_hotte.h"
#include "net_udp_thread.h"
#include "produit.h"
#include "reglages_produit.h"

static hotte::Automate sAutomate;
static surv::Surveillance sSurv;
#if PILOTE_SIMULE
static sim::PiloteSimule sSimu;
static ligne::Pilote &sPilote = sSimu;
// Etat de la hotte simulee a travers un redemarrage qui n'est pas une mise
// sous tension (4.3) : memoire RTC non initialisee, avec un mot de controle.
RTC_NOINIT_ATTR static uint32_t sRtcMot;
RTC_NOINIT_ATTR static uint16_t sRtcEtat;
static constexpr uint32_t kMotRtc = 0x484F5454;  // "HOTT"
#else
#error "pilote reel (A, B ou C) : second plan, apres l'etape 6"
#endif
static uint8_t sHorsBornes = 0;
static hotte::EcritureDifferee sNvsDerniere, sNvsEtatPub;  // 10 s apres le dernier changement (4.4)
static uint16_t sEssaiMin = 0;  // essai de 24 h : minutes restantes (72 h au plus)
static uint32_t sEssaiMinuteMs = 0;
static uint8_t sEssaiDepuisEcrit = 0;

hotte::Automate &produitAutomate() { return sAutomate; }
ligne::Pilote &produitPilote() { return sPilote; }
surv::Surveillance &produitSurveillance() { return sSurv; }
uint8_t produitHorsBornes() { return sHorsBornes; }

bool produitReglerAutomate(const hotte::Params &p) {
  sAutomate.configurer(p);
#if PILOTE_SIMULE
  sim::Reglages r = sSimu.reglages();  // le garde-fou 7 du pilote suit l'automate
  r.delaiMoteurMs = p.delaiMoteurMs;
  sSimu.regler(r);
#endif
  return nvsSauverParams(p);
}

bool produitReglerSurveillance(const surv::Params &p) {
  sSurv.configurer(p);
  return nvsSauverSurveillance(p);
}

void produitEcrireEnAttente() {
  if (sNvsDerniere.enAttente() && nvsEcrireDerniere((hotte::Moteur)sNvsDerniere.valeur()))
    sNvsDerniere.ecrite(sNvsDerniere.valeur());
  if (sNvsEtatPub.enAttente()) {
    hotte::Etat e;
    if (hotte::depuisNvs((uint16_t)sNvsEtatPub.valeur(), &e) && nvsEcrireEtatPub(e))
      sNvsEtatPub.ecrite(sNvsEtatPub.valeur());
  }
  if (sSurv.maxPose() != surv::Surveillance::kAucun && nvsEcrireTempMax(sSurv.maxPose())) sSurv.maxPoseEcrit(millis());
  if (sEssaiMin) nvsEcrireEssaiMin(sEssaiMin);
}

bool produitEssai() { return sEssaiMin != 0; }
uint32_t produitEssaiResteMin() { return sEssaiMin; }
bool produitReglerEssai(bool on) {
  sEssaiMin = on ? 4320 : 0;  // 72 h
  sEssaiMinuteMs = millis();
  sEssaiDepuisEcrit = 0;
  return nvsEcrireEssaiMin(sEssaiMin);
}

#if PILOTE_SIMULE
sim::PiloteSimule &produitSimu() { return sSimu; }

bool produitReglerSimu(const sim::Reglages &r) {
  sim::Reglages n = r;
  n.delaiMoteurMs = sAutomate.params().delaiMoteurMs;
  sSimu.regler(n);
  sAutomate.configurerLigne(n.mode, n.annexe);
  jsonConfigChanged();
  return nvsSauverSimu(n);
}

bool produitSimuApresDemarrage(const sim::EtatHotte &e) { return nvsEcrireSimuDem(e); }

// Cycle d'endurance : marche, V2, lumiere, V3, V3 (arret), marche (eteinte), lumiere.
static const hotte::Touche kCycle[] = {hotte::Touche::Marche, hotte::Touche::V2, hotte::Touche::Lumiere,
                                       hotte::Touche::V3,     hotte::Touche::V3, hotte::Touche::Marche,
                                       hotte::Touche::Lumiere};
static uint32_t sAutoS = 0, sAutoMs = 0;
static uint8_t sAutoRang = 0;

void produitSimuAuto(uint32_t periodeS) {
  sAutoS = periodeS;
  sAutoMs = millis();
  sAutoRang = 0;
}
uint32_t produitSimuAutoS() { return sAutoS; }

static void simuAuto(uint32_t now) {
  if (!sAutoS || now - sAutoMs < sAutoS * 1000) return;
  sAutoMs = now;
  sSimu.appuiPanneau(kCycle[sAutoRang], now);
  sAutoRang = (uint8_t)((sAutoRang + 1) % (sizeof(kCycle) / sizeof(kCycle[0])));
}
#endif

// ---------------------------------------------------------------------------
//  Traces de la pile CHIP (reprises de benq, src/main.cpp) : esp-matter
//  n'emet pas par les macros ESP_LOGx ; le crochet de sortie du log est le
//  seul point de passage. Les lignes "chip[" sont avalees.
// ---------------------------------------------------------------------------

static vprintf_like_t sVprintfPrecedent = nullptr;

static int vprintfSilencieux(const char *fmt, va_list args) {
  char ligne[256];
  va_list copie;
  va_copy(copie, args);
  const int n = vsnprintf(ligne, sizeof(ligne), fmt, copie);
  va_end(copie);
  if (n > 0 && strstr(ligne, "chip[")) return 0;
  return sVprintfPrecedent ? sVprintfPrecedent(fmt, args) : vprintf(fmt, args);
}

static void traceChip(bool on) {
  if (!sVprintfPrecedent) sVprintfPrecedent = esp_log_set_vprintf(vprintfSilencieux);
  chip::Logging::SetLogFilter(on ? chip::Logging::kLogCategory_Progress : chip::Logging::kLogCategory_None);
}

// ---------------------------------------------------------------------------
//  Evenements vers le compagnon et la console
// ---------------------------------------------------------------------------

static void texte(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void texte(const char *fmt, ...) {
  char ligne[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ligne, sizeof(ligne), fmt, ap);
  va_end(ap);
  if (!jsonLog("hotte", "notice", ligne) && !jsonMachine()) jsonTexteUsb(ligne);
}

static void actions(uint32_t now) {
  for (;;) {
    const hotte::Action a = sAutomate.suivante(now);
    switch (a.type) {
      case hotte::Action::Aucune: return;
      case hotte::Action::Appuyer: {
        const ligne::Admission ad = sPilote.appuyer(a.touche, a.idAppui, now);
        if (ad != ligne::Admission::Acceptee) sAutomate.surFinAppui(a.idAppui, ligne::resultatAdmission(ad), now);
        break;
      }
      case hotte::Action::Publier:
        matterHottePublier(a.etat);
        sNvsEtatPub.noter(hotte::versNvs(a.etat), now);
        break;
      case hotte::Action::FinSequence:
        jsonSequence(a);
        texte("[hotte] sequence %s : %s%s%s (%u appuis, %lu ms)", hotte::texte(a.sujet), hotte::texte(a.issue),
              a.cause == hotte::Cause::Aucune ? "" : ", ", a.cause == hotte::Cause::Aucune ? "" : hotte::texte(a.cause),
              (unsigned)a.appuis, (unsigned long)a.dureeMs);
        break;
      case hotte::Action::Changement:
        jsonHotte(a.avant, a.etat, a.origine);
        texte("[hotte] %s/%s -> %s/%s, lampe %s (%s, %s)", hotte::texte(a.avant.marche), hotte::texte(a.avant.moteur),
              hotte::texte(a.etat.marche), hotte::texte(a.etat.moteur),
              a.etat.lampeConnue ? (a.etat.lampe ? "on" : "off") : "?", hotte::texte(a.origine),
              hotte::texte(a.etat.source));
        break;
      case hotte::Action::Evenement: texte("[hotte] %s", hotte::texte(a.evt)); break;
    }
  }
}

static void evenementsPilote(uint32_t now) {
  ligne::Evenement ev;
  while (sPilote.evenement(&ev)) {
    switch (ev.type) {
      case ligne::Evenement::EtatLu: sAutomate.surEtatLu(ev.etat, now); break;
      case ligne::Evenement::Appui: sAutomate.surAppui(ev.touche, ev.origine, now); break;
      case ligne::Evenement::FinAppui: sAutomate.surFinAppui(ev.idAppui, ev.resultat, now); break;
      case ligne::Evenement::Anomalie: sAutomate.surAnomalie(ev.codeAnomalie, now); break;
      case ligne::Evenement::Service: sAutomate.surPilote(ev.enService, now); break;
    }
  }
  ligne::Trame t;
  while (sPilote.trame(&t)) {
    jsonp::TrameD d;
    d.tMs = t.tMs;
    d.origine = (jsonp::OrigineTrame)t.origine;
    d.n = t.n > 8 ? 8 : t.n;
    for (uint8_t i = 0; i < d.n; i++) d.octets[i] = t.octets[i];
    jsonTrameD(d);
  }
}

// Ecritures NVS differees (4.4), maximum de temperature depuis la pose,
// compte a rebours de l'essai de 24 h.
static void nvsDifferee(uint32_t now) {
  sNvsDerniere.noter((uint32_t)sAutomate.derniereVitesse(), now);
  if (sNvsDerniere.echue(now) && nvsEcrireDerniere((hotte::Moteur)sNvsDerniere.valeur()))
    sNvsDerniere.ecrite(sNvsDerniere.valeur());
  if (sNvsEtatPub.echue(now)) {
    hotte::Etat e;
    if (hotte::depuisNvs((uint16_t)sNvsEtatPub.valeur(), &e) && nvsEcrireEtatPub(e))
      sNvsEtatPub.ecrite(sNvsEtatPub.valeur());
  }
  if (sSurv.maxPoseAEcrire(now) && nvsEcrireTempMax(sSurv.maxPose())) sSurv.maxPoseEcrit(now);
  if (sEssaiMin && now - sEssaiMinuteMs >= 60000) {
    sEssaiMinuteMs += 60000;
    sEssaiMin--;
    if (!sEssaiMin || ++sEssaiDepuisEcrit >= 60) {  // une ecriture par heure, et a l'echeance
      sEssaiDepuisEcrit = 0;
      nvsEcrireEssaiMin(sEssaiMin);
    }
  }
}

void setup() {
  // TOUJOURS la premiere instruction (4.3, garde-fou 1) : la sortie
  // d'injection a l'etat bas. Le build simule ne lie aucun code d'injection.
  pinMode(kPinInjectionD, OUTPUT);
  digitalWrite(kPinInjectionD, LOW);
  Serial.setTxBufferSize(8192);  // une ligne machine fait jusqu'a 1024 octets
  Serial.begin(115200);
  jsonBegin();  // identifiant de ce demarrage (hello.boot), avant toute radio
  rgbLedWrite(kPinLedRgb, 0, 0, 0);  // aucun voyant ajoute (8.6)
  traceChip(false);
  const esp_reset_reason_t cause = esp_reset_reason();
  Serial.printf("\n=== Module hotte Haier : firmware %s (%s), pilote %s ===\n", FW_VERSION_FULL, FW_ENV,
                PILOTE_LIGNE_TEXTE);

  // Reglages (bornes appliquees au chargement), derniere vitesse, maxima.
  hotte::Params pa;
  surv::Params ps;
  sHorsBornes = nvsChargerParams(&pa, &ps);
  if (sHorsBornes) Serial.printf("[reglages] %u jeu(x) hors bornes en NVS : valeurs par defaut\n", sHorsBornes);
  sAutomate.configurer(pa);
  sSurv.configurer(ps);
  int32_t tMax = 0;
  sSurv.maxPoseLu(nvsLireTempMax(&tMax) ? tMax : surv::Surveillance::kAucun);
  sEssaiMin = nvsLireEssaiMin();
  sEssaiMinuteMs = millis();

  // Pilote : ecoute seulement (5.7, point 3).
  const uint32_t now = millis();
#if PILOTE_SIMULE
  sim::Reglages sr;
  if (!nvsLireSimu(&sr)) sr = sim::Reglages();
  sr.delaiMoteurMs = pa.delaiMoteurMs;
  sSimu.regler(sr);
  // La hotte simulee survit a un redemarrage qui n'est pas une mise sous
  // tension ; 'simu apres_demarrage' la change une fois, quelle que soit la cause.
  sim::EtatHotte h;
  if (cause != ESP_RST_POWERON && sRtcMot == kMotRtc && sim::depuisMot(sRtcEtat, &h)) sSimu.poserHotte(h, now);
  if (nvsPrendreSimuDem(&h)) {
    sSimu.poserHotte(h, now);
    Serial.println("[simu] etat de la hotte simulee pose au demarrage (simu apres_demarrage)");
  }
#endif
  sPilote.demarrer(now);
  sAutomate.configurerLigne(sPilote.mode(), sPilote.annexe());

  // Etat initial (5.7, point 5) : mise sous tension, eteinte (Deduit) ; autre
  // cause, dernier etat publie relu en NVS (Presume).
  hotte::Moteur derniere = hotte::Moteur::V2;
  const bool derniereLue = nvsLireDerniere(&derniere);
  hotte::Etat publie;
  const bool publieLu = cause != ESP_RST_POWERON && nvsLireEtatPub(&publie);
  sAutomate.demarrer(cause == ESP_RST_POWERON ? hotte::Demarrage::MiseSousTension : hotte::Demarrage::Autre,
                     publieLu ? &publie : nullptr, derniere, now);
  if (derniereLue) sNvsDerniere.ecrite((uint32_t)derniere);
  if (publieLu) sNvsEtatPub.ecrite(hotte::versNvs(publie));

  // Matter : identite, Thread, EP1, EP2 ; premier reflet force (parade du piege de la NVS).
  matterHotteBegin(sAutomate.etat(), sAutomate.derniereVitesse());
  traceChip(false);  // l'init de la pile a pu reposer son propre filtre
  netUdpBegin();     // cle H1 lue en NVS ; le socket s'ouvre au premier tour ou OpenThread est libre
  if (!alimBegin()) Serial.println("[alim] ADC continu indisponible : temperature seule");
  if (!matterHotteMisEnService()) {
    Serial.println("Noeud Matter pas encore mis en service : ajoute l'accessoire dans Maison.");
    matterHotteStatut(Serial);
  }
  cliBegin();
  // En dernier (4.3, garde-fou 5) : la tache loop au chien de garde des taches (5 s).
  enableLoopWDT();
}

void loop() {
  const uint32_t now = millis();
  sPilote.poll(now);
  evenementsPilote(now);
  matterHottePoll(sAutomate, now);
  cliPoll();
  netUdpPoll();
  actions(now);
  matterHotteDerniere(sAutomate.derniereVitesse());
  jsonPoll();
  alimPoll(sSurv, now);
  surv::Alerte al;
  while (sSurv.alerte(&al)) jsonAlerte(al);
  nvsDifferee(now);
#if PILOTE_SIMULE
  simuAuto(now);
  sRtcEtat = sim::versMot(sSimu.hotte());
  sRtcMot = kMotRtc;
#endif
  delay(1);  // la tache IDLE, et le chien de garde nourri par la boucle d'Arduino
}
```

- [ ] **Étape 21 : Lancer le build du produit : il passe**

Relire la sortie : aucun avertissement venant de `src/produit/`. Noter la taille de l'image : c'est la réponse à Q22.

```bash
~/.platformio/penv/bin/pio run -e produit
```

Attendu :

```
RAM:   [=====     ]  47.8% (used 156576 bytes from 327680 bytes)
Flash: [========  ]  79.0% (used 2486660 bytes from 3145728 bytes)
========================= [SUCCESS] Took 36.91 seconds =========================
produit        SUCCESS   00:00:36.906
```

- [ ] **Étape 22 : La sonde et le générateur compilent toujours**

```bash
~/.platformio/penv/bin/pio run -e sonde -e generateur
```

Attendu :

```
========================= [SUCCESS] Took 10.75 seconds =========================
========================= [SUCCESS] Took 6.62 seconds =========================
sonde          SUCCESS   00:00:10.754
generateur     SUCCESS   00:00:06.616
```

- [ ] **Étape 23 : Lancer tous les tests hôte et Python**

```bash
sh tools/tests/test_hote.sh && python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

Attendu :

```
tests hote : OK
Ran 175 tests in 9.596s
OK
```

- [ ] **Étape 24 : Committer**

```bash
git add platformio.ini tools/tests/test_hote.sh src/produit/config_produit.h src/produit/produit.h src/produit/reglages_produit.h src/produit/reglages_produit.cpp src/produit/alim.h src/produit/alim.cpp src/produit/matter_hotte.h src/produit/matter_hotte.cpp src/produit/net_udp_thread.h src/produit/net_udp_thread.cpp src/produit/json_mode_produit.h src/produit/json_mode_produit.cpp src/produit/cli_produit.h src/produit/cli_produit.cpp src/produit/main_produit.cpp
git commit -m "$(cat <<'FIN'
Ajouter le firmware du produit : environnement produit, Matter, compagnon, pilote simule

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 11 : Documents : procédure du banc Matter, README, taille de l'image

**Fichiers :**
- Créer : `docs/BANC-MATTER.md`
- Modifier : `README.md`
- Modifier : `docs/SPEC-PRODUIT.md`

**Interfaces :**
- Produit : `docs/BANC-MATTER.md` (préparation P1 à P8, commandes, essais B1 à B16 et leurs relevés, bilan).

- [ ] **Étape 1 : Écrire `docs/BANC-MATTER.md`**

Relevé B13 : reporter la taille donnée par le build de la tâche 10 si elle diffère.

```markdown
# Banc Matter : procédure et résultats

Banc du point 3 de la spec du produit ([SPEC-PRODUIT.md](SPEC-PRODUIT.md),
§9.2) : le firmware `produit`, **pilote de ligne simulé**, sur une C6 posée sur
l'USB du Mac, mis en service dans la Maison de Majid. **Aucune liaison avec la
hotte.** Il tranche les questions de la spec qui ne dépendent que de Maison et
de la pile : Q7 à Q14, Q22, Q36, Q42.

## 1. Règles

- **Carte :** la SuperMini libre réservée au module (Q23, tranchée le 29/09).
  **Jamais** la ScreenBar (`58:E6:C5:DD:7E:F0`) ni la sonde de maillage Thread
  de benq (`58:E6:C5:DD:A4:6C`) : on relève la MAC par `info` avant tout
  flash, et l'on s'arrête si c'est l'une d'elles.
- Le port série s'ouvre sans redémarrer le C6 : `pio device monitor` ou
  `tools/serie_enregistre.py` (DTR = RTS = 0 en un seul appel). Jamais un
  script pyserial qui pose DTR ou RTS avant l'ouverture.
- Ni mot de passe ni code d'appairage tapé par un agent : Majid met en service
  le nœud dans Maison lui-même.
- À la fin du banc, le nœud de test est retiré de Maison (`decommission`, puis
  effacement), sauf s'il sert encore à l'endurance (B11). **Avant la mise en
  service définitive du module, la carte est effacée** (spec §8.3).

## 2. Préparation

| # | Étape | Commande | Fait |
|---|---|---|---|
| P1 | versions relevées : iOS de l'iPhone, tvOS ou HomePod des routeurs de bordure, macOS | réglages des appareils | ☐ |
| P2 | MAC de la carte relevée par `info` (firmware quelconque), différente de `7E:F0` et `A4:6C` | `pio device monitor`, puis `info` | ☐ |
| P3 | effacement complet, puis premier flash | `~/.platformio/penv/bin/pio run -e produit -t erase` puis `-t upload` | ☐ |
| P4 | démarrage lu : version, pilote `simule`, codes d'appairage | `info`, `matter` | ☐ |
| P5 | clé H1 posée par l'USB, rangée dans `~/.config/hotte-produit/cle` | `python3 tools/hotte_udp.py cle <port>` (port libre) | ☐ |
| P6 | route Thread du Mac | assistant de benq, `tools/macos/halo-routes` (commit `c58a506`) | ☐ |
| P7 | mise en service dans Maison par Majid (code manuel ou QR de `matter`), avertissement « non certifié » accepté ; accessoires séparés, noms « Hotte » et « Éclairage hotte », pièce « Cuisine » | Maison | ☐ |
| P8 | nom SRP et adresse OMR relevés ; session UDP ouverte | `json etat` par l'USB (bloc `reseau` `ip`), puis `python3 tools/hotte_udp.py --appareil produit session <nom SRP ou IPv6> --duree 30` | ☐ |

## 3. Commandes utiles (USB)

| Commande | Effet |
|---|---|
| `hotte` | état de l'automate, ordres, compteurs, réglages |
| `simu` | hotte simulée : état, mode, pannes |
| `simu appui marche\|lumiere\|v1\|v2\|v3` | appui du **panneau** simulé |
| `simu mode repete\|changements\|appuis`, `simu annexe 0\|1` | ce que porte la ligne `D` simulée |
| `simu refuse 1` | la hotte simulée ignore les appuis du module (B3) |
| `simu apres_demarrage v2 lampe` | état de la hotte simulée au prochain démarrage (B10, B16) |
| `simu auto <s>` | endurance : un appui du panneau toutes les s secondes (B11) |
| `matter journal` | écritures brutes de Maison, avec leur heure (B1, B2) |
| `matter derniere 0\|1` | coupe ou remet la substitution de On (B2) |
| `matter med 0\|1`, `matter tx <8..20>` | rôle Thread (au démarrage suivant), puissance (B12) |
| `alim`, `alim arret`, `alim reprise` | tensions et température ; ADC arrêté ou repris (B15) |
| `hotte regle temp_alerte_c <c>` | seuil d'alerte de la puce (B15) |
| `json 1`, `json log 1` | mode machine, messages `log` |

## 4. Essais

Chaque essai a ses relevés ; un essai raté est refait après correction, et la
correction est notée.

### B1. Rendu du ventilateur

- **Méthode :** captures d'écran de la tuile et de la fiche du ventilateur,
  iPhone et Mac ; un geste lent puis rapide sur le curseur ; `matter journal`
  juste après.
- **Relevés :** curseur continu ou crans : ______ ; écart entre deux écritures
  d'un même geste : ______ ms (min) à ______ ms (max).
- **Décide :** `lissage_calme_ms` (plus long que l'écart d'un geste) et
  `lissage_plafond_ms` (Q8).

### B2. Bouton de la tuile

- **Méthode :** hotte simulée armée, puis en V1 ; bouton de la tuile 5 fois,
  avec `matter derniere 1`, puis 5 fois avec `matter derniere 0` ;
  `matter journal` après chaque série ; `json log 1` pour les séquences.
- **Relevés :** écritures (valeurs, ordre, écart entre `FanMode` et
  `PercentSetting`) : ______ ; statut vu par Maison et sa réaction (erreur,
  état affiché) avec et sans substitution : ______ ; vitesse obtenue : ______.
- **Décide :** « dernière vitesse » par la règle 3 ou 4 du §3.3 (Q7).

### B3. Maintien de l'état demandé

- **Méthode :** `simu refuse 1`, puis ordre V2 de Maison depuis l'arrêt ; la
  séquence échoue et l'état réel est republié (`[hotte] sequence ventilo :
  echec`). Chronomètre à partir de cette republication jusqu'au retour de la
  tuile sur l'état réel. Puis un cas d'abandon (`simu hs 1`, ordre, attente).
  `simu refuse 0`, `simu hs 0` à la fin.
- **Relevés :** durée du maintien : ______ s ; `PercentCurrent` affiché pendant
  et après : ______.
- **Décide :** la tolérance du conflit (Q9), présentée à Majid.

### B4. Piège de la NVS

- **Méthode :** hotte simulée en V3, lampe allumée (`simu appui marche`,
  `simu appui v3`, `simu appui lumiere`) ; USB débranché 10 s, rebranché : la
  hotte simulée revient éteinte (mise sous tension).
- **Relevés :** état affiché par Maison après le retour : ______ ; délai du
  retour : ______ s.
- **Critère :** « éteint, 0 % », lampe éteinte ; jamais « High à 0 % » ni
  « Extinction… » (Q10).

### B5. Siri en français

- **Méthode :** « allume la hotte », « éteins la hotte », « mets la hotte à
  66 % », « éteins les lumières de la cuisine », « éteins la hotte dans 15
  minutes ».
- **Relevés :** effet de chaque phrase : ______ ; « éteins les lumières de la
  cuisine » ne touche que la lampe : ☐ ; « éteins la hotte » ne touche que le
  ventilateur : ☐.

### B6. « Éteindre après » dans une automatisation

- **Méthode :** automatisation avec « Éteindre après », durées essayées dont
  la plus longue proposée ; puis une automatisation déclenchée par la hotte
  elle-même (« quand la hotte s'allume, l'éteindre après 15 min »).
- **Relevés :** durée maximale : ______ ; déclenchée par la hotte : réussite ☐,
  sinon contournement : ______ (Q11).

### B7. Tuile regroupée contre accessoires séparés

- **Méthode :** regrouper les deux accessoires, ordres depuis la tuile, puis
  les séparer de nouveau.
- **Relevés :** ce que fait un ordre de la tuile regroupée : ______.

### B8. Deux nœuds 0xFFF1/0x8000

- **Méthode :** la ScreenBar et le banc commandés tour à tour pendant la durée
  du banc.
- **Relevés :** confusion, « Pas de réponse », mauvais nœud : ______ (Q12).

### B9. Latences

- **Méthode :** vidéo de l'écran de Maison avec la console dans le même champ :
  `simu appui v2` jusqu'à l'affichage (10 fois) ; ordre de Maison jusqu'au
  `[hotte] sequence ventilo : ok` de la console (10 fois).
- **Relevés :** appui → Maison : ______ s (médiane), ______ s (max) ; Maison →
  nœud : ______ s.
- **Décide :** le budget du §3.7, hors carte réelle (Q8).

### B10. Redémarrage du nœud

- **Méthode :** hotte simulée en V2 ; `reboot` : la hotte simulée est gardée
  (mémoire RTC). Délai jusqu'au retour des gestes dans Maison. Puis
  `simu apres_demarrage v1` et `reboot` : l'état change pendant le
  redémarrage ; la resynchronisation suit, dans chaque mode simulé.
- **Relevés :** retour des gestes : ______ s (5 essais) ; resynchronisation :
  ______.
- **Décide :** `maxint`, la reprise des abonnements (le calendrier de benq
  seulement si le trou de benq revient), la borne de 75 s (Q36).

### B11. Endurance

- **Méthode :** au moins 3 jours (7 visés) : `simu auto 120`, et une
  automatisation de Maison qui allume et éteint le ventilateur toutes les
  heures ; `python3 tools/serie_enregistre.py <port> endurance --duree 259200`
  pour suivre `etat` `sys`.
- **Critère :** aucun redémarrage ; `heap_min` stable sur les 48 dernières
  heures. Après un plantage : core dump lu (`esptool read_flash 0x3f0000
  0x10000`, puis `esp-coredump`), correction, puis 3 jours de plus.
- **Relevés :** durée : ______ ; redémarrages : ______ ; `heap_min` au début et
  à la fin : ______.

### B12. Puissance d'émission et rôle

- **Méthode :** `json compteurs 10000` dans une session ; `matter tx 20`, puis
  relecture après l'attache et après un changement de rôle ; `matter med 1`,
  `reboot`, puis `matter med 0`, `reboot` : rôle réel, qualité de lien,
  compteurs MAC ; sensibilité rapportée.
- **Relevés :** puissance par défaut de la pile : ______ dBm ; tenue après
  l'attache : ☐ ; rôle réel en `med 0` : ______ ; sensibilité : ______ dBm
  (−120 attendus) ; lien : ______ (Q13, Q14).

### B13. Taille de l'image et tas

- **Relevés :** image : 2 486 622 octets sur 3 145 728 (79,0 %, build du
  29/09) ; `heap` et `heap_min` après la mise en service : ______ (Q22).

### B14. MultiSpeed (facultatif)

Seulement s'il coûte presque rien : noté pour une version suivante.

### B15. Alerte de température ; capteur et ADC

- **Méthode :** `hotte regle temp_alerte_c 25` (sous la température du
  moment) : événement `alerte` et `log` émis sans `json log 1` ; retour sous le
  seuil moins l'hystérésis : fin d'alerte ; seuil remis à 70. Puis `alim arret`,
  dix lectures de température ; `alim reprise`, dix lectures.
- **Relevés :** alerte vue ☐, fin vue ☐ ; températures avec et sans ADC :
  ______ (Q42).

### B16. Coupure du module seul, hotte en marche

- **Méthode :** dans chaque mode simulé (`simu mode ...`, `simu annexe 1` pour
  `changements` et `appuis`) : `simu apres_demarrage v2 lampe`, USB débranché
  10 s, rebranché (départ en « éteinte » déduite, la hotte simulée en V2) ;
  ordre V3 de Maison.
- **Critère :** jamais « marche » avant un arrêt du moteur lu (règle 3 du
  §5.4) ; resynchronisation : ______.

## 5. Bilan

À remplir à la fin du banc, puis présenté à Majid : réponses à Q7 à Q14, Q22,
Q36, Q42 ; réglages retenus (`lissage_calme_ms`, `lissage_plafond_ms`,
`maxint`) ; phrases Siri et mode d'emploi de l'arrêt différé ; corrections
faites pendant le banc.
```

- [ ] **Étape 2 : Présenter le module produit dans `README.md`**

Dans `README.md`, remplacer :

```markdown
## Outils Mac
```

par :

````markdown
## Le module produit

Sous-projet 2 ([docs/SPEC-PRODUIT.md](docs/SPEC-PRODUIT.md)) : un nœud Matter
sur Thread, EP1 Ventilateur (trois paliers) et EP2 Lumière, qui commande la
hotte à travers son panneau d'origine. Environnement PlatformIO `produit`,
sources dans `src/produit/`. **Pour l'instant, pilote de ligne simulé** :
aucune broche pilotée ; le pilote réel (A, B ou C) viendra après l'étape 6.

```
~/.platformio/penv/bin/pio run -e produit -t erase    # premier flash (Thread)
~/.platformio/penv/bin/pio run -e produit -t upload
```

Banc sans hotte : [docs/BANC-MATTER.md](docs/BANC-MATTER.md). Profil
compagnon : [docs/PROTOCOLE-JSON-PRODUIT.md](docs/PROTOCOLE-JSON-PRODUIT.md).

## Outils Mac
````

- [ ] **Étape 3 : Mettre à jour la ligne de `hotte_udp.py cle`**

Dans `README.md`, remplacer :

```markdown
| `tools/hotte_udp.py cle <port>` | clé H1 posée par l'USB, rangée dans `~/.config/hotte-sonde/cle` (jamais affichée) |
```

par :

```markdown
| `tools/hotte_udp.py [--appareil sonde\|produit] cle <port>` | clé H1 posée par l'USB, rangée dans `~/.config/hotte-<appareil>/cle` (jamais affichée) ; l'appareil se lit dans `hello` s'il n'est pas donné |
```

- [ ] **Étape 4 : Mettre à jour la ligne de `json_check.py`**

Dans `README.md`, remplacer :

```markdown
| `tools/json_check.py [--jsonl] <capture>` | conformité des lignes machine au profil hotte |
```

par :

```markdown
| `tools/json_check.py [--jsonl] [--profil sonde\|produit] <capture>` | conformité des lignes machine au profil hotte (sonde ou produit) |
```

- [ ] **Étape 5 : Ajouter les documents du produit**

Dans `README.md`, remplacer :

```markdown
| [docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md) | profil hotte du protocole compagnon, par différence avec la ScreenBar |
```

par :

```markdown
| [docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md) | profil hotte du protocole compagnon, par différence avec la ScreenBar |
| [docs/SPEC-PRODUIT.md](docs/SPEC-PRODUIT.md) | sous-projet 2 : le produit Matter (exposition, automate, matériel, vérification) |
| [docs/PROTOCOLE-JSON-PRODUIT.md](docs/PROTOCOLE-JSON-PRODUIT.md) | profil hotte du module produit, par différence |
| [docs/BANC-MATTER.md](docs/BANC-MATTER.md) | banc Matter sans hotte : procédure et résultats |
```

- [ ] **Étape 6 : Mettre à jour l'état des sous-projets**

Dans `README.md`, remplacer :

```markdown
2. **Produit** : interface définitive et firmware Matter (ventilateur 3 vitesses,
   lumière, arrêt différé).
```

par :

```markdown
2. **Produit** (en cours : automate, couche Matter et compagnon, pilote simulé) :
   interface définitive et firmware Matter (ventilateur 3 vitesses, lumière).
```

- [ ] **Étape 7 : Répondre à Q22 dans `docs/SPEC-PRODUIT.md`**

Reporter la taille exacte de la tâche 10 si elle diffère.

Dans `docs/SPEC-PRODUIT.md`, remplacer :

```markdown
| Q22 | Taille de l'image et tas libre du produit ? | premier build de `produit` (premier plan) | marge de la partition ; fonctions à alléger si elle manque |
```

par :

```markdown
| Q22 | Taille de l'image et tas libre du produit ? | premier build de `produit` (premier plan) : **image de 2 486 660 octets sur 3 145 728 (79,0 %)**, build du premier plan avec le pilote simulé ; tas libre : banc Matter (B13) | marge de la partition ; fonctions à alléger si elle manque |
```

- [ ] **Étape 8 : Lancer les tests Python (liens des documents)**

```bash
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

Attendu :

```
Ran 175 tests in 9.328s
OK
```

- [ ] **Étape 9 : Committer**

```bash
git add docs/BANC-MATTER.md README.md docs/SPEC-PRODUIT.md
git commit -m "$(cat <<'FIN'
Ecrire la procedure du banc Matter et presenter le module produit

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

