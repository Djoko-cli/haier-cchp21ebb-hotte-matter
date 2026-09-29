// Tests hote de la correspondance Matter (src/produit/hotte_map.*) : paliers
// et pourcentages (3.2, 3.3), reflet d'un etat, regroupement des ecritures
// d'EP1 et d'EP2 (5.4, regle 6), garde-fou de demarrage, ecritures NVS
// differees et etat publie en NVS (4.4).
// Lancer : sh tools/tests/test_hote.sh
#include "hotte_map.h"
#include "verif.h"

using namespace hotte;

static void testCorrespondance() {
  VERIF(palier(0) == Moteur::Arret);
  for (int p = 1; p <= 100; p++) {
    const Moteur m = palier((uint8_t)p);
    VERIF(m == (p <= 33 ? Moteur::V1 : p <= 66 ? Moteur::V2 : Moteur::V3));
    // Aller et retour : un curseur pose a p revient a la valeur canonique de son palier.
    VERIF(palier(pourcent(m)) == m);
  }
  VERIF(palier(101) == Moteur::V3 && palier(255) == Moteur::V3);
  VERIF(pourcent(Moteur::Arret) == 0 && pourcent(Moteur::V1) == 33 && pourcent(Moteur::V2) == 66 &&
        pourcent(Moteur::V3) == 100);
  VERIF(fanMode(Moteur::Arret) == kFanOff && fanMode(Moteur::V1) == kFanLow && fanMode(Moteur::V2) == kFanMedium &&
        fanMode(Moteur::V3) == kFanHigh);
  VERIF(cibleDeMode(kFanOff) == CibleVentilo::Eteint && cibleDeMode(kFanLow) == CibleVentilo::V1 &&
        cibleDeMode(kFanMedium) == CibleVentilo::V2 && cibleDeMode(kFanHigh) == CibleVentilo::V3 &&
        cibleDeMode(kFanOn) == CibleVentilo::Derniere);
  VERIF(cibleDeMode(kFanAuto) == CibleVentilo::Aucune && cibleDeMode(kFanSmart) == CibleVentilo::Aucune);
  VERIF(cibleDe(Moteur::Arret) == CibleVentilo::Eteint && cibleDe(Moteur::V2) == CibleVentilo::V2);
  // "Armee" compte comme eteinte ; la prolongee comme son palier.
  Etat e;
  e.marche = Marche::Armee;
  e.moteur = Moteur::Arret;
  e.lampe = true;
  e.lampeConnue = true;
  Reflet r = reflet(e);
  VERIF(r.fanMode == kFanOff && r.pourcent == 0 && r.lampe);
  e.marche = Marche::Prolongee;
  e.moteur = Moteur::V2;
  e.lampeConnue = false;
  r = reflet(e);
  VERIF(r.fanMode == kFanMedium && r.pourcent == 66 && !r.lampe);
}

static CibleVentilo fermer(RegroupementEp1 &g, uint32_t t) {
  CibleVentilo c = CibleVentilo::Aucune;
  VERIF(g.pret(t, 700, 3000, &c));
  VERIF(!g.ouvert());
  return c;
}

// Priorites de la regle 6.
static void testRegroupementEp1() {
  RegroupementEp1 g;
  CibleVentilo c = CibleVentilo::Aucune;
  VERIF(!g.pret(0, 700, 3000, &c));  // rien d'ouvert
  g.ecrireMode(kFanOn, 1000);
  VERIF(!g.pret(1699, 700, 3000, &c));  // pas encore le calme
  VERIF(fermer(g, 1700) == CibleVentilo::Derniere);  // On seul : derniere vitesse
  g.ecrireMode(kFanOn, 0);
  g.ecrirePourcent(40, 10);
  VERIF(fermer(g, 800) == CibleVentilo::V2);  // On et 40 % : V2
  g.ecrirePourcent(40, 0);
  g.ecrireMode(kFanOn, 10);
  VERIF(fermer(g, 800) == CibleVentilo::V2);  // 40 % et On : V2 aussi
  g.ecrirePourcent(40, 0);
  g.ecrireMode(kFanOff, 10);
  VERIF(fermer(g, 800) == CibleVentilo::Eteint);  // 40 % puis Off : eteindre
  g.ecrirePourcent(0, 0);
  g.ecrirePourcent(50, 10);
  VERIF(fermer(g, 800) == CibleVentilo::V2);  // 0 puis 50 % : V2
  g.ecrireMode(kFanOff, 0);
  g.ecrirePourcent(0, 0);  // cascade du serveur CHIP
  VERIF(fermer(g, 800) == CibleVentilo::Eteint);
  g.ecrirePourcent(0, 0);
  g.ecrireMode(kFanOff, 0);
  VERIF(fermer(g, 800) == CibleVentilo::Eteint);
  g.ecrireMode(kFanLow, 0);
  VERIF(fermer(g, 800) == CibleVentilo::V1);
  g.ecrireMode(kFanHigh, 0);
  g.ecrirePourcent(0, 5);  // un 0 plus ancien ne fait pas ceder High
  g.ecrireMode(kFanHigh, 10);
  VERIF(fermer(g, 800) == CibleVentilo::V3);
  g.ecrirePourcent(100, 0);
  VERIF(fermer(g, 800) == CibleVentilo::V3);  // bouton de la tuile a 100 % : V3 (3.3, regle 4)
  g.ecrireMode(kFanAuto, 0);
  VERIF(fermer(g, 800) == CibleVentilo::Aucune);  // hors sequence : rien
  // Premiere ecriture de la fenetre rendue ; plafond a 3 s depuis elle.
  uint32_t premier = 0;
  g.ecrirePourcent(10, 5000);
  for (uint32_t t = 5000; t < 8000; t += 500) g.ecrirePourcent(20, t);
  VERIF(!g.pret(7999, 700, 3000, &c, &premier));
  VERIF(g.pret(8000, 700, 3000, &c, &premier) && premier == 5000 && c == CibleVentilo::V1);
}

