#!/usr/bin/env python3
"""Verificateur de plan de plaque a pastilles (projet hotte Haier).

  python3 verifier_plaque.py <plan.json> [--etape7] [--analyseur]

Le plan (JSON) decrit UNE carte :
{
  "carte": "boitier" | "adaptateur",
  "colonnes": 24, "rangees": 18,           # trous ; colonnes 1..N (gauche->droite), rangees A.. (haut->bas), cote composants
  "composants": [
    {"ref": "Q1", "type": "BC547", "broches": {"C": "F10", "B": "F11", "E": "F12"}, "monte": true},
    {"ref": "R1", "type": "resistance", "valeur": "100k", "broches": {"1": "C3", "2": "C7"}},
    {"ref": "U1", "type": "supermini", "broches": {"5V": "B3", ..., "7": "H12"}},
    ...
  ],
  "liaisons": [{"de": "C3", "a": "C4", "face": "dessous", "genre": "pont" | "fil_nu" | "fil_isole", "etape": 7?}],
}
Types reconnus : resistance, BC547, supermini, xh4, xh3, picot, barrette2, support2 (support femelle 2 broches pour R_be),
fils_cn3 (3 fils soudes de la fiche XH 3 br. qui va sur CN3 : broches "-", "D", "+"),
sortie4 (4 fils du cable de sortie soudes : broches "1".."4"), analyseur3 (barrette 3 broches : "GND", "V1", "V2").
Un composant ou une liaison avec "etape": 7 (etage d'injection) ou "etape": "5b" (diviseur de l'analyseur) n'est
compte que si --etape7 / --analyseur est donne. "monte": false : jamais compte (place reservee).

Sortie : liste d'erreurs (bloquantes) et d'avertissements ; code 0 si aucune erreur.
"""
import json, sys

RANGEES = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

# Brochage releve sur la photo de Majid (28/09), cote composants, USB a gauche :
HAUT = ["5V", "GND", "3V3", "20", "19", "18", "15", "14", "9", "8"]
BAS = ["TX", "RX", "0", "1", "2", "3", "4", "5", "6", "7"]
ECART_RANGEES = 6  # trous entre les deux rangees de la SuperMini (15,24 mm)

NETS = {
    "adaptateur": {
        "M": ["CN3.-", "XH3.-", "S.1"],
        "P": ["CN3.+", "XH3.+", "S.2"],
        "DP": ["XH3.D", "J1.2", "S.3"],
        "DC": ["CN3.D", "J1.1", "S.4"],
    },
    "boitier": {
        "M": ["X1.1", "TPM.1", "U1.GND", "Q1.E", "RBE.2"],
        "P": ["X1.2", "TPP.1", "J2.1"],
        "V5": ["J2.2", "U1.5V"],
        "DP": ["X1.3", "TPDP.1", "R1.1"],
        "DC": ["X1.4", "TPDC.1"],
        "B1": ["R1.2", "RBE.1", "Q1.B"],
        "C1": ["Q1.C", "R2.1", "U1.6"],
        "V33": ["R2.2", "U1.3V3"],
    },
}
NETS_ETAPE7 = {  # etage d'injection (WIRING.md 6)
    "M": ["R4.2", "Q2.E"],
    "DP": ["R5.1"],
    "G7": ["U1.7", "R3.1"],
    "B2": ["R3.2", "R4.1", "Q2.B"],
    "C2": ["Q2.C", "R5.2"],
}
NETS_ANALYSEUR = {  # diviseur de l'analyseur (WIRING.md 8), barrette ANA
    "DP": ["R6.1"],
    "A2": ["R6.2", "R7.1", "ANA.V2"],
    "M": ["R7.2", "ANA.GND"],
    "C1": ["ANA.V1"],
}
VALEURS = {"R1": "100k", "RBE": "100k", "R2": "10k", "R3": "4.7k", "R4": "10k", "R5": "470", "R6": "47k", "R7": "68k"}
# Broches qui ne doivent JAMAIS toucher le bus (regle 10) : toute GPIO de la SuperMini
GPIO = set(HAUT[3:] + BAS)


def trou(t):
    t = t.strip().upper()
    r, c = t[0], t[1:]
    return RANGEES.index(r), int(c)


def nom(rc):
    return "%s%d" % (RANGEES[rc[0]], rc[1])


