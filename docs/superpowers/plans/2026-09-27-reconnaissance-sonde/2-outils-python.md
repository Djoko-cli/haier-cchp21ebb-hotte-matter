# Plan de la sonde, partie 2 : outils-python

> En-tête, contraintes globales, carte des fichiers et interfaces : [../2026-09-27-reconnaissance-sonde.md](../2026-09-27-reconnaissance-sonde.md). Les tâches s'exécutent dans l'ordre des numéros (2 à 6).

### Tâche 2 : Signaux synthétiques et simulation de la sonde (`tools/signaux.py`)

**Fichiers :**
- Créer : `tools/signaux.py`
- Tester : `tools/tests/test_signaux.py`

**Interfaces :**
- Consomme : rien (bibliothèque standard seulement).
- Produit (contrat, à la lettre) :
  - `Segment = tuple[bool, int]` (niveau haut ?, durée en µs, au niveau de la ligne) ;
  - `uart(octets: bytes, bauds: int, repos_haut: bool = True, pause_octet_us: int = 0) -> list[Segment]` ;
  - `distance_impulsion(bits: str, t_us: int = 750) -> list[Segment]` ;
  - `manchester(bits: str, t_us: int) -> list[Segment]` ;
  - `fusionner(segs: list[Segment]) -> list[Segment]` ;
  - `MOTIFS: tuple[str, ...]` ;
  - `octets_motif(nom: str, index: int) -> bytes`, `trame_motif(nom: str, index: int) -> list[Segment]`, `pause_motif_us(nom: str) -> int`, `repos_haut_motif(nom: str) -> bool` ;
  - `simuler_sonde(segments: list[Segment], t0_us: int, silence_us: int, repos_haut: bool, dur_max: int = 110, num0: int = 1) -> list[dict]`.
- Produit (ajout) : `T_WTC_US = 750`.

**Conventions fixées ici (la tâche 13 doit les reproduire dans `src/motifs.cpp`) :**
- UART : les frontières des bits d'un octet sont à `(k * 1000000 + bauds / 2) / bauds` µs du début de l'octet (division entière, k = 0..10) ; le bit de stop du dernier octet fait partie de la trame. « Sans le repos final » veut dire sans la pause, mais avec ce bit de stop.
- `wtc` : trame (départ, 16 bits, 1T haut), puis 4T haut, puis la réponse (départ, 8 bits, 1T haut) ; après fusion, 5T haut entre les deux. `octets_motif("wtc", i)` rend 3 octets : les 16 bits de la trame puis les 8 bits de la réponse.
- `rafale` : `octets_motif` rend `b""`, repos haut.
- `simuler_sonde` : `n` compte les objets à partir de 1 ; `ms` = fin de la partie (plus `silence_us` pour la dernière), en ms. Un palier **hors repos** de `silence_us` ou plus lève `ValueError` : le RMT clôt une réception après `silence_us` sans front, quel que soit le niveau, ce que le simulateur ne reproduit pas. Pour `uart500`, `uart500inv` et `krona` (octet 0x00 : 18 ms hors repos), il faut donc un silence supérieur à 18 000 µs.
- `krona` : pour `index % 8` égal à 0 ou 1, l'octet 0xFF ou 0xFE et la pause de 4 ms donnent 22 ou 20 ms de repos dans la trame, autant ou plus que les 20 ms entre deux trames. Aucun silence ne sépare alors les trames : la sonde les coupe entre deux octets, sans rien perdre. Le test `test_krona_index_0_coupe_entre_deux_octets` le fixe.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_signaux.py` :

