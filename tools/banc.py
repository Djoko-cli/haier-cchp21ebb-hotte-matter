#!/usr/bin/env python3
"""Verdicts du banc de validation (docs/SPEC-RECONNAISSANCE.md section 10,
procedure : docs/BANC.md) : une capture de la sonde, prise pendant que le
generateur emettait un motif, contre le contenu connu de ce motif
(tools/signaux.py, identique a src/motifs.cpp).

Usage :
  python3 tools/banc.py <capture> <motif> [--decalage-us N] [--n N] [--silence us] [--tol-us T]

<capture> : fichier .jsonl (serie_enregistre.py, hotte_udp.py enregistre) ou
capture serie brute. <motif> : un nom de signaux.MOTIFS, lance par
'motif <nom>' sur le generateur, qui numerote ses trames a partir de 0.

Critere 1, trames decodees au bit pres : les trames de la capture (une par
reception de la sonde, ou redecoupees a chaque repos >= --silence us) sont
decodees par decodeurs.auto, SANS forcer le decodeur. Leurs contenus (octets
pour l'UART, messages de bits pour la distance d'impulsion) sont mis bout a
bout, dans l'ordre ; une trame en erreur coupe la suite. On y reconnait les
trames du motif : l'index d'une trame est retrouve par son contenu (modulo
256, ou 8 pour krona) et deroule dans l'ordre. Une trame que la sonde a coupee
en deux (krona) est reconnue a cheval. Taux = index reconnus dans 0..n-1 / n
(n = --n, defaut 1000, rafale 1).

Critere 2, durees : chaque trame de la capture qui porte seule une trame
reconnue est comparee palier par palier a signaux.trame_motif, sans les
paliers de repos de tete et de queue (fondus dans le repos). Les durees
hautes mesurees sont corrigees de -N us et les basses de +N us (N =
--decalage-us : asymetrie de la chaine generateur, ligne, etage d'ecoute ;
positif si les niveaux hauts sont allonges). Ecart = duree corrigee -
nominale : moyenne et maximum en valeur absolue, par niveau. OK si l'ecart
maximal est <= --tol-us (2 us) et si chaque trame comparee a les paliers
attendus. Decalage suggere = N + (moyenne des hauts - moyenne des bas) / 2.

Rafale (critere 3) : pas de contenu ; une rafale est complete si sa reception
a les kRafaleSegs - 1 paliers attendus (le premier, haut, se fond dans le
repos). Pertes : receptions incompletes (partie manquante), parties marquees
debord, et hausses de debord, lignes_perdues et sautes entre le premier et le
dernier message 'compteurs' de la sonde dans la capture. Aucune perte, ou une
perte signalee (debord ou compteurs) : OK ; perte non signalee : ECHEC.

Code de sortie : 0 si les criteres sont remplis (1 et 2 ; rafale : 3, et 2
s'il y a une rafale complete) ; 1 sinon, ou si la capture n'a aucune trame ;
2 si la capture est illisible ou les arguments faux.
"""
import argparse
import sys
from dataclasses import dataclass, field

import analyse
import capture_fmt
import decodeurs
import signaux

TOL_US = 2.0
N_DEFAUT = 1000
COMPTEURS_PERTES = ("debord", "lignes_perdues", "sautes")


def n_defaut(nom: str) -> int:
    return 1 if nom == "rafale" else N_DEFAUT


def decodeur_attendu(nom: str) -> tuple[str, dict | None]:
    """(nom du decodeur, parametres) ; parametres None : seul le nom compte (T estime)."""
    if nom == "wtc":
        return "distance_impulsion", None
    if nom == "krona":
        return "uart", {"bauds": 500, "inverse": True}
    if nom.startswith("uart"):
        return "uart", {"bauds": int(nom[4:].removesuffix("inv")), "inverse": nom.endswith("inv")}
    return "", None  # rafale : pas de decodage


def jetons_attendus(nom: str, index: int) -> tuple:
    """Contenu de la trame index dans l'unite du decodeur : octets, ou messages de bits (wtc)."""
    o = signaux.octets_motif(nom, index)
    if nom == "wtc":
        return (format((o[0] << 8) | o[1], "016b"), format(o[2], "08b"))
    return tuple(o)


