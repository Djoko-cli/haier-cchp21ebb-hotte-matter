// ===========================================================================
//  Module de la hotte Haier CCHP21EBB (docs/SPEC-PRODUIT.md) : noeud Matter
//  sur Thread, EP1 Ventilateur, EP2 Lumiere, canal compagnon profil "hotte".
//
//  setup() (5.7) : GPIO1 bas en premier, console, NVS, pilote (ecoute
//  seulement), etat initial, Matter (identite, Thread, EP1, EP2), transport
//  reseau, mesures, chien de garde en dernier. Rien n'est injecte au demarrage.
//  loop() (4.1) : pilote -> automate ; boite d'intentions Matter -> automate ;
//  console et reseau -> automate ; actions de l'automate (appuis, reflets,
//  evenements) ; mode machine ; mesures et surveillance ; ecritures NVS
//  differees ; delay(1). Aucune attente bloquante.
// ===========================================================================
#include <Arduino.h>
#include <esp_log.h>
#include <esp_system.h>
#include <lib/support/logging/CHIPLogging.h>
#include <stdarg.h>

#include "alim.h"
#include "cli_produit.h"
#include "config_produit.h"
#include "fw_version.h"
#include "hotte_map.h"
#include "json_mode_produit.h"
#include "matter_hotte.h"
#include "net_udp_thread.h"
#include "produit.h"
#include "reglages_produit.h"

static hotte::Automate sAutomate;
static surv::Surveillance sSurv;
#if PILOTE_SIMULE
static sim::PiloteSimule sSimu;
static ligne::Pilote &sPilote = sSimu;
// Etat de la hotte simulee a travers un redemarrage qui n'est pas une mise
// sous tension (4.3) : memoire RTC non initialisee, avec un mot de controle.
RTC_NOINIT_ATTR static uint32_t sRtcMot;
RTC_NOINIT_ATTR static uint16_t sRtcEtat;
static constexpr uint32_t kMotRtc = 0x484F5454;  // "HOTT"
#else
#error "pilote reel (A, B ou C) : second plan, apres l'etape 6"
#endif
static uint8_t sHorsBornes = 0;
static hotte::EcritureDifferee sNvsDerniere, sNvsEtatPub;  // 10 s apres le dernier changement (4.4)
static uint16_t sEssaiMin = 0;  // essai de 24 h : minutes restantes (72 h au plus)
static uint32_t sEssaiMinuteMs = 0;
static uint8_t sEssaiDepuisEcrit = 0;

hotte::Automate &produitAutomate() { return sAutomate; }
ligne::Pilote &produitPilote() { return sPilote; }
surv::Surveillance &produitSurveillance() { return sSurv; }
uint8_t produitHorsBornes() { return sHorsBornes; }

bool produitReglerAutomate(const hotte::Params &p) {
  sAutomate.configurer(p);
#if PILOTE_SIMULE
  sim::Reglages r = sSimu.reglages();  // le garde-fou 7 du pilote suit l'automate
  r.delaiMoteurMs = p.delaiMoteurMs;
  sSimu.regler(r);
#endif
  return nvsSauverParams(p);
}

bool produitReglerSurveillance(const surv::Params &p) {
  sSurv.configurer(p);
  return nvsSauverSurveillance(p);
}

void produitEcrireEnAttente() {
  if (sNvsDerniere.enAttente() && nvsEcrireDerniere((hotte::Moteur)sNvsDerniere.valeur()))
    sNvsDerniere.ecrite(sNvsDerniere.valeur());
  if (sNvsEtatPub.enAttente()) {
    hotte::Etat e;
    if (hotte::depuisNvs((uint16_t)sNvsEtatPub.valeur(), &e) && nvsEcrireEtatPub(e))
      sNvsEtatPub.ecrite(sNvsEtatPub.valeur());
  }
  if (sSurv.maxPose() != surv::Surveillance::kAucun && nvsEcrireTempMax(sSurv.maxPose())) sSurv.maxPoseEcrit(millis());
  if (sEssaiMin) nvsEcrireEssaiMin(sEssaiMin);
}

