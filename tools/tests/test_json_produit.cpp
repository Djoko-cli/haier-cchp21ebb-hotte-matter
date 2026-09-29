// Tests hote des briques du protocole compagnon du produit
// (src/produit/json_out_produit.*) : messages du produit (hotte, sequence,
// alerte, trame_d, blocs d'etat et de compteurs) au caractere pres, pires cas
// sous le budget de 896 octets, reponse 'suite sequence', commande affichee,
// liste blanche a distance du produit (spec 7.5). Les briques communes
// (ecrivain, file, cache, cadence) sont copiees telles quelles de la sonde,
// et testees par test_json.cpp.
// Lancer : sh tools/tests/test_hote.sh
//
// Avec un chemin en argument, les messages realistes y sont ecrits (RS + JSON
// + LF), pour tools/json_check.py --profil produit.
#include <stdio.h>
#include <string.h>

#include <string>

#include "json_out_produit.h"
#include "verif.h"

using namespace jsonp;

static FILE *gCapture = nullptr;
static Writer gW;

static std::string finish(Writer &w, bool *ok = nullptr, bool realiste = true) {
  const bool good = w.finish();
  if (ok) *ok = good;
  std::string s((const char *)w.data(), w.size());
  if (good && realiste && gCapture) fwrite(s.data(), 1, s.size(), gCapture);
  return s;
}

static void expectLine(Writer &w, const char *json, const char *what) {
  bool ok = false;
  const std::string got = finish(w, &ok);
  const std::string want = std::string("\x1e") + json + "\n";
  if (!ok || got != want) printf("  %s :\n  obtenu  %s  attendu %s", what, got.c_str() + (got.empty() ? 0 : 1), want.c_str() + 1);
  VERIF(ok && got == want);
}

static hotte::Etat etat(hotte::Marche m, hotte::Moteur mo, bool lampe, bool connue = true) {
  hotte::Etat e;
  e.marche = m;
  e.moteur = mo;
  e.lampe = lampe;
  e.lampeConnue = connue;
  e.confiance = hotte::Confiance::Confirme;
  e.source = hotte::Source::Fil;
  e.luMs = 1000;
  return e;
}

