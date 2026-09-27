#pragma once
#include "reglages.h"
ReglagesSonde &sondeReglages();          // reglages en vigueur (main.cpp)
// Nouveaux reglages (deja bornes par l'appelant) : capture relancee (captureEnd
// + captureBegin) si elle tourne et que ses reglages changent, puis
// reglagesSauver ; jsonConfigChanged (tache 11). false : echec du RMT ou de la NVS.
bool sondeAppliquer(const ReglagesSonde &r);
