#!/usr/bin/env python3
"""Tests de tools/json_check.py (profil hotte) : python3 -m unittest discover -s tools/tests -p test_json_check.py -v"""
import contextlib
import copy
import io
import json
import os
import sys
import tempfile
import unittest

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ICI, ".."))
import json_check  # noqa: E402

PROTOCOLE = os.path.join(ICI, "..", "..", "docs", "PROTOCOLE-JSON.md")


def tete(t, n=1, ms=10, bloc=None):
    d = {"v": 1, "t": t, "n": n, "ms": ms}
    if bloc:
        d["bloc"] = bloc
    return d


def msg(t, bloc=None, **champs):
    d = tete(t, bloc=bloc)
    d.update(champs)
    return d


# Un message valide de chaque (t, bloc) du profil hotte, dans l'ordre des champs du firmware.
VALIDES = {
    ("hello", "base"): msg(
        "hello", "base", rev=4, fw="0.1.0-1a2b3c4", fw_desc="0.1.0-1a2b3c4", date="Sep 27 2026", heure="14:02:11",
        env="sonde", build="sonde", reseau_build="aucun", puce="esp32c6", idf="v5.5.5", arduino="3.3.12",
        boot="3FA2C901", reset="mise_sous_tension", reset_n=1, up_s=12,
        session={"transport": "usb", "periode_ms": 1000, "compteurs_ms": 1000, "reseau_ms": 5000, "bail_s": 0,
                 "trames": True, "log": False},
        limites={"ligne_max": 1024, "cmd_max": 127}),
    ("hello", "identite"): msg(
        "hello", "identite", boot="3FA2C901", mac="F0F5BD012345",
        id={"fabricant": "Djoko-CLI", "produit": "Sonde hotte Haier", "serie": "HOTTE-F0F5BD012345",
            "nom": "Sonde hotte", "hw": 1, "hw_txt": "C6 SuperMini, etages v1"},
        appareil="hotte", caps=["sonde", "injection", "trames", "log"]),
    ("config", None): msg(
        "config", capture={"gpio": 6, "resol_hz": 1000000, "filtre_us": 1, "silence_us": 5000, "mode": "tout",
                           "inverse": True}),
    ("etat", "bus"): msg("etat", "bus", boot="3FA2C901", up_s=12, repos="haut", fronts=1520, derniere_ms=3,
                         receptions_s=17),
    ("etat", "capture"): msg("etat", "capture", boot="3FA2C901", up_s=12, active=True, mode="tout", debord=0,
                             rep_en_cours=0),
    ("etat", "sys"): msg(
        "etat", "sys", boot="3FA2C901", up_s=12,
        sys={"heap": 250000, "heap_min": 240000, "heap_bloc": 110000, "pile_boucle": 5200, "boucle_max_ms": 2,
             "json_perdus": 0, "json_trop_longs": 0, "rejets": 0}),
    ("compteurs", "sonde"): msg("compteurs", "sonde", receptions=203, parties=203, blocs=203, symboles=3654,
                                debord=0, rep=0, lignes_perdues=0, sautes=0, rejets=0),
    ("hb", None): msg("hb", boot="3FA2C901", up_s=12, json_perdus=0),
    ("fin", None): msg("fin", cause="commande"),
    ("reponse", None): msg("reponse", id=1, etape="fin", cmd="json 1 bail 0", ok=True, code="ok", duree_ms=9,
                           bail_s=0, up_s=12),
    ("trame", None): msg("trame", num=12, part=0, fin=True, t_us=81234567, niv0="bas", dur_us=[1500, 750, 750, 2250],
                         debord=False),
    ("injection", None): msg("injection", id=42, cmd="injecte durees 750 750", resultat="ok", niv0="bas",
                             dur_us=[750, 750], attente_us=20412, relu_us=[752, 748]),
    ("log", None): msg("log", src="capture", niv="notice", txt="[capture] echec du RMT"),
    ("reseau", "ip"): msg(
        "reseau", "ip", frais_ms=0, srp=None, adresses=[{"adr": "192.168.1.42", "type": "autre", "pref": True}],
        udp={"port": 5480, "ouvert": True, "empreinte": "630DCD29", "sessions": 1, "provisoire": False, "rx": 12,
             "rejets": 0, "rx_perdus": 0, "defis": 1, "tx": 230, "tx_perdus": 0, "tx_erreurs": 0,
             "tampons_libres": None, "tampons_min": None},
        mdns={"nom": "hotte-sonde.local"}, wifi={"connecte": True, "rssi_dbm": -58, "ip": "192.168.1.42", "pertes": 0}),
}


