#!/usr/bin/env python3
# Copie de benq-screenbar-halo-matter@c58a506 : tools/json_check.py (adapte : schemas du profil hotte, option --jsonl, continuite par source, taux de perte)
"""Verifie des lignes machine de la sonde contre docs/PROTOCOLE-JSON.md (v1, profil hotte).

Entree :
  - une capture brute du port serie (octets tels quels : texte humain, logs,
    lignes machine RS + JSON + LF), par exemple un fichier
    platformio-device-monitor-*.log. Le decoupage suit la section 2.4 du
    protocole de la ScreenBar : dernier RS de chaque ligne, CR final tolere,
    texte hors RS compte a part ;
  - ou un fichier .jsonl de serie_enregistre.py ou hotte_udp.py (option
    --jsonl, implicite pour l'extension .jsonl) : une ligne
    {"rx_ms": ..., "de": ..., "l": <objet recu>} par message ; l'objet 'l'
    est verifie comme une ligne machine (RS + 'l' tel quel, cles en double
    comprises), la continuite de n suivie par source ('de').

Pour chaque ligne machine :
  - tramage : RS, '{"v":', ASCII imprimable, 1024 octets au plus RS et LF
    compris (au-dela de 896 : avertissement, budget de pire cas), forme
    compacte ;
  - enveloppe : v, t, n, ms en tete et dans cet ordre, puis bloc pour hello,
    etat, compteurs, reseau ; entiers seulement (t_us jusqu'a 2^53 - 1) ;
  - contenu : champs obligatoires, types, bornes, enumerations, tailles des
    chaines, coherences simples (etape et code d'une reponse, reglages de
    capture, trame vide...) ; un champ inconnu est un avertissement ;
  - continuite : trous de n (pertes, avec leur taux), n qui recule.

Usage :
  python3 tools/json_check.py capture.log [autre.log ...]
  python3 tools/json_check.py --jsonl logs/2026-09-27-1403-krona.jsonl
  python3 tools/json_check.py --exemples docs/PROTOCOLE-JSON.md
  options : --strict (avertissements comptes comme erreurs), -q (resume seul),
            --independantes (lignes sans suite : pas de controle de n)

Code de sortie : 0 si aucune erreur, 1 sinon, 2 si l'entree est illisible.
"""
import argparse
import json
import re
import sys

sys.dont_write_bytecode = True

RS = 0x1E
LINE_MAX = 1024
BUDGET = 896
U32_MAX = 2**32 - 1

# Profil hotte (src/config.h, src/capture_model.h, src/injection_regles.h).
RESOLUTIONS = (1000000, 500000)
SILENCE_MIN_US = 1000
SILENCE_MAX_TICKS = 32767
DUR_MAX = 110       # durees par ligne trame
INJ_DUR_MAX = 64    # durees par injection

# ---------------------------------------------------------------------------
#  Petit langage de schema
# ---------------------------------------------------------------------------


class Int:
    def __init__(self, lo=0, hi=U32_MAX):
        self.lo, self.hi = lo, hi


class Bool:
    pass


class Str:
    def __init__(self, maxlen=255, pattern=None):
        self.maxlen, self.pattern = maxlen, re.compile(pattern) if pattern else None


class Enum:
    def __init__(self, *values):
        self.values = set(values)


class Hex:
    """Chaine hexadecimale majuscule sans 0x, de lo a hi octets."""

    def __init__(self, lo, hi=None):
        self.lo, self.hi = lo, hi if hi is not None else lo


class Obj:
    def __init__(self, fields, extra_ok=False):
        self.fields, self.extra_ok = fields, extra_ok


class Arr:
    def __init__(self, item, maxlen=None):
        self.item, self.maxlen = item, maxlen


class Null:
    """Valeur pouvant valoir null."""

    def __init__(self, spec):
        self.spec = spec


class Opt:
    """Champ pouvant etre absent."""

    def __init__(self, spec):
        self.spec = spec