```python
#!/usr/bin/env python3
"""Tests de tools/signaux.py : python3 -m unittest discover -s tools/tests -p test_signaux.py -v"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import signaux  # noqa: E402

H, B = True, False


class Fusionner(unittest.TestCase):
    def test_colle_les_niveaux_egaux_et_retire_les_durees_nulles(self):
        segs = [(H, 5), (H, 3), (B, 0), (B, 2), (H, 1)]
        self.assertEqual(signaux.fusionner(segs), [(H, 8), (B, 2), (H, 1)])

    def test_duree_negative_refusee(self):
        with self.assertRaises(ValueError):
            signaux.fusionner([(H, -1)])


class Uart(unittest.TestCase):
    def test_a5_500_bauds_repos_haut(self):
        # 0xA5 = 10100101, LSB d'abord : 1 0 1 0 0 1 0 1 ; depart bas, stop haut
        self.assertEqual(signaux.uart(b"\xa5", 500), [
            (B, 2000), (H, 2000), (B, 2000), (H, 2000), (B, 4000), (H, 2000), (B, 2000), (H, 4000)])

    def test_a5_500_bauds_repos_bas(self):
        self.assertEqual(signaux.uart(b"\xa5", 500, repos_haut=False), [
            (H, 2000), (B, 2000), (H, 2000), (B, 2000), (H, 4000), (B, 2000), (H, 2000), (B, 4000)])

    def test_9600_bauds_frontieres_arrondies(self):
        # frontieres a (k * 1000000 + 4800) // 9600 : 9 bits = 938 us, octet = 1042 us
        self.assertEqual(signaux.uart(b"\x00", 9600), [(B, 938), (H, 104)])
        self.assertEqual(sum(d for _, d in signaux.uart(b"\xa5\x5a\x00\xff", 9600)), 4 * 1042)

    def test_pause_entre_octets(self):
        segs = signaux.uart(b"\x55\x55", 500, pause_octet_us=4000)
        self.assertEqual(len(segs), 20)
        self.assertEqual(segs[9], (H, 2000 + 4000))  # stop + pause, fusionnes
        self.assertEqual(sum(d for _, d in segs), 2 * 20000 + 4000)

    def test_debit_invalide(self):
        with self.assertRaises(ValueError):
            signaux.uart(b"\x00", 0)


class AutresCodages(unittest.TestCase):
    def test_distance_impulsion(self):
        self.assertEqual(signaux.distance_impulsion("01"), [
            (B, 1500), (H, 750), (B, 750), (H, 750), (B, 2250), (H, 750)])

    def test_distance_impulsion_bits_invalides(self):
        with self.assertRaises(ValueError):
            signaux.distance_impulsion("012")

    def test_manchester_ieee(self):
        # 1 = bas puis haut, 0 = haut puis bas
        self.assertEqual(signaux.manchester("10", 100), [(B, 100), (H, 200), (B, 100)])


class Motifs(unittest.TestCase):
    def test_liste(self):
        self.assertEqual(signaux.MOTIFS, ("uart500", "uart500inv", "uart2400", "uart2400inv",
                                          "uart9600", "uart9600inv", "wtc", "krona", "rafale"))

    def test_octets_uart(self):
        self.assertEqual(signaux.octets_motif("uart500", 7), bytes([0xA5, 0x5A, 0x00, 0xFF, 0x07, 0x05]))
        self.assertEqual(signaux.octets_motif("uart9600inv", 300), bytes([0xA5, 0x5A, 0x00, 0xFF, 0x2C, 0x2A]))

    def test_octets_wtc_et_krona(self):
        self.assertEqual(signaux.octets_motif("wtc", 9), bytes([0x02, 0x00, 0x09]))
        self.assertEqual(signaux.octets_motif("wtc", 7), bytes([0x80, 0x00, 0x07]))
        self.assertEqual(signaux.octets_motif("krona", 10), bytes([0x55, 0x02, 0xFD, 0x00, 0x54]))
        self.assertEqual(signaux.octets_motif("rafale", 3), b"")

    def test_motif_inconnu_ou_index_negatif(self):
        with self.assertRaises(ValueError):
            signaux.octets_motif("uart1200", 0)
        with self.assertRaises(ValueError):
            signaux.trame_motif("wtc", -1)

    def test_trame_uart(self):
        self.assertEqual(signaux.trame_motif("uart2400inv", 4),
                         signaux.uart(signaux.octets_motif("uart2400inv", 4), 2400, repos_haut=False))

    def test_trame_wtc_avec_reponse(self):
        segs = signaux.trame_motif("wtc", 0)
        self.assertEqual(len(segs), 52)
        self.assertEqual(segs[0], (B, 1500))
        self.assertEqual(segs[33], (H, 750 + 4 * 750))  # 1T haut de fin + 4T avant la reponse
        self.assertEqual(segs[34], (B, 1500))           # depart de la reponse
        self.assertEqual(segs[-1], (H, 750))
        self.assertEqual(sum(d for _, d in segs), 60 * 750)

    def test_trame_krona(self):
        segs = signaux.trame_motif("krona", 0)
        self.assertEqual(segs[0], (H, 2000))  # depart inverse
        self.assertEqual(segs[-1][0], B)      # stop au repos bas
        self.assertEqual(sum(d for _, d in segs), 5 * 20000 + 4 * 4000)

    def test_trame_rafale(self):
        segs = signaux.trame_motif("rafale", 0)
        self.assertEqual(len(segs), 100000)
        self.assertEqual(segs[:2], [(H, 100), (B, 100)])
        self.assertEqual(segs[-1], (B, 100))

    def test_pauses_et_repos(self):
        self.assertEqual([signaux.pause_motif_us(m) for m in signaux.MOTIFS],
                         [20000] * 6 + [6000, 18000, 1000000])
        self.assertEqual([signaux.repos_haut_motif(m) for m in signaux.MOTIFS],
                         [True, False] * 3 + [True, False, True])


class SimulerSonde(unittest.TestCase):
    def test_une_reception(self):
        objs = signaux.simuler_sonde(signaux.uart(b"\xa5", 500), 1000, 5000, repos_haut=True)
        self.assertEqual(len(objs), 1)
        o = objs[0]
        self.assertEqual(list(o), ["v", "t", "n", "ms", "num", "part", "fin", "t_us", "niv0", "dur_us", "debord"])
        self.assertEqual(o, {"v": 1, "t": "trame", "n": 1, "ms": 22, "num": 1, "part": 0, "fin": True,
                             "t_us": 1000, "niv0": "bas",
                             "dur_us": [2000, 2000, 2000, 2000, 4000, 2000, 2000], "debord": False})

    def test_repos_initial_decale_t_us(self):
        objs = signaux.simuler_sonde([(H, 300)] + signaux.uart(b"\xa5", 500), 1000, 5000, True)
        self.assertEqual(objs[0]["t_us"], 1300)

    def test_decoupe_en_parties_de_110(self):
        segs = [(k % 2 == 1, 100) for k in range(250)]  # le dernier (haut) se fond dans le repos
        objs = signaux.simuler_sonde(segs, 0, 5000, True)
        self.assertEqual([len(o["dur_us"]) for o in objs], [110, 110, 29])
        self.assertEqual([o["part"] for o in objs], [0, 1, 2])
        self.assertEqual([o["fin"] for o in objs], [False, False, True])
        self.assertEqual([o["t_us"] for o in objs], [0, 11000, 22000])
        self.assertEqual([o["n"] for o in objs], [1, 2, 3])
        self.assertEqual({o["num"] for o in objs}, {1})

    def test_niv0_alterne_selon_le_rang(self):
        segs = [(k % 2 == 1, 10) for k in range(7)]  # bas haut bas haut bas haut bas
        objs = signaux.simuler_sonde(segs, 0, 5000, True, dur_max=3)
        self.assertEqual([o["niv0"] for o in objs], ["bas", "haut", "bas"])
        self.assertEqual([o["dur_us"] for o in objs], [[10, 10, 10], [10, 10, 10], [10]])

    def test_receptions_separees_par_le_silence(self):
        segs = [(B, 100), (H, 5000), (B, 200), (H, 4999), (B, 300)]
        objs = signaux.simuler_sonde(segs, 0, 5000, True, num0=7)
        self.assertEqual([(o["num"], o["t_us"], o["dur_us"]) for o in objs],
                         [(7, 0, [100]), (8, 5100, [200, 4999, 300])])

    def test_palier_hors_repos_trop_long(self):
        # la sonde couperait la reception au milieu du palier (seuil RMT) : cas non simule
        with self.assertRaises(ValueError):
            signaux.simuler_sonde([(B, 18000)], 0, 5000, True)

    def krona(self, indices, silence_us):
        segs = []
        for i in indices:
            segs += signaux.trame_motif("krona", i) + [(B, signaux.pause_motif_us("krona"))]
        return signaux.simuler_sonde(segs, 0, silence_us, repos_haut=False)

    def test_krona_avec_silence_long(self):
        # 0x00 inverse = 18 ms hors repos : il faut silence_us > 18000
        objs = self.krona([2, 3, 4], 19000)
        self.assertEqual(len(objs), 3)
        self.assertTrue(all(o["fin"] and o["niv0"] == "haut" for o in objs))
        self.assertEqual(objs[1]["t_us"], 116000 + 18000)

    def test_krona_index_0_coupe_entre_deux_octets(self):
        # 0xFF + pause = 22 ms de repos dans la trame, plus que les 20 ms entre trames
        objs = self.krona([0], 19000)
        self.assertEqual(len(objs), 2)
        with self.assertRaises(ValueError):
            self.krona([0], 5000)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_signaux.py -v`
Attendu : ÉCHEC avec « ModuleNotFoundError: No module named 'signaux' », puis « FAILED (errors=1) ».

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `tools/signaux.py` :

```python
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
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_signaux.py -v`
Attendu : « Ran 27 tests », puis « OK ».

- [ ] **Étape 5 : Commit**