def brut(obj):
    return b"\x1e" + json.dumps(obj, separators=(",", ":")).encode("ascii")


def verifier(obj):
    _, errs, warns = json_check.check_line(brut(obj))
    return errs, warns


def lancer(*argv):
    sortie = io.StringIO()
    with contextlib.redirect_stdout(sortie):
        code = json_check.main(list(argv))
    return code, sortie.getvalue()


class Schemas(unittest.TestCase):
    def test_messages_valides(self):
        self.assertEqual(set(VALIDES), set(json_check.SCHEMAS))
        for cle, obj in VALIDES.items():
            with self.subTest(cle=cle):
                self.assertEqual(verifier(obj), ([], []))

    def modifie(self, cle, **champs):
        obj = copy.deepcopy(VALIDES[cle])
        for k, v in champs.items():
            if v is None and k.startswith("sans_"):
                del obj[k[5:]]
            else:
                obj[k] = v
        return verifier(obj)

    def test_champ_obligatoire_absent(self):
        errs, _ = self.modifie(("etat", "bus"), sans_fronts=None)
        self.assertTrue(any("fronts : champ obligatoire absent" in e for e in errs), errs)

    def test_champ_inconnu_avertit(self):
        errs, warns = self.modifie(("compteurs", "sonde"), nouveau=1)
        self.assertEqual(errs, [])
        self.assertTrue(any("nouveau : champ inconnu" in w for w in warns), warns)

    def test_ordre_de_tete(self):
        obj = {"t": "hb", "v": 1, "n": 1, "ms": 1, "boot": "3FA2C901", "up_s": 1, "json_perdus": 0}
        errs, _ = verifier(obj)
        self.assertTrue(any("champs de tete" in e for e in errs), errs)

    def test_bloc_inconnu(self):
        errs, _ = verifier(msg("etat", "lampe", boot="3FA2C901", up_s=1))
        self.assertTrue(any("bloc 'lampe' inconnu" in e for e in errs), errs)

    def test_trame_bornes(self):
        cle = ("trame", None)
        self.assertTrue(self.modifie(cle, dur_us=[1] * 111)[0])
        self.assertEqual(self.modifie(cle, dur_us=[1] * 110)[0], [])
        self.assertTrue(self.modifie(cle, dur_us=[0, 5])[0])
        self.assertTrue(self.modifie(cle, dur_us=[1.5])[0])
        self.assertTrue(self.modifie(cle, t_us=2**53)[0])
        self.assertEqual(self.modifie(cle, t_us=2**53 - 1)[0], [])
        self.assertTrue(self.modifie(cle, niv0="HAUT")[0])
        # Partie vide : seulement pour clore une reception ou signaler une perte.
        self.assertTrue(self.modifie(cle, dur_us=[], fin=False)[0])
        self.assertEqual(self.modifie(cle, dur_us=[], fin=True)[0], [])
        self.assertEqual(self.modifie(cle, dur_us=[], fin=False, debord=True)[0], [])
        self.assertEqual(self.modifie(cle, rep=0, sautes=3), ([], []))

    def test_reponse_coherence(self):
        cle = ("reponse", None)
        self.assertEqual(self.modifie(cle, code="accepte", suite="injection")[0], [])
        self.assertTrue(self.modifie(cle, code="accepte")[0])
        self.assertTrue(self.modifie(cle, suite="injection")[0])
        self.assertTrue(self.modifie(cle, code="usage")[0])            # ok true avec un code d'echec
        self.assertTrue(self.modifie(cle, etape="debut")[0])            # debut sans en_cours
        self.assertTrue(self.modifie(cle, sans_duree_ms=None)[0])       # fin sans duree_ms
        self.assertTrue(self.modifie(cle, sans_up_s=None)[0])           # bail_s sans up_s
        self.assertTrue(self.modifie(cle, code="differe")[0])           # code de la ScreenBar
        self.assertTrue(self.modifie(cle, cmd="x" * 41)[0])

    def test_config_coherence(self):
        cle = ("config", None)
        capture = VALIDES[cle]["capture"]
        self.assertTrue(self.modifie(cle, capture=dict(capture, resol_hz=700000))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, silence_us=999))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, silence_us=32768))[0])
        self.assertEqual(self.modifie(cle, capture=dict(capture, silence_us=32767))[0], [])
        self.assertEqual(self.modifie(cle, capture=dict(capture, resol_hz=500000, silence_us=65534))[0], [])
        self.assertTrue(self.modifie(cle, capture=dict(capture, resol_hz=500000, silence_us=65536))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, filtre_us=4))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, mode="rafale"))[0])

    def test_hello(self):
        ident = ("hello", "identite")
        self.assertTrue(self.modifie(ident, appareil="screenbar")[0])
        self.assertTrue(self.modifie(ident, caps=["sonde", "matter"])[0])
        idv = dict(VALIDES[ident]["id"], serie="HALO1-F0F5BD012345")
        self.assertTrue(self.modifie(ident, id=idv)[0])
        base = ("hello", "base")
        self.assertTrue(self.modifie(base, build="produit")[0])
        errs, warns = self.modifie(base, fw_desc="0.0.9")
        self.assertEqual(errs, [])
        self.assertTrue(any("different de fw_desc" in w for w in warns), warns)

    def test_reseau_ip(self):
        cle = ("reseau", "ip")
        ip = VALIDES[cle]
        # Wi-Fi coupe : ni RSSI, ni IP, aucune adresse ; sans cle : socket ferme, empreinte null.
        coupe = dict(ip["wifi"], connecte=False, rssi_dbm=None, ip=None)
        sans_cle = dict(ip["udp"], ouvert=False, empreinte=None, sessions=0)
        self.assertEqual(self.modifie(cle, adresses=[], wifi=coupe, udp=sans_cle), ([], []))
        self.assertTrue(self.modifie(cle, wifi=dict(ip["wifi"], connecte=False))[0])     # deconnecte avec un RSSI
        self.assertTrue(self.modifie(cle, wifi=dict(coupe, connecte=True))[0])          # connecte sans IP
        self.assertTrue(self.modifie(cle, wifi=dict(ip["wifi"], rssi_dbm=5))[0])        # RSSI positif
        self.assertTrue(self.modifie(cle, wifi=dict(ip["wifi"], ip="192.168.1"))[0])    # IPv4 incomplete
        self.assertTrue(self.modifie(cle, udp=dict(ip["udp"], sessions=3))[0])          # 2 sessions H1 au plus
        self.assertTrue(self.modifie(cle, udp=dict(ip["udp"], empreinte="630dcd29"))[0])
        self.assertTrue(self.modifie(cle, sans_mdns=None)[0])
        self.assertTrue(self.modifie(cle, srp={"nom": 5})[0])
        ident = ("hello", "identite")
        caps = ["sonde", "injection", "trames", "log", "udp", "cle", "mdns"]
        self.assertEqual(self.modifie(ident, caps=caps), ([], []))

    def test_log(self):
        self.assertTrue(self.modifie(("log", None), src="lampe")[0])
        self.assertTrue(self.modifie(("log", None), txt="x" * 192)[0])

    def test_trop_long_et_non_compact(self):
        obj = copy.deepcopy(VALIDES[("log", None)])
        obj["txt"] = "x" * 191
        obj["sautes"] = 1
        self.assertEqual(verifier(obj)[0], [])
        _, errs, _ = json_check.check_line(b"\x1e" + json.dumps(VALIDES[("fin", None)]).encode())
        self.assertTrue(any("non compacte" in e for e in errs), errs)


