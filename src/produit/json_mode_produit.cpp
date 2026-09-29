// Copie de src/json_mode.cpp de la sonde (commit 1cb2c7c), elle-meme copiee de
// benq-screenbar-halo-matter@c58a506 (adapte : profil du produit, instantanes
// et evenements du produit, sessions reseau sur UDP/Thread)
#include "json_mode_produit.h"

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

#include "alim.h"
#include "config_produit.h"
#include "fw_version.h"
#include "h1_proto.h"
#include "matter_hotte.h"
#include "net_udp_thread.h"
#include "produit.h"

using namespace jsonp;

// ===========================================================================
//  Etat
// ===========================================================================

// Identite du module (spec produit 7.3, hello.identite).
static constexpr char kFabricant[] = "Djoko-CLI";
static constexpr char kProduit[] = "Module hotte Haier";
static constexpr char kSeriePrefixe[] = "HOTTE-";
static constexpr char kNom[] = "Hotte";
static constexpr uint32_t kHw = 1;
static constexpr char kHwTxt[] = "C6 SuperMini, pilote " PILOTE_LIGNE_TEXTE;
static constexpr char kAppareil[] = "hotte";

static constexpr uint32_t kHbMs = 2000;  // battement quand les etat sont coupes ou lents
// Profil USB : etat 1 s, compteurs 1 s, reseau 5 s.
static constexpr uint32_t kPeriodDefault = 1000, kCountersDefault = 1000, kNetDefault = 5000;
static constexpr uint16_t kLeaseDefault = 30;
static constexpr uint8_t kReplies = 4;          // reponses differees (apres un instantane)
static constexpr uint32_t kHeapBlocMs = 10000;  // plus grand bloc du tas relu au plus toutes les 10 s
// Profil distant (7.6), comme la ScreenBar : etat 2 s, compteurs coupes (les
// essais les demandent : 'json compteurs 10000'), reseau 30 s ; trame_d et log
// coupes au depart. Retard admis dans la file (debit plafonne a 3000 o/s).
static constexpr uint32_t kRemotePeriod = 2000, kRemoteCounters = 0, kRemoteNet = 30000;
static constexpr uint32_t kRemoteLateMs = 6000;
static constexpr uint16_t kLogCap = 20;         // lignes log par seconde et par session
static constexpr uint16_t kTramesDCap = 10;     // lignes trame_d par seconde et par session (7.5)
static constexpr uint32_t kTramesDistantMs = 60000;  // trame_d coupees seules apres 60 s a distance

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
  bool cache;     // gardee pour un id repete (reseau) ; pas un refus deja_traite
  char cmd[kCmdTextMax + 1];
};

// Une session par transport (origine). L'USB (kUsb) existe toujours ; une
// origine reseau n'a de sens que tant que sa session H1 est etablie
// (net_udp_thread.cpp : jsonRemoteReset a chaque changement).
struct Sink {
  bool machine = false;
  uint32_t periodMs = kPeriodDefault, countersMs = kCountersDefault, netMs = kNetDefault;
  uint16_t leaseS = kLeaseDefault;
  bool frames = false, log = false;  // trame_d et log coupes au depart
  uint32_t framesAt = 0;             // 'json trames 1' a distance : coupees seules apres 60 s
  uint32_t nextEtat = 0, nextCpt = 0, nextNet = 0, nextHb = 0;
  uint32_t lastRx = 0, lastCmd = 0;  // bail : dernier octet recu, fin de la derniere commande
  uint32_t n = 0;                    // n de la prochaine ligne produite sur ce transport
  uint32_t lost = 0, tooLong = 0, rejected = 0;
  uint32_t loopMaxMs = 0;            // plus long tour de loop() depuis le bloc sys emis
  uint32_t topId = 0;                // reseau : plus haut id admis dans cette session (jsonRemoteAdmit)
  bool nvsLog = false;               // log des valeurs hors bornes en NVS a emettre (drain)
  Queue q;
  PendingReply replies[kReplies] = {};
  RateCap trameCap{kTramesDCap}, logCap{kLogCap};
  Cadence cadence;
};
static Sink sSinks[kOrigins];
static ReplyCache sCache[kOrigins - 1];  // origines reseau seulement
static uint8_t sOrigin = kUsb;  // origine de la commande en cours

