// ===========================================================================
//  Sonde de reconnaissance de la ligne D (docs/SPEC-RECONNAISSANCE.md)
//
//  setup() : GPIO7 bas en premier, port USB, identifiant de demarrage,
//  reglages NVS, interruption des fronts, capture RMT, console, Wi-Fi, UDP,
//  injection (desarmee).
//  loop() : console, capture (parties -> lignes trame du mode machine, ou
//  texte), mode machine (bail, lignes periodiques), Wi-Fi, UDP (commandes
//  recues, datagrammes emis), injection (attente du silence, emission,
//  evenement), puis la tache IDLE.
// ===========================================================================
#include <Arduino.h>

#include "bord.h"
#include "capture_rmt.h"
#include "cli.h"
#include "config.h"
#include "fw_version.h"
#include "injection.h"
#include "json_mode.h"
#include "json_out.h"
#include "net_udp_wifi.h"
#include "net_wifi.h"
#include "reglages.h"
#include "sonde.h"

static ReglagesSonde sReglages;
static capt::Changements sChangements;  // mode changements : receptions d'une partie
static jsonp::RateCap sTexte(20);       // lignes trame affichees par seconde (mode humain)
static char sLigne[1024];
static uint32_t sRepTotal = 0;          // receptions identiques non emises depuis le demarrage
static uint8_t sHorsBornes = 0;         // valeurs hors bornes lues en NVS au demarrage (spec 8.2)

ReglagesSonde &sondeReglages() { return sReglages; }
uint32_t sondeRepEnCours() { return sChangements.repEnCours(); }
uint32_t sondeRepTotal() { return sRepTotal; }
uint8_t sondeHorsBornes() { return sHorsBornes; }

static bool memeCapture(const capt::Reglages &a, const capt::Reglages &b) {
  return a.resolHz == b.resolHz && a.filtreUs == b.filtreUs && a.silenceUs == b.silenceUs && a.inverse == b.inverse;
}

bool sondeAppliquer(const ReglagesSonde &r) {
  ReglagesSonde n = r;
  capt::borner(&n.capture);  // deja fait par l'appelant ; jamais de valeur hors bornes au RMT
  const bool relance = captureActive() && !memeCapture(n.capture, sReglages.capture);
  if (n.changements != sReglages.changements) sChangements.reset();
  sReglages = n;
  bool ok = true;
  if (relance) ok = captureBegin(sReglages.capture);  // captureEnd() d'abord
  if (!reglagesSauver(sReglages)) ok = false;
  jsonConfigChanged();  // config reemise aux sessions en mode machine
  return ok;
}

// Mode humain : 'trame <num>.<part>[ fin] t=<t_us> <haut|bas> <d1> <d2> ...',
// 20 lignes par seconde au plus. Jamais d'attente (spec 8.7) : sans la place
// d'une ligne entiere dans le tampon d'emission de l'USB (Mac qui ne lit
// plus), la ligne est perdue, car Serial.write bloquerait la boucle jusqu'a
// 2 s. Lignes sautees (plafond) et perdues (tampon plein) sont comptees
// ensemble et signalees sur la suivante.
static void afficherTrame(const capt::Partie &p, bool hasRep, uint32_t rep) {
  if (!sTexte.available(millis()) || Serial.availableForWrite() < (int)sizeof(sLigne)) {
    sTexte.skip();
    return;
  }
  sTexte.take();
  const size_t cap = sizeof(sLigne) - 64;  // place pour les suffixes et le saut de ligne
  size_t k = (size_t)snprintf(sLigne, sizeof(sLigne), "trame %lu.%lu%s t=%llu %s", (unsigned long)p.num,
                              (unsigned long)p.part, p.fin ? " fin" : "", (unsigned long long)p.tUs,
                              p.niv0Haut ? "haut" : "bas");
  for (uint16_t i = 0; i < p.n && k < cap; i++)
    k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " %lu", (unsigned long)p.dur[i]);
  if (p.debord) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " debord");
  if (hasRep) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " rep=%lu", (unsigned long)rep);
  const uint32_t sautees = sTexte.takeSkipped();
  if (sautees) k += (size_t)snprintf(sLigne + k, sizeof(sLigne) - k, " (%lu lignes sautees)", (unsigned long)sautees);
  sLigne[k++] = '\n';
  Serial.write((const uint8_t *)sLigne, k);
}

// Chaque partie produite par la capture (capturePoll) : lignes trame vers les
// sessions en mode machine ; texte sur l'USB hors mode machine seulement.
static void surPartie(const capt::Partie &p, void *) {
  bool hasRep = false;
  uint32_t rep = 0;
  if (sReglages.changements) {
    if (!sChangements.aEmettre(p)) {  // identique a la derniere emise : comptee
      sRepTotal++;
      return;
    }
    hasRep = true;
    rep = sChangements.prendreRep();
  }
  jsonTrame(p, hasRep, rep);
  if (!jsonMachine()) afficherTrame(p, hasRep, rep);
}

void setup() {
  // TOUJOURS la premiere instruction (spec 8.4) : la base de l'etage
  // d'injection a l'etat bas avant tout le reste.
  pinMode(kPinInjection, OUTPUT);
  digitalWrite(kPinInjection, LOW);
  // Tampon d'emission de l'USB (HWCDC) : 256 octets par defaut, moins qu'une
  // ligne machine (1024). 8 Ko : une ligne periodique part s'il en reste 2048
  // libres, et une dizaine de lignes trame tiennent pendant que le Mac lit.
  Serial.setTxBufferSize(8192);
  Serial.begin(115200);
  jsonBegin();  // identifiant de ce demarrage (hello.boot), avant toute radio
  // Nombre garde : un log l'annonce aussi a chaque 'json 1' (json_mode.cpp).
  sHorsBornes = reglagesCharger(&sReglages);
  Serial.printf("firmware %s (%s)\n", FW_VERSION_FULL, FW_ENV);
  if (sHorsBornes) Serial.printf("[reglages] %u valeur(s) hors bornes en NVS : valeur(s) par defaut\n", sHorsBornes);
  if (!bordBegin(kPinEcoute)) Serial.println("[bord] interruption des fronts indisponible");
  if (!captureBegin(sReglages.capture)) Serial.println("[capture] echec du RMT ('capture on' pour reessayer)");
  cliBegin();
  netWifiBegin();  // identifiants en NVS : station, mDNS ; sans eux, radio eteinte
  netUdpBegin();   // cle H1 en NVS (hotte/cle) : socket UDP 5480 des que le Wi-Fi a une adresse
  // Valeurs de l'avenant et etage declare monte (NVS) ; toujours desarmee au demarrage.
  if (!injectionBegin(sReglages.injection, sReglages.injMontee))
    Serial.println("[injection] encodeur RMT indisponible : injection impossible");
}

void loop() {
  cliPoll();
  capturePoll(surPartie, nullptr, 8);
  jsonPoll();
  netWifiPoll();
  netUdpPoll();
  injectionPoll();
  vTaskDelay(1);  // laisse tourner la tache IDLE
}
