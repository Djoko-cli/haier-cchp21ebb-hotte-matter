#pragma once
// Motifs du banc de validation (spec section 10) : segments de LIGNE et contenus
// connus, identiques a tools/signaux.py (croisement : tools/tests/test_motifs.py).
// Pur : compile sur l'hote et dans le firmware generateur (src/gen_main.cpp).
#include <stddef.h>
#include <stdint.h>
namespace motif {
enum class Id : uint8_t { Uart500, Uart500Inv, Uart2400, Uart2400Inv, Uart9600, Uart9600Inv, Wtc, Krona, Rafale };
struct Seg { bool haut; uint32_t us; };   // niveau de LIGNE
const char *nom(Id id);                    // "uart500", "uart500inv", ..., "wtc", "krona", "rafale"
bool depuisNom(const char *s, Id *out);
bool reposHaut(Id id);
uint32_t pauseUs(Id id);                   // repos apres chaque trame
// Contenu connu de la trame index (octets, ou bits pour wtc emballes MSB d'abord).
size_t octets(Id id, uint32_t index, uint8_t *out, size_t cap);
// Segments de la trame index (sans le repos final). Rend le nombre de segments, 0 si cap trop petit.
size_t trame(Id id, uint32_t index, Seg *out, size_t cap);

// Ajouts au contrat (tampons du generateur) :
constexpr size_t kSegMax = 128;             // segments d'une trame, rafale exceptee (verifie par test_motifs.py)
constexpr uint32_t kRafaleSegs = 100000;    // rafale : segments alternes, le premier haut
constexpr uint32_t kRafaleUs = 100;         // rafale : duree de chaque segment
}
