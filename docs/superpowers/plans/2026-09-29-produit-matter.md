# Produit Matter de la hotte : premier plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** livrer les points 2 à 4 du §10 de la spec du produit : l'automate
`hotte_etat` et ses tests sur le Mac, la couche Matter avec le pilote de ligne
simulé et le banc Matter sur la SuperMini libre, le profil compagnon du build
`produit` et les outils du Mac.

**Architecture:** toute la logique qui décide est pure et testée sur le Mac :
l'automate (état, confiance, table, suites d'appuis, règle d'or), la hotte
simulée derrière l'interface du pilote de ligne, la correspondance Matter et le
regroupement des écritures, la surveillance, les messages du protocole. Le
firmware (`src/produit/`, env `produit`) les relie à la pile Matter (recettes
de benq : boîte d'intentions, reflets sous `TryLockChipStack`), à la console
USB et au transport UDP sur Thread (copié de benq). Le pilote réel (A, B ou C)
viendra au second plan, après l'étape 6.

**Tech Stack:** PlatformIO + pioarduino `55.03.312-1` (Arduino-ESP32 3.3.12,
ESP-IDF 5.5.5, esp_matter 1.5.1), ESP32-C6 SuperMini violette ; C++17 ; tests
sur le Mac par clang++ ; Python 3.11 (bibliothèque standard), `unittest`.

**Spec:** [docs/SPEC-PRODUIT.md](../../SPEC-PRODUIT.md), à lire avec ce plan
(§3 à §5 pour l'automate et Matter, §7 pour le compagnon, §9.1 et §9.2 pour la
vérification). Références : benq au commit `c58a506`
(`/Users/Majid/Documents/Dev/esp32/benq`, lecture seule), la sonde au commit
`1cb2c7c` (`src/json_out.*`, `src/json_mode.cpp`, `src/cli.cpp`).

**Comment ce plan a été vérifié (29/09) :** tout le code a d'abord été écrit
et testé dans une copie de travail ; puis les tâches 1 à 11 ont été exécutées
telles qu'écrites, à blanc, dans un clone neuf du dépôt (fichiers, scripts,
commandes, commits) : chaque « Attendu » ci-dessous est la sortie réellement
obtenue. Seules les tâches du banc (12 à 18) n'ont pas été jouées.

## Écarts à la spec (assumés)

1. **Build simulé :** `-DPILOTE_SIMULE=1` et `-DPILOTE_LIGNE_TEXTE='"simule"'`
   au lieu de `-DPILOTE_LIGNE=SIMULE` (le préprocesseur ne compare pas des
   chaînes). Le build avec un pilote réel refuse de compiler (`#error`).
2. **Découpage :** `palier()`, `pourcent()` et les regroupements d'EP1 et d'EP2
   sont dans `hotte_map` (avec la boîte d'intentions), pas dans `hotte_etat`,
   qui ne connaît pas Matter. Tests en plus : `test_sequences.cpp` (bout en
   bout, avec `banc_hotte.h`) et `test_json_check_produit.py`.
3. **Interface de l'automate :** les ordres portent leur `Canal` (Matter ou
   app) ; `configurerLigne(mode, annexe)` ; `Action` porte aussi `Changement`
   (événement `hotte` du compagnon) et `Evenement` (journal).
4. **Appuis seuls, « marche » sans effet :** le voyant marche ne se lit pas
   dans ce mode ; si l'écho de notre trame est vu, la séquence finit `Ok` avec
   le voyant `Deduit` (C1 tient : moteur et lampe sont lus). L'`Echec` du
   tableau du §9.1 ne vient que si notre trame n'a pas d'écho.
5. **État répété, nouvel essai :** « sans anomalie de réception » est lu comme
   « l'état répété qui tranche est postérieur à toute anomalie » (c'est ce que
   demande le cas imposé « lampe, trame illisible »).
6. **Déduction démentie :** une vitesse appuyée depuis « armée » déduite, qui
   ne prend pas deux fois, rend le voyant marche inconnu (événement
   `deduction_dementie`). Absent de la spec ; prudent.
7. **Surveillance :** réglage de plus, `alim_alertes` (0 par défaut : pas
   d'alerte de tension tant que le pont n'est pas monté ; au banc, GPIO2 et
   GPIO3 sont en l'air) ; les réglages de la surveillance sont sous la clé NVS
   `params_s` (`params` : l'automate).
8. **Températures publiées en degrés entiers** (le protocole n'a que des
   entiers).
9. **Pilote :** l'interface gagne `trame()` (trames décodées pour `trame_d`),
   `mode()`, `annexe()`, l'événement `Service` (pilote hors service, puis
   revenu) et `ligne::resultatAdmission`.
10. **Commandes de plus :** `simu auto <s>` (endurance B11), `thermique raz`
    (maximum depuis la pose), `matter journal` (écritures brutes, B2),
    `alim arret|reprise` (B15).
11. **`radio rafale` :** des datagrammes pleins non authentifiés vers
    l'adresse de la session (l'app les rejette) ; compteur `rafale_tx`.