```bash
git add tools/signaux.py tools/tests/test_signaux.py
git commit -m "Ajouter les signaux synthetiques et la simulation de la sonde" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 3 : Lecture des captures (`tools/capture_fmt.py`)

**Fichiers :**
- Créer : `tools/capture_fmt.py`
- Tester : `tools/tests/test_capture_fmt.py`

**Interfaces :**
- Consomme (tests seulement) : `signaux.trame_motif`, `signaux.pause_motif_us`, `signaux.simuler_sonde`, `signaux.fusionner` (tâche 2).
- Produit (contrat, à la lettre) :
  - `@dataclass(frozen=True) class Partie: num: int; part: int; fin: bool; t_us: int; niv0_haut: bool; dur_us: tuple[int, ...]; debord: bool; rep: int | None` ;
  - `@dataclass(frozen=True) class Reception: num: int; t_us: int; niv0_haut: bool; dur_us: tuple[int, ...]; debord: bool; complete: bool` ;
  - `lire_jsonl(chemin) -> Iterator[dict]` (rend les objets `l` ; ligne illisible signalée sur stderr et ignorée) ;
  - `lire_serie_brute(chemin) -> Iterator[dict]` (dernier RS de chaque ligne, CR final toléré, texte ignoré) ;
  - `lire(chemin) -> Iterator[dict]` (`.jsonl`, sinon brute) ;
  - `parties(objets) -> list[Partie]` ;
  - `receptions(parts) -> list[Reception]` (une part 0 ouvre toujours une réception, redémarrage compris ; trou ou absence de fin : `complete = False`) ;
  - `flux(recs, repos_haut: bool) -> list[tuple[bool, int]]` (commence au premier front ; silence inséré seulement si l'écart est positif).
- Produit (ajout) : `repos_majoritaire(recs: list[Reception]) -> bool`, le niveau qui suit la dernière durée de chaque réception, à la majorité (haut en cas d'égalité ou sans réception) ; c'est le défaut de `--repos` dans `analyse.py`.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_capture_fmt.py` :

```python
#!/usr/bin/env python3
"""Tests de tools/capture_fmt.py : python3 -m unittest discover -s tools/tests -p test_capture_fmt.py -v"""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import capture_fmt  # noqa: E402
import signaux  # noqa: E402

H, B = True, False


def trame(num, part, fin, t_us, niv0, dur, **extra):
    o = {"v": 1, "t": "trame", "n": 1, "ms": 0, "num": num, "part": part, "fin": fin,
         "t_us": t_us, "niv0": niv0, "dur_us": dur, "debord": False}
    o.update(extra)
    return o


class Lecture(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def chemin(self, nom, contenu: bytes):
        p = os.path.join(self.dossier.name, nom)
        with open(p, "wb") as f:
            f.write(contenu)
        return p

    def test_jsonl(self):
        a = {"v": 1, "t": "hello", "n": 1, "ms": 5, "bloc": "base"}
        b = trame(1, 0, True, 10, "bas", [100])
        lignes = [json.dumps({"rx_ms": 1, "de": "usb", "l": a}), "",
                  json.dumps({"rx_ms": 2, "de": "192.168.1.20", "l": b}),
                  '{"rx_ms": 3, "de": "usb", "l": {"v": 1, "t": "tr']  # derniere ligne tronquee
        p = self.chemin("x.jsonl", "\n".join(lignes).encode())
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            objets = list(capture_fmt.lire_jsonl(p))
        self.assertEqual(objets, [a, b])
        self.assertIn("ligne 4 illisible", err.getvalue())

    def test_serie_brute(self):
        a = trame(1, 0, True, 10, "bas", [100])
        b = {"v": 1, "t": "log", "n": 2, "ms": 6, "src": "capture", "niv": "info", "txt": "ok"}
        brut = (b"firmware 0.1.0\r\n"
                + b"\x1e" + json.dumps(a, separators=(",", ":")).encode() + b"\n"
                + b"texte colle\x1e" + json.dumps(b, separators=(",", ":")).encode() + b"\r\n"
                + b"\x1e{mauvais\n"
                + b"sans fin de ligne")
        p = self.chemin("capture.log", brut)
        self.assertEqual(list(capture_fmt.lire_serie_brute(p)), [a, b])
        self.assertEqual(list(capture_fmt.lire(p)), [a, b])

    def test_lire_choisit_selon_l_extension(self):
        a = trame(1, 0, True, 10, "bas", [100])
        p = self.chemin("y.jsonl", json.dumps({"rx_ms": 1, "de": "usb", "l": a}).encode() + b"\n")
        self.assertEqual(list(capture_fmt.lire(p)), [a])


class Parties(unittest.TestCase):
    def test_conversion_et_filtre(self):
        objs = [{"v": 1, "t": "etat", "n": 1, "ms": 0, "bloc": "bus"},
                trame(3, 0, False, 1000, "haut", [10, 20], debord=True),
                trame(3, 1, True, 1030, "bas", [30], rep=4)]
        ps = capture_fmt.parties(objs)
        self.assertEqual(ps, [
            capture_fmt.Partie(num=3, part=0, fin=False, t_us=1000, niv0_haut=True, dur_us=(10, 20), debord=True, rep=None),
            capture_fmt.Partie(num=3, part=1, fin=True, t_us=1030, niv0_haut=False, dur_us=(30,), debord=False, rep=4)])


class Receptions(unittest.TestCase):
    def test_recolle_par_num(self):
        ps = capture_fmt.parties([
            trame(1, 0, False, 0, "bas", [10, 20]),
            trame(1, 1, False, 30, "bas", [30, 40]),
            trame(1, 2, True, 100, "bas", [50]),
            trame(2, 0, True, 5000, "haut", [7], debord=True)])
        self.assertEqual(capture_fmt.receptions(ps), [
            capture_fmt.Reception(num=1, t_us=0, niv0_haut=False, dur_us=(10, 20, 30, 40, 50), debord=False, complete=True),
            capture_fmt.Reception(num=2, t_us=5000, niv0_haut=True, dur_us=(7,), debord=True, complete=True)])

    def test_receptions_incompletes(self):
        ps = capture_fmt.parties([
            trame(1, 0, False, 0, "bas", [10]),
            trame(1, 2, True, 100, "bas", [50]),      # part 1 manquante
            trame(2, 1, True, 5000, "haut", [7]),     # part 0 manquante
            trame(3, 0, False, 9000, "bas", [9])])    # pas de fin
        recs = capture_fmt.receptions(ps)
        self.assertEqual([(r.num, r.dur_us, r.complete) for r in recs],
                         [(1, (10,), False), (1, (50,), False), (2, (7,), False), (3, (9,), False)])

    def test_redemarrage_de_la_sonde(self):
        ps = capture_fmt.parties([trame(5, 0, True, 900000, "bas", [10]),
                                  trame(1, 0, True, 1000, "bas", [20])])
        self.assertEqual([(r.num, r.t_us) for r in capture_fmt.receptions(ps)], [(5, 900000), (1, 1000)])


class Flux(unittest.TestCase):
    def test_silences_calcules_par_t_us(self):
        recs = [capture_fmt.Reception(1, 1000, False, (100, 50, 100), False, True),
                capture_fmt.Reception(2, 6250, False, (200,), False, True)]
        self.assertEqual(capture_fmt.flux(recs, repos_haut=True),
                         [(B, 100), (H, 50), (B, 100), (H, 5000), (B, 200)])

    def test_ecart_nul_ou_negatif_sans_silence(self):
        recs = [capture_fmt.Reception(1, 0, False, (100,), False, True),
                capture_fmt.Reception(2, 90, True, (30,), False, True)]
        self.assertEqual(capture_fmt.flux(recs, repos_haut=True), [(B, 100), (H, 30)])

    def test_aller_retour_avec_simuler_sonde(self):
        segs = []
        for i in range(3):
            segs += signaux.trame_motif("wtc", i) + [(H, signaux.pause_motif_us("wtc"))]
        objs = signaux.simuler_sonde(segs, 12345, 5000, True)
        recs = capture_fmt.receptions(capture_fmt.parties(objs))
        self.assertEqual(len(recs), 3)
        self.assertTrue(all(r.complete for r in recs))
        self.assertEqual(capture_fmt.flux(recs, True), signaux.fusionner(segs)[:-1])

    def test_repos_majoritaire(self):
        recs = [capture_fmt.Reception(1, 0, False, (100, 50, 100), False, True),   # fin sur bas : repos haut
                capture_fmt.Reception(2, 0, True, (10, 20), False, True),         # fin sur bas : repos haut
                capture_fmt.Reception(3, 0, True, (10,), False, True)]            # fin sur haut : repos bas
        self.assertTrue(capture_fmt.repos_majoritaire(recs))
        self.assertFalse(capture_fmt.repos_majoritaire(recs[2:]))
        self.assertTrue(capture_fmt.repos_majoritaire([]))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_capture_fmt.py -v`
