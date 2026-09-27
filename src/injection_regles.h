#pragma once
// ===========================================================================
//  Garde-fous de l'injection (docs/SPEC-RECONNAISSANCE.md 8.3 et 8.4) : pur
//
//  - Params : valeurs de l'avenant de l'etape 6 (defauts d'avant l'avenant),
//    bornes appliquees par 'injection regle' et au chargement de la NVS ;
//  - analyser : 'injecte durees <d1> <d2> ...', la seule syntaxe avant
//    l'avenant (durees en us, alternees, la premiere au niveau BAS du bus) ;
//  - Etat : etage declare monte, armement (desarme seul apres armeMaxS),
//    delai minimal entre deux emissions ;
//  - niveauAttendu, frontAnormal, frontProgramme : surveillance des fronts
//    pendant une emission (appelees par l'interruption des fronts : en IRAM
//    sur la carte) ;
//  - versSymboles : durees -> symboles RMT de l'etage d'injection.
//
//  Pur et sans Arduino : teste sur l'hote (tools/tests/test_injection.cpp).
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

namespace inj {

constexpr uint16_t kDurMax = 64;  // durees par demande

struct Params {
  uint32_t basMaxUs = 3000, totalMaxUs = 200000, silenceMinUs = 20000;
  uint32_t attenteMaxMs = 1000, delaiMinMs = 3000, armeMaxS = 600, tolUs = 20;
};
// Chaque valeur dans ses bornes (bornesParam), et basMaxUs <= totalMaxUs.
bool paramsValides(const Params &p);

// Ajouts : reglage par nom ('injection regle <nom> <valeur>', USB seulement)
// et chargement de la NVS. Noms : ceux des champs de config.injection.
constexpr uint8_t kNbParams = 7;
const char *nomParam(uint8_t i);                                // i >= kNbParams : nullptr
bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi);  // false : nom inconnu
enum class Reglage : uint8_t { Ok, NomInconnu, HorsBornes };
Reglage reglerParam(Params *p, const char *nom, uint32_t v);    // *p inchange si refus
// Image lue en NVS (cle inj_params) : acceptee si elle a la taille de Params
// et des valeurs valides (true) ; sinon *out recoit les valeurs par defaut.
bool chargerParams(const void *octets, size_t n, Params *out);

enum class Refus : uint8_t { Aucun, NonMontee, NonArmee, Delai, Syntaxe, BasTropLong, TropLong, TropDeDurees };
const char *refusTexte(Refus r);  // "non montee", "non armee", "delai minimal", "syntaxe", ...

struct Demande {
  uint16_t n = 0;
  uint32_t dur[kDurMax] = {};  // alternees, dur[0] au niveau BAS du bus
};
// args : ce qui suit 'injecte' (ex. "durees 750 750 750 2250"). Bornes : chaque duree basse
// (rang pair) <= basMaxUs, somme <= totalMaxUs, 1..kDurMax durees, entiers > 0.
// *out n'est ecrit que si la demande est acceptee (Refus::Aucun).
Refus analyser(const char *args, const Params &p, Demande *out);
uint32_t totalUs(const Demande &d);  // ajout : somme des durees

class Etat {
 public:
  void setMontee(bool m) { montee_ = m; }
  bool montee() const { return montee_; }
  void armer(uint32_t nowMs);
  void desarmer() { arme_ = false; }
  bool armee(uint32_t nowMs, const Params &p) const;       // faux apres armeMaxS
  uint32_t resteS(uint32_t nowMs, const Params &p) const;  // 0 si non armee
  Refus admettre(uint32_t nowMs, const Params &p) const;   // montee, armee, delai depuis la derniere emission
  void noterEmission(uint32_t nowMs);

 private:
  bool montee_ = false, arme_ = false, aEmis_ = false;
  uint32_t armeAt_ = 0, emisAt_ = 0;
};

// Niveau du bus attendu a tRelUs depuis le debut de l'emission (true = haut ; apres la fin : haut).
bool niveauAttendu(const Demande &d, uint32_t tRelUs);
// Front observe vers le niveau niv a tRelUs : anormal si aucun front programme a +-tolUs
// et si niv differe du niveau attendu.
bool frontAnormal(const Demande &d, uint32_t tRelUs, bool niv, uint32_t tolUs);
// Ajout : un front programme (0, puis la fin de chaque duree) a +-tolUs de tRelUs ?
bool frontProgramme(const Demande &d, uint32_t tRelUs, uint32_t tolUs);

// Ajout : symboles RMT (miroir de rmt_symbol_word_t) de l'etage d'injection,
// niveau GPIO7 : 1 = bus tire bas (rang pair), 0 = bus relache. Une duree de
// plus de 32 767 us est decoupee en demi-symboles de meme niveau ; un nombre
// impair de demi-symboles est complete par 1 us GPIO bas (bus relache). Rend
// le nombre de symboles, 0 si la demande est vide ou si cap est trop petit.
struct Sym {
  uint16_t d0;
  uint8_t l0;
  uint16_t d1;
  uint8_t l1;
};
constexpr size_t kSymMax = 48;  // une memoire de canal RMT ; le pire cas des bornes en prend 35
size_t versSymboles(const Demande &d, Sym *out, size_t cap);

}  // namespace inj
