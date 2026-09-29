// Copie de benq-screenbar-halo-matter@c58a506 : src/net_udp.h (adapte : NVS
// produit/cle, port kPortUdp, modules du produit, toujours Thread, rafale
// de l'essai de 24 h)
#pragma once
// ===========================================================================
//  Transport reseau du protocole compagnon du produit (docs/PROTOCOLE-JSON-PRODUIT.md ;
//  ScreenBar, docs/PROTOCOLE-JSON.fr.md, section 10)
//
//  Socket UDP d'OpenThread sur le port kPortUdp (5480), enveloppe H1
//  (h1_proto.h), cle partagee en NVS (produit/cle).
//  L'app (Mac, iPhone) joint le noeud par son adresse OMR, a travers les
//  routeurs de bordure Thread, sans rien demander a Matter.
//
//  Taches et verrous :
//  - reception : rappel d'OpenThread (tache OT, verrou OT tenu) ; il ne fait
//    que copier le datagramme dans une file FreeRTOS (4 places), jamais de
//    verrou CHIP, jamais de Serial ;
//  - tout le reste dans la tache loop : poignee de main, verification, commandes
//    (cli_produit.cpp : cliRunRemote), formatage des lignes, HMAC ;
//  - emission : datagrammes fermes (en-tete H1 + JSON) dans une file de 6
//    places, remis a OpenThread sous verrou OT pris SANS attente (otLockTry(0)
//    de matter_hotte.cpp), verrou occupe : au tour suivant. Sous ce verrou,
//    seulement des appels OpenThread (regle de la garde d'antenne).
//  - debit plafonne (3000 octets/s, priorite basse), et un datagramme ne part
//    que s'il reste assez de tampons OpenThread (65, partages avec Matter)
//    apres lui ; le DEFI d'une poignee de main passe devant, hors plafond.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "json_out_produit.h"

// setup(), apres matterHotteBegin() : cle lue en NVS, file de reception.
void netUdpBegin();
// loop(), apres cliPoll() : socket (ouvert tant qu'une cle existe), datagrammes
// recus (poignees de main, commandes), oubli des sessions, emission.
void netUdpPoll();

// Ligne machine (objet JSON sans RS ni LF) pour la session etablie slot
// (0..h1::kSlots-1) : enveloppe H1 et mise en file. false : pas de session,
// ou file pleine (ligne perdue, comptee par l'appelant).
bool netUdpSend(uint8_t slot, const uint8_t *json, size_t len);
// Places libres dans la file d'emission (partagee par les sessions).
uint8_t netUdpFreeSlots();
// 'json 0' execute pour la session slot : sa place revient au prochain client
// sans attendre 30 s de silence (h1::Table::end).
void netUdpEnd(uint8_t slot);
// Nouvelle commande admise de la session slot : elle sert de nouveau
// (h1::Table::resume).
void netUdpResume(uint8_t slot);

// Bloc 'reseau' 'ip' (section 5.5) : nom d'hote SRP, adresses, transport UDP.
void netUdpJson(jsonp::Writer &w, uint32_t now);

// Cle (json cle, USB seulement). netUdpKeyNew : cle = HMAC-SHA256(alea de
// l'app, alea de la carte), ecrite en NVS, rendue une fois en hexa (keyHex,
// 64 + 1) avec son empreinte (kid, 8 + 1) ; toutes les sessions tombent.
enum class NetKeyResult : uint8_t {
  Ok,
  Crypto,  // cle non calculee : rien ne change
  Nvs,     // cle non ecrite : rien ne change
  Load,    // cle ecrite en NVS mais pas chargee : transport coupe jusqu'au redemarrage
};
NetKeyResult netUdpKeyNew(const uint8_t appRandom[32], char keyHex[65], char kid[9]);
// Efface la cle (NVS et memoire) : plus de transport reseau. Aussi a la remise
// a zero Matter (matterHotteDecommission). false : NVS en echec (la cle est
// quand meme retiree de la memoire).
bool netUdpKeyErase();
bool netUdpKid(char kid[9]);  // false : aucune cle

// 'radio rafale <s>' (essai de 24 h, 7.5) : datagrammes pleins, non
// authentifies, vers l'adresse de la session slot, pendant s secondes (1 a
// 30), hors plafond de debit mais dans la reserve de tampons d'OpenThread.
// false : session absente ou duree hors bornes.
bool netUdpRafale(uint8_t slot, uint32_t secondes);
