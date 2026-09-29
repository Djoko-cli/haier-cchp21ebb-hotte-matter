# Fiche de sécurité

**Sous-projet 1 : reconnaissance de la ligne `D`.** À imprimer et à garder sous
les yeux à chaque séance. Cette fiche reprend les règles 1 à 13 du §5 de
[SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md), puis les trois contrôles
d'isolement pas à pas (étapes 1, 2b et 3a du §6). **La spec fait foi** : si la
fiche la contredit, on corrige la fiche.

Montage et points de test : [WIRING.md](WIRING.md). Relevés, décisions et
anomalies : [RECONNAISSANCE.md](RECONNAISSANCE.md).

## Les 13 règles

**Débrancher, attendre, ne pas passer la main**

1. **Pour toute intervention, on débranche la fiche et on la garde à vue, hors
   de portée d'autrui.**
   - Avant d'ouvrir le boîtier électronique ou d'y passer la main : **5 minutes**
     d'attente. Côté secteur, E5 et E6 (4,7 µF / 450 V) n'ont pas forcément de
     résistance de décharge.
   - Intervention limitée au boîtier de mesure extérieur : fiche débranchée, sans attente.
2. **Avant la première mise sous tension d'un montage, on teste le différentiel.**
   On le teste juste avant de poser ce montage, hotte dans son état précédent,
   déjà contrôlé ; puis on débranche pour poser. Hotte branchée, lumière
   allumée, bouton T : **la lumière doit s'éteindre**. Sinon, la hotte n'est pas
   derrière ce différentiel : **ARRÊT**. Réarmer.
3. **Dans le boîtier électronique, on ne touche qu'à `CN3` et au câble du panneau.**
   - Hotte débranchée depuis 5 minutes, on peut écarter à la main les fils isolés
     du condensateur moteur et le connecteur blanc en ligne, sans les débrancher
     ni tirer dessus. Photo avant, remise en place après.
   - Jamais `CN1` ni `CN2` dans ce sous-projet.
   - Zones interdites ([photos/1.jpg](photos/1.jpg), [photos/5.jpg](photos/5.jpg),
     [photos/6.jpg](photos/6.jpg), [recadrages/c4_bottom.jpg](photos/recadrages/c4_bottom.jpg)) :
     la moitié gauche de la carte (primaire) et le transformateur ; toute la bande
     du bas (relais, `VH2` à `VH6`, « LAMP », `AC-POWER`) ; le condensateur moteur.
   - Mesure M1 (spec §6) : on pose en plus, dans le quart haut-droit, la sonde
     dans son sachet et sa batterie. Rien d'autre n'est touché. Rien n'appuie
     sur un composant, et rien n'entre dans une zone interdite.
4. **Aucune connexion ne se pose ni ne se retire sous tension :** connecteur,
   pince, résistance, cavalier, sonde, câble USB.
5. **Câble de l'adaptateur :**
   - fil d'au moins 300 V (UL1007 ou UL1015, 105 °C de préférence) dans le corps
     de la hotte et au passage de sortie ;
   - gainé **sur toute sa longueur** : gaine équivalente à celle d'origine
     (VW-1 600 V), ou deux épaisseurs de gaine thermorétractable ;
   - fixé au câble du panneau, jamais serré contre les fils du moteur ni contre
     le connecteur blanc ;
   - pincé ni par le couvercle du boîtier, ni par le filtre.

   **Couvercle fermé et filtre remis avant de rebrancher.**
6. **Aucune main dans la hotte sous tension.** Sous tension, on ne touche qu'aux
   touches du panneau, à la fiche, et au boîtier de mesure extérieur, où l'on lit
   les instruments.

**Portes d'isolement**

7. **Avant toute mise sous tension d'un montage, trois contrôles passent, dans l'ordre :**
   1. le test d'isolation (étape 1) ;
   2. son nouveau passage depuis les points de test (étape 2b), après la pose de
      l'adaptateur et après **chaque** ouverture du boîtier ;
   3. la mesure de fuite en charge (étape 3a), **avant tout Mac ou analyseur**.
8. **Mac et analyseur :** on ne les relie à la hotte sous tension qu'après les trois
   contrôles de la règle 7, et dans ces conditions :
   - pendant toute la session, **Mac sur batterie et sans aucun autre câble** :
     ni chargeur, ni écran, ni Ethernet, ni hub alimenté, ni audio ;
   - on ne rebranche le chargeur qu'après avoir débranché la hotte ou retiré
     l'analyseur ;
   - Mac posé sur une surface isolante, loin de l'acier de la hotte et de l'évier ;
   - on ne touche jamais en même temps le Mac et une pièce métallique reliée à la terre.