Attendu : ÉCHEC avec « ModuleNotFoundError: No module named 'capture_fmt' », puis « FAILED (errors=1) ».

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `tools/capture_fmt.py` :

```python
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
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_capture_fmt.py -v`
Attendu : « Ran 11 tests », puis « OK ».

Lancer : `python3 -m unittest discover -s tools/tests -v`
Attendu : « Ran 38 tests », puis « OK ».

- [ ] **Étape 5 : Commit**

```bash
git add tools/capture_fmt.py tools/tests/test_capture_fmt.py
git commit -m "Ajouter la lecture des captures de la sonde" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 4 : Décodeurs, partie 1 : statistiques, découpage, UART (`tools/decodeurs.py`)

**Fichiers :**
- Créer : `tools/decodeurs.py`
- Tester : `tools/tests/test_decodeurs.py`

**Interfaces :**
- Consomme : `signaux.fusionner` (tâche 2) ; dans les tests, `signaux.uart`.
- Produit (contrat, à la lettre) :
  - `histogramme(durees: Iterable[int], pas_us: int = 10) -> list[tuple[int, int]]` ;
  - `regroupements(durees: Iterable[int], tol: float = 0.15) -> list[tuple[int, int]]` (une durée rejoint le groupe si elle ne dépasse pas le plus petit du groupe de plus de `tol` ; centre = moyenne arrondie) ;
  - `decouper(segments, silence_us: int, repos_haut: bool) -> list[list[tuple[bool, int]]]` ;
  - `@dataclass class Decodage: nom: str; params: dict; octets: bytes; bits: str; erreurs: int; symboles: int` ;
  - `uart(trame, bauds: int, inverse: bool = False) -> Decodage`, avec `params = {"bauds": bauds, "inverse": inverse}`.
- Produit (ajouts) : `Decodage.taux` (propriété : `erreurs / max(1, symboles)`) ; `decouper_dates(segments, silence_us, repos_haut) -> list[tuple[int, list[tuple[bool, int]]]]` (début de chaque trame en µs depuis le début du flux, pour `analyse.py trames`).

**Définition des erreurs UART.** Un octet tenté compte une erreur, au plus, s'il présente l'un de ces défauts :
- un faux départ (le milieu du bit de départ est déjà revenu à 1) ;
- un bit de stop faux (erreur de trame) ;
- un écart entre deux fronts de l'octet qui n'est pas un nombre entier de bits, à un tiers de bit près (§6 de la spec : les durées d'un UART sont des multiples du temps bit).

Sans cette dernière règle, un débit moitié ou double du vrai peut sortir sans erreur et passer en tête. Au-delà du dernier segment, le niveau est l'opposé de ce segment : c'est le repos réel, quelle que soit l'hypothèse. Une polarité fausse fait donc échouer le stop du dernier octet.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_decodeurs.py` :

