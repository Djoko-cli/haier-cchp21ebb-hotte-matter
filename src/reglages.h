#pragma once
#include "capture_model.h"

struct ReglagesSonde {
  capt::Reglages capture;
  bool changements = false;   // mode d'emission
};
// Lit l'espace NVS kNvsEspace ; applique capt::borner ; rend le nombre de valeurs remplacees.
uint8_t reglagesCharger(ReglagesSonde *r);
bool reglagesSauver(const ReglagesSonde &r);