**Sonde et émission**

9. **Pendant les captures sous tension, la sonde est sur batterie USB**, d'un
   modèle vérifié (§12 de la spec), et ses journaux partent par Wi-Fi. Sa seule
   liaison avec la hotte est le câble de sortie de l'adaptateur.
10. **Jamais une tension du bus directement sur une GPIO** : le C6 ne tolère pas
    plus d'environ 3,6 V. On passe toujours par l'étage d'écoute (WIRING.md §5).
11. **Toute émission vers la hotte (étape 7) se fait en présence de Majid.** Il se
    tient à portée de la fiche, voit le panneau, et débranche au moindre doute.

**En cas de problème**

12. **À la moindre anomalie, on débranche la fiche.** Anomalies : déclenchement
    du différentiel ou du disjoncteur, odeur, fumée, claquements de relais
    répétés, bip continu, panneau mort, fil chaud. On ne réarme pas et on ne
    rebranche pas avant d'avoir compris la cause, notée dans `RECONNAISSANCE.md`.
13. **Pas de cuisson tant que la sonde et sa batterie sont en place.** On les
    retire entre deux sessions. L'adaptateur, gainé, avec `J1` fermé, peut rester
    en place.
    - M1 : **jamais la hotte branchée avec la sonde ou sa batterie dans le
      boîtier électronique.**

> **Une lecture qui ne prouve rien.** Entre le secondaire flottant et la terre,
> un multimètre de 10 MΩ peut afficher à vide jusqu'à une centaine de volts
> alternatifs, à cause des condensateurs Y. Ce chiffre ne prouve rien, ni dans un
> sens ni dans l'autre : un défaut franc donnerait à peu près la même lecture.
> **Seule compte la mesure en charge de l'étape 3a.**

## Avant chaque mise sous tension

- [ ] Étape 1 réussie (une fois pour toutes).
- [ ] Étape 2b refaite depuis la dernière ouverture du boîtier (règle 7).
- [ ] Étape 3a réussie (sauf pour l'essai à vide de l'étape 2c, qui la précède) ;
      toujours avant de relier un Mac ou l'analyseur (règle 7). Lecture entre 3 et
      5 V : rediscutée avant de relier un Mac ou l'analyseur.
- [ ] Montage jamais mis sous tension : différentiel testé juste avant de le
      poser, hotte dans son état précédent (règle 2).
- [ ] Toutes les connexions posées hotte débranchée (règle 4).
- [ ] Couvercle fermé, filtre remis, câble de sortie ni pincé ni serré contre les
      fils du moteur (règle 5).
- [ ] `J2` ouvert (sauf étape 4, qui se fait hotte débranchée).
- [ ] Captures : sonde sur batterie ; sa seule liaison avec la hotte est le câble
      de sortie (règle 9).
- [ ] Mac ou analyseur relié : Mac sur batterie, sans aucun autre câble, sur une
      surface isolante (règle 8).
- [ ] Étape 7 : Majid devant la hotte, à portée de la fiche (règle 11).
- [ ] Après M1 : rien d'oublié dans le boîtier électronique (ni sonde, ni
      batterie, ni sachet).

## Banc de validation : aucune liaison avec la hotte

