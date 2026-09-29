#pragma once
// ===========================================================================
//  Mesures du module (docs/SPEC-PRODUIT.md 6.5, 7.4)
//
//  - Tensions des deux voies : ADC1 en mode continu (DMA), GPIO2 (+ de CN3)
//    et GPIO3 (apres diode et fusible), environ 1 kHz chacune ; le minimum de
//    chaque seconde et la derniere valeur vont a la surveillance. Un creux
//    plus court que la periode d'echantillonnage peut echapper (6.5).
//  - Temperature de la puce : temperatureRead() toutes les 10 s.
//
//  Au banc, sans pont, GPIO2 et GPIO3 sont en l'air : les tensions lues ne
//  veulent rien dire, et les alertes de tension sont coupees (alim_alertes 0).
//  Tache loop seulement.
// ===========================================================================
#include <Arduino.h>

#include "surveillance.h"

bool alimBegin();  // setup() : ADC continu demarre ; false : ADC indisponible (temperature seule)
void alimPoll(surv::Surveillance &s, uint32_t now);
// Banc B15 (Q42) : l'ADC continu s'arrete, la temperature se lit seule ; puis reprise.
void alimArreter();
bool alimReprendre();
void alimStatut(Print &out, const surv::Surveillance &s);
