# Sonde de reconnaissance de la ligne D : plan d'implementation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** donner a Majid tout ce qu'il faut pour la reconnaissance : les fiches de terrain (securite, journal, cablage), une sonde ESP32-C6 qui capture la ligne `D` et parle le protocole compagnon v1 (USB puis UDP/Wi-Fi authentifie), un generateur de banc, les outils Mac d'enregistrement et d'analyse, et l'injection bornee de l'etape 7.

**Architecture:** firmware Arduino (coeur 3.3.12) + pilote RMT d'IDF pour la capture continue ; toute la logique testable est dans des modules purs (`capture_model`, `injection_regles`, `motifs`, `json_out`, `h1_proto`) compiles sur l'hote par `clang++`. La sonde ne decoupe pas en trames : elle publie des parties horodatees de durees consecutives ; le decoupage et le decodage se font sur le Mac (`tools/decodeurs.py`). Le protocole compagnon reprend le code de la ScreenBar au commit `c58a506`, scinde et adapte.

**Tech Stack:** PlatformIO 6.2 + pioarduino 55.03.312-1 (Arduino-ESP32 3.3.12, IDF 5.5), ESP32-C6 SuperMini ; C++17 ; Python 3.11 (bibliotheque standard + pyserial 3.5), `unittest` ; clang++ pour les tests hote.

**Spec:** `docs/SPEC-RECONNAISSANCE.md` (revision 2, a lire avec ce plan). Protocole de reference : `benq-screenbar-halo-matter` au commit `c58a506`, `docs/PROTOCOLE-JSON.md` (anglais) ou `docs/PROTOCOLE-JSON.fr.md` (francais). Depot local de la ScreenBar : `/Users/Majid/Documents/Dev/esp32/benq` (lecture seule pour ce plan).