```python
#!/usr/bin/env python3
"""Tests de tools/decodeurs.py : python3 -m unittest discover -s tools/tests -p test_decodeurs.py -v"""
import os
import random
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import decodeurs  # noqa: E402
import signaux  # noqa: E402

H, B = True, False


def isoler(segs, repos_haut):
    """Une trame telle que decouper la rend : sans repos avant ni apres."""
    trames = decodeurs.decouper(segs, 10**9, repos_haut)
    assert len(trames) == 1
    return trames[0]


def bruiter(segs, amplitude, graine=1):
    r = random.Random(graine)
    return [(n, d + r.randint(-amplitude, amplitude)) for n, d in segs]


class Statistiques(unittest.TestCase):
    def test_histogramme(self):
        self.assertEqual(decodeurs.histogramme([5, 12, 14, 25, 25]), [(0, 1), (10, 2), (20, 2)])
        self.assertEqual(decodeurs.histogramme([104, 208], pas_us=100), [(100, 1), (200, 1)])
        self.assertEqual(decodeurs.histogramme([]), [])

    def test_regroupements(self):
        self.assertEqual(decodeurs.regroupements([1502, 750, 752, 748, 1500, 2250]),
                         [(750, 3), (1501, 2), (2250, 1)])
        self.assertEqual(decodeurs.regroupements([]), [])

    def test_regroupements_largeur_bornee(self):
        # 100 et 116 : 116 > 100 * 1.15, deux groupes meme si les ecarts sont petits
        self.assertEqual(decodeurs.regroupements([100, 108, 116]), [(104, 2), (116, 1)])


class Decouper(unittest.TestCase):
    def test_decoupe_au_silence(self):
        segs = [(H, 9000), (B, 100), (H, 50), (B, 100), (H, 5000), (B, 200), (H, 4999), (B, 300), (H, 20)]
        self.assertEqual(decodeurs.decouper(segs, 5000, True),
                         [[(B, 100), (H, 50), (B, 100)], [(B, 200), (H, 4999), (B, 300)]])

    def test_decoupe_datee(self):
        segs = [(H, 9000), (B, 100), (H, 5000), (B, 200)]
        self.assertEqual(decodeurs.decouper_dates(segs, 5000, True),
                         [(9000, [(B, 100)]), (14100, [(B, 200)])])

    def test_repos_bas_et_fusion(self):
        segs = [(H, 10), (H, 20), (B, 7000), (H, 30)]
        self.assertEqual(decodeurs.decouper(segs, 5000, False), [[(H, 30)], [(H, 30)]])


class Uart(unittest.TestCase):
    OCTETS = bytes([0xA5, 0x5A, 0x00, 0xFF, 0x07, 0x05])

    def test_9600_repos_haut(self):
        d = decodeurs.uart(isoler(signaux.uart(self.OCTETS, 9600), True), 9600)
        self.assertEqual((d.nom, d.params), ("uart", {"bauds": 9600, "inverse": False}))
        self.assertEqual(d.octets, self.OCTETS)
        self.assertEqual(d.bits, "".join(format(o, "08b") for o in self.OCTETS))
        self.assertEqual((d.erreurs, d.symboles), (0, 6))
        self.assertEqual(d.taux, 0.0)

    def test_500_inverse_avec_pauses(self):
        segs = signaux.uart(self.OCTETS, 500, repos_haut=False, pause_octet_us=4000)
        d = decodeurs.uart(isoler(segs, False), 500, inverse=True)
        self.assertEqual((d.octets, d.erreurs, d.symboles), (self.OCTETS, 0, 6))

    def test_tolere_la_gigue(self):
        segs = bruiter(signaux.uart(self.OCTETS * 3, 2400), 8)
        d = decodeurs.uart(isoler(segs, True), 2400)
        self.assertEqual((d.octets, d.erreurs), (self.OCTETS * 3, 0))

    def test_bit_de_stop_faux(self):
        # un octet 0x00 dont le stop reste bas (8E1 ou debit faux) : erreur de trame
        d = decodeurs.uart([(B, 10 * 2000)], 500)
        self.assertEqual((d.octets, d.erreurs, d.symboles), (b"\x00", 1, 1))

    def test_front_hors_grille(self):
        # 9600 lu a 4800 : les fronts tombent au milieu des bits
        d = decodeurs.uart(isoler(signaux.uart(self.OCTETS, 9600), True), 4800)
        self.assertGreater(d.erreurs, 0)

    def test_mauvaise_polarite(self):
        segs = signaux.uart(self.OCTETS, 500, repos_haut=False)
        self.assertGreater(decodeurs.uart(isoler(segs, False), 500, inverse=False).erreurs, 0)

    def test_faux_depart(self):
        # impulsion basse de 10 us a 500 bauds : depart tente, erreur, aucun octet
        d = decodeurs.uart([(B, 10)], 500)
        self.assertEqual((d.octets, d.erreurs, d.symboles), (b"", 1, 1))

    def test_trame_vide(self):
        d = decodeurs.uart([], 500)
        self.assertEqual((d.octets, d.bits, d.erreurs, d.symboles, d.taux), (b"", "", 0, 0, 0.0))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_decodeurs.py -v`
Attendu : ÉCHEC avec « ModuleNotFoundError: No module named 'decodeurs' », puis « FAILED (errors=1) ».

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `tools/decodeurs.py` :

```python
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
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_decodeurs.py -v`
Attendu : « Ran 14 tests », puis « OK ».

Lancer : `python3 -m unittest discover -s tools/tests -v`
Attendu : « Ran 52 tests », puis « OK ».

- [ ] **Étape 5 : Commit**

```bash
git add tools/decodeurs.py tools/tests/test_decodeurs.py
git commit -m "Ajouter le decoupage en trames et le decodeur UART" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 5 : Décodeurs, partie 2 : distance d'impulsion, Manchester, classement `auto` (`tools/decodeurs.py`)

**Fichiers :**
- Modifier : `tools/decodeurs.py` (ajout, à la fin : `BAUDS`, `distance_impulsion`, `manchester`, `appliquer`, `auto`)
- Modifier : `tools/tests/test_decodeurs.py` (ajout des classes `DistanceImpulsion`, `Manchester`, `Auto`)
- Tester : `tools/tests/test_decodeurs.py`

**Interfaces :**
- Consomme : `regroupements`, `Decodage`, `uart`, `decouper` (tâche 4) ; `signaux.fusionner` ; dans les tests, `signaux.trame_motif`, `signaux.octets_motif`, `signaux.pause_motif_us`, `signaux.repos_haut_motif`, `signaux.distance_impulsion`, `signaux.manchester` (tâche 2).
- Produit (contrat, à la lettre) :
  - `distance_impulsion(trame, t_us: int | None = None) -> Decodage`, avec `params = {"t_us": T}` ;
  - `manchester(trame, t_us: int | None = None) -> Decodage`, avec `params = {"t_us": T}` ;
  - `auto(trames, bauds=(300, 500, 600, 1200, 2400, 4800, 9600, 19200)) -> list[Decodage]` : 18 hypothèses (UART à chaque débit, normal puis inversé, puis distance d'impulsion, puis Manchester), chacune cumulant toutes les trames ; T est estimé sur l'ensemble des trames.
- Produit (ajouts) :
  - `BAUDS` : le tuple par défaut ;
  - `appliquer(hyp: Decodage, trames) -> list[Decodage]` : redécode chaque trame avec le décodeur et les paramètres de `hyp`. `analyse.py` l'utilise, et `banc.py` (tâche 15) pourra le reprendre.

**Règles :**
- **T estimé :** c'est le centre du plus petit groupe d'au moins deux durées (à défaut, du plus petit groupe) : durées hautes pour la distance d'impulsion, toutes les durées pour Manchester. Tolérance : 25 % de T.
- **Distance d'impulsion :**
  - départ = palier bas de 2T ; `0` = 1T haut puis 1T bas ; `1` = 1T haut puis 3T bas ;
  - un palier haut qui ne vaut pas 1T clôt le message : c'est la fin, ou le blanc avant la réponse de la carte ;
  - un bas de 2T à la place d'un bit ouvre un nouveau message ;
  - erreurs = paliers bas non classables ; après une erreur, on attend un nouveau départ ;
  - `bits` : un groupe par message, séparés par une espace ; `octets` : chaque message emballé MSB d'abord, complété par des 0.
- **Manchester :**
  - chaque palier vaut 1 ou 2 demi-bits ;
  - la phase retenue est celle qui donne le moins de paires sans transition au milieu : on essaie avec et sans un demi-bit de tête, qui peut être fondu dans le repos ;
  - erreurs = paliers qui ne valent ni T ni 2T, plus les paires sans transition au milieu.
- **Classement :**
  - taux d'erreur croissant ;
  - à taux égal, moins de symboles d'abord : un UART à débit double tente plus d'octets ;
  - une hypothèse sans symbole va en queue.

- [ ] **Étape 1 : Écrire le test qui échoue**

Dans `tools/tests/test_decodeurs.py`, repérer la fin de la classe `Uart` et le bloc final :

```python
    def test_trame_vide(self):
        d = decodeurs.uart([], 500)
        self.assertEqual((d.octets, d.bits, d.erreurs, d.symboles, d.taux), (b"", "", 0, 0, 0.0))


