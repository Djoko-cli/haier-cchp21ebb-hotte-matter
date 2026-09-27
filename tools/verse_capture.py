#!/usr/bin/env python3
"""Verse une capture de reference dans captures/ (spec 9 ; journal, etapes 5 et 5b).

  python3 tools/verse_capture.py <capture.jsonl> <nom> [--force]

Copie la capture (.jsonl ecrit dans logs/ par serie_enregistre.py ou
hotte_udp.py enregistre) telle quelle vers captures/<nom>.jsonl, et ajoute sa
provenance au tableau de captures/README.md. Refuse (code 1), sans rien
ecrire :
  - une ligne qui contient un champ 'cle', 'ssid' ou 'mdp' ; un champ 'wifi'
    autre que le bloc wifi de 'reseau' 'ip' (connecte, rssi_dbm, ip, pertes :
    sans SSID) ; la reponse a 'wifi <ssid> <mdp>' (son cmd porte le SSID) ;
  - une ligne qui contient une chaine de 64 caracteres hexadecimaux (cle H1,
    alea de 'json cle nouvelle', cle Wi-Fi de 64 hexa) ;
  - une capture vide, ou que 'json_check.py --jsonl' refuse ;
  - un nom deja verse, sauf avec --force.
Une ligne refusee n'est jamais affichee : seulement son numero et la raison.

Codes de sortie : 0 versee, 1 refusee, 2 usage (nom, fichier illisible).

Exemple :
  python3 tools/verse_capture.py logs/2026-10-04-1412-veille.jsonl etape5-veille
"""
import argparse
import datetime
import json
import os
import re
import shutil
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import json_check  # noqa: E402

CAPTURES = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "captures")
NOM = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]*$")
HEXA64 = re.compile(r"[0-9A-Fa-f]{64}")
INTERDITS = ("cle", "ssid", "mdp")
WIFI_RESEAU = {"connecte", "rssi_dbm", "ip", "pertes"}  # reseau.ip.wifi (docs/PROTOCOLE-JSON.md)


class _Paires(list):
    """Objet JSON lu en liste de paires (cles en double gardees)."""


def _raisons(v, out):
    """Ajoute a out les raisons de refus trouvees dans la valeur v, a toute profondeur."""
    if isinstance(v, _Paires):
        champs = dict(v)
        ip = champs.get("t") == "reseau" and champs.get("bloc") == "ip"
        for k, x in v:
            if k in INTERDITS:
                r = f"champ '{k}'"
            elif k == "wifi" and not (ip and isinstance(x, _Paires) and {c for c, _ in x} <= WIFI_RESEAU):
                r = "champ 'wifi'"
            elif k == "cmd" and isinstance(x, str) and x.split()[:1] == ["wifi"]:
                r = "reponse a 'wifi' (SSID dans cmd)"
            else:
                r = None
            if r and r not in out:
                out.append(r)
            _raisons(k, out)
            _raisons(x, out)
    elif isinstance(v, list):
        for x in v:
            _raisons(x, out)
    elif isinstance(v, str) and HEXA64.search(v) and "chaine de 64 hexa" not in out:
        out.append("chaine de 64 hexa")


def refus(ligne):
    """Raisons de refuser une ligne .jsonl (liste vide : ni cle, ni SSID, ni mot de passe vus)."""
    texte = ligne.decode("utf-8", "replace") if isinstance(ligne, bytes) else ligne
    out = []
    try:
        _raisons(json.loads(texte, object_pairs_hook=_Paires), out)
    except ValueError:
        pass  # illisible : json_check.py la refusera
    if HEXA64.search(texte) and "chaine de 64 hexa" not in out:
        out.append("chaine de 64 hexa")
    return out


def provenance(chemin, nom):
    """Ligne du tableau de captures/README.md (capture deja acceptee par json_check.py)."""
    lignes, debut, transports, fw, boot = 0, None, set(), None, None
    with open(chemin, "rb") as f:
        for brut in f:
            if not brut.strip():
                continue
            rec = json.loads(brut)
            lignes += 1
            if debut is None:
                debut = rec["rx_ms"]
            transports.add("usb" if rec["de"] == "usb" else "udp")
            l = rec["l"]
            if fw is None and l.get("t") == "hello" and l.get("bloc") == "base":
                fw = l.get("fw")
            if boot is None and isinstance(l.get("boot"), str):
                boot = l["boot"]
    quand = datetime.datetime.fromtimestamp(debut / 1000).strftime("%Y-%m-%d %H:%M:%S")
    return "| `%s.jsonl` | `%s` | %s | %d | %s | %s | %s |" % (
        nom, os.path.basename(chemin), quand, lignes, ", ".join(sorted(transports)), fw or "?", boot or "?")


def noter(readme, nom, ligne):
    """Ajoute la ligne en fin de README (le tableau de provenance est sa derniere section)."""
    with open(readme, encoding="utf-8") as f:
        lignes = f.read().splitlines()
    tete = "| `%s.jsonl` |" % nom
    lignes = [x for x in lignes if not x.startswith(tete)] + [ligne]  # --force : remplacee
    with open(readme, "w", encoding="utf-8") as f:
        f.write("\n".join(lignes) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description="Verse une capture .jsonl dans captures/ (ni cle, ni SSID, ni mot de passe).")
    ap.add_argument("capture", help="capture .jsonl (logs/...)")
    ap.add_argument("nom", help="nom dans captures/, sans .jsonl (ex. etape5-veille)")
    ap.add_argument("--force", action="store_true", help="remplacer une capture deja versee sous ce nom")
    args = ap.parse_args(argv)
    if not NOM.match(args.nom):
        print(f"nom refuse : {args.nom!r} (lettres, chiffres, - et _, sans .jsonl)", file=sys.stderr)
        return 2
    readme = os.path.join(CAPTURES, "README.md")
    dest = os.path.join(CAPTURES, args.nom + ".jsonl")
    if not os.path.isfile(readme):
        print(f"{readme} absent : dossier des captures introuvable", file=sys.stderr)
        return 2
    try:
        with open(args.capture, "rb") as f:
            lignes = f.read().split(b"\n")
    except OSError as e:
        print(f"lecture impossible : {e}", file=sys.stderr)
        return 2
    if os.path.exists(dest) and not args.force:
        print(f"refusee : {dest} existe deja (--force pour la remplacer) ; rien n'est verse")
        return 1
    mauvaises = 0
    for k, ligne in enumerate(lignes, 1):
        raisons = refus(ligne)
        if raisons:
            mauvaises += 1
            print(f"{args.capture}:{k} : {', '.join(raisons)}")
    if mauvaises:
        print(f"refusee : {mauvaises} ligne(s) avec une cle, un SSID ou un mot de passe ; rien n'est verse")
        return 1
    if not any(x.strip() for x in lignes):
        print("refusee : capture vide ; rien n'est verse")
        return 1
    if json_check.main(["--jsonl", args.capture]) != 0:
        print("refusee : json_check.py refuse la capture ; rien n'est verse")
        return 1
    shutil.copyfile(args.capture, dest)
    ligne = provenance(args.capture, args.nom)
    noter(readme, args.nom, ligne)
    print(f"versee : {dest} ; provenance ajoutee a {readme} :")
    print(ligne)
    print(f"a committer : git add captures/{args.nom}.jsonl captures/README.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())
