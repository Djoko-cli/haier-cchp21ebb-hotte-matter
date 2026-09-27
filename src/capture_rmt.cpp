// ===========================================================================
//  Capture continue de la ligne D par le pilote RMT d'IDF (spec 8.2)
//
//  Canal RX sur GPIO6 (kPinEcoute), une memoire de 48 symboles (le pilote la
//  vide par moities de 24, en ping-pong), reception partielle
//  (flags.en_partial_rx). Le pilote copie chaque moitie pleine dans sRecu et
//  appelle le rappel on_recv_done (esp_driver_rmt/src/rmt_rx.c, IDF 5.5) :
//   - is_last = false quand sRecu est plein et qu'une moitie de plus arrive
//     (reception de plus de kSymRecus symboles) : edata rend sRecu entier,
//     que le pilote reecrit des le retour du rappel ;
//   - is_last = true au silence (seuil d'inactivite du canal) : edata rend la
//     fin de la reception, eventuellement vide. Le canal est alors arrete ; le
//     rappel le rearme aussitot (rmt_receive est permis en interruption).
//  Le rappel (en IRAM) horodate le bloc (esp_timer_get_time) et le copie dans
//  l'anneau sAnneau de kTamponSymboles symboles, avec son en-tete, sous une
//  section critique (sMux). La tache loop vide l'anneau (capturePoll) a
//  travers le decoupeur (capture_model). Anneau ou en-tetes pleins : le bloc
//  est perdu et compte (debord), le suivant porte debordAvant.
//
//  L'interruption du RMT n'est pas sure pendant une ecriture en flash
//  (CONFIG_RMT_RX_ISR_CACHE_SAFE absent du coeur) : elle attend la fin de
//  l'ecriture, et le rappel peut appeler rmt_receive, qui est en flash.
//
//  Limites connues :
//   - un bloc non final est rendu apres la moitie suivante (24 symboles), ou,
//     s'il est rendu par la fin de reception, apres les derniers symboles et
//     le silence : son t_us (tFinUs - somme, spec 8.2) est en retard d'autant.
//     Seules les receptions de plus de kSymRecus symboles (480 durees) sont
//     concernees ;
//   - pendant une ecriture en NVS (commandes seuils, capture tout|changements),
//     plus de 24 symboles d'affilee ecraseraient la memoire du canal ;
//   - captureBegin() abandonne les blocs pas encore lus.
// ===========================================================================
#include "capture_rmt.h"

#include <Arduino.h>
#include <driver/rmt_rx.h>
#include <esp_attr.h>
#include <esp_timer.h>

#include "config.h"

namespace {

constexpr size_t kMemCanal = 48;   // SOC_RMT_MEM_WORDS_PER_CHANNEL : un bloc, ping-pong de 24
constexpr size_t kSymRecus = 240;  // tampon du pilote : 10 moities
constexpr uint32_t kEntetes = 64;  // blocs en attente dans l'anneau (puissance de 2)

struct Entete {
  uint64_t tFinUs;
  uint32_t debut;  // indice du premier symbole dans sAnneau
  uint16_t n;
  bool dernier, debordAvant;
};

rmt_channel_handle_t sCanal = nullptr;
rmt_receive_config_t sRecv = {};
capt::Reglages sReglages;
capt::Decoupeur sDecoupeur;
volatile bool sActif = false;  // faux : le rappel ne rearme plus le canal

// Tampon du pilote, lu dans le rappel seulement.
rmt_symbol_word_t sRecu[kSymRecus];

// Anneau : ecrit par le rappel, lu par la tache loop. Compteurs libres (indice
// = compteur modulo la taille, puissances de 2), sous sMux.
portMUX_TYPE sMux = portMUX_INITIALIZER_UNLOCKED;
rmt_symbol_word_t sAnneau[kTamponSymboles];
Entete sEntetes[kEntetes];
uint32_t sSymEcrits = 0, sSymLus = 0, sEntEcrits = 0, sEntLus = 0;
bool sPerte = false;
uint32_t sBlocs = 0, sSymboles = 0, sDebord = 0;

// Tache loop seulement.
uint32_t sParties = 0;
capt::Sym sSym[kSymRecus];  // bloc en cours de decoupage

bool IRAM_ATTR surReception(rmt_channel_handle_t canal, const rmt_rx_done_event_data_t *e, void *) {
  const uint64_t t = esp_timer_get_time();
  const uint32_t n = (uint32_t)e->num_symbols;
  portENTER_CRITICAL_ISR(&sMux);
  sBlocs++;
  sSymboles += n;
  if (n > kTamponSymboles - (sSymEcrits - sSymLus) || sEntEcrits - sEntLus >= kEntetes) {
    sPerte = true;  // bloc perdu : le suivant porte debordAvant
    sDebord++;
  } else {
    Entete &h = sEntetes[sEntEcrits % kEntetes];
    h.tFinUs = t;
    h.debut = sSymEcrits % kTamponSymboles;
    h.n = (uint16_t)n;
    h.dernier = e->flags.is_last;
    h.debordAvant = sPerte;
    for (uint32_t i = 0; i < n; i++) sAnneau[(sSymEcrits + i) % kTamponSymboles] = e->received_symbols[i];
    sSymEcrits += n;
    sEntEcrits++;
    sPerte = false;
  }
  portEXIT_CRITICAL_ISR(&sMux);
  // Fin de reception : le pilote a arrete le canal ; on le rearme tout de suite.
  if (e->flags.is_last && sActif) rmt_receive(canal, sRecu, sizeof(sRecu), &sRecv);
  return false;
}

bool echec(const char *quoi, esp_err_t err) {
  log_e("[capture] %s : %s", quoi, esp_err_to_name(err));
  captureEnd();
  return false;
}

}  // namespace