static void testEvenements() {
  evtHotte(gW, 3, 1500, etat(hotte::Marche::Armee, hotte::Moteur::Arret, false),
           etat(hotte::Marche::Armee, hotte::Moteur::V2, true), hotte::Origine::Panneau);
  expectLine(gW,
             "{\"v\":1,\"t\":\"hotte\",\"n\":3,\"ms\":1500,\"avant\":{\"marche\":\"armee\",\"moteur\":0,\"lampe\":false},"
             "\"apres\":{\"marche\":\"armee\",\"moteur\":2,\"lampe\":true},\"origine\":\"panneau\",\"source\":\"fil\","
             "\"confiance\":\"confirme\"}",
             "evenement hotte");
  hotte::Action f;
  f.type = hotte::Action::FinSequence;
  f.sujet = hotte::Sujet::Ventilo;
  f.canal = hotte::Canal::App;
  f.issue = hotte::Issue::Echec;
  f.cause = hotte::Cause::NonConfirme;
  f.dureeMs = 2400;
  f.appuis = 2;
  evtSequence(gW, 4, 1600, f, 17);
  expectLine(gW,
             "{\"v\":1,\"t\":\"sequence\",\"n\":4,\"ms\":1600,\"id\":17,\"origine\":\"app\",\"sujet\":\"ventilo\","
             "\"issue\":\"echec\",\"cause\":\"non_confirme\",\"duree_ms\":2400,\"appuis\":2}",
             "evenement sequence de l'app");
  f.canal = hotte::Canal::Matter;
  f.sujet = hotte::Sujet::Lampe;
  f.issue = hotte::Issue::Ok;
  f.cause = hotte::Cause::Aucune;
  evtSequence(gW, 5, 1700, f, 0);
  expectLine(gW,
             "{\"v\":1,\"t\":\"sequence\",\"n\":5,\"ms\":1700,\"id\":null,\"origine\":\"matter\",\"sujet\":\"lampe\","
             "\"issue\":\"ok\",\"cause\":null,\"duree_ms\":2400,\"appuis\":2}",
             "evenement sequence de Maison");
  surv::Alerte a;
  a.sujet = surv::Sujet::Temperature;
  a.debut = true;
  a.valeur = 704;
  a.seuil = 700;
  evtAlerte(gW, 6, 1800, a);
  expectLine(gW,
             "{\"v\":1,\"t\":\"alerte\",\"n\":6,\"ms\":1800,\"sujet\":\"temperature\",\"etape\":\"debut\",\"valeur\":70,"
             "\"seuil\":70,\"unite\":\"c\"}",
             "alerte de temperature");
  a.sujet = surv::Sujet::LectureTemperature;
  evtAlerte(gW, 7, 1900, a);
  expectLine(gW,
             "{\"v\":1,\"t\":\"alerte\",\"n\":7,\"ms\":1900,\"sujet\":\"lecture_temperature\",\"etape\":\"debut\","
             "\"valeur\":null,\"seuil\":null,\"unite\":\"c\"}",
             "alerte de lecture");
  a.sujet = surv::Sujet::AlimModule;
  a.debut = false;
  a.valeur = 3801;
  a.seuil = 3700;
  evtAlerte(gW, 8, 2000, a);
  expectLine(gW,
             "{\"v\":1,\"t\":\"alerte\",\"n\":8,\"ms\":2000,\"sujet\":\"alim_module\",\"etape\":\"fin\",\"valeur\":3801,"
             "\"seuil\":3700,\"unite\":\"mv\"}",
             "fin d'alerte de tension");
  TrameD t;
  t.tMs = 1234;
  t.origine = OrigineTrame::Carte;
  t.n = 2;
  t.octets[0] = 0x1A;
  t.octets[1] = 0x05;
  evtTrameD(gW, 9, 2100, t, 0);
  expectLine(gW,
             "{\"v\":1,\"t\":\"trame_d\",\"n\":9,\"ms\":2100,\"t_ms\":1234,\"origine\":\"carte\",\"sens\":\"vers_panneau\","
             "\"octets\":\"1A05\"}",
             "trame decodee de la carte");
  t.origine = OrigineTrame::Module;
  evtTrameD(gW, 10, 2200, t, 3);
  expectLine(gW,
             "{\"v\":1,\"t\":\"trame_d\",\"n\":10,\"ms\":2200,\"t_ms\":1234,\"origine\":\"module\",\"sens\":\"vers_carte\","
             "\"octets\":\"1A05\",\"sautes\":3}",
             "trame decodee du module");
  VERIF(degres(415) == 42 && degres(414) == 41 && degres(-5) == -1 && degres(-4) == 0 && degres(0) == 0);
}

// Debut d'un bloc d'etat tel que json_mode_produit l'ecrit : bloc, boot, up_s.
static void blocEtat(const char *bloc, uint32_t n) {
  gW.begin("etat", n, 3000);
  gW.str("bloc", bloc);
  gW.hexU32("boot", 0x3FA2C901, 8);
  gW.u32("up_s", 12);
}