**Écarts à la spec (assumés) :**
1. Le prérequis du §8.6 est levé : le transport réseau de la ScreenBar est committé et poussé (`c58a506`). La tâche 7 met la spec à jour : commit de référence, `rev` = 4 au lieu de 2, et les décisions de la relecture du plan (27/09) : règle 2 (différentiel testé juste avant de poser chaque montage), étape 3a (entre 3 et 5 V : on en rediscute avant tout Mac), étape 4 (polarité de +4,5 à +5,3 V ; couvercle et filtre avant l'étape 2b), niveau bas de `D` jugé fonctionnellement (le FX2 est numérique ; oscilloscope optionnel ; choix de Majid), boîtier de mesure détachable par un XH 4 broches, banc sans aucune liaison avec la hotte, condensateur de 1 nF aux achats.
2. Le mode `changements` (§8.2) ne compare que les réceptions qui tiennent en une partie (110 durées au plus) ; une réception plus longue est toujours émise.
3. Avant l'avenant de l'étape 6, la seule syntaxe d'injection est brute : `injecte durees <d1> <d2> ...` (durées en µs, alternées, la première au niveau BAS du bus). La syntaxe nommée viendra avec l'avenant. Le délai minimal s'applique à toute injection. Les paramètres se règlent par `injection regle <nom> <valeur>` (USB seulement, tâche 22) ; `total_max_us` est borné à 200 000 pour que `relu_us` ne soit jamais tronqué.
4. Pas de capture par interruption ni d'enregistrement local en flash (la spec les renvoie à « si nécessaire »).
5. Événement `injection` : le champ `relu` s'appelle `relu_us` ; résultat `erreur` en plus de `ok`, `collision`, `delai` (RMT indisponible, désarmement pendant l'attente…) ; après une collision, GPIO7 est ramenée à l'état bas (le 10 k bloque Q2).
6. `capture on|off` ne réémet pas `config` (l'état actif est dans `etat.capture.active`) ; `capture tout|changements` et `seuils` la réémettent. Code `refuse` possible (échec NVS ou RMT).
7. `seuils [filtre_us] [silence_us] [resol_hz]` : troisième argument facultatif, seul moyen de régler la résolution de 500 kHz.
8. Pas de `tools/tests/test_h1.sh` séparé : `test_h1` est compilé par `tools/tests/test_hote.sh`. Pas de drapeau `SONDE_UDP` : le transport UDP est toujours dans l'env `sonde`.
9. `hotte_udp.py enregistre` accepte des commandes (pour régler la capture pendant le critère 4 et les scénarios de l'étape 5). La confirmation de `injection on` ne se tape que dans un terminal, phrase exacte « Majid devant la hotte ».
10. Une valeur hors bornes lue en NVS est signalée par un `log` dédié, émis à l'ouverture de chaque session machine, même sans `json log 1`.
11. `maskCmd` masque aussi le mot de passe de `wifi <ssid> <mdp>` dans `reponse.cmd`. `verse_capture.py` refuse clé, SSID et mot de passe, sauf l'objet `wifi` sans SSID du bloc `reseau ip`.
12. Tampon d'émission USB de 8 Ko ; le texte humain de la capture n'est écrit que s'il reste la place d'une ligne (sinon perdu et compté).
13. Hors de ce plan (écrits après l'étape 6, avec l'avenant) : `docs/PROTOCOL.md`, l'avenant d'injection, le relais de la voie 2 (option B) et le prototype PhotoMOS (option C).

## Global Constraints

- **Plateforme** (copie de la ScreenBar) : `platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312-1/platform-espressif32.zip`, `framework = arduino`, `board = esp32-c6-devkitc-1` avec `board_upload.flash_size = 4MB`, `board_upload.maximum_size = 4194304`, `board_build.flash_size = 4MB`, `monitor_speed = 115200`, `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`, `-DCORE_DEBUG_LEVEL=1`.
- **Environnements** : `sonde` (defaut) et `generateur`. Pas de Matter, table de partitions par defaut.
- **Broches (C6 SuperMini)** : ecoute voie 1 = **GPIO6** ; injection voie 1 = **GPIO7** ; sortie du generateur = **GPIO7** du second C6. Interdites : 4, 5, 8, 9, 15 (demarrage), 12, 13 (USB), 16, 17 (UART0), 21, 22 (inaccessibles).
- **GPIO7 de la sonde est mise a l'etat bas en toute premiere instruction de `setup()`** (`pinMode(7, OUTPUT); digitalWrite(7, LOW);`), avant tout le reste.
- **Etage d'ecoute inverse** : GPIO6 bas = bus haut. Tout niveau publie est un niveau **du bus** (`inverse = true` par defaut dans les reglages).
- **Capture** : resolution 1 000 000 Hz (ou 500 000 Hz) ; filtre 0..3 µs (defaut 1) ; silence de fin de reception : de 1 000 µs a 32 767 ticks (32 767 µs a 1 MHz, 65 534 µs a 500 kHz), defaut 5 000 µs ; tampon circulaire de 2 048 symboles ; au plus **110 durees par ligne `trame`**.
- **Protocole** : `v` = 1, `rev` = 4 (`jsonp::kRev` de la ScreenBar, inchange) ; ligne machine = RS (0x1E) + JSON compact ASCII + LF ; 1 024 octets au plus, budget 896 ; port UDP **5480** ; enveloppe H1 identique ; NVS : espace de noms **`hotte`** ; nom mDNS **`hotte-sonde`** ; `appareil` = `hotte` ; `id.serie` = `HOTTE-` + MAC en 12 hexa majuscules.
- **Injection, valeurs par defaut avant l'avenant** : `bas_max_us` 3 000 ; `total_max_us` 200 000 ; `silence_min_us` 20 000 ; `attente_max_ms` 1 000 ; `delai_min_ms` 3 000 ; desarmement automatique apres 600 s ; au plus 64 durees par demande ; tolerance de collision 20 µs.
- **Profil distant** (UDP) : `etat` 2 000 ms, `compteurs` 5 000 ms, `reseau` 30 000 ms, `trames` actives sans coupure automatique, 100 lignes `trame` par seconde et par session (RateCap), file UDP de 32 datagrammes, 50 000 octets/s en moyenne, credit de 8 192 octets. USB : `etat` 1 000 ms, `compteurs` 1 000 ms, `reseau` 5 000 ms, 100 lignes `trame` par seconde.
- **Code** : identifiants, commentaires, textes de la console et du protocole en francais **sans accents** (comme la ScreenBar). Documentation (`docs/`, README) en francais **avec accents**.
- **Tests hote** : `sh tools/tests/test_hote.sh` (clang++ `-std=c++17 -Wall -Wextra -Werror -Isrc`, puis `json_check.py`) ; `python3 -m unittest discover -s tools/tests -v` depuis la racine du depot. Un test qui echoue casse la tache.
- **Build** : `~/.platformio/penv/bin/pio run -e sonde` et `-e generateur` doivent passer sans avertissement nouveau dans `src/`.
- **Commits** : un par tache, message en francais sans accents, a l'imperatif comme la ScreenBar (« Ajouter ... », « Ecrire ... »), termine par une ligne vide puis `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Jamais de `git push` sans demande de Majid.
- **Interdits pour les agents** : flasher (`-t upload`), ouvrir un port serie, `pio device monitor`, `sudo`, toucher au depot `benq` (lecture seule). Les taches marquees **[BANC avec Majid]** sont executees par l'agent principal avec Majid, jamais par un sous-agent.
- **Provenance** : chaque fichier copie de la ScreenBar commence par `// Copie de benq-screenbar-halo-matter@c58a506 : <chemin>` (ou `# ...` en Python), suivi de ` (adapte : <resume>)` s'il est modifie.

## Carte des fichiers

| Fichier | Responsabilite | Tache |
|---|---|---|
| `docs/SECURITE.md` | fiche de securite (regles 1 a 13 du §5, tests d'isolation et de fuite, conduite en cas d'anomalie) | 1 |
| `docs/RECONNAISSANCE.md` | journal des etapes −1 a 7 (tableaux a remplir, decisions) | 1 |
| `docs/WIRING.md` | adaptateur, cable de sortie, boitier de mesure, etages, brochage, nomenclature | 1 |
| `tools/signaux.py` | encodeurs synthetiques : UART, distance d'impulsion (WTC), Manchester, Krona ; simulation des parties de la sonde ; motifs du banc | 2 |
| `tools/capture_fmt.py` | lecture des captures (`.jsonl`, serie brute) ; parties -> receptions -> flux de segments | 3 |
| `tools/decodeurs.py` | histogramme, regroupements, decoupage en trames, decodeurs UART / distance d'impulsion / Manchester, classement `auto` | 4, 5 |
| `tools/analyse.py` | CLI `histo`, `trames`, `auto`, `uart`, `diff` | 6 |
| `platformio.ini`, `src/fw_version.h`, `src/app_desc.c`, `tools/git_rev.py` | projet, version (copies de la ScreenBar) | 7 |
| `src/config.h` | constantes : broches, defauts, bornes, cles NVS | 7 (etendu ensuite) |
| `src/main.cpp` | `setup()` / `loop()` de la sonde | 7 (etendu ensuite) |
| `src/reglages.{h,cpp}` | reglages de capture et d'injection en NVS, bornes appliquees au chargement | 7 (etendu en 22) |
| `tools/tests/test_hote.sh` | tous les tests hote C++ puis `json_check.py` | 7 (etendu ensuite) |
| `src/json_out.{h,cpp}` | briques pures du protocole (scindees de la ScreenBar) + messages de la sonde | 8 |
| `tools/tests/test_json.cpp` | tests hote de `json_out` | 8 |
| `src/capture_model.{h,cpp}` | pur : symboles RMT -> parties, decoupe a 110, mode `changements`, bornes | 7 (bornes), complete en 9 |
| `tools/tests/test_capture.cpp`, `tools/tests/verif.h` | tests hote de `capture_model`, mini-cadre de verification | 7, complete en 9 |
| `src/capture_rmt.{h,cpp}` | pilote RMT d'IDF en reception continue, tampon circulaire rempli par le rappel | 10 |
| `src/bord.{h,cpp}` | interruption GPIO sur les deux fronts de GPIO6 (dernier front, niveau ; surveillance pendant une injection en 23) | 10 (etendu en 23) |
| `src/cli.{h,cpp}` | console humaine (USB), puis lignes `id=`, puis commandes a distance | 10 (etendu en 11, 19, 23) |
| `src/json_mode.{h,cpp}` | sessions, bail, file des periodiques, instantanes, evenements | 11 (etendu en 19, 23) |
| `tools/json_check.py` | conformite des lignes machine au profil hotte ; option `--jsonl` | 11 (etendu en 19, 23) |
| `docs/PROTOCOLE-JSON.md` | profil hotte du protocole compagnon, par difference avec la ScreenBar | 11 (etendu en 19, 23) |
| `tools/serie_enregistre.py` | enregistrement d'une session machine par l'USB en `.jsonl` | 12 |
| `src/motifs.{h,cpp}` | pur : motifs du banc (segments de ligne, contenus connus) | 13 |
| `tools/tests/dump_motifs.cpp`, `tools/tests/test_motifs.py` | croisement `motifs.cpp` / `signaux.py` | 13 |
| `src/gen_main.cpp` | firmware du generateur (env `generateur`) | 14 |
| `tools/banc.py` | verdicts du banc : decodage au bit pres, ecarts de durees | 15 |
| `docs/BANC.md` | procedure et resultats du banc | 15, 16, 16b, 20b, 21, 23b, 24 |
| `src/net_wifi.{h,cpp}` | Wi-Fi STA, identifiants en NVS, mDNS, RSSI | 17 |
| `src/h1_proto.{h,cpp}`, `src/h1_crypto.cpp`, `tools/tests/test_h1.cpp` | enveloppe H1 (copies) | 18 |
| `src/net_udp_wifi.{h,cpp}` | socket UDP 5480 (lwIP), sessions H1, cle NVS, files | 19 |
| `tools/hotte_udp.py` | client H1 : `cle`, `session`, `enregistre` | 20 |
| `src/injection_regles.{h,cpp}`, `tools/tests/test_injection.cpp` | pur : garde-fous et analyse de `injecte durees` | 22 |
| `src/injection.{h,cpp}` | RMT en emission sur GPIO7, attente de silence, arret sur collision | 23 |
| `src/sonde.h` | reglages en vigueur de la sonde (definis dans main.cpp) | 10 (etendu en 11) |
| `tools/tests/test_json_check.py` | tests de `json_check.py` | 11 |
| `tools/verse_capture.py`, `tools/tests/test_verse_capture.py`, `captures/README.md` | verser une capture de reference sans cle ni identifiant Wi-Fi | 12b |
| `src/gen_symboles.h`, `tools/tests/test_generateur.cpp` | pur : segments de ligne -> symboles RMT du generateur ; commande `impulsions` | 14 (etendu en 23b) |
| `tools/tests/test_wifi.cpp` | validation des identifiants Wi-Fi | 17 |
| `tools/tests/test_liens.py` | liens relatifs et ancres de README.md et docs/*.md | 25 |
| `tools/tests/test_*.py` | un fichier de tests par outil Python | 2 a 6, 12, 12b, 15, 20 |

## Interfaces communes (a respecter a la lettre)

### C++ : `src/config.h` (tache 7)

```cpp
#pragma once
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

### C++ : `src/capture_model.h` (tache 9)

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

### C++ : ajouts a `src/json_out.h` (tache 8)

`json_out.{h,cpp}` garde de la ScreenBar : `kVersion`, `kRev` (4), `kLineMax`, `kBudget`, `kCmdMax`, `kCmdTextMax`, `kMsgMax`, `kLogTextMax`, `kStrMax`, `kRS`, `kCtrlU`, `kIdMax`, `kOrigins` (3), `kUsb` (0), `Writer`, `heartbeat`, `sessionEnd`, `logLine`, `Reply` (sans `hasTarget`, `target`, `dirty`, `version`), `reply`, `ReplyCache`, `parseIdPrefix`, `copyCmd`, `maskCmd`, `LineAssembler`, `RateCap`, `Cadence`, `Queue`, `Queued`, `kLateMs`, `leaseExpired`, `remoteRefusal`. Il retire tout ce qui depend de `halo1_*` (`lampsCode`, `kindCode`, `slotCode`, `verdictCode`, `relaunchCode`, `symptomCode`, `state`, `fields`, `rx`, `tx`, `relaunch`, `module`, `led`, `Issue`, `Delivery`, `delivery`, `LampSample`, `DeliveryWatch`, `kIdsMax`). `Reply::Suite` devient `enum Suite : uint8_t { SuiteNone, SuiteInjection };` (valeur JSON `injection`). Ajouts :

```cpp
// Writer : entier 64 bits (t_us), decimal sans zero de tete.
void u64(const char *k, uint64_t v);

enum class Item : uint8_t {
  HelloBase, HelloId, Config, EtatBus, EtatCapture, EtatInjection, EtatSys, Compteurs, NetIp, Heartbeat, Reply
};

// Evenement trame (profil hotte). rep : present si hasRep ; sautes : present si > 0.
void trame(Writer &w, uint32_t n, uint32_t ms, const capt::Partie &p, bool hasRep, uint32_t rep, uint32_t sautes);

struct InjectionEv {
  uint32_t id = 0;              // id de la commande (0 : sans id, champ null)
  const char *cmd = "";         // tronquee a kCmdTextMax
  const char *resultat = "ok";  // ok, collision, delai, erreur
  bool niv0Haut = false;        // niveau du bus pendant dur[0] (toujours bas pour 'injecte durees')
  const uint32_t *dur = nullptr;
  uint16_t n = 0;
  uint32_t attenteUs = 0;
  const uint32_t *relu = nullptr;
  uint16_t nRelu = 0;
};
void injection(Writer &w, uint32_t n, uint32_t ms, const InjectionEv &e);
```

Forme exacte des evenements (ordre des champs) :
- `trame` : `v`, `t`, `n`, `ms`, `num`, `part`, `fin`, `t_us`, `niv0` (`"haut"`/`"bas"`), `dur_us` (tableau), `debord`, puis `rep` si present, puis `sautes` si > 0.
- `injection` : `v`, `t`, `n`, `ms`, `id` (entier, ou null), `cmd`, `resultat`, `niv0`, `dur_us`, `attente_us`, `relu_us`.

`remoteRefusal` (liste blanche hotte) : autorise `json 1 [bail 10..120]`, `json 0`, `json etat`, `json hello`, `json ping`, `json periode 2000..60000`, `json compteurs 0|1000..60000`, `json reseau 0|10000..60000`, `json trames 0|1`, `json log 0|1`, `capture on|off|tout|changements`, `seuils ...`, `injection on|off`, `injecte ...` ; tout le reste rend un msg (reponse `interdite`).

### C++ : `src/capture_rmt.h` et `src/bord.h` (tache 10)

```cpp
// capture_rmt.h
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

```cpp
// bord.h
#pragma once
#include <stdint.h>

bool bordBegin(uint8_t pin);   // service ISR d'IDF (ESP_ERR_INVALID_STATE tolere), deux fronts
uint64_t bordDernierUs();      // esp_timer du dernier front vu
bool bordBusHaut();            // niveau du bus maintenant (inversion de l'etage comprise)
uint32_t bordFronts();         // fronts vus depuis le demarrage
```

### C++ : `src/cli.h` (taches 10, 11, 19)

```cpp
#pragma once
#include <stdint.h>
void cliBegin();                                  // tache 10
void cliPoll();                                   // tache 10 : lit l'USB, execute les lignes
void cliRunRemote(uint8_t origin, char *line, bool tooLong = false);  // tache 19 : ligne recue par le reseau (avec id)
```

### C++ : `src/json_mode.h` (tache 11, etendu en 19 et 23)

```cpp
#pragma once
#include <Arduino.h>
#include "capture_model.h"
#include "json_out.h"

void jsonBegin();             // debut de setup() : identifiant de demarrage
void jsonPoll();              // chaque tour de loop() apres cliPoll()
uint32_t jsonBootId();
bool jsonMachine();           // mode machine sur l'USB
void jsonNoteRx();            // octet recu de l'hote USB (bail)

struct JsonCmd {
  bool hasId;
  uint32_t id;
  const char *cmd;
  uint32_t t0;
};
void jsonCommand(char *arg, const JsonCmd &c);   // famille 'json ...'
bool jsonCadenceOk(uint32_t now);
void jsonRefuse(const JsonCmd &c, const char *code, const char *msg);
void jsonReply(const jsonp::Reply &r);
void jsonReplyEnd(const jsonp::Reply &r);
void jsonAfterCommand();      // bail ; config reemis si un reglage a change (jsonConfigChanged)
void jsonConfigChanged();

// Evenements de la sonde (vers chaque session en mode machine, trames actives)
void jsonTrame(const capt::Partie &p, bool hasRep, uint32_t rep);
void jsonInjection(const jsonp::InjectionEv &e);           // tache 23
bool jsonLog(const char *src, const char *niv, const char *txt);

// Transport reseau (tache 19)
void jsonSetOrigin(uint8_t origin);
uint8_t jsonOrigin();
void jsonRemoteReset(uint8_t origin);
void jsonNoteRemoteRx(uint8_t origin);
bool jsonRemoteAdmit(uint32_t id, const char *shown);
void jsonCountRejected();
```

Instantanes : `json_mode.cpp` les construit en appelant les fonctions des modules :
- `etat` bloc `bus` : `repos` (`"haut"`/`"bas"`, de `bordBusHaut()` hors reception), `fronts` (`bordFronts()`), `derniere_ms` (age du dernier front en ms, ou null), `receptions_s` (receptions par seconde, sur la derniere seconde).
- `etat` bloc `capture` : `active`, `mode` (`"tout"`/`"changements"`), `debord`, `rep_en_cours`.
- `etat` bloc `injection` (tache 23) : `montee`, `armee`, `arme_reste_s`, `derniere` (objet `{id, resultat}` ou null).
- `etat` bloc `sys` : l'objet `sys` de la ScreenBar a l'identique (champs `heap`, `heap_min`, `heap_bloc`, `pile_boucle`, `boucle_max_ms`, `json_perdus`, `json_trop_longs`, `rejets` ; verifier la liste exacte dans `benq/src/json_mode.cpp` a `c58a506`).
- `compteurs` bloc `sonde` : `receptions`, `parties`, `blocs`, `symboles`, `debord`, `rep`, `lignes_perdues`, `sautes`, `rejets`.
- `reseau` bloc `ip` (tache 19 ; avant, absent) : schema de la ScreenBar (`udp.port`, `udp.ouvert`, `udp.empreinte`, `udp.sessions`, `udp.rx`, `udp.rejets`, `udp.tx`, `udp.tx_perdus`), `srp` = null, plus `mdns.nom`, `wifi.rssi_dbm`, `wifi.ip`.
- `config` : `capture` (`gpio`, `resol_hz`, `filtre_us`, `silence_us`, `mode`, `inverse`) et, a partir de la tache 23, `injection` (`gpio`, `bas_max_us`, `total_max_us`, `silence_min_us`, `attente_max_ms`, `delai_min_ms`, `arme_max_s`, `tol_us`).
- `hello` : blocs `base` et `identite`, tous les champs de la ScreenBar (§8.5 de la spec), `build` = `sonde`, `reseau_build` = `aucun`, `appareil` = `hotte`, `caps` = `["sonde","injection","trames","log"]` puis, a partir de la tache 19, `+ ["udp","cle","mdns"]`.

### C++ : `src/reglages.h` (tache 7, etendu en 22)

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
Cles NVS : `resol_hz` (u32), `filtre_us` (u8), `silence_us` (u32), `inverse` (bool), `chgt` (bool). Tache 22 ajoute `inj_monte` (bool) et `injection` (struct `inj::Params` sous la cle `inj_params`, avec controle de bornes au chargement). Tache 17 range `wifi_ssid` et `wifi_mdp` (chaines) dans le meme espace.

### C++ : `src/net_wifi.h` (tache 17), `src/net_udp_wifi.h` (tache 19)

```cpp
// net_wifi.h
#pragma once
#include <stdint.h>
void netWifiBegin();                              // NVS -> STA, WiFi.setSleep(false), mDNS kNomMdns
void netWifiPoll();                               // reconnexion toutes les 10 s si perdu
bool netWifiUp();
int8_t netWifiRssi();                             // 0 si deconnecte
void netWifiIp(char out[16]);                     // "0.0.0.0" si deconnecte
bool netWifiSet(const char *ssid, const char *mdp);  // NVS puis reconnexion
```

```cpp
// net_udp_wifi.h : meme API que benq/src/net_udp.h (sans le #if MATTER_NET_THREAD)
void netUdpBegin();
void netUdpPoll();
bool netUdpSend(uint8_t slot, const uint8_t *json, size_t len);
uint8_t netUdpFreeSlots();
void netUdpEnd(uint8_t slot);
void netUdpResume(uint8_t slot);
void netUdpJson(jsonp::Writer &w, uint32_t now);
enum class NetKeyResult : uint8_t { Ok, Crypto, Nvs, Load };
NetKeyResult netUdpKeyNew(const uint8_t appRandom[32], char keyHex[65], char kid[9]);
bool netUdpKeyErase();
bool netUdpKid(char kid[9]);
```

### C++ : `src/injection_regles.h` (tache 22), `src/injection.h` (tache 23), `src/motifs.h` (tache 13)

```cpp
// injection_regles.h
#pragma once
#include <stdint.h>
namespace inj {
constexpr uint16_t kDurMax = 64;
struct Params {
  uint32_t basMaxUs = 3000, totalMaxUs = 200000, silenceMinUs = 20000;
  uint32_t attenteMaxMs = 1000, delaiMinMs = 3000, armeMaxS = 600, tolUs = 20;
};
bool paramsValides(const Params &p);
enum class Refus : uint8_t { Aucun, NonMontee, NonArmee, Delai, Syntaxe, BasTropLong, TropLong, TropDeDurees };
const char *refusTexte(Refus r);   // "non montee", "non armee", "delai minimal", "syntaxe", ...
struct Demande {
  uint16_t n = 0;
  uint32_t dur[kDurMax] = {};      // alternees, dur[0] au niveau BAS du bus
};
// args : ce qui suit 'injecte' (ex. "durees 750 750 750 2250"). Bornes : chaque duree basse
// (rang pair) <= basMaxUs, somme <= totalMaxUs, 1..kDurMax durees, entiers > 0.
Refus analyser(const char *args, const Params &p, Demande *out);
class Etat {
 public:
  void setMontee(bool m) { montee_ = m; }
  bool montee() const { return montee_; }
  void armer(uint32_t nowMs);
  void desarmer() { arme_ = false; }
  bool armee(uint32_t nowMs, const Params &p) const;        // faux apres armeMaxS
  uint32_t resteS(uint32_t nowMs, const Params &p) const;   // 0 si non armee
  Refus admettre(uint32_t nowMs, const Params &p) const;    // montee, armee, delai depuis la derniere emission
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
}  // namespace inj
```

```cpp
// injection.h
#pragma once
#include "injection_regles.h"
#include "json_out.h"
bool injectionBegin(const inj::Params &p, bool montee);
inj::Refus injectionDemander(const inj::Demande &d, uint32_t id, const char *cmd, uint32_t nowMs);
void injectionPoll();                      // attente de silence, emission, surveillance, evenement
void injectionJson(jsonp::Writer &w, uint32_t nowMs);   // bloc etat.injection
inj::Etat &injectionEtat();
const inj::Params &injectionParams();
```

```cpp
// motifs.h
#pragma once
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
}
```

Contenus des motifs (identiques en C++ et en Python, verifies par la tache 13) :
- `uartNNN` / `uartNNNinv` : 8N1 LSB d'abord, repos haut (`inv` : repos bas, niveaux inverses) ; octets de la trame `index` = `[0xA5, 0x5A, 0x00, 0xFF, index & 0xFF, (0xA5+0x5A+0x00+0xFF+(index&0xFF)) & 0xFF]` ; pause 20 000 µs.
- `wtc` : T = 750 µs, repos haut ; trame = depart 2T bas, puis 16 bits MSB d'abord (`0` = 1T haut + 1T bas, `1` = 1T haut + 3T bas), puis 1T haut ; valeur = `0x0100 << (index % 8)` puis masque 0xFFFF ; reponse simulee de la carte 4T apres la fin : depart 2T bas + 8 bits `index & 0xFF` meme codage + 1T haut ; pause 6 000 µs apres la reponse.
- `krona` : octets `[0x55, index % 8, (~(index % 8)) & 0xFF, 0x00, somme & 0xFF]`, UART 500 bauds 8N1 inverse (repos bas), 4 000 µs de pause entre octets, pause 18 000 µs apres la trame.
- `rafale` : 100 000 segments alternes de 100 µs commencant haut ; une seule « trame » (index ignore) ; pause 1 000 000 µs.

### Python : formats et modules

**Format `.jsonl`** (ecrit par `serie_enregistre.py` et `hotte_udp.py enregistre`, lu par `capture_fmt.py`) : une ligne par message machine accepte,
`{"rx_ms": <int, heure du Mac en ms depuis l'epoque>, "de": "<ip>" | "usb", "l": <objet JSON recu tel quel>}`.

**`tools/signaux.py`** (tache 2) :
```python
Segment = tuple[bool, int]   # (niveau haut ?, duree en µs)
def uart(octets: bytes, bauds: int, repos_haut: bool = True, pause_octet_us: int = 0) -> list[Segment]
def distance_impulsion(bits: str, t_us: int = 750) -> list[Segment]   # depart 2T bas, 0=1T haut+1T bas, 1=1T haut+3T bas, fin 1T haut
def manchester(bits: str, t_us: int) -> list[Segment]                 # IEEE 802.3 : 0 = haut->bas, 1 = bas->haut
def fusionner(segs: list[Segment]) -> list[Segment]                   # colle les segments consecutifs de meme niveau
MOTIFS: tuple[str, ...] = ("uart500","uart500inv","uart2400","uart2400inv","uart9600","uart9600inv","wtc","krona","rafale")
def octets_motif(nom: str, index: int) -> bytes
def trame_motif(nom: str, index: int) -> list[Segment]
def pause_motif_us(nom: str) -> int
def repos_haut_motif(nom: str) -> bool
def simuler_sonde(segments: list[Segment], t0_us: int, silence_us: int, repos_haut: bool, dur_max: int = 110, num0: int = 1) -> list[dict]
    # objets 'trame' tels que la sonde les publierait (v,t,n,ms,num,part,fin,t_us,niv0,dur_us,debord) pour un flux
    # de segments precede et suivi du repos ; une reception se termine au premier palier de repos >= silence_us.
```

**`tools/capture_fmt.py`** (tache 3) :
```python
@dataclass(frozen=True)
class Partie: num: int; part: int; fin: bool; t_us: int; niv0_haut: bool; dur_us: tuple[int, ...]; debord: bool; rep: int | None
@dataclass(frozen=True)
class Reception: num: int; t_us: int; niv0_haut: bool; dur_us: tuple[int, ...]; debord: bool; complete: bool
def lire_jsonl(chemin) -> Iterator[dict]            # rend les objets 'l'
def lire_serie_brute(chemin) -> Iterator[dict]      # decoupe RS ... LF, ignore le texte
def lire(chemin) -> Iterator[dict]                  # choisit selon l'extension (.jsonl sinon brute)
def parties(objets) -> list[Partie]                 # objets t == "trame"
def receptions(parts) -> list[Reception]            # recolle par num, dans l'ordre de part
def flux(recs, repos_haut: bool) -> list[tuple[bool, int]]   # segments continus, silences entre receptions calcules par t_us
```

**`tools/decodeurs.py`** (taches 4, 5) :
```python
def histogramme(durees: Iterable[int], pas_us: int = 10) -> list[tuple[int, int]]      # (debut de classe, effectif), trie
def regroupements(durees: Iterable[int], tol: float = 0.15) -> list[tuple[int, int]]   # (centre, effectif), trie par centre
def decouper(segments, silence_us: int, repos_haut: bool) -> list[list[tuple[bool, int]]]
@dataclass
class Decodage: nom: str; params: dict; octets: bytes; bits: str; erreurs: int; symboles: int
    # taux d'erreur = erreurs / max(1, symboles)
def uart(trame, bauds: int, inverse: bool = False) -> Decodage
def distance_impulsion(trame, t_us: int | None = None) -> Decodage
def manchester(trame, t_us: int | None = None) -> Decodage
def auto(trames, bauds=(300, 500, 600, 1200, 2400, 4800, 9600, 19200)) -> list[Decodage]   # classe par taux d'erreur croissant
```

**`tools/analyse.py`** (tache 6) : `histo`, `trames --silence µs`, `auto`, `uart <bauds> [--inverse]`, `diff <a> <b>` ; chaque sous-commande lit une capture par `capture_fmt.lire`.

**`tools/banc.py`** (tache 15) : `python3 tools/banc.py <capture> <motif> [--decalage-us N]` -> taux de trames decodees au bit pres, ecarts de durees (moyenne, max) par rapport a `signaux.trame_motif`.

**`tools/serie_enregistre.py`** (tache 12) et **`tools/hotte_udp.py`** (tache 20) ecrivent `logs/AAAA-MM-JJ-hhmm-<scenario>.jsonl` et un resume dans `logs/live.log`.

## Évolutions des interfaces pendant la rédaction

Les rédacteurs ont exécuté chaque tâche ; ces ajouts, validés par la compilation et les tests, complètent les interfaces ci-dessus (rien n'y est renommé) :
- `sonde.h` : `uint32_t sondeRepEnCours();`, `uint32_t sondeRepTotal();` (tâche 11) ; `sondeAppliquer` ne relance la capture que si elle tourne et que ses réglages changent.
- `cli.cpp` : `static void runLine(char *line, bool tooLong)` (modèle de la ScreenBar) ; `seuils` accepte `resol_hz`.
- `etat.bus.repos` peut valoir `null` (pas encore de mesure hors réception).
- `net_wifi.h` : `const char *netWifiSsid();`, `uint32_t netWifiPertes();`, `inline bool netWifiValides(const char *ssid, const char *mdp);` (tâche 17).
- `bord.h` : `bool bordActif();`, surveillance pendant l'injection (`bordSurveiller`, `bordFinSurveillance`, `bordCollision`, `uint16_t bordRelu(...)`) (tâche 23).
- `injection.h` : `injectionArmer(nowMs)`, `injectionDesarmer()` ; `injectionBegin` peut être rappelé.
- `injection_regles.h` : `kNbParams`, `nomParam`, `bornesParam`, `Reglage`, `reglerParam`, `chargerParams`, `totalUs`, `frontProgramme`, `Sym`, `kSymMax`, `versSymboles`, `kDepartMaxUs`, classe `Surveillance` (qui juge aussi anormal un front masqué).
- `json_out.h` : `jsonp::kInjDurMax` (64).
- `motifs.h` : `kSegMax`, `kRafaleSegs`, `kRafaleUs` ; `gen_symboles.h` : `gen::impulsionValide`, `kImpulsionMinUs`, `kImpulsionMaxUs`, `kPeriodeMaxMs`.
- Python : `signaux.T_WTC_US`, `capture_fmt.repos_majoritaire`, `decodeurs.decouper_dates`, `decodeurs.appliquer`, `decodeurs.BAUDS`, `Decodage.taux`, `analyse.main(argv) -> int`.

## Points connus (non bloquants, à surveiller au banc)

- Le texte humain de la console (réponses aux commandes, annonces Wi-Fi et injection) peut bloquer jusqu'à 2 s si un hôte USB est branché mais ne lit plus. Pendant les captures, la sonde est sur batterie : sans hôte, le texte est jeté.
- Horodatage des blocs partiels : pour une réception de plus de 240 symboles, `t_us` d'un bloc non final peut être en retard (documenté dans `capture_rmt.cpp`).
- L'interruption RMT n'est pas « IRAM-safe » dans le cœur précompilé : pendant une écriture NVS (`seuils`, `capture tout|changements`), des symboles peuvent être perdus sans marque `debord`.
- Flash de la sonde à environ 82 % de la partition par défaut ; si elle manque plus tard : `huge_app.csv` (pas d'OTA dans ce sous-projet).
- Pendant une émission, la tâche `loop` attend activement (200 ms au plus avec les valeurs par défaut).
- `tol_us` (20 µs par défaut) est à régler d'après la tâche 16b ; les options du décodeur « Jitter » de PulseView sont décrites sans avoir été essayées (repli : curseurs).
- Le générateur et la capture n'ont jamais tourné sur une carte : les tâches 16, 16b, 21 et 24 sont là pour le vérifier.

## Organisation du plan

Ce fichier porte l'en-tête, les contraintes, la carte des fichiers et les interfaces. Les tâches sont dans le dossier `2026-09-27-reconnaissance-sonde/`, à exécuter dans l'ordre des numéros :

| Fichier | Tâches |
|---|---|
| [1-terrain.md](2026-09-27-reconnaissance-sonde/1-terrain.md) | 1 : fiches de terrain (sécurité, journal, câblage) — Majid peut commencer les étapes −1 à 3 |
| [2-outils-python.md](2026-09-27-reconnaissance-sonde/2-outils-python.md) | 2 à 6 : signaux synthétiques, lecture des captures, décodeurs, `analyse.py` |
| [3-firmware-base.md](2026-09-27-reconnaissance-sonde/3-firmware-base.md) | 7 à 10 : projet, `json_out`, `capture_model`, capture RMT et console |
| [4-machine-usb.md](2026-09-27-reconnaissance-sonde/4-machine-usb.md) | 11, 12, 12b : mode machine sur l'USB, `serie_enregistre.py`, `verse_capture.py` |
| [5-banc.md](2026-09-27-reconnaissance-sonde/5-banc.md) | 13 à 16b : motifs, générateur, `banc.py`, banc par l'USB, critère 2 à l'analyseur |
| [6-reseau.md](2026-09-27-reconnaissance-sonde/6-reseau.md) | 17 à 21 : Wi-Fi, H1, UDP, `hotte_udp.py`, banc par le Wi-Fi |
| [7-injection.md](2026-09-27-reconnaissance-sonde/7-injection.md) | 22 à 25 : garde-fous, injection, banc du critère 5, clôture |

Les tâches marquées **[BANC avec Majid]** (16, 16b, 21, 24) sont exécutées par l'agent principal avec Majid : flashage, carte, instruments. Toutes les autres se font sans carte.