def jetons(d: decodeurs.Decodage) -> list | None:
    """Contenu d'une trame decodee ; None si son decodage a des erreurs."""
    if d.erreurs:
        return None
    return list(d.octets) if d.nom == "uart" else d.bits.split()


def reconnaitre(unites: list, nom: str) -> list[tuple[int, int | None]]:
    """(index, unite) des trames du motif reconnues, dans l'ordre.

    unites : jetons de chaque trame de la capture (None : decodage en erreur,
    coupe la suite). unite : rang de la trame de la capture qui porte seule
    la trame reconnue, None si elle est a cheval ou partagee."""
    per = 8 if nom == "krona" else 256
    attendus = {jetons_attendus(nom, r): r for r in range(per)}
    lg = len(jetons_attendus(nom, 0))
    flux: list = []
    taille = []
    for u, jts in enumerate(unites):
        taille.append(len(jts) if jts is not None else 0)
        if jts is None:
            flux.append(None)
        else:
            flux.extend((j, u) for j in jts)
    out = []
    dernier = -1
    p = 0
    while p + lg <= len(flux):
        fen = flux[p:p + lg]
        r = None if None in fen else attendus.get(tuple(j for j, _ in fen))
        if r is None:
            p += 1
            continue
        index = dernier + 1 + (r - dernier - 1) % per
        us = {u for _, u in fen}
        seule = us.pop() if len(us) == 1 else None
        out.append((index, seule if seule is not None and taille[seule] == lg else None))
        dernier = index
        p += lg
    return out


def sans_repos(segs, repos_haut: bool) -> list[tuple[bool, int]]:
    """Segments fusionnes, sans le palier de repos de tete ni celui de queue."""
    s = signaux.fusionner(list(segs))
    if s and s[0][0] == repos_haut:
        s = s[1:]
    if s and s[-1][0] == repos_haut:
        s = s[:-1]
    return s


def compteurs_sonde(objets) -> dict | None:
    """Hausse de debord, lignes_perdues et sautes entre le premier et le dernier
    message compteurs (bloc sonde) ; None s'il y en a moins de deux."""
    cs = [o for o in objets if o.get("t") == "compteurs" and o.get("bloc") == "sonde"]
    if len(cs) < 2:
        return None
    return {k: int(cs[-1].get(k, 0)) - int(cs[0].get(k, 0)) for k in COMPTEURS_PERTES}


@dataclass
class Verdict:
    motif: str
    n: int
    tol_us: float
    decalage_us: float
    receptions: int = 0
    incompletes: int = 0
    avec_debord: int = 0
    trames: int = 0                                # trames de la capture analysees
    hypothese: decodeurs.Decodage | None = None    # meilleure hypothese de auto (None : rafale)
    hypothese_ok: bool = True
    reconnues: list[int] = field(default_factory=list)  # index reconnus dans 0..n-1, tries
    comparees: int = 0
    durees: int = 0
    formes_differentes: int = 0
    ecart_moy_haut: float | None = None
    ecart_max_haut: float | None = None
    ecart_moy_bas: float | None = None
    ecart_max_bas: float | None = None
    decalage_suggere: float | None = None
    compteurs: dict | None = None

    @property
    def taux(self) -> float:
        return len(self.reconnues) / self.n

    @property
    def manquantes(self) -> list[int]:
        vues = set(self.reconnues)
        return [i for i in range(self.n) if i not in vues]

    @property
    def ecart_max(self) -> float | None:
        m = [x for x in (self.ecart_max_haut, self.ecart_max_bas) if x is not None]
        return max(m) if m else None

    @property
    def pertes(self) -> str:
        """'aucune', 'signalee' (debord ou compteurs) ou 'non signalee'."""
        hausse = bool(self.compteurs) and any(v > 0 for v in self.compteurs.values())
        if self.avec_debord or hausse:
            return "signalee"
        return "non signalee" if self.incompletes else "aucune"

    @property
    def ok1(self) -> bool:
        return self.hypothese_ok and len(self.reconnues) == self.n

    @property
    def ok2(self) -> bool:
        return self.comparees > 0 and self.formes_differentes == 0 and self.ecart_max <= self.tol_us

    @property
    def ok3(self) -> bool:
        return self.pertes == "signalee" or (self.pertes == "aucune" and len(self.reconnues) == self.n)

    @property
    def ok(self) -> bool:
        if self.motif == "rafale":
            return self.ok3 and (self.ok2 or self.comparees == 0)
        return self.ok1 and self.ok2


