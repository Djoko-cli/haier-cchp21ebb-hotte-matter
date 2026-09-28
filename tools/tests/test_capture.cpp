// Tests hote de src/capture_model.* : bornes des reglages de capture, symboles
// RMT -> parties (conversion, inversion, durees nulles, decoupe a 110, t_us,
// numerotation, pertes), mode changements.
// Lancer : sh tools/tests/test_hote.sh
#include <initializer_list>

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

// ---------------------------------------------------------------------------
//  Decoupeur : symboles RMT -> parties
// ---------------------------------------------------------------------------

static Partie gParts[8];
static size_t gN = 0;

static void collecte(const Partie &p, void *ctx) {
  VERIF(ctx == &gN);
  if (gN < 8) gParts[gN] = p;
  gN++;
}

static size_t traite(Decoupeur &d, const Bloc &b, const Reglages &r) {
  gN = 0;
  const size_t n = d.traiter(b, r, collecte, &gN);
  VERIF(n == gN);
  return n;
}

// Somme des durees d'une partie.
static uint64_t somme(const Partie &p) {
  uint64_t s = 0;
  for (uint16_t i = 0; i < p.n; i++) s += p.dur[i];
  return s;
}

// Une trame courte : 750 750 750 2250 100, fin de reception (demi-symbole final nul).
// Niveaux GPIO : 0 pendant d0, 1 pendant d1 ; etage inverseur : GPIO bas = bus haut.
static const Sym kCourte[3] = {{750, 0, 750, 1}, {750, 0, 2250, 1}, {100, 0, 0, 1}};

static void testConversion() {
  Decoupeur d;
  Reglages r;  // 1 MHz, inverse, silence 5000 us
  VERIF(traite(d, Bloc{1000000, true, false, 3, kCourte}, r) == 1);
  const Partie &p = gParts[0];
  VERIF(p.num == 1 && p.part == 0 && p.fin && !p.debord && p.niv0Haut && p.n == 5);
  VERIF(p.dur[0] == 750 && p.dur[1] == 750 && p.dur[2] == 750 && p.dur[3] == 2250 && p.dur[4] == 100);
  VERIF(p.tUs == 1000000u - 4600u - 5000u);  // fin - somme - silence
  VERIF(d.receptions() == 1);
  // 500 kHz : un tick vaut 2 us.
  r.resolHz = kResolLenteHz;
  r.silenceUs = 40000;
  VERIF(traite(d, Bloc{2000000, true, false, 3, kCourte}, r) == 1);
  VERIF(gParts[0].num == 2 && gParts[0].n == 5 && gParts[0].dur[0] == 1500 && gParts[0].dur[3] == 4500 &&
        gParts[0].dur[4] == 200);
  VERIF(gParts[0].tUs == 2000000u - 9200u - 40000u);
  // Etage non inverseur : GPIO bas = bus bas.
  r = Reglages();
  r.inverse = false;
  VERIF(traite(d, Bloc{3000000, true, false, 3, kCourte}, r) == 1);
  VERIF(!gParts[0].niv0Haut && gParts[0].n == 5);
}

// Hors bloc dernier (celui du marqueur de fin : testMarqueurFin), une duree
// nulle est retiree.
static void testDureesNulles() {
  Decoupeur d;
  const Reglages r;
  // Nulle en tete : la premiere duree est celle de d1 (GPIO 1 : bus bas).
  const Sym tete[2] = {{0, 0, 300, 1}, {400, 0, 0, 1}};
  VERIF(traite(d, Bloc{1000000, false, false, 2, tete}, r) == 1);
  VERIF(gParts[0].n == 2 && !gParts[0].niv0Haut && gParts[0].dur[0] == 300 && gParts[0].dur[1] == 400);
  // Nulle au milieu : les deux durees voisines de meme niveau sont fondues (les niveaux restent alternes).
  const Sym milieu[3] = {{100, 0, 200, 1}, {0, 0, 50, 1}, {80, 0, 0, 1}};
  VERIF(traite(d, Bloc{1000000, false, false, 3, milieu}, r) == 1);
  VERIF(gParts[0].n == 3 && gParts[0].dur[0] == 100 && gParts[0].dur[1] == 250 && gParts[0].dur[2] == 80);
  VERIF(gParts[0].tUs == 1000000u - 430u);
}

