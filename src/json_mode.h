// Copie de benq-screenbar-halo-matter@c58a506 : src/json_mode.h (adapte : profil hotte, sans lampe, LED, Matter ni livraison)
#pragma once
// ===========================================================================
//  Mode machine : protocole compagnon v1, profil hotte (docs/PROTOCOLE-JSON.md)
//
//  Session ('json 1', bail, 'json 0'), lignes periodiques (etat, compteurs,
//  battement) par la file des periodiques, evenements trame et log, reponses
//  aux lignes portant un id.
//
//  Une session par transport (origine : jsonp::kUsb, puis une par session
//  reseau etablie, net_udp_wifi.cpp) : ses reglages, son n, sa file, ses
//  pertes. Les evenements partent vers chaque session en mode machine,
//  formates pour elle ; une reponse, vers l'origine de sa commande seulement.
//
//  Regles tenues ici (spec 8.7, protocole de la ScreenBar 2.3) :
//  - un seul producteur, la tache loop ; un seul tampon de formatage ;
//  - jamais d'attente : une ligne qui ne tient pas dans le tampon d'emission
//    de HWCDC est perdue et comptee (json_perdus), jamais ecrite a moitie ;
//  - lignes periodiques : une par tour de loop() et par transport, et
//    seulement s'il reste ensuite la place d'un evenement ; perdues apres
//    500 ms de retard ;
//  - reponses differees (apres un instantane, ou apres le texte d'une
//    commande qui a rempli le tampon) : par la meme file, jamais perdues pour
//    retard ;
//  - rien n'est persiste : chaque demarrage repart en mode humain.
// ===========================================================================
#include <Arduino.h>

#include "capture_model.h"
#include "json_out.h"

// Debut de setup(), avant toute radio : identifiant de ce demarrage (hello.boot).
void jsonBegin();
// A chaque tour de loop(), apres cliPoll() et capturePoll() : bail, lignes periodiques.
void jsonPoll();

uint32_t jsonBootId();
bool jsonMachine();  // mode machine en cours sur l'USB
void jsonNoteRx();   // un octet recu de l'hote USB (bail)

// --- CLI (cli.cpp) ----------------------------------------------------------

struct JsonCmd {
  bool hasId;
  uint32_t id;
  const char *cmd;  // commande sans le prefixe, tronquee (reponse.cmd)
  uint32_t t0;      // debut de l'execution (duree_ms)
};
// Famille 'json ...' ; avec id, la reponse part d'ici (immediate, ou apres les
// lignes d'un instantane).
void jsonCommand(char *arg, const JsonCmd &c);
// Au plus 20 lignes par seconde et par transport (mode machine, ou ligne avec id).
bool jsonCadenceOk(uint32_t now);
// Ligne refusee sans execution (trop_long, cadence, interdite) : reponse avec
// id, texte en mode humain sans id ; comptee (sys.rejets, compteurs.rejets).
void jsonRefuse(const JsonCmd &c, const char *code, const char *msg);
// Reponse immediate (non bloquante), en tout mode.
void jsonReply(const jsonp::Reply &r);
// Reponse fin d'une commande a texte : tout de suite si une ligne entiere
// tient, sinon par la file des qu'elle tient.
void jsonReplyEnd(const jsonp::Reply &r);
// Fin d'une commande : bail, et config reemise si un reglage a change.
void jsonAfterCommand();
// Un reglage de capture a change (sondeAppliquer) : config reemise a chaque
// session en mode machine au prochain jsonAfterCommand().
void jsonConfigChanged();

// --- Evenements de la sonde (vers chaque session en mode machine) ----------

// Partie produite par la capture : ligne trame si la session a 'json trames 1'.
// 100 lignes par seconde et par session au plus ; les parties d'une reception
// sont produites ou sautees ensemble ; la suivante produite porte 'sautes'.
void jsonTrame(const capt::Partie &p, bool hasRep, uint32_t rep);
// Mode 'json log 1' : la ligne part en message log (src sonde|capture|
// injection|reseau, niv notice|trace), 20 par seconde au plus ; true si
// l'USB l'a prise (emise, plafonnee ou perdue), false pour l'afficher en texte.
bool jsonLog(const char *src, const char *niv, const char *txt);

// --- Transport reseau (net_udp_wifi.cpp, cli.cpp) --------------------------

// Origine de la commande en cours : jsonp::kUsb hors cliRunRemote().
void jsonSetOrigin(uint8_t origin);
uint8_t jsonOrigin();
// Session reseau neuve dans cet emplacement (origine), ou partie (oubliee,
// remplacee, cle changee) : son etat de session repart de zero, sans rien
// emettre (n continue).
void jsonRemoteReset(uint8_t origin);
void jsonNoteRemoteRx(uint8_t origin);  // message au MAC juste recu (bail)
// Origine reseau, ligne avec id, avant tout le reste (cadence comprise), shown
// etant la commande telle que la reponse la montre (reponse.cmd) :
//  - meme id, meme commande, reponse en cache : elle repart, rien n'est execute ;
//  - reponse differee de cet id encore en file : rien (elle partira) ;
//  - id deja traite (hors cache, ou autre commande) : reponse deja_traite ;
//  - sinon l'id est neuf (le plus haut id de la session avance) : false, a traiter.
bool jsonRemoteAdmit(uint32_t id, const char *shown);
// Ligne refusee sans reponse possible (reseau, sans id) : comptee (rejets).
void jsonCountRejected();
