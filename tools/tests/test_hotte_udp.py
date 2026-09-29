#!/usr/bin/env python3
"""Tests de tools/hotte_udp.py sans sonde : vecteurs H1 (docs/PROTOCOLE-JSON.md
9.3, les memes que tools/tests/test_h1.cpp), fenetre contre le rejeu, fichier
de cle, cle par un USB simule, format .jsonl, confirmation de 'injection on'
(phrase exacte, dans un terminal seulement), et une vraie session UDP contre une
sonde simulee sur 127.0.0.1 :
python3 -m unittest discover -s tools/tests -p test_hotte_udp.py -v"""
import contextlib
import io
import json
import os
import socket
import stat
import sys
import tempfile
import threading
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import capture_fmt  # noqa: E402
import hotte_udp  # noqa: E402
import json_check  # noqa: E402

RS = b"\x1e"

# Vecteurs de la ScreenBar (10.4), repris en 9.3 du profil hotte.
PSK = bytes.fromhex("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F")
KID = "630DCD29"
NA = "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
NC = "505152535455565758595A5B5C5D5E5F"
SID = "1234ABCD"
SALUT = b"H1 SALUT 630DCD29 A0A1A2A3A4A5A6A7A8A9AAABACADAEAF 52D853E3FFE9E9CCEFFA98BB5304B32D"
DEFI = b"H1 DEFI 1234ABCD 505152535455565758595A5B5C5D5E5F BFF13F71B42243E6017D2807F8E6171F"
KS = bytes.fromhex("20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F")
MSG_A = b"H1 1234ABCD 1 FD97A0C9E604524B49C763452D0310CE id=1 json 1"
JSON_C = b'{"v":1,"t":"hb","n":7,"ms":1234}'
HDR_C = b"H1 1234ABCD 1 347A2E6A129BC822ECFF39BEC910451C "
MAC_A_MAX = "62CA08CFED5A5FE89EB9AAE8CC3D9CB7"  # A, ctr 4294967295, charge "x"


def compact(obj):
    return json.dumps(obj, separators=(",", ":")).encode("ascii")


class Entree(io.StringIO):
    """Entree standard simulee : un terminal (Majid au clavier) ou non (script, tube, agent)."""

    def __init__(self, tty):
        super().__init__()
        self.tty = tty

    def isatty(self):
        return self.tty


def terminal(tty):
    return mock.patch.object(sys, "stdin", Entree(tty))


class Vecteurs(unittest.TestCase):
    def test_poignee_de_main(self):
        self.assertEqual(hotte_udp.kid_of(PSK), KID)
        self.assertEqual(hotte_udp.texte_salut(PSK, KID, NA), SALUT)
        self.assertEqual(hotte_udp.lire_defi(PSK, KID, NA, DEFI), (SID, KS))
        self.assertEqual(hotte_udp.cle_session(PSK, NA, NC, SID), KS)
        # DEFI d'un autre na, ou abime : ignore.
        self.assertIsNone(hotte_udp.lire_defi(PSK, KID, "B" * 32, DEFI))
        self.assertIsNone(hotte_udp.lire_defi(PSK, KID, NA, DEFI[:-1] + b"0"))
        self.assertIsNone(hotte_udp.lire_defi(PSK, KID, NA, b"H1 1234ABCD 1 " + b"0" * 32 + b" {}"))

    def test_messages(self):
        self.assertEqual(hotte_udp.sceller(KS, "A", SID, 1, b"id=1 json 1"), MSG_A)
        self.assertEqual(hotte_udp.sceller(KS, "C", SID, 1, JSON_C), HDR_C + JSON_C)
        self.assertEqual(hotte_udp.sceller(KS, "A", SID, 4294967295, b"x")[:-2].split(b" ")[3].decode(), MAC_A_MAX)
        f = hotte_udp.Fenetre()
        self.assertEqual(hotte_udp.ouvrir_message(KS, "C", SID, HDR_C + JSON_C, f), (1, JSON_C))
        self.assertIsNone(hotte_udp.ouvrir_message(KS, "C", SID, HDR_C + JSON_C, f))           # rejeu
        f = hotte_udp.Fenetre()
        self.assertIsNone(hotte_udp.ouvrir_message(KS, "A", SID, HDR_C + JSON_C, f))           # autre sens
        self.assertIsNone(hotte_udp.ouvrir_message(KS, "C", "DEADBEEF", HDR_C + JSON_C, f))    # autre session
        self.assertIsNone(hotte_udp.ouvrir_message(KS, "C", SID, HDR_C + JSON_C[:-1] + b" }", f))  # charge modifiee
        self.assertEqual(hotte_udp.ouvrir_message(KS, "C", SID, HDR_C + JSON_C, f), (1, JSON_C))  # fenetre intacte

    def test_fenetre(self):
        # Memes cas que testWindow de test_h1.cpp (fenetre de 32).
        f = hotte_udp.Fenetre()
        self.assertFalse(f.accepter(0))
        self.assertTrue(f.accepter(1))
        self.assertFalse(f.accepter(1))
        self.assertTrue(f.accepter(40))
        self.assertFalse(f.accepter(8))
        self.assertTrue(f.accepter(9))
        self.assertFalse(f.accepter(9))
        self.assertTrue(f.accepter(41))
        self.assertTrue(f.accepter(100))
        self.assertFalse(f.accepter(68))
        self.assertTrue(f.accepter(69))


