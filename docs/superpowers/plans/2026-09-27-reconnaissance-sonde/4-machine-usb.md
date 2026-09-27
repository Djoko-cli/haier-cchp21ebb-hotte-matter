# Plan de la sonde, partie 4 : machine-usb

> En-tête, contraintes globales, carte des fichiers et interfaces : [../2026-09-27-reconnaissance-sonde.md](../2026-09-27-reconnaissance-sonde.md). Les tâches s'exécutent dans l'ordre des numéros (11 à 12b).

### Tâche 11 : Mode machine sur l'USB : sessions, instantanés, événements `trame`, profil hotte du protocole

**Fichiers :**
- Créer : `tools/json_check.py` (copie très adaptée de `benq@c58a506:tools/json_check.py` : contenu complet ci-dessous)
- Créer : `tools/tests/test_json_check.py`
- Créer : `docs/PROTOCOLE-JSON.md`
- Créer : `src/json_mode.h` (réécrit d'après `benq@c58a506:src/json_mode.h`)
- Créer : `src/json_mode.cpp` (réécrit d'après `benq@c58a506:src/json_mode.cpp`)
- Modifier : `src/sonde.h` (fichier entier : `sondeRepEnCours`, `sondeRepTotal`, `sondeHorsBornes`)
- Modifier : `src/main.cpp` (fichier entier : tampon USB de 8 Ko, `jsonBegin`, `jsonTrame`, `jsonPoll`, `jsonConfigChanged`, compteur de répétitions, nombre de valeurs hors bornes lues en NVS gardé)
- Modifier : `src/cli.cpp` (fichier entier, avec sa ligne de provenance : `capture` et `seuils` scindés en cœur sans texte + affichage, commande `json`, `runLine` avec `id=`, `cliPoll` en mode machine)
- Modifier : `tools/tests/test_hote.sh` (deux lignes `json_check.py` après `test_json`)
- Tester : `tools/tests/test_json_check.py`, `sh tools/tests/test_hote.sh`, `~/.platformio/penv/bin/pio run -e sonde`

Les originaux de la ScreenBar se lisent par `git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:src/json_mode.cpp` (et `src/json_mode.h`, `src/cli.cpp` lignes 1043 à 1151 pour `runLine`, 3168 à 3199 pour `cliPoll`, `tools/json_check.py`). Les fichiers de cette tâche en gardent la mécanique (un seul producteur, jamais d'attente, file des périodiques, bail, cadence, réponses différées, `n` par transport, `json_perdus`) et retirent tout ce qui touche à la lampe, à la LED, à Matter et à l'observateur de livraison : ils sont donnés en entier.

**Interfaces :**
- Consomme :
  - `src/json_out.h` (tâche 8) : `jsonp::Writer` (`begin`, `str`, `u32`, `u64`, `boolean`, `null`, `hex`, `hexU32`, `obj`, `arr`, `end`, `finish`, `data`, `size`), `heartbeat`, `sessionEnd`, `logLine`, `trame(Writer &, uint32_t n, uint32_t ms, const capt::Partie &, bool hasRep, uint32_t rep, uint32_t sautes)`, `Reply` (`Suite::SuiteInjection`), `reply`, `Queue`, `Queued`, `Item` (`HelloBase`, `HelloId`, `Config`, `EtatBus`, `EtatCapture`, `EtatInjection`, `EtatSys`, `Compteurs`, `NetIp`, `Heartbeat`, `Reply`), `RateCap`, `Cadence`, `LineAssembler`, `parseIdPrefix`, `copyCmd`, `maskCmd`, `remoteRefusal`, `leaseExpired`, `kLateMs`, `kRev`, `kLineMax`, `kCmdMax`, `kCmdTextMax`, `kMsgMax`, `kOrigins`, `kUsb`.
  - `src/capture_rmt.h` (tâche 10) : `captureBegin`, `captureEnd`, `captureActive`, `captureStats()` → `CaptureStats{receptions, parties, blocs, symboles, debord}`.
  - `src/bord.h` (tâche 10) : `bordDernierUs()`, `bordBusHaut()`, `bordFronts()`.
  - `src/sonde.h` (tâche 10) : `ReglagesSonde &sondeReglages()`, `bool sondeAppliquer(const ReglagesSonde &)`.
  - `src/capture_model.h` (tâche 9) : `capt::Partie`, `capt::Changements::aEmettre`, `prendreRep`, `repEnCours`, `capt::filtreValide`, `resolValide`, `silenceValide`.
  - `src/config.h` (tâche 7) : `kPinEcoute`, `kTramesParSeconde`, `kFiltreMaxUs`, `kSilenceMinUs`, `kSilenceMaxTicks` ; `src/fw_version.h` : `FW_VERSION_FULL`, `FW_ENV`.
  - `tools/tests/test_json.cpp` (tâche 8) : `test_hote.sh` le lance avec `"$OUT/test_json_lignes.txt"`, où il écrit ses 16 lignes réalistes.
- Produit :
  - `src/json_mode.h` : l'interface du contrat, **sauf** ce qui vient plus tard : `jsonInjection` (tâche 23), `jsonRemoteReset`, `jsonNoteRemoteRx`, `jsonRemoteAdmit` (tâche 19). Présents dès maintenant : `void jsonBegin(); void jsonPoll(); uint32_t jsonBootId(); bool jsonMachine(); void jsonNoteRx(); struct JsonCmd {bool hasId; uint32_t id; const char *cmd; uint32_t t0;}; void jsonCommand(char *arg, const JsonCmd &c); bool jsonCadenceOk(uint32_t now); void jsonRefuse(const JsonCmd &c, const char *code, const char *msg); void jsonReply(const jsonp::Reply &r); void jsonReplyEnd(const jsonp::Reply &r); void jsonAfterCommand(); void jsonConfigChanged(); void jsonTrame(const capt::Partie &p, bool hasRep, uint32_t rep); bool jsonLog(const char *src, const char *niv, const char *txt); void jsonSetOrigin(uint8_t origin); uint8_t jsonOrigin(); void jsonCountRejected();`.
  - `src/json_mode.cpp`, points d'accroche pour les tâches 19 et 23 : `static Sink sSinks[kOrigins]` (une session par origine, `kUsb` seule servie ici) ; `static int room(uint8_t o)` et `static bool emit(uint8_t o)` (branche réseau à écrire en tâche 19) ; `static void pushNet(uint8_t, uint32_t, bool)` (vide : la tâche 19 y pousse `Item::NetIp`) ; `static void pushState(...)` (la tâche 23 y ajoute `Item::EtatInjection`) ; le `default:` de `produce()` (blocs absents) ; `helloIdentity()` (`caps` : la tâche 19 ajoute `udp`, `cle`, `mdns`) ; `config()` (la tâche 23 ajoute l'objet `injection`) ; la branche `cle` de `jsonCommand` (réponse `refuse` jusqu'à la tâche 19) ; `enterMachine()` applique déjà le profil distant (`kRemotePeriod` 2 000, `kRemoteCounters` 5 000, `kRemoteNet` 30 000, trames sans coupure) ; `static void lose(Sink &k, uint32_t count = 1)` compte toute perte (session et `compteurs.sonde.lignes_perdues`).
  - `src/sonde.h` : `uint32_t sondeRepEnCours(); uint32_t sondeRepTotal(); uint8_t sondeHorsBornes();` (définies dans `main.cpp`).
  - `src/cli.cpp` : `struct Resultat { bool ok; const char *code; const char *msg; };`, `static Resultat faireCapture(const char *args)`, `static Resultat faireSeuils(char *args)` (la tâche 23 fait de même pour `injection` et `injecte`), `static bool firstWordIs(const char *s, const char *w)`, `static char *afterWord(char *s)`, `static void runLine(char *line, bool tooLong)` (la tâche 19 y insère `jsonRemoteAdmit` juste après le calcul de `c`, et `cliRunRemote` l'appelle), ligne `{"json", cmdJson, ...}` ajoutée en fin de `kCommandes`.
  - `tools/json_check.py` : `SCHEMAS` (clé `(t, bloc)`), `coherence(t, obj, errs, warns)`, `check_line(raw)`, `examples(path)`, `jsonl_record(line)`, `class Report`, `main(argv=None)`. Les tâches 19 et 23 ajoutent `("reseau", "ip")`, `("etat", "injection")` et l'objet `injection` de `config` à `SCHEMAS`.
  - `docs/PROTOCOLE-JSON.md` : profil hotte par différence ; ses exemples `<RS>{...}` sont vérifiés par `test_hote.sh`.

**Décisions de cette tâche (à connaître pour les suivantes) :**
- **Tampon d'émission de l'USB : 8 Ko.** `HWCDC` n'en a que 256 octets par défaut (`HWCDC.cpp`, `begin()`), moins qu'une ligne machine : sans `Serial.setTxBufferSize(8192)` avant `Serial.begin()`, aucune ligne de plus de 256 octets ne partirait jamais. La ScreenBar prenait 4 Ko ; ici, une dizaine de lignes `trame` doivent tenir pendant que le Mac lit. Le délai d'écriture de `HWCDC` reste celui du cœur (100 ms).
- **Blocs `etat`** : `bus`, `capture`, `sys`, chacun avec `boot` et `up_s` en tête (battement, comme les blocs `etat` de la ScreenBar). Le bloc `sys` porte un objet `sys` identique à celui de la ScreenBar. `repos` est lu à la production de la ligne s'il n'y a eu aucun front depuis le silence de fin (`silence_us`), sinon la dernière valeur lue est reprise ; null tant qu'il n'a jamais été lu.
- **`compteurs` bloc `sonde`** : `receptions`, `parties`, `blocs`, `symboles`, `debord` viennent de `captureStats()` ; `rep` de `sondeRepTotal()` ; `lignes_perdues` (toutes les lignes machine perdues), `sautes` (lignes `trame` non produites, plafond) et `rejets` (lignes de l'hôte refusées) sont comptés **toutes sessions confondues, depuis le démarrage** : la spec ne les définit pas, et les mêmes grandeurs par session sont déjà dans `etat.sys`.
- **Plafond des `trame`** (100 par seconde et par session, `jsonp::RateCap`) : jugé à la première partie vue de chaque réception ; les parties suivantes de la même réception suivent sa décision (§8.5 : « produites ou sautées ensemble »). La première ligne produite après un saut porte `sautes`.
- **`capture` et `seuils <valeurs>` avec un `id`** répondent sans texte (`ok`, `usage` avec `msg`, `refuse`), comme l'exige le §8.3 pour les commandes à distance ; le cœur de chacune (`faireCapture`, `faireSeuils`) sert aussi à la console humaine. `config` est réémise après chaque `capture tout|changements` ou `seuils <valeurs>` accepté (`sondeAppliquer` appelle `jsonConfigChanged`) ; `capture on|off` ne change pas la `config` (état dans `etat.capture.active`).
- **Hors mode machine seulement**, `surPartie` affiche les trames en texte ; en mode machine, elles ne partent qu'en lignes `trame`.
- **`caps`** : `["sonde","injection","trames","log"]` comme le contrat, `injection` compris avant la tâche 23. **`log`** : `src` vaut `sonde`, `capture`, `injection` ou `reseau` (enumération nouvelle, vérifiée par `json_check.py`).
- **Valeurs hors bornes lues en NVS** (spec §8.2 : « un `log` le signale ») : `main.cpp` garde le nombre rendu par `reglagesCharger` (`sondeHorsBornes()`). Le texte `[reglages] ...` du démarrage part en général avant que le Mac n'ouvre le port ; aussi, **à chaque ouverture de session** (`json 1`), si ce nombre N n'est pas nul, un `log` `sonde` `notice` « `N valeur(s) hors bornes en NVS : valeurs par defaut` » suit l'instantané et sa réponse, **même sans `json log 1`** (hors plafond des `log`). Le firmware n'est pas testable sur l'hôte : `test_json_check.py` vérifie l'exemple du protocole (§8.3), que `test_hote.sh` passe aussi à `json_check.py`.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_json_check.py` :

```python
#!/usr/bin/env python3
"""Tests de tools/json_check.py (profil hotte) : python3 -m unittest discover -s tools/tests -p test_json_check.py -v"""
import contextlib
import copy
import io
import json
import os
import sys
import tempfile
import unittest

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ICI, ".."))
import json_check  # noqa: E402

PROTOCOLE = os.path.join(ICI, "..", "..", "docs", "PROTOCOLE-JSON.md")


def tete(t, n=1, ms=10, bloc=None):
    d = {"v": 1, "t": t, "n": n, "ms": ms}
    if bloc:
        d["bloc"] = bloc
    return d


def msg(t, bloc=None, **champs):
    d = tete(t, bloc=bloc)
    d.update(champs)
    return d


# Un message valide de chaque (t, bloc) du profil hotte, dans l'ordre des champs du firmware.
VALIDES = {
    ("hello", "base"): msg(
        "hello", "base", rev=4, fw="0.1.0-1a2b3c4", fw_desc="0.1.0-1a2b3c4", date="Sep 27 2026", heure="14:02:11",
        env="sonde", build="sonde", reseau_build="aucun", puce="esp32c6", idf="v5.5.5", arduino="3.3.12",
        boot="3FA2C901", reset="mise_sous_tension", reset_n=1, up_s=12,
        session={"transport": "usb", "periode_ms": 1000, "compteurs_ms": 1000, "reseau_ms": 5000, "bail_s": 0,
                 "trames": True, "log": False},
        limites={"ligne_max": 1024, "cmd_max": 127}),
    ("hello", "identite"): msg(
        "hello", "identite", boot="3FA2C901", mac="F0F5BD012345",
        id={"fabricant": "Djoko-CLI", "produit": "Sonde hotte Haier", "serie": "HOTTE-F0F5BD012345",
            "nom": "Sonde hotte", "hw": 1, "hw_txt": "C6 SuperMini, etages v1"},
        appareil="hotte", caps=["sonde", "injection", "trames", "log"]),
    ("config", None): msg(
        "config", capture={"gpio": 6, "resol_hz": 1000000, "filtre_us": 1, "silence_us": 5000, "mode": "tout",
                           "inverse": True}),
    ("etat", "bus"): msg("etat", "bus", boot="3FA2C901", up_s=12, repos="haut", fronts=1520, derniere_ms=3,
                         receptions_s=17),
    ("etat", "capture"): msg("etat", "capture", boot="3FA2C901", up_s=12, active=True, mode="tout", debord=0,
                             rep_en_cours=0),
    ("etat", "sys"): msg(
        "etat", "sys", boot="3FA2C901", up_s=12,
        sys={"heap": 250000, "heap_min": 240000, "heap_bloc": 110000, "pile_boucle": 5200, "boucle_max_ms": 2,
             "json_perdus": 0, "json_trop_longs": 0, "rejets": 0}),
    ("compteurs", "sonde"): msg("compteurs", "sonde", receptions=203, parties=203, blocs=203, symboles=3654,
                                debord=0, rep=0, lignes_perdues=0, sautes=0, rejets=0),
    ("hb", None): msg("hb", boot="3FA2C901", up_s=12, json_perdus=0),
    ("fin", None): msg("fin", cause="commande"),
    ("reponse", None): msg("reponse", id=1, etape="fin", cmd="json 1 bail 0", ok=True, code="ok", duree_ms=9,
                           bail_s=0, up_s=12),
    ("trame", None): msg("trame", num=12, part=0, fin=True, t_us=81234567, niv0="bas", dur_us=[1500, 750, 750, 2250],
                         debord=False),
    ("injection", None): msg("injection", id=42, cmd="injecte durees 750 750", resultat="ok", niv0="bas",
                             dur_us=[750, 750], attente_us=20412, relu_us=[752, 748]),
    ("log", None): msg("log", src="capture", niv="notice", txt="[capture] echec du RMT"),
}


def brut(obj):
    return b"\x1e" + json.dumps(obj, separators=(",", ":")).encode("ascii")


def verifier(obj):
    _, errs, warns = json_check.check_line(brut(obj))
    return errs, warns


def lancer(*argv):
    sortie = io.StringIO()
    with contextlib.redirect_stdout(sortie):
        code = json_check.main(list(argv))
    return code, sortie.getvalue()


class Schemas(unittest.TestCase):
    def test_messages_valides(self):
        self.assertEqual(set(VALIDES), set(json_check.SCHEMAS))
        for cle, obj in VALIDES.items():
            with self.subTest(cle=cle):
                self.assertEqual(verifier(obj), ([], []))

    def modifie(self, cle, **champs):
        obj = copy.deepcopy(VALIDES[cle])
        for k, v in champs.items():
            if v is None and k.startswith("sans_"):
                del obj[k[5:]]
            else:
                obj[k] = v
        return verifier(obj)

    def test_champ_obligatoire_absent(self):
        errs, _ = self.modifie(("etat", "bus"), sans_fronts=None)
        self.assertTrue(any("fronts : champ obligatoire absent" in e for e in errs), errs)

    def test_champ_inconnu_avertit(self):
        errs, warns = self.modifie(("compteurs", "sonde"), nouveau=1)
        self.assertEqual(errs, [])
        self.assertTrue(any("nouveau : champ inconnu" in w for w in warns), warns)

    def test_ordre_de_tete(self):
        obj = {"t": "hb", "v": 1, "n": 1, "ms": 1, "boot": "3FA2C901", "up_s": 1, "json_perdus": 0}
        errs, _ = verifier(obj)
        self.assertTrue(any("champs de tete" in e for e in errs), errs)

    def test_bloc_inconnu(self):
        errs, _ = verifier(msg("etat", "lampe", boot="3FA2C901", up_s=1))
        self.assertTrue(any("bloc 'lampe' inconnu" in e for e in errs), errs)

    def test_trame_bornes(self):
        cle = ("trame", None)
        self.assertTrue(self.modifie(cle, dur_us=[1] * 111)[0])
        self.assertEqual(self.modifie(cle, dur_us=[1] * 110)[0], [])
        self.assertTrue(self.modifie(cle, dur_us=[0, 5])[0])
        self.assertTrue(self.modifie(cle, dur_us=[1.5])[0])
        self.assertTrue(self.modifie(cle, t_us=2**53)[0])
        self.assertEqual(self.modifie(cle, t_us=2**53 - 1)[0], [])
        self.assertTrue(self.modifie(cle, niv0="HAUT")[0])
        # Partie vide : seulement pour clore une reception ou signaler une perte.
        self.assertTrue(self.modifie(cle, dur_us=[], fin=False)[0])
        self.assertEqual(self.modifie(cle, dur_us=[], fin=True)[0], [])
        self.assertEqual(self.modifie(cle, dur_us=[], fin=False, debord=True)[0], [])
        self.assertEqual(self.modifie(cle, rep=0, sautes=3), ([], []))

    def test_reponse_coherence(self):
        cle = ("reponse", None)
        self.assertEqual(self.modifie(cle, code="accepte", suite="injection")[0], [])
        self.assertTrue(self.modifie(cle, code="accepte")[0])
        self.assertTrue(self.modifie(cle, suite="injection")[0])
        self.assertTrue(self.modifie(cle, code="usage")[0])            # ok true avec un code d'echec
        self.assertTrue(self.modifie(cle, etape="debut")[0])            # debut sans en_cours
        self.assertTrue(self.modifie(cle, sans_duree_ms=None)[0])       # fin sans duree_ms
        self.assertTrue(self.modifie(cle, sans_up_s=None)[0])           # bail_s sans up_s
        self.assertTrue(self.modifie(cle, code="differe")[0])           # code de la ScreenBar
        self.assertTrue(self.modifie(cle, cmd="x" * 41)[0])

    def test_config_coherence(self):
        cle = ("config", None)
        capture = VALIDES[cle]["capture"]
        self.assertTrue(self.modifie(cle, capture=dict(capture, resol_hz=700000))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, silence_us=999))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, silence_us=32768))[0])
        self.assertEqual(self.modifie(cle, capture=dict(capture, silence_us=32767))[0], [])
        self.assertEqual(self.modifie(cle, capture=dict(capture, resol_hz=500000, silence_us=65534))[0], [])
        self.assertTrue(self.modifie(cle, capture=dict(capture, resol_hz=500000, silence_us=65536))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, filtre_us=4))[0])
        self.assertTrue(self.modifie(cle, capture=dict(capture, mode="rafale"))[0])

    def test_hello(self):
        ident = ("hello", "identite")
        self.assertTrue(self.modifie(ident, appareil="screenbar")[0])
        self.assertTrue(self.modifie(ident, caps=["sonde", "matter"])[0])
        idv = dict(VALIDES[ident]["id"], serie="HALO1-F0F5BD012345")
        self.assertTrue(self.modifie(ident, id=idv)[0])
        base = ("hello", "base")
        self.assertTrue(self.modifie(base, build="produit")[0])
        errs, warns = self.modifie(base, fw_desc="0.0.9")
        self.assertEqual(errs, [])
        self.assertTrue(any("different de fw_desc" in w for w in warns), warns)

    def test_log(self):
        self.assertTrue(self.modifie(("log", None), src="lampe")[0])
        self.assertTrue(self.modifie(("log", None), txt="x" * 192)[0])

    def test_trop_long_et_non_compact(self):
        obj = copy.deepcopy(VALIDES[("log", None)])
        obj["txt"] = "x" * 191
        obj["sautes"] = 1
        self.assertEqual(verifier(obj)[0], [])
        _, errs, _ = json_check.check_line(b"\x1e" + json.dumps(VALIDES[("fin", None)]).encode())
        self.assertTrue(any("non compacte" in e for e in errs), errs)


