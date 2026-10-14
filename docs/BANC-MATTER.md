# Banc Matter : procédure et résultats

Banc du point 3 de la spec du produit ([SPEC-PRODUIT.md](SPEC-PRODUIT.md),
§9.2) : le firmware `produit`, **pilote de ligne simulé**, sur une C6 posée sur
l'USB du Mac, mis en service dans la Maison de l'utilisateur. **Aucune liaison avec la
hotte.** Il tranche les questions de la spec qui ne dépendent que de Maison et
de la pile : Q7 à Q14, Q22, Q36, Q42.

## 1. Règles

- **Carte :** la SuperMini libre réservée au module (Q23, tranchée le 29/09).
  **Jamais** la ScreenBar (`MAC-SCREENBAR`) ni la sonde de maillage Thread
  de benq (`MAC-SONDE-BENQ`) : on relève la MAC par `info` avant tout
  flash, et l'on s'arrête si c'est l'une d'elles. Leurs vraies MAC sont dans
  `garde.local.txt` (local, ignoré par git), jamais dans le dépôt.
- Les codes d'appairage sont tirés une fois par `python3 tools/codes_appairage.py`
  dans `src/produit/codes_appairage.h`, fichier local ignoré par git (sans lui,
  la compilation s'arrête). Jamais changés ensuite, jamais commités.
- Le port série s'ouvre sans redémarrer le C6 : `pio device monitor` ou
  `tools/serie_enregistre.py` (DTR = RTS = 0 en un seul appel). Jamais un
  script pyserial qui pose DTR ou RTS avant l'ouverture.
- Ni mot de passe ni code d'appairage tapé par un agent : l'utilisateur met en service
  le nœud dans Maison lui-même.
- À la fin du banc, le nœud de test est retiré de Maison (`decommission`, puis
  effacement), sauf s'il sert encore à l'endurance (B11). **Avant la mise en
  service définitive du module, la carte est effacée** (spec §8.3).

## 2. Préparation

| # | Étape | Commande | Fait |
|---|---|---|---|
| P1 | versions relevées : iOS de l'iPhone, tvOS ou HomePod des routeurs de bordure, macOS (14/10 : iOS 27.0.1, HomePod 27.0, pas d'Apple TV, macOS 27.0.1) | réglages des appareils | ☑ 14/10 |
| P2 | MAC de la carte relevée par `info` (firmware quelconque), différente de `MAC-SCREENBAR` et `MAC-SONDE-BENQ` | `pio device monitor`, puis `info` | ☑ 14/10 |
| P3 | codes d'appairage tirés (une seule fois), effacement complet, puis premier flash | `python3 tools/codes_appairage.py`, puis `~/.platformio/penv/bin/pio run -e produit -t erase` puis `-t upload` | ☑ 14/10 |
| P4 | démarrage lu : version, pilote `simule`, codes d'appairage | `info`, `matter` | ☑ 14/10 |
| P5 | clé H1 posée par l'USB, rangée dans `~/.config/hotte-produit/cle` | `python3 tools/hotte_udp.py cle <port>` (port libre) | ☑ 14/10 |
| P6 | route Thread du Mac | assistant de benq, `tools/macos/halo-routes` (commit `c58a506`) | ☑ 14/10 |
| P7 | mise en service dans Maison par l'utilisateur (code manuel ou QR de `matter`), avertissement « non certifié » accepté ; accessoires séparés, noms « Hotte » et « Éclairage hotte », pièce « Cuisine » | Maison | ☑ 14/10 |
| P8 | nom SRP et adresse OMR relevés ; session UDP ouverte | `json etat` par l'USB (bloc `reseau` `ip`), puis `python3 tools/hotte_udp.py --appareil produit session <nom SRP ou IPv6> --duree 30` | ☑ 14/10 |

