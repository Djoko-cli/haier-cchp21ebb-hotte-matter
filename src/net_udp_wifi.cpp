// Copie de benq-screenbar-halo-matter@c58a506 : src/net_udp.cpp (adapte : sockets lwIP IPv4 sur le Wi-Fi au lieu d'OpenThread, reception lue dans la tache loop, h1::Peer en IPv4 mappee, file de 32 datagrammes, 50 000 o/s, credit de 8 192, NVS hotte/cle, bloc reseau ip de la sonde)
#include "net_udp_wifi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <errno.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <string.h>

#include "cli.h"
#include "config.h"
#include "h1_proto.h"
#include "json_mode.h"
#include "net_wifi.h"

// Une origine JSON par session etablie, apres l'USB (json_mode.cpp).
static_assert(jsonp::kOrigins == h1::kSlots + 1, "une origine JSON par session H1 etablie");

static const char *const kNvsKey = "cle";  // dans l'espace kNvsEspace ("hotte")

// Commande de 127 octets et son en-tete de 56 : 256 laisse de la marge ; au-dela,
// le datagramme n'est pas pour nous (compte, jete).
static constexpr size_t kRxMax = 256;
static constexpr uint8_t kRxPerTurn = 4;  // la file de reception de lwIP en garde 6
// En-tete H1 et ligne machine sans RS ni LF : 56 + 1022 octets, sous la MTU
// de 1 500 du Wi-Fi (jamais fragmente).
static constexpr size_t kTxMax = h1::kHeaderMax + jsonp::kLineMax - 2;
static constexpr uint8_t kTxN = 32;
static constexpr uint32_t kTxStaleMs = 4000;  // pas parti dans ce delai : perdu
static constexpr uint8_t kTxPerTurn = 4;      // datagrammes remis a lwIP par tour de loop()
// Debit moyen plafonne (spec 8.5) : 50 000 octets/s, credit de 8 192 (huit
// lignes d'un Ko d'affilee). Au repos, le profil distant en consomme moins
// d'un Ko/s ; 100 trames pleines par seconde (~86 Ko/s) depasseraient le
// plafond : la file se remplit, les lignes en trop sont perdues et comptees.
static constexpr uint32_t kTxBytesPerS = 50000;
static constexpr uint32_t kTxBurst = 8192;
static constexpr uint32_t kExpireMs = 1000;
static constexpr uint32_t kOpenRetryMs = 1000;

struct TxItem {
  uint32_t at;
  bool errCounted;  // tx_erreurs compte une fois par datagramme
  uint8_t slot;     // session qui l'a scelle, kRawSlot : DEFI (sans session)
  uint16_t len;
  uint16_t port;
  uint8_t ip[4];    // IPv4 du destinataire, ordre du reseau
  uint8_t data[kTxMax];
};

static constexpr uint8_t kRawSlot = 0xFF;

static uint8_t sRx[kRxMax + 1];  // un octet de plus : un datagramme trop grand se reconnait
static TxItem sTx[kTxN];
static uint8_t sTxHead = 0, sTxN = 0;

static h1::Table sTable;
static int sSock = -1;
static uint32_t sOpenTryAt = 0, sExpireAt = 0;
static uint32_t sTxCredit = kTxBurst, sTxCreditAt = 0;

static struct {
  uint32_t rxTooBig, rxErr;  // perdus avant lecture : trop grands, erreurs de lwIP
  uint32_t rx, rejected, defis, tx, txLost, txErr;
} sSt = {};

// h1::Peer est en IPv6 (16 octets) : IPv4 mappee ::ffff:a.b.c.d (spec 8.5).
// local reste nul : une seule interface, lwIP choisit l'adresse source.
static h1::Peer peerOf(const sockaddr_in &a) {
  h1::Peer p;
  p.ip[10] = p.ip[11] = 0xFF;
  memcpy(p.ip + 12, &a.sin_addr.s_addr, 4);
  p.port = ntohs(a.sin_port);
  return p;
}

// ===========================================================================
//  File d'emission (tache loop)
// ===========================================================================

static TxItem *txTail() { return sTxN < kTxN ? &sTx[(sTxHead + sTxN) % kTxN] : nullptr; }

static void txPop() {
  if (!sTxN) return;
  sTxHead = (uint8_t)((sTxHead + 1) % kTxN);
  sTxN--;
}

static void txClear() {
  sSt.txLost += sTxN;
  sTxHead = sTxN = 0;
}

