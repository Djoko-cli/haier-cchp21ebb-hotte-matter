// ===========================================================================
//  Sonde de reconnaissance de la ligne D (docs/SPEC-RECONNAISSANCE.md)
// ===========================================================================
#include <Arduino.h>

#include "config.h"
#include "fw_version.h"
#include "reglages.h"

static ReglagesSonde sReglages;

void setup() {
  // TOUJOURS la premiere instruction (spec 8.4) : la base de l'etage
  // d'injection a l'etat bas avant tout le reste.
  pinMode(kPinInjection, OUTPUT);
  digitalWrite(kPinInjection, LOW);
  Serial.begin(115200);
  const uint8_t remplaces = reglagesCharger(&sReglages);
  Serial.printf("firmware %s (%s)\n", FW_VERSION_FULL, FW_ENV);
  if (remplaces) Serial.printf("[reglages] %u valeur(s) hors bornes en NVS : valeur(s) par defaut\n", remplaces);
}

void loop() {
  vTaskDelay(1);  // laisse tourner la tache IDLE
}
