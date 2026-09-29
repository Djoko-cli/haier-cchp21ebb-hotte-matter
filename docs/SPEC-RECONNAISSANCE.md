# Spec : sous-projet 1, reconnaissance de la ligne D

**Projet :** piloter la hotte Haier CCHP21EBB (90 cm) depuis Apple Home, en Matter
sur Thread, avec un ESP32-C6.

**Statut :** conception validée section par section avec Majid les 24 et
25/09/2026. Révision 2, après une relecture adverse (sécurité 230 V,
électronique et firmware, cohérence avec le protocole compagnon). Spec à relire.
Ajouts du 28/09, approuvés par Majid : étape −1b, scénarios 7b et 7c de
l'étape 5, tests 5 et 6 de l'étape 6, séquence d'arrêt de l'étape 7, mesure
M1 (§6).

**Documents liés :**
- [BRIEF-RECHERCHE.md](BRIEF-RECHERCHE.md) : faits, niveaux de preuve et 57 sources.
- [SECURITE.md](SECURITE.md) : les règles du §5 en fiche, et les tests
  d'isolation et de fuite pas à pas.
- [RECONNAISSANCE.md](RECONNAISSANCE.md) : le journal des étapes −1 à 7 (§6).
- [WIRING.md](WIRING.md) : le matériel d'interface du §7 en détail.
- [BANC.md](BANC.md) : la procédure et les résultats du banc de validation (§10).
- [PROTOCOLE-JSON.md](PROTOCOLE-JSON.md) : le profil « hotte » du protocole
  compagnon (§8.5).
