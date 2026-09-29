# Protocole compagnon v1 : profil « hotte » du module produit

Ce document décrit le protocole machine du **module produit** (ESP32-C6, env
`produit`, sous-projet 2) **par différence** avec deux références :

- la ScreenBar, `benq-screenbar-halo-matter` au commit **`c58a506`**,
  [docs/PROTOCOLE-JSON.fr.md](https://github.com/Djoko-cli/benq-screenbar-halo-matter/blob/c58a506/docs/PROTOCOLE-JSON.fr.md)
  (notée « ScreenBar ») : tramage, session, enveloppe, commandes, versionnage
  et **transport UDP sur Thread** (§10) ;
- la sonde de la reconnaissance, [PROTOCOLE-JSON.md](PROTOCOLE-JSON.md)
  (notée « sonde ») : champ `appareil`, clé H1, série `HOTTE-`.

Tout ce que ce document ne dit pas est **identique** à la ScreenBar. La spec
du produit fait foi : [SPEC-PRODUIT.md](SPEC-PRODUIT.md), section 7.

- Code : `src/produit/json_out_produit.*` (briques pures, testées sur l'hôte),
  `src/produit/json_mode_produit.*` (sessions, instantanés, événements),
  `src/produit/cli_produit.cpp` (lignes de l'hôte, liste blanche),
  `src/produit/net_udp_thread.*` (transport réseau, copié de la ScreenBar),
  `src/h1_proto.*` (enveloppe H1).
- Vérification : `python3 tools/json_check.py --profil produit ...` contrôle
  toute ligne machine du produit. Les exemples de la section 7 sont vérifiés
  par `python3 tools/json_check.py --profil produit --strict --exemples
  docs/PROTOCOLE-JSON-PRODUIT.md`, lancé par `sh tools/tests/test_hote.sh`.

## 1. Ce qui ne change pas

| Référence | Ici |
|---|---|
| ScreenBar §2 à §4, §6, §9 : tramage, session, enveloppe, commandes `id=<n>`, `reponse`, versionnage | identiques ; `rev` = 4 ; tampon d'émission de l'USB de 8 Ko, comme la sonde |
| ScreenBar §10 : UDP sur Thread, port 5480, enveloppe H1, poignée de main, adresse OMR, débit plafonné à 3 000 octets/s, 24 tampons OpenThread gardés en réserve, priorité basse | identique (code copié) ; clé en NVS `produit/cle` ; découverte par le **nom d'hôte SRP** du nœud, lu dans `reseau` `ip` par l'USB |
| sonde §2 : `appareil` = `hotte`, série `HOTTE-` + MAC | identique ; une app multi-appareils distingue la sonde du produit par `hello.base.build` |

## 2. Écarts généraux

- **`hello` `base`** : `build` = `produit`, `reseau_build` = `thread`, `env` =
  `produit`.
- **`hello` `identite`** : `produit` = `Module hotte Haier`, `nom` = `Hotte`,
  `hw_txt` = `C6 SuperMini, pilote <simule|A|B|C>` ; `caps` = `hotte`, `matter`,
  `udp`, `cle`, `log`, `trames_d`, `alim`, `thermique`, `essai`, et `simule`
  dans le build simulé ; **jamais `injection`**. Un objet `thermique` porte
  `temp_max_pose_c`, le maximum de la puce depuis la pose (NVS `temp_max`),
  en degrés entiers, ou null.
- **Températures** en degrés entiers (arrondis), **tensions** en mV : le
  protocole ne porte que des entiers.
- **Clé H1** : `~/.config/hotte-produit/cle` côté Mac. `tools/hotte_udp.py cle
  <port>` la range d'après `hello.base.build`, lu par l'USB ; `--appareil
  produit` pour les sessions. La remise à zéro Matter (`decommission`) efface
  la clé, comme le départ du dernier contrôleur.

## 3. La commande `json`

Identique à la ScreenBar, avec deux écarts :

- `json trames 0|1` : événements `trame_d` (section 5.4). **Coupés au départ**,
  10 par seconde au plus ; à distance, ils se coupent seuls **60 s** après
  `json trames 1` (`hello` `base` le dit).
- Profil distant : `etat` toutes les 2 s, **`compteurs` coupés** (0 : les essais
  les demandent, `json compteurs 10000`), `reseau` toutes les 30 s, `trame_d`
  et `log` coupés. Profil USB : `etat` 1 s, `compteurs` 1 s, `reseau` 5 s.

## 4. Messages périodiques et de session

### 4.1 `config`

| Objet | Champs |
|---|---|
| `automate` | les 12 réglages de l'automate, noms de `hotte regle` (spec 4.2) |
| `surveillance` | `temp_alerte_c`, `temp_hyst_c`, `alim_hotte_min_mv`, `alim_module_min_mv`, `alim_hyst_mv`, `alim_alertes` (0 au banc : pont de mesure non monté) |
| `ligne` | `pilote` (`simule`, `A`, `B`, `C`), `mode` (`repete`, `changements`, `appuis`), `annexe` |
| `simu` | build simulé : `latence_ms`, `periode_ms`, `trame_ms`, `annexe_ms`, `prolongee_ms`, `fin` (`eteinte`, `armee`), `inconnues` (`rien`, `direct`), `refuse`, `hors_service` ; null ailleurs |

`config` est réémis à chaque session en mode machine après tout réglage
(`hotte regle`, `simu ...`, `matter ...`).

### 4.2 `etat`

Chaque bloc commence par `boot` et `up_s`, comme ceux de la sonde.

| Bloc | Champs |
|---|---|
| `hotte` | `marche` (`eteinte`, `armee`, `prolongee`, `inconnue`), `moteur` (0 à 3), `lampe` (booléen, null si inconnue), `confiance` (`confirme`, `deduit`, `presume`, `inconnu`), `source` (`fil`, `annexe`, `deduit`, `nvs`), `age_ms` (âge du dernier état **lu** ; null : rien lu depuis le démarrage) |
| `automate` | `mode`, `annexe`, `pilote` (en service), `ventilo` (null, ou `cible` `eteint`, `v1`, `v2`, `v3` et `origine` `app` ou `matter`), `lampe` (null, ou `cible` booléen et `origine`), `appui` (null, ou l'appui en vol : `touche`, `essai` 0 ou 1, `depuis_ms`), `delai_moteur_ms` (reste du délai moteur), `marche_autorisee` (arrêt du moteur lu depuis le dernier appui : règle d'or) |
| `matter` | `demarre`, `mis_en_service`, `fabriques`, `abonnements`, `ignore_ms` (reste de la fenêtre d'ignorance du démarrage), `maxint_s`, `role_demarrage` (`routeur`, `med` : au prochain démarrage), `tx_dbm` (null si illisible), `derniere` (substitution de On), `ecritures`, `reflets`, `verrou_occupe`, `ignores` |
| `thermique` | `temp_c`, `temp_max_c` (depuis le démarrage), `temp_max_pose_c` (depuis la pose), `alerte`, `alertes` (nombre), `lectures_ratees`, `seuil_c` ; températures null tant que rien n'est lu |
| `alim` | `hotte` (`+` de `CN3`, GPIO2) et `module` (après diode et fusible, GPIO3) : null, ou `mv`, `min_s_mv` (minimum de la dernière seconde), `min_mv` (depuis le démarrage), `alerte` ; `alertes_actives` |
| `sys` | l'objet `sys` de la ScreenBar, à l'identique |

### 4.3 `compteurs`

| Bloc | Champs |
|---|---|
| `hotte` | `appuis_panneau`, `appuis_module`, séquences `reussies`, `annulees`, `remplacees`, `abandons`, `echecs` par cause (`non_confirme`, `sans_lecture`, `collision`, `garde`, `pilote`, `duree`), `nouveaux_essais`, `transitions_inconnues`, `anomalies`, `marche_inconnue`, `prolongee_perimee`, `lignes_muettes`, `actions_perdues` |
| `alim` | `hotte_passages`, `module_passages` : passages sous les seuils, comptés même sans alerte |
| `radio` | `role` (rôle OpenThread réel ; null : pas encore lu, et rien d'autre), `tx_dbm`, `sensibilite_dbm`, `lien` (null, ou `avec` `parent` en MED, `routeur` voisin de meilleur RSSI en routeur : `rssi_moyen_dbm`, `lq_in`, `lq_out`), `tx_total`, `tx_retry`, `tx_echecs` (compteurs MAC), `changements_parent`, `changements_role` |

### 4.4 `reseau` `ip`

Le schéma de la ScreenBar (`frais_ms`, `srp.nom`, `adresses`, `udp.*`), plus
`udp.rafale_tx` : datagrammes de rafale émis (section 6).

## 5. Événements

### 5.1 `hotte` : changement d'état

`avant` et `apres` (`marche`, `moteur`, `lampe`), `origine` (`panneau`,
`module`, `inconnue`), `source` et `confiance` de l'état après. Un appui du
panneau est publié tout de suite (critère C2) ; pendant une séquence demandée
par Maison ou l'app, les états intermédiaires ne sont pas publiés dans Maison,
mais le compagnon les voit.

### 5.2 `sequence` : fin d'une séquence

`id` (l'entier de la commande de l'app, dans la session qui l'a envoyée ;
null pour un ordre de Maison et dans les autres sessions), `origine` (`app`,
`matter`), `sujet` (`ventilo`, `lampe`), `issue` (`ok`, `annulee`, `echec`,
`abandon`, `remplacee`), `cause` (null pour `ok`, `annulee`, `remplacee` ;
`non_confirme`, `sans_lecture`, `collision`, `garde` ou `pilote` pour un
échec ; `sans_lecture`, `pilote` ou `duree` pour un abandon), `duree_ms`,
`appuis`. L'origine d'un ordre avec id reçoit son `sequence` même hors mode
machine, comme l'`injection` de la sonde.

### 5.3 `alerte` : surveillance

`sujet` (`temperature`, `lecture_temperature`, `alim_hotte`, `alim_module`),
`etape` (`debut`, `fin`), `valeur` et `seuil` (degrés, ou mV ; null pour une
lecture de température en échec), `unite` (`c`, `mv`). **Toujours émis vers
chaque session en mode machine, sans abonnement**, et doublé d'un `log`
`notice` émis même sans `json log 1`. **Aucune action sur la hotte.**

### 5.4 `trame_d` : trame décodée du fil

`t_ms`, `origine` (`carte`, `panneau`, `module`), `sens` (`vers_panneau` pour
la carte, `vers_carte` sinon), `octets` (1 à 8, en hexa), `sautes` (lignes
sautées par le plafond de 10 par seconde, si non nul). Nom distinct du `trame`
de la sonde, qui porte des durées brutes. Dans le build simulé, le codage est
celui du banc : un appui, `A0` + le rang de la touche (`marche` 0 à `v3` 4) ;
un état, `marche << 4 | moteur << 2 | lampe`. Le codage réel viendra de
l'avenant de l'étape 6.

### 5.5 `log`

`src` : `produit`, `matter`, `hotte`, `reseau`, `simu`.

## 6. Commandes et liste blanche (spec 7.5)

| Commande | Réponse avec id | À distance |
|---|---|---|
| `hotte ventilo off\|v1\|v2\|v3\|derniere`, `hotte lampe on\|off` | `accepte`, `suite` = `sequence` ; l'événement `sequence` suit, avec le même `id` | **oui** |
| `hotte regle <nom> <valeur>` | `ok`, `usage` ou `refuse` | non |
| `reboot` | `ok`, puis redémarrage 500 ms plus tard (les valeurs NVS en attente d'abord) | **oui** (Q27) |
| `matter tx 8..20` | `ok` | **oui**, 8 à 20 dBm (Q39) |
| `matter med\|maxint\|derniere <v>`, `essai alim on\|off`, `simu ...`, `json cle ...`, `decommission` | `ok`, `usage` ou `refuse` | non |
| `radio rafale <1..30 s>` | `ok` ; datagrammes pleins non authentifiés vers la session, hors plafond de débit, dans la réserve de tampons | **seulement si `essai alim on` a été posé par l'USB** (effacé seul après 72 h) |
| famille `json` | comme la ScreenBar | liste de la ScreenBar, plus `json trames` |

Tout le reste répond `interdite` à distance, en particulier `injecte`,
`injection`, `capture` et `seuils`, qui n'existent pas dans ce build. Le code
manuel et le QR ne partent jamais à distance.

## 7. Exemples

Chaque ligne commence par `<RS>` (l'octet 0x1E) ; elles sont vérifiées par
`json_check.py --profil produit --strict --exemples`.

### 7.1 Connexion par l'USB

```
> id=1 json 1
<RS>{"v":1,"t":"hello","n":0,"ms":5210,"bloc":"base","rev":4,"fw":"0.1.0-4c1d2e3","fw_desc":"0.1.0-4c1d2e3","date":"Sep 29 2026","heure":"20:02:11","env":"produit","build":"produit","reseau_build":"thread","puce":"esp32c6","idf":"v5.5.5","arduino":"3.3.12","boot":"3FA2C901","reset":"mise_sous_tension","reset_n":1,"up_s":5,"session":{"transport":"usb","periode_ms":1000,"compteurs_ms":1000,"reseau_ms":5000,"bail_s":30,"trames":false,"log":false},"limites":{"ligne_max":1024,"cmd_max":127}}
<RS>{"v":1,"t":"hello","n":1,"ms":5211,"bloc":"identite","boot":"3FA2C901","mac":"F0F5BD012345","id":{"fabricant":"Djoko-CLI","produit":"Module hotte Haier","serie":"HOTTE-F0F5BD012345","nom":"Hotte","hw":1,"hw_txt":"C6 SuperMini, pilote simule"},"appareil":"hotte","caps":["hotte","matter","udp","cle","log","trames_d","alim","thermique","essai","simule"],"thermique":{"temp_max_pose_c":null}}
<RS>{"v":1,"t":"config","n":2,"ms":5213,"automate":{"delai_moteur_ms":3000,"entre_appuis_ms":500,"confirmation_ms":1000,"fraicheur_ms":2000,"lissage_calme_ms":700,"lissage_plafond_ms":3000,"ordre_calme_ms":150,"sequence_max_ms":15000,"attente_etat_ms":5000,"prolongee_max_ms":1200000,"ignore_demarrage_ms":2000,"sonde_vitesse":0},"surveillance":{"temp_alerte_c":70,"temp_hyst_c":5,"alim_hotte_min_mv":4500,"alim_module_min_mv":3700,"alim_hyst_mv":100,"alim_alertes":0},"ligne":{"pilote":"simule","mode":"repete","annexe":false},"simu":{"latence_ms":150,"periode_ms":500,"trame_ms":60,"annexe_ms":200,"prolongee_ms":900000,"fin":"eteinte","inconnues":"rien","refuse":false,"hors_service":false}}
<RS>{"v":1,"t":"etat","n":3,"ms":5214,"bloc":"hotte","boot":"3FA2C901","up_s":5,"marche":"eteinte","moteur":0,"lampe":false,"confiance":"confirme","source":"fil","age_ms":210}
<RS>{"v":1,"t":"etat","n":4,"ms":5215,"bloc":"automate","boot":"3FA2C901","up_s":5,"mode":"repete","annexe":false,"pilote":true,"ventilo":null,"lampe":null,"appui":null,"delai_moteur_ms":0,"marche_autorisee":true}
<RS>{"v":1,"t":"etat","n":5,"ms":5216,"bloc":"matter","boot":"3FA2C901","up_s":5,"demarre":true,"mis_en_service":true,"fabriques":1,"abonnements":1,"ignore_ms":0,"maxint_s":20,"role_demarrage":"routeur","tx_dbm":20,"derniere":true,"ecritures":6,"reflets":2,"verrou_occupe":0,"ignores":0}
<RS>{"v":1,"t":"etat","n":6,"ms":5217,"bloc":"thermique","boot":"3FA2C901","up_s":5,"temp_c":38,"temp_max_c":38,"temp_max_pose_c":null,"alerte":false,"alertes":0,"lectures_ratees":0,"seuil_c":70}
<RS>{"v":1,"t":"etat","n":7,"ms":5218,"bloc":"alim","boot":"3FA2C901","up_s":5,"hotte":{"mv":212,"min_s_mv":180,"min_mv":150,"alerte":false},"module":{"mv":204,"min_s_mv":176,"min_mv":150,"alerte":false},"alertes_actives":false}
<RS>{"v":1,"t":"etat","n":8,"ms":5219,"bloc":"sys","boot":"3FA2C901","up_s":5,"sys":{"heap":118000,"heap_min":96000,"heap_bloc":41000,"pile_boucle":3100,"boucle_max_ms":14,"json_perdus":0,"json_trop_longs":0,"rejets":0}}
<RS>{"v":1,"t":"compteurs","n":9,"ms":5220,"bloc":"hotte","appuis_panneau":0,"appuis_module":0,"reussies":0,"annulees":0,"remplacees":0,"abandons":0,"echecs":{"non_confirme":0,"sans_lecture":0,"collision":0,"garde":0,"pilote":0,"duree":0},"nouveaux_essais":0,"transitions_inconnues":0,"anomalies":0,"marche_inconnue":0,"prolongee_perimee":0,"lignes_muettes":0,"actions_perdues":0}
<RS>{"v":1,"t":"compteurs","n":10,"ms":5221,"bloc":"alim","hotte_passages":1,"module_passages":1}
<RS>{"v":1,"t":"compteurs","n":11,"ms":5222,"bloc":"radio","role":"router","tx_dbm":20,"sensibilite_dbm":-120,"lien":{"avec":"routeur","rssi_moyen_dbm":-58,"lq_in":3,"lq_out":3},"tx_total":412,"tx_retry":9,"tx_echecs":0,"changements_parent":0,"changements_role":2}
<RS>{"v":1,"t":"reseau","n":12,"ms":5223,"bloc":"ip","frais_ms":800,"srp":{"nom":"ESP-HOTTE-012345"},"adresses":[{"adr":"fd11:22::1a2b:3c4d:5e6f:7081","type":"omr","pref":true},{"adr":"fd4a:9b1c:e2d3:1:0:ff:fe00:9c00","type":"ml_eid","pref":true}],"udp":{"port":5480,"ouvert":true,"empreinte":"630DCD29","sessions":0,"provisoire":false,"rx":0,"rejets":0,"rx_perdus":0,"defis":0,"tx":0,"tx_perdus":0,"tx_erreurs":0,"tampons_libres":57,"tampons_min":57,"rafale_tx":0}}
<RS>{"v":1,"t":"reponse","n":13,"ms":5224,"id":1,"etape":"fin","cmd":"json 1","ok":true,"code":"ok","duree_ms":14,"bail_s":30,"up_s":5}
<RS>{"v":1,"t":"log","n":14,"ms":5225,"src":"produit","niv":"notice","txt":"1 jeu(x) de reglages hors bornes en NVS : valeurs par defaut"}
```

### 7.2 Un appui au panneau, puis un ordre de l'app

```
<RS>{"v":1,"t":"hotte","n":20,"ms":61020,"avant":{"marche":"armee","moteur":0,"lampe":false},"apres":{"marche":"armee","moteur":2,"lampe":false},"origine":"panneau","source":"deduit","confiance":"deduit"}
> id=7 hotte ventilo off
<RS>{"v":1,"t":"reponse","n":21,"ms":64000,"id":7,"etape":"fin","cmd":"hotte ventilo off","ok":true,"code":"accepte","duree_ms":0,"suite":"sequence"}
<RS>{"v":1,"t":"hotte","n":22,"ms":64410,"avant":{"marche":"armee","moteur":2,"lampe":false},"apres":{"marche":"armee","moteur":0,"lampe":false},"origine":"module","source":"fil","confiance":"confirme"}
<RS>{"v":1,"t":"hotte","n":23,"ms":65420,"avant":{"marche":"armee","moteur":0,"lampe":false},"apres":{"marche":"eteinte","moteur":0,"lampe":false},"origine":"module","source":"fil","confiance":"confirme"}
<RS>{"v":1,"t":"sequence","n":24,"ms":65421,"id":7,"origine":"app","sujet":"ventilo","issue":"ok","cause":null,"duree_ms":1421,"appuis":2}
<RS>{"v":1,"t":"sequence","n":25,"ms":70104,"id":null,"origine":"matter","sujet":"lampe","issue":"echec","cause":"non_confirme","duree_ms":2890,"appuis":2}
```

### 7.3 Alerte, trames, refus, fin

```
<RS>{"v":1,"t":"alerte","n":30,"ms":90000,"sujet":"temperature","etape":"debut","valeur":71,"seuil":70,"unite":"c"}
<RS>{"v":1,"t":"log","n":31,"ms":90001,"src":"produit","niv":"notice","txt":"alerte temperature debut : 71 C (seuil 70 C)"}
<RS>{"v":1,"t":"alerte","n":32,"ms":91000,"sujet":"alim_module","etape":"fin","valeur":3801,"seuil":3700,"unite":"mv"}
<RS>{"v":1,"t":"trame_d","n":33,"ms":92000,"t_ms":91990,"origine":"panneau","sens":"vers_carte","octets":"A3"}
<RS>{"v":1,"t":"trame_d","n":34,"ms":92200,"t_ms":92190,"origine":"carte","sens":"vers_panneau","octets":"18","sautes":2}
<RS>{"v":1,"t":"reponse","n":35,"ms":93000,"id":8,"etape":"fin","cmd":"injecte durees 750 750","ok":false,"code":"interdite","msg":"interdite a distance (spec 7.5) : USB seulement","duree_ms":0}
<RS>{"v":1,"t":"hb","n":36,"ms":94000,"boot":"3FA2C901","up_s":94,"json_perdus":0}
<RS>{"v":1,"t":"fin","n":37,"ms":95000,"cause":"commande"}
```

## 8. Transport réseau : UDP sur Thread

Celui de la ScreenBar (§10), code copié (`src/produit/net_udp_thread.*`) :

- le Mac joint le nœud par son **adresse OMR** ou son **nom SRP**, lus dans
  `reseau` `ip` par l'USB ; il garde sa route Thread avec l'assistant de benq,
  `tools/macos/halo-routes` au commit `c58a506` ;
- `python3 tools/hotte_udp.py --appareil produit session <nom SRP ou IPv6>
  "hotte ventilo v2"` ouvre une session, envoie l'ordre, montre les lignes ;
- la liste blanche est celle de la section 6.
