// Copie partielle de benq-screenbar-halo-matter@c58a506 : src/cli.cpp (adapte : runLine, cliPoll et cliRunRemote repris, commandes de la sonde)
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
#include "injection_regles.h"
#include "json_mode.h"
#include "json_out.h"
#include "net_udp_wifi.h"
#include "net_wifi.h"
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

static const char kUsageInjection[] = "usage : injection [regle <nom> <valeur>]";

static void afficherInjection() {
  const ReglagesSonde &r = sondeReglages();
  const inj::Params &p = r.injection;
  Serial.printf("injection : etage %s (GPIO%u)\n", r.injMontee ? "declare monte" : "non monte", (unsigned)kPinInjection);
  Serial.printf("  bas_max_us %lu, total_max_us %lu, silence_min_us %lu, attente_max_ms %lu, delai_min_ms %lu, "
                "arme_max_s %lu, tol_us %lu\n",
                (unsigned long)p.basMaxUs, (unsigned long)p.totalMaxUs, (unsigned long)p.silenceMinUs,
                (unsigned long)p.attenteMaxMs, (unsigned long)p.delaiMinMs, (unsigned long)p.armeMaxS,
                (unsigned long)p.tolUs);
}

// Reglages d'injection changes : NVS, config reemise.
static bool appliquerInjection(const ReglagesSonde &r) { return sondeAppliquer(r); }

// injection regle <nom> <valeur> : verifie avant d'ecrire en NVS (bornes : inj::bornesParam).
static Resultat faireRegle(char *args) {
  static char msg[jsonp::kMsgMax + 1];
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  uint32_t v = 0;
  if (!*args || !lireU32(valeur, &v) || *reste)
    return {false, "usage", "usage : injection regle <nom> <valeur> ('injection' : noms et valeurs)"};
  ReglagesSonde r = sondeReglages();
  uint32_t lo = 0, hi = 0;
  switch (inj::reglerParam(&r.injection, args, v)) {
    case inj::Reglage::NomInconnu:
      snprintf(msg, sizeof(msg), "injection regle : parametre %s inconnu ('injection' : noms)", args);
      return {false, "usage", msg};
    case inj::Reglage::HorsBornes:
      inj::bornesParam(args, &lo, &hi);
      snprintf(msg, sizeof(msg), "injection regle : %s %lu hors bornes (%lu..%lu, et bas_max_us <= total_max_us)",
               args, (unsigned long)v, (unsigned long)lo, (unsigned long)hi);
      return {false, "usage", msg};
    case inj::Reglage::Ok: break;
  }
  if (!appliquerInjection(r)) return {false, "refuse", "injection regle : echec de l'ecriture en NVS"};
  return {true, "ok", nullptr};
}

// injection [regle <nom> <valeur>] ; args vide : rien a faire.
static Resultat faireInjection(char *args) {
  char *reste = splitWord(args);
  if (!strcmp(args, "regle")) return faireRegle(reste);
  if (!*args) return {true, "ok", nullptr};
  return {false, "usage", kUsageInjection};
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
  if (!*netWifiSsid()) {
    Serial.println("wifi : non configure ('wifi <ssid> <mdp>')");
  } else {
    char ip[16];
    netWifiIp(ip);
    Serial.printf("wifi : %s, %s, IP %s, RSSI %d dBm, mDNS %s.local, %lu perte(s) depuis le demarrage\n",
                  netWifiSsid(), netWifiUp() ? "connecte" : "deconnecte (nouvel essai toutes les 10 s)", ip,
                  (int)netWifiRssi(), kNomMdns, (unsigned long)netWifiPertes());
  }
  char kid[9];
  if (netUdpKid(kid)) Serial.printf("udp : port %u, cle %s\n", (unsigned)kPortUdp, kid);
  else Serial.printf("udp : port %u ferme, aucune cle ('python3 tools/hotte_udp.py cle <port>')\n", (unsigned)kPortUdp);
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

// wifi <ssid> <mdp> : le mot de passe est le reste de la ligne (vide : reseau
// ouvert). Verifie avant d'ecrire en NVS ; USB seulement (liste blanche).
static void cmdWifi(char *args) {
  char *mdp = splitWord(args);
  if (!*args) {
    Serial.println("usage : wifi <ssid> <mdp>   (mdp absent : reseau ouvert)");
    return;
  }
  if (!netWifiValides(args, mdp)) {
    Serial.println("wifi : refuse (ssid de 1 a 32 caracteres sans espace ; mdp vide, de 8 a 63 caracteres, ou 64 hexa)");
    return;
  }
  if (!netWifiSet(args, mdp)) {
    Serial.println("wifi : echec de l'ecriture en NVS, rien ne change");
    return;
  }
  Serial.printf("wifi : identifiants enregistres, connexion a %s ('info' pour suivre)\n", args);
}

// injection [regle <nom> <valeur>] : USB seulement (liste blanche).
static void cmdInjection(char *args) {
  if (*args) {
    const Resultat r = faireInjection(args);
    if (r.msg) Serial.println(r.msg);
    if (!r.ok) return;
  }
  afficherInjection();
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
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log] : mode machine"},
  {"wifi", cmdWifi, "wifi <ssid> <mdp>"},
  {"injection", cmdInjection, "injection [regle <nom> <valeur>] : garde-fous de l'injection"},
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
// A distance, jsonRemoteAdmit passe avant tout refus (id deja vu).
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
  // A distance, d'abord l'id : une commande renvoyee (reponse perdue) recoit la
  // meme reponse sans nouvelle execution, avant tout refus de cadence qui
  // ecraserait sa reponse.
  if (remote && jsonRemoteAdmit(id, shown)) return;
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

void cliRunRemote(uint8_t origin, char *line, bool tooLong) {
  // Jamais l'USB par ce chemin (il echappe a la liste blanche).
  if (origin == jsonp::kUsb || origin >= jsonp::kOrigins) return;
  const uint8_t prev = jsonOrigin();
  jsonSetOrigin(origin);
  if (jsonOrigin() == origin) runLine(line, tooLong);
  jsonSetOrigin(prev);
}

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
