// Tests hote de l'automate (src/produit/hotte_etat.*) : table des
// transitions (5.2), reglages et leurs bornes (4.2), confiance (5.1), et les
// entrees une a une, sans pilote (les suites completes : test_sequences.cpp).
// Lancer : sh tools/tests/test_hote.sh
#include <string.h>

#include "hotte_etat.h"
#include "verif.h"

using namespace hotte;

static Etat etat(Marche m, Moteur mo, bool lampe = false, Confiance c = Confiance::Confirme) {
  Etat e;
  e.marche = m;
  e.moteur = mo;
  e.lampe = lampe;
  e.lampeConnue = true;
  e.confiance = c;
  e.source = Source::Fil;
  return e;
}

static bool donne(const Etat &avant, Touche t, Marche m, Moteur mo, bool observee) {
  const Transition tr = appliquer(avant, t);
  return tr.apres.marche == m && tr.apres.moteur == mo && tr.observee == observee &&
         tr.apres.lampe == avant.lampe;
}

// Chaque ligne du 2.2 ; les lignes "suppose identique en V1 et V2" codees observees (Q30).
static void testTable() {
  const Moteur vitesses[] = {Moteur::V1, Moteur::V2, Moteur::V3};
  VERIF(donne(etat(Marche::Eteinte, Moteur::Arret), Touche::Marche, Marche::Armee, Moteur::Arret, true));
  VERIF(donne(etat(Marche::Armee, Moteur::Arret), Touche::Marche, Marche::Eteinte, Moteur::Arret, true));
  for (const Moteur x : vitesses) {
    const Touche tx = toucheDe(x);
    VERIF(donne(etat(Marche::Eteinte, Moteur::Arret), tx, Marche::Eteinte, Moteur::Arret, true));  // rien
    VERIF(donne(etat(Marche::Armee, Moteur::Arret), tx, Marche::Armee, x, true));
    VERIF(donne(etat(Marche::Armee, x), tx, Marche::Armee, Moteur::Arret, true));       // vitesse active
    VERIF(donne(etat(Marche::Armee, x), Touche::Marche, Marche::Prolongee, x, true));   // prolongee
    VERIF(donne(etat(Marche::Prolongee, x), tx, Marche::Armee, Moteur::Arret, true));   // 31,2 s
    VERIF(!appliquer(etat(Marche::Prolongee, x), Touche::Marche).observee);             // Q2
    VERIF(donne(etat(Marche::Inconnue, x), tx, Marche::Armee, Moteur::Arret, true));    // armee ou prolongee
    for (const Moteur y : vitesses) {
      if (y == x) continue;
      VERIF(donne(etat(Marche::Armee, x), toucheDe(y), Marche::Armee, y, true));
      VERIF(!appliquer(etat(Marche::Prolongee, x), toucheDe(y)).observee);  // Q2
      VERIF(!appliquer(etat(Marche::Inconnue, x), toucheDe(y)).observee);
    }
  }
  VERIF(!appliquer(etat(Marche::Inconnue, Moteur::Arret), Touche::V2).observee);
  VERIF(!appliquer(etat(Marche::Inconnue, Moteur::Arret), Touche::Marche).observee);
  // La lampe bascule dans tout etat, independante de la marche.
  const Marche marches[] = {Marche::Eteinte, Marche::Armee, Marche::Prolongee, Marche::Inconnue};
  for (const Marche m : marches)
    for (int l = 0; l <= 1; l++) {
      const Etat a = etat(m, m == Marche::Prolongee ? Moteur::V2 : Moteur::Arret, l);
      const Transition tr = appliquer(a, Touche::Lumiere);
      VERIF(tr.observee && tr.apres.lampe == !l && tr.apres.marche == m && tr.apres.moteur == a.moteur);
    }
  // Lampe inconnue : elle le reste.
  Etat inconnue = etat(Marche::Armee, Moteur::Arret);
  inconnue.lampeConnue = false;
  VERIF(!appliquer(inconnue, Touche::Lumiere).apres.lampeConnue);
  VERIF(moteurDe(Touche::V2) == Moteur::V2 && moteurDe(Touche::Marche) == Moteur::Arret);
  VERIF(toucheDe(Moteur::V3) == Touche::V3);
}