U32 = Int()
U8 = Int(0, 255)
U53 = Int(0, 2**53 - 1)  # t_us : entier sur 64 bits, sur en JSON jusqu'a 2^53 - 1
BOOL = Bool()
BOOT = Hex(4)
NIVEAU = Enum("haut", "bas")
MODE = Enum("tout", "changements")
RESET = Enum(
    "mise_sous_tension", "broche", "logiciel", "panique", "chien_int", "chien_tache", "chien",
    "baisse_tension", "usb", "inconnue",
)
OK_CODES = {"ok", "accepte", "en_cours", "execute"}
KO_CODES = {"usage", "refuse", "inconnue", "trop_long", "cadence", "interdite", "deja_traite"}


def counters(*names):
    return Obj({n: U32 for n in names})


SCHEMAS = {
    ("hello", "base"): Obj(
        {
            "rev": U32,
            "fw": Str(64),
            "fw_desc": Str(32),
            "date": Str(16),
            "heure": Str(16),
            "env": Str(64),
            "build": Enum("sonde"),
            "reseau_build": Enum("aucun"),
            "puce": Str(32),
            "idf": Str(32),
            "arduino": Str(32),
            "boot": BOOT,
            "reset": RESET,
            "reset_n": U32,
            "up_s": U32,
            "session": Obj(
                {
                    "transport": Enum("usb", "udp"),
                    "periode_ms": U32,
                    "compteurs_ms": U32,
                    "reseau_ms": U32,
                    "bail_s": U32,
                    "trames": BOOL,
                    "log": BOOL,
                }
            ),
            "limites": Obj({"ligne_max": U32, "cmd_max": U32}),
        }
    ),
    ("hello", "identite"): Obj(
        {
            "boot": BOOT,
            "mac": Null(Hex(6)),
            "id": Obj(
                {
                    "fabricant": Str(32),
                    "produit": Str(32),
                    "serie": Null(Str(32, r"^HOTTE-[0-9A-F]{12}$")),
                    "nom": Str(32),
                    "hw": Int(0, 0xFFFF),
                    "hw_txt": Str(64),
                }
            ),
            "appareil": Enum("hotte"),
            "caps": Arr(Enum("sonde", "injection", "trames", "log", "udp", "cle", "mdns")),
        }
    ),
    ("config", None): Obj(
        {
            "capture": Obj(
                {
                    "gpio": U8,
                    "resol_hz": U32,
                    "filtre_us": Int(0, 3),
                    "silence_us": U32,
                    "mode": MODE,
                    "inverse": BOOL,
                }
            ),
        }
    ),
    ("etat", "bus"): Obj(
        {
            "boot": BOOT,
            "up_s": U32,
            "repos": Null(NIVEAU),
            "fronts": U32,
            "derniere_ms": Null(U32),
            "receptions_s": U32,
        }
    ),
    ("etat", "capture"): Obj(
        {
            "boot": BOOT,
            "up_s": U32,
            "active": BOOL,
            "mode": MODE,
            "debord": U32,
            "rep_en_cours": U32,
        }
    ),
    ("etat", "sys"): Obj(
        {
            "boot": BOOT,
            "up_s": U32,
            "sys": counters(
                "heap", "heap_min", "heap_bloc", "pile_boucle", "boucle_max_ms", "json_perdus", "json_trop_longs",
                "rejets",
            ),
        }
    ),
    ("compteurs", "sonde"): counters(
        "receptions", "parties", "blocs", "symboles", "debord", "rep", "lignes_perdues", "sautes", "rejets"
    ),
    ("hb", None): Obj({"boot": BOOT, "up_s": U32, "json_perdus": U32}),
    ("fin", None): Obj({"cause": Enum("commande", "bail")}),
    ("reponse", None): Obj(
        {
            "id": Int(1, 999999999),
            "etape": Enum("debut", "fin"),
            "cmd": Str(40),
            "ok": BOOL,
            "code": Enum(*(OK_CODES | KO_CODES)),
            "msg": Opt(Str(120)),
            "duree_ms": Opt(U32),
            "suite": Opt(Enum("injection")),
            "bail_s": Opt(U32),
            "up_s": Opt(U32),
            # 'json cle' (USB, transport reseau) : cle rendue une seule fois, empreinte (null sans cle).
            "cle": Opt(Hex(32)),
            "empreinte": Opt(Null(Str(8, r"^[0-9A-F]{8}$"))),
        }
    ),
    ("trame", None): Obj(
        {
            "num": U32,
            "part": U32,
            "fin": BOOL,
            "t_us": U53,
            "niv0": NIVEAU,
            "dur_us": Arr(Int(1, U32_MAX), DUR_MAX),
            "debord": BOOL,
            "rep": Opt(U32),
            "sautes": Opt(U32),
        }
    ),
    ("injection", None): Obj(
        {
            "id": Null(Int(1, 999999999)),
            "cmd": Str(40),
            "resultat": Enum("ok", "collision", "delai"),
            "niv0": NIVEAU,
            "dur_us": Arr(Int(1, U32_MAX), INJ_DUR_MAX),
            "attente_us": U32,
            "relu_us": Arr(U32, INJ_DUR_MAX),
        }
    ),
    ("log", None): Obj(
        {
            "src": Enum("sonde", "capture", "injection", "reseau"),
            "niv": Enum("notice", "trace"),
            "txt": Str(191),
            "sautes": Opt(U32),
        }
    ),
    # Transport UDP sur le Wi-Fi : schema du bloc ip de la ScreenBar (srp toujours
    # null, tampons OpenThread null), plus mdns et wifi.
    ("reseau", "ip"): Obj(
        {
            "frais_ms": Null(U32),
            "srp": Null(Obj({"nom": Null(Str(63))})),
            "adresses": Arr(Obj({"adr": Str(45), "type": Enum("omr", "ml_eid", "autre"), "pref": BOOL}), 4),
            "udp": Obj(
                {
                    "port": Int(1, 65535),
                    "ouvert": BOOL,
                    "empreinte": Null(Str(8, r"^[0-9A-F]{8}$")),
                    "sessions": Int(0, 2),
                    "provisoire": BOOL,
                    "rx": U32,
                    "rejets": U32,
                    "rx_perdus": U32,
                    "defis": U32,
                    "tx": U32,
                    "tx_perdus": U32,
                    "tx_erreurs": U32,
                    "tampons_libres": Null(U32),
                    "tampons_min": Null(U32),
                }
            ),
            "mdns": Obj({"nom": Str(63)}),
            "wifi": Obj(
                {
                    "connecte": BOOL,
                    "rssi_dbm": Null(Int(-128, 0)),
                    "ip": Null(Str(15, r"^[0-9]{1,3}(\.[0-9]{1,3}){3}$")),
                    "pertes": U32,
                }
            ),
        }
    ),
}

