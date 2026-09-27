// Tests hote de src/gen_symboles.h : segments de LIGNE -> symboles RMT du
// generateur (niveaux GPIO inverses, demi-symboles de 32767 ticks au plus,
// nombre pair de demi-symboles), sur des cas simples et sur tous les motifs.
// Lancer : sh tools/tests/test_hote.sh
#include <vector>

#include "gen_symboles.h"
#include "motifs.h"
#include "verif.h"

using motif::Seg;

// Re-deplie les symboles en segments de LIGNE (niveaux egaux fondus) ; false si une duree est nulle.
static bool deplier(const gen::Sym *y, size_t n, std::vector<Seg> *out) {
  out->clear();
  for (size_t i = 0; i < n; i++) {
    const uint16_t d[2] = {y[i].d0, y[i].d1};
    const uint8_t l[2] = {y[i].l0, y[i].l1};
    for (int m = 0; m < 2; m++) {
      if (!d[m] || d[m] > gen::kTicksMax) return false;
      const bool haut = l[m] == 0;  // GPIO bas = ligne haute (NPN bloque)
      if (!out->empty() && out->back().haut == haut) out->back().us += d[m];
      else out->push_back(Seg{haut, d[m]});
    }
  }
  return true;
}

static void testNiveaux() {
  VERIF(gen::niveauGpio(true) == 0);   // ligne haute : GPIO bas, NPN bloque
  VERIF(gen::niveauGpio(false) == 1);  // ligne basse : GPIO haut, NPN sature
}

static void testCasSimples() {
  gen::Sym y[4] = {};
  // Trois segments : nombre impair de demi-symboles, le dernier est coupe en deux.
  const Seg a[] = {{true, 100}, {false, 200}, {true, 301}};
  VERIF(gen::symboles(a, 3, y, 4) == 2);
  VERIF(y[0].d0 == 100 && y[0].l0 == 0 && y[0].d1 == 200 && y[0].l1 == 1);
  VERIF(y[1].d0 == 150 && y[1].l0 == 0 && y[1].d1 == 151 && y[1].l1 == 0);
  // Nombre pair : rien a couper.
  const Seg b[] = {{false, 750}, {true, 2250}};
  VERIF(gen::symboles(b, 2, y, 4) == 1);
  VERIF(y[0].d0 == 750 && y[0].l0 == 1 && y[0].d1 == 2250 && y[0].l1 == 0);
  // Segment plus long que 32767 ticks : decoupe en demi-symboles de meme niveau.
  const Seg c[] = {{false, 70000}};
  VERIF(gen::symboles(c, 1, y, 4) == 2);
  VERIF(y[0].d0 == 32767 && y[0].d1 == 32767 && y[0].l0 == 1 && y[0].l1 == 1);
  VERIF(y[1].d0 == 2233 && y[1].d1 == 2233 && y[1].l0 == 1 && y[1].l1 == 1);
  // Capacite trop petite : 0.
  VERIF(gen::symboles(a, 3, y, 1) == 0);
  VERIF(gen::symboles(c, 1, y, 1) == 0);
  // Dernier demi-symbole impair de 1 us : impossible a couper.
  const Seg d[] = {{true, 1}};
  VERIF(gen::symboles(d, 1, y, 4) == 0);
  // Rien a emettre : 0.
  VERIF(gen::symboles(a, 0, y, 4) == 0);
}

// Tous les motifs (rafale exceptee), index 0..1023 : les symboles redonnent les segments.
static void testTousLesMotifs() {
  Seg s[motif::kSegMax];
  gen::Sym y[gen::kSymMax];
  std::vector<Seg> v;
  bool ok = true;
  size_t pire = 0;
  for (uint8_t k = 0; k < (uint8_t)motif::Id::Rafale; k++) {
    for (uint32_t index = 0; index < 1024; index++) {
      const size_t ns = motif::trame((motif::Id)k, index, s, motif::kSegMax);
      const size_t ny = gen::symboles(s, ns, y, gen::kSymMax);
      if (!ns || !ny || !deplier(y, ny, &v) || v.size() != ns) {
        ok = false;
        continue;
      }
      for (size_t i = 0; i < ns; i++)
        if (v[i].haut != s[i].haut || v[i].us != s[i].us) ok = false;
      if (ny > pire) pire = ny;
    }
  }
  VERIF(ok);
  VERIF(pire > 0 && pire <= gen::kSymMax);
  std::printf("  pire trame : %u symboles (tampon de %u)\n", (unsigned)pire, (unsigned)gen::kSymMax);
}

static void testRafale() {
  // Motif de la boucle du generateur : kRafaleBoucle symboles {haut, bas} de kRafaleUs,
  // rejoues kRafaleTours fois : kRafaleSegs segments en tout, le premier haut.
  VERIF(gen::kRafaleBoucle * 2 * gen::kRafaleTours == motif::kRafaleSegs);
  VERIF(gen::kRafaleBoucle + 1 <= gen::kMemSymboles);  // plus le symbole de fin
  VERIF(gen::kRafaleTours <= gen::kToursParLot);        // un seul lot : pas de relance, pas de trou
  gen::Sym y[gen::kRafaleBoucle];
  gen::rafale(y);
  bool ok = true;
  for (size_t i = 0; i < gen::kRafaleBoucle; i++)
    if (y[i].d0 != motif::kRafaleUs || y[i].l0 != 0 || y[i].d1 != motif::kRafaleUs || y[i].l1 != 1) ok = false;
  VERIF(ok);
}

int main() {
  testNiveaux();
  testCasSimples();
  testTousLesMotifs();
  testRafale();
  return bilan("test_generateur");
}
