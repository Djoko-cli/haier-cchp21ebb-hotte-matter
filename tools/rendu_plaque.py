#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Rendu SVG des plans de plaque a pastilles (projet hotte Haier).

  python3 tools/rendu_plaque.py [plan.json ...] [--sortie DOSSIER]

Sans plan donne : docs/fabrication/plan-boitier.json et plan-adaptateur.json ; sortie dans
docs/fabrication/ (ou --sortie). Pour chaque plan (format lu par verifier_plaque.py) :
  <carte>-composants.svg   vue cote composants (colonne 1 a gauche)
  <carte>-soudures.svg     vue cote soudures, en miroir (colonne 1 a droite)
Sans plan donne, en plus : supermini-brochage.svg (brochage reel de la SuperMini violette, releve le
28/09) et banc-breadboard.svg (banc de la tache 16 : generateur sur la breadboard, sonde sur le
boitier de mesure detache de la hotte).

Conventions du dessin (rappelees dans la legende de chaque figure) :
  - la face regardee est en trait plein ; ce qui est sur l'autre face est pale ;
  - pont de soudure : trait gris epais entre deux pastilles voisines ; fil nu : trait gris fin,
    soude aux points ronds seulement ; fil isole : sa couleur (rouge, vert, bleu, noir, orange) ;
  - etape 7 (etage d'injection) : pointille violet ; option 5b (analyseur) : pointille bleu-vert ;
  - BC547 : le trait jaune du contour est la face plate ;
  - marquages au feutre du plan : numerotes sur la vue cote composants, listes dans la legende.
Aucune dependance externe. Texte en UTF-8, polices sans empattement.
"""
import json
import math
import os
import re
import sys
import unicodedata

ICI = os.path.dirname(os.path.abspath(__file__))
RANGEES = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
PAS = 2.54
POLICE = "Helvetica, Arial, sans-serif"
MOINS = "−"

# Brochage releve sur la photo de Majid (28/09), vue cote composants, USB-C a gauche.
HAUT = ["5V", "GND", "3V3", "20", "19", "18", "15", "14", "9", "8"]
BAS = ["TX", "RX", "0", "1", "2", "3", "4", "5", "6", "7"]

C = {
    "fond": "#ffffff", "texte": "#1b1f23", "gris": "#5f6670",
    "plaque": "#f4ecd9", "plaque_bord": "#b49e70", "cuivre": "#e2b05e", "cuivre_bord": "#a6772c",
    "etain": "#c3c8cd", "etain_bord": "#6c737a", "nu_bord": "#4f565d",
    "rouge": "#d62828", "vert": "#1f9a3a", "bleu": "#1f66d1", "noir": "#1e1e1e", "orange": "#f08c00",
    "blanc": "#ffffff", "marron": "#8b5a2b", "violet_module": "#6a3d9a",
    "e7": "#8e44ad", "e5b": "#0b8584", "feutre": "#1a4fbf", "plat": "#f2a900",
}
COULEUR_FIL = {"rouge": C["rouge"], "vert": C["vert"], "bleu": C["bleu"], "noir": C["noir"],
               "orange": C["orange"], "blanc": C["blanc"], "marron": C["marron"]}
NOM_PICOT = {"TPM": "TP" + MOINS, "TPP": "TP+", "TPDP": "TP_Dp", "TPDC": "TP_Dc"}
COUL_PICOT = {"TPM": "noir", "TPP": "rouge", "TPDP": "blanc", "TPDC": "marron"}
FILS_CN3 = {"-": "noir", "D": "blanc", "+": "rouge"}
FILS_SORTIE = {"1": "noir", "2": "rouge", "3": "blanc", "4": "marron"}
NOM_SORTIE = {"1": "1 " + MOINS, "2": "2 +", "3": "3 Dp", "4": "4 Dc"}
NOM_X1 = {"1": ("1", MOINS, "noir"), "2": ("2", "+", "rouge"), "3": ("3", "Dp", "blanc"), "4": ("4", "Dc", "marron")}
SIGNE = {"-": MOINS, "D": "D", "+": "+"}

REGLAGES = {
    # k : pixels par mm ; marges : gauche, droite, haut, bas (mm, du centre du trou extreme au bord)
    "boitier": {"k": 15.0, "marges": (5.8, 5.8, 3.4, 3.4), "coins": "tous", "titre": "Boîtier de mesure",
                "fils_ext": 0},
    "adaptateur": {"k": 26.0, "marges": (5.8, 2.3, 3.4, 2.3), "coins": "haut-gauche", "titre": "Adaptateur",
                   "fils_ext": 170},
}


# ----------------------------------------------------------------------------- utilitaires
def trou(t):
    t = t.strip().upper()
    return RANGEES.index(t[0]), int(t[1:])


def plat(s):
    """minuscules sans accents, pour chercher des mots dans les notes"""
    return "".join(ch for ch in unicodedata.normalize("NFD", s or "") if unicodedata.category(ch) != "Mn").lower()


def etape_de(x):
    if x.get("monte") is False:
        return "reserve"
    e = str(x.get("etape", ""))
    return e if e in ("7", "5b") else ""


def esc(s):
    return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def pale(hexa, f=0.72):
    h = hexa.lstrip("#")
    r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    r, g, b = [int(round(v + (255 - v) * f)) for v in (r, g, b)]
    return "#%02x%02x%02x" % (r, g, b)


def valeur_courte(v):
    v = (v or "").split(" (")[0].strip().replace(".", ",")
    if re.fullmatch(r"\d+", v):
        v += " Ω"
    return v


def nom_ref(ref):
    return {"RBE": "R_be"}.get(ref, ref)


def couleur_dans(note):
    """couleur d'un fil isole : le mot qui suit « fil », sinon la premiere couleur citee dans la note"""
    n = plat(note)
    m = re.search(r"\bfil (rouge|vert|bleu|noir|orange)\b", n)
    if m:
        return m.group(1)
    trouves = [(m.start(), mot) for mot in ("rouge", "vert", "bleu", "noir", "orange")
               for m in [re.search(r"\b" + mot + r"\b", n)] if m]
    return min(trouves)[1] if trouves else None


def largeur_texte(s, taille, gras=False):
    l = 0.0
    for ch in s:
        if ch in "il.,:;|!'()[]fjtI":
            l += 0.3
        elif ch in "mwMW":
            l += 0.86
        elif ch.isupper() or ch.isdigit() or ch in "_" + MOINS + "+":
            l += 0.66
        elif ch == " ":
            l += 0.28
        else:
            l += 0.55
    return l * taille * (1.06 if gras else 1.0)


def plage(z):
    a, b = z.split("-")
    return trou(a), trou(b)


def unit(ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    n = math.hypot(dx, dy) or 1.0
    return dx / n, dy / n


def rectangle_oriente(cx, cy, tx, ty, long_, large):
    nx, ny = -ty, tx
    h, w = long_ / 2.0, large / 2.0
    return [(cx + tx * h + nx * w, cy + ty * h + ny * w), (cx - tx * h + nx * w, cy - ty * h + ny * w),
            (cx - tx * h - nx * w, cy - ty * h - ny * w), (cx + tx * h - nx * w, cy + ty * h - ny * w)]


def boite_de(pts, marge=0.0):
    xs = [q[0] for q in pts]
    ys = [q[1] for q in pts]
    return (min(xs) - marge, min(ys) - marge, max(xs) + marge, max(ys) + marge)


ACCENTS = [(r"\bcote\b", "côté"), (r"\bapparait\b", "apparaît"), (r"\ba (droite|part|marquer)\b", r"à \1"),
           (r"\bou il\b", "où il"), (r"\brange\b", "rangé"), (r"\betape\b", "étape"), (r"\bferme\b", "fermé"), (r"\bcable\b", "câble")]


def accents(s):
    """remet les accents des mots courants des plans (ecrits sans accents) pour l'affichage"""
    for motif, rempl in ACCENTS:
        s = re.sub(motif, rempl, s)
    return s.replace("- (noir", MOINS + " (noir")


def coupe_texte(s, n):
    mots, lignes, cur = s.split(), [], ""
    for m in mots:
        if len(cur) + len(m) + 1 > n and cur:
            lignes.append(cur)
            cur = m
        else:
            cur = (cur + " " + m).strip()
    if cur:
        lignes.append(cur)
    return lignes


# ----------------------------------------------------------------------------- SVG
class Svg:
    def __init__(self, w, h):
        self.w, self.h = int(w), int(h)
        self.el = []

    def add(self, s):
        self.el.append(s)

    @staticmethod
    def _dash(dash):
        return (' stroke-dasharray="%s"' % dash) if dash else ""

    def ligne(self, pts, coul, ep, dash=None, cap="round"):
        d = "M" + " L".join("%.1f %.1f" % p for p in pts)
        self.add('<path d="%s" fill="none" stroke="%s" stroke-width="%.2f" stroke-linecap="%s" '
                 'stroke-linejoin="round"%s/>' % (d, coul, ep, cap, self._dash(dash)))

    def courbe(self, d, coul, ep, dash=None):
        self.add('<path d="%s" fill="none" stroke="%s" stroke-width="%.2f" stroke-linecap="round"%s/>'
                 % (d, coul, ep, self._dash(dash)))

    def poly(self, pts, fill, stroke=None, ep=1.0, dash=None, op=None):
        self.add('<polygon points="%s" fill="%s"%s%s%s/>' % (
            " ".join("%.1f,%.1f" % p for p in pts), fill,
            (' stroke="%s" stroke-width="%.2f" stroke-linejoin="round"' % (stroke, ep)) if stroke else "",
            self._dash(dash), (' fill-opacity="%.2f"' % op) if op is not None else ""))

    def cercle(self, x, y, r, fill, stroke=None, ep=1.0, dash=None):
        self.add('<circle cx="%.1f" cy="%.1f" r="%.2f" fill="%s"%s%s/>' % (
            x, y, r, fill, (' stroke="%s" stroke-width="%.2f"' % (stroke, ep)) if stroke else "", self._dash(dash)))

    def rect(self, x, y, w, h, fill, stroke=None, ep=1.0, rx=0, dash=None, op=None):
        self.add('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%.1f" fill="%s"%s%s%s/>' % (
            x, y, w, h, rx, fill, (' stroke="%s" stroke-width="%.2f"' % (stroke, ep)) if stroke else "",
            self._dash(dash), (' fill-opacity="%.2f"' % op) if op is not None else ""))

    def texte(self, x, y, s, taille=11, coul=None, ancre="start", gras=False, italique=False, rot=None):
        tr = (' transform="rotate(%d %.1f %.1f)"' % (rot, x, y)) if rot else ""
        self.add('<text x="%.1f" y="%.1f" font-size="%.1f" fill="%s" text-anchor="%s"%s%s%s>%s</text>' % (
            x, y, taille, coul or C["texte"], ancre, ' font-weight="bold"' if gras else "",
            ' font-style="italic"' if italique else "", tr, esc(s)))

    def rendu(self):
        tete = ('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
                'font-family="%s">\n<rect width="%d" height="%d" fill="%s"/>\n'
                % (self.w, self.h, self.w, self.h, POLICE, self.w, self.h, C["fond"]))
        return tete + "\n".join(self.el) + "\n</svg>\n"


class Placeur:
    """pose des etiquettes sans chevauchement (boites approximatives)"""

    def __init__(self):
        self.boites = []

    @staticmethod
    def _chev(a, b):
        return not (a[2] <= b[0] or b[2] <= a[0] or a[3] <= b[1] or b[3] <= a[1])

    def libre(self, b):
        return all(not self._chev(b, o) for o in self.boites)

    def bloque(self, b):
        self.boites.append(b)

    def boite(self, x, y, s, taille, ancre, gras=False):
        w = largeur_texte(s, taille, gras)
        x0 = x if ancre == "start" else (x - w / 2 if ancre == "middle" else x - w)
        return (x0 - 2, y - taille * 0.84, x0 + w + 2, y + taille * 0.26)

    def place(self, svg, x, y, s, taille, candidats, coul=None, gras=False, fond=True):
        choix = None
        for dx, dy, ancre in candidats:
            b = self.boite(x + dx, y + dy, s, taille, ancre, gras)
            if self.libre(b):
                choix = (dx, dy, ancre, b)
                break
        if choix is None:
            dx, dy, ancre = candidats[0]
            choix = (dx, dy, ancre, self.boite(x + dx, y + dy, s, taille, ancre, gras))
            print("  (etiquette forcee : %s)" % s)
        dx, dy, ancre, b = choix
        self.bloque(b)
        if fond:
            svg.rect(b[0], b[1], b[2] - b[0], b[3] - b[1], "#ffffff", op=0.86, rx=2)
        svg.texte(x + dx, y + dy, s, taille, coul, ancre, gras)
        return b


def autour(d, taille=10):
    """positions autour d'un point, a la distance d (px)"""
    h = taille * 0.36
    return [(d, h, "start"), (-d, h, "end"), (0, -d - 1, "middle"), (0, d + taille * 0.8, "middle"),
            (d * 0.8, -d * 0.8, "start"), (-d * 0.8, -d * 0.8, "end"), (d * 0.8, d * 0.8 + taille * 0.7, "start"),
            (-d * 0.8, d * 0.8 + taille * 0.7, "end"), (d * 1.9, h, "start"), (-d * 1.9, h, "end"),
            (0, -d * 1.9, "middle"), (0, d * 1.9 + taille * 0.8, "middle")]


# ----------------------------------------------------------------------------- geometrie d'une vue
class Vue:
    def __init__(self, plan, cote):
        self.plan = plan
        self.carte = plan["carte"]
        self.reg = REGLAGES[self.carte]
        self.miroir = cote == "soudures"
        self.k = self.reg["k"]
        self.ML, self.MR, self.MT, self.MB = self.reg["marges"]
        self.nc, self.nr = plan["colonnes"], plan["rangees"]
        self.W = self.ML + (self.nc - 1) * PAS + self.MR
        self.H = self.MT + (self.nr - 1) * PAS + self.MB
        self.ox = self.oy = 0.0

    def mm(self, r, c):
        return (self.ML + (c - 1) * PAS, self.MT + r * PAS)

    def P(self, x, y):
        if self.miroir:
            x = self.W - x
        return (self.ox + x * self.k, self.oy + y * self.k)

    def T(self, t):
        r, c = trou(t)
        return self.P(*self.mm(r, c))

    def Tm(self, t):
        r, c = trou(t)
        return self.mm(r, c)

    def pts(self, liste):
        return [self.P(x, y) for x, y in liste]

    def a_moi(self, face):
        return (face == "dessous") == self.miroir

    def sx(self, dx):
        """sens horizontal a l'ecran d'un deplacement dx (mm) de la vue composants"""
        return -dx if self.miroir else dx


def style_etape(x):
    e = etape_de(x)
    if e == "7":
        return C["e7"], "6 4"
    if e == "5b":
        return C["e5b"], "6 4"
    if e == "reserve":
        return C["gris"], "2 3"
    return None, None


def suffixe_etape(x):
    return {"7": " (7)", "5b": " (5b)"}.get(etape_de(x), "")


# ----------------------------------------------------------------------------- composants, vue composants
def tulipe(svg, vue, t, coul_et=None, dash=None):
    X, Y = vue.T(t)
    k = vue.k
    svg.rect(X - 1.15 * k, Y - 1.15 * k, 2.3 * k, 2.3 * k, "#d8d8d8" if not coul_et else "#ffffff",
             coul_et or "#555555", 1.6, rx=2, dash=dash)
    svg.cercle(X, Y, 0.6 * k, "#e9c46a" if not coul_et else "#ffffff", coul_et or "#8a6d1f", 1.2)


def geo_resistance(vue, br):
    a, b = vue.Tm(br["1"]), vue.Tm(br["2"])
    tx, ty = unit(a[0], a[1], b[0], b[1])
    d = math.hypot(b[0] - a[0], b[1] - a[1])
    L = min(6.4, d - 2.0)
    cx, cy = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    return a, b, tx, ty, L, cx, cy


def corps_resistance(svg, vue, cp, pl):
    k = vue.k
    br = cp["broches"]
    note = plat(cp.get("note", ""))
    coul_et, dash = style_etape(cp)
    if "tulipe" in note:
        construction = "construction" in note
        for t in (br["1"], br["2"]):
            if construction:
                tulipe(svg, vue, t)
            else:
                tulipe(svg, vue, t, coul_et, dash)
    a, b, tx, ty, L, cx, cy = geo_resistance(vue, br)
    e1 = (cx - tx * L / 2, cy - ty * L / 2)
    e2 = (cx + tx * L / 2, cy + ty * L / 2)
    svg.ligne(vue.pts([a, e1]), coul_et or "#6b6b6b", 0.5 * k)
    svg.ligne(vue.pts([e2, b]), coul_et or "#6b6b6b", 0.5 * k)
    corps = rectangle_oriente(cx, cy, tx, ty, L, 2.4)
    svg.poly(vue.pts(corps), "#ecd9ae" if not coul_et else "#ffffff", coul_et or "#8a6d3b", 1.6, dash=dash)
    pl.bloque(boite_de(vue.pts(corps)))


def texte_resistance(svg, vue, cp, pl):
    k = vue.k
    coul_et, _ = style_etape(cp)
    a, b, tx, ty, L, cx, cy = geo_resistance(vue, cp["broches"])
    lib = "%s %s%s" % (nom_ref(cp["ref"]), valeur_courte(cp.get("valeur")), suffixe_etape(cp))
    X, Y = vue.P(cx, cy)
    taille = 11
    horizontal = abs(tx) > abs(ty)
    if horizontal and largeur_texte(lib, taille, True) < L * k - 6:
        svg.texte(X, Y + 4, lib, taille, coul_et or C["texte"], "middle", gras=True)
        return
    d = 1.3 * k + 3
    if horizontal:
        cands = [(0, -d - 1, "middle"), (0, d + 10, "middle")] + autour(d + 4, taille)
    else:
        cands = [(d, 4, "start"), (-d, 4, "end")] + autour(d + 4, taille)
    pl.place(svg, X, Y, lib, taille, cands, coul=coul_et or C["texte"], gras=True)


def geo_support(vue, cp):
    br = cp["broches"]
    m = re.search(r"corps (?:sur|au-dessus de) ([A-Z]\d+)", cp.get("note", ""))
    corps_t = m.group(1) if m and m.group(1) in br.values() else br.get("2")
    autre_t = [t for t in br.values() if t != corps_t][0]
    return corps_t, autre_t


def corps_support(svg, vue, cp, pl):
    k = vue.k
    br = cp["broches"]
    coul_et, dash = style_etape(cp)
    for t in br.values():
        tulipe(svg, vue, t, coul_et, dash)
        X, Y = vue.T(t)
        pl.bloque((X - 1.2 * k, Y - 1.2 * k, X + 1.2 * k, Y + 1.2 * k))
    corps_t, autre_t = geo_support(vue, cp)
    Xc, Yc = vue.T(corps_t)
    Xa, Ya = vue.T(autre_t)
    dxp, dyp = Xa - Xc, Ya - Yc
    mx, my = (Xc + Xa) / 2 + dyp * 0.45, (Yc + Ya) / 2 - dxp * 0.45
    svg.courbe("M%.1f %.1f Q%.1f %.1f %.1f %.1f" % (Xc, Yc, mx, my, Xa, Ya), coul_et or "#6b6b6b", 0.45 * k)
    svg.cercle(Xc, Yc, 1.3 * k, "#ecd9ae" if not coul_et else "#ffffff", coul_et or "#8a6d3b", 1.8, dash=dash)
    pl.bloque((Xc - 1.35 * k, Yc - 1.35 * k, Xc + 1.35 * k, Yc + 1.35 * k))


def texte_support(svg, vue, cp, pl):
    k = vue.k
    coul_et, _ = style_etape(cp)
    corps_t, autre_t = geo_support(vue, cp)
    Xc, Yc = vue.T(corps_t)
    Xa, Ya = vue.T(autre_t)
    lib = "%s %s%s" % (nom_ref(cp["ref"]), valeur_courte(cp.get("valeur")), suffixe_etape(cp))
    d = 1.6 * k
    if abs(Ya - Yc) > abs(Xa - Xc):
        cands = [(d, 4, "start"), (-d, 4, "end"), (0, (-d - 2) if Ya > Yc else (d + 10), "middle")]
    else:
        cands = [(0, -d - 2, "middle"), (0, d + 10, "middle"),
                 ((-d if Xa > Xc else d), 4, "end" if Xa > Xc else "start")]
    pl.place(svg, Xc, Yc, lib, 10.5, cands + autour(d + 6, 10.5), coul=coul_et or C["texte"], gras=True)
    svg.texte(Xc, Yc + 3.5, "debout", 8.5, coul_et or "#6b4f1d", "middle")


def geometrie_to92(vue, br):
    """contour TO-92 (mm, vue composants) ; la face plate est vers n : C a gauche quand on la regarde"""
    c, b, e = vue.Tm(br["C"]), vue.Tm(br["B"]), vue.Tm(br["E"])
    tx, ty = unit(c[0], c[1], e[0], e[1])
    nx, ny = -ty, tx
    R, a = 2.45, 1.05
    t1 = math.atan2(a, math.sqrt(R * R - a * a))
    t2 = math.pi - t1
    pts = []
    for i in range(41):
        th = t2 + (2 * math.pi + t1 - t2) * i / 40.0
        pts.append((b[0] + R * (math.cos(th) * tx + math.sin(th) * nx),
                    b[1] + R * (math.cos(th) * ty + math.sin(th) * ny)))
    h = math.sqrt(R * R - a * a)
    plat_ = [(b[0] + a * nx - h * tx, b[1] + a * ny - h * ty), (b[0] + a * nx + h * tx, b[1] + a * ny + h * ty)]
    return pts, plat_, (nx, ny), b


def corps_bc547(svg, vue, cp, pl):
    k = vue.k
    br = cp["broches"]
    coul_et, dash = style_etape(cp)
    pts, plat_, n, b = geometrie_to92(vue, br)
    svg.poly(vue.pts(pts), "#3a3a3a" if not coul_et else "#ffffff", coul_et or "#111111", 1.4, dash=dash)
    svg.ligne(vue.pts(plat_), coul_et or C["plat"], 0.5 * k, cap="butt")
    for nom in ("C", "B", "E"):
        X, Y = vue.T(br[nom])
        svg.cercle(X, Y, 0.5 * k, "#ffffff" if not coul_et else "#ffffff", coul_et or "#111111", 1.2)
        svg.texte(X, Y + 3.4, nom, 9.5, coul_et or C["texte"], "middle", gras=True)
    pl.bloque(boite_de(vue.pts(pts)))


def texte_bc547(svg, vue, cp, pl):
    k = vue.k
    br = cp["broches"]
    coul_et, _ = style_etape(cp)
    pts, plat_, n, b = geometrie_to92(vue, br)
    Xb, Yb = vue.P(*b)
    lib = "%s BC547%s" % (cp["ref"], suffixe_etape(cp))
    nsx, nsy = vue.sx(n[0]), n[1]
    d = 2.9 * k
    if abs(nsx) > abs(nsy):
        cands = [(-nsx * d, 4, "start" if nsx < 0 else "end"), (0, -d, "middle"), (0, d + 10, "middle")]
    else:
        cands = [(0, -nsy * d + (10 if nsy < 0 else 0), "middle"), (d, 4, "start"), (-d, 4, "end")]
    pl.place(svg, Xb, Yb, lib, 11, cands + autour(d + 8, 11), coul=coul_et or C["texte"], gras=True)
    fx, fy = (plat_[0][0] + plat_[1][0]) / 2, (plat_[0][1] + plat_[1][1]) / 2
    Xf, Yf = vue.P(fx + n[0] * 0.5, fy + n[1] * 0.5)
    if abs(nsx) > abs(nsy):
        cands = [(nsx * 4, 4, "start" if nsx > 0 else "end"), (0, -0.9 * k, "middle"), (0, 1.4 * k, "middle")]
    else:
        cands = [(0, 11 if nsy > 0 else -4, "middle"), (2.4 * k, 4, "start"), (-2.4 * k, 4, "end")]
    pl.place(svg, Xf, Yf, "face plate", 9.5, cands, coul=coul_et or "#9a6a00", gras=True)


def geo_supermini(vue, br):
    P5, P8, PTX, P7 = vue.Tm(br["5V"]), vue.Tm(br["8"]), vue.Tm(br["TX"]), vue.Tm(br["7"])
    ux, uy = unit(P5[0], P5[1], P8[0], P8[1])
    vx, vy = unit(P5[0], P5[1], PTX[0], PTX[1])
    cx = (P5[0] + P8[0] + PTX[0] + P7[0]) / 4
    cy = (P5[1] + P8[1] + PTX[1] + P7[1]) / 4

    def pt(a, b):
        return (cx + ux * a + vx * b, cy + uy * a + vy * b)
    return pt, (cx, cy)


def corps_supermini(svg, vue, cp, pl, fantome=False):
    k = vue.k
    br = cp["broches"]
    pt, centre = geo_supermini(vue, br)
    L, Wd = 25.6, 17.7
    corps = [pt(-L / 2, -Wd / 2), pt(L / 2, -Wd / 2), pt(L / 2, Wd / 2), pt(-L / 2, Wd / 2)]
    usb = [pt(-L / 2 - 1.1, -4.5), pt(-L / 2 + 6.2, -4.5), pt(-L / 2 + 6.2, 4.5), pt(-L / 2 - 1.1, 4.5)]
    ant = [pt(L / 2 - 2.6, -3.2), pt(L / 2 - 0.8, -3.2), pt(L / 2 - 0.8, 3.2), pt(L / 2 - 2.6, 3.2)]
    puce = [pt(-1.0, -3.0), pt(5.0, -3.0), pt(5.0, 3.0), pt(-1.0, 3.0)]
    if fantome:
        svg.poly(vue.pts(corps), "none", "#b9a3d3", 1.6, dash="6 4")
        svg.poly(vue.pts(usb), "none", "#c0c0c0", 1.2, dash="4 3")
        return
    svg.poly(vue.pts(corps), "#efe6f8", C["violet_module"], 2.2)
    svg.poly(vue.pts(puce), "#ddd0ec", "#9a86b8", 1)
    svg.poly(vue.pts(usb), "#dcdcdc", "#555555", 1.6)
    svg.poly(vue.pts(ant), "#f7f0c6", "#8f7d2a", 1.4)
    X, Y = vue.P(*pt(-L / 2 + 2.6, 0))
    svg.texte(X, Y + 4, "USB-C", 12, C["texte"], "middle", gras=True)
    X, Y = vue.P(*pt(L / 2 - 4.6, 0))
    svg.texte(X, Y + 4, "antenne", 11, "#6b5b12", "middle", gras=True)
    X, Y = vue.P(*pt(2.0, 0))
    svg.texte(X, Y - 4, "U1", 14, C["violet_module"], "middle", gras=True)
    svg.texte(X, Y + 11, "SuperMini", 10.5, C["violet_module"], "middle")
    svg.texte(X, Y + 24, "(sonde)", 10.5, C["violet_module"], "middle")
    for nom, t in br.items():
        X, Y = vue.T(t)
        svg.rect(X - 0.95 * k, Y - 0.95 * k, 1.9 * k, 1.9 * k, "#2b2b2b", None, rx=2)
        svg.cercle(X, Y, 0.42 * k, "#e9c46a")
        role = role_broche(nom)
        px, py = vue.Tm(t)
        dirx = vue.sx(centre[0] - px)
        svg.texte(X + (1.3 * k if dirx > 0 else -1.3 * k), Y + 4, nom, 11 if role[1] else 10, role[0],
                  "start" if dirx > 0 else "end", gras=bool(role[1]))
    pl.bloque(boite_de(vue.pts(corps)))


def role_broche(nom):
    return {
        "6": (C["vert"], True), "7": (C["e7"], True), "GND": (C["noir"], True), "5V": (C["orange"], True),
        "3V3": (C["bleu"], True), "0": ("#4a79b8", True), "1": ("#4a79b8", True),
    }.get(nom, ("#8a8a8a", False))


def geo_xh(vue, cp):
    br = cp["broches"]
    pos = [vue.Tm(t) for t in br.values()]
    xs = [q[0] for q in pos]
    ys = [q[1] for q in pos]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    vertical = (max(ys) - min(ys)) > (max(xs) - min(xs))
    n = len(pos)
    long_ = (n - 1) * 2.5 + 4.9
    tx, ty = (0, 1) if vertical else (1, 0)
    return cx, cy, vertical, long_, tx, ty


def corps_xh(svg, vue, cp, pl):
    k = vue.k
    br = cp["broches"]
    cx, cy, vertical, long_, tx, ty = geo_xh(vue, cp)
    corps = rectangle_oriente(cx, cy, tx, ty, long_, 5.75)
    svg.poly(vue.pts(corps), "#fbf7ee", "#5a5a5a", 2)
    if cp["type"] == "xh4":  # verrou vers le bord le plus proche (X1 : vers l'exterieur)
        s = -1 if cx < vue.W / 2 else 1
        ver = rectangle_oriente(cx + s * 3.3, cy, tx, ty, long_ * 0.55, 0.8)
        svg.poly(vue.pts(ver), "#5a5a5a", "#5a5a5a", 1)
    for nom, t in br.items():
        X, Y = vue.T(t)
        svg.rect(X - 0.4 * k, Y - 0.4 * k, 0.8 * k, 0.8 * k, "#c9a227", "#6b5712", 1)
        if cp["type"] == "xh4":
            num, noeud, coul = NOM_X1[nom]
            lib = "%s %s" % (num, noeud)
        else:
            lib, coul = SIGNE[nom], FILS_CN3[nom]
        s_int = 1 if cx < vue.W / 2 else -1
        d = vue.sx(s_int)
        cote = 0.75 * k if cp["type"] == "xh4" else 0.6 * k
        svg.rect(X + d * cote - (0 if d > 0 else 10), Y - 5, 10, 10, COULEUR_FIL[coul], "#333333", 1, rx=2)
        svg.texte(X + d * (cote + 13), Y + 4.5, lib, 12 if cp["type"] == "xh3" else 11.5, C["texte"],
                  "start" if d > 0 else "end", gras=True)
    pl.bloque(boite_de(vue.pts(corps), 1))


def texte_xh(svg, vue, cp, pl):
    k = vue.k
    cx, cy, vertical, long_, tx, ty = geo_xh(vue, cp)
    X, Y = vue.P(cx, cy - long_ / 2)
    n = len(cp["broches"])
    lib = "%s (XH %d br.)" % (cp["ref"], n)
    pl.place(svg, X, Y, lib, 11.5, [(0, -6, "middle"), (vue.sx(3.0) * k, -6, "start"), (vue.sx(-3.0) * k, -6, "end")]
             + autour(12, 11.5), gras=True)
    if cp["type"] == "xh4":
        s = -1 if cx < vue.W / 2 else 1
        Xv, Yv = vue.P(cx + s * 3.3, cy + long_ / 2)
        pl.place(svg, Xv, Yv, "verrou", 10, [(0, 13, "middle"), (0, 25, "middle")], coul=C["gris"])
    else:
        Xv, Yv = vue.P(cx, cy + long_ / 2)
        pl.place(svg, Xv, Yv, "sens imposé : voir le gabarit", 10.5,
                 [(0, 15, "middle"), (0, 28, "middle"), (vue.sx(-2.9) * k, 15, "end"), (vue.sx(-2.9) * k, 28, "end")],
                 coul=C["rouge"], gras=True)


def corps_picot(svg, vue, cp, pl):
    k = vue.k
    X, Y = vue.T(cp["broches"]["1"])
    svg.rect(X - 1.25 * k, Y - 1.25 * k, 2.5 * k, 2.5 * k, "#1e1e1e", None, rx=1.5)
    svg.rect(X - 0.35 * k, Y - 0.35 * k, 0.7 * k, 0.7 * k, "#e9c46a")
    pl.bloque((X - 1.3 * k, Y - 1.3 * k, X + 1.3 * k, Y + 1.3 * k))


def texte_picot(svg, vue, cp, pl):
    k = vue.k
    X, Y = vue.T(cp["broches"]["1"])
    nom = NOM_PICOT.get(cp["ref"], cp["ref"])
    coul = COULEUR_FIL[COUL_PICOT.get(cp["ref"], "noir")]
    texte_coul = "#ffffff" if coul in (C["noir"], C["rouge"], C["marron"]) else C["texte"]
    w = largeur_texte(nom, 12, True) + 10
    h = 19
    cands = [(1.5 * k, -h / 2), (-1.5 * k - w, -h / 2), (-w / 2, -1.5 * k - h), (-w / 2, 1.5 * k),
             (1.5 * k, -1.5 * k - h + 4), (-1.5 * k - w, -1.5 * k - h + 4), (1.5 * k, 1.5 * k - 4),
             (-1.5 * k - w, 1.5 * k - 4)]
    b = None
    for dx, dy in cands:
        bb = (X + dx, Y + dy, X + dx + w, Y + dy + h)
        if pl.libre(bb):
            b = bb
            break
    if b is None:
        b = (X + cands[0][0], Y + cands[0][1], X + cands[0][0] + w, Y + cands[0][1] + h)
        print("  (etiquette forcee : %s)" % nom)
    pl.bloque(b)
    svg.rect(b[0], b[1], w, h, coul, "#333333", 1.2, rx=4)
    svg.texte(b[0] + w / 2, b[1] + 14, nom, 12, texte_coul, "middle", gras=True)


def geo_barrette(vue, cp):
    br = cp["broches"]
    pos = [vue.Tm(t) for t in br.values()]
    xs = [q[0] for q in pos]
    ys = [q[1] for q in pos]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    vertical = (max(ys) - min(ys)) > (max(xs) - min(xs))
    tx, ty = (0, 1) if vertical else (1, 0)
    return cx, cy, vertical, tx, ty, len(pos)


def barrette_fermee(cp):
    note = plat(cp.get("note", ""))
    return cp["type"] == "barrette2" and ("ferme par defaut" in note or "cavalier ferme" in note)


def corps_barrette(svg, vue, cp, pl):
    k = vue.k
    br = cp["broches"]
    coul_et, dash = style_etape(cp)
    cx, cy, vertical, tx, ty, n = geo_barrette(vue, cp)
    corps = rectangle_oriente(cx, cy, tx, ty, n * 2.54, 2.5)
    svg.poly(vue.pts(corps), "#1e1e1e" if not coul_et else "#ffffff", coul_et or "#1e1e1e", 1.4, dash=dash)
    ferme = barrette_fermee(cp)
    if ferme:
        cav = rectangle_oriente(cx, cy, tx, ty, 2 * 2.54 + 0.5, 2.9)
        svg.poly(vue.pts(cav), "#1d3a8a", "#0e1f4d", 1.4)
    for num, t in br.items():
        X, Y = vue.T(t)
        if ferme:
            continue
        svg.rect(X - 0.35 * k, Y - 0.35 * k, 0.7 * k, 0.7 * k, "#e9c46a" if not coul_et else coul_et)
        if cp["type"] == "analyseur3":
            lib = num
        elif cp["ref"] == "J2":
            lib = {"1": "+", "2": "5V"}.get(num, num)
        else:
            lib = num
        coul_txt = "#ffffff" if not coul_et else coul_et
        if vertical:
            svg.texte(X + 0.52 * k, Y + 3.5, lib, 8.5, coul_txt, "start", gras=True)
        else:
            svg.texte(X, Y - 0.52 * k, lib, 8.5, coul_txt, "middle", gras=True)
    pl.bloque(boite_de(vue.pts(corps), 1))


def texte_barrette(svg, vue, cp, pl):
    k = vue.k
    coul_et, _ = style_etape(cp)
    cx, cy, vertical, tx, ty, n = geo_barrette(vue, cp)
    X, Y = vue.P(cx, cy)
    if cp["type"] == "barrette2":
        ferme = barrette_fermee(cp)
        lib = "%s : cavalier fermé" % cp["ref"] if ferme else "%s OUVERT" % cp["ref"]
        d = 1.9 * k
        cands = [(d, 4, "start"), (-d, 4, "end"), (0, -n * 1.27 * k - 5, "middle"), (0, n * 1.27 * k + 13, "middle")]
        pl.place(svg, X, Y, lib, 11.5, cands + autour(d + 10, 11.5), coul=C["rouge"] if not ferme else "#1d3a8a",
                 gras=True)
    else:
        lib = "%s%s" % (cp["ref"], suffixe_etape(cp))
        d = 1.9 * k
        cands = [(0, -d - 2, "middle"), (0, d + 11, "middle"), (n * 1.27 * k + 6, 4, "start"),
                 (-n * 1.27 * k - 6, 4, "end")]
        pl.place(svg, X, Y, lib, 11, cands + autour(d + 10, 11), coul=coul_et or C["texte"], gras=True)


def fils_entrants(svg, vue, cp, pale_=False):
    """fils de la fiche CN3 (colonne 1) ou du cable de sortie (colonne 2), entres cote composants"""
    k = vue.k
    br = cp["broches"]
    cn3 = cp["type"] == "fils_cn3"
    table = FILS_CN3 if cn3 else FILS_SORTIE
    for nom, t in br.items():
        x, y = vue.Tm(t)
        coul = COULEUR_FIL[table[nom]]
        x0 = -(vue.reg["fils_ext"] - 8) / vue.k
        if cn3:
            chemin = [(x0, y - 0.45), (x - 0.9, y - 0.45), (x, y)]
            ep = 0.36 * k
        else:
            chemin = [(x0, y + 0.55), (x - 1.5, y + 0.55), (x, y)]
            ep = 0.46 * k
        if pale_:
            svg.ligne(vue.pts(chemin), pale(coul if table[nom] != "blanc" else "#999999", 0.62), 0.25 * k)
            continue
        svg.ligne(vue.pts(chemin), "#222222", ep + 3)
        svg.ligne(vue.pts(chemin), coul, ep)
        X, Y = vue.T(t)
        svg.cercle(X, Y, 0.34 * k, "#9aa0a6", "#333333", 1)


def texte_fils_entrants(svg, vue, cp, pl):
    br = cp["broches"]
    cn3 = cp["type"] == "fils_cn3"
    for nom, t in br.items():
        x, y = vue.Tm(t)
        X, Y = vue.P(-(vue.reg["fils_ext"] - 8) / vue.k, y + (-0.45 if cn3 else 0.55))
        lib = ("fiche CN3 %s" % SIGNE[nom]) if cn3 else ("câble %s" % NOM_SORTIE[nom])
        svg.texte(X + 2, Y - 0.36 * vue.k - 3, lib, 11, C["texte"], "start", gras=True)


CORPS = {"resistance": corps_resistance, "support2": corps_support, "BC547": corps_bc547,
         "supermini": corps_supermini, "xh4": corps_xh, "xh3": corps_xh, "picot": corps_picot,
         "barrette2": corps_barrette, "analyseur3": corps_barrette}
TEXTES = {"resistance": texte_resistance, "support2": texte_support, "BC547": texte_bc547,
          "xh4": texte_xh, "xh3": texte_xh, "picot": texte_picot, "barrette2": texte_barrette,
          "analyseur3": texte_barrette, "fils_cn3": texte_fils_entrants, "sortie4": texte_fils_entrants}


# ----------------------------------------------------------------------------- vue soudures
def fantome(svg, vue, cp):
    """contour pale d'une piece posee de l'autre cote"""
    t = cp["type"]
    br = cp["broches"]
    coul = "#b7ab92"
    if t == "supermini":
        corps_supermini(svg, vue, cp, None, fantome=True)
    elif t == "BC547":
        pts, plat_, n, b = geometrie_to92(vue, br)
        svg.poly(vue.pts(pts), "none", coul, 1.3, dash="4 3")
        svg.ligne(vue.pts(plat_), "#e8c983", 3, cap="butt")
    elif t == "resistance":
        a, b, tx, ty, L, cx, cy = geo_resistance(vue, br)
        svg.poly(vue.pts(rectangle_oriente(cx, cy, tx, ty, L, 2.4)), "none", coul, 1.3, dash="4 3")
    elif t in ("xh4", "xh3"):
        cx, cy, vertical, long_, tx, ty = geo_xh(vue, cp)
        svg.poly(vue.pts(rectangle_oriente(cx, cy, tx, ty, long_, 5.75)), "none", coul, 1.3, dash="4 3")
    elif t in ("barrette2", "analyseur3"):
        cx, cy, vertical, tx, ty, n = geo_barrette(vue, cp)
        svg.poly(vue.pts(rectangle_oriente(cx, cy, tx, ty, n * 2.54, 2.5)), "none", coul, 1.3, dash="4 3")
    elif t in ("fils_cn3", "sortie4"):
        fils_entrants(svg, vue, cp, pale_=True)


def soudures_composant(svg, vue, cp, pl):
    k = vue.k
    coul_et, dash = style_etape(cp)
    note = plat(cp.get("note", ""))
    construction = cp["type"] == "resistance" and "tulipe" in note and "construction" in note
    for nom, t in cp["broches"].items():
        X, Y = vue.T(t)
        if coul_et and not construction:
            svg.cercle(X, Y, 1.0 * k, "#ffffff", coul_et, 1.8, dash="4 3")
            svg.cercle(X, Y, 0.42 * k, "#ffffff", coul_et, 1)
        else:
            svg.cercle(X, Y, 1.0 * k, C["etain"], C["etain_bord"], 1.2)
            svg.cercle(X, Y, 0.28 * k, "#6d6d6d")
        pl.bloque((X - 0.95 * k, Y - 0.95 * k, X + 0.95 * k, Y + 0.95 * k))


def textes_soudures(svg, vue, pl, comps):
    k = vue.k
    for cp in comps:
        coul_et, _ = style_etape(cp)
        typ = cp["type"]
        br = cp["broches"]
        for nom, t in br.items():
            X, Y = vue.T(t)
            if typ == "supermini":
                role = role_broche(nom)
                pt, centre = geo_supermini(vue, br)
                px, _ = vue.Tm(t)
                dirx = vue.sx(centre[0] - px)
                d = 1.2 * k
                cands = [(d if dirx > 0 else -d, 4, "start" if dirx > 0 else "end"),
                         (0, -d - 1, "middle"), (0, d + 9, "middle"),
                         (-d if dirx > 0 else d, 4, "end" if dirx > 0 else "start")]
                pl.place(svg, X, Y, nom, 10.5 if role[1] else 9.5, cands, coul=role[0], gras=bool(role[1]))
                continue
            if typ == "picot":
                lib = NOM_PICOT.get(cp["ref"], cp["ref"])
            elif typ == "BC547":
                lib = "%s.%s" % (cp["ref"], nom)
            elif typ == "xh4":
                lib = "X1.%s %s" % (NOM_X1[nom][0], NOM_X1[nom][1])
            elif typ == "xh3":
                lib = "XH3 %s" % SIGNE[nom]
            elif typ == "fils_cn3":
                lib = "CN3 %s" % SIGNE[nom]
            elif typ == "sortie4":
                lib = "câble %s" % NOM_SORTIE[nom]
            elif typ == "barrette2":
                lib = "%s.%s" % (cp["ref"], nom)
                if cp["ref"] == "J2":
                    lib += {"1": " (+)", "2": " (5V)"}.get(nom, "")
            elif typ == "analyseur3":
                lib = "ANA.%s" % nom
            else:
                lib = nom_ref(cp["ref"])
            pl.place(svg, X, Y, lib, 9.5, autour(1.05 * k, 9.5), coul=coul_et or C["texte"], gras=True)


# ----------------------------------------------------------------------------- liaisons
def segments(vue, l):
    chemin = l.get("chemin") or [l["de"], l["a"]]
    return [vue.T(t) for t in chemin]


def obstacles_liaison(vue, l, pl):
    k = vue.k
    pts = segments(vue, l)
    w = {"pont": 0.6, "fil_nu": 0.35, "fil_isole": 0.6}.get(l.get("genre"), 0.4) * k
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        pl.bloque((min(x1, x2) - w, min(y1, y2) - w, max(x1, x2) + w, max(y1, y2) + w))


def dessine_liaison(svg, vue, l, pl=None, pale_=False):
    k = vue.k
    genre = l.get("genre")
    pts = segments(vue, l)
    e = etape_de(l)
    coul_et = C["e7"] if e == "7" else (C["e5b"] if e == "5b" else None)
    dash = "7 5" if coul_et else None
    if pale_:
        if genre == "fil_isole":
            coul = COULEUR_FIL.get(couleur_dans(l.get("note", "")) or "", "#888888")
            svg.ligne(pts, pale(coul, 0.6), 0.5 * k, dash=dash)
        else:
            svg.ligne(pts, pale(coul_et or "#8a8f95", 0.55), 0.28 * k if genre == "fil_nu" else 0.45 * k, dash=dash)
        return
    if genre == "pont":
        if coul_et:
            svg.ligne(pts, coul_et, 1.2 * k, dash="5 3")
            svg.ligne(pts, "#ffffff", 0.75 * k)
        else:
            svg.ligne(pts, C["etain_bord"], 1.3 * k)
            svg.ligne(pts, C["etain"], 0.98 * k)
    elif genre == "fil_nu":
        if coul_et:
            svg.ligne(pts, coul_et, 0.4 * k, dash=dash)
        else:
            svg.ligne(pts, C["nu_bord"], 0.5 * k)
            svg.ligne(pts, "#cfd4d9", 0.2 * k)
        for X, Y in (pts[0], pts[-1]):
            svg.cercle(X, Y, 0.6 * k, "#ffffff" if coul_et else C["etain"], coul_et or C["etain_bord"], 1.4,
                       dash="3 2" if coul_et else None)
    elif genre == "fil_isole":
        nomc = couleur_dans(l.get("note", "")) or "noir"
        coul = COULEUR_FIL[nomc]
        bord = "#000000" if nomc != "noir" else "#8a8a8a"
        svg.ligne(pts, bord, 0.9 * k + 2.4, dash=dash)
        svg.ligne(pts, coul, 0.9 * k, dash=dash)
        note = l.get("note", "")
        m = re.search(r"au-dessus de ([A-Z]\d+)", note)
        chemin = l.get("chemin") or [l["de"], l["a"]]
        if "manchon" in plat(note) and m:
            seg = segment_contenant(chemin, m.group(1))
            if seg:
                (x1, y1), (x2, y2) = vue.Tm(seg[0]), vue.Tm(seg[1])
                tx, ty = unit(x1, y1, x2, y2)
                cx, cy = vue.Tm(m.group(1))
                man = rectangle_oriente(cx, cy, tx, ty, 15.0, 1.5)
                svg.poly(vue.pts(man), "#ffffff", "#333333", 1.4, op=0.45)
                if pl is not None:
                    cands = []
                    for s_ in (-2.5, 2.5, -5.0, 5.0, 0.0):
                        Xm, Ym = vue.P(cx + tx * s_, cy + ty * s_)
                        for dx_, anc in ((1.0 * k, "start"), (-1.0 * k, "end")):
                            cands.append((Xm - vue.P(cx, cy)[0] + dx_, Ym - vue.P(cx, cy)[1] + 4, anc))
                    Xm, Ym = vue.P(cx, cy)
                    pl.place(svg, Xm, Ym, "manchon", 10.5, cands, coul="#333333", gras=True)
        for X, Y in (pts[0], pts[-1]):
            svg.cercle(X, Y, 0.6 * k, C["etain"], C["etain_bord"], 1.4)


def chemin_complet(chemin):
    tous = []
    for a, b in zip(chemin, chemin[1:]):
        (r1, c1), (r2, c2) = trou(a), trou(b)
        if r1 == r2:
            s = 1 if c2 > c1 else -1
            tous += ["%s%d" % (RANGEES[r1], c) for c in range(c1, c2 + s, s)]
        elif c1 == c2:
            s = 1 if r2 > r1 else -1
            tous += ["%s%d" % (RANGEES[r], c1) for r in range(r1, r2 + s, s)]
    return tous


def segment_contenant(chemin, t):
    for a, b in zip(chemin, chemin[1:]):
        if t in chemin_complet([a, b]):
            return a, b
    return None


# ----------------------------------------------------------------------------- zones
def style_zone(nom):
    """(remplissage, trait, pointille, libelle) ou None si la zone n'est pas dessinee"""
    n = plat(nom)
    if n in ("fixation", "gaine", "decoupe", "supermini", "fils isoles", "arret de traction", "epine de masse",
             "fils"):
        return None
    if "etape 7" in n or n.startswith("injection"):
        return ("#f7f0fb", C["e7"], "7 4", "zone de l'étape 7 (trous vides)")
    if "5b" in n or n.startswith("analyseur"):
        return ("#e9f6f5", C["e5b"], "7 4", "zone 5b (trous vides)")
    if "voie 2" in n:
        return ("#edf3fb", "#5b84c4", "3 3", "voie 2 (architecture B) : réservée, vide")
    if n.startswith("garde"):
        return ("#f1f1f1", "#9a9a9a", "2 3", "garde")
    if "couloir" in n:
        return ("#f6f6f6", "#9a9a9a", "2 3", "couloir USB-C : rien de haut")
    if n == "bus":
        return ("#fbecec", "#d9a0a0", None, "bus : +, D_panneau, D_carte, " + MOINS)
    if n.startswith("ecoute"):
        return ("#eef7ea", "#8fbf80", None, "écoute, voie 1")
    if n.startswith("separation"):
        return ("#ebe4d3", "#b8ab8c", "2 3", None)
    if n.startswith("fenetre"):
        return (None, "#333333", "5 3", "fenêtre" + (" (cavalier)" if "j1" in n else " (XH3)"))
    return ("#f3f3f3", "#999999", "3 3", nom)


def rect_zone(vue, z):
    (r1, c1), (r2, c2) = plage(z["trous"])
    m = 1.3
    x1, y1 = vue.mm(min(r1, r2), min(c1, c2))
    x2, y2 = vue.mm(max(r1, r2), max(c1, c2))
    return boite_de(vue.pts([(x1 - m, y1 - m), (x2 + m, y1 - m), (x2 + m, y2 + m), (x1 - m, y2 + m)]))


def dessine_zones(svg, vue):
    for z in vue.plan.get("zones", []):
        st = style_zone(z["nom"])
        if not st:
            continue
        fill, stroke, dash, lib = st
        X0, Y0, X1, Y1 = rect_zone(vue, z)
        svg.rect(X0, Y0, X1 - X0, Y1 - Y0, fill or "none", stroke, 1.3, rx=4, dash=dash)


def textes_zones(svg, vue, pl):
    for z in vue.plan.get("zones", []):
        st = style_zone(z["nom"])
        if not st or not st[3]:
            continue
        fill, stroke, dash, lib = st
        X0, Y0, X1, Y1 = rect_zone(vue, z)
        coul = stroke if stroke not in ("#999999", "#9a9a9a", "#333333") else C["gris"]
        w, h = X1 - X0, Y1 - Y0
        cands = [(3, -3, "start"), (w - 3, -3, "end"), (3, h + 11, "start"), (w - 3, h + 11, "end"),
                 (3, 11, "start"), (w - 3, 11, "end"), (3, h - 4, "start"), (w - 3, h - 4, "end"),
                 (w / 2, -3, "middle"), (w / 2, h + 11, "middle")]
        pl.place(svg, X0, Y0, lib, 10, cands, coul=coul)


# ----------------------------------------------------------------------------- legende
def symbole(svg, x, y, genre):
    if genre == "pastille":
        svg.cercle(x + 14, y, 7, C["cuivre"], C["cuivre_bord"], 0.8)
        svg.cercle(x + 14, y, 3.2, "#ffffff")
    elif genre == "soudure":
        svg.cercle(x + 14, y, 7.5, C["etain"], C["etain_bord"], 1.2)
        svg.cercle(x + 14, y, 2.2, "#6d6d6d")
    elif genre == "a_souder":
        svg.cercle(x + 14, y, 7.5, "#ffffff", C["e7"], 1.8, dash="4 3")
    elif genre == "pont":
        svg.ligne([(x + 4, y), (x + 24, y)], C["etain_bord"], 13)
        svg.ligne([(x + 4, y), (x + 24, y)], C["etain"], 10)
    elif genre == "fil_nu":
        svg.ligne([(x, y), (x + 28, y)], C["nu_bord"], 5)
        svg.ligne([(x, y), (x + 28, y)], "#cfd4d9", 2)
        svg.cercle(x, y, 5.5, C["etain"], C["etain_bord"], 1.3)
        svg.cercle(x + 28, y, 5.5, C["etain"], C["etain_bord"], 1.3)
    elif genre.startswith("isole:"):
        coul = COULEUR_FIL[genre.split(":")[1]]
        svg.ligne([(x, y), (x + 28, y)], "#000000" if coul != C["noir"] else "#8a8a8a", 11.5)
        svg.ligne([(x, y), (x + 28, y)], coul, 9)
    elif genre == "pale":
        svg.ligne([(x, y), (x + 28, y)], pale("#8a8f95", 0.55), 4)
    elif genre == "e7":
        svg.ligne([(x, y), (x + 28, y)], C["e7"], 4, dash="7 5")
    elif genre == "e5b":
        svg.ligne([(x, y), (x + 28, y)], C["e5b"], 4, dash="7 5")
    elif genre == "tulipe":
        svg.rect(x + 5, y - 9, 18, 18, "#d8d8d8", "#555555", 1.6, rx=2)
        svg.cercle(x + 14, y, 5, "#e9c46a", "#8a6d1f", 1.2)
    elif genre == "picot":
        svg.rect(x + 5, y - 9, 18, 18, "#1e1e1e", None, rx=1.5)
        svg.rect(x + 11, y - 3, 6, 6, "#e9c46a")
    elif genre == "bc547":
        svg.add('<path d="M%.1f %.1f A11 11 0 1 1 %.1f %.1f Z" fill="#3a3a3a" stroke="#111"/>'
                % (x + 4, y - 3, x + 24, y - 3))
        svg.ligne([(x + 4, y - 3), (x + 24, y - 3)], C["plat"], 4, cap="butt")
    elif genre == "feutre":
        svg.cercle(x + 14, y, 9, "#ffffff", C["feutre"], 2)
    elif genre == "zone":
        svg.rect(x + 1, y - 9, 26, 18, "#edf3fb", "#5b84c4", 1.3, rx=3, dash="3 3")
    elif genre == "fenetre":
        svg.rect(x + 1, y - 9, 26, 18, "none", "#333333", 1.3, rx=3, dash="5 3")
    elif genre == "vide":
        svg.rect(x + 1, y - 9, 26, 18, "#ebe4d3", "#b8ab8c", 1.3, rx=3, dash="2 3")


def dessine_legende(svg, vue, x, y, marques, x2):
    """legende sous la plaque : symboles a gauche (x), fils, marquages et notes a droite (x2)"""
    carte = vue.plan["carte"]
    y0 = y
    svg.texte(x, y + 6, "Légende", 16, gras=True)
    y += 32
    items = [("pastille", "pastille libre")]
    if vue.miroir:
        items += [("soudure", "patte d'une pièce : on la soude")]
        if carte == "boitier":
            items += [("a_souder", "patte posée plus tard (5b ou étape 7)")]
    items += [("pont", "pont de soudure (2 pastilles voisines)"),
              ("fil_nu", "fil nu étamé : soudé aux points ronds")]
    if carte == "boitier":
        items += [("isole:rouge", "fil isolé rouge : +  (côté soudures)"),
                  ("isole:vert", "fil isolé vert : GPIO6  (côté soudures)"),
                  ("isole:bleu", "fil isolé bleu : 3V3  (côté soudures)"),
                  ("isole:noir", "fil isolé noir : GND  (côté soudures)"),
                  ("isole:orange", "fil isolé orange : 5V  (CÔTÉ COMPOSANTS)"),
                  ("pale", "trait pâle : sur l'autre face"),
                  ("e7", "pointillé violet : étape 7 (plus tard)"),
                  ("e5b", "pointillé bleu-vert : option 5b (plus tard)"),
                  ("zone", "zone réservée : trous vides")]
        if not vue.miroir:
            items += [("tulipe", "contact tulipe : la pièce s'y enfiche"),
                      ("bc547", "BC547 : le trait jaune est la face plate"),
                      ("picot", "picot : point de test (grippe-fil)")]
    else:
        items += [("pale", "trait pâle : sur l'autre face"),
                  ("fenetre", "fenêtre à découper dans la gaine"),
                  ("vide", "rangée vide : sépare deux nœuds")]
    for g, t in items:
        symbole(svg, x, y, g)
        svg.texte(x + 40, y + 4.5, t, 12.5)
        y += 25
    y_fin = y
    y = y0 + 6
    if carte == "adaptateur":
        svg.texte(x2, y, "Fils, entrés côté composants par le bord de la colonne 1 :", 12.5, gras=True)
        y += 22
        for lib, cs in (("fiche CN3, colonne 1 :", [("noir", MOINS), ("blanc", "D (D_carte)"), ("rouge", "+")]),
                        ("câble de sortie, colonne 2 :", [("noir", "1 " + MOINS), ("blanc", "3 Dp"), ("marron", "4 Dc"),
                                                          ("rouge", "2 +")])):
            svg.texte(x2, y + 4, lib, 12)
            y += 20
            xx = x2 + 8
            for c_, n_ in cs:
                svg.rect(xx, y - 7, 14, 12, COULEUR_FIL[c_], "#333333", 1, rx=2)
                svg.texte(xx + 18, y + 4, n_, 12)
                xx += 26 + largeur_texte(n_, 12) + 6
            y += 24
        y += 6
    if marques and not vue.miroir:
        svg.texte(x2, y, "Marquages au feutre (côté composants) :", 12.5, gras=True)
        y += 22
        for i, e in enumerate(marques, 1):
            svg.cercle(x2 + 10, y - 4, 9, "#ffffff", C["feutre"], 2)
            svg.texte(x2 + 10, y, str(i), 10.5, C["feutre"], "middle", gras=True)
            lignes = coupe_texte("%s : %s" % (e["trou"], accents(e["texte"])), 60)
            for j, lg in enumerate(lignes):
                svg.texte(x2 + 26, y + j * 15, lg, 11.5, C["feutre"])
            y += 15 * len(lignes) + 6
    if vue.miroir:
        if carte == "boitier":
            lignes = ["Tout se soude de ce côté, sauf le fil orange (côté composants : pâle ici)."]
        else:
            lignes = ["Tous les ponts et fils nus sont de ce côté.",
                      "Les 7 fils arrivent côté composants (pâles ici)."]
        lignes += ["Point rond = on soude. Entre deux points, le fil nu passe",
                   "sur les pastilles sans étain."]
        for lg in lignes:
            svg.texte(x2, y, lg, 12.5, C["gris"])
            y += 19
    return max(y, y_fin)


# ----------------------------------------------------------------------------- une carte, une face
def dessine_carte(plan, cote, chemin):
    vue = Vue(plan, cote)
    reg = vue.reg
    k = vue.k
    fils_ext = reg["fils_ext"]
    lb, hb = vue.W * k, vue.H * k
    gauche = 70 + (fils_ext if not vue.miroir else 0)
    vue.ox, vue.oy = gauche, 136
    marques = plan.get("etiquettes", [])
    leg_y = vue.oy + hb + 64
    leg_x2 = 500
    tmp = Svg(10, 10)
    fin_legende = dessine_legende(tmp, vue, 24, leg_y, marques, leg_x2)
    largeur = max(gauche + lb + (fils_ext if vue.miroir else 0) + 70, leg_x2 + 470)
    hauteur = fin_legende + 24
    svg = Svg(largeur, hauteur)
    pl = Placeur()
    comps = plan["composants"]
    liais = plan.get("liaisons", [])

    face_txt = "vue côté composants" if not vue.miroir else "vue côté soudures (image miroir)"
    svg.texte(24, 36, "%s — %s" % (reg["titre"], face_txt), 23, gras=True)
    if plan["carte"] == "boitier":
        s1 = "Plaque 5 × 7 cm, 24 colonnes × 18 rangées (A à R), pas de 2,54 mm, pastilles isolées."
    else:
        s1 = ("Coin d'une plaque 5 × 7 : 8 colonnes × 7 rangées (A à G), environ 26 × 21 mm. "
              "Bords gauche et haut d'usine.")
    if vue.miroir:
        s2 = "Plaque retournée gauche-droite : la colonne 1 est à DROITE, le coin A1 en haut à droite."
    else:
        s2 = "Plaque posée pièces vers toi : la colonne 1 est à gauche, le coin A1 en haut à gauche."
    svg.texte(24, 62, s1, 13.5, C["gris"])
    svg.texte(24, 84, s2, 13.5, C["rouge"] if vue.miroir else C["gris"], gras=vue.miroir)
    if plan["carte"] == "boitier":
        svg.texte(24, 106, "Tout est dessiné : la construction en trait plein, l'option 5b et l'étape 7 en "
                           "pointillé.", 13.5, C["gris"])

    svg.poly(vue.pts([(0, 0), (vue.W, 0), (vue.W, vue.H), (0, vue.H)]), C["plaque"], C["plaque_bord"], 2)
    if plan["carte"] == "adaptateur":
        a, b, c_ = vue.P(vue.W, 0), vue.P(vue.W, vue.H), vue.P(0, vue.H)
        svg.ligne([a, b, c_], "#8a6d3b", 3, dash="7 5", cap="butt")
        xr, yr = vue.P(vue.W, vue.H / 2)
        dx = 44 if not vue.miroir else -44
        svg.texte(xr + dx, yr, "bord coupé, limé (colonne 9)", 12, C["gris"], "middle", italique=True, rot=90)
        xb, yb = vue.P(vue.W / 2, vue.H)
        svg.texte(xb, yb + 40, "bord coupé, limé (à travers la rangée H)", 12, C["gris"], "middle", italique=True)
    coins = []
    if reg["coins"] == "tous":
        coins = [(2.6, 2.0), (vue.W - 2.6, 2.0), (2.6, vue.H - 2.0), (vue.W - 2.6, vue.H - 2.0)]
    elif reg["coins"] == "haut-gauche":
        coins = [(2.6, 2.0)]
    for x, y in coins:
        X, Y = vue.P(x, y)
        svg.cercle(X, Y, 1.2 * k, "#ffffff", C["plaque_bord"], 1.5)
        pl.bloque((X - 1.2 * k, Y - 1.2 * k, X + 1.2 * k, Y + 1.2 * k))

    dessine_zones(svg, vue)

    for r in range(vue.nr):
        for c in range(1, vue.nc + 1):
            X, Y = vue.P(*vue.mm(r, c))
            svg.cercle(X, Y, 0.92 * k, C["cuivre"], C["cuivre_bord"], 0.8)
            svg.cercle(X, Y, 0.42 * k, "#ffffff")

    for c in range(1, vue.nc + 1):
        X, _ = vue.P(*vue.mm(0, c))
        _, Yh = vue.P(0, 0)
        _, Yb = vue.P(0, vue.H)
        fort = c in (1, vue.nc) or c % 5 == 0
        for Y in (Yh - 9, Yb + 20):
            svg.texte(X, Y, str(c), 12.5 if fort else 11.5, C["texte"] if fort else C["gris"], "middle", gras=fort)
    xg = vue.P(0, 0)[0] if not vue.miroir else vue.P(vue.W, 0)[0]
    xd = vue.P(vue.W, 0)[0] if not vue.miroir else vue.P(0, 0)[0]
    for r in range(vue.nr):
        _, Y = vue.P(*vue.mm(r, 1))
        svg.texte(xg - 12 - (fils_ext if not vue.miroir else 0), Y + 4.5, RANGEES[r], 12.5, C["texte"], "end", gras=True)
        svg.texte(xd + 12 + (fils_ext if vue.miroir else 0), Y + 4.5, RANGEES[r], 12.5, C["texte"], "start", gras=True)

    for c in range(1, vue.nc + 1):
        X, _ = vue.P(*vue.mm(0, c))
        for Y in (vue.P(0, 0)[1] - 9, vue.P(0, vue.H)[1] + 20):
            pl.bloque((X - 9, Y - 12, X + 9, Y + 4))
    for r in range(vue.nr):
        _, Y = vue.P(*vue.mm(r, 1))
        for X in (xg - 12 - (fils_ext if not vue.miroir else 0), xd + 12 + (fils_ext if vue.miroir else 0)):
            pl.bloque((X - 12, Y - 8, X + 12, Y + 7))

    for l in liais:
        if not vue.a_moi(l.get("face", "dessous")):
            dessine_liaison(svg, vue, l, pale_=True)
    if vue.miroir:
        for cp in comps:
            fantome(svg, vue, cp)

    for l in liais:
        if vue.a_moi(l.get("face", "dessous")):
            obstacles_liaison(vue, l, pl)
    if not vue.miroir:
        for e in marques:
            X, Y = vue.T(e["trou"])
            pl.bloque((X - 10, Y - 10, X + 10, Y + 10))

    if not vue.miroir:
        for cp in comps:
            if cp["type"] in ("fils_cn3", "sortie4"):
                fils_entrants(svg, vue, cp)
        for cp in comps:
            f = CORPS.get(cp["type"])
            if f:
                f(svg, vue, cp, pl)
        for l in liais:
            if vue.a_moi(l.get("face", "dessous")):
                dessine_liaison(svg, vue, l, pl)
        for i, e in enumerate(marques, 1):
            X, Y = vue.T(e["trou"])
            svg.cercle(X, Y, 9.5, "#ffffff", C["feutre"], 2)
            svg.texte(X, Y + 4, str(i), 11, C["feutre"], "middle", gras=True)
        for cp in comps:
            f = TEXTES.get(cp["type"])
            if f:
                f(svg, vue, cp, pl)
    else:
        for cp in comps:
            soudures_composant(svg, vue, cp, pl)
        ordre = {"fil_nu": 0, "pont": 1, "fil_isole": 2}
        for l in sorted(liais, key=lambda l: ordre.get(l.get("genre"), 3)):
            if vue.a_moi(l.get("face", "dessous")):
                dessine_liaison(svg, vue, l, pl)
        textes_soudures(svg, vue, pl, comps)
        for e in marques:
            if plat(e["texte"]).startswith("coin a1"):
                X, Y = vue.T("A1")
                svg.cercle(X, Y, 1.25 * k, "none", C["feutre"], 2.4)
                pl.place(svg, X, Y, "coin A1", 12, [(-1.5 * k, -1.3 * k, "end"), (0, -1.5 * k, "middle"),
                                                   (1.5 * k, -1.3 * k, "start")], coul=C["feutre"], gras=True)
                break
    textes_zones(svg, vue, pl)
    svg.ligne([(24, leg_y - 22), (largeur - 24, leg_y - 22)], "#dddddd", 1)
    dessine_legende(svg, vue, 24, leg_y, marques, leg_x2)
    with open(chemin, "w", encoding="utf-8") as f:
        f.write(svg.rendu())


# ----------------------------------------------------------------------------- SuperMini seule
def dessine_brochage(chemin):
    svg = Svg(1240, 830)
    svg.texte(24, 36, "ESP32-C6 SuperMini violette — brochage réel (relevé le 28/09)", 23, gras=True)
    svg.texte(24, 62, "Vue côté composants, USB-C à gauche, antenne céramique à droite. 10 pastilles par rangée ; "
                      "rangées à 6 trous (15,24 mm) l'une de l'autre.", 13.5, C["gris"])
    k = 17.0
    L, Wd = 25.6, 17.7
    x0, y0 = 300, 200

    def pad(i, rang):
        return (x0 + (1.35 + i * PAS) * k, y0 + ((Wd / 2 - 7.62) + (0 if rang == 0 else 15.24)) * k)

    svg.rect(x0, y0, L * k, Wd * k, "#efe6f8", C["violet_module"], 2.5, rx=8)
    svg.rect(x0 - 1.1 * k, y0 + (Wd / 2 - 4.5) * k, 7.3 * k, 9 * k, "#dcdcdc", "#555555", 2, rx=6)
    svg.texte(x0 + 2.55 * k, y0 + Wd / 2 * k + 5, "USB-C", 15, C["texte"], "middle", gras=True)
    svg.rect(x0 + (L - 2.6) * k, y0 + (Wd / 2 - 3.2) * k, 1.8 * k, 6.4 * k, "#f7f0c6", "#8f7d2a", 1.6)
    svg.texte(x0 + (L + 1.0) * k, y0 + Wd / 2 * k + 5, "antenne", 14, "#6b5b12", "start", gras=True)
    svg.rect(x0 + 10.5 * k, y0 + (Wd / 2 - 3) * k, 6 * k, 6 * k, "#ddd0ec", "#9a86b8", 1.2)
    svg.texte(x0 + 13.5 * k, y0 + Wd / 2 * k + 4, "C6", 12, "#6a5a8a", "middle", gras=True)
    roles_haut = {"15": "15 : à éviter", "9": "9 (BOOT) : à éviter", "8": "8 (LED) : à éviter"}
    roles_bas = {"TX": "TX (16) : à éviter", "RX": "RX (17) : à éviter", "0": "0 : voie 2, écoute (B)",
                 "1": "1 : voie 2, émission (B)", "2": "2", "3": "3", "4": "4 : à éviter", "5": "5 : à éviter",
                 "6": "6 : ÉCOUTE (fil vert)", "7": "7 : INJECTION (étape 7)"}
    for rang, noms in ((0, HAUT), (1, BAS)):
        for i, nom in enumerate(noms):
            X, Y = pad(i, rang)
            coul, important = role_broche(nom)
            svg.rect(X - 12, Y - 12, 24, 24, "#2b2b2b", None, rx=3)
            svg.cercle(X, Y, 6.5, "#e9c46a")
            svg.texte(X, Y + (29 if rang == 0 else -19), nom, 14, coul if important else "#7a7a7a", "middle",
                      gras=True)
            if rang == 0:
                if nom in ("5V", "GND", "3V3"):
                    lib = {"5V": "5V (1re)", "GND": "GND (2e)", "3V3": "3V3 (3e)"}[nom]
                    svg.texte(X + 3, Y - 22, lib, 13.5, coul, "start", gras=True, rot=-45)
                elif nom in roles_haut:
                    svg.texte(X + 3, Y - 22, roles_haut[nom], 13, "#8a8a8a", "start", rot=-45)
            elif roles_bas[nom] != nom:
                svg.texte(X + 3, Y + 26, roles_bas[nom], 13.5 if important else 13,
                          coul if important else "#8a8a8a", "start", gras=important, rot=45)
    xa = x0 - 70
    y1, y2 = pad(0, 0)[1], pad(0, 1)[1]
    svg.ligne([(xa, y1), (xa, y2)], "#444444", 1.2)
    svg.ligne([(xa - 6, y1), (xa + 6, y1)], "#444444", 1.2)
    svg.ligne([(xa - 6, y2), (xa + 6, y2)], "#444444", 1.2)
    svg.texte(xa - 10, (y1 + y2) / 2 - 4, "15,24 mm", 13, C["texte"], "end", gras=True)
    svg.texte(xa - 10, (y1 + y2) / 2 + 13, "(6 trous)", 12.5, C["gris"], "end")
    svg.texte(x0 + (L + 1.0) * k, y0 + Wd / 2 * k + 24, "carte d'environ 25,6 × 17,7 mm", 12.5, C["gris"],
              "start")
    bx, by = 24, 680
    lignes = [
        ("À retenir", True, C["texte"]),
        ("• GPIO6 et GPIO7 sont VOISINES, au bout de la rangée du bas, côté antenne : 6 est l'avant-dernière, "
         "7 la dernière (le coin).", True, C["rouge"]),
        ("  Les intervertir est facile et dangereux (WIRING §6) : le fil vert va sur 6 ; le 4,7k de l'injection, "
         "sur 7.", False, C["rouge"]),
        ("• Rangée du haut, depuis l'USB-C : 5V, GND, 3V3. Trous intérieurs 12, 13, 21, 22, 23 et pastille BAT : "
         "inutilisés.", False, C["texte"]),
        ("• Sur le boîtier, le module est tourné d'un quart de tour : USB-C vers le haut de la plaque, rangée TX…7 "
         "en colonne 18, rangée 5V…8 en colonne 24.", False, C["texte"]),
        ("• Toujours composants en haut : retourné, le module présenterait ses broches en miroir.", False,
         C["texte"]),
    ]
    for i, (lg, g, c_) in enumerate(lignes):
        svg.texte(bx, by + i * 24, lg, 15 if i == 0 else 13.5, c_, gras=g)
    with open(chemin, "w", encoding="utf-8") as f:
        f.write(svg.rendu())


# ----------------------------------------------------------------------------- banc de la tache 16
BB_RANGS = {"A": 0, "B": 1, "C": 2, "D": 3, "E": 4, "F": 7, "G": 8, "H": 9, "I": 10, "J": 11}


def dessine_breadboard(chemin):
    p = 34.0
    ncol = 17
    ox, oy = 250, 170
    svg = Svg(1330, 900)
    svg.texte(24, 36, "Banc de la tâche 16 : générateur sur la breadboard, sonde sur le boîtier détaché", 23,
              gras=True)
    svg.texte(24, 62, "Vue de dessus, rangée A en haut. Rainure centrale entre E et F (3 pas). Chaque "
                      "demi-colonne (A à E, ou F à J) est reliée dessous.", 13.5, C["gris"])
    svg.texte(24, 84, "Tout se câble les USB débranchés (BANC §2). Variante 1 : R_pu = 10k. "
                      "Variante 2 : R_pu = 4,7k, plus le 1 nF (pointillé).", 13.5, C["gris"])

    def H(t):
        r, c = t[0], int(t[1:])
        return (ox + (c - 1) * p, oy + BB_RANGS[r] * p)

    svg.rect(ox - 1.2 * p, oy - 1.3 * p, (ncol + 1.4) * p, 13.6 * p, "#fafafa", "#bdbdbd", 2, rx=10)
    svg.rect(ox - 1.2 * p, oy + 5.1 * p, (ncol + 1.4) * p, 0.8 * p, "#e4e4e4", None)
    svg.texte(ox + (ncol - 0.2) * p, oy + 5.65 * p, "rainure", 11, C["gris"], "end", italique=True)
    for c in range(1, ncol + 1):
        for demi in ("ABCDE", "FGHIJ"):
            x1, y1 = H(demi[0] + str(c))
            x2, y2 = H(demi[-1] + str(c))
            svg.rect(x1 - 6, y1 - 6, 12, y2 - y1 + 12, "#e9edf3", None, rx=5)
        for r in "ABCDEFGHIJ":
            X, Y = H(r + str(c))
            svg.rect(X - 3.5, Y - 3.5, 7, 7, "#555555", None, rx=1)
        fort = c in (1, 5, 10, 13, 14, 15)
        X, _ = H("A" + str(c))
        svg.texte(X, oy - 1.3 * p + 18, str(c), 12.5, C["texte"] if fort else C["gris"], "middle", gras=fort)
        _, Y = H("J" + str(c))
        svg.texte(X, Y + 1.3 * p - 6, str(c), 12.5, C["texte"] if fort else C["gris"], "middle", gras=fort)
    for r in "ABCDEFGHIJ":
        _, Y = H(r + "1")
        svg.texte(ox - 1.2 * p - 10, Y + 4.5, r, 13.5, C["texte"], "end", gras=True)
        svg.texte(ox + (ncol + 0.2) * p + 10, Y + 4.5, r, 13.5, C["texte"], "start", gras=True)

    k = p / PAS
    xg = H("D1")[0] - 1.35 * k
    xd = H("D10")[0] + 1.35 * k
    yh = H("D1")[1] - 1.23 * k
    yb = H("H1")[1] + 1.23 * k
    svg.rect(xg, yh, xd - xg, yb - yh, "#efe6f8", C["violet_module"], 2.2, rx=6)
    svg.rect(xg - 1.1 * k, (yh + yb) / 2 - 4.5 * k, 7.3 * k, 9 * k, "#dcdcdc", "#555555", 1.6, rx=5)
    svg.texte(xg + 2.5 * k, (yh + yb) / 2 + 4, "USB-C", 12.5, C["texte"], "middle", gras=True)
    svg.rect(xd - 2.6 * k, (yh + yb) / 2 - 3.2 * k, 1.8 * k, 6.4 * k, "#f7f0c6", "#8f7d2a", 1.4)
    svg.texte((xg + xd) / 2 + 22, (yh + yb) / 2 - 6, "générateur", 14, C["violet_module"], "middle", gras=True)
    svg.texte((xg + xd) / 2 + 22, (yh + yb) / 2 + 11, "(2e SuperMini)", 12, C["violet_module"], "middle")
    for i, nom in enumerate(HAUT):
        X, Y = H("D" + str(i + 1))
        coul, imp = role_broche(nom)
        svg.rect(X - 6.5, Y - 6.5, 13, 13, "#2b2b2b", None, rx=2)
        svg.texte(X, Y + 21, nom, 11, coul if imp else "#8a8a8a", "middle", gras=imp)
    for i, nom in enumerate(BAS):
        X, Y = H("H" + str(i + 1))
        coul, imp = role_broche(nom)
        if nom == "7":
            coul, imp = (C["e7"], True)
        if nom == "6":
            coul, imp = ("#8a8a8a", False)
        svg.rect(X - 6.5, Y - 6.5, 13, 13, "#2b2b2b", None, rx=2)
        svg.texte(X, Y - 13, nom, 11, coul if imp else "#8a8a8a", "middle", gras=imp)
    svg.texte(ox - 1.2 * p - 34, H("D1")[1] + 4.5, "rangée 5V…8 en D", 12.5, C["violet_module"], "end", gras=True)
    svg.texte(ox - 1.2 * p - 34, H("H1")[1] + 4.5, "rangée TX…7 en H", 12.5, C["violet_module"], "end", gras=True)

    def etiq(x, y, t, taille, coul, ancre="start", gras=True):
        w = largeur_texte(t, taille, gras)
        x0_ = x if ancre == "start" else (x - w / 2 if ancre == "middle" else x - w)
        svg.rect(x0_ - 3, y - taille * 0.9, w + 6, taille * 1.25, "#ffffff", op=0.9, rx=3)
        svg.texte(x, y, t, taille, coul, ancre, gras)

    def fil(a, b, coul, bosse=0.0, ep=4.5):
        (x1, y1), (x2, y2) = H(a), H(b)
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2 - bosse
        bord = "#000000" if coul != C["noir"] else "#8a8a8a"
        d = "M%.1f %.1f Q%.1f %.1f %.1f %.1f" % (x1, y1, mx, my, x2, y2)
        svg.courbe(d, bord, ep + 2.4)
        svg.courbe(d, coul, ep)
        for X, Y in ((x1, y1), (x2, y2)):
            svg.rect(X - 5, Y - 5, 10, 10, "#333333", None, rx=1)

    def resistance(a, b, lib, cote="d"):
        (x1, y1), (x2, y2) = H(a), H(b)
        tx, ty = unit(x1, y1, x2, y2)
        cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
        svg.ligne([(x1, y1), (x2, y2)], "#6b6b6b", 2.6)
        L = min(6.4 * k, math.hypot(x2 - x1, y2 - y1) - 14)
        svg.poly(rectangle_oriente(cx, cy, tx, ty, L, 2.4 * k), "#ecd9ae", "#8a6d3b", 1.6)
        if abs(tx) > abs(ty):
            svg.texte(cx, cy + 4, lib, 11.5, C["texte"], "middle", gras=True)
        else:
            svg.texte(cx + (1.4 * k + 5 if cote == "d" else -1.4 * k - 5), cy + 4, lib, 11.5, C["texte"],
                      "start" if cote == "d" else "end", gras=True)
        for X, Y in ((x1, y1), (x2, y2)):
            svg.cercle(X, Y, 3.4, "#9a9a9a")

    fil("A1", "A13", C["rouge"], bosse=30)
    fil("B2", "B15", C["noir"], bosse=20)
    fil("E15", "F15", C["noir"], ep=4)
    resistance("E13", "F13", "R_pu", cote="g")
    resistance("J10", "J14", "4,7k")
    (x1, y1), (x2, y2) = H("I14"), H("I15")
    svg.courbe("M%.1f %.1f Q%.1f %.1f %.1f %.1f" % (x1, y1, (x1 + x2) / 2, y1 - 18, x2, y2), "#6b6b6b", 2.6)
    svg.cercle(x1, y1, 1.3 * k, "#ecd9ae", "#8a6d3b", 1.6)
    svg.cercle(x2, y2, 3.4, "#9a9a9a")
    xl = ox + (ncol + 0.2) * p + 34
    xe = ox + (ncol + 0.2) * p - 4
    svg.ligne([(x2 + 6, y2), (xe, y2)], "#9a9a9a", 1)
    etiq(xl, y2 + 4.5, "10k debout (base–émetteur)", 11.5, C["texte"])
    (x1, y1), (x2, y2) = H("H13"), H("H15")
    svg.ligne([(x1, y1), (x2, y2)], C["e5b"], 2.2, dash="4 3")
    svg.add('<ellipse cx="%.1f" cy="%.1f" rx="12" ry="7.5" fill="#fff4d6" stroke="%s" stroke-width="1.6" '
            'stroke-dasharray="4 3"/>' % ((x1 + x2) / 2, y1, C["e5b"]))
    svg.ligne([(x2 + 6, y1), (xe, y1)], "#9ccfcf", 1)
    etiq(xl, y1 + 4.5, "1 nF (variante 2)", 11.5, C["e5b"])
    b = H("G14")
    R, a = 2.45 * k, 1.05 * k
    t1 = math.atan2(a, math.sqrt(R * R - a * a))
    t2 = math.pi - t1
    pts = []
    for i in range(41):
        th = t2 + (2 * math.pi + t1 - t2) * i / 40.0
        pts.append((b[0] + R * math.cos(th), b[1] + R * math.sin(th)))
    svg.poly(pts, "#3a3a3a", "#111111", 1.4)
    hh = math.sqrt(R * R - a * a)
    svg.ligne([(b[0] - hh, b[1] + a), (b[0] + hh, b[1] + a)], C["plat"], 4.5, cap="butt")
    for nom in ("C", "B", "E"):
        q = H({"C": "G13", "B": "G14", "E": "G15"}[nom])
        svg.cercle(q[0], q[1], 8, "#ffffff", "#111111", 1.2)
        svg.texte(q[0], q[1] + 4, nom, 11, C["texte"], "middle", gras=True)
    svg.ligne([(H("G15")[0] + 10, b[1]), (xe, b[1])], "#9a9a9a", 1)
    etiq(xl, b[1] + 4.5, "Q3 BC547, face plate vers J", 11.5, C["texte"])
    xs = ox + (ncol + 1.9) * p
    y_bc = (H("B1")[1] + H("C1")[1]) / 2
    y_bas = H("J1")[1] + 1.3 * p + 44
    for t, coul, lib, sous, yfin in (
            ("C15", C["noir"], "GND → TP" + MOINS + " (picot M1 du boîtier)", "fil de masse : le poser en premier",
             y_bc),
            ("I13", C["blanc"], "LIGNE → TP_Dp (picot H4 du boîtier)", "fil Dupont mâle-femelle, femelle sur le picot",
             y_bas)):
        X, Y = H(t)
        if t == "I13":
            xm = X + p / 2
            d = ("M%.1f %.1f C%.1f %.1f %.1f %.1f %.1f %.1f L%.1f %.1f C%.1f %.1f %.1f %.1f %.1f %.1f L%.1f %.1f"
                 % (X, Y, X, Y + 14, xm, Y + 8, xm, Y + 30, xm, yfin - 20, xm, yfin, xm, yfin, xm + 20, yfin, xs, yfin))
        else:
            d = "M%.1f %.1f C%.1f %.1f %.1f %.1f %.1f %.1f L%.1f %.1f" % (
                X, Y, X + 10, Y - 12, X + 30, yfin, X + 60, yfin, xs, yfin)
        svg.courbe(d, "#222222", 6.6)
        svg.courbe(d, coul, 4.2)
        svg.rect(X - 5, Y - 5, 10, 10, "#333333", None, rx=1)
        svg.rect(xs, yfin - 8, 14, 16, "#222222", None, rx=2)
        svg.texte(xs + 22, yfin + 5, lib, 13.5, C["texte"], "start", gras=True)
        svg.texte(xs + 22, yfin + 23, sous, 12, C["gris"])
    svg.texte(H("A13")[0], oy - 1.3 * p - 6, "5V", 12.5, C["orange"], "middle", gras=True)
    svg.texte(H("A15")[0], oy - 1.3 * p - 6, "GND", 12.5, C["noir"], "middle", gras=True)
    _, yj = H("J13")
    svg.texte(H("J13")[0] - 4, yj + 1.3 * p + 20, "LIGNE", 12, C["texte"], "end", gras=True)
    svg.texte(H("J14")[0] + 8, yj + 1.3 * p + 20, "base", 12, C["texte"], "start", gras=True)
    svg.texte(H("J15")[0] + 26, yj + 1.3 * p + 20, "GND", 12, C["texte"], "start", gras=True)
    bx, by = 24, 750
    lignes = [
        ("Fils : rouge A1 → A13 (5V) ; noir B2 → B15 (GND) ; noir court E15 → F15, à travers la rainure.", False,
         C["texte"]),
        ("Pièces : R_pu de E13 à F13 ; Q3 avec C en G13, B en G14, E en G15 ; 4,7k de J10 (broche 7) à J14 ; "
         "10k debout de I14 à I15 ; 1 nF de H13 à H15 (variante 2).", False, C["texte"]),
        ("Vers le boîtier : LIGNE (I13) sur TP_Dp, GND (C15) sur TP" + MOINS + ". Rien d'autre sur le boîtier : "
         "fiche XH 4 br. retirée, J2 ouvert, TP+ libre.", False, C["texte"]),
        ("Si ta breadboard porte la rangée J en haut : tourne-la d'un demi-tour, puis compte les colonnes depuis la "
         "gauche, comme sur la figure.", True, C["rouge"]),
    ]
    for i, (lg, g, c_) in enumerate(lignes):
        svg.texte(bx, by + i * 24, lg, 13.5, c_, gras=g)
    with open(chemin, "w", encoding="utf-8") as f:
        f.write(svg.rendu())


# ----------------------------------------------------------------------------- principal
def main():
    args = sys.argv[1:]
    sortie = os.path.join(ICI, "..", "docs", "fabrication")
    if "--sortie" in args:
        i = args.index("--sortie")
        sortie = args[i + 1]
        del args[i:i + 2]
    plans = args or [os.path.join(ICI, "..", "docs", "fabrication", "plan-boitier.json"),
                     os.path.join(ICI, "..", "docs", "fabrication", "plan-adaptateur.json")]
    os.makedirs(sortie, exist_ok=True)
    ecrits = []
    for chemin in plans:
        plan = json.load(open(chemin, encoding="utf-8"))
        for cote in ("composants", "soudures"):
            f = os.path.join(sortie, "%s-%s.svg" % (plan["carte"], cote))
            print("%s, %s :" % (plan["carte"], cote))
            dessine_carte(plan, cote, f)
            ecrits.append(f)
    if not args:
        f = os.path.join(sortie, "supermini-brochage.svg")
        dessine_brochage(f)
        ecrits.append(f)
        f = os.path.join(sortie, "banc-breadboard.svg")
        dessine_breadboard(f)
        ecrits.append(f)
    for f in ecrits:
        print("ecrit", f)


if __name__ == "__main__":
    main()
