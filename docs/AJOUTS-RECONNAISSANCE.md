# Ajouts à la reconnaissance (a) à (d) et correction F1, prêts à insérer

**Versé le 29/09/2026** dans SPEC-RECONNAISSANCE.md, RECONNAISSANCE.md et
SECURITE.md : ce sont eux qui font foi désormais. F1 était déjà appliqué dans
WIRING.md ; D4 est retiré par l'amendement.

28/09/2026, revu le 28/09 au soir. Ajouts approuvés par Majid : (a) marche
prolongée, (b) arrêt de l'étape 7, (c) repos de 60 s, (d) mesure sans risque.
**L'amendement de Majid du 28/09 au soir réduit (d) à la mesure radio
comparative M1** : le relevé de température en cuisson (l'ancienne M2) et
l'achat du thermomètre sont retirés. S'y ajoute une correction factuelle liée,
confirmée par Majid le 29/09 : le repérage de la SuperMini violette dans WIRING.md
(F1).

## 0. Mode d'emploi

- **Fichiers visés :** `docs/SPEC-RECONNAISSANCE.md` (« SPEC »),
  `docs/RECONNAISSANCE.md` (« JOURNAL »), `docs/SECURITE.md` (« FICHE ») et
  `docs/WIRING.md` (« WIRING »).
- **Numéros de ligne :** ceux du commit `efdece4`. Chaque bloc cite aussi sa
  ligne d'ancrage mot pour mot : c'est elle qui fait foi.
- **Ordre d'application :** dans chaque fichier, du bas vers le haut, pour que
  les numéros restent justes. Sinon, on se fie aux ancres citées. Quand deux blocs
  visent le même endroit, l'ordre est indiqué (« juste après le bloc X »).
- **« Insérer après »** ajoute le texte sans rien retirer. **« Remplacer »**
  retire les lignes citées et met le texte donné à leur place.
- Les numéros des nouvelles lignes suivent l'usage du journal (`6b`, `8a`,
  `a1`). L'étape 5 garde ses numéros 8 et 9.
- **SPEC fait foi** : chaque règle nouvelle y figure, et la FICHE la reprend.

| Bloc | Fichier | Endroit |
|---|---|---|
| A1 | SPEC | §6, nouvelle étape −1b, après la ligne 269 |
| A2 | JOURNAL | nouvelle étape −1b, après la ligne 113 |
| A3 | SPEC | §6, étape 5 : ligne 470 remplacée, puis scénario 7b après la ligne 477 |
| A4 | JOURNAL | étape 5 : ligne 429 remplacée, lignes 7b1 à 7b5 après la ligne 446 |
| A5 | SPEC | §6, étape 6 : test 6, juste après le bloc C3 |
| A6 | JOURNAL | étape 6 : ligne 9 du tableau, juste après le bloc C4 |
| A7 | SPEC | §1, « Hors périmètre », ligne 65 remplacée |
| B1 à B4 | SPEC | §1 (lignes 52-55), étape 5 (ligne 476), étape 6 (après la ligne 539), étape 7 (ligne 562) |
| B5 à B8 | JOURNAL | étape 5 (ligne 444), avenant (après la ligne 567), étape 7 (après les lignes 612 et 621), critères de sortie (lignes 652-654) |
| C1 | SPEC | §6, étape 5 : scénario 7c, juste après le bloc A3 |
| C2 | JOURNAL | étape 5 : lignes 7c1 et 7c2, juste après le bloc A4 |
| C3 | SPEC | §6, étape 6 : test 5, après la ligne 528 |
| C4 | JOURNAL | étape 6 : ligne 8 du tableau, après la ligne 525 |
| AC | JOURNAL | étape 5 : commandes des lignes 7b et 7c, après la ligne 449 |
| D1 | SPEC | §6, mesure M1, après la ligne 578 |
| D2 | SPEC | §5, règles 3 (après la ligne 175) et 13 (après la ligne 230) |
| D3 | SPEC | §1, « Hors périmètre », ligne 62 remplacée |
| D5 | JOURNAL | « Installation et instruments », après la ligne 33 |
| D6 | JOURNAL | nouvelle section M1, après la ligne 643 |
| D7 | FICHE | règles 3 (après la ligne 35) et 13 (après la ligne 86), liste avant mise sous tension (après la ligne 111) |
| E1 à E3 | SPEC | statut (après la ligne 8), §13 (après la ligne 1041), §14 (après la ligne 1052) |
| F1 | WIRING | **appliqué le 29/09**, sous une forme plus complète : nouveau §9 de WIRING.md |

L'amendement du 28/09 au soir retire le bloc D4 (achat du thermomètre) et la
partie M2 des blocs D1, D2, D5, D6 et D7, dont l'ajout au test du différentiel.
Les autres numéros ne changent pas.

---

## (a) Marche prolongée

Ce que l'on sait déjà (JOURNAL, étape −1, ligne 3) : en V3, « marche » moteur
tournant lance 15 min de marche prolongée, avec ⏻ qui clignote à 1 Hz ; la
vitesse active l'interrompt (retour à l'état armé). Ce qui manque : sa fin
(état final, bip, voyant, trame), « marche » et une autre vitesse pendant la
prolongation, et les trames de toutes ces transitions.

Deux temps :
- **étape −1b** (A1, A2) : observation sans montage, faisable tout de suite.
  L'automate `hotte_etat` (point 2 de l'ordre de Majid) a besoin de ces
  transitions avant l'étape 5 ;
- **étape 5, scénario 7b** (A3, A4) : les trames des mêmes transitions.

### A1. SPEC, §6 : nouvelle étape −1b

**Insérer après la ligne 269 :** `**Sortie :** le tableau des comportements est rempli dans `RECONNAISSANCE.md`.`
(avant la ligne 271, `### Étape 0 : photos, inventaire et cheminements`) :

````markdown