Le banc (WIRING.md §10) n'a **jamais** de liaison avec la hotte : ni le
générateur, ni la sonde, ni le Mac. La sonde y est :
- soit sur le boîtier de mesure, seulement **hotte débranchée pendant toute la
  séance** et **câble de sortie déconnecté du boîtier** (fiche XH 4 broches
  retirée de l'embase, hotte débranchée). Avant de brancher le moindre USB,
  contrôle hors tension, au calibre le plus élevé : `TP−` vers la terre de la
  fiche de la hotte (le trou qui reçoit la broche de terre), puis vers la vis de
  la carcasse : **OL** les deux fois. Sinon, on ne branche rien ;
- soit sur une plaque d'essai du banc, avec son propre étage d'écoute (mêmes
  valeurs, WIRING.md §5).

Le boîtier ne retourne sur la hotte (fiche XH 4 broches remise, hotte
débranchée) qu'une fois le banc démonté, et après ses contrôles avant pose
(WIRING.md §12, lignes 11 à 17).

## En cas d'anomalie

1. **Débrancher la fiche**, tout de suite, et la garder à vue. Si le différentiel
   ou le disjoncteur a déclenché : ne pas réarmer.
2. Ne toucher ni la hotte ni le boîtier de mesure avant d'avoir débranché (règle 6).
3. Noter aussitôt dans le journal des anomalies de `RECONNAISSANCE.md` : l'heure,
   l'étape, ce qui était branché, ce qu'on a vu, entendu ou senti.
4. Chercher la cause hotte débranchée. Pour ouvrir le boîtier : 5 minutes
   d'attente (règle 1), puis refaire l'étape 2b avant de rebrancher (règle 7).
5. On ne réarme et on ne rebranche qu'une fois la cause comprise et notée (règle 12).

**Quand un contrôle dit ARRÊT :** on ne raccorde rien et on ne met pas le montage
sous tension ; on note la valeur dans `RECONNAISSANCE.md` ; on revient à la
conception avant d'aller plus loin.

## Procédure : test du différentiel (règle 2)

**Quand :** avant la première mise sous tension de chaque montage, juste avant
de le poser. On le fait hotte dans son état précédent, déjà contrôlé, puis on
débranche pour poser le nouveau montage : avant de poser l'adaptateur (sa
première mise sous tension est l'étape 2c), puis avant la fuite en charge (3a),
la sonde (5), l'analyseur (5b) et l'étage d'injection (7).

1. Brancher la hotte, allumer la lumière au panneau.
2. Appuyer sur le bouton T du différentiel 30 mA.
3. **La lumière doit s'éteindre.** Sinon : la hotte n'est pas derrière ce
   différentiel, **ARRÊT**.
4. Réarmer. Noter la date dans `RECONNAISSANCE.md` (tests du différentiel).
5. Débrancher la hotte, puis poser le nouveau montage (règle 4).

## Procédure : étape 1, test d'isolation

- **Quand :** une fois, avant de construire l'adaptateur.
- **État :** hotte débranchée ; on ouvre le boîtier, donc débranchée depuis
  **5 minutes** (règle 1).
- **Outils :** multimètre, fil à pinces crocodile, aiguille fine.

1. Débrancher la fiche, noter l'heure, attendre 5 minutes.
2. Retirer le filtre, ouvrir le couvercle du boîtier électronique. On ne touche
   qu'à `CN3` et au câble du panneau (règle 3).
3. Relier les deux broches rondes de la fiche par le fil à pinces crocodile.
4. Piquer `CN3` par l'arrière de la fiche, côté fils, avec l'aiguille, sur la
   broche à mesurer. Si la colle de la sortie des fils gêne (étape 0, ligne
   2a), retirer plutôt la fiche du panneau, en la tenant par son boîtier, et
   poser la pointe du cordon sur la broche de `CN3` elle-même : même mesure,
   le panneau n'étant relié à rien d'autre qu'à `CN3`. Remettre la fiche au
   point 7.
5. Ne toucher aucune partie métallique des pointes : le corps en parallèle
   fausserait la mesure.
6. Mesurer, et noter chaque lecture dans `RECONNAISSANCE.md` :

| # | Mesure | Calibre | Attendu | Sinon |
|---|---|---|---|---|
| a | broches reliées de la fiche vers `CN3 −`, puis vers `D`, puis vers `+`, chaque fois dans les deux sens (cordons inversés) | le plus élevé (20 MΩ ou plus) | **OL** dans les deux sens, après 10 s. Une valeur qui monte vers OL est normale : les condensateurs Y se chargent | toute valeur stable : **ARRÊT**, aucun raccordement, retour à la conception |
| b | contact de terre de la fiche (dans le trou qui reçoit la broche de terre de la prise) vers `CN3 −` | le plus élevé | OL : secondaire flottant | environ 0 Ω : secondaire relié à la terre ; ce n'est pas un défaut, on le note. Valeur intermédiaire (kΩ à MΩ) : **ARRÊT** et analyse |
| c | contact de terre de la fiche vers la vis de terre de la carcasse, ou une vis nue (la carcasse est peinte) | 200 Ω ; cordons court-circuités d'abord (valeur à soustraire) | **moins de 1 Ω** : liaison de classe I | **ARRÊT** : défaut de terre |

7. Retirer l'aiguille et le fil à pinces (fiche du panneau remise sur `CN3`,
   si elle a été retirée), refermer le couvercle, remettre le filtre sans rien
   pincer.

