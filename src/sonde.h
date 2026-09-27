#pragma once
#include "reglages.h"
ReglagesSonde &sondeReglages();          // reglages en vigueur (main.cpp)
// Nouveaux reglages (deja bornes par l'appelant) : capture relancee (captureEnd
// + captureBegin) si elle tourne et que ses reglages changent, puis
// reglagesSauver et jsonConfigChanged. false : echec du RMT ou de la NVS.
bool sondeAppliquer(const ReglagesSonde &r);
// Mode changements (etat.capture.rep_en_cours, compteurs.sonde.rep) :
// receptions identiques non emises depuis la derniere emise, et depuis le demarrage.
uint32_t sondeRepEnCours();
uint32_t sondeRepTotal();
// Valeurs hors bornes lues en NVS au demarrage, remplacees par leur valeur par
// defaut (reglagesCharger) : annoncees a chaque ouverture de session (spec 8.2).
uint8_t sondeHorsBornes();
