#!/usr/bin/env python3
"""Tests de tools/verse_capture.py, sans toucher a captures/ :
python3 -m unittest discover -s tools/tests -p test_verse_capture.py -v"""
import contextlib
import datetime
import io
import json
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ICI, ".."))
import verse_capture  # noqa: E402

README = os.path.join(ICI, "..", "..", "captures", "README.md")
SECRET = "0123456789ABCDEF" * 4  # 64 hexa, comme une cle H1
RX0 = 1791100800000

HELLO = {
    "v": 1, "t": "hello", "n": 0, "ms": 12031, "bloc": "base", "rev": 4, "fw": "0.1.0-1a2b3c4",
    "fw_desc": "0.1.0-1a2b3c4", "date": "Sep 27 2026", "heure": "14:02:11", "env": "sonde", "build": "sonde",
    "reseau_build": "aucun", "puce": "esp32c6", "idf": "v5.5.5", "arduino": "3.3.12", "boot": "3FA2C901",
    "reset": "mise_sous_tension", "reset_n": 1, "up_s": 12,
    "session": {"transport": "usb", "periode_ms": 1000, "compteurs_ms": 1000, "reseau_ms": 5000, "bail_s": 0,
                "trames": True, "log": False},
    "limites": {"ligne_max": 1024, "cmd_max": 127},
}
HB = {"v": 1, "t": "hb", "n": 1, "ms": 14031, "boot": "3FA2C901", "up_s": 14, "json_perdus": 0}
TRAME = {"v": 1, "t": "trame", "n": 2, "ms": 15506, "num": 57, "part": 0, "fin": True, "t_us": 15472110,
         "niv0": "bas", "dur_us": [1500, 750, 750, 2250], "debord": False}
REPONSE = {"v": 1, "t": "reponse", "n": 3, "ms": 15600, "id": 2, "etape": "fin", "cmd": "json ping", "ok": True,
           "code": "ok", "duree_ms": 0}
RESEAU_IP = {"v": 1, "t": "reseau", "n": 7, "ms": 12038, "bloc": "ip", "srp": None,
             "wifi": {"connecte": True, "rssi_dbm": -58, "ip": "192.168.1.42", "pertes": 0}}


def enr(l, k=0, de="usb"):
    """Une ligne .jsonl, comme serie_enregistre.py et hotte_udp.py l'ecrivent."""
    return '{"rx_ms":%d,"de":"%s","l":%s}' % (RX0 + k, de, json.dumps(l, separators=(",", ":")))


def avec(obj, **champs):
    o = dict(obj)
    o.update(champs)
    return o


def lancer(*argv):
    sortie = io.StringIO()
    with contextlib.redirect_stdout(sortie), contextlib.redirect_stderr(sortie):
        code = verse_capture.main(list(argv))
    return code, sortie.getvalue()


class Refus(unittest.TestCase):
    def test_ligne_ordinaire(self):
        for l in (HELLO, HB, TRAME, REPONSE):
            self.assertEqual(verse_capture.refus(enr(l)), [])
        self.assertEqual(verse_capture.refus(b"pas du json"), [])  # json_check.py la refusera

    def test_champ_cle(self):
        self.assertEqual(verse_capture.refus(enr(avec(REPONSE, cle="7A"))), ["champ 'cle'"])
        self.assertEqual(verse_capture.refus(enr(avec(HB, x={"y": [{"cle": 1}]}))), ["champ 'cle'"])

    def test_chaine_de_64_hexa(self):
        self.assertIn("chaine de 64 hexa", verse_capture.refus(enr(avec(REPONSE, cmd="x" + SECRET))))
        self.assertIn("chaine de 64 hexa", verse_capture.refus(enr(avec(HB, txt=SECRET.lower()))))
        # Un caractere echappe en JSON coupe la suite dans le texte brut : vue apres lecture du JSON.
        echappee = enr(avec(HB, txt=SECRET)).replace(SECRET, SECRET[:10] + "\\u0041" + SECRET[11:])
        self.assertIn("chaine de 64 hexa", verse_capture.refus(echappee))
        self.assertEqual(verse_capture.refus(enr(avec(HB, txt=SECRET[:63]))), [])

    def test_ssid_mdp_et_wifi(self):
        self.assertEqual(verse_capture.refus(enr(avec(HB, ssid="MonReseau"))), ["champ 'ssid'"])
        self.assertEqual(verse_capture.refus(enr(avec(HB, x={"mdp": "secret12"}))), ["champ 'mdp'"])
        self.assertEqual(verse_capture.refus(enr(avec(HB, wifi="MonReseau"))), ["champ 'wifi'"])

    def test_bloc_wifi_de_reseau_ip(self):
        # Etat de la connexion (docs/PROTOCOLE-JSON.md, reseau ip) : sans SSID, admis.
        self.assertEqual(verse_capture.refus(enr(RESEAU_IP)), [])
        wifi = dict(RESEAU_IP["wifi"], nom="MonReseau")
        self.assertEqual(verse_capture.refus(enr(avec(RESEAU_IP, wifi=wifi))), ["champ 'wifi'"])
        self.assertEqual(verse_capture.refus(enr(avec(HB, wifi=RESEAU_IP["wifi"]))), ["champ 'wifi'"])

    def test_reponse_a_la_commande_wifi(self):
        # maskCmd coupe le mot de passe, pas le SSID.
        self.assertEqual(verse_capture.refus(enr(avec(REPONSE, cmd="wifi MonReseau"))),
                         ["reponse a 'wifi' (SSID dans cmd)"])


