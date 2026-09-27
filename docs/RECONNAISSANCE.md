# Journal de la reconnaissance

Journal du sous-projet 1 (déroulé : §6 de
[SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md)). Chaque étape consigne ce
qu'elle a relevé : date, conditions, mesures, photos et décision. **On ne passe à
l'étape suivante que si son critère de sortie est rempli.**

Règles de sécurité : [SECURITE.md](SECURITE.md). Montage, points de test et
valeurs : [WIRING.md](WIRING.md).

**Comment remplir :**
- une ligne par mesure ; « Relevé » : la valeur lue, avec son unité ;
  « Décision » : la suite, ou **ARRÊT** (on ne raccorde rien, on ne met pas sous
  tension, on revient à la conception) ;
- les pinces se posent et se déplacent hotte débranchée (règle 4) ;
- photos dans `docs/photos/`, nom proposé `etapeN-sujet.jpg`, sans métadonnées
  EXIF ;
- ce journal est versé dans un **dépôt public** : ni SSID, ni mot de passe, ni
  clé, ni adresse, ni numéro de série.

## Installation et instruments

| Élément | Relevé |
|---|---|
| Hotte | Haier CCHP21EBB, fabrication 06/2024 |
| Prise et différentiel 30 mA (repère au tableau électrique) | |
| Multimètre : modèle ; calibre ohmmètre le plus élevé ; fréquence et rapport cyclique ? | |
| Batterie USB : modèle ; essai de 30 min, sonde en Wi-Fi, sans coupure (date, résultat) | |
| Analyseur FX2 : reçu le ; version de PulseView | |
| Oscilloscope, optionnel (niveaux bas chiffrés aux étapes 5b et 7) : modèle, ou « aucun » | |
| Firmware de la sonde (`fw` de `hello`) | |
| Couleurs du câble de sortie : fil 1 `−`, fil 2 `+`, fil 3 `D_panneau`, fil 4 `D_carte` | |
| `R_be` de l'étage d'écoute (100k par défaut ; changements notés à l'étape 5b) | |

## Tests du différentiel (règle 2)

Avant la première mise sous tension de chaque montage, juste avant de le poser,
hotte dans son état précédent, déjà contrôlé : hotte branchée, lumière allumée,
bouton T. **La lumière doit s'éteindre**, sinon **ARRÊT**. Réarmer, puis
débrancher pour poser le montage.

| Date | Avant quel montage | Lumière éteinte par T ? | Décision |
|---|---|---|---|
| | adaptateur (première mise sous tension : étape 2c) | | |
| | fuite en charge (étape 3a) | | |
| | sonde (étape 5) | | |
| | analyseur (étape 5b) | | |
| | étage d'injection (étape 7) | | |

## Journal des anomalies (règle 12)

Anomalie : déclenchement du différentiel ou du disjoncteur, odeur, fumée,
claquements de relais répétés, bip continu, panneau mort, fil chaud. On
débranche ; on ne réarme et on ne rebranche qu'une fois la cause comprise et
notée ici.

| Date, heure | Étape | Ce qui était branché ; ce qu'on a vu, entendu, senti | Cause comprise | Reprise (date) |
|---|---|---|---|---|
| | | | | |

## Étape −1 : observation d'usage

**Date :**

**Conditions :** hotte en usage normal, fermée ; aucun outil.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | depuis la veille, un appui sur une vitesse sans passer par « marche » | — | rien, un bip, ou un démarrage | | |
| 2 | la lumière s'allume-t-elle sans « marche » ? | — | oui ou non | | |
| 3 | « marche » quand le moteur tourne | — | arrêt immédiat, **marche prolongée** (arrêt différé natif, comme les 15 min de la CDA : durée ?), ou rien | | |
| 4 | nouvel appui sur la vitesse active | — | | | |
| 5 | l'état « armé » après « marche » s'éteint-il seul ? Au bout de combien de temps ? | — | | | |
| 6 | **voyants** : lesquels s'allument, et quand ? Fixes, clignotants, intensité ? Voyant de « marche » ? | — | | | |
| 7 | **bips** : lesquels, et quand ? (le buzzer est sur la carte : chaque bip prouve que la carte a reçu quelque chose) | — | | | |
| 8a | appui long (3 s) sur la lumière : effet caché ? comment l'annuler ? | — | verrouillage, rappel de filtre, arrêt différé, ou rien | | |
| 8b | appui long (3 s) sur « marche » | — | idem | | |
| 8c | appui long (3 s) sur la vitesse 1 | — | idem | | |
| 8d | appui long (3 s) sur la vitesse 2 | — | idem | | |
| 8e | appui long (3 s) sur la vitesse 3 | — | idem | | |
| 9 | **reprise après coupure** : vitesse 2 et lumière allumée, fiche débranchée 10 s, rebranchée : quel état revient ? | — | | | |
| 10 | **délai prudent entre deux changements de vitesse**, d'après ce qu'on entend des relais | — | 3 s par défaut | | fixe la valeur de l'étape 7 |

**Sortie :**
- [ ] le tableau des comportements est rempli.

## Étape 0 : photos, inventaire et cheminements

**Date :**

**Conditions :** hotte débranchée depuis 5 min (débranchée à : ____).

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1a | IC2 : marquage, nombre de broches | photo macro | SOP-14 sans marquage (comme la CDA) | | |
| 1b | IC3 : marquage, nombre de broches | photo macro | `ULN2003` | | |
| 1c | IC4 : marquage, nombre de broches | photo macro | abaisseur, SOP-8 | | |
| 1d | relais : marquage | photo macro | 3 relais L, M, H | | |
| 2a | `CN3` : fiche collée ? | visuel | | | si oui : décoller avec un outil en plastique, **sans tirer sur les fils** |
| 2b | pas de `CN3`, entre les broches extrêmes | règle ou pied à coulisse en plastique | 5,0 mm : JST XH ; 4,0 mm : JST PH | | connectique de l'adaptateur |
| 3 | accès : comment retirer le filtre, où est le boîtier, comment s'ouvre son couvercle | — | | | |
| 4 | trajet des fils du condensateur et du connecteur blanc, par rapport à `CN3` et au passage de sortie | photo | | | |
| 5a | câble du panneau : longueur libre | mètre | | | |
| 5b | comment sa fiche peut sortir du boîtier ; ruban et colliers à refaire à l'identique | photo | | | placement de l'adaptateur (WIRING.md §2) |
| 5c | connecteur intermédiaire sur le câble du panneau ? | visuel | | | |
| 5d | le panneau se démonte-t-il, sans forcer ? Si oui, référence de la puce tactile | visuel | | | décide si C est possible |
| 6a | cheminement possible du câble de sortie jusqu'au boîtier de mesure | photo | par exemple le long du câble du panneau vers l'avant, ou au bord du filtre sans pincement | | |
| 6b | longueur nécessaire du câble de sortie | mètre | | | |
| 6c | emplacement du boîtier de mesure | — | à côté de la hotte, visible, hors de l'aplomb de la plaque de cuisson | | |

**Photos :**
- [ ] Photo IC2, IC3, IC4 (macro) : `docs/photos/etape0-ic.jpg`
- [ ] Photo relais : `docs/photos/etape0-relais.jpg`
- [ ] Photo `CN3` de près : `docs/photos/etape0-cn3.jpg`
- [ ] Photo fils du condensateur et connecteur blanc, avant toute manipulation : `docs/photos/etape0-fils.jpg`
- [ ] Photo câble du panneau, sa sortie, ruban et colliers : `docs/photos/etape0-cable.jpg`
- [ ] Photo cheminement choisi et emplacement du boîtier de mesure : `docs/photos/etape0-cheminement.jpg`

**Sortie :**
- [ ] le pas de `CN3` et les longueurs sont connus : connectique ☐ XH ☐ PH ;
- [ ] on sait si le panneau s'ouvre : ☐ oui (C possible) ☐ non, ou résiné ;
- [ ] le cheminement est choisi, avec une photo.

## Étape 1 : test d'isolation

Procédure pas à pas : [SECURITE.md](SECURITE.md), « Procédure : étape 1 ».

**Date :**

**Conditions :** hotte débranchée depuis 5 min ; broches rondes de la fiche
reliées par le fil à pinces crocodile ; `CN3` piqué par l'arrière avec
l'aiguille ; aucune partie métallique des pointes touchée.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| a1 | broches reliées vers `CN3 −` | le plus élevé (20 MΩ ou plus) | OL après 10 s (une valeur qui monte vers OL est normale) | | valeur stable : **ARRÊT** |
| a2 | `CN3 −` vers broches reliées (cordons inversés) | idem | OL | | idem |
| a3 | broches reliées vers `D` | idem | OL | | idem |
| a4 | `D` vers broches reliées | idem | OL | | idem |
| a5 | broches reliées vers `+` | idem | OL | | idem |
| a6 | `+` vers broches reliées | idem | OL | | idem |
| b | contact de terre de la fiche vers `CN3 −` | le plus élevé | OL : secondaire flottant ; environ 0 Ω : relié à la terre (pas un défaut, on le note) | | kΩ à MΩ : **ARRÊT** et analyse |
| c0 | cordons court-circuités | 200 Ω | valeur à soustraire | | |
| c | contact de terre de la fiche vers la vis de terre de la carcasse (ou une vis nue) | 200 Ω | moins de 1 Ω, après soustraction | | sinon **ARRÊT** : défaut de terre |

**Sortie :** ☐ feu vert pour construire l'adaptateur ☐ **ARRÊT** (raison : ____).

## Étape 2 : adaptateur et boîtier de mesure

**Date :**

**Conditions :** hotte débranchée depuis 5 min (on ouvre le boîtier).

| Construction | Relevé |
|---|---|
| connectique (XH ou PH, références) | |
| fiche et embase XH 4 broches du câble de sortie vers le boîtier de mesure (références) | |
| fil du câble de sortie (référence, tension, température) | |
| gaine (type, nombre d'épaisseurs) | |
| longueur du câble de sortie | |

Contrôles au multimètre **avant la pose** (WIRING.md §12, mêmes numéros).
Adaptateur et câble : hotte débranchée, adaptateur pas encore posé, fiche XH 4 br.
du câble de sortie enfichée sur le boîtier de mesure (sauf ligne 10).

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | continuité côté carte vers côté panneau : `−`, `D` (`J1` fermé), `+` | 200 Ω | environ 0 Ω chacune | | sinon on corrige avant la pose |
| 2 | court-circuit `−`/`D`, `−`/`+`, `D`/`+` | le plus élevé | OL (100 à 200 kΩ entre `D` et `−` si l'étage d'écoute est relié) | | court-circuit : on corrige avant la pose |
| 3 | `TP−` vers la broche « − » (fil noir) de la fiche qui ira sur `CN3` | 200 Ω | environ 0 Ω | | sinon on corrige avant la pose |
| 4 | `TP+` vers la broche « + » (fil rouge) | 200 Ω | environ 0 Ω | | idem |
| 5 | `TP_Dc` vers la broche `D` de la fiche ; `TP_Dp` vers la broche `D` de l'embase | 200 Ω | environ 0 Ω | | idem |
| 6 | `J1` fermé : `TP_Dp` vers `TP_Dc` | 200 Ω | environ 0 Ω | | idem |
| 7 | `J2` ouvert : `TP+` vers la broche `5V` des barrettes | le plus élevé | OL | | idem |
| 8 | ordre des contacts de la fiche XH 3 br. identique à celui de la fiche du panneau (noir, blanc, rouge) | visuel | détrompeur du même côté | | idem |
| 9 | fiche XH 4 br. : contacts dans l'ordre `−`, `+`, `D_panneau`, `D_carte` (broches 1 à 4), comme l'embase du boîtier | visuel | détrompeur : elle n'entre que dans un sens | | idem |
| 10 | fiche XH 4 br. retirée : `TP−` vers la broche « − » de la fiche qui ira sur `CN3`, puis `TP_Dp` vers la broche `D` de l'embase de l'adaptateur | le plus élevé | OL (boîtier détaché) ; fiche remise ensuite | | idem |

Boîtier de mesure : sonde retirée des barrettes.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 11 | `TP−` vers la broche `GND` des barrettes | 200 Ω | environ 0 Ω | | sinon on corrige avant la pose |
| 12 | broche `3V3` vers la broche 6 des barrettes | 20 kΩ | environ 10 kΩ (collecteur de Q1) | | idem |
| 13 | `TP_Dp` vers la broche 6 des barrettes | le plus élevé | plus de 100 kΩ dans les deux sens (règle 10) | | moins : liaison directe du bus vers une GPIO, on corrige avant la pose |
| 14 | collecteur de Q1 sur la broche 6 ; le 4,7k de base de Q2 sur la broche 7 | visuel | GPIO6 et GPIO7 non interverties | | sinon on corrige avant la pose |
| 15 | brochage de Q1 et Q2 | visuel | conforme à la fiche du transistor (BC547 = C-B-E, 2N3904 = E-B-C) | | idem |
| 16 | coque de l'USB-C isolée ; USB-C accessible | visuel | | | idem |
| 17 | contrôle électrique du module (WIRING.md §9) | 20 V continu | broche 6 à environ 3,3 V, broche 7 à 0 V | | sérigraphie mal lue : ne rien souder |

**Pose :**
- [ ] 1. fiche de l'adaptateur sur `CN3`, fiche du panneau sur l'embase de l'adaptateur ;
- [ ] 2. câble de sortie selon la règle 5 et le cheminement choisi à l'étape 0 ; sa fiche XH 4 br. enfichée sur le boîtier de mesure ;
- [ ] 3. couvercle fermé, en vérifiant qu'il ne pince rien ;
- [ ] 4. filtre remis, en vérifiant qu'il ne pince rien.

**Photos :**
- [ ] Photo adaptateur posé sur `CN3` : `docs/photos/etape2-adaptateur.jpg`
- [ ] Photo câble de sortie au passage de sortie : `docs/photos/etape2-cable.jpg`
- [ ] Photo boîtier de mesure en place : `docs/photos/etape2-boitier.jpg`

### Étape 2b : isolation revérifiée depuis les points de test

Procédure pas à pas : [SECURITE.md](SECURITE.md), « Procédure : étape 2b ».

**Date :**

**Conditions :** hotte débranchée, couvercle fermé, filtre remis ; fiche XH 4 br.
du câble de sortie enfichée sur le boîtier de mesure (retirée, tout lirait OL
sans rien prouver) ; broches de la fiche reliées.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | broches reliées vers `TP−` | le plus élevé (20 MΩ ou plus) | OL après 10 s | | sinon **ARRÊT** |
| 2 | `TP−` vers broches reliées | idem | OL | | idem |
| 3 | broches reliées vers `TP+` | idem | OL | | idem |
| 4 | `TP+` vers broches reliées | idem | OL | | idem |
| 5 | broches reliées vers `TP_Dp` | idem | OL | | idem |
| 6 | `TP_Dp` vers broches reliées | idem | OL | | idem |
| 7 | broches reliées vers `TP_Dc` | idem | OL | | idem |
| 8 | `TP_Dc` vers broches reliées | idem | OL | | idem |

**Où est le pull-up de `D` ?** `J1` ouvert (hotte débranchée, filtre retiré, sans
ouvrir le boîtier), chaque mesure dans les deux sens.

**Date :**

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| p1 | `TP_Dp` vers `TP+` | 20 kΩ ou automatique | quelques kΩ : pull-up côté panneau | | |
| p2 | `TP_Dp` vers `TP−` | idem | l'étage d'écoute y ajoute 100 à 200 kΩ : n'indique rien | | |
| p3 | `TP_Dc` vers `TP+` | idem | quelques kΩ : pull-up côté carte | | |
| p4 | `TP_Dc` vers `TP−` | idem | | | |
| p5 | `J1` refermé, filtre remis | visuel | | | |

Conclusion : pull-up côté ☐ carte ☐ panneau ☐ aucun trouvé ; valeur environ
____ kΩ (sert à l'étape 4 et à la voie 2 si B).

**Nouveaux passages de la mesure 1** (après chaque ouverture du boîtier, et à la
sortie de l'étape 4) :

| Date | Pourquoi le boîtier a été ouvert | `TP−`, `TP+`, `TP_Dp`, `TP_Dc` : OL dans les deux sens ? | Décision |
|---|---|---|---|
| | | | |

### Étape 2c : essai à vide

**Date :**

**Conditions :** hotte branchée ; différentiel testé (tableau plus haut).

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | touche lumière | — | exactement comme avant (étape −1) : lumière, voyant, bip | | |
| 2 | touche « marche » | — | comme avant | | |
| 3 | vitesse 1 | — | comme avant | | |
| 4 | vitesse 2 | — | comme avant | | |
| 5 | vitesse 3 | — | comme avant | | |

**Sortie :**
- [ ] les étapes 2b et 2c sont concluantes ;
- [ ] les photos du montage sont dans ce journal (étape 2).

## Étape 3 : mesures au multimètre

Toutes les pinces se posent et se déplacent **hotte débranchée** (règle 4), avec
des grippe-fils sur le boîtier de mesure. Sous tension, on ne fait que lire
l'afficheur et toucher les touches du panneau.

### 3a. Fuite en charge, avant tout Mac ou analyseur

Procédure pas à pas : [SECURITE.md](SECURITE.md), « Procédure : étape 3a ».

**Date :**

**Conditions :** fiche XH 4 br. du câble de sortie enfichée sur le boîtier ; COM
sur `TP−`, l'autre pince sur la vis de terre de la carcasse, résistance de
10 kΩ / 2 W en parallèle entre les deux pinces ; différentiel testé.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | tension aux bornes de 10 kΩ / 2 W, lue dans les 5 s après le branchement | tension alternative | 3 V au plus, soit 0,3 mA au plus | ____ V, soit ____ mA (tension / 10 kΩ) | au-delà de 5 V (0,5 mA), ou différentiel déclenché : **ARRÊT** ; entre 3 et 5 V : on le note, et on en rediscute avant de relier un Mac ou l'analyseur |

### 3b. Tensions

**Date :**

**Conditions :** hotte débranchée pour poser les pinces : COM sur `TP−`, V sur
`TP+` (lignes 1 à 7), puis V sur `TP_Dp` (lignes 8 à 18).

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | `+` en veille | 20 V continu | environ 5 V ou environ 12 V, positif | | négatif : adaptateur inversé, **ARRÊT** |
| 2 | `+` après « marche » | 20 V continu | stable | | |
| 3 | `+` lumière allumée | 20 V continu | stable | | |
| 4 | `+` vitesse 1 | 20 V continu | stable | | |
| 5 | `+` vitesse 2 | 20 V continu | stable | | |
| 6 | `+` vitesse 3 | 20 V continu | stable | | |
| 7 | `+` pendant un appui bref (1 à 2 s) | 20 V continu | stable | | |
| 8 | `D` en veille (et fréquence, rapport cyclique si possible) | 20 V continu ; Hz ; % | voir la lecture ci-dessous | | |
| 9 | `D` après « marche » | idem | | | |
| 10 | `D` lumière allumée | idem | | | |
| 11 | `D` vitesse 1 | idem | | | |
| 12 | `D` vitesse 2 | idem | | | |
| 13 | `D` vitesse 3 | idem | | | |
| 14 | `D` pendant un appui bref sur la lumière | idem | | | |
| 15 | `D` pendant un appui bref sur « marche » | idem | | | |
| 16 | `D` pendant un appui bref sur la vitesse 1 | idem | | | |
| 17 | `D` pendant un appui bref sur la vitesse 2 | idem | | | |
| 18 | `D` pendant un appui bref sur la vitesse 3 | idem | | | |

Lecture (cocher ce qui s'applique) :

| Observation | Lecture probable | ☐ |
|---|---|---|
| `V(+,−)` négatif | adaptateur inversé : **ARRÊT** | ☐ |
| `+` ≈ 5 V | sortie de l'abaisseur IC4 : l'étape 4 est possible | ☐ |
| `+` ≈ 12 V | rail du flyback : on saute l'étape 4 | ☐ |
| `D` stable à `+`, avec des creux seulement pendant les appuis | trafic sur événement, repos haut : plutôt A | ☐ |
| `D` entre 40 et 60 % de `+` en permanence | trames continues : plutôt B | ☐ |
| `D` ≈ 0 V au repos, avec de l'activité | UART inversé, repos bas (type Krona) | ☐ |
| une tension continue stable, différente pour chaque touche maintenue | échelle de résistances | ☐ |

### 3c. Test du pull-up

**Date :**

**Conditions :** hotte débranchée pour poser 47 kΩ entre `TP_Dp` et `TP−` sur le
boîtier de mesure ; branchée pour la lecture ; débranchée pour la retirer.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | `D` au repos, sans la résistance (ligne 8 de 3b) | 20 V continu | | | |
| 2 | `D` au repos, avec 47 kΩ entre `TP_Dp` et `TP−` | 20 V continu | sur 5 V : chute d'environ 0,45 V (pull-up d'environ 4,7 kΩ) ; environ 0,9 V (pull-up d'environ 10 kΩ) ; aucune chute (push-pull, ou pull-up très fort) | | |
| 3 | chute = ligne 1 − ligne 2 ; pull-up estimé ≈ 47 kΩ × chute / ligne 2 | — | | | |
| 4 | résistance retirée, hotte débranchée | visuel | | | |

**Sortie :**
- [ ] `+` connu : ____ V ; ☐ étape 4 possible (`+` ≈ 5 V) ☐ étape 4 sautée (`+` ≈ 12 V) ;
- [ ] repos connu : ☐ haut ☐ bas ;
- [ ] première idée du trafic : ____.

## Étape 4 : panneau seul, hotte débranchée

Seulement si `+` ≈ 5 V. **Il n'y a aucun secteur** : la sonde peut être sur
l'USB du Mac. ☐ Étape sautée (`+` ≈ 12 V).

**Date :**

**Conditions :** hotte débranchée depuis 5 min, boîtier ouvert.

Capture par l'USB (lignes 5 à 10), un fichier par ligne, 30 s chacun :
`python3 tools/serie_enregistre.py <port> <scénario> "capture tout" --duree 30`
(port : `ls /dev/cu.usbmodem*` ; ni `pio device monitor` ni autre outil sur ce
port), qui écrit `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl` ; résumé à suivre avec
`tail -f logs/live.log`.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | fiche de l'adaptateur retirée de `CN3` (sinon le 5 V de la sonde alimenterait aussi la carte) | visuel | | | |
| 2 | polarité sans le panneau : fiche du panneau retirée de l'embase, `J2` fermé, USB de la sonde branché ; `TP+` par rapport à `TP−` | 20 V continu | de +4,5 à +5,3 V, positif (un port USB peut monter à 5,25 V) | | sinon **ARRÊT** ; puis débrancher l'USB |
| 3 | USB débranché : en option, 10 kΩ entre `TP_Dp` et `TP+`, si le pull-up est côté carte (étape 2b) | visuel | | ☐ posée ☐ non | |
| 4 | fiche du panneau sur l'embase, puis USB branché, sans toucher les touches (le circuit tactile se calibre) | — | | | |
| 5 | le panneau émet-il tout seul ? Comment ? (`etape4-repos`, sans toucher) | fichier de capture | | | |
| 6 | voyants quand on touche la lumière (`etape4-lumiere`) | — | s'allument seuls : le panneau pilote ses voyants ; restent éteints : la carte les pilote (cas favorable) | | |
| 7 | voyants quand on touche « marche » (`etape4-marche`) | — | idem | | |
| 8 | voyants quand on touche la vitesse 1 (`etape4-v1`) | — | idem | | |
| 9 | voyants quand on touche la vitesse 2 (`etape4-v2`) | — | idem | | |
| 10 | voyants quand on touche la vitesse 3 (`etape4-v3`) | — | idem | | |

**Sortie, dans cet ordre :**
- [ ] 1. débrancher l'USB ;
- [ ] 2. ouvrir `J2` ; retirer la 10 kΩ si elle a été posée ;
- [ ] 3. remettre la fiche de l'adaptateur sur `CN3` ;
- [ ] 4. fermer le couvercle et remettre le filtre, sans rien pincer ;
- [ ] 5. refaire l'étape 2b, qui se mesure couvercle fermé et filtre remis (ligne
  ajoutée au tableau des nouveaux passages).

Le couvercle et le filtre passent avant l'étape 2b : elle se mesure ainsi, et
repère un câble pincé à la fermeture.

**Photos :**
- [ ] Photo montage du panneau seul : `docs/photos/etape4-montage.jpg`
- [ ] Photo voyants pendant un appui : `docs/photos/etape4-voyants.jpg`

## Étape 5 : capture en place

**Date :**

**Conditions :**
- [ ] banc validé (tâches 16 et 21 : critères 1, 2 sans l'analyseur, 3, 4 et 6 dans `BANC.md`) ;
- [ ] batterie vérifiée (30 min) : modèle et essai notés dans « Installation et instruments » ;
- [ ] contrôles de la règle 7 (étapes 1, 2b, 3a) et test du différentiel faits ;
- [ ] sonde sur son boîtier de mesure, fiche XH 4 br. du câble de sortie enfichée, **sur batterie** (modèle vérifié), `J2` ouvert ;
- [ ] session réseau par Wi-Fi ; RSSI au boîtier de mesure : ____ dBm.

Un fichier par scénario, 10 s chacun, dans cet ordre :
`python3 tools/hotte_udp.py enregistre hotte-sonde.local <scénario> "capture tout" --duree 10`,
qui écrit `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl` ; résumé à suivre avec
`tail -f logs/live.log`. La commande `capture tout` remet le mode `tout` : la
sonde garde en NVS le dernier mode choisi, peut-être `changements`.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | veille (`veille`) | — | un fichier de 10 s | | |
| 2 | marche (`marche`) | — | idem | | |
| 3 | lumière allumée (`lumiere-on`) | — | idem | | |
| 4 | lumière éteinte (`lumiere-off`) | — | idem | | |
| 5a | vitesse 1 (`v1`) | — | idem | | |
| 5b | vitesse 2 (`v2`) | — | idem | | |
| 5c | vitesse 3 (`v3`) | — | idem | | |
| 6 | arrêt (`arret`) | — | idem | | |
| 7 | chaque bip entendu (`bip-<touche>`) | — | idem | | |
| 8 | panneau déconnecté (`panneau-off`) : **hotte débranchée**, filtre retiré (si cela ouvre le boîtier : 5 min d'attente), fiche du panneau retirée de l'embase ; couvercle fermé et filtre remis (règle 5), étape 2b si le boîtier a été ouvert ; puis hotte rebranchée | — | on voit si la carte émet seule | | |
| 8b | après `panneau-off` : **hotte débranchée**, filtre retiré (si cela ouvre le boîtier : 5 min d'attente), fiche du panneau remise sur l'embase ; couvercle fermé et filtre remis (règle 5), étape 2b si le boîtier a été ouvert | visuel | à la remise sous tension, le panneau marche comme avant (étape 2c) | | |
| 9 | compteurs de la sonde après la série : `debord`, `lignes_perdues`, `sautes` | — | 0 partout | | sinon on refait le scénario |

Captures versées dans `captures/`, une par scénario (l'outil refuse un fichier
qui contient une clé) :
- [ ] `python3 tools/verse_capture.py logs/<fichier>.jsonl etape5-<scénario>`

**Photos :**
- [ ] Photo sonde sur le boîtier de mesure, sur batterie : `docs/photos/etape5-sonde.jpg`

**Sortie :**
- [ ] un fichier par scénario, versé dans `captures/` ;
- [ ] les compteurs de la sonde sans perte ni débordement ;
- [ ] fin de séance : hotte débranchée, sonde et batterie retirées avant toute cuisson (règle 13).

## Étape 5b : validation croisée à l'analyseur

Obligatoire avant l'étape 6.

**Date :**

**Conditions :**
- [ ] différentiel testé avant de poser l'analyseur (tableau plus haut), puis hotte débranchée ;
- [ ] hotte débranchée pour poser les pinces : masse de l'analyseur **d'abord**, sur `TP−` ;
- [ ] voie 1 sur la sortie de l'étage d'écoute, le même nœud que GPIO6 (3,3 V) ;
- [ ] voie 2 sur `TP_Dp` à travers le diviseur (WIRING.md §8) : ☐ 47k/68k (`+` = 5 V) ☐ 100k/33k (`+` = 12 V) ;
- [ ] Mac sur batterie, sans aucun autre câble (règle 8), analyseur **en USB direct** (pas d'isolateur ADuM3160 : limité au Full Speed, il bride l'échantillonnage) ;
- [ ] USB de l'analyseur branché au Mac **hotte encore débranchée**, après les pinces (règle 4) ; puis hotte branchée ;
- [ ] PulseView, fréquence d'échantillonnage : ____ ;
- [ ] la sonde enregistre en même temps, par le Wi-Fi : `python3 tools/hotte_udp.py enregistre hotte-sonde.local etape5b-<scénario> "capture tout" --duree 10` (fichier : ____).

L'analyseur FX2 est numérique : il ne mesure pas de tension. Le niveau bas de
`D` (lignes 4 et 5) se juge donc fonctionnellement ; une mesure chiffrée, à
l'oscilloscope, est optionnelle.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | voie 1 : durées de la sonde contre celles de l'analyseur | — | écart de ±2 µs au plus, plus le quantum de 1 µs | | |
| 2 | voie 1 : trames décodées par la sonde et par l'analyseur | — | identiques | | |
| 3 | écarts entre réceptions (`t_us`) contre l'analyseur | — | quelques dizaines de µs | | |
| 4 | niveau bas de `D` pendant les trames du panneau | — | jugement fonctionnel : lignes 1 et 2 concluantes, capture propre, chaque impulsion de la voie 2 retrouvée sur la voie 1 ; si mesuré : sous 0,6 V | | sinon : relever le seuil (`R_be`, WIRING.md §5), puis refaire l'étape 5 |
| 5 | niveau bas de `D` pendant les trames de la carte | — | idem | | idem |
| 6 | voie 2 : temps de montée de `D` | — | c'est la tolérance de la voie 2 | | |

Instrument qui a donné les niveaux des lignes 4 et 5 : ☐ jugement fonctionnel
(analyseur) ☐ oscilloscope : ____.

Captures de la sonde versées dans `captures/` :
- [ ] `python3 tools/verse_capture.py logs/<fichier>.jsonl etape5b-<scénario>`

**Fin de séance, dans cet ordre (règle 8) :**
- [ ] 1. débrancher la hotte ;
- [ ] 2. retirer l'analyseur (USB et pinces) ;
- [ ] 3. ensuite seulement, rebrancher le chargeur du Mac.

**Photos :**
- [ ] Photo montage de l'analyseur : `docs/photos/etape5b-analyseur.jpg`
- [ ] Photo capture PulseView (copie d'écran) : `docs/photos/etape5b-pulseview.png`

**Sortie :**
- [ ] critères de la voie 1 remplis, niveaux bas jugés (lignes 4 et 5) : étape 6 permise ;
- [ ] le cas échéant : seuil relevé (`R_be` = ____) et étape 5 refaite.

## Étape 6 : décodage et choix de l'architecture

**Date :**

**Conditions :** captures des étapes 5 et 5b ; `tools/analyse.py`.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | seuil de silence choisi après coup (`analyse.py trames --silence`) | µs | | | |
| 2 | `analyse.py auto` : hypothèse retenue et taux d'erreur | — | UART (débits bas de 300 à 1 200 bauds compris, repos bas compris), distance d'impulsion (type WTC6534), ou Manchester | | |
| 3 | `analyse.py diff` : bits de chaque touche et de chaque état | — | | | |
| 4 | test 1 : après le relâchement, la trame revient-elle à « aucune touche » ? | — | oui : la carte détient l'état de la hotte | | |
| 5 | test 2 : la carte répond-elle après chaque trame ? Avec quel délai ? | — | | | |
| 6 | test 3 : y a-t-il des silences plus longs qu'une trame, où l'on pourrait injecter ? | — | | | |
| 7 | test 4 : la carte émet-elle son état ou un paquet de voyants ? | — | | | |

### Décision de l'étape 6

Entrées (en croisant avec l'étape 4) :

| Entrée | Source | Relevé |
|---|---|---|
| voyants du panneau seul quand on touche | étape 4 | ☐ s'allument seuls ☐ restent éteints ☐ étape sautée : seul le test 4 renseigne |
| état ou paquet de voyants émis par la carte | test 4 | ☐ oui ☐ non |
| `D` en drain ouvert ou en push-pull | étapes 3c et 5b | |
| silences exploitables | test 3 | |
| trafic continu, ou réponse attendue dans un délai court | étape 3b, tests 2 et 3 | |
| le panneau s'ouvre ; il est résiné | étape 0 | |
| échelle de résistances au lieu d'un bus de données | étape 3b | |

Règle de choix (§4 de la spec) :

| Résultat de la reconnaissance | Choix | ☐ |
|---|---|---|
| **la carte pilote les voyants** (voyants éteints à l'étape 4, ou paquet de voyants émis par la carte), `D` en drain ouvert avec des silences exploitables | **A** | ☐ |
| **la carte pilote les voyants**, mais trafic continu, réponse attendue dans un délai court, ou `D` en push-pull | **B** | ☐ |
| **le panneau pilote ses voyants** (il les allume seul et la carte n'émet aucun état), ou `D` unidirectionnel sans état renvoyé | **C**, si le panneau s'ouvre | ☐ |
| le panneau pilote ses voyants **et** il est résiné | pas de solution propre qui respecte la contrainte des voyants : on en rediscute avec Majid, faits en main | ☐ |
| échelle de résistances au lieu d'un bus de données | **A**, avec une résistance commutée vers `−`, si la carte pilote les voyants | ☐ |

**Choix retenu :** ☐ A ☐ B ☐ C ☐ aucune (on en rediscute avec Majid)

**Argumentaire** (critère 3 du §1) :

> (à rédiger : faits relevés, règle appliquée, ce qui reste incertain)

**Avenant d'injection**, écrit quel que soit le choix, chiffré d'après
`PROTOCOL.md`, et relu par Majid avant l'étape 7 :

| Valeur fixée par l'avenant | Valeur |
|---|---|
| syntaxe de `injecte` | |
| durée basse maximale par impulsion (`bas_max_us`) | |
| durée totale maximale par trame (`total_max_us`) | |
| silence minimal avant d'émettre (`silence_min_us`) | |
| attente maximale (`attente_max_ms`) | |
| délai minimal entre deux changements d'état du moteur (`delai_min_ms`, d'après l'étape −1, ligne 10) | |
| pour B : le mode relais ; pour C : le prototype PhotoMOS | |

**Sortie :**
- [ ] `docs/PROTOCOL.md` est rédigé ;
- [ ] le choix est argumenté ci-dessus ;
- [ ] l'avenant est écrit (lien : ____) ;
- [ ] l'avenant est relu par Majid (date : ____).

## Étape 7 : premier essai d'injection

**En présence de Majid** (règle 11), sous tension, sonde sur batterie. Majid se
tient à portée de la fiche, voit le panneau, et débranche au moindre doute.

**Date :**

**Conditions :** choix ☐ A ☐ B ☐ C ; avenant relu ; ☐ critère 5 du banc validé
(collision détectée et émission arrêtée, `BANC.md`).

Préparation (choix A) :

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | différentiel testé, hotte dans son état précédent (tableau plus haut), puis hotte débranchée | — | lumière éteinte par T | | sinon **ARRÊT** |
| 2 | hotte débranchée : étage d'injection monté (WIRING.md §6), contrôles avant pose faits ; voie 2 de l'analyseur sur `TP_Dp` par le diviseur, masse d'abord sur `TP−` | visuel | | | |
| 3 | USB branché, `injection monte 1` envoyé | — | réponse `ok` | | |
| 4 | charger les valeurs de l'avenant : `injection regle <nom> <valeur>` (USB, hotte débranchée), une commande par valeur | — | chacune acceptée | | refusée (hors bornes) : l'avenant sort de l'enveloppe de sécurité du firmware, on le revoit |
| 5 | `config` : valeurs de l'avenant chargées | — | égales à celles de l'avenant | | sinon on corrige avant d'aller plus loin |
| 6 | USB débranché, sonde passée sur batterie | visuel | | | |
| 7 | contrôles de la règle 7 à jour (étape 2b refaite si le boîtier a été ouvert) ; Mac sur batterie, sans aucun autre câble | — | | | |
| 8 | hotte branchée ; `injection on` envoyé par `hotte_udp.py`, depuis un terminal, après la confirmation que Majid tape lui-même : « Majid devant la hotte » (jamais un script ni un agent) | — | injection armée | | |
| 9 | niveau bas obtenu par l'injection, pendant les premiers essais | — | jugement fonctionnel : la carte réagit (bip), et l'impulsion se retrouve sur la voie 2 de l'analyseur et dans `relu_us` de l'événement `injection` ; si mesuré (oscilloscope) : sous 0,8 V | | sinon : on arrête, `injection off`, hotte débranchée, puis 470 Ω remplacées par 220 Ω |

Instrument qui a donné le niveau de la ligne 9 : ☐ jugement fonctionnel
☐ oscilloscope : ____.

Analyseur : pinces puis USB posés hotte débranchée (règle 4) ; fin de séance
comme à l'étape 5b : hotte débranchée, analyseur retiré, puis seulement le
chargeur du Mac (règle 8).

Essais, dans cet ordre, **20 fois sur 20 chacun**. Chaque essai se compte par son
événement `injection` (même `id` que la commande, résultat `ok`). Le firmware
impose le délai minimal entre deux changements d'état du moteur. Une case par
essai : « Essais réussis » si l'on entend le bip, voit l'effet (lumière ou
moteur) et retrouve la trame sur le bus ; « Voyants qui suivent » si **le voyant
du panneau change** comme il faut.

| Action | Essais réussis | Voyants qui suivent | Total |
|---|---|---|---|
| touche lumière | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| vitesse 1 | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| vitesse 2 | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| vitesse 3 | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| « marche » puis une vitesse | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| arrêt | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |

Fichiers d'enregistrement des essais : ____.

**Si le choix est B** (on suit l'avenant) :
- [ ] voie 2 montée (WIRING.md §7), hotte débranchée ;
- [ ] hotte débranchée, filtre retiré (sans ouvrir le boîtier) : `J1` ouvert ; filtre
  remis avant de rebrancher (règle 5) ;
- [ ] mode relais transparent : le panneau marche à travers la sonde ;
- [ ] puis mêmes injections (tableau ci-dessus) et même critère.

**Si le choix est C** (on suit l'avenant) :
- [ ] prototype PhotoMOS sur une touche ; le critère porte sur la touche prototypée.

**Photos :**
- [ ] Photo étage d'injection monté : `docs/photos/etape7-injection.jpg`

**Sortie :**
- [ ] 20 fois sur 20 pour chaque action, et les voyants du panneau suivent ;
- [ ] aucun comportement anormal de la carte ;
- [ ] le vrai panneau fonctionne toujours après l'essai (chaque touche) ;
- [ ] `injection off` ; entre deux sessions, sonde et batterie retirées, `J1` fermé
  (règle 13).

## Critères de sortie du sous-projet

- [ ] 1. **Protocole documenté** dans `docs/PROTOCOL.md` : couche physique
  (tension de `+`, niveau de repos, niveau bas de chaque émetteur, jugé à
  l'étape 5b, drain ouvert ou push-pull, pull-up et son côté), codage, trame de
  chaque action, qui détient l'état et qui pilote les voyants ; **validé par la
  capture croisée de l'étape 5b**.
- [ ] 2. **Injection prouvée** : lumière, chaque vitesse, « marche » puis vitesse,
  arrêt, 20 fois sur 20 chacune, et les voyants du panneau suivent (avec C : la
  touche prototypée).
- [ ] 3. **Architecture du produit choisie** et argumentée ci-dessus, ou constat
  argumenté qu'aucune ne respecte la contrainte des voyants.
