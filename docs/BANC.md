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
