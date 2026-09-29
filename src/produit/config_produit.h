#pragma once
// ===========================================================================
//  Constantes du produit (docs/SPEC-PRODUIT.md 6.2, 7, 8) : broches de la
//  SuperMini violette, identite Matter, codes d'appairage, NVS, reseau. Pur :
//  inclus aussi par les tests hote (les static_assert y sont verifies).
// ===========================================================================
#include <stdint.h>

// --- Broches (6.2) ----------------------------------------------------------
// Ecoute et injection cote USB, loin de l'antenne ; aucune n'a de tirage au reset.
constexpr uint8_t kPinEcouteD = 0;     // ecoute de D (voie 1) : pilotes A, B, C (second plan)
constexpr uint8_t kPinInjectionD = 1;  // injection (voie 1) : a l'etat bas des la premiere instruction
constexpr uint8_t kPinAlimHotte = 2;   // ADC1 canal 2 : + de CN3, pont 100 k / 100 k
constexpr uint8_t kPinAlimModule = 3;  // ADC1 canal 3 : apres diode et fusible, au 470 uF
constexpr uint8_t kPinLedRgb = 8;      // WS2812 de la carte : mise au noir au demarrage (8.6)
constexpr uint32_t kPontRapport = 2;   // tension = 2 x tension a l'ADC (pont 100 k / 100 k)

// --- NVS et reseau -----------------------------------------------------------
constexpr const char *kNvsProduit = "produit";  // espace de noms du produit (4.4)
constexpr uint16_t kPortUdp = 5480;             // UDP sur Thread, comme la ScreenBar (7.2)

// --- Identite (8.1) ----------------------------------------------------------
#define MATTER_VENDOR_NAME "Djoko-CLI"
#define MATTER_PRODUCT_NAME "Module hotte Haier"
#define MATTER_NODE_LABEL "Hotte"
#define MATTER_SERIAL_PREFIX "HOTTE-"
#define MATTER_HW_VERSION 1
#ifndef PILOTE_LIGNE_TEXTE
#define PILOTE_LIGNE_TEXTE "simule"
#endif
#define MATTER_HW_VERSION_STRING "C6 SuperMini + pilote " PILOTE_LIGNE_TEXTE

// --- Codes d'appairage (8.2) -------------------------------------------------
// Tires au hasard le 29/09/2026 (module secrets de Python), distincts de ceux
// de la ScreenBar (0xF00, 20202021). Publics, comme les siens : ils ne servent
// que pendant la fenetre de mise en service. JAMAIS changes apres la premiere
// mise en service.
constexpr uint16_t kDiscriminateur = 0xFA1;
constexpr uint32_t kCodeAppairage = 45924130;

// Regle de la pile (chip::PayloadContents::IsValidSetupPIN, esp_matter 1.5.1,
// lue dans libespressif__esp_matter.a le 29/09) : 1..99999998, sauf 11111111
// a 88888888, 12345678 et 87654321.
constexpr bool codeAppairageValide(uint32_t c) {
  return c >= 1 && c <= 99999998 && c != 11111111 && c != 22222222 && c != 33333333 && c != 44444444 &&
         c != 55555555 && c != 66666666 && c != 77777777 && c != 88888888 && c != 12345678 && c != 87654321;
}
static_assert(codeAppairageValide(kCodeAppairage), "code d'appairage refuse par la pile");
static_assert(kCodeAppairage != 20202021, "code d'appairage de la ScreenBar");
static_assert(kDiscriminateur <= 0xFFF && kDiscriminateur != 0xF00, "discriminateur : 12 bits, pas celui de la ScreenBar");
