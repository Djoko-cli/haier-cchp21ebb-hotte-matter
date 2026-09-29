#!/usr/bin/env python3
"""Tests du profil produit de tools/json_check.py (spec produit 7, 9.1) :
python3 -m unittest discover -s tools/tests -p test_json_check_produit.py -v"""
import contextlib
import copy
import io
import json
import os
import re
import sys
import unittest

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ICI, ".."))
import json_check  # noqa: E402

RACINE = os.path.join(ICI, "..", "..")
PROTOCOLE = os.path.join(RACINE, "docs", "PROTOCOLE-JSON-PRODUIT.md")


def msg(t, bloc=None, **champs):
    d = {"v": 1, "t": t, "n": 1, "ms": 10}
    if bloc:
        d["bloc"] = bloc
    d.update(champs)
    return d


BOOT = {"boot": "3FA2C901", "up_s": 12}
AUTOMATE = {k: lo for k, (lo, _) in json_check.AUTOMATE_BORNES.items()}
AUTOMATE.update(delai_moteur_ms=3000, lissage_calme_ms=700, lissage_plafond_ms=3000)
SURV = {k: lo for k, (lo, _) in json_check.SURVEILLANCE_BORNES.items()}
SURV.update(temp_alerte_c=70, temp_hyst_c=5)

