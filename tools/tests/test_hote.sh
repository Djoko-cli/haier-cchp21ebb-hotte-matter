#!/bin/sh
# Tests hote de la sonde (sans carte). A lancer depuis n'importe ou : sh tools/tests/test_hote.sh
set -e
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/hotte-tests"
mkdir -p "$OUT"
CXX="clang++ -std=c++17 -Wall -Wextra -Werror -Isrc"
# Une commande par ligne : sous 'set -e', un echec a gauche d'un '&&' passerait inapercu.
$CXX src/capture_model.cpp tools/tests/test_capture.cpp -o "$OUT/test_capture"
"$OUT/test_capture"
$CXX src/json_out.cpp tools/tests/test_json.cpp -o "$OUT/test_json"
"$OUT/test_json" "$OUT/test_json_lignes.txt"
# Lignes realistes de test_json et exemples du protocole : conformes au profil hotte.
python3 tools/json_check.py --strict --independantes -q "$OUT/test_json_lignes.txt"
python3 tools/json_check.py --strict -q --exemples docs/PROTOCOLE-JSON.md
$CXX src/motifs.cpp tools/tests/test_generateur.cpp -o "$OUT/test_generateur"
"$OUT/test_generateur"
echo "tests hote : OK"
