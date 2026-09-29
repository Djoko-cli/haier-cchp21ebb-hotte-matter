#!/usr/bin/env python3
# Copie de benq-screenbar-halo-matter@c58a506 : tools/halo_udp.py (adapte : cle dans ~/.config/hotte-sonde/cle, getaddrinfo AF_UNSPEC, sous-commande enregistre, confirmation tapee de 'injection on', resume du profil hotte ; sans trousseau ni 'refus')
"""Client de banc du transport reseau de la sonde (docs/PROTOCOLE-JSON.md, section 9)
et du module produit (docs/PROTOCOLE-JSON-PRODUIT.md).

Session H1 (UDP port 5480 : sur le Wi-Fi pour la sonde, sur Thread pour le
produit) avec la cle posee par l'USB : suivre les lignes, envoyer des
commandes de la liste blanche, ou enregistrer une capture.

Appareil (--appareil sonde|produit) : il choisit le fichier de cle,
~/.config/hotte-<appareil>/cle. Pour 'cle', il se lit dans hello.base.build
par l'USB si --appareil manque ; pour 'session' et 'enregistre', sonde par
defaut (la cle signe la poignee de main : elle se choisit avant).

  cle <port serie>
      Nouvelle cle partagee : 'json cle nouvelle <alea>' par l'USB (ouverture
      sure du C6 : DTR = RTS = 0 en un seul appel). La cle est rangee dans
      ~/.config/hotte-<appareil>/cle (droits 0600 ; HOTTE_CLE=<fichier> pour
      un autre), jamais affichee : seule son empreinte l'est. Toutes les sessions
      reseau tombent. Le port doit etre libre.

  session <hote> [commande ...] [--duree s] [--brut] [--port p]
      Poignee de main (SALUT signe, DEFI verifie), 'json 1', puis chaque
      commande donnee (entre guillemets, sans id=), 'json ping' toutes les
      10 s. Une commande sans reponse en 2 s est renvoyee avec le meme id (2
      fois au plus : la sonde renvoie sa reponse sans reexecuter). Sonde muette
      8 s : nouvelle poignee de main. Un resume de chaque ligne (--brut : le
      JSON tel quel). Fin : 'json 0'.

  enregistre <hote> <scenario> [commande ...] [--duree s] [--port p] [--dossier d]
      Meme session ; chaque ligne acceptee est ecrite dans
      <dossier>/AAAA-MM-JJ-hhmm-<scenario>.jsonl :
        {"rx_ms": <heure du Mac en ms>, "de": "<ip>", "l": <objet recu tel quel>}
      et un resume part dans <dossier>/live.log toutes les 5 s et a la fin
      ('tail -f logs/live.log'). Fin : --duree ecoulee, ou Ctrl-C.

Hote : hotte-sonde.local (mDNS) ou l'adresse IPv4 de la sonde ; pour le
produit, son nom SRP (<nom>.local, bloc reseau/ip) ou une adresse IPv6 OMR
(le Mac garde sa route Thread avec l'assistant de benq, tools/macos/halo-routes).
'injection on' (regle 11 : Majid devant la hotte, a portee de la fiche, le
panneau en vue) ne part qu'apres une confirmation que Majid tape lui-meme,
dans un terminal, avant l'ouverture de la session : la phrase exacte
'Majid devant la hotte'. Sans terminal (script, tube, agent), ou sans la
phrase exacte, rien n'est envoye.

Exemples :
  python3 tools/hotte_udp.py cle /dev/cu.usbmodem101
  python3 tools/hotte_udp.py session hotte-sonde.local --duree 30
  python3 tools/hotte_udp.py enregistre hotte-sonde.local krona-wifi "capture tout" "seuils 1 19000" --duree 660
  python3 tools/hotte_udp.py --appareil produit session fd11:22::1a2b --duree 30 "hotte ventilo v2"
"""
import argparse
import datetime
import hashlib
import hmac
import json
import os
import socket
import sys
import tempfile
import time

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import serie_enregistre  # noqa: E402  (ouverture sure du port, decoupage, bilan)

PORT = 5480
RESUME_S = 5.0
PHRASE_INJECTION = "Majid devant la hotte"  # confirmation de 'injection on' (regle 11)
DOSSIER = serie_enregistre.DOSSIER


APPAREILS = ("sonde", "produit")