class Cle(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)
        self.chemin = os.path.join(self.dossier.name, "config", "hotte-sonde", "cle")

    def test_fichier(self):
        hotte_udp.ranger_cle(PSK.hex().upper(), self.chemin)
        self.assertEqual(stat.S_IMODE(os.stat(self.chemin).st_mode), 0o600)
        self.assertEqual(stat.S_IMODE(os.stat(os.path.dirname(self.chemin)).st_mode), 0o700)
        self.assertEqual(hotte_udp.lire_cle(self.chemin), PSK)
        with open(self.chemin, "w") as f:
            f.write("pas une cle\n")
        with self.assertRaises(SystemExit):
            hotte_udp.lire_cle(self.chemin)
        with self.assertRaises(SystemExit):
            hotte_udp.lire_cle(os.path.join(self.dossier.name, "absente"))

    def usb(self, reponse):
        """Sonde simulee sur l'USB : repond a 'id=<n> json cle nouvelle <alea>' par reponse(id, alea)."""
        ecrit, attente = [], [b"reste d'une ligne\n> "]

        def ecrire(octets):
            ecrit.append(octets)
            for ligne in octets.split(b"\n")[:-1]:
                if ligne.startswith(b"id="):
                    ident, _, _, _, alea = ligne.decode().split(" ")
                    attente.append(RS + compact(reponse(int(ident[3:]), alea)) + b"\r\n")

        def lire(_delai):
            return attente.pop(0) if attente else b""

        return lire, ecrire, ecrit

    def test_demander_cle(self):
        cle = "11" * 32

        def ok(ident, alea):
            self.assertRegex(alea, r"^[0-9A-F]{64}$")
            return {"v": 1, "t": "reponse", "n": 3, "ms": 9, "id": ident, "etape": "fin", "cmd": "json cle nouvelle",
                    "ok": True, "code": "ok", "msg": "nouvelle cle : les sessions reseau tombent", "duree_ms": 2,
                    "cle": cle.upper(), "empreinte": hotte_udp.kid_of(bytes.fromhex(cle))}

        lire, ecrire, ecrit = self.usb(ok)
        alea = "AB" * 32
        r = hotte_udp.demander_cle(lire, ecrire, alea, 900123)
        self.assertEqual(r, (cle.upper(), hotte_udp.kid_of(bytes.fromhex(cle)), "nouvelle cle : les sessions reseau tombent"))
        self.assertEqual(ecrit[0], b"\x15\n")
        self.assertEqual(ecrit[1], f"id=900123 json cle nouvelle {alea}\n".encode())

        def fausse_empreinte(ident, alea):
            m = ok(ident, alea)
            m["empreinte"] = "00000000"
            return m

        lire, ecrire, _ = self.usb(fausse_empreinte)
        with self.assertRaises(SystemExit) as e:
            hotte_udp.demander_cle(lire, ecrire, alea, 900124)
        self.assertIn("empreinte incoherente", str(e.exception))

        def refus(ident, _alea):
            return {"v": 1, "t": "reponse", "n": 3, "ms": 9, "id": ident, "etape": "fin", "cmd": "json cle nouvelle",
                    "ok": False, "code": "refuse", "msg": "tampon USB plein : rien n'est change, reessayer",
                    "duree_ms": 0}

        lire, ecrire, _ = self.usb(refus)
        with self.assertRaises(SystemExit) as e:
            hotte_udp.demander_cle(lire, ecrire, alea, 900125)
        self.assertIn("refuse : refuse tampon USB plein", str(e.exception))

    def test_port_absent(self):
        with self.assertRaises(SystemExit) as e:
            hotte_udp.cmd_cle("/dev/cu.inexistant-hotte", self.chemin)
        self.assertIn("/dev/cu.inexistant-hotte", str(e.exception))
        self.assertFalse(os.path.exists(self.chemin))