// Session partie (remplacee, oubliee) : ses datagrammes deja scelles ne
// prennent plus le debit de la suivante. Les autres gardent leur ordre.
static void txDropSlot(uint8_t slot) {
  uint8_t kept = 0;
  for (uint8_t i = 0; i < sTxN; i++) {
    const uint8_t from = (uint8_t)((sTxHead + i) % kTxN);
    if (sTx[from].slot == slot) {
      sSt.txLost++;
      continue;
    }
    const uint8_t to = (uint8_t)((sTxHead + kept) % kTxN);
    if (to != from) sTx[to] = sTx[from];
    kept++;
  }
  sTxN = kept;
}

// Datagramme sans session (DEFI) vers l'expediteur du SALUT : en tete de file,
// hors plafond de debit (82 octets, 2 par seconde au plus), pour qu'une
// poignee de main n'attende pas derriere l'instantane d'une autre session.
static void queueRaw(const h1::Peer &to, const uint8_t *d, size_t n, uint32_t now) {
  if (sTxN >= kTxN || n > kTxMax) {
    sSt.txLost++;
    return;
  }
  sTxHead = (uint8_t)((sTxHead + kTxN - 1) % kTxN);
  sTxN++;
  TxItem *t = &sTx[sTxHead];
  t->at = now;
  t->errCounted = false;
  t->slot = kRawSlot;
  t->len = (uint16_t)n;
  t->port = to.port;
  memcpy(t->ip, to.ip + 12, 4);
  memcpy(t->data, d, n);
}

bool netUdpSend(uint8_t slot, const uint8_t *json, size_t len) {
  if (slot >= h1::kSlots || len > jsonp::kLineMax - 2 || sSock < 0) return false;
  const h1::Session &s = sTable.slot(slot);
  TxItem *t = txTail();
  if (!s.used || !t) return false;
  char hdr[h1::kHeaderMax + 1];
  const size_t hn = sTable.seal(slot, json, len, hdr);
  if (!hn) return false;
  t->at = millis();
  t->errCounted = false;
  t->slot = slot;
  t->len = (uint16_t)(hn + len);
  t->port = s.peer.port;
  memcpy(t->ip, s.peer.ip + 12, 4);
  memcpy(t->data, hdr, hn);
  memcpy(t->data + hn, json, len);
  sTxN++;
  return true;
}

uint8_t netUdpFreeSlots() { return sSock >= 0 ? (uint8_t)(kTxN - sTxN) : 0; }

void netUdpEnd(uint8_t slot) { sTable.end(slot); }

void netUdpResume(uint8_t slot) { sTable.resume(slot); }

// Remet a lwIP les datagrammes en tete, sans jamais attendre.
static void txFlush() {
  // Heure lue ici, apres la mise en file : un datagramme date d'une
  // milliseconde plus tard que le debut de netUdpPoll() ne doit pas paraitre
  // vieux de 49 jours (comparaison signee).
  const uint32_t now = millis();
  while (sTxN && (int32_t)(now - sTx[sTxHead].at) > (int32_t)kTxStaleMs) {
    txPop();
    sSt.txLost++;
  }
  // Credit de debit (au plus 10 s rattrapees : pas de debordement du produit).
  const uint32_t elapsed = now - sTxCreditAt;
  sTxCreditAt = now;
  const uint32_t add = (elapsed > 10000 ? 10000 : elapsed) * kTxBytesPerS / 1000;
  sTxCredit = sTxCredit + add > kTxBurst ? kTxBurst : sTxCredit + add;
  if (!sTxN) return;
  if (sSock < 0) {
    txClear();
    return;
  }
  if (!netWifiUp()) return;  // Wi-Fi coupe : la file attend (kTxStaleMs au plus)
  for (uint8_t sent = 0; sTxN && sent < kTxPerTurn; sent++) {
    TxItem &t = sTx[sTxHead];
    if (t.slot != kRawSlot && sTxCredit < t.len) break;  // plafond : au tour suivant
    sockaddr_in to = {};
    to.sin_family = AF_INET;
    to.sin_port = htons(t.port);
    memcpy(&to.sin_addr.s_addr, t.ip, 4);
    if (lwip_sendto(sSock, t.data, t.len, 0, (const sockaddr *)&to, sizeof(to)) < 0) {
      if (!t.errCounted) sSt.txErr++;
      t.errCounted = true;
      // Tampons du Wi-Fi pleins : nouvel essai au tour suivant, jusqu'a kTxStaleMs.
      if (errno == ENOMEM || errno == EAGAIN || errno == EWOULDBLOCK) break;
      sSt.txLost++;  // refus durable (adresse, route) : perdu
      txPop();
      continue;
    }
    sSt.tx++;
    if (t.slot != kRawSlot) sTxCredit -= t.len;
    txPop();
  }
}

// ===========================================================================
//  Socket (tache loop)
// ===========================================================================