# Un message valide de chaque (t, bloc) du profil produit, dans l'ordre des champs du firmware.
VALIDES = {
    ("hello", "base"): msg(
        "hello", "base", rev=4, fw="0.1.0-1a2b3c4", fw_desc="0.1.0-1a2b3c4", date="Sep 29 2026", heure="20:02:11",
        env="produit", build="produit", reseau_build="thread", puce="esp32c6", idf="v5.5.5", arduino="3.3.12",
        boot="3FA2C901", reset="mise_sous_tension", reset_n=1, up_s=12,
        session={"transport": "udp", "periode_ms": 2000, "compteurs_ms": 0, "reseau_ms": 30000, "bail_s": 30,
                 "trames": False, "log": False},
        limites={"ligne_max": 1024, "cmd_max": 127}),
    ("hello", "identite"): msg(
        "hello", "identite", boot="3FA2C901", mac="F0F5BD012345",
        id={"fabricant": "Djoko-CLI", "produit": "Module hotte Haier", "serie": "HOTTE-F0F5BD012345", "nom": "Hotte",
            "hw": 1, "hw_txt": "C6 SuperMini, pilote simule"},
        appareil="hotte", caps=["hotte", "matter", "udp", "cle", "log", "trames_d", "alim", "thermique", "essai", "simule"],
        thermique={"temp_max_pose_c": None}),
    ("config", None): msg(
        "config", automate=AUTOMATE, surveillance=SURV, ligne={"pilote": "simule", "mode": "repete", "annexe": False},
        simu={"latence_ms": 150, "periode_ms": 500, "trame_ms": 60, "annexe_ms": 200, "prolongee_ms": 900000,
              "fin": "eteinte", "inconnues": "rien", "refuse": False, "hors_service": False}),
    ("etat", "hotte"): msg("etat", "hotte", **BOOT, marche="armee", moteur=2, lampe=True, confiance="confirme",
                           source="fil", age_ms=120),
    ("etat", "automate"): msg("etat", "automate", **BOOT, mode="repete", annexe=False, pilote=True,
                              ventilo={"cible": "eteint", "origine": "app"}, lampe=None,
                              appui={"touche": "v2", "essai": 0, "depuis_ms": 40}, delai_moteur_ms=0,
                              marche_autorisee=False),
    ("etat", "matter"): msg("etat", "matter", **BOOT, demarre=True, mis_en_service=True, fabriques=1, abonnements=2,
                            ignore_ms=0, maxint_s=20, role_demarrage="routeur", tx_dbm=20, derniere=True, ecritures=14,
                            reflets=9, verrou_occupe=0, ignores=0),
    ("etat", "thermique"): msg("etat", "thermique", **BOOT, temp_c=45, temp_max_c=46, temp_max_pose_c=61,
                               alerte=False, alertes=0, lectures_ratees=0, seuil_c=70),
    ("etat", "alim"): msg("etat", "alim", **BOOT, hotte=None, module={"mv": 4460, "min_s_mv": 4390, "min_mv": 4380,
                                                                       "alerte": False}, alertes_actives=False),
    ("etat", "sys"): msg("etat", "sys", **BOOT, sys={"heap": 120000, "heap_min": 90000, "heap_bloc": 40000,
                                                     "pile_boucle": 3100, "boucle_max_ms": 12, "json_perdus": 0,
                                                     "json_trop_longs": 0, "rejets": 0}),
    ("compteurs", "hotte"): msg(
        "compteurs", "hotte", appuis_panneau=12, appuis_module=30, reussies=14, annulees=1, remplacees=2, abandons=0,
        echecs={"non_confirme": 0, "sans_lecture": 0, "collision": 1, "garde": 0, "pilote": 0, "duree": 0},
        nouveaux_essais=1, transitions_inconnues=0, anomalies=0, marche_inconnue=0, prolongee_perimee=0,
        lignes_muettes=0, actions_perdues=0),
    ("compteurs", "alim"): msg("compteurs", "alim", hotte_passages=0, module_passages=0),
    ("compteurs", "radio"): msg("compteurs", "radio", role="router", tx_dbm=20, sensibilite_dbm=-120,
                                lien={"avec": "routeur", "rssi_moyen_dbm": -71, "lq_in": 3, "lq_out": 3},
                                tx_total=812, tx_retry=20, tx_echecs=0, changements_parent=0, changements_role=2),
    ("hb", None): msg("hb", boot="3FA2C901", up_s=12, json_perdus=0),
    ("fin", None): msg("fin", cause="bail"),
    ("reponse", None): msg("reponse", id=42, etape="fin", cmd="hotte ventilo v2", ok=True, code="accepte", duree_ms=1,
                           suite="sequence"),
    ("log", None): msg("log", src="hotte", niv="notice", txt="[hotte] sequence ventilo : ok (1 appuis, 820 ms)"),
    ("reseau", "ip"): msg(
        "reseau", "ip", frais_ms=1200, srp={"nom": "ESP-HOTTE-012345"},
        adresses=[{"adr": "fd11:22::1a2b:3c4d:5e6f:7081", "type": "omr", "pref": True}],
        udp={"port": 5480, "ouvert": True, "empreinte": "630DCD29", "sessions": 1, "provisoire": False, "rx": 12,
             "rejets": 0, "rx_perdus": 0, "defis": 1, "tx": 230, "tx_perdus": 0, "tx_erreurs": 0, "tampons_libres": 51,
             "tampons_min": 38, "rafale_tx": 0}),
    ("hotte", None): msg("hotte", avant={"marche": "armee", "moteur": 0, "lampe": False},
                         apres={"marche": "armee", "moteur": 2, "lampe": False}, origine="panneau", source="fil",
                         confiance="confirme"),
    ("sequence", None): msg("sequence", id=42, origine="app", sujet="ventilo", issue="ok", cause=None, duree_ms=820,
                            appuis=1),
    ("alerte", None): msg("alerte", sujet="temperature", etape="debut", valeur=71, seuil=70, unite="c"),
    ("trame_d", None): msg("trame_d", t_ms=1234, origine="carte", sens="vers_panneau", octets="18"),
}


def verifier(obj):
    brut = b"\x1e" + json.dumps(obj, separators=(",", ":")).encode("ascii")
    _, errs, warns = json_check.check_line(brut, "produit")
    return errs, warns


def lancer(*argv):
    sortie = io.StringIO()
    with contextlib.redirect_stdout(sortie):
        code = json_check.main(list(argv))
    return code, sortie.getvalue()


