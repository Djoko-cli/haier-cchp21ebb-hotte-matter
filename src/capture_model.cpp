// ===========================================================================
//  Modele pur de la capture (docs/SPEC-RECONNAISSANCE.md section 8.2) : bornes
//  des reglages. Sans Arduino ni IDF : teste sur l'hote (tools/tests/test_capture.cpp).
// ===========================================================================
#include "capture_model.h"

#include "config.h"

namespace capt {

// ===========================================================================
//  Bornes
// ===========================================================================

bool resolValide(uint32_t hz) { return hz == kResolDefautHz || hz == kResolLenteHz; }

// Filtre du RMT : 255 cycles de 80 MHz au plus, soit 3,19 us.
bool filtreValide(uint32_t us) { return us <= kFiltreMaxUs; }

// Seuil de silence du RMT : kSilenceMaxTicks ticks au plus (registre sur 15 bits).
bool silenceValide(uint32_t us, uint32_t resolHz) {
  return us >= kSilenceMinUs && (uint64_t)us * resolHz <= (uint64_t)kSilenceMaxTicks * 1000000u;
}

uint8_t borner(Reglages *r) {
  const Reglages d;
  uint8_t n = 0;
  // La resolution d'abord : la borne du silence en depend.
  if (!resolValide(r->resolHz)) {
    r->resolHz = d.resolHz;
    n++;
  }
  if (!filtreValide(r->filtreUs)) {
    r->filtreUs = d.filtreUs;
    n++;
  }
  if (!silenceValide(r->silenceUs, r->resolHz)) {
    r->silenceUs = d.silenceUs;
    n++;
  }
  return n;
}

}  // namespace capt