// Compteurs du module depuis le demarrage, toutes sessions ('json').
static uint32_t sPerdues = 0, sSautes = 0, sRejets = 0;
static bool sConfigChanged = false;
static uint32_t sLoopAt = 0;
// Ordres de l'app en cours (jsonOrdreApp) : l'evenement sequence porte l'id
// de la commande dans la session qui l'a envoyee, null ailleurs.
struct OrdreApp {
  uint32_t numero = 0;  // numero d'ordre donne a l'automate (0 : place libre)
  uint32_t idCmd = 0;
  uint8_t origine = kUsb;
};
static constexpr uint8_t kOrdresApp = 8;
static OrdreApp sOrdres[kOrdresApp];
static uint32_t sNumeroOrdre = 0;
// heap_caps_get_largest_free_block() parcourt tout le tas en section critique :
// relu au plus toutes les kHeapBlocMs, et a chaque 'json 1' ou 'json etat'.
static uint32_t sHeapBloc = 0, sHeapBlocAt = 0;
static bool sHeapBlocStale = true;

static uint32_t upS() { return (uint32_t)(esp_timer_get_time() / 1000000); }
static bool remote(uint8_t o) { return o != kUsb; }

// ===========================================================================
//  Emission
// ===========================================================================

// Place d'emission libre sur ce transport : octets du tampon de HWCDC (USB),
// ou places de la file des datagrammes x une ligne (reseau).
static int room(uint8_t o) {
  if (o == kUsb) return Serial.availableForWrite();
  return (int)netUdpFreeSlots() * (int)kLineMax;
}

// Ecrit la ligne fermee de sW sur ce transport, entiere ou pas du tout.
static bool emit(uint8_t o) {
  const size_t len = sW.size();
  if (o == kUsb) {
    if (Serial.availableForWrite() < (int)len) return false;
    Serial.write(sW.data(), len);
    return true;
  }
  // Un datagramme : l'objet JSON seul, sans RS ni LF (ScreenBar 10.2).
  return len >= 2 && netUdpSend((uint8_t)(o - 1), sW.data() + 1, len - 2);
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

// Evenement frequent (trame_d, log) vers une session reseau : seulement s'il
// reste ensuite la place d'une ligne periodique ou d'une reponse (deux
// datagrammes libres). Sinon perdu, n consomme et compte.
static bool eventRoom(uint8_t o) {
  if (!remote(o) || room(o) >= 2 * (int)kLineMax) return true;
  sSinks[o].n++;
  lose(sSinks[o]);
  return false;
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

void jsonTexteUsb(const char *txt) {
  const size_t len = strlen(txt);
  if (Serial.availableForWrite() < (int)(len + 2)) {
    lose(sSinks[kUsb]);
    return;
  }
  Serial.write((const uint8_t *)txt, len);
  Serial.write((const uint8_t *)"\r\n", 2);
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
  sW.str("build", "produit");
  sW.str("reseau_build", "thread");
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
  sW.arr("caps");  // jamais "injection" (7.3)
  for (const char *cap : {"hotte", "matter", "udp", "cle", "log", "trames_d", "alim", "thermique", "essai"})
    sW.str(nullptr, cap);
#if PILOTE_SIMULE
  sW.str(nullptr, "simule");
#endif
  sW.end();
  sW.obj("thermique");
  const int32_t pose = produitSurveillance().maxPose();
  if (pose != surv::Surveillance::kAucun) sW.i32("temp_max_pose_c", degres(pose));
  else sW.null("temp_max_pose_c");
  sW.end();
}

static void config(uint8_t o, uint32_t now) {
  const hotte::Params &a = produitAutomate().params();
  const surv::Params &s = produitSurveillance().params();
  sW.begin("config", sSinks[o].n, now);
  sW.obj("automate");  // noms de 'hotte regle' (4.2)
  for (uint8_t i = 0; i < hotte::kNbParams; i++) sW.u32(hotte::nomParam(i), hotte::valeurParam(a, i));
  sW.end();
  sW.obj("surveillance");
  for (uint8_t i = 0; i < surv::kNbParams; i++) sW.u32(surv::nomParam(i), surv::valeurParam(s, i));
  sW.end();
  sW.obj("ligne");
  sW.str("pilote", PILOTE_LIGNE_TEXTE);
  sW.str("mode", hotte::texte(produitPilote().mode()));
  sW.boolean("annexe", produitPilote().annexe());
  sW.end();
#if PILOTE_SIMULE
  const sim::Reglages &r = produitSimu().reglages();
  sW.obj("simu");
  sW.u32("latence_ms", r.latenceMs);
  sW.u32("periode_ms", r.periodeMs);
  sW.u32("trame_ms", r.trameMs);
  sW.u32("annexe_ms", r.annexeMs);
  sW.u32("prolongee_ms", r.prolongeeMs);
  sW.str("fin", r.finArmee ? "armee" : "eteinte");
  sW.str("inconnues", sim::texte(r.inconnues));
  sW.boolean("refuse", produitSimu().refuse());
  sW.boolean("hors_service", produitSimu().horsService());
  sW.end();
#else
  sW.null("simu");
#endif
}

static void etatHotteBloc(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "hotte");
  bootUp();
  etatHotte(sW, produitAutomate().etat(), now);
}

static void etatAutomateBloc(uint8_t o, uint32_t now) {
  const hotte::Automate &a = produitAutomate();
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "automate");
  bootUp();
  etatAutomate(sW, a.diagnostic(now), a.mode(), a.annexe(), now);
}

