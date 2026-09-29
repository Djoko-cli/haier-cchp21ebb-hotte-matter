// Recettes reprises de benq-screenbar-halo-matter@c58a506 : src/matter_bridge.cpp
// (boite d'intentions, echo propre, reflets sous TryLockChipStack, garde-fou de
// demarrage, identite, type Thread des l'init, plafond des abonnements, cle
// effacee au depart du dernier controleur), reecrites pour la hotte.
#include "matter_hotte.h"

#include <Matter.h>
#include <Preferences.h>
#include <app/InteractionModelEngine.h>
#include <app/ReadHandler.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage-null-handling.h>
#include <esp_mac.h>
#include <esp_openthread.h>
#include <esp_openthread_lock.h>
#include <openthread/link.h>
#include <openthread/platform/radio.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <stdarg.h>

#include "config_produit.h"
#include "hotte_map.h"
#include "json_mode_produit.h"
#include "net_udp_thread.h"

using namespace chip::app::Clusters;

#ifndef MATTER_THREAD_MED
#define MATTER_THREAD_MED 0
#endif
#ifndef HALO_WRAP_THREAD_DEVTYPE
#define HALO_WRAP_THREAD_DEVTYPE 0
#endif

// ===========================================================================
//  Boite d'intentions : remplie par les rappels (tache CHIP), videe par la
//  tache loop. Les ecritures d'EP1 vont dans un seul regroupement (5.4, regle
//  6), celles d'EP2 dans le leur ; la derniere valeur gagne.
// ===========================================================================

static TaskHandle_t sLoopTask = nullptr;  // pris dans matterHotteBegin() (setup = tache loop)
static portMUX_TYPE sBoiteMux = portMUX_INITIALIZER_UNLOCKED;
static hotte::RegroupementEp1 sEp1;
static hotte::RegroupementEp2 sEp2;
// Reglages lus par la tache CHIP : copies atomiques ecrites par la tache loop.
static volatile uint8_t sModeDerniere = hotte::kFanMedium;  // FanMode de la derniere vitesse
static volatile bool sSubstitution = true;                  // 'matter derniere 0|1'
static volatile uint16_t sMaxIntCap = 20;                   // plafond des abonnements neufs (s)

// Journal des ecritures brutes (banc Matter, B2) : rempli sous sBoiteMux.
struct Brute {
  uint32_t ms;
  uint32_t attr;
  uint8_t valeur;
  bool nulle, substituee;
};
static constexpr uint8_t kJournal = 16;
static Brute sJournal[kJournal];
static uint8_t sJournalN = 0;  // ecritures depuis le demarrage, modulo 256 (place = n % kJournal)

// Nos reflets repassent par les rappels, mais toujours dans la tache loop ;
// les ordres des controleurs arrivent dans la tache CHIP.
static inline bool ownEcho() { return xTaskGetCurrentTaskHandle() == sLoopTask; }

static void noterBrute(uint32_t attr, uint8_t v, bool nulle, bool substituee) {
  Brute &b = sJournal[sJournalN % kJournal];
  b.ms = millis();
  b.attr = attr;
  b.valeur = v;
  b.nulle = nulle;
  b.substituee = substituee;
  sJournalN++;
}