BLOCKED = {"hello", "etat", "compteurs", "reseau"}
BLOCS = {t: {b for (tt, b) in SCHEMAS if tt == t} for t in BLOCKED}

# ---------------------------------------------------------------------------
#  Verification
# ---------------------------------------------------------------------------


def check(spec, value, path, errs, warns):
    """Verifie value contre spec. Champ inconnu : avertissement (un ajout garde
    v et l'app l'ignore) ; --strict en fait une erreur."""
    if isinstance(spec, Opt):
        spec = spec.spec
    if isinstance(spec, Null):
        if value is None:
            return
        spec = spec.spec
    if value is None:
        errs.append(f"{path} : null interdit")
        return
    if isinstance(spec, Int):
        if isinstance(value, bool) or not isinstance(value, int):
            errs.append(f"{path} : entier attendu, {type(value).__name__} ({value!r})")
        elif not spec.lo <= value <= spec.hi:
            errs.append(f"{path} : {value} hors de {spec.lo}..{spec.hi}")
    elif isinstance(spec, Bool):
        if not isinstance(value, bool):
            errs.append(f"{path} : booleen attendu ({value!r})")
    elif isinstance(spec, Str):
        if not isinstance(value, str):
            errs.append(f"{path} : chaine attendue ({value!r})")
        else:
            if len(value) > spec.maxlen:
                errs.append(f"{path} : {len(value)} caracteres, {spec.maxlen} au plus")
            if spec.pattern and not spec.pattern.search(value):
                errs.append(f"{path} : {value!r} ne suit pas {spec.pattern.pattern}")
    elif isinstance(spec, Enum):
        if not isinstance(value, str):
            errs.append(f"{path} : chaine d'enumeration attendue ({value!r})")
        elif value not in spec.values:
            errs.append(f"{path} : valeur inconnue {value!r} (attendu : {', '.join(sorted(spec.values))})")
    elif isinstance(spec, Hex):
        if not isinstance(value, str) or not re.fullmatch(r"(?:[0-9A-F]{2})*", value):
            errs.append(f"{path} : hexa majuscule sans 0x attendu ({value!r})")
        elif not spec.lo <= len(value) // 2 <= spec.hi:
            errs.append(f"{path} : {len(value) // 2} octet(s), {spec.lo}..{spec.hi} attendu(s)")
    elif isinstance(spec, Arr):
        if not isinstance(value, list):
            errs.append(f"{path} : tableau attendu ({value!r})")
            return
        if spec.maxlen is not None and len(value) > spec.maxlen:
            errs.append(f"{path} : {len(value)} elements, {spec.maxlen} au plus")
        for i, v in enumerate(value):
            check(spec.item, v, f"{path}[{i}]", errs, warns)
    elif isinstance(spec, Obj):
        if not isinstance(value, dict):
            errs.append(f"{path} : objet attendu ({value!r})")
            return
        for k, sub in spec.fields.items():
            if k not in value:
                if not isinstance(sub, Opt):
                    errs.append(f"{path}.{k} : champ obligatoire absent")
                continue
            check(sub, value[k], f"{path}.{k}", errs, warns)
        if not spec.extra_ok:
            for k in value:
                if k not in spec.fields:
                    warns.append(f"{path}.{k} : champ inconnu de la v1 (ignore par l'app)")