def chemin_cle(appareil="sonde"):
    """Fichier de la cle : HOTTE_CLE, sinon ~/.config/hotte-<appareil>/cle."""
    if appareil not in APPAREILS:
        raise SystemExit(f"appareil {appareil!r} : sonde ou produit")
    return os.path.abspath(os.environ.get("HOTTE_CLE") or os.path.expanduser(f"~/.config/hotte-{appareil}/cle"))


# ---------------------------------------------------------------------------
#  Enveloppe H1 (ScreenBar 10.4 ; docs/PROTOCOLE-JSON.md 9.3)
# ---------------------------------------------------------------------------


def mac16(key, text):
    return hmac.new(key, text.encode() if isinstance(text, str) else text, hashlib.sha256).digest()[:16]


def kid_of(key):
    return hashlib.sha256(key).hexdigest().upper()[:8]


def texte_salut(key, kid, na):
    """'H1 SALUT <kid> <na> <mac_salut>' (na : 32 hexa majuscules)."""
    mac = mac16(key, f"H1|SALUT|{kid}|{na}").hex().upper()
    return f"H1 SALUT {kid} {na} {mac}".encode()


def cle_session(key, na, nc, sid):
    return hmac.new(key, f"H1|SESSION|{na}|{nc}|{sid}".encode(), hashlib.sha256).digest()


def lire_defi(key, kid, na, d):
    """(sid, Ks) si d est le DEFI au MAC juste pour ce na ; None sinon."""
    p = d.split(b" ")
    if len(p) != 5 or p[0] != b"H1" or p[1] != b"DEFI":
        return None
    sid, nc = p[2].decode("ascii", "replace"), p[3].decode("ascii", "replace")
    want = mac16(key, f"H1|DEFI|{kid}|{na}|{nc}|{sid}").hex().upper().encode()
    if not hmac.compare_digest(want, p[4]):
        return None
    return sid, cle_session(key, na, nc, sid)


def sceller(ks, sens, sid, ctr, charge):
    """'H1 <sid> <ctr> <mac> <charge>' ; sens 'A' (app -> sonde) ou 'C' (sonde -> app)."""
    mac = mac16(ks, f"{sens}|{sid}|{ctr}|".encode() + charge).hex().upper()
    return f"H1 {sid} {ctr} {mac} ".encode() + charge


class Fenetre:
    """ctr deja vus : fenetre glissante de 32 (h1::Window)."""

    def __init__(self):
        self.top, self.bits = 0, 0

    def accepter(self, ctr):
        if ctr == 0:
            return False
        if ctr > self.top:
            shift = ctr - self.top
            self.bits = 0 if shift >= 32 else (self.bits << shift) & 0xFFFFFFFF
            self.bits |= 1
            self.top = ctr
            return True
        back = self.top - ctr
        if back >= 32 or self.bits & (1 << back):
            return False
        self.bits |= 1 << back
        return True


def ouvrir_message(ks, sens, sid, d, fenetre):
    """(ctr, charge) si d est un message de la session sid, sens donne, MAC juste
    et ctr neuf (la fenetre n'avance qu'apres le MAC) ; None sinon."""
    p = d.split(b" ", 4)
    if len(p) != 5 or p[0] != b"H1" or p[1] != sid.encode() or not p[2].isdigit() or p[2].startswith(b"0"):
        return None
    want = mac16(ks, sens.encode() + b"|" + p[1] + b"|" + p[2] + b"|" + p[4]).hex().upper().encode()
    if not hmac.compare_digest(want, p[3]):
        return None
    ctr = int(p[2])
    if ctr > 0xFFFFFFFF or not fenetre.accepter(ctr):
        return None
    return ctr, p[4]


# ---------------------------------------------------------------------------
#  Cle par l'USB
# ---------------------------------------------------------------------------


