// Tests de bout en bout de l'automate avec le pilote simule (spec produit
// 9.1) : suites d'appuis de chaque couple (etat, cible) du 5.3 dans les trois
// modes d'etat, avec et sans lecture annexe ; cas imposes ; regle d'or sur
// des suites generees ; annulation par le panneau a chaque etape ; delais ;
// confirmation et nouvel essai ; sonde de vitesse ; demarrage.
// Lancer : sh tools/tests/test_hote.sh
#include <stdio.h>

#include "banc_hotte.h"
#include "verif.h"

using namespace hotte;

namespace {

struct Ligne {
  const char *nom;
  ModeEtat mode;
  bool annexe;
};
const Ligne kLignes[] = {
    {"repete", ModeEtat::Repete, false},
    {"repete+annexe", ModeEtat::Repete, true},
    {"changements", ModeEtat::Changements, false},
    {"changements+annexe", ModeEtat::Changements, true},
    {"appuis+annexe", ModeEtat::AppuisSeuls, true},
};

struct Depart {
  const char *nom;
  Marche m;
  Moteur mo;
};
const Depart kDeparts[] = {
    {"eteinte", Marche::Eteinte, Moteur::Arret}, {"armee", Marche::Armee, Moteur::Arret},
    {"v1", Marche::Armee, Moteur::V1},           {"v2", Marche::Armee, Moteur::V2},
    {"v3", Marche::Armee, Moteur::V3},           {"prol-v1", Marche::Prolongee, Moteur::V1},
    {"prol-v2", Marche::Prolongee, Moteur::V2},  {"prol-v3", Marche::Prolongee, Moteur::V3},
};

const CibleVentilo kCibles[] = {CibleVentilo::Eteint, CibleVentilo::V1, CibleVentilo::V2, CibleVentilo::V3};

Moteur moteurCible(CibleVentilo c) {
  return c == CibleVentilo::V1 ? Moteur::V1 : c == CibleVentilo::V2 ? Moteur::V2 : c == CibleVentilo::V3 ? Moteur::V3
                                                                                                          : Moteur::Arret;
}

void echec(const char *quoi, const Ligne &l, const Depart &d, CibleVentilo c) {
  printf("  -> %s : ligne %s, depart %s, cible %s\n", quoi, l.nom, d.nom, texte(c));
}

}  // namespace

// Chaque couple (etat, cible) du 5.3, dans chaque mode : issue, etat final
// de la hotte, regle d'or, aucun refus du pilote.
static void testMatrice() {
  for (const Ligne &l : kLignes)
    for (const Depart &d : kDeparts)
      for (const CibleVentilo c : kCibles) {
        BancHotte b(l.mode, l.annexe);
        b.demarrer();
        b.mener(d.m, d.mo);
        if (!b.hotteEst(d.m, d.mo)) {
          echec("depart non atteint", l, d, c);
          VERIF(false);
          continue;
        }
        b.fins.clear();
        b.a.ordreVentilo(c, 7, Canal::App, b.t);
        const Action *f = b.attendreFin();
        const Action fin0 = f ? *f : Action();
        f = f ? &fin0 : nullptr;
        b.pas(1000);  // appuis seuls : la fin vient de l'echo, avant la reaction de la carte
        const bool sansLecture = l.mode == ModeEtat::Changements && !l.annexe && d.m == Marche::Eteinte;
        const Moteur y = moteurCible(c);
        if (sansLecture && c != CibleVentilo::Eteint) {
          // Aucun etat lu depuis la mise sous tension : "marche" attend un
          // arret lu, qui ne vient pas. Abandon, jamais "marche".
          VERIF(f && f->issue == Issue::Abandon && f->cause == Cause::SansLecture);
          VERIF(b.nbAppuis(Touche::Marche) == 0);
        } else {
          const bool ok = f && f->issue == Issue::Ok && f->idOrdre == 7 && f->canal == Canal::App;
          if (!ok) echec(f ? texte(f->issue) : "pas de fin", l, d, c);
          VERIF(ok);
          bool fin;
          if (c == CibleVentilo::Eteint) fin = b.hotteEst(Marche::Eteinte, Moteur::Arret);
          else if (d.m == Marche::Prolongee && d.mo == y) fin = b.hotteEst(Marche::Prolongee, y);  // Q38
          else fin = b.hotteEst(Marche::Armee, y);
          if (!fin) echec("etat final", l, d, c);
          VERIF(fin);
        }
        if (b.violations || b.refusPilote) echec("regle d'or ou refus du pilote", l, d, c);
        VERIF(b.violations == 0);
        VERIF(b.refusPilote == 0);
      }
}