def find_floats(value, path, errs):
    if isinstance(value, float):
        errs.append(f"{path} : flottant interdit ({value!r})")
    elif isinstance(value, dict):
        for k, v in value.items():
            find_floats(v, f"{path}.{k}", errs)
    elif isinstance(value, list):
        for i, v in enumerate(value):
            find_floats(v, f"{path}[{i}]", errs)


def coherence(t, obj, errs, warns):
    """Regles qui lient plusieurs champs."""
    if t == "reponse":
        code, ok, etape = obj.get("code"), obj.get("ok"), obj.get("etape")
        if code in OK_CODES and ok is not True:
            errs.append(f"reponse : code {code} avec ok {ok}")
        if code in KO_CODES and ok is not False:
            errs.append(f"reponse : code {code} avec ok {ok}")
        if etape == "debut" and code != "en_cours":
            errs.append(f"reponse : etape debut avec code {code}")
        if etape == "fin" and "duree_ms" not in obj:
            errs.append("reponse : etape fin sans duree_ms")
        if etape == "debut" and "duree_ms" in obj:
            warns.append("reponse : etape debut avec duree_ms")
        if (code == "accepte") != (obj.get("suite") == "injection"):
            errs.append(f"reponse : code {code} et suite {obj.get('suite')} (accepte va avec suite injection)")
        if ("bail_s" in obj) != ("up_s" in obj):
            errs.append("reponse : bail_s et up_s vont ensemble")
    elif t == "config":
        c = obj.get("capture")
        if isinstance(c, dict):
            hz, sil = c.get("resol_hz"), c.get("silence_us")
            if isinstance(hz, int) and hz not in RESOLUTIONS:
                errs.append(f"config : resol_hz {hz}, attendu 1000000 ou 500000")
            elif isinstance(hz, int) and isinstance(sil, int) and not isinstance(sil, bool):
                if sil < SILENCE_MIN_US or sil * hz > SILENCE_MAX_TICKS * 1000000:
                    errs.append(f"config : silence_us {sil} hors bornes a {hz} Hz")
    elif t == "trame":
        if obj.get("dur_us") == [] and obj.get("fin") is not True and obj.get("debord") is not True:
            errs.append("trame : dur_us vide sans fin ni debord")
    elif t == "reseau" and obj.get("bloc") == "ip":
        w = obj.get("wifi")
        if isinstance(w, dict) and isinstance(w.get("connecte"), bool):
            vus = [k for k in ("rssi_dbm", "ip") if w.get(k) is not None]
            if w["connecte"] and len(vus) != 2:
                errs.append("reseau/ip : wifi connecte sans rssi_dbm ou sans ip")
            if not w["connecte"] and vus:
                errs.append(f"reseau/ip : wifi deconnecte avec {', '.join(vus)}")
    elif t == "hello" and obj.get("bloc") == "base":
        if obj.get("fw") != obj.get("fw_desc"):
            warns.append(f"hello : fw {obj.get('fw')!r} different de fw_desc {obj.get('fw_desc')!r}")


