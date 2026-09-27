#pragma once
// ===========================================================================
//  Constantes de la sonde : broches du C6 SuperMini, valeurs par defaut et
//  bornes de la capture, noms de la NVS et du reseau (docs/SPEC-RECONNAISSANCE.md
//  sections 7.5, 8.2 et 8.5). Pur : inclus aussi par les tests hote.
// ===========================================================================
#include <stdint.h>

constexpr uint8_t kPinEcoute = 6;      // sortie de l'etage d'ecoute (voie 1)
constexpr uint8_t kPinInjection = 7;   // base de l'etage d'injection (voie 1)
constexpr uint8_t kPinGenerateur = 7;  // sortie du generateur (second C6)

constexpr uint32_t kResolDefautHz = 1000000;
constexpr uint32_t kResolLenteHz = 500000;
constexpr uint8_t kFiltreDefautUs = 1;
constexpr uint8_t kFiltreMaxUs = 3;
constexpr uint32_t kSilenceDefautUs = 5000;
constexpr uint32_t kSilenceMinUs = 1000;
constexpr uint32_t kSilenceMaxTicks = 32767;
constexpr uint32_t kTamponSymboles = 2048;
constexpr uint16_t kTramesParSeconde = 100;

constexpr const char *kNvsEspace = "hotte";
constexpr const char *kNomMdns = "hotte-sonde";
constexpr uint16_t kPortUdp = 5480;
