# Brief de conception : hotte Haier CCHP21EBB en Matter over Thread (ESP32-C6)

> **Document de recherche du 24/09/2026**, produit par un workflow de 8 agents (5 axes de recherche, synthèse, revue adverse, révision). Il est **antérieur aux réponses de Majid** : ses questions (§7) et l'hypothèse « pas de soudure, pas d'achat » sont dépassées. Les décisions qui priment sont dans [SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md) : soudure et achats autorisés, hotte sur prise accessible avec différentiel 30 mA, multimètre disponible, voyants du panneau synchronisés obligatoires.

**Pour :** l'ingénieur principal, qui en discutera avec l'utilisateur. **Date :** 2026-09-24, révision 2, après la revue adverse. **Statut :** avant-projet. Personne n'a encore rien mesuré sur la hotte.

**Niveaux de preuve :**
- **V** (vérifié) : photo de l'utilisateur, code lu ou source citée.
- **P** (probable) : plusieurs indices concordent, sans preuve directe.
- **S** (spéculatif ou déduit) : hypothèse ou calcul. Rien n'a été mesuré.
- **?** (inconnu) : reste à mesurer.

**Références :**
- Photos : `photos/N.jpg` (dans `docs/`) ; recadrages (`c1_left.jpg`, `c6_right.jpg`, `z_cn3_6.jpg`…) : `photos/recadrages/`. Le numéro de série de l'étiquette (`photos/7.jpg`) est masqué.
- Sources `[Sx]` et fichiers locaux `[Lx]` : voir l'annexe.

**En résumé.**
- **La carte.** La carte de puissance XB_DYB_V4 est une carte OEM chinoise, la même pièce que celle de la hotte CDA EVA60BL/1. Elle porte le MCU de commande, le ULN2003, les relais et le buzzer.
- **Le panneau.** Le panneau tactile contient forcément son propre contrôleur tactile. Il peut piloter ses voyants, voire détenir l'état de la hotte. On ne sait pas encore lequel des deux est maître (§1.2, n° 24).
- **La liaison.** Le panneau parle à la carte par un seul fil, D, dans le faisceau 3 fils CN3 « - D + ». CN3 est très probablement du côté basse tension isolé, mais cet isolement n'a pas été mesuré.
- **Zone sûre.** Seul le quart haut-droit de la carte est en très basse tension. Des fils au potentiel du secteur (ceux du condensateur moteur) passent juste au-dessus, et une bande secteur longe tout le bas de la carte.
- **Voie recommandée.** Se brancher sur ce faisceau par un passe-plat sorti du boîtier, sans jamais toucher au secteur. Dans l'ordre :
  1. observer la hotte en usage ;
  2. écouter seulement, à travers un étage d'entrée à haute impédance ;
  3. selon le protocole observé, choisir l'injection en parallèle ou l'homme du milieu.
- **Si D ne transporte pas l'état de la carte,** il faudra une lecture d'état complémentaire.

---

## 1. Ce qu'on sait, ce qu'on suppose, ce qu'on ignore

### 1.1 Corrections aux premières observations

| Observation initiale | Correction | Preuve |
|---|---|---|
| Deux circuits SOP empilés, l'un d'environ 16 à 20 broches | IC2 est un **SOP-14** sans marquage lisible, probablement le MCU. IC3 est un **ULN2003** en SOP-16. À confirmer par une photo macro de la carte de l'utilisateur. | [S4] (photos haute résolution de la même carte) |
| « Embase 2 broches vide sous CN3 » | C'est **CN2 « - + »**. Le boîtier est monté mais rien n'y est branché. L'emplacement vide se trouve encore en dessous. | crop `c6_right.jpg`, [S4] |
| Transformateur « BK-…2232 » | La référence complète est **BK-22-2232**. | [S3][S4] |
| Condensateur moteur de 5 µF / 400 V d'après l'étiquette | À la résolution de la photo, l'étiquette se lit mal (400 ou 450 V). La pièce réelle est un **CBB61 de 5 µF, 450 V~**. Sans effet sur la conception. | `photos/5.jpg`, `6.jpg`, `7.jpg` |
| Rôle Thread MED, comme sur la BenQ | Sur la BenQ, le MED avait été forcé par un wrap de l'éditeur de liens. Sans ce wrap, le nœud peut devenir routeur (voir §5.5). | [L4][L5][S37] |
| « Moitié droite de la carte = côté secondaire » (révision 1 de ce brief) | **Faux.** Seul le **quart haut-droit** est en très basse tension : CN3, CN2, CN1, IC2/IC3, buzzer, E1, IC4. Les deux fils noirs du condensateur CBB61 et le gros connecteur blanc en ligne, au potentiel du secteur, reposent sur le transformateur, IC4 et le buzzer, à quelques centimètres de CN3. La bande basse longe la carte jusqu'au bord droit, sous CN1 : VH2 à VH6, sérigraphie « LAMP », AC-POWER, côté contacts des relais. | `photos/1.jpg`, `5.jpg`, `6.jpg`, `c4_bottom.jpg` |
| VH4 « L M H N » vérifié sur `c4_bottom.jpg` (révision 1) | Sur les photos de l'utilisateur, seul « VH4 » est lisible. 4 ou 5 fils y arrivent (bleu, rouge, jaune, blanc/gris). La sérigraphie « L M H N » ne vient que de [S4]. | `c4_bottom.jpg`, [S4] |
| Fiche CN3 débranchable directement | Un dépôt translucide semble la recouvrir, comme la fiche CN1 « 12V LED ». En l'arrachant, on risque de décoller l'embase de la carte. | `c6_right.jpg`, `z_cn3_6.jpg` (visuel, à confirmer) |

Les corrections qui portent sur les sources (WTC6534, Krona, [S32]) sont intégrées directement aux §2, §3 et §5.

### 1.2 Tableau de synthèse

