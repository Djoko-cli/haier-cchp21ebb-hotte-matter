#!/usr/bin/env python3
"""Codes d'appairage du module (spec 8.2) : tires au hasard, gardes hors du depot.

  python3 tools/codes_appairage.py

Ecrit src/produit/codes_appairage.h (ignore par git, droits 600) : un discriminateur de 12 bits et un code
d'appairage, tires par le module secrets, valides pour la pile et distincts de ceux de la ScreenBar
(0xF00, 20202021). Ajoute le code a garde.local.txt, pour que la garde refuse tout push qui le contiendrait.
Refuse d'ecraser un fichier existant : les codes ne changent JAMAIS apres la premiere mise en service.
N'affiche pas les valeurs : la commande `matter` de la carte les donne au moment de la mise en service.
"""
import os
import secrets
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
DEPOT = os.path.normpath(os.path.join(ICI, ".."))
EN_TETE = os.path.join("src", "produit", "codes_appairage.h")

# Regle de la pile (chip::PayloadContents::IsValidSetupPIN, esp_matter 1.5.1), comme config_produit.h.
CODES_INTERDITS = {11111111 * k for k in range(1, 9)} | {12345678, 87654321}
CODE_SCREENBAR, DISCRIMINATEUR_SCREENBAR = 20202021, 0xF00


def code_valide(c):
    return 1 <= c <= 99999998 and c not in CODES_INTERDITS and c != CODE_SCREENBAR


def tirer(hasard=secrets.SystemRandom()):
    """(discriminateur, code) au hasard, valides."""
    while True:
        d = hasard.randrange(0, 0x1000)
        if d != DISCRIMINATEUR_SCREENBAR:
            break
    while True:
        c = hasard.randrange(1, 99999999)
        if code_valide(c):
            return d, c


def texte_en_tete(d, c):
    return ("// Codes d'appairage du module (spec 8.2), tires par tools/codes_appairage.py.\n"
            "// Fichier LOCAL, ignore par git : jamais dans le depot. JAMAIS changes apres la\n"
            "// premiere mise en service.\n"
            "#pragma once\n"
            "#include <cstdint>\n"
            f"constexpr uint16_t kDiscriminateur = 0x{d:03X};\n"
            f"constexpr uint32_t kCodeAppairage = {c};\n")


def main(depot=DEPOT):
    chemin = os.path.join(depot, EN_TETE)
    if os.path.exists(chemin):
        print(f"codes_appairage : {EN_TETE} existe deja : jamais de nouveaux codes apres la mise en service")
        return 1
    d, c = tirer()
    ancien = os.umask(0o077)
    try:
        with open(chemin, "x", encoding="utf-8") as f:
            f.write(texte_en_tete(d, c))
        with open(os.path.join(depot, "garde.local.txt"), "a", encoding="utf-8") as f:
            f.write(f"# code d'appairage du module ({EN_TETE})\n{c}\n")
    finally:
        os.umask(ancien)
    print(f"codes_appairage : {EN_TETE} ecrit (local, ignore par git) ; code ajoute a garde.local.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())