def check_line(raw):
    """raw : octets de RS (compris) a LF (exclu). Rend (objet ou None, erreurs, avertissements)."""
    errs, warns = [], []
    size = len(raw) + 1  # LF compris
    if size > LINE_MAX:
        errs.append(f"{size} octets, {LINE_MAX} au plus (RS et LF compris)")
    elif size > BUDGET:
        warns.append(f"{size} octets : au-dela du budget de pire cas de {BUDGET}")
    body = raw[1:]
    bad = [b for b in body if b < 0x20 or b > 0x7E]
    if bad:
        errs.append(f"{len(bad)} octet(s) hors ASCII imprimable (premier : 0x{bad[0]:02X})")
    text = body.decode("ascii", "replace")
    if not text.startswith('{"v":'):
        errs.append("ne commence pas par {\"v\":")
    if not text.endswith("}"):
        errs.append("ne finit pas par }")
    try:

        def hook(items):
            keys = [k for k, _ in items]
            if len(keys) != len(set(keys)):
                errs.append(f"cle en double : {keys}")
            return dict(items)

        obj = json.loads(text, object_pairs_hook=hook)
    except ValueError as e:
        errs.append(f"JSON invalide : {e}")
        return None, errs, warns
    if not isinstance(obj, dict):
        errs.append("pas un objet JSON")
        return None, errs, warns
    compact = json.dumps(obj, separators=(",", ":"), ensure_ascii=True)
    if compact != text:
        errs.append("forme non compacte (espace hors chaine, ou echappement inattendu)")
    keys = list(obj)
    if keys[:4] != ["v", "t", "n", "ms"]:
        errs.append(f"champs de tete {keys[:4]} au lieu de v, t, n, ms")
    if obj.get("v") != 1 or isinstance(obj.get("v"), bool):
        errs.append(f"v = {obj.get('v')!r}, version geree : 1")
    t = obj.get("t")
    for k in ("n", "ms"):
        v = obj.get(k)
        if isinstance(v, bool) or not isinstance(v, int) or not 0 <= v <= U32_MAX:
            errs.append(f"{k} = {v!r} : entier 0..4294967295 attendu")
    if not isinstance(t, str):
        errs.append(f"t = {t!r} : chaine attendue")
        return obj, errs, warns
    find_floats(obj, t, errs)
    bloc = obj.get("bloc")
    if t in BLOCKED:
        if len(keys) < 5 or keys[4] != "bloc":
            errs.append(f"{t} : bloc attendu en 5e champ")
        if bloc not in BLOCS[t]:
            errs.append(f"{t} : bloc {bloc!r} inconnu (attendu : {', '.join(sorted(BLOCS[t]))})")
            return obj, errs, warns
    elif "bloc" in obj:
        errs.append(f"{t} : pas de bloc pour ce type")
    schema = SCHEMAS.get((t, bloc if t in BLOCKED else None))
    if schema is None:
        warns.append(f"type inconnu de la v1 : {t!r} (ignore par l'app)")
        return obj, errs, warns
    content = {k: v for k, v in obj.items() if k not in ("v", "t", "n", "ms", "bloc")}
    check(schema, content, f"{t}" + (f"/{bloc}" if bloc else ""), errs, warns)
    coherence(t, obj, errs, warns)
    return obj, errs, warns


