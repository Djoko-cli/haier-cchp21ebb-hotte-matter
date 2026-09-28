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
- testeur USB, s'il y en a un (critère 6, §2.4 et §8.3) ;
- la batterie USB de l'étape 5 (§8) et, sans testeur USB, un câble USB de
  cette batterie qu'on peut ouvrir (fils VBUS et GND accessibles, §8.3) ;
- pull-up de ligne : 10k (variante 1) ; 4,7k et 1 nF (variante 2) ;
- fils Dupont, un fil de masse ;
- multimètre (contrôles, critère 6) ;
- plus tard : l'analyseur FX2 (critère 2 complet) ; l'étage d'injection de la
  sonde (critère 5, §9) : Q2 (BC547 ou 2N3904), 4,7k, 10k, 470 Ω.

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

Deux méthodes ; l'instrument est noté au §7. Wi-Fi actif, sur la batterie :
§8.3.

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
  rafale ; 0 : sans fin), `impulsions <bas_us> <periode_ms> [n]` (critère 5,
  §9 ; 0 ou rien : sans fin), `stop`, `etat`, `help`. Il affiche
  `motif <nom> trame <index>` à chaque trame, puis `motif <nom> fini : <n> trames`
  (`impulsion <index>` à chaque impulsion). Console fermée, le générateur
  n'attend pas : les lignes qui ne tiennent plus dans son tampon USB sont
  sautées et la ligne `fini` en donne le nombre (`; <k> ligne(s) de progression
  sautee(s), console non lue`) ; la cadence des trames n'en dépend pas.
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
| 5 | Collision détectée et émission arrêtée ; à défaut, la limite est documentée | étage d'injection monté au banc (§9) ; `impulsions 500 50` du générateur pendant une injection : résultat `collision`, et la trame capturée s'arrête à l'impulsion du générateur | 24 |
| 6 | Consommation de la sonde relevée, Wi-Fi actif | USB d'abord (tâche 16), Wi-Fi actif ensuite | 16 (USB), 21 (Wi-Fi) |

## 7. Résultats

### Banc 0 : les firmwares seuls, sans étages (28/09/2026)

En attendant les pièces, les deux C6 sont reliés **directement** : GPIO7 du
générateur sur GPIO6 de la sonde, masses communes, les deux sur l'USB du Mac
(câblage fait USB débranché). Ni NPN, ni pull-up, ni étage d'écoute, ni
5 V. Les deux inversions s'annulent : le générateur met GPIO7 haut pour une
ligne basse, et la sonde, en `inverse` (réglage par défaut), lit GPIO bas
comme bus haut. Ce banc valide les firmwares et la chaîne d'outils (capture
RMT, découpe, décodeurs, `banc.py`), **pas les étages** : variantes, décalage,
tensions et consommation restent pour la tâche 16. Les motifs ont été lancés
par un script de l'agent : réglages du §4, générateur mis à `stop` avant
chaque capture, sa console lue pendant le motif.

Sonde `0.1.0-2b223b1` sur `/dev/cu.usbmodem144401`, générateur
`0.1.0-68262a1` sur `/dev/cu.usbmodem144201`.

| Motif | Fichier `logs/` | Trames reconnues / 1000 | Décodeur de `auto` | Écart moyen hauts / bas (µs) | Écart max (µs) | Décalage suggéré (µs) | Verdict |
|---|---|---|---|---|---|---|---|
| `uart500` | `2026-09-28-1348-banc0-uart500.jsonl` | 1000 | uart 500 bauds, 0 erreur | −0,01 / −0,01 | 1 | 0,0 | OK |
| `uart500inv` | `2026-09-28-1351-banc0-uart500inv.jsonl` | 1000 | uart 500 bauds inverse, 0 erreur | −0,01 / −0,01 | 1 | 0,0 | OK |
| `uart2400` | `2026-09-28-1340-banc0-uart2400.jsonl` | 1000 | uart 2400 bauds, 0 erreur | 0,00 / 0,00 | 1 | 0,0 | OK |
| `uart2400inv` | `2026-09-28-1342-banc0-uart2400inv.jsonl` | 1000 | uart 2400 bauds inverse, 0 erreur | 0,00 / 0,00 | 1 | 0,0 | OK |
| `uart9600` | `2026-09-28-1339-banc0-uart9600.jsonl` | 1000 | uart 9600 bauds, 0 erreur | 0,00 / 0,00 | 1 | 0,0 | OK |
| `uart9600inv` | `2026-09-28-1340-banc0-uart9600inv.jsonl` | 1000 | uart 9600 bauds inverse, 0 erreur | 0,00 / 0,00 | 1 | 0,0 | OK |
| `wtc` | `2026-09-28-1343-banc0-wtc.jsonl` | 1000 | distance d'impulsion T = 750 µs, 0 erreur | 0,00 / 0,00 | 1 | 0,0 | OK |
| `krona` | `2026-09-28-1345-banc0-krona.jsonl` | 1000 (1250 réceptions, 750 trames comparées) | uart 500 bauds inverse, 0 erreur | −0,01 / −0,02 | 1 | 0,0 | OK |
| `rafale` | `2026-09-28-1344-banc0-rafale.jsonl` | critère 3 : 1 / 1 rafale complète, aucune perte (10 000 fronts/s) | — | 0,00 / 0,00 | 1 | 0,0 | OK |

