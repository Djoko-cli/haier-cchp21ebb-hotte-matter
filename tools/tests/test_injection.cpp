// Tests hote des garde-fous de l'injection (src/injection_regles.*) : bornes
// des parametres (reglage par nom, image lue en NVS), analyse de
// 'injecte durees ...', etat (montee, armement, desarmement seul apres
// arme_max_s, delai minimal), niveau attendu et front anormal pendant une
// emission, symboles RMT de l'etage d'injection ; liste blanche a distance
// pour la commande 'injection'.
// Lancer : sh tools/tests/test_hote.sh
#include <string.h>

#include <string>

#include "injection_regles.h"
#include "json_out.h"
#include "verif.h"

using namespace inj;

static void testParams() {
  const Params p;
  VERIF(p.basMaxUs == 3000 && p.totalMaxUs == 200000 && p.silenceMinUs == 20000);
  VERIF(p.attenteMaxMs == 1000 && p.delaiMinMs == 3000 && p.armeMaxS == 600 && p.tolUs == 20);
  VERIF(paramsValides(p));  // les defauts d'avant l'avenant sont dans les bornes
  // Noms : ceux des champs de config.injection, dans cet ordre.
  const char *noms[] = {"bas_max_us", "total_max_us", "silence_min_us", "attente_max_ms",
                        "delai_min_ms", "arme_max_s", "tol_us"};
  VERIF(kNbParams == 7);
  for (uint8_t i = 0; i < kNbParams; i++) VERIF_EGAL_STR(nomParam(i), noms[i]);
  VERIF(nomParam(kNbParams) == nullptr);
  uint32_t lo = 0, hi = 0;
  VERIF(bornesParam("delai_min_ms", &lo, &hi) && lo == 1000 && hi == 60000);
  VERIF(bornesParam("arme_max_s", &lo, &hi) && lo == 10 && hi == 600);  // jamais plus de 10 min (spec 8.3)
  VERIF(bornesParam("tol_us", &lo, &hi) && lo == 1 && hi == 500);
  // Borne haute de total_max_us egale au defaut : relu_us n'est jamais tronque (json_out.h).
  VERIF(bornesParam("total_max_us", &lo, &hi) && lo == 100 && hi == 200000);
  VERIF(!bornesParam("inconnu", &lo, &hi));
  // Chaque borne est atteinte, et pas au-dela.
  for (uint8_t i = 0; i < kNbParams; i++) {
    Params q;
    VERIF(bornesParam(nomParam(i), &lo, &hi));
    if (!strcmp(nomParam(i), "total_max_us")) q.basMaxUs = lo;  // bas_max_us <= total_max_us
    VERIF(reglerParam(&q, nomParam(i), lo) == Reglage::Ok);
    VERIF(reglerParam(&q, nomParam(i), hi) == Reglage::Ok);
    VERIF(reglerParam(&q, nomParam(i), lo - 1) == Reglage::HorsBornes);
    VERIF(reglerParam(&q, nomParam(i), hi + 1) == Reglage::HorsBornes);
  }
}

static void testReglerParam() {
  Params p;
  VERIF(reglerParam(&p, "tol_us", 30) == Reglage::Ok && p.tolUs == 30);
  VERIF(reglerParam(&p, "tol_us", 0) == Reglage::HorsBornes && p.tolUs == 30);  // inchange si refus
  VERIF(reglerParam(&p, "vitesse", 3) == Reglage::NomInconnu);
  VERIF(reglerParam(&p, "arme_max_s", 601) == Reglage::HorsBornes && p.armeMaxS == 600);
  VERIF(reglerParam(&p, "delai_min_ms", 999) == Reglage::HorsBornes && p.delaiMinMs == 3000);
  // Coherence : bas_max_us <= total_max_us.
  VERIF(reglerParam(&p, "total_max_us", 2000) == Reglage::HorsBornes && p.totalMaxUs == 200000);
  VERIF(reglerParam(&p, "bas_max_us", 2000) == Reglage::Ok);
  VERIF(reglerParam(&p, "total_max_us", 2000) == Reglage::Ok && p.totalMaxUs == 2000);
  VERIF(reglerParam(&p, "bas_max_us", 2001) == Reglage::HorsBornes && p.basMaxUs == 2000);
}

