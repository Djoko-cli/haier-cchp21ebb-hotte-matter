#!/usr/bin/env python3
"""Decoupage et decodage des trames de la ligne D (etape 6 de la spec).

Une trame est une liste de segments (niveau haut ?, duree en us) au niveau du
bus, telle que decouper() la rend : elle commence au premier front apres un
repos et s'arrete au dernier front avant le repos suivant. Le niveau avant la
trame est donc l'oppose de son premier segment, et le niveau apres, l'oppose
de son dernier.

Chaque decodeur rend un Decodage : octets, bits (dans l'ordre de lecture),
erreurs et symboles tentes ; taux = erreurs / max(1, symboles).
"""
import bisect
from collections import Counter
from dataclasses import dataclass
from typing import Iterable

from signaux import fusionner


def histogramme(durees: Iterable[int], pas_us: int = 10) -> list[tuple[int, int]]:
    """(debut de classe, effectif), trie par classe."""
    if pas_us < 1:
        raise ValueError("pas_us >= 1")
    return sorted(Counter((d // pas_us) * pas_us for d in durees).items())


def regroupements(durees: Iterable[int], tol: float = 0.15) -> list[tuple[int, int]]:
    """(centre, effectif), trie par centre. Une duree rejoint le groupe courant
    si elle ne depasse pas le plus petit du groupe de plus de tol ; centre =
    moyenne arrondie."""
    groupes: list[list[int]] = []
    for d in sorted(durees):
        if groupes and d <= groupes[-1][0] * (1 + tol):
            groupes[-1].append(d)
        else:
            groupes.append([d])
    return [(round(sum(g) / len(g)), len(g)) for g in groupes]


def decouper_dates(segments, silence_us: int, repos_haut: bool) -> list[tuple[int, list[tuple[bool, int]]]]:
    """Comme decouper, avec le debut de chaque trame en us depuis le debut du flux."""
    trames = []
    courante: list[tuple[bool, int]] = []
    debut = t = 0
    for niv, d in fusionner(list(segments)):
        if niv == repos_haut and (d >= silence_us or not courante):
            if courante:
                trames.append((debut, courante))
                courante = []
        else:
            if not courante:
                debut = t
            courante.append((niv, d))
        t += d
    if courante and courante[-1][0] == repos_haut:  # le flux est suivi du repos
        courante.pop()
    if courante:
        trames.append((debut, courante))
    return trames


def decouper(segments, silence_us: int, repos_haut: bool) -> list[list[tuple[bool, int]]]:
    """Coupe le flux en trames a chaque palier de repos >= silence_us ; les
    paliers de repos de tete et de queue sont retires."""
    return [trame for _, trame in decouper_dates(segments, silence_us, repos_haut)]


@dataclass
class Decodage:
    nom: str        # "uart", "distance_impulsion", "manchester"
    params: dict    # parametres nommes du decodeur (rejouables)
    octets: bytes
    bits: str       # ordre de lecture ; messages separes par une espace
    erreurs: int
    symboles: int

    @property
    def taux(self) -> float:
        return self.erreurs / max(1, self.symboles)


class _Trame:
    """Segments fusionnes, dates des fronts, niveau a un instant donne."""

    def __init__(self, trame):
        self.segs = fusionner(list(trame))
        self.debuts = []
        t = 0
        for _, d in self.segs:
            self.debuts.append(t)
            t += d
        self.fin = t
        self.fronts = self.debuts + [t]

    def niveau(self, t: float) -> bool:
        if t >= self.fin:
            return not self.segs[-1][0]
        if t < 0:
            return not self.segs[0][0]
        return self.segs[bisect.bisect_right(self.debuts, t) - 1][0]


def _aligne(tr: _Trame, ts: int, bit: float) -> bool:
    """Chaque ecart entre fronts de l'octet (jusqu'au milieu du stop) vaut un
    nombre entier de bits, a un tiers de bit pres."""
    prec = ts
    j = bisect.bisect_right(tr.fronts, ts)
    while j < len(tr.fronts) and tr.fronts[j] <= ts + 9.5 * bit:
        d = tr.fronts[j] - prec
        k = round(d / bit)
        if k < 1 or abs(d - k * bit) > bit / 3:
            return False
        prec = tr.fronts[j]
        j += 1
    return True


def uart(trame, bauds: int, inverse: bool = False) -> Decodage:
    """UART 8N1, LSB d'abord ; repos haut (inverse : niveaux inverses, repos bas).

    Depart = front vers le niveau logique 0 ; echantillonnage au milieu des
    bits a partir de ce front ; faux depart si le milieu du bit de depart est
    deja revenu a 1. symboles = octets tentes ; erreur (une par octet) : faux
    depart, bit de stop faux (erreur de trame), ou front hors de la grille des
    bits (ecart entre fronts qui n'est pas un nombre entier de bits a 1/3 de
    bit pres : les durees d'un UART sont des multiples du temps bit).
    """
    params = {"bauds": bauds, "inverse": inverse}
    tr = _Trame(trame)
    if not tr.segs:
        return Decodage("uart", params, b"", "", 0, 0)
    bit = 1000000 / bauds

    def logique(t: float) -> bool:
        return tr.niveau(t) != inverse

    octets = bytearray()
    erreurs = symboles = 0
    pos = -1.0
    i = 0
    while True:
        while i < len(tr.segs) and (tr.debuts[i] <= pos or tr.segs[i][0] != inverse):
            i += 1
        if i >= len(tr.segs):
            break
        ts = tr.debuts[i]
        symboles += 1
        if logique(ts + bit / 2):
            erreurs += 1
            pos = ts
            continue
        v = 0
        for k in range(8):
            if logique(ts + (k + 1.5) * bit):
                v |= 1 << k
        octets.append(v)
        if not logique(ts + 9.5 * bit) or not _aligne(tr, ts, bit):
            erreurs += 1
        pos = ts + 9.5 * bit
    bits = "".join(format(o, "08b") for o in octets)
    return Decodage("uart", params, bytes(octets), bits, erreurs, symboles)