def demander_cle(lire, ecrire, alea, ident, delai_s=5.0, horloge=time.time):
    """'id=<ident> json cle nouvelle <alea>' sur un port deja ouvert ; rend
    (cle en 64 hexa, empreinte, msg). SystemExit si refusee ou sans reponse."""
    decoupe = serie_enregistre.Decoupe()
    ecrire(b"\x15\n")  # Ctrl-U : efface un reste de ligne
    decoupe.feed(lire(0.2))
    ecrire(f"id={ident} json cle nouvelle {alea}\n".encode("ascii"))
    limite = horloge() + delai_s
    while horloge() < limite:
        for genre, _, m in decoupe.feed(lire(0.2)):
            if genre != "machine" or m.get("t") != "reponse" or m.get("id") != ident or m.get("etape") != "fin":
                continue
            if not m.get("ok") or "cle" not in m:
                raise SystemExit(f"refuse : {m.get('code')} {m.get('msg', '')}")
            try:
                cle = bytes.fromhex(m["cle"])
            except (TypeError, ValueError):
                cle = b""
            if len(cle) != 32 or kid_of(cle) != m.get("empreinte"):
                raise SystemExit("empreinte incoherente : cle non rangee")
            return m["cle"], m["empreinte"], m.get("msg")
    raise SystemExit("aucune reponse en 5 s (firmware sans transport reseau ? sonde occupee ?). Si la sonde a "
                     "quand meme change de cle, 'json cle' par l'USB montre une autre empreinte : relancer 'cle'.")


def lire_build(lire, ecrire, ident, delai_s=3.0, horloge=time.time):
    """'id=<ident> json hello' sur un port deja ouvert ; rend hello.base.build
    ('sonde', 'produit'), ou None sans reponse (firmware ancien, port muet)."""
    decoupe = serie_enregistre.Decoupe()
    ecrire(b"\x15\n")  # Ctrl-U : efface un reste de ligne
    decoupe.feed(lire(0.2))
    ecrire(f"id={ident} json hello\n".encode("ascii"))
    build = None
    limite = horloge() + delai_s
    while horloge() < limite:
        for genre, _, m in decoupe.feed(lire(0.2)):
            if genre != "machine":
                continue
            if m.get("t") == "hello" and m.get("bloc") == "base" and m.get("build") in APPAREILS:
                build = m["build"]
            if m.get("t") == "reponse" and m.get("id") == ident and m.get("etape") == "fin":
                return build
    return build


def ranger_cle(hex_key, chemin):
    """Ecriture atomique, 0600 (dossier 0700), sans suivre de lien symbolique."""
    d = os.path.dirname(chemin)
    os.makedirs(d, mode=0o700, exist_ok=True)
    fdk, tmp = tempfile.mkstemp(prefix=".cle.", dir=d)  # 0600, nom unique, jamais un lien
    try:
        os.fchmod(fdk, 0o600)
        os.write(fdk, (hex_key + "\n").encode())
        os.fsync(fdk)
    finally:
        os.close(fdk)
    try:
        os.replace(tmp, chemin)
    except OSError:
        os.unlink(tmp)
        raise


def lire_cle(chemin):
    try:
        with open(chemin) as f:
            key = bytes.fromhex(f.read().strip())
    except OSError:
        raise SystemExit(f"{chemin} : illisible (cle posee par 'python3 tools/hotte_udp.py cle <port>' ?)")
    except ValueError:
        raise SystemExit(f"{chemin} : cle illisible (64 hexa attendus)")
    if len(key) != 32:
        raise SystemExit(f"{chemin} : cle illisible (64 hexa attendus)")
    return key


def cmd_cle(port, chemin=None, appareil=None):
    """Nouvelle cle par l'USB ; chemin nul : celui de l'appareil (--appareil,
    ou hello.base.build lu sur le port)."""
    alea = os.urandom(32).hex().upper()
    try:
        fd = serie_enregistre.ouvrir_port(port)
    except OSError as e:
        raise SystemExit(f"{port} : {e.strerror} (port tenu par 'pio device monitor' ou serie_enregistre.py ?)")
    try:
        ident = 900000 + int.from_bytes(os.urandom(2), "big") % 90000
        lire, ecrire = serie_enregistre.lecteur(fd), serie_enregistre.ecrivain(fd)
        if chemin is None:
            if appareil is None:
                appareil = lire_build(lire, ecrire, ident + 1)
                if appareil is None:
                    raise SystemExit("appareil inconnu (aucun hello.base.build) : preciser --appareil sonde|produit")
            chemin = chemin_cle(appareil)
        cle, empreinte, msg = demander_cle(lire, ecrire, alea, ident)
    except serie_enregistre.PortFerme as e:
        raise SystemExit(f"port ferme ({e}) : la sonde a-t-elle redemarre ?")
    finally:
        os.close(fd)
    ranger_cle(cle, chemin)
    print(f"cle rangee dans {chemin} (empreinte {empreinte})")
    if msg:
        print(f"  ({msg})")