| # | Élément | Niveau | Preuve |
|---|---|---|---|
| 1 | Hotte inclinée de 90 cm, 700 m³/h, 3 vitesses, commande tactile, éclairage LED, garantie 2 ans | V | [S1] |
| 2 | Aucun manuel CCHP21EBB ni CCHP20EBB en ligne. La sémantique exacte des touches n'est donc pas documentée. | V (recherche sans résultat) | [S1][S2] |
| 3 | Étiquette : 220-240 V~ 50 Hz, 183 W (lampe 1×3 W max), classe I, fabrication 06/2024, importateur SNC Centrale 97450 St-Louis | V | `photos/7.jpg` |
| 4 | La carte XB_DYB_V4 a la même référence que la « CDA EVA60BL/1 main board 0010902281 BK-22-2232 » : même sérigraphie, même transformateur, même étiquette LIAO YUAN/0010902281, même implantation | V | [S3][S4], `photos/1-6.jpg`, `c5_center.jpg` |
| 5 | Plateforme OEM commune à Haier (La Réunion) et CDA (groupe Amica). Fabricant probable : Zhongshan Liaoyuan | P | [S5][S50][S7] |
| 6 | Alimentation flyback isolée. **Primaire :** fusible 3,15 A, X2, self de mode commun, varistance, MB10S, 2×4,7 µF/450 V, IC1, snubber RCD, BK-22-2232. **Isolement :** PC817C et CY1/CY2. **Secondaire :** une diode zener en verre, à côté de la sérigraphie « 4741 » (1N4741, 11 V), est visible sur la carte de l'utilisateur, près de CY1 et OP1. D'où un secondaire d'environ 12 V. | V (composants, sérigraphie) / P (12 V) | `c1_left.jpg`, `c5_center.jpg`, `z_ycaps4.jpg`, [S4][S10][S54] |
| 7 | **Isolement réel du secondaire par rapport à L, N et PE** | **?** (jamais mesuré) | — |
| 8 | IC4 (SOP-8) avec SS14 (1 A) et L1 « 680 » (68 µH) : abaisseur de 12 V vers probablement 5 V, dimensionné pour 1 A au plus | P | [S4][S53], `c6_right.jpg` |
| 9 | IC3 est un ULN2003 (7 sorties Darlington de 500 mA) qui pilote les relais | V (marquage) / P (rôle) | [S4][S9] |
| 10 | IC2, SOP-14 non marqué, est le MCU de la carte | P | [S4] |
| 11 | Q1 « 8550 » près du buzzer, Q2 « 8050 ». Le buzzer SP1 « ZLFY » est **sur la carte de puissance** | V (marquages) / P (rôles) | [S4], `c1_topright.jpg` |
| 12 | Autour de CN3 : R1 1 k, R13 4,7 k et un condensateur de 1 nF. Probablement une résistance série, un pull-up et un filtre sur D | S | [S4] |
| 13 | CN3 est sérigraphié « - D + ». Noir = -, blanc = D, rouge = +, vers le panneau avant. La fiche semble recouverte d'un dépôt translucide, comme CN1 | V / V visuel, à confirmer | `c6_right.jpg`, `z_cn3_6.jpg`, `photos/6.jpg` |
| 14 | Les connecteurs basse tension sont des JST XH (pas de 2,5 mm) | P (à mesurer) | `c6_right.jpg`, [S22] |
| 15 | CN1 « 12V_LED » alimente la lampe XB-E06-T02 (12 V DC, 3 W) | V | `photos/3.jpg`, `6.jpg` |
| 16 | La sortie 12V_LED est **commutée**, ce n'est pas une alimentation permanente | P | `photos/6.jpg`, `c4_bottom.jpg` |
| 17 | CN2 « - + » est libre. Probablement une 2ᵉ sortie lampe (la CDA a 2 lampes) | V (présence) / S (fonction) | `c6_right.jpg`, [S5] |
| 18 | 3 relais LIAO YUAN (L, M, H). Le 4ᵉ relais « LAMP » (RE1, VH5/VH6) n'est pas monté. VH4 porte la sérigraphie « L M H N » d'après [S4]. Sur les photos de l'utilisateur, seul « VH4 » est lisible, avec 4 ou 5 fils. | V ([S4]) / V (photo, partiel) | [S4], `c4_bottom.jpg` |
| 19 | Relais SPST ou SPDT (verrouillage matériel des prises moteur ?) | ? | — |
| 20 | Condensateur moteur CBB61 de 5 µF, 450 V~ | V | `photos/5.jpg`, `6.jpg` |
| 21 | Le panneau CDA est un bloc scellé et résiné à 5 touches, relié par un câble 3 fils à une fiche 3 broches. Aucune carte n'est accessible. Le fabricant présumé vend un panneau tactile à 5 touches (Power, Light, Speed 1 à 3), annoncé « AC 250V / DC 12V », sans préciser à quoi s'applique chaque valeur. | V pour la CDA et la fiche / P pour la Haier | [S6][S7] |
| 22 | **Tension de CN3 « + » (5 ou 12 V).** Les indices se contredisent : un WTC6534 s'alimente entre 2,7 et 5,5 V [S11], la fiche Liaoyuan cite « DC 12V » [S7]. | **?** | — |
| 23 | **Nature de D** : sens (unidirectionnel ou bidirectionnel), codage (UART, largeur ou distance d'impulsion), trafic continu ou sur événement, sortie open-drain ou push-pull | **?** | familles possibles : [S11][S13][S14][S15] |
| 24 | **Qui détient l'état, la carte ou le panneau ?** Le panneau contient forcément un contrôleur tactile. Il peut piloter ses propres voyants (WTC6534 : broches D1 à D6 [S11]) ou envoyer en continu l'état complet (Krona [S13]). Indice faible pour la carte : le buzzer est sur la carte. | ? | S |
| 25 | Comportement de la CDA : une touche vitesse démarre directement. La touche arrêt donne 15 min de marche prolongée sur 1 appui, l'arrêt sur 2 appuis. | V pour la CDA | [S8] |
| 26 | Comportement de la Haier : touche marche, minuterie, état après coupure | ? (diffère au moins en partie de la CDA, d'après l'utilisateur) | utilisateur, étape −1 |
| 27 | **Besoin en courant du C6 :** 74 mA en réception 802.15.4 ; pics de 305 mA en émission à 20 dBm, 187 mA à 12 dBm, 119 mA à 0 dBm ; 315 mA en émission BLE à 20 dBm. **La marge du secondaire est inconnue.** | V (besoin) / ? (marge) | [S18] tableaux 5-8 et 5-9 |
| 28 | Apple Home liste la catégorie Matter « Fan ». « Extractor Hood » (0x007A) n'y figure pas. | V / P | [S33][S34] |
| 29 | Liaison radio Thread depuis l'intérieur du caisson en acier | ? | — |
| 30 | **Zone très basse tension limitée au quart haut-droit.** Des fils au potentiel du secteur (condensateur CBB61, connecteur blanc en ligne) passent au-dessus. Une bande secteur longe tout le bas jusqu'au bord droit. | V | `photos/1.jpg`, `5.jpg`, `6.jpg`, `c4_bottom.jpg` |
| 31 | **La carte émet-elle son état (voyants, vitesse) sur D, ou D ne porte-t-il que les appuis ?** Avec un WTC6534, le paquet de voyants envoyé par la carte est facultatif, et peut n'être envoyé qu'aux changements. | ? / V (cas WTC) | [S11] §3.2 |
| 32 | Dans esp_matter 1.5.1, les attributs FanMode et OnOff sont persistants (NVS) et relus à la création de l'endpoint. PercentSetting et PercentCurrent ne le sont pas. | V (code) | [S57][L7][L11] |

---

## 2. Architectures possibles

### A. Écoute et injection en parallèle sur D (bus partagé)
- **Principe.** L'ESP lit D en haute impédance. Pour envoyer une commande, il tire D à la masse en drain ouvert pendant un silence et émet la trame d'une touche, comme le ferait le panneau.
- **Conditions :**
  - D est open-drain avec un pull-up ;
  - le trafic se fait sur événement, ou laisse des silences exploitables ;
  - le protocole n'impose pas de réponse dans un délai court ;
  - l'étage d'entrée est à haute impédance (§4.1).
- **Avantages :**
  - Si l'ESP est éteint ou planté, le bus reste libre et le panneau marche normalement. Cela suppose un étage d'entrée à haute impédance (tampon NPN ou diviseur de 100 kΩ ou plus) et un transistor d'injection bloqué par défaut. Un diviseur 10k/20k ne remplit pas cette condition (§4.1).
  - Les bips restent cohérents, puisque c'est la carte qui traite les commandes. Les voyants aussi, si c'est la carte qui les pilote.
  - Rien à couper.
- **Inconvénients :**
  - Collisions possibles si le panneau parle en même temps.
  - Impossible si D est en push-pull, ou si le panneau émet en continu. Deux exemples :
    - **WTC6534** : T = 750 µs. Le panneau émet une trame de touches, attend 8 T, puis la réémet si la carte ne répond pas. La réponse de la carte (paquet de voyants) est facultative ; quand elle existe, elle arrive entre 4 et 6 T [S11].
    - **Krona** : trames d'état répétées toutes les 18 ms environ [S13].
  - En mode UART one-wire, l'ESP entend son propre écho [S19].
  - L'état réel ne remonte que si la carte l'émet sur D. Sinon, voir l'étape 6, cas « D unidirectionnel ».

### B. Homme du milieu (D coupé)
- **Principe.** D est coupé et passe par l'ESP. Côté panneau, l'ESP se fait passer pour la carte ; côté carte, il se fait passer pour le panneau. Il relaie les vraies touches, insère les touches virtuelles et lit l'état dans les trames de la carte, **si la carte en émet**. Exemple du WTC6534 : la carte peut lui envoyer un paquet de voyants de 8 bits (6 de données, 2 de contrôle), mais ce paquet est facultatif et parfois réservé aux changements [S11]. Précédent : les machines à café Philips [S17].
- **Avantages :** fonctionne quel que soit le type de bus. Pas de collisions.
- **Inconvénients :**
  - **Point de défaillance unique** : si l'ESP plante, le panneau ne commande plus rien. Il faut un contournement (relais de signal au repos fermé ou cavalier) et un watchdog.
  - Contrainte temps réel si l'ESP doit répondre au panneau à la place de la carte : de 4 à 6 T, soit 3 à 4,5 ms, dans le cas WTC [S11]. Pendant ce temps, Thread et BLE génèrent des interruptions.
  - Plus de code.
- **Variante écartée : remplacer le panneau par l'ESP**, comme le projet Krona [S13]. On perdrait le panneau.

### C. Simulation des touches et écoute passive
- **Principe.** Un interrupteur à très faible capacité par touche, entre l'électrode et la masse du panneau, par exemple un PhotoMOS AQY221N2S à 1 pF [S41]. L'état est lu en écoutant D.
- **Avantages.** Ne dépend pas du protocole. Le panneau reste fonctionnel si l'ESP tombe.
- **Inconvénients :**
  - Il faut ouvrir le panneau. Il est probablement **résiné** comme celui de la CDA [S6], donc c'est peut-être impossible.
  - Environ 6 fils à souder et 5 PhotoMOS à acheter.
  - Composants à éviter :
    - l'AQY212 : 50 à 75 pF à l'état ouvert, la vraie touche risque de ne plus réagir [S41][S43] ;
    - le CD4066 alimenté en 5 V : il demande 3,5 V sur sa commande [S42].
  - Le circuit tactile se recalibre : appuis de 100 à 300 ms, jamais maintenus [S40].
  - Succès variable selon les appareils [S52].

### D. Remplacement du cerveau (ESP et relais sur les prises moteur)
- **Travail sur le secteur.** On perd le panneau d'origine.
- **Verrouillage matériel obligatoire** : alimenter deux prises moteur en même temps met une partie du bobinage en court-circuit [S44]. Un verrouillage logiciel ne suffit pas [S44].
- La hotte sort de la conformité EN 60335-2-31 [S47].

### E. Boîtier du commerce
- **Shelly Gen4** : Matter uniquement sur Wi-Fi, Thread « non prévu » [S45]. Il apparaît comme des interrupteurs, pas comme un ventilateur.
- **Sonoff iFan04** : ESP8266, pas de Matter natif. Il règle la vitesse par condensateurs sur une seule sortie. Sa sortie lampe est en secteur, alors que la lampe de la hotte est en 12 V DC [S46].
- **Tous ces boîtiers** doivent prendre la main sur les prises moteur. Le panneau perd alors la commande du ventilateur.

### F. Aucune intrusion (doigt robot ou électrode adhésive)
Boucle ouverte, visible et exposée à la graisse [S51]. Dernier recours.

### G. Prise connectée Matter over Thread en amont (aucune intrusion)
- **Principe.** Couper et rétablir l'alimentation de la hotte.
- **Conditions.** La hotte est branchée sur une prise (Q2) et reprend sa vitesse et sa lumière après une coupure (Q3g). À vérifier à l'étape −1.
- **Avantages.** Aucune ouverture, aucun risque lié au secteur, Thread natif.
- **Inconvénients :**
  - un achat ;
  - marche et arrêt seulement, à la dernière vitesse, lumière comprise ;
  - pas de choix de vitesse, pas de lumière seule ;
  - aucun état réel remonté ;
  - la hotte apparaît comme une prise dans Home, pas comme un ventilateur (S).

### Comparatif

| Critère | A. Parallèle | B. Homme du milieu | C. Touches | D. Cerveau | E. Commerce | F. Robot | G. Prise |
|---|---|---|---|---|---|---|---|
| Travail sur le secteur | non | non | non | **oui** | **oui** | non | non |
| Réversible | total | total | partiel | faible | faible | total | total |
| Soudure | non | non | **oui** | oui/cosses | cosses | non | non |
| Achats | 0 à quelques € | contournement éventuel | PhotoMOS | relais, alim | boîtier | robots, pont | prise Thread |
| Panneau d'origine conservé | oui | oui (tant que l'ESP marche) | oui | **non** | **non** (ventilateur) | oui | oui |
| État réel remonté | si la carte l'émet sur D, sinon lecture annexe | idem A | idem A (par l'écoute) | ESP maître | partiel | non | non |
| Ventilateur Matter natif sur Thread | oui | oui | oui | oui | **non** | via un pont | **non** (prise) |
| Si l'ESP tombe | inoffensif (entrée à haute impédance) | panneau mort sans contournement | inoffensif | hotte morte | — | — | — |

**Recommandation.** Passer par la ligne D. Les premières étapes, l'observation d'usage puis l'écoute passive, sont communes à A et à B. On choisit ensuite selon le protocole mesuré en §3 :
- **A** si D est open-drain avec des silences exploitables. C'est le choix préféré, parce qu'il reste sûr si l'ESP tombe.
- **B** s'il y a un trafic continu (type WTC ou Krona), ou si D est en push-pull. Le contournement matériel est alors obligatoire.
- **Si D ne porte pas l'état de la carte**, on ajoute une lecture d'état annexe (étape 6).

