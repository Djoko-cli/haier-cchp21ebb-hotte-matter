// ===========================================================================
//  Garde-fous de l'injection : voir injection_regles.h. Pur (teste sur l'hote).
// ===========================================================================
#include "injection_regles.h"

#include <string.h>

// Sur la carte, les fonctions appelees par l'interruption des fronts
// (bord.cpp) sont en IRAM ; sur l'hote, IRAM_ATTR ne fait rien.
#if defined(__has_include)
#if __has_include(<esp_attr.h>)
#include <esp_attr.h>
#endif
#endif
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

namespace inj {

namespace {

// Bornes des parametres (enveloppe de securite de l'avenant). Ordre : celui de config.injection.
struct Champ {
  const char *nom;
  uint32_t Params::*m;
  uint32_t lo, hi;
};
const Champ kChamps[kNbParams] = {
    {"bas_max_us", &Params::basMaxUs, 10, 20000},
    {"total_max_us", &Params::totalMaxUs, 100, 200000},
    {"silence_min_us", &Params::silenceMinUs, 1000, 1000000},
    {"attente_max_ms", &Params::attenteMaxMs, 10, 10000},
    {"delai_min_ms", &Params::delaiMinMs, 1000, 60000},
    {"arme_max_s", &Params::armeMaxS, 10, 600},
    {"tol_us", &Params::tolUs, 1, 500},
};

const Champ *champ(const char *nom) {
  if (!nom) return nullptr;
  for (const Champ &c : kChamps)
    if (!strcmp(c.nom, nom)) return &c;
  return nullptr;
}

const char *espaces(const char *s) {
  while (*s == ' ') s++;
  return s;
}

}  // namespace

bool paramsValides(const Params &p) {
  for (const Champ &c : kChamps)
    if (p.*c.m < c.lo || p.*c.m > c.hi) return false;
  return p.basMaxUs <= p.totalMaxUs;
}

const char *nomParam(uint8_t i) { return i < kNbParams ? kChamps[i].nom : nullptr; }

bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi) {
  const Champ *c = champ(nom);
  if (!c) return false;
  *lo = c->lo;
  *hi = c->hi;
  return true;
}

Reglage reglerParam(Params *p, const char *nom, uint32_t v) {
  const Champ *c = champ(nom);
  if (!c) return Reglage::NomInconnu;
  Params q = *p;
  q.*c->m = v;
  if (!paramsValides(q)) return Reglage::HorsBornes;
  *p = q;
  return Reglage::Ok;
}

bool chargerParams(const void *octets, size_t n, Params *out) {
  Params p;
  if (octets && n == sizeof(Params)) memcpy(&p, octets, sizeof(Params));
  const bool ok = octets && n == sizeof(Params) && paramsValides(p);
  *out = ok ? p : Params();
  return ok;
}

const char *refusTexte(Refus r) {
  switch (r) {
    case Refus::Aucun: return "aucun";
    case Refus::NonMontee: return "non montee";
    case Refus::NonArmee: return "non armee";
    case Refus::Delai: return "delai minimal";
    case Refus::Syntaxe: return "syntaxe";
    case Refus::BasTropLong: return "duree basse trop longue";
    case Refus::TropLong: return "trame trop longue";
    case Refus::TropDeDurees: return "trop de durees";
  }
  return "?";
}

Refus analyser(const char *args, const Params &p, Demande *out) {
  const char *s = espaces(args ? args : "");
  if (strncmp(s, "durees", 6) || (s[6] && s[6] != ' ')) return Refus::Syntaxe;
  s += 6;
  Demande d;
  uint32_t n = 0;
  uint64_t somme = 0;
  bool basTropLong = false;
  for (s = espaces(s); *s; s = espaces(s)) {
    uint64_t v = 0;
    const char *debut = s;
    for (; *s >= '0' && *s <= '9'; s++) {
      v = v * 10 + (uint64_t)(*s - '0');
      if (v > 0xFFFFFFFFull) return Refus::Syntaxe;
    }
    if (s == debut || (*s && *s != ' ') || !v) return Refus::Syntaxe;
    if (n < kDurMax) d.dur[n] = (uint32_t)v;
    if (n % 2 == 0 && v > p.basMaxUs) basTropLong = true;
    somme += v;
    n++;
  }
  if (!n) return Refus::Syntaxe;
  if (n > kDurMax) return Refus::TropDeDurees;
  if (basTropLong) return Refus::BasTropLong;
  if (somme > p.totalMaxUs) return Refus::TropLong;
  d.n = (uint16_t)n;
  *out = d;
  return Refus::Aucun;
}

uint32_t totalUs(const Demande &d) {
  uint64_t s = 0;
  for (uint16_t i = 0; i < d.n && i < kDurMax; i++) s += d.dur[i];
  return s > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)s;
}

void Etat::armer(uint32_t nowMs) {
  arme_ = true;
  armeAt_ = nowMs;
}