# ---------------------------------------------------------------------------
#  Lecture d'une capture
# ---------------------------------------------------------------------------


def split_lines(data):
    """Lignes terminees par LF, CR final retire."""
    lines = data.split(b"\n")
    tail = lines.pop()  # sans LF : ligne pas finie
    out = [l[:-1] if l.endswith(b"\r") else l for l in lines]
    return out, tail


def examples(path):
    """Lignes <RS>{...} des exemples d'un document."""
    with open(path, "rb") as f:
        data = f.read().decode("utf-8")
    out = []
    for line in data.splitlines():
        if line.startswith("<RS>{"):
            out.append(b"\x1e" + line[4:].encode("ascii"))
    return out


class _Paires(list):
    """Objet JSON lu en liste de paires (ordre et cles en double gardes)."""


def _texte(v):
    """Reecrit une valeur lue par json.loads(object_pairs_hook=_Paires) en JSON compact, telle quelle."""
    if isinstance(v, _Paires):
        return "{" + ",".join(json.dumps(k) + ":" + _texte(x) for k, x in v) + "}"
    if isinstance(v, list):
        return "[" + ",".join(_texte(x) for x in v) + "]"
    return json.dumps(v)


def jsonl_record(line):
    """Une ligne .jsonl -> (source, octets RS + 'l') ; ValueError si elle est illisible."""
    rec = json.loads(line.decode("utf-8"), object_pairs_hook=_Paires)
    if not isinstance(rec, _Paires):
        raise ValueError("pas un objet")
    champs = dict(rec)
    if len(champs) != len(rec):
        raise ValueError("cle en double dans l'enveloppe")
    rx, de, l = champs.get("rx_ms"), champs.get("de"), champs.get("l")
    if isinstance(rx, bool) or not isinstance(rx, int):
        raise ValueError("rx_ms absent ou pas un entier")
    if not isinstance(de, str):
        raise ValueError("de absent ou pas une chaine")
    if not isinstance(l, _Paires):
        raise ValueError("l absent ou pas un objet")
    return de, b"\x1e" + _texte(l).encode("utf-8")