Les autres options :
- **C** est la solution de repli, seulement si le panneau s'ouvre et si le protocole s'avère indéchiffrable.
- **G** est l'alternative minimale. Elle suppose que l'étape −1 montre une reprise d'état après coupure, et qu'un achat soit accepté. Elle ne donne ni les vitesses ni la lumière séparée.
- **D, E et F sont écartées.** Seules A et B respectent à la fois ces conditions :
  - pas de travail sur le secteur ;
  - pas de soudure ;
  - montage réversible ;
  - un ventilateur Matter natif ;
  - le panneau d'origine conservé.

---

## 3. Plan de reconnaissance (ordonné et sûr)

**Règles permanentes :**
- **Différentiel 30 mA.** Avant toute étape sous tension (étape 3 et suivantes), vérifier que le circuit de la hotte passe par un différentiel 30 mA, et le tester avec son bouton T.
- **Tout câblage se fait hotte débranchée**, la fiche dans la main. Si la hotte est raccordée en direct (Q2), voir d'abord l'ingénieur. La procédure est alors :
  1. couper le disjoncteur et le condamner ;
  2. vérifier l'absence de tension au bornier avec un VAT ;
  3. faire le portail d'isolement (étape 1) depuis les bornes L et N du bornier, hors tension.
  
  Sans VAT ou sans aisance, faire intervenir un électricien.
- **Après débranchement, attendre au moins 5 min** : les condensateurs E5/E6 font 4,7 µF / 450 V.
- **Zone autorisée**, et seulement hotte débranchée : le quart haut-droit de la carte (CN3, CN2, CN1, IC2/IC3, buzzer, E1, IC4).
- **Zone interdite :**
  - la moitié gauche (primaire) et le transformateur ;
  - les deux fils noirs du condensateur et le connecteur blanc en ligne ;
  - toute la bande basse : VH2 à VH6, « LAMP », AC-POWER, côté contacts des relais ;
  - les relais, VH4 et le condensateur moteur.
