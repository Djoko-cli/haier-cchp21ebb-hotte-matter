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

int main() {
  testTable();
  testTextes();
  testParams();
  return bilan("test_hotte_etat");
}