- Le protocole compagnon de la ScreenBar, dans sa version de référence, figée
  au commit `c58a506` (§8.6) :
  [benq-screenbar-halo-matter/docs/PROTOCOLE-JSON.md](https://github.com/Djoko-cli/benq-screenbar-halo-matter/blob/c58a506/docs/PROTOCOLE-JSON.md).

---

## 0. Le projet en trois sous-projets

| # | Sous-projet | Contenu | Quand |
|---|---|---|---|
| **1** | **Reconnaissance** (cette spec) | comprendre la ligne `D` entre le panneau tactile et la carte de puissance, choisir l'architecture, prouver qu'on peut commander la hotte par cette ligne | maintenant |
| 2 | Produit | interface matérielle définitive, firmware Matter (ventilateur 3 vitesses, lumière, arrêt différé), choix de la carte C6 (8 Mo de flash pour l'OTA ? antenne ?), essai d'alimentation par `CN3 +`, lecture d'état annexe si besoin | après le 1, avec sa propre spec |
| 3 | Bibliothèque commune et app compagnon multi-appareils | extraire le protocole compagnon commun à la ScreenBar et à la hotte (firmware et package Swift) ; une app qui choisit sa vue d'après `hello` | plus tard |

## 1. Objectif et critères de sortie

**Objectif.** Connaître la ligne `D` assez bien pour choisir l'architecture du
produit, et prouver qu'on commande la hotte par cette ligne **sans jamais
toucher au secteur**.

**Le sous-projet est terminé quand les trois critères sont remplis :**

1. **Protocole documenté** dans `docs/PROTOCOL.md` :
   - couche physique : tension de `+`, niveau de repos, niveau bas de chaque
     émetteur (jugé fonctionnellement à l'étape 5b, chiffré seulement si un
     oscilloscope est disponible ; l'instrument est noté), drain ouvert ou
     push-pull, pull-up et son côté ;
   - codage ;
   - trame de chaque action ;
   - qui détient l'état, et qui pilote les voyants.
   
   Il doit être **validé par la capture croisée de l'étape 5b** (sonde ESP32
   contre analyseur FX2).
2. **Injection prouvée.** On injecte la touche lumière, chaque vitesse, la
   séquence « marche puis vitesse », puis l'arrêt, **20 fois sur 20 chacune**,
   et **les voyants du panneau suivent**. **L'arrêt est une séquence de deux
   touches** : la vitesse active, puis « marche » une fois l'arrêt du moteur
   confirmé (étape 7). « Marche » moteur tournant lancerait la marche
   prolongée au lieu d'éteindre. Avec C, le critère porte sur la touche
   prototypée.
3. **Architecture du produit choisie** (A, B ou C, §4) et argumentée dans
   `docs/RECONNAISSANCE.md`. À défaut, un constat argumenté qu'aucune ne respecte
   la contrainte des voyants.

**Hors périmètre :**
- Matter ;
- la carte et le boîtier définitifs : la mesure M1 (§6) prépare seulement le
  choix de la carte ;
- l'alimentation de l'ESP par la hotte ;
- la lecture d'état annexe (`CN1`, `CN2`, IC3) : sous-projet 2 si besoin ;
- l'arrêt différé : on observe seulement la marche prolongée native (étapes −1
  et −1b) et ses trames (étape 5, scénario 7b) ;
- la bibliothèque commune et l'app.

## 2. Décisions de cadrage

| Sujet | Décision de Majid |
|---|---|
| Contraintes | **soudure et achats autorisés** (contrairement à la ScreenBar) |
| Périmètre du produit | lumière, 3 vitesses, arrêt différé. Pas de capteur, pas de scènes sur les touches |
| Voyants du panneau | **ils doivent toujours suivre Maison** : c'est un critère éliminatoire pour une architecture |
| Radio | routeur de bordure Thread dans la même pièce que la hotte |
| Branchement | **fiche sur une prise accessible**, circuit protégé par un **différentiel 30 mA** |
| Instruments | multimètre et fer à souder possédés. **Achat d'un analyseur logique FX2** (~10 €). Oscilloscope : décision à part, sans urgence |
| Stratégie | passer par la ligne `D` : A de préférence, B sinon, C en repli (§4) |
| App compagnon | la sonde parle **dès le départ** le protocole compagnon v1 de la ScreenBar (§8.5) |

## 3. Ce qu'on sait du matériel

Résumé. Le détail et les sources sont dans le brief (§1.2).

- **Carte de puissance `XB_DYB_V4`.** C'est la même pièce que la carte de la hotte
  CDA EVA60BL/1 (réf. `0010902281`, transformateur `BK-22-2232`). Fabricant
  d'origine probable : Zhongshan Liaoyuan.
- **Sur la carte :**
  - un MCU (IC2, SOP-14 sans marquage) ;
  - un `ULN2003` (IC3) qui pilote 3 relais : vitesses L, M, H ;
  - le **buzzer**.
- **Alimentation.** Un flyback **probablement isolé** : transformateur, optocoupleur
  `PC817C`, condensateurs Y. Le secondaire fait environ 12 V (zener 1N4741),
  suivi d'un abaisseur (IC4) vers environ 5 V.
- **Connecteurs basse tension :**
  - `CN3 « - D + »` (fils noir, blanc, rouge) vers le panneau tactile. Dans le
    boîtier, ces fils sont sous une gaine noire « VW-1 105 °C 600 V ».
  - `CN1 « 12V LED »` vers la lampe `XB-E06-T02` (12 V, 3 W) ;
  - `CN2 « - + »` libre.
- **Emplacement.** Le boîtier électronique est dans le corps de la hotte, sous le
  carter du ventilateur. On y accède filtre retiré.
- **Passage de sortie.** Le câble du panneau sort du boîtier dans le même
  faisceau que les fils du moteur (secteur).
- **Panneau.** Celui de la CDA est un **bloc de 5 touches résiné**. Celui de la
  Haier l'est probablement aussi.
- **Encore inconnus :**
  - la tension de `+` (5 ou 12 V) ;
  - la nature de `D` ;
  - qui est maître de l'état et des voyants ;
  - l'isolement réel du secondaire.

## 4. Architectures candidates et règle de choix

| | Principe | Voyants synchronisés | Si l'ESP tombe |
|---|---|---|---|
| **A. Injection en parallèle** | l'ESP écoute `D` à haute impédance et émet, en drain ouvert pendant un silence, la trame d'une touche | oui, si la carte pilote les voyants | la hotte marche normalement |
| **B. Homme du milieu** | `D` est coupé (cavalier `J1` ouvert, §7.1) et l'ESP relaie dans les deux sens, en ajoutant ses propres trames | oui, si la carte pilote les voyants | panneau mort sans contournement : il faut un relais de secours et un chien de garde |
| **C. Simulation des touches** | un PhotoMOS à faible capacité (type AQY221N2S) par électrode tactile | oui, dans tous les cas | aucun effet |

**Qui pilote les voyants ?** Deux étapes le disent :
- l'étape 4 : les voyants du panneau seul s'allument-ils quand on touche ?
- le test 3.4 de l'étape 6 : la carte émet-elle un état ou un paquet de voyants ?

**Règle de choix, appliquée à l'étape 6 :**

| Résultat de la reconnaissance | Choix |
|---|---|
| **la carte pilote les voyants** (voyants éteints à l'étape 4, ou paquet de voyants émis par la carte), `D` en drain ouvert avec des silences exploitables | **A** |
| **la carte pilote les voyants**, mais trafic continu, réponse attendue dans un délai court, ou `D` en push-pull | **B** |
| **le panneau pilote ses voyants** (il les allume seul et la carte n'émet aucun état) : une injection sur `D` ne les ferait pas suivre. A et B sont éliminés. C'est aussi le cas d'un `D` unidirectionnel sans état renvoyé | **C**, si le panneau s'ouvre |
| le panneau pilote ses voyants **et** il est résiné | **pas de solution propre qui respecte la contrainte des voyants** : on en rediscute avec Majid, faits en main |
| échelle de résistances au lieu d'un bus de données | A, avec une résistance commutée vers `-`, si la carte pilote les voyants |

**Pourquoi la lecture d'état annexe du brief disparaît d'ici.** Le brief
prévoyait de lire l'état sur `CN1`, `CN2` ou IC3 dans le cas « `D`
unidirectionnel ». Avec la contrainte des voyants, ce cas est éliminé pour A et
B. La lecture annexe ne pourrait servir qu'au retour d'état du produit : elle est
renvoyée au sous-projet 2.

**Écartées :**
- remplacer le cerveau par nos propres relais : travail sur le secteur, panneau perdu ;
- les boîtiers du commerce : Shelly n'a pas Matter sur Thread, Sonoff iFan n'a pas Matter et sa sortie lampe est en secteur ;
- la prise connectée : marche et arrêt seulement ;
- le doigt robot.

## 5. Règles de sécurité

Elles valent pour toutes les étapes. [SECURITE.md](SECURITE.md) les reprend en
fiche à garder sous les yeux, avec les procédures des étapes 1, 2b et 3a.

**Débrancher, attendre, ne pas passer la main**

1. **Pour toute intervention, on débranche la fiche et on la garde à vue, hors
   de portée d'autrui.**
   - Avant d'ouvrir le boîtier électronique ou d'y passer la main, on attend
     **5 minutes** : côté secteur, la carte a deux condensateurs de 4,7 µF / 450 V
     (E5 et E6), qui n'ont pas forcément de résistance de décharge.
   - Une intervention limitée au boîtier de mesure extérieur (§7.1) se fait
     fiche débranchée, sans attente.
2. **On teste le différentiel juste avant de poser chaque nouveau montage**,
   la hotte étant encore dans son état précédent, déjà contrôlé :
   1. hotte branchée, lumière allumée, on appuie sur le bouton T du différentiel ;
   2. **la lumière doit s'éteindre.** Sinon, la hotte n'est pas derrière ce
      différentiel : **ARRÊT** ;
   3. on réarme, puis on débranche la fiche pour poser le montage.
3. **Dans le boîtier électronique, on ne touche qu'à `CN3` et au câble du
   panneau.** Hotte débranchée depuis 5 minutes, on peut écarter à la main les
   fils isolés du condensateur moteur et le connecteur blanc en ligne, sans les
   débrancher ni tirer dessus. On photographie leur position avant, et on les
   remet en place après.
   - On ne touche jamais à `CN1` ni à `CN2` dans ce sous-projet.
   - Zones interdites (`photos/1.jpg`, `5.jpg`, `6.jpg`,
     `recadrages/c4_bottom.jpg`) : la moitié gauche de la carte (primaire) et le
     transformateur, toute la bande du bas (relais, `VH2` à `VH6`, « LAMP »,
     `AC-POWER`), le condensateur moteur.
   - Mesure M1 (§6) : on pose en plus, dans le quart haut-droit du boîtier, la
     sonde dans son sachet et sa batterie. On ne touche à rien d'autre. Rien
     n'appuie sur un composant, et rien n'entre dans une zone interdite.
4. **Aucune connexion ne se pose ni ne se retire sous tension :** connecteur,
   pince, résistance, cavalier, sonde, câble USB.
5. **Câble de l'adaptateur :**
   - dans le corps de la hotte et au passage de sortie, il est en fil d'au moins
     300 V (UL1007 ou UL1015, 105 °C de préférence) ;
   - il est glissé **sur toute sa longueur** dans une gaine équivalente à celle
     d'origine (VW-1 600 V), ou dans deux épaisseurs de gaine thermorétractable ;
   - il est fixé au câble du panneau, jamais serré contre les fils du moteur ni
     contre le connecteur blanc ;
   - il n'est pincé ni par le couvercle du boîtier, ni par le filtre.
   
   **Couvercle fermé et filtre remis avant de rebrancher.**
6. **Aucune main dans la hotte sous tension.** Sous tension, on ne touche qu'aux
   touches du panneau, à la fiche, et au boîtier de mesure extérieur, où l'on
   lit les instruments.

**Portes d'isolement**

7. **Avant toute mise sous tension d'un montage, trois contrôles passent dans
   l'ordre :**
   - le test d'isolation (étape 1) ;
   - son nouveau passage depuis les points de test (étape 2b), après la pose de
     l'adaptateur et après **chaque** ouverture du boîtier ;
   - la mesure de fuite en charge (étape 3a), **avant tout Mac ou analyseur**.
8. **Mac et analyseur.** On ne relie un Mac ou l'analyseur à la hotte sous tension
   qu'après les trois contrôles de la règle 7, et dans ces conditions :
   - pendant toute la session, **Mac sur batterie et sans aucun autre câble** :
     ni chargeur, ni écran, ni Ethernet, ni hub alimenté, ni audio ;
   - on ne rebranche le chargeur qu'après avoir débranché la hotte ou retiré
     l'analyseur ;
   - le Mac est posé sur une surface isolante, loin de l'acier de la hotte et de
     l'évier ;
   - on ne touche jamais en même temps le Mac et une pièce métallique reliée à
     la terre.

**Sonde et émission**

9. **Pendant les captures sous tension, la sonde est sur batterie USB**, d'un
   modèle vérifié (§12), et ses journaux partent par Wi-Fi. Sa seule liaison avec
   la hotte est le câble de sortie de l'adaptateur.
10. **Jamais une tension du bus directement sur une GPIO** : le C6 ne tolère pas
    plus d'environ 3,6 V. On passe toujours par l'étage d'écoute (§7.2).
11. **Toute émission vers la hotte (étape 7) se fait en présence de Majid.** Il se
    tient à portée de la fiche, voit le panneau, et débranche au moindre doute.

**En cas de problème**

12. **À la moindre anomalie, on débranche la fiche.** Sont des anomalies : le
    déclenchement du différentiel ou du disjoncteur, une odeur, de la fumée, des
    claquements de relais répétés, un bip continu, un panneau mort, un fil chaud.
    On ne réarme pas et on ne rebranche pas avant d'avoir compris la cause, que
    l'on note dans `RECONNAISSANCE.md`.
13. **Pas de cuisson tant que la sonde et sa batterie sont en place.** Entre deux
    sessions, on les retire. L'adaptateur, gainé, avec `J1` fermé, peut rester en
    place.
    - M1 : **on ne rebranche jamais la hotte avec la sonde ou sa batterie
      dans le boîtier électronique.**

**Une lecture qui ne prouve rien.** Entre le secondaire flottant et la terre, un
multimètre de 10 MΩ peut afficher à vide jusqu'à une centaine de volts
alternatifs, à cause des condensateurs Y. Ce chiffre ne prouve rien, ni dans un
sens ni dans l'autre : un défaut franc donnerait à peu près la même lecture.
**Seule compte la mesure en charge de l'étape 3a.**

## 6. Déroulé

Chaque étape consigne ce qu'elle a relevé dans `docs/RECONNAISSANCE.md` :
date, conditions, mesures, photos et décision. On ne passe à l'étape suivante
que si son critère de sortie est rempli.

### Étape −1 : observation d'usage

- **État de la hotte :** en usage normal, fermée.
- **Outils :** aucun.

Relever :

1. Depuis la veille, que fait un appui sur une vitesse sans passer par
   « marche » : rien, un bip, un démarrage ?
2. La lumière s'allume-t-elle sans « marche » ?
3. Que fait « marche » quand le moteur tourne : arrêt immédiat, **marche
   prolongée** (arrêt différé natif, comme les 15 min de la CDA), ou rien ?
4. Que fait un nouvel appui sur la vitesse active ?
5. L'état « armé » après « marche » s'éteint-il seul ? Au bout de combien de temps ?
6. **Quels voyants s'allument, et quand ?** Fixes, clignotants, intensité ?
   Voyant de « marche » ?
7. **Quels bips, et quand ?** Le buzzer est sur la carte de puissance : chaque
   bip prouve que la carte a reçu quelque chose.
8. Appui long (3 s) sur chaque touche : effet caché ? Verrouillage, rappel de
   filtre, arrêt différé ? Noter comment l'annuler.
9. **Reprise après coupure :** vitesse 2 et lumière allumée, débrancher la fiche
   10 s, rebrancher. Quel état revient ?
10. **Délai prudent entre deux changements de vitesse**, d'après ce qu'on entend
    des relais : il fixera la valeur de l'étape 7 (3 s par défaut).

**Sortie :** le tableau des comportements est rempli dans `RECONNAISSANCE.md`.

### Étape −1b : marche prolongée (ajout du 28/09)

- **État de la hotte :** en usage normal, fermée. L'adaptateur peut être en
  place, `J1` fermé.
- **Outils :** un téléphone qui filme le panneau, avec le son ; un chronomètre.

Elle complète les points 3 et 4 de l'étape −1. **Départ de chaque essai :**
hotte éteinte, puis « marche », V2, et, 10 s après, « marche » moteur tournant :
la marche prolongée commence (⏻ clignote). Relever :

11. **Sa fin**, lampe allumée avant le départ, sans rien toucher : au bout de
    combien de temps ? Quel état final (moteur, ⏻, voyant de vitesse, lampe) ?
    Un bip ?
12. **Une touche 5 s après le début de la prolongation**, une par essai : la
    vitesse active (vue le 27/09 : retour à l'état armé), « marche », puis une
    autre vitesse (V3).

Entre deux essais, on ramène la hotte à l'état éteint : vitesse active, moteur
arrêté, puis « marche ».

**Sortie :** les transitions de la marche prolongée sont notées dans
`RECONNAISSANCE.md`. L'automate du sous-projet 2 les reprend, et l'étape 5 en
capture les trames (scénario 7b).

### Étape 0 : photos, inventaire et cheminements

- **État de la hotte :** débranchée depuis 5 min.

Relever :

1. **Photos macro des puces IC2, IC3 et IC4** (marquages, nombre de broches) et
   des relais.
2. **`CN3` de près.**
   - La fiche est-elle collée ? Si oui, on décolle avec un outil en plastique,
     **sans tirer sur les fils**.
   - On mesure son pas, avec une règle ou un pied à coulisse en plastique :
     3 broches sur 5,0 mm, c'est du JST XH ; sur 4,0 mm, c'est du PH.
3. **Accès :** comment retirer le filtre, où est le boîtier, comment s'ouvre son
   couvercle.
4. **Trajet des fils du condensateur et du connecteur blanc**, par rapport à
   `CN3` et au passage de sortie.
5. **Câble du panneau :**
   - sa longueur libre ;
   - comment sa fiche peut sortir du boîtier. Noter le ruban et les colliers à
     refaire à l'identique ;
   - a-t-il un connecteur intermédiaire ?
   - le panneau se démonte-t-il, sans forcer ? Si oui, relever la référence de
     la puce tactile.
6. **Cheminement possible du câble de sortie** jusqu'au boîtier de mesure
   extérieur (§7.1). Par exemple le long du câble du panneau vers l'avant, ou au
   bord du filtre sans pincement. On mesure la longueur nécessaire, et on
   choisit l'endroit du boîtier de mesure : à côté de la hotte, visible, hors de
   l'aplomb de la plaque de cuisson.

**Sortie :**
- le pas de `CN3` et les longueurs sont connus, ce qui décide de la connectique ;
- on sait si le panneau s'ouvre, ce qui décide si C est possible ;
- le cheminement est choisi, avec une photo.

### Étape 1 : test d'isolation

- **État de la hotte :** débranchée.
- **Outils :** multimètre, fil à pinces crocodile, aiguille fine.
- **Préparation :**
  - relier les deux broches rondes de la fiche par le fil à pinces crocodile ;
  - piquer `CN3` par l'arrière de la fiche, côté fils, avec l'aiguille ;
  - ne toucher aucune partie métallique des pointes : le corps en parallèle
    fausserait la mesure.

| # | Mesure | Calibre | Attendu | Sinon |
|---|---|---|---|---|
| a | broches reliées de la fiche vers `CN3 -`, `D` et `+` | le plus élevé (20 MΩ ou plus) | **OL dans les deux sens** (cordons inversés), après 10 s. Une valeur qui monte vers OL est normale : les condensateurs Y se chargent | toute valeur stable : **ARRÊT**, aucun raccordement, on revient à la conception |
| b | contact de terre de la fiche (dans le trou qui reçoit la broche de terre de la prise) vers `CN3 -` | le plus élevé | OL : secondaire flottant | environ 0 Ω : secondaire relié à la terre. Ce n'est pas un défaut, mais on le note. Valeur intermédiaire (kΩ à MΩ) : **ARRÊT** et analyse |
| c | contact de terre de la fiche vers la vis de terre de la carcasse, ou une vis nue (la carcasse est peinte) | 200 Ω, cordons court-circuités d'abord (valeur à soustraire) | **moins de 1 Ω** : liaison de classe I | **ARRÊT**, défaut de terre |

**Limite.** Un ohmmètre ne voit pas un claquage qui n'apparaîtrait qu'à 230 V.
La mesure en charge de l'étape 3a couvre une partie de ce risque.

**Sortie :** feu vert pour construire l'adaptateur, ou **ARRÊT**.

### Étape 2 : adaptateur et boîtier de mesure

- **État de la hotte :** débranchée depuis 5 min (on ouvre le boîtier).

1. Construire l'adaptateur, le câble de sortie et le boîtier de mesure décrits au
   §7.1.
2. **Contrôler au multimètre, avant la pose :**
   - la continuité broche à broche entre le côté carte et le côté panneau de
     l'adaptateur ;
   - l'absence de court-circuit entre `-`, `D` et `+` ;
   - que `TP−` est bien relié à la broche « − » de la fiche qui ira sur `CN3`
     (fil noir), et `TP+` à la broche « + » (fil rouge) ;
   - `J1` fermé, `J2` ouvert.
3. **Poser :**
   1. fiche de l'adaptateur sur `CN3`, fiche du panneau sur l'embase de
      l'adaptateur ;
   2. câble de sortie selon la règle 5 et le cheminement choisi à l'étape 0 ;
   3. couvercle fermé, en vérifiant qu'il ne pince rien ;
   4. filtre remis, en vérifiant qu'il ne pince rien.

#### Étape 2b : isolation revérifiée depuis les points de test

- **État de la hotte :** débranchée, couvercle fermé, filtre remis.

1. **Même mesure que la ligne a de l'étape 1**, broches de la fiche reliées,
   depuis `TP−`, `TP+`, `TP_Dp` et `TP_Dc`, dans les deux sens. Critère
   identique : OL, sinon **ARRÊT**.
2. **Où est le pull-up de `D` ?**
   1. ouvrir `J1` (hotte débranchée, filtre retiré, sans ouvrir le boîtier) ;
   2. mesurer `TP_Dp` vers `TP+` et `TP_Dp` vers `TP−`, puis `TP_Dc` vers `TP+`
      et `TP_Dc` vers `TP−`, au calibre 20 kΩ ou automatique, dans les deux sens.
      Quelques kΩ vers `+` indiquent un pull-up, et de quel côté il se trouve ;
   3. refermer `J1`.
   
   On refait la mesure 1 après chaque ouverture du boîtier (règle 7).

#### Étape 2c : essai à vide

- **État de la hotte :** branchée.

Le panneau et la hotte doivent fonctionner exactement comme avant : chaque
touche, chaque vitesse, la lumière.

**Sortie :** les étapes 2b et 2c sont concluantes, et les photos du montage sont
dans `RECONNAISSANCE.md`.

### Étape 3 : mesures au multimètre

Toutes les pinces se posent et se déplacent **hotte débranchée** (règle 4). On
utilise des grippe-fils (pinces à crochet) sur le boîtier de mesure extérieur.
Sous tension, on ne fait que lire l'afficheur et toucher les touches du panneau.

#### 3a. Fuite en charge, avant tout Mac ou analyseur

1. Hotte débranchée : pince COM sur `TP−`, l'autre pince sur la vis de terre de
   la carcasse.
2. Multimètre en tension alternative, avec une résistance de 10 kΩ / 2 W en
   parallèle entre les deux pinces.
3. Brancher la hotte, lire dans les 5 s, débrancher.

**Attendu :** 3 V au plus, soit 0,3 mA au plus. **Au-delà de 5 V (0,5 mA), ou si
le différentiel déclenche : ARRÊT.** Entre 3 et 5 V, on le note, et on en
rediscute avant de relier un Mac ou l'analyseur.

#### 3b. Tensions

1. Hotte débranchée : COM sur `TP−`, V sur `TP+`.
2. Brancher, puis relever `+` dans chacune de ces situations :
   - veille ;
   - après « marche » ;
   - lumière allumée ;
   - vitesses 1, 2 et 3 ;
   - pendant un **appui bref** (1 à 2 s).
3. Débrancher, déplacer la pince V sur `TP_Dp`, rebrancher, et relever `D` dans
   les mêmes situations. Ajouter la fréquence et le rapport cyclique si le
   multimètre sait les mesurer.
4. Débrancher.

#### 3c. Test du pull-up

1. Hotte débranchée : 47 kΩ entre `TP_Dp` et `TP−`, sur le boîtier de mesure.
2. Brancher, remesurer `D` au repos, débrancher, retirer la résistance.

Sur un bus de 5 V :
- une chute d'environ 0,45 V indique un pull-up d'environ 4,7 kΩ ;
- une chute d'environ 0,9 V, un pull-up d'environ 10 kΩ ;
- aucune chute : sortie push-pull, ou pull-up très fort.

| Observation | Lecture probable |
|---|---|
| `V(+,−)` négatif | adaptateur inversé : **ARRÊT** |
| `+` ≈ 5 V | sortie de l'abaisseur IC4. L'étape 4 est possible |
| `+` ≈ 12 V | rail du flyback. On saute l'étape 4 |
| `D` stable à `+`, avec des creux seulement pendant les appuis | trafic sur événement, repos haut. Plutôt A |
| `D` entre 40 et 60 % de `+` en permanence | trames continues. Plutôt B |
| `D` ≈ 0 V au repos, avec de l'activité | UART inversé, repos bas (type Krona) |
| une tension continue stable, différente pour chaque touche maintenue | échelle de résistances |

**Sortie :** `+` connu, repos connu, et une première idée du trafic.

### Étape 4 : panneau seul, hotte débranchée

Cette étape n'a lieu que si `+` ≈ 5 V. **Il n'y a aucun secteur** : la sonde peut
être sur l'USB du Mac.

1. Hotte débranchée depuis 5 min : ouvrir le boîtier et **retirer la fiche de
   l'adaptateur de `CN3`**. Sinon, le 5 V de la sonde alimenterait aussi le rail
   5 V de la carte, et le MCU de la carte pourrait parler sur `D`.
2. **Contrôle de polarité, sans le panneau :**
   1. retirer la fiche du panneau de l'embase de l'adaptateur ;
   2. fermer `J2` ;
   3. brancher l'USB de la sonde ;
   4. mesurer `TP+` par rapport à `TP−` : **positif, entre +4,5 et +5,3 V** (un
      port USB peut monter à 5,25 V). Sinon, **ARRÊT** ;
   5. débrancher l'USB.
3. Remettre la fiche du panneau sur l'embase, puis brancher l'USB. En option : un
   pull-up de 10 kΩ vers `+`, si l'étape 2b a trouvé le pull-up côté carte.
4. On ne touche pas les touches pendant la mise sous tension : le circuit
   tactile se calibre.

**Questions tranchées :**
- Le panneau émet-il tout seul, et comment ?
- **Ses voyants s'allument-ils seuls quand on touche ?**
  - Si oui, le panneau pilote ses voyants (§4).
  - S'ils restent éteints, c'est la carte qui les pilote. C'est le cas favorable.

**Sortie, dans cet ordre :**
1. débrancher l'USB ;
2. ouvrir `J2` ;
3. remettre la fiche de l'adaptateur sur `CN3` ;
4. fermer le couvercle et remettre le filtre, en vérifiant qu'ils ne pincent
   rien ;
5. refaire l'étape 2b, qui se fait couvercle fermé et filtre remis.

### Étape 5 : capture en place

- **État de la hotte :** sous tension, après les contrôles de la règle 7.
- **Montage :**
  - la sonde est sur son boîtier de mesure, sur batterie ;
  - la session réseau passe par Wi-Fi (§8.5) ;
  - `tools/hotte_udp.py enregistre` écrit un fichier par scénario (§9) ;
  - capture en mode `tout`.

Scénarios, 10 s chacun sauf 7b et 7c, dans cet ordre :
1. veille ;
2. marche ;
3. lumière allumée ;
4. lumière éteinte ;
5. vitesses 1, 2 et 3 ;
6. arrêt : la vitesse active, puis « marche » une fois le moteur arrêté
   (séquence de l'étape 7) ;
7. chaque bip entendu ;

Ajouts du 28/09, entre les scénarios 7 et 8 (détail et commandes :
`RECONNAISSANCE.md`, étape 5, lignes 7b et 7c) :
- **7b. Marche prolongée.** Son début (« marche » moteur tournant, en V2). Sa
  fin, dans un enregistrement de **17 min** en mode `changements`, lampe
  allumée, sans aucun appui : heure de la fin, état final, bip, ⏻, trame.
  Puis, dans trois prolongations, une touche 5 s après leur début : la vitesse
  active, « marche », une autre vitesse. Les effets attendus sont ceux de
  l'étape −1b.
- **7c. Repos sans appui, 60 s**, moteur en V2 et lampe allumée, puis hotte
  armée. La carte répète-t-elle son état sur `D` ? C'est ce qui dira si un
  module qui redémarre peut retrouver l'état de la hotte (test 5 de
  l'étape 6).

8. panneau déconnecté. On retire sa fiche de l'embase de l'adaptateur **hotte
   débranchée**, puis on rebranche la hotte : on voit ainsi si la carte émet
   seule.

**Sortie :** un fichier par scénario, et les compteurs de la sonde sans perte ni
débordement (§8.2).

### Étape 5b : validation croisée à l'analyseur (obligatoire avant l'étape 6)

- **État de la hotte :** débranchée pour poser les pinces de l'analyseur (masse
  d'abord, sur `TP−`), puis sous tension.
- **Montage :**
  - voie 1 de l'analyseur FX2 sur la **sortie de l'étage d'écoute**, le même
    nœud que GPIO6 (3,3 V) ;
  - voie 2 sur `TP_Dp` à travers un diviseur à haute impédance choisi d'après
    `+` : 47k/68k pour 5 V, 100k/33k pour 12 V ;
  - PulseView ;
  - Mac sur batterie selon la règle 8, **en USB direct**. L'isolateur ADuM3160
    est exclu à cette étape : limité au Full Speed, il bride l'échantillonnage.
- **Critères :**
  - **voie 1 :** les durées de la sonde et de l'analyseur concordent à ±2 µs
    près, plus le quantum de 1 µs. Les trames décodées sont identiques ;
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

### Étape 6 : décodage et choix de l'architecture

1. **`tools/analyse.py` redécoupe le flux en trames** avec un seuil de silence
   choisi après coup, puis **`auto`** classe le signal :
   - **UART** : durées multiples d'un temps bit. On inclut les débits bas non
     standard, de 300 à 1 200 bauds, et le repos bas ;
   - **distance d'impulsion** : une phase fixe, l'autre à deux valeurs, avec un
     en-tête. C'est le type WTC6534 ;
   - **Manchester** : seulement des durées T et 2T.
2. **`tools/analyse.py diff`** compare les scénarios deux à deux et isole les
   bits de chaque touche et de chaque état.
3. **Tests décisifs :**
   1. Après le relâchement, la trame revient-elle à « aucune touche » ? Si oui,
      la carte détient l'état de la hotte.
   2. La carte répond-elle après chaque trame ? Avec quel délai ?
   3. Y a-t-il des silences plus longs qu'une trame, où l'on pourrait injecter ?
   4. La carte émet-elle son état ou un paquet de voyants ?
   5. **Au repos, sans appui** (scénarios 7b et 7c de l'étape 5), la carte
      répète-t-elle son état sur `D` ? Avec quelle période ? Si oui, un module
      qui redémarre relit l'état de la hotte en une période ; sinon, `D` ne le
      lui rend pas.
   6. **La fin de la marche prolongée** (scénario 7b) laisse-t-elle une trame
      sur `D` ? Sinon, le module du produit la verra à la répétition suivante
      de l'état (test 5), ou, sans répétition, par la lecture d'état annexe
      (arrêt du moteur).

   Les tests 5 et 6 ne changent pas la règle de choix du §4 : ils servent au
   sous-projet 2.
4. Appliquer la règle de choix du §4, **en croisant avec le résultat de l'étape 4**.

**Sortie :**
- `docs/PROTOCOL.md` est rédigé ;
- le choix est argumenté dans `RECONNAISSANCE.md` ;
- **un avenant d'injection à cette spec est écrit, quel que soit le choix.** Il
  fixe, chiffré d'après `PROTOCOL.md` :
  - la syntaxe de `injecte` ;
  - la durée basse maximale par impulsion et la durée totale maximale par trame ;
  - le silence minimal avant d'émettre, et l'attente maximale ;
  - le délai minimal entre deux changements d'état du moteur (étape −1) ;
  - pour la séquence d'arrêt de l'étape 7 : le signe qui confirme l'arrêt du
    moteur avant « marche » (trame d'état de la carte, ou constat de Majid),
    et l'attente maximale de ce signe ;
  - pour B, le mode relais ; pour C, le prototype PhotoMOS ;
- **l'avenant est relu par Majid avant l'étape 7.**

### Étape 7 : premier essai d'injection

Cette étape se fait **en présence de Majid** (règle 11), sous tension, sonde sur
batterie.

- **Si le choix est A :**
  1. **Hotte débranchée :**
     1. monter l'étage d'injection (§7.3) ;
     2. brancher l'USB, envoyer `injection monte 1` ;
     3. vérifier dans `config` que les valeurs de l'avenant sont chargées ;
     4. débrancher l'USB et passer la sonde sur batterie.
  2. Brancher la hotte. `hotte_udp.py` n'envoie `injection on` qu'après une
     **confirmation tapée à la main** (« Majid devant la hotte »), jamais depuis
     un script.
  3. Injecter, dans cet ordre :
     1. la touche lumière : on doit entendre le bip, voir la lumière et **le
        voyant changer**, et retrouver la trame sur le bus ;
     2. chaque vitesse ;
     3. la séquence « marche » puis une vitesse ;
     4. l'arrêt, depuis une vitesse (V1, V2 et V3 à tour de rôle) :
        1. injecter la vitesse active : le moteur s'arrête, ⏻ reste fixe
           (état armé, étape −1, ligne 4) ;
        2. attendre la **confirmation de l'arrêt du moteur**, par le signe que
           fixe l'avenant : la trame d'état de la carte si elle en émet,
           sinon le constat de Majid (voyant de vitesse éteint, ⏻ fixe,
           moteur silencieux). On attend aussi au moins le délai minimal ;
        3. injecter « marche » : tout s'éteint, ⏻ compris. La lampe ne change
           pas.

        **Jamais « marche » avant la confirmation** : moteur tournant, elle
        lance la marche prolongée (⏻ clignote). Sans confirmation dans
        l'attente maximale de l'avenant, « marche » ne part pas et l'essai
        est un échec.
     
     Le firmware impose le délai minimal entre deux changements d'état du
     moteur, et refuse sinon.
  4. **Pendant les premiers essais, voie 2 de l'analyseur sur `TP_Dp`.** Le
     niveau bas obtenu par l'injection est jugé fonctionnellement : la carte
     réagit (bip, lumière, voyant) et la sonde relit la trame émise. Une
     mesure chiffrée est optionnelle, à l'oscilloscope : sous 0,8 V.
     L'instrument qui a donné les niveaux est noté.
  - **Critère :** celui du §1, point 2. Aucun comportement anormal de la carte,
    et le vrai panneau toujours fonctionnel après l'essai.
- **Si le choix est B :** on suit l'avenant.
  1. Monter la voie 2 (§7.4).
  2. Ouvrir `J1`.
  3. Mode relais transparent : le panneau doit marcher à travers la sonde.
  4. Puis mêmes injections et même critère.
- **Si le choix est C :** on suit l'avenant (prototype PhotoMOS sur une touche).

### Mesure M1 pour le produit : radio comparative, hotte débranchée (ajout du 28/09)

Elle prépare le sous-projet 2 : elle décide de la carte et de son antenne.
**Elle ne conditionne aucune étape de la reconnaissance.** On peut la faire à
tout moment, mais avant le dessin du boîtier et avant tout achat de carte.
Journal : `RECONNAISSANCE.md`, « Mesure M1 ».

- **État de la hotte :** **débranchée pendant toute la séance**, fiche à vue.
  On attend 5 min avant d'ouvrir le boîtier (règle 1).
- **Matériel :**
  - la sonde, du modèle prévu pour le produit (SuperMini violette, antenne
    céramique ; sinon, on note le modèle). Elle est seule, hors du boîtier de
    mesure, en Wi-Fi, sur sa batterie vérifiée (§12) ;
  - un sachet plastique **non métallisé** (un sachet antistatique métallisé
    ferait écran) ;
  - un support isolant (un carton) ;
  - le Mac, qui enregistre par le Wi-Fi.
- **Principe :** on relève le RSSI Wi-Fi de la sonde, pour le même point
  d'accès, juste sous la hotte, puis dans le boîtier électronique. L'écart est
  l'atténuation de la hotte à 2,4 GHz, la bande de Thread.

Positions, 3 min chacune, avec le RSSI publié toutes les 10 s
(`json reseau 10000`) :
1. **P0** : sous la hotte, sur le support isolant posé sur la plaque éteinte et
   froide. La sonde est à environ 10 cm sous le filtre, à l'aplomb du boîtier
   électronique ;
2. **P1a et P1b** : dans le boîtier électronique, à l'emplacement prévu du
   module (quart haut-droit), l'antenne dans deux orientations. Couvercle
   fermé, filtre remis ;
3. **P0 de nouveau.** Plus de 3 dB d'écart avec le premier P0 : la série est
   à refaire (quelqu'un a bougé, ou le point d'accès a changé de canal).

**Règles propres à M1 :**
- dans le boîtier, la sonde reste dans son sachet : ni sa coque USB-C ni ses
  broches ne touchent quoi que ce soit. Elle et sa batterie n'appuient sur
  aucun composant et n'entrent pas dans les zones interdites (règle 3). Le
  couvercle se ferme sans forcer ; sinon, on ne force pas, et M1 est à revoir ;
- **aucun câble ne sort du boîtier** : il guiderait l'onde vers l'extérieur, et
  la mesure serait trop optimiste ;
- **on ne rebranche jamais la hotte avec la sonde ou sa batterie dans le
  boîtier** (règle 13). À la fin, on retire et on compte la sonde, la batterie
  et le sachet, on ferme le couvercle et on remet le filtre. Puis on refait
  l'étape 2b si l'adaptateur est posé (règle 7).

**Calcul :**
- **atténuation** `A` = médiane en P0 − meilleure médiane en P1. Le boîtier
  imprimé reproduira l'orientation retenue. Si la sonde perd le Wi-Fi en P1,
  `A` ne se mesure pas ainsi : c'est la mesure Thread directe en P1 (point
  suivant) qui décide. Sans elle, on retient la branche « moins de 10 dB » ;
- **marge Thread estimée** `M = R_T − A + 104`, en dB :
  - `R_T` est le RSSI moyen du lien Thread vers le routeur de bordure, en P0.
    Il est lu par la C6 du banc Matter du sous-projet 2 (`parent_rssi`, comme
    dans benq ; la C6 est en MED pendant la mesure) ;
  - −104 dBm est la sensibilité du C6 en 802.15.4 (fiche Espressif, 1 % de
    trames perdues) ;
- **si la C6 du banc Matter peut être posée elle-même en P1** (sur batterie, en
  MED comme en P0, ses diagnostics radio lus par le canal compagnon),
  `M = R_T(P1) + 104`, mesurée directement. Cette mesure fait foi, et `A` sert
  alors de contrôle.

| Marge `M` | Décision |
|---|---|
| plus de 20 dB | SuperMini violette gardée ; routeur Thread par défaut |
| de 10 à 20 dB | SuperMini gardée, en MED ; essai de 48 h du lien avant de figer le boîtier |
| moins de 10 dB | carte à antenne déportée (par exemple XIAO ESP32-C6, u.FL) vers une partie non métallique ; M1 refaite avec elle |

À moins de 3 dB d'un seuil, on refait la série. Si l'écart demeure, on retient
la branche la plus prudente.

**Limites.**
- Le point d'accès Wi-Fi n'est pas le routeur de bordure, et la direction
  diffère. Or l'atténuation d'une caisse d'acier dépend de la direction, à
  cause de ses ouvertures. C'est pourquoi `R_T` vient d'une mesure Thread.
- `M` est la marge dans le sens routeur vers module. Si le produit réduit sa
  puissance d'émission (alimentation par `CN3 +`), la marge dans l'autre sens
  baisse d'autant. On en tient compte au plan du matériel.
- La qualité de lien que rapporte OpenThread n'est pas une marge : sur le C6,
  elle est calculée contre une sensibilité fixe de −120 dBm, et non contre les
  −104 dBm de la fiche (spec du produit, §6.1). Seul `M` décide.

## 7. Matériel d'interface

Le détail (schémas, valeurs, brochage du C6, nomenclature, contrôles avant
pose) est dans [WIRING.md](WIRING.md).

### 7.1 Adaptateur, câble de sortie et boîtier de mesure

L'ensemble a trois parties, pour que **tout ce qu'on mesure ou manipule soit
hors de la hotte**.

```
   DANS LE BOÎTIER / LE CORPS DE LA HOTTE                    HORS DE LA HOTTE, à vue
 ┌──────────────────────────────────────────┐          ┌────────────────────────────────────┐
 │ ADAPTATEUR (petite plaque, gainée)        │          │ BOÎTIER DE MESURE (plaque à        │
 │                                           │          │ pastilles, fixée)                  │
 │  fiche XH 3 br. ──► CN3 [- D +]           │  câble   │  TP−  TP+  TP_Dp  TP_Dc            │
 │  embase XH 3 br. ◄── fiche du panneau     │  de      │  J2 : TP+ ── 5V de la sonde        │
 │                                           │  sortie  │       (ouvert par défaut)          │
 │  D_carte ──┤J1├── D_panneau               │ 4 fils   │  étage d'écoute voie 1 → GPIO6     │
 │   (fermé par défaut ; ouvert = B)         ├─────────►│  étage d'injection voie 1 ← GPIO7  │
 │                                           │ ≥300 V,  │  (voie 2 : GPIO0 / GPIO1, si B)    │
 │  sorties : −, +, D_panneau, D_carte       │ gainé    │  ESP32-C6 SuperMini sur barrettes  │
 └──────────────────────────────────────────┘          │  batterie USB (captures)           │
                                                        └────────────────────────────────────┘
```

- **Adaptateur.** Petite plaque avec la connectique XH (ou PH, selon l'étape 0) et
  `J1`. Il reste près de `CN3`, gainé.
- **Câble de sortie.** 4 fils (`−`, `+`, `D_panneau`, `D_carte`), conforme à la
  règle 5, le long du cheminement choisi à l'étape 0. Il se termine par une
  **fiche JST XH 4 broches** (ordre `−`, `+`, `D_panneau`, `D_carte`, avec
  détrompeur), branchée sur l'embase assortie du boîtier de mesure : le
  boîtier se **détache** du câble, hotte débranchée.
- **Boîtier de mesure.** Les points de test, `J2` (le `+` vers le `5V` de la
  sonde, fermé seulement à l'étape 4), les étages, et la sonde sur barrettes
  femelles. Il est **fixé à côté de la hotte, à vue, hors de l'aplomb de la
  plaque** (étape 0).
- **Réversible :** on retire l'adaptateur, on rebranche la fiche du panneau sur
  `CN3`, et la hotte est d'origine.
- **La sonde n'est jamais alimentée par la hotte** dans ce sous-projet : `J2`
  reste ouvert, sauf à l'étape 4.
- **Pas de broches Dupont** dans les fiches du panneau et de `CN3` d'origine :
  leurs contacts XH se desserrent.

### 7.2 Étage d'écoute (voie 1 : `D_panneau` vers GPIO6)

```
  D ──100k──┬── base   Q1 (BC547 / 2N3904)
          R_be (100k par défaut)
  - ────────┴── émetteur Q1
  3V3 ──10k──── collecteur Q1 ──► GPIO6        (D haut → GPIO6 bas)
```

- **Charge et comportement :**
  - il ne prélève que 40 à 110 µA sur `D` ;
  - il commute en 1 à 2 µs ;
  - il reste neutre si la sonde est éteinte ;
  - il fonctionne tel quel sur un bus de 5 V comme de 12 V.
- **Sa logique est inversée.** Le firmware remet les niveaux dans le sens du bus
  avant de les publier.
- **Seuil : environ 1,2 V, fixe.** Il ne suit pas la tension du bus et baisse à
  chaud, d'environ 4 mV/°C. Il ne convient donc que si le **niveau bas de `D`**
  reste sous 0,6 V environ. On le vérifie à l'étape 5b : fonctionnellement
  (capture propre, concordance avec l'analyseur), et en chiffre seulement si un
  oscilloscope est disponible. Sinon, on relève le seuil par la résistance entre
  base et émetteur, `R_be` :
  - 47k pour `+` = 5 V (seuil d'environ 2 V) ;
  - 15k pour `+` = 12 V (seuil d'environ 5 V).
  
  On garde au moins 25 µA de base au niveau haut.
- **Délai et asymétrie** de l'étage : mesurés au banc (§10).
- Le pilote RMT active le pull-up interne de GPIO6. C'est sans effet avec le 10 k
  de collecteur.

### 7.3 Étage d'injection (voie 1 : GPIO7 vers `D`), monté à l'étape 7 seulement

```
  GPIO7 ──4,7k──┬── base   Q2 (BC547 / 2N3904)
               10k
  - ────────────┴── émetteur Q2
  D ──470 Ω──────── collecteur Q2        (GPIO7 haut → D tiré bas)
```

- **Uniquement en drain ouvert :** on ne pilote jamais `D` en push-pull 3,3 V.
- **Le 10 k entre base et émetteur garde Q2 bloqué** pendant le reset, et quand
  la broche est libérée.
- **Les 470 Ω de collecteur limitent le courant** à environ 10 mA sous 5 V, si
  une sortie active tient `D` haut en face (une puce tactile de type WTC6534
  pilote elle-même la ligne pour émettre). Avec un pull-up de 4,7 k, le niveau
  bas vaut environ 0,5 V. On le vérifie à l'étape 7, point 4. On ne descend vers
  220 Ω que si ce niveau n'est pas atteint.
- **Ne jamais intervertir GPIO6 et GPIO7.** GPIO6 a un pull-up d'environ 45 kΩ
  après le reset : branché sur l'injection, il débloquerait Q2 et tirerait `D`
  à l'état bas.

### 7.4 Voie 2 (`D_carte`), seulement si B

C'est le même étage d'écoute, sur GPIO0. **`J1` ouvert, chaque tronçon ne garde
que le pull-up de son côté.** L'avenant de l'étape 6 fixe donc :
- un pull-up posé côté tronçon qui n'en a pas, de la valeur relevée à l'étape 2b ;
- en émission, si `D` est push-pull, un tampon push-pull alimenté par `+`
  (74HCT si `+` = 5 V) au lieu du NPN ;
- le relais : front par front (interruption GPIO vers GPIO, quelques µs de
  latence), ou la sonde qui répond elle-même au panneau. Le RMT ne rend une
  trame qu'après le silence : il ne peut pas servir au relais.

### 7.5 Choix des GPIO du C6 SuperMini

| Usage | GPIO | Remarque |
|---|---|---|
| écoute, voie 1 | 6 | RMT en réception, plus une interruption sur les deux fronts (§8.4) |
| injection, voie 1 | 7 | sortie à l'état bas dès la première instruction de `setup()` |
| voie 2 (B) | 0 (écoute) et 1 (émission) | |
| générateur du banc (§10) | 7, sur le second C6 | |
| **à éviter** | 4, 5, 8 (WS2812), 9 (BOOT) et 15 (broches de démarrage du C6) ; 12 et 13 (USB) ; 16 et 17 (UART0 : journal de la ROM) ; 21, 22 et 23 (trous intérieurs) | GPIO2 reste libre |

### 7.6 Sonde dans le boîtier de mesure

- La coque de l'USB-C du SuperMini est reliée à `GND`. On l'isole de tout métal.
- Le connecteur USB-C reste accessible, pour flasher hotte débranchée.
- On relève le RSSI Wi-Fi à l'emplacement réel.

## 8. Firmware « sonde »

### 8.1 Projet

- **Projet PlatformIO** à la racine de `hotte haier/`.
- **Même plateforme que la ScreenBar :** pioarduino `55.03.312-1`, c'est-à-dire le
  cœur Arduino-ESP32 3.3.12 et IDF 5.5. Le cache de paquets est partagé.
- **Deux environnements**, sans Matter, avec la table de partitions par défaut :
  - `sonde`, pour le C6 SuperMini ;
  - `generateur`, le banc de validation (§10), sur un second C6.
- **Version :** mécanisme de la ScreenBar, `src/fw_version.h`, `src/app_desc.c`,
  et les drapeaux de `platformio.ini` (`FW_VERSION`, `FW_ENV`,
  `tools/git_rev.py`). `hello` exige `fw_desc` égal à `fw`, ainsi que `env`.

### 8.2 Capture continue

On n'utilise pas l'API RMT d'Arduino, dont le réarmement laisse un temps mort :
la fin de réception passe par la tâche des minuteries. On prend **le pilote RMT
d'IDF** (`driver/rmt_rx.h`, présent dans le cœur 3.3.12).

- **Réception :**
  - `rmt_new_rx_channel` à 1 MHz. Une résolution de 500 kHz est prévue pour les
    débits de 300 à 600 bauds : le silence peut alors aller jusqu'à 65 ms ;
  - `rmt_receive` avec `flags.en_partial_rx = 1`, le ping-pong géré sur C6 ;
  - rappel `on_recv_done` enregistré. **Le réarmement et l'horodatage se font
    dans le rappel** : `rmt_receive` est permis en interruption.
- **Ce que la sonde publie :** des **blocs de durées consécutives**, horodatés.
  Une réception va du premier front jusqu'au silence. Si le silence n'arrive pas,
  elle part en plusieurs parties au fil de l'eau.
  - Chaque partie est copiée du rappel dans un **tampon circulaire**, de
    2 048 symboles par exemple. La tâche `loop` le vide.
  - Une perte, si le tampon est plein, est marquée `debord` sur la partie
    suivante et comptée.
- **Découpage en trames :** il se fait **sur le Mac** (`analyse.py`), avec un
  seuil choisi après coup. Le silence du RMT ne sert qu'à clore une réception, et
  à comparer les trames en mode `changements`.
- **Horodatage :** `t_us` d'une partie vaut `esp_timer_get_time()` dans le rappel,
  moins la somme de ses durées, moins le silence si c'est la dernière partie.
  - Le symbole final de durée nulle est retiré de `dur_us`.
  - Précision attendue : quelques dizaines de µs sur les écarts entre
    réceptions. On la vérifie à l'étape 5b, sur ces écarts. Les durées, elles,
    sont à 1 µs.
- **Filtre anti-parasites :** de 0 à 3 µs, la limite du C6 étant de 255 cycles à
  80 MHz, soit 3,19 µs. Silence : jusqu'à 32 767 µs à 1 MHz.
  - `seuils` refuse les valeurs hors bornes **avant** de les écrire en NVS.
  - Au démarrage, une valeur hors bornes lue en NVS est remplacée par la valeur
    par défaut, et un `log` le signale.
- **Deux modes d'émission :**
  - `tout` : chaque partie ;
  - `changements` : une réception n'est émise que si ses durées diffèrent **de
    la dernière réception émise** de plus de ±5 % (au moins 10 µs). La réception
    suivante porte `rep`, le nombre de réceptions identiques depuis la dernière
    émission, et `etat.capture.rep_en_cours` donne le compte courant.
- **Niveau de repos** échantillonné hors réception, publié dans `etat.bus`.

### 8.3 Console et commandes

Mêmes conventions que la ScreenBar : texte humain par défaut, `help` liste tout.

| Commande | Effet | À distance (§8.5) |
|---|---|---|
| `info` | version, MAC, réseau, configuration de capture | non |
| `wifi <ssid> <mdp>` | identifiants en NVS | non |
| `capture on\|off\|tout\|changements` | démarre ou arrête la capture, choisit le mode | oui : `reponse` `ok` ou `usage`, puis `config` réémis |
| `seuils [filtre_us] [silence_us]` | réglages RMT, bornés (§8.2), en NVS | oui, même forme |
| `bus` | niveau instantané, repos, activité | non : les mêmes données sont dans `etat.bus` |
| `stats` | trames, débordements, pertes, par cause | non : les mêmes données sont dans `compteurs` |
| `json ...` | session machine (§8.5) | selon la liste blanche |
| `injection monte 0\|1` | déclare l'étage d'injection monté (NVS) | non |
| `injection on\|off` | arme ou désarme l'injection. Désarmée au démarrage, et désarmée seule après 10 min | oui |
| `injection regle <nom> <valeur>` | valeurs de l'avenant (§8.4), vérifiées avant la NVS ([PROTOCOLE-JSON.md §5.1](PROTOCOLE-JSON.md#51-injection)) | non |
| `injecte ...` | syntaxe fixée par l'avenant de l'étape 6 ; avant lui, `injecte durees <d1> <d2> ...` ([PROTOCOLE-JSON.md §5.1](PROTOCOLE-JSON.md#51-injection)) | oui |
| `reboot` | redémarrage | non |

**À distance, aucune commande ne produit de texte humain.** Il partirait sur
l'USB, qui n'a pas d'hôte pendant les captures.

### 8.4 Garde-fous de l'injection

Tous ces garde-fous sont dans le firmware :

- **Démarrage.** GPIO7 est mise à l'état bas en toute première instruction de
  `setup()`.
- **Refus.** `injecte` est refusée si l'injection n'est pas armée, pas déclarée
  montée, ou si le **délai minimal entre deux changements d'état du moteur**
  n'est pas écoulé. Ce délai vaut 3 s par défaut ; l'avenant fixe la valeur.
- **Émission.** Elle passe par le **RMT en émission**, pour un timing
  indépendant du Wi-Fi, bornée par la durée basse maximale par impulsion et la
  durée totale maximale par trame que fixe l'avenant.
- **Silence avant émission.** Une **interruption GPIO sur les deux fronts de
  GPIO6**, en plus du RMT en réception, tient l'heure du dernier front. Avec A,
  on n'émet qu'après le silence minimal. Si le bus ne se tait pas dans l'attente
  maximale, résultat `delai`.
- **Collision.** Pendant l'émission, la même interruption compare le niveau lu au
  niveau émis, avec une tolérance égale au délai de l'étage mesuré au banc. En
  cas d'écart, l'émission est arrêtée : on désactive le canal, la broche est
  libérée et le 10 k bloque Q2. Résultat `collision`.
  - Cette détection se valide au banc (§10, [BANC.md §9](BANC.md#9-injection-et-collision-critère-5)).
  - Si elle s'avère impossible, on l'écrit dans l'avenant, et la collision est
    seulement constatée après coup, par relecture.

### 8.5 Protocole compagnon v1, profil « hotte »

**Ce qui ne change pas.** Tramage, enveloppe, session, commandes `id=<n>`,
`reponse` et règles de compatibilité restent **identiques** au protocole de la
ScreenBar (`PROTOCOLE-JSON.md`, sections 2 à 4, 6 et 9) :
- une ligne machine s'écrit `RS` + JSON compact + `LF`, 1 024 octets au plus,
  avec un budget de pire cas de 896 ;
- les champs `v`, `t`, `n`, `ms` viennent en tête, puis `bloc` ;
- `json 1` ouvre la session, avec un bail.

Le profil est documenté dans [PROTOCOLE-JSON.md](PROTOCOLE-JSON.md) de ce
dépôt, **par différence** avec celui de la ScreenBar.

**Messages**

| Message | Contenu |
|---|---|
| `hello` `base` | **tous** les champs de la ScreenBar. `rev` = 4, celle de la ScreenBar au commit `c58a506`. `build` = `sonde` (nouvelle valeur). `reseau_build` = `aucun` (pas de réseau Matter). `session.transport` = `usb` ou `udp` |
| `hello` `identite` | **tous** les champs de la ScreenBar : `mac`, `id.fabricant` = `Djoko-CLI`, `id.produit` = `Sonde hotte Haier`, `id.serie` = `HOTTE-` + MAC, `id.nom` = `Sonde hotte`, `id.hw` = 1, `id.hw_txt` = `C6 SuperMini, etages v1`. **Nouveau champ `appareil` = `hotte`** : une app multi-appareils choisira sa vue d'après lui. Absent, il vaut `screenbar`. `caps` : `sonde`, `injection` (toujours présente dans ce build), `trames`, `log`, `udp`, `cle`, `mdns` |
| `config` | réglages de capture (GPIO, résolution, filtre, silence, mode, étage inversé) et valeurs d'injection de l'avenant |
| `etat` | bloc `bus` : repos, activité, dernière réception, réceptions par seconde. Bloc `capture` : mode, `debord`, `rep_en_cours`. Bloc `injection` : `montee`, `armee`, `arme_reste_s`, dernier `id` et son résultat. Bloc `sys` : **l'objet `sys` de la ScreenBar à l'identique** |
| `reseau` bloc `ip` | le schéma de la ScreenBar : `udp.port`, `udp.ouvert`, `udp.empreinte`, `udp.sessions`, `udp.rx`, `udp.rejets`, `udp.tx`, `udp.tx_perdus`. `srp` = null. En plus : `mdns.nom` = `hotte-sonde.local` et `wifi.rssi_dbm`. La cap `mdns` dit à l'app de résoudre `mdns.nom` au lieu de `srp.nom` |
| `compteurs` bloc `sonde` | `receptions`, `parties`, `debord`, `rep`, `lignes_perdues`, `sautes`, `rejets` |
| événement `trame` | `num` (réception), `part` (à partir de 0), `fin` (dernière partie), `t_us`, `niv0` (niveau **du bus** au début : `haut` ou `bas`), `dur_us` (au plus **110 durées**, alternées), `debord`, `rep` (mode `changements`). Pire cas : environ 850 octets, sous le budget de 896 |
| événement `injection` | `id` de la commande, `cmd` (40 caractères au plus), `resultat` (`ok`, `collision`, `delai`), `niv0`, `dur_us` (émises), `attente_us`, `relu` (durées relues pendant l'émission) |
| `log` | annonces du firmware |

**Écarts aux conventions de la ScreenBar**, à documenter dans le profil :
- **`t_us` est un entier sur 64 bits** (`esp_timer_get_time()`, sûr en JSON
  jusqu'à 2^53). Il déroge à la convention « uint32 ». `Writer` gagne une méthode
  `u64`, testée sur l'hôte, et `json_check.py` l'accepte.
- Les parties d'une réception sont **produites ou sautées ensemble**. `sautes`
  est porté par la première partie produite après un saut.

**`injecte` est asynchrone,** comme les commandes `lampe` de la ScreenBar :
- la `reponse` `fin` part tout de suite : `accepte` avec `suite` = `injection`, ou
  `refuse` avec un `msg` (« non armée », « non montée », « délai minimal ») ;
- l'événement `injection` suit, avec le même `id`. Les 20 essais de l'étape 7 se
  comptent ainsi, un par un.

**Transport :**
- **USB CDC** au banc.
- **UDP, port 5480, sur le Wi-Fi, en IPv4** en v1 de la sonde. Sockets lwIP.
  `h1::Peer` est rempli avec des adresses IPv4 mappées (`::ffff:a.b.c.d`). Le nom
  mDNS est `hotte-sonde.local`.
- **Enveloppe H1 identique** : HMAC-SHA256 et poignée de main de la ScreenBar. La
  clé est créée par `json cle nouvelle` **par l'USB seulement**, et gardée en NVS
  (`hotte/cle`).

**Profil distant de la sonde.** Il s'écarte volontairement de la ScreenBar : là-bas,
Thread et la radio de la lampe partagent la bande. Ici, le Wi-Fi a de la marge, et
les trames sont la raison d'être de la sonde.

| Réglage | USB | UDP (Wi-Fi) |
|---|---|---|
| période des `etat` | 1 000 ms | 2 000 ms |
| période des `compteurs` | 1 000 ms | 5 000 ms |
| période des `reseau` | 5 000 ms | 30 000 ms |
| événements `trame` après `json 1` | oui | **oui, sans coupure automatique** |
| plafond des lignes `trame` | 100 par seconde | 100 par seconde. Au-delà, elles ne sont pas produites, et la suivante porte `sautes` |
| file d'émission UDP | — | 32 datagrammes, 50 000 octets/s en moyenne, crédit de 8 Ko. File pleine : ligne perdue et comptée |

`json trames 0|1` commande l'envoi des événements `trame` à une session.
`capture off` arrête la capture elle-même.

**Liste blanche à distance** (toute ligne reçue par le réseau porte un `id`) :
- `json 1 [bail 10..120]`, `json 0`, `json etat`, `json hello`, `json ping` ;
- `json periode 2000..60000`, `json compteurs 0|1000..60000`,
  `json reseau 0|10000..60000`, `json trames 0|1`, `json log 0|1` ;
- `capture on|off|tout|changements`, `seuils ...` (bornés) ;
- `injection on|off`, `injecte ...`.

Tout le reste répond `interdite`, en particulier `wifi`, `json cle ...`,
`injection monte`, `reboot`, `bus`, `stats` et `info`.

### 8.6 Code repris de la ScreenBar

**Prérequis, levé le 27/09/2026.** Le transport réseau de la ScreenBar (`h1_proto`,
`h1_crypto`, `net_udp`, `tools/halo_udp.py`, `test_h1.cpp`, et les modifications
de `json_out`, `json_mode`, `cli`, `json_check` et `PROTOCOLE-JSON.md`) est
**committé et poussé** dans `benq`. Le commit de référence est **`c58a506`**.
Toute copie se fait depuis ce commit (`git show c58a506:<chemin>`). Il est noté :
- dans l'en-tête de chaque fichier copié ;
- dans `docs/PROTOCOLE-JSON.md` de ce dépôt ;
- dans le lien placé en tête de cette spec.

| Origine (`benq`) | Destination | Reprise |
|---|---|---|
| `src/h1_proto.{h,cpp}`, `src/h1_crypto.cpp` | idem | telle quelle (partie pure, et mbedTLS) |
| `tools/host_tests/test_h1.cpp`, et la commande `clang++` de `tools/test_halo1.sh` | `tools/tests/test_h1.cpp`, `tools/tests/test_h1.sh` | telle quelle (vecteurs H1, CommonCrypto) |
| `src/json_out.{h,cpp}` | idem | **scindé**. On garde `Writer` (plus `u64`), `parseIdPrefix`, `copyCmd` / `maskCmd`, `LineAssembler`, `RateCap`, `Cadence`, `Queue`, `leaseExpired`, `Reply` / `ReplyCache` (sans ce qui touche à la consigne de la lampe). On retire tout ce qui dépend de `halo1_*`. On réécrit `Item` et `remoteRefusal` (§8.5) |
| `src/json_mode.{h,cpp}`, et `runLine` / `cliRunRemote` / `cliPoll` de `src/cli.cpp` | `src/json_mode.*`, `src/cli.*` | **réécrit en partie** : instantanés propres à la hotte ; observateur de livraison et crochets de la lampe, de la LED et de Matter retirés ; `#if MATTER_NET_THREAD` remplacé par `#if SONDE_UDP` ; profil distant du §8.5 |
| `src/net_udp.{h,cpp}` | `src/net_udp_wifi.{h,cpp}` | **réécrit sur lwIP.** On garde la gestion de la clé (création, effacement, empreinte, NVS `hotte/cle`) et la politique de perte : jamais d'attente, perte comptée. Files et plafonds redimensionnés (§8.5) |
| `src/fw_version.h`, `src/app_desc.c`, `tools/git_rev.py`, drapeaux `FW_ENV` et `git_rev` de `platformio.ini` | idem | tels quels |
| `tools/json_check.py` | idem | schéma du profil hotte, et option `--jsonl` (§9) |
| `tools/halo_udp.py` | `tools/hotte_udp.py` | on garde le H1, la clé et la session. On réécrit la résolution et le socket en `AF_UNSPEC`. On ajoute `enregistre` et la confirmation tapée de `injection on` |

Le sous-projet 3 remettra ces fichiers en commun.

### 8.7 Principes de robustesse (repris de la ScreenBar)

- **La sonde n'attend jamais le client.** Une ligne qui ne trouve pas de place
  est perdue et comptée.
- **Un seul producteur de lignes machine :** la tâche `loop`. Les rappels RMT et
  GPIO et la tâche Wi-Fi ne font que poser des données dans des files.
- **Le Wi-Fi ne dort pas** (`WiFi.setSleep(false)`). La consommation reste stable,
  autour de 100 mA : à vérifier au banc. Une batterie USB ne se coupe pas.
- `hello` publie la cause du dernier redémarrage (`esp_reset_reason`).

## 9. Outils Mac

| Outil | Rôle |
|---|---|
| `tools/hotte_udp.py cle <port>` | pose la clé par l'USB. Elle est rangée dans `~/.config/hotte-sonde/cle` (droits 0600) et n'est jamais affichée |
| `tools/hotte_udp.py session <hôte> [cmd ...]` | session H1, résumé lisible de chaque ligne. `injection on` demande une confirmation tapée |
| `tools/hotte_udp.py enregistre <hôte> <scénario> [--duree s]` | écrit `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl`, et un résumé dans `logs/live.log`, à suivre avec `tail -f` |
| `tools/analyse.py histo <capture>` | histogrammes des durées hautes et basses, regroupements |
| `tools/analyse.py trames <capture> --silence <µs>` | redécoupe le flux en trames |
| `tools/analyse.py auto <capture>` | essaie UART (300 à 19 200 bauds, normal et inversé), distance d'impulsion (T estimé à partir des regroupements) et Manchester, puis classe les hypothèses par taux d'erreur |
| `tools/analyse.py uart <capture> <bauds> [--inverse]` | décodage imposé |
| `tools/analyse.py diff <a> <b>` | bits qui changent entre deux scénarios |
| `tools/json_check.py [--jsonl] <capture>` | conformité des lignes machine au profil (capture série brute, ou fichier `.jsonl`) |
| `tools/tests/` | tests hôte sans carte : `python3 -m unittest` sur des vecteurs synthétiques, et `test_h1.sh` (C++) |

- **Format `.jsonl` :** une ligne par datagramme accepté,
  `{"rx_ms":<heure du Mac>,"de":"<ip>","l":<objet reçu tel quel>}`.
- `hotte_udp.py` accepte un nom mDNS ou une adresse IPv4.
- **Captures de référence :** elles sont versées dans `captures/`, **après avoir
  vérifié qu'elles ne contiennent aucune clé**. `logs/` reste ignoré par git.

## 10. Banc de validation, avant la hotte

Le second C6, avec le firmware `generateur`, imite un bus de 5 V à drain ouvert.
Procédure pas à pas, commandes et résultats : [BANC.md](BANC.md).

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
- **Ligne :** pull-up vers le `5V` du générateur. Deux variantes :
  - 10 k seul ;
  - 4,7 k avec 1 nF vers la masse, pour imiter le filtre supposé de la carte.
- **NPN du générateur :** le montage du §7.3 (4,7 k en base, 10 k entre base et
  émetteur), collecteur directement sur la ligne. Sa logique est inversée : GPIO
  haut = ligne basse. On règle en conséquence le niveau de fin d'émission du RMT
  pour les motifs à repos bas.
- **La ligne entre dans l'étage d'écoute de la sonde.**

**Motifs**, tous à contenu connu :

| Motif | Détail |
|---|---|
| UART | 500, 2 400 et 9 600 bauds, 8N1, repos haut puis repos bas |
| distance d'impulsion, type WTC6534 | T = 750 µs : départ = 2T bas ; « 0 » = 1T haut + 1T bas ; « 1 » = 1T haut + 3T bas. Avec une réponse simulée de la carte 4 T après la trame |
| trames continues, type Krona | octets 8N1 à 500 bauds, inversés, 4 ms de pause entre octets, 18 ms de pause après la trame |
| rafale | un front toutes les 100 µs pendant 10 s, pour mesurer le débit maximal de la capture |
| collision | le générateur émet pendant une injection de la sonde (étage d'injection monté au banc) |

**Critères :**
1. Pour chaque motif, 1 000 trames décodées **au bit près** par
   `analyse.py auto`, sans forcer le décodeur.
2. **Durées à ±2 µs du nominal**, une fois corrigé le décalage de l'étage.
   - Dès l'arrivée de l'analyseur, on mesure ce décalage et l'asymétrie de
     l'étage : une voie sur la ligne à travers 47k/68k, une voie sur GPIO6.
   - Le FX2 est numérique : il donne des instants, pas des tensions. Un relevé
     chiffré des niveaux de la ligne est optionnel, à l'oscilloscope, et
     l'instrument est noté.
   - Asymétrie inférieure à 5 µs, ou documentée et compensée.
3. **Rafale :** aucune perte, ou une perte signalée par `debord`, et le débit
   maximal documenté.
4. **Sur 10 min par le Wi-Fi,** motif Krona, mode `tout`, sonde à son emplacement
   de test :
   - côté sonde, **zéro perte** : `lignes_perdues`, `debord` et `sautes` à 0 ;
   - côté Mac, trous de `n` relevés par `json_check.py` inférieurs à 0,1 %, notés
     avec le RSSI.
5. **Collision détectée et émission arrêtée.** À défaut, la limite est
   documentée (§8.4).
6. Consommation de la sonde relevée, Wi-Fi actif.

## 11. Livrables et arborescence

```
hotte haier/
  README.md
  platformio.ini                 envs sonde et generateur
  src/                           firmware sonde et generateur
  tools/                         hotte_udp.py, analyse.py, json_check.py, git_rev.py, tests/
  captures/                      captures de référence des étapes 5 et 5b
  docs/
    SPEC-RECONNAISSANCE.md       cette spec (et ses avenants)
    BRIEF-RECHERCHE.md           recherche du 24/09 et ses sources
    SECURITE.md                  règles du §5 en fiche, tests d'isolation et de fuite
    RECONNAISSANCE.md            journal des étapes −1 à 7 : mesures, photos, décisions
    PROTOCOL.md                  le protocole D décodé
    PROTOCOLE-JSON.md            profil hotte du protocole compagnon (par différence)
    WIRING.md                    adaptateur, boîtier de mesure, étages, brochage
    BANC.md                      banc de validation (§10) : procédure et résultats
    photos/                      photos de Majid (numéro de série masqué), recadrages
  logs/                          ignoré par git
```

## 12. Achats

| Article | Prix indicatif | Quand |
|---|---|---|
| analyseur logique FX2, 24 MHz, 8 voies (à utiliser avec PulseView) | ~10 € | tout de suite : il conditionne l'étape 5b, donc l'étape 6 |
| connectique JST XH précâblée en 3 et 4 broches (fiches, embases, fils sertis), plus un assortiment PH 2,0 mm au cas où | ~10 € | tout de suite, ou après l'étape 0 |
| fil UL1007 ou UL1015 d'au moins 300 V ; gaine VW-1 ou thermorétractable de 3 à 6 mm | ~10 € | tout de suite |
| plaques à pastilles ; NPN BC547 ou 2N3904 (5 environ) ; résistances 100 k, 68 k, 47 k, 33 k, 15 k, 10 k, 4,7 k, 470 Ω, 220 Ω ; résistance 10 kΩ / 2 W ; condensateur céramique 1 nF (banc, variante 2 du §10) ; barrettes et cavaliers 2,54 mm (`J1`, `J2`) ; barrettes femelles pour la sonde ; colliers | ~10 à 15 € selon le stock | tout de suite |
| grippe-fils (pinces à crochet) pour les cordons du multimètre ; 2 fils à pinces crocodile ; aiguille fine | ~10 € | avant l'étape 1 |
| batterie USB : **vérifier le modèle** avant l'étape 5. Sonde en Wi-Fi alimentée 30 min sans coupure, modèle noté dans `RECONNAISSANCE.md` | déjà là ? | avant l'étape 5 |
| *optionnel :* isolateur USB ADuM3160. Exclu à l'étape 5b, parce que limité au Full Speed | ~10 € | si le Mac sur batterie ne convient pas |

Déjà là : multimètre, fer à souder, C6 SuperMini (sonde et générateur), fils Dupont
pour le banc.

## 13. Risques et points ouverts

| Risque | Parade |
|---|---|
| panneau qui pilote ses voyants **et** résiné | pas de solution propre. On rediscute avec Majid (§4) |
| transport réseau de la ScreenBar pas encore committé | levé : commit de référence `c58a506` (§8.6) |
| analyseur pas encore arrivé | l'étape 6 attend l'étape 5b. Les étapes −1 à 5 avancent sans lui |
| niveau bas de `D` au-dessus du seuil de l'étage | jugé à l'étape 5b (capture propre, concordance avec l'analyseur ; oscilloscope en option), puis seuil relevé (§7.2) |
| `CN3` collé | décoller à l'outil en plastique, sans tirer sur les fils |
| `+` ≈ 12 V | on saute l'étape 4 ; l'étage d'écoute tient tel quel |
| Wi-Fi trop faible au boîtier de mesure | on le déplace. Sinon, enregistrement local en flash, relu par l'USB hotte débranchée (écrit seulement si nécessaire) |
| collision pendant une injection | détection et arrêt (§8.4), validés au banc |
| relais du moteur malmenés pendant les essais | délai minimal imposé par le firmware (§8.4) |
| câble de sortie abîmé ou pincé | règle 5, et contrôle 2b après chaque ouverture |
| anomalie pendant une étape sous tension | règle 12 : on débranche, et on ne rebranche qu'après avoir compris |
| garantie | l'adaptateur est réversible ; la hotte redevient d'origine sans trace |
| « marche » injectée moteur tournant : marche prolongée au lieu de l'arrêt | séquence d'arrêt de l'étape 7 : « marche » seulement après la confirmation de l'arrêt du moteur |
| la fin de la marche prolongée ne laisse aucune trame, ou la carte ne répète pas son état au repos | tests 5 et 6 de l'étape 6 ; au sous-projet 2 : lecture d'état annexe si besoin, jamais de minuterie qui devine la fin (spec du produit, §5.9) |
| objet oublié dans le boîtier électronique après M1 | inventaire compté avant de refermer ; case de la fiche avant chaque mise sous tension |
| Wi-Fi perdu dans le boîtier pendant M1 | mesure Thread directe en P1 par la C6 du banc Matter ; sans elle, branche « moins de 10 dB » |

## 14. Et après

Quand les critères du §1 sont remplis, la conception du **sous-projet 2**
reprend avec le brainstorming, à partir de :
- le protocole et l'architecture choisie ;
- le brief (§4.3 alimentation, §5 exposition Matter, §5.4 piège de la NVS au
  démarrage) ;
- l'essai d'alimentation par `CN3 +` ;
- le choix de la carte C6 ;
- la lecture d'état annexe, si le retour d'état l'exige ;
- les transitions et les trames de la marche prolongée (étapes −1b et 5), et
  l'état répété au repos (tests 5 et 6 de l'étape 6) ;
- la mesure M1 (carte et antenne).
