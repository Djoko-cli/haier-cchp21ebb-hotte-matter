#!/usr/bin/env python3
"""Tests de tools/codes_appairage.py : python3 -m unittest discover -s tools/tests -p test_codes_appairage.py -v"""
import contextlib
import io
import os
import random
import re
import stat
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import codes_appairage  # noqa: E402


class Codes(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        os.makedirs(os.path.join(self.tmp.name, "src", "produit"))

    def tearDown(self):
        self.tmp.cleanup()

    def lancer(self):
        with contextlib.redirect_stdout(io.StringIO()) as sortie:
            code = codes_appairage.main(depot=self.tmp.name)
        return code, sortie.getvalue()

    def test_codes_interdits(self):
        for c in (0, 11111111, 88888888, 12345678, 87654321, 20202021, 99999999):
            self.assertFalse(codes_appairage.code_valide(c), c)
        for c in (1, 11223344, 99999998, 99999999 - 2):
            self.assertTrue(codes_appairage.code_valide(c), c)

    def test_tirages_valides(self):
        hasard = random.Random(1)
        for _ in range(2000):
            d, c = codes_appairage.tirer(hasard)
            self.assertTrue(0 <= d <= 0xFFF and d != 0xF00)
            self.assertTrue(codes_appairage.code_valide(c))

    def test_ecrit_en_tete_et_garde_sans_afficher(self):
        code, sortie = self.lancer()
        self.assertEqual(code, 0)
        chemin = os.path.join(self.tmp.name, codes_appairage.EN_TETE)
        texte = open(chemin, encoding="utf-8").read()
        c = int(re.search(r"kCodeAppairage = (\d+);", texte).group(1))
        self.assertRegex(texte, r"kDiscriminateur = 0x[0-9A-F]{3};")
        self.assertEqual(stat.S_IMODE(os.stat(chemin).st_mode), 0o600)
        self.assertIn(f"\n{c}\n", open(os.path.join(self.tmp.name, "garde.local.txt")).read())
        self.assertNotIn(str(c), sortie)

    def test_refuse_d_ecraser(self):
        self.assertEqual(self.lancer()[0], 0)
        chemin = os.path.join(self.tmp.name, codes_appairage.EN_TETE)
        avant = open(chemin).read()
        code, sortie = self.lancer()
        self.assertEqual(code, 1)
        self.assertIn("existe deja", sortie)
        self.assertEqual(open(chemin).read(), avant)


if __name__ == "__main__":
    unittest.main()
