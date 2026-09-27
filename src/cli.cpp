// ===========================================================================
//  Console de la sonde sur l'USB (spec 8.3) : texte humain, 'help' liste tout.
//
//  Les commandes sont dans kCommandes : les taches suivantes y AJOUTENT des
//  lignes (et leurs handlers au-dessus), sans rien renommer. Les commandes
//  interdites a distance le sont par jsonp::remoteRefusal (json_out), pas par
//  la table.
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
  ReglagesSonde r = sondeReglages();
  if (!strcmp(args, "on")) {
    if (!captureActive() && !captureBegin(r.capture)) {
      Serial.println("capture : echec du demarrage du RMT");
      return;
    }
  } else if (!strcmp(args, "off")) {
    captureEnd();
  } else if (!strcmp(args, "tout") || !strcmp(args, "changements")) {
    r.changements = !strcmp(args, "changements");
    if (!sondeAppliquer(r)) Serial.println("capture : mode non enregistre en NVS");
  } else if (*args) {
    Serial.println("usage : capture on|off|tout|changements");
    return;
  }
  afficherReglages(sondeReglages());
}

// seuils [filtre_us] [silence_us] [resol_hz] : tout est verifie avant d'ecrire (spec 8.2).
static void cmdSeuils(char *args) {
  ReglagesSonde r = sondeReglages();
  uint32_t v[3] = {};
  uint8_t n = 0;
  for (char *w = args; *w;) {
    char *reste = splitWord(w);
    if (n == 3 || !lireU32(w, &v[n])) {
      Serial.println("usage : seuils [filtre_us 0..3] [silence_us] [resol_hz 1000000|500000]");
      return;
    }
    n++;
    w = reste;
  }
  if (!n) {
    afficherReglages(r);
    return;
  }
  capt::Reglages c = r.capture;
  if (!capt::filtreValide(v[0])) {
    Serial.printf("seuils : filtre %lu us hors bornes (0..%u)\n", (unsigned long)v[0], (unsigned)kFiltreMaxUs);
    return;
  }
  c.filtreUs = (uint8_t)v[0];
  if (n >= 3) {
    if (!capt::resolValide(v[2])) {
      Serial.printf("seuils : resolution %lu Hz refusee (1000000 ou 500000)\n", (unsigned long)v[2]);
      return;
    }
    c.resolHz = v[2];
  }
  if (n >= 2) c.silenceUs = v[1];
  if (!capt::silenceValide(c.silenceUs, c.resolHz)) {
    Serial.printf("seuils : silence %lu us hors bornes a %lu Hz (%lu..%lu)\n", (unsigned long)c.silenceUs,
                  (unsigned long)c.resolHz, (unsigned long)kSilenceMinUs, (unsigned long)silenceMaxUs(c.resolHz));
    return;
  }
  r.capture = c;
  if (!sondeAppliquer(r)) Serial.println("seuils : echec du RMT ou de la NVS");
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

// Une ligne de l'hote (tache 11 : prefixe id=, cadence, reponse).
static void runLine(char *line) {
  while (*line == ' ') line++;
  if (!executer(line)) Serial.printf("commande inconnue : %s ('help')\n", line);
}

// ---------------------------------------------------------------------------

static jsonp::LineAssembler sLine;  // 127 caracteres au plus

void cliBegin() {
  Serial.println("Tape 'help' pour la liste des commandes.");
  Serial.print("> ");
}

// Echo, effacement, Ctrl-U (vide la ligne), commande, invite.
void cliPoll() {
  while (Serial.available()) {
    const uint8_t c = (uint8_t)Serial.read();
    switch (sLine.feed(c, false)) {
      case jsonp::LineAssembler::Ev::Echo: Serial.print((char)c); break;
      case jsonp::LineAssembler::Ev::Erase: Serial.print("\b \b"); break;
      case jsonp::LineAssembler::Ev::Clear:
        for (uint8_t i = 0; i < sLine.cleared(); i++) Serial.print("\b \b");
        break;
      case jsonp::LineAssembler::Ev::Line:
        Serial.println();
        if (sLine.tooLong()) Serial.printf("ligne de plus de %u caracteres : ignoree\n", (unsigned)jsonp::kCmdMax);
        else runLine(sLine.text());
        sLine.reset();
        Serial.flush();  // l'USB CDC du C6 perd des octets si on enchaine trop vite
        Serial.print("> ");
        break;
      case jsonp::LineAssembler::Ev::None: break;
    }
  }
}