bool produitEssai() { return sEssaiMin != 0; }
uint32_t produitEssaiResteMin() { return sEssaiMin; }
bool produitReglerEssai(bool on) {
  sEssaiMin = on ? 4320 : 0;  // 72 h
  sEssaiMinuteMs = millis();
  sEssaiDepuisEcrit = 0;
  return nvsEcrireEssaiMin(sEssaiMin);
}

#if PILOTE_SIMULE
sim::PiloteSimule &produitSimu() { return sSimu; }

bool produitReglerSimu(const sim::Reglages &r) {
  sim::Reglages n = r;
  n.delaiMoteurMs = sAutomate.params().delaiMoteurMs;
  sSimu.regler(n);
  sAutomate.configurerLigne(n.mode, n.annexe);
  jsonConfigChanged();
  return nvsSauverSimu(n);
}

bool produitSimuApresDemarrage(const sim::EtatHotte &e) { return nvsEcrireSimuDem(e); }

// Cycle d'endurance : marche, V2, lumiere, V3, V3 (arret), marche (eteinte), lumiere.
static const hotte::Touche kCycle[] = {hotte::Touche::Marche, hotte::Touche::V2, hotte::Touche::Lumiere,
                                       hotte::Touche::V3,     hotte::Touche::V3, hotte::Touche::Marche,
                                       hotte::Touche::Lumiere};
static uint32_t sAutoS = 0, sAutoMs = 0;
static uint8_t sAutoRang = 0;

void produitSimuAuto(uint32_t periodeS) {
  sAutoS = periodeS;
  sAutoMs = millis();
  sAutoRang = 0;
}
uint32_t produitSimuAutoS() { return sAutoS; }

static void simuAuto(uint32_t now) {
  if (!sAutoS || now - sAutoMs < sAutoS * 1000) return;
  sAutoMs = now;
  sSimu.appuiPanneau(kCycle[sAutoRang], now);
  sAutoRang = (uint8_t)((sAutoRang + 1) % (sizeof(kCycle) / sizeof(kCycle[0])));
}
#endif

// ---------------------------------------------------------------------------
//  Traces de la pile CHIP (reprises de benq, src/main.cpp) : esp-matter
//  n'emet pas par les macros ESP_LOGx ; le crochet de sortie du log est le
//  seul point de passage. Les lignes "chip[" sont avalees.
// ---------------------------------------------------------------------------

static vprintf_like_t sVprintfPrecedent = nullptr;

static int vprintfSilencieux(const char *fmt, va_list args) {
  char ligne[256];
  va_list copie;
  va_copy(copie, args);
  const int n = vsnprintf(ligne, sizeof(ligne), fmt, copie);
  va_end(copie);
  if (n > 0 && strstr(ligne, "chip[")) return 0;
  return sVprintfPrecedent ? sVprintfPrecedent(fmt, args) : vprintf(fmt, args);
}

static void traceChip(bool on) {
  if (!sVprintfPrecedent) sVprintfPrecedent = esp_log_set_vprintf(vprintfSilencieux);
  chip::Logging::SetLogFilter(on ? chip::Logging::kLogCategory_Progress : chip::Logging::kLogCategory_None);
}

// ---------------------------------------------------------------------------
//  Evenements vers le compagnon et la console
// ---------------------------------------------------------------------------

