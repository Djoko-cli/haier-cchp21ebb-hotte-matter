#pragma once
#include <stddef.h>
#include <stdint.h>

namespace capt {

constexpr uint16_t kDurMax = 110;  // durees par partie (ligne trame)

// Miroir pur de rmt_symbol_word_t : deux demi-symboles (duree en ticks sur 15 bits, niveau GPIO).
struct Sym {
  uint16_t d0;
  uint8_t l0;
  uint16_t d1;
  uint8_t l1;
};

struct Reglages {
  uint32_t resolHz = 1000000;  // 1000000 ou 500000
  uint8_t filtreUs = 1;        // 0..3
  uint32_t silenceUs = 5000;   // 1000 .. kSilenceMaxTicks ticks
  bool inverse = true;         // etage d'ecoute inverseur
};

bool resolValide(uint32_t hz);
bool filtreValide(uint32_t us);
bool silenceValide(uint32_t us, uint32_t resolHz);
// Reglages bornes : toute valeur hors bornes est remplacee par sa valeur par defaut.
// Rend le nombre de valeurs remplacees.
uint8_t borner(Reglages *r);

// Bloc brut depose par le rappel RMT (on_recv_done) dans le tampon circulaire.
struct Bloc {
  uint64_t tFinUs;       // esp_timer_get_time() dans le rappel
  bool dernier;          // edata->flags.is_last
  bool debordAvant;      // des symboles ont ete perdus juste avant ce bloc
  uint16_t nSym;
  const Sym *sym;
};

// Une partie publiee (ligne trame).
struct Partie {
  uint32_t num = 0;       // numero de reception (depuis le demarrage, a partir de 1)
  uint32_t part = 0;      // rang dans la reception (a partir de 0)
  bool fin = false;       // derniere partie de la reception
  uint64_t tUs = 0;       // debut de la partie
  bool niv0Haut = true;   // niveau DU BUS pendant dur[0]
  bool debord = false;    // perte juste avant cette partie
  uint16_t n = 0;         // nombre de durees
  uint32_t dur[kDurMax] = {};  // µs
};

typedef void (*EmetPartie)(const Partie &p, void *ctx);

// Transforme les blocs successifs en parties. Etat : reception en cours, rang, niveau courant.
class Decoupeur {
 public:
  void reset();
  // Convertit b (ticks -> µs selon r.resolHz, niveaux GPIO -> niveaux du bus si r.inverse),
  // retire les durees nulles, decoupe en parties de kDurMax durees au plus et appelle emit
  // pour chacune. tUs de la premiere partie d'un bloc = b.tFinUs - somme(durees du bloc)
  // - (b.dernier ? r.silenceUs : 0). Rend le nombre de parties emises.
  size_t traiter(const Bloc &b, const Reglages &r, EmetPartie emit, void *ctx);
  uint32_t receptions() const { return num_; }

 private:
  uint32_t num_ = 0, part_ = 0;
  bool enCours_ = false;
};

// |a-b| <= max(10, 5 % du plus grand).
bool procheDe(uint32_t a, uint32_t b);

// Mode changements : ne s'applique qu'aux receptions d'une seule partie (part 0 et fin).
class Changements {
 public:
  void reset();
  // true : p differe de la derniere reception emise (ou n'est pas comparable) et doit partir ;
  // false : identique, le compteur de repetitions avance.
  bool aEmettre(const Partie &p);
  // Repetitions a porter par la prochaine partie emise (puis remises a zero).
  uint32_t prendreRep();
  uint32_t repEnCours() const { return rep_; }

 private:
  bool a_ = false;
  bool niv0_ = true;
  uint16_t n_ = 0;
  uint32_t dur_[kDurMax] = {};
  uint32_t rep_ = 0;
};

}  // namespace capt
