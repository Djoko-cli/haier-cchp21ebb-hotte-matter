// ===========================================================================
//  Injection sur la ligne D, voie 1 (docs/SPEC-RECONNAISSANCE.md 8.4)
//
//  Etage (docs/WIRING.md section 6) : GPIO7 -> 4,7k -> base de Q2, collecteur
//  par 470 ohms sur D. GPIO7 haut = bus tire bas ; GPIO7 bas = bus relache
//  (drain ouvert : jamais de 3,3 V pousse sur D). Hors emission, GPIO7 est une
//  sortie simple a l'etat bas (premiere instruction de setup(), puis
//  brocheBasse() apres chaque emission).
//
//  Une demande acceptee (injectionDemander) :
//   1. attend le silence : aucun front depuis silence_min_us (bordDernierUs)
//      et bus haut. Au-dela de attente_max_ms : resultat 'delai', rien n'est
//      emis ;
//   2. part en une seule transaction du pilote RMT d'IDF (driver/rmt_tx.h) :
//      canal cree pour cette emission sur GPIO7, 1 MHz, encodeur copie,
//      niveau initial et de fin GPIO bas. La surveillance des fronts
//      (bord.cpp) est armee juste avant rmt_transmit ;
//   3. la tache loop attend activement la fin de la transaction (la duree de
//      la demande : 200 ms au plus, borne haute de total_max_us), ou
//      une collision signalee par l'interruption des fronts : rmt_disable
//      arrete alors l'emission tout de suite (arret asynchrone du C6) ;
//   4. canal supprime, GPIO7 rendue a une sortie simple a l'etat bas, delai
//      minimal compte depuis la fin de l'emission, evenement 'injection'
//      (ok, collision, delai, erreur) avec les durees relues ; en texte si la
//      commande venait de la console en mode humain.
//  Une seule demande a la fois. L'interruption ne fait que poser des
//  drapeaux ; seule la tache loop produit des lignes (spec 8.7).
// ===========================================================================
#include "injection.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rmt_tx.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include <string.h>

#include "bord.h"
#include "config.h"
#include "json_mode.h"

static_assert(jsonp::kInjDurMax == inj::kDurMax, "injection.dur_us : autant de durees que inj::kDurMax");

