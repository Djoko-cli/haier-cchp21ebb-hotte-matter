# Plan de la sonde, partie 1 : terrain

> En-tête, contraintes globales, carte des fichiers et interfaces : [../2026-09-27-reconnaissance-sonde.md](../2026-09-27-reconnaissance-sonde.md). Les tâches s'exécutent dans l'ordre des numéros (1 à 1).

### Tâche 1 : Fiches de terrain (sécurité, journal, câblage)

Trois documents pour que Majid puisse commencer les étapes −1 à 3 sans attendre le
firmware : la fiche de sécurité à garder sous les yeux, le journal à remplir étape
par étape, et le câblage (adaptateur, boîtier de mesure, étages, brochage). Tout le
contenu vient de la spec (§5, §6, §7, §10, §12) et des décisions prises avec Majid
après relecture ; rien n'est inventé sur les valeurs. Ces décisions : la règle 2
se lit « différentiel testé juste avant de poser chaque nouveau montage, hotte
dans son état précédent, puis débranchée pour poser » ; entre 3 et 5 V à
l'étape 3a, on en rediscute avant tout Mac ou analyseur ; à la sortie de
l'étape 4, couvercle et filtre avant l'étape 2b ; polarité de l'étape 4 de +4,5 à
+5,3 V ; le niveau bas de `D` (étapes 5b et 7) se juge fonctionnellement, la
mesure chiffrée (oscilloscope) étant optionnelle ; le boîtier de mesure se
détache du câble de sortie par une fiche et une embase XH 4 broches ; le banc
n'a jamais de liaison avec la hotte ; le 1 nF du banc fait partie des achats.
Pas de code : le « test » est un script jetable de commandes `grep` qui prouve la
présence des valeurs clés (et la disparition des valeurs remplacées), la
structure attendue (13 règles, un tableau par étape), des tableaux bien formés et
des liens qui mènent quelque part.

