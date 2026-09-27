#pragma once
// ===========================================================================
//  Generateur du banc : segments de LIGNE -> symboles RMT. Pur, en-tete seul
//  (inclus par gen_main.cpp et par tools/tests/test_generateur.cpp).
//
//  Le NPN du banc inverse : GPIO haut = ligne basse. Un symbole RMT porte deux
//  demi-symboles (duree en ticks sur 15 bits, niveau GPIO). Une duree nulle
//  arrete l'emission : le nombre de demi-symboles doit donc etre pair ; s'il
//  est impair, le dernier est coupe en deux moities de meme niveau. Un segment
//  de plus de kTicksMax ticks est decoupe. Canal a 1 MHz : 1 tick = 1 us.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "motifs.h"

namespace gen {

constexpr uint16_t kTicksMax = 32767;       // duree d'un demi-symbole (15 bits)
constexpr size_t kSymMax = motif::kSegMax;  // symboles d'une trame, rafale exceptee
// Memoire du canal TX : deux blocs de 48 mots (SOC_RMT_MEM_WORDS_PER_CHANNEL),
// le second emprunte au canal 1, libre dans le generateur.
constexpr size_t kMemSymboles = 96;
// Rafale : kRafaleBoucle symboles {haut, bas} rejoues kRafaleTours fois par la
// boucle materielle du RMT. La boucle et son symbole de fin tiennent dans la
// memoire du canal, et kRafaleTours ne depasse pas le lot maximal du C6
// (RMT_LL_MAX_LOOP_COUNT_PER_BATCH = 1023) : une seule passe, sans relance
// par l'interruption du pilote, donc sans trou.
constexpr size_t kRafaleBoucle = 50;
constexpr uint32_t kRafaleTours = motif::kRafaleSegs / (2 * kRafaleBoucle);
constexpr uint32_t kToursParLot = 1023;
static_assert(kRafaleTours * 2 * kRafaleBoucle == motif::kRafaleSegs, "rafale : nombre de segments");
static_assert(kRafaleBoucle + 1 <= kMemSymboles && kRafaleTours <= kToursParLot, "rafale : boucle materielle");

// Miroir de rmt_symbol_word_t, niveaux GPIO.
struct Sym {
  uint16_t d0;
  uint8_t l0;
  uint16_t d1;
  uint8_t l1;
};

inline uint8_t niveauGpio(bool ligneHaute) { return ligneHaute ? 0 : 1; }

// Symboles des segments s[0..n) ; rend leur nombre, 0 si n == 0, si cap est
// trop petit ou si le dernier demi-symbole, impair, dure 1 tick.
inline size_t symboles(const motif::Seg *s, size_t n, Sym *out, size_t cap) {
  size_t k = 0;  // demi-symboles poses
  for (size_t i = 0; i < n; i++) {
    const uint8_t niv = niveauGpio(s[i].haut);
    uint32_t reste = s[i].us;
    while (reste) {
      const uint16_t d = reste > kTicksMax ? kTicksMax : (uint16_t)reste;
      if (k / 2 >= cap) return 0;
      Sym &y = out[k / 2];
      if (k % 2 == 0) {
        y = Sym{d, niv, 0, niv};
      } else {
        y.d1 = d;
        y.l1 = niv;
      }
      k++;
      reste -= d;
    }
  }
  if (!k) return 0;
  if (k % 2) {
    Sym &y = out[k / 2];
    if (y.d0 < 2) return 0;
    y.d1 = (uint16_t)(y.d0 - y.d0 / 2);
    y.d0 = (uint16_t)(y.d0 / 2);
    y.l1 = y.l0;
    k++;
  }
  return k / 2;
}

// Commande 'impulsions <bas_us> <periode_ms> [n]' (critere 5 du banc : une
// collision pendant une injection de la sonde) : une impulsion basse de
// bas_us, repetee toutes les periode_ms, ligne haute entre deux. Bornes :
// 10..30000 us, 60 s de periode au plus, et au moins 1 ms de ligne haute
// entre deux impulsions.
constexpr uint32_t kImpulsionMinUs = 10;
constexpr uint32_t kImpulsionMaxUs = 30000;
constexpr uint32_t kPeriodeMaxMs = 60000;
inline bool impulsionValide(uint32_t basUs, uint32_t periodeMs) {
  return basUs >= kImpulsionMinUs && basUs <= kImpulsionMaxUs && periodeMs <= kPeriodeMaxMs &&
         (uint64_t)periodeMs * 1000u >= (uint64_t)basUs + 1000u;
}

// Les kRafaleBoucle symboles de la boucle de la rafale : ligne haute puis basse, kRafaleUs chacune.
inline void rafale(Sym *out) {
  for (size_t i = 0; i < kRafaleBoucle; i++)
    out[i] = Sym{(uint16_t)motif::kRafaleUs, niveauGpio(true), (uint16_t)motif::kRafaleUs, niveauGpio(false)};
}

}  // namespace gen