// Marqueur de fin du RMT (duree nulle) dans le bloc dernier : rien ne compte
// apres. Nombre pair de durees : le marqueur ouvre un mot, dont le C6 remplit
// la seconde moitie avec la derniere duree recue, niveau compris (banc 0 du
// 28/09 : krona, 2000 us lus 4000 ; uart9600inv, 208 us lus 416).
static void testMarqueurFin() {
  Decoupeur d;
  const Reglages r;  // inverse : GPIO 1 = bus bas
  // Repos pris avant la trame (bus bas 18047), puis haut 2000, bas 6000, haut 2000 : 4 durees.
  const Sym paire[3] = {{18047, 1, 2000, 0}, {6000, 1, 2000, 0}, {0, 1, 2000, 0}};
  VERIF(traite(d, Bloc{1000000, true, false, 3, paire}, r) == 1);
  VERIF(gParts[0].n == 4 && !gParts[0].niv0Haut && gParts[0].fin);
  VERIF(gParts[0].dur[0] == 18047 && gParts[0].dur[1] == 2000 && gParts[0].dur[2] == 6000 && gParts[0].dur[3] == 2000);
  VERIF(gParts[0].tUs == 1000000u - 28047u - 5000u);
  // Marqueur en tete du bloc dernier d'une reception en cours : partie vide, fin.
  const Sym plein[1] = {{100, 0, 200, 1}};
  const Sym tete[1] = {{0, 0, 200, 1}};
  VERIF(traite(d, Bloc{2000000, false, false, 1, plein}, r) == 1);
  VERIF(traite(d, Bloc{2005000, true, false, 1, tete}, r) == 1);
  VERIF(gParts[0].part == 1 && gParts[0].fin && gParts[0].n == 0 && gParts[0].tUs == 2005000u - 5000u);
  // Un front seul, puis le silence : rien, aucune reception comptee.
  VERIF(traite(d, Bloc{3000000, true, false, 1, tete}, r) == 0);
  VERIF(d.receptions() == 2);
}

static void testDecoupe() {
  Decoupeur d;
  const Reglages r;
  // 150 symboles de 10 + 20 us, le dernier sans d1 : 299 durees -> 110 + 110 + 79.
  static Sym s[150];
  for (Sym &x : s) x = Sym{10, 0, 20, 1};
  s[149].d1 = 0;
  const uint64_t fin = 50000000;
  VERIF(traite(d, Bloc{fin, true, false, 150, s}, r) == 3);
  VERIF(gParts[0].n == 110 && gParts[1].n == 110 && gParts[2].n == 79);
  VERIF(somme(gParts[0]) == 1650 && somme(gParts[1]) == 1650 && somme(gParts[2]) == 1180);
  // t_us successifs : chaque partie commence ou la precedente finit.
  VERIF(gParts[0].tUs == fin - 4480 - 5000);
  VERIF(gParts[1].tUs == gParts[0].tUs + 1650 && gParts[2].tUs == gParts[1].tUs + 1650);
  // Niveaux alternes d'une partie a la suivante : la premiere duree de la
  // partie k+1 a le niveau oppose de la derniere de la partie k.
  for (int k = 0; k < 2; k++) {
    const Partie &a = gParts[k];
    const bool dernierHaut = a.niv0Haut != ((a.n - 1) % 2 == 1);
    VERIF(gParts[k + 1].niv0Haut == !dernierHaut);
  }
  VERIF(gParts[0].niv0Haut && gParts[1].dur[0] == 10 && gParts[2].dur[78] == 10);
  // Une seule reception : meme num, rangs 0..2, fin sur la derniere seulement.
  for (int k = 0; k < 3; k++) VERIF(gParts[k].num == 1 && gParts[k].part == (uint32_t)k && !gParts[k].debord);
  VERIF(!gParts[0].fin && !gParts[1].fin && gParts[2].fin);
  // Exactement 110 durees : une seule partie, pas de partie vide derriere.
  static Sym t[55];
  for (Sym &x : t) x = Sym{10, 0, 20, 1};
  VERIF(traite(d, Bloc{fin, true, false, 55, t}, r) == 1 && gParts[0].n == 110 && gParts[0].fin);
}