- **Ne jamais brancher ou débrancher un connecteur sous tension.**
- **Rien de libre dans le boîtier** : ni Dupont, ni breadboard, ni ESP. Les fils sortent par le passage du câble du panneau, loin des fils du condensateur, et ils sont fixés. La place disponible est à vérifier sur place ; ne jamais percer le boîtier sans en parler avec l'ingénieur.
- **Pas de Mac relié en USB** (ni d'analyseur USB) à un montage raccordé à une hotte sous tension [S26]. La seule exception vient après un portail d'isolement réussi, avec le Mac sur batterie et le chargeur débranché, ou avec un isolateur USB.

### Étape −1 — Observation d'usage (hotte fermée, gratuite)
1. Répondre aux questions Q3 et Q4 en se servant simplement de la hotte :
   - rôle de chaque touche ;
   - voyants ;
   - bips ;
   - état après une coupure. Pour le tester : vitesse 2 et lumière allumée, débrancher la fiche ou couper le disjoncteur, puis rétablir.
2. **Décision :**
   - Si la hotte reprend son état après la coupure, l'option G devient possible.
   - Les réponses disent aussi quoi attendre du protocole : bascules ou sélecteurs, état « armé », marche prolongée.

### Étape 0 — Photos et inventaire (hotte débranchée)
1. Prendre les photos :
   - macro de IC2, IC3, IC4 et des relais (marquages, nombre de broches) ;
   - une vue du trajet des fils du condensateur et du connecteur blanc par rapport à CN3 et au passage du câble du panneau.
2. Photographier CN3 de près. Si la fiche est collée, décoller la colle avec un outil en plastique, sans tirer sur les fils. Ou prévoir de piquer par l'arrière du boîtier.
3. Mesurer le pas de CN3 : si les 3 broches couvrent 5,0 mm, c'est du XH ; si elles couvrent 4,0 mm, c'est du PH [S22].
4. Suivre le câble du panneau :
   - Y a-t-il un connecteur intermédiaire ? Ce serait un point de branchement sans soudure.
   - Le panneau se démonte-t-il ? Si oui, relever la référence de la puce tactile.
5. **Décision :**
   - Puce WTC65xx : le protocole est documenté [S11], on passe directement à la validation.
   - Puce CMS183x : protocole série sur un fil [S12].
   - Bloc résiné : l'option C est probablement exclue.

### Étape 1 — Portail d'isolement (ohmmètre, hotte débranchée)
- **a. Broches L et N de la fiche vers CN3 -, D et +.**
  - On attend **OL**. Une valeur qui monte vers OL (les condensateurs Y se chargent) est normale.
  - **Une valeur stable en kΩ, ou en MΩ faibles, impose l'ARRÊT** : on ne raccorde aucun ESP et on revient vers l'ingénieur.
- **b. Broche PE vers CN3 -** : OL veut dire secondaire flottant. PE vers la carcasse de la hotte : environ 0 Ω (liaison de classe I).
- **c. Côté commuté de la lampe.** Mesurer CN3 - vers le fil noir de la LED, puis CN3 + vers le fil rouge. On en déduit si la lampe est commutée côté + ou côté -, et quel rail alimente « + ». Relever aussi les tensions inscrites sur le 1000 µF et sur E1 (470 µF).
- **d. D vers + et D vers -**, panneau branché puis débranché, en mesurant côté carte et côté panneau.
  - Quelques kΩ vers + indiquent un pull-up ; noter de quel côté il se trouve.
  - **Limite** : un pull-up peut coexister avec une sortie push-pull. L'ohmmètre ne dit donc pas si D est open-drain ou push-pull. Ce point se tranche sous tension, à l'étape 3.
- **Limite générale.** Un ohmmètre ne détecte pas un claquage qui n'apparaîtrait qu'à 230 V. On accepte ce risque résiduel, compte tenu du transformateur, du PC817 et de CY1/CY2.

### Étape 2 — Passe-plat (hotte débranchée)
1. Débrancher la fiche du panneau de CN3.
2. Relier CN3 à une breadboard par des Dupont femelles. Relier la breadboard à la fiche du panneau par des Dupont mâles : broches carrées de 0,64 mm, compatibles XH [S22].

```
CN3 carte [- D +] --Dupont F--> breadboard <--Dupont M-- fiche panneau [- D +]
                         D_carte --[cavalier, ou 220-470 Ω pour le test « qui parle »]-- D_panneau

Étage d'écoute (ajouté à l'étape 4 ou 5, voir §4.1) :
  D ----100k----+---- base Q (BC547 / 2N3904)
               100k
  - ------------+---- émetteur Q ---- GND ESP
  3V3 ESP --10k-- collecteur Q ------ GPIO6   (D haut -> GPIO bas)
```

3. **Contrôles avant toute mise sous tension :**
   - continuité broche à broche entre CN3 carte et fiche panneau (- vers -, D vers D, + vers +) ;
   - aucun court-circuit entre les trois lignes ;
   - photo du montage.
4. **Pour mesurer CN2 à l'étape 3** : brancher deux Dupont femelles sur CN2, repérées, et les amener sur la breadboard. Le + doit rester isolé du reste.
5. **Points d'attention :**
   - Trois boîtiers Dupont côte à côte n'entrent peut-être pas dans le détrompeur de l'embase XH. Sinon, piquer par l'arrière du boîtier XH, ou acheter une rallonge XH mâle-femelle (quelques €).
   - Des Dupont mâles insérés plusieurs fois dans les contacts XH de la fiche du panneau peuvent les desserrer, d'où un faux contact ensuite (S). Limiter les insertions, et vérifier en fin de projet que la fiche d'origine tient.
6. Faire sortir les fils par le passage du câble du panneau, loin des fils du condensateur. Refermer le couvercle, puis fixer les fils.

### Étape 3 — Mesures sous tension (sur la breadboard, hors du boîtier)
- **Préalables :** différentiel testé, contrôles du passe-plat faits, couvercle fermé.
- **Situations :**
  - veille ;
  - après « marche » ;
  - appui **bref, de 1 à 2 s**, sur chaque touche, puis relâchement. Pas d'appui long : il pourrait déclencher une fonction cachée (verrouillage, rappel de nettoyage du filtre). Noter tout bip inhabituel ;
  - lumière allumée ;
  - vitesses 1, 2 et 3.
- **Grandeurs** : V(+,-), V(D,-) et V(CN2+, CN2-). Ajouter la fréquence et le rapport cyclique si le multimètre le permet.
- **Test open-drain ou push-pull**, après la série de mesures :
  1. débrancher la hotte ;
  2. ajouter 47 k entre D et - sur la breadboard ;
  3. rebrancher et remesurer V(D,-) au repos.
  
  Lecture, sur un bus de 5 V :
  - chute d'environ 0,45 V : pull-up d'environ 4,7 k, bus probablement open-drain ;
  - chute d'environ 0,9 V : pull-up d'environ 10 k ;
  - aucune chute mesurable : sortie push-pull, ou pull-up très fort (S, calcul).

| Observation | Interprétation | Suite |
|---|---|---|
| + ≈ 5 V | sortie de l'abaisseur IC4 | test du panneau seul (étape 4). Alimentation de l'ESP par + envisageable |
| + ≈ 12 V | rail du flyback | pas de test du panneau seul en 5 V. Il faudra un abaisseur pour l'ESP. Le module BSS138 est exclu (§4.1) |
| D stable à Vcc, avec des creux seulement pendant les appuis | trafic sur événement, repos haut | favorise A |
| D autour de 40 à 60 % de Vcc en permanence, fréquence stable | trames continues, repos haut (type WTC [S11]) | favorise B |
| D ≈ 0 V au repos, avec de l'activité | **UART inversé** (repos bas), type Krona : 500 bauds, trames continues [S13] | HardwareSerial avec `invert`. Capture nécessaire. Trafic continu, donc favorise B |
| Un niveau continu stable et différent pour chaque touche maintenue | échelle de résistances, pas un bus de données | A, en simulant par une résistance vers - |
| CN2 alimenté en permanence | possible prise d'alimentation sans soudure (courant disponible inconnu) | à croiser avec l'étape 8 |
| CN2 suit la lampe | capteur d'état de la lampe, sans toucher à CN1 | lecture d'état annexe (étape 6) |

**Qui parle sur D (facultatif) :**
- Remplacer le cavalier par 220 à 470 Ω entre D_carte et D_panneau. Le côté qui émet a le niveau bas le plus franc.
- La mesure est dynamique : un multimètre ne suffit pas. Il faut un oscilloscope, ou l'ADC de l'ESP à l'étape 5 (S).

### Étape 4 — Panneau seul, hotte débranchée (seulement si + = 5 V)
- **Débrancher physiquement le côté carte du passe-plat** : retirer les Dupont femelles de CN3. Sinon, le 5 V de l'USB alimente aussi le rail 5 V de la carte (MCU, sortie de IC4), et le MCU de la carte peut parler sur D.
- **Montage :**
  - La hotte est débranchée de la prise, donc le Mac en USB ne pose pas de problème.
  - Le 5V et le GND du SuperMini alimentent la fiche du panneau.
  - D passe par l'étage d'écoute (§4.1) vers GPIO6.
  - Ajouter en option un pull-up de 10 k vers 5 V, au cas où le pull-up d'origine serait côté carte.
  - Ne pas toucher les touches à la mise sous tension : le circuit tactile se calibre pendant environ 0,5 s [S40].
- **Questions à trancher :**
  - Le panneau émet-il spontanément ?
  - Ses voyants s'allument-ils seuls quand on touche ?
    - Si oui, le panneau détient au moins l'état de ses voyants, peut-être tout l'état, comme dans Krona où le panneau envoie l'état complet [S13].
    - S'ils restent éteints, c'est la carte qui pilote les voyants (type WTC [S11]).

### Étape 5 — Capture complète sur place (sous tension, isolement confirmé)
- **Montage :**
  - ESP sur une batterie USB (flottante) **qui ne se coupe pas à faible consommation**.
  - Journaux envoyés par Wi-Fi (UDP ou telnet) au Mac, **sans câble USB**.
  - La seule masse commune avec la hotte est CN3 -.
  - Tout est câblé hotte débranchée, couvercle fermé, avant la mise sous tension.
- **Scénarios**, 10 s chacun :
  - veille, marche, lumière allumée puis éteinte ;
  - vitesses 1, 2 et 3, puis arrêt ;
  - une capture avec le panneau déconnecté du passe-plat, pour voir si la carte émet seule. On déconnecte **hotte débranchée**, puis on la rebranche.
- **Qui parle (optionnel).** Avec la résistance série en place, deux diviseurs à haute impédance, un de chaque côté, sur deux entrées ADC en mode continu (§4.1 b). À valider au banc (S).
- **Analyse** : histogramme des durées hautes et basses.
  - **UART** si les durées sont des multiples d'un temps bit : 104, 208, 417, 833, 1667 ou 2000 µs. Il faut inclure les débits bas et non standard de 500 à 1200 bauds ; Krona tourne à 500 bauds, soit 2 ms par bit [S13]. Passer ensuite en HardwareSerial au débit déduit, avec `invert` si le repos est bas. Le C6 n'a pas de détection automatique du débit [L1]. Chercher un en-tête, une longueur et une somme de contrôle (somme, XOR, CRC-16/Modbus) [S14][S15].
  - **Codage par distance ou largeur d'impulsion** si une phase est constante, que l'autre prend deux valeurs et qu'une impulsion d'en-tête ouvre la trame. Exemple du WTC6534 [S11] :
    - départ = 2 T bas ;
    - « 0 » = 1 T haut suivi de 1 T bas ;
    - « 1 » = 1 T haut suivi de 3 T bas.
  - **Manchester** si l'on ne voit que des durées T et 2T.
- **Tests décisifs :**
  - **(i)** Après relâchement, la trame revient-elle à « aucune touche » ? Si oui, c'est un clavier et l'état est sur la carte. Si la trame garde le nouvel état, c'est le panneau qui est maître.
  - **(ii)** Y a-t-il une réponse de la carte après chaque trame ?
  - **(iii)** Y a-t-il des silences plus longs qu'une trame, où l'on pourrait injecter ?
  - **(iv)** La carte émet-elle son état (voyants, vitesse) ? Sinon, D est unidirectionnel : voir l'étape 6.

**Outil de capture à écrire** (PlatformIO, cœur 3.3.12). Tout ce qui suit est à valider sur le banc.
- **Réception RMT à 1 MHz** :
  - `rmtInit(pin, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_1, 1000000)` ;
  - filtre anti-parasites de quelques µs avec `rmtSetRxMinThreshold` ;
  - fin de trame après 5 à 10 ms de silence avec `rmtSetRxMaxThreshold` [S20].
- **Mémoire RMT.** Le C6 a 2 canaux de réception de 48 symboles chacun [L10]. Un canal configuré avec 2 blocs prend la mémoire de l'autre (S). Il faut donc 1 bloc par voie si l'on doit écouter les deux côtés (option B). Chaque durée est limitée à 32,7 ms à 1 MHz [S20].
- **Trames longues**, deux possibilités :
  - la réception partielle d'IDF (`en_partial_rx`, ping-pong pris en charge sur C6 [L10][L12]). Elle n'est pas exposée par l'API RMT Arduino [L12] ;
  - une interruption GPIO avec horodatage `esp_timer` dans un tampon circulaire.
- **Autres outils :**
  - ESPHome `remote_receiver` avec `dump: raw` : l'usage sur un fil est déduit [S55].
  - `ok-home/logic_analyzer` ne gère pas le C6 [S21].
  - Un analyseur FX2 à 24 MHz (environ 8 €) avec PulseView [S21]. **Sa masse est celle du Mac.** On ne l'utilise qu'après un portail d'isolement réussi, avec le Mac sur batterie (chargeur débranché) ou derrière un isolateur USB, et toujours derrière un diviseur à haute impédance. Le diviseur protège les entrées de l'analyseur, pas l'utilisateur. À défaut, s'en tenir à l'ESP sur batterie.

### Étape 6 — Choix de l'architecture

| Résultat | Choix |
|---|---|
| D open-drain, trafic sur événement ou silences exploitables | **A** |
| Trafic continu (type WTC ou Krona), réponse dans un délai court, ou D en push-pull | **B**, avec contournement et watchdog |
| Panneau maître de l'état : ses trames codent l'état complet, comme Krona (8 états vitesse × lumière envoyés en continu [S13]) | B, en régénérant les trames côté carte. Les voyants du panneau ne suivront pas les commandes à distance. À discuter, sinon C |
| **D unidirectionnel (panneau vers carte), sans état renvoyé** | A ou B pour les commandes, plus une lecture d'état annexe (détail ci-dessous) |
| Échelle de résistances | A (résistance commutée vers -) |
| Protocole indéchiffrable | C, si le panneau s'ouvre |

**Lecture d'état annexe (cas D unidirectionnel) :**
- **Lampe** : lue sur CN1 ou CN2, à travers le tampon.
- **Vitesse** : pince de test SOIC-16 posée sur IC3 (ULN2003), environ 5 €, sans soudure (S).
  - Lire de préférence les entrées (broches 1 à 7, niveaux logiques du MCU) [S9].
  - À l'état actif, une sortie (broches 10 à 16) reste à 0,9 V typique, 1,1 V maximum à 100 mA [S9]. C'est trop près du seuil du tampon 100k/100k (environ 1,2 à 1,3 V, qui baisse à chaud, S). Pour lire les sorties, relever ce seuil, par exemple avec 100k/33k.
  - La pince se pose hotte débranchée et doit être fixée.
- **À défaut**, un état « optimiste », présenté comme tel.

### Étape 7 — Premier essai d'injection (sous tension, ESP sur batterie)
- Utiliser le firmware de capture (journaux Wi-Fi), avant d'intégrer Matter.
- Commencer par la touche lumière : on doit entendre le bip, voir la lumière changer et retrouver la trame sur le bus.
- Puis les vitesses, en respectant un délai minimal entre deux changements (§5.2). Enfin, la séquence « marche » suivie d'une vitesse.
- **Critères :** 20 réussites sur 20, aucun comportement anormal de la carte, et le vrai panneau toujours fonctionnel.

### Étape 8 — Essai d'alimentation (seulement si + = 5 V)
- **Montage :** ESP alimenté par +, lumière allumée et vitesse maximale, pendant 24 h, avec la puissance d'émission réduite (§4.3).
- **Suivi :**
  - journaliser `esp_reset_reason()` par le pont de journaux (§4.6) ;
  - surveiller les claquements de relais ou les bips intempestifs.

---

## 4. Interface matérielle proposée

### 4.1 Niveaux logiques

**Tolérance.** Le C6 **ne tolère pas le 5 V** [S18, tableau 5-4] :
- maximum VDD + 0,3 V, soit environ 3,6 V ;
- VIH min = 0,75 × VDD ≈ 2,48 V ;
- VIL max = 0,25 × VDD.

**Lecture de D : un étage à haute impédance est obligatoire.** Le diviseur 10k/20k de la révision 1 est abandonné, pour trois raisons (calculs, S) :

1. **Il charge le bus.** Il met 30 kΩ en permanence entre D et -, ce qui abaisse le niveau haut au repos :

| Pull-up d'origine sur D | Niveau haut au repos avec 10k/20k | Conséquence |
|---|---|---|
| 4,7 k (R13 ?, non confirmé) | 4,3 V | acceptable |
| 10 k | 3,75 V | limite |
| Pull-up interne d'un MCU, 30 à 50 k | 2,5 V ou moins | bus illisible pour la carte |

2. **Il pose problème quand l'ESP n'est pas alimenté.** C'est le cas d'une batterie qui se coupe, d'un régulateur défaillant ou d'une source séparée.
   - La GPIO est bloquée vers 0,6 à 1 V par sa diode de protection.
   - D voit alors environ 6,7 kΩ vers 0,5 à 0,7 V. Le repos tombe vers 3,2 V avec un pull-up de 4,7 k, vers 2,4 V avec un pull-up de 10 k.
   - Un ESP alimenté à rebours par sa GPIO peut redémarrer en boucle (brown-out), avec des broches erratiques, y compris celle du transistor d'injection.
3. **Ses marges sont trop justes.** Avec 10k/20k sur un « 5 V » qui mesure 5,3 V, la GPIO reçoit 3,53 V. Avec 27k/10k sur un flyback à 13 V, elle reçoit 3,51 V. Les deux sont collés au maximum absolu.

**Option (a), par défaut pour la capture numérique : un tampon NPN** (BC547 ou 2N3904). Câblage :
- D → 100 k → base ;
- 100 k entre base et émetteur ;
- émetteur sur CN3 - ;
- collecteur → 10 k → 3V3, et vers la GPIO.

Propriétés :
- La logique est inversée : régler `invert` sur l'UART, ou inverser les niveaux RMT.
- Il ne prend que 40 à 110 µA sur D.
- Il fonctionne tel quel sur un bus de 5 V ou de 12 V.
- Il reste neutre si l'ESP est éteint.
- Sa vitesse de commutation est à valider au banc pour des bits d'environ 100 µs (S).

**Option (b), quand une lecture analogique est nécessaire** (échelle de résistances, test « qui parle » par l'ADC) : des diviseurs à haute impédance, **choisis après la mesure de +**.
- Bus de 5 V : 47k/68k. 5 V donne 2,96 V, 5,5 V donne 3,25 V.
- Bus de 12 V : 100k/33k. 12 V donne 2,98 V, 13,5 V donne 3,35 V. Sur un bus de 5 V, ce diviseur ne donnerait que 1,24 V, illisible.
- La constante RC reste sous 1 µs, ce qui suffit pour des bits de 100 µs ou plus.
- Attention au pull-up de 45 kΩ de GPIO6 tant que le sketch ne l'a pas désactivé (§4.2).

**Écriture sur D, en drain ouvert uniquement.** Deux options :
- **Un NPN** (BC547 ou 2N3904). Câblage :
  - 4,7 à 10 k en série dans la base ;
  - **10 k entre base et émetteur**, pour le démarrage ;
  - **100 à 220 Ω dans le collecteur**, pour limiter le courant si D s'avère push-pull ;
  - collecteur sur D, émetteur sur CN3 -.
  
  La logique est inversée. En option, on peut coupler la base par un condensateur, pour qu'une GPIO bloquée à l'état haut ne puisse pas maintenir D à l'état bas. La constante de temps doit couvrir l'impulsion basse la plus longue du protocole : jusqu'à 18 ms pour un octet nul à 500 bauds (S).
- **Un module BSS138** (type Adafruit 757), **seulement si + = 5 V**. Son côté haut est limité à 10 V, et il ajoute des pull-ups de 10 k des deux côtés, qui s'ajoutent à celui du bus [S23]. Non inverseur et bidirectionnel, il permet le mode UART one-wire du cœur 3.3.11 et suivants (RX et TX sur la même broche, en open-drain) [S19][L1].

**Ne jamais piloter D en push-pull 3,3 V.** Une entrée 74HC alimentée en 5 V demande 3,15 V pour un niveau haut. Si l'option B impose du push-pull vers une entrée 5 V, utiliser un 74HCT alimenté en 5 V, ou un BSS138 [S24].

**Pour l'option B**, il faut deux voies (côté panneau et côté carte), chacune avec lecture et écriture.
- Ressources du C6 : 2 canaux RMT en réception, de 48 symboles chacun, donc un bloc par voie [L10] ; 2 UART HP [L1].
- Contournement : un relais de signal au repos fermé, qui relie D panneau et D carte tant que l'ESP ne l'excite pas, ou un cavalier manuel.

### 4.2 Choix des GPIO
- **À éviter :**
  - broches de démarrage 2, 4, 5, 8 (WS2812), 9 (BOOT) et 15 ;
  - USB 12 et 13 ;
  - 21 et 22, inaccessibles ;
  - 16 et 17 (UART0), à réserver au pont de journaux (§4.6). GPIO16 émet le journal de démarrage de la ROM [S18, tableau 2-1].
- **États au reset** [S18, tableau 2-1] :
  - GPIO18 à 23 n'ont leur pull-up **que pendant** le reset ;
  - GPIO6 (MTCK) reste en entrée, **avec un pull-up d'environ 45 kΩ après le reset**. Cela dépend de `EFUSE_DIS_PAD_JTAG`, qui vaut la valeur par défaut ici ;
  - GPIO7 (MTDO) est en entrée sans pull-up.
- **Proposition** (le SuperMini expose bien GPIO0 à GPIO7 [S56]) :
  - **lecture de D sur GPIO6**. Le passer en INPUT sans pull-up dès le début de `setup()`. Avec le tampon NPN, le collecteur a de toute façon son propre 10 k ;
  - **injection sur GPIO7**, avec la résistance de 10 k entre base et émetteur ;
  - **ne jamais inverser ces deux broches** : le pull-up de GPIO6 débloquerait le NPN d'injection pendant le reset et tirerait D à l'état bas ;
  - pour l'option B, la voie côté carte sur GPIO0 et GPIO1.

### 4.3 Alimentation de l'ESP
- **Pendant la reconnaissance** : une batterie USB qui ne se coupe pas à faible consommation. Une coupure laisserait un ESP non alimenté branché sur D (§4.1).
- **Montage définitif si + = 5 V**, et si l'étape 8 est concluante :
  - CN3 + vers la broche 5V, CN3 - vers GND, avec 220 à 470 µF en parallèle. L'ESP est ainsi alimenté par le même rail que le bus : il ne peut pas être éteint pendant que D est actif.
  - **Réduire la puissance d'émission Thread vers 8 à 12 dBm.** Le pic d'émission passe de 305 mA à 20 dBm à 187 mA à 12 dBm [S18, tableau 5-9]. Cela réduit aussi le rayonnement près du faisceau du panneau tactile (S). L'API OpenThread à utiliser reste à identifier et à valider.
  - **Jamais d'USB en même temps.** Sur le SuperMini C3, le VBUS est relié directement au 5V [S25] ; sur le C6, ce n'est pas vérifié. Pour le savoir, mesurer la broche 5V sur USB seul : 5,0 V indique une liaison directe, environ 4,7 V une diode. Le régulateur ME6211 accepte 6 V au maximum [S25].
- **Montage définitif si + = 12 V :**
  - un abaisseur de 12 V vers 5 V (achat), sur le rail 12 V permanent, une fois celui-ci identifié ;
  - **jamais** sur la sortie 12V_LED, qui est commutée ;
  - **jamais** sur l'enroulement auxiliaire du primaire (le 10 µF/50 V près de IC1, `c1_left.jpg`) ;
  - à défaut, un bloc USB externe sur une prise voisine.
- **Budget de courant du C6** [S18, tableaux 5-8 et 5-9, valeurs crêtes] :
  - réception 802.15.4 : 74 mA, en permanence pour un routeur ;
  - émission 802.15.4 : 305 mA à 20 dBm, 187 mA à 12 dBm, 119 mA à 0 dBm ;
  - émission BLE : 315 mA à 20 dBm, pendant la mise en service.

### 4.4 Câblage sans soudure
- **Pour les mesures** : le passe-plat décrit à l'étape 2, sorti du boîtier.
- **Pour le montage définitif** : les Dupont se desserrent avec les vibrations. Prévoir des fils XH précâblés ou une rallonge en Y (petit achat), et des colliers.
- **Emplacement :**
  - **De préférence hors du boîtier électronique**, le long du câble du panneau. C'est aussi probablement mieux pour la radio (S).
  - **Dans le boîtier**, seulement dans le quart haut-droit, et seulement avec une double isolation fixe : gaine thermorétractable, plus une fixation mécanique qui empêche tout contact avec un conducteur secteur, même si un fil se détache. Rester loin des fils du condensateur et du connecteur blanc.
  - Si un fil basse tension frotte contre un fil secteur, l'ESP et le faisceau du panneau deviennent dangereux au toucher (S).
- **Isoler le SuperMini du châssis.** La coque de son USB-C est reliée à GND. Un contact avec l'acier mettrait le secondaire à la terre, ce qui peut modifier la sensibilité du panneau tactile (S).
- **Garder le connecteur USB-C accessible** hotte débranchée, pour les mises à jour (§4.6).
- **Radio** : mesurer RSSI et LQI à l'emplacement définitif. La hotte est en acier.

### 4.5 Matériel

| Article | Possédé ? | Rôle |
|---|---|---|
| ESP32-C6 SuperMini, 2 ou plus | oui | écoute, puis nœud final ; un autre sert de pont de journaux (§4.6) |
| Fils Dupont | oui | passe-plat |
| Mac avec PlatformIO | oui | flashage (hotte débranchée) et journaux Wi-Fi |
| Multimètre | **à confirmer, indispensable** | portail d'isolement, tensions |
| Breadboard ; résistances 100k (2 à 4), 47k, 68k, 33k, 27k, 10k, 4,7k, 220 à 470 Ω | à confirmer | tampon d'entrée, diviseurs, base et collecteur du NPN, test « qui parle », test open-drain |
| NPN BC547/2N3904 (au moins 2) ; module BSS138 si + = 5 V | à confirmer | tampon d'entrée, injection en drain ouvert |
| Condensateur 220 à 470 µF, 10 V ou plus | à confirmer | découplage de l'alimentation |
| Batterie USB sans coupure à faible consommation | à confirmer | ESP flottant pendant la capture, pont de journaux |
| Gaine thermorétractable, colliers | à confirmer | isolation fixe, maintien des fils |
| ST-Link V2, BM5602, CC2500 | oui | inutiles ici |
| *Achats possibles* | non | analyseur FX2 (~8 €), rallonge XH, abaisseur 12→5 V (si + = 12 V), relais de contournement (si B), 5× AQY221N2S (si C), pince SOIC-16 (~5 €, si D ne porte pas l'état), prise Matter over Thread (si G) |

### 4.6 Journaux et mises à jour du nœud installé
**Le constat :**
- En Thread, `Matter.selectNetwork(MATTER_NETWORK_THREAD)` n'initialise jamais le Wi-Fi [L8, lignes 125 à 128].
- La partition `huge_app` n'a pas de partition OTA [L8, lignes 9 et 10][L9].
- L'USB est interdit tant que la hotte est sous tension.

**OTA.** Elle est impossible sur la flash de 4 Mo avec l'image actuelle. L'image Matter du C6 mesure 2,41 Mo sur la BenQ [L8, lignes 12 à 14]. L'emplacement d'application de `ota_nofs_4MB.csv` fait 0x1F0000, soit environ 1,94 Mio [L9]. Il faudrait gagner environ 0,4 Mo sur l'image (S). Les mises à jour se font donc en USB, hotte débranchée : d'où l'USB-C accessible (§4.4).

**Journaux**, deux possibilités :
- **Un 2ᵉ SuperMini comme pont UART vers Wi-Fi :**
  - le TX UART0 du nœud (GPIO16, `Serial0` quand l'USB CDC est actif [L14]) va sur le RX du pont ;
  - masse commune côté secondaire (CN3 -) ;
  - le pont est alimenté par batterie et envoie les journaux au Mac en UDP.
- **Des journaux en UDP sur Thread**, via le routeur de bordure (S).

Journaliser `esp_reset_reason()` à chaque démarrage.

---

## 5. Exposition Matter et Apple Home

### 5.1 Structure
Un seul nœud, avec deux endpoints ordinaires et sans pont :
- **EP1 MatterFan** : `Fan.begin(0, MatterFan::FAN_MODE_OFF, MatterFan::FAN_MODE_SEQ_OFF_LOW_MED_HIGH)`. Type Fan (0x002B) avec Identify, Groups et FanControl, mais **ni OnOff ni MultiSpeed** [S29].
- **EP2 MatterOnOffLight** pour la LED 12 V [S30]. Autre possibilité : `MatterOnOffPlugin`, si la commande « éteins les lumières » ne doit pas agir sur la hotte. C'est le même choix que HALO1_SELECTORS_AS_LIGHTS sur la BenQ.
- Appeler `Matter.selectNetwork(MATTER_NETWORK_THREAD)` avant tout `begin()`. Mise en service en BLE, partition `huge_app`, comme sur le nœud BenQ.
- **Ne pas utiliser Extractor Hood (0x007A) :**
  - Apple ne liste pas ce type [S33], et iOS 27 ne gère toujours pas l'électroménager [S34] ;
  - Arduino n'a pas de classe pour ce type, et la PR des endpoints personnalisés a été fermée sans fusion [S35]. Il faudrait écrire du esp_matter brut.
- FanMode et OnOff sont persistants en NVS : voir le piège du démarrage au §5.4.

### 5.2 Vitesses et pourcentages
Ni la bibliothèque ni le serveur CHIP 1.5 ne font la correspondance entre vitesses et pourcentages. Le serveur CHIP 1.5 se contente de passer FanMode à Off quand PercentSetting vaut 0, et les pourcentages à 0 quand FanMode passe à Off [S31]. Le reste est à la charge du firmware.

| Hotte | FanMode | Pourcentage renvoyé | Plage acceptée depuis un contrôleur |
|---|---|---|---|
| Arrêt | Off | 0 | 0 |
| Faible | Low | 33 | 1–33 |
| Moyenne | Medium | 66 | 34–66 |
| Élevée | High | 100 | 67–100 |

- **Choix des valeurs.** 33, 66 et 100 suivent l'exemple de la spécification.
  - [S32] est un rapport de bug, ouvert le 2026-09-17, contre les seuils 33 / 66 / 100 codés en dur dans le FanControlCluster de Matter 1.6. Il rappelle que la correspondance entre vitesse et pourcentage relève du fabricant.
  - Compatibilité avec un futur cœur : **S**. Si un futur cœur déduit lui-même FanMode du pourcentage, il peut entrer en conflit avec la correspondance du firmware. Refaire les tests après chaque mise à jour du cœur.
- **Écritures de pourcentage.** Le callback note la cible et retourne `true`.
  - **Lissage** : faire glisser le curseur peut produire plusieurs écritures, et chacune injecterait des touches (bips, commutations de relais sur un moteur à prises). N'agir qu'environ 0,5 à 1 s après la dernière écriture, et imposer un délai minimal entre deux changements de vitesse (S). À valider dans les journaux avec `CORE_DEBUG_LEVEL=5`.
  - Ensuite, dans `loop()` : commander la hotte, puis appeler `setMode(bande)` et `setSpeedPercent(33, 66 ou 100)` sous le drapeau `fromHood`.
  - L'arrondi est différé : une écriture faite en PRE_UPDATE serait probablement écrasée (S).
- **Écritures de FanMode**, par Siri ou un autre contrôleur : Low, Medium et High donnent 33, 66 et 100. `On` est converti en High avant nos callbacks, et deux fois : par `resolveFanMode` dans MatterFan.cpp [S29][L7], et par le pré-callback de fan-control-server.cpp [S31].
- **Bouton marche de la tuile.**
  - Apple écrit PercentSetting = 100 juste avant d'allumer ; Siri ne le fait pas [S36, #387]. Cela a été observé sur un ventilateur *avec* un cluster OnOff. Le nôtre n'en a pas : à vérifier avec `CORE_DEBUG_LEVEL=5`.
  - **Compromis de la Q12.** Le firmware ne peut distinguer ni « bouton marche de la tuile » de « curseur poussé à 100 % », ni « On » de « High ». L'option « reprendre la dernière vitesse » empêcherait donc d'allumer directement en vitesse maximale depuis l'arrêt. Le projet cité fait le même constat [S36].
- **Arrêt.** Toujours PercentSetting = 0, sinon Apple affiche « Extinction… » sans fin [S36]. La bibliothèque le fait déjà sur Off [S29].

### 5.3 Touche marche et séquencement
- La touche marche **n'est pas exposée** dans Matter. Le firmware tient un état interne `arme`, lu sur le bus, ou reconstitué si D ne porte pas l'état.
- **Régler une vitesse** quand la hotte n'est pas armée : injecter Marche, attendre le délai mesuré, puis injecter la vitesse.
- **Arrêter** : la séquence dépend de la Q3 (Marche 1 fois, 2 fois, ou nouvel appui sur la vitesse active).
- **Marche prolongée.** Si la Haier a une marche prolongée de 15 min comme la CDA [S8], l'arrêt depuis Home reste un arrêt immédiat. Un 3ᵉ endpoint « Arrêt différé » (Plugin) ne sera ajouté que si l'utilisateur le veut.
- **Lumière** : commande directe si sa touche marche sans Marche, séquence sinon.
- **Idempotence.** Les touches sont des bascules ou des sélecteurs. On n'injecte donc que si l'état lu diffère de la cible. On attend ensuite la confirmation sur le bus (environ 1 s), on réessaie une fois, puis on renvoie l'état réel. Sur la BenQ, Home gardait l'état demandé pendant environ 10 s [L3].

### 5.4 Retour d'état et écho
- **Source de vérité** : l'état décodé sur le bus, **si la carte l'émet**. Sinon :
  - une lecture annexe (CN1 ou CN2 pour la lampe, IC3 pour la vitesse, étape 6) ;
  - ou un état optimiste, présenté comme tel.
  
  Complément possible : la tension de CN1 à travers le tampon, si l'étape 1c confirme le côté commuté.
- **Écho.** Les appels `set*` avec `ATTR_UPDATE` relancent nos propres callbacks [S29][S30]. **La seule parade retenue est le drapeau `fromHood`.**
- **Ne pas utiliser `esp_matter::attribute::report()`.** Il laisse périmé le cache de MatterFan, et `attributeChangeCB` ignore une écriture du contrôleur égale à ce cache (branche `val == currentPercent`, sans appel du callback utilisateur) [L7]. Exemple :
  1. le cache vaut 33, le firmware publie 66 avec `report()` ;
  2. l'utilisateur remet 33 dans Home ;
  3. aucun callback n'est appelé : la hotte reste en vitesse moyenne alors que Home affiche « Faible ».
- **Démarrage :**
  - **Ne pas appeler `updateAccessory()`** : sur des touches bascules, il rejouerait l'état mémorisé [S30].
  - **Piège de la NVS** (V, code) [S57][L7][L11]. Le mécanisme :
    - FanMode est créé en persistant. À la création de l'attribut, esp_matter relit la valeur en NVS, qui écrase celle passée à `begin()`.
    - PercentSetting et PercentCurrent ne sont pas persistants.
    - Le cache `currentFanMode` de MatterFan prend l'argument de `begin()`, et `setMode()` s'arrête tout de suite si le cache est déjà égal à la valeur demandée.
  - **Scénario :**
    1. coupure secteur pendant que la hotte est en vitesse élevée ;
    2. au redémarrage, l'attribut FanMode vaut High (relu en NVS), le cache vaut Off, les pourcentages valent 0 ;
    3. la hotte est arrêtée, le firmware appelle `setMode(OFF)`, qui n'écrit rien ;
    4. Apple affiche un état incohérent : High à 0 %, voire « Extinction… ».
  - **Même mécanisme sur MatterOnOffLight** : OnOff est persistant, `start_up_on_off` est nul, et `setOnOff` est court-circuité par `onOffState` [L7].
  - **Parade**, à valider au banc :
    1. après `Matter.begin()` et le premier décodage du bus, forcer FanMode et OnOff à la valeur du cache Arduino avec `updateAttributeVal()`, méthode publique de MatterEndPoint [L13], ou avec `esp_matter::attribute::update`. Le callback voit alors une valeur égale au cache et ne fait rien (S) ;
    2. publier l'état lu avec les `set*`, sous `fromHood` ;
    3. si le cache doit être réaligné, passer par une autre valeur puis par la vraie, toujours sous `fromHood`.
  - Après une coupure secteur, l'ESP redémarre avec la hotte et reconstruit l'état depuis le bus.

### 5.5 Rôle Thread
- Les bibliothèques C6 précompilées sont en FTD, sans ICD. esp_matter règle donc le type Router, et le nœud peut devenir **routeur** [S37][L5]. C'est adapté à un nœud sur secteur, qui étend le maillage [S38].
- **Ne pas reprendre** le wrap `_SetThreadDeviceType` ni `MATTER_THREAD_MED` de la BenQ [L4].
- Exception (S) : si la liaison radio depuis le caisson est mauvaise, revenir au MED.
- **Consommation** : environ 74 mA en réception permanente, que le nœud soit routeur ou MED [S18][S38]. Pour la puissance d'émission, voir §4.3.

### 5.6 Affichage dans Home
- Apple regroupe par défaut les endpoints d'un même nœud dans une seule tuile. Sur la BenQ, une commande envoyée depuis cette tuile atteignait tous les endpoints [L2]. **Choisir « afficher comme accessoires séparés »** [S39].
- La vitesse est un curseur de 0 à 100 %. Rien n'atteste de boutons Faible, Moyenne et Élevée [S36].
- **Voyants du panneau :**
  - cohérents avec l'architecture A, si c'est la carte qui les pilote ;
  - cohérents avec B sur un protocole de type WTC, si la carte envoie le paquet de voyants, qui est facultatif [S11] ;
  - avec un protocole où le panneau est maître (type Krona [S13]), ils ne suivront pas les commandes à distance, sauf si l'ESP régénère les trames envoyées au panneau.

---

## 6. Risques et sécurité 230 V

1. **Différentiel 30 mA** vérifié et testé (bouton T) avant toute étape sous tension. En cas de raccordement direct : disjoncteur coupé et condamné, absence de tension vérifiée au VAT, ou intervention d'un électricien (§3).
2. **Câblage** uniquement hotte débranchée, puis attendre au moins 5 min. Les X2 ont des résistances de décharge de 1 MΩ (R9/R10, `c1_left.jpg`). Les condensateurs E5/E6 n'en ont pas forcément.
3. **Zones.** Seul le quart haut-droit de la carte est en très basse tension, et on n'y touche que hotte débranchée. Zone interdite :
   - la moitié gauche (primaire) et le transformateur ;
   - les fils noirs du condensateur et le connecteur blanc en ligne, qui passent au-dessus du secondaire ;
   - toute la bande basse : VH2 à VH6, « LAMP », AC-POWER, côté contacts des relais ;
   - les relais, VH4 et le condensateur moteur. Aux bornes d'un condensateur permanent, la tension peut dépasser celle du réseau (S).
   
   Rien de libre dans le boîtier. Les fils sortent par le passage du câble du panneau.
4. **Portail d'isolement** (étape 1) avant tout raccordement d'un appareil relié à la terre ou au Mac. Limite : un ohmmètre ne voit pas un claquage à 230 V.
5. **Pas de Mac en USB, ni d'analyseur USB**, tant que la hotte est sous tension [S26]. La masse d'un analyseur FX2 est celle du Mac. Il faut donc le Mac sur batterie (chargeur débranché) ou un isolateur USB, et seulement après le portail d'isolement.
   - L'ADuM3160 est compatible avec l'USB full speed [S27].
   - La puce elle-même ne fournit aucune alimentation. Les 200 mA environ viennent du convertisseur isolé d'environ 1 W des modules courants (S).
   - Il faut donc une alimentation locale pour l'ESP.
6. **Fuite par les condensateurs Y.** Les valeurs de CY1/CY2 sont inconnues. Le chiffre de 0,16 à 0,32 mA est une estimation pour 1 ou 2 condensateurs de 2,2 nF (S).
   - À 50 Hz, 2,2 nF font environ 1,45 MΩ, et le primaire est à peu près à la moitié du secteur par rapport à la terre.
   - Un multimètre de 10 MΩ peut donc afficher près de **100 V~** entre le secondaire flottant et la terre (S). C'est normal et ce n'est pas un défaut d'isolement.
   - Brancher le fil de masse en premier [S28].
7. **5 ou 12 V directement sur une GPIO détruit l'ESP.** Toujours passer par le tampon, un diviseur à haute impédance ou un transistor.
8. **ESP non alimenté branché sur D** : la GPIO charge le bus, et l'ESP est alimenté à rebours (brown-out, broches erratiques, y compris celle de l'injection). Parades :
   - étage d'entrée à haute impédance ;
   - batterie qui ne se coupe pas ;
   - en définitif, alimentation par le même rail que le bus.
9. **Dupont qui se desserrent** : un court-circuit entre + et D ou entre + et - peut abîmer le MCU ou l'alimentation de la carte. Contrôler la continuité avant chaque mise sous tension, fixer et isoler les fils, et passer au XH serti en définitif.
10. **Contact entre un fil basse tension et un fil secteur** : l'ESP et le faisceau du panneau deviendraient dangereux au toucher. Double isolation fixe, ou ESP hors du boîtier (§4.4).
11. **Contact entre le SuperMini et le châssis** : la coque USB-C met le secondaire à la terre, ce qui peut perturber le panneau tactile (S). Gainer le module.
12. **Surcharge du secondaire** : chute de tension, redémarrage du MCU, relais qui claquent. Valider à charge maximale (étape 8), puissance d'émission réduite.
13. **Panne de l'ESP.**
    - Avec A (entrée à haute impédance) : sans conséquence.
    - Avec B : contournement et watchdog obligatoires.
    - Le firmware ne doit jamais maintenir D à l'état bas. Prévoir un délai maximal sur chaque impulsion injectée, une résistance dans le collecteur et, en option, un couplage capacitif de la base (§4.1).
14. **Commutations répétées du moteur** : lisser les écritures du curseur et imposer un délai minimal entre deux changements de vitesse (§5.2).
15. **Chaleur et graisse** au-dessus de la plaque : mesurer la température dans le boîtier après 30 min en vitesse maximale pendant une cuisson.
16. **Moteur à plusieurs prises** (options D et E seulement) : deux prises alimentées en même temps mettent le bobinage en court-circuit partiel [S44]. C'est l'une des raisons de les écarter.
17. **Garantie et conformité** : garantie de 2 ans [S1]. La hotte a été fabriquée en 06/2024 ; selon la date d'achat, la garantie est peut-être déjà expirée. Toute modification interne sort la hotte de la conformité EN 60335-2-31 déclarée par le fabricant [S47]. L'usage reste privé.

---

## 7. Questions pour l'utilisateur (par impact sur la conception)

**Questions bloquantes (sécurité ou choix d'architecture)**
1. As-tu un **multimètre** ? Idéalement avec un calibre ohmmètre de 20 MΩ, et la mesure de fréquence ou de rapport cyclique.
2. **Branchement :**
   - La hotte est-elle branchée sur une **prise** (fiche accessible) ou raccordée en direct à une boîte ?
   - Son circuit passe-t-il par un **différentiel 30 mA** ? As-tu testé son bouton T ?
   - En cas de raccordement direct, as-tu un VAT, ou préfères-tu un électricien ?
3. **Comportement exact des touches**, à observer avant d'ouvrir (étape −1) :
   - a) Depuis la veille, que fait une vitesse sans Marche : rien, ou un bip ?
   - b) La lumière marche-t-elle sans Marche ?
   - c) Que fait Marche quand le moteur tourne : arrêt immédiat, 15 min de marche prolongée, ou rien ?
   - d) Un nouvel appui sur la vitesse active arrête-t-il le moteur ?
   - e) L'état « armé » s'éteint-il tout seul au bout d'un moment ?
   - f) Quels bips entend-on ?
   - g) Après une coupure de courant, la hotte reprend-elle la vitesse et la lumière d'avant ?
