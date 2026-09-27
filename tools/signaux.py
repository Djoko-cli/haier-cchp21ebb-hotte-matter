#!/usr/bin/env python3
"""Signaux synthetiques de la ligne D, motifs du banc et simulation de la sonde.

Un segment est un couple (niveau haut ?, duree en us) au niveau de la LIGNE
(du bus, pas de la GPIO). Les encodeurs rendent des segments consecutifs, sans
le repos qui precede ni celui qui suit la trame.

- uart : 8N1, LSB d'abord ; depart au niveau oppose au repos, stop au niveau
  du repos ; le bit de stop du dernier octet est compris dans la trame. Les
  frontieres des bits d'un octet sont a (k * 1000000 + bauds // 2) // bauds us
  du debut de l'octet (arrondi entier, sans derive d'un octet a l'autre).
- distance_impulsion : type WTC6534, repos haut : depart 2T bas, 0 = 1T haut
  + 1T bas, 1 = 1T haut + 3T bas, fin 1T haut.
- manchester : IEEE 802.3, 0 = haut puis bas, 1 = bas puis haut ; t_us est la
  duree d'un demi-bit.
- motifs du banc : memes contenus et memes segments que src/motifs.cpp.
- simuler_sonde : les objets 'trame' que la sonde publierait pour un flux.
"""

Segment = tuple[bool, int]  # (niveau haut ?, duree en us)

MOTIFS: tuple[str, ...] = ("uart500", "uart500inv", "uart2400", "uart2400inv",
                           "uart9600", "uart9600inv", "wtc", "krona", "rafale")
T_WTC_US = 750


def fusionner(segs: list[Segment]) -> list[Segment]:
    """Colle les segments consecutifs de meme niveau ; retire les durees nulles."""
    out: list[Segment] = []
    for niv, d in segs:
        if d < 0:
            raise ValueError(f"duree negative : {d}")
        if d == 0:
            continue
        if out and out[-1][0] == niv:
            out[-1] = (niv, out[-1][1] + d)
        else:
            out.append((niv, d))
    return out