Pour tous les motifs : `debord`, `lignes_perdues` et `sautes` restent à 0, et
`json_check.py` ne signale ni erreur ni trou de `n`.

Défauts trouvés par ce banc, puis corrigés :
- `68262a1`, générateur : console fermée, chaque ligne `motif … trame N`
  attendait 2 s (HWCDC : 20 essais de 100 ms), et le motif tombait à une trame
  toutes les 2 s. La ligne est maintenant sautée et comptée (ligne `fini`).
- `2b223b1`, sonde : dans une réception à nombre pair de durées, le mot qui
  porte le marqueur de fin du RMT reprend la dernière durée, qui était comptée
  deux fois. Premier relevé : trame 0 de `krona` (repos pris 18 ms avant, sous
  le silence de 19 ms), 2000 µs lus 4000, soit 0xFF lu 0xFE, d'où
  `critere 1 : 999 / 1000`. Reproduit avec `uart9600inv` et une pause de 3 ms :
  208 µs lus 416. Sur la hotte, une réception peut aussi commencer par un
  palier de repos, par exemple si la capture démarre en pleine trame.

Observation sans effet sur les critères : la pause du générateur vaut la
valeur demandée plus 48 à 51 µs (médiane). Mais 3 à 10 pauses sur 1000 durent
1 à 4 ms de plus, la tâche `loop` du générateur ayant pris du retard. Seul
compte ici un repos au moins égal au silence de la sonde.

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
| | 21 | 1 (10k) | | | | | | | |
| | 24 | 1 (10k) | | | | | | | |

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

| Date | Alimentation, instrument (§2.4) | Situation | Courant (mA) |
|---|---|---|---|
| | USB, sans Wi-Fi ; testeur USB ou multimètre : ____ | capture active, bus au repos | |
| | USB, sans Wi-Fi ; testeur USB ou multimètre : ____ | capture active, motif `krona` | |
| | batterie, Wi-Fi actif (tâche 21) ; testeur USB ou multimètre : ____ (§8.3) | session UDP, motif `krona` | |

### Batterie USB : 30 min en Wi-Fi (tâche 21)

| Date | Modèle de la batterie | Fichier `logs/` | Boots | `up_s` (début → fin) | Plus grand trou de `rx_ms` (s) | Verdict |
|---|---|---|---|---|---|---|
| | | | | | | |

### Conclusions

- Débit maximal de la capture :
- Décalage et asymétrie retenus :
- Délai de l'étage à l'analyseur, et `tol_us` proposé (tâche 16b) :
- Limites constatées :

## 8. Par le Wi-Fi, sur la batterie (critères 4 et 6)

