#!/usr/bin/env python3
"""Analyse des captures de la sonde : histogrammes, trames, decodage (etape 6).

Usage :
  python3 tools/analyse.py histo  <capture> [--pas us]
  python3 tools/analyse.py trames <capture> --silence us [--max n]
  python3 tools/analyse.py auto   <capture> [--silence us] [--max n]
  python3 tools/analyse.py uart   <capture> <bauds> [--inverse] [--silence us] [--max n]
  python3 tools/analyse.py diff   <a> <b> [--silence us]
  option commune : --repos haut|bas (defaut : deduit des receptions, niveau
  qui suit la derniere duree de chaque reception, a la majorite)

<capture> : fichier .jsonl (serie_enregistre.py, hotte_udp.py enregistre) ou
capture serie brute. Sans --silence, chaque reception de la sonde est une
trame ; avec --silence, le flux est redecoupe a chaque repos >= silence us.
--max : trames affichees (defaut 20, 0 : toutes).
diff : les trames de a et de b sont decodees avec la meilleure hypothese de
a ; on compare leurs contenus les plus frequents (octets pour l'UART, bits
par message sinon).

Code de sortie : 0 ; 1 si la capture ne contient aucune trame ; 2 si elle est
illisible ou si les arguments sont faux.
"""
import argparse
import sys

import capture_fmt
import decodeurs

SANS_SILENCE = 10**12


def charger(chemin, repos: str | None = None, silence_us: int | None = None):
    """-> (trames datees [(t_us, segments)], repos_haut, receptions)."""
    recs = capture_fmt.receptions(capture_fmt.parties(capture_fmt.lire(chemin)))
    repos_haut = capture_fmt.repos_majoritaire(recs) if repos is None else repos == "haut"
    trames = []
    if silence_us is None:
        for r in recs:
            for debut, t in decodeurs.decouper_dates(capture_fmt.flux([r], repos_haut), SANS_SILENCE, repos_haut):
                trames.append((r.t_us + debut, t))
    elif recs:
        for debut, t in decodeurs.decouper_dates(capture_fmt.flux(recs, repos_haut), silence_us, repos_haut):
            trames.append((recs[0].t_us + debut, t))
    return trames, repos_haut, recs


def niveaux(recs) -> tuple[list[int], list[int], list[int]]:
    """Durees hautes, durees basses, silences entre receptions consecutives."""
    hautes, basses, silences = [], [], []
    for k, r in enumerate(recs):
        for j, d in enumerate(r.dur_us):
            (hautes if (r.niv0_haut if j % 2 == 0 else not r.niv0_haut) else basses).append(d)
        if k:
            p = recs[k - 1]
            s = r.t_us - (p.t_us + sum(p.dur_us))
            if s > 0:
                silences.append(s)
    return hautes, basses, silences


def libelle(h: decodeurs.Decodage) -> str:
    if h.nom == "uart":
        return f"uart {h.params['bauds']} bauds" + (" inverse" if h.params["inverse"] else "")
    return f"{h.nom} T={h.params['t_us']} us"


def contenu(d: decodeurs.Decodage) -> str:
    if d.nom == "uart":
        return d.octets.hex(" ") or "-"
    return f"{d.bits} = {d.octets.hex(' ')}" if d.bits else "-"


def resume(h: decodeurs.Decodage) -> str:
    if not h.symboles:
        return f"{libelle(h)} : aucun symbole ({h.erreurs} erreurs)"
    return f"{libelle(h)} : {h.erreurs} erreurs / {h.symboles} symboles ({100 * h.taux:.1f} %)"


def paliers(tr, n: int = 64) -> str:
    txt = " ".join(("h" if niv else "b") + str(d) for niv, d in tr[:n])
    return txt + (f" ... (+{len(tr) - n})" if len(tr) > n else "")


def texte_repos(repos_haut: bool) -> str:
    return "repos haut" if repos_haut else "repos bas"


def afficher_trames(hyp, trames, maxi: int) -> None:
    print(f"trames decodees avec : {libelle(hyp)}")
    decs = decodeurs.appliquer(hyp, [t for _, t in trames])
    for k, ((t, _), d) in enumerate(zip(trames, decs)):
        if maxi and k >= maxi:
            print(f"  ... {len(trames) - k} trames de plus (--max 0 : toutes)")
            break
        err = f"  ({d.erreurs} erreurs)" if d.erreurs else ""
        print(f"  #{k} t={t} us : {contenu(d)}{err}")


