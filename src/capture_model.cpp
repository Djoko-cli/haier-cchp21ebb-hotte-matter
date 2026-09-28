// ===========================================================================
//  Modele pur de la capture (docs/SPEC-RECONNAISSANCE.md section 8.2) : bornes
//  des reglages, symboles RMT -> parties publiees (lignes trame), mode
//  changements. Sans Arduino ni IDF : teste sur l'hote (tools/tests/test_capture.cpp).
// ===========================================================================
#include "capture_model.h"

#include "config.h"

namespace capt {

// ===========================================================================
//  Bornes
// ===========================================================================

bool resolValide(uint32_t hz) { return hz == kResolDefautHz || hz == kResolLenteHz; }

// Filtre du RMT : 255 cycles de 80 MHz au plus, soit 3,19 us.
bool filtreValide(uint32_t us) { return us <= kFiltreMaxUs; }

// Seuil de silence du RMT : kSilenceMaxTicks ticks au plus (registre sur 15 bits).
bool silenceValide(uint32_t us, uint32_t resolHz) {
  return us >= kSilenceMinUs && (uint64_t)us * resolHz <= (uint64_t)kSilenceMaxTicks * 1000000u;
}

uint8_t borner(Reglages *r) {
  const Reglages d;
  uint8_t n = 0;
  // La resolution d'abord : la borne du silence en depend.
  if (!resolValide(r->resolHz)) {
    r->resolHz = d.resolHz;
    n++;
  }
  if (!filtreValide(r->filtreUs)) {
    r->filtreUs = d.filtreUs;
    n++;
  }
  if (!silenceValide(r->silenceUs, r->resolHz)) {
    r->silenceUs = d.silenceUs;
    n++;
  }
  return n;
}

// ===========================================================================
//  Decoupeur
//
//  Une reception va du premier front au silence (bloc dernier). Le pilote la
//  rend en un bloc, ou en plusieurs si elle depasse son tampon (blocs non
//  derniers) : ses parties se suivent (part 0, 1, ...), la derniere porte fin.
//  Un bloc marque debordAvant ouvre une nouvelle reception : la precedente
//  reste sans fin, la continuite est perdue.
//
//  reset() clot la reception en cours sans rien emettre ; la numerotation
//  continue (num : depuis le demarrage).
//
//  Dans le bloc dernier, la premiere duree nulle est le marqueur de fin du
//  RMT : rien ne compte apres (demiSymboles). Le pilote alterne les niveaux
//  d'un demi-symbole au suivant ; si une duree nulle retiree ailleurs en
//  laissait deux voisines de meme niveau, elles seraient fondues, pour que les
//  niveaux d'une partie restent alternes (niv0Haut dit celui de dur[0]).
// ===========================================================================

void Decoupeur::reset() {
  part_ = 0;
  enCours_ = false;
}

// Ticks -> us : exact a 1 MHz (x1) et a 500 kHz (x2).
static uint32_t versUs(uint16_t ticks, uint32_t hz) {
  return hz ? (uint32_t)((uint64_t)ticks * 1000000u / hz) : ticks;
}

static uint16_t demi(const Sym &s, uint32_t k) { return k & 1 ? s.d1 : s.d0; }

// Demi-symboles du bloc a lire : dans le bloc dernier, jusqu'au marqueur de
// fin. Quand il ouvre un mot (nombre pair de durees), le C6 remplit la seconde
// moitie de ce mot avec la derniere duree recue, niveau compris : lue, elle
// serait fondue avec l'originale (banc 0 du 28/09 : 2000 us lus 4000).
static uint32_t demiSymboles(const Bloc &b) {
  const uint32_t n = 2u * b.nSym;
  if (b.dernier)
    for (uint32_t k = 0; k < n; k++)
      if (!demi(b.sym[k / 2], k)) return k;
  return n;
}

size_t Decoupeur::traiter(const Bloc &b, const Reglages &r, EmetPartie emit, void *ctx) {
  if (b.debordAvant) enCours_ = false;  // continuite perdue : nouvelle reception
  const uint32_t nDemi = demiSymboles(b);
  uint64_t total = 0;
  for (uint32_t k = 0; k < nDemi; k++) total += versUs(demi(b.sym[k / 2], k), r.resolHz);
  // Rien a publier, sauf pour clore une reception en cours ou signaler une perte.
  if (!total && !b.debordAvant && !(b.dernier && enCours_)) {
    if (b.dernier) enCours_ = false;
    return 0;
  }
  if (!enCours_) {
    num_++;
    part_ = 0;
    enCours_ = true;
  }
  const uint64_t retrait = total + (b.dernier ? r.silenceUs : 0);
  Partie p;
  p.num = num_;
  p.part = part_;
  p.tUs = b.tFinUs > retrait ? b.tFinUs - retrait : 0;
  p.debord = b.debordAvant;
  uint64_t dansPartie = 0;  // somme des durees de p
  bool dernierHaut = false;
  size_t emises = 0;
  for (uint32_t k = 0; k < nDemi; k++) {
    const Sym &s = b.sym[k / 2];
    const uint32_t us = versUs(demi(s, k), r.resolHz);
    if (!us) continue;
    const bool gpioHaut = (k & 1 ? s.l1 : s.l0) != 0;
    const bool haut = r.inverse ? !gpioHaut : gpioHaut;
    if (p.n && haut == dernierHaut) {  // voisine de meme niveau : fondue
      p.dur[p.n - 1] += us;
      dansPartie += us;
      continue;
    }
    if (p.n == kDurMax) {  // partie pleine : elle part, la suivante commence a sa fin
      emit(p, ctx);
      emises++;
      part_++;
      p.part = part_;
      p.tUs += dansPartie;
      p.debord = false;
      p.n = 0;
      dansPartie = 0;
    }
    if (!p.n) p.niv0Haut = haut;
    p.dur[p.n++] = us;
    dansPartie += us;
    dernierHaut = haut;
  }
  p.fin = b.dernier;
  emit(p, ctx);
  emises++;
  part_++;
  if (b.dernier) enCours_ = false;
  return emises;
}

// ===========================================================================
//  Mode changements
// ===========================================================================

bool procheDe(uint32_t a, uint32_t b) {
  const uint32_t haut = a > b ? a : b, ecart = a > b ? a - b : b - a;
  return ecart <= 10 || (uint64_t)ecart * 20 <= haut;  // 5 % = 1/20
}

void Changements::reset() {
  a_ = false;
  n_ = 0;
  rep_ = 0;
}

bool Changements::aEmettre(const Partie &p) {
  // Comparable : une reception d'une seule partie. Une perte (debord) part
  // toujours, pour que la marque arrive.
  const bool comparable = p.part == 0 && p.fin;
  if (comparable && !p.debord && a_ && p.niv0Haut == niv0_ && p.n == n_) {
    bool pareil = true;
    for (uint16_t i = 0; i < n_ && pareil; i++) pareil = procheDe(p.dur[i], dur_[i]);
    if (pareil) {
      rep_++;
      return false;
    }
  }
  // Emise : elle devient la derniere reception emise (sans reference si elle
  // n'est pas comparable : la suivante partira).
  a_ = comparable;
  if (comparable) {
    niv0_ = p.niv0Haut;
    n_ = p.n;
    for (uint16_t i = 0; i < n_; i++) dur_[i] = p.dur[i];
  }
  return true;
}

uint32_t Changements::prendreRep() {
  const uint32_t r = rep_;
  rep_ = 0;
  return r;
}

}  // namespace capt