static void etatMatterBloc(uint8_t o, uint32_t now) {
  MatterEtat m;
  matterHotteEtat(&m, now);  // verrous pris sans attente et rendus avant le formatage
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "matter");
  bootUp();
  etatMatter(sW, m);
}

static void etatThermiqueBloc(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "thermique");
  bootUp();
  etatThermique(sW, produitSurveillance());
}

static void etatAlimBloc(uint8_t o, uint32_t now) {
  sW.begin("etat", sSinks[o].n, now);
  sW.str("bloc", "alim");
  bootUp();
  etatAlim(sW, produitSurveillance());
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

static void compteursHotteBloc(uint8_t o, uint32_t now) {
  sW.begin("compteurs", sSinks[o].n, now);
  sW.str("bloc", "hotte");
  compteursHotte(sW, produitAutomate().diagnostic(now).c);
}

static void compteursAlimBloc(uint8_t o, uint32_t now) {
  sW.begin("compteurs", sSinks[o].n, now);
  sW.str("bloc", "alim");
  compteursAlim(sW, produitSurveillance());
}

static void compteursRadioBloc(uint8_t o, uint32_t now) {
  RadioCompteurs r;
  matterHotteRadio(&r);  // verrou OpenThread pris sans attente, rendu avant le formatage
  sW.begin("compteurs", sSinks[o].n, now);
  sW.str("bloc", "radio");
  compteursRadio(sW, r);
}

// Une reponse vient de partir vers une origine reseau : gardee pour un id repete.
static void cacheReply(uint8_t o, const Reply &r) {
  if (remote(o) && r.fin) sCache[o - 1].put(r);
}

// Une ligne de la file de la session o : formatee maintenant, avec les
// valeurs du moment.
static void produce(uint8_t o, const Queued &q, uint32_t now) {
  if (!claim()) return;
  Sink &k = sSinks[o];
  Reply sent;
  bool isReply = false;
  switch (q.item) {
    case Item::HelloBase: helloBase(o, now); break;
    case Item::HelloId: helloIdentity(o, now); break;
    case Item::Config: config(o, now); break;
    case Item::EtatHotte: etatHotteBloc(o, now); break;
    case Item::EtatAutomate: etatAutomateBloc(o, now); break;
    case Item::EtatMatter: etatMatterBloc(o, now); break;
    case Item::EtatThermique: etatThermiqueBloc(o, now); break;
    case Item::EtatAlim: etatAlimBloc(o, now); break;
    case Item::EtatSys: etatSys(o, now); break;
    case Item::CompteursHotte: compteursHotteBloc(o, now); break;
    case Item::CompteursAlim: compteursAlimBloc(o, now); break;
    case Item::CompteursRadio: compteursRadioBloc(o, now); break;
    case Item::NetIp:
      sW.begin("reseau", k.n, now);
      sW.str("bloc", "ip");
      netUdpJson(sW, now);
      break;
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
      sent = r;
      isReply = p.cache;
      break;
    }
  }
  // Le maximum n'est remis a 0 que s'il est parti : perdue, la ligne suivante le porte.
  if (send(o) && q.item == Item::EtatSys) k.loopMaxMs = 0;
  // Perdue ou non, une reponse est donnee pour cet id : un id repete la renvoie
  // (sauf un refus deja_traite, jamais garde).
  if (isReply) cacheReply(o, sent);
}