class Fichiers(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def ecrire(self, nom, contenu: bytes):
        p = os.path.join(self.dossier.name, nom)
        with open(p, "wb") as f:
            f.write(contenu)
        return p

    def test_capture_brute_trous(self):
        lignes = []
        for n in (1, 2, 5):  # 3 et 4 manquent
            o = copy.deepcopy(VALIDES[("hb", None)])
            o["n"] = n
            lignes.append(b"texte avant " + brut(o) + b"\r\n")
        p = self.ecrire("capture.log", b"> json 1\r\n" + b"".join(lignes))
        code, out = lancer("-q", p)
        self.assertEqual(code, 0, out)  # un trou est un avertissement
        self.assertIn("n : 1 trou(s) (2 ligne(s) perdue(s), 40.000 %)", out)
        code, out = lancer("-q", "--strict", p)
        self.assertEqual(code, 1, out)

    def test_jsonl_par_source(self):
        recs = []
        for n, de in ((7, "usb"), (100, "192.168.1.20"), (8, "usb"), (101, "192.168.1.20")):
            o = copy.deepcopy(VALIDES[("trame", None)])
            o["n"] = n
            recs.append('{"rx_ms":%d,"de":"%s","l":%s}' % (1700000000000 + n, de, json.dumps(o, separators=(",", ":"))))
        p = self.ecrire("s.jsonl", ("\n".join(recs) + "\n").encode())
        code, out = lancer("--strict", p)
        self.assertEqual(code, 0, out)
        self.assertIn("4 ligne(s) machine, 0 erreur(s)", out)
        self.assertIn("n : 0 trou(s)", out)
        # Meme contenu sous un autre nom : --jsonl l'impose.
        q = self.ecrire("s.txt", ("\n".join(recs) + "\n").encode())
        self.assertEqual(lancer("--strict", "--jsonl", q)[0], 0)
        self.assertEqual(lancer("--strict", q)[0], 0)  # lu comme brut : aucune ligne machine, aucune erreur
        self.assertIn("0 ligne(s) machine", lancer("--strict", q)[1])

    def test_jsonl_illisible_et_cle_double(self):
        bon = '{"rx_ms":1,"de":"usb","l":%s}' % json.dumps(VALIDES[("fin", None)], separators=(",", ":"))
        sans_l = '{"rx_ms":2,"de":"usb"}'
        double = '{"rx_ms":3,"de":"usb","l":{"v":1,"t":"fin","n":2,"ms":1,"cause":"bail","cause":"bail"}}'
        p = self.ecrire("x.jsonl", "\n".join([bon, sans_l, "pas du json", double]).encode() + b"\n")
        code, out = lancer(p)
        self.assertEqual(code, 1)
        self.assertIn("l absent", out)
        self.assertIn("ligne .jsonl illisible", out)
        self.assertIn("cle en double", out)

    def test_exemples_du_protocole(self):
        exemples = json_check.examples(PROTOCOLE)
        self.assertGreaterEqual(len(exemples), 15)
        code, out = lancer("--strict", "--exemples", PROTOCOLE)
        self.assertEqual(code, 0, out)
        types = {json.loads(e[1:])["t"] for e in exemples}
        self.assertTrue({"hello", "config", "etat", "compteurs", "reponse", "trame", "log", "hb", "fin"} <= types, types)

    def test_exemple_valeurs_hors_bornes(self):
        # Annonce de json_mode.cpp (logNvs) a chaque 'json 1' : spec 8.2.
        logs = [json.loads(e[1:]) for e in json_check.examples(PROTOCOLE)]
        nvs = [o for o in logs if o["t"] == "log" and "hors bornes en NVS" in o["txt"]]
        self.assertEqual(len(nvs), 1, nvs)
        self.assertEqual((nvs[0]["src"], nvs[0]["niv"]), ("sonde", "notice"))
        self.assertRegex(nvs[0]["txt"], r"^[1-9][0-9]* valeur\(s\) hors bornes en NVS : valeurs par defaut$")


if __name__ == "__main__":
    unittest.main()