// Ouvert des qu'une cle existe et que le Wi-Fi a une adresse (la pile lwIP ne
// tourne qu'avec la station), puis tant que la cle existe (une coupure du
// Wi-Fi ne le ferme pas). Sans cle, le port reste ferme : lwIP repond "port
// injoignable".
static void syncSocket(uint32_t now) {
  const bool open = sSock >= 0;
  const bool want = sTable.hasKey() && (open || netWifiUp());
  if (want == open) return;
  if (!want) {
    lwip_close(sSock);
    sSock = -1;
    txClear();
    return;
  }
  if (sOpenTryAt && now - sOpenTryAt < kOpenRetryMs) return;
  sOpenTryAt = now ? now : 1;
  const int s = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0) return;
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(kPortUdp);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (lwip_bind(s, (const sockaddr *)&a, sizeof(a)) < 0) {
    lwip_close(s);
    return;
  }
  lwip_fcntl(s, F_SETFL, O_NONBLOCK);
  sSock = s;
}

// ===========================================================================
//  Datagrammes recus (tache loop)
// ===========================================================================

// Aleatoire de la plateforme pour nc et sid (tire seulement pour un SALUT
// admis) : vrai aleatoire, la radio Wi-Fi tourne.
static void fillRandom(void *p, size_t n) { esp_fill_random(p, n); }

// Chaque session partie (oubliee, remplacee, cle changee) : son etat JSON repart de zero.
static void resetOrigins(uint8_t mask) {
  for (uint8_t i = 0; i < h1::kSlots; i++)
    if (mask & (1u << i)) {
      txDropSlot(i);
      jsonRemoteReset((uint8_t)(i + 1));
    }
}

static void handle(const uint8_t *d, size_t len, const h1::Peer &from, uint32_t now) {
  const h1::Parsed p = h1::parse(d, len);

  if (p.kind == h1::Kind::Salut) {
    // Sans place pour le DEFI, la poignee de main en cours (celle d'un autre
    // client peut-etre) n'est pas remplacee.
    if (sTxN >= kTxN) {
      sSt.rejected++;  // pas encore authentifie : ne compte pas comme datagramme perdu
      return;
    }
    char defi[h1::kDefiLen + 1];
    if (sTable.onSalut(p, fillRandom, from, now, defi) != h1::Verdict::Ok) {
      sSt.rejected++;
      return;
    }
    sSt.defis++;
    queueRaw(from, (const uint8_t *)defi, h1::kDefiLen, now);
    return;
  }

  uint8_t slot = 0;
  bool fresh = false;
  if (sTable.onData(p, from, now, &slot, &fresh) != h1::Verdict::Ok) {
    sSt.rejected++;  // forme, sid inconnu, MAC faux, rejeu : silence
    return;
  }
  sSt.rx++;
  const uint8_t origin = (uint8_t)(slot + 1);
  if (fresh) resetOrigins((uint8_t)(1u << slot));  // session neuve, ou a la place d'une autre
  jsonNoteRemoteRx(origin);
  // La charge est une ligne de la CLI : 127 octets au plus (au-dela, refusee
  // avec son id, trop_long), ASCII imprimable (tout autre octet devient '?',
  // comme dans les chaines emises).
  char line[jsonp::kCmdMax + 1];
  const bool tooLong = p.payloadLen > jsonp::kCmdMax;
  const size_t n = tooLong ? jsonp::kCmdMax : p.payloadLen;
  for (size_t i = 0; i < n; i++) {
    const uint8_t c = p.payload[i];
    line[i] = c >= 0x20 && c <= 0x7E ? (char)c : '?';
  }
  line[n] = 0;
  cliRunRemote(origin, line, tooLong);
}

static void rxPoll(uint32_t now) {
  for (uint8_t i = 0; i < kRxPerTurn && sSock >= 0; i++) {
    sockaddr_in from = {};
    socklen_t fromLen = sizeof(from);
    const int n = lwip_recvfrom(sSock, sRx, sizeof(sRx), MSG_DONTWAIT, (sockaddr *)&from, &fromLen);
    if (n < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) sSt.rxErr++;
      return;  // rien de plus a lire ce tour-ci
    }
    if ((size_t)n > kRxMax) {
      sSt.rxTooBig++;
      continue;
    }
    if (from.sin_family != AF_INET) {
      sSt.rejected++;
      continue;
    }
    handle(sRx, (size_t)n, peerOf(from), now);
  }
}

// ===========================================================================
//  Cle (NVS hotte/cle)
// ===========================================================================

static void loadKey() {
  Preferences p;
  // Ouverture en ecriture meme pour lire (reglages.cpp) ; isKey() d'abord.
  if (!p.begin(kNvsEspace, false)) return;
  uint8_t k[h1::kKeyLen];
  const bool ok = p.isKey(kNvsKey) && p.getBytes(kNvsKey, k, sizeof(k)) == sizeof(k);
  p.end();
  if (ok) sTable.setKey(k);
  h1::wipe(k, sizeof(k));
}

