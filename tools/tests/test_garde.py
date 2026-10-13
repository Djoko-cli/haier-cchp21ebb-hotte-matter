#!/usr/bin/env python3
"""Tests de tools/garde.py : python3 -m unittest discover -s tools/tests -p test_garde.py -v"""
import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import garde  # noqa: E402


class Garde(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.depot = self.tmp.name
        subprocess.run(["git", "init", "-q", self.depot], check=True)

    def tearDown(self):
        self.tmp.cleanup()

    def ecrire(self, nom, texte, suivre=True):
        with open(os.path.join(self.depot, nom), "w", encoding="utf-8") as f:
            f.write(texte)
        if suivre:
            subprocess.run(["git", "-C", self.depot, "add", nom], check=True)

    def lancer(self, *arguments):
        sortie = io.StringIO()
        with contextlib.redirect_stdout(sortie):
            code = garde.main(list(arguments), depot=self.depot)
        return code, sortie.getvalue()

    def test_sans_liste_refuse(self):
        self.ecrire("a.md", "rien\n")
        code, sortie = self.lancer()
        self.assertEqual(code, 1)
        self.assertIn("garde.local.txt absent", sortie)

    def test_fichier_propre_passe(self):
        self.ecrire("garde.local.txt", "# MAC de test\n02:00:00:00:00:01\n\n", suivre=False)
        self.ecrire("a.md", "MAC-SCREENBAR, fd00::1\n")
        self.assertEqual(self.lancer()[0], 0)

    def test_identifiant_trouve_malgre_casse_et_separateurs(self):
        self.ecrire("garde.local.txt", "02:00:00:00:AB:01\nPersonne-Test\n", suivre=False)
        for texte in ("mac 02-00-00-00-ab-01\n", "0x02 00 00 00 AB 01\n", "par personne.test\n"):
            with self.subTest(texte=texte):
                self.ecrire("a.md", texte)
                code, sortie = self.lancer()
                self.assertEqual(code, 1)
                self.assertIn("a.md", sortie)

    def test_fichier_non_suivi_ignore(self):
        self.ecrire("garde.local.txt", "11223344\n", suivre=False)
        self.ecrire("brouillon.txt", "code 11223344\n", suivre=False)
        self.assertEqual(self.lancer()[0], 0)

    def test_fichier_donne_verifie(self):
        """Le crochet pre-push passe les messages des commits pousses."""
        self.ecrire("garde.local.txt", "11223344\n", suivre=False)
        message = os.path.join(self.depot, "message.txt")
        with open(message, "w", encoding="utf-8") as f:
            f.write("Poser le code 11223344\n")
        code, sortie = self.lancer(message)
        self.assertEqual(code, 1)
        self.assertIn("message.txt", sortie)

    def test_hexa_long_en_tableau_c(self):
        self.ecrire("garde.local.txt", "0000000000000001\n", suivre=False)
        self.ecrire("cle.h", "{0x00, 0x00, 0x00, 0x00,\n 0x00, 0x00, 0x00, 0x01}\n")
        self.assertEqual(self.lancer()[0], 1)


if __name__ == "__main__":
    unittest.main()