// Cas imposes du 9.1, dans les trois modes d'etat.
static void testCasImposes() {
  const Ligne rep = kLignes[0], chg = kLignes[3], app = kLignes[4];
  // Eteindre depuis V2, carte qui repond : Ok partout ; appuis seuls : marche final Deduit.
  for (const Ligne &l : {rep, chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V2);
    b.appuis.clear();
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::Matter, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && f->appuis == 2);
    VERIF(b.appuis.size() == 2 && b.appuis[0] == Touche::V2 && b.appuis[1] == Touche::Marche);
    if (l.mode == ModeEtat::AppuisSeuls) VERIF(b.a.etat().confiance == Confiance::Deduit);
    b.pas(1000);
    VERIF(b.hotteEst(Marche::Eteinte, Moteur::Arret));
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
  // Idem, premier appui V2 sans effet : un nouvel essai, puis Ok.
  for (const Ligne &l : {rep, chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V2);
    b.appuis.clear();
    b.p.sansEffet(1);
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::Matter, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok);
    VERIF(b.nbAppuis(Touche::V2) == 2 && b.nbAppuis(Touche::Marche) == 1);
    VERIF(b.a.diagnostic(b.t).c.nouveauxEssais == 1);
    b.pas(1000);
    VERIF(b.hotteEst(Marche::Eteinte, Moteur::Arret));
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
  // Lampe, trame de la carte illisible (repete) : Presume, puis la
  // repetition suivante tranche. Lampe changee : Ok, un seul appui.
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.pas(1000);
    b.p.illisibles(1);
    b.appuis.clear();
    b.a.ordreLampe(true, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Lumiere) == 1);
    VERIF(b.p.hotte().lampe);
  }
  // Lampe inchangee au-dela de confirmation_ms, apres une trame illisible : nouvel essai.
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.pas(1000);
    b.p.sansEffet(1);
    b.p.illisibles(1);
    b.appuis.clear();
    b.a.ordreLampe(true, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Lumiere) == 2);
    VERIF(b.p.hotte().lampe);
  }
  // Changements et appuis seuls : la lecture annexe tranche pour la lampe.
  for (const Ligne &l : {chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.pas(1000);
    b.p.sansEffet(1);
    b.p.illisibles(1);
    b.appuis.clear();
    b.a.ordreLampe(true, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Lumiere) == 2);
    VERIF(b.p.hotte().lampe);
  }
  // "Marche" (armee vers eteinte) sans effet. Repete et changements : nouvel
  // essai, puis Ok. Appuis seuls : jamais de nouvel essai pour "marche".
  for (const Ligne &l : {rep, chg}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.appuis.clear();
    b.p.sansEffet(1);
    b.a.ordreVentilo(CibleVentilo::Eteint, 3, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Marche) == 2);
    b.pas(1000);
    VERIF(b.hotteEst(Marche::Eteinte, Moteur::Arret));
  }
  {
    // Appuis seuls, trame perdue (ni echo ni effet) : Echec, un seul appui.
    BancHotte b(ModeEtat::AppuisSeuls, true);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.appuis.clear();
    b.p.perdus(1);
    b.a.ordreVentilo(CibleVentilo::Eteint, 3, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Echec && f->cause == Cause::SansLecture && b.nbAppuis(Touche::Marche) == 1);
    // Echo vu, carte sourde : le voyant marche ne se lit pas ; la table
    // conclut a tort (ecart assume, note au plan). Jamais de nouvel essai.
    BancHotte c(ModeEtat::AppuisSeuls, true);
    c.demarrer();
    c.mener(Marche::Armee, Moteur::Arret);
    c.appuis.clear();
    c.p.sansEffet(1);
    c.a.ordreVentilo(CibleVentilo::Eteint, 3, Canal::App, c.t);
    f = c.attendreFin();
    VERIF(f && f->issue == Issue::Ok && c.nbAppuis(Touche::Marche) == 1);
  }
  // Collision sur V2 (arret) qui a pris, puis moteur arrete lu : etape
  // reussie, puis "marche".
  for (const Ligne &l : {rep, chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V2);
    b.appuis.clear();
    b.p.collisions(1, true);
    b.a.ordreVentilo(CibleVentilo::Eteint, 4, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::V2) == 1 && b.nbAppuis(Touche::Marche) == 1);
    VERIF(b.violations == 0);
  }
  // Collision sur "marche", aucun etat utile lu : changements (sans trame
  // d'etat) et appuis seuls : Echec, cause collision.
  for (const Ligne &l : {chg, app}) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.p.collisions(1, false);
    b.a.ordreVentilo(CibleVentilo::Eteint, 5, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Echec && f->cause == Cause::Collision);
  }
  // Collision, etat inchange lu au-dela du delai (repete) : nouvel essai.
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.appuis.clear();
    b.p.collisions(1, false);
    b.a.ordreVentilo(CibleVentilo::Eteint, 5, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && b.nbAppuis(Touche::Marche) == 2);
  }
  // Mise sous tension, hotte reellement en V2 (module redemarre seul), ordre
  // V3 : etat lu V2, puis V3, jamais "marche". Sans lecture annexe en mode
  // changements : Abandon au bout de attente_etat_ms, jamais "marche".
  for (const Ligne &l : kLignes) {
    BancHotte b(l.mode, l.annexe);
    sim::EtatHotte h;
    h.marche = Marche::Armee;
    h.moteur = Moteur::V2;
    b.p.poserHotte(h, b.t);
    b.demarrer(Demarrage::MiseSousTension);
    b.a.ordreVentilo(CibleVentilo::V3, 6, Canal::Matter, b.t);
    const Action *f = b.attendreFin();
    if (l.mode == ModeEtat::Changements && !l.annexe) {
      VERIF(f && f->issue == Issue::Abandon && f->cause == Cause::SansLecture);
      VERIF(b.appuis.empty());
    } else {
      VERIF(f && f->issue == Issue::Ok);
      VERIF(b.appuis.size() == 1 && b.appuis[0] == Touche::V3);
      VERIF(b.hotteEst(Marche::Armee, Moteur::V3));
    }
    VERIF(b.nbAppuis(Touche::Marche) == 0);
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
}

