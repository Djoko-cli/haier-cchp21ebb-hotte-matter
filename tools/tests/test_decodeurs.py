#!/usr/bin/env python3
"""Tests de tools/decodeurs.py : python3 -m unittest discover -s tools/tests -p test_decodeurs.py -v"""
import os
import random
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import decodeurs  # noqa: E402
import signaux  # noqa: E402

H, B = True, False


def isoler(segs, repos_haut):
    """Une trame telle que decouper la rend : sans repos avant ni apres."""
    trames = decodeurs.decouper(segs, 10**9, repos_haut)
    assert len(trames) == 1
    return trames[0]


def bruiter(segs, amplitude, graine=1):
    r = random.Random(graine)
    return [(n, d + r.randint(-amplitude, amplitude)) for n, d in segs]


class Statistiques(unittest.TestCase):
    def test_histogramme(self):
        self.assertEqual(decodeurs.histogramme([5, 12, 14, 25, 25]), [(0, 1), (10, 2), (20, 2)])
        self.assertEqual(decodeurs.histogramme([104, 208], pas_us=100), [(100, 1), (200, 1)])
        self.assertEqual(decodeurs.histogramme([]), [])

    def test_regroupements(self):
        self.assertEqual(decodeurs.regroupements([1502, 750, 752, 748, 1500, 2250]),
                         [(750, 3), (1501, 2), (2250, 1)])
        self.assertEqual(decodeurs.regroupements([]), [])

    def test_regroupements_largeur_bornee(self):
        # 100 et 116 : 116 > 100 * 1.15, deux groupes meme si les ecarts sont petits
        self.assertEqual(decodeurs.regroupements([100, 108, 116]), [(104, 2), (116, 1)])


class Decouper(unittest.TestCase):
    def test_decoupe_au_silence(self):
        segs = [(H, 9000), (B, 100), (H, 50), (B, 100), (H, 5000), (B, 200), (H, 4999), (B, 300), (H, 20)]
        self.assertEqual(decodeurs.decouper(segs, 5000, True),
                         [[(B, 100), (H, 50), (B, 100)], [(B, 200), (H, 4999), (B, 300)]])

    def test_decoupe_datee(self):
        segs = [(H, 9000), (B, 100), (H, 5000), (B, 200)]
        self.assertEqual(decodeurs.decouper_dates(segs, 5000, True),
                         [(9000, [(B, 100)]), (14100, [(B, 200)])])

    def test_repos_bas_et_fusion(self):
        segs = [(H, 10), (H, 20), (B, 7000), (H, 30)]
        self.assertEqual(decodeurs.decouper(segs, 5000, False), [[(H, 30)], [(H, 30)]])


class Uart(unittest.TestCase):
    OCTETS = bytes([0xA5, 0x5A, 0x00, 0xFF, 0x07, 0x05])

    def test_9600_repos_haut(self):
        d = decodeurs.uart(isoler(signaux.uart(self.OCTETS, 9600), True), 9600)
        self.assertEqual((d.nom, d.params), ("uart", {"bauds": 9600, "inverse": False}))
        self.assertEqual(d.octets, self.OCTETS)
        self.assertEqual(d.bits, "".join(format(o, "08b") for o in self.OCTETS))
        self.assertEqual((d.erreurs, d.symboles), (0, 6))
        self.assertEqual(d.taux, 0.0)

    def test_500_inverse_avec_pauses(self):
        segs = signaux.uart(self.OCTETS, 500, repos_haut=False, pause_octet_us=4000)
        d = decodeurs.uart(isoler(segs, False), 500, inverse=True)
        self.assertEqual((d.octets, d.erreurs, d.symboles), (self.OCTETS, 0, 6))

    def test_tolere_la_gigue(self):
        segs = bruiter(signaux.uart(self.OCTETS * 3, 2400), 8)
        d = decodeurs.uart(isoler(segs, True), 2400)
        self.assertEqual((d.octets, d.erreurs), (self.OCTETS * 3, 0))

    def test_bit_de_stop_faux(self):
        # un octet 0x00 dont le stop reste bas (8E1 ou debit faux) : erreur de trame
        d = decodeurs.uart([(B, 10 * 2000)], 500)
        self.assertEqual((d.octets, d.erreurs, d.symboles), (b"\x00", 1, 1))

    def test_front_hors_grille(self):
        # 9600 lu a 4800 : les fronts tombent au milieu des bits
        d = decodeurs.uart(isoler(signaux.uart(self.OCTETS, 9600), True), 4800)
        self.assertGreater(d.erreurs, 0)

    def test_mauvaise_polarite(self):
        segs = signaux.uart(self.OCTETS, 500, repos_haut=False)
        self.assertGreater(decodeurs.uart(isoler(segs, False), 500, inverse=False).erreurs, 0)

    def test_faux_depart(self):
        # impulsion basse de 10 us a 500 bauds : depart tente, erreur, aucun octet
        d = decodeurs.uart([(B, 10)], 500)
        self.assertEqual((d.octets, d.erreurs, d.symboles), (b"", 1, 1))

    def test_trame_vide(self):
        d = decodeurs.uart([], 500)
        self.assertEqual((d.octets, d.bits, d.erreurs, d.symboles, d.taux), (b"", "", 0, 0, 0.0))


if __name__ == "__main__":
    unittest.main()