### Étape −1b : marche prolongée (ajout du 28/09)

- **État de la hotte :** en usage normal, fermée. L'adaptateur peut être en
  place, `J1` fermé.
- **Outils :** un téléphone qui filme le panneau, avec le son ; un chronomètre.

Elle complète les points 3 et 4 de l'étape −1. **Départ de chaque essai :**
hotte éteinte, puis « marche », V2, et, 10 s après, « marche » moteur tournant :
la marche prolongée commence (⏻ clignote). Relever :

11. **Sa fin**, lampe allumée avant le départ, sans rien toucher : au bout de
    combien de temps ? Quel état final (moteur, ⏻, voyant de vitesse, lampe) ?
    Un bip ?
12. **Une touche 5 s après le début de la prolongation**, une par essai : la
    vitesse active (vue le 27/09 : retour à l'état armé), « marche », puis une
    autre vitesse (V3).

Entre deux essais, on ramène la hotte à l'état éteint : vitesse active, moteur
arrêté, puis « marche ».

**Sortie :** les transitions de la marche prolongée sont notées dans
`RECONNAISSANCE.md`. L'automate du sous-projet 2 les reprend, et l'étape 5 en
capture les trames (scénario 7b).
````

### A2. JOURNAL : nouvelle étape −1b

**Insérer après la ligne 113 :** `- [x] le tableau des comportements est rempli (27/09 : vidéo pour les lignes 2, 3, 4, 6 et 7, réponses de Majid pour 1, 5, 8 et 9 ; ligne 10 : 3 s par défaut).`
(avant la ligne 115, `## Étape 0 : photos, inventaire et cheminements`) :

````markdown

## Étape −1b : marche prolongée (ajout du 28/09)

Complète les lignes 3 et 4 de l'étape −1 : les transitions de la marche
prolongée que la vidéo du 27/09 n'a pas montrées. L'étape 5 (lignes 7b1 à 7b5)
en capturera les trames.

**Date :**

**Conditions :** hotte en usage normal, fermée (l'adaptateur peut être en place,
`J1` fermé) ; un téléphone filme le panneau, avec le son ; chronomètre. On ne
touche qu'aux touches du panneau.

**Départ de chaque ligne :** hotte éteinte (⏻ éteint) ; « marche », V2, 10 s
d'attente ; puis « marche » moteur tournant : la prolongation commence (⏻
clignote), et l'on lance le chronomètre. **Retour à l'état éteint entre deux
lignes :** vitesse active, moteur arrêté (⏻ fixe), puis « marche ».

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 11 | fin de la prolongation, **lampe allumée avant le départ**, sans rien toucher ; vidéo de 14 min 30 à 16 min après l'appui | chronomètre ; vidéo | fin vers 15 min (ligne 3) ; état final inconnu | fin à ____ min ____ s ; moteur : ____ ; ⏻ : ☐ éteint ☐ fixe ☐ clignote ; voyant de vitesse : ____ ; lampe : ☐ allumée ☐ éteinte ; bip à la fin : ☐ oui ☐ non | transition « prolongée → fin » de l'automate. Pas de fin à 16 min : on filme jusqu'à la fin, 30 min au plus après l'appui, puis on éteint au panneau et on le note |
| 12 | vitesse active (V2), 5 s après le début de la prolongation | vidéo | état armé : moteur arrêté, ⏻ fixe (vidéo du 27/09, 31,2 s) | | confirme la ligne 3 |
| 13 | « marche », 5 s après le début de la prolongation | vidéo | inconnu : éteinte (sur la CDA, un 2e appui arrête : brief §1.2, ligne 25), état armé, prolongation relancée, ou rien | moteur : ____ ; ⏻ : ____ ; bip : ☐ oui ☐ non | transition de l'automate |
| 14 | autre vitesse (V3), 5 s après le début de la prolongation | vidéo | inconnu : V3 en marche normale (⏻ fixe), prolongation poursuivie en V3 (⏻ clignote), ou rien | moteur : ____ ; ⏻ : ____ ; bip : ☐ oui ☐ non | transition de l'automate |

**Sortie :**
- [ ] les lignes 11 à 14 sont remplies ; l'automate du sous-projet 2 reprend ces transitions.
````

### A3. SPEC, §6, étape 5 : scénario 7b

**1) Remplacer la ligne 470 :**

```
Scénarios, 10 s chacun, dans cet ordre :
```

**par :**

```
Scénarios, 10 s chacun sauf 7b et 7c, dans cet ordre :
```

**2) Insérer après la ligne 477 :** `7. chaque bip entendu ;`
(avant la ligne 478, `8. panneau déconnecté. …`). Les lignes vides comptent :
la liste reprend ensuite à 8.

````markdown

Ajouts du 28/09, entre les scénarios 7 et 8 (détail et commandes :
`RECONNAISSANCE.md`, étape 5, lignes 7b et 7c) :
- **7b. Marche prolongée.** Son début (« marche » moteur tournant, en V2). Sa
  fin, dans un enregistrement de **17 min** en mode `changements`, lampe
  allumée, sans aucun appui : heure de la fin, état final, bip, ⏻, trame.
  Puis, dans trois prolongations, une touche 5 s après leur début : la vitesse
  active, « marche », une autre vitesse. Les effets attendus sont ceux de
  l'étape −1b.
````

(Le bloc C1 vient juste après, puis une ligne vide, puis la ligne `8. panneau déconnecté. …`.)

### A4. JOURNAL, étape 5 : lignes 7b1 à 7b5

**1) Remplacer la ligne 429 :**

```
Un fichier par scénario, 10 s chacun, dans cet ordre :
```

**par :**

```
Un fichier par scénario, 10 s chacun sauf 7b2, 7c1 et 7c2 (commandes sous le
tableau), dans cet ordre :
```