// La lampe : un appui si l'etat lu differe, rien sinon ; independante du ventilateur.
static void testLampe() {
  for (const Ligne &l : kLignes) {
    BancHotte b(l.mode, l.annexe);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V1, true);
    b.appuis.clear();
    b.a.ordreLampe(true, 1, Canal::App, b.t);  // deja allumee
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && f->appuis == 0 && b.appuis.empty());
    b.a.ordreLampe(false, 2, Canal::App, b.t);
    f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok && f->appuis == 1);
    b.pas(1000);
    VERIF(!b.p.hotte().lampe && b.hotteEst(Marche::Armee, Moteur::V1));
  }
}

// Un appui du panneau a chaque etape de chaque sequence : Annulee, etat
// publie tout de suite, plus aucun appui apres celui en vol.
static void testAnnulation() {
  for (const Ligne &l : kLignes)
    for (const Depart &d : kDeparts)
      for (const CibleVentilo c : kCibles) {
        for (size_t k = 0; k < 3; k++) {
          BancHotte b(l.mode, l.annexe);
          b.demarrer();
          b.mener(d.m, d.mo);
          b.appuis.clear();
          b.a.ordreVentilo(c, 9, Canal::App, b.t);
          for (uint32_t e = 0; e < 20000 && b.appuis.size() <= k && b.fins.empty(); e += 10) b.pas(10);
          if (b.appuis.size() <= k) break;  // sequence de moins de k + 1 appuis
          const size_t nPublis = b.publis.size();
          b.p.appuiPanneau(Touche::Lumiere, b.t);
          const Action *f = b.attendreFin();
          VERIF(f && f->issue == Issue::Annulee && f->idOrdre == 9);
          VERIF(b.publis.size() > nPublis);
          b.pas(8000);
          if (b.appuis.size() != k + 1) echec("appui apres annulation", l, d, c);
          VERIF(b.appuis.size() == k + 1);
          VERIF(b.violations == 0 && b.refusPilote == 0);
        }
      }
}

