// ===========================================================================
//  Correspondance Matter et boite d'intentions : voir hotte_map.h. Pur.
// ===========================================================================
#include "hotte_map.h"

namespace hotte {

Moteur palier(uint8_t p) {
  if (p == 0) return Moteur::Arret;
  if (p <= 33) return Moteur::V1;
  if (p <= 66) return Moteur::V2;
  return Moteur::V3;
}

uint8_t pourcent(Moteur m) {
  switch (m) {
    case Moteur::V1: return 33;
    case Moteur::V2: return 66;
    case Moteur::V3: return 100;
    default: return 0;
  }
}

uint8_t fanMode(Moteur m) {
  switch (m) {
    case Moteur::V1: return kFanLow;
    case Moteur::V2: return kFanMedium;
    case Moteur::V3: return kFanHigh;
    default: return kFanOff;
  }
}

CibleVentilo cibleDe(Moteur m) {
  switch (m) {
    case Moteur::V1: return CibleVentilo::V1;
    case Moteur::V2: return CibleVentilo::V2;
    case Moteur::V3: return CibleVentilo::V3;
    default: return CibleVentilo::Eteint;
  }
}

CibleVentilo cibleDeMode(uint8_t m) {
  switch (m) {
    case kFanOff: return CibleVentilo::Eteint;
    case kFanLow: return CibleVentilo::V1;
    case kFanMedium: return CibleVentilo::V2;
    case kFanHigh: return CibleVentilo::V3;
    case kFanOn: return CibleVentilo::Derniere;
    default: return CibleVentilo::Aucune;  // Auto, Smart : hors de la sequence OffLowMedHigh
  }
}

Reflet reflet(const Etat &e) {
  Reflet r;
  r.fanMode = fanMode(e.moteur);
  r.pourcent = pourcent(e.moteur);
  r.lampe = e.lampeConnue && e.lampe;
  return r;
}

// ---------------------------------------------------------------------------

void RegroupementEp1::noter(uint32_t nowMs) {
  if (!ouvert_) {
    ouvert_ = true;
    premierMs_ = nowMs;
  }
  dernierMs_ = nowMs;
}

void RegroupementEp1::ecrireMode(uint8_t m, uint32_t nowMs) {
  noter(nowMs);
  aMode_ = true;
  mode_ = m;
  seqMode_ = ++seq_;
}

void RegroupementEp1::ecrirePourcent(uint8_t p, uint32_t nowMs) {
  noter(nowMs);
  aPourcent_ = true;
  pourcent_ = p;
  seqPourcent_ = ++seq_;
}

bool RegroupementEp1::pret(uint32_t nowMs, uint32_t calmeMs, uint32_t plafondMs, CibleVentilo *cible,
                           uint32_t *premierMs) {
  if (!ouvert_) return false;
  // Differences signees : une ecriture deposee apres le millis() de l'appelant.
  if ((int32_t)(nowMs - dernierMs_) < (int32_t)calmeMs && (int32_t)(nowMs - premierMs_) < (int32_t)plafondMs)
    return false;
  CibleVentilo c = CibleVentilo::Aucune;
  if (aPourcent_ && (!aMode_ || seqPourcent_ > seqMode_)) {  // derniere ecriture : PercentSetting
    c = pourcent_ == 0 ? CibleVentilo::Eteint : cibleDe(palier(pourcent_));
  } else if (mode_ == kFanOff) {
    c = CibleVentilo::Eteint;
  } else if (cibleDeMode(mode_) != CibleVentilo::Aucune) {  // Low, Medium, High, On
    c = aPourcent_ && pourcent_ > 0 ? cibleDe(palier(pourcent_)) : cibleDeMode(mode_);
  }
  *cible = c;
  if (premierMs) *premierMs = premierMs_;
  *this = RegroupementEp1();
  return true;
}

void RegroupementEp2::ecrire(bool allumee, uint32_t nowMs) {
  if (!ouvert_) {
    ouvert_ = true;
    premierMs_ = nowMs;
  }
  dernierMs_ = nowMs;
  valeur_ = allumee;
}

bool RegroupementEp2::pret(uint32_t nowMs, uint32_t calmeMs, uint32_t plafondMs, bool *allumee, uint32_t *premierMs) {
  if (!ouvert_) return false;
  if ((int32_t)(nowMs - dernierMs_) < (int32_t)calmeMs && (int32_t)(nowMs - premierMs_) < (int32_t)plafondMs)
    return false;
  *allumee = valeur_;
  if (premierMs) *premierMs = premierMs_;
  *this = RegroupementEp2();
  return true;
}

bool ignoreeAuDemarrage(uint32_t premierMs, uint32_t demarrageMs, uint32_t ignoreMs) {
  return (int32_t)(premierMs - demarrageMs) < (int32_t)ignoreMs;
}

// ---------------------------------------------------------------------------

void EcritureDifferee::ecrite(uint32_t valeur) {
  connue_ = true;
  ecrite_ = valeur;
  if (attente_ && valeur_ == valeur) attente_ = false;
}

void EcritureDifferee::noter(uint32_t valeur, uint32_t nowMs) {
  if (attente_ && valeur == valeur_) return;  // deja en attente : le delai court depuis le changement
  valeur_ = valeur;
  depuisMs_ = nowMs;
  attente_ = !connue_ || valeur != ecrite_;
}

// ---------------------------------------------------------------------------

namespace {
constexpr uint16_t kMarque = 0xA500, kMasqueMarque = 0xFF00;
}

uint16_t versNvs(const Etat &e) {
  return (uint16_t)(kMarque | (uint16_t)e.marche | (uint16_t)e.moteur << 2 | (uint16_t)(e.lampe ? 1 : 0) << 4 |
                    (uint16_t)(e.lampeConnue ? 1 : 0) << 5);
}

bool depuisNvs(uint16_t v, Etat *out) {
  if ((v & kMasqueMarque) != kMarque || (v & 0xC0)) return false;
  Etat e;
  e.marche = (Marche)(v & 3);
  e.moteur = (Moteur)(v >> 2 & 3);
  e.lampe = v >> 4 & 1;
  e.lampeConnue = v >> 5 & 1;
  *out = e;
  return true;
}

}  // namespace hotte