**2) Insérer après la ligne 446 :** `| 7 | chaque bip entendu (`bip-<touche>`) | — | idem | | |`
(avant la ligne 447, `| 8 | panneau déconnecté (`panneau-off`) …`), sans ligne vide :

````markdown
| 7b1 | début de la marche prolongée (`prol-debut`) : « marche », V2, 10 s d'attente ; enregistrement lancé, appui sur « marche » vers 2 s | — | un fichier de 10 s ; moteur toujours en V2, ⏻ clignote à 1 Hz, bip (étape −1, ligne 3) | | trame de « marche » moteur tournant, identique ou non à celle de « marche » moteur arrêté. Une activité à 1 Hz sur `D` pendant le clignotement est un indice que la carte pilote ⏻ (test 4 de l'étape 6) |
| 7b2 | fin de la marche prolongée (`prol-fin`) : lampe allumée, « marche », V2, 10 s d'attente ; enregistrement de 17 min lancé (commande sous le tableau), appui sur « marche » vers 5 s, chronomètre lancé ; plus aucun appui ; vidéo du panneau de 14 min 30 à 16 min | chronomètre ; vidéo | fin vers 15 min ; état final : celui de l'étape −1b, ligne 11 | fin à ____ min ____ s (heure du Mac : ____) ; moteur : ____ ; ⏻ : ____ ; lampe : ____ ; bip : ☐ oui ☐ non ; trame à la fin : ☐ oui ☐ non | test 6 de l'étape 6. Pas de fin à 17 min : nouvel enregistrement (`prol-fin-suite`) et vidéo jusqu'à la fin, 30 min au plus après l'appui |
| 7b3 | vitesse active pendant la prolongation (`prol-vactive`) : « marche », V2, 10 s d'attente ; enregistrement lancé ; « marche » vers 1 s, puis V2 vers 6 s | — | un fichier de 10 s ; moteur arrêté, ⏻ fixe, bip (étape −1, ligne 3) | | trames de la transition « prolongée → armée » |
| 7b4 | « marche » pendant la prolongation (`prol-marche`) : même départ ; « marche » vers 1 s, puis de nouveau « marche » vers 6 s | — | un fichier de 10 s ; effet relevé à l'étape −1b, ligne 13 (sinon, le noter ici) | | trames de cette transition |
| 7b5 | autre vitesse pendant la prolongation (`prol-autre-v`) : même départ ; « marche » vers 1 s, puis V3 vers 6 s | — | un fichier de 10 s ; effet relevé à l'étape −1b, ligne 14 (sinon, le noter ici) | | trames de cette transition |
````

(Le bloc C2 vient juste après, puis la ligne 447 d'origine.)

### A5. SPEC, §6, étape 6 : test 6

**Insérer juste après le bloc C3** (test 5), avant la ligne `4. Appliquer la règle de choix du §4, …` :

````markdown
   6. **La fin de la marche prolongée** (scénario 7b) laisse-t-elle une trame
      sur `D` ? Sinon, le module du produit la verra à la répétition suivante
      de l'état (test 5), ou, sans répétition, par la lecture d'état annexe
      (arrêt du moteur).

   Les tests 5 et 6 ne changent pas la règle de choix du §4 : ils servent au
   sous-projet 2.
````

### A6. JOURNAL, étape 6 : ligne 9 du tableau

**Insérer juste après le bloc C4** (ligne 8), sans ligne vide :

````markdown
| 9 | test 6 : la fin de la marche prolongée laisse-t-elle une trame sur `D` (7b2) ? | — | | | oui : le module du produit lira la fin sur le bus ; non : il la verra à la répétition suivante de l'état (test 5), ou, sans répétition, par la lecture d'état annexe (arrêt du moteur ; spec du produit, §5.9) |
````

### A7. SPEC, §1 : « Hors périmètre »

**Remplacer la ligne 65 :**

```
- l'arrêt différé : on observe seulement s'il existe nativement ;
```

**par :**

```
- l'arrêt différé : on observe seulement la marche prolongée native (étapes −1
  et −1b) et ses trames (étape 5, scénario 7b) ;
```

---

## (b) L'arrêt de l'étape 7

### La phrase ambiguë

SPEC, §6, étape 7, point 3, ligne 562 :

> `4. l'arrêt.`

Même mot, sans séquence, au §1, point 2 (ligne 53, « puis l'arrêt »), à
l'étape 5 (ligne 476, « 6. arrêt ; »), et dans le JOURNAL (lignes 444, 621 et
653). La seule touche d'arrêt du panneau est « marche ». Or, moteur tournant,
« marche » **lance la marche prolongée** (JOURNAL, étape −1, ligne 3) au lieu
d'éteindre. L'arrêt complet observé le 27/09 (de 12,9 s à 15,1 s) demande deux
touches : la vitesse active (moteur arrêté, ⏻ fixe), puis « marche » (⏻ éteint).

**Définition retenue :** vitesse active, attente de l'arrêt du moteur confirmé,
puis « marche ». Jamais « marche » moteur tournant pour éteindre.

### B1. SPEC, §1, point 2

**Remplacer les lignes 52 à 55 :**

```
2. **Injection prouvée.** On injecte la touche lumière, chaque vitesse, la
   séquence « marche puis vitesse », puis l'arrêt, **20 fois sur 20 chacune**,
   et **les voyants du panneau suivent**. Avec C, le critère porte sur la touche
   prototypée.
```

**par :**

```
2. **Injection prouvée.** On injecte la touche lumière, chaque vitesse, la
   séquence « marche puis vitesse », puis l'arrêt, **20 fois sur 20 chacune**,
   et **les voyants du panneau suivent**. **L'arrêt est une séquence de deux
   touches** : la vitesse active, puis « marche » une fois l'arrêt du moteur
   confirmé (étape 7). « Marche » moteur tournant lancerait la marche
   prolongée au lieu d'éteindre. Avec C, le critère porte sur la touche
   prototypée.
```

### B2. SPEC, §6, étape 7 : la phrase ambiguë

**Remplacer la ligne 562 :**

```
     4. l'arrêt.
```

**par :**

```
     4. l'arrêt, depuis une vitesse (V1, V2 et V3 à tour de rôle) :
        1. injecter la vitesse active : le moteur s'arrête, ⏻ reste fixe
           (état armé, étape −1, ligne 4) ;
        2. attendre la **confirmation de l'arrêt du moteur**, par le signe que
           fixe l'avenant : la trame d'état de la carte si elle en émet,
           sinon le constat de Majid (voyant de vitesse éteint, ⏻ fixe,
           moteur silencieux). On attend aussi au moins le délai minimal ;
        3. injecter « marche » : tout s'éteint, ⏻ compris. La lampe ne change
           pas.

        **Jamais « marche » avant la confirmation** : moteur tournant, elle
        lance la marche prolongée (⏻ clignote). Sans confirmation dans
        l'attente maximale de l'avenant, « marche » ne part pas et l'essai
        est un échec.
```

(Les lignes 563 à 565 d'origine, la ligne vide puis « Le firmware impose le
délai minimal… », restent en place.)

### B3. SPEC, §6, étape 5 : scénario « arrêt »

**Remplacer la ligne 476 :**

```
6. arrêt ;
```

**par :**

```
6. arrêt : la vitesse active, puis « marche » une fois le moteur arrêté
   (séquence de l'étape 7) ;
```

### B4. SPEC, §6, étape 6 : avenant

**Insérer après la ligne 539 :** `  - le délai minimal entre deux changements d'état du moteur (étape −1) ;`

````markdown
  - pour la séquence d'arrêt de l'étape 7 : le signe qui confirme l'arrêt du
    moteur avant « marche » (trame d'état de la carte, ou constat de Majid),
    et l'attente maximale de ce signe ;
````

### B5. JOURNAL, étape 5 : ligne 6

**Remplacer la ligne 444 :**

```
| 6 | arrêt (`arret`) | — | idem | | |
```

**par :**

````markdown
| 6 | arrêt (`arret`) : depuis V2 (moteur en marche depuis 10 s au moins) ; appui sur V2 vers 1 s ; moteur arrêté et ⏻ fixe, appui sur « marche » vers 6 s | — | idem ; hotte éteinte, ⏻ éteint, sans clignotement | | ⏻ qui clignote : « marche » est partie moteur tournant ; on éteint (V2, puis « marche ») et on refait le scénario |
````

### B6. JOURNAL, étape 6 : table de l'avenant

**Insérer après la ligne 567 :** `| délai minimal entre deux changements d'état du moteur (`delai_min_ms`, d'après l'étape −1, ligne 10) | |`

````markdown
| séquence d'arrêt de l'étape 7 : signe qui confirme l'arrêt du moteur avant « marche » (trame d'état de la carte, ou constat de Majid) et attente maximale de ce signe | |
````

### B7. JOURNAL, étape 7 : comptage et ligne « arrêt »

**1) Insérer après la ligne 612 :** `du panneau change** comme il faut.`
(fin du paragraphe « Essais, dans cet ordre, **20 fois sur 20 chacun**. … ») :