// Generateur pseudo-aleatoire deterministe (LCG de Numerical Recipes).
struct Alea {
  uint32_t x;
  uint32_t operator()(uint32_t n) {
    x = x * 1664525u + 1013904223u;
    return (x >> 8) % n;
  }
};

// Regle d'or, propriete : sur des suites generees (appuis du panneau, ordres
// de Maison et de l'app, pannes, attentes), jamais "marche" sans un arret du
// moteur lu depuis le dernier appui, ni moteur reel tournant ; et jamais un
// refus du pilote (l'automate attend lui-meme ses delais).
static void testRegleDor() {
  const Touche touches[] = {Touche::Marche, Touche::Lumiere, Touche::V1, Touche::V2, Touche::V3};
  const CibleVentilo cibles[] = {CibleVentilo::Eteint, CibleVentilo::V1, CibleVentilo::V2, CibleVentilo::V3,
                                 CibleVentilo::Derniere};
  uint32_t marches = 0;
  for (const Ligne &l : kLignes)
    for (uint32_t graine = 1; graine <= 12; graine++) {
      Params pa;
      pa.sondeVitesse = graine % 2;
      BancHotte b(l.mode, l.annexe, pa);
      Alea r{graine * 7919u + (uint32_t)l.mode * 104729u + (l.annexe ? 3u : 0u)};
      b.demarrer(graine % 3 ? Demarrage::MiseSousTension : Demarrage::Autre);
      for (int pasN = 0; pasN < 250; pasN++) {
        switch (r(10)) {
          case 0: b.p.appuiPanneau(touches[r(5)], b.t); break;
          case 1:
          case 2: b.a.ordreVentilo(cibles[r(5)], r(1000), r(2) ? Canal::App : Canal::Matter, b.t); break;
          case 3: b.a.ordreLampe(r(2), r(1000), Canal::Matter, b.t); break;
          case 4:
            switch (r(5)) {
              case 0: b.p.illisibles(1); break;
              case 1: b.p.sansEffet(1); break;
              case 2: b.p.collisions(1, r(2)); break;
              case 3: b.p.perdus(1); break;
              default: b.p.delais(1); break;
            }
            break;
          default: break;
        }
        b.pas(10 * (1 + r(400)));
      }
      b.pas(30000);
      marches += (uint32_t)b.nbAppuis(Touche::Marche);
      if (b.violations || b.refusPilote) printf("  -> ligne %s, graine %u\n", l.nom, graine);
      VERIF(b.violations == 0);
      VERIF(b.refusPilote == 0);
    }
  VERIF(marches > 50);  // la propriete a bien ete mise a l'epreuve
}

