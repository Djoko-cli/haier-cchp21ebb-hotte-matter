# Câblage de la sonde

**Sous-projet 1 : reconnaissance de la ligne `D`.** Adaptateur, câble de sortie,
boîtier de mesure, étages d'écoute et d'injection, brochage du C6 SuperMini,
générateur du banc. Source : §7, §10 et §12 de
[SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md), qui fait foi. Règles de
sécurité : [SECURITE.md](SECURITE.md) (en particulier les règles 4, 5, 9 et 10,
et le banc sans liaison avec la hotte). Les relevés vont dans
[RECONNAISSANCE.md](RECONNAISSANCE.md).

## 1. Vue d'ensemble

L'ensemble a trois parties, pour que **tout ce qu'on mesure ou manipule soit
hors de la hotte**.

```
   DANS LE BOÎTIER / LE CORPS DE LA HOTTE                    HORS DE LA HOTTE, à vue
 ┌───────────────────────────────────────────┐          ┌────────────────────────────────────┐
 │ ADAPTATEUR (petite plaque, gainée)        │          │ BOÎTIER DE MESURE (plaque à        │
 │                                           │          │ pastilles, fixée, détachable)      │
 │  sorties : −, +, D_panneau, D_carte       ├─────────►│  embase XH 4 br. ◄ fiche du câble  │
 │                                           │  câble   │  TP−  TP+  TP_Dp  TP_Dc            │
 │  fiche XH 3 br. ──► CN3 [- D +]           │  de      │  J2 : TP+ ── 5V de la sonde        │
 │  embase XH 3 br. ◄── fiche du panneau     │  sortie, │       (ouvert par défaut)          │
 │                                           │  4 fils, │  étage d'écoute voie 1 → GPIO6     │
 │  D_carte ──┤J1├── D_panneau               │  ≥300 V, │  étage d'injection voie 1 ← GPIO7  │
 │   (fermé par défaut ; ouvert = B)         │  gainé ; │  (voie 2 : GPIO0 / GPIO1, si B)    │
 │                                           │  fiche   │  ESP32-C6 SuperMini sur barrettes  │
 └───────────────────────────────────────────┘  XH 4 br.│  batterie USB (captures)           │
                                                        └────────────────────────────────────┘
```

- **Réversible :** on retire l'adaptateur, on rebranche la fiche du panneau sur
  `CN3`, et la hotte est d'origine.
- **Détachable :** le câble de sortie se branche sur le boîtier de mesure par une
  fiche et une embase XH 4 broches (§3 et §4). On ne l'enfiche et on ne la
  retire que hotte débranchée (règle 4). Fiche retirée, le boîtier n'a plus
  aucune liaison avec la hotte : c'est ainsi qu'il peut servir au banc (§10).
- **La sonde n'est jamais alimentée par la hotte** : `J2` reste ouvert, sauf à
  l'étape 4 (hotte débranchée).
- **Jamais une tension du bus directement sur une GPIO** (règle 10) : `D` passe
  toujours par l'étage d'écoute.

## 2. Adaptateur

Petite plaque, **gainée**, qui reste près de `CN3`. Elle s'intercale entre `CN3`
et la fiche du panneau, et sort les quatre nœuds vers le câble de sortie.

- **Connectique, décidée à l'étape 0** par le pas de `CN3`, mesuré entre ses
  broches extrêmes : 5,0 mm pour 3 broches, c'est du JST XH (pas de 2,5 mm) ;
  4,0 mm, c'est du JST PH (pas de 2,0 mm). Fiches et embases précâblées, contacts
  sertis.
- **Fiche XH 3 broches** enfichée sur `CN3` ; **embase XH 3 broches** qui reçoit
  la fiche du panneau. Les contacts de la fiche sont insérés **dans le même ordre
  que ceux de la fiche du panneau** : noir `−`, blanc `D`, rouge `+` (sérigraphie
  « - D + »), détrompeur du même côté.
- **Pas de broches Dupont** dans les fiches du panneau et de `CN3` d'origine :
  leurs contacts XH se desserrent.
- **`J1`** : barrette 2 broches au pas de 2,54 mm et un cavalier, entre `D_carte`
  et `D_panneau`. **Fermé par défaut** ; ouvert seulement pour la mesure du
  pull-up (étape 2b) et pour l'architecture B.
