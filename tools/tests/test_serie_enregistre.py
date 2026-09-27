#!/usr/bin/env python3
"""Tests de tools/serie_enregistre.py sur un flux simule, sans port reel :
python3 -m unittest discover -s tools/tests -p test_serie_enregistre.py -v"""
import contextlib
import datetime
import io
import json
import os
import socket
import struct
import sys
import tempfile
import termios
import threading
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import capture_fmt  # noqa: E402
import json_check  # noqa: E402
import serie_enregistre  # noqa: E402

RS = b"\x1e"


def compact(obj):
    return json.dumps(obj, separators=(",", ":")).encode("ascii")


class FausseSonde:
    """Repond aux lignes de l'hote comme la sonde en mode machine (docs/PROTOCOLE-JSON.md)."""

    def __init__(self, sauter_n=None, interrompre_a=None, fermer_a=None):
        self.t = 1000.0
        self.n = 0
        self.machine = False
        self.en_attente = b"reste d'une ligne d'avant\n> "  # avant la resynchronisation
        self.recu = []
        self.sauter_n = sauter_n          # n jamais emis (ligne perdue cote sonde)
        self.interrompre_a = interrompre_a  # Ctrl-C simule a cette heure
        self.fermer_a = fermer_a            # port qui disparait (sonde redemarree) a cette heure

    def horloge(self):
        return self.t

    def ligne(self, t, **champs):
        obj = {"v": 1, "t": t, "n": self.n, "ms": int(self.t * 1000) % 2**32}
        obj.update(champs)
        self.n += 1
        if obj["n"] == self.sauter_n:
            return b""
        return RS + compact(obj) + b"\n"

    def reponse(self, ident, cmd, **champs):
        return self.ligne("reponse", id=ident, etape="fin", cmd=cmd, ok=True, code="ok", duree_ms=1, **champs)

    def ecrire(self, octets):
        self.recu.append(octets)
        for brut in octets.split(b"\n")[:-1]:
            texte = brut.decode("ascii")
            if not texte.startswith("id="):
                continue
            ident, cmd = texte[3:].split(" ", 1)
            ident = int(ident)
            if cmd == "json 1 bail 0":
                self.machine = True
                self.en_attente += self.ligne("hb", boot="3FA2C901", up_s=1, json_perdus=0)
                self.en_attente += self.reponse(ident, cmd, bail_s=0, up_s=1)
            elif cmd == "capture tout":
                self.en_attente += self.reponse(ident, cmd)
                self.en_attente += self.ligne("config", capture={"gpio": 6, "resol_hz": 1000000, "filtre_us": 1,
                                                                 "silence_us": 5000, "mode": "tout", "inverse": True})
            elif cmd == "json 0":
                self.en_attente += self.reponse(ident, cmd)
                self.en_attente += self.ligne("fin", cause="commande") + b"> "
                self.machine = False

    def lire(self, delai):
        if self.interrompre_a is not None and self.t >= self.interrompre_a:
            self.interrompre_a = None
            raise KeyboardInterrupt
        if self.fermer_a is not None and self.t >= self.fermer_a:
            raise serie_enregistre.PortFerme("port ferme")
        self.t += delai
        if self.machine:
            self.en_attente += self.ligne("trame", num=self.n, part=0, fin=True, t_us=int(self.t * 1e6), niv0="bas",
                                          dur_us=[1500, 750, 750, 2250], debord=False)
        octets, self.en_attente = self.en_attente, b""
        return octets

    def commandes(self):
        return [l.decode() for o in self.recu for l in o.split(b"\n") if l.startswith(b"id=")]


def sortie_muette(*_):
    pass


def lire_texte(chemin):
    with open(chemin, encoding="utf-8") as f:
        return f.read()