static void testTextes() {
  VERIF_EGAL_STR(texte(Touche::Lumiere), "lumiere");
  VERIF_EGAL_STR(texte(Moteur::V3), "v3");
  VERIF_EGAL_STR(texte(Marche::Prolongee), "prolongee");
  VERIF_EGAL_STR(texte(Confiance::Presume), "presume");
  VERIF_EGAL_STR(texte(Source::Annexe), "annexe");
  VERIF_EGAL_STR(texte(Origine::Panneau), "panneau");
  VERIF_EGAL_STR(texte(ModeEtat::AppuisSeuls), "appuis");
  VERIF_EGAL_STR(texte(ResultatAppui::Collision), "collision");
}

static void testParams() {
  const Params p;
  VERIF(paramsValides(p));
  VERIF(p.delaiMoteurMs == 3000 && p.entreAppuisMs == 500 && p.confirmationMs == 1000 && p.fraicheurMs == 2000);
  VERIF(p.lissageCalmeMs == 700 && p.lissagePlafondMs == 3000 && p.ordreCalmeMs == 150);
  VERIF(p.sequenceMaxMs == 15000 && p.attenteEtatMs == 5000 && p.prolongeeMaxMs == 1200000);
  VERIF(p.ignoreDemarrageMs == 2000 && p.sondeVitesse == 0);
  const char *noms[] = {"delai_moteur_ms",  "entre_appuis_ms",  "confirmation_ms", "fraicheur_ms",
                        "lissage_calme_ms", "lissage_plafond_ms", "ordre_calme_ms", "sequence_max_ms",
                        "attente_etat_ms",  "prolongee_max_ms", "ignore_demarrage_ms", "sonde_vitesse"};
  VERIF(kNbParams == 12);
  for (uint8_t i = 0; i < kNbParams; i++) VERIF_EGAL_STR(nomParam(i), noms[i]);
  VERIF(nomParam(kNbParams) == nullptr);
  VERIF(valeurParam(p, 0) == 3000 && valeurParam(p, 11) == 0);
  uint32_t lo = 0, hi = 0;
  VERIF(bornesParam("delai_moteur_ms", &lo, &hi) && lo == 1000 && hi == 60000);
  VERIF(bornesParam("prolongee_max_ms", &lo, &hi) && lo == 900000 && hi == 3600000);
  VERIF(bornesParam("ignore_demarrage_ms", &lo, &hi) && lo == 0 && hi == 10000);
  VERIF(bornesParam("sonde_vitesse", &lo, &hi) && lo == 0 && hi == 1);
  VERIF(!bornesParam("inconnu", &lo, &hi));
  // Chaque borne est atteinte, et pas au-dela.
  for (uint8_t i = 0; i < kNbParams; i++) {
    Params q;
    VERIF(bornesParam(nomParam(i), &lo, &hi));
    if (!strcmp(nomParam(i), "lissage_plafond_ms")) q.lissageCalmeMs = 300;  // calme <= plafond
    if (!strcmp(nomParam(i), "lissage_calme_ms")) q.lissagePlafondMs = 10000;
    VERIF(reglerParam(&q, nomParam(i), lo) == Reglage::Ok);
    VERIF(reglerParam(&q, nomParam(i), hi) == Reglage::Ok);
    if (lo > 0) VERIF(reglerParam(&q, nomParam(i), lo - 1) == Reglage::HorsBornes);
    VERIF(reglerParam(&q, nomParam(i), hi + 1) == Reglage::HorsBornes);
  }
  Params q;
  VERIF(reglerParam(&q, "vitesse", 3) == Reglage::NomInconnu);
  VERIF(reglerParam(&q, "confirmation_ms", 199) == Reglage::HorsBornes && q.confirmationMs == 1000);
  // Coherence : lissage_calme_ms <= lissage_plafond_ms.
  VERIF(reglerParam(&q, "lissage_plafond_ms", 1000) == Reglage::Ok);
  VERIF(reglerParam(&q, "lissage_calme_ms", 1001) == Reglage::HorsBornes && q.lissageCalmeMs == 700);
  // Image NVS : taille et bornes, sinon tous les defauts.
  Params lu;
  q.delaiMoteurMs = 4000;
  VERIF(chargerParams(&q, sizeof(q), &lu) && lu.delaiMoteurMs == 4000);
  Params mauvais;
  mauvais.fraicheurMs = 100;
  VERIF(!chargerParams(&mauvais, sizeof(mauvais), &lu) && lu.fraicheurMs == 2000 && lu.delaiMoteurMs == 3000);
  VERIF(!chargerParams(&q, sizeof(q) - 1, &lu) && lu.delaiMoteurMs == 3000);
  VERIF(!chargerParams(nullptr, 0, &lu));
}