# ---------------------------------------------------------------------------
#  Session H1
# ---------------------------------------------------------------------------


def resoudre(hote, port):
    """(famille, adresse) : premier resultat de getaddrinfo, IPv4 ou IPv6."""
    try:
        ai = socket.getaddrinfo(hote, port, socket.AF_UNSPEC, socket.SOCK_DGRAM)
    except socket.gaierror as e:
        raise SystemExit(f"{hote} : {e} (hotte-sonde.local, nom SRP du produit, ou adresse donnee par 'info')")
    return ai[0][0], ai[0][4]


def confirmer(commandes, demander=input):
    """'injection on' arme l'injection (spec 8.4). Regle 11 : toute emission vers
    la hotte se fait en presence de Majid. Pour chaque 'injection on' de la
    liste, avant tout envoi, Majid tape lui-meme la phrase exacte
    PHRASE_INJECTION. Seulement depuis un terminal : sans terminal sur l'entree
    standard (script, tube, agent), refus, sans question."""
    n = sum(1 for c in commandes if " ".join(c.split()) == "injection on")
    if not n:
        return True
    if sys.stdin is None or not sys.stdin.isatty():
        return False
    for _ in range(n):
        try:
            r = demander("Regle 11 : Majid est devant la hotte, a portee de la fiche, le panneau en vue, et "
                         "debranche au moindre doute.\n'injection on' arme l'injection de la sonde. Pour "
                         f"l'envoyer, Majid tape lui-meme : {PHRASE_INJECTION}\n> ")
        except EOFError:
            return False
        if r != PHRASE_INJECTION:
            return False
    return True


def poignee(s, key, kid, afficher):
    """SALUT signe, na neuf a chaque essai ; seul un DEFI au MAC juste pour ce na est pris."""
    for _ in range(3):
        na = os.urandom(16).hex().upper()
        try:
            s.send(texte_salut(key, kid, na))
        except OSError as e:
            afficher(f"!! envoi impossible : {e.strerror} (sonde sur le Wi-Fi ? meme reseau ?)")
            time.sleep(1)
            continue
        deadline = time.time() + 2.0  # le DEFI peut attendre la fin d'un datagramme en cours
        while time.time() < deadline:
            s.settimeout(max(0.05, deadline - time.time()))
            try:
                d = s.recv(2048)
            except socket.timeout:
                break
            except ConnectionRefusedError:
                raise SystemExit("port injoignable : pas de cle sur la sonde ('hotte_udp.py cle <port>'), "
                                 "ou Wi-Fi sans adresse ('info' par l'USB)")
            except OSError as e:
                afficher(f"!! reception : {e.strerror}")
                break
            r = lire_defi(key, kid, na, d)
            if r:
                return r
    return None