12. **Essai de 24 h :** le drapeau NVS `essai` compte les minutes restantes
    (72 h de fonctionnement cumulé), écrit une fois par heure.
13. **`hotte_udp.py` :** option globale `--appareil sonde|produit` ; `cle` lit
    l'appareil dans `hello` par l'USB.
14. **Démarrage sans état connu (`Inconnu`) :** rien de nouveau n'est publié,
    mais le premier reflet aligne les pourcentages sur le `FanMode` restauré
    par la pile (le piège de la NVS reste paré).
15. **Abandon pour durée :** `sequence_max_ms` finit en `Abandon` de cause
    `duree` ; le compteur `echecs.duree` reste donc à 0.

## Global Constraints

- **Plateforme** (celle de benq, figée, spec 2.4) : `platform =
  https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312-1/platform-espressif32.zip`,
  `board = esp32-c6-devkitc-1`, flash de 4 Mo ; env `produit` avec
  `board_build.partitions = huge_app.csv` ; pas d'OTA.
- **Rôle Thread :** les quatre lignes vont ensemble : `-DMATTER_NET_THREAD=1`,
  `-DMATTER_THREAD_MED=0`, `-DHALO_WRAP_THREAD_DEVTYPE=1` et l'option
  `-Wl,--wrap=` de `_SetThreadDeviceType` (spec 4.7).
- **Broches :** GPIO1 (injection) à l'état bas en **toute première
  instruction** de `setup()` ; le build simulé ne pilote aucune broche ; GPIO6
  et GPIO7 évitées (antenne) ; GPIO4, 5, 8, 9 et 15 sont des broches de
  démarrage ; la LED d'IO8 est mise au noir.
- **Règle d'or :** jamais « marche » sans un arrêt du moteur **lu** (fil ou
  lecture annexe) postérieur au démarrage et au dernier appui ; deux verrous
  indépendants (automate, pilote).
- **Identité :** `Djoko-CLI`, `Module hotte Haier`, `Hotte`, série `HOTTE-` +
  MAC, VID/PID de test 0xFFF1/0x8000 ; discriminateur **0xFA1**, code
  d'appairage **45924130**, jamais changés après la première mise en service.
- **NVS :** espace `produit`. **Compagnon :** protocole v1, `rev` 4, lignes de
  1 024 octets au plus (budget 896), UDP 5480 sur Thread, clé H1 en NVS
  `produit/cle`, côté Mac `~/.config/hotte-produit/cle`, jamais affichée.
- **Code** : identifiants, commentaires, textes de la console et du protocole
  en français **sans accents** ; documentation en français **avec accents**.
- **Copies :** tout fichier copié de benq ou de la sonde porte en tête son
  origine et le commit ; la sonde n'est jamais modifiée par ce plan.
- **Tests :** `sh tools/tests/test_hote.sh` et
  `python3 -m unittest discover -s tools/tests -p 'test_*.py'`, depuis la
  racine ; un test qui échoue arrête la tâche.
- **Build :** `~/.platformio/penv/bin/pio run -e produit`, `-e sonde` et
  `-e generateur` passent, sans avertissement nouveau dans `src/`.
- **Commits :** un par tâche, message en français sans accents, à
  l'impératif, terminé par une ligne vide puis
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. **Chaque commit est poussé tout de suite**
  (`git push`, règle de Majid) ; jamais de force-push.
- **Interdits pour les agents :** flasher, ouvrir un port série, `pio device
  monitor`, `sudo`, toucher au dépôt de benq (lecture seule), à la ScreenBar
  (`58:E6:C5:DD:7E:F0`) ou à la sonde de maillage de benq
  (`58:E6:C5:DD:A4:6C`), taper un code d'appairage ou un mot de passe. Les
  tâches **[BANC avec Majid]** sont exécutées par l'agent principal avec
  Majid, jamais par un sous-agent.

## Préambule : branche de travail

```bash
git checkout main && git pull
git checkout -b produit-matter
git push -u origin produit-matter
```

