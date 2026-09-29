// Copie de src/json_out.h de la sonde (commit 1cb2c7c), elle-meme copiee de
// benq-screenbar-halo-matter@c58a506 (adapte : profil du produit, sans
// capture ni injection ; Item, messages et liste blanche du produit)
#pragma once
// ===========================================================================
//  Protocole compagnon v1, profil hotte du produit (docs/PROTOCOLE-JSON-PRODUIT.md) :
//  briques pures
//
//  - Writer : une ligne machine, RS + objet JSON compact en ASCII + LF, 1024
//    octets au plus. Chaines echappees ('"' et '\'), tout octet hors
//    0x20..0x7E remplace par '?', jamais de \uXXXX. Au-dela de 1024 octets, la
//    ligne est marquee trop longue : jamais emise.
//  - Messages construits depuis des donnees simples : evenements hotte,
//    sequence, alerte et trame_d du produit, blocs de ses etats et
//    compteurs, reponse, battement, fin, log.
//  - Mecaniques de la session : prefixe id= des lignes de l'hote, assemblage
//    des lignes recues, plafonds de debit par type, cadence, file des lignes
//    periodiques, bail, liste blanche a distance.
//
//  Pur et sans Arduino : teste sur l'hote (tools/tests/test_json_produit.cpp).
//  La session, l'ecriture sur le port serie et les instantanes sont dans
//  json_mode_produit.cpp.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "hotte_etat.h"
#include "surveillance.h"

namespace jsonp {

constexpr uint8_t kVersion = 1;         // v : version majeure
// hello.rev : revision mineure, celle de la ScreenBar au commit c58a506 (4) :
// le profil hotte part de la derniere revision du protocole compagnon.
constexpr uint8_t kRev = 4;
constexpr size_t kLineMax = 1024;       // RS et LF compris
constexpr size_t kBudget = 896;         // pire cas vise par message (marge de 128 pour les ajouts)
constexpr size_t kCmdMax = 127;         // ligne de l'hote, prefixe id= compris
constexpr size_t kCmdTextMax = 40;      // reponse.cmd
constexpr size_t kMsgMax = 120;         // reponse.msg
constexpr size_t kLogTextMax = 191;     // log.txt
constexpr size_t kStrMax = 255;         // toute autre chaine
constexpr uint8_t kRS = 0x1E;
constexpr uint8_t kCtrlU = 0x15;        // vide la ligne en cours de saisie
constexpr uint32_t kIdMax = 999999999;  // id=<1..999999999>
// Origines (transports) : 0 = USB, 1..kOrigins-1 = sessions reseau etablies.
constexpr uint8_t kOrigins = 3;
constexpr uint8_t kUsb = 0;

// ---------------------------------------------------------------------------
//  Ecrivain d'une ligne machine
// ---------------------------------------------------------------------------

class Writer {
 public:
  // Ouvre la ligne : RS {"v":1,"t":type,"n":n,"ms":ms. Le champ suivant d'un
  // message en blocs est "bloc" (str("bloc", ...)).
  void begin(const char *type, uint32_t n, uint32_t ms);
  // Champs. k nul : element du tableau ouvert. Les cles sont des litteraux
  // ASCII du firmware, jamais echappees.
  void str(const char *k, const char *v, size_t max = kStrMax);  // v nul : null ; max caracteres de v
  void u32(const char *k, uint32_t v);
  // Writer : entier 64 bits (t_us), decimal sans zero de tete.
  void u64(const char *k, uint64_t v);
  void i32(const char *k, int32_t v);
  void boolean(const char *k, bool v);
  void null(const char *k);
  void hex(const char *k, const uint8_t *p, size_t n);  // "C5A5" (majuscules, sans 0x) ; n == 0 : ""
  void hexU32(const char *k, uint32_t v, uint8_t digits, bool prefix0x = false);  // "3FA2C901", "0x1A2B"
  void obj(const char *k);
  void arr(const char *k);
  void end();  // ferme l'objet ou le tableau ouvert
  // Ferme la ligne (} LF). false : plus de kLineMax octets, ou objets mal
  // fermes (bogue) ; la ligne ne doit pas etre emise.
  bool finish();
  // Ligne fermee (finish) : n remplace par celui d'un autre transport (meme
  // evenement pour chaque session). false : la ligne depasserait kLineMax.
  bool setN(uint32_t n);
  const uint8_t *data() const { return buf_; }
  size_t size() const { return len_; }
  bool overflow() const { return over_; }

