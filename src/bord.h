#pragma once
#include <stdint.h>

bool bordBegin(uint8_t pin);   // service ISR d'IDF (ESP_ERR_INVALID_STATE tolere), deux fronts
uint64_t bordDernierUs();      // esp_timer du dernier front vu
bool bordBusHaut();            // niveau du bus maintenant (inversion de l'etage comprise)
uint32_t bordFronts();         // fronts vus depuis le demarrage
