// ===========================================================================
//  Motifs du banc de validation (docs/SPEC-RECONNAISSANCE.md section 10)
//
//  Segments de LIGNE (niveau haut ?, duree en us), sans le repos qui precede
//  ni la pause qui suit ; les segments consecutifs de meme niveau sont fondus.
//  Memes contenus et memes segments que tools/signaux.py :
//   - UART 8N1, LSB d'abord ; frontieres des bits d'un octet a
//     (k * 1000000 + bauds / 2) / bauds us de son debut (k = 0..10) ; le bit
//     de stop du dernier octet fait partie de la trame ;
//   - wtc : trame (depart 2T bas, 16 bits, 1T haut), 4T haut, reponse
//     (depart, 8 bits, 1T haut) : 5T haut entre les deux apres fusion ;
//   - krona : UART 500 bauds inverse, 4 000 us de repos entre deux octets ;
//   - rafale : kRafaleSegs segments alternes de kRafaleUs, le premier haut.
//  Pur : compile sur l'hote (tools/tests/dump_motifs.cpp) et dans le
//  firmware generateur.
// ===========================================================================
#include "motifs.h"

#include <string.h>

namespace motif {
namespace {

struct Def {
  const char *nom;
  bool reposHaut;
  uint32_t pauseUs;  // repos apres la trame
  uint32_t bauds;    // UART (krona compris) ; 0 sinon
};

// Dans l'ordre de Id.
const Def kDefs[] = {
    {"uart500", true, 20000, 500},   {"uart500inv", false, 20000, 500}, {"uart2400", true, 20000, 2400},
    {"uart2400inv", false, 20000, 2400}, {"uart9600", true, 20000, 9600}, {"uart9600inv", false, 20000, 9600},
    {"wtc", true, 6000, 0},          {"krona", false, 18000, 500},      {"rafale", true, 1000000, 0},
};
constexpr uint8_t kNombre = sizeof(kDefs) / sizeof(kDefs[0]);
constexpr uint32_t kTWtcUs = 750;
constexpr uint32_t kPauseOctetKronaUs = 4000;

const Def *def(Id id) { return (uint8_t)id < kNombre ? &kDefs[(uint8_t)id] : nullptr; }

// Ajoute des segments en fondant les niveaux egaux ; un depassement de
// capacite est retenu et fin() rend alors 0.
class Ecrivain {
 public:
  Ecrivain(Seg *out, size_t cap) : out_(out), cap_(cap) {}
  void ajouter(bool haut, uint32_t us) {
    if (!us) return;
    if (n_ && out_[n_ - 1].haut == haut) {
      out_[n_ - 1].us += us;
      return;
    }
    if (n_ == cap_) {
      plein_ = true;
      return;
    }
    out_[n_++] = Seg{haut, us};
  }
  size_t fin() const { return plein_ ? 0 : n_; }

 private:
  Seg *out_;
  size_t cap_;
  size_t n_ = 0;
  bool plein_ = false;
};

void uart(Ecrivain &e, const uint8_t *o, size_t n, uint32_t bauds, bool reposHaut, uint32_t pauseOctetUs) {
  uint32_t bornes[11];
  for (uint32_t k = 0; k < 11; k++) bornes[k] = (k * 1000000u + bauds / 2) / bauds;
  for (size_t i = 0; i < n; i++) {
    if (i) e.ajouter(reposHaut, pauseOctetUs);
    for (uint32_t k = 0; k < 10; k++) {  // depart, 8 bits de donnees, stop
      const bool bit = k == 0 ? false : k == 9 ? true : ((o[i] >> (k - 1)) & 1) != 0;
      e.ajouter(bit == reposHaut, bornes[k + 1] - bornes[k]);  // 1 logique = niveau du repos
    }
  }
}

// Distance d'impulsion (type WTC6534) : depart 2T bas, bits MSB d'abord
// (0 = 1T haut + 1T bas, 1 = 1T haut + 3T bas), fin 1T haut.
void impulsions(Ecrivain &e, uint32_t valeur, uint8_t nBits) {
  e.ajouter(false, 2 * kTWtcUs);
  for (int k = nBits - 1; k >= 0; k--) {
    e.ajouter(true, kTWtcUs);
    e.ajouter(false, ((valeur >> k) & 1) ? 3 * kTWtcUs : kTWtcUs);
  }
  e.ajouter(true, kTWtcUs);
}

uint32_t valeurWtc(uint32_t index) { return (0x0100u << (index % 8)) & 0xFFFFu; }

}  // namespace

const char *nom(Id id) {
  const Def *d = def(id);
  return d ? d->nom : "?";
}

bool depuisNom(const char *s, Id *out) {
  for (uint8_t k = 0; k < kNombre; k++) {
    if (!strcmp(s, kDefs[k].nom)) {
      *out = (Id)k;
      return true;
    }
  }
  return false;
}

bool reposHaut(Id id) {
  const Def *d = def(id);
  return d ? d->reposHaut : true;
}

uint32_t pauseUs(Id id) {
  const Def *d = def(id);
  return d ? d->pauseUs : 0;
}

size_t octets(Id id, uint32_t index, uint8_t *out, size_t cap) {
  uint8_t c[6];
  size_t n = 0;
  switch (id) {
    case Id::Uart500: case Id::Uart500Inv: case Id::Uart2400: case Id::Uart2400Inv:
    case Id::Uart9600: case Id::Uart9600Inv: {
      c[0] = 0xA5;
      c[1] = 0x5A;
      c[2] = 0x00;
      c[3] = 0xFF;
      c[4] = (uint8_t)(index & 0xFF);
      c[5] = (uint8_t)((0xA5 + 0x5A + 0x00 + 0xFF + (index & 0xFF)) & 0xFF);
      n = 6;
      break;
    }
    case Id::Wtc: {
      const uint32_t v = valeurWtc(index);
      c[0] = (uint8_t)(v >> 8);
      c[1] = (uint8_t)(v & 0xFF);
      c[2] = (uint8_t)(index & 0xFF);
      n = 3;
      break;
    }
    case Id::Krona: {
      const uint8_t k = (uint8_t)(index % 8);
      c[0] = 0x55;
      c[1] = k;
      c[2] = (uint8_t)(~k & 0xFF);
      c[3] = 0x00;
      c[4] = (uint8_t)((c[0] + c[1] + c[2] + c[3]) & 0xFF);
      n = 5;
      break;
    }
    case Id::Rafale: return 0;
  }
  if (n > cap) return 0;
  memcpy(out, c, n);
  return n;
}

size_t trame(Id id, uint32_t index, Seg *out, size_t cap) {
  const Def *d = def(id);
  if (!d) return 0;
  Ecrivain e(out, cap);
  if (id == Id::Rafale) {
    if (cap < kRafaleSegs) return 0;
    for (uint32_t k = 0; k < kRafaleSegs; k++) out[k] = Seg{k % 2 == 0, kRafaleUs};
    return kRafaleSegs;
  }
  if (id == Id::Wtc) {
    impulsions(e, valeurWtc(index), 16);
    e.ajouter(true, 4 * kTWtcUs);  // reponse simulee de la carte 4T apres la fin
    impulsions(e, index & 0xFF, 8);
    return e.fin();
  }
  uint8_t o[6];
  const size_t n = octets(id, index, o, sizeof(o));
  uart(e, o, n, d->bauds, d->reposHaut, id == Id::Krona ? kPauseOctetKronaUs : 0);
  return e.fin();
}

}  // namespace motif