if __name__ == "__main__":
```

et insérer, entre la dernière ligne de `test_trame_vide` (suivie de ses deux lignes vides) et `if __name__ == "__main__":`, ces trois classes (suivies de deux lignes vides) :

```python
class DistanceImpulsion(unittest.TestCase):
    def test_trame_wtc_et_reponse(self):
        d = decodeurs.distance_impulsion(isoler(signaux.trame_motif("wtc", 3), True))
        self.assertEqual((d.nom, d.params), ("distance_impulsion", {"t_us": 750}))
        self.assertEqual(d.bits, "0000100000000000 00000011")
        self.assertEqual(d.octets, signaux.octets_motif("wtc", 3))
        self.assertEqual((d.erreurs, d.symboles), (0, 24))

    def test_t_impose_et_gigue(self):
        segs = bruiter(signaux.distance_impulsion("1011001110001111", 500), 60)
        d = decodeurs.distance_impulsion(isoler(segs, True), t_us=500)
        self.assertEqual((d.bits, d.octets, d.erreurs), ("1011001110001111", b"\xb3\x8f", 0))

    def test_message_incomplet_complete_par_des_zeros(self):
        d = decodeurs.distance_impulsion(isoler(signaux.distance_impulsion("101"), True), t_us=750)
        self.assertEqual((d.bits, d.octets), ("101", b"\xa0"))

    def test_palier_non_classable(self):
        segs = signaux.distance_impulsion("0000", 750)
        segs[4] = (B, 5 * 750)  # ni 1T ni 3T : erreur, puis attente d'un nouveau depart
        d = decodeurs.distance_impulsion(isoler(segs, True), t_us=750)
        self.assertEqual((d.bits, d.erreurs), ("0", 3))  # le palier, puis les deux bas de 1T hors message

    def test_sans_palier_haut(self):
        d = decodeurs.distance_impulsion([(B, 1500)])
        self.assertEqual((d.params, d.symboles, d.erreurs), ({"t_us": None}, 0, 1))


class Manchester(unittest.TestCase):
    def test_bits_ieee(self):
        d = decodeurs.manchester(isoler(signaux.manchester("1011001110001111", 100), True))
        self.assertEqual((d.nom, d.params), ("manchester", {"t_us": 100}))
        self.assertEqual((d.bits, d.octets, d.erreurs, d.symboles), ("1011001110001111", b"\xb3\x8f", 0, 16))

    def test_premier_demi_bit_fondu_dans_le_repos(self):
        # 0 en tete avec repos haut : le demi-bit haut est invisible, la phase est retrouvee
        segs = [(H, 10000)] + signaux.manchester("0110", 100) + [(H, 10000)]
        d = decodeurs.manchester(isoler(segs, True))
        self.assertEqual((d.bits, d.erreurs), ("0110", 0))

    def test_palier_de_3t(self):
        segs = [(B, 100), (H, 300), (B, 100)]
        self.assertGreater(decodeurs.manchester(segs, t_us=100).erreurs, 0)


class Auto(unittest.TestCase):
    def trames(self, motif, indices, silence_us):
        segs = []
        for i in indices:
            segs += signaux.trame_motif(motif, i) + [(signaux.repos_haut_motif(motif), signaux.pause_motif_us(motif))]
        return decodeurs.decouper(segs, silence_us, signaux.repos_haut_motif(motif))

    def verifier_tete(self, motif, indices, silence_us, nom, params):
        trames = self.trames(motif, indices, silence_us)
        hyps = decodeurs.auto(trames)
        self.assertEqual(len(hyps), 18)
        tete = hyps[0]
        self.assertEqual((tete.nom, tete.params, tete.erreurs), (nom, params, 0))
        attendu = b"".join(signaux.octets_motif(motif, i) for i in indices)
        self.assertEqual(tete.octets, attendu)
        self.assertEqual(b"".join(d.octets for d in decodeurs.appliquer(tete, trames)), attendu)
        taux = [h.taux for h in hyps if h.symboles]
        self.assertEqual(taux, sorted(taux))
        return hyps

    def test_krona_uart_500_inverse(self):
        self.verifier_tete("krona", range(2, 8), 19000, "uart", {"bauds": 500, "inverse": True})

    def test_uart_9600(self):
        self.verifier_tete("uart9600", range(10), 5000, "uart", {"bauds": 9600, "inverse": False})

    def test_uart_2400_inverse(self):
        self.verifier_tete("uart2400inv", range(10), 5000, "uart", {"bauds": 2400, "inverse": True})

    def test_wtc(self):
        self.verifier_tete("wtc", range(10), 5000, "distance_impulsion", {"t_us": 750})

    def test_manchester(self):
        trames = [isoler(signaux.manchester(format(0xA000 | i * 37, "016b"), 250), True) for i in range(8)]
        tete = decodeurs.auto(trames)[0]
        self.assertEqual((tete.nom, tete.params, tete.erreurs), ("manchester", {"t_us": 250}, 0))

    def test_sans_trame(self):
        self.assertEqual(decodeurs.auto([]), [])
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_decodeurs.py -v`
Attendu : ÉCHEC avec « AttributeError: module 'decodeurs' has no attribute 'distance_impulsion' » (et `'manchester'`, `'auto'`), puis « FAILED (errors=14) » ; les 14 tests de la tâche 4 passent toujours.

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Dans `tools/decodeurs.py`, après la fin de la fonction `uart` :

```python
    bits = "".join(format(o, "08b") for o in octets)
    return Decodage("uart", params, bytes(octets), bits, erreurs, symboles)
```

ajouter, après deux lignes vides :

```python
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
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_decodeurs.py -v`
Attendu : « Ran 28 tests », puis « OK ».

Lancer : `python3 -m unittest discover -s tools/tests -v`
Attendu : « Ran 66 tests », puis « OK ».

Contrôle mesuré lors de la rédaction : sur les motifs du banc, 1 000 trames par motif, avec ou sans gigue de ±2 µs et asymétrie de 5 µs, `auto` met en tête le bon décodeur, sans erreur et avec les bons octets. C'est vrai pour `uart500`, `uart500inv`, `uart2400`, `uart2400inv`, `uart9600`, `uart9600inv`, `wtc` et `krona` (silence de 19 000 µs pour `uart500`, `uart500inv` et `krona`, 5 000 µs pour les autres). Il faut moins d'une seconde par motif. La deuxième hypothèse est loin derrière : au mieux 1 000 erreurs sur 25 000 symboles (`wtc` lu en UART à 9 600 bauds).

- [ ] **Étape 5 : Commit**

```bash
git add tools/decodeurs.py tools/tests/test_decodeurs.py
git commit -m "Ajouter les decodeurs distance d'impulsion et Manchester et le classement auto" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 6 : Outil d'analyse (`tools/analyse.py`) et tests de bout en bout

**Fichiers :**
- Créer : `tools/analyse.py`
- Tester : `tools/tests/test_analyse.py`

