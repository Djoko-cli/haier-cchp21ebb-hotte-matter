# Banc Matter : procédure et résultats

Banc du point 3 de la spec du produit ([SPEC-PRODUIT.md](SPEC-PRODUIT.md),
§9.2) : le firmware `produit`, **pilote de ligne simulé**, sur une C6 posée sur
l'USB du Mac, mis en service dans la Maison de Majid. **Aucune liaison avec la
hotte.** Il tranche les questions de la spec qui ne dépendent que de Maison et
de la pile : Q7 à Q14, Q22, Q36, Q42.

## 1. Règles

- **Carte :** la SuperMini libre réservée au module (Q23, tranchée le 29/09).
  **Jamais** la ScreenBar (`58:E6:C5:DD:7E:F0`) ni la sonde de maillage Thread
  de benq (`58:E6:C5:DD:A4:6C`) : on relève la MAC par `info` avant tout
  flash, et l'on s'arrête si c'est l'une d'elles.
- Le port série s'ouvre sans redémarrer le C6 : `pio device monitor` ou
  `tools/serie_enregistre.py` (DTR = RTS = 0 en un seul appel). Jamais un
  script pyserial qui pose DTR ou RTS avant l'ouverture.
- Ni mot de passe ni code d'appairage tapé par un agent : Majid met en service
  le nœud dans Maison lui-même.
- À la fin du banc, le nœud de test est retiré de Maison (`decommission`, puis
  effacement), sauf s'il sert encore à l'endurance (B11). **Avant la mise en
  service définitive du module, la carte est effacée** (spec §8.3).

## 2. Préparation

| # | Étape | Commande | Fait |
|---|---|---|---|
| P1 | versions relevées : iOS de l'iPhone, tvOS ou HomePod des routeurs de bordure, macOS | réglages des appareils | ☐ |
| P2 | MAC de la carte relevée par `info` (firmware quelconque), différente de `7E:F0` et `A4:6C` | `pio device monitor`, puis `info` | ☐ |
| P3 | effacement complet, puis premier flash | `~/.platformio/penv/bin/pio run -e produit -t erase` puis `-t upload` | ☐ |
| P4 | démarrage lu : version, pilote `simule`, codes d'appairage | `info`, `matter` | ☐ |
| P5 | clé H1 posée par l'USB, rangée dans `~/.config/hotte-produit/cle` | `python3 tools/hotte_udp.py cle <port>` (port libre) | ☐ |
| P6 | route Thread du Mac | assistant de benq, `tools/macos/halo-routes` (commit `c58a506`) | ☐ |
| P7 | mise en service dans Maison par Majid (code manuel ou QR de `matter`), avertissement « non certifié » accepté ; accessoires séparés, noms « Hotte » et « Éclairage hotte », pièce « Cuisine » | Maison | ☐ |
| P8 | nom SRP et adresse OMR relevés ; session UDP ouverte | `json etat` par l'USB (bloc `reseau` `ip`), puis `python3 tools/hotte_udp.py --appareil produit session <nom SRP ou IPv6> --duree 30` | ☐ |

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
| `matter med 0\|1`, `matter tx <8..20>` | rôle Thread (au démarrage suivant), puissance (B12) |
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

### B2. Bouton de la tuile

- **Méthode :** hotte simulée armée, puis en V1 ; bouton de la tuile 5 fois,
  avec `matter derniere 1`, puis 5 fois avec `matter derniere 0` ;
  `matter journal` après chaque série ; `json log 1` pour les séquences.
- **Relevés :** écritures (valeurs, ordre, écart entre `FanMode` et
  `PercentSetting`) : ______ ; statut vu par Maison et sa réaction (erreur,
  état affiché) avec et sans substitution : ______ ; vitesse obtenue : ______.
- **Décide :** « dernière vitesse » par la règle 3 ou 4 du §3.3 (Q7).

### B3. Maintien de l'état demandé

- **Méthode :** `simu refuse 1`, puis ordre V2 de Maison depuis l'arrêt ; la
  séquence échoue et l'état réel est republié (`[hotte] sequence ventilo :
  echec`). Chronomètre à partir de cette republication jusqu'au retour de la
  tuile sur l'état réel. Puis un cas d'abandon (`simu hs 1`, ordre, attente).
  `simu refuse 0`, `simu hs 0` à la fin.
- **Relevés :** durée du maintien : ______ s ; `PercentCurrent` affiché pendant
  et après : ______.
- **Décide :** la tolérance du conflit (Q9), présentée à Majid.

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
  relecture après l'attache et après un changement de rôle ; `matter med 1`,
  `reboot`, puis `matter med 0`, `reboot` : rôle réel, qualité de lien,
  compteurs MAC ; sensibilité rapportée.
- **Relevés :** puissance par défaut de la pile : ______ dBm ; tenue après
  l'attache : ☐ ; rôle réel en `med 0` : ______ ; sensibilité : ______ dBm
  (−120 attendus) ; lien : ______ (Q13, Q14).

### B13. Taille de l'image et tas

- **Relevés :** image : 2 486 622 octets sur 3 145 728 (79,0 %, build du
  29/09) ; `heap` et `heap_min` après la mise en service : ______ (Q22).

### B14. MultiSpeed (facultatif)

Seulement s'il coûte presque rien : noté pour une version suivante.

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

À remplir à la fin du banc, puis présenté à Majid : réponses à Q7 à Q14, Q22,
Q36, Q42 ; réglages retenus (`lissage_calme_ms`, `lissage_plafond_ms`,
`maxint`) ; phrases Siri et mode d'emploi de l'arrêt différé ; corrections
faites pendant le banc.