**Déroulé du 14/10/2026 :**
- **Carte :** le générateur du banc 0 (choix de l'utilisateur), libre puisque
  le banc 16 attend une pièce ; ni la ScreenBar ni la sonde de maillage de benq.
  **Avant le banc 16 :** `decommission`, effacement, puis reflash en
  `generateur`.
- **Rôle :** passé au FED par défaut avant la mise en service (`matter role`,
  spec §6.1, révision du 14/10).
- **Premier ajout dans Maison raté** (« ne répond pas », nœud resté non mis en
  service) ; le second, après `reboot` et suppression de l'accessoire fantôme,
  a réussi : mis en service, 2 abonnements actifs.
- **Session UDP par Thread :** ouverte (adresse OMR, nom SRP présent ; ni l'un
  ni l'autre ne sont notés ici). Puce à 59 °C au repos sur le banc (max 64 °C,
  seuil 70 °C) : à suivre en B15.

## 3. Commandes utiles (USB)

| Commande | Effet |
|---|---|
| `hotte` | état de l'automate, ordres, compteurs, réglages |
| `simu` | hotte simulée : état, mode, pannes |
| `simu appui marche\|lumiere\|v1\|v2\|v3` | appui du **panneau** simulé |
| `simu mode repete\|changements\|appuis`, `simu annexe 0\|1` | ce que porte la ligne `D` simulée |
| `simu refuse 1` | la hotte simulée ignore les appuis du module (B3) |
| `simu apres_demarrage v2 lampe` | état de la hotte simulée au prochain démarrage (B10, B16) |
| `simu auto <s>` | endurance : un appui du panneau toutes les s secondes (B11) |
| `matter journal` | écritures brutes de Maison, avec leur heure (B1, B2) |
| `matter derniere 0\|1` | coupe ou remet la substitution de On (B2) |
| `matter role 0\|1\|2`, `matter tx <8..20>` | rôle Thread au démarrage suivant (0 éligible routeur, 1 MED, 2 FED, le défaut), puissance (B12) |
| `alim`, `alim arret`, `alim reprise` | tensions et température ; ADC arrêté ou repris (B15) |
| `hotte regle temp_alerte_c <c>` | seuil d'alerte de la puce (B15) |
| `json 1`, `json log 1` | mode machine, messages `log` |

## 4. Essais

Chaque essai a ses relevés ; un essai raté est refait après correction, et la
correction est notée.

### B1. Rendu du ventilateur

- **Méthode :** captures d'écran de la tuile et de la fiche du ventilateur,
  iPhone et Mac ; un geste lent puis rapide sur le curseur ; `matter journal`
  juste après.
- **Relevés :** curseur continu ou crans : ______ ; écart entre deux écritures
  d'un même geste : ______ ms (min) à ______ ms (max).
- **Décide :** `lissage_calme_ms` (plus long que l'écart d'un geste) et
  `lissage_plafond_ms` (Q8).
- **Résultats du 14/10/2026** (iOS 27.0.1, firmware FED, vidéos d'écran
  gardées par l'utilisateur, hors dépôt) :
  - **Curseur continu**, en pourcentage. Maison n'écrit que `PercentSetting`
    (plus `FanMode 0` à 0 %).
  - **Geste lent :** une écriture toutes les 250 ms environ, écart de 216 ms
    (min) à 422 ms (max). **Geste rapide :** deux écritures, au toucher et à
    la fin, à environ 200 ms ; une pause du doigt peut en ajouter une.
  - **Avec `lissage_plafond_ms` = 3000,** un geste lent de plus de 3 s est
    exécuté en plusieurs fois (11 séquences pour un geste), et chaque reflet
    fait sauter le curseur sous le doigt. **Avec 10000,** une séquence par
    geste. `lissage_calme_ms` = 700 convient (plus long que 422 ms).
  - **Reflet après chaque séquence,** même sans changement d'état (ajouté le
    14/10) : avant, un geste resté dans le palier courant (88 vers 90 %, en V3)
    laissait la valeur du doigt publiée. Mesuré dans `matter journal` : le
    nœud republie 0,7 s après la dernière écriture (même palier) ou 0,9 s
    (changement de palier) ; Maison affiche le palier environ 2 s après le
    geste.
  - **Limite de Maison, acceptée par l'utilisateur :** dans le même palier,
    Maison garde la valeur du doigt affichée (75 % au lieu de 100 %), alors
    que le nœud publie bien 100 %. Ni un délai de reflet de 1,5 s, ni un
    rapport forcé à valeur inchangée n'y changent rien : Maison ne redessine
    le curseur que sur un vrai changement de valeur dans le nœud.
  - **Proposé pour le bilan :** `lissage_plafond_ms` = 10000 par défaut.

### B2. Bouton de la tuile

- **Méthode :** hotte simulée armée, puis en V1 ; bouton de la tuile 5 fois,
  avec `matter derniere 1`, puis 5 fois avec `matter derniere 0` ;
  `matter journal` après chaque série ; `json log 1` pour les séquences.
- **Relevés :** écritures (valeurs, ordre, écart entre `FanMode` et
  `PercentSetting`) : ______ ; statut vu par Maison et sa réaction (erreur,
  état affiché) avec et sans substitution : ______ ; vitesse obtenue : ______.
- **Décide :** « dernière vitesse » par la règle 3 ou 4 du §3.3 (Q7).
- **Résultats du 14/10/2026 :**
  - **Allumer** par la tuile : Maison écrit `PercentSetting 100`, **jamais
    `FanMode On`**, que la hotte ait été en V3 ou en V1 avant l'arrêt. La
    substitution de On (`matter derniere`) ne sert donc pas avec Maison ; la
    série `matter derniere 0` est sans objet.
  - **Éteindre :** `FanMode 0` et `PercentSetting 0` dans la même
    milliseconde ; la hotte passe à éteinte.
  - Aucune erreur affichée par Maison. Tuile dégroupée pour l'essai
    (l'utilisateur la gardera groupée : voir B7).
  - **Décision de l'utilisateur :** le bouton ramène la dernière vitesse
    (spec §3.3, règle 4 révisée). Règle : moteur à l'arrêt **et** fenêtre ne
    contenant que `PercentSetting 100` (aucune autre valeur, aucun FanMode).
    Moteur en marche, 100 % reste V3 ; un geste du curseur commence toujours
    par une petite valeur, donc reste lu tel quel. Limite : depuis l'arrêt, un
    toucher unique en haut du curseur se lit aussi comme le bouton.
  - **Vérifié :** depuis V1, éteindre puis rallumer mène directement en V1
    (aucun passage par V3) ; le nœud publie 33 % environ 1,5 s après.
    Maison affiche d'abord 100 % (son affichage provisoire), puis 33 %.

### B3. Maintien de l'état demandé

- **Méthode :** `simu refuse 1`, puis ordre V2 de Maison depuis l'arrêt ; la
  séquence échoue et l'état réel est republié (`[hotte] sequence ventilo :
  echec`). Chronomètre à partir de cette republication jusqu'au retour de la
  tuile sur l'état réel. Puis un cas d'abandon (`simu hs 1`, ordre, attente).
  `simu refuse 0`, `simu hs 0` à la fin.
- **Relevés :** durée du maintien : ______ s ; `PercentCurrent` affiché pendant
  et après : ______.
- **Décide :** la tolérance du conflit (Q9), présentée à l'utilisateur.

### B4. Piège de la NVS

- **Méthode :** hotte simulée en V3, lampe allumée (`simu appui marche`,
  `simu appui v3`, `simu appui lumiere`) ; USB débranché 10 s, rebranché : la
  hotte simulée revient éteinte (mise sous tension).
- **Relevés :** état affiché par Maison après le retour : ______ ; délai du
  retour : ______ s.
- **Critère :** « éteint, 0 % », lampe éteinte ; jamais « High à 0 % » ni
  « Extinction… » (Q10).

### B5. Siri en français

- **Méthode :** « allume la hotte », « éteins la hotte », « mets la hotte à
  66 % », « éteins les lumières de la cuisine », « éteins la hotte dans 15
  minutes ».
- **Relevés :** effet de chaque phrase : ______ ; « éteins les lumières de la
  cuisine » ne touche que la lampe : ☐ ; « éteins la hotte » ne touche que le
  ventilateur : ☐.

### B6. « Éteindre après » dans une automatisation

- **Méthode :** automatisation avec « Éteindre après », durées essayées dont
  la plus longue proposée ; puis une automatisation déclenchée par la hotte
  elle-même (« quand la hotte s'allume, l'éteindre après 15 min »).
- **Relevés :** durée maximale : ______ ; déclenchée par la hotte : réussite ☐,
  sinon contournement : ______ (Q11).

### B7. Tuile regroupée contre accessoires séparés

- **Méthode :** regrouper les deux accessoires, ordres depuis la tuile, puis
  les séparer de nouveau.
- **Relevés :** ce que fait un ordre de la tuile regroupée : ______.

### B8. Deux nœuds 0xFFF1/0x8000

- **Méthode :** la ScreenBar et le banc commandés tour à tour pendant la durée
  du banc.
- **Relevés :** confusion, « Pas de réponse », mauvais nœud : ______ (Q12).

### B9. Latences

- **Méthode :** vidéo de l'écran de Maison avec la console dans le même champ :
  `simu appui v2` jusqu'à l'affichage (10 fois) ; ordre de Maison jusqu'au
  `[hotte] sequence ventilo : ok` de la console (10 fois).
- **Relevés :** appui → Maison : ______ s (médiane), ______ s (max) ; Maison →
  nœud : ______ s.
- **Décide :** le budget du §3.7, hors carte réelle (Q8).

### B10. Redémarrage du nœud

- **Méthode :** hotte simulée en V2 ; `reboot` : la hotte simulée est gardée
  (mémoire RTC). Délai jusqu'au retour des gestes dans Maison. Puis
  `simu apres_demarrage v1` et `reboot` : l'état change pendant le
  redémarrage ; la resynchronisation suit, dans chaque mode simulé.
- **Relevés :** retour des gestes : ______ s (5 essais) ; resynchronisation :
  ______.
- **Décide :** `maxint`, la reprise des abonnements (le calendrier de benq
  seulement si le trou de benq revient), la borne de 75 s (Q36).

### B11. Endurance

- **Méthode :** au moins 3 jours (7 visés) : `simu auto 120`, et une
  automatisation de Maison qui allume et éteint le ventilateur toutes les
  heures ; `python3 tools/serie_enregistre.py <port> endurance --duree 259200`
  pour suivre `etat` `sys`.
- **Critère :** aucun redémarrage ; `heap_min` stable sur les 48 dernières
  heures. Après un plantage : core dump lu (`esptool read_flash 0x3f0000
  0x10000`, puis `esp-coredump`), correction, puis 3 jours de plus.
- **Relevés :** durée : ______ ; redémarrages : ______ ; `heap_min` au début et
  à la fin : ______.

### B12. Puissance d'émission et rôle

- **Méthode :** `json compteurs 10000` dans une session ; `matter tx 20`, puis
  relecture après l'attache et après un changement de rôle ; `matter role 1`
  (MED), `reboot`, `matter role 0` (éligible routeur), `reboot`, puis
  `matter role 2` (FED), `reboot` : rôle réel, qualité de lien, compteurs MAC ;
  sensibilité rapportée.
- **Relevés :** puissance par défaut de la pile : ______ dBm ; tenue après
  l'attache : ☐ ; rôle réel en `role 0` : ______ ; en `role 2` : ______ ; sensibilité : ______ dBm
  (−120 attendus) ; lien : ______ (Q13, Q14).

### B13. Taille de l'image et tas

- **Relevés :** image : 2 486 622 octets sur 3 145 728 (79,0 %, build du
  29/09) ; `heap` et `heap_min` après la mise en service : ______ (Q22).

### B14. MultiSpeed (facultatif)

Seulement s'il coûte presque rien : noté pour une version suivante.

- **Essai du 14/10/2026,** à la demande de l'utilisateur (trois vitesses,
  donc trois crans ?) : option de compilation `-DHOTTE_MULTISPEED=1`
  (`SpeedMax` = 3, environ 20 lignes ; la pile tient la correspondance
  vitesse et pourcentage), accessoire retiré puis rajouté dans Maison.
- **Résultat :** Maison garde un curseur continu et n'écrit toujours que
  `PercentSetting`. Aucun effet visible. Gardé pendant le banc pour B5
  (Siri se sert-il de la vitesse ?), décision au bilan ; défaut : désactivé.

### B15. Alerte de température ; capteur et ADC

- **Méthode :** `hotte regle temp_alerte_c 25` (sous la température du
  moment) : événement `alerte` et `log` émis sans `json log 1` ; retour sous le
  seuil moins l'hystérésis : fin d'alerte ; seuil remis à 70. Puis `alim arret`,
  dix lectures de température ; `alim reprise`, dix lectures.
- **Relevés :** alerte vue ☐, fin vue ☐ ; températures avec et sans ADC :
  ______ (Q42).

### B16. Coupure du module seul, hotte en marche

- **Méthode :** dans chaque mode simulé (`simu mode ...`, `simu annexe 1` pour
  `changements` et `appuis`) : `simu apres_demarrage v2 lampe`, USB débranché
  10 s, rebranché (départ en « éteinte » déduite, la hotte simulée en V2) ;
  ordre V3 de Maison.
- **Critère :** jamais « marche » avant un arrêt du moteur lu (règle 3 du
  §5.4) ; resynchronisation : ______.

## 5. Bilan

À remplir à la fin du banc, puis présenté à l'utilisateur : réponses à Q7 à Q14, Q22,
Q36, Q42 ; réglages retenus (`lissage_calme_ms`, `lissage_plafond_ms`,
`maxint`) ; phrases Siri et mode d'emploi de l'arrêt différé ; corrections
faites pendant le banc.