class Fichiers(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def ecrire(self, nom, contenu: bytes):
        p = os.path.join(self.dossier.name, nom)
        with open(p, "wb") as f:
            f.write(contenu)
        return p

    def test_capture_brute_trous(self):
        lignes = []
        for n in (1, 2, 5):  # 3 et 4 manquent
            o = copy.deepcopy(VALIDES[("hb", None)])
            o["n"] = n
            lignes.append(b"texte avant " + brut(o) + b"\r\n")
        p = self.ecrire("capture.log", b"> json 1\r\n" + b"".join(lignes))
        code, out = lancer("-q", p)
        self.assertEqual(code, 0, out)  # un trou est un avertissement
        self.assertIn("n : 1 trou(s) (2 ligne(s) perdue(s), 40.000 %)", out)
        code, out = lancer("-q", "--strict", p)
        self.assertEqual(code, 1, out)

    def test_jsonl_par_source(self):
        recs = []
        for n, de in ((7, "usb"), (100, "192.168.1.20"), (8, "usb"), (101, "192.168.1.20")):
            o = copy.deepcopy(VALIDES[("trame", None)])
            o["n"] = n
            recs.append('{"rx_ms":%d,"de":"%s","l":%s}' % (1700000000000 + n, de, json.dumps(o, separators=(",", ":"))))
        p = self.ecrire("s.jsonl", ("\n".join(recs) + "\n").encode())
        code, out = lancer("--strict", p)
        self.assertEqual(code, 0, out)
        self.assertIn("4 ligne(s) machine, 0 erreur(s)", out)
        self.assertIn("n : 0 trou(s)", out)
        # Meme contenu sous un autre nom : --jsonl l'impose.
        q = self.ecrire("s.txt", ("\n".join(recs) + "\n").encode())
        self.assertEqual(lancer("--strict", "--jsonl", q)[0], 0)
        self.assertEqual(lancer("--strict", q)[0], 0)  # lu comme brut : aucune ligne machine, aucune erreur
        self.assertIn("0 ligne(s) machine", lancer("--strict", q)[1])

    def test_jsonl_illisible_et_cle_double(self):
        bon = '{"rx_ms":1,"de":"usb","l":%s}' % json.dumps(VALIDES[("fin", None)], separators=(",", ":"))
        sans_l = '{"rx_ms":2,"de":"usb"}'
        double = '{"rx_ms":3,"de":"usb","l":{"v":1,"t":"fin","n":2,"ms":1,"cause":"bail","cause":"bail"}}'
        p = self.ecrire("x.jsonl", "\n".join([bon, sans_l, "pas du json", double]).encode() + b"\n")
        code, out = lancer(p)
        self.assertEqual(code, 1)
        self.assertIn("l absent", out)
        self.assertIn("ligne .jsonl illisible", out)
        self.assertIn("cle en double", out)

    def test_exemples_du_protocole(self):
        exemples = json_check.examples(PROTOCOLE)
        self.assertGreaterEqual(len(exemples), 15)
        code, out = lancer("--strict", "--exemples", PROTOCOLE)
        self.assertEqual(code, 0, out)
        types = {json.loads(e[1:])["t"] for e in exemples}
        self.assertTrue({"hello", "config", "etat", "compteurs", "reponse", "trame", "log", "hb", "fin"} <= types, types)

    def test_exemple_valeurs_hors_bornes(self):
        # Annonce de json_mode.cpp (logNvs) a chaque 'json 1' : spec 8.2.
        logs = [json.loads(e[1:]) for e in json_check.examples(PROTOCOLE)]
        nvs = [o for o in logs if o["t"] == "log" and "hors bornes en NVS" in o["txt"]]
        self.assertEqual(len(nvs), 1, nvs)
        self.assertEqual((nvs[0]["src"], nvs[0]["niv"]), ("sonde", "notice"))
        self.assertRegex(nvs[0]["txt"], r"^[1-9][0-9]* valeur\(s\) hors bornes en NVS : valeurs par defaut$")


if __name__ == "__main__":
    unittest.main()
```

Dans `tools/tests/test_hote.sh`, après la ligne existante

```sh
"$OUT/test_json" "$OUT/test_json_lignes.txt"
```

insérer :

```sh
# Lignes realistes de test_json et exemples du protocole : conformes au profil hotte.
python3 tools/json_check.py --strict --independantes -q "$OUT/test_json_lignes.txt"
python3 tools/json_check.py --strict -q --exemples docs/PROTOCOLE-JSON.md
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_json_check.py -v`
Attendu : ÉCHEC avec « ModuleNotFoundError: No module named 'json_check' »

Lancer : `sh tools/tests/test_hote.sh`
Attendu : ÉCHEC après « test_json : 243 verifications, 0 echecs », avec « python3: can't open file '.../tools/json_check.py': [Errno 2] No such file or directory » (et pas de « tests hote : OK »)

- [ ] **Étape 3 : Écrire `tools/json_check.py`**

Le vérificateur d'abord, seul : le document du protocole vient à l'étape 5, et ses exemples passeront par ce vérificateur.

Créer `tools/json_check.py` (en-tête de provenance compris) :

```python
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
```

- [ ] **Étape 4 : Vérifier `json_check.py` seul**

Lancer : `python3 -m unittest discover -s tools/tests -p test_json_check.py -v`
Attendu : « Ran 16 tests », puis « FAILED (errors=2) » : seuls `test_exemples_du_protocole` et `test_exemple_valeurs_hors_bornes` échouent, sur « FileNotFoundError: [Errno 2] No such file or directory: '.../docs/PROTOCOLE-JSON.md' » ; les 14 autres sont à `ok`.

Lancer : `sh tools/tests/test_hote.sh`
Attendu (fin de la sortie) : les 16 lignes de `test_json` passent, le document manque encore :

```text
16 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
plus longue ligne : 218 octets (.../hotte-tests/test_json_lignes.txt:15)
par type : fin 1, hb 1, injection 3, log 1, reponse 7, trame 3
lecture impossible : [Errno 2] No such file or directory: 'docs/PROTOCOLE-JSON.md'
```
(et pas de « tests hote : OK »)

- [ ] **Étape 5 : Écrire `docs/PROTOCOLE-JSON.md`**

Le profil hotte par différence avec la ScreenBar ; chaque exemple `<RS>{...}` de la section 8 passera par `json_check.py --strict --exemples`.

Créer `docs/PROTOCOLE-JSON.md` :

````markdown
# Protocole compagnon v1 : profil « hotte »

Ce document décrit le protocole machine de la **sonde de la ligne `D`**
(ESP32-C6, env `sonde`) **par différence** avec celui de la ScreenBar. La
référence est le protocole de `benq-screenbar-halo-matter` au commit
**`c58a506`** :
[docs/PROTOCOLE-JSON.fr.md](https://github.com/Djoko-cli/benq-screenbar-halo-matter/blob/c58a506/docs/PROTOCOLE-JSON.fr.md)
(noté « ScreenBar » ci-dessous, avec le numéro de section). Tout ce que ce
document ne dit pas est **identique** à la ScreenBar.

- Code : `src/json_out.*` (briques pures, testées sur l'hôte),
  `src/json_mode.*` (sessions, instantanés, événements), `src/cli.cpp`
  (lignes de l'hôte).
- Vérification : `tools/json_check.py` contrôle toute ligne machine contre ce
  profil (capture série brute, ou `.jsonl` avec `--jsonl`). Les exemples de la
  section 8 sont vérifiés par
  `python3 tools/json_check.py --strict --exemples docs/PROTOCOLE-JSON.md`,
  lancé par `sh tools/tests/test_hote.sh`.
- État : transport USB. Le transport réseau (UDP sur le Wi-Fi, ScreenBar §10)
  et l'injection viennent ensuite ; ce document s'étendra avec eux.

## 1. Ce qui ne change pas

| ScreenBar | Ici |
|---|---|
| §2 Tramage : RS + JSON compact ASCII + LF, 1 024 octets au plus, budget de pire cas de 896, découpage par le dernier RS, sens app → carte | identique, sauf le tampon d'émission de l'USB : **8 Ko** au lieu de 4 (section 2) |
| §3 Session : ouverture du port, **DTR = RTS = 0 en un seul appel** (§3.2), séquence de connexion, bail et `json ping`, battement, aucune persistance, écho et invite | identique ; la commande `json` a ses bornes en section 3 |
| §4 Enveloppe : `v`, `t`, `n`, `ms`, puis `bloc` ; entiers seulement | identique, plus `t_us` sur 64 bits (section 2) |
| §6 Commandes : préfixe `id=<n>`, message `reponse`, cadence de 20 lignes par seconde, une commande en vol | identique ; ce qui a une réponse sans texte est en section 5 |
| §9 Versionnage : `v` majeure, ajouts sans changer `v`, l'app ignore l'inconnu | identique ; `rev` = 4 |
| §5, §7, §8, §11, §12 : messages et exemples de la lampe | remplacés par les sections 4 à 8 |

## 2. Écarts généraux

- **`appareil`** : nouveau champ de `hello` `identite`, `"hotte"` ici. Une app
  multi-appareils choisit sa vue d'après lui ; absent, il vaut `"screenbar"`.
- **`t_us` est un entier sur 64 bits** (`esp_timer_get_time()`, en µs depuis le
  démarrage). Il déroge à la convention « uint32 » de la ScreenBar (§4) ; il
  reste sûr en JSON jusqu'à 2^53 − 1 (285 ans). `Writer::u64` l'écrit ;
  `json_check.py` l'accepte jusqu'à 2^53 − 1.
- **Tampon d'émission de l'USB : 8 Ko** (`Serial.setTxBufferSize(8192)`), pour
  qu'une dizaine de lignes `trame` tiennent pendant que le Mac lit. Les règles
  de la ScreenBar §2.3 sont inchangées : une ligne qui ne tient pas est perdue
  et comptée, jamais écrite à moitié ; une ligne périodique ne part que s'il
  reste ensuite 1 024 octets libres.
- **`rev` = 4** : la révision de la ScreenBar au commit `c58a506`, dont le
  profil hotte part. `build` = `sonde`, `reseau_build` = `aucun`.
- **Codes de `reponse`** : `ok`, `accepte`, `en_cours`, `execute` (avec `ok`
  vrai) ; `usage`, `refuse`, `inconnue`, `trop_long`, `cadence`, `interdite`,
  `deja_traite` (avec `ok` faux). Les codes de la lampe (`differe`,
  `radio_absente`, `radio_perdue`) n'existent pas ici. `suite` ne prend
  qu'une valeur, `injection`, avec le code `accepte`.

## 3. Session : la commande `json`

Comme la ScreenBar §3.4, avec ces bornes :

| Commande | Effet | Bornes (USB) |
|---|---|---|
| `json` | état de la session, en texte | |
| `json 1 [bail <s>]` | mode machine ; `hello`, `config` et l'instantané complet par la file des périodiques, puis la `reponse` | bail 0 (aucun) ou 10..600 s, défaut 30 |
| `json 0` | retour au mode humain : `fin`, puis l'invite | |
| `json etat` | instantané : `etat` (3 blocs) et `compteurs` | |
| `json hello` | `hello` (2 blocs) et `config` | |
| `json ping` | renouvelle le bail ; la `reponse` porte `bail_s` et `up_s` | |
| `json periode <ms>` | période des `etat` | 0 ou 200..60000, défaut **1 000** |
| `json compteurs <ms>` | période des `compteurs` | 0 ou 200..60000, défaut **1 000** |
| `json reseau <ms>` | période des `reseau` (aucun bloc tant que le transport réseau manque) | 0 ou 1000..60000, défaut **5 000** |
| `json trames 0\|1` | événements `trame` | défaut 1 |
| `json log 0\|1` | annonces du firmware en messages `log` | défaut 0 |
| `json cle ...` | clé du transport réseau | `refuse` tant que le transport réseau manque |

`json trames 0` coupe l'envoi des `trame` à cette session ; `capture off`
arrête la capture elle-même. Les trames ne se coupent jamais seules (écart à
la ScreenBar §10.5 : elles sont la raison d'être de la sonde).

Le bail compte comme à la ScreenBar (§3.5). Pour un enregistrement long par
l'USB, `tools/serie_enregistre.py` ouvre la session par `json 1 bail 0` et la
ferme par `json 0`.

## 4. Messages périodiques et de session

Les blocs `etat` portent `boot` et `up_s` en tête, comme à la ScreenBar : ils
servent de battement (`hb` toutes les 2 s si `periode_ms` vaut 0 ou plus de
2 000) et révèlent un redémarrage (§3.6).

### 4.1 `hello`

**Bloc `base`** : **tous** les champs de la ScreenBar §5.1, dans le même ordre.
Valeurs propres : `build` = `sonde`, `reseau_build` = `aucun`, `env` =
`sonde`, `session.transport` = `usb`. `fw` doit égaler `fw_desc`.

**Bloc `identite`** : **tous** les champs de la ScreenBar, puis `appareil`,
puis `caps`.

| Champ | Valeur |
|---|---|
| `mac` | MAC-48 d'usine, 12 hexa |
| `id.fabricant` | `Djoko-CLI` |
| `id.produit` | `Sonde hotte Haier` |
| `id.serie` | `HOTTE-` + MAC en 12 hexa majuscules |
| `id.nom` | `Sonde hotte` |
| `id.hw`, `id.hw_txt` | `1`, `C6 SuperMini, etages v1` |
| `appareil` | `hotte` |
| `caps` | `sonde` (capture de la ligne `D`), `injection` (présente dans ce build), `trames`, `log` ; puis `udp`, `cle`, `mdns` avec le transport réseau |

### 4.2 `config`

Émise avec `hello`, et de nouveau après chaque `capture tout`,
`capture changements` ou `seuils <valeurs>` accepté, même si les valeurs ne
changent pas (`sondeAppliquer`). `capture on` et `capture off` ne changent pas
la `config` : l'état se lit dans `etat.capture.active`.

| Champ | Type | Sens |
|---|---|---|
| `capture.gpio` | entier | broche d'écoute (6) |
| `capture.resol_hz` | entier | 1 000 000 ou 500 000 |
| `capture.filtre_us` | entier 0..3 | filtre anti-parasites du RMT |
| `capture.silence_us` | entier | silence de fin de réception : 1 000 µs à 32 767 ticks |
| `capture.mode` | `tout` ou `changements` | mode d'émission (spec §8.2) |
| `capture.inverse` | booléen | étage d'écoute inverseur : les niveaux publiés sont ceux **du bus** |

### 4.3 `etat`

Période `periode_ms` (1 000 ms sur l'USB). Trois blocs.

**Bloc `bus`** :

| Champ | Type | Sens |
|---|---|---|
| `repos` | `haut`, `bas` ou null | niveau du bus lu hors réception (aucun front depuis le silence de fin) ; null s'il n'a jamais été lu |
| `fronts` | entier | fronts de la ligne vus depuis le démarrage (interruption GPIO) |
| `derniere_ms` | entier ou null | âge du dernier front ; null avant le premier |
| `receptions_s` | entier | réceptions pendant la dernière seconde complète |

**Bloc `capture`** :

| Champ | Type | Sens |
|---|---|---|
| `active` | booléen | canal RMT en réception (`capture on`, `capture off`) |
| `mode` | `tout` ou `changements` | |
| `debord` | entier | blocs perdus depuis le démarrage, tampon circulaire plein |
| `rep_en_cours` | entier | mode `changements` : réceptions identiques depuis la dernière émise |

**Bloc `sys`** : un objet `sys` **identique** à celui de la ScreenBar (§5.3,
bloc `sante`) : `heap`, `heap_min`, `heap_bloc`, `pile_boucle`,
`boucle_max_ms`, `json_perdus`, `json_trop_longs`, `rejets`, comptés pour la
session qui reçoit la ligne.

### 4.4 `compteurs`

Période `compteurs_ms` (1 000 ms sur l'USB). Un bloc, `sonde`, de compteurs
cumulés depuis le démarrage (l'app trace des différences) :

| Champ | Sens |
|---|---|
| `receptions` | réceptions commencées (numéro `num` de la dernière) |
| `parties` | parties produites par le découpeur (lignes `trame` possibles) |
| `blocs`, `symboles` | blocs et symboles rendus par le pilote RMT, perdus compris |
| `debord` | blocs perdus, tampon circulaire plein |
| `rep` | mode `changements` : réceptions identiques non émises |
| `lignes_perdues` | lignes machine perdues, toutes sessions (tampon d'émission plein, retard, file pleine) |
| `sautes` | lignes `trame` non produites, toutes sessions (plafond de 100 par seconde) |
| `rejets` | lignes de l'hôte refusées, toutes origines (`trop_long`, `cadence`, `interdite`) |

Critère 4 du banc (spec §10) : `lignes_perdues`, `debord` et `sautes` à 0.

### 4.5 `hb` et `fin`

Identiques à la ScreenBar §5.6.

## 5. Commandes de l'app

Même forme que la ScreenBar §6 : texte de la console préfixé par `id=<n> `.

| Commande avec `id` | Déroulement | Messages |
|---|---|---|
| famille `json` | comme la ScreenBar | lignes produites, puis `reponse` `fin` |
| `capture on\|off\|tout\|changements`, `seuils <filtre_us> [silence_us] [resol_hz]` | **sans texte** : la réponse porte le résultat | `reponse` `fin` : `ok` (avec un `msg` si le mode n'a pas pu être écrit en NVS), `usage` (mot inconnu ou valeur hors bornes, avec `msg`) ou `refuse` (RMT ou NVS en échec) ; puis `config` après `tout`, `changements` et `seuils` |
| toute autre commande (`info`, `bus`, `stats`, `help`, `seuils` seul...) | inchangée : texte humain | `reponse` `debut`, le texte, puis `reponse` `fin` `execute` (ou `inconnue`) |

Sans `id`, rien ne change : texte humain, aucune `reponse`. `seuils` vérifie
toutes les valeurs **avant** d'écrire en NVS (spec §8.2).

## 6. Événements

Plafonds par type et par session (au-delà, l'événement n'est pas produit pour
cette session, ne consomme pas de `n`, et le suivant du même type porte
`sautes`) :

| Type | Plafond | Condition |
|---|---|---|
| `trame` | 100 par seconde | `json trames 1` (défaut) |
| `log` | 20 par seconde | `json log 1` |
| `injection` | aucun | toujours |

### 6.1 `trame` : une partie de réception

La sonde ne découpe pas en trames : elle publie des **parties** horodatées de
durées consécutives. Une réception va du premier front jusqu'au silence de
fin ; au-delà de 110 durées, elle part en plusieurs parties au fil de l'eau.
Le découpage en trames se fait sur le Mac (`tools/analyse.py`).

| Champ | Type | Sens |
|---|---|---|
| `num` | entier | numéro de réception depuis le démarrage (à partir de 1) |
| `part` | entier | rang de la partie dans la réception (à partir de 0) |
| `fin` | booléen | dernière partie de la réception |
| `t_us` | entier 64 bits | début de la partie (µs depuis le démarrage) |
| `niv0` | `haut` ou `bas` | niveau **du bus** pendant `dur_us[0]` ; les niveaux alternent ensuite |
| `dur_us` | tableau d'entiers ≥ 1 | 110 durées au plus, en µs ; vide seulement pour clore une réception (`fin`) ou signaler une perte (`debord`) |
| `debord` | booléen | des symboles ont été perdus juste avant cette partie (tampon plein) |
| `rep` | entier, facultatif | mode `changements` : réceptions identiques non émises avant celle-ci |
| `sautes` | entier, facultatif | lignes `trame` non produites (plafond) depuis la précédente produite pour cette session |

Règles :
- **Les parties d'une réception sont produites ou sautées ensemble** : le
  plafond se juge à la première partie vue de chaque réception.
- `t_us` : heure de fin du bloc RMT, moins la somme de ses durées, moins le
  silence si c'est la dernière partie (spec §8.2). La partie suivante d'une
  même réception commence à la fin de la précédente.
- Pire cas : 860 octets (110 durées de 65 534 µs, compteurs au maximum), sous
  le budget de 896 (test hôte `test_json`).

### 6.2 `injection` : fin d'une injection

Les commandes `injection` et `injecte` (spec §8.3 et §8.4) arrivent avec
l'étage d'injection ; le format de leur événement est fixé :
`id` (celui de la commande, ou null), `cmd` (40 caractères au plus),
`resultat` (`ok`, `collision`, `delai`), `niv0`, `dur_us` (durées émises, 64
au plus), `attente_us` (attente du silence), `relu_us` (durées relues pendant
l'émission).

### 6.3 `log` : annonces du firmware

Comme la ScreenBar §7.10, avec `src` = `sonde`, `capture`, `injection` ou
`reseau`, et `niv` = `notice` ou `trace`. Les modules y passent leurs
annonces par `jsonLog()` (Wi-Fi, injection : avec eux) ; sans `json log 1`,
elles restent du texte.

Une annonce part sans `json log 1` : si des réglages lus en NVS au démarrage
étaient hors bornes (remplacés par leur valeur par défaut, spec §8.2), chaque
ouverture de session (`json 1`) le dit par un `log` `sonde` `notice`, après
l'instantané et sa réponse, hors plafond (exemple en 8.3). Le texte
`[reglages] ...` du démarrage part en général avant que l'app n'ouvre le port.

## 7. Débit sur l'USB

| Situation | Débit |
|---|---|
| Au repos (`etat` et `compteurs` à 1 Hz, tailles de la section 8) | ~0,7 Ko/s |
| Trames de type WTC (une réception de 33 durées toutes les 100 ms, 258 octets par ligne) | + ~2,6 Ko/s |
| Plafond des trames (100 lignes pleines par seconde) | + ~86 Ko/s |

L'USB full-speed n'est pas le goulot : ce sont le tampon de 8 Ko et la
lecture du Mac. Une ligne perdue se voit à un trou de `n` et dans
`lignes_perdues`.

## 8. Exemples

`<RS>` note l'octet 0x1E ; le LF final est omis. Chaque ligne `<RS>{...}` est
vérifiée par `json_check.py --strict --exemples`.

### 8.1 Connexion

App → sonde (`\x15` : l'octet 0x15, Ctrl-U, suivi de LF) :

```
\x15
id=1 json 1
```

Sonde → app :

```
<RS>{"v":1,"t":"hello","n":0,"ms":12031,"bloc":"base","rev":4,"fw":"0.1.0-1a2b3c4","fw_desc":"0.1.0-1a2b3c4","date":"Sep 27 2026","heure":"14:02:11","env":"sonde","build":"sonde","reseau_build":"aucun","puce":"esp32c6","idf":"v5.5.5","arduino":"3.3.12","boot":"3FA2C901","reset":"mise_sous_tension","reset_n":1,"up_s":12,"session":{"transport":"usb","periode_ms":1000,"compteurs_ms":1000,"reseau_ms":5000,"bail_s":30,"trames":true,"log":false},"limites":{"ligne_max":1024,"cmd_max":127}}
<RS>{"v":1,"t":"hello","n":1,"ms":12032,"bloc":"identite","boot":"3FA2C901","mac":"F0F5BD012345","id":{"fabricant":"Djoko-CLI","produit":"Sonde hotte Haier","serie":"HOTTE-F0F5BD012345","nom":"Sonde hotte","hw":1,"hw_txt":"C6 SuperMini, etages v1"},"appareil":"hotte","caps":["sonde","injection","trames","log"]}
<RS>{"v":1,"t":"config","n":2,"ms":12033,"capture":{"gpio":6,"resol_hz":1000000,"filtre_us":1,"silence_us":5000,"mode":"tout","inverse":true}}
<RS>{"v":1,"t":"etat","n":3,"ms":12034,"bloc":"bus","boot":"3FA2C901","up_s":12,"repos":"haut","fronts":1520,"derniere_ms":3,"receptions_s":10}
<RS>{"v":1,"t":"etat","n":4,"ms":12035,"bloc":"capture","boot":"3FA2C901","up_s":12,"active":true,"mode":"tout","debord":0,"rep_en_cours":0}
<RS>{"v":1,"t":"etat","n":5,"ms":12036,"bloc":"sys","boot":"3FA2C901","up_s":12,"sys":{"heap":247812,"heap_min":241600,"heap_bloc":110580,"pile_boucle":5316,"boucle_max_ms":2,"json_perdus":0,"json_trop_longs":0,"rejets":0}}
<RS>{"v":1,"t":"compteurs","n":6,"ms":12037,"bloc":"sonde","receptions":118,"parties":118,"blocs":118,"symboles":1947,"debord":0,"rep":0,"lignes_perdues":0,"sautes":0,"rejets":0}
<RS>{"v":1,"t":"reponse","n":7,"ms":12038,"id":1,"etape":"fin","cmd":"json 1","ok":true,"code":"ok","duree_ms":7,"bail_s":30,"up_s":12}
```

Puis `etat` et `compteurs` chaque seconde, et les `trame`.

### 8.2 Trames

Une réception courte, d'une partie (motif `wtc` du banc : départ de 2T bas,
16 bits, T = 750 µs ; le dernier 1T haut se fond dans le repos) :

```
<RS>{"v":1,"t":"trame","n":214,"ms":15506,"num":57,"part":0,"fin":true,"t_us":15472110,"niv0":"bas","dur_us":[1500,749,751,748,748,752,748,750,752,748,752,749,748,748,751,751,2248,749,748,752,751,748,752,748,749,752,748,752,752,751,748,749,748],"debord":false}
```

Une longue réception (motif `rafale` : un front toutes les 100 µs) part en
parties de 110 durées, sans `fin` jusqu'au silence :

```
<RS>{"v":1,"t":"trame","n":301,"ms":16020,"num":61,"part":0,"fin":false,"t_us":16003300,"niv0":"haut","dur_us":[102,99,100,101,99,102,98,102,100,102,99,98,102,102,99,100,98,102,98,102,98,102,99,101,102,101,100,101,102,101,100,100,99,99,99,98,102,100,102,101,100,101,100,102,98,98,102,101,99,100,99,101,101,98,98,102,102,100,100,100,102,101,102,101,98,98,100,101,98,98,100,102,101,100,101,100,98,101,100,99,102,98,101,98,99,100,99,99,101,101,101,98,99,101,101,102,100,99,101,102,100,101,100,101,99,99,98,99,99,99],"debord":false}
```

Mode `changements` : douze réceptions identiques n'ont pas été émises avant
celle-ci. Plus loin, 37 lignes ont été sautées (plafond de 100 par seconde),
et un bloc a été perdu juste avant la partie :

```
<RS>{"v":1,"t":"trame","n":420,"ms":31002,"num":190,"part":0,"fin":true,"t_us":30975310,"niv0":"bas","dur_us":[1500,750,750,750,2250,751],"debord":false,"rep":12}
<RS>{"v":1,"t":"trame","n":988,"ms":52001,"num":4102,"part":0,"fin":false,"t_us":51990112,"niv0":"haut","dur_us":[100,101,99,100],"debord":true,"rep":0,"sautes":37}
```

### 8.3 Réglages de capture

```
id=2 capture changements
id=3 seuils 2 40000
```

```
<RS>{"v":1,"t":"reponse","n":502,"ms":40002,"id":2,"etape":"fin","cmd":"capture changements","ok":true,"code":"ok","duree_ms":14}
<RS>{"v":1,"t":"config","n":503,"ms":40003,"capture":{"gpio":6,"resol_hz":1000000,"filtre_us":1,"silence_us":5000,"mode":"changements","inverse":true}}
<RS>{"v":1,"t":"reponse","n":510,"ms":40410,"id":3,"etape":"fin","cmd":"seuils 2 40000","ok":false,"code":"usage","msg":"seuils : silence 40000 us hors bornes a 1000000 Hz (1000..32767)","duree_ms":0}
```

Une valeur hors bornes lue en NVS au démarrage (écrite par un autre firmware,
par exemple) est remplacée par sa valeur par défaut. Chaque `json 1` le dit
ensuite, juste après sa réponse, même sans `json log 1` :

```
<RS>{"v":1,"t":"log","n":1208,"ms":305012,"src":"sonde","niv":"notice","txt":"1 valeur(s) hors bornes en NVS : valeurs par defaut"}
```

### 8.4 Commande à texte, refus

```
id=4 stats
```

```
<RS>{"v":1,"t":"reponse","n":530,"ms":42002,"id":4,"etape":"debut","cmd":"stats","ok":true,"code":"en_cours"}
receptions 1204, parties 1204, blocs 1204, symboles 20468, debord 0 (blocs perdus, tampon plein)
<RS>{"v":1,"t":"reponse","n":531,"ms":42003,"id":4,"etape":"fin","cmd":"stats","ok":true,"code":"execute","duree_ms":1}
<RS>{"v":1,"t":"reponse","n":588,"ms":43010,"id":25,"etape":"fin","cmd":"json ping","ok":false,"code":"cadence","msg":"plus de 20 lignes par seconde : rien n'est execute","duree_ms":0}
```

### 8.5 Mode `log`, battement, fin de session

Après `id=5 json log 1`, `id=6 json periode 0` et `id=7 json compteurs 0`
(battement `hb` toutes les 2 s), l'app se tait ; 30 s après sa dernière ligne,
le bail rend le mode humain.

```
<RS>{"v":1,"t":"log","n":640,"ms":200100,"src":"capture","niv":"notice","txt":"[capture] echec du RMT ('capture on' pour reessayer)"}
<RS>{"v":1,"t":"hb","n":641,"ms":202000,"boot":"3FA2C901","up_s":202,"json_perdus":0}
<RS>{"v":1,"t":"fin","n":662,"ms":231400,"cause":"bail"}
json : mode machine coupe (hote muet depuis 30 s)
>
```

### 8.6 Événement `injection`

Format seulement (les commandes viennent avec l'étage d'injection) :

```
<RS>{"v":1,"t":"reponse","n":900,"ms":123001,"id":42,"etape":"fin","cmd":"injecte durees 750 750 750 2250","ok":true,"code":"accepte","duree_ms":1,"suite":"injection"}
<RS>{"v":1,"t":"injection","n":901,"ms":123456,"id":42,"cmd":"injecte durees 750 750 750 2250","resultat":"ok","niv0":"bas","dur_us":[750,750,750,2250],"attente_us":20412,"relu_us":[752,748,751,2249]}
```
````

- [ ] **Étape 6 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_json_check.py -v`
Attendu : « Ran 16 tests » puis « OK »

Lancer : `sh tools/tests/test_hote.sh`
Attendu (fin de la sortie) :

```text
test_json : 243 verifications, 0 echecs
16 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
plus longue ligne : 218 octets (.../hotte-tests/test_json_lignes.txt:15)
par type : fin 1, hb 1, injection 3, log 1, reponse 7, trame 3
24 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
plus longue ligne : 526 octets (docs/PROTOCOLE-JSON.md exemple 10)
par type : compteurs/sonde 1, config 2, etat/bus 1, etat/capture 1, etat/sys 1, fin 1, hb 1, hello/base 1, hello/identite 1, injection 1, log 2, reponse 7, trame 4
tests hote : OK
```

- [ ] **Étape 7 : Brancher le firmware sur le mode machine (le test est la compilation)**

Le « test » du firmware est sa compilation : on écrit d'abord les appelants de `json_mode`, qui n'existe pas encore.

Remplacer tout le contenu de `src/sonde.h` par :

```cpp
#pragma once
#include "reglages.h"
ReglagesSonde &sondeReglages();          // reglages en vigueur (main.cpp)
// Nouveaux reglages (deja bornes par l'appelant) : capture relancee (captureEnd
// + captureBegin) si elle tourne et que ses reglages changent, puis
// reglagesSauver et jsonConfigChanged. false : echec du RMT ou de la NVS.
bool sondeAppliquer(const ReglagesSonde &r);
// Mode changements (etat.capture.rep_en_cours, compteurs.sonde.rep) :
// receptions identiques non emises depuis la derniere emise, et depuis le demarrage.
uint32_t sondeRepEnCours();
uint32_t sondeRepTotal();
// Valeurs hors bornes lues en NVS au demarrage, remplacees par leur valeur par
// defaut (reglagesCharger) : annoncees a chaque ouverture de session (spec 8.2).
uint8_t sondeHorsBornes();
```

Remplacer tout le contenu de `src/main.cpp` par :

```cpp
// ===========================================================================
//  Sonde de reconnaissance de la ligne D (docs/SPEC-RECONNAISSANCE.md)
//
//  setup() : GPIO7 bas en premier, port USB, identifiant de demarrage,
//  reglages NVS, interruption des fronts, capture RMT, console. loop() :
//  console, capture (parties -> lignes trame du mode machine, ou texte),
//  mode machine (bail, lignes periodiques), puis la tache IDLE.
// ===========================================================================
#include <Arduino.h>

#include "bord.h"
#include "capture_rmt.h"
#include "cli.h"
#include "config.h"
#include "fw_version.h"
#include "json_mode.h"
#include "json_out.h"
#include "reglages.h"
#include "sonde.h"

static ReglagesSonde sReglages;
static capt::Changements sChangements;  // mode changements : receptions d'une partie
static jsonp::RateCap sTexte(20);       // lignes trame affichees par seconde (mode humain)
static char sLigne[1024];
static uint32_t sRepTotal = 0;          // receptions identiques non emises depuis le demarrage
static uint8_t sHorsBornes = 0;         // valeurs hors bornes lues en NVS au demarrage (spec 8.2)

ReglagesSonde &sondeReglages() { return sReglages; }
uint32_t sondeRepEnCours() { return sChangements.repEnCours(); }
uint32_t sondeRepTotal() { return sRepTotal; }
uint8_t sondeHorsBornes() { return sHorsBornes; }

static bool memeCapture(const capt::Reglages &a, const capt::Reglages &b) {
  return a.resolHz == b.resolHz && a.filtreUs == b.filtreUs && a.silenceUs == b.silenceUs && a.inverse == b.inverse;
}

bool sondeAppliquer(const ReglagesSonde &r) {
  ReglagesSonde n = r;
  capt::borner(&n.capture);  // deja fait par l'appelant ; jamais de valeur hors bornes au RMT
  const bool relance = captureActive() && !memeCapture(n.capture, sReglages.capture);
  if (n.changements != sReglages.changements) sChangements.reset();
  sReglages = n;
  bool ok = true;
  if (relance) ok = captureBegin(sReglages.capture);  // captureEnd() d'abord
  if (!reglagesSauver(sReglages)) ok = false;
  jsonConfigChanged();  // config reemise aux sessions en mode machine
  return ok;
}

// Mode humain : 'trame <num>.<part>[ fin] t=<t_us> <haut|bas> <d1> <d2> ...',
// 20 lignes par seconde au plus. Jamais d'attente (spec 8.7) : sans la place
// d'une ligne entiere dans le tampon d'emission de l'USB (Mac qui ne lit
// plus), la ligne est perdue, car Serial.write bloquerait la boucle jusqu'a
// 2 s. Lignes sautees (plafond) et perdues (tampon plein) sont comptees
// ensemble et signalees sur la suivante.
static void afficherTrame(const capt::Partie &p, bool hasRep, uint32_t rep) {
  if (!sTexte.available(millis()) || Serial.availableForWrite() < (int)sizeof(sLigne)) {
    sTexte.skip();
    return;
  }
  sTexte.take();
  const size_t cap = sizeof(sLigne) - 64;  // place pour les suffixes et le saut de ligne
  size_t k = (size_t)snprintf(sLigne, sizeof(sLigne), "trame %lu.%lu%s t=%llu %s", (unsigned long)p.num,
                              (unsigned long)p.part, p.fin ? " fin" : "", (unsigned long long)p.tUs,
                              p.niv0Haut ? "haut" : "bas");
  for (uint16_t i = 0; i < p.n && k < cap; i++)
    k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " %lu", (unsigned long)p.dur[i]);
  if (p.debord) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " debord");
  if (hasRep) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " rep=%lu", (unsigned long)rep);
  const uint32_t sautees = sTexte.takeSkipped();
  if (sautees) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " (%lu lignes sautees)", (unsigned long)sautees);
  sLigne[k++] = '\n';
  Serial.write((const uint8_t *)sLigne, k);
}

// Chaque partie produite par la capture (capturePoll) : lignes trame vers les
// sessions en mode machine ; texte sur l'USB hors mode machine seulement.
static void surPartie(const capt::Partie &p, void *) {
  bool hasRep = false;
  uint32_t rep = 0;
  if (sReglages.changements) {
    if (!sChangements.aEmettre(p)) {  // identique a la derniere emise : comptee
      sRepTotal++;
      return;
    }
    hasRep = true;
    rep = sChangements.prendreRep();
  }
  jsonTrame(p, hasRep, rep);
  if (!jsonMachine()) afficherTrame(p, hasRep, rep);
}

void setup() {
  // TOUJOURS la premiere instruction (spec 8.4) : la base de l'etage
  // d'injection a l'etat bas avant tout le reste.
  pinMode(kPinInjection, OUTPUT);
  digitalWrite(kPinInjection, LOW);
  // Tampon d'emission de l'USB (HWCDC) : 256 octets par defaut, moins qu'une
  // ligne machine (1024). 8 Ko : une ligne periodique part s'il en reste 2048
  // libres, et une dizaine de lignes trame tiennent pendant que le Mac lit.
  Serial.setTxBufferSize(8192);
  Serial.begin(115200);
  jsonBegin();  // identifiant de ce demarrage (hello.boot), avant toute radio
  // Nombre garde : un log l'annonce aussi a chaque 'json 1' (json_mode.cpp).
  sHorsBornes = reglagesCharger(&sReglages);
  Serial.printf("firmware %s (%s)\n", FW_VERSION_FULL, FW_ENV);
  if (sHorsBornes) Serial.printf("[reglages] %u valeur(s) hors bornes en NVS : valeur(s) par defaut\n", sHorsBornes);
  if (!bordBegin(kPinEcoute)) Serial.println("[bord] interruption des fronts indisponible");
  if (!captureBegin(sReglages.capture)) Serial.println("[capture] echec du RMT ('capture on' pour reessayer)");
  cliBegin();
}

void loop() {
  cliPoll();
  capturePoll(surPartie, nullptr, 8);
  jsonPoll();
  vTaskDelay(1);  // laisse tourner la tache IDLE
}
```

Remplacer tout le contenu de `src/cli.cpp` par :

```cpp
// Copie partielle de benq-screenbar-halo-matter@c58a506 : src/cli.cpp (adapte : runLine et cliPoll repris, commandes de la sonde)
// ===========================================================================
//  Console de la sonde sur l'USB (spec 8.3) : texte humain, 'help' liste tout.
//
//  Les commandes sont dans kCommandes : les taches suivantes y AJOUTENT des
//  lignes (et leurs handlers au-dessus), sans rien renommer. Les commandes
//  interdites a distance le sont par jsonp::remoteRefusal (json_out), pas par
//  la table.
//
//  Lignes de l'hote (docs/PROTOCOLE-JSON.md) : un prefixe id=<n> fait la
//  semantique (runLine). Avec id, 'json ...', 'capture ...' et 'seuils <v>'
//  repondent sans texte (reponse ok, usage ou refuse) ; les autres commandes
//  gardent leur texte, entre une reponse debut et une reponse fin.
// ===========================================================================
#include "cli.h"

#include <Arduino.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <string.h>

#include "bord.h"
#include "capture_rmt.h"
#include "config.h"
#include "fw_version.h"
#include "json_mode.h"
#include "json_out.h"
#include "sonde.h"

typedef void (*Handler)(char *args);   // args : ce qui suit le mot-cle (peut etre "")
struct Commande {
  const char *nom;      // mot-cle
  Handler h;
  const char *aide;     // une ligne pour 'help'
};

// ---------------------------------------------------------------------------
//  Outils
// ---------------------------------------------------------------------------

// Detache le premier mot de s et rend le reste (jamais nul, espaces de tete sautes).
static char *splitWord(char *s) {
  char *sp = strchr(s, ' ');
  if (!sp) return s + strlen(s);
  *sp++ = 0;
  while (*sp == ' ') sp++;
  return sp;
}

// Entier decimal strict : chiffres seulement, 4294967295 au plus.
static bool lireU32(const char *s, uint32_t *v) {
  if (!*s) return false;
  uint64_t x = 0;
  for (const char *p = s; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    x = x * 10 + (uint64_t)(*p - '0');
    if (x > 0xFFFFFFFFull) return false;
  }
  *v = (uint32_t)x;
  return true;
}

// Silence maximal a une resolution : kSilenceMaxTicks ticks.
static uint32_t silenceMaxUs(uint32_t resolHz) {
  return (uint32_t)((uint64_t)kSilenceMaxTicks * 1000000u / resolHz);
}

static void afficherReglages(const ReglagesSonde &r) {
  Serial.printf("capture %s, mode %s : GPIO%u, %lu Hz, filtre %u us, silence %lu us, etage %s\n",
                captureActive() ? "active" : "arretee", r.changements ? "changements" : "tout", (unsigned)kPinEcoute,
                (unsigned long)r.capture.resolHz, (unsigned)r.capture.filtreUs, (unsigned long)r.capture.silenceUs,
                r.capture.inverse ? "inverseur" : "direct");
}

// Resultat d'une commande de reglage, sans texte : reponse.code et reponse.msg
// avec un id, texte humain sans id.
struct Resultat {
  bool ok;
  const char *code;  // ok, usage, refuse
  const char *msg;   // nul : rien a dire ; 120 caracteres au plus (reponse.msg)
};

static const char kUsageCapture[] = "usage : capture on|off|tout|changements";
static const char kUsageSeuils[] = "usage : seuils [filtre_us 0..3] [silence_us] [resol_hz 1000000|500000]";

// capture on|off|tout|changements ; args vide : rien a faire.
static Resultat faireCapture(const char *args) {
  if (!strcmp(args, "on")) {
    if (!captureActive() && !captureBegin(sondeReglages().capture))
      return {false, "refuse", "capture : echec du demarrage du RMT"};
  } else if (!strcmp(args, "off")) {
    captureEnd();
  } else if (!strcmp(args, "tout") || !strcmp(args, "changements")) {
    ReglagesSonde r = sondeReglages();
    r.changements = !strcmp(args, "changements");
    if (!sondeAppliquer(r)) return {true, "ok", "capture : mode applique mais pas enregistre en NVS"};
  } else if (*args) {
    return {false, "usage", kUsageCapture};
  }
  return {true, "ok", nullptr};
}

// seuils <filtre_us> [silence_us] [resol_hz] : tout est verifie avant d'ecrire (spec 8.2).
static Resultat faireSeuils(char *args) {
  static char msg[jsonp::kMsgMax + 1];
  ReglagesSonde r = sondeReglages();
  uint32_t v[3] = {};
  uint8_t n = 0;
  for (char *w = args; *w;) {
    char *reste = splitWord(w);
    if (n == 3 || !lireU32(w, &v[n])) return {false, "usage", kUsageSeuils};
    n++;
    w = reste;
  }
  if (!n) return {false, "usage", kUsageSeuils};
  capt::Reglages c = r.capture;
  if (!capt::filtreValide(v[0])) {
    snprintf(msg, sizeof(msg), "seuils : filtre %lu us hors bornes (0..%u)", (unsigned long)v[0],
             (unsigned)kFiltreMaxUs);
    return {false, "usage", msg};
  }
  c.filtreUs = (uint8_t)v[0];
  if (n >= 3) {
    if (!capt::resolValide(v[2])) {
      snprintf(msg, sizeof(msg), "seuils : resolution %lu Hz refusee (1000000 ou 500000)", (unsigned long)v[2]);
      return {false, "usage", msg};
    }
    c.resolHz = v[2];
  }
  if (n >= 2) c.silenceUs = v[1];
  if (!capt::silenceValide(c.silenceUs, c.resolHz)) {
    snprintf(msg, sizeof(msg), "seuils : silence %lu us hors bornes a %lu Hz (%lu..%lu)", (unsigned long)c.silenceUs,
             (unsigned long)c.resolHz, (unsigned long)kSilenceMinUs, (unsigned long)silenceMaxUs(c.resolHz));
    return {false, "usage", msg};
  }
  r.capture = c;
  if (!sondeAppliquer(r)) return {false, "refuse", "seuils : echec du RMT ou de la NVS"};
  return {true, "ok", nullptr};
}

// ---------------------------------------------------------------------------
//  Commandes
// ---------------------------------------------------------------------------

static void cmdHelp(char *args);  // apres la table, qu'elle parcourt

static void cmdInfo(char *) {
  uint8_t mac[8] = {};  // 8 octets : esp_read_mac peut ecrire une EUI-64
  Serial.printf("firmware %s (env %s), IDF %s, Arduino %s\n", FW_VERSION_FULL, FW_ENV, esp_get_idf_version(),
                ESP_ARDUINO_VERSION_STR);
  if (esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK)
    Serial.printf("MAC %02X:%02X:%02X:%02X:%02X:%02X, serie HOTTE-%02X%02X%02X%02X%02X%02X\n", mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  else Serial.println("MAC illisible");
  Serial.printf("ecoute GPIO%u, injection GPIO%u (tenue basse)\n", (unsigned)kPinEcoute, (unsigned)kPinInjection);
  afficherReglages(sondeReglages());
}

static void cmdCapture(char *args) {
  const Resultat r = faireCapture(args);
  if (r.msg) Serial.println(r.msg);
  if (r.ok) afficherReglages(sondeReglages());
}

static void cmdSeuils(char *args) {
  if (*args) {
    const Resultat r = faireSeuils(args);
    if (r.msg) Serial.println(r.msg);
    if (!r.ok) return;
  }
  afficherReglages(sondeReglages());
}

static void cmdBus(char *) {
  const bool haut = bordBusHaut();
  const uint64_t dernier = bordDernierUs();
  Serial.printf("bus %s, %lu fronts depuis le demarrage", haut ? "haut" : "bas", (unsigned long)bordFronts());
  if (!dernier) {
    Serial.println(", aucun encore");
    return;
  }
  const uint64_t age = (uint64_t)esp_timer_get_time() - dernier;
  Serial.printf(", dernier il y a %llu ms", (unsigned long long)(age / 1000));
  if (age >= sondeReglages().capture.silenceUs) Serial.printf(" : repos %s\n", haut ? "haut" : "bas");
  else Serial.println(" : reception en cours");
}

static void cmdStats(char *) {
  const CaptureStats s = captureStats();
  Serial.printf("receptions %lu, parties %lu, blocs %lu, symboles %lu, debord %lu (blocs perdus, tampon plein)\n",
                (unsigned long)s.receptions, (unsigned long)s.parties, (unsigned long)s.blocs,
                (unsigned long)s.symboles, (unsigned long)s.debord);
}

static void cmdJson(char *args) { jsonCommand(args, JsonCmd{false, 0, "", millis()}); }

static void cmdReboot(char *) {
  Serial.println("redemarrage");
  Serial.flush();
  delay(100);
  ESP.restart();
}

// Table des commandes : les taches suivantes AJOUTENT des lignes a cette table
// (et leurs handlers au-dessus), sans rien renommer.
static const Commande kCommandes[] = {
  {"help", cmdHelp, "liste des commandes"},
  {"info", cmdInfo, "version, MAC, reglages de capture"},
  {"capture", cmdCapture, "capture on|off|tout|changements"},
  {"seuils", cmdSeuils, "seuils [filtre_us 0..3] [silence_us] [resol_hz]"},
  {"bus", cmdBus, "niveau du bus, repos, fronts"},
  {"stats", cmdStats, "compteurs de capture"},
  {"reboot", cmdReboot, "redemarrage"},
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log] : mode machine"},
};

static void cmdHelp(char *) {
  Serial.println("=== Commandes ===");
  for (const Commande &c : kCommandes) Serial.printf("  %-10s %s\n", c.nom, c.aide);
}

// Decoupe le mot-cle, cherche dans kCommandes ; false : inconnue.
static bool executer(char *line) {
  while (*line == ' ') line++;
  size_t n = strlen(line);
  while (n && line[n - 1] == ' ') line[--n] = 0;
  if (!*line) return true;
  char *args = splitWord(line);
  for (const Commande &c : kCommandes) {
    if (!strcmp(line, c.nom)) {
      c.h(args);
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
//  Lignes de l'hote (protocole de la ScreenBar, sections 2.6 et 6). runLine et
//  cliPoll sont repris de benq-screenbar-halo-matter@c58a506 : src/cli.cpp.
// ---------------------------------------------------------------------------

// Premier mot de s egal a w (sans couper la ligne).
static bool firstWordIs(const char *s, const char *w) {
  const size_t n = strlen(w);
  return !strncmp(s, w, n) && (s[n] == ' ' || !s[n]);
}

// Ce qui suit le premier mot, espaces sautes.
static char *afterWord(char *s) {
  while (*s && *s != ' ') s++;
  while (*s == ' ') s++;
  return s;
}

// Une ligne de l'hote : prefixe id=<n> retire avant l'aiguillage, refus sans
// execution (trop longue, cadence, interdite a distance), puis :
//  - sans id : comme toujours (texte) ;
//  - avec id : famille json (reponse seule) ; capture et seuils <valeurs>
//    sans texte (reponse ok, usage ou refuse ; spec 8.3 : a distance, aucune
//    commande ne produit de texte) ; sinon commande a texte entre reponse
//    debut et reponse fin.
// A distance (tache 19), jsonRemoteAdmit passera avant tout refus.
static void runLine(char *line, bool tooLong) {
  const uint32_t t0 = millis();
  const bool remote = jsonOrigin() != jsonp::kUsb;
  uint32_t id = 0;
  char *cmd = line;
  const bool hasId = jsonp::parseIdPrefix(line, &id, &cmd);
  while (*cmd == ' ') cmd++;
  size_t len = strlen(cmd);
  while (len && cmd[len - 1] == ' ') cmd[--len] = 0;
  if (remote && !hasId) {
    jsonCountRejected();  // a distance, toujours un id : sans lui, aucune reponse possible
    return;
  }
  if (!hasId && !*cmd && !tooLong) return;  // ligne vide (Ctrl-U compris) : rien
  char shown[jsonp::kCmdTextMax + 1];
  jsonp::copyCmd(shown, cmd);  // avant que l'aiguillage ne coupe la ligne en mots
  jsonp::maskCmd(shown);       // jamais l'alea de 'json cle nouvelle' dans la reponse
  const JsonCmd c{hasId, id, shown, t0};
  if (tooLong) {
    jsonRefuse(c, "trop_long", "ligne de plus de 127 octets : rien n'est execute");
    return;
  }
  if ((hasId || jsonMachine() || remote) && !jsonCadenceOk(t0)) {
    jsonRefuse(c, "cadence", "plus de 20 lignes par seconde : rien n'est execute");
    return;
  }
  if (remote) {
    if (const char *why = jsonp::remoteRefusal(cmd)) {
      jsonRefuse(c, "interdite", why);
      jsonAfterCommand();
      return;
    }
  }
  if (!hasId) {
    if (!executer(cmd)) Serial.printf("commande inconnue : %s ('help')\n", cmd);
    jsonAfterCommand();
    return;
  }
  jsonp::Reply r;
  r.id = id;
  r.cmd = shown;
  if (!*cmd) {
    r.ok = false;
    r.code = "inconnue";
    r.msg = "commande vide";
    jsonReply(r);
    return;
  }
  if (firstWordIs(cmd, "json")) {
    jsonCommand(afterWord(cmd), c);
  } else if (firstWordIs(cmd, "capture") || (firstWordIs(cmd, "seuils") && *afterWord(cmd))) {
    const Resultat res = firstWordIs(cmd, "capture") ? faireCapture(afterWord(cmd)) : faireSeuils(afterWord(cmd));
    r.ok = res.ok;
    r.code = res.code;
    r.msg = res.msg;
    r.durMs = millis() - t0;
    jsonReply(r);
  } else {
    r.fin = false;
    r.code = "en_cours";
    jsonReply(r);  // la commande va ecrire son texte
    const bool known = executer(cmd);
    if (!known) Serial.printf("commande inconnue : %s ('help')\n", cmd);
    r.fin = true;
    r.ok = known;
    r.code = known ? "execute" : "inconnue";
    r.durMs = millis() - t0;
    jsonReplyEnd(r);  // 'help' peut remplir le tampon d'emission : jamais perdue
  }
  jsonAfterCommand();
}

// ---------------------------------------------------------------------------

static jsonp::LineAssembler sLine;  // 127 caracteres au plus, prefixe id= compris

void cliBegin() {
  Serial.println("Tape 'help' pour la liste des commandes.");
  Serial.print("> ");
}

// Mode humain : echo, saut de ligne, commande, flush, invite. Mode machine :
// ni echo, ni saut de ligne, ni invite, ni Serial.flush() (qui attendrait un
// hote muet, puis viderait tout le tampon d'emission, lignes machine
// comprises) ; octets hors 0x20..0x7E ignores. Ctrl-U (0x15) vide la ligne
// en cours dans les deux modes.
void cliPoll() {
  while (Serial.available()) {
    const uint8_t c = (uint8_t)Serial.read();
    jsonNoteRx();
    const bool machine = jsonMachine();
    switch (sLine.feed(c, machine)) {
      case jsonp::LineAssembler::Ev::Echo:
        if (!machine) Serial.print((char)c);
        break;
      case jsonp::LineAssembler::Ev::Erase:
        if (!machine) Serial.print("\b \b");
        break;
      case jsonp::LineAssembler::Ev::Clear:
        if (!machine)
          for (uint8_t i = 0; i < sLine.cleared(); i++) Serial.print("\b \b");
        break;
      case jsonp::LineAssembler::Ev::Line: {
        if (!machine) Serial.println();
        const bool tooLong = sLine.tooLong();
        runLine(sLine.text(), tooLong);
        sLine.reset();
        // 'json 1' vient de couper l'invite ; 'json 0' l'a deja reaffichee.
        if (!machine && !jsonMachine()) {
          Serial.flush();  // l'USB CDC du C6 perd des octets si on enchaine trop vite
          Serial.print("> ");
        }
        break;
      }
      case jsonp::LineAssembler::Ev::None: break;
    }
  }
}
```

- [ ] **Étape 8 : Compiler et vérifier l'échec**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "error|FAILED"`
Attendu : ÉCHEC avec « src/cli.cpp:26:10: fatal error: json_mode.h: No such file or directory » (et la même erreur pour `src/main.cpp:16:10`)

- [ ] **Étape 9 : Écrire `json_mode.h` et le début de `json_mode.cpp` (état, émission, textes)**

`json_mode.cpp` s'écrit en quatre morceaux, chacun compilé : le compilateur vérifie chaque morceau, et l'édition de liens ne réclame plus que les fonctions publiques pas encore écrites.

Créer `src/json_mode.h` :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/json_mode.h (adapte : profil hotte, sans lampe, LED, Matter ni livraison)
#pragma once
// ===========================================================================
//  Mode machine : protocole compagnon v1, profil hotte (docs/PROTOCOLE-JSON.md)
//
//  Session ('json 1', bail, 'json 0'), lignes periodiques (etat, compteurs,
//  battement) par la file des periodiques, evenements trame et log, reponses
//  aux lignes portant un id.
//
//  Une session par transport (origine, jsonp::kUsb puis, en tache 19, une
//  par session reseau) : ses reglages, son n, sa file, ses pertes. Les
//  evenements partent vers chaque session en mode machine, formates pour
//  elle ; une reponse, vers l'origine de sa commande seulement.
//
//  Regles tenues ici (spec 8.7, protocole de la ScreenBar 2.3) :
//  - un seul producteur, la tache loop ; un seul tampon de formatage ;
//  - jamais d'attente : une ligne qui ne tient pas dans le tampon d'emission
//    de HWCDC est perdue et comptee (json_perdus), jamais ecrite a moitie ;
//  - lignes periodiques : une par tour de loop() et par transport, et
//    seulement s'il reste ensuite la place d'un evenement ; perdues apres
//    500 ms de retard ;
//  - reponses differees (apres un instantane, ou apres le texte d'une
//    commande qui a rempli le tampon) : par la meme file, jamais perdues pour
//    retard ;
//  - rien n'est persiste : chaque demarrage repart en mode humain.
// ===========================================================================
#include <Arduino.h>

#include "capture_model.h"
#include "json_out.h"

// Debut de setup(), avant toute radio : identifiant de ce demarrage (hello.boot).
void jsonBegin();
// A chaque tour de loop(), apres cliPoll() et capturePoll() : bail, lignes periodiques.
void jsonPoll();

uint32_t jsonBootId();
bool jsonMachine();  // mode machine en cours sur l'USB
void jsonNoteRx();   // un octet recu de l'hote USB (bail)

// --- CLI (cli.cpp) ----------------------------------------------------------

struct JsonCmd {
  bool hasId;
  uint32_t id;
  const char *cmd;  // commande sans le prefixe, tronquee (reponse.cmd)
  uint32_t t0;      // debut de l'execution (duree_ms)
};
// Famille 'json ...' ; avec id, la reponse part d'ici (immediate, ou apres les
// lignes d'un instantane).
void jsonCommand(char *arg, const JsonCmd &c);
// Au plus 20 lignes par seconde et par transport (mode machine, ou ligne avec id).
bool jsonCadenceOk(uint32_t now);
// Ligne refusee sans execution (trop_long, cadence, interdite) : reponse avec
// id, texte en mode humain sans id ; comptee (sys.rejets, compteurs.rejets).
void jsonRefuse(const JsonCmd &c, const char *code, const char *msg);
// Reponse immediate (non bloquante), en tout mode.
void jsonReply(const jsonp::Reply &r);
// Reponse fin d'une commande a texte : tout de suite si une ligne entiere
// tient, sinon par la file des qu'elle tient.
void jsonReplyEnd(const jsonp::Reply &r);
// Fin d'une commande : bail, et config reemise si un reglage a change.
void jsonAfterCommand();
// Un reglage de capture a change (sondeAppliquer) : config reemise a chaque
// session en mode machine au prochain jsonAfterCommand().
void jsonConfigChanged();

// --- Evenements de la sonde (vers chaque session en mode machine) ----------

// Partie produite par la capture : ligne trame si la session a 'json trames 1'.
// 100 lignes par seconde et par session au plus ; les parties d'une reception
// sont produites ou sautees ensemble ; la suivante produite porte 'sautes'.
void jsonTrame(const capt::Partie &p, bool hasRep, uint32_t rep);
// Mode 'json log 1' : la ligne part en message log (src sonde|capture|
// injection|reseau, niv notice|trace), 20 par seconde au plus ; true si
// l'USB l'a prise (emise, plafonnee ou perdue), false pour l'afficher en texte.
bool jsonLog(const char *src, const char *niv, const char *txt);

// --- Transport reseau (cli.cpp ; etendu en tache 19) ------------------------

// Origine de la commande en cours : jsonp::kUsb hors cliRunRemote().
void jsonSetOrigin(uint8_t origin);
uint8_t jsonOrigin();
// Ligne refusee sans reponse possible (reseau, sans id) : comptee (rejets).
void jsonCountRejected();
```

Créer `src/json_mode.cpp` :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/json_mode.cpp (adapte : profil hotte, instantanes de la sonde, evenements trame et log ; sans lampe, LED, Matter ni livraison ; sessions reseau en tache 19)
#include "json_mode.h"

#include <bootloader_random.h>
#include <esp_app_desc.h>
#include <esp_arduino_version.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <sdkconfig.h>
#include <stdlib.h>
#include <string.h>

#include "bord.h"
#include "capture_rmt.h"
#include "config.h"
#include "fw_version.h"
#include "sonde.h"

using namespace jsonp;

// ===========================================================================
//  Etat
// ===========================================================================

// Identite de la sonde (spec 8.5, hello.identite).
static constexpr char kFabricant[] = "Djoko-CLI";
static constexpr char kProduit[] = "Sonde hotte Haier";
static constexpr char kSeriePrefixe[] = "HOTTE-";
static constexpr char kNom[] = "Sonde hotte";
static constexpr uint32_t kHw = 1;
static constexpr char kHwTxt[] = "C6 SuperMini, etages v1";
static constexpr char kAppareil[] = "hotte";

static constexpr uint32_t kHbMs = 2000;  // battement quand les etat sont coupes ou lents
// Profil USB (spec 8.5) : etat 1 s, compteurs 1 s, reseau 5 s.
static constexpr uint32_t kPeriodDefault = 1000, kCountersDefault = 1000, kNetDefault = 5000;
static constexpr uint16_t kLeaseDefault = 30;
static constexpr uint8_t kReplies = 4;          // reponses differees (apres un instantane)
static constexpr uint32_t kHeapBlocMs = 10000;  // plus grand bloc du tas relu au plus toutes les 10 s
// Profil distant (spec 8.5) : etat 2 s, compteurs 5 s, reseau 30 s, trames
// sans coupure automatique ; retard admis dans la file (debit plafonne).
static constexpr uint32_t kRemotePeriod = 2000, kRemoteCounters = 5000, kRemoteNet = 30000;
static constexpr uint32_t kRemoteLateMs = 6000;
static constexpr uint16_t kLogCap = 20;  // lignes log par seconde et par session

static Writer sW;           // le seul tampon de formatage (1024 octets)
static bool sBusy = false;  // une ligne en cours de formatage dans sW
static uint32_t sBoot = 0;

// Reponse differee : part par la file (apres les lignes deja en file, ou
// quand une ligne entiere tient dans le tampon d'emission).
struct PendingReply {
  bool used;
  Reply r;        // r.cmd pointe sur cmd ; r.code litteral ; r.msg nul
  uint32_t t0;    // durAtSend : duree_ms mesuree a l'envoi (instantane)
  bool durAtSend;
  char cmd[kCmdTextMax + 1];
};

// Une session par transport (origine). L'USB (kUsb) existe toujours ; les
// origines reseau viennent avec le transport UDP (tache 19).
struct Sink {
  bool machine = false;
  uint32_t periodMs = kPeriodDefault, countersMs = kCountersDefault, netMs = kNetDefault;
  uint16_t leaseS = kLeaseDefault;
  bool frames = true, log = false;
  uint32_t nextEtat = 0, nextCpt = 0, nextNet = 0, nextHb = 0;
  uint32_t lastRx = 0, lastCmd = 0;  // bail : dernier octet recu, fin de la derniere commande
  uint32_t n = 0;                    // n de la prochaine ligne produite sur ce transport
  uint32_t lost = 0, tooLong = 0, rejected = 0;
  uint32_t loopMaxMs = 0;            // plus long tour de loop() depuis le bloc sys emis
  uint32_t trameNum = 0;             // reception dont les parties sont en cours (0 : aucune)
  bool trameSautee = false;          // ses parties sont sautees (plafond)
  bool nvsLog = false;               // log des valeurs hors bornes en NVS a emettre (drain)
  Queue q;
  PendingReply replies[kReplies] = {};
  RateCap trameCap{kTramesParSeconde}, logCap{kLogCap};
  Cadence cadence;
};
static Sink sSinks[kOrigins];
static uint8_t sOrigin = kUsb;  // origine de la commande en cours

// Compteurs de la sonde depuis le demarrage, toutes sessions (compteurs.sonde).
static uint32_t sPerdues = 0, sSautes = 0, sRejets = 0;
static bool sConfigChanged = false;
static uint32_t sLoopAt = 0;
// Receptions par seconde (etat.bus), sur la derniere seconde complete.
static uint32_t sRecAt = 0, sRecPrev = 0, sRecParS = 0;
// Niveau de repos echantillonne hors reception (0 : jamais, 1 : haut, 2 : bas).
static uint8_t sRepos = 0;
// heap_caps_get_largest_free_block() parcourt tout le tas en section critique :
// relu au plus toutes les kHeapBlocMs, et a chaque 'json 1' ou 'json etat'.
static uint32_t sHeapBloc = 0, sHeapBlocAt = 0;
static bool sHeapBlocStale = true;

static uint32_t upS() { return (uint32_t)(esp_timer_get_time() / 1000000); }
static bool remote(uint8_t o) { return o != kUsb; }

static bool anyMachine() {
  for (const Sink &s : sSinks)
    if (s.machine) return true;
  return false;
}

// ===========================================================================
//  Emission
// ===========================================================================

// Place d'emission libre sur ce transport : octets du tampon de HWCDC (USB).
// Transport reseau : tache 19 (d'ici la, aucune place).
static int room(uint8_t o) {
  if (o == kUsb) return Serial.availableForWrite();
  return 0;
}

// Ecrit la ligne fermee de sW sur ce transport, entiere ou pas du tout.
static bool emit(uint8_t o) {
  const size_t len = sW.size();
  if (o != kUsb || Serial.availableForWrite() < (int)len) return false;
  Serial.write(sW.data(), len);
  return true;
}

// Reserve le tampon unique. false : une ligne est deja en cours (bogue :
// aucun formatage ne doit en appeler un autre), rien n'est produit.
static bool claim() {
  if (sBusy) return false;
  sBusy = true;
  return true;
}

// Ligne perdue (tampon plein, retard, file pleine) : n consomme et comptee.
static void lose(Sink &k, uint32_t count = 1) {
  k.lost += count;
  sPerdues += count;
}

// Ferme la ligne formatee pour la session o et l'ecrit, ou la perd sans
// attendre. n compte toute ligne produite, ecrite ou perdue.
static bool send(uint8_t o) {
  sBusy = false;
  Sink &s = sSinks[o];
  s.n++;
  if (!sW.finish()) {
    s.tooLong++;
    return false;
  }
  if (!emit(o)) {
    lose(s);
    return false;
  }
  return true;
}

// Texte emis par le protocole lui-meme sur l'USB (fin de bail, invite) : meme
// chemin non bloquant, perdu et compte si le tampon est plein.
static void textNb(const char *s) {
  const size_t len = strlen(s);
  if (Serial.availableForWrite() < (int)len) {
    lose(sSinks[kUsb]);
    return;
  }
  Serial.write((const uint8_t *)s, len);
}

// ===========================================================================
//  Textes
// ===========================================================================

static const char *resetCode(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "mise_sous_tension";
    case ESP_RST_EXT: return "broche";
    case ESP_RST_SW: return "logiciel";
    case ESP_RST_PANIC: return "panique";
    case ESP_RST_INT_WDT: return "chien_int";
    case ESP_RST_TASK_WDT: return "chien_tache";
    case ESP_RST_WDT: return "chien";
    case ESP_RST_BROWNOUT: return "baisse_tension";
    case ESP_RST_USB: return "usb";
    default: return "inconnue";
  }
}

// MAC-48 d'usine (tampon de 8 : esp_read_mac peut ecrire une EUI-64).
static bool factoryMac(uint8_t mac[8]) {
  memset(mac, 0, 8);
  return esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK;
}
```

Lancer :

```bash
~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "error|warning|undefined" | sed -E "s/.*(undefined reference to .[A-Za-z]+).*/\1/" | sort -u
```

Attendu : aucune ligne `src/json_mode.cpp` ni `src/json_mode.h` (ce morceau compile sans erreur ni avertissement) ; l'édition de liens échoue sur les 14 fonctions publiques pas encore écrites :

```text
collect2: error: ld returned 1 exit status
undefined reference to `jsonAfterCommand
undefined reference to `jsonBegin
undefined reference to `jsonCadenceOk
undefined reference to `jsonCommand
undefined reference to `jsonConfigChanged
undefined reference to `jsonCountRejected
undefined reference to `jsonMachine
undefined reference to `jsonNoteRx
undefined reference to `jsonOrigin
undefined reference to `jsonPoll
undefined reference to `jsonRefuse
undefined reference to `jsonReply
undefined reference to `jsonReplyEnd
undefined reference to `jsonTrame
```

- [ ] **Étape 10 : Ajouter les messages, la file des périodiques et les réponses**

Ajouter à la fin de `src/json_mode.cpp` (le bloc commence par une ligne vide) :

```cpp

// ===========================================================================
//  Messages de session et instantanes
// ===========================================================================

static void bootUp() {
  sW.hexU32("boot", sBoot, 8);
  sW.u32("up_s", upS());
}

static void helloBase(uint8_t o, uint32_t now) {
  const Sink &s = sSinks[o];
  const esp_app_desc_t *d = esp_app_get_description();
  sW.begin("hello", s.n, now);
  sW.str("bloc", "base");
  sW.u32("rev", kRev);
  sW.str("fw", FW_VERSION_FULL);
  sW.str("fw_desc", d->version, sizeof(d->version));
  sW.str("date", d->date, sizeof(d->date));
  sW.str("heure", d->time, sizeof(d->time));
  sW.str("env", FW_ENV);
  sW.str("build", "sonde");
  sW.str("reseau_build", "aucun");
  sW.str("puce", CONFIG_IDF_TARGET);
  sW.str("idf", esp_get_idf_version());
  sW.str("arduino", ESP_ARDUINO_VERSION_STR);
  sW.hexU32("boot", sBoot, 8);
  const esp_reset_reason_t r = esp_reset_reason();
  sW.str("reset", resetCode(r));
  sW.u32("reset_n", (uint32_t)r);
  sW.u32("up_s", upS());
  sW.obj("session");
  sW.str("transport", remote(o) ? "udp" : "usb");
  sW.u32("periode_ms", s.periodMs);
  sW.u32("compteurs_ms", s.countersMs);
  sW.u32("reseau_ms", s.netMs);
  sW.u32("bail_s", s.leaseS);
  sW.boolean("trames", s.frames);
  sW.boolean("log", s.log);
  sW.end();
  sW.obj("limites");
  sW.u32("ligne_max", kLineMax);
  sW.u32("cmd_max", kCmdMax);
  sW.end();
}

static void helloIdentity(uint8_t o, uint32_t now) {
  sW.begin("hello", sSinks[o].n, now);
  sW.str("bloc", "identite");
  sW.hexU32("boot", sBoot, 8);
  uint8_t mac[8];
  const bool macOk = factoryMac(mac);
  if (macOk) sW.hex("mac", mac, 6);
  else sW.null("mac");
  sW.obj("id");
  sW.str("fabricant", kFabricant, 32);
  sW.str("produit", kProduit, 32);
  if (macOk) {
    char serial[sizeof(kSeriePrefixe) + 12];
    snprintf(serial, sizeof(serial), "%s%02X%02X%02X%02X%02X%02X", kSeriePrefixe, mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
    sW.str("serie", serial, 32);
  } else {
    sW.null("serie");
  }
  sW.str("nom", kNom, 32);
  sW.u32("hw", kHw);
  sW.str("hw_txt", kHwTxt, 64);
  sW.end();
  sW.str("appareil", kAppareil);
  sW.arr("caps");
  sW.str(nullptr, "sonde");
  sW.str(nullptr, "injection");
  sW.str(nullptr, "trames");
  sW.str(nullptr, "log");
  sW.end();
}

static void config(uint8_t o, uint32_t now) {
  const ReglagesSonde &r = sondeReglages();
  sW.begin("config", sSinks[o].n, now);
  sW.obj("capture");
  sW.u32("gpio", kPinEcoute);
  sW.u32("resol_hz", r.capture.resolHz);
  sW.u32("filtre_us", r.capture.filtreUs);
  sW.u32("silence_us", r.capture.silenceUs);
  sW.str("mode", r.changements ? "changements" : "tout");
  sW.boolean("inverse", r.capture.inverse);
  sW.end();
}

static void etatBus(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "bus");
  bootUp();
  // Repos : niveau lu hors reception (aucun front depuis le silence de fin).
  const uint64_t dernier = bordDernierUs();
  const uint64_t age = dernier ? (uint64_t)esp_timer_get_time() - dernier : 0;
  if (!dernier || age >= sondeReglages().capture.silenceUs) sRepos = bordBusHaut() ? 1 : 2;
  if (sRepos) sW.str("repos", sRepos == 1 ? "haut" : "bas");
  else sW.null("repos");
  sW.u32("fronts", bordFronts());
  if (dernier) sW.u32("derniere_ms", age / 1000 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)(age / 1000));
  else sW.null("derniere_ms");
  sW.u32("receptions_s", sRecParS);
}

static void etatCapture(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "capture");
  bootUp();
  sW.boolean("active", captureActive());
  sW.str("mode", sondeReglages().changements ? "changements" : "tout");
  sW.u32("debord", captureStats().debord);
  sW.u32("rep_en_cours", sondeRepEnCours());
}

static void etatSys(uint8_t o, uint32_t now) {
  const Sink &k = sSinks[o];
  sW.begin("etat", k.n, now);
  sW.str("bloc", "sys");
  bootUp();
  sW.obj("sys");
  sW.u32("heap", esp_get_free_heap_size());
  sW.u32("heap_min", esp_get_minimum_free_heap_size());
  if (sHeapBlocStale || now - sHeapBlocAt >= kHeapBlocMs) {
    sHeapBloc = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    sHeapBlocAt = now;
    sHeapBlocStale = false;
  }
  sW.u32("heap_bloc", sHeapBloc);
  sW.u32("pile_boucle", (uint32_t)uxTaskGetStackHighWaterMark(nullptr));  // octets sous ESP-IDF
  sW.u32("boucle_max_ms", k.loopMaxMs);  // remis a 0 quand la ligne part (produce)
  sW.u32("json_perdus", k.lost);
  sW.u32("json_trop_longs", k.tooLong);
  sW.u32("rejets", k.rejected);
  sW.end();
}

static void compteurs(uint8_t o, uint32_t now) {
  const CaptureStats s = captureStats();
  sW.begin("compteurs", sSinks[o].n, now);
  sW.str("bloc", "sonde");
  sW.u32("receptions", s.receptions);
  sW.u32("parties", s.parties);
  sW.u32("blocs", s.blocs);
  sW.u32("symboles", s.symboles);
  sW.u32("debord", s.debord);
  sW.u32("rep", sondeRepTotal());
  sW.u32("lignes_perdues", sPerdues);
  sW.u32("sautes", sSautes);
  sW.u32("rejets", sRejets);
}

// Une ligne de la file de la session o : formatee maintenant, avec les
// valeurs du moment.
static void produce(uint8_t o, const Queued &q, uint32_t now) {
  if (!claim()) return;
  Sink &k = sSinks[o];
  switch (q.item) {
    case Item::HelloBase: helloBase(o, now); break;
    case Item::HelloId: helloIdentity(o, now); break;
    case Item::Config: config(o, now); break;
    case Item::EtatBus: etatBus(o, now); break;
    case Item::EtatCapture: etatCapture(o, now); break;
    case Item::EtatSys: etatSys(o, now); break;
    case Item::Compteurs: compteurs(o, now); break;
    case Item::Heartbeat: heartbeat(sW, k.n, now, sBoot, upS(), k.lost); break;
    case Item::Reply: {
      PendingReply &p = k.replies[q.arg < kReplies ? q.arg : 0];
      Reply r = p.r;
      r.cmd = p.cmd;
      if (p.durAtSend) r.durMs = now - p.t0;
      if (r.hasLease) {
        r.leaseS = k.leaseS;
        r.upS = upS();
      }
      reply(sW, k.n, now, r);
      p.used = false;
      break;
    }
    default:  // bloc absent de ce build (etat.injection : tache 23 ; reseau.ip : tache 19)
      sBusy = false;
      return;
  }
  // Le maximum n'est remis a 0 que s'il est parti : perdue, la ligne suivante le porte.
  if (send(o) && q.item == Item::EtatSys) k.loopMaxMs = 0;
}

// Valeurs hors bornes lues en NVS au demarrage (spec 8.2) : un log a chaque
// ouverture de session, apres l'instantane de 'json 1' et sa reponse, meme
// sans 'json log 1' (le texte du demarrage part en general avant que le Mac
// n'ouvre le port). Hors plafond des log : une ligne par session.
static void logNvs(uint8_t o, uint32_t now) {
  Sink &k = sSinks[o];
  k.nvsLog = false;
  if (!claim()) return;
  char txt[64];
  snprintf(txt, sizeof(txt), "%u valeur(s) hors bornes en NVS : valeurs par defaut", (unsigned)sondeHorsBornes());
  logLine(sW, k.n, now, "sonde", "notice", txt, 0);
  send(o);
}

static void drain(uint8_t o, uint32_t now) {
  Sink &k = sSinks[o];
  // Ligne periodique perdue par retard : n consomme (trou visible), comptee.
  // Jamais une reponse (Queue::dropLate).
  const uint8_t late = k.q.dropLate(now, remote(o) ? kRemoteLateMs : kLateMs);
  k.n += late;
  if (late) lose(k, late);
  const Queued *q = k.q.front();
  if (sBusy) return;
  if (!q) {
    // File vide : l'instantane de 'json 1' et sa reponse sont partis.
    if (k.nvsLog && k.machine && room(o) >= (int)kLineMax) logNvs(o, now);
    return;
  }
  // Periodique : avec 2048 octets libres, il en reste 1024 apres elle pour un
  // evenement. Reponse : des que 1024 sont libres. Sinon, au tour suivant.
  if (!k.q.frontReady(room(o))) return;
  const Queued item = *q;
  k.q.pop();
  produce(o, item, now);
}

static void push(uint8_t o, Item item, uint32_t now, bool session) {
  Sink &k = sSinks[o];
  if (!k.q.push(item, now, session)) {
    // File pleine : la ligne est perdue, comme une ligne en retard.
    k.n++;
    lose(k);
  }
}

static void pushState(uint8_t o, uint32_t now, bool session) {
  push(o, Item::EtatBus, now, session);
  push(o, Item::EtatCapture, now, session);
  push(o, Item::EtatSys, now, session);
}

static void pushCounters(uint8_t o, uint32_t now, bool session) { push(o, Item::Compteurs, now, session); }

// Bloc reseau.ip : avec le transport reseau (tache 19). Rien avant.
static void pushNet(uint8_t, uint32_t, bool) {}

static void pushHello(uint8_t o, uint32_t now, bool session) {
  push(o, Item::HelloBase, now, session);
  push(o, Item::HelloId, now, session);
  push(o, Item::Config, now, session);
}

// ===========================================================================
//  Reponses (vers l'origine de la commande en cours)
// ===========================================================================

// Ecrit la reponse tout de suite (perdue et comptee si elle ne tient pas).
static void replyEmit(uint8_t o, const Reply &r) {
  if (claim()) {
    reply(sW, sSinks[o].n, millis(), r);
    send(o);
  } else {
    // Ligne deja en cours de formatage (jamais vu) : perdue, n consomme et compte.
    sSinks[o].n++;
    lose(sSinks[o]);
  }
}

// Reponse par la file de la session o (apres les lignes deja en file) ; sans
// place (4 reponses en attente, file pleine) : tout de suite. Differee, elle
// perd son msg (il pointerait sur un tampon disparu).
static void replyQueue(uint8_t o, const Reply &r, uint32_t t0, bool durAtSend) {
  Sink &k = sSinks[o];
  for (uint8_t i = 0; i < kReplies; i++) {
    PendingReply &p = k.replies[i];
    if (p.used) continue;
    p.used = true;
    p.r = r;
    p.r.msg = nullptr;
    p.r.key = nullptr;
    p.r.kid = nullptr;
    p.r.hasKid = false;
    p.t0 = t0;
    p.durAtSend = durAtSend;
    copyCmd(p.cmd, r.cmd);
    if (k.q.push(Item::Reply, millis(), false, i)) return;
    p.used = false;
    break;
  }
  Reply now = r;
  if (durAtSend) now.durMs = millis() - t0;
  if (now.hasLease) {
    now.leaseS = k.leaseS;
    now.upS = upS();
  }
  replyEmit(o, now);
}

// Reponse immediate. Reseau : la cle n'y part jamais ; sans place, ou derriere
// une reponse deja en file (l'ordre des reponses est garde), elle attend dans
// la file de la session au lieu d'etre perdue.
static void replyTo(uint8_t o, const Reply &r) {
  Reply out = r;
  if (remote(o)) out.key = nullptr;
  if (remote(o) && (room(o) < (int)kLineMax || sSinks[o].q.has(Item::Reply))) {
    replyQueue(o, out, 0, false);
    return;
  }
  replyEmit(o, out);
}

void jsonReply(const Reply &r) { replyTo(sOrigin, r); }

// Reponse apres les lignes d'un instantane deja en file.
static void replyAfterQueue(const JsonCmd &c, bool lease) {
  if (!c.hasId) return;
  Reply r;
  r.id = c.id;
  r.cmd = c.cmd;
  r.hasLease = lease;
  replyQueue(sOrigin, r, c.t0, true);
}

void jsonReplyEnd(const Reply &r) {
  // Rien a doubler et la place d'une ligne entiere : tout de suite, juste
  // apres le texte. Sinon (le texte de la commande a rempli le tampon
  // d'emission), par la file, des que la place revient.
  if (!sSinks[sOrigin].q.has(Item::Reply) && room(sOrigin) >= (int)kLineMax) {
    jsonReply(r);
    return;
  }
  replyQueue(sOrigin, r, 0, false);
}

static void replyNow(const JsonCmd &c, bool ok, const char *code, const char *msg, bool lease = false) {
  if (!c.hasId) return;
  Reply r;
  r.id = c.id;
  r.cmd = c.cmd;
  r.ok = ok;
  r.code = code;
  r.msg = msg;
  r.durMs = millis() - c.t0;
  r.hasLease = lease;
  r.leaseS = sSinks[sOrigin].leaseS;
  r.upS = upS();
  jsonReply(r);
}

void jsonRefuse(const JsonCmd &c, const char *code, const char *msg) {
  sSinks[sOrigin].rejected++;
  sRejets++;
  if (c.hasId) replyNow(c, false, code, msg);
  else if (sOrigin == kUsb && !sSinks[kUsb].machine) Serial.printf("Ligne refusee (%s) : %s\n", code, msg);
}
```

Lancer :

```bash
~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "error|warning|undefined" | sed -E "s/.*(undefined reference to .[A-Za-z]+).*/\1/" | sort -u
```

Attendu : toujours aucune ligne `src/json_mode.cpp` ; `jsonRefuse`, `jsonReply` et `jsonReplyEnd` ne manquent plus :

```text
collect2: error: ld returned 1 exit status
undefined reference to `jsonAfterCommand
undefined reference to `jsonBegin
undefined reference to `jsonCadenceOk
undefined reference to `jsonCommand
undefined reference to `jsonConfigChanged
undefined reference to `jsonCountRejected
undefined reference to `jsonMachine
undefined reference to `jsonNoteRx
undefined reference to `jsonOrigin
undefined reference to `jsonPoll
undefined reference to `jsonTrame
```

- [ ] **Étape 11 : Ajouter la session et la commande `json`**

Ajouter à la fin de `src/json_mode.cpp` (le bloc commence par une ligne vide) :

```cpp

// ===========================================================================
//  Session
// ===========================================================================

static void resetTimers(Sink &k, uint32_t now) {
  k.nextEtat = now + k.periodMs;
  k.nextCpt = now + k.countersMs;
  k.nextNet = now + k.netMs;
  k.nextHb = now + kHbMs;
}

static void enterMachine(uint8_t o, uint16_t leaseS, uint32_t now) {
  Sink &k = sSinks[o];
  k.machine = true;
  k.loopMaxMs = 0;  // pas les tours du mode humain avant la session
  if (remote(o)) {
    k.periodMs = kRemotePeriod;
    k.countersMs = kRemoteCounters;
    k.netMs = kRemoteNet;
  } else {
    k.periodMs = kPeriodDefault;
    k.countersMs = kCountersDefault;
    k.netMs = kNetDefault;
  }
  k.frames = true;  // USB et reseau : les trames sont la raison d'etre de la sonde
  k.trameNum = 0;
  k.trameSautee = false;
  k.leaseS = leaseS;
  k.log = false;
  k.nvsLog = sondeHorsBornes() > 0;  // apres l'instantane (drain), meme sans 'json log 1'
  k.lastRx = k.lastCmd = now;
  resetTimers(k, now);
}

// Fin du mode machine : lignes de session retirees, message fin, puis (USB)
// texte et invite par le chemin non bloquant.
static void leaveMachine(uint8_t o, bool lease, uint32_t now) {
  Sink &k = sSinks[o];
  k.q.dropSession();
  if (claim()) {
    sessionEnd(sW, k.n, now, lease ? "bail" : "commande");
    send(o);
  }
  k.machine = false;
  if (o != kUsb) return;
  if (lease) {
    char t[80];
    snprintf(t, sizeof(t), "json : mode machine coupe (hote muet depuis %u s)\r\n> ", (unsigned)k.leaseS);
    textNb(t);
  } else {
    textNb("> ");
  }
}

bool jsonMachine() { return sSinks[kUsb].machine; }
void jsonNoteRx() { sSinks[kUsb].lastRx = millis(); }
uint32_t jsonBootId() { return sBoot; }

// Une origine invalide ne change rien (jamais de repli silencieux sur l'USB,
// qui echappe a la liste blanche).
void jsonSetOrigin(uint8_t origin) {
  if (origin < kOrigins) sOrigin = origin;
}
uint8_t jsonOrigin() { return sOrigin; }

void jsonCountRejected() {
  sSinks[sOrigin].rejected++;
  sRejets++;
}

bool jsonCadenceOk(uint32_t now) { return sSinks[sOrigin].cadence.allow(now); }

void jsonConfigChanged() { sConfigChanged = true; }

void jsonAfterCommand() {
  const uint32_t now = millis();
  sSinks[sOrigin].lastCmd = now;
  if (!sConfigChanged) return;
  sConfigChanged = false;  // une seule fois par changement, meme si la ligne se perd
  for (uint8_t o = 0; o < kOrigins; o++)
    if (sSinks[o].machine) push(o, Item::Config, now, true);
}

static void printSession(Print &out) {
  const Sink &k = sSinks[kUsb];
  out.printf("Mode machine : %s", k.machine ? "ACTIF" : "coupe ('json 1' pour l'activer)");
  if (k.machine) {
    if (k.leaseS) out.printf(", bail de %u s", k.leaseS);
    else out.print(", sans bail (jusqu'a 'json 0')");
  }
  out.println();
  out.printf("  periodes : etat %lu ms, compteurs %lu ms, reseau %lu ms ; trames %s ; log %s\n",
             (unsigned long)k.periodMs, (unsigned long)k.countersMs, (unsigned long)k.netMs,
             k.frames ? "oui" : "non", k.log ? "oui" : "non");
  out.printf("  lignes   : n = %lu, %lu perdue(s), %lu trop longue(s), %lu ligne(s) de l'hote refusee(s)\n",
             (unsigned long)k.n, (unsigned long)k.lost, (unsigned long)k.tooLong, (unsigned long)k.rejected);
  out.printf("  sonde    : %lu ligne(s) perdue(s), %lu trame(s) sautee(s), %lu rejet(s), toutes sessions\n",
             (unsigned long)sPerdues, (unsigned long)sSautes, (unsigned long)sRejets);
  out.printf("  demarrage : boot %08lX\n", (unsigned long)sBoot);
  out.println("  protocole : docs/PROTOCOLE-JSON.md (v1, profil hotte)");
}

static char *nextWord(char *&p) {
  while (*p == ' ') p++;
  char *w = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = 0;
  return w;
}

static bool parseU32(const char *s, uint32_t *v) {
  if (!*s) return false;
  char *end = nullptr;
  const unsigned long x = strtoul(s, &end, 10);
  if (!end || *end || *s == '-' || *s == '+') return false;
  *v = (uint32_t)x;
  return true;
}

// Periode : 0 (coupe, si permis) ou lo..60000 ms.
static bool setPeriod(char *p, uint32_t lo, bool zeroOk, uint32_t *out, uint32_t *next, uint32_t now) {
  uint32_t v;
  if (!parseU32(nextWord(p), &v) || *nextWord(p) || (!v && !zeroOk) || (v && (v < lo || v > 60000))) return false;
  *out = v;
  *next = now + v;
  return true;
}

void jsonCommand(char *arg, const JsonCmd &c) {
  char *p = arg;
  const char *sub = nextWord(p);
  const uint32_t now = millis();
  const uint8_t o = sOrigin;
  Sink &k = sSinks[o];
  static const char *const kUsage =
      "json [1 [bail 0|10..600] | 0 | etat | hello | ping | periode ms | compteurs ms | reseau ms | "
      "trames 0|1 | log 0|1]";
  // Reseau : bornes propres (liste blanche, spec 8.5), verifiees ici aussi.
  const bool rem = remote(o);
  const char *usage = nullptr;  // non nul : arguments refuses
  bool sessionChanged = false;

  if (!*sub) {
    printSession(Serial);
    replyNow(c, true, "ok", nullptr);
    return;
  }
  if (!strcmp(sub, "1")) {
    uint32_t lease = kLeaseDefault;
    const char *w = nextWord(p);
    const bool bad = *w && (strcmp(w, "bail") || !parseU32(nextWord(p), &lease) || *nextWord(p) ||
                            (lease && (lease < 10 || lease > 600)));
    if (bad || (rem && (lease < 10 || lease > 120))) {
      usage = rem ? "json 1 [bail 10..120] (reseau)" : "json 1 [bail 0|10..600]";
    } else {
      // Idempotent : renvoyer 'json 1' resynchronise (instantane complet).
      enterMachine(o, (uint16_t)lease, now);
      sHeapBlocStale = true;
      pushHello(o, now, true);
      pushState(o, now, true);
      pushCounters(o, now, true);
      pushNet(o, now, true);
      replyAfterQueue(c, true);
      return;
    }
  } else if (!strcmp(sub, "0")) {
    if (*nextWord(p)) {
      usage = "json 0";
    } else {
      replyNow(c, true, "ok", k.machine ? nullptr : "deja en mode humain");
      if (k.machine) leaveMachine(o, false, now);
      else if (!c.hasId) Serial.println("json : mode machine deja coupe");
      return;
    }
  } else if (!strcmp(sub, "etat")) {
    sHeapBlocStale = true;
    pushState(o, now, false);
    pushCounters(o, now, false);
    pushNet(o, now, false);
    replyAfterQueue(c, false);
    return;
  } else if (!strcmp(sub, "hello")) {
    pushHello(o, now, false);
    replyAfterQueue(c, false);
    return;
  } else if (!strcmp(sub, "ping")) {
    // Le bail court deja depuis cette ligne (octets recus, fin de commande).
    replyNow(c, true, "ok", nullptr, true);
    if (!c.hasId) {
      if (k.machine) Serial.printf("json : bail renouvele (%u s)\n", k.leaseS);
      else Serial.println("json : pas de session machine ('json 1')");
    }
    return;
  } else if (!strcmp(sub, "periode")) {
    if (!setPeriod(p, rem ? 2000 : 200, !rem, &k.periodMs, &k.nextEtat, now))
      usage = rem ? "json periode 2000..60000 (reseau)" : "json periode 0|200..60000";
    k.nextHb = now + kHbMs;
    sessionChanged = !usage;
  } else if (!strcmp(sub, "compteurs")) {
    if (!setPeriod(p, rem ? 1000 : 200, true, &k.countersMs, &k.nextCpt, now))
      usage = rem ? "json compteurs 0|1000..60000 (reseau)" : "json compteurs 0|200..60000";
    sessionChanged = !usage;
  } else if (!strcmp(sub, "reseau")) {
    if (!setPeriod(p, rem ? 10000 : 1000, true, &k.netMs, &k.nextNet, now))
      usage = rem ? "json reseau 0|10000..60000 (reseau)" : "json reseau 0|1000..60000";
    sessionChanged = !usage;
  } else if (!strcmp(sub, "trames") || !strcmp(sub, "log")) {
    const char *w = nextWord(p);
    const bool on = !strcmp(w, "1");
    if ((!on && strcmp(w, "0")) || *nextWord(p)) {
      usage = !strcmp(sub, "trames") ? "json trames 0|1" : "json log 0|1";
    } else {
      const bool frames = !strcmp(sub, "trames");
      (frames ? k.frames : k.log) = on;
      if (frames) {
        k.trameNum = 0;  // la reception en cours est jugee de nouveau
        k.trameSautee = false;
      }
      sessionChanged = true;
    }
  } else if (!strcmp(sub, "cle")) {
    // Cle du transport reseau (spec 8.5) : avec le transport UDP (tache 19).
    replyNow(c, false, "refuse", "json cle : transport reseau absent de ce firmware");
    if (!c.hasId) Serial.println("json cle : transport reseau absent de ce firmware (USB seulement)");
    return;
  } else {
    usage = kUsage;
  }

  if (usage) {
    replyNow(c, false, "usage", usage);
    if (!c.hasId) Serial.printf("Usage : %s\n", usage);
    return;
  }
  // Reglage de session change : hello.base le porte.
  if (sessionChanged && k.machine) push(o, Item::HelloBase, now, true);
  replyNow(c, true, "ok", nullptr);
  if (!c.hasId)
    Serial.printf("json : etat %lu ms, compteurs %lu ms, reseau %lu ms, trames %s, log %s\n",
                  (unsigned long)k.periodMs, (unsigned long)k.countersMs, (unsigned long)k.netMs,
                  k.frames ? "oui" : "non", k.log ? "oui" : "non");
}
```

Lancer :

```bash
~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "error|warning|undefined" | sed -E "s/.*(undefined reference to .[A-Za-z]+).*/\1/" | sort -u
```

Attendu : toujours aucune ligne `src/json_mode.cpp` ; il ne manque plus que les événements et le cycle de vie :

```text
collect2: error: ld returned 1 exit status
undefined reference to `jsonBegin
undefined reference to `jsonPoll
undefined reference to `jsonTrame
```

- [ ] **Étape 12 : Ajouter les événements et le cycle de vie**

Ajouter à la fin de `src/json_mode.cpp` (le bloc commence par une ligne vide) :

```cpp

// ===========================================================================
//  Evenements (vers chaque session en mode machine)
// ===========================================================================

void jsonTrame(const capt::Partie &p, bool hasRep, uint32_t rep) {
  const uint32_t now = millis();
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || !k.frames) continue;
    // Les parties d'une reception sont produites ou sautees ensemble (spec
    // 8.5) : le plafond ne se juge qu'a la premiere partie vue de chacune.
    if (p.num != k.trameNum) {
      k.trameNum = p.num;
      k.trameSautee = !k.trameCap.available(now);
    }
    if (k.trameSautee) {
      k.trameCap.skip();  // aucun n consomme ; la suivante produite porte 'sautes'
      sSautes++;
      continue;
    }
    if (!claim()) continue;
    k.trameCap.take();
    trame(sW, k.n, now, p, hasRep, rep, k.trameCap.takeSkipped());
    send(o);
  }
}

bool jsonLog(const char *src, const char *niv, const char *txt) {
  const uint32_t now = millis();
  bool usbTaken = false;  // le texte n'est plus ecrit sur l'USB
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || !k.log) continue;
    if (!k.logCap.available(now)) {
      k.logCap.skip();
      if (o == kUsb) usbTaken = true;
      continue;
    }
    if (!claim()) continue;  // ligne en cours (jamais attendu) : en texte sur l'USB
    k.logCap.take();
    logLine(sW, k.n, now, src, niv, txt, k.logCap.takeSkipped());
    send(o);
    if (o == kUsb) usbTaken = true;
  }
  return usbTaken;
}

// ===========================================================================
//  Cycle de vie
// ===========================================================================

void jsonBegin() {
  // Aucune radio de l'ESP32 n'est active a ce stade : la source d'entropie
  // de l'ADC donne l'aleatoire (sans elle, IDF ne garantit qu'un pseudo-alea).
  bootloader_random_enable();
  sBoot = esp_random();
  bootloader_random_disable();
  sRecAt = millis();
}

// Periode echue : la suivante part de la precedente ; tres en retard (commande
// bloquante), de maintenant.
static bool due(uint32_t &next, uint32_t period, uint32_t now) {
  if (!period || (int32_t)(now - next) < 0) return false;
  next += period;
  if ((int32_t)(now - next) >= 0) next = now + period;
  return true;
}

void jsonPoll() {
  const uint32_t now = millis();
  // Premier tour : pas de mesure (le reste de setup() n'est pas un tour de loop()).
  if (sLoopAt) {
    const uint32_t turn = now - sLoopAt;
    for (Sink &k : sSinks)
      if (turn > k.loopMaxMs) k.loopMaxMs = turn;
  }
  sLoopAt = now ? now : 1;

  if (now - sRecAt >= 1000) {
    const uint32_t r = captureStats().receptions;
    sRecParS = r - sRecPrev;
    sRecPrev = r;
    sRecAt = now;
  }

  // L'USB d'abord ; les sessions reseau, qui partageront la file des
  // datagrammes et son debit, a tour de role.
  static uint8_t sTurn = 0;
  sTurn++;
  for (uint8_t i = 0; i < kOrigins; i++) {
    const uint8_t o = i == 0 || kOrigins < 3 ? i : (uint8_t)(1 + ((i - 1 + sTurn) % (kOrigins - 1)));
    Sink &k = sSinks[o];
    if (k.machine && leaseExpired(now, k.lastRx, k.lastCmd, k.leaseS)) leaveMachine(o, true, now);
    if (k.machine) {
      if (due(k.nextEtat, k.periodMs, now)) pushState(o, now, true);
      if (due(k.nextCpt, k.countersMs, now)) pushCounters(o, now, true);
      if (due(k.nextNet, k.netMs, now)) pushNet(o, now, true);
      if ((!k.periodMs || k.periodMs > kHbMs) && due(k.nextHb, kHbMs, now)) push(o, Item::Heartbeat, now, true);
    }
    drain(o, now);
  }
}
```

- [ ] **Étape 13 : Compiler et relancer tous les tests**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|RAM:|Flash:|SUCCESS|FAILED"`
Attendu :

```text
RAM:   [=         ]  10.1% (used 33084 bytes from 327680 bytes)
Flash: [==        ]  24.2% (used 3171xx bytes from 1310720 bytes)
========================= [SUCCESS] Took ... seconds =========================
```
Aucun `warning`. Les octets de flash varient un peu avec le hash de version.

Lancer : `sh tools/tests/test_hote.sh | tail -1`
Attendu : « tests hote : OK »

Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « Ran 97 tests », puis « OK » (toutes les suites Python, `test_json_check` compris)

- [ ] **Étape 14 : Commit**

```bash
git add src/json_mode.h src/json_mode.cpp src/sonde.h src/main.cpp src/cli.cpp tools/json_check.py tools/tests/test_json_check.py tools/tests/test_hote.sh docs/PROTOCOLE-JSON.md
git commit -m "Ajouter le mode machine sur l'USB et le profil hotte du protocole" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 12 : `tools/serie_enregistre.py` : enregistrer une session machine par l'USB

**Fichiers :**
- Créer : `tools/serie_enregistre.py`
- Créer : `tools/tests/test_serie_enregistre.py`
- Tester : `tools/tests/test_serie_enregistre.py`

**Interfaces :**
- Consomme :
  - le protocole de la tâche 11 : `json 1 bail 0` (réponse `fin` avec `bail_s` 0), `json 0` (réponse puis message `fin`), lignes `RS + JSON + LF`, préfixe `id=<n>`, une commande en vol à la fois (ScreenBar §6.5) ;
  - l'ouverture sûre du C6 de `benq@c58a506:tools/halo_udp.py` (`open_serial`) : `TIOCEXCL`, puis **un seul** `TIOCMSET` à 0 (DTR = RTS = 0 ensemble), `HUPCL` retiré, 8N1 brut ;
  - le format `.jsonl` du contrat : `{"rx_ms": <int>, "de": "usb", "l": <objet reçu tel quel>}` ;
  - dans les tests : `capture_fmt.lire_jsonl`, `capture_fmt.parties` (tâche 3), `json_check.main(argv)` (tâche 11).
- Produit :
  - la commande `python3 tools/serie_enregistre.py <port> <scenario> [commande ...] [--duree s] [--dossier d]` : fichier `<dossier>/AAAA-MM-JJ-hhmm-<scenario>.jsonl` (défaut `logs/` du dépôt) et résumé dans `<dossier>/live.log`, une ligne `AAAA-MM-JJ hh:mm:ss usb <scenario> <etat> <duree> s : <resume>` toutes les 5 s (`etat` = `en cours`) et à la fin (`fin`, `interrompu` ou `port ferme (...)`) ;
  - pour `hotte_udp.py enregistre` (tâche 20), réutilisables : `nom_fichier(scenario: str, quand: datetime) -> str`, `class Bilan` (`noter(obj)`, `resume() -> str`, champs `lignes`, `abimees`, `texte`, `trous`, `perdues`, `reculs`, `par_type`), `journaliser(journal, horloge, scenario, etat, duree_s, bilan, afficher)` ;
  - le reste : `ouvrir_port(chemin) -> int`, `lecteur(fd)`, `ecrivain(fd)`, `class PortFerme(Exception)`, `class Decoupe(resynchro=True)` avec `feed(octets) -> list[(genre, octets, objet)]` (`genre` : `machine`, `abimee`, `texte`), `enregistrement(rx_ms, brut) -> str`, `class Commandes(lignes, premier_id=1)` (`a_envoyer(maintenant)`, `recu(obj)`, `fini()`, `resultats`, `prochain_id`), `enregistrer(lire, ecrire, sortie, journal, scenario, commandes, duree_s, horloge=time.time, afficher=print) -> Bilan`, `main(argv=None)`.

**Comportement :** Ctrl-U, puis `id=1 json 1 bail 0` et les commandes demandées, une à la fois (la suivante part après la réponse `fin`, ou 3 s sans elle) ; chaque ligne machine reçue est écrite telle quelle dans le `.jsonl` (texte, lignes abîmées et reste d'avant le premier LF ne le sont pas, mais sont comptés) ; à la fin (`--duree` écoulée ou Ctrl-C), `json 0` part et l'outil attend le message `fin` (1 s au plus). Si le port disparaît (sonde redémarrée), l'enregistrement s'arrête proprement, sans `json 0`. Les tests n'ouvrent aucun port : une fausse sonde répond aux commandes, `ouvrir_port` est vérifiée avec des `ioctl` simulés, et `main()` tourne sur une paire de sockets.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_serie_enregistre.py` :

```python
#!/usr/bin/env python3
"""Tests de tools/serie_enregistre.py sur un flux simule, sans port reel :
python3 -m unittest discover -s tools/tests -p test_serie_enregistre.py -v"""
import contextlib
import datetime
import io
import json
import os
import socket
import struct
import sys
import tempfile
import termios
import threading
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import capture_fmt  # noqa: E402
import json_check  # noqa: E402
import serie_enregistre  # noqa: E402

RS = b"\x1e"


def compact(obj):
    return json.dumps(obj, separators=(",", ":")).encode("ascii")


class FausseSonde:
    """Repond aux lignes de l'hote comme la sonde en mode machine (docs/PROTOCOLE-JSON.md)."""

    def __init__(self, sauter_n=None, interrompre_a=None, fermer_a=None):
        self.t = 1000.0
        self.n = 0
        self.machine = False
        self.en_attente = b"reste d'une ligne d'avant\n> "  # avant la resynchronisation
        self.recu = []
        self.sauter_n = sauter_n          # n jamais emis (ligne perdue cote sonde)
        self.interrompre_a = interrompre_a  # Ctrl-C simule a cette heure
        self.fermer_a = fermer_a            # port qui disparait (sonde redemarree) a cette heure

    def horloge(self):
        return self.t

    def ligne(self, t, **champs):
        obj = {"v": 1, "t": t, "n": self.n, "ms": int(self.t * 1000) % 2**32}
        obj.update(champs)
        self.n += 1
        if obj["n"] == self.sauter_n:
            return b""
        return RS + compact(obj) + b"\n"

    def reponse(self, ident, cmd, **champs):
        return self.ligne("reponse", id=ident, etape="fin", cmd=cmd, ok=True, code="ok", duree_ms=1, **champs)

    def ecrire(self, octets):
        self.recu.append(octets)
        for brut in octets.split(b"\n")[:-1]:
            texte = brut.decode("ascii")
            if not texte.startswith("id="):
                continue
            ident, cmd = texte[3:].split(" ", 1)
            ident = int(ident)
            if cmd == "json 1 bail 0":
                self.machine = True
                self.en_attente += self.ligne("hb", boot="3FA2C901", up_s=1, json_perdus=0)
                self.en_attente += self.reponse(ident, cmd, bail_s=0, up_s=1)
            elif cmd == "capture tout":
                self.en_attente += self.reponse(ident, cmd)
                self.en_attente += self.ligne("config", capture={"gpio": 6, "resol_hz": 1000000, "filtre_us": 1,
                                                                 "silence_us": 5000, "mode": "tout", "inverse": True})
            elif cmd == "json 0":
                self.en_attente += self.reponse(ident, cmd)
                self.en_attente += self.ligne("fin", cause="commande") + b"> "
                self.machine = False

    def lire(self, delai):
        if self.interrompre_a is not None and self.t >= self.interrompre_a:
            self.interrompre_a = None
            raise KeyboardInterrupt
        if self.fermer_a is not None and self.t >= self.fermer_a:
            raise serie_enregistre.PortFerme("port ferme")
        self.t += delai
        if self.machine:
            self.en_attente += self.ligne("trame", num=self.n, part=0, fin=True, t_us=int(self.t * 1e6), niv0="bas",
                                          dur_us=[1500, 750, 750, 2250], debord=False)
        octets, self.en_attente = self.en_attente, b""
        return octets

    def commandes(self):
        return [l.decode() for o in self.recu for l in o.split(b"\n") if l.startswith(b"id=")]


def sortie_muette(*_):
    pass


def lire_texte(chemin):
    with open(chemin, encoding="utf-8") as f:
        return f.read()


class Decoupage(unittest.TestCase):
    def test_machine_texte_abimee(self):
        d = serie_enregistre.Decoupe(resynchro=False)
        hb = b'{"v":1,"t":"hb","n":3,"ms":5,"boot":"3FA2C901","up_s":1,"json_perdus":0}'
        fin = b'{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}'
        flux = (b"> json 1\r\n" + RS + hb + b"\n" + b"E (123) tag: un log coupe " + RS + fin + b"\r\n"
                + RS + b'{"v":1,"t":"hb","n"\n' + RS + b'{"v":2,"t":"hb","n":1,"ms":1}\n' + b"pas fini")
        ev = []
        for i in range(0, len(flux), 7):  # arrive par morceaux de 7 octets
            ev += d.feed(flux[i:i + 7])
        self.assertEqual([g for g, _, _ in ev], ["texte", "machine", "texte", "machine", "abimee", "abimee"])
        self.assertEqual(ev[0][1], b"> json 1")
        self.assertEqual(ev[1][1], hb)
        self.assertEqual(ev[1][2]["t"], "hb")
        self.assertEqual(ev[2][1], b"E (123) tag: un log coupe ")  # texte avant le RS
        self.assertEqual(ev[3][1], fin)  # dernier RS de la ligne, CR retire

    def test_resynchronisation(self):
        d = serie_enregistre.Decoupe()
        ev = d.feed(b'1,"ms":5}\n' + RS + b'{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}\n')
        self.assertEqual([g for g, _, _ in ev], ["machine"])  # le reste d'avant le premier LF est jete

    def test_ligne_sans_fin(self):
        d = serie_enregistre.Decoupe(resynchro=False)
        ev = d.feed(b"x" * 3000)
        self.assertEqual([(g, len(b)) for g, b, _ in ev], [("texte", 3000)])
        ev = d.feed(RS + b'{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}\n')
        self.assertEqual([g for g, _, _ in ev], ["machine"])


class Fichiers(unittest.TestCase):
    def test_nom_fichier(self):
        quand = datetime.datetime(2026, 9, 27, 14, 3, 59)
        self.assertEqual(serie_enregistre.nom_fichier("krona", quand), "2026-09-27-1403-krona.jsonl")
        self.assertEqual(serie_enregistre.nom_fichier("banc wtc/1", quand), "2026-09-27-1403-banc-wtc-1.jsonl")
        self.assertEqual(serie_enregistre.nom_fichier("//", quand), "2026-09-27-1403-session.jsonl")

    def test_enregistrement_jsonl(self):
        brut = b'{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}'
        self.assertEqual(serie_enregistre.enregistrement(1790000000123, brut),
                         '{"rx_ms":1790000000123,"de":"usb","l":{"v":1,"t":"fin","n":4,"ms":6,"cause":"bail"}}\n')


class Commandes(unittest.TestCase):
    def test_une_commande_en_vol(self):
        c = serie_enregistre.Commandes(["json 1 bail 0", "capture tout", "seuils 1 8000"])
        self.assertEqual(c.a_envoyer(0.0), b"id=1 json 1 bail 0\n")
        self.assertIsNone(c.a_envoyer(1.0))  # la premiere est en vol
        self.assertIsNone(c.recu({"t": "reponse", "id": 1, "etape": "debut", "code": "en_cours"}))
        self.assertIsNone(c.recu({"t": "trame", "n": 5}))
        cr = c.recu({"t": "reponse", "id": 1, "etape": "fin", "ok": True, "code": "ok", "cmd": "json 1 bail 0"})
        self.assertEqual(cr, "id=1 json 1 bail 0 : ok")
        self.assertEqual(c.a_envoyer(1.1), b"id=2 capture tout\n")
        # Sans reponse fin en 3 s : la suivante part, la commande est notee sans reponse.
        self.assertIsNone(c.a_envoyer(4.0))
        self.assertEqual(c.a_envoyer(4.2), b"id=3 seuils 1 8000\n")
        cr = c.recu({"t": "reponse", "id": 3, "etape": "fin", "ok": False, "code": "usage", "cmd": "seuils 1 8000",
                     "msg": "seuils : ..."})
        self.assertEqual(cr, "id=3 seuils 1 8000 : usage (seuils : ...)")
        self.assertTrue(c.fini())
        self.assertEqual(c.resultats, ["id=1 json 1 bail 0 : ok", "id=2 capture tout : sans reponse",
                                       "id=3 seuils 1 8000 : usage (seuils : ...)"])
        self.assertEqual(c.prochain_id, 4)


class Ouverture(unittest.TestCase):
    def test_dtr_rts_a_zero_en_un_seul_appel(self):
        appels = []
        attrs = [termios.ICRNL | termios.IXON, termios.OPOST, termios.HUPCL | termios.CS7 | termios.PARENB,
                 termios.ECHO | termios.ICANON, termios.B9600, termios.B9600, [0] * 20]
        with mock.patch.object(serie_enregistre.os, "open", return_value=42) as ouvre, \
                mock.patch.object(serie_enregistre.fcntl, "ioctl",
                                  side_effect=lambda fd, req, arg=0: appels.append((fd, req, arg))), \
                mock.patch.object(serie_enregistre.termios, "tcgetattr", return_value=attrs), \
                mock.patch.object(serie_enregistre.termios, "tcsetattr") as regle:
            self.assertEqual(serie_enregistre.ouvrir_port("/dev/cu.usbmodem101"), 42)
        flags = ouvre.call_args[0][1]
        self.assertEqual(flags & (os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK), os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        # TIOCEXCL puis UN SEUL TIOCMSET a 0 (DTR = RTS = 0 ensemble) ; jamais TIOCMBIS ni TIOCMBIC.
        self.assertEqual([(fd, req) for fd, req, _ in appels], [(42, termios.TIOCEXCL), (42, termios.TIOCMSET)])
        self.assertEqual(appels[1][2], struct.pack("I", 0))
        iflag, oflag, cflag, lflag, ispeed, ospeed, _ = regle.call_args[0][2]
        self.assertFalse(cflag & termios.HUPCL)  # la fermeture ne touche plus DTR ni RTS
        self.assertEqual(cflag & (termios.CSIZE | termios.PARENB | termios.CLOCAL | termios.CREAD),
                         termios.CS8 | termios.CLOCAL | termios.CREAD)
        self.assertFalse(iflag & (termios.ICRNL | termios.IXON) or oflag & termios.OPOST
                         or lflag & (termios.ECHO | termios.ICANON))
        self.assertEqual((ispeed, ospeed), (termios.B115200, termios.B115200))


class Session(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)
        self.jsonl = os.path.join(self.dossier.name, "s.jsonl")
        self.journal = os.path.join(self.dossier.name, "live.log")

    def lancer(self, sonde, duree_s, commandes=("capture tout",)):
        with open(self.jsonl, "w", encoding="ascii") as sortie:
            return serie_enregistre.enregistrer(sonde.lire, sonde.ecrire, sortie, self.journal, "essai",
                                                list(commandes), duree_s, horloge=sonde.horloge,
                                                afficher=sortie_muette)

    def test_enregistrement_complet(self):
        sonde = FausseSonde(sauter_n=20)
        bilan = self.lancer(sonde, 12.0)
        # Ctrl-U d'abord, puis une commande a la fois, 'json 0' a la fin.
        self.assertEqual(sonde.recu[0], b"\x15\n")
        self.assertEqual(sonde.commandes(), ["id=1 json 1 bail 0", "id=2 capture tout", "id=3 json 0"])
        lignes = lire_texte(self.jsonl).splitlines()
        rec = json.loads(lignes[0])
        self.assertEqual(list(rec), ["rx_ms", "de", "l"])
        self.assertEqual(rec["de"], "usb")
        self.assertEqual(rec["rx_ms"], 1000200)  # heure du Mac en ms (horloge simulee)
        self.assertEqual(rec["l"]["t"], "hb")
        dernier = json.loads(lignes[-1])["l"]
        self.assertEqual((dernier["t"], dernier["cause"], dernier["n"]), ("fin", "commande", sonde.n - 1))
        # Lisible par capture_fmt, conforme au profil (json_check --jsonl) avec un trou de n.
        objets = list(capture_fmt.lire_jsonl(self.jsonl))
        self.assertEqual(len(objets), len(lignes))
        self.assertGreater(len(capture_fmt.parties(objets)), 50)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(json_check.main(["-q", self.jsonl]), 0)
        self.assertIn("0 erreur(s)", out.getvalue())
        self.assertIn("n : 1 trou(s) (1 ligne(s) perdue(s)", out.getvalue())
        # Bilan : lignes, trou, texte (invites), resultats des commandes.
        self.assertEqual(bilan.lignes, len(lignes))
        self.assertEqual((bilan.trous, bilan.perdues), (1, 1))
        self.assertEqual(bilan.par_type["reponse"], 3)
        self.assertEqual(bilan.abimees, 0)
        self.assertGreaterEqual(bilan.texte, 1)
        # live.log : un resume toutes les 5 s, puis le resume final.
        journal = lire_texte(self.journal).splitlines()
        self.assertEqual(len(journal), 3)
        self.assertTrue(all(" usb essai " in l for l in journal), journal)
        self.assertIn("fin", journal[-1])
        self.assertIn(f"{len(lignes)} lignes", journal[-1])
        self.assertIn("1 trou", journal[-1])

    def test_ctrl_c_ferme_la_session(self):
        sonde = FausseSonde(interrompre_a=1003.0)
        bilan = self.lancer(sonde, None)
        self.assertEqual(sonde.commandes()[-1], "id=3 json 0")
        self.assertFalse(sonde.machine)
        self.assertGreater(bilan.lignes, 5)
        self.assertIn("interrompu", lire_texte(self.journal))

    def test_port_ferme(self):
        sonde = FausseSonde(fermer_a=1002.0)
        bilan = self.lancer(sonde, 10.0)
        self.assertNotIn("json 0", " ".join(sonde.commandes()))  # plus de port : rien a envoyer
        self.assertGreater(bilan.lignes, 5)
        self.assertIn("port ferme", lire_texte(self.journal).splitlines()[-1])


class PortReel(unittest.TestCase):
    """main() de bout en bout sur une paire de sockets a la place du port serie."""

    def test_main(self):
        a, b = socket.socketpair()
        self.addCleanup(a.close)
        self.addCleanup(b.close)
        recu = []

        def sonde():
            tampon = b""
            n = 0
            while True:
                try:
                    morceau = b.recv(4096)
                except OSError:  # socket fermee par le nettoyage du test
                    return
                if not morceau:
                    return
                tampon += morceau
                while b"\n" in tampon:
                    ligne, tampon = tampon.split(b"\n", 1)
                    recu.append(ligne)
                    if ligne.startswith(b"id="):
                        ident, cmd = ligne[3:].split(b" ", 1)
                        obj = {"v": 1, "t": "reponse", "n": n, "ms": 10 + n, "id": int(ident), "etape": "fin",
                               "cmd": cmd.decode(), "ok": True, "code": "ok", "duree_ms": 0}
                        b.sendall(b"\n" + RS + compact(obj) + b"\n")
                        n += 1
                        if cmd == b"json 0":
                            fin = {"v": 1, "t": "fin", "n": n, "ms": 10 + n, "cause": "commande"}
                            b.sendall(RS + compact(fin) + b"\n> ")
                            n += 1

        fil = threading.Thread(target=sonde, daemon=True)
        fil.start()
        with tempfile.TemporaryDirectory() as dossier, \
                mock.patch.object(serie_enregistre, "ouvrir_port", return_value=os.dup(a.fileno())), \
                contextlib.redirect_stdout(io.StringIO()) as out:
            code = serie_enregistre.main(["/dev/cu.usbmodem101", "banc", "capture tout", "--duree", "1",
                                          "--dossier", dossier])
            fichiers = os.listdir(dossier)
            jsonl = [f for f in fichiers if f.endswith(".jsonl")]
            self.assertEqual(len(jsonl), 1)
            self.assertTrue(jsonl[0].endswith("-banc.jsonl"))
            self.assertIn("live.log", fichiers)
            lignes = lire_texte(os.path.join(dossier, jsonl[0])).splitlines()
        self.assertEqual(code, 0)
        self.assertEqual(recu[:4], [b"\x15", b"id=1 json 1 bail 0", b"id=2 capture tout", b"id=3 json 0"])
        self.assertEqual([json.loads(l)["l"].get("id") for l in lignes], [1, 2, 3, None])
        self.assertIn("id=2 capture tout : ok", out.getvalue())


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_serie_enregistre.py -v`
Attendu : ÉCHEC avec « ModuleNotFoundError: No module named 'serie_enregistre' »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `tools/serie_enregistre.py` :

```python
#!/usr/bin/env python3
"""Enregistre une session machine de la sonde par l'USB (docs/PROTOCOLE-JSON.md).

  python3 tools/serie_enregistre.py <port> <scenario> [commande ...] [--duree s] [--dossier d]

Ouvre le port serie du C6 sans le redemarrer : DTR = RTS = 0 en un seul appel,
HUPCL retire (protocole de la ScreenBar, sections 3.1 et 3.2 ; RTS=1 DTR=0
redemarre le C6). Envoie Ctrl-U (reste de ligne efface), passe la sonde en
mode machine ('json 1 bail 0'), puis chaque commande donnee (entre
guillemets, sans id=), une a la fois : la suivante part apres la reponse fin,
ou apres 3 s sans elle.

Chaque ligne machine recue est ecrite dans <dossier>/AAAA-MM-JJ-hhmm-<scenario>.jsonl :
  {"rx_ms": <heure du Mac en ms>, "de": "usb", "l": <objet recu tel quel>}
Un resume part dans <dossier>/live.log toutes les 5 s et a la fin (a suivre
avec 'tail -f logs/live.log') : lignes par type, trous de n, compteurs de la
sonde.

Fin : --duree ecoulee, ou Ctrl-C. 'json 0' est alors envoye : la sonde repasse
en mode humain (sans bail, elle y resterait sinon jusqu'au redemarrage).
Le port doit etre libre (ni 'pio device monitor', ni l'app).

Exemples :
  python3 tools/serie_enregistre.py /dev/cu.usbmodem101 wtc --duree 60
  python3 tools/serie_enregistre.py /dev/cu.usbmodem101 krona-chgt "capture changements" --duree 600
"""
import argparse
import datetime
import fcntl
import json
import os
import re
import select
import struct
import sys
import termios
import time
from collections import Counter

sys.dont_write_bytecode = True

RS = 0x1E
MAX_SANS_LF = 2048   # au-dela, le tampon part en texte (protocole de la ScreenBar, 2.4)
DELAI_REPONSE_S = 3.0
RESUME_S = 5.0
DOSSIER = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "logs")


class PortFerme(Exception):
    """Le port a disparu (sonde redemarree, cable tire)."""


# ---------------------------------------------------------------------------
#  Port serie
# ---------------------------------------------------------------------------


def ouvrir_port(chemin):
    """Ouverture sure du C6 (comme halo_udp.py cle de la ScreenBar, c58a506)."""
    fd = os.open(chemin, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    fcntl.ioctl(fd, termios.TIOCEXCL)
    # DTR = RTS = 0 dans un seul appel (RTS=1 DTR=0, meme un instant, redemarre le C6).
    fcntl.ioctl(fd, termios.TIOCMSET, struct.pack("I", 0))
    iflag, oflag, cflag, lflag, _, _, cc = termios.tcgetattr(fd)
    iflag &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP | termios.INLCR | termios.IGNCR
               | termios.ICRNL | termios.IXON)
    oflag &= ~termios.OPOST
    lflag &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)
    cflag &= ~(termios.CSIZE | termios.PARENB | termios.HUPCL)
    cflag |= termios.CS8 | termios.CLOCAL | termios.CREAD
    termios.tcsetattr(fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, termios.B115200, termios.B115200, cc])
    return fd


def lecteur(fd):
    """lire(delai_s) -> octets recus (b"" si rien pendant le delai)."""

    def lire(delai):
        try:
            prets, _, _ = select.select([fd], [], [], delai)
            if not prets:
                return b""
            octets = os.read(fd, 4096)
        except BlockingIOError:
            return b""
        except OSError as e:
            raise PortFerme(e.strerror) from e
        if not octets:
            raise PortFerme("fin de fichier")
        return octets

    return lire


def ecrivain(fd):
    def ecrire(octets):
        while octets:
            try:
                octets = octets[os.write(fd, octets):]
            except BlockingIOError:
                select.select([], [fd], [], 0.1)
            except OSError as e:
                raise PortFerme(e.strerror) from e

    return ecrire


# ---------------------------------------------------------------------------
#  Decoupage du flux (protocole de la ScreenBar, 2.4)
# ---------------------------------------------------------------------------


class Decoupe:
    """Octets du port -> evenements (genre, octets, objet) :
    ('machine', JSON sans RS ni CR, objet), ('abimee', octets apres le RS, None),
    ('texte', ligne ou texte avant le RS, None)."""

    def __init__(self, resynchro=True):
        self.tampon = b""
        self.synchro = not resynchro  # jeter ce qui precede le premier LF (reste d'une ligne ancienne)

    def feed(self, octets):
        self.tampon += octets
        ev = []
        if not self.synchro:
            i = self.tampon.find(b"\n")
            if i < 0:
                self.tampon = b""
                return ev
            self.tampon = self.tampon[i + 1:]
            self.synchro = True
        while b"\n" in self.tampon:
            ligne, self.tampon = self.tampon.split(b"\n", 1)
            if ligne.endswith(b"\r"):
                ligne = ligne[:-1]
            ev += self.ligne(ligne)
        if len(self.tampon) > MAX_SANS_LF:
            ev.append(("texte", self.tampon, None))
            self.tampon = b""
        return ev

    @staticmethod
    def ligne(ligne):
        i = ligne.rfind(bytes([RS]))
        if i < 0:
            return [("texte", ligne, None)] if ligne.strip() else []
        ev = [("texte", ligne[:i], None)] if ligne[:i].strip() else []
        brut = ligne[i + 1:]
        try:
            obj = json.loads(brut.decode("ascii"))
        except (UnicodeDecodeError, ValueError):
            obj = None
        valide = (isinstance(obj, dict) and obj.get("v") == 1 and not isinstance(obj.get("v"), bool)
                  and isinstance(obj.get("t"), str) and isinstance(obj.get("n"), int)
                  and not isinstance(obj.get("n"), bool))
        ev.append(("machine", brut, obj) if valide else ("abimee", brut, None))
        return ev


# ---------------------------------------------------------------------------
#  Fichiers
# ---------------------------------------------------------------------------


def nom_fichier(scenario, quand):
    propre = re.sub(r"[^A-Za-z0-9_.-]+", "-", scenario).strip("-.") or "session"
    return f"{quand:%Y-%m-%d-%H%M}-{propre}.jsonl"


def enregistrement(rx_ms, brut):
    """Une ligne .jsonl : l'objet recu tel quel (octets ASCII du firmware)."""
    return '{"rx_ms":%d,"de":"usb","l":%s}\n' % (rx_ms, brut.decode("ascii"))


class Bilan:
    def __init__(self):
        self.lignes = self.abimees = self.texte = 0
        self.trous = self.perdues = self.reculs = 0
        self.par_type = Counter()
        self.dernier_n = None
        self.sonde = {}         # dernier compteurs.sonde
        self.json_perdus = None  # dernier etat.sys.sys.json_perdus

    def noter(self, obj):
        self.lignes += 1
        t, n = obj["t"], obj["n"]
        self.par_type[t] += 1
        if self.dernier_n is not None:
            if n > self.dernier_n + 1:
                self.trous += 1
                self.perdues += n - self.dernier_n - 1
            elif n <= self.dernier_n:
                self.reculs += 1
        self.dernier_n = n
        if t == "compteurs" and obj.get("bloc") == "sonde":
            self.sonde = obj
        elif t == "etat" and obj.get("bloc") == "sys" and isinstance(obj.get("sys"), dict):
            self.json_perdus = obj["sys"].get("json_perdus")

    def resume(self):
        types = ", ".join(f"{t} {c}" for t, c in sorted(self.par_type.items()))
        s = (f"{self.lignes} lignes ({types}) ; n : {self.trous} trou(s), {self.perdues} perdue(s), "
             f"{self.reculs} recul(s) ; {self.abimees} abimee(s), {self.texte} ligne(s) de texte")
        cles = ("receptions", "debord", "lignes_perdues", "sautes", "rep")
        if self.sonde:
            s += " ; sonde : " + ", ".join(f"{k} {self.sonde.get(k)}" for k in cles)
        if self.json_perdus is not None:
            s += f", json_perdus {self.json_perdus}"
        return s


class Commandes:
    """Une commande en vol a la fois (protocole de la ScreenBar, 6.5) : la
    suivante part apres la reponse fin, ou apres DELAI_REPONSE_S sans elle."""

    def __init__(self, lignes, premier_id=1):
        self.file = list(lignes)
        self.prochain_id = premier_id
        self.en_vol = None  # (id, commande, heure d'envoi)
        self.resultats = []

    def a_envoyer(self, maintenant):
        if self.en_vol and maintenant - self.en_vol[2] >= DELAI_REPONSE_S:
            self.resultats.append(f"id={self.en_vol[0]} {self.en_vol[1]} : sans reponse")
            self.en_vol = None
        if self.en_vol or not self.file:
            return None
        cmd = self.file.pop(0)
        self.en_vol = (self.prochain_id, cmd, maintenant)
        self.prochain_id += 1
        return f"id={self.en_vol[0]} {cmd}\n".encode("ascii")

    def recu(self, obj):
        """Compte rendu lisible si obj est la reponse fin de la commande en vol."""
        if (obj.get("t") != "reponse" or obj.get("etape") != "fin" or not self.en_vol
                or obj.get("id") != self.en_vol[0]):
            return None
        cr = f"id={self.en_vol[0]} {self.en_vol[1]} : {obj.get('code')}"
        if obj.get("msg"):
            cr += f" ({obj['msg']})"
        self.resultats.append(cr)
        self.en_vol = None
        return cr

    def fini(self):
        return not self.file and not self.en_vol


def journaliser(journal, horloge, scenario, etat, duree_s, bilan, afficher):
    quand = datetime.datetime.fromtimestamp(horloge())
    ligne = f"{quand:%Y-%m-%d %H:%M:%S} usb {scenario} {etat} {duree_s:.0f} s : {bilan.resume()}"
    with open(journal, "a", encoding="utf-8") as f:
        f.write(ligne + "\n")
    afficher(ligne)


# ---------------------------------------------------------------------------
#  Session
# ---------------------------------------------------------------------------


def enregistrer(lire, ecrire, sortie, journal, scenario, commandes, duree_s, horloge=time.time, afficher=print):
    """Session complete sur un port deja ouvert. lire(delai_s) -> octets ;
    ecrire(octets) ; sortie : fichier .jsonl ouvert en texte ; journal : chemin
    de live.log ; duree_s None : jusqu'a Ctrl-C. Rend le Bilan."""
    decoupe, bilan = Decoupe(), Bilan()
    cmds = Commandes(["json 1 bail 0"] + list(commandes))

    def traiter(octets):
        vu_fin = False
        for genre, brut, obj in decoupe.feed(octets):
            if genre == "machine":
                sortie.write(enregistrement(int(round(horloge() * 1000)), brut))
                bilan.noter(obj)
                cr = cmds.recu(obj)
                if cr:
                    afficher(cr)
                vu_fin = vu_fin or obj["t"] == "fin"
            elif genre == "abimee":
                bilan.abimees += 1
            else:
                bilan.texte += 1
        return vu_fin

    debut = horloge()
    prochain_resume = debut + RESUME_S
    etat = "fin"
    try:
        ecrire(b"\x15\n")  # Ctrl-U : efface un reste de ligne laisse par une session precedente
        while duree_s is None or horloge() - debut < duree_s:
            ligne = cmds.a_envoyer(horloge())
            if ligne:
                ecrire(ligne)
            traiter(lire(0.2))
            if horloge() >= prochain_resume:
                sortie.flush()
                journaliser(journal, horloge, scenario, "en cours", horloge() - debut, bilan, afficher)
                prochain_resume += RESUME_S
    except KeyboardInterrupt:
        etat = "interrompu"
    except PortFerme as e:
        etat = f"port ferme ({e})"
    if not etat.startswith("port ferme"):
        try:
            ecrire(f"id={cmds.prochain_id} json 0\n".encode("ascii"))
            limite = horloge() + 1.0
            while horloge() < limite:
                if traiter(lire(0.1)):
                    break  # message fin : la sonde est en mode humain
        except PortFerme as e:
            etat += f", port ferme ({e})"
    sortie.flush()
    journaliser(journal, horloge, scenario, etat, horloge() - debut, bilan, afficher)
    return bilan


def main(argv=None):
    ap = argparse.ArgumentParser(description="Enregistre une session machine de la sonde par l'USB (.jsonl).")
    ap.add_argument("port", help="port serie du C6, ex. /dev/cu.usbmodem101")
    ap.add_argument("scenario", help="nom du scenario (nom du fichier)")
    ap.add_argument("commandes", nargs="*", help="commandes envoyees apres 'json 1 bail 0', sans id=")
    ap.add_argument("--duree", type=float, default=None, help="duree en secondes (defaut : jusqu'a Ctrl-C)")
    ap.add_argument("--dossier", default=DOSSIER, help="dossier des enregistrements (defaut : logs/ du depot)")
    args = ap.parse_args(argv)
    os.makedirs(args.dossier, exist_ok=True)
    chemin = os.path.join(args.dossier, nom_fichier(args.scenario, datetime.datetime.now()))
    journal = os.path.join(args.dossier, "live.log")
    try:
        fd = ouvrir_port(args.port)
    except OSError as e:
        raise SystemExit(f"{args.port} : {e.strerror} (port tenu par 'pio device monitor' ou l'app ?)")
    try:
        with open(chemin, "w", encoding="ascii") as sortie:
            print(f"enregistrement dans {chemin} (resume dans {journal})")
            enregistrer(lecteur(fd), ecrivain(fd), sortie, journal, args.scenario, args.commandes, args.duree)
    finally:
        os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_serie_enregistre.py -v`
Attendu : « Ran 11 tests » (environ 1 s : `test_main` enregistre pendant 1 s réelle) puis « OK »

Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « Ran 108 tests », puis « OK »

Lancer : `python3 tools/serie_enregistre.py /dev/cu.inexistant essai --duree 1 --dossier "$TMPDIR/essai-serie"; echo "code $?"`
Attendu : « /dev/cu.inexistant : No such file or directory (port tenu par 'pio device monitor' ou l'app ?) » puis « code 1 » (aucun port n'est ouvert par cette vérification)

- [ ] **Étape 5 : Commit**

```bash
git add tools/serie_enregistre.py tools/tests/test_serie_enregistre.py
git commit -m "Ajouter l'enregistrement d'une session machine par l'USB" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 12b : `tools/verse_capture.py` : verser les captures de référence

**Fichiers :**
- Créer : `tools/verse_capture.py`
- Créer : `tools/tests/test_verse_capture.py`
- Créer : `captures/README.md`
- Tester : `tools/tests/test_verse_capture.py`, `python3 -m unittest discover -s tools/tests`

**Interfaces :**
- Consomme :
  - le format `.jsonl` du contrat (`serie_enregistre.py`, tâche 12 ; `hotte_udp.py enregistre`, tâche 20) : `{"rx_ms": <int>, "de": "usb" | "<ip>", "l": <objet reçu tel quel>}` ;
  - `json_check.main(argv)` (tâche 11), appelé avec `["--jsonl", <capture>]` : 0 si la capture est conforme au profil hotte ;
  - le bloc `reseau` `ip` de la tâche 19 : son objet `wifi` (`connecte`, `rssi_dbm`, `ip`, `pertes`) ne porte ni SSID ni mot de passe ;
  - `docs/RECONNAISSANCE.md` (tâche 1) nomme déjà l'outil : `python3 tools/verse_capture.py logs/<fichier>.jsonl etape5-<scénario>` (et `etape5b-<scénario>`).
- Produit :
  - la commande `python3 tools/verse_capture.py <capture.jsonl> <nom> [--force]` : copie telle quelle vers `captures/<nom>.jsonl`, ligne de provenance dans `captures/README.md` ; codes de sortie 0 (versée), 1 (refusée), 2 (usage) ;
  - `CAPTURES` (dossier, remplacé par les tests), `refus(ligne) -> list[str]` (raisons, vide si rien de secret), `provenance(chemin, nom) -> str`, `noter(readme, nom, ligne)`, `main(argv=None)` ;
  - `captures/README.md` : les règles (aucune clé, aucun SSID ni mot de passe), l'usage de l'outil, le tableau de provenance, dernière section du fichier.

**Comportement :** dans cet ordre, et sans rien écrire au moindre refus : nom (lettres, chiffres, `-`, `_`, sans `.jsonl`) et `captures/README.md` présent, sinon code 2 ; capture lisible, sinon code 2 ; nom pas encore versé, sauf `--force`, sinon code 1 ; chaque ligne passée à `refus`, code 1 si l'une a une raison (numéro et raison affichés, **jamais la ligne**) ; capture non vide et acceptée par `json_check.py --jsonl`, sinon code 1. Puis copie, ligne de provenance (remplacée avec `--force`), rappel du `git add`.

**Décisions de cette tâche :**
- **Raisons de refus** (spec §9 : « aucune clé ») : un champ `cle`, `ssid` ou `mdp` à toute profondeur ; une chaîne de 64 caractères hexadécimaux, cherchée dans le texte brut **et** dans les chaînes et noms de champ décodés (un caractère échappé en JSON ne la cache pas) : clé H1, aléa de `json cle nouvelle`, clé Wi-Fi WPA de 64 hexa ; un champ `wifi` ; la réponse à `wifi <ssid> <mdp>`, dont `maskCmd` garde le SSID dans `cmd`.
- **Seule exception : l'objet `wifi` du bloc `reseau` `ip`**, s'il n'a que ses quatre champs documentés. L'instantané de `json 1` porte ce bloc dès la tâche 19 : refuser tout champ `wifi` refuserait chaque capture de l'étape 5 (par UDP). Un `wifi` ailleurs, ou avec un autre champ, reste refusé.
- **`json_check.py` sans `--strict`** : un trou de `n` est un avertissement ; le journal exige déjà zéro perte à l'étape 5 (ligne 9), sinon on refait le scénario.
- **Provenance écrite par l'outil**, pour que chaque fichier de `captures/` ait sa ligne sans rien à faire à la main : fichier, source dans `logs/`, heure du Mac de la première ligne, nombre de lignes, transport (`usb`, `udp`), `fw` du premier `hello` `base`, premier `boot` vu (`?` s'ils manquent).

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_verse_capture.py` :

```python
#!/usr/bin/env python3
"""Tests de tools/verse_capture.py, sans toucher a captures/ :
python3 -m unittest discover -s tools/tests -p test_verse_capture.py -v"""
import contextlib
import datetime
import io
import json
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ICI, ".."))
import verse_capture  # noqa: E402

README = os.path.join(ICI, "..", "..", "captures", "README.md")
SECRET = "0123456789ABCDEF" * 4  # 64 hexa, comme une cle H1
RX0 = 1791100800000

HELLO = {
    "v": 1, "t": "hello", "n": 0, "ms": 12031, "bloc": "base", "rev": 4, "fw": "0.1.0-1a2b3c4",
    "fw_desc": "0.1.0-1a2b3c4", "date": "Sep 27 2026", "heure": "14:02:11", "env": "sonde", "build": "sonde",
    "reseau_build": "aucun", "puce": "esp32c6", "idf": "v5.5.5", "arduino": "3.3.12", "boot": "3FA2C901",
    "reset": "mise_sous_tension", "reset_n": 1, "up_s": 12,
    "session": {"transport": "usb", "periode_ms": 1000, "compteurs_ms": 1000, "reseau_ms": 5000, "bail_s": 0,
                "trames": True, "log": False},
    "limites": {"ligne_max": 1024, "cmd_max": 127},
}
HB = {"v": 1, "t": "hb", "n": 1, "ms": 14031, "boot": "3FA2C901", "up_s": 14, "json_perdus": 0}
TRAME = {"v": 1, "t": "trame", "n": 2, "ms": 15506, "num": 57, "part": 0, "fin": True, "t_us": 15472110,
         "niv0": "bas", "dur_us": [1500, 750, 750, 2250], "debord": False}
REPONSE = {"v": 1, "t": "reponse", "n": 3, "ms": 15600, "id": 2, "etape": "fin", "cmd": "json ping", "ok": True,
           "code": "ok", "duree_ms": 0}
RESEAU_IP = {"v": 1, "t": "reseau", "n": 7, "ms": 12038, "bloc": "ip", "srp": None,
             "wifi": {"connecte": True, "rssi_dbm": -58, "ip": "192.168.1.42", "pertes": 0}}


def enr(l, k=0, de="usb"):
    """Une ligne .jsonl, comme serie_enregistre.py et hotte_udp.py l'ecrivent."""
    return '{"rx_ms":%d,"de":"%s","l":%s}' % (RX0 + k, de, json.dumps(l, separators=(",", ":")))


def avec(obj, **champs):
    o = dict(obj)
    o.update(champs)
    return o


def lancer(*argv):
    sortie = io.StringIO()
    with contextlib.redirect_stdout(sortie), contextlib.redirect_stderr(sortie):
        code = verse_capture.main(list(argv))
    return code, sortie.getvalue()


class Refus(unittest.TestCase):
    def test_ligne_ordinaire(self):
        for l in (HELLO, HB, TRAME, REPONSE):
            self.assertEqual(verse_capture.refus(enr(l)), [])
        self.assertEqual(verse_capture.refus(b"pas du json"), [])  # json_check.py la refusera

    def test_champ_cle(self):
        self.assertEqual(verse_capture.refus(enr(avec(REPONSE, cle="7A"))), ["champ 'cle'"])
        self.assertEqual(verse_capture.refus(enr(avec(HB, x={"y": [{"cle": 1}]}))), ["champ 'cle'"])

    def test_chaine_de_64_hexa(self):
        self.assertIn("chaine de 64 hexa", verse_capture.refus(enr(avec(REPONSE, cmd="x" + SECRET))))
        self.assertIn("chaine de 64 hexa", verse_capture.refus(enr(avec(HB, txt=SECRET.lower()))))
        # Un caractere echappe en JSON coupe la suite dans le texte brut : vue apres lecture du JSON.
        echappee = enr(avec(HB, txt=SECRET)).replace(SECRET, SECRET[:10] + "\\u0041" + SECRET[11:])
        self.assertIn("chaine de 64 hexa", verse_capture.refus(echappee))
        self.assertEqual(verse_capture.refus(enr(avec(HB, txt=SECRET[:63]))), [])

    def test_ssid_mdp_et_wifi(self):
        self.assertEqual(verse_capture.refus(enr(avec(HB, ssid="MonReseau"))), ["champ 'ssid'"])
        self.assertEqual(verse_capture.refus(enr(avec(HB, x={"mdp": "secret12"}))), ["champ 'mdp'"])
        self.assertEqual(verse_capture.refus(enr(avec(HB, wifi="MonReseau"))), ["champ 'wifi'"])

    def test_bloc_wifi_de_reseau_ip(self):
        # Etat de la connexion (docs/PROTOCOLE-JSON.md, reseau ip) : sans SSID, admis.
        self.assertEqual(verse_capture.refus(enr(RESEAU_IP)), [])
        wifi = dict(RESEAU_IP["wifi"], nom="MonReseau")
        self.assertEqual(verse_capture.refus(enr(avec(RESEAU_IP, wifi=wifi))), ["champ 'wifi'"])
        self.assertEqual(verse_capture.refus(enr(avec(HB, wifi=RESEAU_IP["wifi"]))), ["champ 'wifi'"])

    def test_reponse_a_la_commande_wifi(self):
        # maskCmd coupe le mot de passe, pas le SSID.
        self.assertEqual(verse_capture.refus(enr(avec(REPONSE, cmd="wifi MonReseau"))),
                         ["reponse a 'wifi' (SSID dans cmd)"])


class Versement(unittest.TestCase):
    def setUp(self):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        self.dossier = d.name
        self.captures = os.path.join(d.name, "captures")
        os.makedirs(self.captures)
        shutil.copy(README, os.path.join(self.captures, "README.md"))
        p = mock.patch.object(verse_capture, "CAPTURES", self.captures)
        p.start()
        self.addCleanup(p.stop)

    def ecrire(self, nom, lignes):
        p = os.path.join(self.dossier, nom)
        with open(p, "w", encoding="ascii") as f:
            f.write("".join(x + "\n" for x in lignes))
        return p

    def readme(self):
        with open(os.path.join(self.captures, "README.md"), encoding="utf-8") as f:
            return f.read()

    def test_verse_telle_quelle_avec_provenance(self):
        src = self.ecrire("2026-10-04-1412-veille.jsonl", [enr(HELLO, 0), enr(HB, 1), enr(TRAME, 2)])
        code, out = lancer(src, "etape5-veille")
        self.assertEqual(code, 0, out)
        with open(src, "rb") as a, open(os.path.join(self.captures, "etape5-veille.jsonl"), "rb") as b:
            self.assertEqual(a.read(), b.read())
        quand = datetime.datetime.fromtimestamp(RX0 / 1000).strftime("%Y-%m-%d %H:%M:%S")
        self.assertTrue(self.readme().endswith(
            "| `etape5-veille.jsonl` | `2026-10-04-1412-veille.jsonl` | %s | 3 | usb | 0.1.0-1a2b3c4 | 3FA2C901 |\n"
            % quand), self.readme()[-300:])
        self.assertIn("versee : ", out)

    def test_refus_sans_rien_ecrire(self):
        avant = self.readme()
        src = self.ecrire("c.jsonl", [enr(HB, 1), enr(avec(REPONSE, cle=SECRET), 2)])
        code, out = lancer(src, "etape5-marche")
        self.assertEqual(code, 1, out)
        self.assertIn("c.jsonl:2 : champ 'cle'", out)
        self.assertNotIn(SECRET, out)  # jamais la ligne refusee
        self.assertFalse(os.path.exists(os.path.join(self.captures, "etape5-marche.jsonl")))
        self.assertEqual(self.readme(), avant)

    def test_refus_de_json_check(self):
        src = self.ecrire("c.jsonl", [enr(HB, 1), enr(avec(TRAME, dur_us=[0, 5]), 2)])
        code, out = lancer(src, "etape5-marche")
        self.assertEqual(code, 1, out)
        self.assertIn("json_check.py refuse la capture", out)
        self.assertEqual(os.listdir(self.captures), ["README.md"])

    def test_pas_d_ecrasement_sans_force(self):
        a = self.ecrire("a.jsonl", [enr(HB, 1)])
        b = self.ecrire("b.jsonl", [enr(HB, 1), enr(TRAME, 2)])
        dest = os.path.join(self.captures, "etape5-v1.jsonl")
        self.assertEqual(lancer(a, "etape5-v1")[0], 0)
        code, out = lancer(b, "etape5-v1")
        self.assertEqual(code, 1, out)
        self.assertIn("--force", out)
        self.assertEqual(os.path.getsize(dest), os.path.getsize(a))
        self.assertEqual(lancer(b, "etape5-v1", "--force")[0], 0)
        self.assertEqual(os.path.getsize(dest), os.path.getsize(b))
        lignes = [x for x in self.readme().splitlines() if x.startswith("| `etape5-v1.jsonl` |")]
        self.assertEqual(len(lignes), 1, lignes)
        self.assertIn("| `b.jsonl` |", lignes[0])

    def test_usage(self):
        src = self.ecrire("c.jsonl", [enr(HB, 1)])
        for nom in ("../x", "a/b", "x.jsonl", "", "-x"):
            with self.subTest(nom=nom):
                self.assertEqual(lancer(src, "--", nom)[0], 2)
        self.assertEqual(lancer(os.path.join(self.dossier, "absent.jsonl"), "x")[0], 2)
        self.assertEqual(lancer(self.ecrire("vide.jsonl", []), "x")[0], 1)
        self.assertEqual(os.listdir(self.captures), ["README.md"])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_verse_capture.py -v`
Attendu : ÉCHEC avec « ModuleNotFoundError: No module named 'verse_capture' »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `tools/verse_capture.py` :

```python
#!/usr/bin/env python3
"""Verse une capture de reference dans captures/ (spec 9 ; journal, etapes 5 et 5b).

  python3 tools/verse_capture.py <capture.jsonl> <nom> [--force]

Copie la capture (.jsonl ecrit dans logs/ par serie_enregistre.py ou
hotte_udp.py enregistre) telle quelle vers captures/<nom>.jsonl, et ajoute sa
provenance au tableau de captures/README.md. Refuse (code 1), sans rien
ecrire :
  - une ligne qui contient un champ 'cle', 'ssid' ou 'mdp' ; un champ 'wifi'
    autre que le bloc wifi de 'reseau' 'ip' (connecte, rssi_dbm, ip, pertes :
    sans SSID) ; la reponse a 'wifi <ssid> <mdp>' (son cmd porte le SSID) ;
  - une ligne qui contient une chaine de 64 caracteres hexadecimaux (cle H1,
    alea de 'json cle nouvelle', cle Wi-Fi de 64 hexa) ;
  - une capture vide, ou que 'json_check.py --jsonl' refuse ;
  - un nom deja verse, sauf avec --force.
Une ligne refusee n'est jamais affichee : seulement son numero et la raison.

Codes de sortie : 0 versee, 1 refusee, 2 usage (nom, fichier illisible).

Exemple :
  python3 tools/verse_capture.py logs/2026-10-04-1412-veille.jsonl etape5-veille
"""
import argparse
import datetime
import json
import os
import re
import shutil
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import json_check  # noqa: E402

CAPTURES = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "captures")
NOM = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]*$")
HEXA64 = re.compile(r"[0-9A-Fa-f]{64}")
INTERDITS = ("cle", "ssid", "mdp")
WIFI_RESEAU = {"connecte", "rssi_dbm", "ip", "pertes"}  # reseau.ip.wifi (docs/PROTOCOLE-JSON.md)


class _Paires(list):
    """Objet JSON lu en liste de paires (cles en double gardees)."""


def _raisons(v, out):
    """Ajoute a out les raisons de refus trouvees dans la valeur v, a toute profondeur."""
    if isinstance(v, _Paires):
        champs = dict(v)
        ip = champs.get("t") == "reseau" and champs.get("bloc") == "ip"
        for k, x in v:
            if k in INTERDITS:
                r = f"champ '{k}'"
            elif k == "wifi" and not (ip and isinstance(x, _Paires) and {c for c, _ in x} <= WIFI_RESEAU):
                r = "champ 'wifi'"
            elif k == "cmd" and isinstance(x, str) and x.split()[:1] == ["wifi"]:
                r = "reponse a 'wifi' (SSID dans cmd)"
            else:
                r = None
            if r and r not in out:
                out.append(r)
            _raisons(k, out)
            _raisons(x, out)
    elif isinstance(v, list):
        for x in v:
            _raisons(x, out)
    elif isinstance(v, str) and HEXA64.search(v) and "chaine de 64 hexa" not in out:
        out.append("chaine de 64 hexa")


def refus(ligne):
    """Raisons de refuser une ligne .jsonl (liste vide : ni cle, ni SSID, ni mot de passe vus)."""
    texte = ligne.decode("utf-8", "replace") if isinstance(ligne, bytes) else ligne
    out = []
    try:
        _raisons(json.loads(texte, object_pairs_hook=_Paires), out)
    except ValueError:
        pass  # illisible : json_check.py la refusera
    if HEXA64.search(texte) and "chaine de 64 hexa" not in out:
        out.append("chaine de 64 hexa")
    return out


def provenance(chemin, nom):
    """Ligne du tableau de captures/README.md (capture deja acceptee par json_check.py)."""
    lignes, debut, transports, fw, boot = 0, None, set(), None, None
    with open(chemin, "rb") as f:
        for brut in f:
            if not brut.strip():
                continue
            rec = json.loads(brut)
            lignes += 1
            if debut is None:
                debut = rec["rx_ms"]
            transports.add("usb" if rec["de"] == "usb" else "udp")
            l = rec["l"]
            if fw is None and l.get("t") == "hello" and l.get("bloc") == "base":
                fw = l.get("fw")
            if boot is None and isinstance(l.get("boot"), str):
                boot = l["boot"]
    quand = datetime.datetime.fromtimestamp(debut / 1000).strftime("%Y-%m-%d %H:%M:%S")
    return "| `%s.jsonl` | `%s` | %s | %d | %s | %s | %s |" % (
        nom, os.path.basename(chemin), quand, lignes, ", ".join(sorted(transports)), fw or "?", boot or "?")


def noter(readme, nom, ligne):
    """Ajoute la ligne en fin de README (le tableau de provenance est sa derniere section)."""
    with open(readme, encoding="utf-8") as f:
        lignes = f.read().splitlines()
    tete = "| `%s.jsonl` |" % nom
    lignes = [x for x in lignes if not x.startswith(tete)] + [ligne]  # --force : remplacee
    with open(readme, "w", encoding="utf-8") as f:
        f.write("\n".join(lignes) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description="Verse une capture .jsonl dans captures/ (ni cle, ni SSID, ni mot de passe).")
    ap.add_argument("capture", help="capture .jsonl (logs/...)")
    ap.add_argument("nom", help="nom dans captures/, sans .jsonl (ex. etape5-veille)")
    ap.add_argument("--force", action="store_true", help="remplacer une capture deja versee sous ce nom")
    args = ap.parse_args(argv)
    if not NOM.match(args.nom):
        print(f"nom refuse : {args.nom!r} (lettres, chiffres, - et _, sans .jsonl)", file=sys.stderr)
        return 2
    readme = os.path.join(CAPTURES, "README.md")
    dest = os.path.join(CAPTURES, args.nom + ".jsonl")
    if not os.path.isfile(readme):
        print(f"{readme} absent : dossier des captures introuvable", file=sys.stderr)
        return 2
    try:
        with open(args.capture, "rb") as f:
            lignes = f.read().split(b"\n")
    except OSError as e:
        print(f"lecture impossible : {e}", file=sys.stderr)
        return 2
    if os.path.exists(dest) and not args.force:
        print(f"refusee : {dest} existe deja (--force pour la remplacer) ; rien n'est verse")
        return 1
    mauvaises = 0
    for k, ligne in enumerate(lignes, 1):
        raisons = refus(ligne)
        if raisons:
            mauvaises += 1
            print(f"{args.capture}:{k} : {', '.join(raisons)}")
    if mauvaises:
        print(f"refusee : {mauvaises} ligne(s) avec une cle, un SSID ou un mot de passe ; rien n'est verse")
        return 1
    if not any(x.strip() for x in lignes):
        print("refusee : capture vide ; rien n'est verse")
        return 1
    if json_check.main(["--jsonl", args.capture]) != 0:
        print("refusee : json_check.py refuse la capture ; rien n'est verse")
        return 1
    shutil.copyfile(args.capture, dest)
    ligne = provenance(args.capture, args.nom)
    noter(readme, args.nom, ligne)
    print(f"versee : {dest} ; provenance ajoutee a {readme} :")
    print(ligne)
    print(f"a committer : git add captures/{args.nom}.jsonl captures/README.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Étape 4 : Écrire `captures/README.md`**

Les tests en copient le tableau : il doit exister avant de les relancer.

Créer `captures/README.md` :

````markdown
# Captures de référence

Captures de la ligne `D` gardées dans le dépôt : étapes 5 (`etape5-<scénario>`)
et 5b (`etape5b-<scénario>`) du journal (`docs/RECONNAISSANCE.md`). Les
enregistrements bruts restent dans `logs/`, que git ignore ; on n'en verse ici
qu'une copie vérifiée.

Format : le `.jsonl` du protocole (`docs/PROTOCOLE-JSON.md`), une ligne par
message machine reçu : `{"rx_ms": <heure du Mac en ms>, "de": "usb" | "<ip>",
"l": {...}}`. Lecture : `tools/analyse.py`, `tools/json_check.py --jsonl`.

## Règles

- **Aucune clé** : ni champ `cle`, ni chaîne de 64 caractères hexadécimaux
  (clé H1 de `json cle nouvelle`, aléa de l'app, clé Wi-Fi de 64 hexa).
- **Aucun SSID ni mot de passe** : ni champ `ssid` ou `mdp`, ni champ `wifi`
  autre que le bloc `wifi` de `reseau` `ip` (état de la connexion, sans SSID),
  ni réponse à `wifi <ssid> <mdp>` (son `cmd` porte le SSID).
- **Toujours par l'outil**, jamais à la main :
  `python3 tools/verse_capture.py logs/<fichier>.jsonl <nom>`. Il refuse
  (code 1, sans rien écrire) une capture qui enfreint l'une de ces règles, une
  capture vide ou que `json_check.py --jsonl` refuse ; il n'écrase une capture
  déjà versée qu'avec `--force`. Il n'affiche jamais une ligne refusée,
  seulement son numéro et la raison.
- Une capture refusée ne se corrige pas à la main : on refait
  l'enregistrement sans la commande en cause (`json cle`, `wifi` : par l'USB,
  hors enregistrement).
- **Provenance** : l'outil ajoute une ligne au tableau ci-dessous, ou la
  remplace avec `--force`. Ce tableau reste la dernière section du fichier.

## Provenance

| Fichier | Source (`logs/`) | Début (heure du Mac) | Lignes | Transport | Firmware | Démarrage |
|---|---|---|---|---|---|---|
````

- [ ] **Étape 5 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_verse_capture.py -v`
Attendu : les 11 tests à `ok` (`Refus` 6, `Versement` 5), « Ran 11 tests », « OK »

Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « Ran 119 tests », puis « OK »

Lancer : `python3 tools/verse_capture.py logs/absent.jsonl essai; echo "code $?"`
Attendu : « lecture impossible : [Errno 2] No such file or directory: 'logs/absent.jsonl' » puis « code 2 » ; `captures/` ne contient toujours que `README.md`.

- [ ] **Étape 6 : Commit**

```bash
git add tools/verse_capture.py tools/tests/test_verse_capture.py captures/README.md
git commit -m "Ajouter le versement des captures de reference" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
