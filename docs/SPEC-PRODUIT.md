# Spec : sous-projet 2, le produit Matter de la hotte

**Projet :** piloter la hotte Haier CCHP21EBB (90 cm) depuis Apple Maison, en
Matter sur Thread, avec un ESP32-C6, **sans jamais toucher au secteur**.

**Statut :** conception tirée des décisions de Majid du 28/09/2026, validées
partie par partie (exposition, logiciel, matériel, vérification, ordre), et de
son **amendement du 28/09 au soir**, qui prime : boîtier en ASA imposé, ni
thermomètre ni relevé de température en cuisson, température de la puce en
filet de sécurité, achats faits. Relue le 28/09 (cohérence, technique,
sécurité) et corrigée. Questions de relecture tranchées par Majid le 29/09 (§13).

**Portée.** Cette spec couvre tout le produit. Le **premier plan
d'implémentation** n'en couvre que les points 2 à 4 du §10 (automate, couche
Matter avec pilote simulé et banc Matter, profil compagnon). Les points 6 à 9
(alimentation, pilote réel, boîtier, installation) auront **leur propre plan,
après l'étape 6** de la reconnaissance.

**Documents liés :**
- [SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md) : sous-projet 1. Ses règles
  de sécurité (§5) valent pour toute intervention sur la hotte, y compris
  pendant ce sous-projet, avec l'exception écrite au §6.7 (règle 10).
- [RECONNAISSANCE.md](RECONNAISSANCE.md) : journal (étape −1 complète, étapes 0
  et 1 du 28/09).
- [AJOUTS-RECONNAISSANCE.md](AJOUTS-RECONNAISSANCE.md) : les ajouts à la reconnaissance (blocs A1 à F1), à verser au
  point 1 du §10.1 : étape −1b, scénarios 7b et 7c, tests 5 et 6 de l'étape 6,
  arrêt de l'étape 7, mesure radio M1, repérage de la SuperMini.
- [SECURITE.md](SECURITE.md), [WIRING.md](WIRING.md),
  [PROTOCOLE-JSON.md](PROTOCOLE-JSON.md) (profil « hotte » de la sonde),
  [BRIEF-RECHERCHE.md](BRIEF-RECHERCHE.md) (§4.3, §5, §6).
- Projet frère `benq` au commit `c58a506` : `README.fr.md`, `platformio.ini`,
  `docs/PROTOCOLE-JSON.fr.md` (§10, transport UDP sur Thread),
  `tools/macos/halo-routes`.

**Convention.** « **(NV)** » marque une affirmation technique **non vérifiée**.
Elle renvoie à une question du §13, qui dit quand et comment on la vérifie :
« (NV, Q7) ». Tout ce qui n'est pas marqué est lu dans une source citée ou dans
le dépôt.

**Vocabulaire**

| Terme | Sens |
|---|---|
| Maison | l'app Maison d'Apple et ses concentrateurs (HomePod, Apple TV) |
| panneau | le panneau tactile d'origine (5 touches, voyants), relié à `CN3` |
| carte | la carte de puissance `XB_DYB_V4` (MCU IC2, relais, buzzer) |
| module | le produit : un ESP32-C6 dans un boîtier imprimé, dans le boîtier électronique de la hotte |
| traversée | petite plaque XH qui s'intercale entre `CN3` et la fiche du panneau ; elle porte la diode, le fusible réarmable et la résistance haute du pont de mesure, et alimente le module |
| armée | ⏻ allumé fixe, moteur arrêté |
| prolongée-Vx | marche prolongée : moteur en Vx, ⏻ clignote à 1 Hz, 15 min |
| palier | V1, V2 ou V3, vus depuis Matter comme 33, 66 ou 100 % |
| appui | une touche injectée par le module, ou appuyée au panneau |
| séquence | la suite d'appuis qui mène la hotte de son état à une cible |
| confirmation | un état lu **sur le fil** (ou par la lecture annexe), postérieur à un appui, qui en montre l'effet |
| lecture annexe | lecture de la lampe et du moteur hors de `D` (§6.4), si le fil ne dit pas l'état |
| surveillance | tension des deux voies d'alimentation et température de la puce, avec leurs alertes (§7.4) |

---

## 0. En bref

- **Un seul nœud Matter, sans pont.** EP1 Ventilateur (Fan, 0x002B), trois
  paliers ; EP2 Lumière (On/Off Light). Pas d'arrêt différé exposé : Siri ou
  « Éteindre après » s'en chargent.
- **Critère des voyants, chiffré :** moteur et lampe toujours identiques entre
  panneau et Maison ; appui au panneau visible dans Maison en 2 s ; ordre de
  Maison sur les voyants en 3 s depuis « armée », 5 s depuis l'arrêt.
