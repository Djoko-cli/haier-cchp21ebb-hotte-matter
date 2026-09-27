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


BAUDS = (300, 500, 600, 1200, 2400, 4800, 9600, 19200)


def _plus_petit_groupe(durees) -> int | None:
    """Centre du plus petit groupe d'au moins 2 durees (a defaut, du plus petit groupe)."""
    g = regroupements(durees)
    if not g:
        return None
    frequents = [c for c, n in g if n >= 2]
    return (frequents or [g[0][0]])[0]


def _vaut(d: int, t: float, m: int) -> bool:
    """d vaut m fois T, a 25 % de T pres."""
    return abs(d - m * t) <= t / 4


def _emballer(messages: list[str]) -> bytes:
    """Bits MSB d'abord, chaque message complete par des 0 jusqu'a l'octet."""
    out = bytearray()
    for m in messages:
        m = m + "0" * (-len(m) % 8)
        out += bytes(int(m[k:k + 8], 2) for k in range(0, len(m), 8))
    return bytes(out)


def distance_impulsion(trame, t_us: int | None = None) -> Decodage:
    """Distance d'impulsion type WTC6534, repos haut. T estime (plus petit groupe
    des durees hautes) si t_us est None ; tolerance 25 % de T.

    Depart = palier bas de 2T ; puis chaque bit = 1T haut suivi de 1T bas (0)
    ou de 3T bas (1) ; un palier haut qui ne vaut pas 1T clot le message (fin,
    reponse de la carte...) ; un bas de 2T a la place d'un bit ouvre un nouveau
    message. erreurs = paliers bas non classables (hors message : autre chose
    que 2T ; dans un message : ni 1T, ni 2T, ni 3T), apres quoi on attend un
    nouveau depart. symboles = bits lus ; bits : un groupe par message.
    """
    segs = fusionner(list(trame))
    if t_us is None:
        t_us = _plus_petit_groupe([d for niv, d in segs if niv])
    params = {"t_us": t_us}
    if not t_us:
        return Decodage("distance_impulsion", params, b"", "", len(segs), 0)
    messages: list[str] = []
    bits = None          # None : attente d'un depart
    attente_haut = False
    erreurs = 0
    for niv, d in segs:
        if bits is None:
            if not niv:
                if _vaut(d, t_us, 2):
                    bits, attente_haut = "", True
                else:
                    erreurs += 1
            continue
        if attente_haut:
            if niv and _vaut(d, t_us, 1):
                attente_haut = False
            else:
                messages.append(bits)
                bits = None
            continue
        if _vaut(d, t_us, 1):
            bits += "0"
            attente_haut = True
        elif _vaut(d, t_us, 3):
            bits += "1"
            attente_haut = True
        elif _vaut(d, t_us, 2):
            messages.append(bits)
            bits, attente_haut = "", True
        else:
            erreurs += 1
            messages.append(bits)
            bits = None
    if bits is not None:
        messages.append(bits)
    messages = [m for m in messages if m]
    return Decodage("distance_impulsion", params, _emballer(messages), " ".join(messages),
                    erreurs, sum(len(m) for m in messages))


def manchester(trame, t_us: int | None = None) -> Decodage:
    """Manchester IEEE 802.3 (0 = haut puis bas, 1 = bas puis haut), t_us = demi-bit.

    T estime (plus petit groupe de toutes les durees) si t_us est None ;
    chaque palier vaut 1 ou 2 demi-bits (25 % de T pres) ; la phase (premier
    demi-bit fondu ou non dans le repos) est celle qui donne le moins de paires
    sans transition au milieu. erreurs = paliers ni T ni 2T + paires sans
    transition ; symboles = paires (bits tentes).
    """
    segs = fusionner(list(trame))
    if t_us is None:
        t_us = _plus_petit_groupe([d for _, d in segs])
    params = {"t_us": t_us}
    if not segs or not t_us:
        return Decodage("manchester", params, b"", "", len(segs), 0)
    demis: list[bool] = []
    erreurs = 0
    for niv, d in segs:
        if _vaut(d, t_us, 1):
            n = 1
        elif _vaut(d, t_us, 2):
            n = 2
        else:
            erreurs += 1
            n = min(2, max(1, round(d / t_us)))
        demis += [niv] * n
    avant, apres = not segs[0][0], not segs[-1][0]
    meilleur = None
    for tete in ([], [avant]):
        suite = tete + demis
        if len(suite) % 2:
            suite.append(apres)
        paires = [(suite[k], suite[k + 1]) for k in range(0, len(suite), 2)]
        invalides = sum(1 for a, b in paires if a == b)
        if meilleur is None or invalides < meilleur[0]:
            meilleur = (invalides, paires)
    invalides, paires = meilleur
    bits = "".join("0" if a else "1" for a, b in paires if a != b)
    return Decodage("manchester", params, _emballer([bits] if bits else []), bits,
                    erreurs + invalides, len(paires))


_DECODEURS = {"uart": uart, "distance_impulsion": distance_impulsion, "manchester": manchester}


def appliquer(hyp: Decodage, trames) -> list[Decodage]:
    """Redecode chaque trame avec le decodeur et les parametres de hyp."""
    f = _DECODEURS[hyp.nom]
    return [f(t, **hyp.params) for t in trames]


def _cumuler(nom: str, params: dict, decs: list[Decodage]) -> Decodage:
    return Decodage(nom, params, b"".join(d.octets for d in decs), " ".join(d.bits for d in decs if d.bits),
                    sum(d.erreurs for d in decs), sum(d.symboles for d in decs))


def auto(trames, bauds=BAUDS) -> list[Decodage]:
    """Essaie tous les decodeurs sur toutes les trames : UART a chaque debit,
    normal et inverse, distance d'impulsion et Manchester (T estime sur
    l'ensemble des trames). Une hypothese cumule les trames. Classement : taux
    d'erreur croissant, puis moins de symboles (a taux egal, un UART a debit
    double tente plus d'octets) ; une hypothese sans symbole va en queue."""
    trames = [list(t) for t in trames if t]
    if not trames:
        return []
    hyps = []
    for b in bauds:
        for inv in (False, True):
            hyps.append(_cumuler("uart", {"bauds": b, "inverse": inv}, [uart(t, b, inv) for t in trames]))
    fus = [fusionner(t) for t in trames]
    t_di = _plus_petit_groupe([d for t in fus for niv, d in t if niv])
    hyps.append(_cumuler("distance_impulsion", {"t_us": t_di}, [distance_impulsion(t, t_di) for t in trames]))
    t_m = _plus_petit_groupe([d for t in fus for _, d in t])
    hyps.append(_cumuler("manchester", {"t_us": t_m}, [manchester(t, t_m) for t in trames]))
    return sorted(hyps, key=lambda h: (h.symboles == 0, h.taux, h.symboles))
