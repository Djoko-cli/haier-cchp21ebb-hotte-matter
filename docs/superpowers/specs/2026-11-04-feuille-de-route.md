# Hotte Haier : feuille de route

Vue d'ensemble du projet, lisible par Jalons. Elle ne remplace aucune spec : le
détail reste dans [SPEC-RECONNAISSANCE.md](../../SPEC-RECONNAISSANCE.md)
(sous-projet 1) et [SPEC-PRODUIT.md](../../SPEC-PRODUIT.md) (sous-projet 2).

## Le projet

Piloter la hotte depuis Matter sur Thread, panneau d'origine conservé et voyants
synchronisés, par un ESP32-C6 branché sur la ligne de données entre le panneau
tactile et la carte de puissance. Trois sous-projets :

1. **Reconnaissance** : comprendre la ligne `D`, choisir l'architecture, prouver
   qu'on peut commander la hotte par cette ligne (phases 1 à 5).
2. **Produit** : firmware Matter, matériel définitif et pose (phases 2, 6 et 7).
   La logique et le firmware ont été écrits en avance, pendant que le matériel de
   la reconnaissance arrivait.
3. **Bibliothèque commune et app compagnon** multi-appareils (phase 8).

## Feuille de route

Dans l'ordre d'exécution. Une case se coche quand la phase est finie, banc compris.

- [x] Phase 1 : outils de la sonde (firmware, outils du Mac, procédures)
- [x] Phase 2 : produit, logique et firmware Matter (tâches 1 à 11)
- [ ] Phase 3 : boîtier de mesure
- [ ] Phase 4 : bancs de la sonde (tâches 16, 16b, 21, 24)
- [ ] Phase 5 : reconnaissance sur la hotte (étapes −1 à 7, mesure M1)
- [ ] Phase 6 : banc Matter (tâches 12 à 18)
- [ ] Phase 7 : produit, matériel et pose (second plan)
- [ ] Phase 8 : bibliothèque commune et app compagnon

## Les plans de chaque phase

Dans [../plans/](../plans/). Jalons rattache un plan à sa phase par `phase-N`
dans son nom.

| Phase | Plans | Remarque |
|---|---|---|
| 1 | `2026-09-27-phase-1a-terrain.md` à `2026-09-27-phase-1g-injection.md` | en-tête : [2026-09-27-reconnaissance-sonde/en-tete.md](../plans/2026-09-27-reconnaissance-sonde/en-tete.md). Les tâches de banc (16, 16b, 21, 24) y sont reportées en phase 4 |
| 2 | `2026-09-29-phase-2a-automate.md`, `2b-compagnon`, `2c-firmware` | en-tête : [2026-09-29-produit-matter/en-tete.md](../plans/2026-09-29-produit-matter/en-tete.md) |
| 3 | `2026-11-04-phase-3-boitier-de-mesure.md` | suit [FABRICATION.md](../../FABRICATION.md) §5 et §6 |
| 4 | `2026-11-04-phase-4-bancs-de-la-sonde.md` | renvoie aux tâches de banc des plans de la phase 1 |
| 5 | `2026-11-04-phase-5-reconnaissance-sur-la-hotte.md` | suit [RECONNAISSANCE.md](../../RECONNAISSANCE.md), le journal qui fait foi |
| 6 | `2026-09-29-phase-6-banc-matter.md` | en pause depuis la tâche 14 |
| 7 | à écrire | le second plan de SPEC-PRODUIT §10.1 (points 6 à 9), après l'étape 6 |
| 8 | à écrire | sa propre spec, plus tard |

## Tenir la feuille de route à jour

- Cocher une étape d'un plan dès qu'elle est faite et vérifiée, et la commiter
  avec le travail.
- Cocher la phase ci-dessus quand toutes ses étapes sont faites.
- Une étape renvoyée ailleurs se marque `- [-]`, avec sa raison.