namespace {

constexpr uint32_t kMargeUs = 20000;  // transaction pas finie 20 ms apres la fin prevue : abandonnee (jamais vu)

enum class Resultat : uint8_t { Ok, Collision, Delai, Erreur };

const char *texte(Resultat r) {
  switch (r) {
    case Resultat::Ok: return "ok";
    case Resultat::Collision: return "collision";
    case Resultat::Delai: return "delai";
    case Resultat::Erreur: return "erreur";
  }
  return "erreur";
}

// Demande acceptee, en attente du silence.
struct EnAttente {
  bool active = false;
  inj::Demande d;
  inj::Params p;  // valeurs au moment de l'acceptation
  uint32_t id = 0;
  uint8_t origine = jsonp::kUsb;
  char cmd[jsonp::kCmdTextMax + 1] = {};
  uint64_t accepteUs = 0;
};

inj::Params sParams;
inj::Etat sEtat;
bool sArmee = false;  // armee au tour precedent : annonce du desarmement seul
EnAttente sE;
rmt_encoder_handle_t sCopie = nullptr;
rmt_symbol_word_t sSym[inj::kSymMax];  // lu par le pilote pendant l'emission
size_t sNSym = 0;
volatile bool sFini = false;  // rappel on_trans_done
uint32_t sRelu[inj::kDurMax];
bool sDerniere = false;  // etat.injection.derniere
uint32_t sDerniereId = 0;
Resultat sDerniereRes = Resultat::Ok;
char sTexte[jsonp::kLineMax];

bool IRAM_ATTR finEmission(rmt_channel_handle_t, const rmt_tx_done_event_data_t *, void *) {
  sFini = true;
  return false;
}

// Annonce : log 'injection' en mode 'json log 1', sinon texte sur l'USB.
void annonce(const char *txt) {
  if (!jsonLog("injection", "notice", txt)) Serial.println(txt);
}

// GPIO7 : sortie simple a l'etat bas (bus relache, Q2 bloque).
void brocheBasse() {
  gpio_set_level((gpio_num_t)kPinInjection, 0);
  gpio_set_direction((gpio_num_t)kPinInjection, GPIO_MODE_OUTPUT);
}

// Emet sE.d (tache loop, attente active). *nRelu : durees relues.
Resultat emettre(uint16_t *nRelu) {
  *nRelu = 0;
  rmt_channel_handle_t canal = nullptr;
  rmt_tx_channel_config_t c = {};
  c.gpio_num = (gpio_num_t)kPinInjection;
  c.clk_src = RMT_CLK_SRC_DEFAULT;
  c.resolution_hz = 1000000;  // 1 tick = 1 us
  c.mem_block_symbols = inj::kSymMax;
  c.trans_queue_depth = 1;
  c.flags.init_level = 0;  // bus relache
  esp_err_t err = rmt_new_tx_channel(&c, &canal);
  if (err == ESP_OK) {
    rmt_tx_event_callbacks_t cb = {};
    cb.on_trans_done = finEmission;
    err = rmt_tx_register_event_callbacks(canal, &cb, nullptr);
  }
  if (err == ESP_OK) err = rmt_enable(canal);
  if (err != ESP_OK) {
    if (canal) rmt_del_channel(canal);
    brocheBasse();
    snprintf(sTexte, sizeof(sTexte), "[injection] canal RMT : %s, rien n'est emis", esp_err_to_name(err));
    annonce(sTexte);
    return Resultat::Erreur;
  }
  rmt_transmit_config_t t = {};
  t.flags.eot_level = 0;  // fin : bus relache
  sFini = false;
  const uint64_t t0 = esp_timer_get_time();
  const uint64_t finFenetre = t0 + inj::kDepartMaxUs + inj::totalUs(sE.d) + sE.p.tolUs;
  bordSurveiller(&sE.d, t0, sE.p.tolUs);
  Resultat res = Resultat::Ok;
  bool arrete = false;
  err = rmt_transmit(canal, sCopie, sSym, sNSym * sizeof(rmt_symbol_word_t), &t);
  if (err != ESP_OK) {
    res = Resultat::Erreur;
  } else {
    for (;;) {
      if (bordCollision()) {
        rmt_disable(canal);  // arret immediat de l'emission
        arrete = true;
        res = Resultat::Collision;
        break;
      }
      const uint64_t maintenant = esp_timer_get_time();
      if (sFini && maintenant >= finFenetre) break;  // fin, plus la tolerance du dernier front
      if (maintenant >= finFenetre + kMargeUs) {
        res = Resultat::Erreur;
        break;
      }
    }
  }
  bordFinSurveillance();
  *nRelu = bordRelu(sRelu, inj::kDurMax);
  if (!arrete) rmt_disable(canal);
  rmt_del_channel(canal);
  brocheBasse();
  if (res == Resultat::Erreur) {
    snprintf(sTexte, sizeof(sTexte), "[injection] emission : %s",
             err != ESP_OK ? esp_err_to_name(err) : "transaction jamais finie, arretee");
    annonce(sTexte);
  }
  return res;
}

// Fin d'une demande : evenement 'injection', etat, texte.
void terminer(Resultat r, uint32_t attenteUs, uint16_t nRelu) {
  sE.active = false;
  jsonp::InjectionEv e;
  e.id = sE.id;
  e.cmd = sE.cmd;
  e.resultat = texte(r);
  e.niv0Haut = false;  // 'injecte durees' : la premiere duree est basse
  e.dur = sE.d.dur;
  e.n = sE.d.n;
  e.attenteUs = attenteUs;
  e.relu = sRelu;
  e.nRelu = nRelu;
  // L'evenement est la suite de la commande : son id va a la session d'origine.
  const uint8_t avant = jsonOrigin();
  jsonSetOrigin(sE.origine);
  jsonInjection(e);
  jsonSetOrigin(avant);
  sDerniere = true;
  sDerniereId = sE.id;
  sDerniereRes = r;
  if (r == Resultat::Ok && !nRelu) annonce("[injection] aucun front relu : etage d'injection, ligne ou etage d'ecoute ?");
  if (sE.origine != jsonp::kUsb || jsonMachine() || sE.id) return;
  // Console en mode humain, commande sans id : 'injection : <resultat>, attente <us> us ; emises ... ; relues ...'.
  const size_t cap = sizeof(sTexte) - 16;
  size_t k = (size_t)snprintf(sTexte, sizeof(sTexte), "injection : %s, attente %lu us ; emises", texte(r),
                              (unsigned long)attenteUs);
  for (uint16_t i = 0; i < sE.d.n && k < cap; i++)
    k += (size_t)snprintf(sTexte + k, sizeof(sTexte) - k, " %lu", (unsigned long)sE.d.dur[i]);
  if (k < cap) k += (size_t)snprintf(sTexte + k, sizeof(sTexte) - k, " ; relues%s", nRelu ? "" : " aucune");
  for (uint16_t i = 0; i < nRelu && k < cap; i++)
    k += (size_t)snprintf(sTexte + k, sizeof(sTexte) - k, " %lu", (unsigned long)sRelu[i]);
  Serial.println(sTexte);
}

}  // namespace