def evaluer(chemin, nom: str, n: int | None = None, decalage_us: float = 0.0, silence_us: int | None = None,
            tol_us: float = TOL_US) -> Verdict:
    """Verdict d'une capture pour le motif nom (OSError si elle est illisible)."""
    repos_haut = signaux.repos_haut_motif(nom)
    v = Verdict(motif=nom, n=n or n_defaut(nom), tol_us=tol_us, decalage_us=decalage_us)
    trames, _, recs = analyse.charger(chemin, "haut" if repos_haut else "bas", silence_us)
    v.compteurs = compteurs_sonde(capture_fmt.lire(chemin))
    v.receptions = len(recs)
    v.incompletes = sum(1 for r in recs if not r.complete)
    v.avec_debord = sum(1 for r in recs if r.debord)
    segs = [t for _, t in trames]
    v.trames = len(segs)

    if nom == "rafale":
        attendu = sans_repos(signaux.trame_motif(nom, 0), repos_haut)
        paires = []
        for u, s in enumerate(segs):
            if len(s) == len(attendu) and s[0][0] == attendu[0][0]:
                paires.append((len(paires), u))
    else:
        if not segs:
            return v
        hyp = decodeurs.auto(segs)[0]
        v.hypothese = hyp
        nom_att, params_att = decodeur_attendu(nom)
        v.hypothese_ok = hyp.nom == nom_att and (params_att is None or hyp.params == params_att)
        paires = reconnaitre([jetons(d) for d in decodeurs.appliquer(hyp, segs)], nom)
    v.reconnues = sorted({i for i, _ in paires if i < v.n})

    somme = {True: 0.0, False: 0.0}
    nombre = {True: 0, False: 0}
    pire = {True: 0.0, False: 0.0}
    for index, u in paires:
        if u is None or index >= v.n:
            continue
        attendu = sans_repos(signaux.trame_motif(nom, index), repos_haut)
        mesure = segs[u]
        if len(mesure) != len(attendu) or any(a[0] != b[0] for a, b in zip(mesure, attendu)):
            v.formes_differentes += 1
            continue
        v.comparees += 1
        for (haut, d), (_, nominal) in zip(mesure, attendu):
            e = (d - decalage_us if haut else d + decalage_us) - nominal
            somme[haut] += e
            nombre[haut] += 1
            pire[haut] = max(pire[haut], abs(e))
    v.durees = nombre[True] + nombre[False]
    if nombre[True]:
        v.ecart_moy_haut, v.ecart_max_haut = somme[True] / nombre[True], pire[True]
    if nombre[False]:
        v.ecart_moy_bas, v.ecart_max_bas = somme[False] / nombre[False], pire[False]
    if nombre[True] and nombre[False]:
        v.decalage_suggere = decalage_us + (v.ecart_moy_haut - v.ecart_moy_bas) / 2
    return v


def fmt(x: float) -> str:
    return f"{x:g}"


def ok(b: bool) -> str:
    return "OK" if b else "ECHEC"