// Delais : delai moteur toutes origines, entre deux appuis, entrelacement
// de la lampe pendant l'attente du moteur, sequence_max_ms, attente_etat_ms.
static void testDelais() {
  {  // un appui du panneau change le moteur : l'ordre suivant attend delai_moteur_ms
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.panneau(Touche::V2);
    b.pas(300);  // la carte a reagi, l'etat est lu
    const uint32_t lu = b.t;
    b.appuis.clear();
    b.appuisT.clear();
    b.a.ordreVentilo(CibleVentilo::V3, 1, Canal::Matter, b.t);
    b.a.ordreLampe(true, 2, Canal::Matter, b.t);
    b.pas(6000);
    VERIF(b.appuis.size() == 2 && b.appuis[0] == Touche::Lumiere && b.appuis[1] == Touche::V3);
    VERIF(b.appuisT.size() == 2 && b.appuisT[1] - lu >= 2700);  // delai compte depuis l'etat lu (300 ms avant)
    VERIF(b.appuisT[1] - b.appuisT[0] >= 500 + 60);                // entre_appuis_ms apres la fin du premier
    VERIF(b.hotteEst(Marche::Armee, Moteur::V3) && b.p.hotte().lampe);
    VERIF(b.refusPilote == 0);
  }
  {  // attente_etat_ms : aucun etat utilisable, abandon au bout de 5 s
    BancHotte b(ModeEtat::Changements);
    b.demarrer();
    b.a.ordreVentilo(CibleVentilo::V2, 3, Canal::App, b.t);
    const uint32_t t0 = b.t;
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Abandon && f->cause == Cause::SansLecture);
    VERIF(b.t - t0 >= 5000 && b.t - t0 <= 5100);
    VERIF(!b.publis.empty());  // l'etat reel est republie
  }
  {  // sequence_max_ms : plus court que l'attente d'un etat
    Params pa;
    pa.attenteEtatMs = 60000;
    pa.sequenceMaxMs = 5000;
    BancHotte b(ModeEtat::Changements, false, pa);
    b.demarrer();
    b.a.ordreVentilo(CibleVentilo::V2, 3, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Abandon && f->cause == Cause::Duree && f->dureeMs >= 5000);
  }
  {  // le pilote plus strict que l'automate : Garde, Echec, etat republie
    Params pa;
    BancHotte b(ModeEtat::Repete, false, pa);
    sim::Reglages r = b.p.reglages();
    r.delaiMoteurMs = 20000;
    b.p.regler(r);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V1);
    const size_t nPublis = b.publis.size();
    b.a.ordreVentilo(CibleVentilo::V2, 4, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Echec && f->cause == Cause::Garde);
    VERIF(b.refusPilote == 1 && b.publis.size() > nPublis);
  }
}

// Remplacement, idempotence, et "derniere vitesse".
static void testOrdres() {
  {  // meme cible pendant sa propre sequence : rien ne repart
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::V2);
    b.appuis.clear();
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::App, b.t);
    b.pas(200);
    b.a.ordreVentilo(CibleVentilo::Eteint, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->idOrdre == 1 && f->issue == Issue::Remplacee);
    f = b.attendreFin();
    VERIF(f && f->idOrdre == 2 && f->issue == Issue::Ok);
    VERIF(b.appuis.size() == 2);  // V2 puis marche, une seule fois
  }
  {  // nouvelle cible : l'ancien ordre est remplace, la sequence repart de l'etat lu
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Armee, Moteur::Arret);
    b.a.ordreVentilo(CibleVentilo::V3, 1, Canal::Matter, b.t);
    b.pas(20);
    b.a.ordreVentilo(CibleVentilo::V1, 2, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->idOrdre == 1 && f->issue == Issue::Remplacee && f->canal == Canal::Matter);
    f = b.attendreFin();
    VERIF(f && f->idOrdre == 2 && f->issue == Issue::Ok);
    b.pas(500);
    VERIF(b.hotteEst(Marche::Armee, Moteur::V1));
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
  {  // derniere vitesse : V2 la toute premiere fois, puis le dernier palier atteint, toutes origines
    BancHotte b(ModeEtat::Repete);
    b.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::Arret);
    VERIF(b.a.derniereVitesse() == Moteur::V2);
    b.mener(Marche::Armee, Moteur::V3);
    VERIF(b.a.derniereVitesse() == Moteur::V3);
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::App, b.t);
    b.attendreFin();
    b.pas(4000);
    b.a.ordreVentilo(CibleVentilo::Derniere, 2, Canal::Matter, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Ok);
    b.pas(500);
    VERIF(b.hotteEst(Marche::Armee, Moteur::V3));
  }
}