class Versement(unittest.TestCase):
    def setUp(self):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        self.dossier = d.name
        self.captures = os.path.join(d.name, "captures")
        os.makedirs(self.captures)
        shutil.copy(README, os.path.join(self.captures, "README.md"))
        p = mock.patch.object(verse_capture, "CAPTURES", self.captures)
        p.start()
        self.addCleanup(p.stop)

    def ecrire(self, nom, lignes):
        p = os.path.join(self.dossier, nom)
        with open(p, "w", encoding="ascii") as f:
            f.write("".join(x + "\n" for x in lignes))
        return p

    def readme(self):
        with open(os.path.join(self.captures, "README.md"), encoding="utf-8") as f:
            return f.read()

    def test_verse_telle_quelle_avec_provenance(self):
        src = self.ecrire("2026-10-04-1412-veille.jsonl", [enr(HELLO, 0), enr(HB, 1), enr(TRAME, 2)])
        code, out = lancer(src, "etape5-veille")
        self.assertEqual(code, 0, out)
        with open(src, "rb") as a, open(os.path.join(self.captures, "etape5-veille.jsonl"), "rb") as b:
            self.assertEqual(a.read(), b.read())
        quand = datetime.datetime.fromtimestamp(RX0 / 1000).strftime("%Y-%m-%d %H:%M:%S")
        self.assertTrue(self.readme().endswith(
            "| `etape5-veille.jsonl` | `2026-10-04-1412-veille.jsonl` | %s | 3 | usb | 0.1.0-1a2b3c4 | 3FA2C901 |\n"
            % quand), self.readme()[-300:])
        self.assertIn("versee : ", out)

    def test_refus_sans_rien_ecrire(self):
        avant = self.readme()
        src = self.ecrire("c.jsonl", [enr(HB, 1), enr(avec(REPONSE, cle=SECRET), 2)])
        code, out = lancer(src, "etape5-marche")
        self.assertEqual(code, 1, out)
        self.assertIn("c.jsonl:2 : champ 'cle'", out)
        self.assertNotIn(SECRET, out)  # jamais la ligne refusee
        self.assertFalse(os.path.exists(os.path.join(self.captures, "etape5-marche.jsonl")))
        self.assertEqual(self.readme(), avant)

    def test_refus_de_json_check(self):
        src = self.ecrire("c.jsonl", [enr(HB, 1), enr(avec(TRAME, dur_us=[0, 5]), 2)])
        code, out = lancer(src, "etape5-marche")
        self.assertEqual(code, 1, out)
        self.assertIn("json_check.py refuse la capture", out)
        self.assertEqual(os.listdir(self.captures), ["README.md"])

    def test_pas_d_ecrasement_sans_force(self):
        a = self.ecrire("a.jsonl", [enr(HB, 1)])
        b = self.ecrire("b.jsonl", [enr(HB, 1), enr(TRAME, 2)])
        dest = os.path.join(self.captures, "etape5-v1.jsonl")
        self.assertEqual(lancer(a, "etape5-v1")[0], 0)
        code, out = lancer(b, "etape5-v1")
        self.assertEqual(code, 1, out)
        self.assertIn("--force", out)
        self.assertEqual(os.path.getsize(dest), os.path.getsize(a))
        self.assertEqual(lancer(b, "etape5-v1", "--force")[0], 0)
        self.assertEqual(os.path.getsize(dest), os.path.getsize(b))
        lignes = [x for x in self.readme().splitlines() if x.startswith("| `etape5-v1.jsonl` |")]
        self.assertEqual(len(lignes), 1, lignes)
        self.assertIn("| `b.jsonl` |", lignes[0])

    def test_usage(self):
        src = self.ecrire("c.jsonl", [enr(HB, 1)])
        for nom in ("../x", "a/b", "x.jsonl", "", "-x"):
            with self.subTest(nom=nom):
                self.assertEqual(lancer(src, "--", nom)[0], 2)
        self.assertEqual(lancer(os.path.join(self.dossier, "absent.jsonl"), "x")[0], 2)
        self.assertEqual(lancer(self.ecrire("vide.jsonl", []), "x")[0], 1)
        self.assertEqual(os.listdir(self.captures), ["README.md"])


if __name__ == "__main__":
    unittest.main()