// EP1 : sous-classe de MatterFan (4.4). Elle recoit chaque ecriture AVANT le
// serveur CHIP, valeur brute : depot dans la boite, cache de MatterFan mis a
// jour, jamais MatterFan::attributeChangeCB pour un ordre (il publierait
// PercentCurrent = la cible, MatterFan.cpp l. 110 a 118). FanMode = On :
// intention "derniere vitesse", puis la valeur est remplacee par le FanMode
// de la derniere vitesse (la pile ne la convertit plus en High, et Maison
// recoit Success).
class MatterFanHotte : public MatterFan {
 public:
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                         esp_matter_attr_val_t *val) override {
    if (!val || endpoint_id != getEndPointId() || cluster_id != FanControl::Id || ownEcho())
      return MatterFan::attributeChangeCB(endpoint_id, cluster_id, attribute_id, val);
    const uint32_t now = millis();
    switch (attribute_id) {
      case FanControl::Attributes::FanMode::Id: {
        uint8_t m = val->val.u8;
        const bool subst = m == hotte::kFanOn && sSubstitution;
        portENTER_CRITICAL(&sBoiteMux);
        noterBrute(attribute_id, m, false, subst);
        sEp1.ecrireMode(m, now);  // la valeur brute, On compris
        portEXIT_CRITICAL(&sBoiteMux);
        if (subst) {
          m = sModeDerniere;
          val->val.u8 = m;  // la pile ecrit ce mode en base, sans conversion en High
        }
        if (m <= hotte::kFanHigh) currentFanMode = (FanMode_t)m;
        return true;
      }
      case FanControl::Attributes::PercentSetting::Id: {
        const bool nulle = val->type == ESP_MATTER_VAL_TYPE_NULLABLE_UINT8 &&
                           chip::app::NumericAttributeTraits<uint8_t>::IsNullValue(val->val.u8);
        portENTER_CRITICAL(&sBoiteMux);
        noterBrute(attribute_id, val->val.u8, nulle, false);
        if (!nulle && val->val.u8 <= 100) sEp1.ecrirePourcent(val->val.u8, now);
        portEXIT_CRITICAL(&sBoiteMux);
        if (!nulle && val->val.u8 <= 100) currentPercent = val->val.u8;
        return true;
      }
      case FanControl::Attributes::PercentCurrent::Id:  // cascade du serveur (Off) : le cache suit
        if (val->val.u8 <= 100) currentPercent = val->val.u8;
        return true;
      default:
        return MatterFan::attributeChangeCB(endpoint_id, cluster_id, attribute_id, val);
    }
  }
};

static MatterFanHotte sFan;
static MatterOnOffLight sLampe;

// Un rappel par attribut, jamais onChange() : celui-ci transmet aussi les
// valeurs en cache des autres attributs, qui passeraient pour des ordres.
static bool onLampe(bool on) {
  if (ownEcho()) return true;
  const uint32_t now = millis();
  portENTER_CRITICAL(&sBoiteMux);
  noterBrute(OnOff::Attributes::OnOff::Id, on, false, false);
  sEp2.ecrire(on, now);
  portEXIT_CRITICAL(&sBoiteMux);
  return true;
}

static volatile uint32_t sIdentify = 0;  // demandes Identify (aucun effet, 3.3)
static bool onIdentify(bool active) {
  if (active) __atomic_fetch_add(&sIdentify, 1, __ATOMIC_RELAXED);
  return true;
}

// ===========================================================================
//  Etat du pont (tache loop)
// ===========================================================================

static bool sDemarre = false;  // pile Thread demarree (verrou OT utilisable)
static uint32_t sBootMs = 0;
static bool sGardeDemarrage = true;
static bool sAPublier = false;  // reflet en attente
// Demarrage sans etat connu (confiance Inconnu, 5.1) : rien de nouveau n'est
// publie, mais les pourcentages suivent le FanMode restaure par la pile (piege
// de la NVS : jamais "High a 0 %").
static bool sRefletBase = false;
static uint32_t sIgnoreMs = 2000;  // ignore_demarrage_ms en vigueur (json : ignore_ms)
static hotte::Reflet sVoulu = {hotte::kFanOff, 0, false};
static struct {
  uint32_t fenetres, ignores, reflets, ecritures, echecs, verrouOccupe, identifies;
} sStats = {};