static void testChargerParams() {
  Params lu;
  lu.tolUs = 35;
  lu.delaiMinMs = 5000;
  Params out;
  VERIF(chargerParams(&lu, sizeof(lu), &out) && out.tolUs == 35 && out.delaiMinMs == 5000);
  // Image d'une autre taille (autre version du firmware) : valeurs par defaut.
  out.tolUs = 99;
  VERIF(!chargerParams(&lu, sizeof(lu) - 4, &out) && out.tolUs == 20 && out.delaiMinMs == 3000);
  VERIF(!chargerParams(nullptr, 0, &out) && out.tolUs == 20);
  // Valeur hors bornes en NVS : tout revient aux valeurs par defaut.
  Params mauvais;
  mauvais.basMaxUs = 50000;
  mauvais.tolUs = 40;
  VERIF(!chargerParams(&mauvais, sizeof(mauvais), &out) && out.basMaxUs == 3000 && out.tolUs == 20);
  mauvais = Params();
  mauvais.armeMaxS = 3600;
  VERIF(!chargerParams(&mauvais, sizeof(mauvais), &out) && out.armeMaxS == 600);
}

static void testRefusTexte() {
  VERIF_EGAL_STR(refusTexte(Refus::NonMontee), "non montee");
  VERIF_EGAL_STR(refusTexte(Refus::NonArmee), "non armee");
  VERIF_EGAL_STR(refusTexte(Refus::Delai), "delai minimal");
  VERIF_EGAL_STR(refusTexte(Refus::Syntaxe), "syntaxe");
  VERIF_EGAL_STR(refusTexte(Refus::BasTropLong), "duree basse trop longue");
  VERIF_EGAL_STR(refusTexte(Refus::TropLong), "trame trop longue");
  VERIF_EGAL_STR(refusTexte(Refus::TropDeDurees), "trop de durees");
  VERIF_EGAL_STR(refusTexte(Refus::Aucun), "aucun");
}

static void testAnalyser() {
  const Params p;
  Demande d;
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  VERIF(d.n == 4 && d.dur[0] == 750 && d.dur[1] == 750 && d.dur[2] == 750 && d.dur[3] == 2250);
  VERIF(totalUs(d) == 4500);
  VERIF(analyser("  durees   1500  750 ", p, &d) == Refus::Aucun && d.n == 2 && d.dur[0] == 1500 && d.dur[1] == 750);
  // Syntaxe : mot-cle, entiers stricts > 0, au moins une duree.
  Demande garde;
  garde.n = 7;
  VERIF(analyser("durees", p, &garde) == Refus::Syntaxe && garde.n == 7);  // *out inchange si refus
  VERIF(analyser("", p, &d) == Refus::Syntaxe);
  VERIF(analyser(nullptr, p, &d) == Refus::Syntaxe);
  VERIF(analyser("touche lumiere", p, &d) == Refus::Syntaxe);  // syntaxe nommee : avec l'avenant
  VERIF(analyser("dureesx 750", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 0 750", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 -5", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 +5", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 7a", p, &d) == Refus::Syntaxe);
  VERIF(analyser("durees 750 4294967296", p, &d) == Refus::Syntaxe);
  // Duree basse (rang pair) : bas_max_us au plus ; une duree haute peut depasser.
  VERIF(analyser("durees 3000 5000 3000", p, &d) == Refus::Aucun);
  VERIF(analyser("durees 3001", p, &d) == Refus::BasTropLong);
  VERIF(analyser("durees 750 750 3001", p, &d) == Refus::BasTropLong);
  // Somme : total_max_us au plus.
  VERIF(analyser("durees 1000 99000 1000 99000", p, &d) == Refus::Aucun && totalUs(d) == 200000);
  VERIF(analyser("durees 1000 99000 1000 99001", p, &d) == Refus::TropLong);
  // 64 durees au plus.
  std::string s = "durees";
  for (int i = 0; i < kDurMax; i++) s += " 1";
  VERIF(analyser(s.c_str(), p, &d) == Refus::Aucun && d.n == kDurMax);
  s += " 1";
  VERIF(analyser(s.c_str(), p, &d) == Refus::TropDeDurees);
  // Les bornes suivent les parametres en vigueur.
  Params q;
  q.basMaxUs = 500;
  VERIF(analyser("durees 750", q, &d) == Refus::BasTropLong);
}