static void testBlocs() {
  blocEtat("hotte", 11);
  etatHotte(gW, etat(hotte::Marche::Prolongee, hotte::Moteur::V3, false, false), 3000);
  expectLine(gW,
             "{\"v\":1,\"t\":\"etat\",\"n\":11,\"ms\":3000,\"bloc\":\"hotte\",\"boot\":\"3FA2C901\",\"up_s\":12,\"marche\":\"prolongee\",\"moteur\":3,"
             "\"lampe\":null,\"confiance\":\"confirme\",\"source\":\"fil\",\"age_ms\":2000}",
             "bloc hotte");
  hotte::Diagnostic d;
  d.ventilo = true;
  d.cibleEteint = true;
  d.canalVentilo = hotte::Canal::App;
  d.enVol = true;
  d.touche = hotte::Touche::V2;
  d.essai = 1;
  d.volDepuisMs = 2500;
  d.delaiMoteurResteMs = 1200;
  blocEtat("automate", 12);
  etatAutomate(gW, d, hotte::ModeEtat::Changements, true, 3000);
  expectLine(gW,
             "{\"v\":1,\"t\":\"etat\",\"n\":12,\"ms\":3000,\"bloc\":\"automate\",\"boot\":\"3FA2C901\",\"up_s\":12,\"mode\":\"changements\",\"annexe\":true,"
             "\"pilote\":true,\"ventilo\":{\"cible\":\"eteint\",\"origine\":\"app\"},\"lampe\":null,"
             "\"appui\":{\"touche\":\"v2\",\"essai\":1,\"depuis_ms\":500},\"delai_moteur_ms\":1200,"
             "\"marche_autorisee\":false}",
             "bloc automate");
  surv::Params sp;
  sp.alimAlertes = 1;
  surv::Surveillance s(sp);
  s.maxPoseLu(612);
  s.temperature(true, 453, 0);
  s.tensions(5010, 4980, 4460, 4390);
  blocEtat("thermique", 13);
  etatThermique(gW, s);
  expectLine(gW,
             "{\"v\":1,\"t\":\"etat\",\"n\":13,\"ms\":3000,\"bloc\":\"thermique\",\"boot\":\"3FA2C901\",\"up_s\":12,\"temp_c\":45,\"temp_max_c\":45,"
             "\"temp_max_pose_c\":61,\"alerte\":false,\"alertes\":0,\"lectures_ratees\":0,\"seuil_c\":70}",
             "bloc thermique");
  blocEtat("alim", 14);
  etatAlim(gW, s);
  expectLine(gW,
             "{\"v\":1,\"t\":\"etat\",\"n\":14,\"ms\":3000,\"bloc\":\"alim\",\"boot\":\"3FA2C901\",\"up_s\":12,\"hotte\":{\"mv\":5010,\"min_s_mv\":4980,"
             "\"min_mv\":4980,\"alerte\":false},\"module\":{\"mv\":4460,\"min_s_mv\":4390,\"min_mv\":4390,"
             "\"alerte\":false},\"alertes_actives\":true}",
             "bloc alim");
  hotte::Compteurs c;
  c.appuisPanneau = 12;
  c.echecs[(int)hotte::Cause::Collision] = 1;
  gW.begin("compteurs", 15, 3000);
  gW.str("bloc", "hotte");
  compteursHotte(gW, c);
  expectLine(gW,
             "{\"v\":1,\"t\":\"compteurs\",\"n\":15,\"ms\":3000,\"bloc\":\"hotte\",\"appuis_panneau\":12,"
             "\"appuis_module\":0,\"reussies\":0,\"annulees\":0,\"remplacees\":0,\"abandons\":0,\"echecs\":"
             "{\"non_confirme\":0,\"sans_lecture\":0,\"collision\":1,\"garde\":0,\"pilote\":0,\"duree\":0},"
             "\"nouveaux_essais\":0,\"transitions_inconnues\":0,\"anomalies\":0,\"marche_inconnue\":0,"
             "\"prolongee_perimee\":0,\"lignes_muettes\":0,\"actions_perdues\":0}",
             "compteurs hotte");
  gW.begin("compteurs", 16, 3000);
  gW.str("bloc", "alim");
  compteursAlim(gW, s);
  expectLine(gW,
             "{\"v\":1,\"t\":\"compteurs\",\"n\":16,\"ms\":3000,\"bloc\":\"alim\",\"hotte_passages\":0,"
             "\"module_passages\":0}",
             "compteurs alim");
  MatterEtat m;
  m.demarre = m.misEnService = true;
  m.fabriques = 1;
  m.abonnements = 2;
  m.maxintS = 20;
  m.txConnu = true;
  m.txDbm = 20;
  blocEtat("matter", 17);
  etatMatter(gW, m);
  expectLine(gW,
             "{\"v\":1,\"t\":\"etat\",\"n\":17,\"ms\":3000,\"bloc\":\"matter\",\"boot\":\"3FA2C901\",\"up_s\":12,\"demarre\":true,\"mis_en_service\":true,"
             "\"fabriques\":1,\"abonnements\":2,\"ignore_ms\":0,\"maxint_s\":20,\"role_demarrage\":\"routeur\","
             "\"tx_dbm\":20,\"derniere\":true,\"ecritures\":0,\"reflets\":0,\"verrou_occupe\":0,\"ignores\":0}",
             "bloc matter");
  RadioCompteurs r;
  r.connu = true;
  r.role = "router";
  r.txConnu = true;
  r.txDbm = 20;
  r.sensibiliteDbm = -120;
  r.lien = true;
  r.rssiMoyenDbm = -71;
  r.lqIn = 3;
  r.lqOut = 3;
  r.txTotal = 812;
  r.txRetry = 20;
  gW.begin("compteurs", 18, 3000);
  gW.str("bloc", "radio");
  compteursRadio(gW, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"compteurs\",\"n\":18,\"ms\":3000,\"bloc\":\"radio\",\"role\":\"router\",\"tx_dbm\":20,"
             "\"sensibilite_dbm\":-120,\"lien\":{\"avec\":\"routeur\",\"rssi_moyen_dbm\":-71,\"lq_in\":3,\"lq_out\":3},"
             "\"tx_total\":812,\"tx_retry\":20,\"tx_echecs\":0,\"changements_parent\":0,\"changements_role\":0}",
             "compteurs radio");
  RadioCompteurs inconnu;
  gW.begin("compteurs", 19, 3000);
  gW.str("bloc", "radio");
  compteursRadio(gW, inconnu);
  expectLine(gW, "{\"v\":1,\"t\":\"compteurs\",\"n\":19,\"ms\":3000,\"bloc\":\"radio\",\"role\":null}", "radio inconnue");
}

