#!/usr/bin/env python3
"""Tests de tools/signaux.py : python3 -m unittest discover -s tools/tests -p test_signaux.py -v"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import signaux  # noqa: E402

H, B = True, False


class Fusionner(unittest.TestCase):
    def test_colle_les_niveaux_egaux_et_retire_les_durees_nulles(self):
        segs = [(H, 5), (H, 3), (B, 0), (B, 2), (H, 1)]
        self.assertEqual(signaux.fusionner(segs), [(H, 8), (B, 2), (H, 1)])

    def test_duree_negative_refusee(self):
        with self.assertRaises(ValueError):
            signaux.fusionner([(H, -1)])


class Uart(unittest.TestCase):
    def test_a5_500_bauds_repos_haut(self):
        # 0xA5 = 10100101, LSB d'abord : 1 0 1 0 0 1 0 1 ; depart bas, stop haut
        self.assertEqual(signaux.uart(b"\xa5", 500), [
            (B, 2000), (H, 2000), (B, 2000), (H, 2000), (B, 4000), (H, 2000), (B, 2000), (H, 4000)])

    def test_a5_500_bauds_repos_bas(self):
        self.assertEqual(signaux.uart(b"\xa5", 500, repos_haut=False), [
            (H, 2000), (B, 2000), (H, 2000), (B, 2000), (H, 4000), (B, 2000), (H, 2000), (B, 4000)])

    def test_9600_bauds_frontieres_arrondies(self):
        # frontieres a (k * 1000000 + 4800) // 9600 : 9 bits = 938 us, octet = 1042 us
        self.assertEqual(signaux.uart(b"\x00", 9600), [(B, 938), (H, 104)])
        self.assertEqual(sum(d for _, d in signaux.uart(b"\xa5\x5a\x00\xff", 9600)), 4 * 1042)

    def test_pause_entre_octets(self):
        segs = signaux.uart(b"\x55\x55", 500, pause_octet_us=4000)
        self.assertEqual(len(segs), 20)
        self.assertEqual(segs[9], (H, 2000 + 4000))  # stop + pause, fusionnes
        self.assertEqual(sum(d for _, d in segs), 2 * 20000 + 4000)

    def test_debit_invalide(self):
        with self.assertRaises(ValueError):
            signaux.uart(b"\x00", 0)


class AutresCodages(unittest.TestCase):
    def test_distance_impulsion(self):
        self.assertEqual(signaux.distance_impulsion("01"), [
            (B, 1500), (H, 750), (B, 750), (H, 750), (B, 2250), (H, 750)])

    def test_distance_impulsion_bits_invalides(self):
        with self.assertRaises(ValueError):
            signaux.distance_impulsion("012")

    def test_manchester_ieee(self):
        # 1 = bas puis haut, 0 = haut puis bas
        self.assertEqual(signaux.manchester("10", 100), [(B, 100), (H, 200), (B, 100)])


class Motifs(unittest.TestCase):
    def test_liste(self):
        self.assertEqual(signaux.MOTIFS, ("uart500", "uart500inv", "uart2400", "uart2400inv",
                                          "uart9600", "uart9600inv", "wtc", "krona", "rafale"))

    def test_octets_uart(self):
        self.assertEqual(signaux.octets_motif("uart500", 7), bytes([0xA5, 0x5A, 0x00, 0xFF, 0x07, 0x05]))
        self.assertEqual(signaux.octets_motif("uart9600inv", 300), bytes([0xA5, 0x5A, 0x00, 0xFF, 0x2C, 0x2A]))

    def test_octets_wtc_et_krona(self):
        self.assertEqual(signaux.octets_motif("wtc", 9), bytes([0x02, 0x00, 0x09]))
        self.assertEqual(signaux.octets_motif("wtc", 7), bytes([0x80, 0x00, 0x07]))
        self.assertEqual(signaux.octets_motif("krona", 10), bytes([0x55, 0x02, 0xFD, 0x00, 0x54]))
        self.assertEqual(signaux.octets_motif("rafale", 3), b"")

    def test_motif_inconnu_ou_index_negatif(self):
        with self.assertRaises(ValueError):
            signaux.octets_motif("uart1200", 0)
        with self.assertRaises(ValueError):
            signaux.trame_motif("wtc", -1)

    def test_trame_uart(self):
        self.assertEqual(signaux.trame_motif("uart2400inv", 4),
                         signaux.uart(signaux.octets_motif("uart2400inv", 4), 2400, repos_haut=False))

    def test_trame_wtc_avec_reponse(self):
        segs = signaux.trame_motif("wtc", 0)
        self.assertEqual(len(segs), 52)
        self.assertEqual(segs[0], (B, 1500))
        self.assertEqual(segs[33], (H, 750 + 4 * 750))  # 1T haut de fin + 4T avant la reponse
        self.assertEqual(segs[34], (B, 1500))           # depart de la reponse
        self.assertEqual(segs[-1], (H, 750))
        self.assertEqual(sum(d for _, d in segs), 60 * 750)

    def test_trame_krona(self):
        segs = signaux.trame_motif("krona", 0)
        self.assertEqual(segs[0], (H, 2000))  # depart inverse
        self.assertEqual(segs[-1][0], B)      # stop au repos bas
        self.assertEqual(sum(d for _, d in segs), 5 * 20000 + 4 * 4000)

    def test_trame_rafale(self):
        segs = signaux.trame_motif("rafale", 0)
        self.assertEqual(len(segs), 100000)
        self.assertEqual(segs[:2], [(H, 100), (B, 100)])
        self.assertEqual(segs[-1], (B, 100))

    def test_pauses_et_repos(self):
        self.assertEqual([signaux.pause_motif_us(m) for m in signaux.MOTIFS],
                         [20000] * 6 + [6000, 18000, 1000000])
        self.assertEqual([signaux.repos_haut_motif(m) for m in signaux.MOTIFS],
                         [True, False] * 3 + [True, False, True])


class SimulerSonde(unittest.TestCase):
    def test_une_reception(self):
        objs = signaux.simuler_sonde(signaux.uart(b"\xa5", 500), 1000, 5000, repos_haut=True)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(list(o), ["v", "t", "n", "ms", "num", "part", "fin", "t_us", "niv0", "dur_us", "debord"])
        self.assertEqual(o, {"v": 1, "t": "trame", "n": 1, "ms": 22, "num": 1, "part": 0, "fin": True,
                             "t_us": 1000, "niv0": "bas",
                             "dur_us": [2000, 2000, 2000, 2000, 4000, 2000, 2000], "debord": False})

    def test_repos_initial_decale_t_us(self):
        objs = signaux.simuler_sonde([(H, 300)] + signaux.uart(b"\xa5", 500), 1000, 5000, True)
        self.assertEqual(objs[0]["t_us"], 1300)

    def test_decoupe_en_parties_de_110(self):
        segs = [(k % 2 == 1, 100) for k in range(250)]  # le dernier (haut) se fond dans le repos
        objs = signaux.simuler_sonde(segs, 0, 5000, True)
        self.assertEqual([len(o["dur_us"]) for o in objs], [110, 110, 29])
        self.assertEqual([o["part"] for o in objs], [0, 1, 2])
        self.assertEqual([o["fin"] for o in objs], [False, False, True])
        self.assertEqual([o["t_us"] for o in objs], [0, 11000, 22000])
        self.assertEqual([o["n"] for o in objs], [1, 2, 3])
        self.assertEqual({o["num"] for o in objs}, {1})

    def test_niv0_alterne_selon_le_rang(self):
        segs = [(k % 2 == 1, 10) for k in range(7)]  # bas haut bas haut bas haut bas
        objs = signaux.simuler_sonde(segs, 0, 5000, True, dur_max=3)
        self.assertEqual([o["niv0"] for o in objs], ["bas", "haut", "bas"])
        self.assertEqual([o["dur_us"] for o in objs], [[10, 10, 10], [10, 10, 10], [10]])

    def test_receptions_separees_par_le_silence(self):
        segs = [(B, 100), (H, 5000), (B, 200), (H, 4999), (B, 300)]
        objs = signaux.simuler_sonde(segs, 0, 5000, True, num0=7)
        self.assertEqual([(o["num"], o["t_us"], o["dur_us"]) for o in objs],
                         [(7, 0, [100]), (8, 5100, [200, 4999, 300])])

    def test_palier_hors_repos_trop_long(self):
        # la sonde couperait la reception au milieu du palier (seuil RMT) : cas non simule
        with self.assertRaises(ValueError):
            signaux.simuler_sonde([(B, 18000)], 0, 5000, True)

    def krona(self, indices, silence_us):
        segs = []
        for i in indices:
            segs += signaux.trame_motif("krona", i) + [(B, signaux.pause_motif_us("krona"))]
        return signaux.simuler_sonde(segs, 0, silence_us, repos_haut=False)

    def test_krona_avec_silence_long(self):
        # 0x00 inverse = 18 ms hors repos : il faut silence_us > 18000
        objs = self.krona([2, 3, 4], 19000)
        self.assertEqual(len(objs), 3)
        self.assertTrue(all(o["fin"] and o["niv0"] == "haut" for o in objs))
        self.assertEqual(objs[1]["t_us"], 116000 + 18000)

    def test_krona_index_0_coupe_entre_deux_octets(self):
        # 0xFF + pause = 22 ms de repos dans la trame, plus que les 20 ms entre trames
        objs = self.krona([0], 19000)
        self.assertEqual(len(objs), 2)
        with self.assertRaises(ValueError):
            self.krona([0], 5000)


if __name__ == "__main__":
    unittest.main()