**Limite.** Un ohmmètre ne voit pas un claquage qui n'apparaîtrait qu'à 230 V.
La mesure en charge de l'étape 3a couvre une partie de ce risque.

**Sortie :** feu vert pour construire l'adaptateur, ou **ARRÊT**.

## Procédure : étape 2b, isolation depuis les points de test

- **Quand :** après la pose de l'adaptateur (étape 2), après **chaque** ouverture
  du boîtier (règle 7), et à la sortie de l'étape 4 (couvercle fermé et filtre
  remis d'abord).
- **État :** hotte débranchée, couvercle fermé, filtre remis ; fiche XH 4 br. du
  câble de sortie enfichée sur le boîtier de mesure (retirée, les points de test
  liraient OL sans rien prouver).
- **Outils :** multimètre, fil à pinces crocodile, grippe-fils.

**Mesure 1 : isolation (à chaque passage).**

1. Relier les deux broches rondes de la fiche par le fil à pinces crocodile.
2. Sur le boîtier de mesure, mesurer depuis les broches reliées vers chaque point
   de test, dans les deux sens :

| # | Mesure | Calibre | Attendu | Sinon |
|---|---|---|---|---|
| 1 | broches reliées ↔ `TP−` | le plus élevé (20 MΩ ou plus) | **OL** dans les deux sens, après 10 s | **ARRÊT** |
| 2 | broches reliées ↔ `TP+` | idem | **OL** | **ARRÊT** |
| 3 | broches reliées ↔ `TP_Dp` | idem | **OL** | **ARRÊT** |
| 4 | broches reliées ↔ `TP_Dc` | idem | **OL** | **ARRÊT** |

3. Retirer le fil à pinces.

**Mesure 2 : où est le pull-up de `D` (premier passage seulement).**

1. Hotte débranchée, filtre retiré, sans ouvrir le boîtier : ouvrir `J1`.
2. Mesurer, au calibre 20 kΩ ou automatique, dans les deux sens :
   `TP_Dp` vers `TP+`, `TP_Dp` vers `TP−`, puis `TP_Dc` vers `TP+`, `TP_Dc` vers `TP−`.
   Quelques kΩ vers `+` indiquent un pull-up, et son côté : `TP_Dp` côté panneau,
   `TP_Dc` côté carte. Entre `TP_Dp` et `TP−`, l'étage d'écoute ajoute de l'ordre
   de 100 à 200 kΩ selon le sens : cette lecture-là n'indique rien.
3. Refermer `J1`. Remettre le filtre sans rien pincer.

**Sortie :** tout est OL, le côté du pull-up est noté. Sinon, **ARRÊT**.

## Procédure : étape 3a, fuite en charge

- **Quand :** une fois, après les étapes 2b et 2c, **avant tout Mac ou
  analyseur** (règle 7).
- **État :** hotte débranchée pour poser les pinces ; une brève mise sous tension
  pour la lecture.
- **Outils :** multimètre, deux grippe-fils, résistance 10 kΩ / 2 W.

La résistance charge la mesure : la lecture donne le vrai courant de fuite, là
où la lecture à vide ne prouve rien (encadré plus haut).

1. Tester le différentiel (procédure plus haut), puis débrancher.
2. Hotte débranchée, fiche XH 4 br. du câble de sortie enfichée sur le boîtier
   (retirée, la lecture ne prouverait rien) : pince COM sur `TP−`, l'autre pince
   sur la vis de terre de la carcasse (celle de l'étape 1, ligne c).
3. Poser la résistance de 10 kΩ / 2 W en parallèle entre les deux pinces.
   Multimètre en tension alternative.
4. Brancher la hotte, lire dans les 5 s, débrancher.
5. Courant de fuite = tension lue / 10 kΩ.
6. Hotte débranchée : retirer les pinces et la résistance.

| Lecture | Décision |
|---|---|
| 3 V au plus (0,3 mA au plus) | attendu : on continue |
| entre 3 et 5 V | au-dessus de l'attendu : on le note, et on en rediscute avant de relier un Mac ou l'analyseur |
| au-delà de 5 V (0,5 mA), ou différentiel déclenché | **ARRÊT** |