4. Chaque touche a-t-elle un **voyant** ? Lesquels s'allument, et dans quel état ?
5. Le panneau **se démonte-t-il** ? Voit-on sa carte (référence de la puce tactile), ou est-ce un bloc résiné ? Le câble a-t-il un **connecteur intermédiaire** ?
6. **Photos** :
   - macros de IC2, IC3, IC4 et des relais ;
   - CN3 est-il collé ? Quel est le pas mesuré ?
   - trajet des fils du condensateur et du connecteur blanc par rapport à CN3 et au passage du câble du panneau.
7. **Consignes « pas de soudure, pas d'achat » :**
   - Un budget de 10 à 15 € est-il acceptable ? Il couvrirait l'analyseur, des rallonges XH, un BSS138, un abaisseur et une pince SOIC-16.
   - Une prise Matter over Thread (option G) ?
   - Une soudure légère, si le bus reste indéchiffrable ?

**Questions importantes (interface et alimentation)**

8. Quels composants as-tu en stock ?
   - breadboard ;
   - résistances de 100k, 47k, 68k, 33k, 10k, 4,7k, 220 à 470 Ω ;
   - NPN ou BSS138, condensateur ;
   - batterie USB : se coupe-t-elle à faible consommation ? ;
   - gaine thermorétractable, relais de signal, autre carte ESP32 (S3 ou C3).