// Pires cas : chaque message du produit, champs au maximum, sous le budget.
static void testPiresCas() {
  size_t pire = 0;
  auto noter = [&](const char *quoi) {
    bool ok = false;
    const std::string s = finish(gW, &ok, false);
    if (!ok || s.size() > kBudget) printf("  %s : %zu octets\n", quoi, s.size());
    VERIF(ok && s.size() <= kBudget);
    if (s.size() > pire) pire = s.size();
  };
  hotte::Diagnostic d;
  d.ventilo = d.lampe = d.enVol = true;
  d.idVentilo = d.idLampe = 999999999;
  d.volDepuisMs = 0;
  d.delaiMoteurResteMs = 4294967295u;
  gW.begin("etat", 4294967295u, 4294967295u);
  gW.str("bloc", "automate");
  gW.hexU32("boot", 0xFFFFFFFF, 8);
  gW.u32("up_s", 4294967295u);
  etatAutomate(gW, d, hotte::ModeEtat::AppuisSeuls, true, 4294967295u);
  noter("etat automate");
  hotte::Compteurs c;
  for (uint32_t &e : c.echecs) e = 4294967295u;
  c.appuisPanneau = c.appuisModule = c.reussies = c.annulees = c.remplacees = c.abandons = 4294967295u;
  c.nouveauxEssais = c.transitionsInconnues = c.anomalies = c.marcheInconnue = 4294967295u;
  c.prolongeePerimee = c.lignesMuettes = c.actionsPerdues = 4294967295u;
  gW.begin("compteurs", 4294967295u, 4294967295u);
  gW.str("bloc", "hotte");
  compteursHotte(gW, c);
  noter("compteurs hotte");
  RadioCompteurs r;
  r.connu = r.txConnu = r.lien = true;
  r.role = "detached";
  r.txTotal = r.txRetry = r.txEchecs = r.changementsParent = r.changementsRole = 4294967295u;
  r.rssiMoyenDbm = -128;
  r.sensibiliteDbm = -128;
  r.txDbm = -128;
  gW.begin("compteurs", 4294967295u, 4294967295u);
  gW.str("bloc", "radio");
  compteursRadio(gW, r);
  noter("compteurs radio");
  MatterEtat m;
  m.ignoreResteMs = m.ecritures = m.reflets = m.verrouOccupe = m.ignores = 4294967295u;
  m.fabriques = 255;
  m.abonnements = 65535;
  m.maxintS = 65535;
  gW.begin("etat", 4294967295u, 4294967295u);
  gW.str("bloc", "matter");
  etatMatter(gW, m);
  noter("etat matter");
  TrameD t;
  t.n = 8;
  gW.begin("trame_d", 4294967295u, 4294967295u);
  evtTrameD(gW, 4294967295u, 4294967295u, t, 4294967295u);
  noter("trame_d");
  printf("  pire message du produit : %zu octets (budget %zu)\n", pire, kBudget);
}