static void testReceptions() {
  Decoupeur d;
  const Reglages r;
  const Sym a[3] = {{100, 0, 200, 1}, {100, 0, 200, 1}, {100, 0, 200, 1}};
  const Sym b[2] = {{100, 0, 50, 1}, {70, 0, 0, 1}};
  // Reception en deux blocs : le premier sans fin (tampon du pilote plein), sans silence retranche.
  VERIF(traite(d, Bloc{2000000, false, false, 3, a}, r) == 1);
  VERIF(gParts[0].num == 1 && gParts[0].part == 0 && !gParts[0].fin && gParts[0].tUs == 2000000u - 900u);
  VERIF(traite(d, Bloc{2005300, true, false, 2, b}, r) == 1);
  VERIF(gParts[0].num == 1 && gParts[0].part == 1 && gParts[0].fin && gParts[0].tUs == 2005300u - 220u - 5000u);
  // La suivante porte le numero 2.
  VERIF(traite(d, Bloc{3000000, true, false, 2, b}, r) == 1 && gParts[0].num == 2 && gParts[0].part == 0);
  VERIF(d.receptions() == 2);
  // Perte avant un bloc : la reception en cours est abandonnee (sans fin), une nouvelle commence.
  VERIF(traite(d, Bloc{4000000, false, false, 3, a}, r) == 1 && gParts[0].num == 3);
  VERIF(traite(d, Bloc{4100000, true, true, 2, b}, r) == 1);
  VERIF(gParts[0].num == 4 && gParts[0].part == 0 && gParts[0].fin && gParts[0].debord);
  // reset() : la reception en cours est close, la numerotation continue.
  VERIF(traite(d, Bloc{5000000, false, false, 3, a}, r) == 1 && gParts[0].num == 5);
  d.reset();
  VERIF(traite(d, Bloc{5100000, true, false, 2, b}, r) == 1 && gParts[0].num == 6 && gParts[0].part == 0);
  VERIF(d.receptions() == 6);
}

static void testBlocsVides() {
  Decoupeur d;
  const Reglages r;
  const Sym a[1] = {{100, 0, 200, 1}};
  // Bloc final vide qui clot une reception en cours : partie vide, fin.
  VERIF(traite(d, Bloc{1000000, false, false, 1, a}, r) == 1);
  VERIF(traite(d, Bloc{1010000, true, false, 0, nullptr}, r) == 1);
  VERIF(gParts[0].num == 1 && gParts[0].part == 1 && gParts[0].fin && gParts[0].n == 0);
  VERIF(gParts[0].tUs == 1010000u - 5000u);
  // Bloc vide hors reception, ou seulement des durees nulles : rien, aucune reception comptee.
  const Sym zero[1] = {{0, 0, 0, 1}};
  VERIF(traite(d, Bloc{1020000, true, false, 0, nullptr}, r) == 0);
  VERIF(traite(d, Bloc{1030000, true, false, 1, zero}, r) == 0);
  VERIF(traite(d, Bloc{1040000, false, false, 1, zero}, r) == 0);
  VERIF(d.receptions() == 1);
  // Bloc vide apres une perte : une partie vide porte debord.
  VERIF(traite(d, Bloc{1050000, true, true, 0, nullptr}, r) == 1);
  VERIF(gParts[0].num == 2 && gParts[0].debord && gParts[0].fin && gParts[0].n == 0);
  // Horloge plus courte que la reception (impossible en service) : t_us borne a 0.
  VERIF(traite(d, Bloc{1000, true, false, 1, a}, r) == 1 && gParts[0].tUs == 0);
}