- **Placement :** `J1` doit rester accessible filtre retiré, **sans ouvrir le
  boîtier** électronique (étape 2b). Si la fiche du panneau peut sortir du boîtier
  (étape 0), la plaque se place juste à la sortie, et une fiche précâblée courte
  va jusqu'à `CN3`.

```
  CÔTÉ CARTE : fiche XH 3 br. sur CN3             CÔTÉ PANNEAU : embase XH 3 br.
  (ordre des broches sur CN3 : − D +)             (reçoit la fiche du panneau)

  −  noir   ○──────────────────────────────────────○  −  noir   ─────► fil 1 : −
  +  rouge  ○──────────────────────────────────────○  +  rouge  ─────► fil 2 : +
  D  blanc  ○── D_carte ──●──┤J1├──●── D_panneau ──○  D  blanc
                          │        │
                          │        └───────────────────────────────► fil 3 : D_panneau
                          └────────────────────────────────────────► fil 4 : D_carte
```

| Nœud | Côté carte (fiche sur `CN3`) | Côté panneau (embase) | Câble de sortie | Boîtier de mesure |
|---|---|---|---|---|
| `−` | broche `−`, fil noir | broche `−` | fil 1 | `TP−` |
| `+` | broche `+`, fil rouge | broche `+` | fil 2 | `TP+` |
| `D_panneau` | (par `J1`) | broche `D` | fil 3 | `TP_Dp` |
| `D_carte` | broche `D` | (par `J1`) | fil 4 | `TP_Dc` |

## 3. Câble de sortie

**4 fils** (`−`, `+`, `D_panneau`, `D_carte`), conformes à la règle 5 :

- fil d'au moins 300 V, UL1007 ou UL1015, 105 °C de préférence ;
- glissé **sur toute sa longueur** dans une gaine équivalente à celle d'origine
  (VW-1 600 V), ou dans deux épaisseurs de gaine thermorétractable (3 à 6 mm) ;
- posé le long du cheminement choisi à l'étape 0, fixé au câble du panneau par
  des colliers ; jamais serré contre les fils du moteur ni contre le connecteur
  blanc ; pincé ni par le couvercle du boîtier, ni par le filtre ;
- longueur : celle mesurée à l'étape 0, plus une marge ;
- terminé, côté boîtier de mesure, par une **fiche XH 4 broches** précâblée :
  contacts dans l'ordre `−`, `+`, `D_panneau`, `D_carte` (broches 1 à 4), avec
  son détrompeur. Ses fils sertis sont soudés aux quatre fils du câble, chaque
  soudure sous gaine thermorétractable, hors de la hotte. Elle s'enfiche sur
  l'embase XH 4 broches du boîtier (§4), dans un seul sens.

Couleurs proposées (les couleurs réelles sont notées dans `RECONNAISSANCE.md`) :

| Fil | Nœud | Couleur proposée | Broche de la fiche XH 4 br. | Arrive sur |
|---|---|---|---|---|
| 1 | `−` | noir | 1 | `TP−` |
| 2 | `+` | rouge | 2 | `TP+` |
| 3 | `D_panneau` | blanc | 3 | `TP_Dp` |
| 4 | `D_carte` | jaune (ou toute autre couleur distincte) | 4 | `TP_Dc` |

## 4. Boîtier de mesure

Plaque à pastilles **fixée à côté de la hotte, à vue, hors de l'aplomb de la
plaque de cuisson** (emplacement choisi à l'étape 0). On y pose et on y déplace
les pinces hotte débranchée ; sous tension, on n'y fait que lire les instruments.

- **Embase XH 4 broches**, soudée sur la plaque, qui reçoit la fiche du câble de
  sortie (§3) : broche 1 sur `TP−`, 2 sur `TP+`, 3 sur `TP_Dp`, 4 sur `TP_Dc`,
  détrompeur tourné comme celui de la fiche. On enfiche et on retire la fiche
  hotte débranchée (règle 4) ; retirée, le boîtier est détaché de la hotte (§10).
- **Points de test** `TP−`, `TP+`, `TP_Dp`, `TP_Dc` : des picots (barrette mâle
  2,54 mm) où l'on accroche les grippe-fils.
- **`J2`** : barrette 2 broches et un cavalier, entre `TP+` et la broche `5V` de la
  sonde. **Ouvert par défaut**, fermé seulement à l'étape 4 (panneau seul, hotte
  débranchée) : le 5 V de l'USB de la sonde alimente alors le panneau.
