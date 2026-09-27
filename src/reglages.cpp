// ===========================================================================
//  Reglages de la sonde en NVS (espace kNvsEspace, "hotte")
//
//  Cles : resol_hz (u32), filtre_us (u8), silence_us (u32), inverse (bool),
//  chgt (bool, mode changements), inj_monte (bool, etage d'injection monte),
//  inj_params (octets : inj::Params, valeurs de l'avenant). Une cle absente
//  vaut sa valeur par defaut ; une valeur hors bornes est remplacee par sa
//  valeur par defaut au chargement (capt::borner ; inj::chargerParams remet
//  tout le jeu d'injection aux defauts), et l'appelant le signale (spec 8.2).
// ===========================================================================
#include "reglages.h"

#include <Preferences.h>

#include "config.h"

uint8_t reglagesCharger(ReglagesSonde *r) {
  *r = ReglagesSonde();
  uint8_t remplaces = 0;
  Preferences p;
  // Ouverture en ecriture meme pour lire : en lecture seule, un espace de noms
  // absent fait loguer une erreur NVS au premier demarrage.
  if (p.begin(kNvsEspace, false)) {
    // isKey() d'abord : interroger une cle absente logue une erreur NVS.
    if (p.isKey("resol_hz")) r->capture.resolHz = p.getUInt("resol_hz", r->capture.resolHz);
    if (p.isKey("filtre_us")) r->capture.filtreUs = p.getUChar("filtre_us", r->capture.filtreUs);
    if (p.isKey("silence_us")) r->capture.silenceUs = p.getUInt("silence_us", r->capture.silenceUs);
    if (p.isKey("inverse")) r->capture.inverse = p.getBool("inverse", r->capture.inverse);
    if (p.isKey("chgt")) r->changements = p.getBool("chgt", r->changements);
    if (p.isKey("inj_monte")) r->injMontee = p.getBool("inj_monte", false);
    if (p.isKey("inj_params")) {
      uint8_t image[sizeof(inj::Params)] = {};
      const size_t n = p.getBytesLength("inj_params");
      const bool lu = n == sizeof(image) && p.getBytes("inj_params", image, sizeof(image)) == sizeof(image);
      if (!inj::chargerParams(lu ? image : nullptr, lu ? n : 0, &r->injection)) remplaces++;
    }
    p.end();
  }
  return (uint8_t)(remplaces + capt::borner(&r->capture));
}

bool reglagesSauver(const ReglagesSonde &r) {
  Preferences p;
  if (!p.begin(kNvsEspace, false)) return false;
  bool ok = p.putUInt("resol_hz", r.capture.resolHz) == sizeof(uint32_t);
  ok &= p.putUChar("filtre_us", r.capture.filtreUs) == 1;
  ok &= p.putUInt("silence_us", r.capture.silenceUs) == sizeof(uint32_t);
  ok &= p.putBool("inverse", r.capture.inverse) == 1;
  ok &= p.putBool("chgt", r.changements) == 1;
  ok &= p.putBool("inj_monte", r.injMontee) == 1;
  ok &= p.putBytes("inj_params", &r.injection, sizeof(r.injection)) == sizeof(r.injection);
  p.end();
  return ok;
}