def afficher(v: Verdict, chemin) -> None:
    print(f"capture {chemin} : {v.receptions} receptions ({v.incompletes} incompletes, {v.avec_debord} avec debord), "
          f"{v.trames} trames analysees")
    repos = "haut" if signaux.repos_haut_motif(v.motif) else "bas"
    print(f"motif {v.motif} : repos {repos}, {v.n} trames attendues")
    if v.motif == "rafale":
        completes = f"critere 3 : {len(v.reconnues)} / {v.n} rafales completes"
        debit = 1000000 // signaux.trame_motif("rafale", 0)[0][1]
        if v.pertes == "aucune" and v.ok3:
            print(f"{completes} ; aucune perte : OK ({debit} fronts/s tenus)")
        elif v.pertes == "aucune":
            print(f"{completes} ; aucune perte signalee, mais rafale absente ou deformee : ECHEC")
        elif v.pertes == "signalee":
            print(f"{completes} ; perte signalee (debord ou compteurs) : OK, debit maximal < {debit} fronts/s")
        else:
            print(f"{completes} ; perte non signalee (reception incomplete sans debord) : ECHEC")
    else:
        nom_att, params_att = decodeur_attendu(v.motif)
        att = analyse.libelle(decodeurs.Decodage(nom_att, params_att, b"", "", 0, 0)) if params_att else nom_att
        print(f"decodeur (auto, sans forcer) : {analyse.resume(v.hypothese)} ; attendu : {att} : "
              + ("OK" if v.hypothese_ok else "INATTENDU"))
        print(f"critere 1 : {len(v.reconnues)} / {v.n} trames decodees au bit pres ({100 * v.taux:.1f} %) : {ok(v.ok1)}")
        manq = v.manquantes
        if manq:
            suite = f" ... (+{len(manq) - 20})" if len(manq) > 20 else ""
            print("  manquantes : " + ", ".join(str(i) for i in manq[:20]) + suite)
    print(f"durees : {v.comparees} trames comparees ({v.durees} durees), decalage {fmt(v.decalage_us)} us, "
          f"{v.formes_differentes} formes differentes")
    if v.durees:
        parts = []
        for nom, moy, pire in (("hautes", v.ecart_moy_haut, v.ecart_max_haut), ("basses", v.ecart_moy_bas, v.ecart_max_bas)):
            if moy is not None:
                parts.append(f"{nom} : ecart moyen {moy:+.2f} us, max {fmt(pire)} us")
        sugg = f" ; decalage suggere {v.decalage_suggere:+.1f} us" if v.decalage_suggere is not None else ""
        print("  " + " ; ".join(parts) + sugg)
        print(f"critere 2 : ecart max {fmt(v.ecart_max)} us <= {fmt(v.tol_us)} us : {ok(v.ok2)}")
    else:
        print("critere 2 : aucune trame comparee : ECHEC")
    if v.compteurs is None:
        print("compteurs de la sonde : moins de deux messages dans la capture, pertes cote sonde inconnues")
    else:
        print("compteurs de la sonde pendant la capture : "
              + ", ".join(f"{k} {v.compteurs[k]:+d}" for k in COMPTEURS_PERTES))
    print(f"verdict : {ok(v.ok)}")


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="banc.py", description="Verdicts du banc : capture de la sonde contre un motif.")
    p.add_argument("capture")
    p.add_argument("motif", choices=signaux.MOTIFS)
    p.add_argument("--decalage-us", type=float, default=0.0,
                   help="asymetrie a corriger : hauts - N us, bas + N us (defaut 0)")
    p.add_argument("--n", type=int, help="trames emises par le generateur (defaut 1000, rafale 1)")
    p.add_argument("--silence", type=int, help="redecoupe a chaque repos >= silence us (defaut : une trame par reception)")
    p.add_argument("--tol-us", type=float, default=TOL_US, help="ecart maximal admis (defaut 2 us)")
    a = p.parse_args(argv)
    if (a.n is not None and a.n < 1) or (a.silence is not None and a.silence < 1) or a.tol_us < 0:
        p.error("--n et --silence doivent etre >= 1, --tol-us >= 0")
    try:
        v = evaluer(a.capture, a.motif, a.n, a.decalage_us, a.silence, a.tol_us)
    except OSError as e:
        print(f"banc.py : {e}", file=sys.stderr)
        return 2
    if not v.trames:
        print(f"aucune trame dans {a.capture}")
        return 1
    afficher(v, a.capture)
    return 0 if v.ok else 1


if __name__ == "__main__":
    sys.exit(main())
