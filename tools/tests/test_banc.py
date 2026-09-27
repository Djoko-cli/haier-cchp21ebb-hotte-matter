#!/usr/bin/env python3
"""Tests de tools/banc.py sur des captures synthetiques (signaux.simuler_sonde),
dont une capture volontairement abimee.
python3 -m unittest discover -s tools/tests -p test_banc.py -v"""
import contextlib
import io
import json
import os
import re
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import banc  # noqa: E402
import signaux  # noqa: E402


def objets_motif(nom, indices, silence_us, t0_us=1000000):
    """Objets 'trame' de la sonde pour les trames indices du motif, chacune suivie de sa pause."""
    rh = signaux.repos_haut_motif(nom)
    segs = []
    for i in indices:
        segs += signaux.trame_motif(nom, i) + [(rh, signaux.pause_motif_us(nom))]
    return signaux.simuler_sonde(segs, t0_us, silence_us, rh)


def compteurs(n, **champs):
    c = {"v": 1, "t": "compteurs", "n": n, "ms": 0, "bloc": "sonde", "receptions": 0, "parties": 0, "blocs": 0,
         "symboles": 0, "debord": 0, "rep": 0, "lignes_perdues": 0, "sautes": 0, "rejets": 0}
    c.update(champs)
    return c


def niveaux(o):
    """Niveau (haut ?) de chaque duree d'un objet trame."""
    h = o["niv0"] == "haut"
    return [h if k % 2 == 0 else not h for k in range(len(o["dur_us"]))]


