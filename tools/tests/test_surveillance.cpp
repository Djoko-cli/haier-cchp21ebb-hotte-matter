// Tests hote de la surveillance (src/produit/surveillance.*) : bornes des
// reglages, seuils et hysteresis de la temperature et des deux tensions,
// lecture en echec, maximum depuis la pose (spec produit 4.2, 7.4, 9.1).
// Lancer : sh tools/tests/test_hote.sh
#include <string.h>

#include "surveillance.h"
#include "verif.h"

using namespace surv;

static void testParams() {
  const Params p;
  VERIF(paramsValides(p) && p.tempAlerteC == 70 && p.tempHystC == 5 && p.alimHotteMinMv == 4500 &&
        p.alimModuleMinMv == 3700 && p.alimHystMv == 100 && p.alimAlertes == 0);
  const char *noms[] = {"temp_alerte_c", "temp_hyst_c", "alim_hotte_min_mv", "alim_module_min_mv", "alim_hyst_mv",
                        "alim_alertes"};
  for (uint8_t i = 0; i < kNbParams; i++) VERIF_EGAL_STR(nomParam(i), noms[i]);
  VERIF(nomParam(kNbParams) == nullptr && valeurParam(p, 0) == 70);
  uint32_t lo = 0, hi = 0;
  VERIF(bornesParam("temp_alerte_c", &lo, &hi) && lo == 20 && hi == 85);
  VERIF(bornesParam("alim_module_min_mv", &lo, &hi) && lo == 3000 && hi == 5000);
  for (uint8_t i = 0; i < kNbParams; i++) {
    Params q;
    VERIF(bornesParam(nomParam(i), &lo, &hi));
    if (!strcmp(nomParam(i), "temp_alerte_c")) q.tempHystC = 1;
    VERIF(reglerParam(&q, nomParam(i), lo) == Reglage::Ok);
    VERIF(reglerParam(&q, nomParam(i), hi) == Reglage::Ok);
    if (lo) VERIF(reglerParam(&q, nomParam(i), lo - 1) == Reglage::HorsBornes);
    VERIF(reglerParam(&q, nomParam(i), hi + 1) == Reglage::HorsBornes);
  }
  Params q;
  VERIF(reglerParam(&q, "temp_alerte_c", 25) == Reglage::Ok);  // B15 : seuil abaisse au banc
  VERIF(reglerParam(&q, "temp_hyst_c", 10) == Reglage::Ok);
  VERIF(reglerParam(&q, "inconnu", 1) == Reglage::NomInconnu);
  Params lu;
  VERIF(chargerParams(&q, sizeof(q), &lu) && lu.tempAlerteC == 25);
  q.alimHystMv = 1;
  VERIF(!chargerParams(&q, sizeof(q), &lu) && lu.tempAlerteC == 70);
  VERIF_EGAL_STR(texte(Sujet::LectureTemperature), "lecture_temperature");
  VERIF_EGAL_STR(texte(Sujet::AlimModule), "alim_module");
}

static void testTemperature() {
  Surveillance s;
  Alerte a;
  s.temperature(true, 415, 0);
  VERIF(s.tempConnue() && s.tempDixiemes() == 415 && s.tempMaxDixiemes() == 415 && !s.alerte(&a));
  s.temperature(true, 700, 10000);  // au seuil : pas encore
  VERIF(!s.alerteTemperature() && !s.alerte(&a));
  s.temperature(true, 701, 20000);
  VERIF(s.alerteTemperature() && s.alerte(&a) && a.sujet == Sujet::Temperature && a.debut && a.valeur == 701 &&
        a.seuil == 700);
  s.temperature(true, 660, 30000);  // hysteresis : retour sous 65
  VERIF(s.alerteTemperature() && !s.alerte(&a));
  s.temperature(true, 649, 40000);
  VERIF(!s.alerteTemperature() && s.alerte(&a) && !a.debut && s.alertesTemperature() == 1);
  VERIF(s.tempMaxDixiemes() == 701);
  // Lecture en echec (NAN) : alerte, puis fin a la lecture suivante.
  s.temperature(false, 0, 50000);
  VERIF(s.alerteTemperature() && s.alerte(&a) && a.sujet == Sujet::LectureTemperature && a.debut);
  s.temperature(false, 0, 60000);
  VERIF(!s.alerte(&a) && s.lecturesRatees() == 2);
  s.temperature(true, 420, 70000);
  VERIF(!s.alerteTemperature() && s.alerte(&a) && a.sujet == Sujet::LectureTemperature && !a.debut);
}

static void testTensions() {
  Params p;
  Surveillance s(p);  // alertes de tension coupees (banc) : passages comptes, aucune alerte
  Alerte a;
  s.tensions(20, 10, 15, 5);
  VERIF(s.hotte().connue && s.hotte().minSecondeMv == 10 && s.hotte().passages == 1 && !s.alerte(&a));
  p.alimAlertes = 1;
  s.configurer(p);
  s.tensions(5000, 4990, 4400, 4380);
  VERIF(!s.alerte(&a) && s.hotte().minDemarrageMv == 10);
  s.tensions(4600, 4499, 4400, 3699);  // les deux voies passent sous leur seuil
  VERIF(s.alerte(&a) && a.sujet == Sujet::AlimHotte && a.debut && a.valeur == 4499 && a.seuil == 4500);
  VERIF(s.alerte(&a) && a.sujet == Sujet::AlimModule && a.debut && a.valeur == 3699 && a.seuil == 3700);
  s.tensions(4600, 4550, 4400, 3790);  // hysteresis de 100 mV
  VERIF(!s.alerte(&a));
  s.tensions(4700, 4601, 4400, 3801);
  VERIF(s.alerte(&a) && a.sujet == Sujet::AlimHotte && !a.debut);
  VERIF(s.alerte(&a) && a.sujet == Sujet::AlimModule && !a.debut);
  VERIF(s.hotte().passages == 2 && s.module().passages == 2 && s.module().minDemarrageMv == 5);
}

static void testMaxPose() {
  Surveillance s;
  s.maxPoseLu(Surveillance::kAucun);  // NVS vide
  VERIF(!s.maxPoseAEcrire(0));
  s.temperature(true, 400, 0);
  VERIF(s.maxPose() == 400 && s.maxPoseAEcrire(0));
  s.maxPoseEcrit(0);
  s.temperature(true, 409, 70000);  // moins d'un degre de plus : rien
  VERIF(!s.maxPoseAEcrire(70000));
  s.temperature(true, 410, 80000);
  VERIF(s.maxPoseAEcrire(80000));
  s.maxPoseEcrit(80000);
  s.temperature(true, 500, 100000);  // moins d'une minute apres la derniere ecriture
  VERIF(!s.maxPoseAEcrire(100000) && s.maxPoseAEcrire(140000));
  Surveillance t;
  t.maxPoseLu(550);  // relu : une temperature plus basse ne l'ecrase pas
  t.temperature(true, 450, 0);
  VERIF(t.maxPose() == 550 && !t.maxPoseAEcrire(0));
  t.maxPoseRaz();
  VERIF(t.maxPose() == Surveillance::kAucun);
}

int main() {
  testParams();
  testTemperature();
  testTensions();
  testMaxPose();
  return bilan("test_surveillance");
}
