#!/bin/sh
# Tests hote de la sonde et du produit (sans carte). A lancer depuis n'importe ou : sh tools/tests/test_hote.sh
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
$CXX tools/tests/test_wifi.cpp -o "$OUT/test_wifi"
"$OUT/test_wifi"
# Enveloppe H1 : crypto de CommonCrypto (macOS, dans libSystem : rien a lier) ; mbedTLS sur la carte.
$CXX src/h1_proto.cpp tools/tests/test_h1.cpp -o "$OUT/test_h1"
"$OUT/test_h1"
$CXX src/injection_regles.cpp src/json_out.cpp tools/tests/test_injection.cpp -o "$OUT/test_injection"
"$OUT/test_injection"
# Produit (sous-projet 2) : modules purs de src/produit/.
CXXP="$CXX -Isrc/produit"
$CXXP src/produit/hotte_etat.cpp tools/tests/test_hotte_etat.cpp -o "$OUT/test_hotte_etat"
"$OUT/test_hotte_etat"
$CXXP src/produit/hotte_etat.cpp src/produit/pilote_simule.cpp tools/tests/test_pilote_simule.cpp -o "$OUT/test_pilote_simule"
"$OUT/test_pilote_simule"
$CXXP src/produit/hotte_etat.cpp src/produit/hotte_map.cpp src/produit/pilote_simule.cpp tools/tests/test_sequences.cpp -o "$OUT/test_sequences"
"$OUT/test_sequences"
$CXXP src/produit/hotte_etat.cpp src/produit/hotte_map.cpp tools/tests/test_hotte_map.cpp -o "$OUT/test_hotte_map"
"$OUT/test_hotte_map"
$CXXP src/produit/surveillance.cpp tools/tests/test_surveillance.cpp -o "$OUT/test_surveillance"
"$OUT/test_surveillance"
echo "tests hote : OK"