// Vide l'automate ; rend le nombre d'actions du type demande, *derniere recoit la derniere.
static int vider(Automate &a, uint32_t t, Action::Type type, Action *derniere = nullptr) {
  int n = 0;
  for (Action x = a.suivante(t); x.type != Action::Aucune; x = a.suivante(t))
    if (x.type == type) {
      n++;
      if (derniere) *derniere = x;
    }
  return n;
}

static void testConfiance() {
  {  // mise sous tension : eteinte, Deduit, publiee
    Automate a;
    a.configurerLigne(ModeEtat::Changements, false);
    a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::Arret, 1000);
    VERIF(a.etat().marche == Marche::Eteinte && a.etat().confiance == Confiance::Deduit && a.etat().lampeConnue);
    VERIF(a.derniereVitesse() == Moteur::V2);
    VERIF(vider(a, 1000, Action::Publier) == 1);
    // Etat complet lu : Confirme. Sans evenement, il le reste des heures
    // (mode changements) : un etat ne vieillit pas vers Inconnu.
    a.surEtatLu(etat(Marche::Armee, Moteur::Arret), 2000);
    VERIF(a.etat().confiance == Confiance::Confirme && a.diagnostic(2000).marcheAutorisee);
    VERIF(vider(a, 2000, Action::Changement) == 1);
    vider(a, 2000 + 3600000u, Action::Aucune);
    VERIF(a.etat().confiance == Confiance::Confirme);
    // Anomalie de reception : Presume, valeurs gardees, "marche" interdite.
    a.surAnomalie(1, 3700000);
    VERIF(a.etat().confiance == Confiance::Presume && a.etat().marche == Marche::Armee);
    VERIF(!a.diagnostic(3700000).marcheAutorisee && a.diagnostic(3700000).c.anomalies == 1);
  }
  {  // mode repete : ligne muette au-dela de fraicheur_ms
    Automate a;
    a.configurerLigne(ModeEtat::Repete, false);
    a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::V2, 0);
    a.surEtatLu(etat(Marche::Eteinte, Moteur::Arret), 100);
    vider(a, 100, Action::Aucune);
    VERIF(a.etat().confiance == Confiance::Confirme);
    VERIF(vider(a, 2099, Action::Evenement) == 0);
    Action e;
    VERIF(vider(a, 2100, Action::Evenement, &e) == 1 && e.evt == Evt::LigneMuette);
    VERIF(a.etat().confiance == Confiance::Presume && a.diagnostic(2100).c.lignesMuettes == 1);
    VERIF(vider(a, 5000, Action::Evenement) == 0);  // signalee une fois
    a.surEtatLu(etat(Marche::Eteinte, Moteur::Arret), 5000);
    VERIF(a.etat().confiance == Confiance::Confirme);
  }
  {  // autre cause : dernier etat publie relu en NVS, Presume ; sans NVS, Inconnu et rien de publie
    Automate a;
    a.configurerLigne(ModeEtat::Changements, false);
    const Etat nvs = etat(Marche::Armee, Moteur::V3, true);
    a.demarrer(Demarrage::Autre, &nvs, Moteur::V3, 0);
    VERIF(a.etat().confiance == Confiance::Presume && a.etat().source == Source::Nvs);
    VERIF(a.etat().moteur == Moteur::V3 && a.etat().lampe);
    Action pub;
    VERIF(vider(a, 0, Action::Publier, &pub) == 1 && pub.etat.moteur == Moteur::V3);
    Automate b;
    b.configurerLigne(ModeEtat::Changements, false);
    b.demarrer(Demarrage::Autre, nullptr, Moteur::V2, 0);
    VERIF(b.etat().confiance == Confiance::Inconnu && vider(b, 0, Action::Publier) == 0);
  }
  {  // lecture annexe : moteur et lampe lus, voyant marche deduit ou inconnu
    Automate a;
    a.configurerLigne(ModeEtat::AppuisSeuls, true);
    a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::V2, 0);
    Etat an;
    an.marche = Marche::Inconnue;
    an.moteur = Moteur::Arret;
    an.lampe = false;
    an.lampeConnue = true;
    an.source = Source::Annexe;
    a.surEtatLu(an, 100);
    VERIF(a.etat().marche == Marche::Eteinte && a.etat().confiance == Confiance::Deduit);
    VERIF(a.diagnostic(100).marcheAutorisee);
    an.moteur = Moteur::V2;  // moteur en marche, voyant deduit eteint : incoherent
    a.surEtatLu(an, 200);
    VERIF(a.etat().marche == Marche::Inconnue && a.etat().moteur == Moteur::V2);
  }
  {  // appui du panneau sur la vitesse active en V1 : Deduit (ligne supposee, codee observee, Q30)
    Automate a;
    a.configurerLigne(ModeEtat::Changements, false);
    a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::V2, 0);
    a.surEtatLu(etat(Marche::Armee, Moteur::V1), 100);
    vider(a, 100, Action::Aucune);
    a.surAppui(Touche::V1, Origine::Panneau, 200);
    VERIF(a.etat().moteur == Moteur::Arret && a.etat().confiance == Confiance::Deduit);
    Action c;
    VERIF(vider(a, 200, Action::Changement, &c) == 1 && c.origine == Origine::Panneau);
    VERIF(a.diagnostic(200).c.appuisPanneau == 1);
    // Transition inconnue (autre vitesse pendant la prolongation) : Presume.
    a.surEtatLu(etat(Marche::Prolongee, Moteur::V2), 300);
    a.surAppui(Touche::V3, Origine::Panneau, 400);
    VERIF(a.etat().confiance == Confiance::Presume && a.diagnostic(400).c.transitionsInconnues == 1);
  }
}

