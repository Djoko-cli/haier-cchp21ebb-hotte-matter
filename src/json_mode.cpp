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