def repartition(decs) -> list[list]:
    """[effectif, decodage] par contenu distinct, du plus frequent au moins frequent."""
    groupes: dict = {}
    for d in decs:
        cle = (d.octets, d.bits)
        if cle in groupes:
            groupes[cle][0] += 1
        else:
            groupes[cle] = [1, d]
    return sorted(groupes.values(), key=lambda g: -g[0])


def positions_octets(a: bytes, b: bytes) -> list[tuple[int, int | None, int | None]]:
    """(position, octet de a, octet de b) qui different ; None : absent."""
    out = []
    for k in range(max(len(a), len(b))):
        va = a[k] if k < len(a) else None
        vb = b[k] if k < len(b) else None
        if va != vb:
            out.append((k, va, vb))
    return out


def positions_bits(a: str, b: str) -> list[tuple[int, int, str | None, str | None]]:
    """(message, position, bit de a, bit de b) qui different ; messages separes par une espace."""
    ma, mb = a.split(), b.split()
    out = []
    for m in range(max(len(ma), len(mb))):
        xa = ma[m] if m < len(ma) else ""
        xb = mb[m] if m < len(mb) else ""
        for k in range(max(len(xa), len(xb))):
            va = xa[k] if k < len(xa) else None
            vb = xb[k] if k < len(xb) else None
            if va != vb:
                out.append((m, k, va, vb))
    return out


def _hex(v) -> str:
    return "--" if v is None else f"{v:02x}"


def cmd_histo(a) -> int:
    _, repos_haut, recs = charger(a.capture, a.repos)
    if not recs:
        print(f"aucune trame dans {a.capture}")
        return 1
    hautes, basses, silences = niveaux(recs)
    print(f"{a.capture} : {len(recs)} receptions, {texte_repos(repos_haut)}")
    for nom, ds, histo in (("durees hautes", hautes, True), ("durees basses", basses, True),
                           ("silences entre receptions", silences, False)):
        print(f"{nom} : {len(ds)}")
        if not ds:
            continue
        print("  regroupements (centre x effectif) : "
              + ", ".join(f"{c} x {n}" for c, n in decodeurs.regroupements(ds)))
        if histo:
            for debut, n in decodeurs.histogramme(ds, a.pas):
                print(f"  {debut:>7} us : {n}")
    return 0


def cmd_trames(a) -> int:
    trames, repos_haut, _ = charger(a.capture, a.repos, a.silence)
    print(f"{a.capture} : {len(trames)} trames ({texte_repos(repos_haut)}, silence {a.silence} us)")
    fin_prec = None
    for k, (t, tr) in enumerate(trames):
        duree = sum(d for _, d in tr)
        if not a.max or k < a.max:
            apres = "" if fin_prec is None else f", {t - fin_prec} us apres la precedente"
            print(f"#{k} t={t} us, {len(tr)} paliers, {duree} us{apres}")
            print("  " + paliers(tr))
        elif k == a.max:
            print(f"... {len(trames) - k} trames de plus (--max 0 : toutes)")
        fin_prec = t + duree
    return 0 if trames else 1


def _entete(a, trames, repos_haut) -> None:
    decoupe = f"silence {a.silence} us" if a.silence else "une trame par reception"
    print(f"{a.capture} : {len(trames)} trames, {texte_repos(repos_haut)}, {decoupe}")


def cmd_auto(a) -> int:
    trames, repos_haut, _ = charger(a.capture, a.repos, a.silence)
    if not trames:
        print(f"aucune trame dans {a.capture}")
        return 1
    _entete(a, trames, repos_haut)
    hyps = decodeurs.auto([t for _, t in trames])
    print("hypotheses (taux d'erreur croissant) :")
    for k, h in enumerate(hyps, 1):
        print(f"  {k:2}. {resume(h)}")
    afficher_trames(hyps[0], trames, a.max)
    return 0


