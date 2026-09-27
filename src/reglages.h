#pragma once
#include "capture_model.h"
#include "injection_regles.h"

struct ReglagesSonde {
  capt::Reglages capture;
  bool changements = false;   // mode d'emission
  bool injMontee = false;     // etage d'injection declare monte ('injection monte 0|1', cle inj_monte)
  inj::Params injection;      // valeurs de l'avenant ('injection regle', cle inj_params)
};
// Lit l'espace NVS kNvsEspace ; applique capt::borner et inj::chargerParams ;
// rend le nombre de valeurs remplacees (un jeu inj_params refuse compte pour une).
uint8_t reglagesCharger(ReglagesSonde *r);
bool reglagesSauver(const ReglagesSonde &r);
