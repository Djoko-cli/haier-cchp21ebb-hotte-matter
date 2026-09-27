#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
static int gEchecs = 0, gVerifs = 0;
#define VERIF(c) do { gVerifs++; if (!(c)) { gEchecs++; std::printf("ECHEC %s:%d : %s\n", __FILE__, __LINE__, #c); } } while (0)
#define VERIF_EGAL_STR(a, b) VERIF(std::strcmp((a), (b)) == 0)
static int bilan(const char *nom) {
  std::printf("%s : %d verifications, %d echecs\n", nom, gVerifs, gEchecs);
  return gEchecs ? 1 : 0;
}