class Capture(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def jsonl(self, nom, objets, debut=None, fin=None):
        """Capture .jsonl : hello, compteurs de debut, objets, compteurs de fin."""
        tete = [{"v": 1, "t": "hello", "n": 0, "ms": 0, "bloc": "base"}] + ([debut] if debut else [])
        p = os.path.join(self.dossier.name, nom + ".jsonl")
        with open(p, "w") as f:
            for k, o in enumerate(tete + objets + ([fin] if fin else [])):
                f.write(json.dumps({"rx_ms": 1790000000000 + k, "de": "usb", "l": o}) + "\n")
        return p

    def lancer(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            code = banc.main(list(argv))
        return code, out.getvalue()


class Contenus(Capture):
    def test_uart500_parfait(self):
        p = self.jsonl("uart500", objets_motif("uart500", range(20), 19000))
        v = banc.evaluer(p, "uart500", n=20)
        self.assertEqual((v.hypothese.nom, v.hypothese.params, v.hypothese_ok), ("uart", {"bauds": 500, "inverse": False}, True))
        self.assertEqual(v.reconnues, list(range(20)))
        self.assertEqual((v.taux, v.comparees, v.formes_differentes, v.ecart_max), (1.0, 20, 0, 0))
        self.assertTrue(v.ok1 and v.ok2 and v.ok)
        code, sortie = self.lancer(p, "uart500", "--n", "20")
        self.assertEqual(code, 0)
        self.assertIn("critere 1 : 20 / 20 trames decodees au bit pres (100.0 %) : OK", sortie)
        self.assertIn("critere 2 : ecart max 0 us <= 2 us : OK", sortie)

    def test_uart9600inv_index_au_dela_de_255(self):
        p = self.jsonl("uart9600inv", objets_motif("uart9600inv", range(300), 5000))
        v = banc.evaluer(p, "uart9600inv", n=300)
        self.assertEqual(v.hypothese.params, {"bauds": 9600, "inverse": True})
        self.assertEqual(v.reconnues, list(range(300)))
        self.assertTrue(v.ok)

    def test_wtc_trame_et_reponse(self):
        p = self.jsonl("wtc", objets_motif("wtc", range(12), 5000))
        v = banc.evaluer(p, "wtc", n=12)
        self.assertEqual((v.hypothese.nom, v.hypothese_ok), ("distance_impulsion", True))
        self.assertEqual(v.reconnues, list(range(12)))
        self.assertEqual((v.comparees, v.ecart_max), (12, 0))
        self.assertTrue(v.ok)

    def test_krona_trames_coupees_par_la_sonde(self):
        # index % 8 = 0 ou 1 : 20 ms de repos ou plus dans la trame, la sonde la coupe en deux
        p = self.jsonl("krona", objets_motif("krona", range(16), 19000))
        v = banc.evaluer(p, "krona", n=16)
        self.assertEqual(v.receptions, 20)
        self.assertEqual(v.reconnues, list(range(16)))
        self.assertEqual(v.comparees, 12)  # les 4 trames coupees ne sont pas comparees en durees
        self.assertTrue(v.ok)

    def test_capture_serie_brute(self):
        p = os.path.join(self.dossier.name, "uart2400.log")
        with open(p, "wb") as f:
            f.write(b"firmware 0.1.0-abc1234 (sonde)\r\n")
            for o in objets_motif("uart2400", range(10), 5000):
                f.write(b"\x1e" + json.dumps(o, separators=(",", ":")).encode() + b"\n")
        v = banc.evaluer(p, "uart2400", n=10)
        self.assertEqual(v.reconnues, list(range(10)))
        self.assertTrue(v.ok)

    def test_mauvais_motif(self):
        p = self.jsonl("uart9600", objets_motif("uart9600", range(10), 5000))
        v = banc.evaluer(p, "wtc", n=10)
        self.assertFalse(v.hypothese_ok)
        self.assertEqual(v.reconnues, [])
        self.assertFalse(v.ok)


class Abimee(Capture):
    def test_capture_abimee(self):
        objs = objets_motif("uart2400", range(50), 5000)
        # index 10 perdu (reception num 11) ; index 20 : un palier allonge de 300 us (decodage faux) ;
        # index 30 : un palier allonge de 3 us (decodage juste, ecart de duree hors tolerance).
        objs = [o for o in objs if o["num"] != 11]
        vingt = next(o for o in objs if o["num"] == 21)
        vingt["dur_us"][3] += 300
        trente = next(o for o in objs if o["num"] == 31)
        trente["dur_us"][5] += 3
        p = self.jsonl("abimee", objs)
        v = banc.evaluer(p, "uart2400", n=50)
        self.assertEqual(v.manquantes, [10, 20])
        self.assertEqual(len(v.reconnues), 48)
        self.assertEqual(v.ecart_max, 3)
        self.assertFalse(v.ok1)
        self.assertFalse(v.ok2)
        code, sortie = self.lancer(p, "uart2400", "--n", "50")
        self.assertEqual(code, 1)
        self.assertIn("critere 1 : 48 / 50 trames decodees au bit pres (96.0 %) : ECHEC", sortie)
        self.assertIn("manquantes : 10, 20", sortie)
        self.assertIn("critere 2 : ecart max 3 us <= 2 us : ECHEC", sortie)

    def test_decalage_corrige_l_asymetrie(self):
        objs = objets_motif("uart9600", range(20), 5000)
        for o in objs:  # etage asymetrique : hauts +3 us, bas -3 us
            o["dur_us"] = [d + 3 if h else d - 3 for d, h in zip(o["dur_us"], niveaux(o))]
        p = self.jsonl("asym", objs)
        v = banc.evaluer(p, "uart9600", n=20)
        self.assertEqual(v.reconnues, list(range(20)))
        self.assertEqual((v.ecart_moy_haut, v.ecart_moy_bas, v.ecart_max, v.decalage_suggere), (3, -3, 3, 3))
        self.assertFalse(v.ok2)
        v = banc.evaluer(p, "uart9600", n=20, decalage_us=3)
        self.assertEqual((v.ecart_max, v.decalage_suggere), (0, 3))
        self.assertTrue(v.ok)
        code, sortie = self.lancer(p, "uart9600", "--n", "20", "--decalage-us", "3")
        self.assertEqual(code, 0)
        self.assertIn("decalage suggere +3.0 us", sortie)


class Rafale(Capture):
    def test_rafale_complete(self):
        objs = objets_motif("rafale", [0], 5000)
        self.assertEqual(len(objs), 910)  # 99 999 durees, 110 par partie
        p = self.jsonl("rafale", objs, compteurs(1, debord=4), compteurs(2, debord=4))
        v = banc.evaluer(p, "rafale")
        self.assertEqual((v.n, v.reconnues, v.durees, v.ecart_max), (1, [0], 99999, 0))
        self.assertEqual(v.pertes, "aucune")
        self.assertEqual(v.compteurs, {"debord": 0, "lignes_perdues": 0, "sautes": 0})
        self.assertTrue(v.ok3 and v.ok)
        code, sortie = self.lancer(p, "rafale")
        self.assertEqual(code, 0)
        self.assertIn("critere 3 : 1 / 1 rafales completes ; aucune perte : OK (10000 fronts/s tenus)", sortie)

    def test_rafale_perte_signalee_par_debord(self):
        objs = [o for o in objets_motif("rafale", [0], 5000) if o["part"] != 500]
        next(o for o in objs if o["part"] == 501)["debord"] = True
        v = banc.evaluer(self.jsonl("rafale", objs), "rafale")
        self.assertEqual((v.reconnues, v.incompletes, v.pertes), ([], 2, "signalee"))
        self.assertTrue(v.ok3)

    def test_rafale_perte_non_signalee(self):
        objs = [o for o in objets_motif("rafale", [0], 5000) if o["part"] != 500]
        p = self.jsonl("rafale", objs)
        v = banc.evaluer(p, "rafale")
        self.assertEqual(v.pertes, "non signalee")
        self.assertFalse(v.ok3 or v.ok)
        code, sortie = self.lancer(p, "rafale")
        self.assertEqual(code, 1)
        self.assertIn("critere 3 : 0 / 1 rafales completes ; perte non signalee (reception incomplete sans debord) : ECHEC",
                      sortie)


class Ligne(Capture):
    def test_compteurs_de_la_sonde(self):
        p = self.jsonl("krona", objets_motif("krona", range(8), 19000), compteurs(1, sautes=2), compteurs(2, sautes=3))
        code, sortie = self.lancer(p, "krona", "--n", "8")
        self.assertEqual(code, 0)
        self.assertIn("compteurs de la sonde pendant la capture : debord +0, lignes_perdues +0, sautes +1", sortie)

    def test_codes_de_sortie(self):
        self.assertEqual(self.lancer(os.path.join(self.dossier.name, "absent.jsonl"), "krona")[0], 2)
        vide = self.jsonl("vide", [])
        code, sortie = self.lancer(vide, "krona")
        self.assertEqual(code, 1)
        self.assertIn("aucune trame", sortie)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
            banc.main([vide, "inconnu"])
        self.assertEqual(e.exception.code, 2)


class Documentation(unittest.TestCase):
    """docs/BANC.md : le tableau des reglages par motif est coherent avec les motifs."""

    def test_reglages_par_motif(self):
        chemin = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "BANC.md")
        with open(chemin, encoding="utf-8") as f:
            lignes = [re.match(r"^\| `(\w+)` \| `seuils 1 (\d+)` \| (\d+) \| `motif \1` \|", l) for l in f]
        reglages = {m.group(1): (int(m.group(2)), int(m.group(3))) for m in lignes if m}
        self.assertEqual(sorted(reglages), sorted(signaux.MOTIFS))
        for nom, (silence, duree) in reglages.items():
            rh = signaux.repos_haut_motif(nom)
            n = banc.n_defaut(nom)
            trames = [signaux.trame_motif(nom, i) for i in range(min(n, 256))]
            # Aucun palier hors repos ne clot la reception ; le repos apres la trame la clot.
            hors_repos = max(d for t in trames for niv, d in t if niv != rh)
            apres = min((t[-1][1] if t[-1][0] == rh else 0) + signaux.pause_motif_us(nom) for t in trames)
            self.assertTrue(1000 <= silence <= 32767, nom)
            self.assertLess(hors_repos, silence, nom)
            self.assertGreater(apres, silence, nom)
            # L'enregistrement couvre l'emission des n trames, plus 20 s pour lancer le motif.
            emission = sum(sum(d for _, d in signaux.trame_motif(nom, i)) + signaux.pause_motif_us(nom)
                           for i in range(n)) / 1e6
            self.assertGreaterEqual(duree, emission + 20, nom)


if __name__ == "__main__":
    unittest.main()
