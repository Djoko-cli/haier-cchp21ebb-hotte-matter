# Phase 3 : boîtier de mesure

Feuille de route : [../specs/2026-11-04-feuille-de-route.md](../specs/2026-11-04-feuille-de-route.md).
Guide de fabrication : [FABRICATION.md](../../FABRICATION.md), qui fait foi. Repères :
toujours les repères imprimés de la plaque, en disant de quelle face ils viennent
(les lettres sont en miroir côté soudures).

### Tâche 1 : Préparer la SuperMini de la sonde (FABRICATION §5.1 à §5.3)

- [x] Brochage réel relevé (§5.1)
- [x] Barrettes de la sonde soudées (§5.2)
- [x] Contrôle 17 du module : conforme (03/11, RECONNAISSANCE.md)

### Tâche 2 : Pièces du boîtier (FABRICATION §6.1 et §6.2)

- [x] Barrettes femelles du module
- [x] `X1`, picots mâles, `J2` mâle
- [x] `R1`, `R2`, `Q1`, support de `R_be`, contacts de `R3` ; `X1` à OL sur ses trois paires

### Tâche 3 : Fils nus et ponts (FABRICATION §6.3)

- [x] Groupe 1 : masse (le `−`, l'épine, la barre de l'écoute)
- [x] Groupe 2 : bus (`D_carte`, `D_panneau`, `+`)
- [x] Groupe 3 : écoute et broche 7
- [x] Pattes coupées ; contrôles a) et d)
- [x] `R_be` 100k enfichée ; contrôle e) : `TP_Dp` / `TP−` à 199,6 kΩ, les autres paires à OL (04/11)
- [x] Flux nettoyé à l'alcool isopropylique (04/11)

### Tâche 4 : Fils isolés (FABRICATION §6.4)

- [x] Rouge (`+`), contrôlé
- [x] Marron (GPIO6), contrôlé
- [x] Gris (3V3), contrôlé
- [x] Noir (GND), contrôlé
- [x] Rouge du 5V, côté composants, gaine sur C11, ponts U08-U07 et A11-A10 côté soudures, contrôlé (04/11)

### Tâche 5 : Contrôles complets et marquages (FABRICATION §6.5 et §9.2)

- [x] Lignes 7, 11, 12, 13 et 14 de WIRING §12 (04/11)
- [x] Contrôles a), b), c), e) et f) (04/11)
- [ ] Marquages au feutre ; cavalier de `J2` rangé hors de la plaque

### Tâche 6 : Fixation

- [x] Plateau dessiné : `cao/plateau-boitier.scad`, conception `docs/superpowers/specs/2026-11-04-plateau-boitier-design.md` (04/11)
- [x] Pièce d'essai imprimée en PETG : téton, jeu et clip conformes sur la carte, sans retouche (04/11)
- [x] Plateau imprimé en PETG, aux cotes réelles de la carte (04/11)
- [x] Coque USB-C isolée au ruban d'électricien PVC, en U, bouche libre (contrôle 16, 04/11)
- [x] Carte posée dans son plateau, module enfiché (04/11)