class SchemasProduit(unittest.TestCase):
    def test_messages_valides(self):
        self.assertEqual(set(VALIDES), set(json_check.SCHEMAS_PRODUIT))
        for cle, obj in VALIDES.items():
            errs, warns = verifier(obj)
            self.assertEqual((errs, warns), ([], []), cle)

    def test_profil_sonde_ignore_le_produit(self):
        # Sous le profil de la sonde, un bloc du produit est inconnu.
        brut = b"\x1e" + json.dumps(VALIDES[("etat", "hotte")], separators=(",", ":")).encode()
        _, errs, _ = json_check.check_line(brut)
        self.assertTrue(any("bloc 'hotte' inconnu" in e for e in errs), errs)

    def test_build_et_caps(self):
        o = copy.deepcopy(VALIDES[("hello", "base")])
        o["build"] = "sonde"
        self.assertTrue(verifier(o)[0])
        o = copy.deepcopy(VALIDES[("hello", "identite")])
        o["caps"].append("injection")  # jamais dans le produit (7.3)
        self.assertTrue(any("injection" in e for e in verifier(o)[0]))

    def test_sequence_coherence(self):
        for issue, cause, ok in (("ok", None, True), ("ok", "garde", False), ("echec", "collision", True),
                                 ("echec", None, False), ("echec", "duree", False), ("abandon", "duree", True),
                                 ("abandon", "sans_lecture", True), ("abandon", "garde", False),
                                 ("annulee", None, True), ("remplacee", None, True)):
            o = copy.deepcopy(VALIDES[("sequence", None)])
            o["issue"], o["cause"] = issue, cause
            self.assertEqual(not verifier(o)[0], ok, (issue, cause))
        o = copy.deepcopy(VALIDES[("sequence", None)])
        o["id"] = None  # ordre de Maison, ou d'une autre session
        self.assertEqual(verifier(o), ([], []))

    def test_reponse_accepte_suite_sequence(self):
        o = copy.deepcopy(VALIDES[("reponse", None)])
        del o["suite"]
        self.assertTrue(any("accepte va avec suite sequence" in e for e in verifier(o)[0]))
        o["suite"] = "injection"  # celle de la sonde
        self.assertTrue(verifier(o)[0])

    def test_alerte_et_trame_d(self):
        o = copy.deepcopy(VALIDES[("alerte", None)])
        o["unite"] = "mv"
        self.assertTrue(any("attendu c" in e for e in verifier(o)[0]))
        o = copy.deepcopy(VALIDES[("trame_d", None)])
        o["sens"] = "vers_carte"
        self.assertTrue(any("trame_d : origine carte" in e for e in verifier(o)[0]))
        o = copy.deepcopy(VALIDES[("trame_d", None)])
        o["octets"] = "00" * 9
        self.assertTrue(verifier(o)[0])

    def test_config_bornes(self):
        o = copy.deepcopy(VALIDES[("config", None)])
        o["automate"]["confirmation_ms"] = 199
        self.assertTrue(any("automate.confirmation_ms 199 hors bornes" in e for e in verifier(o)[0]))
        o = copy.deepcopy(VALIDES[("config", None)])
        o["automate"]["lissage_calme_ms"] = 2000
        o["automate"]["lissage_plafond_ms"] = 1000
        self.assertTrue(any("lissage_calme_ms au-dela" in e for e in verifier(o)[0]))
        o = copy.deepcopy(VALIDES[("config", None)])
        o["surveillance"]["temp_alerte_c"] = 90
        self.assertTrue(verifier(o)[0])

    def test_radio_inconnue(self):
        # OpenThread pas encore lu : role null, rien d'autre.
        self.assertEqual(verifier(msg("compteurs", "radio", role=None)), ([], []))


class BornesProduit(unittest.TestCase):
    def bornes(self, chemin, n):
        with open(os.path.join(RACINE, chemin), encoding="utf-8") as f:
            texte = f.read()
        c = {m[0]: (int(m[1]), int(m[2])) for m in re.findall(r'\{"(\w+)", &Params::\w+, (\d+), (\d+)\}', texte)}
        self.assertEqual(len(c), n)
        return c

    def test_memes_bornes_que_le_firmware(self):
        self.assertEqual(self.bornes("src/produit/hotte_etat.cpp", 12), json_check.AUTOMATE_BORNES)
        self.assertEqual(self.bornes("src/produit/surveillance.cpp", 6), json_check.SURVEILLANCE_BORNES)


class ExemplesProduit(unittest.TestCase):
    def test_exemples_du_protocole(self):
        exemples = json_check.examples(PROTOCOLE)
        self.assertGreaterEqual(len(exemples), 20)
        code, out = lancer("--strict", "--profil", "produit", "--exemples", PROTOCOLE)
        self.assertEqual(code, 0, out)
        types = {json.loads(e[1:])["t"] for e in exemples}
        self.assertTrue({"hello", "config", "etat", "compteurs", "reponse", "hotte", "sequence", "alerte", "trame_d",
                         "reseau", "log", "hb", "fin"} <= types, types)


if __name__ == "__main__":
    unittest.main()