 private:
  static constexpr uint8_t kDepth = 8;
  void put(char c);
  void puts(const char *s);
  void sep(const char *k);
  void num(uint32_t v, bool neg);
  void open(const char *k, char o, char c);
  uint8_t buf_[kLineMax];
  size_t len_ = 0;
  bool over_ = false, bad_ = false;
  uint8_t depth_ = 0;
  bool first_[kDepth] = {};
  char close_[kDepth] = {};
};

// ---------------------------------------------------------------------------
//  Messages d'evenement et de reponse
// ---------------------------------------------------------------------------

void heartbeat(Writer &w, uint32_t n, uint32_t ms, uint32_t boot, uint32_t upS, uint32_t lost);
void sessionEnd(Writer &w, uint32_t n, uint32_t ms, const char *cause);  // t "fin" : commande, bail
void logLine(Writer &w, uint32_t n, uint32_t ms, const char *src, const char *niv, const char *txt, uint32_t skipped);

// ---------------------------------------------------------------------------
//  Messages du produit (docs/PROTOCOLE-JSON-PRODUIT.md)
// ---------------------------------------------------------------------------

// Evenement hotte : changement d'etat (avant, apres, origine, source).
void evtHotte(Writer &w, uint32_t n, uint32_t ms, const hotte::Etat &avant, const hotte::Etat &apres,
              hotte::Origine origine);
// Evenement sequence : fin d'une sequence (Action::FinSequence). idCmd : id de
// la commande de l'app, pour la session qui l'a envoyee ; 0 (null) sinon.
void evtSequence(Writer &w, uint32_t n, uint32_t ms, const hotte::Action &fin, uint32_t idCmd);
// Evenement alerte (surveillance) : temperature en degres entiers, tensions en mV.
void evtAlerte(Writer &w, uint32_t n, uint32_t ms, const surv::Alerte &a);

// Trame decodee du fil (diagnostic, 'json trames 1') : 1 a 8 octets. Nom
// distinct du 'trame' de la sonde, qui porte des durees brutes.
enum class OrigineTrame : uint8_t { Carte, Panneau, Module };
struct TrameD {
  uint32_t tMs = 0;
  OrigineTrame origine = OrigineTrame::Carte;
  uint8_t n = 0;
  uint8_t octets[8] = {};
};
void evtTrameD(Writer &w, uint32_t n, uint32_t ms, const TrameD &t, uint32_t sautes);

// Degres entiers les plus proches de d dixiemes (-5 -> -1, 415 -> 42).
int32_t degres(int32_t dixiemes);

// Champs des blocs, apres "bloc", "boot" et "up_s" (json_mode_produit.cpp).
void etatHotte(Writer &w, const hotte::Etat &e, uint32_t now);
void etatAutomate(Writer &w, const hotte::Diagnostic &d, hotte::ModeEtat mode, bool annexe, uint32_t now);
void etatThermique(Writer &w, const surv::Surveillance &s);
void etatAlim(Writer &w, const surv::Surveillance &s);
void compteursHotte(Writer &w, const hotte::Compteurs &c);
void compteursAlim(Writer &w, const surv::Surveillance &s);

// Bloc etat matter : valeurs lues par matter_hotte.cpp dans la tache loop.
struct MatterEtat {
  bool demarre = false, misEnService = false;
  uint8_t fabriques = 0;
  uint16_t abonnements = 0;
  uint32_t ignoreResteMs = 0;  // fenetre d'ignorance du demarrage restante
  uint16_t maxintS = 0;        // plafond des abonnements (0 : celui du controleur)
  uint8_t med = 0;             // role au prochain demarrage : 0 routeur, 1 MED
  bool txConnu = false;
  int8_t txDbm = 0;            // puissance d'emission en vigueur
  bool derniere = true;        // substitution de On par la derniere vitesse (3.3, regle 3)
  uint32_t ecritures = 0, reflets = 0, verrouOccupe = 0, ignores = 0;
};
void etatMatter(Writer &w, const MatterEtat &m);

// Bloc compteurs radio (7.4), lu sous le verrou OpenThread par matter_hotte.cpp.
struct RadioCompteurs {
  bool connu = false;
  const char *role = "inconnu";  // disabled, detached, child, router, leader
  bool txConnu = false;
  int8_t txDbm = 0;
  int8_t sensibiliteDbm = 0;
  bool lien = false;             // parent (MED), ou routeur voisin de meilleur lien (routeur)
  bool lienParent = false;
  int8_t rssiMoyenDbm = 0;
  uint8_t lqIn = 0, lqOut = 0;
  uint32_t txTotal = 0, txRetry = 0, txEchecs = 0;
  uint32_t changementsParent = 0, changementsRole = 0;
};
void compteursRadio(Writer &w, const RadioCompteurs &r);

// Message reponse.
struct Reply {
  enum Suite : uint8_t { SuiteNone, SuiteSequence };  // absent, sequence (evenement a suivre)
  uint32_t id = 0;
  bool fin = true;              // false : etape debut
  const char *cmd = "";         // la commande sans le prefixe, tronquee a kCmdTextMax
  bool ok = true;
  const char *code = "ok";
  const char *msg = nullptr;    // nul : absent ; tronque a kMsgMax
  uint32_t durMs = 0;           // fin seulement
  Suite suite = SuiteNone;
  bool hasLease = false;        // bail_s, up_s (json 1, json ping)
  uint32_t leaseS = 0, upS = 0;
  const char *key = nullptr;    // cle : 64 hexa, une seule fois ('json cle nouvelle', USB)
  bool hasKid = false;          // empreinte ('json cle ...') : 8 hexa, ou null sans cle
  const char *kid = nullptr;
};
void reply(Writer &w, uint32_t n, uint32_t ms, const Reply &r);

// Dernieres reponses fin d'une session reseau : un id repete (l'app renvoie
// une commande restee sans reponse) recoit la meme reponse, sans nouvelle
// execution. Ni msg, ni cle, ni empreinte ne sont gardes.
class ReplyCache {
 public:
  static constexpr uint8_t kN = 8;
  void clear();
  void put(const Reply &r);              // etape fin avec id ; remplace la plus ancienne
  const Reply *find(uint32_t id) const;  // r.cmd pointe dans le cache
 private:
  struct Entry {
    bool used = false;
    Reply r;
    char cmd[kCmdTextMax + 1] = {};
  };
  Entry e_[kN];
  uint8_t next_ = 0;
};

// ---------------------------------------------------------------------------
//  Lignes de l'hote
// ---------------------------------------------------------------------------

// Commande permise sur le transport reseau (liste blanche du produit, spec
// 7.5) ? nullptr : oui ; sinon le msg de la reponse 'interdite'. cmd : la
// commande sans le prefixe id=. essai : drapeau NVS de l'essai de 24 h
// ('essai alim on', USB), qui seul autorise 'radio rafale'. Seules les
// bornes propres au reseau sont verifiees ici (bail 10..120, periodes
// minimales, matter tx 8..20) ; le reste des arguments l'est par
// l'aiguillage (usage).
const char *remoteRefusal(const char *cmd, bool essai);

// Prefixe "id=<n> " (n decimal 1..999999999, sans zero de tete superflu
// exige) en tete de ligne, espaces de tete ignores. true : *id rempli et
// *rest pointe sur la commande (espaces sautes). false : pas de prefixe
// valide, *rest = line.
bool parseIdPrefix(char *line, uint32_t *id, char **rest);
// Copie la commande pour reponse.cmd : kCmdTextMax caracteres au plus.
void copyCmd(char out[kCmdTextMax + 1], const char *cmd);
// reponse.cmd ne renvoie jamais un secret : 'json cle nouvelle <64 hexa>'
// (alea de l'app) devient 'json cle nouvelle'.
void maskCmd(char *shown);

// Assemblage des octets recus en lignes (cliPoll). Mode machine : octets hors
// 0x20..0x7E ignores, sauf LF, CR, Ctrl-U et retour arriere, pour qu'aucun RS
// ne revienne dans un message. Au-dela de kCmdMax caracteres, la ligne est
// marquee trop longue (refusee a son LF, rien n'est execute).
class LineAssembler {
 public:
  enum class Ev : uint8_t { None, Echo, Erase, Clear, Line };
  Ev feed(uint8_t c, bool machine);
  char *text();  // ligne terminee par 0 (apres Ev::Line)
  bool tooLong() const { return tooLong_; }
  uint8_t cleared() const { return cleared_; }  // caracteres effaces par le dernier Ctrl-U
  uint8_t length() const { return len_; }
  void reset() {
    len_ = 0;
    tooLong_ = false;
  }