**Interfaces :**
- Consomme :
  - de `capture_fmt` (tâche 3) : `lire`, `parties`, `receptions`, `flux`, `repos_majoritaire` ;
  - de `decodeurs` (tâches 4 et 5) : `decouper_dates`, `regroupements`, `histogramme`, `auto`, `appliquer`, `Decodage` ;
  - dans les tests, de `signaux` : `trame_motif`, `pause_motif_us`, `repos_haut_motif`, `simuler_sonde`, `octets_motif`.
- Produit :
  - la ligne de commande du contrat : `histo <capture> [--pas us]`, `trames <capture> --silence us [--max n]`, `auto <capture> [--silence us] [--max n]`, `uart <capture> <bauds> [--inverse] [--silence us] [--max n]`, `diff <a> <b> [--silence us]`, avec l'option commune `--repos haut|bas` (défaut : `capture_fmt.repos_majoritaire`) ;
  - les fonctions `main(argv=None) -> int` (0 ; 1 sans trame ; 2 si le fichier est illisible), `charger(chemin, repos=None, silence_us=None) -> (trames datées [(t_us, segments)], repos_haut, réceptions)`, `niveaux(recs) -> (hautes, basses, silences)`, `positions_octets(a, b)` et `positions_bits(a, b)`.
- **Découpage :**
  - sans `--silence`, chaque réception de la sonde est une trame ;
  - avec `--silence`, le flux (silences recalculés par `t_us`) est redécoupé (étape 6 de la spec).
- **`diff` :**
  - décode `a` et `b` avec la meilleure hypothèse de `a` ;
  - liste les contenus distincts de chaque capture, avec leurs effectifs ;
  - compare les contenus les plus fréquents : octets pour l'UART (avec les numéros des bits qui changent, LSB = 0), bits par message sinon.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_analyse.py`. Les captures `.jsonl` et série brute y sont fabriquées par `signaux.simuler_sonde`, au format `.jsonl` du contrat :

```python
#!/usr/bin/env python3
"""Tests de bout en bout de tools/analyse.py sur des captures synthetiques
(signaux.simuler_sonde) : python3 -m unittest discover -s tools/tests -p test_analyse.py -v"""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import analyse  # noqa: E402
import decodeurs  # noqa: E402
import signaux  # noqa: E402


def objets_sonde(motif, indices, silence_us, t0_us=1000000):
    rh = signaux.repos_haut_motif(motif)
    segs = []
    for i in indices:
        segs += signaux.trame_motif(motif, i) + [(rh, signaux.pause_motif_us(motif))]
    return signaux.simuler_sonde(segs, t0_us, silence_us, rh)


class Capture(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def jsonl(self, nom, motif, indices, silence_us):
        """Capture au format .jsonl, precedee d'un hello comme une vraie session."""
        objs = [{"v": 1, "t": "hello", "n": 0, "ms": 0, "bloc": "base"}] + objets_sonde(motif, indices, silence_us)
        p = os.path.join(self.dossier.name, nom + ".jsonl")
        with open(p, "w") as f:
            for k, o in enumerate(objs):
                f.write(json.dumps({"rx_ms": 1790000000000 + k, "de": "192.168.1.30", "l": o}) + "\n")
        return p

    def brute(self, nom, motif, indices, silence_us):
        """Capture serie brute : texte humain, puis lignes RS + JSON compact + LF."""
        p = os.path.join(self.dossier.name, nom + ".log")
        with open(p, "wb") as f:
            f.write(b"firmware 0.1.0+abc1234\r\n")
            for o in objets_sonde(motif, indices, silence_us):
                f.write(b"\x1e" + json.dumps(o, separators=(",", ":")).encode() + b"\n")
        return p

    def lancer(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = analyse.main(list(argv))
        return code, out.getvalue()


class Auto(Capture):
    def verifier(self, chemin, motif, indices, nom, params):
        trames, _, _ = analyse.charger(chemin)
        tete = decodeurs.auto([t for _, t in trames])[0]
        self.assertEqual((tete.nom, tete.params, tete.erreurs), (nom, params, 0))
        decs = decodeurs.appliquer(tete, [t for _, t in trames])
        self.assertEqual([d.octets for d in decs], [signaux.octets_motif(motif, i) for i in indices])

    def test_krona_uart_500_inverse(self):
        p = self.jsonl("krona", "krona", range(2, 8), 19000)
        self.verifier(p, "krona", range(2, 8), "uart", {"bauds": 500, "inverse": True})
        code, sortie = self.lancer("auto", p)
        self.assertEqual(code, 0)
        self.assertIn("6 trames, repos bas, une trame par reception", sortie)
        self.assertIn(" 1. uart 500 bauds inverse : 0 erreurs / 30 symboles (0.0 %)", sortie)
        self.assertIn("#0 t=1000000 us : 55 02 fd 00 54", sortie)

    def test_krona_trames_coupees_entre_deux_octets(self):
        # index 0 et 1 : 0xFF ou 0xFE + pause >= 19 ms, la sonde coupe la trame ; aucun octet perdu
        p = self.jsonl("krona", "krona", range(10), 19000)
        trames, _, _ = analyse.charger(p)
        self.assertEqual(len(trames), 14)
        tete = decodeurs.auto([t for _, t in trames])[0]
        self.assertEqual((tete.nom, tete.params, tete.erreurs), ("uart", {"bauds": 500, "inverse": True}, 0))
        self.assertEqual(tete.octets, b"".join(signaux.octets_motif("krona", i) for i in range(10)))

    def test_uart_9600(self):
        p = self.jsonl("u9600", "uart9600", range(10), 5000)
        self.verifier(p, "uart9600", range(10), "uart", {"bauds": 9600, "inverse": False})
        code, sortie = self.lancer("auto", p, "--max", "3")
        self.assertIn(" 1. uart 9600 bauds : 0 erreurs / 60 symboles (0.0 %)", sortie)
        self.assertIn("#2 t=", sortie)
        self.assertIn("a5 5a 00 ff 02 00", sortie)
        self.assertIn("... 7 trames de plus (--max 0 : toutes)", sortie)

    def test_uart_9600_capture_brute(self):
        p = self.brute("u9600", "uart9600", range(10), 5000)
        self.verifier(p, "uart9600", range(10), "uart", {"bauds": 9600, "inverse": False})

    def test_wtc(self):
        p = self.jsonl("wtc", "wtc", range(10), 5000)
        self.verifier(p, "wtc", range(10), "distance_impulsion", {"t_us": 750})
        code, sortie = self.lancer("auto", p)
        self.assertIn(" 1. distance_impulsion T=750 us : 0 erreurs / 240 symboles (0.0 %)", sortie)
        self.assertIn(": 0000000100000000 00000000 = 01 00 00", sortie)

    def test_uart_impose(self):
        p = self.jsonl("krona", "krona", range(2, 8), 19000)
        code, sortie = self.lancer("uart", p, "500", "--inverse")
        self.assertEqual(code, 0)
        self.assertIn("uart 500 bauds inverse : 0 erreurs / 30 symboles (0.0 %)", sortie)
        code, sortie = self.lancer("uart", p, "500")
        self.assertNotIn("uart 500 bauds : 0 erreurs", sortie)


class Trames(Capture):
    def test_redecoupage_au_silence(self):
        # sonde a 19 ms : le motif wtc (6,75 ms entre trames) arrive en une seule reception
        p = self.jsonl("wtc19", "wtc", range(10), 19000)
        trames, _, recs = analyse.charger(p)
        self.assertEqual((len(recs), len(trames)), (1, 1))
        code, sortie = self.lancer("trames", p, "--silence", "5000")
        self.assertEqual(code, 0)
        self.assertIn("10 trames (repos haut, silence 5000 us)", sortie)
        self.assertIn("#1 t=1051000 us, 51 paliers, 45750 us, 6750 us apres la precedente", sortie)
        self.assertIn("  b1500 h750 b750 h750", sortie)
        code, sortie = self.lancer("auto", p, "--silence", "5000")
        self.assertIn(" 1. distance_impulsion T=750 us : 0 erreurs / 240 symboles", sortie)


class Diff(Capture):
    def test_octets_qui_changent(self):
        a = self.jsonl("a", "krona", [2, 2, 2], 19000)
        b = self.jsonl("b", "krona", [3, 3, 3, 3], 19000)
        code, sortie = self.lancer("diff", a, b)
        self.assertEqual(code, 0)
        self.assertIn("hypothese (meilleure pour a) : uart 500 bauds inverse", sortie)
        self.assertIn("      3 x 55 02 fd 00 54", sortie)
        self.assertIn("      4 x 55 03 fc 00 54", sortie)
        self.assertIn("octets qui changent : 1 (02 -> 03 : bit 0), 2 (fd -> fc : bit 0)", sortie)

    def test_bits_qui_changent(self):
        a = self.jsonl("a", "wtc", [0, 0, 0], 5000)
        b = self.jsonl("b", "wtc", [1, 1], 5000)
        code, sortie = self.lancer("diff", a, b)
        self.assertIn("bits qui changent : message 0 bit 6 (0 -> 1), message 0 bit 7 (1 -> 0), "
                      "message 1 bit 7 (0 -> 1)", sortie)

    def test_aucune_difference(self):
        a = self.jsonl("a", "uart2400", [5, 5], 5000)
        code, sortie = self.lancer("diff", a, a)
        self.assertIn("aucune difference", sortie)

    def test_positions(self):
        self.assertEqual(analyse.positions_octets(b"\x01\x02", b"\x01\x03\x04"), [(1, 2, 3), (2, None, 4)])
        self.assertEqual(analyse.positions_bits("0110 01", "0100"), [(0, 2, "1", "0"), (1, 0, "0", None),
                                                                       (1, 1, "1", None)])


class Histo(Capture):
    def test_regroupements(self):
        p = self.jsonl("u500", "uart500", range(4), 19000)
        _, _, recs = analyse.charger(p)
        hautes, basses, silences = analyse.niveaux(recs)
        self.assertEqual(decodeurs.regroupements(hautes)[0][0], 2000)
        self.assertEqual(len(silences), 3)
        code, sortie = self.lancer("histo", p, "--pas", "1000")
        self.assertEqual(code, 0)
        self.assertIn("4 receptions, repos haut", sortie)
        self.assertIn("durees hautes : ", sortie)
        self.assertIn("     2000 us : ", sortie)
        self.assertIn("silences entre receptions : 3", sortie)


class Repos(Capture):
    def test_deduit_ou_impose(self):
        p = self.jsonl("krona", "krona", range(2, 5), 19000)
        self.assertFalse(analyse.charger(p)[1])
        self.assertTrue(analyse.charger(p, repos="haut")[1])
        code, sortie = self.lancer("auto", p, "--repos", "haut")
        self.assertIn("repos haut", sortie)

    def test_capture_sans_trame(self):
        p = os.path.join(self.dossier.name, "vide.jsonl")
        with open(p, "w") as f:
            f.write(json.dumps({"rx_ms": 1, "de": "usb", "l": {"v": 1, "t": "hello", "n": 0, "ms": 0}}) + "\n")
        code, sortie = self.lancer("auto", p)
        self.assertEqual(code, 1)
        self.assertIn("aucune trame", sortie)

    def test_fichier_absent(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code, _ = self.lancer("auto", os.path.join(self.dossier.name, "absent.jsonl"))
        self.assertEqual(code, 2)
        self.assertIn("analyse.py :", err.getvalue())


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_analyse.py -v`
Attendu : ÉCHEC avec « ModuleNotFoundError: No module named 'analyse' », puis « FAILED (errors=1) ».

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `tools/analyse.py` :