- **Firmware dans ce dépôt**, environnement PlatformIO `produit`, en quatre
  couches : automate pur testé sur le Mac, pilote de la ligne `D`
  interchangeable (simulé dès maintenant ; A, B ou C après l'étape 6), couche
  Matter reprise des recettes de benq, canal compagnon profil « hotte ».
- **Règle d'or de l'automate :** jamais « marche » moteur tournant (cela
  déclenche la marche prolongée). « Marche » n'est appuyée qu'après un arrêt du
  moteur **lu** (fil ou lecture annexe), y compris juste après un redémarrage
  du module. Pour éteindre depuis une vitesse : vitesse active, arrêt confirmé,
  puis marche.
- **Matériel :** SuperMini violette, sous condition de la mesure radio M1 ;
  alimentation par le `+` de `CN3`, s'il est entre 4,75 et 5,5 V, à travers une
  1N5819 et un fusible réarmable (RXEF025, ou RXEF050 si l'essai de 24 h
  l'exige) ; rien de soudé sur la carte de la hotte ; **boîtier imprimé en ASA,
  imposé** ; distances de la CEI 60335-1.
- **Température :** ni thermomètre ni relevé en cuisson (amendement du 28/09 au
  soir). Filet de sécurité : la température de la puce, publiée par le
  compagnon, avec une alerte au-delà d'un seuil.
- **Ce qu'on fait sans attendre l'analyseur :** l'automate et ses tests, la
  couche Matter et le banc Matter sur une C6 de test, le profil compagnon, la
  mesure radio M1, le volume et les dimensions, hotte débranchée.

---

## 1. But, périmètre et critères de sortie

### 1.1 But

Commander depuis Maison les trois vitesses et la lumière de la hotte, en gardant
le panneau d'origine, **ses voyants toujours d'accord avec Maison**, et la
carte d'origine maître des relais, des bips et des voyants.

### 1.2 Ce que fait le produit

- Allumer la hotte à une vitesse, changer de vitesse, l'éteindre depuis Maison,
  Siri, une automatisation ou l'app compagnon.
- Allumer et éteindre la lampe, y compris par « éteins les lumières de la
  cuisine ».
- Refléter dans Maison, en 2 s au plus, tout appui fait au panneau, marche
  prolongée comprise.
- Laisser le panneau prioritaire : un appui physique annule une séquence en
  cours.
- Arrêt différé : par Siri (« éteins la hotte dans 15 minutes ») ou par
  « Éteindre après » dans une automatisation (arrêt net). La marche prolongée
  native reste au panneau.
- Diagnostics par l'app compagnon (état, compteurs, trames décodées, radio,
  alimentation, température de la puce), sans injection brute ; alertes de
  tension et de température (§7.4), sans action sur la hotte.

### 1.3 Hors périmètre

- Une tuile « Arrêt différé » ou une minuterie dans le firmware.
- Les capteurs, les scènes sur les touches du panneau.
- Le type Extractor Hood (0x007A) : absent des listes d'Apple et de Google, sans
  classe Arduino.
- MultiSpeed (SpeedMax = 3) : pas dans la première version. Essai facultatif au
  banc Matter s'il ne coûte presque rien (§9.2, B14).
- L'OTA : l'image Thread (environ 2,62 Mo) dépasse l'emplacement OTA d'une
  flash de 4 Mo. Mises à jour par l'USB, hotte débranchée (§6.8).
- La remise à zéro à distance, et tout voyant ajouté au module.
- Google Home (il refuse un appareil au VID de test sans projet dans sa
  console) ; Home Assistant n'est pas visé, sans être exclu.
- Toute intervention côté 230 V, toute soudure sur la carte de la hotte, tout
  perçage ou limage de la hotte.
- Tout thermomètre dans la hotte et tout relevé de température en cuisson
  (amendement du 28/09 au soir).
- La bibliothèque commune et l'app multi-appareils : sous-projet 3. Le code
  commun est **copié au besoin** de benq et de la sonde, puis extrait au
  sous-projet 3.

### 1.4 Critères de sortie du sous-projet 2

Le sous-projet est terminé quand les cinq critères sont remplis :

1. **Critère des voyants validé sur la hotte** (§9.5) : chaque touche du panneau
   20 fois, chaque ordre de Maison 20 fois, dans les délais du §3.6, voyants
   corrects à chaque fois.
2. **Robustesse validée** (§9.5) : appui physique en pleine séquence,
   redémarrage du module hotte en marche (méthode : Q27), coupure de courant.
3. **Une semaine d'usage normal** (§9.6) sans désaccord hors des fenêtres
   tolérées du §3.6, sans redémarrage inexpliqué du module, et sans alerte de
   température (sinon on en rediscute avec Majid).
4. **Essai d'alimentation de 24 h réussi** (§9.4), et montage conforme aux
   règles du §6.7, contrôlé à la pose en deux temps (point 8 : couvercle
   ouvert, puis couvercle fermé).
5. **Documents à jour :** cette spec et ses avenants, le profil compagnon
   produit, le journal d'installation, le README.

---

## 2. Ce qui est fixé

### 2.1 Décisions de Majid (28/09/2026) et amendement du 28/09 au soir

Elles priment sur le dossier de cadrage ; l'amendement prime sur elles. Les
lignes qu'il change sont marquées « (amendement) ».

| Sujet | Décision |
|---|---|
| Nœud Matter | un seul nœud, sans pont |
| EP1 | Fan (0x002B), `FanModeSequence` = OffLowMedHigh, **sans Auto** ; curseur 0-100 % en trois paliers : 1-33 % V1, 34-66 % V2, 67-100 % V3 ; 33/66/100 publiés quand la vitesse change au panneau |
| EP2 | On/Off Light, type Lumière : obéit à « éteins les lumières de la cuisine ». Figé avant la première mise en service |
| Affichage | accessoires séparés, conseillé |
| Correspondance | éteinte ou armée : ventilateur éteint ; moteur Vx : allumé au palier ; prolongée : allumé, puis éteint seul à la fin ; lampe indépendante |
| Allumer sans vitesse | **dernière vitesse** (V2 la toute première fois), faisabilité à valider au banc Matter |
| Éteindre depuis Maison | tout s'éteint, voyants compris |
| Arrêt différé | **non exposé** : Siri ou « Éteindre après » ; marche prolongée native au panneau |
| Critère des voyants | chiffré (§3.6) |
| Repli si l'étape 6 conclut « panneau maître de ses voyants et résiné » | panneau de rechange à dérésiner pour passer en C sans risquer l'original (disponibilité à vérifier) ; sinon arrêt du produit pour cette hotte |
| Firmware | dans ce dépôt, environnement PlatformIO `produit` à côté de `sonde` et `generateur` |
| Plateforme | celle de benq, figée (§2.4) ; carte de 4 Mo ; pas d'OTA |
| Code commun | repris de benq et de la sonde au besoin ; extraction au sous-projet 3 |
| Compagnon | profil « hotte » : UDP sur Thread (H1, port 5480, nom SRP) comme la ScreenBar, et USB ; état, commandes vitesse et lumière, diagnostics ; **pas d'injection brute** ; valeur de `build` distincte de la sonde ; clé H1 posée par l'USB avant le montage |
| Identité | fabricant `Djoko-CLI`, série `HOTTE-<MAC>`, discriminateur et code d'appairage distincts de la ScreenBar, VID/PID de test 0xFFF1/0x8000 ; étiquette QR et code manuel derrière le filtre ; remise à zéro par l'USB seulement ; aucun voyant ajouté |
| Réseau Thread | routeur par défaut (éligible routeur, §6.1), MED en repli si la radio est juste ; puissance d'émission réglable |
| Carte | SuperMini violette, sous condition d'une mesure radio (§6.1) ; une SuperMini libre est réservée au module (amendement) |
| Raccordement | traversée XH (fiche sur `CN3`, embase pour la fiche du panneau) ; rien de soudé sur la carte de la hotte ; montage réversible |
| Alimentation | `+` de `CN3` → Schottky en série (**1N5819**, achetée) → fusible réarmable (**RXEF025 ou RXEF050**, achetés, choisi à l'essai de 24 h : amendement) → 470 µF (**16 V, 105 °C, 8×12**, acheté) → broche `5V` ; émission réduite si la marge est juste ; repli : rail 12 V permanent et abaisseur dédié, jamais `12V_LED` ; jamais d'USB branché sur le module raccordé à `CN3` |
| Boîtier | imprimé en **ASA, imposé** (amendement : PETG exclu, il se déforme vers 70 °C ; ASA jusque vers 90-95 °C) ; CEI 60335-1 ; deux fixations indépendantes ; aucun perçage ; quart haut-droit du boîtier électronique |
| Température | **ni thermomètre, ni relevé de température en cuisson** (amendement : le module est dans le boîtier d'origine, derrière la grille, et Majid juge l'ASA suffisant) ; filet de sécurité : la température interne du C6, publiée dans les diagnostics du compagnon, avec une alerte au-delà d'un seuil (§7.4) |
| Achats | faits (amendement) : 1N5819, RXEF025 et RXEF050, 470 µF 16 V 105 °C 8×12 ; ASA déjà là ; une SuperMini libre pour le module |
| Garantie | Majid accepte la perte de garantie et la responsabilité de la modification |
| Radio | routeurs de bordure dans la même pièce, à moins de 5 m |

### 2.2 Comportement de la hotte (étape −1, 27/09)

Source : RECONNAISSANCE.md, étape −1 (tableau et chronologie de la vidéo
`IMG_9091`). Temps de la vidéo entre parenthèses.

| Départ | Appui | Résultat | Preuve |
|---|---|---|---|
| éteinte | marche | armée | observé (1,8 s ; 5,8 s ; 20,2 s) |
| éteinte | Vx | **rien**, pas de bip | réponse de Majid (ligne 1) |
| armée | marche | éteinte | observé (3,9 s ; 15,1 s ; 33,0 s) |
| armée | Vx | Vx, le moteur démarre | observé (7,6 s ; 21,6 s) |
| Vx | Vy | Vy directement | observé (9,8 s ; 11,5 s ; 24,0 s ; 25,8 s) |
| Vx | Vx (active) | armée : moteur arrêté, ⏻ fixe | observé en V3 (12,9 s) ; supposé identique en V1 et V2 (NV, Q30), codé comme observé (§5.2) |
| Vx | marche | prolongée-Vx | observé en V3 (27,7 s) ; supposé identique en V1 et V2 (NV, Q30), codé comme observé (§5.2) |
| prolongée-Vx | Vx (active) | armée | observé (31,2 s) |
| prolongée-Vx | fin des 15 min | **inconnu** | ajout (a) à la reconnaissance (Q2) |
| prolongée-Vx | marche | **inconnu** | ajout (a) (Q2) |
| prolongée-Vx | Vy | **inconnu** | ajout (a) (Q2) |
| tout état | lumière | la lampe bascule, indépendante de la marche | observé (16,7 s ; 18,6 s ; 20,9 s ; 33,6 s) |
| tout état | coupure secteur | éteinte, lampe éteinte, un bip au retour | réponse de Majid (ligne 9) |

Autres faits :
- un bip identique (environ 200 ms à 4,11 kHz) pour chaque appui : chaque appui
  du panneau passe par `D` jusqu'à la carte ;
- aucun appui long caché (3 s sur chaque touche) ;
- « armée » ne s'éteint pas seule, ou pas avant 30 min ;
- le moteur ralentit en environ 1,5 s après son arrêt ;
- délai prudent entre deux changements de vitesse : non mesuré, 3 s par défaut
  (Majid a changé toutes les 1,5 à 2,5 s sans incident).

### 2.3 Matériel de la hotte (28/09)

- **Carte `XB_DYB_V4`**, alimentation isolée par flyback (`BK-22-2232`,
  `PC817C`, `CY1`). IC2 : MCU SOP-14 sans marquage. IC3 : `ULN2003H` vers
  3 relais.
- **IC4 = `XL1509-5.0`** avec `D1` SS14 et `L1` 68 µH : le `+` de `CN3` est
  probablement à 5 V (NV, Q4 : mesure 3b). La SS14 limite la branche à environ
  1 A.
- **`CN3` en JST XH** 3 broches, `- D +` (noir, blanc, rouge). La fiche du
  panneau se débranche ; une goutte de colle à la sortie des fils.
- **`CN1` = `12V_LED`**, sortie commutée. **`CN2` « - + »** libre, fonction
  inconnue.
- **Le `−` de `CN3` est relié à la terre** (0 Ω), la carcasse aussi (moins de
  1 Ω). Conséquence : le `+` contre la carcasse est un court-circuit franc.
- **Accès sans outil :** filtre à languette, puis boîtier électronique **en
  plastique noir sous le moteur**, couvercle clipsé. Condensateur moteur CBB61 à
  gauche, gros connecteur blanc en bas à droite, `CN3` en haut à droite.
- Près de `CN3` : `R13` 4,7 kΩ et `R1` 1 kΩ, sans doute pull-up et résistance
  série de `D` (NV, Q33 : étapes 2b et 3c).

### 2.4 Plateforme (vérifiée le 28/09 dans les fichiers)

| Élément | Valeur | Source |
|---|---|---|
| Plateforme PlatformIO | pioarduino `55.03.312-1` | `benq/platformio.ini` ; `hotte haier/platformio.ini` |
| Cœur Arduino-ESP32 | 3.3.12 | idem |
| ESP-IDF | v5.5.5 (`b774170ff46`) | `framework-arduinoespressif32-libs/esp32c6/versions.txt` |
| esp_matter | 1.5.1 | idem |
| Carte PlatformIO | `esp32-c6-devkitc-1`, flash forcée à 4 Mo | les deux `platformio.ini` |
| Partitions | `huge_app.csv` : application 0x300000 (3 145 728 octets), `coredump` 64 Ko à 0x3F0000, pas d'OTA | `tools/partitions/huge_app.csv` |
| Image Thread de référence | 2 616 992 octets (benq, `esp32c6thread`) | `benq/README.fr.md` |
| Réglages de la plateforme utiles ici | core dump en flash actif (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`), chien de garde des tâches à 5 s, BLE à 9 dBm par défaut (`CONFIG_BT_LE_DFT_TX_POWER_LEVEL_DBM_EFF`), horloge lente sur l'oscillateur RC interne (GPIO0 et GPIO1 libres) | `sdkconfig.h` des bibliothèques C6 |

On ne monte ni vers Arduino 4.0-RC1 ni vers CHIP 1.6 sans refaire le banc
Matter : en 1.6, FanControl devient « code-driven » et les paires 33/66/100 y
sont codées en dur. Plusieurs points de ce document reposent sur le code exact
d'esp_matter 1.5.1 (§3.3, §4.4) : une montée de version les rouvre.

### 2.5 Ce qui attend encore la reconnaissance

Le détail est au §10.3. En résumé : la tension du `+` (3b), le pull-up de `D`
(3c), qui pilote les voyants (4 et 6), la nature de `D` et le choix A, B ou C
(6), si la carte émet son état et le répète au repos (5 et ajout c), la marche
prolongée et la trace de sa fin sur `D` (ajout a, test 6), l'avenant
d'injection (6), l'injection prouvée 20 fois sur 20 (7).

---

## 3. Exposition dans Maison

### 3.1 Structure du nœud

| Endpoint | Type | Classe Arduino | Clusters utiles |
|---|---|---|---|
| EP0 | Root Node | pile Matter | Basic Information (identité, §8.1), Network Commissioning (Thread seul) |
| EP1 « Hotte » | Fan (0x002B) | `MatterFan`, dans une sous-classe `MatterFanHotte` (§4.4) | FanControl : `FanMode`, `FanModeSequence` = OffLowMedHigh, `PercentSetting`, `PercentCurrent` ; Identify, Groups (obligatoires pour le type). **Ni OnOff, ni MultiSpeed, ni Auto** |
| EP2 « Éclairage hotte » | On/Off Light | `MatterOnOffLight` | OnOff |

- `Fan.begin(0, FAN_MODE_OFF, FAN_MODE_SEQ_OFF_LOW_MED_HIGH)`.
- `Matter.selectNetwork(MATTER_NETWORK_THREAD)` avant le premier `begin()`
  d'endpoint : sinon l'EP0 n'annonce que le Wi-Fi.
- **La structure est figée avant la première mise en service** : changer le
  type ou le nombre d'endpoints oblige à remettre le nœud en service.

### 3.2 De l'état de la hotte vers Maison

| État de la hotte | EP1 `FanMode` | EP1 `PercentSetting` / `PercentCurrent` | Tuile |
|---|---|---|---|
| éteinte | Off | 0 / 0 | éteinte |
| armée | Off | 0 / 0 | éteinte |
| V1 | Low | 33 / 33 | allumée, 33 % |
| V2 | Medium | 66 / 66 | allumée, 66 % |
| V3 | High | 100 / 100 | allumée, 100 % |
| prolongée-Vx | comme Vx | comme Vx | allumée au palier ; s'éteint quand l'état lu passe à l'arrêt |

- EP2 `OnOff` = lampe allumée ou non.
- **L'arrêt publie toujours `PercentSetting` = 0 avec `FanMode` = Off** :
  sinon Apple affiche « Extinction… » sans fin.
- Après tout changement, le nœud publie la **valeur canonique du palier**
  (33, 66 ou 100) dans `PercentSetting` et `PercentCurrent`. Un curseur posé à
  50 % revient donc à 66 % une fois la séquence finie.
- **Le nœud ne publie de lui-même que l'état lu** (§5.1) : `PercentCurrent`,
  les valeurs de palier, `OnOff`. La pile, elle, aligne l'attribut écrit sur la
  valeur reçue ; et pour `FanMode` = Off ou `PercentSetting` = 0, le serveur
  CHIP met aussitôt `FanMode` à Off et les deux pourcentages à 0 (post-rappel,
  `fan-control-server.cpp`). Maison peut donc voir la cible avant tout appui.
  **L'état réel est republié en fin de séquence** : réussite, échec, abandon ou
  annulation. Le seul écart évitable l'est : `MatterFan` publierait
  `PercentCurrent` = la valeur demandée dès l'écriture (`MatterFan.cpp`,
  l. 110 à 118) ; `MatterFanHotte` ne le fait pas (§4.4).

### 3.3 Des ordres de Maison vers la hotte

| Écriture reçue | Cible |
|---|---|
| EP1 `PercentSetting` = 0 | éteindre |
| EP1 `PercentSetting` 1-33 / 34-66 / 67-100 | V1 / V2 / V3 |
| EP1 `FanMode` = Off | éteindre |
| EP1 `FanMode` = Low / Medium / High | V1 / V2 / V3 |
| EP1 « allumer sans vitesse » (règle ci-dessous) | dernière vitesse |
| EP2 `OnOff` (tuile, Siri, « éteins les lumières de la cuisine ») | lampe allumée ou éteinte |
| Identify | aucun effet (pas de voyant, et la lampe de la hotte n'est pas détournée) ; la demande est journalisée |

Les écritures d'EP1 arrivent souvent par deux (`FanMode` et `PercentSetting`) :
elles passent par **un seul regroupement**, avec les priorités de la règle 6 du
§5.4.

**Éteindre** mène la hotte à l'état **éteinte** : moteur arrêté **et** ⏻
éteint, jamais « armée ». La lampe, accessoire séparé, n'est pas touchée : elle
reste indépendante, comme au panneau. C'est la lecture retenue de « tout
s'éteint, voyants compris » (Q26).

**Allumer sans vitesse : dernière vitesse.** La dernière vitesse vaut le
dernier palier atteint, quelle que soit son origine (panneau, Maison, app). Elle
est gardée en NVS ; V2 la toute première fois.

**Ce que fait la pile (esp_matter 1.5.1, lu dans le code).** Une écriture de
`FanMode` passe d'abord par le pré-rappel PRE_UPDATE, donc par
`attributeChangeCB`, **avec la valeur brute**
(`esp_matter_data_model_provider.cpp`, l. 360). Le pré-rappel du serveur CHIP
ne vient qu'après (l. 412). C'est lui qui traite On (4) : il écrit High, écriture
qui repasse par nos rappels, et rend `WriteIgnored` (0xF0, code interne hors
spécification), que la pile renvoie tel quel au contrôleur (l. 412 à 414 ;
`WriteHandler.cpp` ne le convertit pas). Sans parade, Maison recevrait donc un
statut hors norme à chaque On, sans doute lu comme un échec, et `onChangeMode`
ne verrait jamais On, seulement High
(`resolveFanMode`, `MatterFan.cpp`, l. 76). Règle :

1. Le banc Matter (B2) relève ce qu'écrit Maison au bouton de la tuile d'un Fan
   **sans** cluster OnOff (`FanMode` = On, `PercentSetting` = 100, ou sa
   dernière valeur), le statut qu'elle reçoit et sa réaction (Q7).
2. **Si Maison réécrit elle-même son dernier pourcentage**, la décision est
   tenue sans rien détecter : on applique la valeur reçue.
3. **Si Maison écrit `FanMode` = On**, `MatterFanHotte::attributeChangeCB` le
   voit, valeur brute. Elle dépose l'intention « dernière vitesse », puis
   **remplace la valeur** (`val->val.u8`) par le `FanMode` de la dernière
   vitesse (Low, Medium ou High), lu dans une copie atomique tenue par la tâche
   `loop`. Le pointeur est celui de la valeur décodée, que reprennent ensuite le
   pré-rappel CHIP et l'écriture en base (`set_val_internal`) : la conversion
   en High n'a plus lieu, la base garde le mode de la dernière vitesse, et
   Maison reçoit Success. Lu dans le code ; à confirmer au banc (B2). **La boîte
   d'intentions est alimentée par cette surcharge, avec la valeur brute, jamais
   par `onChangeMode`.** Un `PercentSetting` > 0 dans la même fenêtre de
   regroupement l'emporte (§5.4, règle 6).
4. **Si Maison écrit `PercentSetting` = 100** au bouton de la tuile, rien ne
   distingue le bouton d'un curseur posé à 100 % : le bouton donne V3. Le
   constat, mesures en main, est présenté à Majid à la fin du banc Matter.

### 3.4 Arrêt différé : non exposé

- **Siri :** « éteins la hotte dans 15 minutes » crée une automatisation
  temporaire, qui exige un concentrateur (les routeurs de bordure en sont). À
  l'échéance, le nœud reçoit un Off ordinaire : arrêt net, séquence
  « éteindre » (§5.3).
- **Automatisation :** « Éteindre après » une durée choisie. Même effet.
- **Limite (NV, Q11) :** une automatisation ne peut pas éteindre après un délai
  l'accessoire qui l'a déclenchée sans passer par un accessoire factice
  (exploration « Matter et Maison », fait 16 et option B2). Le cas naturel,
  « quand la hotte s'allume, l'éteindre au bout de 15 min », n'est donc
  peut-être pas faisable tel quel. Le banc Matter l'essaie et note le
  contournement nécessaire (B6). Le résultat est présenté à Majid avec les
  phrases Siri.
- La formulation française et la durée maximale de « Éteindre après » sont
  relevées au banc Matter (NV, Q11).
- **La marche prolongée native reste au panneau.** Maison la voit comme
  « allumée au palier », puis éteinte quand la hotte s'arrête (§5.9).

### 3.5 Affichage conseillé

- **Accessoires séparés** dans Maison. Si Majid garde la tuile regroupée, un
  ordre de la tuile atteint les deux endpoints (constaté sur benq) : « tout
  éteindre » ou « tout allumer » ont ici un sens acceptable, sans garde-fou
  particulier.
- Noms proposés : « Hotte » (ventilateur) et « Éclairage hotte » (lampe), dans
  la pièce « Cuisine ». Le banc Matter vérifie que « éteins la hotte » ne touche
  que le ventilateur (B5).

### 3.6 Critère des voyants

**Critère, chiffré et testable** (décision de Majid ; les compléments proposés
sont marqués et renvoient à une question) :

| # | Exigence | Mesure |
|---|---|---|
| C1 | moteur (arrêt, V1, V2, V3) et lampe **toujours identiques** entre panneau et Maison, **en régime établi** : hors des fenêtres de C2 et de C3 qui suivent un appui ou un ordre, et hors des désaccords tolérés ci-dessous | observation, et journal des désaccords (§9.6) |
| C2 | un appui au panneau est visible dans Maison **en 2 s au plus** | journaux horodatés du module pour les 100 appuis de V1 (de la trame de l'appui sur `D` à la publication de l'attribut) ; au moins 5 appuis par touche filmés, écran de Maison et panneau dans le même champ : affichage en 2 s au plus |
| C3 | un ordre de Maison met les voyants à jour **en 3 s au plus depuis « armée », 5 s depuis l'arrêt**. Complément proposé pour les autres ordres (Q35) : **3 s** pour un ordre d'un appui (changer de palier, lampe, éteindre depuis « armée »), **5 s** pour un ordre de deux appuis (éteindre depuis un palier ou depuis la marche prolongée) | vidéo horodatée : écran de Maison et panneau dans le même champ |

**Écarts à valider par Majid, absents de sa décision :**
- un ordre qui suit un changement du moteur de moins de `delai_moteur_ms`
  (§4.2) attend ce délai, qui protège les relais : C3 se mesure depuis un état
  stable (Q35) ;
- dans un conflit, Maison garde environ 10 s l'état qu'elle a demandé
  (ci-dessous, Q9).

**Désaccords tolérés, et seulement eux :**
- pendant un **redémarrage du module** : au plus **75 s** après le redémarrage
  (borne proposée pour « environ 1 min », Q36). Apple se réabonne en 49 à 56 s
  sur benq avec le plafond de 20 s (43 s avec 10 s ; plancher d'Apple vers
  40 s : `benq/src/matter_bridge.h`, l. 66 à 70) ;
- quand Maison affiche **« Pas de réponse »**.

« Armée » compte comme « éteinte » dans Maison, par construction (§3.2) : ce
n'est pas un désaccord.

**Un écart à valider par Majid (Q9).** Maison garde l'état qu'elle a demandé
environ 10 s, même si le nœud rapporte le contraire (constaté sur une prise de
benq ; pour un ventilateur : NV, Q9). Dans un
**conflit** (un appui au panneau pendant une séquence demandée par Maison), le
panneau gagne (§5.6) ; Maison peut alors afficher la cible abandonnée jusqu'à
environ 10 s. Proposition : tolérer ce cas, borné par le maintien mesuré au banc
(B3). Les voyants, eux, restent justes : ce sont ceux de la carte.

### 3.7 Budget de latence

Estimations de conception, à confirmer au banc Matter (B9) puis à l'étape 7 et à
la validation.

| Poste | Estimation | Source ou réglage |
|---|---|---|
| Maison → nœud | 0,1 à 0,5 s (NV, Q8) | banc Matter |
| regroupement d'EP1 (`FanMode` et `PercentSetting`) | `lissage_calme_ms`, 700 ms par défaut | §4.2 ; réglé au banc (B1, B2, B9) |
| regroupement d'EP2 (`OnOff`) | `ordre_calme_ms`, 150 ms | §4.2 |
| attente du silence sur `D`, puis trame | jusqu'à `attente_max_ms` (1 s par défaut) + la trame | avenant de l'étape 6 |
| réaction de la carte et de ses voyants | inconnue (NV, Q5) | étape 7 |
| confirmation lue | jusqu'à `confirmation_ms` (1 s par défaut) | avenant |
| appui suivant (depuis l'arrêt : marche puis vitesse) | `entre_appuis_ms` + un second passage | avenant |
| panneau → publication | quelques dizaines de ms dans le module | §4.3 |
| publication → affichage | intervalle minimal de l'abonnement d'Apple (NV, Q8) | banc Matter |

Depuis « armée » (un appui) : environ 1,5 à 2,5 s plus la réaction de la carte.
Depuis l'arrêt (deux appuis ; en régime établi, l'arrêt du moteur est déjà lu,
et « marche » part sans attente supplémentaire) : environ 2,5 à 4,5 s plus deux
réactions. Le budget de 5 s est donc tendu : si l'étape 7 mesure une réaction
de la carte au-delà de 0,5 s, on raccourcit `lissage_calme_ms` et
`attente_max_ms` avant toute autre chose.

---

## 4. Architecture logicielle

### 4.1 Vue d'ensemble

```
        Maison (Matter sur Thread)               App compagnon (UDP 5480 sur Thread, ou USB)
                  │                                              │
     ┌────────────▼─────────────┐                  ┌─────────────▼────────────┐
     │ 3. matter_hotte          │                  │ 4. compagnon, profil     │
     │ boîte d'intentions,      │                  │ « hotte », build produit │
     │ reflets, démarrage, NVS  │                  │ (json_mode, H1, UDP)     │
     └────────────┬─────────────┘                  └─────────────┬────────────┘
        ordres ▼  │  ▲ état à publier                 ordres ▼   │  ▲ état, diagnostics
     ┌────────────▼──┴──────────────────────────────────────────▼──┴────────────┐
     │ 1. hotte_etat : automate en logique pure (testé sur le Mac)              │
     │ état et confiance, table des transitions, suites d'appuis, idempotence,  │
     │ délais, regroupement, annulation par le panneau                          │
     └────────────┬─────────────────────────────────────────────▲──────────────┘
        appuyer ▼ │                                              │ événements : état lu,
     ┌────────────▼──────────────────────────────────────────────┴──────────────┐
     │ 2. pilote de ligne : simulé (maintenant) | A | B | C (après l'étape 6)   │  appuis, fins d'appui
     │ garde-fous : GPIO d'injection basse d'abord, drain ouvert, durées bornées │
     └────────────┬─────────────────────────────────────────────────────────────┘
                  │ GPIO0 écoute, GPIO1 injection (A) ; voie 2 (B) ; PhotoMOS (C)
             ligne D de CN3 (traversée XH)

     5. identité (transverse) : EP0, série HOTTE-<MAC>, codes d'appairage, étiquette
     6. surveillance (transverse) : tensions (GPIO2, GPIO3), température de la puce, alertes
```

**Ordre de `loop()`** (un tour dure quelques ms ; aucune attente bloquante) :
pilote (`poll`, puis ses événements vers l'automate) ; boîte d'intentions
Matter (regroupement, ordres vers l'automate) ; compagnon (lignes USB et UDP,
ordres vers l'automate) ; automate (`suivante`) ; appuis vers le pilote ; reflets
vers Matter ; messages périodiques du compagnon ; mesures (ADC des deux voies
en continu ; température de la puce toutes les 10 s) et surveillance (seuils,
alertes) ; écritures NVS différées ; `delay(1)`.

### 4.2 Couche 1 : l'automate `hotte_etat`

**Rôle.** Tenir l'état de la hotte et sa confiance, calculer le prochain appui
vers la cible, confirmer, réessayer une fois quand l'échec est prouvé, publier
l'état réel. **Logique pure** : ni Arduino, ni pile Matter, ni horloge propre
(le temps est passé en argument). Compilée et testée sur le Mac, comme
`injection_regles` de la sonde ou `status_led` de benq.

**Interface, en C++ indicatif** (noms et signatures ; pas d'implémentation) :

```cpp
namespace hotte {

enum class Touche : uint8_t { Marche, Lumiere, V1, V2, V3 };
enum class Moteur : uint8_t { Arret, V1, V2, V3 };
enum class Marche : uint8_t { Eteinte, Armee, Prolongee, Inconnue };
enum class Confiance : uint8_t { Confirme, Deduit, Presume, Inconnu };   // §5.1
enum class Source : uint8_t { Fil, Annexe, Deduit, Nvs };
enum class Origine : uint8_t { Panneau, Module, Inconnue };
enum class Demarrage : uint8_t { MiseSousTension, Autre };               // §5.7

struct Etat {
  Marche marche; Moteur moteur; bool lampe; bool lampeConnue;
  Confiance confiance; Source source;
  uint32_t depuisMs;   // heure du dernier état lu ou déduit
  uint32_t luMs;       // heure du dernier état LU (fil ou annexe) ; 0 : aucun depuis le démarrage
};

// Table du §5.2. observee = faux : transition jamais observée (résultat inconnu).
struct Transition { Etat apres; bool observee; };
Transition appliquer(const Etat &avant, Touche t);

// Correspondance Matter (§3.2, §3.3), pure.
Moteur palier(uint8_t pourcent);            // 0 -> Arret ; 1..33 V1 ; 34..66 V2 ; 67..100 V3
uint8_t pourcent(Moteur m);                 // Arret 0 ; V1 33 ; V2 66 ; V3 100

enum class CibleVentilo : uint8_t { Aucune, Eteint, V1, V2, V3, Derniere };
enum class Issue : uint8_t { Ok, Annulee, Echec, Abandon, Remplacee };
enum class Cause : uint8_t { Aucune, NonConfirme, SansLecture, Collision, Garde, Pilote, Duree };

struct Action {                              // une seule par appel de suivante()
  enum Type : uint8_t { Aucune, Appuyer, Publier, FinSequence } type;
  Touche touche; uint32_t idAppui;           // Appuyer
  Etat etat;                                 // Publier
  uint32_t idOrdre; Issue issue; Cause cause;  // FinSequence (ordres de l'app : même id)
};

struct Params;                               // tableau ci-dessous
bool paramsValides(const Params &p);

class Automate {
 public:
  explicit Automate(const Params &p);
  void demarrer(Demarrage cause, const Etat *etatPublieNvs, Moteur derniereVitesse, uint32_t nowMs);

  // Entrées
  void ordreVentilo(CibleVentilo c, uint32_t idOrdre, uint32_t nowMs);
  void ordreLampe(bool allumee, uint32_t idOrdre, uint32_t nowMs);
  void surEtatLu(const Etat &lu, uint32_t nowMs);                 // fil ou lecture annexe (§5.1)
  void surAppui(Touche t, Origine o, uint32_t nowMs);             // appui vu sur le fil
  void surFinAppui(uint32_t idAppui, ligne::ResultatAppui r, uint32_t nowMs);  // §5.4, règle 2
  void surAnomalie(uint16_t code, uint32_t nowMs);                // trame illisible, débordement, ligne muette
  void surPilote(bool enService, uint32_t nowMs);                 // pilote hors service, puis revenu

  // Sortie
  Action suivante(uint32_t nowMs);
  const Etat &etat() const;
  Moteur derniereVitesse() const;
  Diagnostic diagnostic() const;             // séquence en cours, compteurs (§7.4)
};

// Regroupement des écritures d'EP1, FanMode et PercentSetting ensemble (§5.4, règle 6), pur.
class RegroupementEp1 {
 public:
  void ecrireMode(uint8_t fanMode, uint32_t nowMs);        // 0..4 ; 4 = On
  void ecrirePourcent(uint8_t pourcent, uint32_t nowMs);
  bool pret(uint32_t nowMs, CibleVentilo *cible);          // vrai après le calme ou le plafond
};

}  // namespace hotte
```

**Paramètres de l'automate** (réglables par l'USB seulement,
`hotte regle <nom> <valeur>`, vérifiés avant la NVS, remplacés tous par les
défauts si un jeu lu en NVS est hors bornes, comme `inj::Params` de la sonde) :

| Nom | Défaut | Bornes | Fixé par |
|---|---|---|---|
| `delai_moteur_ms` | 3 000 | 1 000..60 000 | avenant de l'étape 6 (délai minimal entre deux changements d'état du moteur, arrêt ou démarrage, toutes origines) |
| `entre_appuis_ms` | 500 | 100..5 000 | avenant (attente minimale entre deux appuis injectés) |
| `confirmation_ms` | 1 000 | 200..5 000 | avenant, d'après le délai de réponse de la carte (test 2 de l'étape 6) |
| `fraicheur_ms` | 2 000 | 200..120 000 | étape 5, ajout (c) : **en mode « état répété » seulement**, durée sans répétition au-delà de laquelle la ligne est dite muette (§5.1) ; sans objet dans les autres modes |
| `lissage_calme_ms` | 700 | 300..2 000 | regroupement d'EP1 : banc Matter (B1, B2, B9), plus long que l'écart entre deux écritures d'un même geste, dans la limite du budget de C3 (§3.7) |
| `lissage_plafond_ms` | 3 000 | 1 000..10 000 | banc Matter |
| `ordre_calme_ms` | 150 | 50..1 000 | EP2 (`OnOff`) seulement ; repris des 120 ms de benq |
| `sequence_max_ms` | 15 000 | 5 000..60 000 | abandon d'une séquence trop longue |
| `attente_etat_ms` | 5 000 | 1 000..60 000 | attente d'un état utilisable (ou du retour du pilote) avant d'exécuter un ordre (§5.1) |
| `prolongee_max_ms` | 1 200 000 | 900 000..3 600 000 | ajout (a) : durée mesurée de la prolongation, plus une marge (§5.9) |
| `ignore_demarrage_ms` | 2 000 | 0..10 000 | repris de benq : ordres Matter ignorés après le démarrage |

**Paramètres de surveillance** (même mécanisme ; logique pure `surveillance`,
testée sur le Mac ; §7.4) :

| Nom | Défaut | Bornes | Fixé par |
|---|---|---|---|
| `temp_alerte_c` | 70 | 20..85 | Majid, à la relecture (Q34) ; revu après l'essai de 24 h, d'après l'auto-échauffement mesuré |
| `temp_hyst_c` | 5 | 1..10 | idem |
| `alim_hotte_min_mv` | 4 500 | 3 000..6 000 | critère de l'essai de 24 h (§9.4), ajusté à la valeur de la 3b |
| `alim_module_min_mv` | 3 700 | 3 000..5 000 | régulateur de la SuperMini : 3,3 V plus sa chute (§9.4) |
| `alim_hyst_mv` | 100 | 20..500 | — |

### 4.3 Couche 2 : le pilote de ligne

**Rôle.** Traduire l'écoute de `D` (et, s'il le faut, la lecture annexe) en
événements, et un appui demandé en trame (A, B) ou en contact (C). **Il ne
décide rien** : il n'a pas de cible, il n'enchaîne pas les touches. Il porte les
garde-fous électriques et temporels.

**Interface, en C++ indicatif :**

```cpp
namespace ligne {

enum class ResultatAppui : uint8_t {
  Ok,          // émis en entier et relu sur le fil (A, B), ou contact fait (C)
  Collision,   // le fil ne suivait pas l'émission : émission coupée (l'appui a pu prendre)
  Delai,       // pas de silence dans attente_max_ms : rien émis
  Garde,       // refusé par un garde-fou (durées, délai, « marche » interdite, touche interdite)
  Erreur       // pilote hors service
};

struct Evenement {
  enum Type : uint8_t { EtatLu, Appui, FinAppui, Anomalie } type;
  uint32_t tMs;
  hotte::Etat etat;                 // EtatLu : décodé d'une trame de la carte, ou lecture annexe (source)
  hotte::Touche touche;             // Appui : touche vue sur le fil
  hotte::Origine origine;           // Appui : Panneau ou Module (écho de notre propre trame)
  uint32_t idAppui; ResultatAppui resultat;   // FinAppui
  uint16_t codeAnomalie;            // Anomalie : trame illisible, débordement, ligne muette, ligne tenue basse
};

enum class Admission : uint8_t { Acceptee, Occupee, Refusee };

class Pilote {
 public:
  virtual ~Pilote() = default;
  virtual void demarrer(uint32_t nowMs) = 0;            // écoute seulement
  virtual void poll(uint32_t nowMs) = 0;                // tâche loop
  virtual bool evenement(Evenement *out) = 0;           // file, vidée à chaque tour
  virtual Admission appuyer(hotte::Touche t, uint32_t idAppui, uint32_t nowMs) = 0;  // asynchrone
  virtual void diagnostic(Diagnostic *out) const = 0;   // compteurs, dernières trames
};

}  // namespace ligne
```

**Réalisations :**

| Réalisation | Quand | Contenu |
|---|---|---|
| `PiloteSimule` | **maintenant** (tests sur le Mac, banc Matter) | une hotte modèle qui suit la table du §5.2 : latence de la carte réglable, transitions inconnues réglables, trois modes d'état (état répété au repos, état aux changements seulement, appuis seulement), perte de trame réglable, **lecture annexe simulée (moteur, lampe)** activable dans chaque mode (obligatoire dans les deux derniers, §5.7) ; appuis du panneau et coupures simulés par la console (`simu ...`). **L'état de la hotte simulée survit à un redémarrage qui n'est pas une mise sous tension** (mémoire `RTC_NOINIT_ATTR`, avec un mot de contrôle) et revient à « éteinte » à la mise sous tension, comme H-coupure (§5.7). `simu apres_demarrage <état>` (une fois, en NVS) donne l'état que prend la hotte simulée au prochain démarrage, quelle qu'en soit la cause : hotte qui change pendant le redémarrage, ou qui continue de tourner pendant une coupure du module seul (B10, B16). **Aucune broche pilotée** : GPIO1 reste à l'état bas, et le code d'injection n'est pas lié dans ce build |
| `PiloteA` | après l'avenant de l'étape 6 | écoute sur GPIO0 (RMT en réception, comme la sonde), injection en drain ouvert sur GPIO1 pendant un silence, surveillance des fronts et arrêt sur collision (repris de `injection_regles` et `injection` de la sonde) ; décodeur de `PROTOCOL.md` |
| `PiloteB` | idem, si B | deux voies (voie 1 : GPIO0 et GPIO1, côté panneau ; voie 2, côté carte : broches de l'avenant, §6.2), relais des trames, réponses dans le délai de la carte, relais de contournement et chien de garde matériel (§6.3) |
| `PiloteC` | idem, si C | un PhotoMOS par touche, contact de durée bornée ; écoute de `D` sur GPIO0 pour l'état |

**Garde-fous, permanents, dans le firmware** (ils remplacent l'armement de
10 min et la phrase tapée de la sonde, qui n'ont pas de sens dans un produit) :

1. **GPIO1 (et toute sortie d'injection) mise à l'état bas en toute première
   instruction de `setup()`.** Au reset, GPIO1 n'a aucun tirage (fiche du C6,
   tableau 2-2), et le 10 kΩ entre base et émetteur garde Q2 bloqué.
2. **Drain ouvert seulement :** le module ne peut que tirer `D` vers le bas.
3. **Durées bornées :** durée basse maximale par impulsion, durée totale
   maximale par trame, silence minimal avant d'émettre, attente maximale ; les
   valeurs de l'avenant, vérifiées comme dans la sonde (`inj::Params`).
4. **L'émission passe par le RMT** : sa transaction se termine seule, au niveau
   de repos, même si le processeur plante en cours d'émission (NV, Q29).
5. **Chien de garde** : `enableLoopWDT()` **en fin de `setup()`** inscrit la
   tâche `loop` au chien de garde des tâches d'IDF (5 s) ; le cœur Arduino ne
   le fait pas de lui-même (`loopTaskWDTEnabled = false`, `main.cpp`, l. 111),
   et le chien n'est nourri que dans la boucle. **Après une panique, le core
   dump est écrit en flash avant le redémarrage** (actif sur la plateforme) :
   pendant cette écriture, les broches gardent leur état, et seul le RMT
   (garde-fou 4) borne la durée de `D` basse (Q29). Au redémarrage, les broches
   repassent en entrée et le 10 kΩ bloque Q2.
6. **Jamais « marche » moteur tournant** : l'automate ne la planifie pas sans
   un arrêt du moteur lu (§5.4, règle 3), et le pilote la refuse (`Garde`) si
   l'état qu'il a lu le plus récemment dit « moteur en marche », **ou s'il n'a
   lu aucun état depuis son démarrage**. Deux verrous indépendants.
7. **Délai moteur** : un appui qui change le moteur est refusé (`Garde`) avant
   `delai_moteur_ms` depuis le dernier changement du moteur vu sur le fil.
   L'automate attend lui-même ce délai (§5.4, règle 4) : un refus est une erreur
   de l'automate (règle 2).
8. **Proposition, non étudiée :** une limite matérielle sur l'étage d'injection
   (couplage capacitif de la base de Q2, dimensionné pour environ dix fois la
   durée basse maximale de l'avenant). Décidée au plan des points 6 à 9, d'après
   l'avenant (Q29).

### 4.4 Couche 3 : `matter_hotte`

Recettes reprises de benq (`src/matter_bridge.cpp` au commit `c58a506`),
réécrites pour deux endpoints :

| Recette | Ce qu'elle fait ici |
|---|---|
| boîte d'intentions | `MatterFanHotte` (EP1) et le rappel `onChangeOnOff` (EP2), un rappel **par attribut**, jamais `onChange()`, ne font que déposer la **valeur brute** reçue, avec son heure, dans une boîte protégée par un verrou court ; dernière valeur gagnante, champ par champ, chacune avec son heure, qui donne l'ordre d'arrivée des écritures d'EP1 (§5.4, règle 6) ; ils rendent `true`. La tâche `loop` vide la boîte, applique le regroupement et donne l'ordre à l'automate |
| écho propre écarté | nos propres reflets repassent par les rappels, dans la tâche `loop` : ils sont reconnus à la tâche appelante et ignorés |
| reflet vers Matter | `updateAttributeVal` sous `TryLockChipStack` ; si le verrou n'est pas pris, on réessaie au tour suivant ; jamais `set_val()` ni `attribute::report()` (cache de `MatterFan` périmé) ; jamais de reflet tant que la boîte n'est pas vide ; le reflet de fin de séquence écrit toujours `FanMode`, `PercentSetting` et `PercentCurrent`, sans se fier au cache |
| démarrage | rien n'est injecté vers la hotte au démarrage (jamais `updateAccessory()`) ; ordres Matter ignorés pendant `ignore_demarrage_ms` |
| réseau | `Matter.selectNetwork(MATTER_NETWORK_THREAD)` avant le premier `begin()` ; mise en service en BLE ; premier flash avec effacement |
| identité | EP0 posée à chaque démarrage, avant `Matter.begin()` (§8.1) ; MAC lue par `esp_read_mac(ESP_MAC_BASE)` |
| état du réseau | `netPoll` (verrou OpenThread pris sans attente, une fois par seconde au plus) au lieu de `isDeviceConnected()` |
| verrous | jamais le verrou CHIP sous le verrou OpenThread |
| abonnements | plafond de l'intervalle maximal des abonnements neufs à 20 s (`matter maxint`, NVS), repris de benq ; le calendrier de reprise (`ResumePlanner`) seulement si le banc (B10) montre le même trou que sur benq |

**`MatterFanHotte`**, sous-classe de `MatterFan`, surcharge `attributeChangeCB`
(virtuelle dans `MatterEndPoint`). Elle reçoit chaque écriture **avant** le
serveur CHIP, avec sa valeur brute (§3.3). Dans la tâche CHIP :
- elle journalise chaque écriture brute (banc Matter, B2) ;
- `FanMode` et `PercentSetting`, qu'ils viennent du contrôleur ou d'une cascade
  du serveur CHIP : dépôt dans la boîte, mise à jour de `currentFanMode` et
  `currentPercent` (membres protégés) pour que le cache de `MatterFan` suive la
  base, retour `true`, **sans appeler `MatterFan::attributeChangeCB`**, donc
  sans `reportPercentCurrent` de la cible (`MatterFan.cpp`, l. 110 à 118) ;
- `FanMode` = On (4) : dépôt de l'intention « dernière vitesse », puis
  remplacement de `val->val.u8` par le `FanMode` de la dernière vitesse (§3.3,
  règle 3) ;
- `PercentCurrent` venu de la tâche CHIP (cascade du serveur pour Off) : mise à
  jour du cache seulement.

Elle ne délègue à `MatterFan::attributeChangeCB` que les reflets de la tâche
`loop` (écho propre), pour garder son cache juste. Un réglage USB,
`matter derniere 0|1` (1 par défaut), coupe la substitution de On au banc, pour
observer la réaction de Maison à la conversion faite par la pile (B2).

**Piège de la NVS au démarrage.** `FanMode` (EP1) et `OnOff` (EP2) sont
persistants et relus à la création des endpoints ; `PercentSetting` et
`PercentCurrent` ne le sont pas. Après une coupure en V3, Maison afficherait
« High à 0 % ». Parade, à valider au banc (NV, Q10) :
1. après `Matter.begin()`, dès que l'automate a un état initial (§5.7), forcer
   `FanMode` et `OnOff` à la valeur du cache Arduino par `updateAttributeVal` ;
2. puis publier l'état initial (mode, pourcentages, lampe), comme un reflet ;
3. si le cache doit être réaligné, passer par une autre valeur puis par la
   vraie, toujours comme un reflet.

**Publication pendant une séquence.** Pendant une séquence demandée par Maison
ou l'app, les états intermédiaires (par exemple « armée » entre V2 et
« éteinte ») **ne sont pas publiés** : l'état réel l'est à la fin (réussite,
échec, abandon, annulation). Un appui au panneau, lui, est publié tout de suite
(critère C2).

**Réglages en NVS** (espace `produit`), par l'USB seulement :

| Clé | Rôle | Défaut |
|---|---|---|
| `med` | rôle Thread : 0 éligible routeur (le nœud s'attache en enfant, puis devient routeur si la partition en manque), 1 MED dès l'init (enveloppe `_SetThreadDeviceType` de benq) | 0, par `-DMATTER_THREAD_MED=0` quand la NVS est vide (§4.7) |
| `tx_dbm` | puissance d'émission 802.15.4, appliquée au démarrage par `otPlatRadioSetTransmitPower`, sous le verrou OpenThread ; relue par `otPlatRadioGetTransmitPower` | absente : valeur de la pile (NV, Q13) ; posée avant le montage à la valeur retenue à l'essai de 24 h ; bornes 8..20 ; procédure du §6.1 |
| `maxint` | plafond de l'intervalle maximal des abonnements | 20 s |
| `derniere_v` | dernière vitesse (1..3) | 2 ; écrite quand le palier change et diffère, 10 s après le dernier changement |
| `etat_pub` | dernier état publié (marche, moteur, lampe) | écrit 10 s après un changement ; relu après un redémarrage qui n'est pas une mise sous tension (§5.7) |
| `temp_max` | maximum de la température de la puce depuis la pose | écrit au plus une fois par minute, et seulement s'il dépasse l'ancien d'au moins 1 °C ; remis à zéro par l'USB à la pose |
| `params` | paramètres de l'automate et de la surveillance | §4.2 |
| `cle` | clé H1 du canal compagnon | §7.7 |
| `essai` | drapeau de l'essai de 24 h : autorise `radio rafale` à distance (§7.5) | absent ; posé et effacé par l'USB, effacé seul au bout de 72 h |
| `simu_dem` | build simulé : état de la hotte simulée au prochain démarrage (une fois) | absent |

**Écritures différées** (`derniere_v`, `etat_pub`) : 10 s après le dernier
changement. **`reboot` et `decommission` écrivent d'abord ce qui attend**, comme
le `reboot` de benq (`lamp.persistNow()`). Quelques écritures par jour :
l'usure de la flash est négligeable.

### 4.5 Couche 4 : le canal compagnon

Détaillée au §7. Les ordres de l'app passent par **le même automate** que ceux
de Maison, et Matter reflète le résultat. Aucune commande d'injection brute
n'existe dans le build `produit`.

### 4.6 Couche 5 : l'identité

Détaillée au §8 : EP0, codes d'appairage, étiquette, remise à zéro.

### 4.7 Organisation du dépôt et environnements

**Environnements PlatformIO** (même plateforme que `sonde` et `generateur`) :

| Environnement | Contenu | Quand |
|---|---|---|
| `produit` | firmware du module : `board_build.partitions = huge_app.csv` ; les **quatre lignes du rôle Thread, qui vont ensemble** (ci-dessous) ; `-DPILOTE_LIGNE=SIMULE` | **maintenant** : la seule réalisation disponible est la simulée |
| `produit` (suite) | `-DPILOTE_LIGNE=A`, `B` ou `C` | au plan des points 6 à 9 |
| `produit_simule` | `extends = env:produit`, force `PILOTE_LIGNE=SIMULE` | créé quand `produit` passe au pilote réel : il reste l'environnement du banc Matter |

Les quatre lignes du rôle Thread. Chez benq, les deux dernières vont ensemble
(`platformio.ini`, l. 150 à 152 : « l'une sans l'autre ne lie pas »), et le
défaut quand la NVS est vide vient de `-DMATTER_THREAD_MED`
(`matter_bridge.cpp`, l. 598-599), que l'environnement `esp32c6thread` met à 1,
soit MED (l. 147). Copié tel quel, le produit démarrerait en MED, contre la
décision « routeur par défaut ». D'où :

```
    -DMATTER_NET_THREAD=1
    -DMATTER_THREAD_MED=0
    -DHALO_WRAP_THREAD_DEVTYPE=1      ; ou son nouveau nom
    -Wl,--wrap=_ZN4chip11DeviceLayer8Internal40GenericThreadStackManagerImpl_OpenThreadINS0_22ThreadStackManagerImplEE20_SetThreadDeviceTypeENS0_19ConnectivityManager16ThreadDeviceTypeE
```

- **Sources :** `src/produit/` pour tout ce qui est propre au produit. Les
  modules purs partagés restent où ils sont (`h1_proto`, `h1_crypto`,
  `fw_version.h`, `app_desc.c`). Le filtre de `sonde`
  (`+<*> -<gen_main.cpp>`) gagne `-<produit/>` ; celui de `produit` part de
  `-<*>` et ajoute ce qu'il prend.
- **`json_out` n'est pas un module pur partagé** : il porte le profil de la
  sonde. Son énumération `Item` ne connaît que les blocs de la sonde
  (`json_out.h`, l. 250 à 252), son en-tête inclut `capture_model.h` (l. 23),
  et `remoteRefusal` autorise à distance `capture`, `seuils`,
  `injection on|off` et `injecte` (`json_out.cpp`, l. 345 à 351). Réutilisé tel
  quel, il donnerait au produit la liste blanche de la sonde : injection brute
  « autorisée », `hotte ventilo` et `reboot` refusés. **Le produit a donc sa
  copie `json_out_produit.{h,cpp}`** : les briques pures reprises (`Writer`,
  `parseIdPrefix`, `Queue`, `RateCap`, `ReplyCache`…) et les parties propres au
  produit (`Item`, `remoteRefusal`, messages), sans `capture_model.h`. La sonde
  n'est pas touchée ; la séparation des briques communes revient au
  sous-projet 3 (décision : copie au besoin, extraction au sous-projet 3).
- **Fichiers prévus :** `hotte_etat.{h,cpp}` (automate, table,
  `RegroupementEp1`), `hotte_map.{h,cpp}` (correspondance Matter, boîte
  d'intentions pure), `pilote_ligne.h`, `pilote_simule.{h,cpp}` (modèle pur de
  la hotte, lecture annexe simulée, plus son adaptateur),
  `matter_hotte.{h,cpp}`, `alim.{h,cpp}` (ADC des deux voies, température de la
  puce), `surveillance.{h,cpp}` (seuils, hystérésis, maxima : pur),
  `net_udp_thread.{h,cpp}` (copié de `benq/src/net_udp.*` au commit `c58a506`),
  `json_out_produit.{h,cpp}`, `json_mode_produit.{h,cpp}`, `cli_produit.cpp`,
  `config_produit.h`, `main_produit.cpp`. Plus tard :
  `pilote_a|b|c.{h,cpp}`.
- **Copies :** chaque fichier copié de benq ou de la sonde porte en en-tête son
  origine et le commit. Le sous-projet 3 les remettra en commun.
- **Tests sur le Mac :** `tools/tests/test_hote.sh` gagne `test_hotte_etat.cpp`,
  `test_hotte_map.cpp`, `test_pilote_simule.cpp`, `test_surveillance.cpp`,
  `test_json_produit.cpp` (`clang++ -std=c++17 -Wall -Wextra -Werror`, comme
  l'existant) ; `tools/json_check.py` gagne le profil produit.
- **Outils du Mac :** `tools/hotte_udp.py` est aujourd'hui figé sur la sonde
  (clé `~/.config/hotte-sonde/cle` en dur, l. 66 ; aide « hotte-sonde.local ou
  IPv4 »). Il choisit désormais la clé d'après `build` de `hello` (`sonde` :
  `~/.config/hotte-sonde/cle` ; `produit` : `~/.config/hotte-produit/cle`), ou
  d'après `--appareil produit` ; il accepte un nom SRP (`<nom>.local`) ou une
  adresse IPv6 (il passe déjà par `getaddrinfo`, IPv4 ou IPv6) ; il résume le
  profil produit (`etat.hotte`, `thermique`, `alim`, événements `sequence`,
  `alerte`, `trame_d`). **Prérequis du Mac** pour toute session par Thread
  (banc Matter, mesure directe de M1, essai de 24 h, validation) : l'assistant
  de route de benq, `tools/macos/halo-routes` au commit `c58a506`, qui garde la
  route Thread du Mac.
- **Taille :** l'image du produit est mesurée au premier build. Attendu : sous
  les 3 145 728 octets de `huge_app`, comme benq (NV, Q22).

### 4.8 Principes de robustesse

- **Un seul producteur** de lignes machine et d'appuis : la tâche `loop`. Les
  rappels (CHIP, OpenThread, RMT, GPIO) ne font que déposer dans des files.
- **Le module n'attend jamais personne** : ni l'app, ni la pile Matter, ni un
  verrou. Une ligne qui ne trouve pas de place est perdue et comptée.
- **Core dump en flash** (partition `coredump` de `huge_app`), lu par l'USB
  après un plantage, hotte débranchée ; `hello` publie la cause du dernier
  redémarrage et leur nombre.
- **Traces CHIP** filtrées par le crochet `esp_log_set_vprintf` de benq.
- **Bouton BOOT (IO9) : aucune action dans le build `produit`.** Chez benq, un
  appui de 8 s retire Matter (`boot_button.h`) : copié, il contredirait
  « remise à zéro par l'USB seulement » (§8.5).

---

## 5. Comportement

### 5.1 État, source et confiance

L'automate tient un état (§4.2) et **sa confiance** :

| Confiance | Quand | Planifier une séquence ? |
|---|---|---|
| `Confirme` | le dernier événement est un état complet lu (fil ou lecture annexe), sans appui ni anomalie depuis ; en mode « état répété », il a en outre moins de `fraicheur_ms` | oui |
| `Deduit` | un état lu, suivi d'appuis vus sur le fil dont la transition est **observée** (§5.2) ; ou l'état initial après une mise sous tension (§5.7) | oui ; « marche » attend toujours un arrêt du moteur **lu** (§5.4, règle 3) |
| `Presume` | après un redémarrage du module qui n'est pas une mise sous tension (dernier état publié, relu en NVS) ; après une transition **inconnue** ; après une **anomalie de réception** : trame illisible, débordement, ligne muette en mode « état répété » (aucune répétition depuis `fraicheur_ms`) ; « prolongée » sans lecture depuis `prolongee_max_ms` (§5.9) | non : seulement la sonde de vitesse (§5.4, règle 7) |
| `Inconnu` | aucune information : redémarrage qui n'est pas une mise sous tension, sans état publié en NVS | non |

- **Un état ne vieillit pas vers `Inconnu`.** Le fil est écouté en continu :
  sans appui ni anomalie, le dernier état lu reste juste, et son âge est publié
  (`age_ms`). `fraicheur_ms` ne sert qu'au mode « état répété », pour repérer
  une ligne muette ; il est sans objet dans les autres modes.
- **Publication :** toujours la meilleure estimation disponible (`Confirme`,
  `Deduit` ou `Presume`). En `Inconnu`, rien de nouveau n'est publié.
- **Un ordre reçu en `Presume` ou `Inconnu`** est gardé comme cible. Il
  s'exécute dès que l'état devient `Confirme` ou `Deduit`. Faute de quoi, au
  bout de `attente_etat_ms`, il est abandonné (`Abandon`) et l'état est
  republié.
- **La lampe a sa propre connaissance** (`lampeConnue`). Un ordre de lampe
  attend, de la même façon, que l'état de la lampe soit connu : une bascule
  appuyée à l'aveugle ferait l'inverse une fois sur deux.
- **Exigence qui en découle :** le critère des voyants demande une **source
  d'état réelle**. Ou bien la carte émet son état sur `D` (A, B), ou bien le
  panneau l'émet (C, type Krona), ou bien la lecture annexe complète le fil
  (§6.4). L'étape 6 et l'ajout (c) le disent (Q1).

### 5.2 Table des transitions utilisée par l'automate

C'est la table du §2.2, codée dans `appliquer()`. Chaque ligne porte le drapeau
`observee`. Une transition inconnue fait passer la confiance à `Presume`,
jusqu'au prochain état lu. **Quand les captures de l'ajout (a) auront montré
les transitions de la marche prolongée, la table est complétée** par un avenant
à cette spec, et les tests suivent.

**Lignes « supposé identique en V1 et V2 » (Q30).** Jusqu'à la réponse, elles
sont codées `observee` = vrai : un appui du panneau sur la vitesse active, ou
sur « marche » moteur tournant, en V1 ou V2, garde la confiance `Deduit` au lieu
de bloquer les ordres. Le risque est borné : « marche » exige un arrêt du
moteur **lu**, jamais déduit (§5.4, règle 3), et le premier état lu corrige une
déduction fausse. Si Q30 infirme, la ligne change par avenant.

### 5.3 Suites d'appuis

« ⇒ » : attendre la confirmation de l'état indiqué (§5.4, règle 1) avant l'appui
suivant. **Tout appui qui change le moteur** (arrêt ou démarrage) **attend en
plus `delai_moteur_ms` depuis le dernier changement du moteur, toutes origines**
(règle 4) : les tableaux ne le répètent pas. **« Marche » attend toujours un
arrêt du moteur lu** (règle 3).

**Vers « éteindre »**

| État lu | Suite |
|---|---|
| éteinte | rien |
| armée | marche |
| Vx | Vx ⇒ armée (arrêt du moteur lu) ⇒ marche |
| prolongée-Vx | Vx ⇒ armée (arrêt du moteur lu) ⇒ marche |
| marche inconnue, moteur arrêté lu | rien sur ⏻ ; événement `marche_inconnue` journalisé (C1 reste tenu : moteur et lampe) |

**Vers Vy**

| État lu | Suite |
|---|---|
| éteinte | marche ⇒ armée ⇒ Vy (si l'état « éteinte » est seulement déduit, par exemple après une mise sous tension, « marche » attend d'abord un arrêt du moteur lu) |
| armée | Vy |
| Vx (x ≠ y) | Vy |
| Vy | rien |
| prolongée-Vy | rien : le moteur est déjà en Vy, la prolongation continue (choix proposé, Q38 ; §5.5) |
| prolongée-Vx (x ≠ y) | Vx ⇒ armée ⇒ Vy. Remplacée par « Vy » seul si l'ajout (a) montre que Vy agit directement et proprement |
| marche inconnue, moteur arrêté lu | sonde de vitesse (§5.4, règle 7) |

**Vers la lampe :** un appui sur lumière si l'état lu diffère de la cible ;
rien sinon. La lampe n'a pas de délai moteur.

**Entrelacement.** Un seul appui est en vol à la fois. Si une séquence du
ventilateur attend son délai moteur, un appui de lampe peut passer entre-temps.

### 5.4 Règles d'exécution

1. **Un appui à la fois**, puis confirmation : un état lu postérieur à la fin
   de l'appui, égal au résultat attendu de la table, dans `confirmation_ms`. Ce
   qui confirme dépend du mode (tableau ci-dessous).
2. **Un nouvel essai, un seul, par étape, seulement si l'on sait que l'appui
   n'a pas pris** (tableau ci-dessous). Une touche bascule appuyée deux fois
   ferait l'inverse : dans le doute, on ne réessaie pas, la séquence s'arrête
   (`Echec`) et l'état réel est publié. Selon le résultat du pilote :
   - `Ok` : on attend la confirmation ;
   - `Delai` (rien émis) : nouvel essai immédiat, dans la limite d'un ;
   - `Collision` : l'appui a pu prendre (la trame a pu passer presque entière),
     ou l'autre émetteur était le panneau, prioritaire. On attend
     `confirmation_ms`, puis on juge sur l'état lu depuis l'appui : état de
     départ inchangé, lu après ce délai, nouvel essai ; état attendu, étape
     réussie ; autre état, c'est un appui du panneau, séquence `Annulee` ; aucun
     état lu, `Echec` (cause `collision`) ;
   - `Garde` : l'automate a demandé un appui que le pilote refuse, c'est une
     erreur de l'automate. Séquence `Echec` (cause `garde`), compteur, état
     republié ;
   - `Erreur` : pilote hors service. Séquence `Echec` (cause `pilote`) ; les
     ordres suivants attendent le retour du pilote, au plus `attente_etat_ms`,
     puis `Abandon`.
3. **Jamais « marche » moteur tournant.** « Marche » n'est appuyée qu'après
   **un arrêt du moteur confirmé par une lecture (fil ou lecture annexe)
   postérieure au démarrage du module et au dernier appui**, quelle que soit la
   cible : éteindre depuis Vx ou prolongée-Vx (Vx ⇒ arrêt lu ⇒ marche), allumer
   depuis « éteinte », y compris depuis l'état initial d'une mise sous tension,
   qui est déduit et non lu (§5.7). C'est la décision (b) : seul l'arrêt du
   moteur est exigé, pas l'état « armée » (la lecture annexe ne lit pas ⏻). Un
   Vx non confirmé arrête la séquence.
4. **Délai moteur** : `delai_moteur_ms` entre deux changements d'état du moteur,
   **quelle que soit leur origine** (panneau, Maison, app). L'automate diffère
   l'appui ; il ne le fait jamais refuser par le pilote.
5. **Entre deux appuis injectés :** au moins `entre_appuis_ms`.
6. **Regroupement d'EP1.** `FanMode` et `PercentSetting` alimentent **une
   seule** fenêtre, fermée après `lissage_calme_ms` sans nouvelle écriture, ou
   au plus tard `lissage_plafond_ms` après la première. À la fermeture, la cible
   vient de la **dernière écriture** de la fenêtre, dans l'ordre d'arrivée,
   avec une exception : un `FanMode` Low, Medium, High ou On cède devant un
   `PercentSetting` > 0 de la même fenêtre, dont on prend la dernière valeur.
   Ainsi : Off ou 0 en dernier (écrits par Maison ou en cascade par le serveur
   CHIP) donnent « éteindre » ; On seul, « dernière vitesse » ; On et 40 %, V2 ;
   40 % puis Off, « éteindre » ; 0 puis 50 %, V2 ; Low, Medium ou High seul, V1,
   V2 ou V3. EP2 (`OnOff`) a sa propre fenêtre, `ordre_calme_ms`.
7. **Sonde de vitesse** (état de ⏻ inconnu, moteur arrêté lu, cible Vy) :
   appuyer Vy. Si Vy est confirmé, fini. Si rien ne change dans
   `confirmation_ms`, la hotte était éteinte (une vitesse sans marche ne fait
   rien, étape −1) : marche ⇒ armée ⇒ Vy. **Permise seulement si le scénario
   6b de l'étape 5 montre que la carte reçoit une vitesse appuyée sans marche et
   l'ignore** (Q6). Sinon, l'ordre attend un état `Confirme` ou `Deduit`.
8. **Durée maximale :** une séquence qui dépasse `sequence_max_ms` est
   abandonnée (`Abandon`) ; l'état réel est publié.

**Ce qui confirme un appui, et ce qui prouve qu'il n'a pas pris, selon le mode
d'état (§5.7, Q1) :**

| Mode | Confirmation | L'appui n'a pas pris (nouvel essai permis) |
|---|---|---|
| état répété | l'état répété après l'appui, égal au résultat attendu | l'état répété reçu au-delà de `confirmation_ms` après l'appui (délai de réponse de la carte), encore égal à l'état de départ, sans anomalie de réception |
| état aux changements (lecture annexe obligatoire) | la trame d'état qui suit l'appui, égale au résultat attendu ; pour le moteur et la lampe, la lecture annexe aussi | l'écho de notre trame vu sur le fil, puis aucune trame d'état ni anomalie de réception dans `confirmation_ms` ; ou la lecture annexe inchangée (moteur, lampe) |
| appuis seuls (lecture annexe obligatoire) | moteur et lampe : la lecture annexe après l'appui ; ⏻ : l'écho de notre trame et la table (confiance `Deduit` : la lecture annexe ne lit pas ⏻) | la lecture annexe inchangée après `confirmation_ms` (moteur, lampe) ; pour « marche », jamais : pas de nouvel essai |

### 5.5 Idempotence

- On n'appuie **que si l'état lu diffère de la cible** : moteur pour le
  ventilateur, lampe pour la lampe.
- Un ordre égal à l'état lu produit une publication de l'état (Maison se
  réaligne), aucun appui.
- Un ordre répété pendant sa propre séquence ne relance rien : la cible est la
  même.
- **Pendant la marche prolongée**, un ordre égal à la vitesse en cours (V2
  pendant prolongée-V2, ou « allumer » quand la dernière vitesse est V2) ne
  fait rien : la prolongation continue, et la hotte s'arrêtera seule à la fin,
  alors que Maison vient de demander « allumé ». C'est le comportement proposé.
  L'autre choix, annuler la prolongation (Vx ⇒ armée ⇒ Vx), est à trancher par
  Majid (Q38) ; le cas retenu est testé.

### 5.6 Conflits

| Situation | Règle |
|---|---|
| **appui au panneau pendant une séquence** | la séquence est **annulée** (`Annulee`) : le panneau est prioritaire (CEI 60335-1, 22.50). L'appui en vol, s'il y en a un, finit (on n'interrompt pas une trame), puis plus rien. L'état réel est publié tout de suite |
| reconnaître un appui du panneau | le pilote marque l'origine : `Module` pour l'écho de nos trames (il sait quand il émet), `Panneau` sinon. Un changement d'état lu qui n'est pas le résultat attendu de notre appui compte comme un appui du panneau (y compris après une collision, §5.4, règle 2) |
| nouvel ordre de Maison pendant une séquence | la cible est **remplacée** (`Remplacee` pour l'ancien ordre) ; la séquence repart de l'état lu vers la nouvelle cible ; l'appui en vol finit d'abord |
| ordres simultanés sur EP1 et EP2 (tuile regroupée, scène) | traités comme deux ordres indépendants, entrelacés (§5.3) |
| ordre de l'app et ordre de Maison | même traitement : le dernier gagne |
| ordre pendant `ignore_demarrage_ms` | ignoré ; l'état réel est republié à la fin de la fenêtre |

### 5.7 Démarrage et redémarrage du module

**Ordre de `setup()` :**
1. **GPIO1 à l'état bas** (et toute sortie d'injection), première instruction.
   Avec B : le relais de contournement reste au repos (fermé : `D` rétabli)
   jusqu'à ce que le pilote soit prêt.
2. Console USB, lecture de la NVS (paramètres bornés).
3. Pilote : `demarrer()`, écoute seulement.
4. Identité EP0, codes d'appairage, `selectNetwork(THREAD)`, EP1, EP2,
   `Matter.begin()`.
5. **État initial**, d'après `esp_reset_reason()` :
   - **mise sous tension** : éteinte, lampe éteinte, confiance `Deduit`. Le
     module est alimenté par le `+` de `CN3` : s'il démarre à froid, la hotte
     vient elle aussi d'être remise sous tension, et elle revient toujours
     éteinte (étape −1, ligne 9). C'est l'hypothèse H-coupure (NV, Q20). Aucun
     état n'est encore lu : « marche » attend un arrêt du moteur lu (§5.4,
     règle 3), ce qui protège aussi le cas où H-coupure est fausse (module seul
     redémarré par un creux du rail, un faux contact ou un fusible qui se
     réarme, hotte en marche) ;
   - **toute autre cause** (panique, chien de garde, `reboot`, baisse de
     tension) : dernier état publié, relu en NVS (`etat_pub`), confiance
     `Presume`. La hotte a pu continuer à tourner.
6. Parade du piège de la NVS, puis publication de l'état initial (§4.4).
7. Ordres Matter ignorés pendant `ignore_demarrage_ms`.
8. **Aucun appui n'est injecté au démarrage.** Le premier état lu sur le fil
   (ou par la lecture annexe) remplace l'état initial.
9. `enableLoopWDT()`, en dernier (§4.3, garde-fou 5). Le bouton BOOT n'est pas
   surveillé (§4.8).

**Resynchronisation, hotte en marche.** Elle dépend de ce que porte `D` (Q1) :

| Ce que montre l'étape 5 et l'ajout (c) | Resynchronisation |
|---|---|
| la carte répète son état au repos | au premier état reçu : `Confirme`. Désaccord borné par la période de répétition |
| la carte n'émet son état qu'aux changements | `Presume` jusqu'au prochain changement : le critère C1 peut tomber au-delà de la fenêtre tolérée. **Lecture annexe obligatoire** (§6.4) |
| `D` ne porte que les appuis | idem : **lecture annexe obligatoire** |

### 5.8 Coupure de courant

- La hotte revient éteinte, lampe éteinte, avec un bip (étape −1) ; le module
  démarre à froid (§5.7, point 5).
- Maison voit le nœud « Pas de réponse » pendant la coupure, puis se réabonne
  (75 s au plus, Q36). La parade du piège de la NVS garantit « éteint, 0 % » au
  retour (B4).
- **Coupure brève** (quelques centaines de ms) : le module peut redémarrer
  alors que IC2 garde son état. Si c'est le cas, H-coupure tombe ; la
  validation le teste (V5). Si elle tombe, la mise sous tension est traitée
  comme « autre cause » (`Presume`) et la resynchronisation suit le tableau du
  §5.7. En attendant, la règle 3 du §5.4 empêche le pire : aucun « marche »
  avant un arrêt lu.

### 5.9 Marche prolongée lancée au panneau

- Maison montre le ventilateur allumé au palier pendant les 15 min.
- **La fin se lit, elle ne se devine pas.** Le module voit la fin à la trame
  qu'elle laisse sur `D` (test 6 de l'étape 6), à la répétition suivante de
  l'état (test 5), ou, si `D` ne dit rien, par la lecture annexe, qui voit le
  moteur s'arrêter. Or la lecture annexe est déjà obligatoire dès que la carte
  ne répète pas son état (§5.7). **Aucune minuterie de 15 min dans le
  module** : elle publierait un état deviné. L'ajout (a) à la reconnaissance
  est corrigé dans ce sens (test 6 : Q2).
- **Garde-fou :** un état « prolongée » sans lecture depuis `prolongee_max_ms`
  (20 min par défaut : 15 min et une marge ; fixé d'après l'ajout a) passe en
  `Presume`. L'état publié ne change pas ; un événement est journalisé ; et
  aucune séquence n'est planifiée sur un état périmé (si la fin laisse la hotte
  « armée », appuyer Vx pour l'« arrêter » la redémarrerait).
- À la fin, le nœud publie ce qu'on a lu : probablement « éteinte » (Q2) ; avec
  la lecture annexe seule, moteur arrêté et ⏻ inconnu, ce qui suffit à C1.
- Pendant la prolongation, un ordre de Maison suit le §5.3 : « éteindre » passe
  par Vx ⇒ armée ⇒ marche ; un autre palier, par Vx ⇒ armée ⇒ Vy. Un ordre égal
  à la vitesse en cours : §5.5 (Q38).

---

## 6. Matériel

### 6.1 Carte et condition radio

**Carte : ESP32-C6 SuperMini violette** (antenne céramique, flash de 4 Mo ; une
SuperMini libre est réservée au module), **sous condition** de la mesure radio
comparative M1, faite hotte débranchée (§9.3) :

| Marge Thread `M` à l'emplacement | Décision |
|---|---|
| plus de 20 dB | SuperMini gardée ; rôle éligible routeur |
| de 10 à 20 dB | SuperMini gardée, rôle **MED** (`med 1`), **essai de 48 h** module en place (critères ci-dessous, et « Pas de réponse » relevé) |
| moins de 10 dB | carte à **antenne déportée** (par exemple XIAO ESP32-C6, connecteur u.FL, flash de 4 Mo à vérifier à l'achat) ; antenne sur une partie non métallique de la hotte, à au moins 10 à 15 mm du métal, **tenue par deux fixations indépendantes, jamais la colle seule** ; coaxial fixé au câble du panneau, jamais contre les fils du moteur, passé par une ouverture existante (aucun perçage) ; antenne et coaxial aux distances du §6.7 de toute pièce au 230 V |

- **La marge se calcule sur la sensibilité de la fiche**, comme dans M1 :
  `M` = RSSI du lien − atténuation + 104 dB (−104 dBm : fiche du C6, 1 % de
  trames perdues). **La qualité de lien d'OpenThread n'est pas une marge :**
  le portage d'ESP-IDF rend une sensibilité fixe de −120 dBm
  (`ESP_RECEIVE_SENSITIVITY`, `esp_openthread_radio.c`, lu dans IDF 5.5.3),
  qu'OpenThread prend comme plancher de bruit (`SubMac::GetNoiseFloor`). La
  qualité 3 (plus de 20 dB au-dessus de ce plancher) est donc atteinte dès
  −100 dBm, à 4 dB du seuil de réception. À relire au banc (B12, Q14).
- **« Routeur » veut dire éligible routeur :** le nœud s'attache d'abord en
  enfant, puis devient routeur si la partition compte moins de 16 routeurs, ce
  qui est le cas d'une maison ordinaire. Un routeur n'a pas de parent :
  `otThreadGetParentInfo` rend une erreur (`Mle::GetParentInfo` :
  `VerifyOrExit(IsChild())`), et le RSSI moyen du parent ne lit au mieux que
  l'ancien. Les mesures « avec le parent » ne valent donc qu'en MED ; en
  routeur, on lit le routeur voisin de meilleur lien (§7.4).
- Le boîtier électronique est en plastique : ce qui blinde, c'est la carcasse en
  acier et le filtre métallique. Espressif demande 15 mm de dégagement autour de
  l'antenne ; benq, aucun métal à moins de 10 mm. Les fils du module comptent :
  c'est pourquoi l'écoute et l'injection quittent GPIO6 et GPIO7 (§6.2).
- Si la carte change, le brochage du §6.2 est repris dans un avenant.

**Puissance d'émission : une seule procédure.** La décision de Majid la
règle : l'émission n'est réduite que si la marge d'alimentation est juste.
1. **Banc Matter**, sur l'USB : valeur de la pile (20 dBm attendus, NV, Q13).
2. **Essai de 24 h** (§9.4) : les 2 premières heures à 20 dBm, pire cas pour
   l'alimentation. Critères du côté hotte tenus : l'essai continue à 20 dBm, et
   c'est la puissance du produit. Critères du côté hotte non tenus (la marge est
   juste) : émission réduite, 12 dBm pour la suite de l'essai ; si 12 dBm ne
   tient pas, nouvel essai à 8 dBm ; au-delà, repli 12 V (Q24). Le côté module
   décide du fusible, pas de la puissance (§6.5).
3. **Installation**, à la puissance retenue : essai du lien de 48 h module en
   place, dans le rôle retenu (c'est aussi l'essai de 48 h de la branche
   10-20 dB). Critères, lus avec le parent (MED) ou avec le routeur voisin de
   meilleur lien (routeur) :
   - sens descendant : RSSI entrant moyen d'au moins −84 dBm en routeur,
     −94 dBm en MED (20 et 10 dB au-dessus de −104 dBm) ;
   - sens montant : nouveaux essais MAC (`mTxRetry`) au plus 10 % des émissions
     et échecs MAC (`mTxDirectMaxRetryExpiry`) au plus 1 %, aucun changement de
     parent, aucun « Pas de réponse » (seuils proposés, revus au banc, B12).
   Lien insuffisant : on ne monte pas au-delà de ce que l'essai de 24 h a
   tenu ; on passe en MED, puis à l'antenne déportée (tableau ci-dessus),
   décision de Majid faits en main.
4. Tout changement ultérieur passe par `matter tx`. Par l'USB, il faut ouvrir
   la hotte ; à distance, il faudrait l'autoriser, borné à 8..20 (Q39).

Pointe de courant d'émission : 187 mA à 12 dBm contre 305 mA à 20 dBm (fiche de
la puce ESP32-C6, v1.5, tableau 5-9 : la SuperMini porte la puce nue, pas le
module MINI-1).

### 6.2 Brochage

SuperMini violette, 10 + 10 pastilles, l'USB-C à un bout, l'antenne céramique à
l'autre : rangée haute, depuis l'USB, `5V GND 3V3 20 19 18 15 14 9 8` ; rangée
basse `TX RX 0 1 2 3 4 5 6 7`. **GPIO6, GPIO7, GPIO8 et GPIO9 sont les
pastilles les plus proches de l'antenne** : 3 à 4 mm pour GPIO6 et GPIO7 sur la
photo `supermini-zoom.jpg`, pour une carte d'environ 25,5 mm. Un fil soudé là
est du métal à moins de 10 mm de l'antenne, contre la règle du §6.6. D'où, pour
le produit, l'écoute et l'injection **côté USB**, sur des broches sans tirage au
reset.

| GPIO | Usage | Architecture |
|---|---|---|
| 0 | écoute de `D` (voie 1) : RMT en réception et interruption sur les deux fronts ; environ 17 mm de l'antenne (estimé sur la photo) | A, B, C |
| 1 | injection (voie 1), à l'état bas dès la première instruction ; aucun tirage au reset (fiche du C6, tableau 2-2) ; environ 15 mm de l'antenne (idem) | A, B |
| 2 | ADC1, canal 2 : tension du `+` de `CN3`, point milieu du pont dont la résistance haute est sur la traversée (§6.5) | toutes |
| 3 | ADC1, canal 3 : tension côté module, au 470 µF, après diode et fusible (§6.5) | toutes |
| 14, 18, 19, 20 | réserve : voie 2 de B, PhotoMOS de C, relais de contournement et chien de garde de B, lecture annexe (§6.4) ; attribution dans l'avenant de l'étape 6. GPIO18 à 20 ont un tirage haut pendant le reset (tableau 2-2) : écoute seulement, jamais une commande qui doit rester au repos | selon l'avenant |
| 6, 7 | **évitées** pour le produit : voisines de l'antenne. GPIO6 a en plus un tirage haut d'environ 45 kΩ après le reset : branchée sur une injection, elle tirerait `D` à l'état bas | — |
| 4, 5, 8, 9, 15 | **à éviter** : broches de démarrage du C6 (fiche technique : GPIO4, 5, 8, 9, 15 ; 8 et 9 sont aussi voisines de l'antenne). La LED RGB d'IO8, si la carte en porte une, est mise au noir au démarrage, puis n'est plus pilotée. GPIO2 n'est pas une broche de démarrage d'après la fiche du C6 (le brief et benq la classaient ainsi) | — |
| 12, 13 | USB | — |
| 16, 17 (`TX`, `RX`) | UART0, journal de la ROM au démarrage | — |

- GPIO0 et GPIO1 sont libres : l'horloge lente de la plateforme vient de
  l'oscillateur RC interne, pas d'un quartz de 32 kHz (§2.4).
- **L'injection n'est jamais branchée sur une broche tirée vers le haut au
  reset** (GPIO6, GPIO9, GPIO18 à 20).
- La sonde de la reconnaissance garde GPIO6 et GPIO7 : elle est dehors, dans le
  boîtier de mesure. **Le repérage du §9 de WIRING.md est faux pour cette
  carte** (il décrit la carte de la ScreenBar) : il est corrigé au point 1 du
  §10.1 (document des ajouts, bloc F1), maintenant, car la sonde en a besoin dès
  les bancs 16, 21 et 24.

### 6.3 Raccordement : la traversée XH

```
   BOÎTIER ÉLECTRONIQUE DE LA HOTTE, quart haut-droit (très basse tension)

   CN3 [- D +] ◄── fiche XH 3 br. ─┐
                                   │  TRAVERSÉE (petite plaque gainée, deux fixations)
   fiche du panneau ──► embase XH ─┤   − ── − ; D ── D ; + ── +       (A : D traverse sans coupure)
                        3 br.      │   + ──►|── [PTC] ──── +protégé    Schottky puis fusible réarmable
                                   │   + ── 100 k ──────── mesure      résistance haute du pont, au nœud
                                   │                                   + de CN3, avant la diode
                                   └─ fiche XH vers le module (A) : −, D, +protégé, mesure
                                      (4 br. ; 5 à 7 pour B)

   MODULE (boîtier imprimé)
     +protégé ── 470 µF ── 5V (SuperMini) ; pont 100 k / 100 k ──► GPIO3 (tension côté module)
     − ── GND
     mesure ── 100 k vers GND, 1 à 10 nF ──► GPIO2 (tension du + de CN3)
     D ── étage d'écoute ──► GPIO0 ; GPIO1 ──► étage d'injection ── D (A)
```

- **Rien de soudé sur la carte de la hotte.** On ne fait qu'enficher : la fiche
  de la traversée sur `CN3`, la fiche du panneau sur l'embase de la traversée.
  **Réversible :** on retire la traversée, on rebranche la fiche du panneau sur
  `CN3`, et la hotte est d'origine.
- **La diode, le fusible réarmable et la résistance haute du pont sont sur la
  traversée**, au plus près de `CN3` : un court-circuit sur le câble vers le
  module est protégé lui aussi. **Aucun conducteur ne sort de la traversée sans
  limitation** (§6.7, règle 9) : le `+` passe par le fusible ; la mesure, par
  100 kΩ, et un court-circuit de ce fil ne laisse passer que 50 µA.
- **Fixation de la traversée.** La fiche sur `CN3` **ne compte pas** comme
  fixation. La traversée est tenue par **deux fixations indépendantes**, par
  exemple deux colliers sur le câble du panneau, de part et d'autre de la
  plaque. **La longueur libre de chaque fil après sa dernière fixation est plus
  courte que la distance à la plus proche pièce au 230 V.** Sans cela, une
  fiche qui quitte `CN3` déconnecte la plaque du `−`, donc de la terre ; si elle
  touchait alors une pièce au 230 V, le panneau, accessible, et le module
  passeraient au secteur (CEI 60335-1, 22.31). Contrôle à la pose (§6.7,
  point 8a).
- **Fusible :** corps (enrobage isolant) à l'air, sans thermorétractable serrée
  dessus, pattes gainées seulement, à 5 mm au moins des fiches XH et des
  isolants. Déclenché, il dissipe 0,45 W (RXEF025) ou 0,77 W (RXEF050) tant que
  la hotte reste branchée (§6.5, §6.8).
- **Connectique :** JST XH sertie (fiches et embases précâblées), aucune broche
  Dupont.
- **Fil : 105 °C obligatoire** pour la traversée et la liaison vers le module :
  UL1015 (600 V) ou UL1569 (300 V), avec la gaine VW-1 ou deux épaisseurs de
  thermorétractable sur toute la longueur. L'UL1007 n'est donné que pour 80 °C :
  acheté pour la reconnaissance (journal, 27/09), il reste au câble provisoire
  de la sonde. Dans le produit, il serait la pièce la moins tenue du boîtier
  électronique, sous le câble d'origine du panneau (gaine VW-1 105 °C,
  SPEC-RECONNAISSANCE §3), alors qu'aucune température d'air n'est plus
  mesurée. Les fils des fiches XH précâblées sont souvent de l'UL1007 : on lit
  leur marquage, et, s'il le faut, on sertit du fil 105 °C dans les contacts XH
  (Q41). Fil fixé au câble du panneau, jamais serré contre les fils du moteur ni
  contre le connecteur blanc (règle 5 de SPEC-RECONNAISSANCE.md).

**Étages selon l'architecture** (valeurs chiffrées dans l'avenant de l'étape 6) :

| | A | B | C |
|---|---|---|---|
| `D` | traverse sans coupure | coupé dans la traversée : `D_carte` et `D_panneau` vont au module | traverse sans coupure |
| écoute | étage NPN de la sonde (100 k série, `R_be` 100 k ou relevée, 10 k de collecteur), GPIO0 | deux étages, GPIO0 et une broche de l'avenant | un étage, GPIO0 |
| émission | NPN en drain ouvert (4,7 k de base, 10 k base-émetteur, 470 Ω de collecteur), GPIO1 | par voie : NPN, ou tampon push-pull (74HCT) si `D` est push-pull | un PhotoMOS faible capacité (type AQY221N2S) par touche, près du panneau, 6 fils vers le panneau ouvert |
| si le module tombe | la hotte marche normalement | **relais de contournement à repos fermé** (`D` rétabli module hors tension) commandé par un **chien de garde matériel** indépendant du C6 | aucun effet |
| condition | la carte pilote les voyants, silences exploitables | la carte pilote les voyants | le panneau s'ouvre ; sinon panneau de rechange (repli) |

### 6.4 Lecture annexe (seulement si le fil ne suffit pas)

Si l'étape 6 et l'ajout (c) montrent que `D` ne répète pas l'état (§5.7), la
lecture annexe devient **obligatoire** pour tenir C1 après un redémarrage du
module ; elle voit aussi la fin de la marche prolongée (§5.9) :
- **lampe :** tension de `CN1` (`12V_LED`), par une traversée enfichée et un
  étage d'écoute (jamais la tension sur une GPIO) ;
- **vitesse :** entrées 1 à 7 d'IC3 (`ULN2003H`), par une pince SOIC-16, ou
  un autre point sans soudure trouvé d'ici là.

Elle ne lit pas ⏻ (§5.4). Elle touche `CN1` et IC3, que la reconnaissance
interdit : elle fait donc l'objet d'un **avenant à cette spec, cadré avec
Majid** avant le plan des points 6 à 9 (Q25). La tenue d'une pince dans une
hotte qui vibre pendant des années est le point à y trancher.

### 6.5 Alimentation

**Chaîne :** `+` de `CN3` → **diode Schottky en série** → **fusible réarmable**
→ **470 µF** → broche `5V` de la SuperMini ; `−` de `CN3` → `GND`.

**Condition préalable : la 3b.** La chaîne vers la broche `5V` n'est montée que
si la 3b lit le `+` **entre 4,75 et 5,5 V dans toutes ses situations** (veille,
marche, lampe, V1 à V3, appui). Le régulateur de la SuperMini (ME6211 probable,
Q18) ne tient pas plus de 6,5 V en entrée (fiche ME6211) : sur 12 V, le module
serait détruit dès la mise sous tension. **Sinon, rien sur la broche `5V`** : un
abaisseur dédié, alimenté par le `+` de `CN3`, sans soudure (si le `+` est à
12 V, c'est lui le 12 V permanent du repli), et une nouvelle revue avant tout
essai (Q24).

| Élément | Rôle | Choix |
|---|---|---|
| Schottky **1N5819** (achetée ; 1 A, 40 V) | empêche l'USB, branché par erreur, de réalimenter le rail 5 V de la hotte et son panneau. La liaison VBUS-5V de la SuperMini semble directe (NV, Q18) | chute d'environ 0,3 à 0,45 V |
| fusible réarmable **RXEF025 ou RXEF050** (achetés ; un seul est posé) | **borne la durée d'un défaut franc du module et protège le câblage.** Il met de quelques dixièmes de seconde à quelques secondes à déclencher, selon le courant : pendant ce temps, la branche 5 V (XL1509) débite dans le défaut, et la hotte peut redémarrer, sans danger | RXEF025 : maintien 0,25 A, déclenchement 0,50 A, 1,25 à 1,95 Ω neuf, jusqu'à 3,00 Ω après un déclenchement, 0,45 W dissipés déclenché. RXEF050 : maintien 0,50 A, déclenchement 1,00 A, 0,50 à 1,17 Ω, 0,77 W (fiche Littelfuse RXEF). Choix à l'essai de 24 h (ci-dessous). Les pointes d'émission (quelques ms) ne le font pas déclencher (NV, Q19). Sa tenue baisse à chaud : hypothèse de pire cas écrite, 85 °C (carte OEM de référence, fiches XH), à confirmer sur la fiche (Q19) ; la température de la puce, relevée à l'essai de 24 h puis en usage, dit si l'on s'en approche |
| 470 µF **16 V, 105 °C, 8×12** (acheté), faible résistance série | absorbe les pointes d'émission et de BLE | côté module, près de la broche `5V` |
| pont de la mesure du `+` de `CN3`, vers GPIO2 | tension du rail de la hotte, avant la diode : minimum glissant publié | **100 k sur la traversée**, au nœud `+` de `CN3` (§6.3) ; côté module, 100 k vers `GND` et un condensateur de **1 à 10 nF** (τ de 50 à 500 µs ; l'erreur de partage de charge avec l'échantillonneur reste sous 1 %) : 2,5 V à l'ADC pour 5 V. Au plus 50 µA dans GPIO2 si le module est hors tension (fusible déclenché) : acceptable (NV, Q18). Autre `+` que 5 V : pont recalculé à la nouvelle revue |
| pont de la mesure côté module, vers GPIO3 | tension après diode et fusible, au 470 µF : c'est elle qui juge la chute de la chaîne (l'ADC côté hotte ne voit ni la diode ni le fusible) | 100 k / 100 k et 1 à 10 nF, côté module |

**Choix du fusible (à l'essai de 24 h, §9.4).** Les deux mesures ont chacune
leur rôle : le côté hotte (GPIO2) juge la marge du rail de la hotte, donc la
puissance d'émission (§6.1) ; le côté module (GPIO3) juge la chute de la chaîne,
donc le fusible (amendement : choix d'après la chute de tension). RXEF025 par
défaut. RXEF050 seulement si, avec le RXEF025, le côté hotte tenant ses
critères, le minimum côté module passe sous 3,7 V, ou si le module redémarre :
3,3 V du régulateur plus sa chute (ME6211C33 : 260 mV à 200 mA, fiche ; NV,
Q18). L'autre choix, garder le RXEF025 et réduire l'émission, reste ouvert
(Q19). On note alors que le
RXEF050 **ne protège plus la branche entre 0,5 et 1 A** : il déclenche vers 1 A,
au niveau de la limite que fixe la SS14 (§2.3), et plus lentement. Un défaut du
module jusqu'à 0,5 A ne le fait jamais déclencher, et jusqu'à 1 A pas forcément ;
ce courant pèse alors sur un flyback de marge inconnue. Si Majid a une
alimentation de laboratoire, un court-circuit franc du module au banc le vérifie.
Le choix pèse donc le courant de déclenchement face à la limite de la branche,
pas seulement la chute de tension.

**Budget :** environ 75 à 85 mA en moyenne (récepteur 802.15.4 toujours
allumé, routeur comme MED ; moyenne déduite de la fiche et d'une mesure
tierce) ; pointes de 187 mA à 12 dBm et 305 mA à 20 dBm, quelques ms par trame
(fiche de la puce ESP32-C6, v1.5, tableau 5-9 : la SuperMini porte la puce nue,
pas le module MINI-1). En BLE, pendant la mise en service : environ 190 mA à
9 dBm, puissance par défaut de la plateforme
(`CONFIG_BT_LE_DFT_TX_POWER_LEVEL_DBM_EFF` = 9), 315 mA à 20 dBm (tableau 5-8) ;
la mise en service se fait au banc, sur l'USB (§8.3). Le régulateur de la
SuperMini dissiperait environ 0,1 W sous les 4,4 V qui lui restent après la
diode et le fusible (estimation ; sa référence : NV, Q18).

**Mesure des tensions :** ADC en mode continu, les deux voies à environ 1 kHz
chacune ; minimum sur chaque seconde et minimum depuis le démarrage, publiés
dans le bloc `alim` du compagnon (§7.4), avec une alerte sous
`alim_hotte_min_mv` ou `alim_module_min_mv`, et journalisés pendant l'essai de
24 h. **Limite :** un creux plus court que la période d'échantillonnage peut
échapper à l'ADC. Majid n'a pas d'oscilloscope (journal, 27/09) : ce sont les
critères « aucun redémarrage » et « hotte toujours en V3 » de l'essai de 24 h
qui jugent le reste. Le capteur de température et l'ADC partagent le même
périphérique sur le C6 : leur cohabitation se vérifie au banc (NV, Q42).

**Règles :**
- **Jamais d'USB branché sur le module raccordé à `CN3`.** Pour flasher, on
  débranche le module de la traversée, hotte débranchée (§6.8).
- **Jamais `12V_LED`**, qui est commutée ; jamais l'enroulement auxiliaire du
  primaire.
- **Courant d'appel du 470 µF :** hotte et module montent ensemble, et le
  XL1509 démarre en rampe (NV, Q19). On l'observe à l'essai de 24 h : bip de mise
  sous tension normal, panneau fonctionnel.

**Repli : rail 12 V permanent et abaisseur dédié.** Si la 3b lit un `+` hors de
4,75-5,5 V (le `+` de `CN3` est alors lui-même le rail permanent, et
l'abaisseur s'y branche sans soudure), ou si l'essai de 24 h échoue même à
8 dBm. Aucun autre connecteur ne porte à coup sûr un 12 V permanent : `CN1` est
commuté, la fonction de `CN2` est inconnue, et prendre le 12 V sur la carte
obligerait à y souder, ce que la décision interdit. Le repli passe donc par une
décision de Majid, faits en main (Q24).

### 6.6 Boîtier imprimé

- **Emplacement :** quart haut-droit du boîtier électronique, loin du
  condensateur CBB61 et du gros connecteur blanc, d'après la photo avec règle
  (§9.3).
- **Matière : ASA, imposé** (amendement de Majid du 28/09 au soir). Le PETG est
  exclu : il se déforme vers 70 °C. L'ASA tient jusque vers 90-95 °C
  (Prusament ASA : HDT de 93 °C à 0,45 MPa, 86 °C à 1,8 MPa). Pas de relevé de
  température : le filet de sécurité est la température de la puce, publiée
  avec une alerte (§7.4).
- **Tenue au feu :** l'ASA n'est pas ignifugé. Elle repose sur la **limitation
  d'énergie** : tout ce qui entre dans le boîtier imprimé est en aval du fusible
  réarmable ou d'une résistance de limitation (§6.7, règle 9). Le module est
  ainsi un circuit de faible puissance au sens de la CEI 60335-1 (19.11.1 :
  15 W au plus), exempté des essais au fil incandescent du 30.2.3 (exploration
  « matériel », fait 35 : lecture de la CEI 60335-1:2020). Les classements des
  filaments valent pour des éprouvettes moulées, pas pour une pièce imprimée :
  Majid a imposé l'ASA et accepte la responsabilité de la modification (§2.1).
- **Fermé**, sans ouverture vers le flux d'air gras ; entrée du câble par une
  fente ajustée.
- **Deux fixations indépendantes**, jamais la colle seule, **sans perçage ni
  limage** de la hotte : par exemple un clip sur une nervure du boîtier
  électronique plus un collier passé dans une ouverture existante. Le choix
  suit la photo des points d'ancrage (Q16).
- **Antenne :** 15 mm de dégagement dans le boîtier imprimé, aucun métal à
  moins de 10 mm, fils du module compris (ils partent à l'opposé de l'antenne,
  §6.2), tournée vers la direction choisie après M1.
- **USB-C accessible** en ouvrant le boîtier imprimé, pour flasher.

### 6.7 Règles d'isolement et de pose

Objectif de conception : l'isolation renforcée de la CEI 60335-1 entre le module
(fils compris) et toute pièce au 230 V.

1. **Au moins 3 mm dans l'air et 8 mm en ligne de fuite** de toute pièce au
   230 V (la hotte compte en degré de pollution 3 dans l'édition lue de la
   partie hottes ; édition en vigueur : NV, Q31).
2. **Paroi pleine d'au moins 2 mm** si l'isolation repose sur elle.
3. **Deux fixations indépendantes, pour le module et pour la traversée**
   (§6.3) : un fil ou une pièce qui se détache ne doit jamais pouvoir atteindre
   un conducteur secteur (22.31). La fiche sur `CN3` ne compte pas comme
   fixation. Serre-fils près des soudures du module.
4. **Le `+` ne touche jamais la carcasse** (le `−` est à la terre) : aucun
   métal nu côté `+`, fusible au plus près de `CN3`.
5. **Aucun perçage, aucun limage** dans la hotte : les copeaux métalliques
   sont une pollution conductrice.
6. **Rien de libre** dans le boîtier électronique ; câbles le long du câble du
   panneau ; couvercle et filtre sans rien pincer.
7. **Toute intervention hotte débranchée**, 5 min d'attente avant d'ouvrir le
   boîtier électronique (règle 1 de SPEC-RECONNAISSANCE.md).
8. **Contrôle après chaque intervention, en deux temps**, hotte débranchée :
   - **(a) couvercle ouvert**, 5 min après le débranchement : fixations (module
     et traversée), cheminement et distances vérifiés à l'œil ; broches rondes
     de la fiche reliées, ohmmètre au calibre le plus élevé vers `−`, `D` et
     `+` sur la traversée : **OL** dans les deux sens après 10 s (critère et
     **ARRÊT** de la ligne a de l'étape 1) ; résistance entre `+` et `−` de la
     traversée **non nulle** (un `+` contre la carcasse, reliée au `−`, la
     mettrait à 0 Ω) ;
   - **(b) couvercle fermé et filtre remis** : broches rondes reliées,
     ohmmètre au calibre le plus élevé vers le **contact de terre de la
     fiche** : **OL** dans les deux sens après 10 s, même critère, même
     **ARRÊT**. Le `−` de `CN3` étant relié à la terre (étape 1, ligne b),
     cette mesure couvre le module, la traversée et leurs fils ; un défaut vers
     `D` ou vers `+` s'y voit à travers les circuits du rail (pont de la
     mesure, pull-up de `D`). Elle repère un conducteur secteur pincé à la
     fermeture.
   - **Limite :** un `+` pincé contre la carcasse pendant la fermeture échappe
     aux deux mesures : il ne se voit que couvercle ouvert. À la mise sous
     tension, il donnerait un panneau mort : on débranche aussitôt (règle 12).
9. **Aucun conducteur ne sort de la traversée vers le module sans limitation de
   courant** : le `+` passe par le fusible réarmable ; la mesure, par la
   résistance haute du pont (100 kΩ) ; `D` est bornée par les résistances de la
   carte (`R13` 4,7 kΩ et `R1` 1 kΩ, rôle supposé : NV, Q33). Si la 3c ne le
   confirme pas, une résistance série sur la traversée, chiffrée dans l'avenant
   de l'étape 6.
10. **Exception à la règle 3 de SPEC-RECONNAISSANCE.md**, qui ne permet de
    toucher que `CN3` et le câble du panneau : pour la pose et les
    interventions du produit, hotte débranchée depuis 5 min, on touche en plus
    la traversée, le module, son boîtier et les seuls points d'ancrage retenus
    (photos de la Q16), jamais dans les zones interdites. Le second plan la
    reprend dans SECURITE.md ou dans INSTALLATION.md.

### 6.8 Mises à jour et interventions

Pas d'OTA. Une mise à jour :
1. hotte débranchée, 5 min ;
2. filtre retiré, couvercle ouvert ;
3. **fiche du module retirée de la traversée** (le module n'a plus aucune
   liaison avec la hotte) ;
4. USB-C branché, flash (`pio run -e produit -t upload`), vérification de la
   version (`hello`) ;
5. USB-C débranché, fiche remise, couvercle, filtre ;
6. contrôle du §6.7, point 8 (a, puis b) ; puis rebranchement.

Premier flash : `-t erase` puis `-t upload` (Thread). Un flash par un autre
environnement rend le nœud injoignable jusqu'au retour du bon.

**Si Maison affiche durablement « Pas de réponse » et que le compagnon ne
répond plus :** le fusible a pu déclencher, et il dissipe alors 0,45 ou 0,77 W
sans autre signe. On débranche la hotte, on attend 5 min, on ouvre, on retire
la fiche du module de la traversée, et l'on cherche le défaut avant de
rebrancher.

### 6.9 Achats

| Article | État |
|---|---|
| diode Schottky 1N5819 | **achetée** (amendement) |
| fusibles réarmables RXEF025 et RXEF050 | **achetés** ; un seul est posé (§6.5, §9.4) |
| condensateur 470 µF, 16 V, 105 °C, 8×12 | **acheté** |
| filament ASA | **déjà là** |
| une SuperMini violette libre pour le module | **déjà là** ; sert-elle aussi au banc Matter ? (Q23) |
| fil 105 °C : UL1015 (600 V) ou UL1569 (300 V), gaine VW-1 | **à acheter**, avant la traversée (§6.3, Q41) |
| condensateurs céramique de 1 à 10 nF, résistances de 100 kΩ | stock de la reconnaissance (1 nF et 100 k y sont) ; complément au besoin |
| fiches et embases XH 3 et 4 broches | déjà achetées pour la reconnaissance ; complément au besoin |
| une C6 de test pour le banc Matter, si ni la SuperMini libre ni une carte du projet ne convient (Q23) | au début du point 3 |
| *repli radio :* XIAO ESP32-C6 et antenne u.FL | seulement si la marge est sous 10 dB |
| *repli voyants :* panneau de rechange | seulement si l'étape 6 conclut « panneau maître et résiné » |

---

## 7. Canal compagnon : profil « hotte », build `produit`

### 7.1 Ce qui ne change pas

Tramage, enveloppe, session, commandes `id=<n>`, `reponse`, versionnage et
enveloppe H1 : **identiques** à la ScreenBar au commit `c58a506`, comme pour la
sonde. `rev` = 4 ; les messages propres au produit sont des ajouts (l'app ignore
l'inconnu). Le profil est documenté dans un nouveau document,
`docs/PROTOCOLE-JSON-PRODUIT.md`, **par différence** avec la ScreenBar (transport
Thread) et avec la sonde (messages de la hotte).

### 7.2 Transport

- **USB CDC**, au banc et hotte débranchée seulement.
- **UDP sur Thread, port 5480**, comme la ScreenBar : socket OpenThread, joint
  par l'adresse OMR à travers les routeurs de bordure ; découverte par le **nom
  d'hôte SRP** du nœud, appris par l'USB. Ouvert seulement si une clé existe.
  Code : `net_udp` de benq, copié.
- Débit plafonné à 3 000 octets/s, 24 tampons OpenThread gardés en réserve,
  priorité basse, comme la ScreenBar : Thread partage la bande avec Matter.
  Seule exception : `radio rafale`, réservé à l'essai de 24 h (§7.5), hors
  plafond de débit mais dans la réserve de tampons.

### 7.3 `hello`

| Champ | Valeur |
|---|---|
| `base.build` | **`produit`** (la sonde dit `sonde`) |
| `base.reseau_build` | `thread` |
| `base.env` | `produit` (ou `produit_simule`) |
| `identite.appareil` | `hotte` |
| `identite.id.fabricant`, `id.produit`, `id.serie`, `id.nom` | `Djoko-CLI`, `Module hotte Haier`, `HOTTE-` + MAC, `Hotte` |
| `identite.id.hw_txt` | `C6 SuperMini, pilote <simule\|A\|B\|C>` |
| `identite.caps` | `hotte` (commandes du §7.5), `matter`, `udp`, `cle`, `log`, `trames_d` (trames décodées, bornées), `alim`, `thermique`, `essai` (commandes d'essai) ; `simule` dans le build simulé. **Jamais `injection`** |
| `thermique.temp_max_pose_c` | maximum de la puce depuis la pose (NVS `temp_max`) |

Une app multi-appareils distingue la sonde du produit par `build`, et les deux
de la ScreenBar par `appareil`.

### 7.4 Messages

| Message, bloc | Contenu |
|---|---|
| `etat`, `hotte` | `marche` (`eteinte`, `armee`, `prolongee`, `inconnue`), `moteur` (0 à 3), `lampe`, `confiance`, `source` (`fil`, `annexe`, `deduit`, `nvs`), `age_ms` |
| `etat`, `automate` | cibles en cours, séquence (`id`, étape, touche, essai, depuis), délai moteur restant |
| `etat`, `matter` | mis en service, fabriques, abonnements, fenêtre d'ignorance du démarrage |
| `etat`, `thermique` | `temp_c` (puce, lue toutes les 10 s), `temp_max_c` (depuis le démarrage), `temp_max_pose_c` (depuis la pose, NVS `temp_max`), `alerte` (vrai ou faux), `alertes` (nombre), `seuil_c` |
| `etat`, `alim` | deux voies, `hotte` (`+` de `CN3`, GPIO2) et `module` (après diode et fusible, GPIO3) : tension actuelle, minimum de la dernière seconde et minimum depuis le démarrage (mV), `alerte` |
| `etat`, `sys` | l'objet `sys` de la ScreenBar, à l'identique |
| `compteurs`, `hotte` | appuis du panneau, appuis du module, séquences (réussies, annulées, échouées par cause : `non_confirme`, `sans_lecture`, `collision`, `garde`, `pilote`, `duree` ; abandonnées), nouveaux essais, transitions inconnues, anomalies de réception, états `marche_inconnue`, `prolongee_perimee` |
| `compteurs`, `alim` | passages sous `alim_hotte_min_mv` et sous `alim_module_min_mv` |
| `compteurs`, `radio` | rôle réel (`otThreadGetDeviceRole`), puissance d'émission (`otPlatRadioGetTransmitPower`), sensibilité rapportée (`otPlatRadioGetReceiveSensitivity`, −120 attendus) ; en MED, RSSI moyen et qualité de lien entrée et sortie avec le parent (`otThreadGetParentInfo`, `otThreadGetParentAverageRssi`) ; en routeur, qualité de lien entrée et sortie avec les routeurs voisins (`otThreadGetRouterInfo` : `mLinkQualityIn`, `mLinkQualityOut`) et leur RSSI moyen (`otThreadGetNextNeighborInfo`), le meilleur en tête ; compteurs MAC (`otLinkGetCounters` : `mTxTotal`, `mTxRetry`, `mTxDirectMaxRetryExpiry`) ; changements de parent et de rôle (`otThreadGetMleCounters`) ; pertes du réseau |
| `reseau`, `ip` | schéma de la ScreenBar (`srp.nom`, adresses, `udp.*`) |
| événement `hotte` | changement d'état : avant, après, origine (`panneau`, `module`, `inconnue`), source (`fil`, `annexe`) |
| événement `sequence` | fin d'une séquence : `id` (l'entier de l'ordre de l'app ; null hors de l'app), `origine` (`app` ou `matter`), issue, cause, durée, nombre d'appuis |
| événement `alerte` | `sujet` (`temperature`, `lecture_temperature`, `alim_hotte`, `alim_module`), `debut` ou `fin`, valeur, seuil. **Toujours émis vers chaque session, sans abonnement**, et doublé d'un `log` de niveau `notice` émis même sans `json log 1`. Une lecture de température en échec (NAN) est une alerte. **Aucune action sur la hotte** |
| événement `trame_d` | trame décodée du fil (origine, octets, sens) ; diagnostic. Nom distinct du `trame` de la sonde, qui porte des durées brutes : `json_check.py` vérifie chaque schéma d'après `build` |
| `log` | annonces du firmware |

**Température de la puce.** Lue par `temperatureRead()` du cœur 3.3.12, toutes
les 10 s, dans `loop()`. Ce cœur installe le capteur pour 10 à 50 °C
(`esp32-hal-misc.c`, l. 91), mais l'IDF 5.5 **change seul de plage** quand la
mesure en sort (`temp_sensor_get_raw_value`, `sar_periph_ctrl_common.c`, et la
bibliothèque précompilée 5.5.5) : la lecture reste valable jusqu'à 125 °C,
avec une erreur de 1 à 3 °C selon la plage. Seule une valeur hors de −40 à
125 °C rend une erreur (NAN). La puce lit un peu plus chaud que l'air, ce qui va
dans le sens de la prudence. Seuil et hystérésis : §4.2 (Q34). Au franchissement
du seuil : événement `alerte` et `log`. Le maximum depuis la pose est gardé en
NVS (§4.4) et publié dans `hello`.

Pour la validation (§9.5), chaque événement porte l'heure du module en ms ; les
journaux se comparent à la vidéo par un repère commun (un appui filmé).

### 7.5 Commandes et liste blanche

| Commande | Effet | À distance |
|---|---|---|
| `hotte` | état et paramètres, en texte | non |
| `hotte ventilo off\|v1\|v2\|v3\|derniere` | ordre à l'automate, **asynchrone** : `reponse` `accepte` avec `suite` = `sequence`, puis l'événement `sequence` avec le même `id` | **oui** |
| `hotte lampe on\|off` | idem | **oui** |
| famille `json` | comme la ScreenBar, mêmes bornes à distance | oui, liste de la ScreenBar |
| `json trames 0\|1` | trames décodées (`trame_d`) vers la session | oui ; coupées seules après 60 s à distance, 10 par seconde au plus |
| `reboot` | redémarrage ; **écrit d'abord les valeurs NVS en attente** (§4.4) | **oui** (écart à la ScreenBar, Q27) : nécessaire au test V4 sans USB ; sans effet sur la hotte |
| `radio rafale <s>` | rafale d'émission pour l'essai de 24 h : datagrammes pleins vers l'adresse de la session, à la puissance en cours, pendant s secondes (1 à 30), hors plafond de débit, dans la réserve de tampons | **seulement si `essai alim on` a été posé par l'USB** (drapeau NVS `essai`) |
| `essai alim on\|off` | arme et désarme l'essai de 24 h ; effacé seul au bout de 72 h | **non** |
| `json cle nouvelle\|efface`, `decommission`, `matter [med\|tx\|maxint\|derniere]`, `hotte regle <nom> <valeur>`, `simu ...` (build simulé) | réglages, clé, remise à zéro, simulation | **non** : USB seulement, réponse `interdite` (`matter tx` à distance : Q39) |

Tout le reste répond `interdite` à distance, en particulier `injecte`,
`injection`, `capture` et `seuils`, qui n'existent pas dans ce build (vérifié par
un test, §9.1). Le code manuel et le QR valent toujours null à distance.
**Aucune commande d'injection brute n'est compilée dans le build `produit`.**

### 7.6 Profil distant

Comme la ScreenBar : `etat` toutes les 2 s (avec `thermique` et `alim`),
`compteurs` coupés (0), `reseau` toutes les 30 s, `trame_d` et `log` coupés au
départ. `alerte` part toujours. Les essais qui ont besoin des compteurs (radio,
alimentation) les demandent (`json compteurs 10000`).

### 7.7 Clé H1

- Créée par `json cle nouvelle` **par l'USB seulement**, au banc, **avant le
  montage** ; gardée en NVS (`produit/cle`) ; côté Mac, dans
  `~/.config/hotte-produit/cle` (droits 0600), jamais affichée ;
  `hotte_udp.py` la choisit d'après `build` (§4.7).
- **La remise à zéro Matter l'efface**, comme le départ du dernier contrôleur
  (règle de benq) : un nouveau propriétaire ne garde pas l'accès de l'ancien.
  Il faut alors l'USB, donc ouvrir la hotte (§6.8).

---

## 8. Identité et mise en service

### 8.1 Identité (EP0, posée à chaque démarrage)

| Attribut | Valeur |
|---|---|
| VendorName | `Djoko-CLI` |
| ProductName | `Module hotte Haier` |
| NodeLabel | `Hotte` (réécrit à chaque démarrage) |
| SerialNumber | `HOTTE-` + MAC d'usine en 12 hexa majuscules |
| HardwareVersion / HardwareVersionString | `1` / `C6 SuperMini + pilote <A\|B\|C\|simule>` |
| SoftwareVersionString | `FW_VERSION-<commit>` par `src/app_desc.c` (« Programme interne » dans Maison) |
| VendorID / ProductID | 0xFFF1 / 0x8000 (certificat de test, comme la ScreenBar) |

La sonde et le module peuvent porter le même numéro de série s'ils utilisent la
même carte : `build` les distingue.

### 8.2 Codes d'appairage

- **Discriminateur et code d'appairage distincts de ceux de la ScreenBar**
  (qui garde les valeurs de test de la bibliothèque : 0xF00 et 20202021).
- Tirés au hasard à l'implémentation (tâche identité du premier plan) : un
  discriminateur de 12 bits différent de 0xF00 ; un code de 1 à 99 999 998, hors
  des valeurs interdites par Matter (00000000, 11111111 à 99999999, 12345678,
  87654321 : liste citée de mémoire, NV, Q32).
- Posés par `Matter.setSetupDiscriminator()` et `Matter.setSetupPasscode()`
  **avant** `Matter.begin()`, dans `config_produit.h`.
- **Jamais changés après la première mise en service.** Ils sont publics dans le
  dépôt, comme ceux de la ScreenBar : ils ne servent que pendant la fenêtre de
  mise en service, nœud non mis en service, à portée de BLE.
- Deux nœuds 0xFFF1/0x8000 dans la même maison : cohabitation vérifiée au banc
  (NV, Q12).

### 8.3 Mise en service

Au banc, avant le montage, module sur l'USB du Mac (aucune liaison avec la
hotte) :
1. si la carte a servi au banc Matter (Q23) : `decommission` d'abord (le nœud de
   test sort de Maison) ; puis premier flash (`-t erase`, puis `-t upload`) ;
2. `json cle nouvelle` (clé H1) ; `matter tx` à la puissance retenue à l'essai
   de 24 h (§6.1) ;
3. mise en service dans Maison (BLE, Thread), avertissement « non certifié »
   accepté ;
4. accessoires séparés, noms, pièce « Cuisine » (§3.5) ;
5. relevé du nom SRP pour l'app compagnon ;
6. impression de l'étiquette.

Les identifiants Thread restent en NVS : une fois installé, le module rejoint le
réseau seul.

### 8.4 Étiquette

- **QR code de la charge d'appairage « MT:… ».** C'est le paramètre `data=` de
  l'URL que rend `Matter.getOnboardingQRCodeUrl()`, avec les `%XX` décodés
  (`%3A` donne « : »), comme le fait benq (`docs/PROTOCOLE-JSON.fr.md`,
  l. 680). L'URL elle-même mène à une page qui dessine le QR (`GetQRCodeUrl`,
  `MatterIdentity.cpp`, l. 239) : imprimée telle quelle, elle coderait une URL,
  que Maison ne prend pas pour un code d'appairage. On peut aussi imprimer
  l'image que dessine cette page.
- **Code manuel :** `Matter.getManualPairingCode()`.
- Imprimés, protégés de la graisse (plastifiés ou sous ruban), collés
  **derrière le filtre**, à un endroit visible filtre retiré, hors de toute
  pièce au 230 V.

### 8.5 Remise à zéro

- **Par l'USB seulement** (`decommission`), module débranché de la traversée
  (§6.8). Elle retire toutes les fabriques et efface la clé H1.
- Pas de remise à zéro à distance, pas de séquence de touches, pas de séquence
  de coupures.
- **Le bouton BOOT (IO9) n'a aucune action dans le build `produit`** (§4.8).

### 8.6 Aucun voyant ajouté

L'état du module se lit dans l'app compagnon. La LED RGB d'IO8, si la carte
en porte une, est mise au noir au démarrage (§6.2). Identify : aucun effet (§3.3).

---

## 9. Vérification

### 9.1 Tests sur le Mac

`sh tools/tests/test_hote.sh`, sans carte :

| Sujet | Ce qui est vérifié |
|---|---|
| table des transitions | chaque ligne du §2.2 ; les transitions inconnues marquées comme telles ; les lignes « supposé identique en V1 et V2 » codées observées (§5.2) : un appui du panneau sur V1 active garde `Deduit` |
| suites d'appuis | chaque couple (état, cible) du §5.3, avec le pilote simulé, dans les trois modes d'état, avec et sans lecture annexe ; issues attendues des cas imposés ci-dessous |
| règle d'or | propriété : sur **toutes** les suites générées, jamais « marche » sans un arrêt du moteur lu (fil ou lecture annexe) après le démarrage et après le dernier appui |
| confiance | un état `Confirme` sans événement reste utilisable, jamais `Inconnu` ; ligne muette en mode répété (`fraicheur_ms`) ou anomalie : `Presume` ; « prolongée » au-delà de `prolongee_max_ms` sans lecture : `Presume`, état publié inchangé |
| confirmation et nouvel essai | par mode (tableau du §5.4) ; `Delai` : nouvel essai immédiat ; `Collision` : état inchangé, nouvel essai ; attendu, étape réussie ; autre, `Annulee` ; aucun, `Echec` ; `Garde` : `Echec` (cause `garde`), état republié ; `Erreur` : `Echec` (cause `pilote`), ordres suivants en attente puis `Abandon` ; jamais deux nouveaux essais ; jamais de nouvel essai sans preuve |
| annulation | un appui du panneau à chaque étape de chaque séquence : `Annulee`, état réel publié, plus aucun appui |
| délais | délai moteur toutes origines, sur tout appui qui change le moteur ; aucun `Garde` du pilote dans les suites normales ; entre deux appuis ; `sequence_max_ms` ; `attente_etat_ms` |
| regroupement d'EP1 | priorités de la règle 6 : On seul, dernière vitesse ; On et 40 %, V2 ; 40 % puis Off, éteindre ; 0 puis 50 %, V2 ; Off et son 0 en cascade, éteindre. Écritures espacées de moins de `lissage_calme_ms` pendant 8 s : au plus ⌈8 s / plafond⌉ + 1 ordres, le dernier égal à la dernière écriture ; écritures plus espacées que le calme : un ordre chacune, et au plus un changement du moteur par `delai_moteur_ms`. EP2 : `ordre_calme_ms` |
| correspondance | `palier()` et `pourcent()` pour 0 à 100, aller et retour |
| démarrage | mise sous tension (éteinte, `Deduit`, aucun « marche » avant un arrêt lu) ; autre cause (`Presume`) ; dernière vitesse (V2 par défaut) ; ordres ignorés 2 s ; aucun appui au démarrage |
| écritures NVS différées | échéance de 10 s ; `reboot` et `decommission` écrivent d'abord ce qui attend |
| sonde de vitesse | permise ou interdite selon le réglage lié à Q6 |
| surveillance | seuils et hystérésis (température, deux tensions) ; lecture en échec (NAN) : alerte ; maximum depuis la pose écrit au plus une fois par minute, et seulement s'il monte d'au moins 1 °C ; aucune action sur la hotte |
| profil compagnon | lignes produites conformes (`json_check.py`, profil produit, schéma de `trame_d` distinct du `trame` de la sonde) ; exemples du document vérifiés ; liste blanche du produit : `injecte`, `injection`, `capture` et `seuils` répondent `interdite` à distance ; `radio rafale` refusé sans `essai alim on` ; `alerte` émis sans abonnement ; `sequence` : `id` entier ou null, `origine` |
| paramètres | bornes, rejet d'un jeu NVS hors bornes (automate et surveillance) |

**Cas imposés, avec leurs issues attendues** (pilote simulé ; « changements »
et « appuis seuls » avec lecture annexe, sauf mention) :

| Cas | État répété | État aux changements | Appuis seuls |
|---|---|---|---|
| éteindre depuis V2, carte qui répond | `Ok` | `Ok` | `Ok` ; ⏻ final `Deduit` |
| idem, premier appui V2 sans effet | nouvel essai (état répété inchangé), puis `Ok` | nouvel essai (écho vu, aucune trame ; ou moteur inchangé à la lecture annexe), puis `Ok` | nouvel essai (moteur inchangé à la lecture annexe), puis `Ok` |
| lampe, trame de la carte illisible | `Presume` ; la répétition suivante tranche : lampe changée, `Ok` ; inchangée au-delà de `confirmation_ms`, nouvel essai | la lecture annexe tranche : `Ok`, ou nouvel essai si la lampe est inchangée | la lecture annexe tranche |
| « marche » (armée vers éteinte) sans effet | nouvel essai, puis `Ok` | nouvel essai si l'écho a été vu et aucune trame d'état ni anomalie n'a suivi | pas de nouvel essai : `Echec` (⏻ illisible) |
| collision sur V2 (arrêt), puis moteur arrêté lu | étape réussie, puis « marche » | idem | idem |
| collision, aucun état utile lu dans `confirmation_ms` | sans objet (l'état répété arrive) | « marche » sans trame d'état (la lecture annexe ne voit pas ⏻) : `Echec` ; moteur et lampe : la lecture annexe tranche | « marche » : `Echec` ; moteur et lampe : la lecture annexe tranche |
| **mise sous tension, hotte réellement en V2, ordre V3** | état lu V2, puis V3 ; **jamais « marche »** | idem ; **sans lecture annexe** : `Abandon` au bout de `attente_etat_ms`, jamais « marche » | idem |

### 9.2 Banc Matter, sans hotte

**Montage.** Une C6 de test (Q23), jamais la ScreenBar (`7E:F0`) ni la sonde de
maillage benq (`A4:6C`), sur l'USB du Mac, environnement `produit` avec le
pilote simulé. Mise en service dans la Maison de Majid, à côté de la ScreenBar.
Versions d'iOS, de tvOS et des HomePod relevées au début. Un build de trace
(`CORE_DEBUG_LEVEL=5`) sert aux relevés B2 et B3. Le Mac a l'assistant de route
de benq (§4.7).

| # | Question | Méthode | Ce que ça décide |
|---|---|---|---|
| B1 | rendu du ventilateur : curseur continu ou trois crans ? | captures d'écran, iPhone et Mac ; écart entre deux écritures d'un geste de curseur | réglage du regroupement (Q8) |
| B2 | qu'écrit Maison au bouton de la tuile ? | journal des écritures brutes (`MatterFanHotte`) : valeurs, ordre, écart entre `FanMode` et `PercentSetting` d'un même geste ; statut rendu à Maison et sa réaction, substitution de On active, puis coupée (`matter derniere 0`) | « dernière vitesse » (§3.3, Q7) ; regroupement d'EP1 |
| B3 | combien de temps Maison garde-t-elle l'état demandé si le nœud rapporte autre chose ? | `simu refuse` : la hotte simulée ne suit pas ; chronométrage **à partir de la republication de l'état réel** ; `PercentCurrent` relevé pendant et après la séquence refusée ; un cas de séquence en échec | tolérance du conflit (Q9) ; §3.2 vérifié |
| B4 | piège de la NVS | hotte simulée en V3 et lampe, USB coupé 10 s, rebranché (la hotte simulée revient éteinte, comme H-coupure) | parade validée : « éteint, 0 % », lampe éteinte, jamais « High à 0 % » ni « Extinction… » |
| B5 | Siri en français | « allume la hotte », « éteins la hotte », « mets la hotte à 66 % », « éteins les lumières de la cuisine » (seule la lampe), « éteins la hotte dans 15 minutes » | phrases retenues pour Majid ; arrêt différé |
| B6 | « Éteindre après » dans une automatisation | durées essayées, dont la plus longue proposée ; automatisation **déclenchée par la hotte elle-même** (« quand la hotte s'allume, l'éteindre après 15 min ») : réussite, ou contournement noté (accessoire factice, déclencheur horaire) | arrêt différé (Q11) |
| B7 | tuile regroupée contre accessoires séparés | ordres depuis chaque tuile | conseil d'affichage |
| B8 | deux nœuds 0xFFF1/0x8000 | la ScreenBar et le banc, commandés tour à tour pendant la durée du banc | cohabitation (Q12) |
| B9 | latences | `simu appui` : de l'appui simulé à l'affichage ; ordres : du toucher à l'état simulé | budget du §3.7, hors carte réelle |
| B10 | redémarrage du nœud | `reboot`, la hotte simulée gardée (`RTC_NOINIT`) : délai de retour des gestes dans Maison ; puis `simu apres_demarrage` : état changé pendant le redémarrage, resynchronisation | `maxint`, reprise des abonnements, borne de 75 s (Q36) ; §5.7 |
| B11 | endurance | au moins 3 jours (7 visés) : appuis simulés et ordres par une automatisation, tas minimal suivi. **Critère :** aucun redémarrage, `heap_min` stable sur les 48 dernières heures ; après un plantage : core dump lu, correction, puis 3 jours de plus | stabilité du code repris (risque 12) |
| B12 | puissance d'émission et rôle | `otPlatRadioSetTransmitPower`, relecture après l'attache et après un changement de rôle ; RSSI vu par un autre nœud ; `med 1` puis `med 0` : rôle réel, qualité de lien avec le parent (MED) ou le routeur voisin (routeur), compteurs MAC ; `otPlatRadioGetReceiveSensitivity()` (−120 attendus) ; carte posée hors de la hotte : référence des critères du §6.1 | Q13, Q14 |
| B13 | taille de l'image, tas libre | build, `sys` | Q22 |
| B14 | *facultatif :* MultiSpeed, SpeedMax = 3 | seulement s'il coûte presque rien | information pour une version suivante |
| B15 | alerte de température ; cohabitation du capteur et de l'ADC | seuil abaissé par l'USB sous la température du moment (`hotte regle temp_alerte_c 25`) : événement `alerte`, `log` émis sans `json log 1` ; retour sous le seuil moins l'hystérésis : fin d'alerte ; seuil remis. Lectures de température et des deux voies ADC, simultanées, comparées à des lectures isolées | §7.4 ; Q42 |
| B16 | coupure du module seul, hotte en marche | `simu apres_demarrage v2 lampe`, USB coupé 10 s, rebranché : départ en « éteinte » déduite, puis ordre V3 de Maison, dans chaque mode d'état simulé | jamais « marche » avant un arrêt lu (§5.4, règle 3) ; resynchronisation |

À la fin du banc, le nœud de test est retiré de Maison (sauf s'il sert à
l'endurance) et les résultats sont consignés dans `docs/BANC-MATTER.md`.

### 9.3 Mesures sans risque, hotte débranchée

**Radio : mesure M1.** Le protocole fait foi dans SPEC-RECONNAISSANCE.md, §6,
« Mesure M1 » (document des ajouts, bloc D1), règles de la reconnaissance
comprises : étape 2b refaite après l'ouverture si l'adaptateur est posé
(règle 7) ; jamais la hotte rebranchée avec la sonde ou sa batterie dans le
boîtier (règle 13). En résumé : RSSI Wi-Fi de la sonde, sur batterie, juste
sous la hotte (P0), puis dans le boîtier électronique à l'emplacement du module
(P1), couvercle fermé et filtre remis ; atténuation `A` ; marge
`M = R_T − A + 104` dB, où `R_T` est le RSSI moyen du parent Thread lu par la C6
du banc Matter **en MED** (`med 1` : en routeur, elle n'a pas de parent, §6.1),
et −104 dBm la sensibilité de la fiche du C6. **La mesure directe fait foi :**
la C6 du banc (la SuperMini libre, si Q23 le permet), posée elle-même en P1,
sur batterie, firmware `produit` du banc, en MED : `M = R_T(P1) + 104`. La
qualité de lien d'OpenThread n'entre pas dans le calcul (§6.1). La décision
suit le tableau du §6.1.

**Volume libre et points d'ancrage :** hotte débranchée 5 min, boîtier ouvert,
photos du quart haut-droit avec une **règle en plastique, posée sans toucher la
carte ni les zones interdites**, dans les trois directions ; distances aux fils
du CBB61 et au connecteur blanc ; nervures, fentes et colliers existants
utilisables sans perçage. Rejoint les lignes 5 et 6 de l'étape 0.

**Dimensions de la SuperMini violette :** pied à coulisse en plastique,
longueur, largeur, épaisseur composants compris, position de l'antenne et de
l'USB-C.

(Le relevé de température en cuisson est retiré par l'amendement du 28/09 au
soir.)

### 9.4 Essai d'alimentation de 24 h (après la 3b)

**Préalables :**
- `+` mesuré à la 3b, **entre 4,75 et 5,5 V dans toutes les situations**
  relevées ; sinon, la chaîne n'est pas montée et une nouvelle revue a lieu
  (§6.5) ;
- contrôles d'isolement de la reconnaissance faits (étapes 1, 2b, 3a) ;
  différentiel testé ;
- SECURITE.md, liste « Avant chaque mise sous tension », porte l'exception,
  ajoutée par le second plan avant l'essai : « `J2` ouvert, sauf étape 4 et
  essai de 24 h du produit (chaîne diode-fusible entre `TP+` et `5V`, aucun
  USB) » ;
- `essai alim on` posé par l'USB (il autorise `radio rafale` à distance) ; clé
  H1 posée.

**Montage :** le module du produit, sur sa plaque définitive (deux ponts de
mesure compris), posé **sur le boîtier de mesure extérieur** à la place de la
sonde. Il est alimenté par `TP+` à travers la chaîne définitive (1N5819,
**RXEF025** d'abord, 470 µF), montée pour l'essai entre `TP+` et la broche
`5V` ; la résistance haute du pont côté hotte est à `TP+`, et GPIO2 lit donc
`TP+`, GPIO3 le 470 µF. C'est une **exception voulue** à la règle de la
reconnaissance qui interdit d'alimenter la sonde par la hotte (`J2` ouvert) :
elle ne vaut que pour cet essai. Firmware `produit` avec le pilote simulé
(aucune injection). Tout raccordement hotte débranchée ; **aucun USB** sur le
module.

**Surveillance :** présence dans le logement pendant tout l'essai, sommeil
compris, avec un détecteur de fumée en service près de la cuisine. En cas
d'absence, hotte débranchée, puis reprise : les tranches cumulent 24 h. Des
heures sans surveillance ne sont possibles que si Majid les accepte, écrites
comme une exception explicite à la règle 12 (Q40).

**Déroulé :**
- hotte en **V3, lampe allumée**, pendant tout l'essai ;
- les 2 premières heures à **20 dBm** (pire cas) ; la suite à 20 dBm si le
  côté hotte tient ses critères, sinon à 12 dBm (émission réduite : §6.1) ;
- trafic : une session compagnon par Thread (`etat` toutes les 2 s,
  `json compteurs 10000`) et des ordres réguliers à la hotte simulée ;
- **touches testées pendant les émissions**, à l'heure 0, à l'heure 2 et à la
  fin : le Mac relance `radio rafale 30` toutes les 40 s
  pendant une série d'appuis qui laisse la hotte en V3, lampe allumée :
  1. lumière par paires (éteinte, puis rallumée) : 5 paires, soit 10 appuis ;
  2. V1, puis V2, puis V3, à 3 s d'intervalle au moins, 4 fois : jamais la
     vitesse active, qui arrêterait le moteur ;
  3. « marche » seulement depuis « armée » : V3 (le moteur s'arrête), arrêt du
     moteur attendu, « marche » (éteinte), « marche » (armée), V3 ; 5 fois,
     soit 10 appuis sur « marche ». Jamais « marche » moteur tournant : elle
     lancerait la marche prolongée, qui arrêterait la hotte 15 min plus tard
     pour le reste de l'essai ;
  4. état final vérifié : V3, lampe allumée ;
- **relevés** (`MESURES-PRODUIT.md`) : au début, après 1 h, puis au moins
  toutes les 4 h hors sommeil, et à la fin : état de la hotte vu au panneau
  (le module, sur le pilote simulé, ne lit pas la hotte), minimum des deux
  tensions, température de la puce et température de la pièce, redémarrages.

**Critères :**
- aucun redémarrage du module ;
- aucun comportement anormal de la hotte (claquements de relais, bips
  intempestifs, touche sans effet) ; **à chaque relevé et à la fin, la hotte est
  toujours en V3, lampe allumée** : un redémarrage d'IC2 la remettrait éteinte
  (étape −1, ligne 9) ;
- côté hotte (GPIO2) : minimum au moins 90 % de la tension au repos de la 3b,
  et jamais sous 4,5 V (seuils proposés, à confirmer avec la valeur de la 3b) ;
- côté module (GPIO3) : minimum jamais sous 3,7 V (§6.5) ;
- fusible jamais déclenché ;
- température de la puce relevée avec celle de la pièce : l'écart donne
  l'auto-échauffement du module, qui sert à revoir le seuil d'alerte (Q34).

**Choix du fusible et de la puissance :** règles du §6.5 et du §6.1. Les 2 h à
20 dBm se font d'abord avec le RXEF025. Côté module sous 3,7 V, ou module qui
redémarre, le côté hotte tenant : on refait l'essai avec le RXEF050, en notant
la perte de protection de la branche entre 0,5 et 1 A (ou, si Majid le
préfère, on garde le RXEF025 et on réduit l'émission : Q19). Côté hotte qui ne
tient pas : émission réduite, 12 puis 8 dBm ; échec encore : repli 12 V,
décision de Majid (Q24).

### 9.5 Validation sur la hotte (après l'étape 7 et l'installation)

Majid présent, à portée de la fiche, pendant toute la validation.

| # | Essai | Critère |
|---|---|---|
| V1 | chaque touche du panneau, **20 fois** (5 touches) | publication en 2 s au plus (journaux horodatés du module), 100 sur 100 ; au moins 5 appuis par touche filmés : affichage dans Maison en 2 s au plus ; C1 tenu |
| V2 | chaque ordre de Maison, **20 fois**, depuis un état stable (plus de `delai_moteur_ms` après le dernier changement du moteur) : allumer depuis l'arrêt ; allumer depuis « armée » ; chaque palier, moteur tournant ; éteindre depuis chaque palier ; lampe allumée et éteinte | voyants à jour (vidéo horodatée) en **5 s** depuis l'arrêt et **3 s** depuis « armée » (décision) ; **3 s** pour changer de palier et pour la lampe, **5 s** pour éteindre depuis un palier (complément proposé, Q35) ; jamais de marche prolongée déclenchée par un arrêt |
| V3 | appui physique en pleine séquence, 10 fois, à des étapes différentes | séquence annulée, état réel publié, voyants justes |
| V4 | redémarrage du module hotte en marche (V2 et lampe), 5 fois, par `reboot` à distance (Q27 ; repli si « non » : Q27) | aucun appui injecté ; état retrouvé ; Maison réalignée en **75 s au plus** après le redémarrage (Q36) |
| V5 | coupure de courant : fiche débranchée 10 s (5 fois), puis coupures brèves d'environ 0,5 s (5 fois, par l'interrupteur d'une multiprise) | après une coupure longue : « éteint, 0 % », lampe éteinte, sans piège de la NVS ; après une coupure brève : H-coupure confirmée ou infirmée (Q20), et jamais « marche » avant un arrêt lu |
| V6 | marche prolongée au panneau jusqu'à la fin des 15 min | Maison allumée au palier pendant, éteinte en 2 s après la fin |
| V7 | Siri et « Éteindre après », une fois chacun | arrêt complet : moteur arrêté, ⏻ et voyant de vitesse éteints, lampe inchangée (Q26) |
| V8 | essai du lien de 48 h, à la puissance retenue à l'essai de 24 h (§6.1) | critères du §6.1 tenus ; puissance et rôle notés |
| V9 | surveillance | aucune alerte de tension ni de température pendant la validation ; maximum de la puce relevé |

### 9.6 Semaine d'usage

Usage normal pendant 7 jours. Majid note chaque désaccord vu (heure, ce que
montrait Maison, ce que montrait le panneau). Le module journalise séquences,
échecs, annulations, redémarrages, tensions minimales, qualité de lien et
température de la puce (maximum depuis la pose). Critère : aucun désaccord hors
des fenêtres tolérées du §3.6, aucun redémarrage inexpliqué, aucune alerte de
température ; sinon, on en rediscute avec Majid. Une cuisson intensive
volontaire (tous les foyers, V3, 30 min, puis la marche prolongée), pour
relever le maximum de la puce dans le pire cas, n'est faite que si Majid la veut
(Q37) : l'amendement a retiré le relevé en cuisson.

---

## 10. Ordre des travaux et dépendances

### 10.1 Ordre

| # | Travail | Quand | Plan |
|---|---|---|---|
| 1 | ajouts à la reconnaissance (§10.2), dont la correction du repérage de WIRING.md §9 (bloc F1) | maintenant | avenant aux documents du sous-projet 1 |
| 2 | automate `hotte_etat` et ses tests | maintenant | **premier plan** |
| 3 | couche Matter, pilote simulé, banc Matter | maintenant | **premier plan** |
| 4 | profil compagnon produit, outils du Mac (`hotte_udp.py`) | maintenant | **premier plan** |
| 5 | mesures sans risque : radio (M1), volume, dimensions | maintenant, hotte débranchée ; la marge Thread après le point 3 (C6 du banc, en MED) | hors plan : Majid, protocole M1 et §9.3 |
| 6 | alimentation protégée, essai de 24 h | après la 3b | second plan |
| 7 | pilote A, B ou C ; injection 20 sur 20 (étape 7) | après l'avenant de l'étape 6 | second plan |
| 8 | boîtier imprimé | après les mesures du point 5 et le choix du point 7 | second plan |
| 9 | installation, essai du lien de 48 h à la puissance retenue (§6.1), validation du critère, semaine d'usage | après 6, 7 et 8 | second plan |

Le **second plan** (points 6 à 9) s'écrit après l'étape 6, avec l'avenant
d'injection, la table des transitions complétée et, s'il le faut, l'avenant de
la lecture annexe.

### 10.2 Ajouts à la reconnaissance (approuvés)

Le détail est dans le document des ajouts (blocs A1 à F1), à verser dans
SPEC-RECONNAISSANCE.md, RECONNAISSANCE.md, SECURITE.md et WIRING.md. Une fois
versé, c'est lui qui fait foi ; les noms ci-dessous sont les siens.
- **(a) Marche prolongée :** étape −1b (lignes 11 à 14 : fin, vitesse active,
  « marche », autre vitesse), puis scénarios 7b1 à 7b5 de l'étape 5
  (`prol-debut`, `prol-fin` sur 17 min en mode `changements`, `prol-vactive`,
  `prol-marche`, `prol-autre-v`) ; test 6 de l'étape 6 (la fin laisse-t-elle
  une trame sur `D` ?).
- **(b) « L'arrêt » de l'étape 7 défini :** vitesse active, attente de l'arrêt
  du moteur confirmé, puis « marche » (blocs B1 à B8). « Marche » appuyée moteur
  tournant déclencherait la marche prolongée.
- **(c) Repos sans appui :** 60 s moteur tournant (`repos-v2`, ligne 7c1) et
  60 s armée (`repos-arme`, ligne 7c2) ; test 5 de l'étape 6.
- **(d) Mesure radio comparative M1**, hotte débranchée. Le relevé de
  température est retiré par l'amendement du 28/09 au soir.
- **Correction liée, à confirmer par Majid (bloc F1) :** repérage de la
  SuperMini violette dans WIRING.md §9.

### 10.3 Dépendances envers la reconnaissance

| Sortie de la reconnaissance | Étape | Ce qu'elle fixe dans le produit |
|---|---|---|
| tension du `+` | 3b | chaîne vers la broche `5V` montée ou non (4,75 à 5,5 V, §6.5) ; pont de la mesure ; seuils de l'essai de 24 h |
| pull-up et résistance série de `D` | 2b, 3c | étages d'écoute et d'injection ; courant de `D` vers le module (§6.7, règle 9) |
| le panneau s'ouvre, ou est résiné | 0, ligne 5d | C possible, ou repli |
| qui pilote les voyants | 4, et test 4 de l'étape 6 | A ou B contre C ; repli |
| drain ouvert ou push-pull, codage, silences | 6 | A contre B ; décodeur ; étages |
| la carte émet son état ; le répète au repos | 5, 6, ajout (c) | source d'état ; mode ; `fraicheur_ms` ; lecture annexe ou non |
| une vitesse sans marche passe sur `D` | 5, scénario 6b | sonde de vitesse permise ou non |
| transitions de la marche prolongée ; trace de sa fin sur `D` | ajout (a), test 6 | table du §5.2 ; suites depuis prolongée ; lecture de la fin (§5.9) ; `prolongee_max_ms` |
| avenant d'injection | 6 | `delai_moteur_ms`, `entre_appuis_ms`, `confirmation_ms`, durées bornées |
| injection 20 sur 20, voyants qui suivent | 7 | feu vert du pilote réel |

---

## 11. Livrables et arborescence

```
hotte haier/
  platformio.ini                 envs sonde, generateur, produit (puis produit_simule)
  src/                           sonde et generateur (inchangés), modules purs partagés
  src/produit/                   automate, pilote(s), Matter, compagnon, alimentation, surveillance
  tools/tests/                   tests hôte : + automate, correspondance, pilote simulé, surveillance, profil produit
  tools/json_check.py            + profil produit (schéma de trame_d)
  tools/hotte_udp.py             + profil produit : clé d'après build, nom SRP ou IPv6, résumé
  docs/
    SPEC-PRODUIT.md              cette spec (et ses avenants)
    PROTOCOLE-JSON-PRODUIT.md    profil compagnon du produit, par différence
    BANC-MATTER.md               banc Matter : procédure et résultats (§9.2)
    MESURES-PRODUIT.md           radio (M1), volume, dimensions, essai de 24 h (tensions, température de la puce)
    INSTALLATION.md              pose, contrôles, exception à la règle 3, validation, semaine d'usage (second plan)
```

---

## 12. Risques

| # | Risque | Gravité | Parade |
|---|---|---|---|
| 1 | le panneau pilote ses voyants et il est résiné : pas de solution propre | bloquant | repli fixé : panneau de rechange à dérésiner (C) ; sinon arrêt du produit (Q21) |
| 2 | lien radio faible (acier, filtre métallique) : « Pas de réponse », C1 tombe | élevée | mesure M1 avant le dessin (§9.3), marge calculée sur la sensibilité de la fiche, pas sur la qualité de lien d'OpenThread ; MED et 48 h ; antenne déportée ; fils du module loin de l'antenne (§6.2) |
| 3 | marge d'alimentation : le 5 V chute, IC2 redémarre, relais qui claquent, touches déréglées | élevée | diode, fusible (RXEF025 d'abord), 470 µF, émission réduite, essai de 24 h (touches pendant les rafales, deux tensions mesurées, hotte toujours en V3) ; repli 12 V (Q24) |
| 4 | l'USB réalimente le rail 5 V de la hotte pendant un flash | moyenne | Schottky en série ; module débranché de la traversée pour flasher |
| 5 | un fil basse tension touche un conducteur secteur, ou le `+` la carcasse | grave | §6.7 : distances, deux fixations pour le module **et la traversée**, gaine, fil 105 °C, fusible ou résistance sur tout conducteur qui sort de la traversée ; contrôle couvercle ouvert, puis fermé |
| 6 | chaleur et graisse : boîtier qui flue, composants de la SuperMini sans température nominale | moyenne | ASA imposé, boîtier fermé ; température de la puce publiée, avec alerte (§7.4) ; pièces tenues à 85 °C au moins autour du module (fiches XH, RXEF), fil 105 °C |
| 7 | une mauvaise séquence déclenche la prolongation au lieu de l'arrêt ; bascule appuyée deux fois | moyenne | règle d'or à deux verrous, « marche » seulement après un arrêt **lu**, y compris après un redémarrage ; confirmation par mode ; nouvel essai seulement si l'échec est prouvé ; tests de propriété |
| 8 | le module redémarre hotte en marche et `D` ne répète pas l'état | moyenne | ajout (c) ; confiance `Presume` ; lecture annexe obligatoire dans ce cas (Q25) ; aucun « marche » avant un arrêt lu |
| 9 | fenêtres de désaccord (maintien de 10 s, réabonnement de 49 à 56 s sur benq avec le plafond de 20 s) | faible à moyenne | critère chiffré ; `maxint` 20 s ; borne de 75 s (Q36) ; tolérance du conflit (Q9) |
| 10 | piège de la NVS : « High à 0 % » après une coupure | moyenne | parade validée au banc (B4) |
| 11 | le module plante ou tient `D` basse | moyenne | RMT borné, y compris pendant l'écriture du core dump (Q29) ; chien de garde (`enableLoopWDT`) ; 10 kΩ de base ; couplage capacitif proposé ; relais de contournement si B |
| 12 | code repris instable (la sonde a planté une fois le 28/09, tas corrompu, cause inconnue) | moyenne | endurance B11 et son critère ; core dump ; le sous-projet 3 fiabilise le code commun |
| 13 | montée de version de la pile (FanControl en 1.6 ; §3.3 et §4.4 reposent sur le code d'esp_matter 1.5.1) | faible | plateforme figée (§2.4) |
| 14 | comportements de Maison inconnus (bouton de la tuile, statut 0xF0, rendu, Siri) | moyenne | banc Matter (§9.2) |
| 15 | « dernière vitesse » infaisable | faible | faisable si Maison écrit On (§3.3, règle 3) ; sinon V3 au bouton de la tuile (règle 4) |
| 16 | maintenance sans OTA ; clé et remise à zéro par l'USB | faible | accès sans outil ; procédure du §6.8 |
| 17 | garantie et conformité EN 60335-2-31 : appareil commandé à distance, « sans surveillance » (30.2) | acceptée par Majid | montage réversible ; panneau prioritaire (22.50) ; boîtier en ASA non ignifugé : tenue au feu par limitation d'énergie (tout ce qui entre dans le boîtier imprimé est en aval du fusible ou d'une résistance ; circuit de faible puissance, 19.11.1, 15 W au plus, exempté du 30.2.3) ; responsabilité acceptée par Majid |
| 18 | courant d'appel du 470 µF à la mise sous tension | faible | observé à l'essai de 24 h (Q19) |
| 19 | fusible déclenché sans signe visible : 0,45 W (RXEF025) ou 0,77 W (RXEF050) dissipés tant que la hotte reste branchée, près des fiches XH | faible à moyenne | corps à l'air, à 5 mm des fiches (§6.3) ; « Pas de réponse » durable : procédure du §6.8 |
| 20 | `+` de `CN3` hors de 4,75-5,5 V (ME6211 : 6,5 V au plus en entrée) | grave pour le module | rien sur la broche `5V` avant la 3b ; abaisseur dédié et nouvelle revue (§6.5, Q24) |

---

## 13. Questions ouvertes

Chacune dit quand et comment elle se tranche, et ce qu'elle décide. **Le 29/09, Majid a tranché** Q19 (préférence), Q23, Q26, Q27, Q34 à Q40, et retenu par défaut la proposition de Q41 : la décision est écrite dans la colonne « Quand, comment », et elle prime sur le mot « proposé » là où le texte y renvoie. Q15
(température en cuisson) et Q28 (choix du thermomètre) sont retirées par
l'amendement du 28/09 au soir ; les numéros ne sont pas réattribués.

| # | Question | Quand, comment | Ce que ça décide |
|---|---|---|---|
| Q1 | La carte émet-elle son état sur `D`, et le répète-t-elle au repos ? | étapes 5 et 6, ajout (c) : captures de 60 s (7c1, 7c2), `analyse.py` | source d'état (§5.1), mode, `fraicheur_ms`, lecture annexe (§6.4) |
| Q2 | Que fait la hotte à la fin des 15 min, sur « marche » ou sur une autre vitesse pendant la prolongation ? **La fin laisse-t-elle une trame sur `D` ?** | étape −1b (lignes 11 à 14) ; étape 5 (7b1 à 7b5) ; test 6 de l'étape 6 | table du §5.2, suites depuis prolongée (§5.3), lecture de la fin (§5.9), `prolongee_max_ms` |
| Q3 | A, B ou C ? | étape 6, règle du §4 de SPEC-RECONNAISSANCE.md | pilote réel, étages (§6.3), repli |
| Q4 | Tension du `+` de `CN3` ? | 3b, multimètre, dans toutes les situations (veille, marche, lampe, V1 à V3, appui) | chaîne vers la broche `5V` montée ou non (§6.5), pont, seuils de l'essai de 24 h |
| Q5 | Valeurs d'injection, délai moteur, attente entre appuis, délai de réponse de la carte, réaction des voyants ? | avenant de l'étape 6 ; réaction mesurée à l'étape 7 | paramètres du §4.2, budget du §3.7 |
| Q6 | Une vitesse appuyée sans marche passe-t-elle sur `D` (la carte l'ignore) ? | étape 5, scénario 6b | sonde de vitesse (§5.4, règle 7) |
| Q7 | Qu'écrit Maison au bouton de la tuile : `FanMode` = On, `PercentSetting` = 100, ou autre chose ? Quel statut reçoit-elle, et comment réagit-elle ? | banc Matter, B2 : écritures brutes ; substitution de On active, puis coupée (`matter derniere 0`) | règle 3 ou 4 du §3.3 |
| Q8 | Latence Maison → nœud, intervalle minimal des abonnements d'Apple, rendu du curseur, écart entre deux écritures d'un même geste (dont `FanMode` puis `PercentSetting`) ? | banc Matter, B1, B2 et B9 | budget du §3.7, `lissage_calme_ms`, `lissage_plafond_ms` |
| Q9 | Le maintien d'environ 10 s dans un conflit est-il accepté comme tolérance ? | durée mesurée au banc (B3), puis décision de Majid à la relecture du banc | ligne ajoutée ou non aux tolérances du §3.6 |
| Q10 | La parade du piège de la NVS marche-t-elle ? | banc Matter, B4 | §4.4 ; sinon réalignement par une valeur intermédiaire, retesté |
| Q11 | Formulations Siri en français, durée maximale de « Éteindre après » ? Une automatisation déclenchée par la hotte peut-elle l'éteindre après un délai (limite connue : pas sans accessoire factice) ? | banc Matter, B5 et B6 | mode d'emploi de l'arrêt différé, présenté à Majid avec les phrases Siri |
| Q12 | Deux nœuds 0xFFF1/0x8000 cohabitent-ils dans la même maison ? | banc Matter, B8 | identité gardée ; sinon PID de test distinct, avec son certificat, étudié avant la mise en service du module |
| Q13 | La puissance posée par `otPlatRadioSetTransmitPower` (`openthread/platform/radio.h`, l. 619 ; sous le verrou OpenThread) tient-elle après l'attache et après un changement de rôle ? Quelle est la valeur par défaut de la pile ? | au banc (B12) : relecture par `otPlatRadioGetTransmitPower`, déjà lue par benq (`PROTOCOLE-JSON.fr.md`, l. 684) | réglage `tx_dbm`, procédure du §6.1 |
| Q14 | Marge Thread à l'emplacement ? Sensibilité rapportée par la pile (−120 dBm attendus, `ESP_RECEIVE_SENSITIVITY`) ? | M1 (§9.3), après le point 3 ; `otPlatRadioGetReceiveSensitivity()` relu au banc (B12) | carte, rôle, puissance (§6.1) ; qualité de lien d'OpenThread écartée comme marge |
| Q16 | Volume libre et points d'ancrage sans perçage ? | photos avec une règle en plastique (§9.3, étape 0 lignes 5-6) | dessin du boîtier, fixations (module et traversée) |
| Q17 | Dimensions exactes de la SuperMini violette ? | pied à coulisse en plastique (§9.3) | dessin du boîtier |
| Q18 | VBUS relié directement à `5V` ? Référence du régulateur (ME6211C33 ?) ? Courant tolérable dans GPIO2 hors tension (50 µA au plus) ? | au banc, multimètre (contrôle du §9 de WIRING.md : 5,0 V direct, environ 4,7 V par diode) ; marquage du régulateur à la loupe ; fiche du C6 | rôle de la Schottky confirmé ; seuil de 3,7 V ; pont gardé ou déplacé |
| Q19 | Fusible : RXEF025 ou RXEF050, choisi à l'essai de 24 h ? Si seul le côté module faiblit, Majid préfère-t-il le RXEF050 (amendement : choix d'après la chute) ou garder le RXEF025 et réduire l'émission ? Courant d'appel du 470 µF ? Déclassement à chaud : à 85 °C (hypothèse de pire cas), la tenue du RXEF025 reste-t-elle au-dessus de la moyenne du module (75 à 85 mA) ? | **Préférence tranchée le 29/09 :** baisser d'abord l'émission et garder le RXEF025 ; le RXEF050 seulement en dernier recours, en écrivant la perte de protection. Le reste (courant d'appel, déclassement à chaud) se tranche à l'essai de 24 h et sur la fiche Littelfuse | fusible posé ; avec le RXEF050, branche non protégée entre 0,5 et 1 A |
| Q20 | H-coupure : une mise sous tension du module implique-t-elle une hotte éteinte ? | validation V5 (coupures brèves) | état initial `Deduit` gardé, ou traité comme `Presume` |
| Q21 | Un panneau de rechange est-il disponible ? | maintenant, recherche de la pièce (Haier, CDA, Liaoyuan) ; décisif seulement si l'étape 6 conclut « maître et résiné » | repli possible ou arrêt |
| Q22 | Taille de l'image et tas libre du produit ? | premier build de `produit` (premier plan) : **image de 2 486 660 octets sur 3 145 728 (79,0 %)**, build du premier plan avec le pilote simulé ; tas libre : banc Matter (B13) | marge de la partition ; fonctions à alléger si elle manque |
| Q23 | Quelle C6 pour le banc Matter ? La SuperMini libre réservée au module (puis `decommission` et effacement avant la mise en service définitive, §8.3 ; elle permet aussi la mesure directe de M1), une carte du projet libre (hors `7E:F0` et `A4:6C` ; la sonde `76:30` et le générateur `8E:78` servent aux bancs 16, 21 et 24), ou un achat ? | **Tranchée le 29/09 :** la SuperMini libre, future carte du module ; `decommission` et effacement avant la mise en service définitive (§8.3) ; elle sert aussi à la mesure directe de M1 | montage du banc ; mesure directe de M1 en P1 |
| Q24 | Où prendre l'alimentation si la chaîne ne peut pas aller sur la broche `5V` : `+` de `CN3` hors de 4,75-5,5 V (à 12 V, il est lui-même le 12 V permanent : abaisseur dédié sans soudure), ou essai de 24 h raté même à 8 dBm avec le RXEF050 ? | après la 3b ou après l'essai ; décision de Majid faits en main, nouvelle revue : abaisseur dédié sur le `+` de `CN3`, mesure cadrée de `CN2`, exception à la règle (souder un fil, hotte débranchée), ou arrêt | repli d'alimentation |
| Q25 | Lecture annexe nécessaire ? Comment, sans soudure et de façon durable ? | après l'étape 6 si Q1 est négative : avenant cadré avec Majid (`CN1`, IC3) | §6.4, second plan |
| Q26 | « Tout s'éteint » : la lampe reste-t-elle hors de l'arrêt du ventilateur ? | **Tranchée le 29/09 :** non, la lampe reste hors de l'arrêt du ventilateur (accessoire séparé ; pour tout couper : « éteins la hotte et sa lumière », ou une scène) | §3.3, V7 ; si non, « éteindre » ajoute la lampe |
| Q27 | `reboot` autorisé à distance (écart à la ScreenBar) ? Sinon, quel repli pour V4 : (a) redémarrages programmés par l'USB avant le montage (`essai reboot <n> <min>`, qui s'arrêtent seuls) ; (b) build de validation qui l'autorise, remplacé après V4 (une ouverture de plus) ? Une coupure du module seul, hotte débranchée, ne teste pas un redémarrage hotte en marche | **Tranchée le 29/09 :** `reboot` autorisé à distance, protégé par la clé H1 (écart assumé à la ScreenBar) | liste blanche du §7.5 ; sans « oui » ni repli, V4 n'a pas de méthode, et le critère de sortie 2 non plus |
| Q29 | Le RMT finit-il sa transaction au niveau de repos si le processeur plante, y compris pendant l'écriture du core dump, qui précède le redémarrage ? Faut-il le couplage capacitif de Q2 ? | au banc de la sonde (générateur et analyseur) : panique provoquée pendant une émission, core dump actif ; durée de `D` basse et délai avant le redémarrage, à l'analyseur ; décision au second plan | garde-fous 4 et 5 du §4.3 ; limite matérielle |
| Q30 | Vx sur Vx active, et marche moteur tournant, font-ils en V1 et V2 ce qu'ils font en V3 ? D'ici là, ces lignes sont codées comme observées (§5.2) | captures de l'étape 5, puis V1 et V2 de la validation | table du §5.2 |
| Q31 | Le degré de pollution 3 vaut-il encore dans l'édition en vigueur de la CEI 60335-2-31 ? (lu dans une reprise indienne de 2009) | on ne l'achète pas : la conception retient d'office le cas le plus sévère (PD3, 8 mm), qui couvre aussi le PD2 | aucun changement, quelle que soit la réponse |
| Q32 | Liste exacte des codes d'appairage interdits | à l'implémentation de l'identité (premier plan) : lue dans la validation de la pile (`SetupPayload` de CHIP) ; le code tiré est vérifié par un test | code d'appairage retenu |
| Q33 | Pull-up et résistance série de `D` (`R13` 4,7 kΩ, `R1` 1 kΩ) ? | étapes 2b et 3c | étages d'écoute et d'injection ; courant de `D` vers le module (§6.7, règle 9) |
| Q34 | Seuil d'alerte de la puce : 70 °C proposés, hystérésis de 5 °C ? Il reste sous les pièces les moins tenues autour du module : fiches XH et RXEF (85 °C), ASA (HDT de 86 °C à 1,8 MPa), UL1007 (80 °C) s'il restait (Q41) ; et loin de la limite de la puce (ESP32-C6FH4 : 105 °C ambiant, fiche v1.5). La puce lit plus chaud que l'air | **Tranchée le 29/09 :** 70 °C, hystérésis de 5 °C (retour à la normale sous 65 °C) ; revu après l'essai de 24 h | `temp_alerte_c` (§4.2) |
| Q35 | C3 complété : 3 s pour un ordre d'un appui, 5 s pour deux appuis ? Écart accepté : un ordre qui suit un changement du moteur attend `delai_moteur_ms` ? | **Tranchée le 29/09 :** oui, 3 s pour un ordre d'un appui, 5 s pour deux ; écart accepté après un changement du moteur (`delai_moteur_ms`) | §3.6 ; critères de V2 |
| Q36 | Borne ferme de la tolérance au redémarrage : 75 s pour « environ 1 min » ? | **Tranchée le 29/09 :** 75 s | §3.6, V4 |
| Q37 | Pendant la semaine d'usage, une cuisson intensive volontaire (tous les foyers, V3, 30 min, puis la marche prolongée), pour relever le maximum de la puce dans le pire cas ? Proposée seulement : l'amendement a retiré le relevé en cuisson | **Tranchée le 29/09 :** oui, une cuisson intensive volontaire pendant la semaine d'usage, maximum de la puce relevé | §9.6 |
| Q38 | Ordre égal à la vitesse pendant la prolongation : la laisser finir (proposé), ou l'annuler (Vx ⇒ armée ⇒ Vx) ? | **Tranchée le 29/09 :** la laisser finir | §5.5 ; test correspondant |
| Q39 | Réglage de la puissance après l'installation : `matter tx 8..20` autorisé à distance, ou une ouverture de la hotte à chaque changement ? | **Tranchée le 29/09 :** oui, `matter tx 8..20` autorisé à distance, protégé par la clé H1 | liste blanche du §7.5 ; procédure du §6.1 |
| Q40 | Essai de 24 h : des heures sans surveillance sont-elles acceptées (exception explicite à la règle 12) ? Un détecteur de fumée est-il en service près de la cuisine ? | **Tranchée le 29/09 :** oui, des heures sans surveillance sont acceptées, avec un détecteur de fumée en service près de la cuisine | surveillance de l'essai (§9.4) |
| Q41 | Fils des fiches XH précâblées en UL1007 (80 °C) : sertir du fil 105 °C, ou garder l'UL1007 avec un seuil d'alerte plus bas ? | **Proposition retenue par défaut le 29/09**, à confirmer à la réception (marquage lu) : garder l'UL1007 pour la reconnaissance, provisoire ; fil 105 °C pour la traversée du produit | §6.3 ; `temp_alerte_c` |
| Q42 | Le capteur de température et l'ADC en mode continu cohabitent-ils sur le C6 (même périphérique APB_SARADC) ? | banc Matter, B15 | sinon, la lecture de température suspend l'ADC continu quelques ms toutes les 10 s |