class Formats(unittest.TestCase):
    def test_jsonl(self):
        charge = b'{"v":1,"t":"hb","n":7,"ms":1234,"boot":"3FA2C901","up_s":1,"json_perdus":0}'
        ligne = hotte_udp.enregistrement(1790000000123, "192.168.1.42", charge)
        self.assertEqual(ligne, '{"rx_ms":1790000000123,"de":"192.168.1.42","l":' + charge.decode() + "}\n")
        de, brut = json_check.jsonl_record(ligne.encode().rstrip(b"\n"))
        self.assertEqual((de, brut), ("192.168.1.42", RS + charge))
        with tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False) as f:
            f.write(ligne)
        self.addCleanup(os.unlink, f.name)
        self.assertEqual(list(capture_fmt.lire_jsonl(f.name)), [json.loads(charge)])

    def test_confirmation(self):
        questions = []

        def repond(texte):
            def demander(q):
                questions.append(q)
                return texte
            return demander

        def ferme(_q):
            raise EOFError

        with terminal(True):
            self.assertTrue(hotte_udp.confirmer(["capture tout", "json etat"], repond("non")))
            self.assertEqual(questions, [])  # rien a confirmer : aucune question
            # La phrase exacte, tapee par Majid ; la question rappelle la regle 11.
            self.assertTrue(hotte_udp.confirmer(["injection  on"], repond("Majid devant la hotte")))
            for mot in ("Regle 11", "a portee de la fiche", "le panneau en vue", "Majid devant la hotte"):
                self.assertIn(mot, questions[0])
            self.assertFalse(hotte_udp.confirmer(["injection on"], repond("oui")))
            self.assertFalse(hotte_udp.confirmer(["injection on"], repond("majid devant la hotte")))
            self.assertFalse(hotte_udp.confirmer(["injection on"], repond("Majid devant la hotte ")))
            self.assertFalse(hotte_udp.confirmer(["injection on"], ferme))
        # Sans terminal (script, tube, agent) : refus, meme avec la phrase, et sans question.
        del questions[:]
        with terminal(False):
            self.assertFalse(hotte_udp.confirmer(["injection on"], repond("Majid devant la hotte")))
            self.assertTrue(hotte_udp.confirmer(["capture tout"], repond("")))
        self.assertEqual(questions, [])

    def test_resolution(self):
        self.assertEqual(hotte_udp.resoudre("127.0.0.1", 5480)[0], socket.AF_INET)
        self.assertEqual(hotte_udp.resoudre("::1", 5480)[0], socket.AF_INET6)
        with self.assertRaises(SystemExit):
            hotte_udp.resoudre("nom-qui-n-existe-pas.invalid", 5480)


class SondeSimulee(threading.Thread):
    """Joue la sonde sur 127.0.0.1 : poignee de main H1, une reponse par
    commande, quelques lignes du profil hotte, et un datagramme au MAC faux."""

    def __init__(self, cle):
        super().__init__(daemon=True)
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(("127.0.0.1", 0))
        self.s.settimeout(0.05)
        self.port = self.s.getsockname()[1]
        self.cle, self.kid = cle, hotte_udp.kid_of(cle)
        self.ks, self.ctr, self.n = None, 0, 0
        self.fenetre = hotte_udp.Fenetre()
        self.recues = []
        self.arret = threading.Event()

    def ligne(self, t, **champs):
        obj = {"v": 1, "t": t, "n": self.n, "ms": 5000 + self.n}
        obj.update(champs)
        self.n += 1
        return compact(obj)

    def envoyer(self, adr, charge):
        self.ctr += 1
        self.s.sendto(hotte_udp.sceller(self.ks, "C", SID, self.ctr, charge), adr)

    def reponse(self, ident, cmd, **champs):
        return self.ligne("reponse", id=ident, etape="fin", cmd=cmd, ok=True, code="ok", duree_ms=1, **champs)

    def trame(self):
        return self.ligne("trame", num=self.n, part=0, fin=True, t_us=1000000 + self.n, niv0="bas",
                          dur_us=[2000, 4000, 2000], debord=False)

    def run(self):
        while not self.arret.is_set():
            try:
                d, adr = self.s.recvfrom(2048)
            except socket.timeout:
                continue
            p = d.split(b" ")
            if len(p) == 5 and p[1] == b"SALUT":
                na = p[3].decode()
                if hotte_udp.texte_salut(self.cle, self.kid, na) != d:
                    continue
                mac = hotte_udp.mac16(self.cle, f"H1|DEFI|{self.kid}|{na}|{NC}|{SID}").hex().upper()
                self.s.sendto(f"H1 DEFI {SID} {NC} {mac}".encode(), adr)
                self.ks, self.ctr, self.fenetre = hotte_udp.cle_session(self.cle, na, NC, SID), 0, hotte_udp.Fenetre()
                continue
            r = hotte_udp.ouvrir_message(self.ks, "A", SID, d, self.fenetre) if self.ks else None
            if r is None:
                continue
            ident, cmd = r[1].decode()[3:].split(" ", 1)
            ident = int(ident)
            self.recues.append(cmd)
            if cmd == "json 1":
                self.envoyer(adr, self.ligne("hb", boot="3FA2C901", up_s=5, json_perdus=0))
                self.envoyer(adr, self.trame())
                self.envoyer(adr, self.reponse(ident, cmd, bail_s=30, up_s=5))
                self.s.sendto(b"H1 1234ABCD 99 " + b"0" * 32 + b" {}", adr)  # MAC faux : rejete
            elif cmd == "json 0":
                self.envoyer(adr, self.reponse(ident, cmd))
                self.envoyer(adr, self.ligne("fin", cause="commande"))
            else:
                self.envoyer(adr, self.reponse(ident, cmd))
                self.envoyer(adr, self.trame())