````markdown

**L'arrêt** est une séquence de deux injections : la vitesse active, puis
« marche ». « Marche » ne part qu'après **la confirmation de l'arrêt du
moteur**, par le signe que fixe l'avenant (trame d'état de la carte si elle en
émet, sinon constat de Majid : voyant de vitesse éteint, ⏻ fixe, moteur
silencieux), et au moins `delai_min_ms` après la première injection.
**Jamais « marche » moteur tournant** : elle lancerait la marche prolongée
(⏻ clignote). Un essai compte deux événements `injection`. Il est réussi si
les deux sont `ok` et si la hotte finit éteinte : moteur arrêté, ⏻ éteint,
sans clignotement, lampe inchangée. Si ⏻ clignote, c'est un échec : on revient
à l'état éteint au panneau (vitesse active, puis « marche ») et on note la
cause.
````

**2) Remplacer la ligne 621 :**

```
| arrêt | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
```

**par :**

````markdown
| arrêt : vitesse active, arrêt du moteur confirmé, puis « marche » (depuis V1, V2 et V3 à tour de rôle) | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
````

### B8. JOURNAL : critères de sortie

**Remplacer les lignes 652 à 654 :**

```
- [ ] 2. **Injection prouvée** : lumière, chaque vitesse, « marche » puis vitesse,
  arrêt, 20 fois sur 20 chacune, et les voyants du panneau suivent (avec C : la
  touche prototypée).
```

**par :**

```
- [ ] 2. **Injection prouvée** : lumière, chaque vitesse, « marche » puis vitesse,
  arrêt (vitesse active, arrêt du moteur confirmé, puis « marche »), 20 fois sur
  20 chacune, et les voyants du panneau suivent (avec C : la touche prototypée).
```

---

## (c) Repos de 60 s sans appui

La question : la carte répète-t-elle son état sur `D` au repos ? Si oui, un
module qui redémarre, hotte en marche, relit l'état de la hotte et publie
l'état réel (piège de la NVS). Sinon, `D` ne le lui rend pas.

La capture est demandée moteur tournant (7c1). La ligne 7c2 pose la même
question pour l'état armé : Maison confond armée et éteinte, mais pas
l'automate, qui doit savoir s'il faut appuyer sur « marche » avant une vitesse.
Les 17 min de 7b2 couvrent l'état prolongé.

### C1. SPEC, §6, étape 5 : scénario 7c

**Insérer juste après le bloc A3** (après la puce `7b.`), puis une ligne vide
avant `8. panneau déconnecté. …` :

