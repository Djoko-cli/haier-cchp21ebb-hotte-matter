// ===========================================================================
//  NVS du produit : voir reglages_produit.h.
// ===========================================================================
#include "reglages_produit.h"

#include <Preferences.h>

#include "config_produit.h"
#include "hotte_map.h"

namespace {

const char *const kParams = "params", *const kParamsS = "params_s", *const kDerniere = "derniere_v";
const char *const kEtatPub = "etat_pub", *const kTempMax = "temp_max", *const kEssai = "essai";
const char *const kSimu = "simu", *const kSimuDem = "simu_dem";

// Ouvert en ecriture meme pour lire (comme benq) : en lecture seule, un espace
// absent fait loguer une erreur NVS au premier demarrage. isKey() avant toute
// lecture : interroger une cle absente logue aussi une erreur.
template <class F>
bool avec(F f) {
  Preferences p;
  if (!p.begin(kNvsProduit, false)) return false;
  const bool ok = f(p);
  p.end();
  return ok;
}

}  // namespace

uint8_t nvsChargerParams(hotte::Params *a, surv::Params *s) {
  uint8_t remplaces = 0;
  hotte::Params pa;
  surv::Params ps;
  avec([&](Preferences &p) {
    if (p.isKey(kParams)) {
      const size_t n = p.getBytesLength(kParams);
      uint8_t buf[sizeof(hotte::Params) + 8];
      const size_t lu = n <= sizeof(buf) ? p.getBytes(kParams, buf, n) : 0;
      if (!hotte::chargerParams(buf, lu, &pa)) remplaces++;
    }
    if (p.isKey(kParamsS)) {
      const size_t n = p.getBytesLength(kParamsS);
      uint8_t buf[sizeof(surv::Params) + 8];
      const size_t lu = n <= sizeof(buf) ? p.getBytes(kParamsS, buf, n) : 0;
      if (!surv::chargerParams(buf, lu, &ps)) remplaces++;
    }
    return true;
  });
  *a = pa;
  *s = ps;
  return remplaces;
}

bool nvsSauverParams(const hotte::Params &a) {
  return avec([&](Preferences &p) { return p.putBytes(kParams, &a, sizeof(a)) == sizeof(a); });
}

bool nvsSauverSurveillance(const surv::Params &s) {
  return avec([&](Preferences &p) { return p.putBytes(kParamsS, &s, sizeof(s)) == sizeof(s); });
}

bool nvsLireDerniere(hotte::Moteur *m) {
  uint8_t v = 0;
  const bool ok = avec([&](Preferences &p) {
    if (!p.isKey(kDerniere)) return false;
    v = p.getUChar(kDerniere, 0);
    return v >= 1 && v <= 3;
  });
  if (ok) *m = (hotte::Moteur)v;
  return ok;
}

bool nvsEcrireDerniere(hotte::Moteur m) {
  return avec([&](Preferences &p) { return p.putUChar(kDerniere, (uint8_t)m) == 1; });
}

bool nvsLireEtatPub(hotte::Etat *e) {
  uint16_t v = 0;
  const bool lu = avec([&](Preferences &p) {
    if (!p.isKey(kEtatPub)) return false;
    v = p.getUShort(kEtatPub, 0);
    return true;
  });
  return lu && hotte::depuisNvs(v, e);
}

bool nvsEcrireEtatPub(const hotte::Etat &e) {
  return avec([&](Preferences &p) { return p.putUShort(kEtatPub, hotte::versNvs(e)) == 2; });
}

bool nvsLireTempMax(int32_t *d) {
  return avec([&](Preferences &p) {
    if (!p.isKey(kTempMax)) return false;
    *d = p.getInt(kTempMax, 0);
    return true;
  });
}

bool nvsEcrireTempMax(int32_t d) {
  return avec([&](Preferences &p) { return p.putInt(kTempMax, d) == 4; });
}

bool nvsEffacerTempMax() {
  return avec([&](Preferences &p) { return !p.isKey(kTempMax) || p.remove(kTempMax); });
}

uint16_t nvsLireEssaiMin() {
  uint16_t v = 0;
  avec([&](Preferences &p) {
    if (p.isKey(kEssai)) v = p.getUShort(kEssai, 0);
    return true;
  });
  return v > 4320 ? 4320 : v;  // 72 h au plus
}

bool nvsEcrireEssaiMin(uint16_t min) {
  return avec([&](Preferences &p) {
    if (!min) return !p.isKey(kEssai) || p.remove(kEssai);
    return p.putUShort(kEssai, min) == 2;
  });
}

#if PILOTE_SIMULE
bool nvsLireSimu(sim::Reglages *r) {
  sim::Reglages lu;
  const bool ok = avec([&](Preferences &p) {
    return p.isKey(kSimu) && p.getBytesLength(kSimu) == sizeof(lu) && p.getBytes(kSimu, &lu, sizeof(lu)) == sizeof(lu);
  });
  if (!ok || !sim::reglagesValides(lu)) return false;
  *r = lu;
  return true;
}

bool nvsSauverSimu(const sim::Reglages &r) {
  return avec([&](Preferences &p) { return p.putBytes(kSimu, &r, sizeof(r)) == sizeof(r); });
}

bool nvsPrendreSimuDem(sim::EtatHotte *e) {
  uint16_t v = 0;
  const bool lu = avec([&](Preferences &p) {
    if (!p.isKey(kSimuDem)) return false;
    v = p.getUShort(kSimuDem, 0);
    p.remove(kSimuDem);  // une fois
    return true;
  });
  return lu && sim::depuisMot(v, e);
}

bool nvsEcrireSimuDem(const sim::EtatHotte &e) {
  return avec([&](Preferences &p) { return p.putUShort(kSimuDem, sim::versMot(e)) == 2; });
}
#endif