static void testTextesOrdres() {
  VERIF_EGAL_STR(texte(CibleVentilo::Derniere), "derniere");
  VERIF_EGAL_STR(texte(Issue::Remplacee), "remplacee");
  VERIF_EGAL_STR(texte(Cause::NonConfirme), "non_confirme");
  VERIF_EGAL_STR(texte(Cause::SansLecture), "sans_lecture");
  VERIF_EGAL_STR(texte(Canal::App), "app");
  VERIF_EGAL_STR(texte(Sujet::Lampe), "lampe");
  VERIF_EGAL_STR(texte(Evt::ProlongeePerimee), "prolongee_perimee");
}

// Ordre egal a l'etat : publication, aucun appui. Pilote hors service : Echec
// (cause pilote) si un appui etait en vol, puis attente et Abandon.
static void testOrdresUnitaires() {
  Automate a;
  a.configurerLigne(ModeEtat::Changements, false);
  a.demarrer(Demarrage::MiseSousTension, nullptr, Moteur::V2, 0);
  a.surEtatLu(etat(Marche::Armee, Moteur::V2), 100);
  vider(a, 100, Action::Aucune);
  a.ordreVentilo(CibleVentilo::V2, 5, Canal::App, 200);
  Action f;
  int appuis = 0, publis = 0, fins = 0;
  for (Action x = a.suivante(200); x.type != Action::Aucune; x = a.suivante(200)) {
    appuis += x.type == Action::Appuyer;
    publis += x.type == Action::Publier;
    if (x.type == Action::FinSequence) {
      fins++;
      f = x;
    }
  }
  VERIF(appuis == 0 && publis == 1 && fins == 1 && f.issue == Issue::Ok && f.idOrdre == 5 && f.appuis == 0);
  // Un appui en vol, puis le pilote rend Erreur.
  a.ordreLampe(true, 6, Canal::Matter, 5000);
  Action x = a.suivante(5000);
  VERIF(x.type == Action::Appuyer && x.touche == Touche::Lumiere);
  a.surFinAppui(x.idAppui, ResultatAppui::Erreur, 5060);
  VERIF(vider(a, 5060, Action::FinSequence, &f) == 1 && f.issue == Issue::Echec && f.cause == Cause::Pilote);
  VERIF(!a.diagnostic(5060).piloteEnService);
  a.ordreLampe(true, 7, Canal::Matter, 6000);
  VERIF(vider(a, 6000, Action::Appuyer) == 0);
  VERIF(vider(a, 10999, Action::FinSequence) == 0);
  VERIF(vider(a, 11000, Action::FinSequence, &f) == 1 && f.issue == Issue::Abandon && f.cause == Cause::Pilote);
  a.surPilote(true, 12000);
  a.ordreLampe(true, 8, Canal::Matter, 12000);
  VERIF(vider(a, 12000, Action::Appuyer) == 1);
}

int main() {
  testTable();
  testTextes();
  testParams();
  testTextesOrdres();
  testConfiance();
  testOrdresUnitaires();
  return bilan("test_hotte_etat");
}