9. Y a-t-il une prise secteur près de la hotte, pour une alimentation USB de secours ?
10. Où veux-tu loger l'ESP ? De préférence hors du boîtier électronique, le long du câble du panneau. Quels routeurs de bordure Thread as-tu (HomePod mini, Apple TV), et à quelle distance ?

**Questions de confort (présentation dans Home)**

11. Lumière de la hotte : « Lumière » (concernée par « éteins les lumières ») ou « Prise » ?
12. **Bouton marche de la tuile**, deux choix :
    - lancer la vitesse maximale, le comportement Apple probable ;
    - reprendre la dernière vitesse. Dans ce cas, il devient impossible d'allumer directement en vitesse maximale depuis l'arrêt (§5.2).
13. Faut-il exposer la marche prolongée, si elle existe ?
14. Quelle version d'iOS : 26 ou 27 ?
15. Date d'achat : acceptes-tu de perdre la garantie ?

---

## Annexe : sources

| Réf. | Source |
|---|---|
| S1 | https://www.kitm.re/produit/hotte-inclinee-90cm-haier-cchp21ebb/ |
| S2 | https://www.haier-europe.com/fr_FR/assistance-technique/manuels/ |
| S3 | https://www.ebay.co.uk/itm/395858633061 |
| S4 | https://www.ebay.co.uk/itm/156288915049 (images ugcAAeSwhaRpMYfh, 7D8AAeSwaU9pMYfz, QnAAAeSwMrlpMYgC, QOIAAeSwj4dpMYgJ, MS0AAeSwadlpMYfq) |
| S5 | https://www.ebay.co.uk/itm/196548017973 |
| S6 | https://www.ebay.co.uk/itm/156288916804 |
| S7 | https://www.zhongshanliaoyuan.com/range-hood-touch-5-key-switch-8344606.html (5 touches Power, Light, Speed 1 à 3 ; « AC 250V / DC 12V ») ; https://www.zhongshanliaoyuan.com/range-hood-control-board.html |
| S8 | https://www.manua.ls/cda/eva60bl/manual?p=7 |
| S9 | https://www.ti.com/lit/ds/symlink/uln2003a.pdf (brochage ; VCE(sat) 0,9 V typ. / 1,1 V max. à 100 mA) |
| S10 | https://www.taydaelectronics.com/diodes/1n4741a-zener-diode-1w-11v.html |
| S11 | https://img03.71360.com/w3/jjozod/20241114/cb16529fddf2a9fa747b6c3f2c735a7f.pdf (WTC6534 : §3.1 à 3.4, alimentation de 2,7 à 5,5 V) |
| S12 | https://www.mcu.com.cn/Products/142.html |
| S13 | https://github.com/lagomCat/kitchenhood-esphome (README : UART 500 bauds, signal inversé, pauses fixes) ; https://github.com/lagomCat/kitchenhood-esphome/blob/main/custom_components/kitchen_hood/kitchen_hood.h (8 états vitesse × lumière, pause de 4 ms, FRAME_DELAY de 18 ms) |
| S14 | https://github.com/splattner/oekoboiler-uart-reverse-engineering |
| S15 | https://hackaday.io/project/195170-vevor-diesel-heater-protocol ; https://pekaway.de/en/blogs/tutorials/china-diesel-heater-adapter-bauen |
| S17 | https://github.com/TillFleisch/ESPHome-Philips-Smart-Coffee |
| S18 | https://documentation.espressif.com/esp32-c6_datasheet_en.pdf (tableaux 2-1, 5-1, 5-2, 5-4, 5-8, 5-9 ; IO MUX ; RPU 45 kΩ) |
| S19 | https://docs.espressif.com/projects/arduino-esp32/en/latest/api/serial.html ; https://github.com/espressif/arduino-esp32/pull/12744 |
| S20 | https://docs.espressif.com/projects/arduino-esp32/en/latest/api/rmt.html |
| S21 | https://github.com/ok-home/logic_analyzer ; https://sigrok.org/wiki/Fx2lafw |
| S22 | https://www.jst-mfg.com/product/pdf/eng/eXH.pdf ; https://www.jst-mfg.com/product/pdf/eng/ePH.pdf |
| S23 | https://www.adafruit.com/product/757 ; https://assets.nexperia.com/documents/application-note/AN10441.pdf |
| S24 | https://www.ti.com/lit/ds/symlink/sn74hc125.pdf ; https://www.ti.com/lit/ds/symlink/sn74hct125.pdf |
| S25 | https://sigmdel.ca/michel/ha/esp8266/super_mini_esp32c3_en.html ; https://www.lcsc.com/product-detail/C82942.html |
| S26 | https://tasmota.github.io/docs/Getting-Started/ |
| S27 | https://www.analog.com/media/en/technical-documentation/data-sheets/adum3160.pdf |
| S28 | http://ww1.microchip.com/downloads/en/AppNotes/AN18-APID.pdf |
| S29 | https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/Matter/src/MatterEndpoints/MatterFan.cpp ; https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_fan.html |
| S30 | https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/Matter/src/MatterEndpoints/MatterOnOffLight.cpp |
| S31 | https://github.com/project-chip/connectedhomeip/blob/v1.5-branch/src/app/clusters/fan-control-server/fan-control-server.cpp (pré-callback On → High, lignes 156 à 187 ; Off ↔ 0 %) |
| S32 | https://github.com/project-chip/connectedhomeip/issues/74272 (rapport de bug ouvert le 2026-09-17 contre les seuils 33 / 66 / 100 codés en dur dans Matter 1.6) |
| S33 | https://developer.apple.com/apple-home/works-with-apple-home/ |
| S34 | https://github.com/project-chip/connectedhomeip/blob/master/data_model/1.5/device_types/ExtractorHood.xml ; https://www.matteralpha.com/industry-news/ios-27-apple-home-thread-1-4-4k-energy |
| S35 | https://github.com/espressif/arduino-esp32/pull/12923 |
| S36 | https://github.com/RiDDiX/home-assistant-matter-hub/issues/387 ; …/issues/219 ; …/issues/275 |
| S37 | https://github.com/espressif/esp-matter/blob/release/v1.5/components/esp_matter/esp_matter_core.cpp |
| S38 | https://openthread.io/guides/thread-primer/router-selection ; https://openthread.io/guides/thread-primer/node-roles-and-types |
| S39 | https://support.apple.com/en-sa/guide/home/hmeb738f98cf/mac |
| S40 | https://files.seeedstudio.com/wiki/Grove-Touch_Sensor/res/TTP223.pdf ; https://manuals.plus/holtek/ht8-touch-mcu-library-manual |
| S41 | https://industry.panasonic.com/ac/cdn/e/control/relay/photomos/catalog/semi_eng_gu1a_aqy21_s.pdf ; https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/8522/semi_eng_rf_sop4_1a_cr.pdf |
| S42 | https://www.ti.com/lit/ds/symlink/cd4066b.pdf |
| S43 | https://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/CapTIvate_Design_Center/1_83_00_08/exports/docs/users_guide/html/CapTIvate_Technology_Guide_html/markdown/ch_design_guide.html |
| S44 | https://forum.arduino.cc/t/motor-with-multiple-speed-taps/658792 ; https://esphome.io/components/switch/gpio/ |
| S45 | https://support.shelly.cloud/en/support/solutions/articles/103000396565-do-shelly-devices-support-matter-over-thread- |
| S46 | https://github.com/rh1rich/esphome-ifan04 |
| S47 | https://standards.globalspec.com/std/9893105/EN%2060335-2-31 ; https://products.cooley.com/2022/07/08/the-new-blue-eu-updates-key-blue-guide-to-products-laws/ |
| S50 | https://www.amica-international.co.uk/blog/amica-acquires-100-of-cda-group |
| S51 | https://alexmeub.com/automating-capacitive-buttons-with-switchbot/ |
| S52 | https://simplymaker.net/electronics/how-to-hack-a-capacitive-touch-spring-button-with-esphome/ |
| S53 | https://www.digikey.com/en/products/detail/onsemi/SS14/965474 |
| S54 | https://www.utmel.com/components/pc817-photocoupler-datasheet-pinout-application?id=625 |
| S55 | https://esphome.io/components/remote_receiver/ |
| S56 | https://mischianti.org/esp32-c6-supermini-high-resolution-pinout-datasheet-schema-and-specs/ |
| S57 | https://github.com/espressif/esp-matter/blob/release/v1.5/components/esp_matter/data_model/esp_matter_attribute.cpp (`create_fan_mode`, `create_on_off` : ATTRIBUTE_FLAG_NONVOLATILE) ; https://github.com/espressif/esp-matter/blob/release/v1.5/components/esp_matter/data_model/esp_matter_data_model.cpp (vers la ligne 605 : relecture NVS à la création). |