````markdown
- **7c. Repos sans appui, 60 s**, moteur en V2 et lampe allumée, puis hotte
  armée. La carte répète-t-elle son état sur `D` ? C'est ce qui dira si un
  module qui redémarre peut retrouver l'état de la hotte (test 5 de
  l'étape 6).
````

### C2. JOURNAL, étape 5 : lignes 7c1 et 7c2

**Insérer juste après le bloc A4** (après la ligne `7b5`), sans ligne vide, avant
la ligne 447 d'origine (`| 8 | panneau déconnecté …`) :

````markdown
| 7c1 | repos moteur tournant (`repos-v2`) : « marche », V2, lampe allumée, 10 s d'attente ; enregistrement de 60 s, **sans aucun appui** | — | un fichier de 60 s : trames périodiques, ou silence | réceptions en 60 s : ____ ; période : ____ s | test 5 de l'étape 6 |
| 7c2 | repos armé (`repos-arme`) : « marche » seule (⏻ fixe, moteur arrêté), lampe éteinte ; 60 s sans appui | — | idem | réceptions en 60 s : ____ ; période : ____ s | test 5 : armée ou éteinte, l'automate doit le savoir pour choisir la séquence |
````

### C3. SPEC, §6, étape 6 : test 5

**Insérer après la ligne 528 :** `   4. La carte émet-elle son état ou un paquet de voyants ?`
(avant la ligne 529, `4. Appliquer la règle de choix du §4, …`) ; le bloc A5 suit :

````markdown
   5. **Au repos, sans appui** (scénarios 7b et 7c de l'étape 5), la carte
      répète-t-elle son état sur `D` ? Avec quelle période ? Si oui, un module
      qui redémarre relit l'état de la hotte en une période ; sinon, `D` ne le
      lui rend pas.
````

### C4. JOURNAL, étape 6 : ligne 8 du tableau

**Insérer après la ligne 525 :** `| 7 | test 4 : la carte émet-elle son état ou un paquet de voyants ? | — | | | |`
(le bloc A6 suit) :

````markdown
| 8 | test 5 : au repos sans appui (7b2, 7c1, 7c2), la carte répète-t-elle son état ? Avec quelle période ? | s | trames périodiques qui portent l'état (moteur, vitesse, ⏻, lampe), ou silence | | répété : au démarrage, le module du produit relira l'état sur `D` avant de publier. Silence : l'état ne se relit pas par `D` ; la parade (lecture d'état annexe) est à cadrer au sous-projet 2 |
````

### AC. JOURNAL, étape 5 : commandes des lignes 7b et 7c (commun à a et c)

**Insérer après la ligne 449 :** `| 9 | compteurs de la sonde après la série : `debord`, `lignes_perdues`, `sautes` | — | 0 partout | | sinon on refait le scénario |`
Mettre une ligne vide avant le bloc, et une autre après, avant la ligne 451 (`Captures versées dans `captures/`…`) :

````markdown
**Lignes 7b et 7c (ajout du 28/09).** Départ de chaque ligne : hotte éteinte
(⏻ éteint). Entre deux lignes, on revient à l'état éteint au panneau : vitesse
active, moteur arrêté (⏻ fixe), puis « marche ». Jamais « marche » moteur
tournant : elle lance la prolongation. Deux commandes changent :
- 7b2 : `python3 tools/hotte_udp.py enregistre hotte-sonde.local prol-fin "capture changements" --duree 1020`.
  Le mode `changements` garde le fichier petit si la carte répète ses trames.
  En mode `tout`, un trafic continu donnerait jusqu'à 90 Mo en 17 min
  (100 lignes par seconde). Une réception identique à la précédente n'est pas
  émise, mais elle se voit dans `etat.capture.rep_en_cours`. La ligne suivante
  remet `capture tout` ;
- 7c1 et 7c2 : `python3 tools/hotte_udp.py enregistre hotte-sonde.local <scénario> "capture tout" --duree 60`.

Pendant les 17 min de 7b2, on reste dans la pièce (règle 12). La batterie est
chargée avant la séance.
````

---

## (d) Mesure sans risque : M1 (radio comparative)

Cette mesure ne fait pas partie des étapes −1 à 7 : elle prépare le
sous-projet 2, avant le dessin du boîtier. On la nomme M1 pour éviter toute
confusion avec les numéros d'étape. Le relevé de température en cuisson,
d'abord prévu comme M2, est retiré par l'amendement de Majid du 28/09 au soir :
le module du produit publiera la température de sa puce, avec une alerte (spec
du produit, §7.4).

### D1. SPEC, §6 : section « Mesure M1 »

**Insérer après la ligne 578 :** `- **Si le choix est C :** on suit l'avenant (prototype PhotoMOS sur une touche).`
(avant la ligne 580, `## 7. Matériel d'interface`) :

````markdown

### Mesure M1 pour le produit : radio comparative, hotte débranchée (ajout du 28/09)

Elle prépare le sous-projet 2 : elle décide de la carte et de son antenne.
**Elle ne conditionne aucune étape de la reconnaissance.** On peut la faire à
tout moment, mais avant le dessin du boîtier et avant tout achat de carte.
Journal : `RECONNAISSANCE.md`, « Mesure M1 ».

- **État de la hotte :** **débranchée pendant toute la séance**, fiche à vue.
  On attend 5 min avant d'ouvrir le boîtier (règle 1).
- **Matériel :**
  - la sonde, du modèle prévu pour le produit (SuperMini violette, antenne
    céramique ; sinon, on note le modèle). Elle est seule, hors du boîtier de
    mesure, en Wi-Fi, sur sa batterie vérifiée (§12) ;
  - un sachet plastique **non métallisé** (un sachet antistatique métallisé
    ferait écran) ;
  - un support isolant (un carton) ;
  - le Mac, qui enregistre par le Wi-Fi.