// ---------------------------------------------------------------------------
//  Mode changements
// ---------------------------------------------------------------------------

static void testProcheDe() {
  VERIF(procheDe(100, 100));
  VERIF(procheDe(100, 110) && procheDe(110, 100));  // 10 us au moins
  VERIF(!procheDe(100, 111));
  VERIF(procheDe(0, 10) && !procheDe(0, 11));
  VERIF(procheDe(1000, 1050) && !procheDe(1000, 1053));  // 5 % du plus grand
  VERIF(procheDe(1000000, 1050000) && !procheDe(1000000, 1060000));
  VERIF(procheDe(4294967295u, 4294967294u) && !procheDe(0, 4294967295u));  // sans debordement
}

static Partie simple(std::initializer_list<uint32_t> dur, bool niv0Haut = true) {
  Partie p;
  p.num = 1;
  p.fin = true;
  p.niv0Haut = niv0Haut;
  for (uint32_t x : dur) p.dur[p.n++] = x;
  return p;
}

static void testChangements() {
  Changements c;
  const Partie p1 = simple({750, 750, 750, 2250});
  VERIF(c.aEmettre(p1) && c.prendreRep() == 0);
  VERIF(!c.aEmettre(p1) && c.repEnCours() == 1);
  VERIF(!c.aEmettre(p1) && c.repEnCours() == 2);
  // A 5 % pres : identique.
  VERIF(!c.aEmettre(simple({760, 745, 750, 2300})) && c.repEnCours() == 3);
  // Une duree hors tolerance : emise, elle porte les 3 repetitions.
  const Partie p2 = simple({750, 800, 750, 2250});
  VERIF(c.aEmettre(p2) && c.prendreRep() == 3 && c.repEnCours() == 0 && c.prendreRep() == 0);
  // Nombre de durees ou niveau de depart differents : emise.
  VERIF(c.aEmettre(simple({750, 800, 750})));
  VERIF(c.aEmettre(simple({750, 800, 750}, false)));
  // Comparaison a la derniere EMISE : une derive lente finit par partir.
  const Partie q0 = simple({1000}), q1 = simple({1040}), q2 = simple({1080});
  VERIF(c.aEmettre(q0));
  VERIF(!c.aEmettre(q1));  // 40 us de 1000 : identique
  VERIF(c.aEmettre(q2));   // 80 us de 1000 : differente (40 us de 1040 seulement)
  VERIF(c.prendreRep() == 1);
  // Reception en plusieurs parties : pas comparable, toujours emise ; elle
  // devient la derniere emise, la reception simple suivante part aussi.
  Partie longue = simple({1080});
  longue.fin = false;
  VERIF(c.aEmettre(longue));
  longue.part = 1;
  longue.fin = true;
  VERIF(c.aEmettre(longue));
  VERIF(c.aEmettre(q2) && !c.aEmettre(q2));
  // Une perte (debord) part toujours, meme identique.
  Partie perte = q2;
  perte.debord = true;
  VERIF(c.aEmettre(perte) && c.prendreRep() == 1);
  // reset : plus de reference, plus de repetitions.
  VERIF(!c.aEmettre(q2) && c.repEnCours() == 1);
  c.reset();
  VERIF(c.repEnCours() == 0 && c.aEmettre(q2));
}

int main() {
  testBornes();
  testBorner();
  testConversion();
  testDureesNulles();
  testMarqueurFin();
  testDecoupe();
  testReceptions();
  testBlocsVides();
  testProcheDe();
  testChangements();
  return bilan("test_capture");
}