// Reglages hors bornes lus en NVS au demarrage (4.2) : un log a chaque
// ouverture de session, apres l'instantane de 'json 1' et sa reponse, meme
// sans 'json log 1' (le texte du demarrage part en general avant que le Mac
// n'ouvre le port). Hors plafond des log : une ligne par session.
static void logNvs(uint8_t o, uint32_t now) {
  Sink &k = sSinks[o];
  k.nvsLog = false;
  if (!claim()) return;
  char txt[64];
  snprintf(txt, sizeof(txt), "%u jeu(x) de reglages hors bornes en NVS : valeurs par defaut",
           (unsigned)produitHorsBornes());
  logLine(sW, k.n, now, "produit", "notice", txt, 0);
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
  push(o, Item::EtatHotte, now, session);
  push(o, Item::EtatAutomate, now, session);
  push(o, Item::EtatMatter, now, session);
  push(o, Item::EtatThermique, now, session);
  push(o, Item::EtatAlim, now, session);
  push(o, Item::EtatSys, now, session);
}

static void pushCounters(uint8_t o, uint32_t now, bool session) {
  push(o, Item::CompteursHotte, now, session);
  push(o, Item::CompteursAlim, now, session);
  push(o, Item::CompteursRadio, now, session);
}

static void pushNet(uint8_t o, uint32_t now, bool session) { push(o, Item::NetIp, now, session); }

static void pushHello(uint8_t o, uint32_t now, bool session) {
  push(o, Item::HelloBase, now, session);
  push(o, Item::HelloId, now, session);
  push(o, Item::Config, now, session);
}

// ===========================================================================
//  Reponses (vers l'origine de la commande en cours)
// ===========================================================================

// Ecrit la reponse tout de suite (perdue et comptee si elle ne tient pas).
// Perdue ou non, elle est donnee pour cet id : gardee (reseau) pour un renvoi.
static void replyEmit(uint8_t o, const Reply &r, bool cache) {
  if (claim()) {
    reply(sW, sSinks[o].n, millis(), r);
    send(o);
  } else {
    // Ligne deja en cours de formatage (jamais vu) : perdue, n consomme et compte.
    sSinks[o].n++;
    lose(sSinks[o]);
  }
  if (cache) cacheReply(o, r);
}

// Reponse par la file de la session o (apres les lignes deja en file) ; sans
// place (4 reponses en attente, file pleine) : tout de suite. Differee, elle
// perd son msg (il pointerait sur un tampon disparu).
static void replyQueue(uint8_t o, const Reply &r, uint32_t t0, bool durAtSend, bool cache = true) {
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
    p.cache = cache;
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
  replyEmit(o, now, cache);
}