def main():
    args = sys.argv[1:]
    chemin = [a for a in args if not a.startswith("--")][0]
    etape7 = "--etape7" in args
    analyseur = "--analyseur" in args
    plan = json.load(open(chemin))
    carte = plan["carte"]
    ncol, nrang = plan["colonnes"], plan["rangees"]
    err, avert = [], []

    def compte(x):
        if x.get("monte") is False:
            return False
        e = str(x.get("etape", ""))
        if e == "7":
            return etape7
        if e == "5b":
            return analyseur
        return True

    comps = [c for c in plan["composants"] if compte(c)]
    liais = [l for l in plan.get("liaisons", []) if compte(l)]

    # --- trous dans la plaque, un seul fil de composant par trou
    occ = {}
    for c in comps:
        for b, t in c["broches"].items():
            try:
                rc = trou(t)
            except Exception:
                err.append("%s.%s : trou illisible %r" % (c["ref"], b, t))
                continue
            if not (0 <= rc[0] < nrang and 1 <= rc[1] <= ncol):
                err.append("%s.%s : trou %s hors de la plaque" % (c["ref"], b, t))
            if rc in occ:
                err.append("trou %s occupe deux fois : %s et %s.%s" % (nom(rc), occ[rc], c["ref"], b))
            occ[rc] = "%s.%s" % (c["ref"], b)
    for l in liais:
        for t in (l["de"], l["a"]):
            rc = trou(t)
            if not (0 <= rc[0] < nrang and 1 <= rc[1] <= ncol):
                err.append("liaison %s-%s : trou %s hors de la plaque" % (l["de"], l["a"], t))
        a, b = trou(l["de"]), trou(l["a"])
        d = abs(a[0] - b[0]) + abs(a[1] - b[1])
        if l.get("genre") == "pont" and d != 1:
            err.append("pont de soudure %s-%s : trous non voisins" % (l["de"], l["a"]))
        if d == 0:
            err.append("liaison %s-%s : meme trou" % (l["de"], l["a"]))

    # --- empreintes
    def alignes(pts):
        rs = {p[0] for p in pts}
        cs = {p[1] for p in pts}
        if len(rs) == 1:
            s = sorted(p[1] for p in pts)
        elif len(cs) == 1:
            s = sorted(p[0] for p in pts)
        else:
            return False
        return all(b - a == 1 for a, b in zip(s, s[1:]))

    for c in comps:
        t, br = c["type"], c["broches"]
        P = {k: trou(v) for k, v in br.items()}
        if t == "BC547":
            if set(P) != {"C", "B", "E"} or not alignes(list(P.values())):
                err.append("%s : C, B, E doivent etre trois trous voisins alignes" % c["ref"])
            else:
                m = P["B"]
                if abs(P["C"][0] - m[0]) + abs(P["C"][1] - m[1]) != 1 or abs(P["E"][0] - m[0]) + abs(P["E"][1] - m[1]) != 1:
                    err.append("%s : B doit etre au milieu (BC547 : C-B-E)" % c["ref"])
        elif t == "resistance":
            if len(P) != 2:
                err.append("%s : deux broches attendues" % c["ref"])
            else:
                a, b = P.values()
                d = abs(a[0] - b[0]) + abs(a[1] - b[1])
                if a[0] != b[0] and a[1] != b[1]:
                    err.append("%s : broches ni sur une rangee ni sur une colonne" % c["ref"])
                elif d < 3:
                    avert.append("%s : %d pas entre les broches (resistance debout ; couchee il en faut 3 a 4)" % (c["ref"], d))
            v = VALEURS.get(c["ref"])
            if v and c.get("valeur", "").replace(",", ".").lower() != v.lower():
                err.append("%s : valeur %r, attendu %s (WIRING.md)" % (c["ref"], c.get("valeur"), v))
        elif t == "xh4":
            if set(P) != {"1", "2", "3", "4"} or not alignes(list(P.values())):
                err.append("%s : 4 trous voisins alignes attendus" % c["ref"])
            else:
                s = [P[k] for k in "1234"]
                if not all(abs(a[0] - b[0]) + abs(a[1] - b[1]) == 1 for a, b in zip(s, s[1:])):
                    err.append("%s : broches 1 a 4 pas dans l'ordre" % c["ref"])
        elif t == "xh3":
            if set(P) != {"-", "D", "+"} or not alignes(list(P.values())):
                err.append("%s : trois trous voisins alignes attendus (-, D, +)" % c["ref"])
            else:
                m = P["D"]
                if abs(P["-"][0] - m[0]) + abs(P["-"][1] - m[1]) != 1 or abs(P["+"][0] - m[0]) + abs(P["+"][1] - m[1]) != 1:
                    err.append("%s : D doit etre au milieu (fiche du panneau : - D +)" % c["ref"])
        elif t in ("barrette2", "support2"):
            if len(P) != 2:
                err.append("%s : deux broches attendues" % c["ref"])
            else:
                a, b = P.values()
                if abs(a[0] - b[0]) + abs(a[1] - b[1]) != 1:
                    err.append("%s : broches voisines attendues" % c["ref"])
        elif t == "analyseur3":
            if set(P) != {"GND", "V1", "V2"} or not alignes(list(P.values())):
                err.append("%s : trois trous voisins alignes (GND, V1, V2)" % c["ref"])
        elif t == "supermini":
            if set(P) != set(HAUT + BAS):
                err.append("U1 : les 20 pastilles attendues : %s" % sorted(set(HAUT + BAS) ^ set(P)))
                continue
            h = [P[k] for k in HAUT]
            b = [P[k] for k in BAS]
            if not alignes(h) or not alignes(b):
                err.append("U1 : chaque rangee doit occuper 10 trous voisins alignes")
                continue
            u = (h[1][0] - h[0][0], h[1][1] - h[0][1])  # le long de la rangee (5V -> GND)
            if any((h[i + 1][0] - h[i][0], h[i + 1][1] - h[i][1]) != u for i in range(9)) or \
               any((b[i + 1][0] - b[i][0], b[i + 1][1] - b[i][1]) != u for i in range(9)):
                err.append("U1 : ordre des pastilles incoherent le long des rangees")
                continue
            v = (b[0][0] - h[0][0], b[0][1] - h[0][1])  # de 5V vers TX
            if abs(v[0]) + abs(v[1]) != ECART_RANGEES or (v[0] * u[0] + v[1] * u[1]) != 0:
                err.append("U1 : les rangees doivent etre a %d trous l'une de l'autre, 5V en face de TX" % ECART_RANGEES)
                continue
            if any((b[i][0] - h[i][0], b[i][1] - h[i][1]) != v for i in range(10)):
                err.append("U1 : les deux rangees ne sont pas en face l'une de l'autre")
            # Sens : photo (x = colonne vers la droite, y = rangee vers le bas) : u = (0, +1) en (rangee, colonne), v = (+1, 0).
            # Rotation sans retournement : le produit u x v (en coordonnees x, y) doit garder le signe de la photo.
            ux, uy = u[1], u[0]
            vx, vy = v[1], v[0]
            if ux * vy - uy * vx <= 0:
                err.append("U1 : SuperMini RETOURNEE (image miroir) : les pastilles ne correspondent plus a la carte posee composants en haut")
            # Antenne (cote 8/7) pres d'un bord, USB (cote 5V/TX) accessible
            fin = h[9]
            pas_bord = []
            for axe, sens in ((0, u[0]), (1, u[1])):
                if sens == 0:
                    continue
                lim = (nrang - 1) if axe == 0 else ncol
                depart = 0 if axe == 0 else 1
                reste = (lim - fin[axe]) if sens > 0 else (fin[axe] - depart)
                pas_bord.append(reste)
            if pas_bord and min(pas_bord) > 1:
                avert.append("U1 : l'antenne (cote 8/7) est a %d trous du bord : la mettre au bord de la plaque" % min(pas_bord))
        elif t in ("picot",):
            if len(P) != 1:
                err.append("%s : un seul trou" % c["ref"])
        elif t == "fils_cn3":
            if set(P) != {"-", "D", "+"}:
                err.append("%s : broches -, D, + attendues" % c["ref"])
        elif t == "sortie4":
            if set(P) != {"1", "2", "3", "4"}:
                err.append("%s : broches 1 a 4 attendues" % c["ref"])
        else:
            avert.append("%s : type %r inconnu du verificateur" % (c["ref"], t))

    # --- connexite : union des trous relies par les liaisons
    parent = {}

    def f(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def u(a, b):
        parent[f(a)] = f(b)

    for l in liais:
        u(trou(l["de"]), trou(l["a"]))
    broche = {}
    for c in comps:
        for b, t in c["broches"].items():
            broche["%s.%s" % (c["ref"], b)] = f(trou(t))
    # support femelle de R_be : ses deux broches deviennent RBE.1 / RBE.2 si RBE est un support
    attendu = {k: list(v) for k, v in NETS[carte].items()}
    if carte == "boitier" and etape7:
        for k, v in NETS_ETAPE7.items():
            attendu.setdefault(k, []).extend(v)
    if carte == "boitier" and analyseur:
        for k, v in NETS_ANALYSEUR.items():
            attendu.setdefault(k, []).extend(v)
    net_de = {}
    for n, membres in attendu.items():
        racines = set()
        for m in membres:
            if m not in broche:
                err.append("net %s : broche %s absente du plan" % (n, m))
                continue
            racines.add(broche[m])
            net_de[m] = n
        if len(racines) > 1:
            err.append("net %s coupe en %d morceaux : %s" % (n, len(racines), ", ".join(membres)))
    # aucun melange entre nets, et aucune broche non listee reliee a un net (sauf picots/barrettes internes)
    racine_net = {}
    for m, n in net_de.items():
        r = broche[m]
        if r in racine_net and racine_net[r] != n:
            err.append("COURT-CIRCUIT : nets %s et %s relies (%s)" % (racine_net[r], n, m))
        racine_net.setdefault(r, n)
    for m, r in broche.items():
        if m in net_de:
            continue
        if r in racine_net:
            ref, b = m.split(".", 1)
            if ref == "U1":
                err.append("U1.%s (non utilisee) reliee au net %s" % (b, racine_net[r]))
            else:
                err.append("%s relie au net %s alors qu'il ne devrait l'etre a aucun" % (m, racine_net[r]))
    # regle 10 : aucune GPIO sur un net du bus sans passer par un etage
    for g in GPIO:
        m = "U1." + g
        if m in broche and broche[m] in racine_net and racine_net[broche[m]] in ("DP", "DC", "P", "M", "V5"):
            err.append("REGLE 10 : GPIO %s sur le net %s" % (g, racine_net[broche[m]]))
    if carte == "boitier" and not etape7 and "U1.7" in broche and broche["U1.7"] in racine_net:
        err.append("GPIO7 reliee (%s) avant l'etape 7" % racine_net[broche["U1.7"]])

    # --- croisements de fils nus sur une meme face
    def seg(l):
        return trou(l["de"]), trou(l["a"])

    nus = [l for l in liais if l.get("genre") in ("fil_nu", "pont")]
    for i in range(len(nus)):
        for j in range(i + 1, len(nus)):
            if nus[i].get("face") != nus[j].get("face"):
                continue
            (a, b), (c, d) = seg(nus[i]), seg(nus[j])
            if f(a) == f(c):
                continue

            def cote(p, q, r):
                return (q[1] - p[1]) * (r[0] - p[0]) - (q[0] - p[0]) * (r[1] - p[1])

            d1, d2, d3, d4 = cote(a, b, c), cote(a, b, d), cote(c, d, a), cote(c, d, b)
            if d1 * d2 < 0 and d3 * d4 < 0:
                err.append("fils nus croises (%s-%s et %s-%s), face %s" % (nus[i]["de"], nus[i]["a"], nus[j]["de"], nus[j]["a"], nus[i].get("face")))
            elif (d1 == 0 and min(a[0], b[0]) <= c[0] <= max(a[0], b[0]) and min(a[1], b[1]) <= c[1] <= max(a[1], b[1])) or \
                 (d2 == 0 and min(a[0], b[0]) <= d[0] <= max(a[0], b[0]) and min(a[1], b[1]) <= d[1] <= max(a[1], b[1])):
                err.append("fil nu %s-%s passe par un trou d'un autre net (%s-%s)" % (nus[i]["de"], nus[i]["a"], nus[j]["de"], nus[j]["a"]))
    # un fil nu qui passe sur un trou occupe par une broche d'un autre net
    for l in nus:
        a, b = seg(l)
        if a[0] == b[0]:
            pts = [(a[0], c) for c in range(min(a[1], b[1]) + 1, max(a[1], b[1]))]
        elif a[1] == b[1]:
            pts = [(r, a[1]) for r in range(min(a[0], b[0]) + 1, max(a[0], b[0]))]
        else:
            avert.append("fil nu en diagonale %s-%s" % (l["de"], l["a"]))
            pts = []
        for p in pts:
            if p in occ and f(p) != f(a):
                err.append("fil nu %s-%s passe sur le trou %s (%s, autre net)" % (l["de"], l["a"], nom(p), occ[p]))

    print("carte %s%s%s : %d erreur(s), %d avertissement(s)" % (carte, " +etape7" if etape7 else "", " +analyseur" if analyseur else "", len(err), len(avert)))
    for e in err:
        print("ERREUR :", e)
    for a in avert:
        print("avert. :", a)
    return 1 if err else 0


if __name__ == "__main__":
    sys.exit(main())
