# Protocole compagnon v1 : profil « hotte »

Ce document décrit le protocole machine de la **sonde de la ligne `D`**
(ESP32-C6, env `sonde`) **par différence** avec celui de la ScreenBar. La
référence est le protocole de `benq-screenbar-halo-matter` au commit
**`c58a506`** :
[docs/PROTOCOLE-JSON.fr.md](https://github.com/Djoko-cli/benq-screenbar-halo-matter/blob/c58a506/docs/PROTOCOLE-JSON.fr.md)
(noté « ScreenBar » ci-dessous, avec le numéro de section). Tout ce que ce
document ne dit pas est **identique** à la ScreenBar.

- Code : `src/json_out.*` (briques pures, testées sur l'hôte),
  `src/json_mode.*` (sessions, instantanés, événements), `src/cli.cpp`
  (lignes de l'hôte).
- Vérification : `tools/json_check.py` contrôle toute ligne machine contre ce
  profil (capture série brute, ou `.jsonl` avec `--jsonl`). Les exemples de la
  section 8 sont vérifiés par
  `python3 tools/json_check.py --strict --exemples docs/PROTOCOLE-JSON.md`,
  lancé par `sh tools/tests/test_hote.sh`.
- État : transport USB. Le transport réseau (UDP sur le Wi-Fi, ScreenBar §10)
  et l'injection viennent ensuite ; ce document s'étendra avec eux.

## 1. Ce qui ne change pas

| ScreenBar | Ici |
|---|---|
| §2 Tramage : RS + JSON compact ASCII + LF, 1 024 octets au plus, budget de pire cas de 896, découpage par le dernier RS, sens app → carte | identique, sauf le tampon d'émission de l'USB : **8 Ko** au lieu de 4 (section 2) |
| §3 Session : ouverture du port, **DTR = RTS = 0 en un seul appel** (§3.2), séquence de connexion, bail et `json ping`, battement, aucune persistance, écho et invite | identique ; la commande `json` a ses bornes en section 3 |
| §4 Enveloppe : `v`, `t`, `n`, `ms`, puis `bloc` ; entiers seulement | identique, plus `t_us` sur 64 bits (section 2) |
| §6 Commandes : préfixe `id=<n>`, message `reponse`, cadence de 20 lignes par seconde, une commande en vol | identique ; ce qui a une réponse sans texte est en section 5 |
| §9 Versionnage : `v` majeure, ajouts sans changer `v`, l'app ignore l'inconnu | identique ; `rev` = 4 |
| §5, §7, §8, §11, §12 : messages et exemples de la lampe | remplacés par les sections 4 à 8 |

## 2. Écarts généraux

- **`appareil`** : nouveau champ de `hello` `identite`, `"hotte"` ici. Une app
  multi-appareils choisit sa vue d'après lui ; absent, il vaut `"screenbar"`.
- **`t_us` est un entier sur 64 bits** (`esp_timer_get_time()`, en µs depuis le
  démarrage). Il déroge à la convention « uint32 » de la ScreenBar (§4) ; il
  reste sûr en JSON jusqu'à 2^53 − 1 (285 ans). `Writer::u64` l'écrit ;
  `json_check.py` l'accepte jusqu'à 2^53 − 1.
- **Tampon d'émission de l'USB : 8 Ko** (`Serial.setTxBufferSize(8192)`), pour
  qu'une dizaine de lignes `trame` tiennent pendant que le Mac lit. Les règles
  de la ScreenBar §2.3 sont inchangées : une ligne qui ne tient pas est perdue
  et comptée, jamais écrite à moitié ; une ligne périodique ne part que s'il
  reste ensuite 1 024 octets libres.
- **`rev` = 4** : la révision de la ScreenBar au commit `c58a506`, dont le
  profil hotte part. `build` = `sonde`, `reseau_build` = `aucun`.
- **Codes de `reponse`** : `ok`, `accepte`, `en_cours`, `execute` (avec `ok`
  vrai) ; `usage`, `refuse`, `inconnue`, `trop_long`, `cadence`, `interdite`,
  `deja_traite` (avec `ok` faux). Les codes de la lampe (`differe`,
  `radio_absente`, `radio_perdue`) n'existent pas ici. `suite` ne prend
  qu'une valeur, `injection`, avec le code `accepte`.

## 3. Session : la commande `json`

Comme la ScreenBar §3.4, avec ces bornes :

| Commande | Effet | Bornes (USB) |
|---|---|---|
| `json` | état de la session, en texte | |
| `json 1 [bail <s>]` | mode machine ; `hello`, `config` et l'instantané complet par la file des périodiques, puis la `reponse` | bail 0 (aucun) ou 10..600 s, défaut 30 |
| `json 0` | retour au mode humain : `fin`, puis l'invite | |
| `json etat` | instantané : `etat` (3 blocs) et `compteurs` | |
| `json hello` | `hello` (2 blocs) et `config` | |
| `json ping` | renouvelle le bail ; la `reponse` porte `bail_s` et `up_s` | |
| `json periode <ms>` | période des `etat` | 0 ou 200..60000, défaut **1 000** |
| `json compteurs <ms>` | période des `compteurs` | 0 ou 200..60000, défaut **1 000** |
| `json reseau <ms>` | période des `reseau` (aucun bloc tant que le transport réseau manque) | 0 ou 1000..60000, défaut **5 000** |
| `json trames 0\|1` | événements `trame` | défaut 1 |
| `json log 0\|1` | annonces du firmware en messages `log` | défaut 0 |
| `json cle ...` | clé du transport réseau | `refuse` tant que le transport réseau manque |

`json trames 0` coupe l'envoi des `trame` à cette session ; `capture off`
arrête la capture elle-même. Les trames ne se coupent jamais seules (écart à
la ScreenBar §10.5 : elles sont la raison d'être de la sonde).

Le bail compte comme à la ScreenBar (§3.5). Pour un enregistrement long par
l'USB, `tools/serie_enregistre.py` ouvre la session par `json 1 bail 0` et la
ferme par `json 0`.

## 4. Messages périodiques et de session

Les blocs `etat` portent `boot` et `up_s` en tête, comme à la ScreenBar : ils
servent de battement (`hb` toutes les 2 s si `periode_ms` vaut 0 ou plus de
2 000) et révèlent un redémarrage (§3.6).

### 4.1 `hello`

**Bloc `base`** : **tous** les champs de la ScreenBar §5.1, dans le même ordre.
Valeurs propres : `build` = `sonde`, `reseau_build` = `aucun`, `env` =
`sonde`, `session.transport` = `usb`. `fw` doit égaler `fw_desc`.

**Bloc `identite`** : **tous** les champs de la ScreenBar, puis `appareil`,
puis `caps`.

| Champ | Valeur |
|---|---|
| `mac` | MAC-48 d'usine, 12 hexa |
| `id.fabricant` | `Djoko-CLI` |
| `id.produit` | `Sonde hotte Haier` |
| `id.serie` | `HOTTE-` + MAC en 12 hexa majuscules |
| `id.nom` | `Sonde hotte` |
| `id.hw`, `id.hw_txt` | `1`, `C6 SuperMini, etages v1` |
| `appareil` | `hotte` |
| `caps` | `sonde` (capture de la ligne `D`), `injection` (présente dans ce build), `trames`, `log` ; puis `udp`, `cle`, `mdns` avec le transport réseau |

### 4.2 `config`

Émise avec `hello`, et de nouveau après chaque `capture tout`,
`capture changements` ou `seuils <valeurs>` accepté, même si les valeurs ne
changent pas (`sondeAppliquer`). `capture on` et `capture off` ne changent pas
la `config` : l'état se lit dans `etat.capture.active`.

| Champ | Type | Sens |
|---|---|---|
| `capture.gpio` | entier | broche d'écoute (6) |
| `capture.resol_hz` | entier | 1 000 000 ou 500 000 |
| `capture.filtre_us` | entier 0..3 | filtre anti-parasites du RMT |
| `capture.silence_us` | entier | silence de fin de réception : 1 000 µs à 32 767 ticks |
| `capture.mode` | `tout` ou `changements` | mode d'émission (spec §8.2) |
| `capture.inverse` | booléen | étage d'écoute inverseur : les niveaux publiés sont ceux **du bus** |

### 4.3 `etat`

Période `periode_ms` (1 000 ms sur l'USB). Trois blocs.

**Bloc `bus`** :

| Champ | Type | Sens |
|---|---|---|
| `repos` | `haut`, `bas` ou null | niveau du bus lu hors réception (aucun front depuis le silence de fin) ; null s'il n'a jamais été lu |
| `fronts` | entier | fronts de la ligne vus depuis le démarrage (interruption GPIO) |
| `derniere_ms` | entier ou null | âge du dernier front ; null avant le premier |
| `receptions_s` | entier | réceptions pendant la dernière seconde complète |

**Bloc `capture`** :

| Champ | Type | Sens |
|---|---|---|
| `active` | booléen | canal RMT en réception (`capture on`, `capture off`) |
| `mode` | `tout` ou `changements` | |
| `debord` | entier | blocs perdus depuis le démarrage, tampon circulaire plein |
| `rep_en_cours` | entier | mode `changements` : réceptions identiques depuis la dernière émise |

**Bloc `sys`** : un objet `sys` **identique** à celui de la ScreenBar (§5.3,
bloc `sante`) : `heap`, `heap_min`, `heap_bloc`, `pile_boucle`,
`boucle_max_ms`, `json_perdus`, `json_trop_longs`, `rejets`, comptés pour la
session qui reçoit la ligne.

### 4.4 `compteurs`

Période `compteurs_ms` (1 000 ms sur l'USB). Un bloc, `sonde`, de compteurs
cumulés depuis le démarrage (l'app trace des différences) :

| Champ | Sens |
|---|---|
| `receptions` | réceptions commencées (numéro `num` de la dernière) |
| `parties` | parties produites par le découpeur (lignes `trame` possibles) |
| `blocs`, `symboles` | blocs et symboles rendus par le pilote RMT, perdus compris |
| `debord` | blocs perdus, tampon circulaire plein |
| `rep` | mode `changements` : réceptions identiques non émises |
| `lignes_perdues` | lignes machine perdues, toutes sessions (tampon d'émission plein, retard, file pleine) |
| `sautes` | lignes `trame` non produites, toutes sessions (plafond de 100 par seconde) |
| `rejets` | lignes de l'hôte refusées, toutes origines (`trop_long`, `cadence`, `interdite`) |

Critère 4 du banc (spec §10) : `lignes_perdues`, `debord` et `sautes` à 0.

### 4.5 `hb` et `fin`

Identiques à la ScreenBar §5.6.

## 5. Commandes de l'app

Même forme que la ScreenBar §6 : texte de la console préfixé par `id=<n> `.

| Commande avec `id` | Déroulement | Messages |
|---|---|---|
| famille `json` | comme la ScreenBar | lignes produites, puis `reponse` `fin` |
| `capture on\|off\|tout\|changements`, `seuils <filtre_us> [silence_us] [resol_hz]` | **sans texte** : la réponse porte le résultat | `reponse` `fin` : `ok` (avec un `msg` si le mode n'a pas pu être écrit en NVS), `usage` (mot inconnu ou valeur hors bornes, avec `msg`) ou `refuse` (RMT ou NVS en échec) ; puis `config` après `tout`, `changements` et `seuils` |
| toute autre commande (`info`, `bus`, `stats`, `help`, `seuils` seul...) | inchangée : texte humain | `reponse` `debut`, le texte, puis `reponse` `fin` `execute` (ou `inconnue`) |

Sans `id`, rien ne change : texte humain, aucune `reponse`. `seuils` vérifie
toutes les valeurs **avant** d'écrire en NVS (spec §8.2).

## 6. Événements

Plafonds par type et par session (au-delà, l'événement n'est pas produit pour
cette session, ne consomme pas de `n`, et le suivant du même type porte
`sautes`) :

| Type | Plafond | Condition |
|---|---|---|
| `trame` | 100 par seconde | `json trames 1` (défaut) |
| `log` | 20 par seconde | `json log 1` |
| `injection` | aucun | toujours |

### 6.1 `trame` : une partie de réception

La sonde ne découpe pas en trames : elle publie des **parties** horodatées de
durées consécutives. Une réception va du premier front jusqu'au silence de
fin ; au-delà de 110 durées, elle part en plusieurs parties au fil de l'eau.
Le découpage en trames se fait sur le Mac (`tools/analyse.py`).

| Champ | Type | Sens |
|---|---|---|
| `num` | entier | numéro de réception depuis le démarrage (à partir de 1) |
| `part` | entier | rang de la partie dans la réception (à partir de 0) |
| `fin` | booléen | dernière partie de la réception |
| `t_us` | entier 64 bits | début de la partie (µs depuis le démarrage) |
| `niv0` | `haut` ou `bas` | niveau **du bus** pendant `dur_us[0]` ; les niveaux alternent ensuite |
| `dur_us` | tableau d'entiers ≥ 1 | 110 durées au plus, en µs ; vide seulement pour clore une réception (`fin`) ou signaler une perte (`debord`) |
| `debord` | booléen | des symboles ont été perdus juste avant cette partie (tampon plein) |
| `rep` | entier, facultatif | mode `changements` : réceptions identiques non émises avant celle-ci |
| `sautes` | entier, facultatif | lignes `trame` non produites (plafond) depuis la précédente produite pour cette session |

Règles :
- **Les parties d'une réception sont produites ou sautées ensemble** : le
  plafond se juge à la première partie vue de chaque réception.
- `t_us` : heure de fin du bloc RMT, moins la somme de ses durées, moins le
  silence si c'est la dernière partie (spec §8.2). La partie suivante d'une
  même réception commence à la fin de la précédente.
- Pire cas : 860 octets (110 durées de 65 534 µs, compteurs au maximum), sous
  le budget de 896 (test hôte `test_json`).

### 6.2 `injection` : fin d'une injection

Les commandes `injection` et `injecte` (spec §8.3 et §8.4) arrivent avec
l'étage d'injection ; le format de leur événement est fixé :
`id` (celui de la commande, ou null), `cmd` (40 caractères au plus),
`resultat` (`ok`, `collision`, `delai`), `niv0`, `dur_us` (durées émises, 64
au plus), `attente_us` (attente du silence), `relu_us` (durées relues pendant
l'émission).

### 6.3 `log` : annonces du firmware

Comme la ScreenBar §7.10, avec `src` = `sonde`, `capture`, `injection` ou
`reseau`, et `niv` = `notice` ou `trace`. Les modules y passent leurs
annonces par `jsonLog()` (Wi-Fi, injection : avec eux) ; sans `json log 1`,
elles restent du texte.

Une annonce part sans `json log 1` : si des réglages lus en NVS au démarrage
étaient hors bornes (remplacés par leur valeur par défaut, spec §8.2), chaque
ouverture de session (`json 1`) le dit par un `log` `sonde` `notice`, après
l'instantané et sa réponse, hors plafond (exemple en 8.3). Le texte
`[reglages] ...` du démarrage part en général avant que l'app n'ouvre le port.

## 7. Débit sur l'USB

| Situation | Débit |
|---|---|
| Au repos (`etat` et `compteurs` à 1 Hz, tailles de la section 8) | ~0,7 Ko/s |
| Trames de type WTC (une réception de 33 durées toutes les 100 ms, 258 octets par ligne) | + ~2,6 Ko/s |
| Plafond des trames (100 lignes pleines par seconde) | + ~86 Ko/s |

L'USB full-speed n'est pas le goulot : ce sont le tampon de 8 Ko et la
lecture du Mac. Une ligne perdue se voit à un trou de `n` et dans
`lignes_perdues`.

## 8. Exemples

`<RS>` note l'octet 0x1E ; le LF final est omis. Chaque ligne `<RS>{...}` est
vérifiée par `json_check.py --strict --exemples`.

### 8.1 Connexion

App → sonde (`\x15` : l'octet 0x15, Ctrl-U, suivi de LF) :

```
\x15
id=1 json 1
```

Sonde → app :

```
<RS>{"v":1,"t":"hello","n":0,"ms":12031,"bloc":"base","rev":4,"fw":"0.1.0-1a2b3c4","fw_desc":"0.1.0-1a2b3c4","date":"Sep 27 2026","heure":"14:02:11","env":"sonde","build":"sonde","reseau_build":"aucun","puce":"esp32c6","idf":"v5.5.5","arduino":"3.3.12","boot":"3FA2C901","reset":"mise_sous_tension","reset_n":1,"up_s":12,"session":{"transport":"usb","periode_ms":1000,"compteurs_ms":1000,"reseau_ms":5000,"bail_s":30,"trames":true,"log":false},"limites":{"ligne_max":1024,"cmd_max":127}}
<RS>{"v":1,"t":"hello","n":1,"ms":12032,"bloc":"identite","boot":"3FA2C901","mac":"F0F5BD012345","id":{"fabricant":"Djoko-CLI","produit":"Sonde hotte Haier","serie":"HOTTE-F0F5BD012345","nom":"Sonde hotte","hw":1,"hw_txt":"C6 SuperMini, etages v1"},"appareil":"hotte","caps":["sonde","injection","trames","log"]}
<RS>{"v":1,"t":"config","n":2,"ms":12033,"capture":{"gpio":6,"resol_hz":1000000,"filtre_us":1,"silence_us":5000,"mode":"tout","inverse":true}}
<RS>{"v":1,"t":"etat","n":3,"ms":12034,"bloc":"bus","boot":"3FA2C901","up_s":12,"repos":"haut","fronts":1520,"derniere_ms":3,"receptions_s":10}
<RS>{"v":1,"t":"etat","n":4,"ms":12035,"bloc":"capture","boot":"3FA2C901","up_s":12,"active":true,"mode":"tout","debord":0,"rep_en_cours":0}
<RS>{"v":1,"t":"etat","n":5,"ms":12036,"bloc":"sys","boot":"3FA2C901","up_s":12,"sys":{"heap":247812,"heap_min":241600,"heap_bloc":110580,"pile_boucle":5316,"boucle_max_ms":2,"json_perdus":0,"json_trop_longs":0,"rejets":0}}
<RS>{"v":1,"t":"compteurs","n":6,"ms":12037,"bloc":"sonde","receptions":118,"parties":118,"blocs":118,"symboles":1947,"debord":0,"rep":0,"lignes_perdues":0,"sautes":0,"rejets":0}
<RS>{"v":1,"t":"reponse","n":7,"ms":12038,"id":1,"etape":"fin","cmd":"json 1","ok":true,"code":"ok","duree_ms":7,"bail_s":30,"up_s":12}
```

Puis `etat` et `compteurs` chaque seconde, et les `trame`.

### 8.2 Trames

Une réception courte, d'une partie (motif `wtc` du banc : départ de 2T bas,
16 bits, T = 750 µs ; le dernier 1T haut se fond dans le repos) :

```
<RS>{"v":1,"t":"trame","n":214,"ms":15506,"num":57,"part":0,"fin":true,"t_us":15472110,"niv0":"bas","dur_us":[1500,749,751,748,748,752,748,750,752,748,752,749,748,748,751,751,2248,749,748,752,751,748,752,748,749,752,748,752,752,751,748,749,748],"debord":false}
```

Une longue réception (motif `rafale` : un front toutes les 100 µs) part en
parties de 110 durées, sans `fin` jusqu'au silence :

```
<RS>{"v":1,"t":"trame","n":301,"ms":16020,"num":61,"part":0,"fin":false,"t_us":16003300,"niv0":"haut","dur_us":[102,99,100,101,99,102,98,102,100,102,99,98,102,102,99,100,98,102,98,102,98,102,99,101,102,101,100,101,102,101,100,100,99,99,99,98,102,100,102,101,100,101,100,102,98,98,102,101,99,100,99,101,101,98,98,102,102,100,100,100,102,101,102,101,98,98,100,101,98,98,100,102,101,100,101,100,98,101,100,99,102,98,101,98,99,100,99,99,101,101,101,98,99,101,101,102,100,99,101,102,100,101,100,101,99,99,98,99,99,99],"debord":false}
```

Mode `changements` : douze réceptions identiques n'ont pas été émises avant
celle-ci. Plus loin, 37 lignes ont été sautées (plafond de 100 par seconde),
et un bloc a été perdu juste avant la partie :

```
<RS>{"v":1,"t":"trame","n":420,"ms":31002,"num":190,"part":0,"fin":true,"t_us":30975310,"niv0":"bas","dur_us":[1500,750,750,750,2250,751],"debord":false,"rep":12}
<RS>{"v":1,"t":"trame","n":988,"ms":52001,"num":4102,"part":0,"fin":false,"t_us":51990112,"niv0":"haut","dur_us":[100,101,99,100],"debord":true,"rep":0,"sautes":37}
```

### 8.3 Réglages de capture

```
id=2 capture changements
id=3 seuils 2 40000
```

```
<RS>{"v":1,"t":"reponse","n":502,"ms":40002,"id":2,"etape":"fin","cmd":"capture changements","ok":true,"code":"ok","duree_ms":14}
<RS>{"v":1,"t":"config","n":503,"ms":40003,"capture":{"gpio":6,"resol_hz":1000000,"filtre_us":1,"silence_us":5000,"mode":"changements","inverse":true}}
<RS>{"v":1,"t":"reponse","n":510,"ms":40410,"id":3,"etape":"fin","cmd":"seuils 2 40000","ok":false,"code":"usage","msg":"seuils : silence 40000 us hors bornes a 1000000 Hz (1000..32767)","duree_ms":0}
```

Une valeur hors bornes lue en NVS au démarrage (écrite par un autre firmware,
par exemple) est remplacée par sa valeur par défaut. Chaque `json 1` le dit
ensuite, juste après sa réponse, même sans `json log 1` :

```
<RS>{"v":1,"t":"log","n":1208,"ms":305012,"src":"sonde","niv":"notice","txt":"1 valeur(s) hors bornes en NVS : valeurs par defaut"}
```

### 8.4 Commande à texte, refus

```
id=4 stats
```

```
<RS>{"v":1,"t":"reponse","n":530,"ms":42002,"id":4,"etape":"debut","cmd":"stats","ok":true,"code":"en_cours"}
receptions 1204, parties 1204, blocs 1204, symboles 20468, debord 0 (blocs perdus, tampon plein)
<RS>{"v":1,"t":"reponse","n":531,"ms":42003,"id":4,"etape":"fin","cmd":"stats","ok":true,"code":"execute","duree_ms":1}
<RS>{"v":1,"t":"reponse","n":588,"ms":43010,"id":25,"etape":"fin","cmd":"json ping","ok":false,"code":"cadence","msg":"plus de 20 lignes par seconde : rien n'est execute","duree_ms":0}
```

### 8.5 Mode `log`, battement, fin de session

Après `id=5 json log 1`, `id=6 json periode 0` et `id=7 json compteurs 0`
(battement `hb` toutes les 2 s), l'app se tait ; 30 s après sa dernière ligne,
le bail rend le mode humain.

```
<RS>{"v":1,"t":"log","n":640,"ms":200100,"src":"capture","niv":"notice","txt":"[capture] echec du RMT ('capture on' pour reessayer)"}
<RS>{"v":1,"t":"hb","n":641,"ms":202000,"boot":"3FA2C901","up_s":202,"json_perdus":0}
<RS>{"v":1,"t":"fin","n":662,"ms":231400,"cause":"bail"}
json : mode machine coupe (hote muet depuis 30 s)
>
```

### 8.6 Événement `injection`

Format seulement (les commandes viennent avec l'étage d'injection) :

```
<RS>{"v":1,"t":"reponse","n":900,"ms":123001,"id":42,"etape":"fin","cmd":"injecte durees 750 750 750 2250","ok":true,"code":"accepte","duree_ms":1,"suite":"injection"}
<RS>{"v":1,"t":"injection","n":901,"ms":123456,"id":42,"cmd":"injecte durees 750 750 750 2250","resultat":"ok","niv0":"bas","dur_us":[750,750,750,2250],"attente_us":20412,"relu_us":[752,748,751,2249]}
```
