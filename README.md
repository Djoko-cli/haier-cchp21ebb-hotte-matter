# Hotte Haier CCHP21EBB → Matter sur Thread

Piloter une hotte **Haier CCHP21EBB** (90 cm, 3 vitesses, éclairage LED)
depuis Apple Home ou tout autre contrôleur Matter, avec un **ESP32-C6** en
Matter sur Thread. La hotte garde son panneau tactile d'origine, dont les
voyants restent synchronisés avec l'app.

> **Statut : sous-projet 1, la reconnaissance.** La spec est écrite. Rien n'a
> encore été mesuré sur la hotte, et il n'y a pas encore de firmware.

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
[docs/SPEC-RECONNAISSANCE.md §5](docs/SPEC-RECONNAISSANCE.md#5-règles-de-sécurité).
Si tu reproduis ce projet, tu le fais sous ta propre responsabilité.

## Documents

| Document | Contenu |
|---|---|
| [docs/SPEC-RECONNAISSANCE.md](docs/SPEC-RECONNAISSANCE.md) | sous-projet 1 : objectif, sécurité, étapes, adaptateur, firmware « sonde », outils, banc de validation |
| [docs/BRIEF-RECHERCHE.md](docs/BRIEF-RECHERCHE.md) | recherche du 24/09/2026 : identification de la carte, protocoles plausibles, Matter et Apple Home, 57 sources |
| [docs/photos/](docs/photos/) | photos de la carte et de l'étiquette (numéro de série masqué) |

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
plateforme (Arduino-ESP32 3.3.12 via pioarduino), même protocole compagnon.
