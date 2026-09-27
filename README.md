# Hotte Haier CCHP21EBB → Matter sur Thread

Piloter une hotte **Haier CCHP21EBB** (90 cm, 3 vitesses, éclairage LED)
depuis Apple Home ou tout autre contrôleur Matter, avec un **ESP32-C6** en
Matter sur Thread. La hotte garde son panneau tactile d'origine, dont les
voyants restent synchronisés avec l'app.

> **Statut : sous-projet 1, la reconnaissance.** Les fiches de terrain, le
> firmware de la sonde, celui du générateur de banc et les outils Mac sont
> écrits et testés sur l'hôte ; le banc de validation consigne ses résultats
> dans [docs/BANC.md](docs/BANC.md). Restent les étapes sur la hotte (−1 à 7,
> [journal](docs/RECONNAISSANCE.md)), puis le protocole décodé
> (`docs/PROTOCOL.md`, à l'étape 6) et le choix de l'architecture.

## Principe visé

Le panneau tactile parle à la carte de puissance (`XB_DYB_V4`) par **un seul
fil de données**, sur le connecteur `CN3 « - D + »`. L'ESP32 se branche sur ce
fil par un adaptateur réversible, **sans jamais toucher au secteur**. Il
« appuie » sur les touches en émettant les mêmes trames que le panneau. La
carte d'origine reste maître : relais, bips et voyants restent cohérents.

Avant d'écrire le produit, il faut connaître ce protocole. C'est l'objet de la
reconnaissance.

## ⚠️ Sécurité

La carte de puissance est **alimentée en 230 V**. Seul le coin des connecteurs
basse tension (`CN1`, `CN2`, `CN3`) est concerné par ce projet, et on n'y
touche **que hotte débranchée**. Les règles sont dans
[docs/SPEC-RECONNAISSANCE.md §5](docs/SPEC-RECONNAISSANCE.md#5-règles-de-sécurité),
en fiche à garder sous les yeux dans [docs/SECURITE.md](docs/SECURITE.md).
Si tu reproduis ce projet, tu le fais sous ta propre responsabilité.

## La sonde

Un ESP32-C6 SuperMini sur le boîtier de mesure, hors de la hotte
([docs/WIRING.md](docs/WIRING.md)) :

- **écoute** de la ligne `D` sur GPIO6, par un étage à transistor (niveaux
  remis dans le sens du bus), avec le pilote RMT d'IDF en réception continue :
  des parties horodatées de durées consécutives, découpées en trames sur le
  Mac ;
- **protocole compagnon v1** de la ScreenBar, profil « hotte »
  ([docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md)) : par l'USB, ou en UDP
  sur le Wi-Fi (port 5480, enveloppe H1 authentifiée, `hotte-sonde.local`) ;
- **injection bornée** de l'étape 7 sur GPIO7 (étage en drain ouvert), avec
  les garde-fous du §8.4 de la spec : étage déclaré monté, armement limité à
  10 min, délai minimal, attente du silence, arrêt sur collision.

Le second C6, avec le firmware `generateur`, imite la ligne au banc avec des
motifs à contenu connu ([docs/BANC.md](docs/BANC.md)).

Construire (PlatformIO, même plateforme que la ScreenBar : pioarduino
55.03.312-1, Arduino-ESP32 3.3.12, IDF 5.5) :

```
~/.platformio/penv/bin/pio run -e sonde
~/.platformio/penv/bin/pio run -e generateur
```

On ne flashe la sonde que **hotte débranchée** ou au banc
(`-t upload --upload-port <port>`). Sur la console USB (115200 bauds), `help`
liste les commandes.

## Outils Mac

Python 3.11, bibliothèque standard seulement (le port série s'ouvre par
`termios`, sans redémarrer le C6).

| Outil | Rôle |
|---|---|
| `tools/serie_enregistre.py <port> <scénario> [commande ...]` | session machine par l'USB, enregistrée dans `logs/AAAA-MM-JJ-hhmm-<scénario>.jsonl` |
| `tools/hotte_udp.py cle <port>` | clé H1 posée par l'USB, rangée dans `~/.config/hotte-sonde/cle` (jamais affichée) |
| `tools/hotte_udp.py session <hôte> [commande ...]` | session réseau, résumé lisible ; `injection on` ne part qu'après la phrase « Majid devant la hotte », tapée dans un terminal (règle 11) |
| `tools/hotte_udp.py enregistre <hôte> <scénario> [commande ...]` | même session, enregistrée en `.jsonl`, résumé dans `logs/live.log` |
| `tools/analyse.py histo\|trames\|auto\|uart\|diff <capture>` | histogrammes, redécoupage en trames, décodage UART, distance d'impulsion ou Manchester, différences entre scénarios (étape 6) |
| `tools/banc.py <capture> <motif>` | verdicts du banc : trames décodées au bit près, écarts de durées |
| `tools/json_check.py [--jsonl] <capture>` | conformité des lignes machine au profil hotte |
| `tools/verse_capture.py <capture.jsonl> <nom>` | verse une capture de référence (étapes 5 et 5b) de `logs/` dans `captures/`, avec sa provenance ; refuse toute clé, tout SSID ou mot de passe |

Tests sans carte, depuis la racine du dépôt :

```
sh tools/tests/test_hote.sh                     # C++ (clang++) : capture, protocole, motifs, H1, injection
python3 -m unittest discover -s tools/tests -v  # outils Python
```

## Documents

| Document | Contenu |
|---|---|
| [docs/SPEC-RECONNAISSANCE.md](docs/SPEC-RECONNAISSANCE.md) | sous-projet 1 : objectif, sécurité, étapes, adaptateur, firmware « sonde », outils, banc de validation |
| [docs/SECURITE.md](docs/SECURITE.md) | les 13 règles en fiche, tests d'isolation et de fuite, conduite en cas d'anomalie |
| [docs/RECONNAISSANCE.md](docs/RECONNAISSANCE.md) | journal des étapes −1 à 7 : mesures, photos, décisions |
| [docs/WIRING.md](docs/WIRING.md) | adaptateur, câble de sortie, boîtier de mesure, étages, brochage, nomenclature |
| [docs/BANC.md](docs/BANC.md) | banc de validation : procédure, critères 1 à 6, résultats |
| [docs/PROTOCOLE-JSON.md](docs/PROTOCOLE-JSON.md) | profil hotte du protocole compagnon, par différence avec la ScreenBar |
| [docs/BRIEF-RECHERCHE.md](docs/BRIEF-RECHERCHE.md) | recherche du 24/09/2026 : identification de la carte, protocoles plausibles, Matter et Apple Home, 57 sources |
| [docs/photos/](docs/photos/) | photos de la carte et de l'étiquette (numéro de série masqué) |
| [captures/](captures/README.md) | captures de référence des étapes 5 et 5b, versées par `tools/verse_capture.py` : règles et provenance |

## Les trois sous-projets

1. **Reconnaissance** (en cours) : décoder la ligne `D`, choisir l'architecture,
   prouver l'injection.
2. **Produit** : interface définitive et firmware Matter (ventilateur 3 vitesses,
   lumière, arrêt différé).
3. **Bibliothèque commune et app compagnon** multi-appareils, partagées avec le
   projet frère.

## Projet frère

[benq-screenbar-halo-matter](https://github.com/Djoko-cli/benq-screenbar-halo-matter) :
une BenQ ScreenBar Halo pilotée en Matter sur Thread par un ESP32-C6. Même
plateforme (Arduino-ESP32 3.3.12 via pioarduino), même protocole compagnon. Le
code repris ici vient de son commit
[`c58a506`](https://github.com/Djoko-cli/benq-screenbar-halo-matter/tree/c58a506).