// Reponse immediate. Reseau : la cle n'y part jamais ; sans place, ou derriere
// une reponse deja en file (l'ordre des reponses est garde), elle attend dans
// la file de la session au lieu d'etre perdue.
static void replyTo(uint8_t o, const Reply &r, bool cache = true) {
  Reply out = r;
  if (remote(o)) out.key = nullptr;
  if (remote(o) && (room(o) < (int)kLineMax || sSinks[o].q.has(Item::Reply))) {
    replyQueue(o, out, 0, false, cache);
    return;
  }
  replyEmit(o, out, cache);
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
  k.frames = false;  // trame_d : diagnostic, a la demande ('json trames 1')
  k.leaseS = leaseS;
  k.log = false;
  k.nvsLog = produitHorsBornes() > 0;  // apres l'instantane (drain), meme sans 'json log 1'
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

void jsonRemoteReset(uint8_t origin) {
  if (origin == kUsb || origin >= kOrigins) return;
  // Sur place (un Sink temporaire couterait pres de 2 Ko de pile). n continue :
  // numero de ligne du transport depuis le demarrage ; le reste repart des
  // valeurs par defaut.
  Sink &k = sSinks[origin];
  k.machine = false;
  k.periodMs = kPeriodDefault;
  k.countersMs = kCountersDefault;
  k.netMs = kNetDefault;
  k.leaseS = kLeaseDefault;
  k.frames = false;
  k.log = false;
  k.nextEtat = k.nextCpt = k.nextNet = k.nextHb = 0;
  k.lastRx = k.lastCmd = 0;
  k.lost = k.tooLong = k.rejected = 0;
  k.loopMaxMs = 0;
  k.topId = 0;
  k.q.clear();
  for (PendingReply &p : k.replies) p.used = false;
  k.trameCap = RateCap(kTramesDCap);
  k.logCap = RateCap(kLogCap);
  k.cadence = Cadence();
  sCache[origin - 1].clear();
}

void jsonNoteRemoteRx(uint8_t origin) {
  if (origin != kUsb && origin < kOrigins) sSinks[origin].lastRx = millis();
}

bool jsonRemoteAdmit(uint32_t id, const char *shown) {
  const uint8_t o = sOrigin;
  if (o == kUsb || o >= kOrigins) return false;
  Sink &k = sSinks[o];
  // Reponse differee de cet id encore en file (instantane) : elle partira.
  for (const PendingReply &p : k.replies)
    if (p.used && p.r.id == id) return true;
  const Reply *cached = sCache[o - 1].find(id);
  if (cached && !strcmp(cached->cmd, shown)) {
    // Meme id, meme commande : la reponse perdue repart, rien n'est reexecute.
    Reply out = *cached;
    char cmd[kCmdTextMax + 1];
    copyCmd(cmd, cached->cmd);  // put() va reecrire l'entree : plus de pointeur dedans
    out.cmd = cmd;
    replyTo(o, out);
    return true;
  }
  if (cached || id <= k.topId) {
    // id deja traite, reponse plus en cache (ou autre commande sous le meme id) :
    // jamais de nouvelle execution. Refus non garde (le cache reste celui de l'id).
    Reply r;
    r.id = id;
    r.cmd = shown;
    r.ok = false;
    r.code = "deja_traite";
    r.msg = "id deja traite (reponse plus disponible) : rien n'est reexecute";
    replyTo(o, r, false);
    return true;
  }
  k.topId = id;
  // Nouvelle commande : la session sert de nouveau, meme apres un 'json 0'
  // (qui, s'il est cette commande, la termine ensuite a son tour).
  netUdpResume((uint8_t)(o - 1));
  return false;
}

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
  for (uint8_t o = 1; o < kOrigins; o++) {
    const Sink &r = sSinks[o];
    if (!r.n && !r.machine) continue;
    out.printf("  reseau %u : mode machine %s ; n = %lu, %lu perdue(s), %lu refusee(s)\n", (unsigned)o,
               r.machine ? "actif" : "coupe", (unsigned long)r.n, (unsigned long)r.lost, (unsigned long)r.rejected);
  }
  out.printf("  module   : %lu ligne(s) perdue(s), %lu trame(s) sautee(s), %lu rejet(s), toutes sessions\n",
             (unsigned long)sPerdues, (unsigned long)sSautes, (unsigned long)sRejets);
  out.printf("  demarrage : boot %08lX\n", (unsigned long)sBoot);
  out.println("  protocole : docs/PROTOCOLE-JSON-PRODUIT.md (v1, profil hotte du produit)");
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

// 'json cle ...' (ScreenBar 10.4) : USB seulement. La liste blanche la refuse
// deja au reseau ; refusee ici aussi (defense en profondeur : la reponse de
// 'nouvelle' porte la cle).
static void keyCommand(char *p, const JsonCmd &c) {
  if (sOrigin != kUsb) {
    replyNow(c, false, "interdite", "json cle : USB seulement");
    return;
  }
  const char *w = nextWord(p);
  char kid[9] = {};
  if (!*w) {
    const bool has = netUdpKid(kid);
    Reply r;
    r.id = c.id;
    r.cmd = c.cmd;
    r.durMs = millis() - c.t0;
    r.hasKid = true;
    r.kid = has ? kid : nullptr;
    if (c.hasId) jsonReply(r);
    else if (has) Serial.printf("json cle : empreinte %s (transport reseau actif, port UDP %u)\n", kid, kPortUdp);
    else Serial.println("json cle : aucune cle, transport reseau coupe");
    return;
  }
  if (!strcmp(w, "nouvelle")) {
    const char *hex = nextWord(p);
    uint8_t appRandom[32];
    bool hexOk = strlen(hex) == 64 && !*nextWord(p);
    for (uint8_t i = 0; hexOk && i < 32; i++) {
      auto nib = [](char ch) -> int {
        return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
      };
      const int hi = nib(hex[2 * i]), lo = nib(hex[2 * i + 1]);
      if (hi < 0 || lo < 0) hexOk = false;
      else appRandom[i] = (uint8_t)(hi << 4 | lo);
    }
    if (!c.hasId) {
      // La cle ne s'affiche jamais en texte : 'pio device monitor' enregistre la
      // session (log2file) dans un fichier a la racine du depot.
      h1::wipe(appRandom, sizeof(appRandom));
      Serial.println("json cle nouvelle : reservee a l'outil (ligne avec id=, la cle part dans la reponse) :");
      Serial.println("  python3 tools/hotte_udp.py cle <port>");
      return;
    }
    if (!hexOk) {
      h1::wipe(appRandom, sizeof(appRandom));
      replyNow(c, false, "usage", "json cle nouvelle <64 hexa majuscules> (alea de l'app)");
      return;
    }
    // La reponse est la seule copie de la cle : pas de cle neuve si elle ne peut
    // pas partir tout de suite (tampon d'emission USB plein).
    if (Serial.availableForWrite() < (int)kLineMax) {
      h1::wipe(appRandom, sizeof(appRandom));
      replyNow(c, false, "refuse", "tampon USB plein : rien n'est change, reessayer");
      return;
    }
    char keyHex[65];
    const NetKeyResult res = netUdpKeyNew(appRandom, keyHex, kid);
    h1::wipe(appRandom, sizeof(appRandom));
    if (res == NetKeyResult::Crypto || res == NetKeyResult::Nvs) {
      replyNow(c, false, "refuse",
               res == NetKeyResult::Nvs ? "cle non ecrite (NVS) : ancienne cle gardee"
                                        : "cle non creee (crypto) : ancienne cle gardee");
      return;
    }
    Reply r;
    r.id = c.id;
    r.cmd = c.cmd;
    r.durMs = millis() - c.t0;
    r.msg = res == NetKeyResult::Ok ? "nouvelle cle : les sessions reseau tombent"
                                    : "cle ecrite mais pas chargee : transport reseau coupe jusqu'au redemarrage";
    r.key = keyHex;
    r.hasKid = true;
    r.kid = kid;
    jsonReply(r);
    h1::wipe(keyHex, sizeof(keyHex));
    return;
  }
  if (!strcmp(w, "efface") && !*nextWord(p)) {
    const bool ok = netUdpKeyErase();
    if (c.hasId) {
      Reply r;
      r.id = c.id;
      r.cmd = c.cmd;
      r.ok = ok;
      r.code = ok ? "ok" : "refuse";
      r.msg = ok ? "cle effacee : transport reseau coupe" : "effacement NVS en echec (cle retiree de la memoire)";
      r.durMs = millis() - c.t0;
      r.hasKid = true;
      r.kid = nullptr;
      jsonReply(r);
    } else {
      Serial.println(ok ? "json cle : cle effacee, transport reseau coupe"
                        : "json cle : effacement NVS en echec (cle retiree de la memoire)");
    }
    return;
  }
  replyNow(c, false, "usage", "json cle [nouvelle <64 hexa> | efface]");
  if (!c.hasId) Serial.println("Usage : json cle [efface]   ('json cle nouvelle' : tools/hotte_udp.py cle <port>)");
}

void jsonCommand(char *arg, const JsonCmd &c) {
  char *p = arg;
  const char *sub = nextWord(p);
  const uint32_t now = millis();
  const uint8_t o = sOrigin;
  Sink &k = sSinks[o];
  static const char *const kUsage =
      "json [1 [bail 0|10..600] | 0 | etat | hello | ping | periode ms | compteurs ms | reseau ms | "
      "trames 0|1 | log 0|1 | cle]";
  // Reseau : bornes propres (liste blanche, spec produit 7.5), verifiees ici aussi.
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
      // Le client s'en va : sa place revient au suivant sans attendre 30 s (ScreenBar 10.4).
      if (rem) netUdpEnd((uint8_t)(o - 1));
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
      if (frames) k.framesAt = now;  // a distance : coupees seules 60 s plus tard
      sessionChanged = true;
    }
  } else if (!strcmp(sub, "cle")) {
    keyCommand(p, c);
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

uint32_t jsonOrdreApp(uint32_t idCmd) {
  if (++sNumeroOrdre == 0) sNumeroOrdre = 1;
  OrdreApp &o = sOrdres[sNumeroOrdre % kOrdresApp];  // le plus ancien cede sa place
  o.numero = sNumeroOrdre;
  o.idCmd = idCmd;
  o.origine = sOrigin;
  return sNumeroOrdre;
}

void jsonSequence(const hotte::Action &fin) {
  const uint32_t now = millis();
  OrdreApp ordre;
  if (fin.canal == hotte::Canal::App && fin.idOrdre)
    for (OrdreApp &o : sOrdres)
      if (o.numero == fin.idOrdre) {
        ordre = o;
        o.numero = 0;  // un seul evenement par ordre
      }
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    // L'origine attend l'evenement de son id, meme hors mode machine (comme
    // l'injection de la sonde) ; les autres le recoivent en mode machine.
    const bool attendu = ordre.numero && o == ordre.origine && ordre.idCmd;
    if ((!k.machine && !attendu) || !claim()) continue;
    evtSequence(sW, k.n, now, fin, o == ordre.origine ? ordre.idCmd : 0);
    send(o);
  }
}

