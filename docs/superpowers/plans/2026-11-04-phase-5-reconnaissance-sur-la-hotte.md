# Phase 5 : reconnaissance sur la hotte

Feuille de route : [../specs/2026-11-04-feuille-de-route.md](../specs/2026-11-04-feuille-de-route.md).
Le journal qui fait foi est [RECONNAISSANCE.md](../../RECONNAISSANCE.md) ; les
étapes sont décrites dans [SPEC-RECONNAISSANCE.md](../../SPEC-RECONNAISSANCE.md) §6,
et la sécurité dans [SECURITE.md](../../SECURITE.md). Jamais de 230 V touché sous
tension : hotte débranchée, et 5 minutes d'attente avant d'ouvrir.

### Tâche 1 : Étapes −1 et −1b : observation d'usage

- [x] Étape −1 : tableau des comportements rempli (27/09)
- [ ] Étape −1b : lignes 11 à 14 de la marche prolongée

### Tâche 2 : Étape 0 : photos, inventaire et cheminements

- [x] Connectique de `CN3` : XH (28/09)
- [ ] Longueurs relevées
- [ ] Le panneau s'ouvre, ou il est résiné
- [ ] Cheminement choisi, avec une photo

### Tâche 3 : Étape 1 : test d'isolation

- [x] Feu vert pour construire l'adaptateur (28/09)

### Tâche 4 : Câble de sortie et adaptateur (FABRICATION §7, §8 et §9.1)

- [ ] Câble de sortie
- [ ] Adaptateur
- [ ] Contrôles avant la première mise sous tension (WIRING §12, lignes 1 à 10)

### Tâche 5 : Étape 2 : adaptateur et boîtier en place

- [ ] Étape 2b : isolation revérifiée depuis les points de test
- [ ] Étape 2c : essai à vide
- [ ] Photos du montage dans le journal

### Tâche 6 : Étape 3 : mesures au multimètre

- [ ] 3a : fuite en charge, avant tout Mac ou analyseur
- [ ] 3b : tensions
- [ ] 3c : test du pull-up

### Tâche 7 : Étape 4 : panneau seul, hotte débranchée

- [ ] On sait qui pilote les voyants

### Tâche 8 : Étape 5 : capture en place

- [ ] Préalable : critère 4 du banc 21 refait, dix minutes de `krona` par le Wi-Fi
  ([BANC.md §8.4](../../BANC.md#84-dix-minutes-de-krona-critère-4)), quand 2 min de
  ping vers la sonde sont propres (venu de la phase 4, tâche 4 ; premier essai et
  coupures au §7 de BANC.md, « Critère 4 : Wi-Fi (tâche 21) »)
- [ ] Scénarios capturés, dont la marche prolongée et le repos sans appui

### Tâche 9 : Étape 5b : validation croisée à l'analyseur

- [ ] Captures de la sonde et de l'analyseur concordantes

### Tâche 10 : Étape 6 : décodage et choix de l'architecture

- [ ] Décodage et tests de l'étape 6
- [ ] Architecture choisie : A, B ou C
- [ ] Avenant d'injection écrit

### Tâche 11 : Étape 7 : premier essai d'injection

- [ ] Préalable : étage d'injection soudé sur le boîtier
  ([FABRICATION.md §6.7](../../FABRICATION.md#67-plus-tard--étage-dinjection-étape-7-ou-banc-critère-5)), quand les contacts seront arrivés
  (venu de la phase 4, tâche 5 ; banc 24 fait le 10/11 avec l'étage sur la plaque
  d'essai, BANC.md §7, « Montage du 10/11 »)
- [ ] Injection 20 sur 20, voyants qui suivent

### Tâche 12 : Mesure M1 pour le produit : radio comparative, hotte débranchée

- [ ] Relevé M1 consigné