Même montage (§2, variante 1), **toujours sans aucune liaison avec la hotte**
(§2.1) : sonde sur le boîtier détaché (hotte débranchée pendant toute la
séance, fiche XH 4 broches retirée de l'embase, les deux **OL** relevés avant
tout USB, batterie comprise), ou sur la plaque d'essai du banc. La sonde parle
au Mac par le Wi-Fi (UDP, port 5480,
[PROTOCOLE-JSON.md §9](PROTOCOLE-JSON.md#9-transport-réseau--udp-sur-le-wi-fi)).
À partir du §8.2, elle est alimentée par la **batterie USB** de l'étape 5
(spec §12), pas par le Mac : sans hôte qui lit, l'USB ne ralentit jamais la
sonde. Le générateur reste sur l'USB du Mac, console ouverte ; le fil de masse
relie les deux C6. Rien de ce qui suit ne s'écrit dans ce fichier : ni SSID,
ni mot de passe, ni clé, ni adresse IP.

Ordre : Wi-Fi et clé (§8.1), trente minutes sur la batterie (§8.2),
consommation (§8.3), dix minutes de `krona` (§8.4).

### 8.1 Une fois : Wi-Fi et clé

1. **Identifiants Wi-Fi (Majid, au clavier).** Sonde sur l'USB du Mac. Majid
   ouvre une console **hors du dépôt**, pour que `log2file` n'enregistre pas le
   mot de passe :
   `cd ~ && ~/.platformio/penv/bin/pio device monitor -p $PORT_SONDE -b 115200`
   (ou, depuis le dépôt, avec `-f direct` en plus : un filtre donné en ligne de
   commande remplace ceux de `platformio.ini`, `log2file` compris).
   Il tape `wifi <ssid> <mdp>`, puis `info` quelques secondes plus tard. Le mot
   de passe se **tape à la main** : collé, il peut porter un caractère invisible
   (espace insécable, apostrophe typographique), et la sonde refuse en donnant
   l'octet et sa position (`wifi : refuse, rien ne change : mot de passe : octet 0xC2 en position 10 ...`).
   Attendu : `wifi : <ssid>, connecte, IP <a.b.c.d>, RSSI <-xx> dBm, mDNS hotte-sonde.local, 0 perte(s) depuis le demarrage`.
   Il ferme la console (Ctrl-C) pour libérer le port.
2. **Clé H1 (l'agent).** `python3 tools/hotte_udp.py cle $PORT_SONDE`.
   Attendu : `cle rangee dans /Users/<nom>/.config/hotte-sonde/cle (empreinte XXXXXXXX)`.
   La clé n'est jamais affichée.
3. **Chemin (l'agent).** `ping -c 3 hotte-sonde.local`, puis
   `python3 tools/hotte_udp.py session hotte-sonde.local --duree 15`.
   Attendu : `session XXXXXXXX ouverte`, `hello/base` avec `'transport': 'udp'`,
   `reponse id=1 fin ok « json 1 »`, un `etat` toutes les 2 s, `reseau/ip` avec
   le RSSI, et à la fin `0 rejetee(s)`.

### 8.2 Trente minutes sur la batterie

La batterie de l'étape 5 doit tenir la sonde en Wi-Fi 30 min sans se couper
(spec §12) : certaines batteries s'éteignent seules sous un faible courant.

1. **Sonde sur la batterie (Majid).** Le banc à l'emplacement de test (là où
   la sonde sera posée à l'étape 5), toujours sans liaison avec la hotte.
   Majid débranche la sonde du Mac et la branche sur la batterie : elle
   redémarre et rejoint le Wi-Fi. Après 10 s,
   `python3 tools/hotte_udp.py session hotte-sonde.local --duree 10` (l'agent)
   doit ouvrir la session.
2. **Motif sans fin (Majid).** `motif krona 0` dans la console du générateur.
3. **Enregistrement (l'agent)**, 31 min :
   `python3 tools/hotte_udp.py enregistre hotte-sonde.local batterie-30min "capture tout" "seuils 1 19000" --duree 1860`.
   À la fin, Majid tape `stop` dans la console du générateur.
4. **Verdict (l'agent)**, sur le fichier écrit :
   `python3 -c "import json,sys; L=[json.loads(x) for x in open(sys.argv[1])]; B={r['l']['boot'] for r in L if 'boot' in r['l']}; U=[r['l']['up_s'] for r in L if 'up_s' in r['l']]; T=[r['rx_ms'] for r in L]; print('boots', len(B), '; up_s', U[0], 'a', U[-1], '(croissant)' if U == sorted(U) else '(RECUL)', '; plus grand trou de rx_ms', max(b - a for a, b in zip(T, T[1:])) / 1000, 's')" logs/<fichier>.jsonl`.
   Attendu : `boots 1 ; up_s <début> a <fin> (croissant) ; plus grand trou de rx_ms <g> s`.
   La batterie convient si : **un seul boot** dans le fichier, `up_s`
   croissant jusqu'à **1 800 au moins**, et **aucun trou de `rx_ms` de plus
   de 10 s**. Sinon (la sonde a redémarré, ou s'est tue) : une autre batterie,
   et on recommence.
5. Noter le modèle de la batterie, la date et le résultat au §7 (« Batterie
   USB ») et dans [RECONNAISSANCE.md](RECONNAISSANCE.md#installation-et-instruments)
   (« Installation et instruments »).

### 8.3 Consommation, Wi-Fi actif (critère 6)

Sonde sur la batterie, pendant une session Wi-Fi avec le motif `krona`.
L'instrument est noté au §7 (critère 6). Deux méthodes :

- **Testeur USB**, s'il y en a un : entre la batterie et le câble USB de la
  sonde.
- **Sinon, multimètre en série sur VBUS**, depuis la batterie :
  1. batterie débranchée ; **l'USB-C de la sonde reste débranché pendant
     toute la mesure** : jamais deux sources sur sa broche `5V` ;
  2. un câble USB de la batterie, **ouvert** (fils VBUS et GND accessibles) :
     son fil GND sur `TP−` (le `GND` de la sonde) ; multimètre en courant
     continu, **calibre 200 mA**, borne mA sur le fil VBUS, COM sur la broche
     `5V` de la sonde ; sur le boîtier, `J2` reste ouvert ;
  3. brancher la batterie : la sonde démarre, capture active, et rejoint le
     Wi-Fi. Si le courant saute sans cesse, ou si la session tombe, la chute
     de tension dans le calibre est trop forte : passer sur l'entrée 10 A ;
  4. à la fin, débrancher la batterie, retirer le multimètre et le câble
     ouvert, puis rebrancher la batterie sur l'USB-C de la sonde.

Mesure : l'agent lance
`python3 tools/hotte_udp.py session hotte-sonde.local --duree 60`, Majid tape
`motif krona 400` dans la console du générateur et lit le courant moyen (de
l'ordre de 100 mA, spec §8.7 ; les pointes d'émission ne s'y lisent pas,
§2.4).

### 8.4 Dix minutes de `krona` (critère 4)

1. **Emplacement (Majid).** Sonde sur la batterie, à l'emplacement de test ;
   générateur sur l'USB du Mac, console ouverte ; fil de masse en place.
   Attendre 10 s après le branchement de la batterie, puis
   `python3 tools/hotte_udp.py session hotte-sonde.local --duree 10` (l'agent)
   doit ouvrir la session.
2. **Enregistrement (l'agent).**
   `python3 tools/hotte_udp.py enregistre hotte-sonde.local banc-wifi-krona "capture tout" "seuils 1 19000" --duree 660`.
   Dans un second terminal : `tail -f logs/live.log` (un résumé toutes les
   5 s).
3. **Motif (Majid).** Dès que le terminal affiche
   `reponse id=3 fin ok « seuils 1 19000 »`, Majid tape `motif krona 4500` dans
   la console du générateur : 4 500 trames de 134 ms, soit 10 min 3 s. Il
   regarde défiler `motif krona trame <index>` jusqu'à
   `motif krona fini : 4500 trames`. L'enregistrement s'arrête seul à 660 s.
4. **Verdicts (l'agent)**, sur le fichier écrit :
   - `python3 tools/json_check.py --jsonl logs/<fichier>.jsonl` : 0 erreur, et
     la ligne `n : ... (... ligne(s) perdue(s), X %)` ; il faut **X < 0,100**.
   - `python3 tools/banc.py logs/<fichier>.jsonl krona --n 4500 --decalage-us <retenu au §7>` :
     `compteurs de la sonde pendant la capture : debord +0, lignes_perdues +0, sautes +0`
     (zéro perte côté sonde) ; `critere 1` vaut 4500 / 4500 si le Wi-Fi n'a
     rien perdu.
   - RSSI pendant la capture :
     `python3 -c "import json,sys; r=[json.loads(l)['l']['wifi']['rssi_dbm'] for l in open(sys.argv[1]) if json.loads(l)['l'].get('bloc')=='ip']; print('RSSI', min(r), 'a', max(r), 'dBm sur', len(r), 'releves')" logs/<fichier>.jsonl`.
5. Noter au §7 (critère 4) : date, emplacement (en mots, sans adresse), RSSI,
   durée, hausses des compteurs, trous de `n`, verdict.

Si le critère échoue : noter le RSSI et `wifi.pertes`, rapprocher la sonde du
point d'accès et recommencer ; sinon, reprendre le risque « Wi-Fi trop
faible » du §13 de la spec (enregistrement local, relu par l'USB).

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