def uart(octets: bytes, bauds: int, repos_haut: bool = True, pause_octet_us: int = 0) -> list[Segment]:
    """Octets en 8N1 ; pause_octet_us de repos entre deux octets (pas apres le dernier)."""
    if not 0 < bauds <= 1000000 or pause_octet_us < 0:
        raise ValueError("bauds dans 1..1000000, pause_octet_us >= 0")
    bornes = [(k * 1000000 + bauds // 2) // bauds for k in range(11)]
    segs: list[Segment] = []
    for i, octet in enumerate(octets):
        if i and pause_octet_us:
            segs.append((repos_haut, pause_octet_us))
        bits = [0] + [(octet >> k) & 1 for k in range(8)] + [1]
        for k, bit in enumerate(bits):
            segs.append(((bit == 1) == repos_haut, bornes[k + 1] - bornes[k]))
    return fusionner(segs)


def _verifier_bits(bits: str) -> None:
    if any(c not in "01" for c in bits):
        raise ValueError(f"bits : seulement 0 et 1 ({bits!r})")


def distance_impulsion(bits: str, t_us: int = 750) -> list[Segment]:
    """Depart 2T bas, 0 = 1T haut + 1T bas, 1 = 1T haut + 3T bas, fin 1T haut."""
    _verifier_bits(bits)
    segs: list[Segment] = [(False, 2 * t_us)]
    for b in bits:
        segs.append((True, t_us))
        segs.append((False, t_us if b == "0" else 3 * t_us))
    segs.append((True, t_us))
    return segs


def manchester(bits: str, t_us: int) -> list[Segment]:
    """IEEE 802.3 : 0 = haut puis bas, 1 = bas puis haut ; t_us = demi-bit."""
    _verifier_bits(bits)
    segs: list[Segment] = []
    for b in bits:
        segs += [(True, t_us), (False, t_us)] if b == "0" else [(False, t_us), (True, t_us)]
    return fusionner(segs)


def _verifier_motif(nom: str, index: int) -> None:
    if nom not in MOTIFS:
        raise ValueError(f"motif inconnu : {nom}")
    if index < 0:
        raise ValueError(f"index negatif : {index}")


def octets_motif(nom: str, index: int) -> bytes:
    """Contenu connu de la trame index (wtc : 16 bits de la trame puis 8 bits de la reponse)."""
    _verifier_motif(nom, index)
    if nom.startswith("uart"):
        c = [0xA5, 0x5A, 0x00, 0xFF, index & 0xFF]
        return bytes(c + [sum(c) & 0xFF])
    if nom == "wtc":
        v = (0x0100 << (index % 8)) & 0xFFFF
        return bytes([v >> 8, v & 0xFF, index & 0xFF])
    if nom == "krona":
        k = index % 8
        c = [0x55, k, (~k) & 0xFF, 0x00]
        return bytes(c + [sum(c) & 0xFF])
    return b""  # rafale


def trame_motif(nom: str, index: int) -> list[Segment]:
    """Segments de la trame index, sans le repos final (pause_motif_us)."""
    o = octets_motif(nom, index)
    if nom.startswith("uart"):
        bauds = int(nom[4:].removesuffix("inv"))
        return uart(o, bauds, repos_haut=not nom.endswith("inv"))
    if nom == "wtc":
        trame = distance_impulsion(format((o[0] << 8) | o[1], "016b"), T_WTC_US)
        reponse = distance_impulsion(format(o[2], "08b"), T_WTC_US)
        return fusionner(trame + [(True, 4 * T_WTC_US)] + reponse)
    if nom == "krona":
        return uart(o, 500, repos_haut=False, pause_octet_us=4000)
    return [(k % 2 == 0, 100) for k in range(100000)]  # rafale


def pause_motif_us(nom: str) -> int:
    """Repos apres chaque trame."""
    _verifier_motif(nom, 0)
    if nom.startswith("uart"):
        return 20000
    return {"wtc": 6000, "krona": 18000, "rafale": 1000000}[nom]


def repos_haut_motif(nom: str) -> bool:
    _verifier_motif(nom, 0)
    return not (nom.endswith("inv") or nom == "krona")


def simuler_sonde(segments: list[Segment], t0_us: int, silence_us: int, repos_haut: bool,
                  dur_max: int = 110, num0: int = 1) -> list[dict]:
    """Objets 'trame' que la sonde publierait pour ce flux (mode tout, sans perte).

    Le flux commence a t0_us ; il est precede et suivi du repos. Une reception
    commence au premier front apres un repos et se termine au premier palier
    de repos >= silence_us (ce palier n'est pas publie). Parties de dur_max
    durees au plus ; t_us de la premiere partie = instant du premier front,
    t_us des suivantes = t_us precedent + somme des durees precedentes ; niv0
    = niveau du bus au debut de la partie. n compte les objets a partir de 1 ;
    ms = fin de la partie (plus silence_us pour la derniere) en ms.
    Un palier hors repos >= silence_us leve ValueError : la sonde couperait la
    reception au milieu (seuil du RMT), ce que ce simulateur ne reproduit pas.
    """
    if dur_max < 1 or silence_us < 1:
        raise ValueError("dur_max et silence_us doivent etre >= 1")
    receptions: list[tuple[int, bool, list[int]]] = []  # (debut, niv0, durees)
    courante = None
    t = t0_us
    for niv, d in fusionner(list(segments)):
        if niv == repos_haut:
            if courante is not None:
                if d >= silence_us:
                    receptions.append(courante)
                    courante = None
                else:
                    courante[2].append(d)
        else:
            if d >= silence_us:
                raise ValueError(f"palier hors repos de {d} us >= silence_us ({silence_us}) a t={t} us")
            if courante is None:
                courante = (t, niv, [])
            courante[2].append(d)
        t += d
    if courante is not None:
        if len(courante[2]) % 2 == 0:  # derniere duree au niveau du repos : elle se fond dans le repos final
            courante[2].pop()
        receptions.append(courante)

    objets = []
    for r, (debut, niv0, durees) in enumerate(receptions):
        t_part = debut
        for part, k in enumerate(range(0, len(durees), dur_max)):
            morceau = durees[k:k + dur_max]
            fin = k + dur_max >= len(durees)
            haut = niv0 if k % 2 == 0 else not niv0
            fin_us = t_part + sum(morceau) + (silence_us if fin else 0)
            objets.append({"v": 1, "t": "trame", "n": len(objets) + 1, "ms": fin_us // 1000,
                           "num": num0 + r, "part": part, "fin": fin, "t_us": t_part,
                           "niv0": "haut" if haut else "bas", "dur_us": morceau, "debord": False})
            t_part += sum(morceau)
    return objets