class SessionUdp(unittest.TestCase):
    def setUp(self):
        self.sonde = SondeSimulee(PSK)
        self.sonde.start()
        self.addCleanup(self.sonde.s.close)
        self.addCleanup(self.sonde.arret.set)
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def test_session(self):
        sortie = []
        stats = hotte_udp.cmd_session("127.0.0.1", self.sonde.port, ["capture tout"], 2.0, False, PSK, sortie.append)
        texte = "\n".join(sortie)
        self.assertIn(f"sonde 127.0.0.1 port {self.sonde.port}, cle {KID}", texte)
        self.assertIn("session 1234ABCD ouverte", texte)
        self.assertIn(">> id=1 json 1", texte)
        self.assertIn("n=0 hb", texte)
        self.assertIn("n=2 reponse id=1 fin ok « json 1 »", texte)
        self.assertIn(">> id=2 capture tout", texte)
        self.assertEqual(self.sonde.recues, ["json 1", "capture tout", "json 0"])
        self.assertEqual(stats["rejetees"], 1)
        self.assertEqual(stats["sessions"], 1)

    def test_enregistre(self):
        sortie = []
        chemin = hotte_udp.cmd_enregistre("127.0.0.1", self.sonde.port, "essai udp", ["capture tout"], 2.0,
                                          self.dossier.name, PSK, sortie.append)
        self.assertRegex(os.path.basename(chemin), r"^\d{4}-\d{2}-\d{2}-\d{4}-essai-udp\.jsonl$")
        with open(chemin, encoding="ascii") as f:
            recs = [json.loads(l) for l in f]
        self.assertEqual({r["de"] for r in recs}, {"127.0.0.1"})
        self.assertEqual([r["l"]["t"] for r in recs], ["hb", "trame", "reponse", "reponse", "trame", "reponse", "fin"])
        self.assertTrue(all(isinstance(r["rx_ms"], int) for r in recs))
        with contextlib.redirect_stdout(io.StringIO()) as out:
            code = json_check.main(["--strict", "--jsonl", chemin])
        self.assertEqual(code, 0, out.getvalue())
        self.assertIn("7 ligne(s) machine, 0 erreur(s)", out.getvalue())
        with open(os.path.join(self.dossier.name, "live.log"), encoding="utf-8") as f:
            journal = f.read()
        self.assertRegex(journal, r"udp 127\.0\.0\.1 essai udp fin 2 s : 7 lignes .*1 rejetee")

    def test_injection_confirmee(self):
        # 'oui' au clavier, ou la phrase sans terminal (script, agent) : rien n'est envoye.
        for tty, reponse in ((True, "oui"), (False, "Majid devant la hotte")):
            with terminal(tty), self.assertRaises(SystemExit) as e:
                hotte_udp.cmd_session("127.0.0.1", self.sonde.port, ["injection on"], 1.0, False, PSK,
                                      lambda *_: None, demander=lambda _q, r=reponse: r)
            self.assertIn("rien n'est envoye", str(e.exception))
        self.assertEqual(self.sonde.recues, [])
        # La phrase exacte, dans un terminal : la commande part apres 'json 1'.
        with terminal(True):
            hotte_udp.cmd_session("127.0.0.1", self.sonde.port, ["injection on"], 2.0, False, PSK, lambda *_: None,
                                  demander=lambda _q: "Majid devant la hotte")
        self.assertEqual(self.sonde.recues, ["json 1", "injection on", "json 0"])