- **Étage d'écoute, voie 1** (§5), et **étage d'injection, voie 1** (§6), monté à
  l'étape 7 seulement. Place réservée pour la voie 2 (§7), si B.
- **La sonde**, ESP32-C6 SuperMini, sur deux barrettes femelles. Son `GND` est
  relié à `TP−` : masse commune avec le `−` du bus, indispensable aux étages.
- **Batterie USB** pendant les captures (modèle vérifié, règle 9).
- La coque de l'USB-C du SuperMini est reliée à `GND` : l'isoler de tout métal
  (gaine ou ruban). Le connecteur USB-C reste accessible, pour flasher hotte
  débranchée. On relève le RSSI Wi-Fi à cet emplacement.

```
  câble de sortie   XH 4 br.   BOÎTIER DE MESURE (hors de la hotte, à vue)
                  fiche  embase

  fil 1 : −          ──► 1 ──●  TP−   ────────────────────────────── GND de la sonde
  fil 2 : +          ──► 2 ──●  TP+   ──┤J2├───────────────────────── 5V de la sonde
                                        ouvert (fermé à l'étape 4 seulement)
  fil 3 : D_panneau  ──► 3 ──●  TP_Dp ─┬─ étage d'écoute, voie 1 ───────► GPIO6
                                       └─ étage d'injection, voie 1 ◄─── GPIO7  (étape 7)
  fil 4 : D_carte    ──► 4 ──●  TP_Dc     (voie 2 : GPIO0 / GPIO1, seulement si B)

  1 à 4 : broches de la fiche et de l'embase XH 4 broches, détrompeur en face.
  Les émetteurs des transistors des deux étages sont sur TP− (§5 et §6).
```

## 5. Étage d'écoute (voie 1 : `D_panneau` vers GPIO6)

```
  TP_Dp ── 100k ──┬──────────── base        ┐
                  │                         │
                 R_be                       │  Q1 : BC547 ou 2N3904
            (100k par défaut)               │
                  │                         │
  TP− ────────────┴──────────── émetteur    │
                                            │
  3V3 (sonde) ── 10k ──┬─────── collecteur  ┘
                       │
                       └───────────────────────► GPIO6     (D haut → GPIO6 bas)
```

- **Charge et comportement :** il ne prélève que 40 à 110 µA sur `D` ; il commute
  en 1 à 2 µs ; il reste neutre si la sonde est éteinte ; il fonctionne tel quel
  sur un bus de 5 V comme de 12 V.
- **Sa logique est inversée** : GPIO6 bas = bus haut. Le firmware remet les
  niveaux dans le sens du bus avant de les publier.
