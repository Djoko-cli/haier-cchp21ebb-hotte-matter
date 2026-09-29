# Partie 4 : banc Matter avec Majid

Plan : [2026-09-29-produit-matter.md](../2026-09-29-produit-matter.md) (contraintes globales, écarts à la spec).

**Toutes ces tâches sont [BANC avec Majid]** : exécutées par l'agent
principal, Majid présent, jamais par un sous-agent. Aucune liaison avec la
hotte : une SuperMini sur l'USB du Mac, la hotte est simulée. Chaque tâche
remplit sa part de `docs/BANC-MATTER.md`, puis la committe et la pousse.

### Tâche 12 : Banc Matter : préparer la carte et la mettre en service (P1 à P8)

**[BANC avec Majid]** — exécutée par l'agent principal avec Majid, jamais par un sous-agent.

Toute la suite du banc part de cette carte. Rien n'est relié à la hotte.

**Fichiers :**
- Modifier : `docs/BANC-MATTER.md`

- [ ] **Étape 1 : Relever les versions (P1)**

Majid relève la version d'iOS de son iPhone, de tvOS ou des HomePod qui
servent de routeurs de bordure, et de macOS. Les noter dans la ligne P1 de
`docs/BANC-MATTER.md`.

- [ ] **Étape 2 : Identifier la carte (P2)**

Majid branche la SuperMini libre sur l'USB du Mac. Trouver son port
(`ls /dev/cu.usbmodem*`), puis lire sa MAC sans rien écrire :

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --port <port> read_mac
```

**Arrêt si la MAC est `58:E6:C5:DD:7E:F0` (ScreenBar) ou `58:E6:C5:DD:A4:6C`
(sonde de maillage de benq)** : ce n'est pas la bonne carte. Noter la MAC en
P2.

- [ ] **Étape 3 : Effacer et flasher (P3)**

```bash
~/.platformio/penv/bin/pio run -e produit -t erase --upload-port <port>
~/.platformio/penv/bin/pio run -e produit -t upload --upload-port <port>
```

Attendu : `SUCCESS` deux fois. Le premier flash d'une image Thread se fait
toujours après un effacement (spec 6.8).

- [ ] **Étape 4 : Lire le démarrage (P4)**

```bash
~/.platformio/penv/bin/pio device monitor -p <port> -b 115200
```

Taper `info`, puis `matter`, `hotte`, `simu`. Attendu : `firmware 0.1.0-<commit>
(produit)`, `pilote simule`, `matter : pile demarree, pas encore mis en service`,
le code manuel et le QR, la hotte simulée `eteinte`, la confiance `confirme`
dès la première répétition. Fermer le moniteur (Ctrl-C).

- [ ] **Étape 5 : Poser la clé H1 (P5)**

Le port libre :

```bash
python3 tools/hotte_udp.py cle <port>
```

Attendu : `cle rangee dans ~/.config/hotte-produit/cle (empreinte XXXXXXXX)` :
l'appareil a été lu dans `hello` (`build` = `produit`). La clé n'est jamais
affichée.

- [ ] **Étape 6 : Garder la route Thread du Mac (P6)**

Majid vérifie que l'assistant de route de benq est installé :
`sudo launchctl print system/fr.djoko.halo.routes` (commande tapée par Majid :
un agent ne lance jamais `sudo`). Sinon, Majid l'installe :
`sudo sh ~/Documents/Dev/esp32/benq/tools/macos/halo-routes/installer.sh`
(voir son `README.fr.md`).

- [ ] **Étape 7 : Mettre en service dans Maison (P7)**

Majid, dans Maison : « Ajouter un accessoire », le code manuel donné par
`matter` (ou le QR), avertissement « non certifié » accepté. Puis :
accessoires **séparés**, noms « Hotte » et « Éclairage hotte », pièce
« Cuisine » (spec 3.5). Un agent ne tape jamais ce code.

- [ ] **Étape 8 : Joindre le nœud par Thread (P8)**

Par l'USB (moniteur ouvert) : `json etat`, puis relever dans la ligne
`reseau` `ip` le nom SRP (`srp.nom`) et l'adresse `omr`. Fermer le moniteur,
puis :

```bash
python3 tools/hotte_udp.py --appareil produit session <adresse omr> --duree 30
```

Attendu : `hello`, `config`, puis `etat/hotte`, `etat/thermique`, ... toutes
les 2 s. Cocher P1 à P8 dans `docs/BANC-MATTER.md` ; y noter le nom SRP (pas
d'adresse IP de la maison).

- [ ] **Étape 9 : Committer**

```bash
git add docs/BANC-MATTER.md
git commit -m "$(cat <<'FIN'
Banc Matter : carte preparee et mise en service

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 13 : Banc Matter : rendu, bouton de la tuile, affichage, latences (B1, B2, B7, B9, B13)