class Client:
    """Session H1 cote app sur un socket UDP connecte."""

    def __init__(self, s, key, afficher=print):
        self.s, self.key, self.kid, self.afficher = s, key, kid_of(key), afficher
        self.sid = None
        self.a_faire = []
        self.stats = {"lignes": 0, "rejetees": 0, "renvois": 0, "sessions": 0}

    def ouvrir(self):
        r = poignee(self.s, self.key, self.kid, self.afficher)
        if not r:
            return False
        self.sid, self.ks = r
        self.ctr, self.ident, self.fenetre = 0, 0, Fenetre()
        self.dernier_rx, self.derniere_cmd, self.en_vol = time.time(), 0.0, None
        self.stats["sessions"] += 1
        self.afficher(f"session {self.sid} ouverte")
        self.envoyer("json 1")
        return True

    def _emettre(self, ident, ligne):
        self.ctr += 1
        texte = f"id={ident} {ligne}"
        try:
            self.s.send(sceller(self.ks, "A", self.sid, self.ctr, texte.encode("ascii")))
        except OSError as e:
            self.afficher(f"!! envoi impossible : {e.strerror}")
        return texte

    def envoyer(self, ligne):
        self.ident += 1
        texte = self._emettre(self.ident, ligne)
        self.en_vol = {"id": self.ident, "ligne": ligne, "at": time.time(), "essais": 0}
        self.derniere_cmd = time.time()
        self.afficher(f">> {texte}")

    def tour(self):
        """Renvoi, commande suivante ou ping, puis un datagramme (0,3 s au plus).
        Rend (charge, objet ou None) pour un message accepte, sinon None."""
        now = time.time()
        if now - self.dernier_rx > 8:
            self.afficher("!! sonde muette depuis 8 s : nouvelle poignee de main")
            if not self.ouvrir():
                time.sleep(1)
                return None
            now = time.time()
        p = self.en_vol
        if p and now - p["at"] > 2:
            if p["essais"] < 2:
                p["essais"] += 1
                p["at"] = now
                self.stats["renvois"] += 1
                self.afficher(f">> (renvoi) {self._emettre(p['id'], p['ligne'])}")
            else:
                self.afficher(f"!! id={p['id']} sans reponse")
                self.en_vol = None
        if not self.en_vol:
            if self.a_faire and now - self.derniere_cmd >= 1.5:
                self.envoyer(self.a_faire.pop(0))
            elif now - self.derniere_cmd >= 10:
                self.envoyer("json ping")
        self.s.settimeout(0.3)
        try:
            d = self.s.recv(2048)
        except socket.timeout:
            return None
        except ConnectionRefusedError:
            self.afficher("!! port injoignable : cle effacee ? sonde redemarree ?")
            time.sleep(0.3)
            return None
        except OSError as e:
            self.afficher(f"!! reception : {e.strerror}")
            time.sleep(0.5)
            return None
        r = ouvrir_message(self.ks, "C", self.sid, d, self.fenetre)
        if r is None:
            self.stats["rejetees"] += 1
            return None
        charge = r[1]
        self.dernier_rx = time.time()
        self.stats["lignes"] += 1
        try:
            m = json.loads(charge)
        except ValueError:
            self.afficher(f"!! JSON illisible : {charge[:80]!r}")
            return charge, None
        if not isinstance(m, dict):
            return charge, None
        if (m.get("t") == "reponse" and m.get("etape") == "fin" and self.en_vol
                and m.get("id") == self.en_vol["id"]):
            self.en_vol = None
        return charge, m

    def fermer(self, suite=None, delai_s=1.0):
        """'json 0' ; les lignes recues jusqu'au message fin (1 s au plus) passent par suite."""
        if not self.sid:
            return
        self.a_faire = []
        self.en_vol = None
        self.envoyer("json 0")
        limite = time.time() + delai_s
        while time.time() < limite:
            r = self.tour()
            if r and suite:
                suite(*r)
            if r and r[1] and r[1].get("t") == "fin":
                break


def resume(m):
    """Une ligne lisible par message du profil hotte (sonde et produit)."""
    t, b = m.get("t"), m.get("bloc")
    tete = f"n={m.get('n')} {t}" + (f"/{b}" if b else "")
    produit = resume_produit(t, b, m)
    if produit is not None:
        return tete + produit
    if t == "reponse":
        extra = f" id={m.get('id')} {m.get('etape')} {m.get('code')} « {m.get('cmd')} »"
        if m.get("msg"):
            extra += f" : {m['msg']}"
        return tete + extra
    if t == "trame":
        d = m.get("dur_us") or []
        extra = f" {m.get('num')}.{m.get('part')}{' fin' if m.get('fin') else ''} {m.get('niv0')} {len(d)} duree(s)"
        for k in ("rep", "sautes"):
            if k in m:
                extra += f" {k} {m[k]}"
        return tete + extra + (" DEBORD" if m.get("debord") else "")
    if t == "compteurs":
        return tete + "".join(f" {k} {m.get(k)}" for k in ("receptions", "debord", "lignes_perdues", "sautes", "rejets"))
    if t == "etat" and b == "bus":
        return tete + f" repos {m.get('repos')} fronts {m.get('fronts')} receptions/s {m.get('receptions_s')}"
    if t == "reseau" and b == "ip":
        w, u = m.get("wifi", {}), m.get("udp", {})
        return tete + (f" wifi {w.get('ip')} {w.get('rssi_dbm')} dBm pertes {w.get('pertes')} | udp sessions "
                       f"{u.get('sessions')} rx {u.get('rx')} tx {u.get('tx')} perdus {u.get('tx_perdus')}")
    if t == "hello" and b == "base":
        return tete + f" fw {m.get('fw')} rev {m.get('rev')} session {m.get('session')}"
    if t == "injection":
        return tete + f" id={m.get('id')} {m.get('resultat')} attente {m.get('attente_us')} us"
    if t == "log":
        return tete + f" [{m.get('src')}] {m.get('txt')}"
    if t == "fin":
        return tete + f" cause {m.get('cause')}"
    return tete


