#pragma once
#include <stdint.h>

#include "injection_regles.h"

bool bordBegin(uint8_t pin);   // service ISR d'IDF (ESP_ERR_INVALID_STATE tolere), deux fronts
uint64_t bordDernierUs();      // esp_timer du dernier front vu
bool bordBusHaut();            // niveau du bus maintenant (inversion de l'etage comprise)
uint32_t bordFronts();         // fronts vus depuis le demarrage

// Tache 23 : surveillance d'une injection. L'interruption passe chaque front,
// avec le niveau du bus lu sur la broche, a une inj::Surveillance
// (injection_regles.h) : origine au premier front vers le bas qui suit t0Us,
// collision sur un front anormal, durees relues. d doit rester valide jusqu'a
// bordFinSurveillance().
void bordSurveiller(const inj::Demande *d, uint64_t t0Us, uint32_t tolUs);
void bordFinSurveillance();
bool bordCollision();                             // front anormal vu depuis bordSurveiller
uint16_t bordRelu(uint32_t *out, uint16_t cap);   // durees relues ; rend leur nombre
bool bordActif();                                 // ajout : interruption installee (bordBegin reussi)