**[BANC avec Majid]** — exécutée par l'agent principal avec Majid, jamais par un sous-agent.

Ces essais disent comment Maison écrit dans le nœud : ils fixent le
regroupement d'EP1 (`lissage_calme_ms`, `lissage_plafond_ms`) et la règle
« dernière vitesse » (Q7, Q8).

**Fichiers :**
- Modifier : `docs/BANC-MATTER.md`

- [ ] **Étape 1 : Préparer**

**Avant de commencer :** Majid présent ; la carte du banc est la SuperMini
libre dont la MAC a été relevée à la tâche 12 (jamais `7E:F0`, jamais
`A4:6C`) ; aucune liaison avec la hotte ; le port série est libre (fermer
`pio device monitor` avant tout outil qui l'ouvre).

- [ ] **Étape 2 : B1 : rendu du ventilateur**

Moniteur ouvert. Captures d'écran de la tuile et de la fiche du
ventilateur, sur l'iPhone et sur le Mac. Un geste lent, puis rapide, sur le
curseur ; `matter journal` juste après chaque geste. Noter : curseur continu
ou crans, écart minimal et maximal entre deux écritures d'un même geste.

- [ ] **Étape 3 : B2 : bouton de la tuile**

`simu appui marche` (hotte simulée armée). Bouton de la tuile 5 fois
(substitution active par défaut) ; `matter journal`. Puis `simu appui v1`,
encore 5 fois. Puis `matter derniere 0`, et les deux séries de nouveau ;
`matter derniere 1` à la fin. Noter pour chaque série : écritures (valeurs,
ordre, écart entre `FanMode` et `PercentSetting`), statut vu par Maison (erreur
ou non), vitesse obtenue (`hotte`).

- [ ] **Étape 4 : B7 : tuile regroupée**

Majid regroupe les deux accessoires, donne des ordres depuis la tuile
regroupée (allumer, éteindre), puis les sépare de nouveau. Noter ce que fait
un ordre de la tuile regroupée.

- [ ] **Étape 5 : B9 : latences**

Vidéo de l'écran de Maison avec la console dans le même champ. Dix fois
`simu appui v2` puis `simu appui v2` (arrêt) : de la commande à l'affichage.
Dix ordres depuis Maison : du toucher à `[hotte] sequence ventilo : ok` sur la
console. Noter médiane et maximum.

- [ ] **Étape 6 : B13 : tas**

`json 1` puis lire `etat/sys` (`heap`, `heap_min`) après la mise en
service ; `json 0`. Noter à côté de la taille de l'image (tâche 10).

- [ ] **Étape 7 : Consigner**

Remplir B1, B2, B7, B9 et B13 dans `docs/BANC-MATTER.md` : valeurs
mesurées, captures décrites (pas d'image de l'écran avec des données de la
maison).

- [ ] **Étape 8 : Committer**

```bash
git add docs/BANC-MATTER.md
git commit -m "$(cat <<'FIN'
Banc Matter : rendu, bouton de la tuile, affichage et latences

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 14 : Banc Matter : conflits, redémarrages, coupures (B3, B4, B10, B16)

**[BANC avec Majid]** — exécutée par l'agent principal avec Majid, jamais par un sous-agent.

Ces essais vérifient la republication de l'état réel, la parade du piège
de la NVS et la règle d'or après un redémarrage (Q9, Q10, Q36 ; §5.7).

**Fichiers :**
- Modifier : `docs/BANC-MATTER.md`

- [ ] **Étape 1 : Préparer**

**Avant de commencer :** Majid présent ; la carte du banc est la SuperMini
libre dont la MAC a été relevée à la tâche 12 (jamais `7E:F0`, jamais
`A4:6C`) ; aucune liaison avec la hotte ; le port série est libre (fermer
`pio device monitor` avant tout outil qui l'ouvre).

- [ ] **Étape 2 : B3 : maintien de l'état demandé**

`simu appui marche`, puis `simu refuse 1`. Ordre V2 depuis Maison : la
séquence échoue (`[hotte] sequence ventilo : echec, non_confirme`) et l'état
réel est republié. Chronométrer de cette ligne jusqu'au retour de la tuile sur
« éteint ». Puis `simu refuse 0`, `simu hs 1`, un ordre : abandon (`pilote`) au
bout de 5 s ; `simu hs 0`. Noter les durées et l'affichage de
`PercentCurrent`.

- [ ] **Étape 3 : B4 : piège de la NVS**

`simu appui marche`, `simu appui v3`, `simu appui lumiere`. Majid
débranche l'USB 10 s, le rebranche (mise sous tension : la hotte simulée
revient éteinte). Noter l'état affiché par Maison au retour et le délai.
**Critère :** « éteint, 0 % », lampe éteinte ; jamais « High à 0 % » ni
« Extinction… ».

- [ ] **Étape 4 : B10 : redémarrage du nœud**

Hotte simulée en V2. `reboot` (5 fois) : noter le délai de retour des
gestes dans Maison. Puis `simu apres_demarrage v1` et `reboot` : l'état change
pendant le redémarrage ; noter la resynchronisation dans chaque mode
(`simu mode repete`, `simu mode changements` avec `simu annexe 1`,
`simu mode appuis` avec `simu annexe 1`). Remettre `simu mode repete`,
`simu annexe 0`.

- [ ] **Étape 5 : B16 : coupure du module seul, hotte en marche**

Dans chaque mode : `simu apres_demarrage v2 lampe`, Majid débranche
l'USB 10 s, le rebranche ; le module part en « éteinte » déduite, la hotte
simulée est en V2. Avant l'ordre : `json 1`, `json trames 1`. Ordre V3
depuis Maison. **Critère :** aucune `trame_d` `origine` `module` d'octet `A0`
(« marche ») avant un état lu moteur arrêté ; `hotte` dit `marche attend un
arret lu` tant qu'aucun arrêt n'est lu. Noter la resynchronisation (V3 direct
en mode répété ; abandon sans lecture annexe en mode changements).

- [ ] **Étape 6 : Consigner**

Remplir B3, B4, B10 et B16 dans `docs/BANC-MATTER.md`.

- [ ] **Étape 7 : Committer**

```bash
git add docs/BANC-MATTER.md
git commit -m "$(cat <<'FIN'
Banc Matter : conflits, redemarrages et coupures

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 15 : Banc Matter : Siri, automatisations, cohabitation (B5, B6, B8)

**[BANC avec Majid]** — exécutée par l'agent principal avec Majid, jamais par un sous-agent.

Mode d'emploi de l'arrêt différé et des phrases Siri, présenté ensuite à
Majid (Q11, Q12).

**Fichiers :**
- Modifier : `docs/BANC-MATTER.md`

- [ ] **Étape 1 : Préparer**

**Avant de commencer :** Majid présent ; la carte du banc est la SuperMini
libre dont la MAC a été relevée à la tâche 12 (jamais `7E:F0`, jamais
`A4:6C`) ; aucune liaison avec la hotte ; le port série est libre (fermer
`pio device monitor` avant tout outil qui l'ouvre).

- [ ] **Étape 2 : B5 : Siri**

Majid dit : « allume la hotte », « éteins la hotte », « mets la hotte à
66 % », « éteins les lumières de la cuisine », « éteins la hotte dans 15
minutes ». Noter l'effet de chacune ; vérifier que les lumières de la cuisine
ne touchent que la lampe, et « éteins la hotte » que le ventilateur.

- [ ] **Étape 3 : B6 : « Éteindre après »**

Majid crée une automatisation avec « Éteindre après » ; essayer
plusieurs durées, dont la plus longue proposée. Puis une automatisation
déclenchée par la hotte elle-même (« quand la hotte s'allume, l'éteindre
après 15 min »). Noter la durée maximale, la réussite ou le contournement.

- [ ] **Étape 4 : B8 : deux nœuds de test**

Pendant toute la durée du banc, la ScreenBar et le banc sont commandés
tour à tour. Noter toute confusion, tout « Pas de réponse ».

- [ ] **Étape 5 : Consigner**

Remplir B5, B6 et B8 dans `docs/BANC-MATTER.md`.

- [ ] **Étape 6 : Committer**

```bash
git add docs/BANC-MATTER.md
git commit -m "$(cat <<'FIN'
Banc Matter : Siri, automatisations et cohabitation

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 16 : Banc Matter : radio, température et ADC (B12, B15, B14 facultatif)

**[BANC avec Majid]** — exécutée par l'agent principal avec Majid, jamais par un sous-agent.

Puissance d'émission et rôle Thread (Q13, Q14) ; alerte de la puce et
cohabitation du capteur avec l'ADC continu (Q42).

**Fichiers :**
- Modifier : `docs/BANC-MATTER.md`

- [ ] **Étape 1 : Préparer**

**Avant de commencer :** Majid présent ; la carte du banc est la SuperMini
libre dont la MAC a été relevée à la tâche 12 (jamais `7E:F0`, jamais
`A4:6C`) ; aucune liaison avec la hotte ; le port série est libre (fermer
`pio device monitor` avant tout outil qui l'ouvre).

- [ ] **Étape 2 : B12 : puissance et rôle**

Session UDP avec `json compteurs 10000` :

```bash
python3 tools/hotte_udp.py --appareil produit session <adresse omr> --duree 60 "json compteurs 10000"
```

Relever `compteurs/radio` : rôle, `tx_dbm` (valeur de la pile), sensibilité
(−120 attendus). Par l'USB : `matter tx 20`, relire après l'attache ;
`matter med 1`, `reboot`, relire (rôle `child`, lien `parent`) ; `matter med 0`,
`reboot`, relire (rôle `router` après quelques minutes, lien `routeur`).

- [ ] **Étape 3 : B15 : alerte de température**

Par l'USB : `hotte regle temp_alerte_c 25` (sous la température du
moment) ; `json 1` : l'événement `alerte` (début) et un `log` arrivent sans
`json log 1`. `hotte regle temp_alerte_c 70` : fin d'alerte sous 65 °C.
Puis `alim arret`, dix lectures (`alim` toutes les 10 s) ; `alim reprise`,
dix lectures. Noter les deux séries.

- [ ] **Étape 4 : B14 : MultiSpeed (facultatif)**

Seulement si Majid le souhaite et s'il ne coûte presque rien ; sinon, noter « non essayé ».

- [ ] **Étape 5 : Consigner**

Remplir B12, B14 et B15 dans `docs/BANC-MATTER.md`.

- [ ] **Étape 6 : Committer**

```bash
git add docs/BANC-MATTER.md
git commit -m "$(cat <<'FIN'
Banc Matter : radio, temperature de la puce et ADC

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 17 : Banc Matter : endurance (B11)

**[BANC avec Majid]** — exécutée par l'agent principal avec Majid, jamais par un sous-agent.

Stabilité du code repris (risque 12 de la spec) : au moins 3 jours, 7
visés.

**Fichiers :**
- Modifier : `docs/BANC-MATTER.md`

- [ ] **Étape 1 : Préparer**

**Avant de commencer :** Majid présent ; la carte du banc est la SuperMini
libre dont la MAC a été relevée à la tâche 12 (jamais `7E:F0`, jamais
`A4:6C`) ; aucune liaison avec la hotte ; le port série est libre (fermer
`pio device monitor` avant tout outil qui l'ouvre).

- [ ] **Étape 2 : Lancer l'endurance**

Par l'USB : `simu auto 120` (un appui du panneau toutes les 2 min). Majid
crée une automatisation de Maison qui allume puis éteint le ventilateur
toutes les heures. Puis, le port libre :

```bash
python3 tools/serie_enregistre.py <port> endurance --duree 259200
```

(3 jours ; `tail -f logs/live.log` pour suivre).

- [ ] **Étape 3 : Suivre et juger**

**Critère :** aucun redémarrage (`hello` `reset_n`, `up_s`), `heap_min`
stable sur les 48 dernières heures. Après un plantage : lire le core dump
(`esptool read_flash 0x3f0000 0x10000`, puis `esp-coredump` de
`~/.espressif/python_env/idf5.5_py3.14_env` avec
`--rom-elf ~/.espressif/tools/esp-rom-elfs/20241011/esp32c6_rev0_rom.elf`),
corriger (nouvelle tâche, TDD), puis 3 jours de plus. À la fin :
`simu auto 0`.

- [ ] **Étape 4 : Consigner**

Remplir B11 dans `docs/BANC-MATTER.md` (durée, redémarrages, `heap_min` au début et à la fin).

- [ ] **Étape 5 : Committer**

```bash
git add docs/BANC-MATTER.md
git commit -m "$(cat <<'FIN'
Banc Matter : endurance

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 18 : Bilan du banc Matter et décisions

**[BANC avec Majid]** — exécutée par l'agent principal avec Majid, jamais par un sous-agent.

Fin du premier plan. Le second plan (points 6 à 9 : alimentation, pilote réel, boîtier, installation) s'écrit après l'étape 6.

**Fichiers :**
- Modifier : `docs/BANC-MATTER.md`
- Modifier : `docs/SPEC-PRODUIT.md`
- Modifier (si le banc l'impose) : `src/produit/hotte_etat.h, tools/tests/test_hotte_etat.cpp`

- [ ] **Étape 1 : Écrire le bilan**

Remplir la section 5 de `docs/BANC-MATTER.md` : réponses à Q7 à Q14, Q22
(tas), Q36, Q42 ; réglages retenus ; phrases Siri et mode d'emploi de l'arrêt
différé ; corrections faites pendant le banc.

- [ ] **Étape 2 : Reporter les réponses dans la spec**

Dans le tableau du §13 de `docs/SPEC-PRODUIT.md`, écrire la réponse
mesurée dans la colonne « Quand, comment » des questions tranchées par le banc
(Q7, Q8, Q10, Q11, Q12, Q13, Q22, Q36, Q42), avec la date. Q9 (tolérance du
conflit) : la durée mesurée, **la décision restant à Majid**.

- [ ] **Étape 3 : Changer les défauts si le banc l'impose**

Si B1, B2 ou B9 imposent d'autres valeurs de `lissage_calme_ms` ou de
`lissage_plafond_ms` : changer le défaut dans `struct Params`
(`src/produit/hotte_etat.h`) **et** l'attendu de `testParams`
(`tools/tests/test_hotte_etat.cpp`), puis `sh tools/tests/test_hote.sh` et le
build `produit`. Sinon, rien.

- [ ] **Étape 4 : Présenter à Majid**

Présenter le bilan : ce que Maison écrit au bouton de la tuile, le maintien
dans un conflit (Q9 : décision de Majid), les phrases Siri, l'arrêt différé,
la radio. Puis, sauf si l'endurance continue : `decommission` par l'USB (le
nœud sort de Maison, la clé H1 est effacée) et effacement de la carte
(`pio run -e produit -t erase`), qui redevient la SuperMini réservée au module
(spec 8.3).

- [ ] **Étape 5 : Committer**

```bash
git add docs/BANC-MATTER.md docs/SPEC-PRODUIT.md
git commit -m "$(cat <<'FIN'
Bilan du banc Matter : reponses aux questions de la spec

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