Les tâches 1 à 11 se font sur cette branche. Après la tâche 11 :
`superpowers:finishing-a-development-branch` (fusion en avance rapide dans
`main`, avec l'accord de Majid). Les tâches du banc (12 à 18) se font ensuite
sur `main`.

## Carte des fichiers

| Fichier | Rôle | Tâche |
|---|---|---|
| `src/produit/hotte_etat.{h,cpp}` | automate : types, table, réglages, ordres, confiance, séquences, règle d'or | 1, 2 |
| `src/produit/pilote_ligne.h` | interface du pilote de ligne (§4.3) | 3 |
| `src/produit/pilote_simule.{h,cpp}` | hotte modèle et pilote simulé, pannes à la demande | 3 |
| `src/produit/hotte_map.{h,cpp}` | correspondance Matter, regroupements d'EP1 et d'EP2, écritures NVS différées | 5 |
| `src/produit/surveillance.{h,cpp}` | seuils de la température de la puce et des tensions | 6 |
| `src/produit/json_out_produit.{h,cpp}` | briques pures du protocole, messages du produit, liste blanche | 7 |
| `src/produit/config_produit.h` | broches, identité, codes d'appairage | 10 |
| `src/produit/produit.h`, `main_produit.cpp` | cœur de l'application, `setup()` et `loop()` | 10 |
| `src/produit/reglages_produit.{h,cpp}` | NVS du produit | 10 |
| `src/produit/alim.{h,cpp}` | ADC continu des deux voies, température de la puce | 10 |
| `src/produit/matter_hotte.{h,cpp}` | EP1 Ventilateur, EP2 Lumière, boîte d'intentions, reflets, identité, Thread | 10 |
| `src/produit/net_udp_thread.{h,cpp}` | transport UDP sur Thread (copie de benq) et rafale | 10 |
| `src/produit/json_mode_produit.{h,cpp}` | sessions, instantanés, événements (copie de la sonde) | 10 |
| `src/produit/cli_produit.{h,cpp}` | console USB et lignes du réseau | 10 |
| `platformio.ini` | env `produit` ; la sonde écarte `src/produit/` | 10 |
| `tools/tests/test_hotte_etat.cpp`, `test_pilote_simule.cpp`, `banc_hotte.h`, `test_sequences.cpp`, `test_hotte_map.cpp`, `test_surveillance.cpp`, `test_json_produit.cpp` | tests sur le Mac | 1 à 7 |
| `tools/tests/test_hote.sh` | tous les tests C++ du Mac | 1, 3 à 8, 10 |
| `tools/json_check.py`, `tools/tests/test_json_check_produit.py` | profil produit des lignes machine | 8 |
| `tools/hotte_udp.py`, `tools/tests/test_hotte_udp.py` | clé d'après l'appareil, résumé du produit | 9 |
| `docs/PROTOCOLE-JSON-PRODUIT.md` | profil compagnon du produit, par différence | 8 |
| `docs/BANC-MATTER.md` | banc Matter : procédure et résultats | 11 à 18 |
| `README.md`, `docs/SPEC-PRODUIT.md` | présentation du module ; Q22 et réponses du banc | 11, 18 |

## Tâches

### [Partie 1 : logique pure (automate, hotte simulée, correspondance, surveillance)](2026-09-29-produit-matter/1-automate.md)

- Tâche 1 : Automate, partie 1 : types, table des transitions, réglages
- Tâche 2 : Automate, partie 2 : ordres, confiance, séquences, règle d'or
- Tâche 3 : Pilote de ligne et hotte simulée
- Tâche 4 : Tests de bout en bout de l'automate
- Tâche 5 : Correspondance Matter et boîte d'intentions
- Tâche 6 : Surveillance : température de la puce et tensions

### [Partie 2 : protocole compagnon du produit et outils du Mac](2026-09-29-produit-matter/2-compagnon.md)

- Tâche 7 : Briques du protocole compagnon du produit
- Tâche 8 : Profil produit de `json_check.py` et document du protocole
- Tâche 9 : `hotte_udp.py` : clé du produit, nom SRP, résumé

### [Partie 3 : firmware du produit et documents](2026-09-29-produit-matter/3-firmware.md)

- Tâche 10 : Firmware du produit : environnement `produit`, Matter, compagnon, build
- Tâche 11 : Documents : procédure du banc Matter, README, taille de l'image

### [Partie 4 : banc Matter avec Majid](2026-09-29-produit-matter/4-banc.md)

- Tâche 12 : Banc Matter : préparer la carte et la mettre en service (P1 à P8) **[BANC avec Majid]**
- Tâche 13 : Banc Matter : rendu, bouton de la tuile, affichage, latences (B1, B2, B7, B9, B13) **[BANC avec Majid]**
- Tâche 14 : Banc Matter : conflits, redémarrages, coupures (B3, B4, B10, B16) **[BANC avec Majid]**
- Tâche 15 : Banc Matter : Siri, automatisations, cohabitation (B5, B6, B8) **[BANC avec Majid]**
- Tâche 16 : Banc Matter : radio, température et ADC (B12, B15, B14 facultatif) **[BANC avec Majid]**
- Tâche 17 : Banc Matter : endurance (B11) **[BANC avec Majid]**
- Tâche 18 : Bilan du banc Matter et décisions **[BANC avec Majid]**

