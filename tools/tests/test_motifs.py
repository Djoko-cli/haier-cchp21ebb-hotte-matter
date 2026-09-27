#!/usr/bin/env python3
"""Croisement src/motifs.cpp / tools/signaux.py : compile tools/tests/dump_motifs.cpp
avec clang++, puis compare ses segments et ses octets a signaux.trame_motif et
signaux.octets_motif, motif par motif.
python3 -m unittest discover -s tools/tests -p test_motifs.py -v"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.normpath(os.path.join(ICI, "..", ".."))
sys.path.insert(0, os.path.join(ICI, ".."))
import signaux  # noqa: E402

INDICES = list(range(10)) + [255, 256, 999]  # dump_motifs.cpp : kIndices


def compiler_et_lancer() -> dict:
    """Compile dump_motifs.cpp avec src/motifs.cpp (drapeaux de test_hote.sh) et rend son JSON."""
    cxx = shutil.which("clang++")
    if cxx is None:
        raise AssertionError("clang++ introuvable (Xcode Command Line Tools)")
    with tempfile.TemporaryDirectory() as dossier:
        exe = os.path.join(dossier, "dump_motifs")
        r = subprocess.run([cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I" + os.path.join(RACINE, "src"),
                            os.path.join(RACINE, "src", "motifs.cpp"), os.path.join(ICI, "dump_motifs.cpp"),
                            "-o", exe], capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("compilation de dump_motifs.cpp :\n" + r.stderr)
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode:
            raise AssertionError(f"dump_motifs a echoue ({r.returncode}) :\n{r.stderr}")
    return json.loads(r.stdout)


def segs(liste) -> list[tuple[bool, int]]:
    return [(bool(h), int(d)) for h, d in liste]


class Motifs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dump = compiler_et_lancer()
        cls.par_nom = {m["nom"]: m for m in cls.dump["motifs"]}

    def test_noms_dans_l_ordre_de_signaux(self):
        self.assertEqual([m["nom"] for m in self.dump["motifs"]], list(signaux.MOTIFS))
        for m in self.dump["motifs"]:
            self.assertTrue(m["retrouve"], m["nom"])  # depuisNom(nom(id)) == id
        self.assertFalse(self.dump["inconnu"])        # depuisNom("inconnu")

    def test_repos_et_pause(self):
        for nom in signaux.MOTIFS:
            m = self.par_nom[nom]
            self.assertEqual(m["repos_haut"], signaux.repos_haut_motif(nom), nom)
            self.assertEqual(m["pause_us"], signaux.pause_motif_us(nom), nom)

    def test_octets(self):
        for nom in signaux.MOTIFS:
            trames = self.par_nom[nom]["trames"]
            self.assertEqual([t["index"] for t in trames], INDICES)
            for t in trames:
                self.assertEqual(bytes(t["octets"]), signaux.octets_motif(nom, t["index"]), (nom, t["index"]))

    def test_segments(self):
        for nom in signaux.MOTIFS:
            if nom == "rafale":
                continue
            for t in self.par_nom[nom]["trames"]:
                attendu = signaux.trame_motif(nom, t["index"])
                self.assertEqual(segs(t["segments"]), attendu, (nom, t["index"]))
                self.assertEqual(t["n"], len(attendu), (nom, t["index"]))
                self.assertEqual(t["somme_us"], sum(d for _, d in attendu), (nom, t["index"]))

    def test_rafale_longueur_et_debut(self):
        attendu = signaux.trame_motif("rafale", 0)
        for t in self.par_nom["rafale"]["trames"]:  # index ignore
            self.assertEqual(t["n"], len(attendu))
            self.assertEqual(t["n"], self.dump["rafale_segs"])
            self.assertEqual(segs(t["segments"]), attendu[:20])
            self.assertEqual(t["somme_us"], sum(d for _, d in attendu))
            self.assertEqual(t["octets"], [])

    def test_tampon_du_generateur_suffit(self):
        # gen_main.cpp alloue kSegMax segments par trame (rafale exceptee) ;
        # max_segments est le maximum sur les index 0..1023 (tous les restes mod 256).
        for nom in signaux.MOTIFS:
            if nom == "rafale":
                continue
            m = self.par_nom[nom]
            self.assertLessEqual(m["max_segments"], self.dump["seg_max"], nom)
            pire = max(len(signaux.trame_motif(nom, i)) for i in range(1024))
            self.assertEqual(m["max_segments"], pire, nom)

    def test_capacite_trop_petite(self):
        self.assertEqual(self.dump["cap_trop_petite"], {"uart500": 0, "rafale": 0, "octets": 0})


if __name__ == "__main__":
    unittest.main()