- **Principe :** on relève le RSSI Wi-Fi de la sonde, pour le même point
  d'accès, juste sous la hotte, puis dans le boîtier électronique. L'écart est
  l'atténuation de la hotte à 2,4 GHz, la bande de Thread.

Positions, 3 min chacune, avec le RSSI publié toutes les 10 s
(`json reseau 10000`) :
1. **P0** : sous la hotte, sur le support isolant posé sur la plaque éteinte et
   froide. La sonde est à environ 10 cm sous le filtre, à l'aplomb du boîtier
   électronique ;
2. **P1a et P1b** : dans le boîtier électronique, à l'emplacement prévu du
   module (quart haut-droit), l'antenne dans deux orientations. Couvercle
   fermé, filtre remis ;
3. **P0 de nouveau.** Plus de 3 dB d'écart avec le premier P0 : la série est
   à refaire (quelqu'un a bougé, ou le point d'accès a changé de canal).

**Règles propres à M1 :**
- dans le boîtier, la sonde reste dans son sachet : ni sa coque USB-C ni ses
  broches ne touchent quoi que ce soit. Elle et sa batterie n'appuient sur
  aucun composant et n'entrent pas dans les zones interdites (règle 3). Le
  couvercle se ferme sans forcer ; sinon, on ne force pas, et M1 est à revoir ;
- **aucun câble ne sort du boîtier** : il guiderait l'onde vers l'extérieur, et
  la mesure serait trop optimiste ;
- **on ne rebranche jamais la hotte avec la sonde ou sa batterie dans le
  boîtier** (règle 13). À la fin, on retire et on compte la sonde, la batterie
  et le sachet, on ferme le couvercle et on remet le filtre. Puis on refait
  l'étape 2b si l'adaptateur est posé (règle 7).

**Calcul :**
- **atténuation** `A` = médiane en P0 − meilleure médiane en P1. Le boîtier
  imprimé reproduira l'orientation retenue. Si la sonde perd le Wi-Fi en P1,
  `A` ne se mesure pas ainsi : c'est la mesure Thread directe en P1 (point
  suivant) qui décide. Sans elle, on retient la branche « moins de 10 dB » ;
- **marge Thread estimée** `M = R_T − A + 104`, en dB :
  - `R_T` est le RSSI moyen du lien Thread vers le routeur de bordure, en P0.
    Il est lu par la C6 du banc Matter du sous-projet 2 (`parent_rssi`, comme
    dans benq ; la C6 est en MED pendant la mesure) ;
  - −104 dBm est la sensibilité du C6 en 802.15.4 (fiche Espressif, 1 % de
    trames perdues) ;
- **si la C6 du banc Matter peut être posée elle-même en P1** (sur batterie, en
  MED comme en P0, ses diagnostics radio lus par le canal compagnon),
  `M = R_T(P1) + 104`, mesurée directement. Cette mesure fait foi, et `A` sert
  alors de contrôle.

| Marge `M` | Décision |
|---|---|
| plus de 20 dB | SuperMini violette gardée ; routeur Thread par défaut |
| de 10 à 20 dB | SuperMini gardée, en MED ; essai de 48 h du lien avant de figer le boîtier |
| moins de 10 dB | carte à antenne déportée (par exemple XIAO ESP32-C6, u.FL) vers une partie non métallique ; M1 refaite avec elle |

À moins de 3 dB d'un seuil, on refait la série. Si l'écart demeure, on retient
la branche la plus prudente.

**Limites.**
- Le point d'accès Wi-Fi n'est pas le routeur de bordure, et la direction
  diffère. Or l'atténuation d'une caisse d'acier dépend de la direction, à
  cause de ses ouvertures. C'est pourquoi `R_T` vient d'une mesure Thread.
- `M` est la marge dans le sens routeur vers module. Si le produit réduit sa
  puissance d'émission (alimentation par `CN3 +`), la marge dans l'autre sens
  baisse d'autant. On en tient compte au plan du matériel.
- La qualité de lien que rapporte OpenThread n'est pas une marge : sur le C6,
  elle est calculée contre une sensibilité fixe de −120 dBm, et non contre les
  −104 dBm de la fiche (spec du produit, §6.1). Seul `M` décide.
````

### D2. SPEC, §5 : règles 3 et 13

**1) Insérer après la ligne 175 :** `     `AC-POWER`), le condensateur moteur.` (fin de la règle 3) :

````markdown
   - Mesure M1 (§6) : on pose en plus, dans le quart haut-droit du boîtier, la
     sonde dans son sachet et sa batterie. On ne touche à rien d'autre. Rien
     n'appuie sur un composant, et rien n'entre dans une zone interdite.
````

**2) Insérer après la ligne 230 :** `    place.` (fin de la règle 13) :

````markdown
    - M1 : **on ne rebranche jamais la hotte avec la sonde ou sa batterie
      dans le boîtier électronique.**
````

### D3. SPEC, §1 : « Hors périmètre »

**Remplacer la ligne 62 :**

```
- la carte et le boîtier définitifs ;
```

**par :**

```
- la carte et le boîtier définitifs : la mesure M1 (§6) prépare seulement le
  choix de la carte ;
```

### D5. JOURNAL : « Installation et instruments »

**Insérer après la ligne 33 :** `| `R_be` de l'étage d'écoute (100k par défaut ; changements notés à l'étape 5b) | |`

````markdown
| Carte de la sonde : modèle (le produit prévoit la SuperMini violette, antenne céramique) | |
| Point d'accès Wi-Fi pour M1 : pièce, distance à la hotte, canal 2,4 GHz (ni SSID ni adresse : dépôt public) | |
| Routeurs de bordure Thread : modèles, distance à la hotte | dans la même pièce, à moins de 5 m (Majid, 28/09) ; modèles : ____ |
````

### D6. JOURNAL : section « Mesure M1 »

