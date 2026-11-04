# Plateau du boîtier de mesure : conception

Phase 3 de la [feuille de route](2026-11-04-feuille-de-route.md), tâche 6. Remplace
les entretoises, les vis en nylon et la feuille isolante de
[FABRICATION.md](../../FABRICATION.md) §6.5.

## Pourquoi

Pendant les étapes sur la hotte, le boîtier de mesure est **posé sur le plan de
travail**, à côté de la hotte, hors de l'aplomb de la plaque de cuisson, au bout
d'un câble de sortie assez long (longueur fixée à l'étape 0). Il sert aussi aux
bancs, détaché de la hotte. Il faut :

- isoler la face soudures, où le `−` court en fil nu le long de deux bords ;
- que la carte ne bouge pas, et qu'une traction sur le câble XH ou sur le câble
  USB-C porte sur le plateau, jamais sur X1 ni sur le module ;
- pouvoir retirer la carte sans outil, pour les contrôles côté soudures.

**Hors sujet** : couvercle, logement de la batterie (elle reste à côté),
boîtier définitif du produit (ASA imposé, SPEC-PRODUIT §6).

## Approches

| | Approche | Pour | Contre |
|---|---|---|---|
| A | plateau à tétons (prévu par la fiche) | simple, compact | tétons Ø 1,8 fragiles ; bords et `−` nus exposés ; la carte peut sauter des tétons |
| **B** | **bac à rebord et clips** (choisie par Djoko le 04/11) | bords protégés ; rien ne se soulève ; tétons courts, pour centrer seulement | clips à régler (pièce d'essai) |
| C | cadre qui pince aussi le dessus | | n'apporte rien, gêne les grippe-fils sur les picots : écartée |

## Cotes de la carte

| Cote | Valeur | Source |
|---|---|---|
| carte | 69,5 × 49,5 mm | constaté : entraxes + 2 × 1,85 (mesures du 04/11) |
| trous de coin | Ø 2,0 mm, centre à 1,85 mm des deux bords | constaté, pied à coulisse |
| entraxes | 65,8 × 45,8 mm | constaté |
| relief côté soudures | 2,1 mm au plus | constaté |
| épaisseur | 1,6 mm | **supposé** (épaisseur courante) : la pièce d'essai le vérifie |

## Ce qui est fixé

- **Matière** : PETG. Il convient ici : le boîtier reste hors de la hotte.
- **Fond** : 2 mm. **Vide sous la carte** : 4,5 mm (relief 2,1 mm + jeu).
- **Quatre plots** de Ø 3,6 mm sous les trous de coin, en plastique ; aucun métal
  aux coins. **Tétons** de Ø 1,7 mm et 1,2 mm de haut, moins que l'épaisseur de la
  carte : ils centrent, ils ne tiennent pas.
- **Rebord** de 2 mm d'épaisseur, 0,3 mm de jeu autour de la carte, son sommet
  0,8 mm au-dessus de la face composants. Assez bas pour que la fiche XH
  (verticale) et le câble USB-C du module passent par-dessus : pas de découpe.
- **Deux clips**, au milieu des deux grands côtés : une languette de 12 mm
  détachée du rebord par deux fentes et amincie à 1,2 mm, une lèvre de 0,6 mm
  au-dessus du bord de la carte, chanfreinée pour l'entrée. Avec 1 mm de lèvre, la
  languette plierait d'environ 4 %, près de la limite du PETG imprimé, couches en
  travers : 0,6 mm la ramène vers 3 % (calcul, à confirmer par la pièce d'essai). On retire la carte en écartant les deux
  languettes.
- **Deux pattes à collier de serrage**, à l'extérieur du rebord : une à gauche,
  en face de X1, pour le câble XH ; une en haut à droite, côté USB-C du module.
- **Dessous** : plat. Pas de patins adhésifs sous la main (04/11) : les logements
  existent dans le fichier (Ø 10,5 × 0,8 mm), désactivés par défaut, à activer
  s'il en achète. Le collier du câble XH retient le plateau.
- **Impression** : fond à plat, sans support (les lèvres sont chanfreinées à 45°).

## Laissé au fabricant

- Le fichier OpenSCAD paramétrique : `cao/plateau-boitier.scad`, committé. Les
  STL et 3MF exportés ne sont pas committés (`cao/sortie/`, ignoré par git).
- La **pièce d'essai** : un coin avec son plot, son téton et un tronçon de
  rebord, plus un clip. À imprimer avant le plateau, pour régler le téton, le jeu
  et la force du clip sur la vraie carte.

## Risques

| Risque | Statut | Parade |
|---|---|---|
| téton trop serré ou cassant à Ø 1,7 | supposé | pièce d'essai ; paramètre dans le fichier |
| épaisseur de la carte différente de 1,6 mm | supposé | pièce d'essai ; paramètre de la lèvre des clips |
| clip trop raide en PETG | supposé | pièce d'essai ; longueur des fentes en paramètre |
