#!/usr/bin/env python3
"""Liens de la documentation : python3 -m unittest discover -s tools/tests -p test_liens.py -v

Chaque lien relatif de README.md et de docs/*.md mene a un fichier present et,
s'il porte une ancre, a un titre de ce fichier (ancre calculee comme GitHub :
minuscules, ponctuation retiree, espaces en tirets). README.md et la spec
renvoient aux documents de terrain, du banc et du protocole ; le README cite
aussi captures/ et tools/verse_capture.py.
"""
import os
import re
import sys
import unittest

sys.dont_write_bytecode = True

RACINE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def ancre(titre):
    return re.sub(r"[^\w\- ]", "", titre.strip().lower()).replace(" ", "-")


def ancres(chemin):
    with open(chemin, encoding="utf-8") as f:
        return {ancre(l.lstrip("#")) for l in f if l.startswith("#")}


def documents():
    docs = os.path.join(RACINE, "docs")
    return ["README.md"] + ["docs/" + n for n in sorted(os.listdir(docs)) if n.endswith(".md")]


def lire(relatif):
    with open(os.path.join(RACINE, relatif), encoding="utf-8") as f:
        return f.read()


class Liens(unittest.TestCase):
    def test_liens_relatifs(self):
        erreurs = []
        for doc in documents():
            chemin = os.path.join(RACINE, doc)
            for cible in re.findall(r"\]\(([^)\s]+)\)", lire(doc)):
                if cible.startswith(("http://", "https://")):
                    continue
                fichier, _, a = cible.partition("#")
                vise = os.path.normpath(os.path.join(os.path.dirname(chemin), fichier)) if fichier else chemin
                if not os.path.exists(vise):
                    erreurs.append(f"{doc} : {cible} : fichier absent")
                elif a and vise.endswith(".md") and a not in ancres(vise):
                    erreurs.append(f"{doc} : {cible} : ancre absente")
        self.assertEqual(erreurs, [])

    def test_renvois(self):
        readme = lire("README.md")
        for n in ("SPEC-RECONNAISSANCE.md", "SECURITE.md", "RECONNAISSANCE.md", "WIRING.md", "BANC.md",
                  "PROTOCOLE-JSON.md", "BRIEF-RECHERCHE.md"):
            self.assertIn(f"](docs/{n}", readme, n)
        self.assertIn("](captures/README.md)", readme)  # captures de reference (tache 12b)
        self.assertIn("tools/verse_capture.py", readme)
        spec = lire("docs/SPEC-RECONNAISSANCE.md")
        for n in ("SECURITE.md", "RECONNAISSANCE.md", "WIRING.md", "BANC.md", "PROTOCOLE-JSON.md"):
            self.assertIn(f"]({n}", spec, n)


if __name__ == "__main__":
    unittest.main()
