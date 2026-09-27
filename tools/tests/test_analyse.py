#!/usr/bin/env python3
"""Tests de bout en bout de tools/analyse.py sur des captures synthetiques
(signaux.simuler_sonde) : python3 -m unittest discover -s tools/tests -p test_analyse.py -v"""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import analyse  # noqa: E402
import decodeurs  # noqa: E402
import signaux  # noqa: E402


def objets_sonde(motif, indices, silence_us, t0_us=1000000):
    rh = signaux.repos_haut_motif(motif)
    segs = []
    for i in indices:
        segs += signaux.trame_motif(motif, i) + [(rh, signaux.pause_motif_us(motif))]
    return signaux.simuler_sonde(segs, t0_us, silence_us, rh)


class Capture(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def jsonl(self, nom, motif, indices, silence_us):
        """Capture au format .jsonl, precedee d'un hello comme une vraie session."""
        objs = [{"v": 1, "t": "hello", "n": 0, "ms": 0, "bloc": "base"}] + objets_sonde(motif, indices, silence_us)
        p = os.path.join(self.dossier.name, nom + ".jsonl")
        with open(p, "w") as f:
            for k, o in enumerate(objs):
                f.write(json.dumps({"rx_ms": 1790000000000 + k, "de": "192.168.1.30", "l": o}) + "\n")
        return p

    def brute(self, nom, motif, indices, silence_us):
        """Capture serie brute : texte humain, puis lignes RS + JSON compact + LF."""
        p = os.path.join(self.dossier.name, nom + ".log")
        with open(p, "wb") as f:
            f.write(b"firmware 0.1.0+abc1234\r\n")
            for o in objets_sonde(motif, indices, silence_us):
                f.write(b"\x1e" + json.dumps(o, separators=(",", ":")).encode() + b"\n")
        return p

    def lancer(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = analyse.main(list(argv))
        return code, out.getvalue()


class Auto(Capture):
    def verifier(self, chemin, motif, indices, nom, params):
        trames, _, _ = analyse.charger(chemin)
        tete = decodeurs.auto([t for _, t in trames])[0]
        self.assertEqual((tete.nom, tete.params, tete.erreurs), (nom, params, 0))
        decs = decodeurs.appliquer(tete, [t for _, t in trames])
        self.assertEqual([d.octets for d in decs], [signaux.octets_motif(motif, i) for i in indices])

    def test_krona_uart_500_inverse(self):
        p = self.jsonl("krona", "krona", range(2, 8), 19000)
        self.verifier(p, "krona", range(2, 8), "uart", {"bauds": 500, "inverse": True})
        code, sortie = self.lancer("auto", p)
        self.assertEqual(code, 0)
        self.assertIn("6 trames, repos bas, une trame par reception", sortie)
        self.assertIn(" 1. uart 500 bauds inverse : 0 erreurs / 30 symboles (0.0 %)", sortie)
        self.assertIn("#0 t=1000000 us : 55 02 fd 00 54", sortie)

    def test_krona_trames_coupees_entre_deux_octets(self):
        # index 0 et 1 : 0xFF ou 0xFE + pause >= 19 ms, la sonde coupe la trame ; aucun octet perdu
        p = self.jsonl("krona", "krona", range(10), 19000)
        trames, _, _ = analyse.charger(p)
        self.assertEqual(len(trames), 14)
        tete = decodeurs.auto([t for _, t in trames])[0]
        self.assertEqual((tete.nom, tete.params, tete.erreurs), ("uart", {"bauds": 500, "inverse": True}, 0))
        self.assertEqual(tete.octets, b"".join(signaux.octets_motif("krona", i) for i in range(10)))

    def test_uart_9600(self):
        p = self.jsonl("u9600", "uart9600", range(10), 5000)
        self.verifier(p, "uart9600", range(10), "uart", {"bauds": 9600, "inverse": False})
        code, sortie = self.lancer("auto", p, "--max", "3")
        self.assertIn(" 1. uart 9600 bauds : 0 erreurs / 60 symboles (0.0 %)", sortie)
        self.assertIn("#2 t=", sortie)
        self.assertIn("a5 5a 00 ff 02 00", sortie)
        self.assertIn("... 7 trames de plus (--max 0 : toutes)", sortie)

    def test_uart_9600_capture_brute(self):
        p = self.brute("u9600", "uart9600", range(10), 5000)
        self.verifier(p, "uart9600", range(10), "uart", {"bauds": 9600, "inverse": False})

    def test_wtc(self):
        p = self.jsonl("wtc", "wtc", range(10), 5000)
        self.verifier(p, "wtc", range(10), "distance_impulsion", {"t_us": 750})
        code, sortie = self.lancer("auto", p)
        self.assertIn(" 1. distance_impulsion T=750 us : 0 erreurs / 240 symboles (0.0 %)", sortie)
        self.assertIn(": 0000000100000000 00000000 = 01 00 00", sortie)

    def test_uart_impose(self):
        p = self.jsonl("krona", "krona", range(2, 8), 19000)
        code, sortie = self.lancer("uart", p, "500", "--inverse")
        self.assertEqual(code, 0)
        self.assertIn("uart 500 bauds inverse : 0 erreurs / 30 symboles (0.0 %)", sortie)
        code, sortie = self.lancer("uart", p, "500")
        self.assertNotIn("uart 500 bauds : 0 erreurs", sortie)


class Trames(Capture):
    def test_redecoupage_au_silence(self):
        # sonde a 19 ms : le motif wtc (6,75 ms entre trames) arrive en une seule reception
        p = self.jsonl("wtc19", "wtc", range(10), 19000)
        trames, _, recs = analyse.charger(p)
        self.assertEqual((len(recs), len(trames)), (1, 1))
        code, sortie = self.lancer("trames", p, "--silence", "5000")
        self.assertEqual(code, 0)
        self.assertIn("10 trames (repos haut, silence 5000 us)", sortie)
        self.assertIn("#1 t=1051000 us, 51 paliers, 45750 us, 6750 us apres la precedente", sortie)
        self.assertIn("  b1500 h750 b750 h750", sortie)
        code, sortie = self.lancer("auto", p, "--silence", "5000")
        self.assertIn(" 1. distance_impulsion T=750 us : 0 erreurs / 240 symboles", sortie)


class Diff(Capture):
    def test_octets_qui_changent(self):
        a = self.jsonl("a", "krona", [2, 2, 2], 19000)
        b = self.jsonl("b", "krona", [3, 3, 3, 3], 19000)
        code, sortie = self.lancer("diff", a, b)
        self.assertEqual(code, 0)
        self.assertIn("hypothese (meilleure pour a) : uart 500 bauds inverse", sortie)
        self.assertIn("      3 x 55 02 fd 00 54", sortie)
        self.assertIn("      4 x 55 03 fc 00 54", sortie)
        self.assertIn("octets qui changent : 1 (02 -> 03 : bit 0), 2 (fd -> fc : bit 0)", sortie)

    def test_bits_qui_changent(self):
        a = self.jsonl("a", "wtc", [0, 0, 0], 5000)
        b = self.jsonl("b", "wtc", [1, 1], 5000)
        code, sortie = self.lancer("diff", a, b)
        self.assertIn("bits qui changent : message 0 bit 6 (0 -> 1), message 0 bit 7 (1 -> 0), "
                      "message 1 bit 7 (0 -> 1)", sortie)

    def test_aucune_difference(self):
        a = self.jsonl("a", "uart2400", [5, 5], 5000)
        code, sortie = self.lancer("diff", a, a)
        self.assertIn("aucune difference", sortie)

    def test_positions(self):
        self.assertEqual(analyse.positions_octets(b"\x01\x02", b"\x01\x03\x04"), [(1, 2, 3), (2, None, 4)])
        self.assertEqual(analyse.positions_bits("0110 01", "0100"), [(0, 2, "1", "0"), (1, 0, "0", None),
                                                                       (1, 1, "1", None)])


class Histo(Capture):
    def test_regroupements(self):
        p = self.jsonl("u500", "uart500", range(4), 19000)
        _, _, recs = analyse.charger(p)
        hautes, basses, silences = analyse.niveaux(recs)
        self.assertEqual(decodeurs.regroupements(hautes)[0][0], 2000)
        self.assertEqual(len(silences), 3)
        code, sortie = self.lancer("histo", p, "--pas", "1000")
        self.assertEqual(code, 0)
        self.assertIn("4 receptions, repos haut", sortie)
        self.assertIn("durees hautes : ", sortie)
        self.assertIn("     2000 us : ", sortie)
        self.assertIn("silences entre receptions : 3", sortie)


class Repos(Capture):
    def test_deduit_ou_impose(self):
        p = self.jsonl("krona", "krona", range(2, 5), 19000)
        self.assertFalse(analyse.charger(p)[1])
        self.assertTrue(analyse.charger(p, repos="haut")[1])
        code, sortie = self.lancer("auto", p, "--repos", "haut")
        self.assertIn("repos haut", sortie)

    def test_capture_sans_trame(self):
        p = os.path.join(self.dossier.name, "vide.jsonl")
        with open(p, "w") as f:
            f.write(json.dumps({"rx_ms": 1, "de": "usb", "l": {"v": 1, "t": "hello", "n": 0, "ms": 0}}) + "\n")
        code, sortie = self.lancer("auto", p)
        self.assertEqual(code, 1)
        self.assertIn("aucune trame", sortie)

    def test_fichier_absent(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code, _ = self.lancer("auto", os.path.join(self.dossier.name, "absent.jsonl"))
        self.assertEqual(code, 2)
        self.assertIn("analyse.py :", err.getvalue())


if __name__ == "__main__":
    unittest.main()