// Sonde de vitesse (regle 7) : voyant marche inconnu, moteur arrete lu.
static void testSondeVitesse() {
  sim::EtatHotte eteinte;  // la verite : eteinte
  Etat nvs;                // ce que croit le module : armee (dernier etat publie)
  nvs.marche = Marche::Armee;
  nvs.moteur = Moteur::Arret;
  nvs.lampe = false;
  nvs.lampeConnue = true;
  for (uint32_t permise = 0; permise <= 1; permise++) {
    Params pa;
    pa.sondeVitesse = permise;
    BancHotte b(ModeEtat::Changements, true, pa);
    b.p.poserHotte(eteinte, b.t);
    b.demarrer(Demarrage::Autre, &nvs);
    b.pas(500);  // premiere lecture annexe : moteur arrete, voyant marche inconnu
    VERIF(b.a.etat().marche == Marche::Inconnue && b.a.etat().confiance == Confiance::Deduit);
    b.pas(3000);
    b.a.ordreVentilo(CibleVentilo::V2, 1, Canal::App, b.t);
    const Action *f = b.attendreFin();
    if (permise) {
      VERIF(f && f->issue == Issue::Ok);
      VERIF(b.appuis.size() == 3 && b.appuis[0] == Touche::V2 && b.appuis[1] == Touche::Marche &&
            b.appuis[2] == Touche::V2);
      bool sonde = false;
      for (const Action &e : b.evts) sonde |= e.evt == Evt::SondeVitesse;
      VERIF(sonde);
      b.pas(500);
      VERIF(b.hotteEst(Marche::Armee, Moteur::V2));
    } else {
      VERIF(f && f->issue == Issue::Abandon && b.appuis.empty());
    }
    VERIF(b.violations == 0 && b.refusPilote == 0);
  }
}

// Demarrage (5.7) : aucun appui injecte ; etat initial et premiere publication.
static void testDemarrage() {
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    VERIF(!b.publis.empty() && b.publis[0].etat.marche == Marche::Eteinte &&
          b.publis[0].etat.confiance == Confiance::Deduit);
    b.pas(10000);
    VERIF(b.appuis.empty());
    VERIF(b.a.etat().confiance == Confiance::Confirme);  // la repetition a confirme
  }
  {  // autre cause, sans etat en NVS : Inconnu, rien de publie jusqu'a la premiere lecture
    BancHotte b(ModeEtat::Changements);
    b.demarrer(Demarrage::Autre, nullptr);
    VERIF(b.publis.empty() && b.a.etat().confiance == Confiance::Inconnu);
    b.pas(10000);
    VERIF(b.publis.empty() && b.appuis.empty());
    b.panneau(Touche::Marche);
    b.pas(500);
    VERIF(!b.publis.empty() && b.a.etat().confiance == Confiance::Confirme);
  }
}

// Marche prolongee : la fin se lit (5.9) ; sans lecture au-dela de
// prolongee_max_ms, Presume, etat publie inchange.
static void testProlongee() {
  {
    BancHotte b(ModeEtat::Repete);
    b.demarrer();
    b.mener(Marche::Prolongee, Moteur::V2);
    b.pas(900000);
    b.pas(2000);
    VERIF(b.hotteEst(Marche::Eteinte, Moteur::Arret));
    VERIF(!b.publis.empty() && b.publis.back().etat.marche == Marche::Eteinte &&
          b.publis.back().etat.moteur == Moteur::Arret);
    VERIF(!b.changements.empty() && b.changements.back().origine == Origine::Inconnue);
  }
  {
    BancHotte b(ModeEtat::Changements);
    sim::Reglages r = b.p.reglages();
    r.prolongeeMs = 3600000;  // la hotte simulee ne finit pas : rien ne se lit
    b.p.regler(r);
    b.demarrer();
    b.mener(Marche::Prolongee, Moteur::V2);
    const size_t n = b.publis.size();
    b.pas(1200000);
    bool perimee = false;
    for (const Action &e : b.evts) perimee |= e.evt == Evt::ProlongeePerimee;
    VERIF(perimee && b.a.etat().confiance == Confiance::Presume);
    VERIF(b.publis.size() == n);  // etat publie inchange
    b.a.ordreVentilo(CibleVentilo::Eteint, 1, Canal::App, b.t);
    const Action *f = b.attendreFin();
    VERIF(f && f->issue == Issue::Abandon && b.appuis.empty());  // rien planifie sur un etat perime
  }
}

int main() {
  testMatrice();
  testCasImposes();
  testLampe();
  testAnnulation();
  testRegleDor();
  testDelais();
  testOrdres();
  testSondeVitesse();
  testDemarrage();
  testProlongee();
  return bilan("test_sequences");
}