class Decoupage(unittest.TestCase):
    def test_machine_texte_abimee(self):
        d = serie_enregistre.Decoupe(resynchro=False)
        hb = b'{"v":1,"t":"hb","n":3,"ms":5,"boot":"3FA2C901","up_s":1,"json_perdus":0}'
        fin = b'{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}'
        flux = (b"> json 1\r\n" + RS + hb + b"\n" + b"E (123) tag: un log coupe " + RS + fin + b"\r\n"
                + RS + b'{"v":1,"t":"hb","n"\n' + RS + b'{"v":2,"t":"hb","n":1,"ms":1}\n' + b"pas fini")
        ev = []
        for i in range(0, len(flux), 7):  # arrive par morceaux de 7 octets
            ev += d.feed(flux[i:i + 7])
        self.assertEqual([g for g, _, _ in ev], ["texte", "machine", "texte", "machine", "abimee", "abimee"])
        self.assertEqual(ev[0][1], b"> json 1")
        self.assertEqual(ev[1][1], hb)
        self.assertEqual(ev[1][2]["t"], "hb")
        self.assertEqual(ev[2][1], b"E (123) tag: un log coupe ")  # texte avant le RS
        self.assertEqual(ev[3][1], fin)  # dernier RS de la ligne, CR retire

    def test_resynchronisation(self):
        d = serie_enregistre.Decoupe()
        ev = d.feed(b'1,"ms":5}\n' + RS + b'{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}\n')
        self.assertEqual([g for g, _, _ in ev], ["machine"])  # le reste d'avant le premier LF est jete

    def test_ligne_sans_fin(self):
        d = serie_enregistre.Decoupe(resynchro=False)
        ev = d.feed(b"x" * 3000)
        self.assertEqual([(g, len(b)) for g, b, _ in ev], [("texte", 3000)])
        ev = d.feed(RS + b'{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}\n')
        self.assertEqual([g for g, _, _ in ev], ["machine"])


class Fichiers(unittest.TestCase):
    def test_nom_fichier(self):
        quand = datetime.datetime(2026, 9, 27, 14, 3, 59)
        self.assertEqual(serie_enregistre.nom_fichier("krona", quand), "2026-09-27-1403-krona.jsonl")
        self.assertEqual(serie_enregistre.nom_fichier("banc wtc/1", quand), "2026-09-27-1403-banc-wtc-1.jsonl")
        self.assertEqual(serie_enregistre.nom_fichier("//", quand), "2026-09-27-1403-session.jsonl")

    def test_enregistrement_jsonl(self):
        brut = b'{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}'
        self.assertEqual(serie_enregistre.enregistrement(1790000000123, brut),
                         '{"rx_ms":1790000000123,"de":"usb","l":{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}}\n')


class Commandes(unittest.TestCase):
    def test_une_commande_en_vol(self):
        c = serie_enregistre.Commandes(["json 1 bail 0", "capture tout", "seuils 1 8000"])
        self.assertEqual(c.a_envoyer(0.0), b"id=1 json 1 bail 0\n")
        self.assertIsNone(c.a_envoyer(1.0))  # la premiere est en vol
        self.assertIsNone(c.recu({"t": "reponse", "id": 1, "etape": "debut", "code": "en_cours"}))
        self.assertIsNone(c.recu({"t": "trame", "n": 5}))
        cr = c.recu({"t": "reponse", "id": 1, "etape": "fin", "ok": True, "code": "ok", "cmd": "json 1 bail 0"})
        self.assertEqual(cr, "id=1 json 1 bail 0 : ok")
        self.assertEqual(c.a_envoyer(1.1), b"id=2 capture tout\n")
        # Sans reponse fin en 3 s : la suivante part, la commande est notee sans reponse.
        self.assertIsNone(c.a_envoyer(4.0))
        self.assertEqual(c.a_envoyer(4.2), b"id=3 seuils 1 8000\n")
        cr = c.recu({"t": "reponse", "id": 3, "etape": "fin", "ok": False, "code": "usage", "cmd": "seuils 1 8000",
                     "msg": "seuils : ..."})
        self.assertEqual(cr, "id=3 seuils 1 8000 : usage (seuils : ...)")
        self.assertTrue(c.fini())
        self.assertEqual(c.resultats, ["id=1 json 1 bail 0 : ok", "id=2 capture tout : sans reponse",
                                       "id=3 seuils 1 8000 : usage (seuils : ...)"])
        self.assertEqual(c.prochain_id, 4)