def _lampe(v):
    return "?" if v is None else "on" if v else "off"


def resume_produit(t, b, m):
    """La fin de la ligne pour un message propre au produit ; None sinon."""
    if t == "etat" and b == "hotte":
        return (f" {m.get('marche')} moteur {m.get('moteur')} lampe {_lampe(m.get('lampe'))} "
                f"({m.get('confiance')}, {m.get('source')}, lu il y a {m.get('age_ms')} ms)")
    if t == "etat" and b == "thermique":
        return (f" puce {m.get('temp_c')} C (max {m.get('temp_max_c')}, pose {m.get('temp_max_pose_c')}), "
                f"seuil {m.get('seuil_c')} C{' ALERTE' if m.get('alerte') else ''}")
    if t == "etat" and b == "alim":
        voies = []
        for k in ("hotte", "module"):
            v = m.get(k)
            voies.append(f"{k} ?" if not isinstance(v, dict) else
                         f"{k} {v.get('mv')} mV (min {v.get('min_mv')}){' ALERTE' if v.get('alerte') else ''}")
        return " " + ", ".join(voies)
    if t == "hotte":
        av, ap = m.get("avant") or {}, m.get("apres") or {}
        return (f" {av.get('marche')}/{av.get('moteur')} -> {ap.get('marche')}/{ap.get('moteur')} "
                f"lampe {_lampe(ap.get('lampe'))} ({m.get('origine')}, {m.get('source')})")
    if t == "sequence":
        cause = f", {m['cause']}" if m.get("cause") else ""
        return (f" id={m.get('id')} {m.get('origine')} {m.get('sujet')} : {m.get('issue')}{cause} "
                f"({m.get('appuis')} appuis, {m.get('duree_ms')} ms)")
    if t == "alerte":
        return f" {m.get('sujet')} {m.get('etape')} : {m.get('valeur')} {m.get('unite')} (seuil {m.get('seuil')})"
    if t == "trame_d":
        return f" {m.get('origine')} {m.get('octets')}"
    return None


def ouvrir_client(hote, port, commandes, key, afficher, demander):
    if not confirmer(commandes, demander):
        raise SystemExit(f"injection on non confirmee (phrase '{PHRASE_INJECTION}' tapee par Majid, dans un "
                         "terminal : regle 11) : rien n'est envoye")
    famille, adresse = resoudre(hote, port)
    s = socket.socket(famille, socket.SOCK_DGRAM)
    try:
        s.connect(adresse)
    except OSError as e:
        s.close()
        raise SystemExit(f"{adresse[0]} : {e.strerror}")
    afficher(f"sonde {adresse[0]} port {adresse[1]}, cle {kid_of(key)}")
    c = Client(s, key, afficher)
    c.a_faire = list(commandes)
    if not c.ouvrir():
        s.close()
        raise SystemExit("aucun DEFI en 6 s (cle differente ? sonde hors ligne ? 'info' par l'USB)")
    return c, adresse[0]


def cmd_session(hote, port, commandes, duree, brut, key, afficher=print, demander=input):
    c, _ = ouvrir_client(hote, port, commandes, key, afficher, demander)

    def montrer(charge, m):
        afficher(charge.decode("ascii", "replace") if brut or m is None else resume(m))

    debut = time.time()
    try:
        while time.time() - debut < duree:
            r = c.tour()
            if r:
                montrer(*r)
    except KeyboardInterrupt:
        pass
    c.fermer(montrer)
    c.s.close()
    st = c.stats
    afficher(f"{st['lignes']} ligne(s) recue(s), {st['rejetees']} rejetee(s), {st['renvois']} renvoi(s), "
             f"{st['sessions']} session(s)")
    return st


def enregistrement(rx_ms, de, charge):
    """Une ligne .jsonl : l'objet recu tel quel (octets ASCII du firmware)."""
    return '{"rx_ms":%d,"de":%s,"l":%s}\n' % (rx_ms, json.dumps(de), charge.decode("ascii"))


