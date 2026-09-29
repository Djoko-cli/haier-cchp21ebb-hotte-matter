"""Plans de plaque (docs/fabrication/plan-*.json) : le verificateur ne doit
trouver aucune erreur, dans chaque configuration ; le rendu doit produire les
vues sans erreur.

  python3 -m unittest discover -s tools/tests -p 'test_plaque.py'
"""
import os
import subprocess
import sys
import tempfile
import unittest

RACINE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
VERIF = os.path.join(RACINE, "tools", "verifier_plaque.py")
RENDU = os.path.join(RACINE, "tools", "rendu_plaque.py")
PLANS = os.path.join(RACINE, "docs", "fabrication")


def verifier(plan, *options):
    r = subprocess.run([sys.executable, VERIF, os.path.join(PLANS, plan), *options],
                       capture_output=True, text=True)
    return r.returncode, r.stdout


class TestPlaque(unittest.TestCase):
    def test_adaptateur(self):
        code, sortie = verifier("plan-adaptateur.json")
        self.assertEqual(code, 0, sortie)

    def test_boitier_toutes_configurations(self):
        for options in ((), ("--etape7",), ("--analyseur",), ("--etape7", "--analyseur")):
            with self.subTest(options=options):
                code, sortie = verifier("plan-boitier.json", *options)
                self.assertEqual(code, 0, sortie)

    def test_rendu(self):
        with tempfile.TemporaryDirectory() as d:
            r = subprocess.run([sys.executable, RENDU, "--sortie", d], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            attendus = {"boitier-composants.svg", "boitier-soudures.svg", "adaptateur-composants.svg",
                        "adaptateur-soudures.svg", "supermini-brochage.svg", "banc-breadboard.svg"}
            self.assertEqual(attendus, set(os.listdir(d)))


if __name__ == "__main__":
    unittest.main()