**Fichiers locaux :**

| Réf. | Fichier |
|---|---|
| L1 | `~/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-uart.c` (pas de détection du débit sur C6, mode one-wire) |
| L2 | `/Users/Majid/Documents/Dev/esp32/benq/README.md`, lignes 23 à 25 |
| L3 | `/Users/Majid/Documents/Dev/esp32/benq/docs/PISTES-FUTURES.md`, ligne 41 |
| L4 | `/Users/Majid/Documents/Dev/esp32/benq/src/matter_bridge.cpp`, lignes 620 à 661 |
| L5 | `~/.platformio/packages/framework-arduinoespressif32-libs/esp32c6/sdkconfig` |
| L6 | `~/.platformio/packages/framework-arduinoespressif32-libs/esp32c6/include/esp_matter/data_model/esp_matter_attribute_utils.h`, lignes 410 à 424 |
| L7 | `~/.platformio/packages/framework-arduinoespressif32/libraries/Matter/src/MatterEndpoints/MatterFan.cpp` (`resolveFanMode` lignes 37 à 45, `attributeChangeCB` lignes 60 à 141, `begin` lignes 144 à 199, `setMode` lignes 214 à 222) ; `…/MatterOnOffLight.cpp` (lignes 61 à 72 et 106) |
| L8 | `/Users/Majid/Documents/Dev/esp32/benq/platformio.ini` (lignes 9 à 14 : huge_app, pas d'OTA, image C6 de 2,41 Mo ; lignes 125 à 128 : Thread sans Wi-Fi) |
| L9 | `~/.platformio/packages/framework-arduinoespressif32/tools/partitions/huge_app.csv` ; `…/ota_nofs_4MB.csv` (emplacement d'application de 0x1F0000) |
| L10 | `~/.platformio/packages/framework-arduinoespressif32-libs/esp32c6/include/soc/esp32c6/include/soc/soc_caps.h`, lignes 322 à 326 (2 canaux RMT en réception, 48 mots par canal, ping-pong en réception) |
| L11 | `~/.platformio/packages/framework-arduinoespressif32-libs/esp32c6/versions.txt` (esp_matter 1.5.1, IDF 5.5.5) |
| L12 | `~/.platformio/packages/framework-arduinoespressif32-libs/esp32c6/include/esp_driver_rmt/include/driver/rmt_rx.h`, ligne 59 (`en_partial_rx`) ; `~/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-rmt.c` et `.h` (pas de réception partielle) |
| L13 | `~/.platformio/packages/framework-arduinoespressif32/libraries/Matter/src/MatterEndPoint.h`, ligne 99 (`updateAttributeVal` public) ; `MatterEndPoint.cpp`, lignes 106 à 113 |
| L14 | `~/.platformio/packages/framework-arduinoespressif32/variants/esp32c6/pins_arduino.h`, ligne 16 (TX = 16) ; `cores/esp32/HardwareSerial.h`, lignes 451 à 454 |

---

## Corrections rejetées

Toutes les autres corrections de la revue sont appliquées. Certaines sont précisées au passage : le niveau de repos avec un pull-up de 30 à 50 k, le dimensionnement du couplage capacitif, et la mesure analogique qu'exige le test « qui parle ». Deux points sont rejetés, en tout ou en partie :

1. **Revue n° 10, « évaluer une table de partitions avec OTA, si l'image y tient (à mesurer) » : rejeté.** La mesure existe déjà. L'image Matter du C6 fait 2,41 Mo [L8, lignes 12 à 14]. L'emplacement d'application le plus grand avec OTA sur 4 Mo fait 0x1F0000, soit environ 1,94 Mio [L9]. L'OTA ne tient donc pas. Le reste du point 10 est appliqué : pont de journaux, USB-C accessible (§4.6).
2. **Revue n° 4, « vitesse : sorties relais de IC3 en 12 V, lues via le tampon » : rejeté en partie.** À l'état actif, une sortie ULN2003 reste à 0,9 V typique et 1,1 V maximum à 100 mA [S9]. Le seuil du tampon 100k/100k est d'environ 2 × VBE, soit 1,2 à 1,3 V, et il baisse à chaud (S). La marge est insuffisante dans une hotte chaude. Le brief lit donc de préférence les entrées de IC3 (broches 1 à 7), ou les sorties avec un seuil relevé (étape 6). Le reste du point 4 est appliqué.