def journaliser(journal, de, scenario, etat, duree_s, bilan, stats, afficher):
    quand = datetime.datetime.now()
    ligne = (f"{quand:%Y-%m-%d %H:%M:%S} udp {de} {scenario} {etat} {duree_s:.0f} s : {bilan.resume()} ; "
             f"H1 : {stats['rejetees']} rejetee(s), {stats['renvois']} renvoi(s), {stats['sessions']} session(s)")
    with open(journal, "a", encoding="utf-8") as f:
        f.write(ligne + "\n")
    afficher(ligne)


def cmd_enregistre(hote, port, scenario, commandes, duree, dossier, key, afficher=print, demander=input):
    """Session enregistree ; rend le chemin du .jsonl."""
    c, de = ouvrir_client(hote, port, commandes, key, afficher, demander)
    os.makedirs(dossier, exist_ok=True)
    chemin = os.path.join(dossier, serie_enregistre.nom_fichier(scenario, datetime.datetime.now()))
    journal = os.path.join(dossier, "live.log")
    bilan = serie_enregistre.Bilan()
    afficher(f"enregistrement dans {chemin} (resume dans {journal})")
    with open(chemin, "w", encoding="ascii") as sortie:

        def noter(charge, m):
            valide = (isinstance(m, dict) and m.get("v") == 1 and not isinstance(m.get("v"), bool)
                      and isinstance(m.get("t"), str) and isinstance(m.get("n"), int)
                      and not isinstance(m.get("n"), bool))
            if not valide:
                bilan.abimees += 1
                return
            sortie.write(enregistrement(int(round(time.time() * 1000)), de, charge))
            bilan.noter(m)
            if m["t"] == "reponse" and m.get("etape") == "fin":
                afficher(resume(m))

        debut = time.time()
        prochain = debut + RESUME_S
        etat = "fin"
        try:
            while duree is None or time.time() - debut < duree:
                r = c.tour()
                if r:
                    noter(*r)
                if time.time() >= prochain:
                    sortie.flush()
                    journaliser(journal, de, scenario, "en cours", time.time() - debut, bilan, c.stats, afficher)
                    prochain += RESUME_S
        except KeyboardInterrupt:
            etat = "interrompu"
        duree_s = time.time() - debut
        c.fermer(noter)
        c.s.close()
    journaliser(journal, de, scenario, etat, duree_s, bilan, c.stats, afficher)
    return chemin


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--appareil", choices=APPAREILS, help="sonde ou produit (fichier de cle ; cle : lu dans hello)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("cle", help="nouvelle cle par l'USB")
    a.add_argument("port", help="port serie du C6, ex. /dev/cu.usbmodem101")
    for nom in ("session", "enregistre"):
        a = sub.add_parser(nom, help="session H1" if nom == "session" else "session H1 enregistree en .jsonl")
        a.add_argument("hote", help="hotte-sonde.local, nom SRP du produit (<nom>.local), ou adresse IPv4/IPv6")
        if nom == "enregistre":
            a.add_argument("scenario", help="nom du scenario (nom du fichier)")
            a.add_argument("--dossier", default=DOSSIER, help="dossier des enregistrements (defaut : logs/ du depot)")
        a.add_argument("commandes", nargs="*", help="commandes envoyees apres 'json 1', sans id=")
        a.add_argument("--duree", type=float, default=60.0 if nom == "session" else None,
                       help="duree en secondes (session : 60 ; enregistre : jusqu'a Ctrl-C)")
        a.add_argument("--port", type=int, default=PORT)
        if nom == "session":
            a.add_argument("--brut", action="store_true", help="JSON tel quel au lieu du resume")
    args = ap.parse_args(argv)
    if args.cmd == "cle":
        cmd_cle(args.port, chemin_cle(args.appareil) if os.environ.get("HOTTE_CLE") else None, args.appareil)
    elif args.cmd == "session":
        cmd_session(args.hote, args.port, args.commandes, args.duree, args.brut,
                    lire_cle(chemin_cle(args.appareil or "sonde")))
    else:
        cmd_enregistre(args.hote, args.port, args.scenario, args.commandes, args.duree, args.dossier,
                       lire_cle(chemin_cle(args.appareil or "sonde")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
