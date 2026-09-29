// ===========================================================================
//  Mesures du module : voir alim.h.
// ===========================================================================
#include "alim.h"

#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_continuous.h>
#include <math.h>

#include "config_produit.h"

namespace {

constexpr uint32_t kFreqHz = 2000;          // les deux voies se partagent l'echantillonneur
constexpr uint32_t kTrameOctets = 256;      // 64 resultats (4 octets chacun sur le C6)
constexpr uint32_t kReserveOctets = 1024;   // 4 trames d'avance
constexpr uint32_t kTemperatureMs = 10000;  // 7.4
constexpr uint8_t kLecturesParTour = 4;     // au plus 4 x 64 resultats par tour de loop()

adc_continuous_handle_t sAdc = nullptr;
adc_cali_handle_t sCali[2] = {nullptr, nullptr};
adc_channel_t sCanal[2] = {};
bool sEnService = false;

struct Voie {
  bool vu = false;        // un echantillon dans la seconde en cours
  uint32_t minBrut = 0;   // minimum brut de la seconde
  uint32_t dernierBrut = 0;
};
Voie sVoies[2];
uint32_t sSecondeMs = 0, sTemperatureMs = 0;
bool sTemperatureLue = false;
uint32_t sEchantillons = 0, sErreurs = 0, sSecondes = 0;
float sDerniereTemperature = NAN;

uint32_t versMv(uint8_t v, uint32_t brut) {
  int mv = 0;
  if (!sCali[v] || adc_cali_raw_to_voltage(sCali[v], (int)brut, &mv) != ESP_OK) return 0;
  return (uint32_t)mv * kPontRapport;  // pont 100 k / 100 k
}

}  // namespace

bool alimBegin() {
  adc_unit_t u;
  if (adc_continuous_io_to_channel(kPinAlimHotte, &u, &sCanal[0]) != ESP_OK ||
      adc_continuous_io_to_channel(kPinAlimModule, &u, &sCanal[1]) != ESP_OK)
    return false;
  adc_continuous_handle_cfg_t cfg = {};
  cfg.max_store_buf_size = kReserveOctets;
  cfg.conv_frame_size = kTrameOctets;
  if (adc_continuous_new_handle(&cfg, &sAdc) != ESP_OK) return false;
  adc_digi_pattern_config_t motif[2] = {};
  for (uint8_t v = 0; v < 2; v++) {
    motif[v].atten = ADC_ATTEN_DB_12;  // 0 a ~3,1 V a l'ADC : 6,2 V au + de CN3
    motif[v].channel = (uint8_t)sCanal[v];
    motif[v].unit = ADC_UNIT_1;
    motif[v].bit_width = ADC_BITWIDTH_12;
    adc_cali_curve_fitting_config_t cc = {};
    cc.unit_id = ADC_UNIT_1;
    cc.chan = sCanal[v];
    cc.atten = ADC_ATTEN_DB_12;
    cc.bitwidth = ADC_BITWIDTH_12;
    if (adc_cali_create_scheme_curve_fitting(&cc, &sCali[v]) != ESP_OK) sCali[v] = nullptr;
  }
  adc_continuous_config_t conf = {};
  conf.pattern_num = 2;
  conf.adc_pattern = motif;
  conf.sample_freq_hz = kFreqHz;
  conf.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  conf.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
  if (adc_continuous_config(sAdc, &conf) != ESP_OK) return false;
  sEnService = adc_continuous_start(sAdc) == ESP_OK;
  return sEnService;
}

void alimArreter() {
  if (sAdc && sEnService) adc_continuous_stop(sAdc);
  sEnService = false;
}

bool alimReprendre() {
  if (!sAdc) return false;
  if (!sEnService) sEnService = adc_continuous_start(sAdc) == ESP_OK;
  return sEnService;
}

void alimPoll(surv::Surveillance &s, uint32_t now) {
  if (sEnService) {
    adc_continuous_data_t lu[kTrameOctets / SOC_ADC_DIGI_RESULT_BYTES];
    for (uint8_t k = 0; k < kLecturesParTour; k++) {
      uint32_t n = 0;
      const esp_err_t e = adc_continuous_read_parse(sAdc, lu, sizeof(lu) / sizeof(lu[0]), &n, 0);
      if (e == ESP_ERR_TIMEOUT || !n) break;  // plus rien d'echantillonne
      if (e != ESP_OK) {
        sErreurs++;
        break;
      }
      for (uint32_t i = 0; i < n; i++) {
        if (!lu[i].valid) continue;
        const uint8_t v = lu[i].channel == sCanal[0] ? 0 : lu[i].channel == sCanal[1] ? 1 : 2;
        if (v > 1) continue;
        Voie &x = sVoies[v];
        if (!x.vu || lu[i].raw_data < x.minBrut) x.minBrut = lu[i].raw_data;
        x.dernierBrut = lu[i].raw_data;
        x.vu = true;
        sEchantillons++;
      }
    }
  }
  if (now - sSecondeMs >= 1000) {
    sSecondeMs = now;
    if (sVoies[0].vu && sVoies[1].vu) {
      s.tensions(versMv(0, sVoies[0].dernierBrut), versMv(0, sVoies[0].minBrut), versMv(1, sVoies[1].dernierBrut),
                 versMv(1, sVoies[1].minBrut));
      sSecondes++;
    }
    sVoies[0].vu = sVoies[1].vu = false;
  }
  if (!sTemperatureLue || now - sTemperatureMs >= kTemperatureMs) {
    sTemperatureLue = true;
    sTemperatureMs = now;
    const float t = temperatureRead();  // NAN si hors de -40..125 C (IDF change seul de plage)
    sDerniereTemperature = t;
    s.temperature(!isnan(t), isnan(t) ? 0 : (int32_t)lroundf(t * 10.0f), now);
  }
}

void alimStatut(Print &out, const surv::Surveillance &s) {
  out.printf("alim : ADC %s, %lu echantillons, %lu secondes, %lu erreurs\n", sEnService ? "en service" : "arrete",
             (unsigned long)sEchantillons, (unsigned long)sSecondes, (unsigned long)sErreurs);
  const surv::Voie &h = s.hotte(), &m = s.module();
  if (h.connue)
    out.printf("  hotte  (GPIO%u) : %lu mV, min seconde %lu, min demarrage %lu, passages %lu%s\n",
               (unsigned)kPinAlimHotte, (unsigned long)h.actuelMv, (unsigned long)h.minSecondeMv,
               (unsigned long)h.minDemarrageMv, (unsigned long)h.passages, h.alerte ? ", ALERTE" : "");
  if (m.connue)
    out.printf("  module (GPIO%u) : %lu mV, min seconde %lu, min demarrage %lu, passages %lu%s\n",
               (unsigned)kPinAlimModule, (unsigned long)m.actuelMv, (unsigned long)m.minSecondeMv,
               (unsigned long)m.minDemarrageMv, (unsigned long)m.passages, m.alerte ? ", ALERTE" : "");
  out.printf("  alertes de tension %s ; puce %.1f C (max %.1f, depuis la pose %.1f), seuil %lu C%s\n",
             s.params().alimAlertes ? "en service" : "coupees (pont non monte)", (double)sDerniereTemperature,
             s.tempMaxDixiemes() / 10.0, s.maxPose() / 10.0, (unsigned long)s.params().tempAlerteC,
             s.alerteTemperature() ? ", ALERTE" : "");
}
