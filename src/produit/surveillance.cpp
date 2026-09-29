// ===========================================================================
//  Surveillance : voir surveillance.h. Pur (teste sur l'hote).
// ===========================================================================
#include "surveillance.h"

#include <string.h>

namespace surv {

namespace {

struct Champ {
  const char *nom;
  uint32_t Params::*m;
  uint32_t lo, hi;
};
const Champ kChamps[kNbParams] = {
    {"temp_alerte_c", &Params::tempAlerteC, 20, 85},
    {"temp_hyst_c", &Params::tempHystC, 1, 10},
    {"alim_hotte_min_mv", &Params::alimHotteMinMv, 3000, 6000},
    {"alim_module_min_mv", &Params::alimModuleMinMv, 3000, 5000},
    {"alim_hyst_mv", &Params::alimHystMv, 20, 500},
    {"alim_alertes", &Params::alimAlertes, 0, 1},
};

const Champ *champ(const char *nom) {
  if (!nom) return nullptr;
  for (const Champ &c : kChamps)
    if (!strcmp(c.nom, nom)) return &c;
  return nullptr;
}

}  // namespace

bool paramsValides(const Params &p) {
  for (const Champ &c : kChamps)
    if (p.*c.m < c.lo || p.*c.m > c.hi) return false;
  return p.tempHystC < p.tempAlerteC;
}

const char *nomParam(uint8_t i) { return i < kNbParams ? kChamps[i].nom : nullptr; }

uint32_t valeurParam(const Params &p, uint8_t i) { return i < kNbParams ? p.*kChamps[i].m : 0; }

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

const char *texte(Sujet s) {
  static const char *const k[] = {"temperature", "lecture_temperature", "alim_hotte", "alim_module"};
  return (uint8_t)s < 4 ? k[(uint8_t)s] : "?";
}

void Surveillance::configurer(const Params &p) { p_ = p; }

void Surveillance::pousser(Sujet s, bool debut, int32_t valeur, int32_t seuil) {
  if (nb_ == kFile) {  // la plus ancienne cede : la file n'est jamais pleine en pratique
    tete_ = (uint8_t)((tete_ + 1) % kFile);
    nb_--;
  }
  Alerte &a = file_[(tete_ + nb_) % kFile];
  a.sujet = s;
  a.debut = debut;
  a.valeur = valeur;
  a.seuil = seuil;
  nb_++;
}

bool Surveillance::alerte(Alerte *out) {
  if (!nb_) return false;
  *out = file_[tete_];
  tete_ = (uint8_t)((tete_ + 1) % kFile);
  nb_--;
  return true;
}

void Surveillance::temperature(bool valide, int32_t d, uint32_t) {
  if (!valide) {  // une lecture en echec est une alerte (7.4)
    lecturesRatees_++;
    if (!alerteLecture_) {
      alerteLecture_ = true;
      pousser(Sujet::LectureTemperature, true, 0, 0);
    }
    return;
  }
  if (alerteLecture_) {
    alerteLecture_ = false;
    pousser(Sujet::LectureTemperature, false, d, 0);
  }
  tempConnue_ = true;
  temp_ = d;
  if (d > tempMax_) tempMax_ = d;
  if (d > maxPose_) maxPose_ = d;
  const int32_t seuil = (int32_t)p_.tempAlerteC * 10;
  const int32_t retour = seuil - (int32_t)p_.tempHystC * 10;
  if (!alerteTemp_ && d > seuil) {
    alerteTemp_ = true;
    alertesTemp_++;
    pousser(Sujet::Temperature, true, d, seuil);
  } else if (alerteTemp_ && d < retour) {
    alerteTemp_ = false;
    pousser(Sujet::Temperature, false, d, seuil);
  }
}

void Surveillance::voie(Voie &v, Sujet s, uint32_t mv, uint32_t minMv, uint32_t seuilMv) {
  v.actuelMv = mv;
  v.minSecondeMv = minMv;
  if (!v.connue || minMv < v.minDemarrageMv) v.minDemarrageMv = minMv;
  v.connue = true;
  if (!v.sous && minMv < seuilMv) {
    v.sous = true;
    v.passages++;  // compte meme sans alerte : l'essai de 24 h le lit
  } else if (v.sous && minMv > seuilMv + p_.alimHystMv) {
    v.sous = false;
  }
  const bool alerte = v.sous && p_.alimAlertes;
  if (alerte != v.alerte) {
    v.alerte = alerte;
    pousser(s, alerte, (int32_t)minMv, (int32_t)seuilMv);
  }
}

void Surveillance::tensions(uint32_t hotteMv, uint32_t hotteMinMv, uint32_t moduleMv, uint32_t moduleMinMv) {
  voie(hotte_, Sujet::AlimHotte, hotteMv, hotteMinMv, p_.alimHotteMinMv);
  voie(module_, Sujet::AlimModule, moduleMv, moduleMinMv, p_.alimModuleMinMv);
}

bool Surveillance::maxPoseAEcrire(uint32_t nowMs) const {
  if (maxPose_ == kAucun || maxPose_ < maxPoseEcrit_ + 10) return false;
  return !maxPoseEcritUneFois_ || nowMs - maxPoseEcritMs_ >= 60000;
}

void Surveillance::maxPoseEcrit(uint32_t nowMs) {
  maxPoseEcrit_ = maxPose_;
  maxPoseEcritUneFois_ = true;
  maxPoseEcritMs_ = nowMs;
}

}  // namespace surv
