#!/usr/bin/env python3
"""Tests de tools/capture_fmt.py : python3 -m unittest discover -s tools/tests -p test_capture_fmt.py -v"""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import capture_fmt  # noqa: E402
import signaux  # noqa: E402

H, B = True, False


def trame(num, part, fin, t_us, niv0, dur, **extra):
    o = {"v": 1, "t": "trame", "n": 1, "ms": 0, "num": num, "part": part, "fin": fin,
         "t_us": t_us, "niv0": niv0, "dur_us": dur, "debord": False}
    o.update(extra)
    return o


class Lecture(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def chemin(self, nom, contenu: bytes):
        p = os.path.join(self.dossier.name, nom)
        with open(p, "wb") as f:
            f.write(contenu)
        return p

    def test_jsonl(self):
        a = {"v": 1, "t": "hello", "n": 1, "ms": 5, "bloc": "base"}
        b = trame(1, 0, True, 10, "bas", [100])
        lignes = [json.dumps({"rx_ms": 1, "de": "usb", "l": a}), "",
                  json.dumps({"rx_ms": 2, "de": "192.168.1.20", "l": b}),
                  '{"rx_ms": 3, "de": "usb", "l": {"v": 1, "t": "tr']  # derniere ligne tronquee
        p = self.chemin("x.jsonl", "\n".join(lignes).encode())
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            objets = list(capture_fmt.lire_jsonl(p))
        self.assertEqual(objets, [a, b])
        self.assertIn("ligne 4 illisible", err.getvalue())

    def test_serie_brute(self):
        a = trame(1, 0, True, 10, "bas", [100])
        b = {"v": 1, "t": "log", "n": 2, "ms": 6, "src": "capture", "niv": "info", "txt": "ok"}
        brut = (b"firmware 0.1.0\r\n"
                + b"\x1e" + json.dumps(a, separators=(",", ":")).encode() + b"\n"
                + b"texte colle\x1e" + json.dumps(b, separators=(",", ":")).encode() + b"\r\n"
                + b"\x1e{mauvais\n"
                + b"sans fin de ligne")
        p = self.chemin("capture.log", brut)
        self.assertEqual(list(capture_fmt.lire_serie_brute(p)), [a, b])
        self.assertEqual(list(capture_fmt.lire(p)), [a, b])

    def test_lire_choisit_selon_l_extension(self):
        a = trame(1, 0, True, 10, "bas", [100])
        p = self.chemin("y.jsonl", json.dumps({"rx_ms": 1, "de": "usb", "l": a}).encode() + b"\n")
        self.assertEqual(list(capture_fmt.lire(p)), [a])


class Parties(unittest.TestCase):
    def test_conversion_et_filtre(self):
        objs = [{"v": 1, "t": "etat", "n": 1, "ms": 0, "bloc": "bus"},
                trame(3, 0, False, 1000, "haut", [10, 20], debord=True),
                trame(3, 1, True, 1030, "bas", [30], rep=4)]
        ps = capture_fmt.parties(objs)
        self.assertEqual(ps, [
            capture_fmt.Partie(num=3, part=0, fin=False, t_us=1000, niv0_haut=True, dur_us=(10, 20), debord=True, rep=None),
            capture_fmt.Partie(num=3, part=1, fin=True, t_us=1030, niv0_haut=False, dur_us=(30,), debord=False, rep=4)])


class Receptions(unittest.TestCase):
    def test_recolle_par_num(self):
        ps = capture_fmt.parties([
            trame(1, 0, False, 0, "bas", [10, 20]),
            trame(1, 1, False, 30, "bas", [30, 40]),
            trame(1, 2, True, 100, "bas", [50]),
            trame(2, 0, True, 5000, "haut", [7], debord=True)])
        self.assertEqual(capture_fmt.receptions(ps), [
            capture_fmt.Reception(num=1, t_us=0, niv0_haut=False, dur_us=(10, 20, 30, 40, 50), debord=False, complete=True),
            capture_fmt.Reception(num=2, t_us=5000, niv0_haut=True, dur_us=(7,), debord=True, complete=True)])

    def test_receptions_incompletes(self):
        ps = capture_fmt.parties([
            trame(1, 0, False, 0, "bas", [10]),
            trame(1, 2, True, 100, "bas", [50]),      # part 1 manquante
            trame(2, 1, True, 5000, "haut", [7]),     # part 0 manquante
            trame(3, 0, False, 9000, "bas", [9])])    # pas de fin
        recs = capture_fmt.receptions(ps)
        self.assertEqual([(r.num, r.dur_us, r.complete) for r in recs],
                         [(1, (10,), False), (1, (50,), False), (2, (7,), False), (3, (9,), False)])

    def test_redemarrage_de_la_sonde(self):
        ps = capture_fmt.parties([trame(5, 0, True, 900000, "bas", [10]),
                                  trame(1, 0, True, 1000, "bas", [20])])
        self.assertEqual([(r.num, r.t_us) for r in capture_fmt.receptions(ps)], [(5, 900000), (1, 1000)])


class Flux(unittest.TestCase):
    def test_silences_calcules_par_t_us(self):
        recs = [capture_fmt.Reception(1, 1000, False, (100, 50, 100), False, True),
                capture_fmt.Reception(2, 6250, False, (200,), False, True)]
        self.assertEqual(capture_fmt.flux(recs, repos_haut=True),
                         [(B, 100), (H, 50), (B, 100), (H, 5000), (B, 200)])

    def test_ecart_nul_ou_negatif_sans_silence(self):
        recs = [capture_fmt.Reception(1, 0, False, (100,), False, True),
                capture_fmt.Reception(2, 90, True, (30,), False, True)]
        self.assertEqual(capture_fmt.flux(recs, repos_haut=True), [(B, 100), (H, 30)])

    def test_aller_retour_avec_simuler_sonde(self):
        segs = []
        for i in range(3):
            segs += signaux.trame_motif("wtc", i) + [(H, signaux.pause_motif_us("wtc"))]
        objs = signaux.simuler_sonde(segs, 12345, 5000, True)
        recs = capture_fmt.receptions(capture_fmt.parties(objs))
        self.assertEqual(len(recs), 3)
        self.assertTrue(all(r.complete for r in recs))
        self.assertEqual(capture_fmt.flux(recs, True), signaux.fusionner(segs)[:-1])

    def test_repos_majoritaire(self):
        recs = [capture_fmt.Reception(1, 0, False, (100, 50, 100), False, True),   # fin sur bas : repos haut
                capture_fmt.Reception(2, 0, True, (10, 20), False, True),         # fin sur bas : repos haut
                capture_fmt.Reception(3, 0, True, (10,), False, True)]            # fin sur haut : repos bas
        self.assertTrue(capture_fmt.repos_majoritaire(recs))
        self.assertFalse(capture_fmt.repos_majoritaire(recs[2:]))
        self.assertTrue(capture_fmt.repos_majoritaire([]))


if __name__ == "__main__":
    unittest.main()