- **Seuil : environ 1,2 V, fixe.** Il ne suit pas la tension du bus et baisse à
  chaud, d'environ 4 mV/°C. Il ne convient donc que si le **niveau bas de `D`**
  reste sous 0,6 V environ, ce qu'on juge à l'étape 5b. L'analyseur FX2 est
  numérique et ne mesure pas de tension : par défaut, le jugement est
  fonctionnel (capture propre, sonde et analyseur d'accord sur la voie 1, chaque
  impulsion de `D` retrouvée à la sortie de l'étage) ; une mesure chiffrée, à
  l'oscilloscope, est optionnelle. Si le jugement échoue, ou si la mesure dépasse
  0,6 V, on relève le seuil par `R_be` :

| `+` (étape 3b) | Niveau bas de `D` (étape 5b) | `R_be` | Seuil sur `D`, environ |
|---|---|---|---|
| 5 V ou 12 V | jugé bon (sous 0,6 V, s'il est mesuré) | 100k (défaut) | 1,2 V |
| 5 V | jugé trop haut (au-dessus de 0,6 V, s'il est mesuré) | 47k | environ 2 V |
| 12 V | jugé trop haut (au-dessus de 0,6 V, s'il est mesuré) | 15k | environ 5 V |

- On garde au moins 25 µA de base au niveau haut (calcul : environ 30 µA avec 47k
  sous 5 V, 70 µA avec 15k sous 12 V).
- **Délai et asymétrie** de l'étage : mesurés au banc (§10).
- Le pilote RMT active le pull-up interne de GPIO6 : sans effet avec le 10k de
  collecteur.

## 6. Étage d'injection (voie 1 : GPIO7 vers `D_panneau`), étape 7 seulement

```
  GPIO7 ── 4,7k ──┬──────────── base        ┐
                  │                         │
                 10k                        │  Q2 : BC547 ou 2N3904
                  │                         │
  TP− ────────────┴──────────── émetteur    │
                                            │
  TP_Dp ── 470 Ω ────────────── collecteur  ┘     (GPIO7 haut → D tiré bas)
```

- **Uniquement en drain ouvert :** on ne pilote jamais `D` en push-pull 3,3 V.
- **Le 10k entre base et émetteur garde Q2 bloqué** pendant le reset, et quand la
  broche est libérée. Le firmware met GPIO7 à l'état bas en toute première
  instruction de `setup()`.
- **Les 470 Ω de collecteur limitent le courant** à environ 10 mA sous 5 V, si une
  sortie active tient `D` haut en face (une puce tactile de type WTC6534 pilote
  elle-même la ligne pour émettre). Avec un pull-up de 4,7k, le niveau bas vaut
  environ 0,5 V. On le juge à l'étape 7, point 4 : fonctionnellement (la carte
  réagit à l'injection), ou, avec un oscilloscope, par la mesure : il doit rester
  sous 0,8 V. On ne descend vers 220 Ω que si ce niveau n'est pas atteint
  (`injection off` et hotte débranchée d'abord).
- **Ne jamais intervertir GPIO6 et GPIO7.** GPIO6 a un pull-up d'environ 45 kΩ
  après le reset : branché sur l'injection, il débloquerait Q2 et tirerait `D` à
  l'état bas.
- Tant que l'étage n'est pas monté, rien ne relie GPIO7 à `D`.

## 7. Voie 2 (`D_carte`), seulement si B

Rien à construire avant l'avenant de l'étape 6. Pour mémoire (§7.4 de la spec) :

- le même étage d'écoute, sur `TP_Dc`, vers GPIO0 ; l'émission sur GPIO1 ;
- `J1` ouvert : chaque tronçon ne garde que le pull-up de son côté. L'avenant
  fixe un pull-up posé côté tronçon qui n'en a pas, de la valeur relevée à
  l'étape 2b ;
- en émission, si `D` est push-pull : un tampon push-pull alimenté par `+`
  (74HCT si `+` = 5 V) au lieu du NPN ;
- le relais : front par front, ou la sonde qui répond elle-même au panneau.

## 8. Analyseur : voies et diviseurs

Étape 5b (validation croisée) et étape 7 (niveau bas de l'injection). Pinces
posées hotte débranchée, **masse de l'analyseur d'abord, sur `TP−`**, puis son
USB branché au Mac, hotte toujours débranchée (règle 4). Mac sur batterie, sans
aucun autre câble (règle 8).

L'analyseur FX2 est **numérique** : il dit si une voie est au-dessus ou
au-dessous de son seuil, pas à quelle tension. La voie 2 montre donc les
impulsions de `D` et leurs fronts, pas la valeur du niveau bas : celui-ci se juge
fonctionnellement (étapes 5b et 7), et ne se chiffre qu'avec un oscilloscope.

- **Voie 1** : sortie de l'étage d'écoute, le même nœud que GPIO6 (3,3 V), en direct.
- **Voie 2** : `TP_Dp` à travers un diviseur à haute impédance choisi d'après `+` :
  47k/68k pour 5 V (5 V donne 2,96 V), 100k/33k pour 12 V (12 V donne 2,98 V).

```
  TP_Dp ── 47k (100k si + = 12 V) ──┬───────► voie 2 de l'analyseur
                                    │
                           68k (33k si + = 12 V)
                                    │
  TP− ──────────────────────────────┴───────► masse de l'analyseur
```

## 9. Brochage du C6 SuperMini

| Broche | Reliée à | Remarque |
|---|---|---|
| GPIO6 | collecteur de Q1 (étage d'écoute) | entrée : RMT en réception, plus une interruption sur les deux fronts |
| GPIO7 | 4,7k de base de Q2 (étage d'injection) | sortie à l'état bas dès la première instruction de `setup()` ; rien de branché avant l'étape 7 (au banc : critère 5) |
| 3V3 | 10k de collecteur de Q1 | |
| GND | `TP−` | masse commune avec le `−` du bus |
| 5V | `J2` (ouvert), puis `TP+` | fermé seulement à l'étape 4 |
| GPIO0, GPIO1 | voie 2, seulement si B | écoute sur GPIO0, émission sur GPIO1 |

Sur le second C6 (générateur du banc, §10) : GPIO7 sur le 4,7k de base de Q3.

**Broches à éviter :**

| GPIO | Pourquoi |
|---|---|
| 4, 5, 8, 9, 15 | broches de démarrage du C6 (8 : LED WS2812 ; 9 : bouton BOOT) |
| 12, 13 | USB natif |
| 16, 17 | UART0 : journal de la ROM au démarrage |
| 21, 22 | trous intérieurs, inaccessibles |

GPIO2 reste libre.

**Repérage.** On repère chaque broche par sa sérigraphie. D'après le brief du
boîtier de la ScreenBar (même carte), une rangée extérieure porte, dans l'ordre,
`6 · 14 · 15 · 18 · 19 · 20 · 3V3 · GND · 5V`. GPIO7 se repère sur la sérigraphie.

**Contrôle électrique du module**, avant de le poser sur les barrettes : module
seul sur l'USB du Mac, rien d'autre de branché, firmware `sonde` chargé. COM du
multimètre sur `GND`, calibre 20 V continu.

| Mesure | Attendu | Sinon |
|---|---|---|
| broche marquée 6 | environ 3,3 V (pull-up interne) | sérigraphie mal lue : ne rien souder |
| broche marquée 7 | 0 V (sortie à l'état bas) | idem |
| broche `5V` | noter la valeur : 5,0 V environ si VBUS est relié directement, 4,7 V environ s'il passe par une diode | |

## 10. Générateur du banc

Banc de validation, avant la hotte (§10 de la spec). Le second C6, avec le
firmware `generateur`, imite un bus de 5 V à drain ouvert. Aucun secteur au banc.

**Le banc n'a jamais de liaison avec la hotte.** La sonde s'y monte de l'une de
ces deux façons :
- **sur le boîtier de mesure**, seulement si la hotte reste débranchée pendant
  toute la séance **et** si le câble de sortie est déconnecté du boîtier (fiche
  XH 4 broches retirée de l'embase, hotte débranchée). Avant de brancher le
  moindre USB, contrôle hors tension, au calibre le plus élevé : `TP−` vers la
  terre de la fiche de la hotte (le trou qui reçoit la broche de terre), puis
  vers la vis de la carcasse : **OL** les deux fois. Sinon, on ne branche rien ;
- **sur une plaque d'essai du banc**, avec son propre étage d'écoute (mêmes
  valeurs qu'au §5 : 100k, `R_be` de 100k, 10k de collecteur).

Ci-dessous, `TP_Dp` et `TP−` désignent l'entrée de cet étage d'écoute et son `−`,
sur le boîtier ou sur la plaque d'essai.

```
  5V (générateur) ── R_pu ──┬─────────────────────────────► LIGNE ──► TP_Dp de la sonde
                            │                                         (étage d'écoute)
                            ├── 1 nF ── GND  (variante 2 seulement)
                            │
                            └────────────── collecteur  ┐
  GPIO7 (générateur) ── 4,7k ──┬─────────── base        │  Q3 : BC547 ou 2N3904
                               │                        │
                              10k                       │
                               │                        │
  GND (générateur) ──┬─────────┴─────────── émetteur    ┘
                     │
                     └──────────────────────────────────────► TP− de la sonde (fil de masse)

  R_pu : 10k seul (variante 1), ou 4,7k avec 1 nF vers la masse (variante 2).
```

- **Fil de masse** entre le `GND` du générateur et le `−` de l'étage d'écoute de
  la sonde (`TP−`).
- **Ligne** : pull-up vers le `5V` du générateur. Variante 1 : 10k seul.
  Variante 2 : 4,7k avec 1 nF vers la masse, pour imiter le filtre supposé de la
  carte.
- **NPN du générateur** : le montage du §6 (4,7k en base, 10k entre base et
  émetteur), collecteur directement sur la ligne. Logique inversée :
  GPIO7 haut = ligne basse.
- **Collision (critère 5)** : l'étage d'injection de la sonde est monté au banc,
  son collecteur (470 Ω) aussi sur la LIGNE. Monté sur le boîtier de mesure, il
  en est retiré avant que le boîtier retourne sur la hotte (§6 : étape 7
  seulement).
- **Retour sur la hotte** (boîtier de mesure seulement) : banc démonté, puis les
  contrôles du boîtier du §12 (lignes 11 à 17), puis la fiche XH 4 broches
  remise, hotte débranchée.
- **Décalage et asymétrie de l'étage (critère 2)** : une voie de l'analyseur sur
  la LIGNE à travers 47k/68k, une voie sur GPIO6.

## 11. Nomenclature

| Partie | Article | Quantité |
|---|---|---|
| Adaptateur | fiche 3 broches précâblée (XH, ou PH selon l'étape 0) | 1 |
| Adaptateur | embase 3 broches (même série) | 1 |
| Adaptateur | barrette mâle 2 broches 2,54 mm et cavalier (`J1`) | 1 |
| Adaptateur | petite plaque à pastilles | 1 |
| Câble de sortie | fil UL1007 ou UL1015, au moins 300 V, 4 couleurs | 4 × la longueur de l'étape 0 |
| Câble de sortie | gaine VW-1, ou gaine thermorétractable de 3 à 6 mm (deux épaisseurs) | toute la longueur |
| Câble de sortie | colliers | selon le cheminement |
| Câble de sortie | fiche XH 4 broches précâblée (fils sertis), côté boîtier de mesure ; gaine thermorétractable pour les soudures | 1 |
| Boîtier de mesure | plaque à pastilles | 1 |
| Boîtier de mesure | embase XH 4 broches (câble de sortie) | 1 |
| Boîtier de mesure | picots (barrette mâle 2,54 mm) : `TP−`, `TP+`, `TP_Dp`, `TP_Dc` | 4 |
| Boîtier de mesure | barrette mâle 2 broches et cavalier (`J2`) | 1 |
| Boîtier de mesure | barrettes femelles 2,54 mm pour le SuperMini | 2 |
| Boîtier de mesure | ESP32-C6 SuperMini (sonde) | 1 |
| Boîtier de mesure | batterie USB, modèle vérifié (30 min en Wi-Fi sans coupure) | 1 |
| Étage d'écoute | Q1 : BC547 ou 2N3904 ; 100k × 2 (série et `R_be`) ; 10k (collecteur) | 1 de chaque |
| Étage d'écoute | `R_be` de rechange : 47k (`+` = 5 V), 15k (`+` = 12 V) | 1 de chaque |
| Étage d'injection | Q2 : BC547 ou 2N3904 ; 4,7k ; 10k ; 470 Ω ; 220 Ω de rechange | 1 de chaque |
| Mesures | résistance 10 kΩ / 2 W (étape 3a) | 1 |
| Mesures | 47k (étape 3c) ; 10k (pull-up optionnel de l'étape 4) | 1 de chaque |
| Mesures | diviseur de l'analyseur : 47k et 68k (5 V), ou 100k et 33k (12 V) | 1 paire |
| Mesures | grippe-fils ; fils à pinces crocodile ; aiguille fine | 2 ; 2 ; 1 |
| Mesures | analyseur logique FX2, 24 MHz, 8 voies, avec PulseView | 1 |
| Banc | ESP32-C6 SuperMini (générateur) | 1 |
| Banc | Q3 : BC547 ou 2N3904 ; 4,7k (base) ; 10k (base-émetteur) | 1 de chaque |
| Banc | pull-up de ligne : 10k (variante 1) ; 4,7k et 1 nF (variante 2) | 1 de chaque |
| Banc | plaque d'essai sans soudure, si la sonde n'y est pas sur le boîtier de mesure (§10) | 1 |
| Banc | étage d'écoute de la plaque d'essai : BC547 ou 2N3904 ; 100k × 2 (série et `R_be`) ; 10k (collecteur) | 1 de chaque |
| Banc | fils Dupont, fil de masse | déjà là |

Totaux par valeur, 1 nF du banc compris (liste d'achats : §12 de la spec) :

| Valeur | Usages | Quantité minimale |
|---|---|---|
| 100k | écoute (série, `R_be`) ; diviseur 12 V ; banc : écoute de la plaque d'essai (série, `R_be`) | 5 |
| 68k | diviseur 5 V | 1 |
| 47k | `R_be` relevé (5 V) ; étape 3c ; diviseur 5 V | 3 |
| 33k | diviseur 12 V | 1 |
| 15k | `R_be` relevé (12 V) | 1 |
| 10k | collecteur de Q1 ; base-émetteur de Q2 ; pull-up de l'étape 4 ; banc : base-émetteur de Q3, pull-up variante 1, collecteur de l'écoute de la plaque d'essai | 6 |
| 4,7k | base de Q2 ; banc : base de Q3, pull-up variante 2 | 3 |
| 470 Ω | collecteur de Q2 | 1 |
| 220 Ω | rechange du collecteur de Q2 | 1 |
| 10 kΩ / 2 W | étape 3a | 1 |
| 1 nF | banc, variante 2 | 1 |
| NPN BC547 ou 2N3904 | Q1, Q2, Q3, écoute de la plaque d'essai du banc, et une rechange | 5 |

**Brochage des NPN** (boîtier TO-92, face plate vers soi, pattes en bas, de gauche
à droite) : BC547 = C-B-E ; 2N3904 = E-B-C. Ils sont inversés l'un par rapport à
l'autre : vérifier sur la fiche du transistor utilisé.

## 12. Contrôles avant pose

**Adaptateur et câble** (étape 2, point 2) : hotte débranchée, adaptateur pas
encore posé, fiche XH 4 br. du câble de sortie enfichée sur le boîtier de mesure
(sauf ligne 10).

| # | Contrôle | Calibre | Attendu |
|---|---|---|---|
| 1 | continuité broche à broche, côté carte vers côté panneau : `−`, `D` (`J1` fermé), `+` | 200 Ω | environ 0 Ω chacune |
| 2 | pas de court-circuit entre `−`, `D` et `+` | le plus élevé | OL (de l'ordre de 100 à 200 kΩ entre `D` et `−` si l'étage d'écoute est déjà relié) |
| 3 | `TP−` vers la broche « − » (fil noir) de la fiche qui ira sur `CN3` | 200 Ω | environ 0 Ω |
| 4 | `TP+` vers la broche « + » (fil rouge) | 200 Ω | environ 0 Ω |
| 5 | `TP_Dc` vers la broche `D` de la fiche ; `TP_Dp` vers la broche `D` de l'embase | 200 Ω | environ 0 Ω |
| 6 | `J1` fermé : `TP_Dp` vers `TP_Dc` | 200 Ω | environ 0 Ω |
| 7 | `J2` ouvert : `TP+` vers la broche `5V` des barrettes | le plus élevé | OL |
| 8 | ordre des contacts de la fiche XH 3 br. identique à celui de la fiche du panneau (noir, blanc, rouge) | visuel | détrompeur du même côté |
| 9 | fiche XH 4 br. du câble de sortie : contacts dans l'ordre `−`, `+`, `D_panneau`, `D_carte` (broches 1 à 4), comme l'embase du boîtier (§4) | visuel | détrompeur : elle n'entre que dans un sens |
| 10 | fiche XH 4 br. retirée de l'embase : `TP−` vers la broche « − » de la fiche qui ira sur `CN3`, puis `TP_Dp` vers la broche `D` de l'embase de l'adaptateur | le plus élevé | OL : le boîtier est bien détaché ; remettre la fiche ensuite |

**Boîtier de mesure** : sonde retirée des barrettes.

| # | Contrôle | Calibre | Attendu |
|---|---|---|---|
| 11 | `TP−` vers la broche `GND` des barrettes | 200 Ω | environ 0 Ω |
| 12 | broche `3V3` vers la broche 6 des barrettes | 20 kΩ | environ 10 kΩ (collecteur de Q1) |
| 13 | `TP_Dp` vers la broche 6 des barrettes | le plus élevé | plus de 100 kΩ dans les deux sens : aucune liaison directe du bus vers une GPIO (règle 10) |
| 14 | collecteur de Q1 sur la broche 6 ; le 4,7k de base de Q2 sur la broche 7 | visuel | GPIO6 et GPIO7 non interverties |
| 15 | brochage de Q1 et Q2 | visuel | conforme à la fiche du transistor (BC547 = C-B-E, 2N3904 = E-B-C) |
| 16 | coque de l'USB-C isolée ; USB-C accessible | visuel | |
| 17 | contrôle électrique du module (§9) | 20 V continu | broche 6 à environ 3,3 V, broche 7 à 0 V |

**Après la pose** : l'étape 2b (isolation depuis les points de test), puis, à
l'étape 4 seulement, le contrôle de polarité : `J2` fermé, USB de la sonde
branché, fiche du panneau retirée, `TP+` par rapport à `TP−` de +4,5 à +5,3 V,
positif (un port USB peut monter à 5,25 V) ; sinon **ARRÊT**.