static void journal(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void journal(const char *fmt, ...) {
  char ligne[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ligne, sizeof(ligne), fmt, ap);
  va_end(ap);
  if (!jsonLog("matter", "notice", ligne)) jsonTexteUsb(ligne);
}

// --- Reglages (NVS produit) --------------------------------------------------

static const char *const kNvsMed = "med";
static const char *const kNvsTx = "tx_dbm";
static const char *const kNvsMaxInt = "maxint";
static const char *const kNvsDerniere = "derniere";
static uint8_t sMedProchain = MATTER_THREAD_MED ? 1 : 0;  // prochain demarrage
static uint8_t sMedDemarrage = MATTER_THREAD_MED ? 1 : 0;  // ce demarrage
static bool sTxPose = false;  // tx_dbm present en NVS
static int8_t sTxDbm = 20;

static void chargerReglages() {
  Preferences p;
  if (!p.begin(kNvsProduit, false)) return;
  if (p.isKey(kNvsMed)) sMedProchain = p.getUChar(kNvsMed, sMedProchain) ? 1 : 0;
  if (p.isKey(kNvsTx)) {
    const int8_t v = p.getChar(kNvsTx, 20);
    if (v >= 8 && v <= 20) {
      sTxPose = true;
      sTxDbm = v;
    }
  }
  if (p.isKey(kNvsMaxInt)) {
    const uint16_t v = p.getUShort(kNvsMaxInt, 20);
    if (v == 0 || (v >= 10 && v <= 3600)) sMaxIntCap = v;
  }
  if (p.isKey(kNvsDerniere)) sSubstitution = p.getUChar(kNvsDerniere, 1) != 0;
  p.end();
  sMedDemarrage = sMedProchain;
}

template <class F>
static bool ecrireNvs(F ecrire) {
  Preferences p;
  if (!p.begin(kNvsProduit, false)) return false;
  const bool ok = ecrire(p);
  p.end();
  return ok;
}

bool matterReglerMed(uint32_t v, bool *saved) {
  if (v > 1) return false;
  sMedProchain = (uint8_t)v;
  *saved = ecrireNvs([&](Preferences &p) { return p.putUChar(kNvsMed, (uint8_t)v) == 1; });
  return true;
}

// Puissance d'emission 802.15.4 (6.1, Q13) : sous le verrou OpenThread.
static bool appliquerTx(int8_t dbm) {
  if (!sDemarre || !esp_openthread_lock_acquire(pdMS_TO_TICKS(50))) return false;
  const otError e = otPlatRadioSetTransmitPower(esp_openthread_get_instance(), dbm);
  esp_openthread_lock_release();
  return e == OT_ERROR_NONE;
}

bool matterReglerTx(uint32_t dbm, bool *saved) {
  if (dbm < 8 || dbm > 20) return false;
  sTxPose = true;
  sTxDbm = (int8_t)dbm;
  const bool applique = appliquerTx(sTxDbm);
  *saved = ecrireNvs([&](Preferences &p) { return p.putChar(kNvsTx, (int8_t)dbm) == 1; });
  if (!applique) journal("[matter] tx %lu dBm enregistre, pas encore applique (pile occupee)", (unsigned long)dbm);
  return true;
}

bool matterReglerMaxint(uint32_t s, bool *saved) {
  if (s != 0 && (s < 10 || s > 3600)) return false;
  sMaxIntCap = (uint16_t)s;
  *saved = ecrireNvs([&](Preferences &p) { return p.putUShort(kNvsMaxInt, (uint16_t)s) == 2; });
  return true;
}

bool matterReglerDerniere(bool on, bool *saved) {
  sSubstitution = on;
  *saved = ecrireNvs([&](Preferences &p) { return p.putUChar(kNvsDerniere, on ? 1 : 0) == 1; });
  return true;
}

// --- Type Thread des l'init (benq, section b) ---------------------------------
//  esp_matter::start demande le role routeur (_SetThreadDeviceType) ;
//  l'enveloppe le remplace par MED quand 'matter med 1' est pose. Editeur de
//  liens : -Wl,--wrap=<symbole> et HALO_WRAP_THREAD_DEVTYPE vont ensemble.
#if HALO_WRAP_THREAD_DEVTYPE
using ThreadDeviceType = chip::DeviceLayer::ConnectivityManager::ThreadDeviceType;
extern "C" CHIP_ERROR
__real__ZN4chip11DeviceLayer8Internal40GenericThreadStackManagerImpl_OpenThreadINS0_22ThreadStackManagerImplEE20_SetThreadDeviceTypeENS0_19ConnectivityManager16ThreadDeviceTypeE(
    void *self, ThreadDeviceType type);

extern "C" CHIP_ERROR
__wrap__ZN4chip11DeviceLayer8Internal40GenericThreadStackManagerImpl_OpenThreadINS0_22ThreadStackManagerImplEE20_SetThreadDeviceTypeENS0_19ConnectivityManager16ThreadDeviceTypeE(
    void *self, ThreadDeviceType type) {
  if (sMedDemarrage && type == chip::DeviceLayer::ConnectivityManager::kThreadDeviceType_Router)
    type = chip::DeviceLayer::ConnectivityManager::kThreadDeviceType_MinimalEndDevice;
  return __real__ZN4chip11DeviceLayer8Internal40GenericThreadStackManagerImpl_OpenThreadINS0_22ThreadStackManagerImplEE20_SetThreadDeviceTypeENS0_19ConnectivityManager16ThreadDeviceTypeE(
      self, type);
}
#endif

// --- Plafond de l'intervalle maximal des abonnements neufs (benq, section c) --

// Ecrits par la tache CHIP, lus par la tache loop : operations atomiques.
static uint32_t sAbonnes = 0, sAbonnementsDemandes = 0, sAbonnementsPlafonnes = 0;

class SurveillanceAbonnements : public chip::app::ReadHandler::ApplicationCallback {
 public:
  CHIP_ERROR OnSubscriptionRequested(chip::app::ReadHandler &rh, chip::Transport::SecureSession &) override {
    uint16_t plancher = 0, max = 0;
    rh.GetReportingIntervals(plancher, max);
    const uint16_t cap = sMaxIntCap;
    __atomic_fetch_add(&sAbonnementsDemandes, 1, __ATOMIC_RELAXED);
    if (cap) {
      const uint16_t voulu = cap < plancher ? plancher : cap;
      if (voulu < max && rh.SetMaxReportingInterval(voulu) == CHIP_NO_ERROR)
        __atomic_fetch_add(&sAbonnementsPlafonnes, 1, __ATOMIC_RELAXED);
    }
    return CHIP_NO_ERROR;
  }
  void OnSubscriptionEstablished(chip::app::ReadHandler &) override {
    __atomic_fetch_add(&sAbonnes, 1, __ATOMIC_RELAXED);
  }
  void OnSubscriptionTerminated(chip::app::ReadHandler &) override {
    uint32_t n = __atomic_load_n(&sAbonnes, __ATOMIC_RELAXED);
    while (n && !__atomic_compare_exchange_n(&sAbonnes, &n, n - 1, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
    }
  }
};
static SurveillanceAbonnements sAbonnements;

// ===========================================================================
//  Identite du noeud (8.1), posee a chaque demarrage avant Matter.begin()
// ===========================================================================

static char sSerie[sizeof(MATTER_SERIAL_PREFIX) + 12] = {};

static void poserIdentite() {
  // esp_read_mac(ESP_MAC_BASE) : la MAC-48 d'usine, propre a la carte ;
  // tampon de 8 par prudence (le C6 peut rendre une EUI-64 ailleurs).
  uint8_t mac[8] = {};
  const bool macOk = esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK;
  if (macOk)
    snprintf(sSerie, sizeof(sSerie), "%s%02X%02X%02X%02X%02X%02X", MATTER_SERIAL_PREFIX, mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
  // Evaluees dans l'ordre, toutes meme apres un refus.
  const bool ok[] = {
      Matter.setVendorName(MATTER_VENDOR_NAME),
      Matter.setProductName(MATTER_PRODUCT_NAME),
      Matter.setHardwareVersion(MATTER_HW_VERSION),
      Matter.setHardwareVersionString(MATTER_HW_VERSION_STRING),
      macOk && Matter.setSerialNumber(sSerie),  // MAC illisible : numero d'usine de la pile
      Matter.setDeviceName(MATTER_NODE_LABEL),
      Matter.setSetupDiscriminator(kDiscriminateur),
      Matter.setSetupPasscode(kCodeAppairage),
  };
  static const char *const kQuoi[] = {"fabricant", "produit", "version materielle", "materiel (texte)",
                                      "numero de serie", "nom du noeud", "discriminateur", "code d'appairage"};
  for (size_t i = 0; i < sizeof(ok); i++)
    if (!ok[i]) {
      char ligne[96];
      snprintf(ligne, sizeof(ligne), "!! identite Matter : %s refuse, valeur d'usine de la pile gardee", kQuoi[i]);
      jsonTexteUsb(ligne);
    }
}

// ===========================================================================
//  Cycle de vie
// ===========================================================================

bool matterOtTryLock(uint32_t totalMs) {
  return sDemarre && esp_openthread_lock_acquire(pdMS_TO_TICKS(totalMs / 2));
}
void matterOtUnlock() { esp_openthread_lock_release(); }

void matterHotteDerniere(hotte::Moteur m) { sModeDerniere = hotte::fanMode(m == hotte::Moteur::Arret ? hotte::Moteur::V2 : m); }

void matterHotteBegin(const hotte::Etat &etatInitial, hotte::Moteur derniereVitesse) {
  sLoopTask = xTaskGetCurrentTaskHandle();
  chargerReglages();
  matterHotteDerniere(derniereVitesse);
  // Avant le premier begin() d'endpoint : c'est lui qui cree le noeud (4.4).
  if (!Matter.selectNetwork(MATTER_NETWORK_THREAD))
    jsonTexteUsb("!! selectNetwork(THREAD) refuse : le noeud resterait en Wi-Fi");
  const hotte::Reflet r = hotte::reflet(etatInitial);
  sFan.begin(r.pourcent, (MatterFan::FanMode_t)r.fanMode, MatterFan::FAN_MODE_SEQ_OFF_LOW_MED_HIGH);
  sLampe.begin(r.lampe);
  sLampe.onChangeOnOff(onLampe);
  sFan.onIdentify(onIdentify);
  sLampe.onIdentify(onIdentify);
  poserIdentite();
  Matter.begin();
  // Matter.begin() ne rend rien : l'instance OpenThread n'existe qu'apres
  // esp_openthread_init, qui cree le verrou.
  sDemarre = chip::DeviceLayer::ThreadStackMgrImpl().OTInstance() != nullptr;
  if (!sDemarre) {
    jsonTexteUsb("!! pile Thread absente");
  } else {
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    chip::app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&sAbonnements);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    if (sTxPose && !appliquerTx(sTxDbm)) jsonTexteUsb("!! puissance d'emission non appliquee au demarrage");
  }
  // Les ecritures de demarrage de la pile passent par les rappels : le delai
  // du garde-fou part d'ici. Premier reflet force : la pile a restaure FanMode
  // et OnOff depuis sa NVS, pas les pourcentages (parade du piege, 4.4).
  sBootMs = millis();
  sVoulu = r;
  sRefletBase = etatInitial.confiance == hotte::Confiance::Inconnu;
  sAPublier = true;
}

void matterHottePublier(const hotte::Etat &e) {
  if (e.confiance == hotte::Confiance::Inconnu) return;  // rien de nouveau n'est publie (5.1)
  sVoulu = hotte::reflet(e);
  sRefletBase = false;
  sAPublier = true;
}

// Valeur d'un attribut uint8 ou booleen, nullable ou non.
static bool lire(MatterEndPoint &ep, uint32_t cl, uint32_t attr, esp_matter_attr_val_t *v, uint8_t *out) {
  *v = esp_matter_invalid(nullptr);
  if (!ep.getAttributeVal(cl, attr, v)) return false;
  switch ((int)v->type & ~ESP_MATTER_VAL_NULLABLE_BASE) {
    case ESP_MATTER_VAL_TYPE_BOOLEAN: *out = v->val.b ? 1 : 0; return true;
    case ESP_MATTER_VAL_TYPE_UINT8:
    case ESP_MATTER_VAL_TYPE_ENUM8: *out = v->val.u8; return true;
    default: return false;
  }
}

// Ecrit la valeur voulue si l'attribut (la base, pas le cache) differe.
static void aligner(MatterEndPoint &ep, uint32_t cl, uint32_t attr, uint8_t voulu) {
  esp_matter_attr_val_t v;
  uint8_t cur = 0;
  if (!lire(ep, cl, attr, &v, &cur)) {
    sStats.echecs++;
    return;
  }
  if (cur == voulu && !(((int)v.type & ESP_MATTER_VAL_NULLABLE_BASE) &&
                        chip::app::NumericAttributeTraits<uint8_t>::IsNullValue(v.val.u8)))
    return;
  if (((int)v.type & ~ESP_MATTER_VAL_NULLABLE_BASE) == ESP_MATTER_VAL_TYPE_BOOLEAN) v.val.b = voulu != 0;
  else v.val.u8 = voulu;
  if (ep.updateAttributeVal(cl, attr, &v)) sStats.ecritures++;
  else sStats.echecs++;
}

// Sous le verrou de la pile, sans attendre : occupe, nouvel essai au tour
// suivant. Jamais de reflet tant que la boite n'est pas vide.
static bool refleter() {
  if (!chip::DeviceLayer::PlatformMgr().TryLockChipStack()) {
    sStats.verrouOccupe++;
    return false;
  }
  portENTER_CRITICAL(&sBoiteMux);
  const bool attente = sEp1.ouvert() || sEp2.ouvert();
  portEXIT_CRITICAL(&sBoiteMux);
  if (attente) {
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    return false;
  }
  if (sRefletBase) {  // la base telle que la pile l'a restauree, pourcentages alignes
    esp_matter_attr_val_t v;
    uint8_t mode = hotte::kFanOff, on = 0;
    lire(sFan, FanControl::Id, FanControl::Attributes::FanMode::Id, &v, &mode);
    lire(sLampe, OnOff::Id, OnOff::Attributes::OnOff::Id, &v, &on);
    const hotte::Moteur m = mode == hotte::kFanLow      ? hotte::Moteur::V1
                            : mode == hotte::kFanMedium ? hotte::Moteur::V2
                            : mode == hotte::kFanHigh   ? hotte::Moteur::V3
                                                        : hotte::Moteur::Arret;
    sVoulu.fanMode = hotte::fanMode(m);
    sVoulu.pourcent = hotte::pourcent(m);
    sVoulu.lampe = on != 0;
    sRefletBase = false;
  }
  // L'arret publie toujours FanMode Off ET 0 % (3.2) ; un palier, sa valeur canonique.
  aligner(sFan, FanControl::Id, FanControl::Attributes::FanMode::Id, sVoulu.fanMode);
  aligner(sFan, FanControl::Id, FanControl::Attributes::PercentSetting::Id, sVoulu.pourcent);
  aligner(sFan, FanControl::Id, FanControl::Attributes::PercentCurrent::Id, sVoulu.pourcent);
  aligner(sLampe, OnOff::Id, OnOff::Attributes::OnOff::Id, sVoulu.lampe ? 1 : 0);
  chip::DeviceLayer::PlatformMgr().UnlockChipStack();
  sStats.reflets++;
  return true;
}

// Dernier controleur parti sans remise a zero (accessoire retire de Maison) :
// la cle de l'app part aussi (7.7). Seul un passage observe compte.
static void surveillerProprietaire(uint32_t now) {
  static int8_t sVu = -1;
  static uint32_t sAt = 0;
  if (!sDemarre || (sVu >= 0 && now - sAt < 500)) return;
  sAt = now;
  const bool enService = Matter.isDeviceCommissioned();
  char kid[9];
  if (sVu == 1 && !enService && netUdpKid(kid)) {
    const bool ok = netUdpKeyErase();
    journal(ok ? "[matter] plus aucun controleur : cle H1 effacee" : "[matter] plus aucun controleur : effacement NVS de la cle en echec");
  }
  sVu = enService ? 1 : 0;
}

void matterHottePoll(hotte::Automate &a, uint32_t now) {
  surveillerProprietaire(now);
  const hotte::Params &p = a.params();
  sIgnoreMs = p.ignoreDemarrageMs;
  hotte::CibleVentilo cible = hotte::CibleVentilo::Aucune;
  bool lampe = false, ep1 = false, ep2 = false;
  uint32_t premier1 = 0, premier2 = 0;
  portENTER_CRITICAL(&sBoiteMux);
  ep1 = sEp1.pret(now, p.lissageCalmeMs, p.lissagePlafondMs, &cible, &premier1);
  ep2 = sEp2.pret(now, p.ordreCalmeMs, p.lissagePlafondMs, &lampe, &premier2);
  portEXIT_CRITICAL(&sBoiteMux);
  if (ep1 || ep2) {
    sStats.fenetres++;
    const uint32_t premier = ep1 && ep2 ? ((int32_t)(premier1 - premier2) < 0 ? premier1 : premier2)
                                        : ep1 ? premier1 : premier2;
    if (sGardeDemarrage && hotte::ignoreeAuDemarrage(premier, sBootMs, p.ignoreDemarrageMs)) {
      // Rien n'est injecte au demarrage (5.7) ; l'etat reel est republie.
      sStats.ignores++;
      journal("[matter] ordres ignores au demarrage :%s%s", ep1 ? " ventilateur" : "", ep2 ? " lampe" : "");
      sAPublier = true;
    } else {
      if (ep1 && cible != hotte::CibleVentilo::Aucune) a.ordreVentilo(cible, 0, hotte::Canal::Matter, now);
      if (ep2) a.ordreLampe(lampe, 0, hotte::Canal::Matter, now);
      if (ep1 && cible == hotte::CibleVentilo::Aucune) sAPublier = true;  // Auto, Smart : realigner
    }
  }
  if (sGardeDemarrage && now - sBootMs >= p.ignoreDemarrageMs + p.lissagePlafondMs) sGardeDemarrage = false;
  if (sAPublier && refleter()) sAPublier = false;
  const uint32_t id = __atomic_load_n(&sIdentify, __ATOMIC_RELAXED);
  if (id != sStats.identifies) {
    sStats.identifies = id;
    journal("[matter] Identify demande : aucun effet (3.3)");
  }
}

// ===========================================================================
//  Statut
// ===========================================================================

bool matterHotteMisEnService() { return Matter.isDeviceCommissioned(); }

void matterHotteDecommission() {
  // La cle de l'app part d'abord (7.7) ; deux essais : un echec la laisserait
  // revenir au demarrage suivant.
  if (!netUdpKeyErase() && !netUdpKeyErase()) journal("[matter] cle H1 : effacement NVS en echec");
  Matter.decommission();
}

void matterHotteEtat(jsonp::MatterEtat *m, uint32_t now) {
  static uint8_t sFabriques = 0;
  m->demarre = sDemarre;
  m->misEnService = Matter.isDeviceCommissioned();
  if (sDemarre && chip::DeviceLayer::PlatformMgr().TryLockChipStack()) {
    sFabriques = chip::Server::GetInstance().GetFabricTable().FabricCount();
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
  }
  m->fabriques = sFabriques;
  m->abonnements = (uint16_t)__atomic_load_n(&sAbonnes, __ATOMIC_RELAXED);
  const uint32_t fin = sBootMs + sIgnoreMs;
  m->ignoreResteMs = sGardeDemarrage && (int32_t)(fin - now) > 0 ? fin - now : 0;
  m->maxintS = sMaxIntCap;
  m->med = sMedProchain;
  m->txConnu = false;
  if (sDemarre && esp_openthread_lock_acquire(0)) {
    int8_t dbm = 0;
    m->txConnu = otPlatRadioGetTransmitPower(esp_openthread_get_instance(), &dbm) == OT_ERROR_NONE;
    m->txDbm = dbm;
    esp_openthread_lock_release();
  }
  m->derniere = sSubstitution;
  m->ecritures = sStats.ecritures;
  m->reflets = sStats.reflets;
  m->verrouOccupe = sStats.verrouOccupe;
  m->ignores = sStats.ignores;
}

void matterHotteRadio(jsonp::RadioCompteurs *r) {
  static jsonp::RadioCompteurs sDernier;
  if (!sDemarre || !esp_openthread_lock_acquire(0)) {  // occupe : la derniere lecture
    *r = sDernier;
    return;
  }
  otInstance *ot = esp_openthread_get_instance();
  jsonp::RadioCompteurs n;
  n.connu = true;
  const otDeviceRole role = otThreadGetDeviceRole(ot);
  n.role = otThreadDeviceRoleToString(role);
  int8_t dbm = 0;
  n.txConnu = otPlatRadioGetTransmitPower(ot, &dbm) == OT_ERROR_NONE;
  n.txDbm = dbm;
  n.sensibiliteDbm = otPlatRadioGetReceiveSensitivity(ot);
  if (role == OT_DEVICE_ROLE_CHILD) {  // MED : le lien avec le parent
    otRouterInfo parent;
    int8_t rssi = 0;
    if (otThreadGetParentInfo(ot, &parent) == OT_ERROR_NONE && otThreadGetParentAverageRssi(ot, &rssi) == OT_ERROR_NONE) {
      n.lien = n.lienParent = true;
      n.rssiMoyenDbm = rssi;
      n.lqIn = parent.mLinkQualityIn;
      n.lqOut = parent.mLinkQualityOut;
    }
  } else if (role == OT_DEVICE_ROLE_ROUTER || role == OT_DEVICE_ROLE_LEADER) {
    // Routeur : le voisin routeur de meilleur RSSI moyen, et sa qualite de lien.
    otNeighborInfoIterator it = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    otNeighborInfo nb;
    bool trouve = false;
    uint16_t rloc = 0;
    while (otThreadGetNextNeighborInfo(ot, &it, &nb) == OT_ERROR_NONE) {
      if (nb.mIsChild || (trouve && nb.mAverageRssi <= n.rssiMoyenDbm)) continue;
      trouve = true;
      n.rssiMoyenDbm = nb.mAverageRssi;
      rloc = nb.mRloc16;
    }
    otRouterInfo ri;
    if (trouve && otThreadGetRouterInfo(ot, (uint16_t)(rloc >> 10), &ri) == OT_ERROR_NONE) {
      n.lien = true;
      n.lqIn = ri.mLinkQualityIn;
      n.lqOut = ri.mLinkQualityOut;
    }
  }
  if (const otMacCounters *mc = otLinkGetCounters(ot)) {
    n.txTotal = mc->mTxTotal;
    n.txRetry = mc->mTxRetry;
    n.txEchecs = mc->mTxDirectMaxRetryExpiry;
  }
  if (const otMleCounters *ml = otThreadGetMleCounters(ot)) {
    n.changementsParent = ml->mParentChanges;
    n.changementsRole = (uint32_t)ml->mDetachedRole + ml->mChildRole + ml->mRouterRole + ml->mLeaderRole;
  }
  esp_openthread_lock_release();
  sDernier = n;
  *r = n;
}

void matterHotteJournal(Print &out) {
  portENTER_CRITICAL(&sBoiteMux);
  Brute copie[kJournal];
  const uint8_t n = sJournalN;
  for (uint8_t i = 0; i < kJournal; i++) copie[i] = sJournal[i];
  portEXIT_CRITICAL(&sBoiteMux);
  const uint8_t nb = n < kJournal ? n : kJournal;
  out.printf("ecritures brutes (%u dernieres) :\n", nb);
  for (uint8_t k = 0; k < nb; k++) {
    const Brute &b = copie[(uint8_t)(n - nb + k) % kJournal];
    const char *quoi = b.attr == FanControl::Attributes::FanMode::Id          ? "FanMode"
                       : b.attr == FanControl::Attributes::PercentSetting::Id ? "PercentSetting"
                                                                               : "OnOff";
    out.printf("  %8lu ms  %-14s %s%s\n", (unsigned long)b.ms, quoi, b.nulle ? "null" : String(b.valeur).c_str(),
               b.substituee ? " (On -> derniere vitesse)" : "");
  }
}

void matterHotteStatut(Print &out) {
  out.printf("matter : %s, %s, identite %s / %s / n/s %s\n", sDemarre ? "pile demarree" : "PILE ABSENTE",
             Matter.isDeviceCommissioned() ? "mis en service" : "pas encore mis en service", MATTER_VENDOR_NAME,
             MATTER_PRODUCT_NAME, sSerie[0] ? sSerie : "?");
  if (!Matter.isDeviceCommissioned()) {
    out.printf("  code manuel     : %s\n", Matter.getManualPairingCode().c_str());
    out.printf("  QR              : %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
  out.printf("  role            : %s au prochain demarrage (%s a celui-ci)\n", sMedProchain ? "MED" : "routeur",
             sMedDemarrage ? "MED" : "routeur");
  out.printf("  tx              : %s\n", sTxPose ? String(String(sTxDbm) + " dBm (NVS)").c_str() : "valeur de la pile");
  out.printf("  maxint          : %u s%s\n", (unsigned)sMaxIntCap, sMaxIntCap ? "" : " (celui du controleur)");
  out.printf("  derniere        : substitution de On %s\n", sSubstitution ? "active" : "coupee (banc B2)");
  out.printf("  abonnements     : %lu actifs, %lu demandes, %lu plafonnes\n",
             (unsigned long)__atomic_load_n(&sAbonnes, __ATOMIC_RELAXED),
             (unsigned long)__atomic_load_n(&sAbonnementsDemandes, __ATOMIC_RELAXED),
             (unsigned long)__atomic_load_n(&sAbonnementsPlafonnes, __ATOMIC_RELAXED));
  out.printf("  fenetres %lu, ignorees %lu, reflets %lu, ecritures %lu (echecs %lu), verrou occupe %lu, Identify %lu\n",
             (unsigned long)sStats.fenetres, (unsigned long)sStats.ignores, (unsigned long)sStats.reflets,
             (unsigned long)sStats.ecritures, (unsigned long)sStats.echecs, (unsigned long)sStats.verrouOccupe,
             (unsigned long)sStats.identifies);
}