// Toutes les sessions tombent (nouvelle cle, cle effacee) : leurs datagrammes aussi.
static void dropAll() {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < h1::kSlots; i++)
    if (sTable.slot(i).used) mask |= (uint8_t)(1u << i);
  resetOrigins(mask);
  txClear();
}

NetKeyResult netUdpKeyNew(const uint8_t appRandom[32], char keyHex[65], char kid[9]) {
  uint8_t card[32], key[h1::kKeyLen];
  esp_fill_random(card, sizeof(card));
  const h1::Part part = {card, sizeof(card)};
  char newKid[h1::kKidHex + 1];
  const bool made = h1::hmacSha256(appRandom, 32, &part, 1, key) && h1::keyId(key, newKid);
  h1::wipe(card, sizeof(card));
  if (!made) {
    h1::wipe(key, sizeof(key));
    return NetKeyResult::Crypto;
  }
  Preferences p;
  const bool written = p.begin(kNvsEspace, false) && p.putBytes(kNvsKey, key, sizeof(key)) == sizeof(key);
  p.end();
  if (!written) {  // NVS : ecriture atomique par entree, l'ancienne cle reste
    h1::wipe(key, sizeof(key));
    return NetKeyResult::Nvs;
  }
  dropAll();
  const bool loaded = sTable.setKey(key);
  h1::toHex(key, sizeof(key), keyHex);
  memcpy(kid, newKid, h1::kKidHex + 1);
  h1::wipe(key, sizeof(key));
  return loaded ? NetKeyResult::Ok : NetKeyResult::Load;
}

bool netUdpKeyErase() {
  Preferences p;
  bool ok = p.begin(kNvsEspace, false);
  if (ok && p.isKey(kNvsKey)) ok = p.remove(kNvsKey);
  p.end();
  // Meme si la NVS refuse : plus de cle en memoire, plus de transport jusqu'au redemarrage.
  dropAll();
  sTable.setKey(nullptr);
  return ok;
}

bool netUdpKid(char kid[9]) {
  if (!sTable.hasKey()) return false;
  memcpy(kid, sTable.kid(), h1::kKidHex + 1);
  return true;
}

// ===========================================================================
//  Cycle de vie
// ===========================================================================

void netUdpBegin() { loadKey(); }

void netUdpPoll() {
  const uint32_t now = millis();
  syncSocket(now);
  rxPoll(now);
  if (now - sExpireAt >= kExpireMs) {
    sExpireAt = now;
    resetOrigins(sTable.expire(now));
  }
  txFlush();
}

// ===========================================================================
//  JSON : bloc reseau ip
// ===========================================================================

void netUdpJson(jsonp::Writer &w, uint32_t /*now*/) {
  const bool up = netWifiUp();
  char ip[16];
  netWifiIp(ip);
  w.u32("frais_ms", 0);  // releve a la production de la ligne
  w.null("srp");         // pas de SRP : le nom se resout en mDNS (mdns.nom, cap mdns)
  w.arr("adresses");
  if (up) {
    w.obj(nullptr);
    w.str("adr", ip);
    w.str("type", "autre");
    w.boolean("pref", true);
    w.end();
  }
  w.end();
  w.obj("udp");
  w.u32("port", kPortUdp);
  w.boolean("ouvert", sSock >= 0);
  char kid[9];
  const bool hasKey = netUdpKid(kid);
  w.str("empreinte", hasKey ? kid : nullptr);  // l'app verifie qu'elle a la bonne cle
  w.u32("sessions", sTable.established());
  w.boolean("provisoire", sTable.provisional().used);
  w.u32("rx", sSt.rx);
  w.u32("rejets", sSt.rejected);
  w.u32("rx_perdus", sSt.rxTooBig + sSt.rxErr);
  w.u32("defis", sSt.defis);
  w.u32("tx", sSt.tx);
  w.u32("tx_perdus", sSt.txLost);
  w.u32("tx_erreurs", sSt.txErr);
  w.null("tampons_libres");  // tampons d'OpenThread : sans objet sur le Wi-Fi
  w.null("tampons_min");
  w.end();
  w.obj("mdns");
  char nom[40];
  snprintf(nom, sizeof(nom), "%s.local", kNomMdns);
  w.str("nom", nom);
  w.end();
  w.obj("wifi");
  w.boolean("connecte", up);
  if (up) w.i32("rssi_dbm", netWifiRssi());
  else w.null("rssi_dbm");
  w.str("ip", up ? ip : nullptr);
  w.u32("pertes", netWifiPertes());
  w.end();
}
