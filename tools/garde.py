#!/usr/bin/env python3
"""Garde des vrais identifiants (depot public, anonyme des le premier commit).

  python3 tools/garde.py [fichier ...]

Lit garde.local.txt a la racine du depot (ignore par git) : les vrais identifiants a ne jamais publier,
un par ligne (MAC des cartes, code d'appairage, identite, noms) ; les lignes vides et celles qui
commencent par « # » sont ignorees. Depuis un worktree git, ou ce fichier ignore n'est pas, la garde prend
celui du depot principal. Echoue (code 1) si l'un des identifiants apparait dans un fichier suivi par git
ou dans l'un des fichiers donnes (le crochet pre-push y passe les messages des commits pousses), sans tenir
compte de la casse ni des separateurs « : », « - », « . », espace et « 0x ». Un identifiant hexadecimal long
(16 chiffres ou plus) est aussi cherche dans les seuls chiffres hexadecimaux du fichier, pour le trouver
ecrit en tableau C (« 0x81, 0x42, ... »). Sans garde.local.txt, la garde echoue aussi : le crochet ne
laisse rien partir sans elle. Copie adaptee de la garde de maillage-zigbee (outils/garde.py).
"""
import os
import re
import subprocess
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
DEPOT = os.path.normpath(os.path.join(ICI, ".."))


def normaliser(texte):
    return re.sub(r"0x|[:.\- ]", "", texte, flags=re.IGNORECASE).upper()


def chiffres_hexa(texte):
    """Les seuls chiffres hexadecimaux du texte, en majuscules, sans les prefixes 0x."""
    return re.sub(r"[^0-9A-F]", "", re.sub(r"0X", "", texte.upper()))


def hexa_long(identifiant):
    return len(identifiant) >= 16 and re.fullmatch(r"[0-9A-F]+", identifiant) is not None


def lire_identifiants(chemin):
    with open(chemin, encoding="utf-8") as f:
        lignes = [l.strip() for l in f]
    return sorted({normaliser(l) for l in lignes if l and not l.startswith("#")} - {""})


def fichiers_suivis(depot):
    sortie = subprocess.run(["git", "-C", depot, "ls-files", "-z"], capture_output=True, check=True).stdout
    return [os.path.join(depot, f) for f in sortie.decode("utf-8").split("\0") if f]


def trouver(chemins, identifiants):
    """[(fichier, identifiant)] pour chaque identifiant present dans l'un des fichiers."""
    trouves = []
    for chemin in chemins:
        if not os.path.isfile(chemin):
            continue
        with open(chemin, "rb") as g:
            brut = g.read().decode("utf-8", "replace")
        texte, hexa = normaliser(brut), chiffres_hexa(brut)
        trouves += [(chemin, i) for i in identifiants if i in texte or (hexa_long(i) and i in hexa)]
    return trouves


def emplacements(depot):
    """garde.local.txt du depot, puis celui du depot principal (depuis un worktree)."""
    places = [os.path.join(depot, "garde.local.txt")]
    r = subprocess.run(["git", "-C", depot, "rev-parse", "--path-format=absolute", "--git-common-dir"],
                       capture_output=True, text=True)
    if r.returncode == 0 and r.stdout.strip():
        places.append(os.path.join(os.path.dirname(r.stdout.strip()), "garde.local.txt"))
    return places


def main(arguments, depot=DEPOT):
    chemin = next((p for p in emplacements(depot) if os.path.exists(p)), None)
    if not chemin:
        print("garde : garde.local.txt absent : rien ne part sans la liste des vrais identifiants")
        return 1
    identifiants = lire_identifiants(chemin)
    trouves = trouver(fichiers_suivis(depot) + list(arguments), identifiants)
    for f, i in trouves:
        print(f"garde : identifiant reel dans {os.path.relpath(f, depot)} ({i[:2]}...{i[-2:]})")
    if trouves:
        return 1
    print(f"garde : {len(identifiants)} identifiants, aucun dans les fichiers verifies")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