class Produit(unittest.TestCase):
    """Cle d'apres l'appareil (build de hello), resume du profil produit."""

    def test_chemin_par_appareil(self):
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop("HOTTE_CLE", None)
            self.assertTrue(hotte_udp.chemin_cle().endswith(os.path.join(".config", "hotte-sonde", "cle")))
            self.assertTrue(hotte_udp.chemin_cle("produit").endswith(os.path.join(".config", "hotte-produit", "cle")))
            with self.assertRaises(SystemExit):
                hotte_udp.chemin_cle("screenbar")
        with mock.patch.dict(os.environ, {"HOTTE_CLE": "/tmp/une-cle"}):
            self.assertEqual(hotte_udp.chemin_cle("produit"), "/tmp/une-cle")

    def usb_hello(self, build):
        """Module simule sur l'USB : 'id=<n> json hello' -> hello.base (build), puis reponse fin."""
        attente = [b"reste d'une ligne\n> "]  # le decoupage se cale sur le premier LF

        def ecrire(octets):
            for ligne in octets.split(b"\n")[:-1]:
                if ligne.startswith(b"id=") and ligne.endswith(b"json hello"):
                    ident = int(ligne.split(b" ")[0][3:])
                    if build:
                        attente.append(RS + compact({"v": 1, "t": "hello", "n": 1, "ms": 5, "bloc": "base",
                                                     "build": build}) + b"\r\n")
                    attente.append(RS + compact({"v": 1, "t": "reponse", "n": 2, "ms": 6, "id": ident, "etape": "fin",
                                                 "cmd": "json hello", "ok": True, "code": "ok", "duree_ms": 1}) + b"\r\n")

        def lire(_delai):
            return attente.pop(0) if attente else b""

        return lire, ecrire

    def test_lire_build(self):
        for build in ("produit", "sonde"):
            lire, ecrire = self.usb_hello(build)
            self.assertEqual(hotte_udp.lire_build(lire, ecrire, 900001), build)
        lire, ecrire = self.usb_hello(None)  # firmware sans build : rien
        self.assertIsNone(hotte_udp.lire_build(lire, ecrire, 900002))
        t = [0.0]

        def horloge():
            t[0] += 0.5
            return t[0]

        self.assertIsNone(hotte_udp.lire_build(lambda _d: b"", lambda _o: None, 900003, horloge=horloge))

    def test_resume_produit(self):
        r = hotte_udp.resume({"v": 1, "t": "sequence", "n": 4, "ms": 1, "id": 17, "origine": "app", "sujet": "ventilo",
                              "issue": "echec", "cause": "non_confirme", "duree_ms": 2400, "appuis": 2})
        self.assertEqual(r, "n=4 sequence id=17 app ventilo : echec, non_confirme (2 appuis, 2400 ms)")
        r = hotte_udp.resume({"v": 1, "t": "etat", "n": 5, "ms": 1, "bloc": "hotte", "boot": "3FA2C901", "up_s": 1,
                              "marche": "armee", "moteur": 2, "lampe": None, "confiance": "confirme", "source": "fil",
                              "age_ms": 120})
        self.assertEqual(r, "n=5 etat/hotte armee moteur 2 lampe ? (confirme, fil, lu il y a 120 ms)")
        r = hotte_udp.resume({"v": 1, "t": "alerte", "n": 6, "ms": 1, "sujet": "temperature", "etape": "debut",
                              "valeur": 71, "seuil": 70, "unite": "c"})
        self.assertEqual(r, "n=6 alerte temperature debut : 71 c (seuil 70)")
        r = hotte_udp.resume({"v": 1, "t": "hotte", "n": 7, "ms": 1,
                              "avant": {"marche": "armee", "moteur": 0, "lampe": False},
                              "apres": {"marche": "armee", "moteur": 2, "lampe": True},
                              "origine": "panneau", "source": "fil", "confiance": "confirme"})
        self.assertEqual(r, "n=7 hotte armee/0 -> armee/2 lampe on (panneau, fil)")
        self.assertIn("puce 45 C", hotte_udp.resume({"v": 1, "t": "etat", "n": 8, "ms": 1, "bloc": "thermique",
                                                      "temp_c": 45, "temp_max_c": 46, "temp_max_pose_c": 61,
                                                      "alerte": False, "seuil_c": 70}))
        # Un message de la sonde garde son resume.
        self.assertEqual(hotte_udp.resume({"v": 1, "t": "fin", "n": 9, "ms": 1, "cause": "bail"}), "n=9 fin cause bail")


if __name__ == "__main__":
    unittest.main()
