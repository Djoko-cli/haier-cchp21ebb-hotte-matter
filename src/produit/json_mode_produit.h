// Copie de src/json_mode.h de la sonde (commit 1cb2c7c), elle-meme copiee de
// benq-screenbar-halo-matter@c58a506 (adapte : profil du produit, evenements
// hotte, sequence, alerte et trame_d, sessions reseau sur UDP/Thread)
#pragma once
// ===========================================================================
//  Mode machine : protocole compagnon v1, profil hotte du produit
//  (docs/PROTOCOLE-JSON-PRODUIT.md)
//
//  Session ('json 1', bail, 'json 0'), lignes periodiques (etat, compteurs,
//  reseau, battement) par la file des periodiques, evenements hotte,
//  sequence, alerte, trame_d et log, reponses aux lignes portant un id.
//
//  Une session par transport (origine : jsonp::kUsb, puis une par session
//  reseau etablie, net_udp_thread.cpp) : ses reglages, son n, sa file, ses
//  pertes. Les evenements partent vers chaque session en mode machine,
//  formates pour elle ; une reponse, vers l'origine de sa commande seulement.
//
//  Regles tenues ici (spec produit 4.8) :
//  - un seul producteur, la tache loop ; un seul tampon de formatage ;
//  - jamais d'attente : une ligne qui ne tient pas dans le tampon d'emission
//    est perdue et comptee (json_perdus), jamais ecrite a moitie ;
//  - rien n'est persiste : chaque demarrage repart en mode humain.
// ===========================================================================
#include <Arduino.h>

#include "json_out_produit.h"

// Debut de setup(), avant toute radio : identifiant de ce demarrage (hello.boot).
void jsonBegin();
// A chaque tour de loop() : bail, lignes periodiques.
void jsonPoll();

uint32_t jsonBootId();
bool jsonMachine();  // mode machine en cours sur l'USB
void jsonNoteRx();   // un octet recu de l'hote USB (bail)

// --- Console (cli_produit.cpp) ------------------------------------------------

struct JsonCmd {
  bool hasId;
  uint32_t id;
  const char *cmd;  // commande sans le prefixe, tronquee (reponse.cmd)
  uint32_t t0;      // debut de l'execution (duree_ms)
};
void jsonCommand(char *arg, const JsonCmd &c);
bool jsonCadenceOk(uint32_t now);
void jsonRefuse(const JsonCmd &c, const char *code, const char *msg);
void jsonReply(const jsonp::Reply &r);
void jsonReplyEnd(const jsonp::Reply &r);
void jsonAfterCommand();
// Un reglage a change ('hotte regle', 'simu ...', 'matter ...') : config
// reemise a chaque session en mode machine au prochain jsonAfterCommand().
void jsonConfigChanged();

// --- Evenements du produit (vers chaque session en mode machine) ------------

// Ordre de l'app ('hotte ventilo|lampe') : numero d'ordre pour l'automate,
// retenu avec l'origine et l'id de la commande, pour que l'evenement sequence
// porte cet id dans la session qui l'a envoyee (null ailleurs).
uint32_t jsonOrdreApp(uint32_t idCmd);
void jsonSequence(const hotte::Action &fin);
void jsonHotte(const hotte::Etat &avant, const hotte::Etat &apres, hotte::Origine o);
// Alerte : vers CHAQUE session (meme hors mode machine pour l'USB, sans
// abonnement), doublee d'un log notice, meme sans 'json log 1' (7.4).
void jsonAlerte(const surv::Alerte &a);
// Trame decodee du fil : sessions avec 'json trames 1' ; 10 par seconde au
// plus ; coupees seules apres 60 s a distance.
void jsonTrameD(const jsonp::TrameD &t);
// Mode 'json log 1' : la ligne part en message log (src produit|matter|hotte|
// reseau|simu, niv notice|trace), 20 par seconde au plus ; true si l'USB l'a
// prise (emise, plafonnee ou perdue), false pour l'afficher en texte.
bool jsonLog(const char *src, const char *niv, const char *txt);
// Annonce en texte sur l'USB, quand jsonLog ne l'a pas prise : la ligne
// entiere ou rien, jamais d'attente ; perdue, elle est comptee.
void jsonTexteUsb(const char *txt);

// --- Transport reseau (net_udp_thread.cpp, cli_produit.cpp) -------------------

void jsonSetOrigin(uint8_t origin);
uint8_t jsonOrigin();
void jsonRemoteReset(uint8_t origin);
void jsonNoteRemoteRx(uint8_t origin);
bool jsonRemoteAdmit(uint32_t id, const char *shown);
void jsonCountRejected();
