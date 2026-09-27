// Imprime en JSON les motifs du banc de src/motifs.cpp (noms, repos, pauses,
// octets et segments des trames), pour tools/tests/test_motifs.py qui les
// compare a tools/signaux.py. Rafale : longueur, somme et 20 premiers segments.
// Compile et lance par test_motifs.py (clang++ -std=c++17 -Wall -Wextra -Werror -Isrc).
#include <cstdio>
#include <vector>

#include "motifs.h"

using namespace motif;

static const uint32_t kIndices[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255, 256, 999};

static const char *b(bool v) { return v ? "true" : "false"; }

static void segments(const Seg *s, size_t n) {
  std::printf("[");
  for (size_t i = 0; i < n; i++) std::printf("%s[%s,%u]", i ? "," : "", b(s[i].haut), (unsigned)s[i].us);
  std::printf("]");
}

int main() {
  std::vector<Seg> seg(kRafaleSegs);
  uint8_t oct[16];
  std::printf("{\"motifs\":[");
  for (uint8_t k = 0; k <= (uint8_t)Id::Rafale; k++) {
    const Id id = (Id)k;
    Id retrouve = Id::Rafale;
    const bool ok = depuisNom(nom(id), &retrouve) && retrouve == id;
    std::printf("%s{\"nom\":\"%s\",\"retrouve\":%s,\"repos_haut\":%s,\"pause_us\":%u,\"trames\":[", k ? "," : "",
                nom(id), b(ok), b(reposHaut(id)), (unsigned)pauseUs(id));
    for (size_t j = 0; j < sizeof(kIndices) / sizeof(kIndices[0]); j++) {
      const uint32_t index = kIndices[j];
      const size_t no = octets(id, index, oct, sizeof(oct));
      const size_t ns = trame(id, index, seg.data(), seg.size());
      unsigned long long somme = 0;
      for (size_t i = 0; i < ns; i++) somme += seg[i].us;
      std::printf("%s{\"index\":%u,\"octets\":[", j ? "," : "", (unsigned)index);
      for (size_t i = 0; i < no; i++) std::printf("%s%u", i ? "," : "", (unsigned)oct[i]);
      std::printf("],\"n\":%u,\"somme_us\":%llu,\"segments\":", (unsigned)ns, somme);
      segments(seg.data(), id == Id::Rafale && ns > 20 ? 20 : ns);
      std::printf("}");
    }
    // Pire nombre de segments sur les index 0..1023 (tous les restes modulo 256 et 8).
    size_t pire = 0;
    for (uint32_t index = 0; index < 1024; index++) {
      const size_t ns = trame(id, index, seg.data(), seg.size());
      if (ns > pire) pire = ns;
    }
    std::printf("],\"max_segments\":%u}", (unsigned)pire);
  }
  Id inutile = Id::Rafale;
  std::printf("],\"seg_max\":%u,\"rafale_segs\":%u,\"inconnu\":%s", (unsigned)kSegMax, (unsigned)kRafaleSegs,
              b(depuisNom("inconnu", &inutile)));
  // Capacite trop petite : 0, jamais une trame tronquee.
  std::printf(",\"cap_trop_petite\":{\"uart500\":%u,\"rafale\":%u,\"octets\":%u}}\n",
              (unsigned)trame(Id::Uart500, 0, seg.data(), 3), (unsigned)trame(Id::Rafale, 0, seg.data(), kRafaleSegs - 1),
              (unsigned)octets(Id::Uart500, 0, oct, 5));
  return 0;
}
