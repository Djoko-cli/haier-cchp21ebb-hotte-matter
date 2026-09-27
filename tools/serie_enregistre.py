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
