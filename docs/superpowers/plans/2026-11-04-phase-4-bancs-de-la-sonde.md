# Phase 4 : bancs de la sonde

Feuille de route : [../specs/2026-11-04-feuille-de-route.md](../specs/2026-11-04-feuille-de-route.md).
Le détail de chaque banc est dans sa tâche d'origine (plans de la phase 1, où
elle est marquée « reportée en phase 4 ») ; les résultats vont dans
[BANC.md](../../BANC.md). Aucune liaison avec la hotte.

### Tâche 1 : Préparer le banc (FABRICATION §5.4 et §10)

- [x] Générateur prêt : nanoESP32-C6 (MUSE LAB) en attendant une SuperMini, barrettes soudées, firmware `generateur` flashé, console vérifiée (04/11)
- [x] Banc de la tâche 16 monté sur la breadboard (§10), contrôles hors tension conformes, plus 7 mesures de placement (05/11)

### Tâche 2 : Banc 16 : validation de la sonde par l'USB

Détail : [2026-09-27-phase-1e-banc.md](2026-09-27-phase-1e-banc.md), tâche 16.

- [ ] Critères 1 et 2, variante 1 (10k)
- [ ] Critères 1 et 2, variante 2 (4,7k + 1 nF) ; décalage retenu
- [ ] Critère 3 : rafale
- [ ] Critère 6 : consommation de la sonde
- [ ] Conclusions dans BANC.md

### Tâche 3 : Banc 16b : critère 2 à l'analyseur

Détail : [2026-09-27-phase-1e-banc.md](2026-09-27-phase-1e-banc.md), tâche 16b.

- [ ] Critère 2 à l'analyseur FX2, consigné dans BANC.md

### Tâche 4 : Banc 21 : par le Wi-Fi et sur la batterie

Détail : [2026-09-27-phase-1f-reseau.md](2026-09-27-phase-1f-reseau.md), tâche 21.

- [ ] Critère 4 : dix minutes de `krona` par le Wi-Fi
- [ ] Trente minutes sur la batterie
- [ ] Consommation, Wi-Fi actif

### Tâche 5 : Banc 24 : injection et collision

Détail : [2026-09-27-phase-1g-injection.md](2026-09-27-phase-1g-injection.md), tâche 24.

- [ ] Étage d'injection posé sur le boîtier (FABRICATION §6.7)
- [ ] Injection simple, relue par la voie 1
- [ ] Collision provoquée, et arrêt de l'injection