// Rafales : ecritures plus rapprochees que le calme pendant 8 s, puis plus espacees.
static void testRafales() {
  RegroupementEp1 g;
  CibleVentilo derniere = CibleVentilo::Aucune;
  int ordres = 0;
  uint8_t ecrit = 0;
  for (uint32_t t = 0; t <= 12000; t += 10) {
    if (t < 8000 && t % 500 == 0) {
      ecrit = (uint8_t)(1 + (t / 500 * 37) % 100);
      g.ecrirePourcent(ecrit, t);
    }
    CibleVentilo c;
    if (g.pret(t, 700, 3000, &c)) {
      ordres++;
      derniere = c;
    }
  }
  VERIF(ordres <= 8000 / 3000 + 1 + 1);  // au plus ceil(8 s / plafond) + 1
  VERIF(derniere == cibleDe(palier(ecrit)));
  // Plus espacees que le calme : un ordre chacune.
  RegroupementEp1 h;
  ordres = 0;
  for (uint32_t t = 0; t <= 9000; t += 10) {
    if (t < 8000 && t % 1000 == 0) h.ecrireMode(t / 1000 % 2 ? kFanHigh : kFanLow, t);
    CibleVentilo c;
    if (h.pret(t, 700, 3000, &c)) ordres++;
  }
  VERIF(ordres == 8);
}

static void testRegroupementEp2() {
  RegroupementEp2 g;
  bool on = false;
  uint32_t premier = 0;
  g.ecrire(true, 100);
  g.ecrire(false, 200);
  g.ecrire(true, 300);
  VERIF(!g.pret(449, 150, 3000, &on));
  VERIF(g.pret(450, 150, 3000, &on, &premier) && on && premier == 100 && !g.ouvert());
}

static void testDemarrage() {
  VERIF(ignoreeAuDemarrage(1000, 1000, 2000));
  VERIF(ignoreeAuDemarrage(2999, 1000, 2000));
  VERIF(!ignoreeAuDemarrage(3000, 1000, 2000));
  VERIF(ignoreeAuDemarrage(990, 1000, 2000));  // deposee pendant Matter.begin()
  VERIF(!ignoreeAuDemarrage(1000, 1000, 0));
}

static void testEcritureDifferee() {
  EcritureDifferee d;
  d.ecrite(2);  // relue au demarrage
  d.noter(2, 0);
  VERIF(!d.enAttente());  // egale a l'ecrite : rien
  d.noter(3, 1000);
  VERIF(d.enAttente() && !d.echue(10999) && d.echue(11000) && d.valeur() == 3);
  d.noter(1, 5000);  // un nouveau changement repousse l'echeance
  VERIF(!d.echue(14999) && d.echue(15000) && d.valeur() == 1);
  d.ecrite(1);
  VERIF(!d.enAttente());
  d.noter(3, 20000);
  d.noter(1, 21000);  // revenue a la valeur ecrite : plus rien a ecrire
  VERIF(!d.enAttente());
  EcritureDifferee neuve;  // NVS vide : la premiere valeur s'ecrit
  neuve.noter(2, 0);
  VERIF(neuve.enAttente() && neuve.echue(10000));
}

static void testEtatNvs() {
  const Marche marches[] = {Marche::Eteinte, Marche::Armee, Marche::Prolongee, Marche::Inconnue};
  const Moteur moteurs[] = {Moteur::Arret, Moteur::V1, Moteur::V2, Moteur::V3};
  for (const Marche m : marches)
    for (const Moteur mo : moteurs)
      for (int l = 0; l < 4; l++) {
        Etat e;
        e.marche = m;
        e.moteur = mo;
        e.lampe = l & 1;
        e.lampeConnue = l & 2;
        Etat r;
        VERIF(depuisNvs(versNvs(e), &r) && memeEtat(r, e));
      }
  Etat r;
  VERIF(!depuisNvs(0, &r) && !depuisNvs(0xFFFF, &r) && !depuisNvs(0xA5C0, &r));
}

int main() {
  testCorrespondance();
  testRegroupementEp1();
  testRafales();
  testRegroupementEp2();
  testDemarrage();
  testEcritureDifferee();
  testEtatNvs();
  return bilan("test_hotte_map");
}