bool captureBegin(const capt::Reglages &r) {
  captureEnd();
  sReglages = r;
  capt::borner(&sReglages);
  portENTER_CRITICAL(&sMux);
  sSymEcrits = sSymLus = sEntEcrits = sEntLus = 0;
  sPerte = false;
  portEXIT_CRITICAL(&sMux);
  sDecoupeur.reset();

  rmt_rx_channel_config_t c = {};
  c.gpio_num = (gpio_num_t)kPinEcoute;
  c.clk_src = RMT_CLK_SRC_DEFAULT;  // PLL 80 MHz : filtre jusqu'a 255 cycles (3,19 us)
  c.resolution_hz = sReglages.resolHz;
  c.mem_block_symbols = kMemCanal;
  esp_err_t err = rmt_new_rx_channel(&c, &sCanal);
  if (err != ESP_OK) {
    sCanal = nullptr;
    return echec("rmt_new_rx_channel", err);
  }
  rmt_rx_event_callbacks_t cb = {};
  cb.on_recv_done = surReception;
  err = rmt_rx_register_event_callbacks(sCanal, &cb, nullptr);
  if (err != ESP_OK) return echec("rmt_rx_register_event_callbacks", err);
  err = rmt_enable(sCanal);
  if (err != ESP_OK) return echec("rmt_enable", err);
  sRecv = {};
  sRecv.signal_range_min_ns = (uint32_t)sReglages.filtreUs * 1000u;  // 0 : filtre coupe
  sRecv.signal_range_max_ns = sReglages.silenceUs * 1000u;           // silence : fin de reception
  sRecv.flags.en_partial_rx = 1;
  sActif = true;
  err = rmt_receive(sCanal, sRecu, sizeof(sRecu), &sRecv);
  if (err != ESP_OK) return echec("rmt_receive", err);
  return true;
}

void captureEnd() {
  sActif = false;
  if (!sCanal) return;
  rmt_disable(sCanal);  // arrete le canal et son interruption
  rmt_del_channel(sCanal);
  sCanal = nullptr;
}

bool captureActive() { return sActif && sCanal; }

size_t capturePoll(capt::EmetPartie emit, void *ctx, size_t maxParties) {
  size_t emises = 0;
  while (emises < maxParties) {
    Entete h{};
    portENTER_CRITICAL(&sMux);
    const bool vide = sEntLus == sEntEcrits;
    if (!vide) h = sEntetes[sEntLus % kEntetes];
    portEXIT_CRITICAL(&sMux);
    if (vide) break;
    // Parties que ce bloc peut donner ; au-dela de maxParties, il attend le tour suivant.
    const size_t besoin = h.n ? (2u * h.n + capt::kDurMax - 1) / capt::kDurMax : 1;
    if (emises && emises + besoin > maxParties) break;
    // Hors section critique : le rappel n'ecrit jamais dans un bloc pas encore lu.
    for (uint16_t i = 0; i < h.n; i++) {
      const rmt_symbol_word_t &s = sAnneau[(h.debut + i) % kTamponSymboles];
      sSym[i] = capt::Sym{(uint16_t)s.duration0, (uint8_t)s.level0, (uint16_t)s.duration1, (uint8_t)s.level1};
    }
    portENTER_CRITICAL(&sMux);
    sSymLus += h.n;
    sEntLus++;
    portEXIT_CRITICAL(&sMux);
    const capt::Bloc b{h.tFinUs, h.dernier, h.debordAvant, h.n, sSym};
    const size_t n = sDecoupeur.traiter(b, sReglages, emit, ctx);
    sParties += n;
    emises += n;
  }
  return emises;
}

CaptureStats captureStats() {
  CaptureStats s;
  portENTER_CRITICAL(&sMux);
  s.blocs = sBlocs;
  s.symboles = sSymboles;
  s.debord = sDebord;
  portEXIT_CRITICAL(&sMux);
  s.receptions = sDecoupeur.receptions();
  s.parties = sParties;
  return s;
}

const capt::Reglages &captureReglages() { return sReglages; }