**Insérer après la ligne 643 :** `  (règle 13).` (fin de la sortie de l'étape 7),
avant la ligne 645, `## Critères de sortie du sous-projet` :

`````markdown

## Mesure M1 pour le produit : radio comparative, hotte débranchée (ajout du 28/09)

Elle prépare le sous-projet 2 et ne conditionne aucune étape de la
reconnaissance (spec §6, « Mesure M1 »). On la fait avant le dessin du boîtier
et avant tout achat de carte.

**Date :**

**Conditions :**
- [ ] hotte **débranchée pendant toute la séance** (débranchée à : ____), fiche à vue ; boîtier ouvert après 5 min ;
- [ ] sonde (modèle : ____) seule, hors du boîtier de mesure, sur sa batterie vérifiée, dans un sachet non métallisé ;
- [ ] point d'accès noté dans « Installation et instruments » ; personne ne passe entre lui et la hotte pendant les relevés.

Une commande par position, 30 s après avoir posé la sonde et refermé :
`python3 tools/hotte_udp.py enregistre hotte-sonde.local m1-<position> "json reseau 10000" --duree 180`.
Médiane du RSSI d'un fichier :

```
python3 -c 'import json,sys,statistics as s
v=[l["l"]["wifi"]["rssi_dbm"] for l in map(json.loads,open(sys.argv[1])) if l["l"].get("t")=="reseau" and (l["l"].get("wifi") or {}).get("rssi_dbm") is not None]
print(len(v),"releves ; mediane",s.median(v),"dBm ; min",min(v),"; max",max(v)) if v else print("aucun releve")' logs/<fichier>.jsonl
```

Les fichiers restent dans `logs/` : seules les médianes vont dans ce journal.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | P0 (`m1-p0`) : sous la hotte, sur un support isolant posé sur la plaque éteinte et froide, environ 10 cm sous le filtre, à l'aplomb du boîtier électronique, bord de l'antenne vers l'avant de la hotte | RSSI Wi-Fi, médiane sur 3 min | 15 relevés au moins | médiane ____ dBm (min ____, max ____) | |
| 2 | P1a (`m1-p1a`) : dans le boîtier, à l'emplacement prévu du module (quart haut-droit), bord de l'antenne vers l'avant de la hotte ; couvercle fermé, filtre remis, aucun câble sortant | idem | | | aucun relevé : Wi-Fi perdu, la ligne 8 décide |
| 3 | P1b (`m1-p1b`) : même endroit, bord de l'antenne vers le bas (vers le filtre) | idem | | | idem |
| 4 | P0 de nouveau (`m1-p0bis`), comme à la ligne 1 | idem | à 3 dB au plus de la ligne 1 | | plus de 3 dB : série à refaire |
| 5 | *facultatif :* P1, meilleure orientation, couvercle fermé, **filtre retiré** (`m1-p1-sansfiltre`) | idem | | | dit si le filtre est la principale barrière |
| 6 | *facultatif, boîtier ouvert :* quart haut-droit photographié avec une règle en plastique, posée sans toucher la carte ni les zones interdites, dans deux directions (`docs/photos/m1-volume.jpg`) | photo | | | volume libre pour le dessin du boîtier ; évite une ouverture de plus |
| 7 | `A` = ligne 1 − meilleure des lignes 2 et 3 | dB | | `A` = ____ dB ; orientation retenue : ____ | |
| 8 | `R_T` : RSSI moyen du lien Thread vers le routeur de bordure, lu par la C6 du banc Matter en MED (`parent_rssi`), médiane sur 3 min, en P0 ; ou directement en P1 si la C6 du banc peut y être posée | dBm | | `R_T` = ____ dBm, en ☐ P0 ☐ P1 (date du banc : ____) | en attente tant que le banc Matter n'est pas prêt |
| 9 | `M` = ligne 8 − `A` + 104 (mesure en P0), ou ligne 8 + 104 (mesure directe en P1) | dB | | `M` = ____ dB | tableau ci-dessous |

| Marge `M` | Décision | ☐ |
|---|---|---|
| plus de 20 dB | SuperMini violette gardée ; routeur Thread par défaut | ☐ |
| de 10 à 20 dB | SuperMini gardée, en MED ; essai de 48 h du lien avant de figer le boîtier | ☐ |
| moins de 10 dB, ou Wi-Fi perdu en P1 sans mesure Thread directe | carte à antenne déportée (par exemple XIAO ESP32-C6, u.FL) vers une partie non métallique ; M1 refaite avec elle | ☐ |

À moins de 3 dB d'un seuil : série refaite ; si l'écart demeure, la branche la
plus prudente.

**Fin de séance, hotte toujours débranchée :**
- [ ] sonde, batterie et sachet retirés du boîtier et comptés ;
- [ ] couvercle fermé, filtre remis, sans rien pincer ;
- [ ] étape 2b refaite si l'adaptateur est posé (tableau des nouveaux passages) ;
- [ ] sonde remise sur le boîtier de mesure dans le même sens, si elle en venait.

**Sortie :**
- [ ] `A` et `M` notées ; carte décidée (tableau ci-dessus).
`````

### D7. FICHE (SECURITE.md)

**1) Règle 3 : insérer après la ligne 35 :** `     du bas (relais, `VH2` à `VH6`, « LAMP », `AC-POWER`) ; le condensateur moteur.`

````markdown
   - Mesure M1 (spec §6) : on pose en plus, dans le quart haut-droit, la sonde
     dans son sachet et sa batterie. Rien d'autre n'est touché. Rien n'appuie
     sur un composant, et rien n'entre dans une zone interdite.
````

**2) Règle 13 : insérer après la ligne 86 :** `    en place.`

````markdown
    - M1 : **jamais la hotte branchée avec la sonde ou sa batterie dans le
      boîtier électronique.**
````

**3) « Avant chaque mise sous tension » : insérer après la ligne 111 :** `- [ ] Étape 7 : Majid devant la hotte, à portée de la fiche (règle 11).`

