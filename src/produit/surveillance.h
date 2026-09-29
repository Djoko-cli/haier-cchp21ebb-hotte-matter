#pragma once
// ===========================================================================
//  Surveillance (docs/SPEC-PRODUIT.md 4.2, 7.4) : pur
//
//  Temperature de la puce (lue toutes les 10 s) et tensions des deux voies
//  d'alimentation (minimum de chaque seconde, en mV) : seuils avec
//  hysteresis, alertes de debut et de fin, maxima, passages sous les seuils,
//  maximum depuis la pose (NVS temp_max). AUCUNE action sur la hotte : les
//  alertes ne sont que des messages.
//
//  Temperatures en dixiemes de degre (41,5 C -> 415), tensions en mV.
//  Teste sur l'hote (tools/tests/test_surveillance.cpp).
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

namespace surv {

struct Params {
  uint32_t tempAlerteC = 70, tempHystC = 5;  // Q34 : alerte au-dela de 70, retour sous 65
  uint32_t alimHotteMinMv = 4500, alimModuleMinMv = 3700, alimHystMv = 100;
  // 1 : alertes de tension en service (pont de mesure monte). 0 au banc,
  // ou GPIO2 et GPIO3 sont en l'air : les tensions sont publiees, sans alerte.
  uint32_t alimAlertes = 0;
};
// Chaque valeur dans ses bornes, et temp_hyst_c < temp_alerte_c.
bool paramsValides(const Params &p);
constexpr uint8_t kNbParams = 6;
const char *nomParam(uint8_t i);  // temp_alerte_c, temp_hyst_c, alim_hotte_min_mv, alim_module_min_mv, alim_hyst_mv, alim_alertes
uint32_t valeurParam(const Params &p, uint8_t i);
bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi);
enum class Reglage : uint8_t { Ok, NomInconnu, HorsBornes };
Reglage reglerParam(Params *p, const char *nom, uint32_t v);
bool chargerParams(const void *octets, size_t n, Params *out);

enum class Sujet : uint8_t { Temperature, LectureTemperature, AlimHotte, AlimModule };
const char *texte(Sujet s);  // temperature, lecture_temperature, alim_hotte, alim_module

struct Alerte {
  Sujet sujet = Sujet::Temperature;
  bool debut = true;  // faux : fin
  int32_t valeur = 0;  // dixiemes de degre, ou mV ; lecture en echec : 0
  int32_t seuil = 0;
};

// Une voie d'alimentation : minimum de la derniere seconde, depuis le
// demarrage, alerte avec hysteresis, passages sous le seuil.
struct Voie {
  bool connue = false;
  uint32_t actuelMv = 0, minSecondeMv = 0, minDemarrageMv = 0;
  bool sous = false;    // sous le seuil (sortie au-dela du seuil + hysteresis)
  bool alerte = false;  // sous le seuil, alertes de tension en service
  uint32_t passages = 0;
};

class Surveillance {
 public:
  explicit Surveillance(const Params &p = Params()) : p_(p) {}
  void configurer(const Params &p);
  const Params &params() const { return p_; }

  // Lecture de la temperature : valide faux si elle a echoue (NAN).
  void temperature(bool valide, int32_t dixiemes, uint32_t nowMs);
  // Tensions : actuelle et minimum de la seconde ecoulee, pour chaque voie.
  void tensions(uint32_t hotteMv, uint32_t hotteMinMv, uint32_t moduleMv, uint32_t moduleMinMv);
  // File des alertes (debut, fin), videe par la tache loop.
  bool alerte(Alerte *out);

  bool tempConnue() const { return tempConnue_; }
  int32_t tempDixiemes() const { return temp_; }
  int32_t tempMaxDixiemes() const { return tempMax_; }  // depuis le demarrage
  bool alerteTemperature() const { return alerteTemp_ || alerteLecture_; }
  uint32_t alertesTemperature() const { return alertesTemp_; }
  uint32_t lecturesRatees() const { return lecturesRatees_; }
  const Voie &hotte() const { return hotte_; }
  const Voie &module() const { return module_; }

  // Maximum depuis la pose (NVS temp_max). maxPoseLu : relu au demarrage
  // (absent : -1000). A ecrire au plus une fois par minute, et seulement
  // s'il depasse l'ecrit d'au moins 1 degre ; maxPoseEcrit apres succes.
  void maxPoseLu(int32_t dixiemes) { maxPose_ = maxPoseEcrit_ = dixiemes; }
  int32_t maxPose() const { return maxPose_; }
  bool maxPoseAEcrire(uint32_t nowMs) const;
  void maxPoseEcrit(uint32_t nowMs);
  void maxPoseRaz() { maxPose_ = maxPoseEcrit_ = kAucun; }  // a la pose, par l'USB

  static constexpr int32_t kAucun = -1000;

 private:
  void pousser(Sujet s, bool debut, int32_t valeur, int32_t seuil);
  void voie(Voie &v, Sujet s, uint32_t mv, uint32_t minMv, uint32_t seuilMv);

  Params p_;
  bool tempConnue_ = false, alerteTemp_ = false, alerteLecture_ = false;
  int32_t temp_ = 0, tempMax_ = kAucun;
  uint32_t alertesTemp_ = 0, lecturesRatees_ = 0;
  int32_t maxPose_ = kAucun, maxPoseEcrit_ = kAucun;
  bool maxPoseEcritUneFois_ = false;
  uint32_t maxPoseEcritMs_ = 0;
  Voie hotte_, module_;
  static constexpr uint8_t kFile = 8;
  Alerte file_[kFile];
  uint8_t tete_ = 0, nb_ = 0;
};

}  // namespace surv