**Fichiers :**
- Créer : `docs/SECURITE.md`
- Créer : `docs/WIRING.md`
- Créer : `docs/RECONNAISSANCE.md`
- Tester : `${TMPDIR:-/tmp}/verif_terrain.sh` (script jetable, **hors dépôt**, jamais committé : il n'est pas dans la carte des fichiers)

**Interfaces :**
- Consomme : `docs/SPEC-RECONNAISSANCE.md` (révision 2) : §5 (règles 1 à 13), §6 (étapes −1 à 7 : mesures, calibres, attendus, critères d'ARRÊT), §7.1 à §7.6 (adaptateur, étages, GPIO), §10 (banc), §12 (achats). Aucune tâche précédente. Le journal nomme des outils écrits plus tard, sans lien (le script ne vérifie que les liens) : `tools/serie_enregistre.py` (tâche 12), `tools/verse_capture.py <capture.jsonl> <nom>` (tâche 12b), `tools/hotte_udp.py` (tâche 20), `docs/BANC.md` (tâche 15).
- Produit (noms repris tels quels par les tâches suivantes) :
  - points de test `TP−` (U+2212), `TP+`, `TP_Dp`, `TP_Dc` ; cavaliers `J1` (entre `D_carte` et `D_panneau`, fermé par défaut) et `J2` (entre `TP+` et le `5V` de la sonde, ouvert par défaut, fermé à l'étape 4 seulement) ; transistors `Q1` (écoute, GPIO6), `Q2` (injection, GPIO7), `Q3` (générateur du banc, GPIO7 du second C6) ; résistance `R_be` ;
  - fiche et embase XH 4 broches entre le câble de sortie et le boîtier de mesure, broches 1 à 4 = `−`, `+`, `D_panneau`, `D_carte`, détrompeur : fiche retirée (hotte débranchée), le boîtier n'a plus aucune liaison avec la hotte ;
  - `docs/WIRING.md`, sections numérotées : `## 1. Vue d'ensemble`, `## 2. Adaptateur`, `## 3. Câble de sortie`, `## 4. Boîtier de mesure`, `## 5. Étage d'écoute (voie 1 : ...)`, `## 6. Étage d'injection (voie 1 : ...)`, `## 7. Voie 2 ...`, `## 8. Analyseur : voies et diviseurs`, `## 9. Brochage du C6 SuperMini`, `## 10. Générateur du banc`, `## 11. Nomenclature`, `## 12. Contrôles avant pose` (17 contrôles numérotés : 1 à 10 adaptateur et câble, 11 à 17 boîtier). `docs/BANC.md` (tâche 15) renvoie à « WIRING.md §10 » pour le montage du banc, qui pose la règle « le banc n'a jamais de liaison avec la hotte » (boîtier détaché, hotte débranchée toute la séance, contrôle `TP−` vers la terre de la fiche et vers la carcasse = OL ; sinon plaque d'essai du banc avec son propre étage d'écoute) ; le `README.md` (tâche 25) renvoie aux trois fiches ;
  - `docs/SECURITE.md` : `## Les 13 règles`, `## Avant chaque mise sous tension`, `## Banc de validation : aucune liaison avec la hotte`, `## En cas d'anomalie`, `## Procédure : test du différentiel (règle 2)`, `## Procédure : étape 1, test d'isolation`, `## Procédure : étape 2b, isolation depuis les points de test`, `## Procédure : étape 3a, fuite en charge` ;
  - `docs/RECONNAISSANCE.md` : un tableau `| # | Mesure | Calibre | Attendu | Relevé | Décision |` par étape ; noms de scénarios de l'étape 5, passés à `tools/hotte_udp.py enregistre <hôte> <scénario> "capture tout" --duree 10` (tâche 20) : `veille`, `marche`, `lumiere-on`, `lumiere-off`, `v1`, `v2`, `v3`, `arret`, `bip-<touche>`, `panneau-off` ; captures versées par `tools/verse_capture.py logs/<fichier>.jsonl etape5-<scénario>` (et `etape5b-<scénario>`) ; captures de l'étape 4 par `tools/serie_enregistre.py <port> etape4-<sujet> "capture tout" --duree 30` ; noms des valeurs de l'avenant alignés sur `config.injection` (tâche 23) : `bas_max_us`, `total_max_us`, `silence_min_us`, `attente_max_ms`, `delai_min_ms`. Étape 7, tableau « Préparation (choix A) » : les lignes 1 à 6 sont celles que la tâche 22 peut prendre pour ancre (elle y ajoute `injection regle <nom> <valeur>`).

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `${TMPDIR:-/tmp}/verif_terrain.sh` (hors dépôt) avec ce contenu exact. Il se
lance depuis la racine du dépôt ; les textes cherchés contiennent des caractères
non ASCII (`−` U+2212, `µ` U+00B5, `Ω` U+03A9, `±`, `☐`, `→`, `─` des schémas)
comparés à l'octet près, comme dans la spec.

```sh
#!/bin/sh
# Verification des fiches de terrain (tache 1) : presence des valeurs cles de la spec,
# structure des trois documents, tableaux bien formes. Script jetable, hors depot.
# A lancer depuis la racine du depot : sh "${TMPDIR:-/tmp}/verif_terrain.sh"
S=docs/SECURITE.md
R=docs/RECONNAISSANCE.md
W=docs/WIRING.md
n=0
e=0

# verif <fichier> <texte exact> ... : chaque texte doit figurer dans le fichier. Les lignes
# sont recollees (un seul espace) : un paragraphe recoupe autrement reste verifie.
verif() {
  f=$1
  shift
  plat=""
  if [ -f "$f" ]; then
    plat=$(tr '\n' ' ' < "$f" | tr -s ' ')
  fi
  for t in "$@"; do
    n=$((n + 1))
    case "$plat" in
      *"$t"*) ;;
      *)
        e=$((e + 1))
        echo "MANQUE dans $f : $t"
        ;;
    esac
  done
}

# au_moins <fichier> <texte exact> <minimum> : nombre de lignes qui contiennent le texte.
au_moins() {
  n=$((n + 1))
  c=$(grep -cF -- "$2" "$1" 2>/dev/null)
  if [ "${c:-0}" -lt "$3" ]; then
    e=$((e + 1))
    echo "TROP PEU dans $1 : '$2' (${c:-0} lignes, au moins $3)"
  fi
}

# absent <fichier> <texte exact> ... : chaque texte (valeur remplacee) doit avoir disparu du
# fichier, lignes recollees comme pour verif. Un fichier absent compte comme un echec.
absent() {
  f=$1
  shift
  plat=""
  if [ -f "$f" ]; then
    plat=$(tr '\n' ' ' < "$f" | tr -s ' ')
  fi
  for t in "$@"; do
    n=$((n + 1))
    if [ -z "$plat" ]; then
      e=$((e + 1))
      echo "RIEN A RELIRE dans $f : $t"
      continue
    fi
    case "$plat" in
      *"$t"*)
        e=$((e + 1))
        echo "A RETIRER dans $f : $t"
        ;;
    esac
  done
}

# --- SECURITE.md : les 13 regles du §5, numerotees comme dans la spec ---
regles=$(sed -n '/^## Les 13 règles/,/^## Avant chaque mise sous tension/p' "$S" 2>/dev/null)
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13; do
  n=$((n + 1))
  if ! printf '%s\n' "$regles" | grep -q "^$i\. \*\*"; then
    e=$((e + 1))
    echo "MANQUE dans $S : regle $i"
  fi
done
# Regles 1 a 6
verif "$S" "5 minutes" "E5 et E6" "4,7 µF / 450 V" "fiche débranchée, sans attente" \
  "bouton T" "la lumière doit s'éteindre" "Réarmer" \
  "\`CN1\` ni \`CN2\`" "photos/1.jpg" "photos/5.jpg" "photos/6.jpg" "recadrages/c4_bottom.jpg" \
  "\`VH2\` à \`VH6\`" "« LAMP »" "\`AC-POWER\`" "condensateur moteur" "connecteur blanc" \
  "connecteur, pince, résistance, cavalier, sonde, câble USB" \
  "300 V" "UL1007 ou UL1015" "105 °C" "VW-1 600 V" "deux épaisseurs" \
  "Couvercle fermé et filtre remis avant de rebrancher" "Aucune main dans la hotte sous tension"
# Regles 7 a 13 et encadre
verif "$S" "après **chaque** ouverture du boîtier" "avant tout Mac ou analyseur" \
  "ni chargeur, ni écran, ni Ethernet, ni hub alimenté, ni audio" "surface isolante" \
  "sur batterie USB" "3,6 V" "en présence de Majid" \
  "différentiel ou du disjoncteur" "odeur" "fumée" "claquements de relais répétés" "bip continu" \
  "panneau mort" "fil chaud" "Pas de cuisson" "\`J1\` fermé" \
  "une centaine de volts" "Seule compte la mesure en charge de l'étape 3a"
# Procedures des etapes 1, 2b, 3a
verif "$S" "## Procédure : étape 1" "## Procédure : étape 2b" "## Procédure : étape 3a" \
  "## En cas d'anomalie" "## Procédure : test du différentiel" \
  "fil à pinces crocodile" "aiguille" "20 MΩ ou plus" "**OL**" "10 s" "cordons inversés" \
  "trou qui reçoit la broche de terre" "200 Ω" "moins de 1 Ω" "valeur à soustraire" \
  "TP−" "TP+" "TP_Dp" "TP_Dc" "ouvrir \`J1\`" "Refermer \`J1\`" "20 kΩ ou automatique" \
  "10 kΩ / 2 W" "tension alternative" "dans les 5 s" "3 V au plus" "0,3 mA" "au-delà de 5 V" "0,5 mA" \
  "**ARRÊT**"
# Decisions : lecture de la regle 2, banc sans liaison avec la hotte, fiche XH 4 br. enfichee
verif "$S" "juste avant de poser ce montage" "état précédent, déjà contrôlé" "puis on débranche pour poser" \
  "5. Débrancher la hotte, puis poser le nouveau montage" "Lecture entre 3 et 5 V : rediscutée" \
  "## Banc de validation : aucune liaison avec la hotte" "hotte débranchée pendant toute la séance" \
  "câble de sortie déconnecté du boîtier" "plaque d'essai du banc" "**OL** les deux fois" \
  "fiche XH 4 br. du câble de sortie enfichée" "couvercle fermé et filtre remis d'abord"
absent "$S" "Différentiel testé, si ce montage n'a jamais été mis sous tension"

# --- WIRING.md : §7.1 a §7.6, §10, §12 ---
verif "$W" "## 1. Vue d'ensemble" "## 2. Adaptateur" "## 3. Câble de sortie" "## 4. Boîtier de mesure" \
  "## 5. Étage d'écoute" "## 6. Étage d'injection" "## 7. Voie 2" "## 8. Analyseur" \
  "## 9. Brochage du C6 SuperMini" "## 10. Générateur du banc" "## 11. Nomenclature" "## 12. Contrôles avant pose"
verif "$W" "fiche XH 3 br." "embase XH 3 br." "5,0 mm" "4,0 mm" "Dupont" \
  "\`J1\`" "\`J2\`" "TP−" "TP+" "TP_Dp" "TP_Dc" "D_panneau" "D_carte" \
  "4 fils" "300 V" "UL1007" "VW-1 600 V" "thermorétractable" \
  "fermé par défaut" "ouvert par défaut" "fermé seulement à l'étape 4" "sans ouvrir le boîtier"
verif "$W" "── 100k ──" "R_be" "(100k par défaut)" "47k" "15k" "25 µA" "1,2 V" "environ 2 V" "environ 5 V" \
  "0,6 V" "4 mV/°C" "40 à 110 µA" "1 à 2 µs" "3V3 (sonde) ── 10k" "GPIO6" "D haut → GPIO6 bas" \
  "GPIO7 ── 4,7k" "470 Ω" "220 Ω" "10 mA" "0,5 V" "0,8 V" "45 kΩ" "GPIO7 haut → D tiré bas" \
  "Ne jamais intervertir GPIO6 et GPIO7" "drain ouvert" "BC547" "2N3904" "C-B-E" "E-B-C" \
  "47k/68k" "100k/33k" "2,96 V" "2,98 V" "GPIO0" "GPIO1" "74HCT"
verif "$W" "| 4, 5, 8, 9, 15 |" "| 12, 13 |" "| 16, 17 |" "| 21, 22 |" "GPIO2 reste libre" \
  "| 5V |" "| GND |" "| 3V3 |" "USB-C" "isoler de tout métal" "+4,5 à +5,3 V" \
  "1 nF" "fil de masse" "GPIO7 haut = ligne basse" "10k seul" \
  "10 kΩ / 2 W" "grippe-fils" "aiguille fine" "FX2" "batterie USB" "barrettes femelles" "colliers" \
  "| 100k |" "| 68k |" "| 47k |" "| 33k |" "| 15k |" "| 10k |" "| 4,7k |" "| 470 Ω |" "| 220 Ω |"
# Decisions : boitier detachable (XH 4 br.), banc sans liaison avec la hotte, niveau bas juge
verif "$W" "fiche XH 4 broches" "embase XH 4 broches" "fiche XH 4 br." "embase XH 4 br." "détachable" \
  "\`−\`, \`+\`, \`D_panneau\`, \`D_carte\` (broches 1 à 4)" "détrompeur" "| Broche de la fiche XH 4 br. |" \
  "| 10 | fiche XH 4 br. retirée" "| 17 | contrôle électrique du module" "5,25 V" \
  "**Le banc n'a jamais de liaison avec la hotte.**" "hotte reste débranchée pendant toute la séance" \
  "câble de sortie est déconnecté du boîtier" "plaque d'essai du banc" "**OL** les deux fois" \
  "numérique" "le jugement est fonctionnel" "oscilloscope" "\`injection off\` et hotte débranchée d'abord" \
  "1 nF du banc compris"
absent "$W" "+4,7 à +5,0 V" "n'y figure pas"

# --- RECONNAISSANCE.md : un tableau par etape -1 a 7, photos, decision ---
verif "$R" "## Étape −1" "## Étape 0" "## Étape 1" "## Étape 2 :" "### Étape 2b" "### Étape 2c" \
  "## Étape 3" "### 3a." "### 3b." "### 3c." "## Étape 4" "## Étape 5 :" "## Étape 5b" \
  "## Étape 6" "### Décision de l'étape 6" "## Étape 7" "## Critères de sortie du sous-projet" \
  "## Journal des anomalies" "## Tests du différentiel" "## Installation et instruments"
au_moins "$R" "| # | Mesure | Calibre | Attendu | Relevé | Décision |" 16
au_moins "$R" "- [ ] Photo" 15
au_moins "$R" "**Date :**" 15
verif "$R" "20 MΩ ou plus" "OL" "moins de 1 Ω" "cordons court-circuités" "5,0 mm" "4,0 mm" \
  "10 kΩ / 2 W" "3 V au plus" "0,3 mA" "0,5 mA" "47 kΩ" "0,45 V" "0,9 V" "+4,5 à +5,3 V" \
  "±2 µs" "0,6 V" "0,8 V" "47k/68k" "100k/33k" "ADuM3160" "RSSI" \
  "hotte_udp.py enregistre" "logs/live.log" "\`debord\`" "\`lignes_perdues\`" "\`sautes\`" \
  "injection monte 1" "Majid devant la hotte" "PROTOCOL.md" "avenant" \
  "\`bas_max_us\`" "\`total_max_us\`" "\`silence_min_us\`" "\`attente_max_ms\`" "\`delai_min_ms\`" \
  "☐ A" "☐ B" "☐ C" "20 fois sur 20" "☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐" "**ARRÊT**" \
  "dépôt public" "EXIF"
# Trous du journal (etapes 2, 4, 5, 5b, 7) et decisions
verif "$R" "| 10 | fiche XH 4 br. retirée" "| 13 | \`TP_Dp\` vers la broche 6 des barrettes" \
  "GPIO6 et GPIO7 non interverties" "| 17 | contrôle électrique du module" "fiche XH 4 br." \
  "serie_enregistre.py <port> <scénario> \"capture tout\" --duree 30" \
  "USB débranché : en option, 10 kΩ" "retirer la 10 kΩ si elle a été posée" \
  "Le couvercle et le filtre passent avant l'étape 2b" \
  "banc validé (tâches 16 et 21" "batterie vérifiée (30 min)" \
  "enregistre hotte-sonde.local <scénario> \"capture tout\" --duree 10" "| 8b | après \`panneau-off\`" \
  "verse_capture.py logs/<fichier>.jsonl etape5-<scénario>" \
  "hotte encore débranchée" "enregistre hotte-sonde.local etape5b-<scénario> \"capture tout\" --duree 10" \
  "verse_capture.py logs/<fichier>.jsonl etape5b-<scénario>" \
  "Fin de séance, dans cet ordre (règle 8)" "ensuite seulement, rebrancher le chargeur du Mac" \
  "jugement fonctionnel" "oscilloscope" "Instrument qui a donné les niveaux des lignes 4 et 5" \
  "critère 5 du banc validé" "jamais un script ni un agent" "Instrument qui a donné le niveau de la ligne 8" \
  "on arrête, \`injection off\`, hotte débranchée, puis 470 Ω remplacées par 220 Ω" \
  "hotte débranchée, filtre retiré (sans ouvrir le boîtier) : \`J1\` ouvert" \
  "juste avant de le poser" "on le note, et on en rediscute avant de relier un Mac ou l'analyseur"
absent "$R" "+4,7 à +5,0 V"

# --- Tableaux : chaque ligne a autant de colonnes que l'en-tete de son tableau ---
for f in "$S" "$W" "$R"; do
  n=$((n + 1))
  if [ ! -f "$f" ]; then
    e=$((e + 1))
    echo "ABSENT : $f"
    continue
  fi
  mal=$(awk '/^\|/ { c = gsub(/\|/, "|"); if (!t) t = c; else if (c != t) printf "%d ", FNR; next } { t = 0 }' "$f")
  if [ -n "$mal" ]; then
    e=$((e + 1))
    echo "TABLEAU MAL FORME dans $f, lignes : $mal"
  fi
done

# --- Liens relatifs : chaque cible existe (chemins relatifs a docs/) ---
for f in "$S" "$W" "$R"; do
  [ -f "$f" ] || continue
  for cible in $(grep -oE '\]\([^)#]+' "$f" | cut -c3- | sort -u); do
    n=$((n + 1))
    if [ ! -e "docs/$cible" ]; then
      e=$((e + 1))
      echo "LIEN CASSE dans $f : $cible"
    fi
  done
done

echo "verif_terrain : $n verifications, $e echecs"
[ "$e" -eq 0 ]
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer (depuis la racine du dépôt) : `sh "${TMPDIR:-/tmp}/verif_terrain.sh"; echo "code=$?"`

Attendu : ÉCHEC, code 1. Les premières lignes sont
« MANQUE dans docs/SECURITE.md : regle 1 », « MANQUE dans docs/SECURITE.md : regle 2 »… ;
les valeurs remplacées donnent « RIEN A RELIRE dans docs/WIRING.md : +4,7 à +5,0 V »
et ses pareilles ; avant la fin, « ABSENT : docs/SECURITE.md », « ABSENT : docs/WIRING.md »,
« ABSENT : docs/RECONNAISSANCE.md » ; dernière ligne :
« verif_terrain : 323 verifications, 323 echecs » (les liens ne sont comptés que
pour les fichiers présents).

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Documentation en français **avec** accents (`docs/`). Copier les trois blocs tels
quels : le script de l'étape 1 vérifie leurs valeurs à l'octet près, et les
schémas sont alignés en police à chasse fixe (ne pas les ré-indenter).

**3a. Créer `docs/SECURITE.md`** (fiche courte à garder sous les yeux, puis les
procédures des étapes 1, 2b et 3a pas à pas) :

````markdown
# Fiche de sécurité

**Sous-projet 1 : reconnaissance de la ligne `D`.** À imprimer et à garder sous
les yeux à chaque séance. Cette fiche reprend les règles 1 à 13 du §5 de
[SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md), puis les trois contrôles
d'isolement pas à pas (étapes 1, 2b et 3a du §6). **La spec fait foi** : si la
fiche la contredit, on corrige la fiche.

Montage et points de test : [WIRING.md](WIRING.md). Relevés, décisions et
anomalies : [RECONNAISSANCE.md](RECONNAISSANCE.md).

## Les 13 règles

**Débrancher, attendre, ne pas passer la main**

1. **Pour toute intervention, on débranche la fiche et on la garde à vue, hors
   de portée d'autrui.**
   - Avant d'ouvrir le boîtier électronique ou d'y passer la main : **5 minutes**
     d'attente. Côté secteur, E5 et E6 (4,7 µF / 450 V) n'ont pas forcément de
     résistance de décharge.
   - Intervention limitée au boîtier de mesure extérieur : fiche débranchée, sans attente.
2. **Avant la première mise sous tension d'un montage, on teste le différentiel.**
   On le teste juste avant de poser ce montage, hotte dans son état précédent,
   déjà contrôlé ; puis on débranche pour poser. Hotte branchée, lumière
   allumée, bouton T : **la lumière doit s'éteindre**. Sinon, la hotte n'est pas
   derrière ce différentiel : **ARRÊT**. Réarmer.
3. **Dans le boîtier électronique, on ne touche qu'à `CN3` et au câble du panneau.**
   - Hotte débranchée depuis 5 minutes, on peut écarter à la main les fils isolés
     du condensateur moteur et le connecteur blanc en ligne, sans les débrancher
     ni tirer dessus. Photo avant, remise en place après.
   - Jamais `CN1` ni `CN2` dans ce sous-projet.
   - Zones interdites ([photos/1.jpg](photos/1.jpg), [photos/5.jpg](photos/5.jpg),
     [photos/6.jpg](photos/6.jpg), [recadrages/c4_bottom.jpg](photos/recadrages/c4_bottom.jpg)) :
     la moitié gauche de la carte (primaire) et le transformateur ; toute la bande
     du bas (relais, `VH2` à `VH6`, « LAMP », `AC-POWER`) ; le condensateur moteur.
4. **Aucune connexion ne se pose ni ne se retire sous tension :** connecteur,
   pince, résistance, cavalier, sonde, câble USB.
5. **Câble de l'adaptateur :**
   - fil d'au moins 300 V (UL1007 ou UL1015, 105 °C de préférence) dans le corps
     de la hotte et au passage de sortie ;
   - gainé **sur toute sa longueur** : gaine équivalente à celle d'origine
     (VW-1 600 V), ou deux épaisseurs de gaine thermorétractable ;
   - fixé au câble du panneau, jamais serré contre les fils du moteur ni contre
     le connecteur blanc ;
   - pincé ni par le couvercle du boîtier, ni par le filtre.

   **Couvercle fermé et filtre remis avant de rebrancher.**
6. **Aucune main dans la hotte sous tension.** Sous tension, on ne touche qu'aux
   touches du panneau, à la fiche, et au boîtier de mesure extérieur, où l'on lit
   les instruments.

**Portes d'isolement**

7. **Avant toute mise sous tension d'un montage, trois contrôles passent, dans l'ordre :**
   1. le test d'isolation (étape 1) ;
   2. son nouveau passage depuis les points de test (étape 2b), après la pose de
      l'adaptateur et après **chaque** ouverture du boîtier ;
   3. la mesure de fuite en charge (étape 3a), **avant tout Mac ou analyseur**.
8. **Mac et analyseur :** on ne les relie à la hotte sous tension qu'après les trois
   contrôles de la règle 7, et dans ces conditions :
   - pendant toute la session, **Mac sur batterie et sans aucun autre câble** :
     ni chargeur, ni écran, ni Ethernet, ni hub alimenté, ni audio ;
   - on ne rebranche le chargeur qu'après avoir débranché la hotte ou retiré
     l'analyseur ;
   - Mac posé sur une surface isolante, loin de l'acier de la hotte et de l'évier ;
   - on ne touche jamais en même temps le Mac et une pièce métallique reliée à la terre.

**Sonde et émission**

9. **Pendant les captures sous tension, la sonde est sur batterie USB**, d'un
   modèle vérifié (§12 de la spec), et ses journaux partent par Wi-Fi. Sa seule
   liaison avec la hotte est le câble de sortie de l'adaptateur.
10. **Jamais une tension du bus directement sur une GPIO** : le C6 ne tolère pas
    plus d'environ 3,6 V. On passe toujours par l'étage d'écoute (WIRING.md §5).
11. **Toute émission vers la hotte (étape 7) se fait en présence de Majid.** Il se
    tient à portée de la fiche, voit le panneau, et débranche au moindre doute.

**En cas de problème**

12. **À la moindre anomalie, on débranche la fiche.** Anomalies : déclenchement
    du différentiel ou du disjoncteur, odeur, fumée, claquements de relais
    répétés, bip continu, panneau mort, fil chaud. On ne réarme pas et on ne
    rebranche pas avant d'avoir compris la cause, notée dans `RECONNAISSANCE.md`.
13. **Pas de cuisson tant que la sonde et sa batterie sont en place.** On les
    retire entre deux sessions. L'adaptateur, gainé, avec `J1` fermé, peut rester
    en place.

> **Une lecture qui ne prouve rien.** Entre le secondaire flottant et la terre,
> un multimètre de 10 MΩ peut afficher à vide jusqu'à une centaine de volts
> alternatifs, à cause des condensateurs Y. Ce chiffre ne prouve rien, ni dans un
> sens ni dans l'autre : un défaut franc donnerait à peu près la même lecture.
> **Seule compte la mesure en charge de l'étape 3a.**

## Avant chaque mise sous tension

- [ ] Étape 1 réussie (une fois pour toutes).
- [ ] Étape 2b refaite depuis la dernière ouverture du boîtier (règle 7).
- [ ] Étape 3a réussie (sauf pour l'essai à vide de l'étape 2c, qui la précède) ;
      toujours avant de relier un Mac ou l'analyseur (règle 7). Lecture entre 3 et
      5 V : rediscutée avant de relier un Mac ou l'analyseur.
- [ ] Montage jamais mis sous tension : différentiel testé juste avant de le
      poser, hotte dans son état précédent (règle 2).
- [ ] Toutes les connexions posées hotte débranchée (règle 4).
- [ ] Couvercle fermé, filtre remis, câble de sortie ni pincé ni serré contre les
      fils du moteur (règle 5).
- [ ] `J2` ouvert (sauf étape 4, qui se fait hotte débranchée).
- [ ] Captures : sonde sur batterie ; sa seule liaison avec la hotte est le câble
      de sortie (règle 9).
- [ ] Mac ou analyseur relié : Mac sur batterie, sans aucun autre câble, sur une
      surface isolante (règle 8).
- [ ] Étape 7 : Majid devant la hotte, à portée de la fiche (règle 11).

## Banc de validation : aucune liaison avec la hotte

Le banc (WIRING.md §10) n'a **jamais** de liaison avec la hotte : ni le
générateur, ni la sonde, ni le Mac. La sonde y est :
- soit sur le boîtier de mesure, seulement **hotte débranchée pendant toute la
  séance** et **câble de sortie déconnecté du boîtier** (fiche XH 4 broches
  retirée de l'embase, hotte débranchée). Avant de brancher le moindre USB,
  contrôle hors tension, au calibre le plus élevé : `TP−` vers la terre de la
  fiche de la hotte (le trou qui reçoit la broche de terre), puis vers la vis de
  la carcasse : **OL** les deux fois. Sinon, on ne branche rien ;
- soit sur une plaque d'essai du banc, avec son propre étage d'écoute (mêmes
  valeurs, WIRING.md §5).

Le boîtier ne retourne sur la hotte (fiche XH 4 broches remise, hotte
débranchée) qu'une fois le banc démonté, et après ses contrôles avant pose
(WIRING.md §12, lignes 11 à 17).

## En cas d'anomalie

1. **Débrancher la fiche**, tout de suite, et la garder à vue. Si le différentiel
   ou le disjoncteur a déclenché : ne pas réarmer.
2. Ne toucher ni la hotte ni le boîtier de mesure avant d'avoir débranché (règle 6).
3. Noter aussitôt dans le journal des anomalies de `RECONNAISSANCE.md` : l'heure,
   l'étape, ce qui était branché, ce qu'on a vu, entendu ou senti.
4. Chercher la cause hotte débranchée. Pour ouvrir le boîtier : 5 minutes
   d'attente (règle 1), puis refaire l'étape 2b avant de rebrancher (règle 7).
5. On ne réarme et on ne rebranche qu'une fois la cause comprise et notée (règle 12).

**Quand un contrôle dit ARRÊT :** on ne raccorde rien et on ne met pas le montage
sous tension ; on note la valeur dans `RECONNAISSANCE.md` ; on revient à la
conception avant d'aller plus loin.

## Procédure : test du différentiel (règle 2)

**Quand :** avant la première mise sous tension de chaque montage, juste avant
de le poser. On le fait hotte dans son état précédent, déjà contrôlé, puis on
débranche pour poser le nouveau montage : avant de poser l'adaptateur (sa
première mise sous tension est l'étape 2c), puis avant la fuite en charge (3a),
la sonde (5), l'analyseur (5b) et l'étage d'injection (7).

1. Brancher la hotte, allumer la lumière au panneau.
2. Appuyer sur le bouton T du différentiel 30 mA.
3. **La lumière doit s'éteindre.** Sinon : la hotte n'est pas derrière ce
   différentiel, **ARRÊT**.
4. Réarmer. Noter la date dans `RECONNAISSANCE.md` (tests du différentiel).
5. Débrancher la hotte, puis poser le nouveau montage (règle 4).

## Procédure : étape 1, test d'isolation

- **Quand :** une fois, avant de construire l'adaptateur.
- **État :** hotte débranchée ; on ouvre le boîtier, donc débranchée depuis
  **5 minutes** (règle 1).
- **Outils :** multimètre, fil à pinces crocodile, aiguille fine.

1. Débrancher la fiche, noter l'heure, attendre 5 minutes.
2. Retirer le filtre, ouvrir le couvercle du boîtier électronique. On ne touche
   qu'à `CN3` et au câble du panneau (règle 3).
3. Relier les deux broches rondes de la fiche par le fil à pinces crocodile.
4. Piquer `CN3` par l'arrière de la fiche, côté fils, avec l'aiguille, sur la
   broche à mesurer.
5. Ne toucher aucune partie métallique des pointes : le corps en parallèle
   fausserait la mesure.
6. Mesurer, et noter chaque lecture dans `RECONNAISSANCE.md` :

| # | Mesure | Calibre | Attendu | Sinon |
|---|---|---|---|---|
| a | broches reliées de la fiche vers `CN3 −`, puis vers `D`, puis vers `+`, chaque fois dans les deux sens (cordons inversés) | le plus élevé (20 MΩ ou plus) | **OL** dans les deux sens, après 10 s. Une valeur qui monte vers OL est normale : les condensateurs Y se chargent | toute valeur stable : **ARRÊT**, aucun raccordement, retour à la conception |
| b | contact de terre de la fiche (dans le trou qui reçoit la broche de terre de la prise) vers `CN3 −` | le plus élevé | OL : secondaire flottant | environ 0 Ω : secondaire relié à la terre ; ce n'est pas un défaut, on le note. Valeur intermédiaire (kΩ à MΩ) : **ARRÊT** et analyse |
| c | contact de terre de la fiche vers la vis de terre de la carcasse, ou une vis nue (la carcasse est peinte) | 200 Ω ; cordons court-circuités d'abord (valeur à soustraire) | **moins de 1 Ω** : liaison de classe I | **ARRÊT** : défaut de terre |

7. Retirer l'aiguille et le fil à pinces, refermer le couvercle, remettre le
   filtre sans rien pincer.

**Limite.** Un ohmmètre ne voit pas un claquage qui n'apparaîtrait qu'à 230 V.
La mesure en charge de l'étape 3a couvre une partie de ce risque.

**Sortie :** feu vert pour construire l'adaptateur, ou **ARRÊT**.

## Procédure : étape 2b, isolation depuis les points de test

- **Quand :** après la pose de l'adaptateur (étape 2), après **chaque** ouverture
  du boîtier (règle 7), et à la sortie de l'étape 4 (couvercle fermé et filtre
  remis d'abord).
- **État :** hotte débranchée, couvercle fermé, filtre remis ; fiche XH 4 br. du
  câble de sortie enfichée sur le boîtier de mesure (retirée, les points de test
  liraient OL sans rien prouver).
- **Outils :** multimètre, fil à pinces crocodile, grippe-fils.

**Mesure 1 : isolation (à chaque passage).**

1. Relier les deux broches rondes de la fiche par le fil à pinces crocodile.
2. Sur le boîtier de mesure, mesurer depuis les broches reliées vers chaque point
   de test, dans les deux sens :

| # | Mesure | Calibre | Attendu | Sinon |
|---|---|---|---|---|
| 1 | broches reliées ↔ `TP−` | le plus élevé (20 MΩ ou plus) | **OL** dans les deux sens, après 10 s | **ARRÊT** |
| 2 | broches reliées ↔ `TP+` | idem | **OL** | **ARRÊT** |
| 3 | broches reliées ↔ `TP_Dp` | idem | **OL** | **ARRÊT** |
| 4 | broches reliées ↔ `TP_Dc` | idem | **OL** | **ARRÊT** |

3. Retirer le fil à pinces.

**Mesure 2 : où est le pull-up de `D` (premier passage seulement).**

1. Hotte débranchée, filtre retiré, sans ouvrir le boîtier : ouvrir `J1`.
2. Mesurer, au calibre 20 kΩ ou automatique, dans les deux sens :
   `TP_Dp` vers `TP+`, `TP_Dp` vers `TP−`, puis `TP_Dc` vers `TP+`, `TP_Dc` vers `TP−`.
   Quelques kΩ vers `+` indiquent un pull-up, et son côté : `TP_Dp` côté panneau,
   `TP_Dc` côté carte. Entre `TP_Dp` et `TP−`, l'étage d'écoute ajoute de l'ordre
   de 100 à 200 kΩ selon le sens : cette lecture-là n'indique rien.
3. Refermer `J1`. Remettre le filtre sans rien pincer.

**Sortie :** tout est OL, le côté du pull-up est noté. Sinon, **ARRÊT**.

## Procédure : étape 3a, fuite en charge

- **Quand :** une fois, après les étapes 2b et 2c, **avant tout Mac ou
  analyseur** (règle 7).
- **État :** hotte débranchée pour poser les pinces ; une brève mise sous tension
  pour la lecture.
- **Outils :** multimètre, deux grippe-fils, résistance 10 kΩ / 2 W.

La résistance charge la mesure : la lecture donne le vrai courant de fuite, là
où la lecture à vide ne prouve rien (encadré plus haut).

1. Tester le différentiel (procédure plus haut), puis débrancher.
2. Hotte débranchée, fiche XH 4 br. du câble de sortie enfichée sur le boîtier
   (retirée, la lecture ne prouverait rien) : pince COM sur `TP−`, l'autre pince
   sur la vis de terre de la carcasse (celle de l'étape 1, ligne c).
3. Poser la résistance de 10 kΩ / 2 W en parallèle entre les deux pinces.
   Multimètre en tension alternative.
4. Brancher la hotte, lire dans les 5 s, débrancher.
5. Courant de fuite = tension lue / 10 kΩ.
6. Hotte débranchée : retirer les pinces et la résistance.

| Lecture | Décision |
|---|---|
| 3 V au plus (0,3 mA au plus) | attendu : on continue |
| entre 3 et 5 V | au-dessus de l'attendu : on le note, et on en rediscute avant de relier un Mac ou l'analyseur |
| au-delà de 5 V (0,5 mA), ou différentiel déclenché | **ARRÊT** |
````

**3b. Créer `docs/WIRING.md`** (schémas ASCII, nomenclature, brochage, contrôles
avant pose) :

````markdown
# Câblage de la sonde

**Sous-projet 1 : reconnaissance de la ligne `D`.** Adaptateur, câble de sortie,
boîtier de mesure, étages d'écoute et d'injection, brochage du C6 SuperMini,
générateur du banc. Source : §7, §10 et §12 de
[SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md), qui fait foi. Règles de
sécurité : [SECURITE.md](SECURITE.md) (en particulier les règles 4, 5, 9 et 10,
et le banc sans liaison avec la hotte). Les relevés vont dans
[RECONNAISSANCE.md](RECONNAISSANCE.md).

## 1. Vue d'ensemble

L'ensemble a trois parties, pour que **tout ce qu'on mesure ou manipule soit
hors de la hotte**.

```
   DANS LE BOÎTIER / LE CORPS DE LA HOTTE                    HORS DE LA HOTTE, à vue
 ┌───────────────────────────────────────────┐          ┌────────────────────────────────────┐
 │ ADAPTATEUR (petite plaque, gainée)        │          │ BOÎTIER DE MESURE (plaque à        │
 │                                           │          │ pastilles, fixée, détachable)      │
 │  sorties : −, +, D_panneau, D_carte       ├─────────►│  embase XH 4 br. ◄ fiche du câble  │
 │                                           │  câble   │  TP−  TP+  TP_Dp  TP_Dc            │
 │  fiche XH 3 br. ──► CN3 [- D +]           │  de      │  J2 : TP+ ── 5V de la sonde        │
 │  embase XH 3 br. ◄── fiche du panneau     │  sortie, │       (ouvert par défaut)          │
 │                                           │  4 fils, │  étage d'écoute voie 1 → GPIO6     │
 │  D_carte ──┤J1├── D_panneau               │  ≥300 V, │  étage d'injection voie 1 ← GPIO7  │
 │   (fermé par défaut ; ouvert = B)         │  gainé ; │  (voie 2 : GPIO0 / GPIO1, si B)    │
 │                                           │  fiche   │  ESP32-C6 SuperMini sur barrettes  │
 └───────────────────────────────────────────┘  XH 4 br.│  batterie USB (captures)           │
                                                        └────────────────────────────────────┘
```

- **Réversible :** on retire l'adaptateur, on rebranche la fiche du panneau sur
  `CN3`, et la hotte est d'origine.
- **Détachable :** le câble de sortie se branche sur le boîtier de mesure par une
  fiche et une embase XH 4 broches (§3 et §4). On ne l'enfiche et on ne la
  retire que hotte débranchée (règle 4). Fiche retirée, le boîtier n'a plus
  aucune liaison avec la hotte : c'est ainsi qu'il peut servir au banc (§10).
- **La sonde n'est jamais alimentée par la hotte** : `J2` reste ouvert, sauf à
  l'étape 4 (hotte débranchée).
- **Jamais une tension du bus directement sur une GPIO** (règle 10) : `D` passe
  toujours par l'étage d'écoute.

## 2. Adaptateur

Petite plaque, **gainée**, qui reste près de `CN3`. Elle s'intercale entre `CN3`
et la fiche du panneau, et sort les quatre nœuds vers le câble de sortie.

- **Connectique, décidée à l'étape 0** par le pas de `CN3`, mesuré entre ses
  broches extrêmes : 5,0 mm pour 3 broches, c'est du JST XH (pas de 2,5 mm) ;
  4,0 mm, c'est du JST PH (pas de 2,0 mm). Fiches et embases précâblées, contacts
  sertis.
- **Fiche XH 3 broches** enfichée sur `CN3` ; **embase XH 3 broches** qui reçoit
  la fiche du panneau. Les contacts de la fiche sont insérés **dans le même ordre
  que ceux de la fiche du panneau** : noir `−`, blanc `D`, rouge `+` (sérigraphie
  « - D + »), détrompeur du même côté.
- **Pas de broches Dupont** dans les fiches du panneau et de `CN3` d'origine :
  leurs contacts XH se desserrent.
- **`J1`** : barrette 2 broches au pas de 2,54 mm et un cavalier, entre `D_carte`
  et `D_panneau`. **Fermé par défaut** ; ouvert seulement pour la mesure du
  pull-up (étape 2b) et pour l'architecture B.
- **Placement :** `J1` doit rester accessible filtre retiré, **sans ouvrir le
  boîtier** électronique (étape 2b). Si la fiche du panneau peut sortir du boîtier
  (étape 0), la plaque se place juste à la sortie, et une fiche précâblée courte
  va jusqu'à `CN3`.

```
  CÔTÉ CARTE : fiche XH 3 br. sur CN3             CÔTÉ PANNEAU : embase XH 3 br.
  (ordre des broches sur CN3 : − D +)             (reçoit la fiche du panneau)

  −  noir   ○──────────────────────────────────────○  −  noir   ─────► fil 1 : −
  +  rouge  ○──────────────────────────────────────○  +  rouge  ─────► fil 2 : +
  D  blanc  ○── D_carte ──●──┤J1├──●── D_panneau ──○  D  blanc
                          │        │
                          │        └───────────────────────────────► fil 3 : D_panneau
                          └────────────────────────────────────────► fil 4 : D_carte
```

| Nœud | Côté carte (fiche sur `CN3`) | Côté panneau (embase) | Câble de sortie | Boîtier de mesure |
|---|---|---|---|---|
| `−` | broche `−`, fil noir | broche `−` | fil 1 | `TP−` |
| `+` | broche `+`, fil rouge | broche `+` | fil 2 | `TP+` |
| `D_panneau` | (par `J1`) | broche `D` | fil 3 | `TP_Dp` |
| `D_carte` | broche `D` | (par `J1`) | fil 4 | `TP_Dc` |

## 3. Câble de sortie

**4 fils** (`−`, `+`, `D_panneau`, `D_carte`), conformes à la règle 5 :

- fil d'au moins 300 V, UL1007 ou UL1015, 105 °C de préférence ;
- glissé **sur toute sa longueur** dans une gaine équivalente à celle d'origine
  (VW-1 600 V), ou dans deux épaisseurs de gaine thermorétractable (3 à 6 mm) ;
- posé le long du cheminement choisi à l'étape 0, fixé au câble du panneau par
  des colliers ; jamais serré contre les fils du moteur ni contre le connecteur
  blanc ; pincé ni par le couvercle du boîtier, ni par le filtre ;
- longueur : celle mesurée à l'étape 0, plus une marge ;
- terminé, côté boîtier de mesure, par une **fiche XH 4 broches** précâblée :
  contacts dans l'ordre `−`, `+`, `D_panneau`, `D_carte` (broches 1 à 4), avec
  son détrompeur. Ses fils sertis sont soudés aux quatre fils du câble, chaque
  soudure sous gaine thermorétractable, hors de la hotte. Elle s'enfiche sur
  l'embase XH 4 broches du boîtier (§4), dans un seul sens.

Couleurs proposées (les couleurs réelles sont notées dans `RECONNAISSANCE.md`) :

| Fil | Nœud | Couleur proposée | Broche de la fiche XH 4 br. | Arrive sur |
|---|---|---|---|---|
| 1 | `−` | noir | 1 | `TP−` |
| 2 | `+` | rouge | 2 | `TP+` |
| 3 | `D_panneau` | blanc | 3 | `TP_Dp` |
| 4 | `D_carte` | jaune (ou toute autre couleur distincte) | 4 | `TP_Dc` |

## 4. Boîtier de mesure

Plaque à pastilles **fixée à côté de la hotte, à vue, hors de l'aplomb de la
plaque de cuisson** (emplacement choisi à l'étape 0). On y pose et on y déplace
les pinces hotte débranchée ; sous tension, on n'y fait que lire les instruments.

- **Embase XH 4 broches**, soudée sur la plaque, qui reçoit la fiche du câble de
  sortie (§3) : broche 1 sur `TP−`, 2 sur `TP+`, 3 sur `TP_Dp`, 4 sur `TP_Dc`,
  détrompeur tourné comme celui de la fiche. On enfiche et on retire la fiche
  hotte débranchée (règle 4) ; retirée, le boîtier est détaché de la hotte (§10).
- **Points de test** `TP−`, `TP+`, `TP_Dp`, `TP_Dc` : des picots (barrette mâle
  2,54 mm) où l'on accroche les grippe-fils.
- **`J2`** : barrette 2 broches et un cavalier, entre `TP+` et la broche `5V` de la
  sonde. **Ouvert par défaut**, fermé seulement à l'étape 4 (panneau seul, hotte
  débranchée) : le 5 V de l'USB de la sonde alimente alors le panneau.
- **Étage d'écoute, voie 1** (§5), et **étage d'injection, voie 1** (§6), monté à
  l'étape 7 seulement. Place réservée pour la voie 2 (§7), si B.
- **La sonde**, ESP32-C6 SuperMini, sur deux barrettes femelles. Son `GND` est
  relié à `TP−` : masse commune avec le `−` du bus, indispensable aux étages.
- **Batterie USB** pendant les captures (modèle vérifié, règle 9).
- La coque de l'USB-C du SuperMini est reliée à `GND` : l'isoler de tout métal
  (gaine ou ruban). Le connecteur USB-C reste accessible, pour flasher hotte
  débranchée. On relève le RSSI Wi-Fi à cet emplacement.

```
  câble de sortie   XH 4 br.   BOÎTIER DE MESURE (hors de la hotte, à vue)
                  fiche  embase

  fil 1 : −          ──► 1 ──●  TP−   ────────────────────────────── GND de la sonde
  fil 2 : +          ──► 2 ──●  TP+   ──┤J2├───────────────────────── 5V de la sonde
                                        ouvert (fermé à l'étape 4 seulement)
  fil 3 : D_panneau  ──► 3 ──●  TP_Dp ─┬─ étage d'écoute, voie 1 ───────► GPIO6
                                       └─ étage d'injection, voie 1 ◄─── GPIO7  (étape 7)
  fil 4 : D_carte    ──► 4 ──●  TP_Dc     (voie 2 : GPIO0 / GPIO1, seulement si B)

  1 à 4 : broches de la fiche et de l'embase XH 4 broches, détrompeur en face.
  Les émetteurs des transistors des deux étages sont sur TP− (§5 et §6).
```

## 5. Étage d'écoute (voie 1 : `D_panneau` vers GPIO6)

```
  TP_Dp ── 100k ──┬──────────── base        ┐
                  │                         │
                 R_be                       │  Q1 : BC547 ou 2N3904
            (100k par défaut)               │
                  │                         │
  TP− ────────────┴──────────── émetteur    │
                                            │
  3V3 (sonde) ── 10k ──┬─────── collecteur  ┘
                       │
                       └───────────────────────► GPIO6     (D haut → GPIO6 bas)
```

- **Charge et comportement :** il ne prélève que 40 à 110 µA sur `D` ; il commute
  en 1 à 2 µs ; il reste neutre si la sonde est éteinte ; il fonctionne tel quel
  sur un bus de 5 V comme de 12 V.
- **Sa logique est inversée** : GPIO6 bas = bus haut. Le firmware remet les
  niveaux dans le sens du bus avant de les publier.
- **Seuil : environ 1,2 V, fixe.** Il ne suit pas la tension du bus et baisse à
  chaud, d'environ 4 mV/°C. Il ne convient donc que si le **niveau bas de `D`**
  reste sous 0,6 V environ, ce qu'on juge à l'étape 5b. L'analyseur FX2 est
  numérique et ne mesure pas de tension : par défaut, le jugement est
  fonctionnel (capture propre, sonde et analyseur d'accord sur la voie 1, chaque
  impulsion de `D` retrouvée à la sortie de l'étage) ; une mesure chiffrée, à
  l'oscilloscope, est optionnelle. Si le jugement échoue, ou si la mesure dépasse
  0,6 V, on relève le seuil par `R_be` :

| `+` (étape 3b) | Niveau bas de `D` (étape 5b) | `R_be` | Seuil sur `D`, environ |
|---|---|---|---|
| 5 V ou 12 V | jugé bon (sous 0,6 V, s'il est mesuré) | 100k (défaut) | 1,2 V |
| 5 V | jugé trop haut (au-dessus de 0,6 V, s'il est mesuré) | 47k | environ 2 V |
| 12 V | jugé trop haut (au-dessus de 0,6 V, s'il est mesuré) | 15k | environ 5 V |

- On garde au moins 25 µA de base au niveau haut (calcul : environ 30 µA avec 47k
  sous 5 V, 70 µA avec 15k sous 12 V).
- **Délai et asymétrie** de l'étage : mesurés au banc (§10).
- Le pilote RMT active le pull-up interne de GPIO6 : sans effet avec le 10k de
  collecteur.

## 6. Étage d'injection (voie 1 : GPIO7 vers `D_panneau`), étape 7 seulement

```
  GPIO7 ── 4,7k ──┬──────────── base        ┐
                  │                         │
                 10k                        │  Q2 : BC547 ou 2N3904
                  │                         │
  TP− ────────────┴──────────── émetteur    │
                                            │
  TP_Dp ── 470 Ω ────────────── collecteur  ┘     (GPIO7 haut → D tiré bas)
```

- **Uniquement en drain ouvert :** on ne pilote jamais `D` en push-pull 3,3 V.
- **Le 10k entre base et émetteur garde Q2 bloqué** pendant le reset, et quand la
  broche est libérée. Le firmware met GPIO7 à l'état bas en toute première
  instruction de `setup()`.
- **Les 470 Ω de collecteur limitent le courant** à environ 10 mA sous 5 V, si une
  sortie active tient `D` haut en face (une puce tactile de type WTC6534 pilote
  elle-même la ligne pour émettre). Avec un pull-up de 4,7k, le niveau bas vaut
  environ 0,5 V. On le juge à l'étape 7, point 4 : fonctionnellement (la carte
  réagit à l'injection), ou, avec un oscilloscope, par la mesure : il doit rester
  sous 0,8 V. On ne descend vers 220 Ω que si ce niveau n'est pas atteint
  (`injection off` et hotte débranchée d'abord).
- **Ne jamais intervertir GPIO6 et GPIO7.** GPIO6 a un pull-up d'environ 45 kΩ
  après le reset : branché sur l'injection, il débloquerait Q2 et tirerait `D` à
  l'état bas.
- Tant que l'étage n'est pas monté, rien ne relie GPIO7 à `D`.

## 7. Voie 2 (`D_carte`), seulement si B

Rien à construire avant l'avenant de l'étape 6. Pour mémoire (§7.4 de la spec) :

- le même étage d'écoute, sur `TP_Dc`, vers GPIO0 ; l'émission sur GPIO1 ;
- `J1` ouvert : chaque tronçon ne garde que le pull-up de son côté. L'avenant
  fixe un pull-up posé côté tronçon qui n'en a pas, de la valeur relevée à
  l'étape 2b ;
- en émission, si `D` est push-pull : un tampon push-pull alimenté par `+`
  (74HCT si `+` = 5 V) au lieu du NPN ;
- le relais : front par front, ou la sonde qui répond elle-même au panneau.

## 8. Analyseur : voies et diviseurs

Étape 5b (validation croisée) et étape 7 (niveau bas de l'injection). Pinces
posées hotte débranchée, **masse de l'analyseur d'abord, sur `TP−`**, puis son
USB branché au Mac, hotte toujours débranchée (règle 4). Mac sur batterie, sans
aucun autre câble (règle 8).

L'analyseur FX2 est **numérique** : il dit si une voie est au-dessus ou
au-dessous de son seuil, pas à quelle tension. La voie 2 montre donc les
impulsions de `D` et leurs fronts, pas la valeur du niveau bas : celui-ci se juge
fonctionnellement (étapes 5b et 7), et ne se chiffre qu'avec un oscilloscope.

- **Voie 1** : sortie de l'étage d'écoute, le même nœud que GPIO6 (3,3 V), en direct.
- **Voie 2** : `TP_Dp` à travers un diviseur à haute impédance choisi d'après `+` :
  47k/68k pour 5 V (5 V donne 2,96 V), 100k/33k pour 12 V (12 V donne 2,98 V).

```
  TP_Dp ── 47k (100k si + = 12 V) ──┬───────► voie 2 de l'analyseur
                                    │
                           68k (33k si + = 12 V)
                                    │
  TP− ──────────────────────────────┴───────► masse de l'analyseur
```

## 9. Brochage du C6 SuperMini

| Broche | Reliée à | Remarque |
|---|---|---|
| GPIO6 | collecteur de Q1 (étage d'écoute) | entrée : RMT en réception, plus une interruption sur les deux fronts |
| GPIO7 | 4,7k de base de Q2 (étage d'injection) | sortie à l'état bas dès la première instruction de `setup()` ; rien de branché avant l'étape 7 (au banc : critère 5) |
| 3V3 | 10k de collecteur de Q1 | |
| GND | `TP−` | masse commune avec le `−` du bus |
| 5V | `J2` (ouvert), puis `TP+` | fermé seulement à l'étape 4 |
| GPIO0, GPIO1 | voie 2, seulement si B | écoute sur GPIO0, émission sur GPIO1 |

Sur le second C6 (générateur du banc, §10) : GPIO7 sur le 4,7k de base de Q3.

**Broches à éviter :**

| GPIO | Pourquoi |
|---|---|
| 4, 5, 8, 9, 15 | broches de démarrage du C6 (8 : LED WS2812 ; 9 : bouton BOOT) |
| 12, 13 | USB natif |
| 16, 17 | UART0 : journal de la ROM au démarrage |
| 21, 22 | trous intérieurs, inaccessibles |

GPIO2 reste libre.

**Repérage.** On repère chaque broche par sa sérigraphie. D'après le brief du
boîtier de la ScreenBar (même carte), une rangée extérieure porte, dans l'ordre,
`6 · 14 · 15 · 18 · 19 · 20 · 3V3 · GND · 5V`. GPIO7 se repère sur la sérigraphie.

**Contrôle électrique du module**, avant de le poser sur les barrettes : module
seul sur l'USB du Mac, rien d'autre de branché, firmware `sonde` chargé. COM du
multimètre sur `GND`, calibre 20 V continu.

| Mesure | Attendu | Sinon |
|---|---|---|
| broche marquée 6 | environ 3,3 V (pull-up interne) | sérigraphie mal lue : ne rien souder |
| broche marquée 7 | 0 V (sortie à l'état bas) | idem |
| broche `5V` | noter la valeur : 5,0 V environ si VBUS est relié directement, 4,7 V environ s'il passe par une diode | |

## 10. Générateur du banc

Banc de validation, avant la hotte (§10 de la spec). Le second C6, avec le
firmware `generateur`, imite un bus de 5 V à drain ouvert. Aucun secteur au banc.

**Le banc n'a jamais de liaison avec la hotte.** La sonde s'y monte de l'une de
ces deux façons :
- **sur le boîtier de mesure**, seulement si la hotte reste débranchée pendant
  toute la séance **et** si le câble de sortie est déconnecté du boîtier (fiche
  XH 4 broches retirée de l'embase, hotte débranchée). Avant de brancher le
  moindre USB, contrôle hors tension, au calibre le plus élevé : `TP−` vers la
  terre de la fiche de la hotte (le trou qui reçoit la broche de terre), puis
  vers la vis de la carcasse : **OL** les deux fois. Sinon, on ne branche rien ;
- **sur une plaque d'essai du banc**, avec son propre étage d'écoute (mêmes
  valeurs qu'au §5 : 100k, `R_be` de 100k, 10k de collecteur).

Ci-dessous, `TP_Dp` et `TP−` désignent l'entrée de cet étage d'écoute et son `−`,
sur le boîtier ou sur la plaque d'essai.

```
  5V (générateur) ── R_pu ──┬─────────────────────────────► LIGNE ──► TP_Dp de la sonde
                            │                                         (étage d'écoute)
                            ├── 1 nF ── GND  (variante 2 seulement)
                            │
                            └────────────── collecteur  ┐
  GPIO7 (générateur) ── 4,7k ──┬─────────── base        │  Q3 : BC547 ou 2N3904
                               │                        │
                              10k                       │
                               │                        │
  GND (générateur) ──┬─────────┴─────────── émetteur    ┘
                     │
                     └──────────────────────────────────────► TP− de la sonde (fil de masse)

  R_pu : 10k seul (variante 1), ou 4,7k avec 1 nF vers la masse (variante 2).
```

- **Fil de masse** entre le `GND` du générateur et le `−` de l'étage d'écoute de
  la sonde (`TP−`).
- **Ligne** : pull-up vers le `5V` du générateur. Variante 1 : 10k seul.
  Variante 2 : 4,7k avec 1 nF vers la masse, pour imiter le filtre supposé de la
  carte.
- **NPN du générateur** : le montage du §6 (4,7k en base, 10k entre base et
  émetteur), collecteur directement sur la ligne. Logique inversée :
  GPIO7 haut = ligne basse.
- **Collision (critère 5)** : l'étage d'injection de la sonde est monté au banc,
  son collecteur (470 Ω) aussi sur la LIGNE. Monté sur le boîtier de mesure, il
  en est retiré avant que le boîtier retourne sur la hotte (§6 : étape 7
  seulement).
- **Retour sur la hotte** (boîtier de mesure seulement) : banc démonté, puis les
  contrôles du boîtier du §12 (lignes 11 à 17), puis la fiche XH 4 broches
  remise, hotte débranchée.
- **Décalage et asymétrie de l'étage (critère 2)** : une voie de l'analyseur sur
  la LIGNE à travers 47k/68k, une voie sur GPIO6.

## 11. Nomenclature

| Partie | Article | Quantité |
|---|---|---|
| Adaptateur | fiche 3 broches précâblée (XH, ou PH selon l'étape 0) | 1 |
| Adaptateur | embase 3 broches (même série) | 1 |
| Adaptateur | barrette mâle 2 broches 2,54 mm et cavalier (`J1`) | 1 |
| Adaptateur | petite plaque à pastilles | 1 |
| Câble de sortie | fil UL1007 ou UL1015, au moins 300 V, 4 couleurs | 4 × la longueur de l'étape 0 |
| Câble de sortie | gaine VW-1, ou gaine thermorétractable de 3 à 6 mm (deux épaisseurs) | toute la longueur |
| Câble de sortie | colliers | selon le cheminement |
| Câble de sortie | fiche XH 4 broches précâblée (fils sertis), côté boîtier de mesure ; gaine thermorétractable pour les soudures | 1 |
| Boîtier de mesure | plaque à pastilles | 1 |
| Boîtier de mesure | embase XH 4 broches (câble de sortie) | 1 |
| Boîtier de mesure | picots (barrette mâle 2,54 mm) : `TP−`, `TP+`, `TP_Dp`, `TP_Dc` | 4 |
| Boîtier de mesure | barrette mâle 2 broches et cavalier (`J2`) | 1 |
| Boîtier de mesure | barrettes femelles 2,54 mm pour le SuperMini | 2 |
| Boîtier de mesure | ESP32-C6 SuperMini (sonde) | 1 |
| Boîtier de mesure | batterie USB, modèle vérifié (30 min en Wi-Fi sans coupure) | 1 |
| Étage d'écoute | Q1 : BC547 ou 2N3904 ; 100k × 2 (série et `R_be`) ; 10k (collecteur) | 1 de chaque |
| Étage d'écoute | `R_be` de rechange : 47k (`+` = 5 V), 15k (`+` = 12 V) | 1 de chaque |
| Étage d'injection | Q2 : BC547 ou 2N3904 ; 4,7k ; 10k ; 470 Ω ; 220 Ω de rechange | 1 de chaque |
| Mesures | résistance 10 kΩ / 2 W (étape 3a) | 1 |
| Mesures | 47k (étape 3c) ; 10k (pull-up optionnel de l'étape 4) | 1 de chaque |
| Mesures | diviseur de l'analyseur : 47k et 68k (5 V), ou 100k et 33k (12 V) | 1 paire |
| Mesures | grippe-fils ; fils à pinces crocodile ; aiguille fine | 2 ; 2 ; 1 |
| Mesures | analyseur logique FX2, 24 MHz, 8 voies, avec PulseView | 1 |
| Banc | ESP32-C6 SuperMini (générateur) | 1 |
| Banc | Q3 : BC547 ou 2N3904 ; 4,7k (base) ; 10k (base-émetteur) | 1 de chaque |
| Banc | pull-up de ligne : 10k (variante 1) ; 4,7k et 1 nF (variante 2) | 1 de chaque |
| Banc | plaque d'essai sans soudure, si la sonde n'y est pas sur le boîtier de mesure (§10) | 1 |
| Banc | étage d'écoute de la plaque d'essai : BC547 ou 2N3904 ; 100k × 2 (série et `R_be`) ; 10k (collecteur) | 1 de chaque |
| Banc | fils Dupont, fil de masse | déjà là |

Totaux par valeur, 1 nF du banc compris (liste d'achats : §12 de la spec) :

| Valeur | Usages | Quantité minimale |
|---|---|---|
| 100k | écoute (série, `R_be`) ; diviseur 12 V ; banc : écoute de la plaque d'essai (série, `R_be`) | 5 |
| 68k | diviseur 5 V | 1 |
| 47k | `R_be` relevé (5 V) ; étape 3c ; diviseur 5 V | 3 |
| 33k | diviseur 12 V | 1 |
| 15k | `R_be` relevé (12 V) | 1 |
| 10k | collecteur de Q1 ; base-émetteur de Q2 ; pull-up de l'étape 4 ; banc : base-émetteur de Q3, pull-up variante 1, collecteur de l'écoute de la plaque d'essai | 6 |
| 4,7k | base de Q2 ; banc : base de Q3, pull-up variante 2 | 3 |
| 470 Ω | collecteur de Q2 | 1 |
| 220 Ω | rechange du collecteur de Q2 | 1 |
| 10 kΩ / 2 W | étape 3a | 1 |
| 1 nF | banc, variante 2 | 1 |
| NPN BC547 ou 2N3904 | Q1, Q2, Q3, écoute de la plaque d'essai du banc, et une rechange | 5 |

**Brochage des NPN** (boîtier TO-92, face plate vers soi, pattes en bas, de gauche
à droite) : BC547 = C-B-E ; 2N3904 = E-B-C. Ils sont inversés l'un par rapport à
l'autre : vérifier sur la fiche du transistor utilisé.

## 12. Contrôles avant pose

**Adaptateur et câble** (étape 2, point 2) : hotte débranchée, adaptateur pas
encore posé, fiche XH 4 br. du câble de sortie enfichée sur le boîtier de mesure
(sauf ligne 10).

| # | Contrôle | Calibre | Attendu |
|---|---|---|---|
| 1 | continuité broche à broche, côté carte vers côté panneau : `−`, `D` (`J1` fermé), `+` | 200 Ω | environ 0 Ω chacune |
| 2 | pas de court-circuit entre `−`, `D` et `+` | le plus élevé | OL (de l'ordre de 100 à 200 kΩ entre `D` et `−` si l'étage d'écoute est déjà relié) |
| 3 | `TP−` vers la broche « − » (fil noir) de la fiche qui ira sur `CN3` | 200 Ω | environ 0 Ω |
| 4 | `TP+` vers la broche « + » (fil rouge) | 200 Ω | environ 0 Ω |
| 5 | `TP_Dc` vers la broche `D` de la fiche ; `TP_Dp` vers la broche `D` de l'embase | 200 Ω | environ 0 Ω |
| 6 | `J1` fermé : `TP_Dp` vers `TP_Dc` | 200 Ω | environ 0 Ω |
| 7 | `J2` ouvert : `TP+` vers la broche `5V` des barrettes | le plus élevé | OL |
| 8 | ordre des contacts de la fiche XH 3 br. identique à celui de la fiche du panneau (noir, blanc, rouge) | visuel | détrompeur du même côté |
| 9 | fiche XH 4 br. du câble de sortie : contacts dans l'ordre `−`, `+`, `D_panneau`, `D_carte` (broches 1 à 4), comme l'embase du boîtier (§4) | visuel | détrompeur : elle n'entre que dans un sens |
| 10 | fiche XH 4 br. retirée de l'embase : `TP−` vers la broche « − » de la fiche qui ira sur `CN3`, puis `TP_Dp` vers la broche `D` de l'embase de l'adaptateur | le plus élevé | OL : le boîtier est bien détaché ; remettre la fiche ensuite |

**Boîtier de mesure** : sonde retirée des barrettes.

| # | Contrôle | Calibre | Attendu |
|---|---|---|---|
| 11 | `TP−` vers la broche `GND` des barrettes | 200 Ω | environ 0 Ω |
| 12 | broche `3V3` vers la broche 6 des barrettes | 20 kΩ | environ 10 kΩ (collecteur de Q1) |
| 13 | `TP_Dp` vers la broche 6 des barrettes | le plus élevé | plus de 100 kΩ dans les deux sens : aucune liaison directe du bus vers une GPIO (règle 10) |
| 14 | collecteur de Q1 sur la broche 6 ; le 4,7k de base de Q2 sur la broche 7 | visuel | GPIO6 et GPIO7 non interverties |
| 15 | brochage de Q1 et Q2 | visuel | conforme à la fiche du transistor (BC547 = C-B-E, 2N3904 = E-B-C) |
| 16 | coque de l'USB-C isolée ; USB-C accessible | visuel | |
| 17 | contrôle électrique du module (§9) | 20 V continu | broche 6 à environ 3,3 V, broche 7 à 0 V |

**Après la pose** : l'étape 2b (isolation depuis les points de test), puis, à
l'étape 4 seulement, le contrôle de polarité : `J2` fermé, USB de la sonde
branché, fiche du panneau retirée, `TP+` par rapport à `TP−` de +4,5 à +5,3 V,
positif (un port USB peut monter à 5,25 V) ; sinon **ARRÊT**.
````

**3c. Créer `docs/RECONNAISSANCE.md`** (journal à remplir : un tableau par étape,
cases pour les photos, zone de décision de l'étape 6) :

````markdown
# Journal de la reconnaissance

Journal du sous-projet 1 (déroulé : §6 de
[SPEC-RECONNAISSANCE.md](SPEC-RECONNAISSANCE.md)). Chaque étape consigne ce
qu'elle a relevé : date, conditions, mesures, photos et décision. **On ne passe à
l'étape suivante que si son critère de sortie est rempli.**

Règles de sécurité : [SECURITE.md](SECURITE.md). Montage, points de test et
valeurs : [WIRING.md](WIRING.md).

**Comment remplir :**
- une ligne par mesure ; « Relevé » : la valeur lue, avec son unité ;
  « Décision » : la suite, ou **ARRÊT** (on ne raccorde rien, on ne met pas sous
  tension, on revient à la conception) ;
- les pinces se posent et se déplacent hotte débranchée (règle 4) ;
- photos dans `docs/photos/`, nom proposé `etapeN-sujet.jpg`, sans métadonnées
  EXIF ;
- ce journal est versé dans un **dépôt public** : ni SSID, ni mot de passe, ni
  clé, ni adresse, ni numéro de série.

## Installation et instruments

| Élément | Relevé |
|---|---|
| Hotte | Haier CCHP21EBB, fabrication 06/2024 |
| Prise et différentiel 30 mA (repère au tableau électrique) | |
| Multimètre : modèle ; calibre ohmmètre le plus élevé ; fréquence et rapport cyclique ? | |
| Batterie USB : modèle ; essai de 30 min, sonde en Wi-Fi, sans coupure (date, résultat) | |
| Analyseur FX2 : reçu le ; version de PulseView | |
| Oscilloscope, optionnel (niveaux bas chiffrés aux étapes 5b et 7) : modèle, ou « aucun » | |
| Firmware de la sonde (`fw` de `hello`) | |
| Couleurs du câble de sortie : fil 1 `−`, fil 2 `+`, fil 3 `D_panneau`, fil 4 `D_carte` | |
| `R_be` de l'étage d'écoute (100k par défaut ; changements notés à l'étape 5b) | |

## Tests du différentiel (règle 2)

Avant la première mise sous tension de chaque montage, juste avant de le poser,
hotte dans son état précédent, déjà contrôlé : hotte branchée, lumière allumée,
bouton T. **La lumière doit s'éteindre**, sinon **ARRÊT**. Réarmer, puis
débrancher pour poser le montage.

| Date | Avant quel montage | Lumière éteinte par T ? | Décision |
|---|---|---|---|
| | adaptateur (première mise sous tension : étape 2c) | | |
| | fuite en charge (étape 3a) | | |
| | sonde (étape 5) | | |
| | analyseur (étape 5b) | | |
| | étage d'injection (étape 7) | | |

## Journal des anomalies (règle 12)

Anomalie : déclenchement du différentiel ou du disjoncteur, odeur, fumée,
claquements de relais répétés, bip continu, panneau mort, fil chaud. On
débranche ; on ne réarme et on ne rebranche qu'une fois la cause comprise et
notée ici.

| Date, heure | Étape | Ce qui était branché ; ce qu'on a vu, entendu, senti | Cause comprise | Reprise (date) |
|---|---|---|---|---|
| | | | | |

## Étape −1 : observation d'usage

**Date :**

**Conditions :** hotte en usage normal, fermée ; aucun outil.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | depuis la veille, un appui sur une vitesse sans passer par « marche » | — | rien, un bip, ou un démarrage | | |
| 2 | la lumière s'allume-t-elle sans « marche » ? | — | oui ou non | | |
| 3 | « marche » quand le moteur tourne | — | arrêt immédiat, **marche prolongée** (arrêt différé natif, comme les 15 min de la CDA : durée ?), ou rien | | |
| 4 | nouvel appui sur la vitesse active | — | | | |
| 5 | l'état « armé » après « marche » s'éteint-il seul ? Au bout de combien de temps ? | — | | | |
| 6 | **voyants** : lesquels s'allument, et quand ? Fixes, clignotants, intensité ? Voyant de « marche » ? | — | | | |
| 7 | **bips** : lesquels, et quand ? (le buzzer est sur la carte : chaque bip prouve que la carte a reçu quelque chose) | — | | | |
| 8a | appui long (3 s) sur la lumière : effet caché ? comment l'annuler ? | — | verrouillage, rappel de filtre, arrêt différé, ou rien | | |
| 8b | appui long (3 s) sur « marche » | — | idem | | |
| 8c | appui long (3 s) sur la vitesse 1 | — | idem | | |
| 8d | appui long (3 s) sur la vitesse 2 | — | idem | | |
| 8e | appui long (3 s) sur la vitesse 3 | — | idem | | |
| 9 | **reprise après coupure** : vitesse 2 et lumière allumée, fiche débranchée 10 s, rebranchée : quel état revient ? | — | | | |
| 10 | **délai prudent entre deux changements de vitesse**, d'après ce qu'on entend des relais | — | 3 s par défaut | | fixe la valeur de l'étape 7 |

**Sortie :**
- [ ] le tableau des comportements est rempli.

## Étape 0 : photos, inventaire et cheminements

**Date :**

**Conditions :** hotte débranchée depuis 5 min (débranchée à : ____).

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1a | IC2 : marquage, nombre de broches | photo macro | SOP-14 sans marquage (comme la CDA) | | |
| 1b | IC3 : marquage, nombre de broches | photo macro | `ULN2003` | | |
| 1c | IC4 : marquage, nombre de broches | photo macro | abaisseur, SOP-8 | | |
| 1d | relais : marquage | photo macro | 3 relais L, M, H | | |
| 2a | `CN3` : fiche collée ? | visuel | | | si oui : décoller avec un outil en plastique, **sans tirer sur les fils** |
| 2b | pas de `CN3`, entre les broches extrêmes | règle ou pied à coulisse en plastique | 5,0 mm : JST XH ; 4,0 mm : JST PH | | connectique de l'adaptateur |
| 3 | accès : comment retirer le filtre, où est le boîtier, comment s'ouvre son couvercle | — | | | |
| 4 | trajet des fils du condensateur et du connecteur blanc, par rapport à `CN3` et au passage de sortie | photo | | | |
| 5a | câble du panneau : longueur libre | mètre | | | |
| 5b | comment sa fiche peut sortir du boîtier ; ruban et colliers à refaire à l'identique | photo | | | placement de l'adaptateur (WIRING.md §2) |
| 5c | connecteur intermédiaire sur le câble du panneau ? | visuel | | | |
| 5d | le panneau se démonte-t-il, sans forcer ? Si oui, référence de la puce tactile | visuel | | | décide si C est possible |
| 6a | cheminement possible du câble de sortie jusqu'au boîtier de mesure | photo | par exemple le long du câble du panneau vers l'avant, ou au bord du filtre sans pincement | | |
| 6b | longueur nécessaire du câble de sortie | mètre | | | |
| 6c | emplacement du boîtier de mesure | — | à côté de la hotte, visible, hors de l'aplomb de la plaque de cuisson | | |

**Photos :**
- [ ] Photo IC2, IC3, IC4 (macro) : `docs/photos/etape0-ic.jpg`
- [ ] Photo relais : `docs/photos/etape0-relais.jpg`
- [ ] Photo `CN3` de près : `docs/photos/etape0-cn3.jpg`
- [ ] Photo fils du condensateur et connecteur blanc, avant toute manipulation : `docs/photos/etape0-fils.jpg`
- [ ] Photo câble du panneau, sa sortie, ruban et colliers : `docs/photos/etape0-cable.jpg`
- [ ] Photo cheminement choisi et emplacement du boîtier de mesure : `docs/photos/etape0-cheminement.jpg`

**Sortie :**
- [ ] le pas de `CN3` et les longueurs sont connus : connectique ☐ XH ☐ PH ;
- [ ] on sait si le panneau s'ouvre : ☐ oui (C possible) ☐ non, ou résiné ;
- [ ] le cheminement est choisi, avec une photo.

## Étape 1 : test d'isolation

Procédure pas à pas : [SECURITE.md](SECURITE.md), « Procédure : étape 1 ».

**Date :**

**Conditions :** hotte débranchée depuis 5 min ; broches rondes de la fiche
reliées par le fil à pinces crocodile ; `CN3` piqué par l'arrière avec
l'aiguille ; aucune partie métallique des pointes touchée.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| a1 | broches reliées vers `CN3 −` | le plus élevé (20 MΩ ou plus) | OL après 10 s (une valeur qui monte vers OL est normale) | | valeur stable : **ARRÊT** |
| a2 | `CN3 −` vers broches reliées (cordons inversés) | idem | OL | | idem |
| a3 | broches reliées vers `D` | idem | OL | | idem |
| a4 | `D` vers broches reliées | idem | OL | | idem |
| a5 | broches reliées vers `+` | idem | OL | | idem |
| a6 | `+` vers broches reliées | idem | OL | | idem |
| b | contact de terre de la fiche vers `CN3 −` | le plus élevé | OL : secondaire flottant ; environ 0 Ω : relié à la terre (pas un défaut, on le note) | | kΩ à MΩ : **ARRÊT** et analyse |
| c0 | cordons court-circuités | 200 Ω | valeur à soustraire | | |
| c | contact de terre de la fiche vers la vis de terre de la carcasse (ou une vis nue) | 200 Ω | moins de 1 Ω, après soustraction | | sinon **ARRÊT** : défaut de terre |

**Sortie :** ☐ feu vert pour construire l'adaptateur ☐ **ARRÊT** (raison : ____).

## Étape 2 : adaptateur et boîtier de mesure

**Date :**

**Conditions :** hotte débranchée depuis 5 min (on ouvre le boîtier).

| Construction | Relevé |
|---|---|
| connectique (XH ou PH, références) | |
| fiche et embase XH 4 broches du câble de sortie vers le boîtier de mesure (références) | |
| fil du câble de sortie (référence, tension, température) | |
| gaine (type, nombre d'épaisseurs) | |
| longueur du câble de sortie | |

Contrôles au multimètre **avant la pose** (WIRING.md §12, mêmes numéros).
Adaptateur et câble : hotte débranchée, adaptateur pas encore posé, fiche XH 4 br.
du câble de sortie enfichée sur le boîtier de mesure (sauf ligne 10).

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | continuité côté carte vers côté panneau : `−`, `D` (`J1` fermé), `+` | 200 Ω | environ 0 Ω chacune | | sinon on corrige avant la pose |
| 2 | court-circuit `−`/`D`, `−`/`+`, `D`/`+` | le plus élevé | OL (100 à 200 kΩ entre `D` et `−` si l'étage d'écoute est relié) | | court-circuit : on corrige avant la pose |
| 3 | `TP−` vers la broche « − » (fil noir) de la fiche qui ira sur `CN3` | 200 Ω | environ 0 Ω | | sinon on corrige avant la pose |
| 4 | `TP+` vers la broche « + » (fil rouge) | 200 Ω | environ 0 Ω | | idem |
| 5 | `TP_Dc` vers la broche `D` de la fiche ; `TP_Dp` vers la broche `D` de l'embase | 200 Ω | environ 0 Ω | | idem |
| 6 | `J1` fermé : `TP_Dp` vers `TP_Dc` | 200 Ω | environ 0 Ω | | idem |
| 7 | `J2` ouvert : `TP+` vers la broche `5V` des barrettes | le plus élevé | OL | | idem |
| 8 | ordre des contacts de la fiche XH 3 br. identique à celui de la fiche du panneau (noir, blanc, rouge) | visuel | détrompeur du même côté | | idem |
| 9 | fiche XH 4 br. : contacts dans l'ordre `−`, `+`, `D_panneau`, `D_carte` (broches 1 à 4), comme l'embase du boîtier | visuel | détrompeur : elle n'entre que dans un sens | | idem |
| 10 | fiche XH 4 br. retirée : `TP−` vers la broche « − » de la fiche qui ira sur `CN3`, puis `TP_Dp` vers la broche `D` de l'embase de l'adaptateur | le plus élevé | OL (boîtier détaché) ; fiche remise ensuite | | idem |

Boîtier de mesure : sonde retirée des barrettes.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 11 | `TP−` vers la broche `GND` des barrettes | 200 Ω | environ 0 Ω | | sinon on corrige avant la pose |
| 12 | broche `3V3` vers la broche 6 des barrettes | 20 kΩ | environ 10 kΩ (collecteur de Q1) | | idem |
| 13 | `TP_Dp` vers la broche 6 des barrettes | le plus élevé | plus de 100 kΩ dans les deux sens (règle 10) | | moins : liaison directe du bus vers une GPIO, on corrige avant la pose |
| 14 | collecteur de Q1 sur la broche 6 ; le 4,7k de base de Q2 sur la broche 7 | visuel | GPIO6 et GPIO7 non interverties | | sinon on corrige avant la pose |
| 15 | brochage de Q1 et Q2 | visuel | conforme à la fiche du transistor (BC547 = C-B-E, 2N3904 = E-B-C) | | idem |
| 16 | coque de l'USB-C isolée ; USB-C accessible | visuel | | | idem |
| 17 | contrôle électrique du module (WIRING.md §9) | 20 V continu | broche 6 à environ 3,3 V, broche 7 à 0 V | | sérigraphie mal lue : ne rien souder |

**Pose :**
- [ ] 1. fiche de l'adaptateur sur `CN3`, fiche du panneau sur l'embase de l'adaptateur ;
- [ ] 2. câble de sortie selon la règle 5 et le cheminement choisi à l'étape 0 ; sa fiche XH 4 br. enfichée sur le boîtier de mesure ;
- [ ] 3. couvercle fermé, en vérifiant qu'il ne pince rien ;
- [ ] 4. filtre remis, en vérifiant qu'il ne pince rien.

**Photos :**
- [ ] Photo adaptateur posé sur `CN3` : `docs/photos/etape2-adaptateur.jpg`
- [ ] Photo câble de sortie au passage de sortie : `docs/photos/etape2-cable.jpg`
- [ ] Photo boîtier de mesure en place : `docs/photos/etape2-boitier.jpg`

### Étape 2b : isolation revérifiée depuis les points de test

Procédure pas à pas : [SECURITE.md](SECURITE.md), « Procédure : étape 2b ».

**Date :**

**Conditions :** hotte débranchée, couvercle fermé, filtre remis ; fiche XH 4 br.
du câble de sortie enfichée sur le boîtier de mesure (retirée, tout lirait OL
sans rien prouver) ; broches de la fiche reliées.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | broches reliées vers `TP−` | le plus élevé (20 MΩ ou plus) | OL après 10 s | | sinon **ARRÊT** |
| 2 | `TP−` vers broches reliées | idem | OL | | idem |
| 3 | broches reliées vers `TP+` | idem | OL | | idem |
| 4 | `TP+` vers broches reliées | idem | OL | | idem |
| 5 | broches reliées vers `TP_Dp` | idem | OL | | idem |
| 6 | `TP_Dp` vers broches reliées | idem | OL | | idem |
| 7 | broches reliées vers `TP_Dc` | idem | OL | | idem |
| 8 | `TP_Dc` vers broches reliées | idem | OL | | idem |

**Où est le pull-up de `D` ?** `J1` ouvert (hotte débranchée, filtre retiré, sans
ouvrir le boîtier), chaque mesure dans les deux sens.

**Date :**

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| p1 | `TP_Dp` vers `TP+` | 20 kΩ ou automatique | quelques kΩ : pull-up côté panneau | | |
| p2 | `TP_Dp` vers `TP−` | idem | l'étage d'écoute y ajoute 100 à 200 kΩ : n'indique rien | | |
| p3 | `TP_Dc` vers `TP+` | idem | quelques kΩ : pull-up côté carte | | |
| p4 | `TP_Dc` vers `TP−` | idem | | | |
| p5 | `J1` refermé, filtre remis | visuel | | | |

Conclusion : pull-up côté ☐ carte ☐ panneau ☐ aucun trouvé ; valeur environ
____ kΩ (sert à l'étape 4 et à la voie 2 si B).

**Nouveaux passages de la mesure 1** (après chaque ouverture du boîtier, et à la
sortie de l'étape 4) :

| Date | Pourquoi le boîtier a été ouvert | `TP−`, `TP+`, `TP_Dp`, `TP_Dc` : OL dans les deux sens ? | Décision |
|---|---|---|---|
| | | | |

### Étape 2c : essai à vide

**Date :**

**Conditions :** hotte branchée ; différentiel testé (tableau plus haut).

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | touche lumière | — | exactement comme avant (étape −1) : lumière, voyant, bip | | |
| 2 | touche « marche » | — | comme avant | | |
| 3 | vitesse 1 | — | comme avant | | |
| 4 | vitesse 2 | — | comme avant | | |
| 5 | vitesse 3 | — | comme avant | | |

**Sortie :**
- [ ] les étapes 2b et 2c sont concluantes ;
- [ ] les photos du montage sont dans ce journal (étape 2).

## Étape 3 : mesures au multimètre

Toutes les pinces se posent et se déplacent **hotte débranchée** (règle 4), avec
des grippe-fils sur le boîtier de mesure. Sous tension, on ne fait que lire
l'afficheur et toucher les touches du panneau.

### 3a. Fuite en charge, avant tout Mac ou analyseur

Procédure pas à pas : [SECURITE.md](SECURITE.md), « Procédure : étape 3a ».

**Date :**

**Conditions :** fiche XH 4 br. du câble de sortie enfichée sur le boîtier ; COM
sur `TP−`, l'autre pince sur la vis de terre de la carcasse, résistance de
10 kΩ / 2 W en parallèle entre les deux pinces ; différentiel testé.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | tension aux bornes de 10 kΩ / 2 W, lue dans les 5 s après le branchement | tension alternative | 3 V au plus, soit 0,3 mA au plus | ____ V, soit ____ mA (tension / 10 kΩ) | au-delà de 5 V (0,5 mA), ou différentiel déclenché : **ARRÊT** ; entre 3 et 5 V : on le note, et on en rediscute avant de relier un Mac ou l'analyseur |

### 3b. Tensions

**Date :**

**Conditions :** hotte débranchée pour poser les pinces : COM sur `TP−`, V sur
`TP+` (lignes 1 à 7), puis V sur `TP_Dp` (lignes 8 à 18).

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | `+` en veille | 20 V continu | environ 5 V ou environ 12 V, positif | | négatif : adaptateur inversé, **ARRÊT** |
| 2 | `+` après « marche » | 20 V continu | stable | | |
| 3 | `+` lumière allumée | 20 V continu | stable | | |
| 4 | `+` vitesse 1 | 20 V continu | stable | | |
| 5 | `+` vitesse 2 | 20 V continu | stable | | |
| 6 | `+` vitesse 3 | 20 V continu | stable | | |
| 7 | `+` pendant un appui bref (1 à 2 s) | 20 V continu | stable | | |
| 8 | `D` en veille (et fréquence, rapport cyclique si possible) | 20 V continu ; Hz ; % | voir la lecture ci-dessous | | |
| 9 | `D` après « marche » | idem | | | |
| 10 | `D` lumière allumée | idem | | | |
| 11 | `D` vitesse 1 | idem | | | |
| 12 | `D` vitesse 2 | idem | | | |
| 13 | `D` vitesse 3 | idem | | | |
| 14 | `D` pendant un appui bref sur la lumière | idem | | | |
| 15 | `D` pendant un appui bref sur « marche » | idem | | | |
| 16 | `D` pendant un appui bref sur la vitesse 1 | idem | | | |
| 17 | `D` pendant un appui bref sur la vitesse 2 | idem | | | |
| 18 | `D` pendant un appui bref sur la vitesse 3 | idem | | | |

Lecture (cocher ce qui s'applique) :

| Observation | Lecture probable | ☐ |
|---|---|---|
| `V(+,−)` négatif | adaptateur inversé : **ARRÊT** | ☐ |
| `+` ≈ 5 V | sortie de l'abaisseur IC4 : l'étape 4 est possible | ☐ |
| `+` ≈ 12 V | rail du flyback : on saute l'étape 4 | ☐ |
| `D` stable à `+`, avec des creux seulement pendant les appuis | trafic sur événement, repos haut : plutôt A | ☐ |
| `D` entre 40 et 60 % de `+` en permanence | trames continues : plutôt B | ☐ |
| `D` ≈ 0 V au repos, avec de l'activité | UART inversé, repos bas (type Krona) | ☐ |
| une tension continue stable, différente pour chaque touche maintenue | échelle de résistances | ☐ |

### 3c. Test du pull-up

**Date :**

**Conditions :** hotte débranchée pour poser 47 kΩ entre `TP_Dp` et `TP−` sur le
boîtier de mesure ; branchée pour la lecture ; débranchée pour la retirer.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | `D` au repos, sans la résistance (ligne 8 de 3b) | 20 V continu | | | |
| 2 | `D` au repos, avec 47 kΩ entre `TP_Dp` et `TP−` | 20 V continu | sur 5 V : chute d'environ 0,45 V (pull-up d'environ 4,7 kΩ) ; environ 0,9 V (pull-up d'environ 10 kΩ) ; aucune chute (push-pull, ou pull-up très fort) | | |
| 3 | chute = ligne 1 − ligne 2 ; pull-up estimé ≈ 47 kΩ × chute / ligne 2 | — | | | |
| 4 | résistance retirée, hotte débranchée | visuel | | | |

**Sortie :**
- [ ] `+` connu : ____ V ; ☐ étape 4 possible (`+` ≈ 5 V) ☐ étape 4 sautée (`+` ≈ 12 V) ;
- [ ] repos connu : ☐ haut ☐ bas ;
- [ ] première idée du trafic : ____.

## Étape 4 : panneau seul, hotte débranchée

Seulement si `+` ≈ 5 V. **Il n'y a aucun secteur** : la sonde peut être sur
l'USB du Mac. ☐ Étape sautée (`+` ≈ 12 V).

**Date :**

**Conditions :** hotte débranchée depuis 5 min, boîtier ouvert.

Capture par l'USB (lignes 5 à 10), un fichier par ligne, 30 s chacun :
`python3 tools/serie_enregistre.py <port> <scénario> "capture tout" --duree 30`
(port : `ls /dev/cu.usbmodem*` ; ni `pio device monitor` ni autre outil sur ce
port), qui écrit `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl` ; résumé à suivre avec
`tail -f logs/live.log`.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | fiche de l'adaptateur retirée de `CN3` (sinon le 5 V de la sonde alimenterait aussi la carte) | visuel | | | |
| 2 | polarité sans le panneau : fiche du panneau retirée de l'embase, `J2` fermé, USB de la sonde branché ; `TP+` par rapport à `TP−` | 20 V continu | de +4,5 à +5,3 V, positif (un port USB peut monter à 5,25 V) | | sinon **ARRÊT** ; puis débrancher l'USB |
| 3 | USB débranché : en option, 10 kΩ entre `TP_Dp` et `TP+`, si le pull-up est côté carte (étape 2b) | visuel | | ☐ posée ☐ non | |
| 4 | fiche du panneau sur l'embase, puis USB branché, sans toucher les touches (le circuit tactile se calibre) | — | | | |
| 5 | le panneau émet-il tout seul ? Comment ? (`etape4-repos`, sans toucher) | fichier de capture | | | |
| 6 | voyants quand on touche la lumière (`etape4-lumiere`) | — | s'allument seuls : le panneau pilote ses voyants ; restent éteints : la carte les pilote (cas favorable) | | |
| 7 | voyants quand on touche « marche » (`etape4-marche`) | — | idem | | |
| 8 | voyants quand on touche la vitesse 1 (`etape4-v1`) | — | idem | | |
| 9 | voyants quand on touche la vitesse 2 (`etape4-v2`) | — | idem | | |
| 10 | voyants quand on touche la vitesse 3 (`etape4-v3`) | — | idem | | |

**Sortie, dans cet ordre :**
- [ ] 1. débrancher l'USB ;
- [ ] 2. ouvrir `J2` ; retirer la 10 kΩ si elle a été posée ;
- [ ] 3. remettre la fiche de l'adaptateur sur `CN3` ;
- [ ] 4. fermer le couvercle et remettre le filtre, sans rien pincer ;
- [ ] 5. refaire l'étape 2b, qui se mesure couvercle fermé et filtre remis (ligne
  ajoutée au tableau des nouveaux passages).

Le couvercle et le filtre passent avant l'étape 2b : elle se mesure ainsi, et
repère un câble pincé à la fermeture.

**Photos :**
- [ ] Photo montage du panneau seul : `docs/photos/etape4-montage.jpg`
- [ ] Photo voyants pendant un appui : `docs/photos/etape4-voyants.jpg`

## Étape 5 : capture en place

**Date :**

**Conditions :**
- [ ] banc validé (tâches 16 et 21 : critères 1, 2 sans l'analyseur, 3, 4 et 6 dans `BANC.md`) ;
- [ ] batterie vérifiée (30 min) : modèle et essai notés dans « Installation et instruments » ;
- [ ] contrôles de la règle 7 (étapes 1, 2b, 3a) et test du différentiel faits ;
- [ ] sonde sur son boîtier de mesure, fiche XH 4 br. du câble de sortie enfichée, **sur batterie** (modèle vérifié), `J2` ouvert ;
- [ ] session réseau par Wi-Fi ; RSSI au boîtier de mesure : ____ dBm.

Un fichier par scénario, 10 s chacun, dans cet ordre :
`python3 tools/hotte_udp.py enregistre hotte-sonde.local <scénario> "capture tout" --duree 10`,
qui écrit `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl` ; résumé à suivre avec
`tail -f logs/live.log`. La commande `capture tout` remet le mode `tout` : la
sonde garde en NVS le dernier mode choisi, peut-être `changements`.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | veille (`veille`) | — | un fichier de 10 s | | |
| 2 | marche (`marche`) | — | idem | | |
| 3 | lumière allumée (`lumiere-on`) | — | idem | | |
| 4 | lumière éteinte (`lumiere-off`) | — | idem | | |
| 5a | vitesse 1 (`v1`) | — | idem | | |
| 5b | vitesse 2 (`v2`) | — | idem | | |
| 5c | vitesse 3 (`v3`) | — | idem | | |
| 6 | arrêt (`arret`) | — | idem | | |
| 7 | chaque bip entendu (`bip-<touche>`) | — | idem | | |
| 8 | panneau déconnecté (`panneau-off`) : **hotte débranchée**, filtre retiré (si cela ouvre le boîtier : 5 min d'attente), fiche du panneau retirée de l'embase ; couvercle fermé et filtre remis (règle 5), étape 2b si le boîtier a été ouvert ; puis hotte rebranchée | — | on voit si la carte émet seule | | |
| 8b | après `panneau-off` : **hotte débranchée**, filtre retiré (si cela ouvre le boîtier : 5 min d'attente), fiche du panneau remise sur l'embase ; couvercle fermé et filtre remis (règle 5), étape 2b si le boîtier a été ouvert | visuel | à la remise sous tension, le panneau marche comme avant (étape 2c) | | |
| 9 | compteurs de la sonde après la série : `debord`, `lignes_perdues`, `sautes` | — | 0 partout | | sinon on refait le scénario |

Captures versées dans `captures/`, une par scénario (l'outil refuse un fichier
qui contient une clé) :
- [ ] `python3 tools/verse_capture.py logs/<fichier>.jsonl etape5-<scénario>`

**Photos :**
- [ ] Photo sonde sur le boîtier de mesure, sur batterie : `docs/photos/etape5-sonde.jpg`

**Sortie :**
- [ ] un fichier par scénario, versé dans `captures/` ;
- [ ] les compteurs de la sonde sans perte ni débordement ;
- [ ] fin de séance : hotte débranchée, sonde et batterie retirées avant toute cuisson (règle 13).

## Étape 5b : validation croisée à l'analyseur

Obligatoire avant l'étape 6.

**Date :**

**Conditions :**
- [ ] différentiel testé avant de poser l'analyseur (tableau plus haut), puis hotte débranchée ;
- [ ] hotte débranchée pour poser les pinces : masse de l'analyseur **d'abord**, sur `TP−` ;
- [ ] voie 1 sur la sortie de l'étage d'écoute, le même nœud que GPIO6 (3,3 V) ;
- [ ] voie 2 sur `TP_Dp` à travers le diviseur (WIRING.md §8) : ☐ 47k/68k (`+` = 5 V) ☐ 100k/33k (`+` = 12 V) ;
- [ ] Mac sur batterie, sans aucun autre câble (règle 8), analyseur **en USB direct** (pas d'isolateur ADuM3160 : limité au Full Speed, il bride l'échantillonnage) ;
- [ ] USB de l'analyseur branché au Mac **hotte encore débranchée**, après les pinces (règle 4) ; puis hotte branchée ;
- [ ] PulseView, fréquence d'échantillonnage : ____ ;
- [ ] la sonde enregistre en même temps, par le Wi-Fi : `python3 tools/hotte_udp.py enregistre hotte-sonde.local etape5b-<scénario> "capture tout" --duree 10` (fichier : ____).

L'analyseur FX2 est numérique : il ne mesure pas de tension. Le niveau bas de
`D` (lignes 4 et 5) se juge donc fonctionnellement ; une mesure chiffrée, à
l'oscilloscope, est optionnelle.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | voie 1 : durées de la sonde contre celles de l'analyseur | — | écart de ±2 µs au plus, plus le quantum de 1 µs | | |
| 2 | voie 1 : trames décodées par la sonde et par l'analyseur | — | identiques | | |
| 3 | écarts entre réceptions (`t_us`) contre l'analyseur | — | quelques dizaines de µs | | |
| 4 | niveau bas de `D` pendant les trames du panneau | — | jugement fonctionnel : lignes 1 et 2 concluantes, capture propre, chaque impulsion de la voie 2 retrouvée sur la voie 1 ; si mesuré : sous 0,6 V | | sinon : relever le seuil (`R_be`, WIRING.md §5), puis refaire l'étape 5 |
| 5 | niveau bas de `D` pendant les trames de la carte | — | idem | | idem |
| 6 | voie 2 : temps de montée de `D` | — | c'est la tolérance de la voie 2 | | |

Instrument qui a donné les niveaux des lignes 4 et 5 : ☐ jugement fonctionnel
(analyseur) ☐ oscilloscope : ____.

Captures de la sonde versées dans `captures/` :
- [ ] `python3 tools/verse_capture.py logs/<fichier>.jsonl etape5b-<scénario>`

**Fin de séance, dans cet ordre (règle 8) :**
- [ ] 1. débrancher la hotte ;
- [ ] 2. retirer l'analyseur (USB et pinces) ;
- [ ] 3. ensuite seulement, rebrancher le chargeur du Mac.

**Photos :**
- [ ] Photo montage de l'analyseur : `docs/photos/etape5b-analyseur.jpg`
- [ ] Photo capture PulseView (copie d'écran) : `docs/photos/etape5b-pulseview.png`

**Sortie :**
- [ ] critères de la voie 1 remplis, niveaux bas jugés (lignes 4 et 5) : étape 6 permise ;
- [ ] le cas échéant : seuil relevé (`R_be` = ____) et étape 5 refaite.

## Étape 6 : décodage et choix de l'architecture

**Date :**

**Conditions :** captures des étapes 5 et 5b ; `tools/analyse.py`.

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | seuil de silence choisi après coup (`analyse.py trames --silence`) | µs | | | |
| 2 | `analyse.py auto` : hypothèse retenue et taux d'erreur | — | UART (débits bas de 300 à 1 200 bauds compris, repos bas compris), distance d'impulsion (type WTC6534), ou Manchester | | |
| 3 | `analyse.py diff` : bits de chaque touche et de chaque état | — | | | |
| 4 | test 1 : après le relâchement, la trame revient-elle à « aucune touche » ? | — | oui : la carte détient l'état de la hotte | | |
| 5 | test 2 : la carte répond-elle après chaque trame ? Avec quel délai ? | — | | | |
| 6 | test 3 : y a-t-il des silences plus longs qu'une trame, où l'on pourrait injecter ? | — | | | |
| 7 | test 4 : la carte émet-elle son état ou un paquet de voyants ? | — | | | |

### Décision de l'étape 6

Entrées (en croisant avec l'étape 4) :

| Entrée | Source | Relevé |
|---|---|---|
| voyants du panneau seul quand on touche | étape 4 | ☐ s'allument seuls ☐ restent éteints ☐ étape sautée : seul le test 4 renseigne |
| état ou paquet de voyants émis par la carte | test 4 | ☐ oui ☐ non |
| `D` en drain ouvert ou en push-pull | étapes 3c et 5b | |
| silences exploitables | test 3 | |
| trafic continu, ou réponse attendue dans un délai court | étape 3b, tests 2 et 3 | |
| le panneau s'ouvre ; il est résiné | étape 0 | |
| échelle de résistances au lieu d'un bus de données | étape 3b | |

Règle de choix (§4 de la spec) :

| Résultat de la reconnaissance | Choix | ☐ |
|---|---|---|
| **la carte pilote les voyants** (voyants éteints à l'étape 4, ou paquet de voyants émis par la carte), `D` en drain ouvert avec des silences exploitables | **A** | ☐ |
| **la carte pilote les voyants**, mais trafic continu, réponse attendue dans un délai court, ou `D` en push-pull | **B** | ☐ |
| **le panneau pilote ses voyants** (il les allume seul et la carte n'émet aucun état), ou `D` unidirectionnel sans état renvoyé | **C**, si le panneau s'ouvre | ☐ |
| le panneau pilote ses voyants **et** il est résiné | pas de solution propre qui respecte la contrainte des voyants : on en rediscute avec Majid, faits en main | ☐ |
| échelle de résistances au lieu d'un bus de données | **A**, avec une résistance commutée vers `−`, si la carte pilote les voyants | ☐ |

**Choix retenu :** ☐ A ☐ B ☐ C ☐ aucune (on en rediscute avec Majid)

**Argumentaire** (critère 3 du §1) :

> (à rédiger : faits relevés, règle appliquée, ce qui reste incertain)

**Avenant d'injection**, écrit quel que soit le choix, chiffré d'après
`PROTOCOL.md`, et relu par Majid avant l'étape 7 :

| Valeur fixée par l'avenant | Valeur |
|---|---|
| syntaxe de `injecte` | |
| durée basse maximale par impulsion (`bas_max_us`) | |
| durée totale maximale par trame (`total_max_us`) | |
| silence minimal avant d'émettre (`silence_min_us`) | |
| attente maximale (`attente_max_ms`) | |
| délai minimal entre deux changements d'état du moteur (`delai_min_ms`, d'après l'étape −1, ligne 10) | |
| pour B : le mode relais ; pour C : le prototype PhotoMOS | |

**Sortie :**
- [ ] `docs/PROTOCOL.md` est rédigé ;
- [ ] le choix est argumenté ci-dessus ;
- [ ] l'avenant est écrit (lien : ____) ;
- [ ] l'avenant est relu par Majid (date : ____).

## Étape 7 : premier essai d'injection

**En présence de Majid** (règle 11), sous tension, sonde sur batterie. Majid se
tient à portée de la fiche, voit le panneau, et débranche au moindre doute.

**Date :**

**Conditions :** choix ☐ A ☐ B ☐ C ; avenant relu ; ☐ critère 5 du banc validé
(collision détectée et émission arrêtée, `BANC.md`).

Préparation (choix A) :

| # | Mesure | Calibre | Attendu | Relevé | Décision |
|---|---|---|---|---|---|
| 1 | différentiel testé, hotte dans son état précédent (tableau plus haut), puis hotte débranchée | — | lumière éteinte par T | | sinon **ARRÊT** |
| 2 | hotte débranchée : étage d'injection monté (WIRING.md §6), contrôles avant pose faits ; voie 2 de l'analyseur sur `TP_Dp` par le diviseur, masse d'abord sur `TP−` | visuel | | | |
| 3 | USB branché, `injection monte 1` envoyé | — | réponse `ok` | | |
| 4 | `config` : valeurs de l'avenant chargées | — | égales à celles de l'avenant | | sinon on corrige avant d'aller plus loin |
| 5 | USB débranché, sonde passée sur batterie | visuel | | | |
| 6 | contrôles de la règle 7 à jour (étape 2b refaite si le boîtier a été ouvert) ; Mac sur batterie, sans aucun autre câble | — | | | |
| 7 | hotte branchée ; `injection on` envoyé par `hotte_udp.py`, depuis un terminal, après la confirmation que Majid tape lui-même : « Majid devant la hotte » (jamais un script ni un agent) | — | injection armée | | |
| 8 | niveau bas obtenu par l'injection, pendant les premiers essais | — | jugement fonctionnel : la carte réagit (bip), et l'impulsion se retrouve sur la voie 2 de l'analyseur et dans `relu_us` de l'événement `injection` ; si mesuré (oscilloscope) : sous 0,8 V | | sinon : on arrête, `injection off`, hotte débranchée, puis 470 Ω remplacées par 220 Ω |

Instrument qui a donné le niveau de la ligne 8 : ☐ jugement fonctionnel
☐ oscilloscope : ____.

Analyseur : pinces puis USB posés hotte débranchée (règle 4) ; fin de séance
comme à l'étape 5b : hotte débranchée, analyseur retiré, puis seulement le
chargeur du Mac (règle 8).

Essais, dans cet ordre, **20 fois sur 20 chacun**. Chaque essai se compte par son
événement `injection` (même `id` que la commande, résultat `ok`). Le firmware
impose le délai minimal entre deux changements d'état du moteur. Une case par
essai : « Essais réussis » si l'on entend le bip, voit l'effet (lumière ou
moteur) et retrouve la trame sur le bus ; « Voyants qui suivent » si **le voyant
du panneau change** comme il faut.

| Action | Essais réussis | Voyants qui suivent | Total |
|---|---|---|---|
| touche lumière | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| vitesse 1 | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| vitesse 2 | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| vitesse 3 | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| « marche » puis une vitesse | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |
| arrêt | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ ☐☐☐☐☐ | ____ / 20 |

Fichiers d'enregistrement des essais : ____.

**Si le choix est B** (on suit l'avenant) :
- [ ] voie 2 montée (WIRING.md §7), hotte débranchée ;
- [ ] hotte débranchée, filtre retiré (sans ouvrir le boîtier) : `J1` ouvert ; filtre
  remis avant de rebrancher (règle 5) ;
- [ ] mode relais transparent : le panneau marche à travers la sonde ;
- [ ] puis mêmes injections (tableau ci-dessus) et même critère.

**Si le choix est C** (on suit l'avenant) :
- [ ] prototype PhotoMOS sur une touche ; le critère porte sur la touche prototypée.

**Photos :**
- [ ] Photo étage d'injection monté : `docs/photos/etape7-injection.jpg`

**Sortie :**
- [ ] 20 fois sur 20 pour chaque action, et les voyants du panneau suivent ;
- [ ] aucun comportement anormal de la carte ;
- [ ] le vrai panneau fonctionne toujours après l'essai (chaque touche) ;
- [ ] `injection off` ; entre deux sessions, sonde et batterie retirées, `J1` fermé
  (règle 13).

## Critères de sortie du sous-projet

- [ ] 1. **Protocole documenté** dans `docs/PROTOCOL.md` : couche physique
  (tension de `+`, niveau de repos, niveau bas de chaque émetteur, jugé à
  l'étape 5b, drain ouvert ou push-pull, pull-up et son côté), codage, trame de
  chaque action, qui détient l'état et qui pilote les voyants ; **validé par la
  capture croisée de l'étape 5b**.
- [ ] 2. **Injection prouvée** : lumière, chaque vitesse, « marche » puis vitesse,
  arrêt, 20 fois sur 20 chacune, et les voyants du panneau suivent (avec C : la
  touche prototypée).
- [ ] 3. **Architecture du produit choisie** et argumentée ci-dessus, ou constat
  argumenté qu'aucune ne respecte la contrainte des voyants.
````

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer (depuis la racine du dépôt) : `sh "${TMPDIR:-/tmp}/verif_terrain.sh"; echo "code=$?"`

Attendu : une seule ligne « verif_terrain : 336 verifications, 0 echecs », puis
`code=0` (323 contrôles de contenu, de structure et de tableaux, plus 13 liens
relatifs vérifiés).

Puis relire les trois fichiers contre la spec avec cette liste de contrôle de
cohérence ; chaque point doit être vrai avant le commit (sinon corriger le
document et relancer le script) :

- [ ] `SECURITE.md` reprend les 13 règles du §5, **numérotées comme la spec**, sans en affaiblir aucune : 5 minutes et E5/E6 4,7 µF / 450 V (1) ; bouton T, lumière éteinte, sinon ARRÊT, testé juste avant de poser chaque nouveau montage, hotte dans son état précédent, puis débranchée pour poser (2) ; seulement `CN3` et le câble du panneau, jamais `CN1`/`CN2`, zones interdites avec leurs photos (3) ; aucune connexion sous tension (4) ; câble ≥ 300 V UL1007/UL1015, gaine VW-1 600 V ou deux épaisseurs, couvercle fermé et filtre remis (5) ; aucune main sous tension (6) ; trois portes dans l'ordre 1, 2b, 3a (7) ; Mac sur batterie sans aucun autre câble (8) ; sonde sur batterie (9) ; 3,6 V et étage d'écoute (10) ; présence de Majid (11) ; liste des anomalies et cause notée (12) ; pas de cuisson, `J1` fermé (13) ; et l'encadré « Une lecture qui ne prouve rien ». La section du banc dit : aucune liaison avec la hotte ; boîtier de mesure seulement hotte débranchée toute la séance et fiche XH 4 broches retirée, contrôle `TP−` vers la terre de la fiche et vers la carcasse = OL avant tout USB ; sinon plaque d'essai du banc.
- [ ] Procédure de l'étape 1 = tableau de la spec : lignes a (vers `CN3 −`, `D`, `+`, deux sens, OL après 10 s), b (terre vers `CN3 −` : OL, ~0 Ω noté, intermédiaire = ARRÊT), c (terre vers la carcasse, 200 Ω cordons court-circuités, < 1 Ω) ; même limite (claquage à 230 V).
- [ ] Procédure de l'étape 2b = spec : mesure 1 depuis `TP−`, `TP+`, `TP_Dp`, `TP_Dc`, deux sens, OL ; mesure 2 `J1` ouvert (hotte débranchée, filtre retiré, boîtier fermé), calibre 20 kΩ ou automatique, `J1` refermé ; mesure 1 refaite après chaque ouverture ; fiche XH 4 broches enfichée (sinon tout lirait OL) ; à la sortie de l'étape 4, couvercle et filtre d'abord.
- [ ] Procédure de l'étape 3a = spec : COM sur `TP−`, vis de terre, 10 kΩ / 2 W en parallèle, tension alternative, lecture dans les 5 s ; attendu 3 V (0,3 mA) ; au-delà de 5 V (0,5 mA) ou différentiel déclenché : ARRÊT ; entre 3 et 5 V, noté et rediscuté avant de relier un Mac ou l'analyseur ; fiche XH 4 broches enfichée.
- [ ] `RECONNAISSANCE.md` a une section et au moins un tableau Mesure/Calibre/Attendu/Relevé/Décision pour −1, 0, 1, 2, 2b, 2c, 3a, 3b, 3c, 4, 5, 5b, 6, 7 ; les 10 relevés de l'étape −1 (appui long détaillé par touche), les 6 relevés de l'étape 0, les 3 lignes de l'étape 1, les 17 contrôles de WIRING §12 (mêmes numéros) et la pose de l'étape 2, les situations de 3b et la table de lecture, 3c avec les chutes 0,45 V / 0,9 V, à l'étape 4 la polarité +4,5 à +5,3 V, la 10 kΩ posée et retirée USB débranché, la commande `serie_enregistre.py ... "capture tout"` et les voyants, la sortie couvercle et filtre avant la 2b ; à l'étape 5 les prérequis (banc validé, batterie vérifiée 30 min), la commande avec `"capture tout"`, les 8 scénarios, la remise de la fiche du panneau (8b), les compteurs et le versement par `verse_capture.py` ; à l'étape 5b l'USB de l'analyseur branché hotte débranchée, l'enregistrement de la sonde, le niveau bas jugé fonctionnellement (0,6 V si mesuré, instrument noté), le versement et la fin de séance de la règle 8 ; les 4 tests décisifs et la règle de choix du §4 à l'étape 6 avec l'avenant ; à l'étape 7 le critère 5 du banc, la confirmation tapée par Majid depuis un terminal, le niveau bas jugé (0,8 V si mesuré, `injection off` et hotte débranchée avant la 220 Ω), la branche B (`J1` ouvert hotte débranchée, filtre retiré) et les 6 actions × 20 ; les trois critères de sortie du §1.
- [ ] `WIRING.md` reprend le schéma du §7.1, l'adaptateur (XH 5,0 mm / PH 4,0 mm, pas de Dupont, `J1`), le câble 4 fils (≥ 300 V, gainé), le boîtier de mesure (`J2`, masse commune, coque USB-C isolée, USB-C accessible, RSSI), l'étage d'écoute (100k, `R_be` 100k / 47k / 15k, seuils 1,2 / 2 / 5 V, 25 µA, 10k de collecteur, 40 à 110 µA, 1 à 2 µs, 4 mV/°C, 0,6 V), l'étage d'injection (4,7k, 10k, 470 Ω, 220 Ω, 10 mA, 0,5 V, 0,8 V, GPIO6/GPIO7 jamais intervertis, 45 kΩ), la voie 2 (GPIO0/GPIO1, 74HCT), les diviseurs 47k/68k et 100k/33k, le tableau du §7.5 (6, 7, 0 et 1 ; à éviter 4, 5, 8, 9, 15, 12, 13, 16, 17, 21, 22 ; GPIO2 libre), le générateur du §10 (10k seul ; 4,7k + 1 nF ; fil de masse ; GPIO7 haut = ligne basse) et la règle du banc sans liaison avec la hotte ; la fiche et l'embase XH 4 broches (§1, §3, §4, contrôles 9 et 10 du §12) ; le FX2 numérique (§8) et le niveau bas jugé fonctionnellement (§5, §6) ; la polarité de +4,5 à +5,3 V (§12).
- [ ] La nomenclature contient toutes les valeurs du §12 (100k, 68k, 47k, 33k, 15k, 10k, 4,7k, 470 Ω, 220 Ω, 10 kΩ / 2 W, NPN, barrettes et cavaliers, barrettes femelles, colliers, connectique XH/PH, fil UL1007/UL1015, gaine, grippe-fils, pinces crocodile, aiguille, batterie, FX2), plus le 1 nF du §10 (compté dans les totaux), la fiche et l'embase XH 4 broches, et la plaque d'essai du banc avec son étage d'écoute.
- [ ] Les trois documents emploient les mêmes noms (`TP−`, `TP+`, `TP_Dp`, `TP_Dc`, `J1`, `J2`, `Q1`, `Q2`, `Q3`, `R_be`) et les mêmes valeurs pour les étapes 1, 2b et 3a (SECURITE et RECONNAISSANCE), pour la polarité de l'étape 4 et les 17 contrôles avant pose (WIRING et RECONNAISSANCE).
- [ ] Les motifs et critères chiffrés du banc (T = 750 µs, 4 ms, 18 ms, 100 µs, 10 min, 5 µs) ne sont **pas** dans ces fiches : ils vont dans `docs/BANC.md` (tâche 15). `WIRING.md` ne donne que le montage du banc.

- [ ] **Étape 5 : Commit**

```bash
git add docs/SECURITE.md docs/WIRING.md docs/RECONNAISSANCE.md
git commit -m "Ecrire les fiches de terrain : securite, journal et cablage" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