bool Etat::armee(uint32_t nowMs, const Params &p) const {
  return arme_ && (uint64_t)(uint32_t)(nowMs - armeAt_) < (uint64_t)p.armeMaxS * 1000u;
}

uint32_t Etat::resteS(uint32_t nowMs, const Params &p) const {
  if (!armee(nowMs, p)) return 0;
  const uint64_t reste = (uint64_t)p.armeMaxS * 1000u - (uint32_t)(nowMs - armeAt_);
  return (uint32_t)((reste + 999) / 1000);
}

Refus Etat::admettre(uint32_t nowMs, const Params &p) const {
  if (!montee_) return Refus::NonMontee;
  if (!armee(nowMs, p)) return Refus::NonArmee;
  if (aEmis_ && (uint32_t)(nowMs - emisAt_) < p.delaiMinMs) return Refus::Delai;
  return Refus::Aucun;
}

void Etat::noterEmission(uint32_t nowMs) {
  aEmis_ = true;
  emisAt_ = nowMs;
}

bool IRAM_ATTR niveauAttendu(const Demande &d, uint32_t tRelUs) {
  uint32_t fin = 0;
  for (uint16_t i = 0; i < d.n && i < kDurMax; i++) {
    fin += d.dur[i];
    if (tRelUs < fin) return i % 2 == 1;  // rang pair : bus tire bas
  }
  return true;  // apres la fin : bus relache
}

bool IRAM_ATTR frontProgramme(const Demande &d, uint32_t tRelUs, uint32_t tolUs) {
  // Fronts programmes : 0, puis la fin de chaque duree (la derniere comprise).
  uint32_t front = 0;
  for (uint16_t i = 0; i <= d.n && i <= kDurMax; i++) {
    const uint32_t ecart = tRelUs > front ? tRelUs - front : front - tRelUs;
    if (ecart <= tolUs) return true;
    if (i < d.n && i < kDurMax) front += d.dur[i];
  }
  return false;
}

bool IRAM_ATTR frontAnormal(const Demande &d, uint32_t tRelUs, bool niv, uint32_t tolUs) {
  return !frontProgramme(d, tRelUs, tolUs) && niv != niveauAttendu(d, tRelUs);
}

size_t versSymboles(const Demande &d, Sym *out, size_t cap) {
  size_t k = 0;  // demi-symboles poses
  for (uint16_t i = 0; i < d.n && i < kDurMax; i++) {
    const uint8_t niv = i % 2 == 0 ? 1 : 0;  // GPIO7 haut = bus bas
    for (uint32_t reste = d.dur[i]; reste;) {
      const uint16_t t = reste > 32767 ? 32767 : (uint16_t)reste;
      if (k / 2 >= cap) return 0;
      Sym &y = out[k / 2];
      if (k % 2 == 0) {
        y = Sym{t, niv, 0, 0};
      } else {
        y.d1 = t;
        y.l1 = niv;
      }
      k++;
      reste -= t;
    }
  }
  if (!k) return 0;
  if (k % 2) {
    out[k / 2].d1 = 1;  // 1 us bus relache : une duree nulle arreterait l'emission trop tot
    out[k / 2].l1 = 0;
    k++;
  }
  return k / 2;
}

void Surveillance::debut(const Demande *d, uint64_t t0Us, uint32_t tolUs) {
  d_ = d;
  t0_ = prec_ = t0Us;
  tol_ = tolUs;
  finRel_ = totalUs(*d) + tolUs;
  n_ = 0;
  ancre_ = coll_ = false;
  actif_ = true;
}

void IRAM_ATTR Surveillance::front(uint64_t tUs, bool busHaut) {
  if (!actif_ || coll_) return;
  if (!ancre_) {
    if (!busHaut && tUs >= t0_ && tUs - t0_ <= kDepartMaxUs) {
      ancre_ = true;
      t0_ = prec_ = tUs;
    } else {
      coll_ = true;  // premier front inattendu : un autre emetteur
    }
    return;
  }
  if (tUs < t0_ || tUs - t0_ > finRel_) return;  // apres la fenetre : trafic d'un autre
  if (n_ < kDurMax) relu_[n_++] = (uint32_t)(tUs - prec_);
  prec_ = tUs;
  const uint32_t rel = (uint32_t)(tUs - t0_);
  // Vers un autre niveau que l'emis (frontAnormal), ou vers le niveau emis
  // loin de tout front programme : le front precedent a ete masque.
  if (frontAnormal(*d_, rel, busHaut, tol_) || !frontProgramme(*d_, rel, tol_)) coll_ = true;
}

uint16_t Surveillance::relu(uint32_t *out, uint16_t cap) const {
  const uint16_t n = n_ < cap ? n_ : cap;
  for (uint16_t i = 0; i < n; i++) out[i] = relu_[i];
  return n;
}

}  // namespace inj
