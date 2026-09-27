# Captures de référence

Captures de la ligne `D` gardées dans le dépôt : étapes 5 (`etape5-<scénario>`)
et 5b (`etape5b-<scénario>`) du journal (`docs/RECONNAISSANCE.md`). Les
enregistrements bruts restent dans `logs/`, que git ignore ; on n'en verse ici
qu'une copie vérifiée.

Format : le `.jsonl` du protocole (`docs/PROTOCOLE-JSON.md`), une ligne par
message machine reçu : `{"rx_ms": <heure du Mac en ms>, "de": "usb" | "<ip>",
"l": {...}}`. Lecture : `tools/analyse.py`, `tools/json_check.py --jsonl`.

## Règles

- **Aucune clé** : ni champ `cle`, ni chaîne de 64 caractères hexadécimaux
  (clé H1 de `json cle nouvelle`, aléa de l'app, clé Wi-Fi de 64 hexa).
- **Aucun SSID ni mot de passe** : ni champ `ssid` ou `mdp`, ni champ `wifi`
  autre que le bloc `wifi` de `reseau` `ip` (état de la connexion, sans SSID),
  ni réponse à `wifi <ssid> <mdp>` (son `cmd` porte le SSID).
- **Toujours par l'outil**, jamais à la main :
  `python3 tools/verse_capture.py logs/<fichier>.jsonl <nom>`. Il refuse
  (code 1, sans rien écrire) une capture qui enfreint l'une de ces règles, une
  capture vide ou que `json_check.py --jsonl` refuse ; il n'écrase une capture
  déjà versée qu'avec `--force`. Il n'affiche jamais une ligne refusée,
  seulement son numéro et la raison.
- Une capture refusée ne se corrige pas à la main : on refait
  l'enregistrement sans la commande en cause (`json cle`, `wifi` : par l'USB,
  hors enregistrement).
- **Provenance** : l'outil ajoute une ligne au tableau ci-dessous, ou la
  remplace avec `--force`. Ce tableau reste la dernière section du fichier.

## Provenance

| Fichier | Source (`logs/`) | Début (heure du Mac) | Lignes | Transport | Firmware | Démarrage |
|---|---|---|---|---|---|---|
