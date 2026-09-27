#!/usr/bin/env python3
"""Lecture des captures de la sonde : fichiers .jsonl ou capture serie brute.

- .jsonl (serie_enregistre.py, hotte_udp.py enregistre) : une ligne par
  message machine, {"rx_ms": ..., "de": "usb" | "<ip>", "l": <objet recu>}.
- capture serie brute : octets tels quels ; une ligne machine est RS + JSON
  + LF (dernier RS de la ligne, CR final tolere), le texte humain est ignore.

Chaine d'analyse : lire -> parties (objets 'trame') -> receptions (parties
recollees) -> flux (segments continus, silences entre receptions calcules par
t_us). En mode 'changements', les receptions repetees ne sont pas publiees :
le flux montre alors un silence a leur place.
"""
import json
import sys
from dataclasses import dataclass
from typing import Iterable, Iterator

RS = 0x1E


@dataclass(frozen=True)
class Partie:
    num: int
    part: int
    fin: bool
    t_us: int
    niv0_haut: bool
    dur_us: tuple[int, ...]
    debord: bool
    rep: int | None


@dataclass(frozen=True)
class Reception:
    num: int
    t_us: int
    niv0_haut: bool
    dur_us: tuple[int, ...]
    debord: bool
    complete: bool  # part 0 .. fin, sans trou


def lire_jsonl(chemin) -> Iterator[dict]:
    """Rend les objets 'l' ; une ligne illisible est signalee sur stderr et ignoree."""
    with open(chemin, encoding="utf-8") as f:
        for no, ligne in enumerate(f, 1):
            if not ligne.strip():
                continue
            try:
                message = json.loads(ligne)["l"]
                if not isinstance(message, dict):
                    raise ValueError("'l' n'est pas un objet")
            except (ValueError, KeyError, TypeError) as e:
                print(f"capture_fmt : {chemin} ligne {no} illisible, ignoree ({e})", file=sys.stderr)
                continue
            yield message


def lire_serie_brute(chemin) -> Iterator[dict]:
    """Decoupe RS ... LF ; ignore le texte humain et les lignes machine illisibles."""
    with open(chemin, "rb") as f:
        donnees = f.read()
    for ligne in donnees.split(b"\n")[:-1]:  # le dernier morceau n'a pas de LF
        i = ligne.rfind(bytes([RS]))
        if i < 0:
            continue
        corps = ligne[i + 1:]
        if corps.endswith(b"\r"):
            corps = corps[:-1]
        try:
            o = json.loads(corps.decode("ascii"))
        except (UnicodeDecodeError, ValueError):
            continue
        if isinstance(o, dict):
            yield o


def lire(chemin) -> Iterator[dict]:
    """.jsonl : lire_jsonl ; toute autre extension : lire_serie_brute."""
    if str(chemin).endswith(".jsonl"):
        return lire_jsonl(chemin)
    return lire_serie_brute(chemin)


def parties(objets: Iterable[dict]) -> list[Partie]:
    """Objets t == 'trame', dans l'ordre d'arrivee."""
    out = []
    for o in objets:
        if o.get("t") != "trame":
            continue
        out.append(Partie(num=o["num"], part=o["part"], fin=o["fin"], t_us=o["t_us"],
                          niv0_haut=o["niv0"] == "haut", dur_us=tuple(o["dur_us"]),
                          debord=bool(o.get("debord", False)), rep=o.get("rep")))
    return out


def receptions(parts: list[Partie]) -> list[Reception]:
    """Recolle les parties consecutives d'un meme num, dans l'ordre de part.

    Une part 0 ouvre toujours une nouvelle reception (redemarrage de la sonde
    compris) ; une part manquante ou une reception sans fin donne une
    reception incomplete (complete = False), jamais une erreur.
    """
    out: list[Reception] = []
    groupe: list[Partie] = []
    entier = True

    def clore(complete: bool):
        nonlocal groupe
        if groupe:
            dur = tuple(d for p in groupe for d in p.dur_us)
            out.append(Reception(num=groupe[0].num, t_us=groupe[0].t_us, niv0_haut=groupe[0].niv0_haut,
                                 dur_us=dur, debord=any(p.debord for p in groupe), complete=complete))
        groupe = []

    for p in parts:
        suite = groupe and p.num == groupe[-1].num and p.part == groupe[-1].part + 1
        if not suite:
            clore(False)
            entier = p.part == 0
        groupe.append(p)
        if p.fin:
            clore(entier)
    clore(False)
    return out


def flux(recs: list[Reception], repos_haut: bool) -> list[tuple[bool, int]]:
    """Segments continus : durees de chaque reception (niveaux alternes depuis niv0),
    separees par le silence calcule par t_us (rien si l'ecart est nul ou negatif).
    Le flux commence au premier front de la premiere reception."""
    segs: list[tuple[bool, int]] = []

    def ajouter(niv: bool, d: int):
        if d <= 0:
            return
        if segs and segs[-1][0] == niv:
            segs[-1] = (niv, segs[-1][1] + d)
        else:
            segs.append((niv, d))

    fin_prec = None
    for r in recs:
        if fin_prec is not None:
            ajouter(repos_haut, r.t_us - fin_prec)
        for k, d in enumerate(r.dur_us):
            ajouter(r.niv0_haut if k % 2 == 0 else not r.niv0_haut, d)
        fin_prec = r.t_us + sum(r.dur_us)
    return segs


def repos_majoritaire(recs: list[Reception]) -> bool:
    """Niveau de repos deduit : le niveau qui suit la derniere duree de chaque
    reception (la sonde clot une reception sur un silence), a la majorite ;
    haut en cas d'egalite ou sans reception."""
    haut = bas = 0
    for r in recs:
        if not r.dur_us:
            continue
        dernier_haut = r.niv0_haut if len(r.dur_us) % 2 == 1 else not r.niv0_haut
        if dernier_haut:
            bas += 1
        else:
            haut += 1
    return haut >= bas