void jsonHotte(const hotte::Etat &avant, const hotte::Etat &apres, hotte::Origine origine) {
  const uint32_t now = millis();
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || !claim()) continue;
    evtHotte(sW, k.n, now, avant, apres, origine);
    send(o);
  }
}

void jsonAlerte(const surv::Alerte &a) {
  const uint32_t now = millis();
  // Toujours emise vers chaque session, sans abonnement ; l'USB la recoit
  // aussi hors mode machine, en texte (7.4). Aucune action sur la hotte.
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || !claim()) continue;
    evtAlerte(sW, k.n, now, a);
    send(o);
  }
  char txt[96];
  if (a.sujet == surv::Sujet::Temperature || a.sujet == surv::Sujet::LectureTemperature)
    snprintf(txt, sizeof(txt), "alerte %s %s : %ld C (seuil %ld C)", surv::texte(a.sujet),
             a.debut ? "debut" : "fin", (long)degres(a.valeur), (long)degres(a.seuil));
  else
    snprintf(txt, sizeof(txt), "alerte %s %s : %ld mV (seuil %ld mV)", surv::texte(a.sujet),
             a.debut ? "debut" : "fin", (long)a.valeur, (long)a.seuil);
  // Doublee d'un log notice, meme sans 'json log 1'.
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || k.log || !claim()) continue;  // 'json log 1' : jsonLog l'emet ci-dessous
    logLine(sW, k.n, now, "produit", "notice", txt, 0);
    send(o);
  }
  if (!jsonLog("produit", "notice", txt) && !sSinks[kUsb].machine) jsonTexteUsb(txt);
}

