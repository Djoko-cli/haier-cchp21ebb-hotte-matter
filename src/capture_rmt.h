#pragma once
#include "capture_model.h"

bool captureBegin(const capt::Reglages &r);  // GPIO kPinEcoute ; false : echec IDF (log)
void captureEnd();
bool captureActive();
// Tache loop : vide le tampon circulaire, au plus maxParties parties par appel.
size_t capturePoll(capt::EmetPartie emit, void *ctx, size_t maxParties);
struct CaptureStats {
  uint32_t receptions, parties, blocs, symboles, debord;
};
CaptureStats captureStats();
const capt::Reglages &captureReglages();