static void testReponse() {
  Reply r;
  r.id = 42;
  r.cmd = "hotte ventilo v2";
  r.code = "accepte";
  r.durMs = 1;
  r.suite = Reply::SuiteSequence;
  reply(gW, 20, 4000, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":20,\"ms\":4000,\"id\":42,\"etape\":\"fin\",\"cmd\":\"hotte ventilo v2\","
             "\"ok\":true,\"code\":\"accepte\",\"duree_ms\":1,\"suite\":\"sequence\"}",
             "reponse accepte, suite sequence");
  char shown[kCmdTextMax + 1];
  copyCmd(shown, "json cle nouvelle 0123456789abcdef");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "json cle nouvelle");
  copyCmd(shown, "hotte lampe on");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "hotte lampe on");
}

// Liste blanche a distance du produit (spec 7.5).
static void testDistance() {
  const char *permises[] = {
      "json 1",           "json 1 bail 30",    "json 0",         "json etat",       "json hello",
      "json ping",        "json trames 1",     "json log 0",     "json periode 2000", "json compteurs 10000",
      "json reseau 0",    "hotte ventilo v2",  "hotte ventilo off", "hotte ventilo derniere", "hotte lampe on",
      "hotte lampe off",  "reboot",            "matter tx 8",    "matter tx 20",
  };
  for (const char *c : permises) {
    if (remoteRefusal(c, false)) printf("  refusee a tort : %s\n", c);
    VERIF(remoteRefusal(c, false) == nullptr);
  }
  const char *interdites[] = {
      "json",          "json cle nouvelle", "json cle efface", "json 1 bail 0", "json periode 1000",
      "hotte",         "hotte regle confirmation_ms 900",    "hotte ventilo",   "simu appui v1",
      "matter",        "matter med 1",      "matter maxint 20", "matter derniere 0", "matter tx 7",
      "matter tx 21",  "matter tx",         "essai alim on",    "decommission",     "reboot maintenant",
      "injecte durees 750 750", "injection on", "capture on", "seuils 1", "wifi reseau mdp",
      "help",          "info",              "radio rafale 10",
  };
  for (const char *c : interdites) {
    if (!remoteRefusal(c, false)) printf("  permise a tort : %s\n", c);
    VERIF(remoteRefusal(c, false) != nullptr);
  }
  // 'radio rafale' : seulement avec le drapeau de l'essai de 24 h.
  VERIF(remoteRefusal("radio rafale 10", true) == nullptr);
  VERIF(remoteRefusal("radio rafale", true) != nullptr);
  VERIF(!strcmp(remoteRefusal("matter tx 25", false), "matter tx : 8..20 dBm a distance"));
}

int main(int argc, char **argv) {
  if (argc > 1) {
    gCapture = fopen(argv[1], "wb");
    if (!gCapture) {
      printf("impossible d'ecrire %s\n", argv[1]);
      return 2;
    }
  }
  testEvenements();
  testBlocs();
  testPiresCas();
  testReponse();
  testDistance();
  if (gCapture) fclose(gCapture);
  return bilan("test_json_produit");
}