class Ouverture(unittest.TestCase):
    def test_dtr_rts_a_zero_en_un_seul_appel(self):
        appels = []
        attrs = [termios.ICRNL | termios.IXON, termios.OPOST, termios.HUPCL | termios.CS7 | termios.PARENB,
                 termios.ECHO | termios.ICANON, termios.B9600, termios.B9600, [0] * 20]
        with mock.patch.object(serie_enregistre.os, "open", return_value=42) as ouvre, \
                mock.patch.object(serie_enregistre.fcntl, "ioctl",
                                  side_effect=lambda fd, req, arg=0: appels.append((fd, req, arg))), \
                mock.patch.object(serie_enregistre.termios, "tcgetattr", return_value=attrs), \
                mock.patch.object(serie_enregistre.termios, "tcsetattr") as regle:
            self.assertEqual(serie_enregistre.ouvrir_port("/dev/cu.usbmodem101"), 42)
        flags = ouvre.call_args[0][1]
        self.assertEqual(flags & (os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK), os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        # TIOCEXCL puis UN SEUL TIOCMSET a 0 (DTR = RTS = 0 ensemble) ; jamais TIOCMBIS ni TIOCMBIC.
        self.assertEqual([(fd, req) for fd, req, _ in appels], [(42, termios.TIOCEXCL), (42, termios.TIOCMSET)])
        self.assertEqual(appels[1][2], struct.pack("I", 0))
        iflag, oflag, cflag, lflag, ispeed, ospeed, _ = regle.call_args[0][2]
        self.assertFalse(cflag & termios.HUPCL)  # la fermeture ne touche plus DTR ni RTS
        self.assertEqual(cflag & (termios.CSIZE | termios.PARENB | termios.CLOCAL | termios.CREAD),
                         termios.CS8 | termios.CLOCAL | termios.CREAD)
        self.assertFalse(iflag & (termios.ICRNL | termios.IXON) or oflag & termios.OPOST
                         or lflag & (termios.ECHO | termios.ICANON))
        self.assertEqual((ispeed, ospeed), (termios.B115200, termios.B115200))


class Session(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)
        self.jsonl = os.path.join(self.dossier.name, "s.jsonl")
        self.journal = os.path.join(self.dossier.name, "live.log")

    def lancer(self, sonde, duree_s, commandes=("capture tout",)):
        with open(self.jsonl, "w", encoding="ascii") as sortie:
            return serie_enregistre.enregistrer(sonde.lire, sonde.ecrire, sortie, self.journal, "essai",
                                                list(commandes), duree_s, horloge=sonde.horloge,
                                                afficher=sortie_muette)

    def test_enregistrement_complet(self):
        sonde = FausseSonde(sauter_n=20)
        bilan = self.lancer(sonde, 12.0)
        # Ctrl-U d'abord, puis une commande a la fois, 'json 0' a la fin.
        self.assertEqual(sonde.recu[0], b"\x15\n")
        self.assertEqual(sonde.commandes(), ["id=1 json 1 bail 0", "id=2 capture tout", "id=3 json 0"])
        lignes = lire_texte(self.jsonl).splitlines()
        rec = json.loads(lignes[0])
        self.assertEqual(list(rec), ["rx_ms", "de", "l"])
        self.assertEqual(rec["de"], "usb")
        self.assertEqual(rec["rx_ms"], 1000200)  # heure du Mac en ms (horloge simulee)
        self.assertEqual(rec["l"]["t"], "hb")
        dernier = json.loads(lignes[-1])["l"]
        self.assertEqual((dernier["t"], dernier["cause"], dernier["n"]), ("fin", "commande", sonde.n - 1))
        # Lisible par capture_fmt, conforme au profil (json_check --jsonl) avec un trou de n.
        objets = list(capture_fmt.lire_jsonl(self.jsonl))
        self.assertEqual(len(objets), len(lignes))
        self.assertGreater(len(capture_fmt.parties(objets)), 50)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(json_check.main(["-q", self.jsonl]), 0)
        self.assertIn("0 erreur(s)", out.getvalue())
        self.assertIn("n : 1 trou(s) (1 ligne(s) perdue(s)", out.getvalue())
        # Bilan : lignes, trou, texte (invites), resultats des commandes.
        self.assertEqual(bilan.lignes, len(lignes))
        self.assertEqual((bilan.trous, bilan.perdues), (1, 1))
        self.assertEqual(bilan.par_type["reponse"], 3)
        self.assertEqual(bilan.abimees, 0)
        self.assertGreaterEqual(bilan.texte, 1)
        # live.log : un resume toutes les 5 s, puis le resume final.
        journal = lire_texte(self.journal).splitlines()
        self.assertEqual(len(journal), 3)
        self.assertTrue(all(" usb essai " in l for l in journal), journal)
        self.assertIn("fin", journal[-1])
        self.assertIn(f"{len(lignes)} lignes", journal[-1])
        self.assertIn("1 trou", journal[-1])

    def test_ctrl_c_ferme_la_session(self):
        sonde = FausseSonde(interrompre_a=1003.0)
        bilan = self.lancer(sonde, None)
        self.assertEqual(sonde.commandes()[-1], "id=3 json 0")
        self.assertFalse(sonde.machine)
        self.assertGreater(bilan.lignes, 5)
        self.assertIn("interrompu", lire_texte(self.journal))

    def test_port_ferme(self):
        sonde = FausseSonde(fermer_a=1002.0)
        bilan = self.lancer(sonde, 10.0)
        self.assertNotIn("json 0", " ".join(sonde.commandes()))  # plus de port : rien a envoyer
        self.assertGreater(bilan.lignes, 5)
        self.assertIn("port ferme", lire_texte(self.journal).splitlines()[-1])


class PortReel(unittest.TestCase):
    """main() de bout en bout sur une paire de sockets a la place du port serie."""

    def test_main(self):
        a, b = socket.socketpair()
        self.addCleanup(a.close)
        self.addCleanup(b.close)
        recu = []

        def sonde():
            tampon = b""
            n = 0
            while True:
                try:
                    morceau = b.recv(4096)
                except OSError:  # socket fermee par le nettoyage du test
                    return
                if not morceau:
                    return
                tampon += morceau
                while b"\n" in tampon:
                    ligne, tampon = tampon.split(b"\n", 1)
                    recu.append(ligne)
                    if ligne.startswith(b"id="):
                        ident, cmd = ligne[3:].split(b" ", 1)
                        obj = {"v": 1, "t": "reponse", "n": n, "ms": 10 + n, "id": int(ident), "etape": "fin",
                               "cmd": cmd.decode(), "ok": True, "code": "ok", "duree_ms": 0}
                        b.sendall(b"\n" + RS + compact(obj) + b"\n")
                        n += 1
                        if cmd == b"json 0":
                            fin = {"v": 1, "t": "fin", "n": n, "ms": 10 + n, "cause": "commande"}
                            b.sendall(RS + compact(fin) + b"\n> ")
                            n += 1

        fil = threading.Thread(target=sonde, daemon=True)
        fil.start()
        with tempfile.TemporaryDirectory() as dossier, \
                mock.patch.object(serie_enregistre, "ouvrir_port", return_value=os.dup(a.fileno())), \
                contextlib.redirect_stdout(io.StringIO()) as out:
            code = serie_enregistre.main(["/dev/cu.usbmodem101", "banc", "capture tout", "--duree", "1",
                                          "--dossier", dossier])
            fichiers = os.listdir(dossier)
            jsonl = [f for f in fichiers if f.endswith(".jsonl")]
            self.assertEqual(len(jsonl), 1)
            self.assertTrue(jsonl[0].endswith("-banc.jsonl"))
            self.assertIn("live.log", fichiers)
            lignes = lire_texte(os.path.join(dossier, jsonl[0])).splitlines()
        self.assertEqual(code, 0)
        self.assertEqual(recu[:4], [b"\x15", b"id=1 json 1 bail 0", b"id=2 capture tout", b"id=3 json 0"])
        self.assertEqual([json.loads(l)["l"].get("id") for l in lignes], [1, 2, 3, None])
        self.assertIn("id=2 capture tout : ok", out.getvalue())


if __name__ == "__main__":
    unittest.main()