static void testEtat() {
  Params p;
  Etat e;
  VERIF(!e.montee() && !e.armee(0, p) && e.resteS(0, p) == 0);
  VERIF(e.admettre(1000, p) == Refus::NonMontee);
  e.setMontee(true);
  VERIF(e.admettre(1000, p) == Refus::NonArmee);
  e.armer(1000);
  VERIF(e.armee(1000, p) && e.resteS(1000, p) == 600);
  VERIF(e.resteS(1000 + 599001, p) == 1);                           // arrondi au-dessus
  VERIF(e.armee(1000 + 599999, p) && !e.armee(1000 + 600000, p));   // desarmee seule apres 600 s
  VERIF(e.resteS(1000 + 600000, p) == 0);
  VERIF(e.admettre(1000 + 600000, p) == Refus::NonArmee);
  // Rearmee ; le delai minimal court depuis la derniere emission.
  e.armer(700000);
  VERIF(e.admettre(700000, p) == Refus::Aucun);  // aucune emission encore
  e.noterEmission(701000);
  VERIF(e.admettre(701000 + 2999, p) == Refus::Delai);
  VERIF(e.admettre(701000 + 3000, p) == Refus::Aucun);
  // Ordre des refus : montee, puis armee, puis delai.
  e.setMontee(false);
  VERIF(e.admettre(701500, p) == Refus::NonMontee);
  e.setMontee(true);
  e.desarmer();
  VERIF(e.admettre(701500, p) == Refus::NonArmee && !e.armee(701500, p));
  // arme_max_s regle plus court : desarme plus tot.
  p.armeMaxS = 30;
  e.armer(800000);
  VERIF(e.armee(800000 + 29999, p) && !e.armee(800000 + 30000, p));
  // millis() qui repasse par 0 (49,7 jours) : les ecarts restent justes.
  e.armer(0xFFFFFF00u);
  VERIF(e.armee(0x100u, p) && e.resteS(0x100u, p) == 30);
}

static void testNiveauEtFront() {
  const Params p;
  Demande d;
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  // Fronts programmes : 0 (vers le bas), 750, 1500, 2250, 4500 (fin).
  VERIF(!niveauAttendu(d, 0) && !niveauAttendu(d, 749));
  VERIF(niveauAttendu(d, 750) && niveauAttendu(d, 1499));
  VERIF(!niveauAttendu(d, 1500) && niveauAttendu(d, 2250) && niveauAttendu(d, 4499));
  VERIF(niveauAttendu(d, 4500) && niveauAttendu(d, 100000));  // apres la fin : bus relache, haut
  // Front pres d'un front programme (+-tol) : normal, quel que soit son sens.
  VERIF(!frontAnormal(d, 752, true, 20));
  VERIF(!frontAnormal(d, 730, true, 20) && !frontAnormal(d, 770, false, 20));
  VERIF(!frontAnormal(d, 4520, true, 20));
  // Loin d'un front programme : anormal seulement si le niveau differe de l'attendu.
  VERIF(frontAnormal(d, 1000, false, 20));   // on relache (haut attendu), quelqu'un tire la ligne
  VERIF(!frontAnormal(d, 1000, true, 20));
  VERIF(frontAnormal(d, 771, false, 20) && !frontAnormal(d, 770, false, 20));
  VERIF(frontAnormal(d, 1521, true, 20) && !frontAnormal(d, 1521, false, 20));
  VERIF(frontAnormal(d, 3000, false, 20));
  VERIF(frontAnormal(d, 5000, false, 20));   // apres la fin (la surveillance s'arrete avant)
  VERIF(!frontAnormal(d, 750, false, 0));    // tolerance nulle : exactement au front
  VERIF(frontAnormal(d, 751, false, 0));
  // Front programme a +-tol : 0, 750, 1500, 2250, 4500.
  VERIF(frontProgramme(d, 0, 20) && frontProgramme(d, 752, 20) && frontProgramme(d, 4515, 20));
  VERIF(!frontProgramme(d, 1100, 20) && !frontProgramme(d, 771, 20) && !frontProgramme(d, 4521, 20));
  // Nombre impair : la derniere duree est basse, le relachement final est un front programme.
  VERIF(analyser("durees 1500 750 750", p, &d) == Refus::Aucun);
  VERIF(!niveauAttendu(d, 2999) && niveauAttendu(d, 3000));
  VERIF(!frontAnormal(d, 3010, true, 20));
}