def cmd_uart(a) -> int:
    trames, repos_haut, _ = charger(a.capture, a.repos, a.silence)
    if not trames:
        print(f"aucune trame dans {a.capture}")
        return 1
    _entete(a, trames, repos_haut)
    hyp = decodeurs.Decodage("uart", {"bauds": a.bauds, "inverse": a.inverse}, b"", "", 0, 0)
    decs = decodeurs.appliquer(hyp, [t for _, t in trames])
    total = decodeurs.Decodage("uart", hyp.params, b"", "", sum(d.erreurs for d in decs),
                               sum(d.symboles for d in decs))
    print(resume(total))
    afficher_trames(hyp, trames, a.max)
    return 0


def cmd_diff(a) -> int:
    ta, _, _ = charger(a.a, a.repos, a.silence)
    tb, _, _ = charger(a.b, a.repos, a.silence)
    for chemin, t in ((a.a, ta), (a.b, tb)):
        if not t:
            print(f"aucune trame dans {chemin}")
            return 1
    hyp = decodeurs.auto([t for _, t in ta])[0]
    print(f"hypothese (meilleure pour a) : {libelle(hyp)}")
    tetes = []
    for chemin, trames in ((a.a, ta), (a.b, tb)):
        groupes = repartition(decodeurs.appliquer(hyp, [t for _, t in trames]))
        print(f"{chemin} : {len(trames)} trames, {len(groupes)} contenus distincts")
        for n, d in groupes:
            print(f"  {n:5} x {contenu(d)}")
        tetes.append(groupes[0][1])
    da, db = tetes
    print(f"contenus les plus frequents : a = {contenu(da)} ; b = {contenu(db)}")
    if hyp.nom == "uart":
        pos = positions_octets(da.octets, db.octets)
        details = []
        for k, va, vb in pos:
            txt = f"{k} ({_hex(va)} -> {_hex(vb)}"
            if va is not None and vb is not None:
                x = va ^ vb
                bits = [str(j) for j in range(8) if x >> j & 1]
                txt += (" : bit " if len(bits) == 1 else " : bits ") + ", ".join(bits)
            details.append(txt + ")")
        print("octets qui changent : " + ", ".join(details) if details else "aucune difference")
    else:
        pos = positions_bits(da.bits, db.bits)
        details = [f"message {m} bit {k} ({va or '-'} -> {vb or '-'})" for m, k, va, vb in pos]
        print("bits qui changent : " + ", ".join(details) if details else "aucune difference")
    return 0


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="analyse.py", description="Analyse des captures de la sonde (ligne D).")
    commun = argparse.ArgumentParser(add_help=False)
    commun.add_argument("--repos", choices=("haut", "bas"), help="niveau de repos du bus (defaut : deduit)")
    sous = p.add_subparsers(dest="cmd", required=True)

    s = sous.add_parser("histo", parents=[commun], help="histogrammes et regroupements des durees")
    s.add_argument("capture")
    s.add_argument("--pas", type=int, default=10, help="largeur des classes en us (defaut 10)")
    s = sous.add_parser("trames", parents=[commun], help="redecoupe le flux en trames")
    s.add_argument("capture")
    s.add_argument("--silence", type=int, required=True, help="repos minimal entre deux trames, en us")
    s.add_argument("--max", type=int, default=20)
    s = sous.add_parser("auto", parents=[commun], help="classe les hypotheses de decodage")
    s.add_argument("capture")
    s.add_argument("--silence", type=int)
    s.add_argument("--max", type=int, default=20)
    s = sous.add_parser("uart", parents=[commun], help="decodage UART impose")
    s.add_argument("capture")
    s.add_argument("bauds", type=int)
    s.add_argument("--inverse", action="store_true", help="niveaux inverses, repos bas")
    s.add_argument("--silence", type=int)
    s.add_argument("--max", type=int, default=20)
    s = sous.add_parser("diff", parents=[commun], help="ce qui change entre deux scenarios")
    s.add_argument("a")
    s.add_argument("b")
    s.add_argument("--silence", type=int)

    a = p.parse_args(argv)
    if getattr(a, "silence", None) is not None and a.silence < 1:
        p.error("--silence doit etre >= 1")
    if getattr(a, "pas", 1) < 1 or getattr(a, "bauds", 1) < 1 or getattr(a, "max", 0) < 0:
        p.error("--pas et bauds doivent etre >= 1, --max >= 0")
    commandes = {"histo": cmd_histo, "trames": cmd_trames, "auto": cmd_auto, "uart": cmd_uart, "diff": cmd_diff}
    try:
        return commandes[a.cmd](a)
    except OSError as e:
        print(f"analyse.py : {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
