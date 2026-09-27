// Copie de benq-screenbar-halo-matter@c58a506 : src/net_udp.h (adapte : UDP sur le Wi-Fi par les sockets lwIP, IPv4, port kPortUdp, NVS hotte/cle, files et plafonds de la sonde, sans #if MATTER_NET_THREAD)
#pragma once
// ===========================================================================
//  Transport reseau du protocole compagnon (docs/PROTOCOLE-JSON.md, section 9 ;
//  ScreenBar section 10)
//
//  Socket UDP de lwIP sur le Wi-Fi, IPv4, port kPortUdp (5480), enveloppe H1
//  (h1_proto.h) identique a la ScreenBar, cle partagee en NVS (hotte/cle).
//  L'app (Mac) joint la sonde par son nom mDNS (hotte-sonde.local) ou son
//  adresse IPv4.
//
//  Tout se passe dans la tache loop (spec 8.7 : un seul producteur) :
//  - reception : recvfrom non bloquant, au plus 4 datagrammes par tour (la
//    file de reception de lwIP en garde 6) ; poignee de main, verification,
//    commandes (cli.cpp : cliRunRemote) ;
//  - emission : datagrammes fermes (en-tete H1 + JSON) dans une file de 32,
//    remis a lwIP par sendto non bloquant, au plus 4 par tour ; debit moyen
//    plafonne a 50 000 octets/s, credit de 8 192 ; pas partis en 4 s :
//    perdus et comptes. Le DEFI d'une poignee de main passe devant, hors
//    plafond. Jamais d'attente : file pleine, la ligne est perdue et comptee
//    par l'appelant.
//  - le socket s'ouvre des qu'une cle existe et que le Wi-Fi a une adresse
//    (la pile lwIP ne demarre qu'avec la station) ; il se ferme a
//    l'effacement de la cle.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "json_out.h"

// setup(), apres netWifiBegin() : cle lue en NVS.
void netUdpBegin();
// loop(), apres netWifiPoll() : socket, datagrammes recus (poignees de main,
// commandes), oubli des sessions, emission.
void netUdpPoll();

// Ligne machine (objet JSON sans RS ni LF) pour la session etablie slot
// (0..h1::kSlots-1) : enveloppe H1 et mise en file. false : pas de session,
// ou file pleine (ligne perdue, comptee par l'appelant).
bool netUdpSend(uint8_t slot, const uint8_t *json, size_t len);
// Places libres dans la file d'emission (partagee par les sessions) ; 0 socket ferme.
uint8_t netUdpFreeSlots();
// 'json 0' execute pour la session slot : sa place revient au prochain client
// sans attendre 30 s de silence (h1::Table::end).
void netUdpEnd(uint8_t slot);
// Nouvelle commande admise de la session slot : elle sert de nouveau
// (h1::Table::resume).
void netUdpResume(uint8_t slot);

// Bloc 'reseau' 'ip' : schema de la ScreenBar (srp null, tampons null), plus
// mdns et wifi.
void netUdpJson(jsonp::Writer &w, uint32_t now);

// Cle (json cle, USB seulement). netUdpKeyNew : cle = HMAC-SHA256(alea de
// l'app, alea de la sonde), ecrite en NVS, rendue une fois en hexa (keyHex,
// 64 + 1) avec son empreinte (kid, 8 + 1) ; toutes les sessions tombent.
enum class NetKeyResult : uint8_t {
  Ok,
  Crypto,  // cle non calculee : rien ne change
  Nvs,     // cle non ecrite : rien ne change
  Load,    // cle ecrite en NVS mais pas chargee : transport coupe jusqu'au redemarrage
};
NetKeyResult netUdpKeyNew(const uint8_t appRandom[32], char keyHex[65], char kid[9]);
// Efface la cle (NVS et memoire) : plus de transport reseau. false : NVS en
// echec (la cle est quand meme retiree de la memoire).
bool netUdpKeyErase();
bool netUdpKid(char kid[9]);  // false : aucune cle