static void testSymboles() {
  Sym y[kSymMax] = {};
  const Params p;
  Demande d;
  // Etage inverseur : GPIO7 haut (1) = bus tire bas ; rang pair = bas.
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  VERIF(versSymboles(d, y, kSymMax) == 2);
  VERIF(y[0].d0 == 750 && y[0].l0 == 1 && y[0].d1 == 750 && y[0].l1 == 0);
  VERIF(y[1].d0 == 750 && y[1].l0 == 1 && y[1].d1 == 2250 && y[1].l1 == 0);
  // Nombre impair de demi-symboles : complete par 1 us GPIO bas (bus relache).
  VERIF(analyser("durees 1500 750 750", p, &d) == Refus::Aucun);
  VERIF(versSymboles(d, y, kSymMax) == 2);
  VERIF(y[0].d0 == 1500 && y[0].l0 == 1 && y[0].d1 == 750 && y[0].l1 == 0);
  VERIF(y[1].d0 == 750 && y[1].l0 == 1 && y[1].d1 == 1 && y[1].l1 == 0);
  // Duree haute de plus de 32 767 us : decoupee en demi-symboles de meme niveau.
  VERIF(analyser("durees 100 70000", p, &d) == Refus::Aucun);
  VERIF(versSymboles(d, y, kSymMax) == 2);
  VERIF(y[0].d0 == 100 && y[0].l0 == 1 && y[0].d1 == 32767 && y[0].l1 == 0);
  VERIF(y[1].d0 == 32767 && y[1].l0 == 0 && y[1].d1 == 4466 && y[1].l1 == 0);
  // Pire cas des bornes : 64 durees, somme de 200 000 us (borne haute de
  // total_max_us, le defaut) : 63 demi-symboles de 10 us, puis 199 370 us
  // haut en 7 demi-symboles, soit 35 symboles, sous kSymMax.
  std::string s = "durees";
  for (int i = 0; i < 63; i++) s += " 10";
  s += " 199370";
  VERIF(analyser(s.c_str(), p, &d) == Refus::Aucun && totalUs(d) == 200000);
  VERIF(versSymboles(d, y, kSymMax) == 35);
  // Capacite trop petite, demande vide : 0.
  VERIF(analyser("durees 750 750 750 2250", p, &d) == Refus::Aucun);
  VERIF(versSymboles(d, y, 1) == 0);
  VERIF(versSymboles(Demande(), y, kSymMax) == 0);
}

// Liste blanche a distance (jsonp::remoteRefusal) : 'injection on|off' et
// 'injecte ...' seulement ; 'injection regle' et 'injection monte' : USB.
static void testDistance() {
  VERIF(jsonp::remoteRefusal("injection on") == nullptr);
  VERIF(jsonp::remoteRefusal("injection off") == nullptr);
  VERIF(jsonp::remoteRefusal("injecte durees 750 750") == nullptr);
  VERIF(jsonp::remoteRefusal("injection regle tol_us 30") != nullptr);
  VERIF(jsonp::remoteRefusal("injection monte 1") != nullptr);
  VERIF(jsonp::remoteRefusal("injection") != nullptr);
}

int main() {
  testParams();
  testReglerParam();
  testChargerParams();
  testRefusTexte();
  testAnalyser();
  testEtat();
  testNiveauEtFront();
  testSymboles();
  testDistance();
  return bilan("test_injection");
}