bool injectionBegin(const inj::Params &p, bool montee) {
  sParams = inj::paramsValides(p) ? p : inj::Params();
  sEtat.setMontee(montee);
  if (!montee) {
    sEtat.desarmer();
    sArmee = false;
  }
  if (sCopie) return true;
  rmt_copy_encoder_config_t e = {};
  const esp_err_t err = rmt_new_copy_encoder(&e, &sCopie);
  if (err != ESP_OK) {
    sCopie = nullptr;
    log_e("[injection] encodeur RMT : %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

inj::Refus injectionDemander(const inj::Demande &d, uint32_t id, const char *cmd, uint32_t nowMs) {
  if (sE.active) return inj::Refus::Delai;  // une demande deja en attente du silence
  const inj::Refus r = sEtat.admettre(nowMs, sParams);
  if (r != inj::Refus::Aucun) return r;
  inj::Sym y[inj::kSymMax];
  const size_t n = inj::versSymboles(d, y, inj::kSymMax);
  if (!n) return inj::Refus::Syntaxe;  // demande vide (inj::analyser n'en rend jamais)
  for (size_t i = 0; i < n; i++) {
    sSym[i].duration0 = y[i].d0;
    sSym[i].level0 = y[i].l0;
    sSym[i].duration1 = y[i].d1;
    sSym[i].level1 = y[i].l1;
  }
  sNSym = n;
  sE.d = d;
  sE.p = sParams;
  sE.id = id;
  sE.origine = jsonOrigin();
  jsonp::copyCmd(sE.cmd, cmd ? cmd : "");
  sE.accepteUs = esp_timer_get_time();
  sE.active = true;
  sEtat.noterEmission(nowMs);  // une seule injection a la fois ; le delai repart a la fin de l'emission
  return inj::Refus::Aucun;
}

void injectionPoll() {
  const uint32_t nowMs = millis();
  const bool armee = sEtat.armee(nowMs, sParams);
  if (sArmee && !armee) {
    sEtat.desarmer();
    snprintf(sTexte, sizeof(sTexte), "[injection] desarmee seule (%lu s ecoulees)", (unsigned long)sParams.armeMaxS);
    annonce(sTexte);
  }
  sArmee = armee;
  if (!sE.active) return;
  const uint64_t maintenant = esp_timer_get_time();
  const uint32_t attente = (uint32_t)(maintenant - sE.accepteUs);
  if (!sEtat.montee() || !armee) {
    annonce("[injection] desarmee ou etage non monte avant l'emission : rien n'est emis");
    terminer(Resultat::Erreur, attente, 0);
    return;
  }
  if (!bordActif() || !sCopie) {
    annonce("[injection] interruption des fronts ou encodeur RMT indisponible : rien n'est emis");
    terminer(Resultat::Erreur, attente, 0);
    return;
  }
  const uint64_t dernier = bordDernierUs();
  const bool silence = (!dernier || maintenant - dernier >= sE.p.silenceMinUs) && bordBusHaut();
  if (!silence) {
    if (attente >= (uint64_t)sE.p.attenteMaxMs * 1000u) terminer(Resultat::Delai, attente, 0);
    return;
  }
  uint16_t nRelu = 0;
  const Resultat r = emettre(&nRelu);
  if (r != Resultat::Erreur) sEtat.noterEmission(millis());  // le delai minimal court depuis la fin
  terminer(r, attente, nRelu);
}

void injectionJson(jsonp::Writer &w, uint32_t nowMs) {
  w.boolean("montee", sEtat.montee());
  w.boolean("armee", sEtat.armee(nowMs, sParams));
  w.u32("arme_reste_s", sEtat.resteS(nowMs, sParams));
  if (!sDerniere) {
    w.null("derniere");
    return;
  }
  w.obj("derniere");
  if (sDerniereId) w.u32("id", sDerniereId);
  else w.null("id");
  w.str("resultat", texte(sDerniereRes));
  w.end();
}

inj::Etat &injectionEtat() { return sEtat; }

const inj::Params &injectionParams() { return sParams; }

inj::Refus injectionArmer(uint32_t nowMs) {
  if (!sEtat.montee()) return inj::Refus::NonMontee;
  sEtat.armer(nowMs);
  sArmee = true;
  return inj::Refus::Aucun;
}

void injectionDesarmer() {
  sEtat.desarmer();
  sArmee = false;
}