````markdown
- [ ] Après M1 : rien d'oublié dans le boîtier électronique (ni sonde, ni
      batterie, ni sachet).
````

La procédure du différentiel (lignes 150 et 151) ne change pas.

---

## (e) Liens communs dans la SPEC

### E1. Statut

**Insérer après la ligne 8 :** `électronique et firmware, cohérence avec le protocole compagnon). Spec à relire.`

```
Ajouts du 28/09, approuvés par Majid : étape −1b, scénarios 7b et 7c de
l'étape 5, tests 5 et 6 de l'étape 6, séquence d'arrêt de l'étape 7, mesure
M1 (§6).
```

### E2. §13, risques

**Insérer après la ligne 1041 :** `| garantie | l'adaptateur est réversible ; la hotte redevient d'origine sans trace |`

````markdown
| « marche » injectée moteur tournant : marche prolongée au lieu de l'arrêt | séquence d'arrêt de l'étape 7 : « marche » seulement après la confirmation de l'arrêt du moteur |
| la fin de la marche prolongée ne laisse aucune trame, ou la carte ne répète pas son état au repos | tests 5 et 6 de l'étape 6 ; au sous-projet 2 : lecture d'état annexe si besoin, jamais de minuterie qui devine la fin (spec du produit, §5.9) |
| objet oublié dans le boîtier électronique après M1 | inventaire compté avant de refermer ; case de la fiche avant chaque mise sous tension |
| Wi-Fi perdu dans le boîtier pendant M1 | mesure Thread directe en P1 par la C6 du banc Matter ; sans elle, branche « moins de 10 dB » |
````

### E3. §14, « Et après »

**Insérer après la ligne 1052 :** `- la lecture d'état annexe, si le retour d'état l'exige.`

````markdown
- les transitions et les trames de la marche prolongée (étapes −1b et 5), et
  l'état répété au repos (tests 5 et 6 de l'étape 6) ;
- la mesure M1 (carte et antenne).
````

---

## (f) Correction liée : repérage de la SuperMini violette

Hors des ajouts (a) à (d), à confirmer par Majid. Les cartes du projet (sonde et
générateur) sont des SuperMini **violettes**. Le §9 de WIRING.md décrit la
rangée d'une autre carte, d'après le brief du boîtier de la ScreenBar. Les bancs
16, 21 et 24 en ont besoin avant tout plan du produit. Les GPIO de la sonde ne
changent pas (6 et 7) : seul le repérage est corrigé.

### F1. WIRING, §9 : repérage

**Appliqué le 29/09 :** WIRING.md §9 a été réécrit en entier avec le brochage réel (bloc W5 du guide de fabrication), ce bloc n'est plus à verser.

**Remplacer les lignes 280 à 282 :**

```
**Repérage.** On repère chaque broche par sa sérigraphie. D'après le brief du
boîtier de la ScreenBar (même carte), une rangée extérieure porte, dans l'ordre,
`6 · 14 · 15 · 18 · 19 · 20 · 3V3 · GND · 5V`. GPIO7 se repère sur la sérigraphie.
```

**par :**

```
**Repérage.** Les cartes du projet sont des SuperMini **violettes** : 10 + 10
pastilles, l'USB-C à un bout, l'antenne céramique à l'autre. Rangée haute,
depuis l'USB : `5V · GND · 3V3 · 20 · 19 · 18 · 15 · 14 · 9 · 8` ; rangée basse :
`TX · RX · 0 · 1 · 2 · 3 · 4 · 5 · 6 · 7`. GPIO6 et GPIO7 sont les deux dernières
pastilles de la rangée basse, côté antenne. Le brief du boîtier de la ScreenBar
décrit une autre carte : sa rangée `6 · 14 · 15 · 18 · 19 · 20 · 3V3 · GND · 5V`
ne vaut pas ici. On repère chaque broche par sa sérigraphie ; le contrôle
ci-dessous le confirme.
```

---

## Points à faire confirmer par Majid

Les décisions du 28/09 ne tranchent pas ces choix. Je les ai faits pour
rendre le texte testable :

1. **Étape −1b** : observation sans montage, dès maintenant. Elle est en plus
   des captures de l'étape 5, parce que l'automate en a besoin avant.
2. **Ligne 7c2** (repos armé, 60 s) : c'est une extension de (c), qui ne
   demandait que « moteur tournant ».
3. **Définition de la marge** : `M = R_T − A + 104`, c'est-à-dire la marge
   au-dessus de la sensibilité du C6 (−104 dBm). Elle demande une lecture
   Thread faite par le banc Matter : la décision sur la carte attend donc ce
   banc. Si le Wi-Fi est perdu en P1 et qu'il n'y a pas de mesure Thread, je
   retiens la branche « moins de 10 dB ».
4. **Seuils près d'une limite** : à moins de 3 dB d'un seuil, on refait la
   série, puis on retient la branche prudente.
5. **Lignes facultatives de M1** : filtre retiré, et photo du volume libre
   avec une règle en plastique, pour profiter de l'ouverture du boîtier.
6. **Non traité, mais repéré** : à l'étape 7, « chaque vitesse » ne dit pas
   l'état de départ. Depuis l'état éteint, une vitesse ne fait rien (étape −1,
   ligne 1) : il faut sans doute partir de l'état armé.
7. **Test 6 (A5, A6)** : sans trame de fin sur `D`, le module du produit voit la
   fin de la marche prolongée à la répétition de l'état, ou par la lecture
   d'état annexe. La minuterie de 15 min d'abord proposée est écartée : elle
   publierait un état deviné (spec du produit, §5.9). Validé par Majid le 29/09.
8. **Bloc F1** : correction du repérage de WIRING.md §9, hors des ajouts (a) à
   (d) approuvés, mais validée par Majid le 29/09. Elle ne change que le texte de repérage.