static void texte(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void texte(const char *fmt, ...) {
  char ligne[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ligne, sizeof(ligne), fmt, ap);
  va_end(ap);
  if (!jsonLog("hotte", "notice", ligne) && !jsonMachine()) jsonTexteUsb(ligne);
}

static void actions(uint32_t now) {
  for (;;) {
    const hotte::Action a = sAutomate.suivante(now);
    switch (a.type) {
      case hotte::Action::Aucune: return;
      case hotte::Action::Appuyer: {
        const ligne::Admission ad = sPilote.appuyer(a.touche, a.idAppui, now);
        if (ad != ligne::Admission::Acceptee) sAutomate.surFinAppui(a.idAppui, ligne::resultatAdmission(ad), now);
        break;
      }
      case hotte::Action::Publier:
        matterHottePublier(a.etat);
        sNvsEtatPub.noter(hotte::versNvs(a.etat), now);
        break;
      case hotte::Action::FinSequence:
        jsonSequence(a);
        texte("[hotte] sequence %s : %s%s%s (%u appuis, %lu ms)", hotte::texte(a.sujet), hotte::texte(a.issue),
              a.cause == hotte::Cause::Aucune ? "" : ", ", a.cause == hotte::Cause::Aucune ? "" : hotte::texte(a.cause),
              (unsigned)a.appuis, (unsigned long)a.dureeMs);
        break;
      case hotte::Action::Changement:
        jsonHotte(a.avant, a.etat, a.origine);
        texte("[hotte] %s/%s -> %s/%s, lampe %s (%s, %s)", hotte::texte(a.avant.marche), hotte::texte(a.avant.moteur),
              hotte::texte(a.etat.marche), hotte::texte(a.etat.moteur),
              a.etat.lampeConnue ? (a.etat.lampe ? "on" : "off") : "?", hotte::texte(a.origine),
              hotte::texte(a.etat.source));
        break;
      case hotte::Action::Evenement: texte("[hotte] %s", hotte::texte(a.evt)); break;
    }
  }
}

static void evenementsPilote(uint32_t now) {
  ligne::Evenement ev;
  while (sPilote.evenement(&ev)) {
    switch (ev.type) {
      case ligne::Evenement::EtatLu: sAutomate.surEtatLu(ev.etat, now); break;
      case ligne::Evenement::Appui: sAutomate.surAppui(ev.touche, ev.origine, now); break;
      case ligne::Evenement::FinAppui: sAutomate.surFinAppui(ev.idAppui, ev.resultat, now); break;
      case ligne::Evenement::Anomalie: sAutomate.surAnomalie(ev.codeAnomalie, now); break;
      case ligne::Evenement::Service: sAutomate.surPilote(ev.enService, now); break;
    }
  }
  ligne::Trame t;
  while (sPilote.trame(&t)) {
    jsonp::TrameD d;
    d.tMs = t.tMs;
    d.origine = (jsonp::OrigineTrame)t.origine;
    d.n = t.n > 8 ? 8 : t.n;
    for (uint8_t i = 0; i < d.n; i++) d.octets[i] = t.octets[i];
    jsonTrameD(d);
  }
}

// Ecritures NVS differees (4.4), maximum de temperature depuis la pose,
// compte a rebours de l'essai de 24 h.
static void nvsDifferee(uint32_t now) {
  sNvsDerniere.noter((uint32_t)sAutomate.derniereVitesse(), now);
  if (sNvsDerniere.echue(now) && nvsEcrireDerniere((hotte::Moteur)sNvsDerniere.valeur()))
    sNvsDerniere.ecrite(sNvsDerniere.valeur());
  if (sNvsEtatPub.echue(now)) {
    hotte::Etat e;
    if (hotte::depuisNvs((uint16_t)sNvsEtatPub.valeur(), &e) && nvsEcrireEtatPub(e))
      sNvsEtatPub.ecrite(sNvsEtatPub.valeur());
  }
  if (sSurv.maxPoseAEcrire(now) && nvsEcrireTempMax(sSurv.maxPose())) sSurv.maxPoseEcrit(now);
  if (sEssaiMin && now - sEssaiMinuteMs >= 60000) {
    sEssaiMinuteMs += 60000;
    sEssaiMin--;
    if (!sEssaiMin || ++sEssaiDepuisEcrit >= 60) {  // une ecriture par heure, et a l'echeance
      sEssaiDepuisEcrit = 0;
      nvsEcrireEssaiMin(sEssaiMin);
    }
  }
}

void setup() {
  // TOUJOURS la premiere instruction (4.3, garde-fou 1) : la sortie
  // d'injection a l'etat bas. Le build simule ne lie aucun code d'injection.
  pinMode(kPinInjectionD, OUTPUT);
  digitalWrite(kPinInjectionD, LOW);
  Serial.setTxBufferSize(8192);  // une ligne machine fait jusqu'a 1024 octets
  Serial.begin(115200);
  jsonBegin();  // identifiant de ce demarrage (hello.boot), avant toute radio
  rgbLedWrite(kPinLedRgb, 0, 0, 0);  // aucun voyant ajoute (8.6)
  traceChip(false);
  const esp_reset_reason_t cause = esp_reset_reason();
  Serial.printf("\n=== Module hotte Haier : firmware %s (%s), pilote %s ===\n", FW_VERSION_FULL, FW_ENV,
                PILOTE_LIGNE_TEXTE);

  // Reglages (bornes appliquees au chargement), derniere vitesse, maxima.
  hotte::Params pa;
  surv::Params ps;
  sHorsBornes = nvsChargerParams(&pa, &ps);
  if (sHorsBornes) Serial.printf("[reglages] %u jeu(x) hors bornes en NVS : valeurs par defaut\n", sHorsBornes);
  sAutomate.configurer(pa);
  sSurv.configurer(ps);
  int32_t tMax = 0;
  sSurv.maxPoseLu(nvsLireTempMax(&tMax) ? tMax : surv::Surveillance::kAucun);
  sEssaiMin = nvsLireEssaiMin();
  sEssaiMinuteMs = millis();

  // Pilote : ecoute seulement (5.7, point 3).
  const uint32_t now = millis();
#if PILOTE_SIMULE
  sim::Reglages sr;
  if (!nvsLireSimu(&sr)) sr = sim::Reglages();
  sr.delaiMoteurMs = pa.delaiMoteurMs;
  sSimu.regler(sr);
  // La hotte simulee survit a un redemarrage qui n'est pas une mise sous
  // tension ; 'simu apres_demarrage' la change une fois, quelle que soit la cause.
  sim::EtatHotte h;
  if (cause != ESP_RST_POWERON && sRtcMot == kMotRtc && sim::depuisMot(sRtcEtat, &h)) sSimu.poserHotte(h, now);
  if (nvsPrendreSimuDem(&h)) {
    sSimu.poserHotte(h, now);
    Serial.println("[simu] etat de la hotte simulee pose au demarrage (simu apres_demarrage)");
  }
#endif
  sPilote.demarrer(now);
  sAutomate.configurerLigne(sPilote.mode(), sPilote.annexe());

  // Etat initial (5.7, point 5) : mise sous tension, eteinte (Deduit) ; autre
  // cause, dernier etat publie relu en NVS (Presume).
  hotte::Moteur derniere = hotte::Moteur::V2;
  const bool derniereLue = nvsLireDerniere(&derniere);
  hotte::Etat publie;
  const bool publieLu = cause != ESP_RST_POWERON && nvsLireEtatPub(&publie);
  sAutomate.demarrer(cause == ESP_RST_POWERON ? hotte::Demarrage::MiseSousTension : hotte::Demarrage::Autre,
                     publieLu ? &publie : nullptr, derniere, now);
  if (derniereLue) sNvsDerniere.ecrite((uint32_t)derniere);
  if (publieLu) sNvsEtatPub.ecrite(hotte::versNvs(publie));

  // Matter : identite, Thread, EP1, EP2 ; premier reflet force (parade du piege de la NVS).
  matterHotteBegin(sAutomate.etat(), sAutomate.derniereVitesse());
  traceChip(false);  // l'init de la pile a pu reposer son propre filtre
  netUdpBegin();     // cle H1 lue en NVS ; le socket s'ouvre au premier tour ou OpenThread est libre
  if (!alimBegin()) Serial.println("[alim] ADC continu indisponible : temperature seule");
  if (!matterHotteMisEnService()) {
    Serial.println("Noeud Matter pas encore mis en service : ajoute l'accessoire dans Maison.");
    matterHotteStatut(Serial);
  }
  cliBegin();
  // En dernier (4.3, garde-fou 5) : la tache loop au chien de garde des taches (5 s).
  enableLoopWDT();
}

void loop() {
  const uint32_t now = millis();
  sPilote.poll(now);
  evenementsPilote(now);
  matterHottePoll(sAutomate, now);
  cliPoll();
  netUdpPoll();
  actions(now);
  matterHotteDerniere(sAutomate.derniereVitesse());
  jsonPoll();
  alimPoll(sSurv, now);
  surv::Alerte al;
  while (sSurv.alerte(&al)) jsonAlerte(al);
  nvsDifferee(now);
#if PILOTE_SIMULE
  simuAuto(now);
  sRtcEtat = sim::versMot(sSimu.hotte());
  sRtcMot = kMotRtc;
#endif
  delay(1);  // la tache IDLE, et le chien de garde nourri par la boucle d'Arduino
}