class Report:
    def __init__(self, strict, quiet, continuity=True):
        self.strict, self.quiet, self.continuity = strict, quiet, continuity
        self.lines = self.errors = self.warnings = self.text = self.broken = self.fragments = 0
        self.by_type = {}
        self.worst = (0, None)
        self.gaps = self.lost = self.restarts = 0
        self.last_n = {}  # par source : dernier n vu
        self.after_broken = False

    def say(self, where, level, msg):
        if not self.quiet:
            print(f"{where} : {level} : {msg}")

    def error(self, where, msg):
        self.errors += 1
        self.say(where, "ERREUR", msg)

    def feed(self, where, line, source=None):
        i = line.rfind(bytes([RS]))
        if i < 0:
            if line.strip().endswith(b"}") and self.after_broken:
                self.fragments += 1
            else:
                self.text += 1
            self.after_broken = False
            return
        machine = line[i:]
        obj, errs, warns = check_line(machine)
        self.lines += 1
        size = len(machine) + 1
        if size > self.worst[0]:
            self.worst = (size, where)
        if obj is None or errs and not isinstance(obj, dict):
            self.broken += 1
        self.after_broken = obj is None
        if isinstance(obj, dict):
            key = obj.get("t"), obj.get("bloc")
            self.by_type[key] = self.by_type.get(key, 0) + 1
            n = obj.get("n")
            if self.continuity and isinstance(n, int) and not isinstance(n, bool):
                last = self.last_n.get(source)
                if last is not None:
                    if n == last + 1:
                        pass
                    elif n > last + 1:
                        self.gaps += 1
                        self.lost += n - last - 1
                        warns.append(f"n saute de {last} a {n} : {n - last - 1} ligne(s) perdue(s)")
                    else:
                        self.restarts += 1
                        self.say(where, "info", f"n recule ({last} -> {n}) : redemarrage ou lignes anciennes")
                self.last_n[source] = n
        for e in errs:
            self.error(where, e)
        for w in warns:
            if self.strict:
                self.errors += 1
                self.say(where, "ERREUR (strict)", w)
            else:
                self.warnings += 1
                self.say(where, "avertissement", w)

    def summary(self):
        total = self.lines + self.lost
        pct = 100.0 * self.lost / total if total else 0.0
        print(
            f"{self.lines} ligne(s) machine, {self.errors} erreur(s), {self.warnings} avertissement(s) ; "
            f"{self.text} ligne(s) de texte, {self.fragments} fragment(s), {self.broken} ligne(s) abimee(s) ; "
            f"n : {self.gaps} trou(s) ({self.lost} ligne(s) perdue(s), {pct:.3f} %), {self.restarts} recul(s)"
        )
        if self.worst[1]:
            print(f"plus longue ligne : {self.worst[0]} octets ({self.worst[1]})")
        if self.by_type:
            parts = [f"{t}{'/' + b if b else ''} {c}" for (t, b), c in sorted(self.by_type.items(), key=str)]
            print("par type : " + ", ".join(parts))


def main(argv=None):
    ap = argparse.ArgumentParser(description="Verifie les lignes machine de la sonde (docs/PROTOCOLE-JSON.md).")
    ap.add_argument("captures", nargs="*", help="captures brutes du port serie, ou fichiers .jsonl")
    ap.add_argument("--exemples", metavar="MD", help="verifier les exemples <RS>{...} d'un document")
    ap.add_argument("--jsonl", action="store_true", help="captures au format .jsonl (implicite pour l'extension .jsonl)")
    ap.add_argument("--strict", action="store_true", help="avertissements comptes comme erreurs")
    ap.add_argument("-q", "--quiet", action="store_true", help="resume seulement")
    ap.add_argument(
        "--independantes", action="store_true", help="lignes independantes (exemples, tests) : pas de controle de n"
    )
    args = ap.parse_args(argv)
    if not args.captures and not args.exemples:
        ap.error("donner au moins une capture, ou --exemples")
    rep = Report(args.strict, args.quiet, not args.independantes)
    try:
        if args.exemples:
            continuity, rep.continuity = rep.continuity, False  # exemples independants
            for k, line in enumerate(examples(args.exemples), 1):
                rep.feed(f"{args.exemples} exemple {k}", line)
            rep.continuity = continuity
        for path in args.captures:
            with open(path, "rb") as f:
                data = f.read()
            lines, tail = split_lines(data)
            if args.jsonl or path.endswith(".jsonl"):
                rep.last_n = {}
                for k, line in enumerate(lines + ([tail] if tail.strip() else []), 1):
                    if not line.strip():
                        continue
                    try:
                        source, machine = jsonl_record(line)
                    except ValueError as e:
                        rep.broken += 1
                        rep.error(f"{path}:{k}", f"ligne .jsonl illisible ({e})")
                        continue
                    rep.feed(f"{path}:{k}", machine, source)
                continue
            rep.last_n = {}
            rep.after_broken = True  # debut de capture : une suite de ligne est un fragment
            for k, line in enumerate(lines, 1):
                rep.feed(f"{path}:{k}", line, path)
            if tail.strip():
                rep.say(path, "info", f"derniere ligne sans LF ({len(tail)} octets) ignoree")
    except OSError as e:
        print(f"lecture impossible : {e}", file=sys.stderr)
        return 2
    rep.summary()
    return 1 if rep.errors else 0


if __name__ == "__main__":
    sys.exit(main())