```python
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
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_analyse.py -v`
Attendu : « Ran 15 tests », puis « OK ».

Lancer : `python3 -m unittest discover -s tools/tests -v`
Attendu : « Ran 81 tests », puis « OK » (plus si d'autres tâches ont déjà ajouté des tests Python, mais aucun échec).

Essai à la main, sur une capture `krona` synthétique :

```sh
python3 - <<'EOF'
import json, os, sys
sys.path.insert(0, "tools")
import signaux
segs = []
for i in range(2, 8):
    segs += signaux.trame_motif("krona", i) + [(False, signaux.pause_motif_us("krona"))]
chemin = os.path.join(os.environ.get("TMPDIR", "/tmp"), "krona.jsonl")
with open(chemin, "w") as f:
    for k, o in enumerate(signaux.simuler_sonde(segs, 1000000, 19000, False)):
        f.write(json.dumps({"rx_ms": 1790000000000 + k, "de": "usb", "l": o}) + "\n")
EOF
python3 tools/analyse.py auto "${TMPDIR:-/tmp}/krona.jsonl" --max 3
```

Attendu (extrait) :

```text
<chemin>/krona.jsonl : 6 trames, repos bas, une trame par reception
hypotheses (taux d'erreur croissant) :
   1. uart 500 bauds inverse : 0 erreurs / 30 symboles (0.0 %)
   2. uart 2400 bauds inverse : 25 erreurs / 83 symboles (30.1 %)
   3. uart 2400 bauds : 28 erreurs / 77 symboles (36.4 %)
   ...
  18. distance_impulsion T=2000 us : aucun symbole (74 erreurs)
trames decodees avec : uart 500 bauds inverse
  #0 t=1000000 us : 55 02 fd 00 54
  #1 t=1134000 us : 55 03 fc 00 54
  #2 t=1268000 us : 55 04 fb 00 54
  ... 3 trames de plus (--max 0 : toutes)
```

- [ ] **Étape 5 : Commit**

```bash
git add tools/analyse.py tools/tests/test_analyse.py
git commit -m "Ajouter l'outil d'analyse des captures" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
