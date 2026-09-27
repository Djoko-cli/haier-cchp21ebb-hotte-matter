// Tests hote de src/capture_model.* : bornes des reglages de capture.
// Lancer : sh tools/tests/test_hote.sh
#include "capture_model.h"
#include "config.h"
#include "verif.h"

using namespace capt;

static void testBornes() {
  VERIF(resolValide(1000000));
  VERIF(resolValide(500000));
  VERIF(!resolValide(0));
  VERIF(!resolValide(250000));
  VERIF(!resolValide(2000000));
  VERIF(filtreValide(0));
  VERIF(filtreValide(3));
  VERIF(!filtreValide(4));
  VERIF(!filtreValide(255));
  // Silence : 1000 us au moins, 32767 ticks au plus (32767 us a 1 MHz, 65534 us a 500 kHz).
  VERIF(silenceValide(1000, 1000000));
  VERIF(!silenceValide(999, 1000000));
  VERIF(silenceValide(32767, 1000000));
  VERIF(!silenceValide(32768, 1000000));
  VERIF(silenceValide(65534, 500000));
  VERIF(!silenceValide(65535, 500000));
  VERIF(!silenceValide(999, 500000));
  VERIF(!silenceValide(0, 1000000));
  VERIF(!silenceValide(4294967295u, 500000));  // produit sur 64 bits : pas de debordement
}

static void testBorner() {
  Reglages r;
  VERIF(r.resolHz == kResolDefautHz && r.filtreUs == kFiltreDefautUs && r.silenceUs == kSilenceDefautUs && r.inverse);
  VERIF(borner(&r) == 0);
  r.resolHz = kResolLenteHz;
  r.filtreUs = 3;
  r.silenceUs = 65534;
  r.inverse = false;
  VERIF(borner(&r) == 0 && r.resolHz == kResolLenteHz && r.filtreUs == 3 && r.silenceUs == 65534 && !r.inverse);
  // Silence valable a 500 kHz mais pas a 1 MHz : remplace par sa valeur par defaut.
  r.resolHz = kResolDefautHz;
  VERIF(borner(&r) == 1 && r.silenceUs == kSilenceDefautUs && r.filtreUs == 3);
  // Tout hors bornes (NVS corrompue) : trois valeurs remplacees.
  r.resolHz = 123;
  r.filtreUs = 200;
  r.silenceUs = 5;
  VERIF(borner(&r) == 3 && r.resolHz == kResolDefautHz && r.filtreUs == kFiltreDefautUs &&
        r.silenceUs == kSilenceDefautUs);
  // Resolution remplacee d'abord : le silence est juge avec la resolution par defaut.
  r.resolHz = 0;
  r.silenceUs = 40000;
  VERIF(borner(&r) == 2 && r.resolHz == kResolDefautHz && r.silenceUs == kSilenceDefautUs);
}

int main() {
  testBornes();
  testBorner();
  return bilan("test_capture");
}