 private:
  char buf_[kCmdMax + 1] = {};
  uint8_t len_ = 0, cleared_ = 0;
  bool tooLong_ = false;
};

// ---------------------------------------------------------------------------
//  Debit
// ---------------------------------------------------------------------------

// Plafond par fenetre d'une seconde. Au-dela, l'evenement n'est pas produit
// (aucun n consomme) et le suivant du meme type porte 'sautes'.
class RateCap {
 public:
  explicit RateCap(uint16_t perSecond) : limit_(perSecond) {}
  void setLimit(uint16_t perSecond) { limit_ = perSecond; }
  bool available(uint32_t now);  // place dans la fenetre en cours
  void take() { count_++; }
  void skip() { skipped_++; }
  uint32_t takeSkipped() {
    const uint32_t s = skipped_;
    skipped_ = 0;
    return s;
  }

 private:
  uint16_t limit_, count_ = 0;
  bool started_ = false;
  uint32_t winAt_ = 0, skipped_ = 0;
};

// Au plus kLines lignes acceptees par kWindowMs glissantes.
class Cadence {
 public:
  static constexpr uint8_t kLines = 20;
  static constexpr uint32_t kWindowMs = 1000;
  bool allow(uint32_t now);  // true : ligne acceptee et comptee

 private:
  uint32_t at_[kLines] = {};
  uint8_t idx_ = 0, n_ = 0;
};

// ---------------------------------------------------------------------------
//  File des lignes periodiques
// ---------------------------------------------------------------------------

constexpr uint32_t kLateMs = 500;  // ligne periodique perdue apres ce retard

enum class Item : uint8_t {
  HelloBase, HelloId, Config, EtatHotte, EtatAutomate, EtatMatter, EtatThermique, EtatAlim, EtatSys,
  CompteursHotte, CompteursAlim, CompteursRadio, NetIp, Heartbeat, Reply
};
struct Queued {
  Item item;
  uint8_t arg;   // Reply : place de la reponse differee
  bool session;  // periodique ou instantane de 'json 1' : retire a la fin du mode machine
  uint32_t at;   // mise en file (ou derniere demande explicite fondue dedans)
};

class Queue {
 public:
  static constexpr uint8_t kN = 28;
  // Ajoute en queue. Un element deja en file (hors Reply) n'est pas double ;
  // une demande explicite (session faux) fondue dedans le rend explicite et
  // repart de maintenant (son retard ne compte que depuis la demande).
  // false : file pleine.
  bool push(Item item, uint32_t now, bool session, uint8_t arg = 0);
  const Queued *front() const { return n_ ? &q_[head_] : nullptr; }
  void pop();
  uint8_t size() const { return n_; }
  bool has(Item item) const;
  // Retire de la tete les lignes en retard de plus de lateMs et rend leur
  // nombre (n consomme, json_perdus). Une reponse n'est jamais perdue pour
  // retard : elle arrete le balayage, les lignes derriere elle attendent.
  uint8_t dropLate(uint32_t now, uint32_t lateMs = kLateMs);
  // La tete peut-elle partir avec 'room' octets libres dans le tampon
  // d'emission ? Ligne periodique : 2 x kLineMax (elle, puis la place d'un
  // evenement). Reponse : kLineMax (elle tient, quelle qu'elle soit).
  bool frontReady(int room) const;
  // Retire les elements de session ; ceux qui restent gardent leur ordre.
  uint8_t dropSession();
  void clear() { head_ = n_ = 0; }

 private:
  Queued q_[kN] = {};
  uint8_t head_ = 0, n_ = 0;
};

// ---------------------------------------------------------------------------
//  Bail
// ---------------------------------------------------------------------------

// Le bail court depuis le plus recent : dernier octet recu, ou fin de la
// derniere commande. leaseS nul : sans bail, jamais expire.
bool leaseExpired(uint32_t now, uint32_t lastRx, uint32_t lastCmd, uint16_t leaseS);

}  // namespace jsonp
