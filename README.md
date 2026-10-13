**English** · [Français](#hotte-haier-cchp21ebb--matter-sur-thread)

# Haier CCHP21EBB cooker hood → Matter over Thread

Control a **Haier CCHP21EBB** cooker hood (90 cm, 3 speeds, LED light) from
Apple Home or any other Matter controller, with an **ESP32-C6** running Matter
over Thread. The hood keeps its original touch panel, whose indicators stay in
sync with the app.

> **Status: sub-project 1, reconnaissance.** The field sheets, the probe
> firmware, the bench generator firmware and the Mac tools are written and
> tested on the host; the validation bench records its results in
> [docs/BANC.md](docs/BANC.md). Still to do: the steps on the hood (−1 to 7,
> [log](docs/RECONNAISSANCE.md)), then the decoded protocol
> (`docs/PROTOCOL.md`, at step 6) and the choice of architecture.
>
> The documentation is written in French; this README is bilingual.

## Intended design

The touch panel talks to the power board (`XB_DYB_V4`) over **a single data
wire**, on connector `CN3 "- D +"`. The ESP32 taps this wire through a
removable adapter, **never touching mains voltage**. It "presses" keys by
sending the same frames as the panel. The original board stays in charge:
relays, beeps and indicators remain consistent.

Before writing the product, this protocol has to be understood. That is the
purpose of the reconnaissance.

## ⚠️ Safety

The power board is **powered from 230 V mains**. Only the corner with the
low-voltage connectors (`CN1`, `CN2`, `CN3`) is involved in this project, and
it is touched **only with the hood unplugged**. The rules are in
[docs/SPEC-RECONNAISSANCE.md §5](docs/SPEC-RECONNAISSANCE.md#5-règles-de-sécurité),
and as a sheet to keep at hand in [docs/SECURITE.md](docs/SECURITE.md).
If you reproduce this project, you do so at your own risk.

## The probe

An ESP32-C6 SuperMini on the measurement box, outside the hood
([docs/WIRING.md](docs/WIRING.md)):

- **listens** to line `D` on GPIO6, through a transistor stage (levels restored
  to the bus polarity), with the IDF RMT driver in continuous receive:
  timestamped chunks of consecutive durations, split into frames on the Mac;
- **companion protocol v1** of the ScreenBar, "hood" profile
  ([docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md)): over USB, or over UDP
  on Wi-Fi (port 5480, authenticated H1 envelope, `hotte-sonde.local`);
- **bounded injection** for step 7 on GPIO7 (open-drain stage), with the
  safeguards of §8.4 of the spec: stage declared as fitted, arming limited to
  10 min, minimum delay, waiting for silence, stop on collision.

The second C6, running the `generateur` firmware, imitates the line on the
bench with patterns of known content ([docs/BANC.md](docs/BANC.md)).

Build (PlatformIO, same platform as the ScreenBar: pioarduino 55.03.312-1,
Arduino-ESP32 3.3.12, IDF 5.5):

```
~/.platformio/penv/bin/pio run -e sonde
~/.platformio/penv/bin/pio run -e generateur
```

The probe is flashed **only with the hood unplugged** or on the bench
(`-t upload --upload-port <port>`). On the USB console (115200 baud), `help`
lists the commands.

## The product module

Sub-project 2 ([docs/SPEC-PRODUIT.md](docs/SPEC-PRODUIT.md)): a Matter node
over Thread, EP1 Fan (three speeds) and EP2 Light, which drives the hood
through its original panel. PlatformIO environment `produit`, sources in
`src/produit/`. **For now, the line driver is simulated**: no pin is driven;
the real driver (A, B or C) will come after step 6.

```
python3 tools/codes_appairage.py                        # once: local pairing codes, ignored by git
~/.platformio/penv/bin/pio run -e produit -t erase    # first flash (Thread)
~/.platformio/penv/bin/pio run -e produit -t upload
```

Bench without the hood: [docs/BANC-MATTER.md](docs/BANC-MATTER.md). Companion
profile: [docs/PROTOCOLE-JSON-PRODUIT.md](docs/PROTOCOLE-JSON-PRODUIT.md).

## Mac tools

Python 3.11, standard library only (the serial port is opened through
`termios`, without resetting the C6).

| Tool | Role |
|---|---|
| `tools/serie_enregistre.py <port> <scenario> [command ...]` | machine session over USB, recorded in `logs/YYYY-MM-DD-hhmm-<scenario>.jsonl` |
| `tools/hotte_udp.py [--appareil sonde\|produit] cle <port>` | H1 key set over USB, stored in `~/.config/hotte-<device>/cle` (never displayed); the device is read from `hello` when not given |
| `tools/hotte_udp.py session <host> [command ...]` | network session, readable summary; `injection on` is sent only after the phrase "je suis devant la hotte" ("I am at the hood") is typed in a terminal (rule 11) |
| `tools/hotte_udp.py enregistre <host> <scenario> [command ...]` | same session, recorded as `.jsonl`, summary in `logs/live.log` |
| `tools/analyse.py histo\|trames\|auto\|uart\|diff <capture>` | histograms, re-splitting into frames, UART decoding, pulse-distance or Manchester, differences between scenarios (step 6) |
| `tools/banc.py <capture> <pattern>` | bench verdicts: frames decoded bit for bit, duration deviations |
| `tools/json_check.py [--jsonl] [--profil sonde\|produit] <capture>` | conformance of machine lines to the hood profile (probe or product) |
| `tools/verse_capture.py <capture.jsonl> <name>` | files a reference capture (steps 5 and 5b) from `logs/` into `captures/`, with its provenance; rejects any key, SSID or password |
| `tools/codes_appairage.py` | draws the module's pairing codes once into `src/produit/codes_appairage.h` (local, ignored by git) |
| `tools/garde.py` | refuses real identifiers (listed in the local `garde.local.txt`) in tracked files; run by the `pre-push` hook |

Tests without a board, from the repository root:

```
sh tools/tests/test_hote.sh                     # C++ (clang++): capture, protocol, patterns, H1, injection
python3 -m unittest discover -s tools/tests -v  # Python tools
```

## Documents

All documents are in French.

| Document | Content |
|---|---|
| [docs/SPEC-RECONNAISSANCE.md](docs/SPEC-RECONNAISSANCE.md) | sub-project 1: goal, safety, steps, adapter, "probe" firmware, tools, validation bench |
| [docs/SECURITE.md](docs/SECURITE.md) | the 13 rules as a sheet, isolation and leakage tests, what to do on an anomaly |
| [docs/RECONNAISSANCE.md](docs/RECONNAISSANCE.md) | log of steps −1 to 7: measurements, photos, decisions |
| [docs/WIRING.md](docs/WIRING.md) | adapter, output cable, measurement box, stages, pinout, bill of materials |
| [docs/BANC.md](docs/BANC.md) | validation bench: procedure, criteria 1 to 6, results |
| [docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md) | hood profile of the companion protocol, as differences from the ScreenBar |
| [docs/SPEC-PRODUIT.md](docs/SPEC-PRODUIT.md) | sub-project 2: the Matter product (exposure, state machine, hardware, verification) |
| [docs/PROTOCOLE-JSON-PRODUIT.md](docs/PROTOCOLE-JSON-PRODUIT.md) | hood profile of the product module, as differences |
| [docs/BANC-MATTER.md](docs/BANC-MATTER.md) | Matter bench without the hood: procedure and results |
| [docs/BRIEF-RECHERCHE.md](docs/BRIEF-RECHERCHE.md) | research of 24/09/2026: board identification, plausible protocols, Matter and Apple Home, 57 sources |
| [docs/photos/](docs/photos/) | photos of the board and the rating label (serial number hidden) |
| [captures/](captures/README.md) | reference captures of steps 5 and 5b, filed by `tools/verse_capture.py`: rules and provenance |

## The three sub-projects

1. **Reconnaissance** (in progress): decode line `D`, choose the architecture,
   prove injection.
2. **Product** (in progress: state machine, Matter layer and companion,
   simulated driver): final interface and Matter firmware (3-speed fan, light).
3. **Shared library and companion app** for several devices, shared with the
   sibling project.

## Sibling project

[benq-screenbar-halo-matter](https://github.com/Djoko-cli/benq-screenbar-halo-matter):
a BenQ ScreenBar Halo controlled over Matter on Thread by an ESP32-C6. Same
platform (Arduino-ESP32 3.3.12 via pioarduino), same companion protocol. The
code reused here comes from its commit
[`c58a506`](https://github.com/Djoko-cli/benq-screenbar-halo-matter/tree/c58a506).

---

[English](#haier-cchp21ebb-cooker-hood--matter-over-thread) · **Français**

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
>
> La documentation est en français ; ce README est bilingue.

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

## Le module produit

Sous-projet 2 ([docs/SPEC-PRODUIT.md](docs/SPEC-PRODUIT.md)) : un nœud Matter
sur Thread, EP1 Ventilateur (trois paliers) et EP2 Lumière, qui commande la
hotte à travers son panneau d'origine. Environnement PlatformIO `produit`,
sources dans `src/produit/`. **Pour l'instant, pilote de ligne simulé** :
aucune broche pilotée ; le pilote réel (A, B ou C) viendra après l'étape 6.

```
python3 tools/codes_appairage.py                        # une seule fois : codes d'appairage locaux, ignores par git
~/.platformio/penv/bin/pio run -e produit -t erase    # premier flash (Thread)
~/.platformio/penv/bin/pio run -e produit -t upload
```

Banc sans hotte : [docs/BANC-MATTER.md](docs/BANC-MATTER.md). Profil
compagnon : [docs/PROTOCOLE-JSON-PRODUIT.md](docs/PROTOCOLE-JSON-PRODUIT.md).

## Outils Mac

Python 3.11, bibliothèque standard seulement (le port série s'ouvre par
`termios`, sans redémarrer le C6).

| Outil | Rôle |
|---|---|
| `tools/serie_enregistre.py <port> <scénario> [commande ...]` | session machine par l'USB, enregistrée dans `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl` |
| `tools/hotte_udp.py [--appareil sonde\|produit] cle <port>` | clé H1 posée par l'USB, rangée dans `~/.config/hotte-<appareil>/cle` (jamais affichée) ; l'appareil se lit dans `hello` s'il n'est pas donné |
| `tools/hotte_udp.py session <hôte> [commande ...]` | session réseau, résumé lisible ; `injection on` ne part qu'après la phrase « je suis devant la hotte », tapée dans un terminal (règle 11) |
| `tools/hotte_udp.py enregistre <hôte> <scénario> [commande ...]` | même session, enregistrée en `.jsonl`, résumé dans `logs/live.log` |
| `tools/analyse.py histo\|trames\|auto\|uart\|diff <capture>` | histogrammes, redécoupage en trames, décodage UART, distance d'impulsion ou Manchester, différences entre scénarios (étape 6) |
| `tools/banc.py <capture> <motif>` | verdicts du banc : trames décodées au bit près, écarts de durées |
| `tools/json_check.py [--jsonl] [--profil sonde\|produit] <capture>` | conformité des lignes machine au profil hotte (sonde ou produit) |
| `tools/verse_capture.py <capture.jsonl> <nom>` | verse une capture de référence (étapes 5 et 5b) de `logs/` dans `captures/`, avec sa provenance ; refuse toute clé, tout SSID ou mot de passe |
| `tools/codes_appairage.py` | tire une fois les codes d'appairage du module dans `src/produit/codes_appairage.h` (local, ignoré par git) |
| `tools/garde.py` | refuse les vrais identifiants (listés dans `garde.local.txt`, local) dans les fichiers suivis ; lancé par le crochet `pre-push` |

Tests sans carte, depuis la racine du dépôt :

```
sh tools/tests/test_hote.sh                     # C++ (clang++) : capture, protocole, motifs, H1, injection
python3 -m unittest discover -s tools/tests -v  # outils Python
```

## Documents

Tous les documents sont en français.

| Document | Contenu |
|---|---|
| [docs/SPEC-RECONNAISSANCE.md](docs/SPEC-RECONNAISSANCE.md) | sous-projet 1 : objectif, sécurité, étapes, adaptateur, firmware « sonde », outils, banc de validation |
| [docs/SECURITE.md](docs/SECURITE.md) | les 13 règles en fiche, tests d'isolation et de fuite, conduite en cas d'anomalie |
| [docs/RECONNAISSANCE.md](docs/RECONNAISSANCE.md) | journal des étapes −1 à 7 : mesures, photos, décisions |
| [docs/WIRING.md](docs/WIRING.md) | adaptateur, câble de sortie, boîtier de mesure, étages, brochage, nomenclature |
| [docs/BANC.md](docs/BANC.md) | banc de validation : procédure, critères 1 à 6, résultats |
| [docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md) | profil hotte du protocole compagnon, par différence avec la ScreenBar |
| [docs/SPEC-PRODUIT.md](docs/SPEC-PRODUIT.md) | sous-projet 2 : le produit Matter (exposition, automate, matériel, vérification) |
| [docs/PROTOCOLE-JSON-PRODUIT.md](docs/PROTOCOLE-JSON-PRODUIT.md) | profil hotte du module produit, par différence |
| [docs/BANC-MATTER.md](docs/BANC-MATTER.md) | banc Matter sans hotte : procédure et résultats |
| [docs/BRIEF-RECHERCHE.md](docs/BRIEF-RECHERCHE.md) | recherche du 24/09/2026 : identification de la carte, protocoles plausibles, Matter et Apple Home, 57 sources |
| [docs/photos/](docs/photos/) | photos de la carte et de l'étiquette (numéro de série masqué) |
| [captures/](captures/README.md) | captures de référence des étapes 5 et 5b, versées par `tools/verse_capture.py` : règles et provenance |

## Les trois sous-projets

1. **Reconnaissance** (en cours) : décoder la ligne `D`, choisir l'architecture,
   prouver l'injection.
2. **Produit** (en cours : automate, couche Matter et compagnon, pilote simulé) :
   interface définitive et firmware Matter (ventilateur 3 vitesses, lumière).
3. **Bibliothèque commune et app compagnon** multi-appareils, partagées avec le
   projet frère.

## Projet frère

[benq-screenbar-halo-matter](https://github.com/Djoko-cli/benq-screenbar-halo-matter) :
une BenQ ScreenBar Halo pilotée en Matter sur Thread par un ESP32-C6. Même
plateforme (Arduino-ESP32 3.3.12 via pioarduino), même protocole compagnon. Le
code repris ici vient de son commit
[`c58a506`](https://github.com/Djoko-cli/benq-screenbar-halo-matter/tree/c58a506).