void jsonTrameD(const TrameD &t) {
  const uint32_t now = millis();
  for (uint8_t o = 0; o < kOrigins; o++) {
    Sink &k = sSinks[o];
    if (!k.machine || !k.frames) continue;
    if (!k.trameCap.available(now)) {
      k.trameCap.skip();  // aucun n consomme ; la suivante produite porte 'sautes'
      sSautes++;
      continue;
    }
    if (!eventRoom(o) || !claim()) continue;
    k.trameCap.take();
    evtTrameD(sW, k.n, now, t, k.trameCap.takeSkipped());
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
    if (!eventRoom(o)) continue;
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

  // L'USB d'abord ; les sessions reseau, qui partagent la file des
  // datagrammes et son debit, a tour de role (deux instantanes simultanes
  // avancent ensemble).
  static uint8_t sTurn = 0;
  sTurn++;
  for (uint8_t i = 0; i < kOrigins; i++) {
    const uint8_t o = i == 0 || kOrigins < 3 ? i : (uint8_t)(1 + ((i - 1 + sTurn) % (kOrigins - 1)));
    Sink &k = sSinks[o];
    if (k.machine && leaseExpired(now, k.lastRx, k.lastCmd, k.leaseS)) leaveMachine(o, true, now);
    // trame_d a distance : coupees seules apres 60 s (7.5) ; hello.base le dit.
    if (remote(o) && k.frames && now - k.framesAt >= kTramesDistantMs) {
      k.frames = false;
      if (k.machine) push(o, Item::HelloBase, now, true);
    }
    if (k.machine) {
      if (due(k.nextEtat, k.periodMs, now)) pushState(o, now, true);
      if (due(k.nextCpt, k.countersMs, now)) pushCounters(o, now, true);
      if (due(k.nextNet, k.netMs, now)) pushNet(o, now, true);
      if ((!k.periodMs || k.periodMs > kHbMs) && due(k.nextHb, kHbMs, now)) push(o, Item::Heartbeat, now, true);
    }
    drain(o, now);
  }
}
