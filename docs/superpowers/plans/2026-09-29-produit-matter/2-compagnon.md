# Partie 2 : protocole compagnon du produit et outils du Mac

Plan : [2026-09-29-produit-matter.md](../2026-09-29-produit-matter.md) (contraintes globales, écarts à la spec).

Les briques pures du profil « hotte » du produit (copiées de la sonde puis
adaptées), le profil produit de `tools/json_check.py`, le document du
protocole, et `tools/hotte_udp.py`. Les copies passent par des scripts qui
échouent si un texte à remplacer manque ou se répète.

### Tâche 7 : Briques du protocole compagnon du produit

**Fichiers :**
- Créer : `src/produit/json_out_produit.h (copie adaptée de src/json_out.h, commit 1cb2c7c)`
- Créer : `src/produit/json_out_produit.cpp (copie adaptée de src/json_out.cpp, commit 1cb2c7c)`
- Créer : `tools/tests/test_json_produit.cpp`
- Modifier : `tools/tests/test_hote.sh`

**Interfaces :**
- Consomme : `hotte::Etat`, `Action`, `Diagnostic`, `Compteurs`, `texte(...)` ; `surv::Surveillance`, `Alerte`.
- Produit (namespace `jsonp`, copie de la sonde) : `Writer`, `Reply` (`Suite` : `SuiteNone`, `SuiteSequence`),
  `reply`, `ReplyCache`, `heartbeat`, `sessionEnd`, `logLine`, `parseIdPrefix`, `copyCmd`, `maskCmd`,
  `LineAssembler`, `RateCap`, `Cadence`, `Queue`, `Queued`, `kLateMs`, `leaseExpired`,
  `const char *remoteRefusal(const char *cmd, bool essai)` ;
  `enum class Item` du produit (`HelloBase`, `HelloId`, `Config`, `EtatHotte`, `EtatAutomate`, `EtatMatter`,
  `EtatThermique`, `EtatAlim`, `EtatSys`, `CompteursHotte`, `CompteursAlim`, `CompteursRadio`, `NetIp`,
  `Heartbeat`, `Reply`) ; messages `evtHotte`, `evtSequence(w, n, ms, fin, idCmd)`, `evtAlerte`, `evtTrameD` ;
  `OrigineTrame`, `struct TrameD` ; `int32_t degres(int32_t dixiemes)` ; blocs `etatHotte`, `etatAutomate`,
  `etatThermique`, `etatAlim`, `compteursHotte`, `compteursAlim`, `etatMatter` (`struct MatterEtat`),
  `compteursRadio` (`struct RadioCompteurs`).

- [ ] **Étape 1 : Écrire le test (messages au caractère près, pires cas, liste blanche du §7.5)**

```cpp
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
```

- [ ] **Étape 2 : Brancher le test**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
echo "tests hote : OK"
```

par :

```sh
$CXXP src/produit/hotte_etat.cpp src/produit/surveillance.cpp src/produit/json_out_produit.cpp tools/tests/test_json_produit.cpp -o "$OUT/test_json_produit"
"$OUT/test_json_produit" "$OUT/test_json_produit_lignes.txt"
echo "tests hote : OK"
```

- [ ] **Étape 3 : Lancer les tests : ils échouent**

```bash
sh tools/tests/test_hote.sh
```

Attendu : **échec**, avec :

```
clang++: error: no such file or directory: 'src/produit/json_out_produit.cpp'
```

- [ ] **Étape 4 : Copier les briques de la sonde et les adapter au produit**

Le script prend `src/json_out.{h,cpp}` au commit `1cb2c7c` (celui de la
sonde, inchangé depuis), vérifie que chaque texte remplacé s'y trouve une
seule fois, puis écrit `src/produit/json_out_produit.{h,cpp}` : plus de
capture ni d'injection, les messages et la liste blanche du produit. La sonde
n'est pas touchée (spec 4.7 : copie au besoin, extraction au sous-projet 3).

Depuis la racine du dépôt :

```bash
python3 - <<'EOF'
#!/usr/bin/env python3
"""Copie src/json_out.{h,cpp} de la sonde (commit 1cb2c7c) vers
src/produit/json_out_produit.{h,cpp}, puis l'adapte au profil du produit.
Chaque remplacement doit trouver son texte exactement une fois."""
import subprocess
import sys

DECL = r'''// ---------------------------------------------------------------------------
//  Messages du produit (docs/PROTOCOLE-JSON-PRODUIT.md)
// ---------------------------------------------------------------------------

// Evenement hotte : changement d'etat (avant, apres, origine, source).
void evtHotte(Writer &w, uint32_t n, uint32_t ms, const hotte::Etat &avant, const hotte::Etat &apres,
              hotte::Origine origine);
// Evenement sequence : fin d'une sequence (Action::FinSequence). idCmd : id de
// la commande de l'app, pour la session qui l'a envoyee ; 0 (null) sinon.
void evtSequence(Writer &w, uint32_t n, uint32_t ms, const hotte::Action &fin, uint32_t idCmd);
// Evenement alerte (surveillance) : temperature en degres entiers, tensions en mV.
void evtAlerte(Writer &w, uint32_t n, uint32_t ms, const surv::Alerte &a);

// Trame decodee du fil (diagnostic, 'json trames 1') : 1 a 8 octets. Nom
// distinct du 'trame' de la sonde, qui porte des durees brutes.
enum class OrigineTrame : uint8_t { Carte, Panneau, Module };
struct TrameD {
  uint32_t tMs = 0;
  OrigineTrame origine = OrigineTrame::Carte;
  uint8_t n = 0;
  uint8_t octets[8] = {};
};
void evtTrameD(Writer &w, uint32_t n, uint32_t ms, const TrameD &t, uint32_t sautes);

// Degres entiers les plus proches de d dixiemes (-5 -> -1, 415 -> 42).
int32_t degres(int32_t dixiemes);

// Champs des blocs, apres "bloc", "boot" et "up_s" (json_mode_produit.cpp).
void etatHotte(Writer &w, const hotte::Etat &e, uint32_t now);
void etatAutomate(Writer &w, const hotte::Diagnostic &d, hotte::ModeEtat mode, bool annexe, uint32_t now);
void etatThermique(Writer &w, const surv::Surveillance &s);
void etatAlim(Writer &w, const surv::Surveillance &s);
void compteursHotte(Writer &w, const hotte::Compteurs &c);
void compteursAlim(Writer &w, const surv::Surveillance &s);

// Bloc etat matter : valeurs lues par matter_hotte.cpp dans la tache loop.
struct MatterEtat {
  bool demarre = false, misEnService = false;
  uint8_t fabriques = 0;
  uint16_t abonnements = 0;
  uint32_t ignoreResteMs = 0;  // fenetre d'ignorance du demarrage restante
  uint16_t maxintS = 0;        // plafond des abonnements (0 : celui du controleur)
  uint8_t med = 0;             // role au prochain demarrage : 0 routeur, 1 MED
  bool txConnu = false;
  int8_t txDbm = 0;            // puissance d'emission en vigueur
  bool derniere = true;        // substitution de On par la derniere vitesse (3.3, regle 3)
  uint32_t ecritures = 0, reflets = 0, verrouOccupe = 0, ignores = 0;
};
void etatMatter(Writer &w, const MatterEtat &m);

// Bloc compteurs radio (7.4), lu sous le verrou OpenThread par matter_hotte.cpp.
struct RadioCompteurs {
  bool connu = false;
  const char *role = "inconnu";  // disabled, detached, child, router, leader
  bool txConnu = false;
  int8_t txDbm = 0;
  int8_t sensibiliteDbm = 0;
  bool lien = false;             // parent (MED), ou routeur voisin de meilleur lien (routeur)
  bool lienParent = false;
  int8_t rssiMoyenDbm = 0;
  uint8_t lqIn = 0, lqOut = 0;
  uint32_t txTotal = 0, txRetry = 0, txEchecs = 0;
  uint32_t changementsParent = 0, changementsRole = 0;
};
void compteursRadio(Writer &w, const RadioCompteurs &r);
'''

IMPL = r'''// ---------------------------------------------------------------------------
//  Messages du produit
// ---------------------------------------------------------------------------

int32_t degres(int32_t d) { return d >= 0 ? (d + 5) / 10 : -((-d + 5) / 10); }

static void lampe(Writer &w, const hotte::Etat &e) {
  if (e.lampeConnue) w.boolean("lampe", e.lampe);
  else w.null("lampe");
}

static void etatCourt(Writer &w, const char *k, const hotte::Etat &e) {
  w.obj(k);
  w.str("marche", hotte::texte(e.marche));
  w.u32("moteur", (uint32_t)e.moteur);
  lampe(w, e);
  w.end();
}

void evtHotte(Writer &w, uint32_t n, uint32_t ms, const hotte::Etat &avant, const hotte::Etat &apres,
              hotte::Origine origine) {
  w.begin("hotte", n, ms);
  etatCourt(w, "avant", avant);
  etatCourt(w, "apres", apres);
  w.str("origine", hotte::texte(origine));
  w.str("source", hotte::texte(apres.source));
  w.str("confiance", hotte::texte(apres.confiance));
}

void evtSequence(Writer &w, uint32_t n, uint32_t ms, const hotte::Action &f, uint32_t idCmd) {
  w.begin("sequence", n, ms);
  if (idCmd) w.u32("id", idCmd);
  else w.null("id");
  w.str("origine", hotte::texte(f.canal));
  w.str("sujet", hotte::texte(f.sujet));
  w.str("issue", hotte::texte(f.issue));
  if (f.cause == hotte::Cause::Aucune) w.null("cause");
  else w.str("cause", hotte::texte(f.cause));
  w.u32("duree_ms", f.dureeMs);
  w.u32("appuis", f.appuis);
}

void evtAlerte(Writer &w, uint32_t n, uint32_t ms, const surv::Alerte &a) {
  w.begin("alerte", n, ms);
  w.str("sujet", surv::texte(a.sujet));
  w.str("etape", a.debut ? "debut" : "fin");
  switch (a.sujet) {
    case surv::Sujet::Temperature:
      w.i32("valeur", degres(a.valeur));
      w.i32("seuil", degres(a.seuil));
      w.str("unite", "c");
      break;
    case surv::Sujet::LectureTemperature:  // debut : lecture en echec, pas de valeur
      if (a.debut) w.null("valeur");
      else w.i32("valeur", degres(a.valeur));
      w.null("seuil");
      w.str("unite", "c");
      break;
    default:
      w.u32("valeur", (uint32_t)a.valeur);
      w.u32("seuil", (uint32_t)a.seuil);
      w.str("unite", "mv");
      break;
  }
}

void evtTrameD(Writer &w, uint32_t n, uint32_t ms, const TrameD &t, uint32_t sautes) {
  static const char *const kOrigine[] = {"carte", "panneau", "module"};
  const uint8_t o = (uint8_t)t.origine < 3 ? (uint8_t)t.origine : 0;
  w.begin("trame_d", n, ms);
  w.u32("t_ms", t.tMs);
  w.str("origine", kOrigine[o]);
  w.str("sens", o == 0 ? "vers_panneau" : "vers_carte");
  w.hex("octets", t.octets, t.n > 8 ? 8 : t.n);
  if (sautes) w.u32("sautes", sautes);
}

void etatHotte(Writer &w, const hotte::Etat &e, uint32_t now) {
  w.str("marche", hotte::texte(e.marche));
  w.u32("moteur", (uint32_t)e.moteur);
  lampe(w, e);
  w.str("confiance", hotte::texte(e.confiance));
  w.str("source", hotte::texte(e.source));
  if (e.luMs) w.u32("age_ms", now - e.luMs);
  else w.null("age_ms");  // aucun etat lu depuis le demarrage
}

void etatAutomate(Writer &w, const hotte::Diagnostic &d, hotte::ModeEtat mode, bool annexe, uint32_t now) {
  w.str("mode", hotte::texte(mode));
  w.boolean("annexe", annexe);
  w.boolean("pilote", d.piloteEnService);
  if (d.ventilo) {
    w.obj("ventilo");
    w.str("cible", d.cibleEteint ? "eteint" : hotte::texte(d.cibleMoteur));
    w.str("origine", hotte::texte(d.canalVentilo));
    w.end();
  } else {
    w.null("ventilo");
  }
  if (d.lampe) {
    w.obj("lampe");
    w.boolean("cible", d.cibleLampe);
    w.str("origine", hotte::texte(d.canalLampe));
    w.end();
  } else {
    w.null("lampe");
  }
  if (d.enVol) {
    w.obj("appui");
    w.str("touche", hotte::texte(d.touche));
    w.u32("essai", d.essai);
    w.u32("depuis_ms", now - d.volDepuisMs);
    w.end();
  } else {
    w.null("appui");
  }
  w.u32("delai_moteur_ms", d.delaiMoteurResteMs);
  w.boolean("marche_autorisee", d.marcheAutorisee);
}

void etatThermique(Writer &w, const surv::Surveillance &s) {
  if (s.tempConnue()) w.i32("temp_c", degres(s.tempDixiemes()));
  else w.null("temp_c");
  if (s.tempMaxDixiemes() != surv::Surveillance::kAucun) w.i32("temp_max_c", degres(s.tempMaxDixiemes()));
  else w.null("temp_max_c");
  if (s.maxPose() != surv::Surveillance::kAucun) w.i32("temp_max_pose_c", degres(s.maxPose()));
  else w.null("temp_max_pose_c");
  w.boolean("alerte", s.alerteTemperature());
  w.u32("alertes", s.alertesTemperature());
  w.u32("lectures_ratees", s.lecturesRatees());
  w.u32("seuil_c", s.params().tempAlerteC);
}

static void voie(Writer &w, const char *k, const surv::Voie &v) {
  if (!v.connue) {
    w.null(k);
    return;
  }
  w.obj(k);
  w.u32("mv", v.actuelMv);
  w.u32("min_s_mv", v.minSecondeMv);
  w.u32("min_mv", v.minDemarrageMv);
  w.boolean("alerte", v.alerte);
  w.end();
}

void etatAlim(Writer &w, const surv::Surveillance &s) {
  voie(w, "hotte", s.hotte());
  voie(w, "module", s.module());
  w.boolean("alertes_actives", s.params().alimAlertes != 0);
}

void compteursHotte(Writer &w, const hotte::Compteurs &c) {
  w.u32("appuis_panneau", c.appuisPanneau);
  w.u32("appuis_module", c.appuisModule);
  w.u32("reussies", c.reussies);
  w.u32("annulees", c.annulees);
  w.u32("remplacees", c.remplacees);
  w.u32("abandons", c.abandons);
  w.obj("echecs");
  for (uint8_t i = 1; i < hotte::kNbCauses; i++) w.u32(hotte::texte((hotte::Cause)i), c.echecs[i]);
  w.end();
  w.u32("nouveaux_essais", c.nouveauxEssais);
  w.u32("transitions_inconnues", c.transitionsInconnues);
  w.u32("anomalies", c.anomalies);
  w.u32("marche_inconnue", c.marcheInconnue);
  w.u32("prolongee_perimee", c.prolongeePerimee);
  w.u32("lignes_muettes", c.lignesMuettes);
  w.u32("actions_perdues", c.actionsPerdues);
}

void compteursAlim(Writer &w, const surv::Surveillance &s) {
  w.u32("hotte_passages", s.hotte().passages);
  w.u32("module_passages", s.module().passages);
}

void etatMatter(Writer &w, const MatterEtat &m) {
  w.boolean("demarre", m.demarre);
  w.boolean("mis_en_service", m.misEnService);
  w.u32("fabriques", m.fabriques);
  w.u32("abonnements", m.abonnements);
  w.u32("ignore_ms", m.ignoreResteMs);
  w.u32("maxint_s", m.maxintS);
  w.str("role_demarrage", m.med ? "med" : "routeur");
  if (m.txConnu) w.i32("tx_dbm", m.txDbm);
  else w.null("tx_dbm");
  w.boolean("derniere", m.derniere);
  w.u32("ecritures", m.ecritures);
  w.u32("reflets", m.reflets);
  w.u32("verrou_occupe", m.verrouOccupe);
  w.u32("ignores", m.ignores);
}

void compteursRadio(Writer &w, const RadioCompteurs &r) {
  if (!r.connu) {  // OpenThread pas encore lu (verrou occupe, pile absente)
    w.null("role");
    return;
  }
  w.str("role", r.role);
  if (r.txConnu) w.i32("tx_dbm", r.txDbm);
  else w.null("tx_dbm");
  w.i32("sensibilite_dbm", r.sensibiliteDbm);
  if (r.lien) {
    w.obj("lien");
    w.str("avec", r.lienParent ? "parent" : "routeur");
    w.i32("rssi_moyen_dbm", r.rssiMoyenDbm);
    w.u32("lq_in", r.lqIn);
    w.u32("lq_out", r.lqOut);
    w.end();
  } else {
    w.null("lien");
  }
  w.u32("tx_total", r.txTotal);
  w.u32("tx_retry", r.txRetry);
  w.u32("tx_echecs", r.txEchecs);
  w.u32("changements_parent", r.changementsParent);
  w.u32("changements_role", r.changementsRole);
}

'''

REFUS = r'''const char *remoteRefusal(const char *cmd, bool essai) {
  static const char kDenied[] = "interdite a distance (spec 7.5) : USB seulement";
  size_t n0, n1, n2, n3, n4;
  const char *w0 = word(cmd, 0, &n0), *w1 = word(cmd, 1, &n1), *w2 = word(cmd, 2, &n2);
  const char *w3 = word(cmd, 3, &n3), *w4 = word(cmd, 4, &n4);
  uint32_t v = 0;
  if (wordIs(w0, n0, "json")) {
    if (wordIs(w1, n1, "1")) {
      // Jamais 'bail 0' a distance : le module emettrait pour un client parti
      // jusqu'a l'oubli de la session.
      if (wordIs(w2, n2, "bail") && wordNum(w3, n3, &v) && !w4 && (v < 10 || v > 120))
        return "json 1 : bail de 10 a 120 s a distance";
      return nullptr;
    }
    if (wordIs(w1, n1, "0") || wordIs(w1, n1, "etat") || wordIs(w1, n1, "hello") || wordIs(w1, n1, "ping") ||
        wordIs(w1, n1, "trames") || wordIs(w1, n1, "log"))
      return nullptr;
    if (wordIs(w1, n1, "periode"))
      return wordNum(w2, n2, &v) && v < 2000 ? "json periode : 2000..60000 ms a distance" : nullptr;
    if (wordIs(w1, n1, "compteurs"))
      return wordNum(w2, n2, &v) && v && v < 1000 ? "json compteurs : 0 ou 1000..60000 ms a distance" : nullptr;
    if (wordIs(w1, n1, "reseau"))
      return wordNum(w2, n2, &v) && v && v < 10000 ? "json reseau : 0 ou 10000..60000 ms a distance" : nullptr;
    return kDenied;  // 'json' seul (texte humain), 'json cle ...'
  }
  // Ordres a l'automate : la syntaxe est verifiee par l'aiguillage (usage).
  if (wordIs(w0, n0, "hotte") && (wordIs(w1, n1, "ventilo") || wordIs(w1, n1, "lampe")) && w2 && !w3)
    return nullptr;
  if (wordIs(w0, n0, "reboot") && !w1) return nullptr;  // Q27 : protege par la cle H1
  if (wordIs(w0, n0, "matter") && wordIs(w1, n1, "tx") && w2 && !w3)  // Q39
    return wordNum(w2, n2, &v) && v >= 8 && v <= 20 ? nullptr : "matter tx : 8..20 dBm a distance";
  if (wordIs(w0, n0, "radio") && wordIs(w1, n1, "rafale") && w2 && !w3)
    return essai ? nullptr : "radio rafale : seulement pendant l'essai de 24 h ('essai alim on' par l'USB)";
  return kDenied;  // hotte (texte), hotte regle, simu, matter (autres), essai, decommission, json cle...
}

'''


SRC = "1cb2c7c"


def show(chemin):
    return subprocess.run(["git", "show", f"{SRC}:{chemin}"], check=True, capture_output=True, text=True).stdout


def remplacer(texte, avant, apres, fichier):
    n = texte.count(avant)
    if n != 1:
        sys.exit(f"{fichier} : {n} occurrence(s) de {avant[:60]!r} (1 attendue)")
    return texte.replace(avant, apres)


# ---------------------------------------------------------------------------
#  En-tete
# ---------------------------------------------------------------------------
h = show("src/json_out.h")
R = [
    (
        "// Copie de benq-screenbar-halo-matter@c58a506 : src/json_out.h (adapte : scinde, sans halo1_*, u64, Item hotte, trame, injection, liste blanche hotte, masque wifi)\n",
        "// Copie de src/json_out.h de la sonde (commit 1cb2c7c), elle-meme copiee de\n"
        "// benq-screenbar-halo-matter@c58a506 (adapte : profil du produit, sans\n"
        "// capture ni injection ; Item, messages et liste blanche du produit)\n",
    ),
    (
        "//  Protocole compagnon v1, profil hotte (docs/PROTOCOLE-JSON.md) : briques pures\n",
        "//  Protocole compagnon v1, profil hotte du produit (docs/PROTOCOLE-JSON-PRODUIT.md) :\n"
        "//  briques pures\n",
    ),
    (
        "//  - Messages construits depuis des donnees simples : evenements trame et\n"
        "//    injection de la sonde, reponse, battement, fin, log.\n",
        "//  - Messages construits depuis des donnees simples : evenements hotte,\n"
        "//    sequence, alerte et trame_d du produit, blocs de ses etats et\n"
        "//    compteurs, reponse, battement, fin, log.\n",
    ),
    (
        "//  Pur et sans Arduino : teste sur l'hote (tools/tests/test_json.cpp). La\n"
        "//  session, l'ecriture sur le port serie et les instantanes sont dans\n"
        "//  json_mode.cpp.\n",
        "//  Pur et sans Arduino : teste sur l'hote (tools/tests/test_json_produit.cpp).\n"
        "//  La session, l'ecriture sur le port serie et les instantanes sont dans\n"
        "//  json_mode_produit.cpp.\n",
    ),
    ('#include "capture_model.h"\n', '#include "hotte_etat.h"\n#include "surveillance.h"\n'),
    ("constexpr size_t kCmdTextMax = 40;      // reponse.cmd, injection.cmd\n", "constexpr size_t kCmdTextMax = 40;      // reponse.cmd\n"),
    ("constexpr uint16_t kInjDurMax = 64;     // injection.dur_us : durees ecrites au plus (inj::kDurMax)\n", ""),
]
for a, b in R:
    h = remplacer(h, a, b, "json_out_produit.h")

debut = h.index("// Evenement trame (profil hotte).")
fin = h.index("void injection(Writer &w, uint32_t n, uint32_t ms, const InjectionEv &e);\n") + len(
    "void injection(Writer &w, uint32_t n, uint32_t ms, const InjectionEv &e);\n"
)
h = h[:debut] + DECL + h[fin:]

R = [
    (
        "  enum Suite : uint8_t { SuiteNone, SuiteInjection };  // absent, injection\n",
        "  enum Suite : uint8_t { SuiteNone, SuiteSequence };  // absent, sequence (evenement a suivre)\n",
    ),
    (
        "// Commande permise sur le transport reseau (liste blanche du profil hotte) ?\n"
        "// nullptr : oui ; sinon le msg de la reponse 'interdite'. cmd : la commande\n"
        "// sans le prefixe id=. Seules les bornes propres au reseau sont verifiees ici\n"
        "// (bail 10..120, periodes minimales) ; le reste des arguments l'est par\n"
        "// l'aiguillage (usage).\n"
        "const char *remoteRefusal(const char *cmd);\n",
        "// Commande permise sur le transport reseau (liste blanche du produit, spec\n"
        "// 7.5) ? nullptr : oui ; sinon le msg de la reponse 'interdite'. cmd : la\n"
        "// commande sans le prefixe id=. essai : drapeau NVS de l'essai de 24 h\n"
        "// ('essai alim on', USB), qui seul autorise 'radio rafale'. Seules les\n"
        "// bornes propres au reseau sont verifiees ici (bail 10..120, periodes\n"
        "// minimales, matter tx 8..20) ; le reste des arguments l'est par\n"
        "// l'aiguillage (usage).\n"
        "const char *remoteRefusal(const char *cmd, bool essai);\n",
    ),
    (
        "// reponse.cmd ne renvoie jamais un secret : 'json cle nouvelle <64 hexa>'\n"
        "// (alea de l'app) devient 'json cle nouvelle', et 'wifi <ssid> <mdp>' devient\n"
        "// 'wifi <ssid>' (le mot de passe, meme tronque, ne part jamais).\n",
        "// reponse.cmd ne renvoie jamais un secret : 'json cle nouvelle <64 hexa>'\n"
        "// (alea de l'app) devient 'json cle nouvelle'.\n",
    ),
    (
        "enum class Item : uint8_t {\n"
        "  HelloBase, HelloId, Config, EtatBus, EtatCapture, EtatInjection, EtatSys, Compteurs, NetIp, Heartbeat, Reply\n"
        "};\n",
        "enum class Item : uint8_t {\n"
        "  HelloBase, HelloId, Config, EtatHotte, EtatAutomate, EtatMatter, EtatThermique, EtatAlim, EtatSys,\n"
        "  CompteursHotte, CompteursAlim, CompteursRadio, NetIp, Heartbeat, Reply\n"
        "};\n",
    ),
    ("  static constexpr uint8_t kN = 24;\n", "  static constexpr uint8_t kN = 28;\n"),
]
for a, b in R:
    h = remplacer(h, a, b, "json_out_produit.h")
open("src/produit/json_out_produit.h", "w").write(h)

# ---------------------------------------------------------------------------
#  Source
# ---------------------------------------------------------------------------
c = show("src/json_out.cpp")
R = [
    (
        "// Copie de benq-screenbar-halo-matter@c58a506 : src/json_out.cpp (adapte : scinde, sans halo1_*, u64, trame, injection, liste blanche hotte, masque wifi, sections reordonnees)\n"
        '#include "json_out.h"\n',
        "// Copie de src/json_out.cpp de la sonde (commit 1cb2c7c), elle-meme copiee de\n"
        "// benq-screenbar-halo-matter@c58a506 (adapte : profil du produit, sans\n"
        "// capture ni injection ; messages et liste blanche du produit)\n"
        '#include "json_out_produit.h"\n',
    ),
    ("  if (r.suite == Reply::SuiteInjection) w.str(\"suite\", \"injection\");\n", "  if (r.suite == Reply::SuiteSequence) w.str(\"suite\", \"sequence\");\n"),
    (
        '  if (wordIs(w0, n0, "json") && wordIs(w1, n1, "cle") && wordIs(w2, n2, "nouvelle")) strcpy(shown, "json cle nouvelle");\n'
        "  // 'wifi <ssid> <mdp>' : coupe apres le ssid (reponse.cmd, cache des reponses).\n"
        "  if (wordIs(w0, n0, \"wifi\") && w1) shown[(size_t)(w1 - shown) + n1] = 0;\n",
        '  if (wordIs(w0, n0, "json") && wordIs(w1, n1, "cle") && wordIs(w2, n2, "nouvelle")) strcpy(shown, "json cle nouvelle");\n',
    ),
]
for a, b in R:
    c = remplacer(c, a, b, "json_out_produit.cpp")

# trame() et injection() : remplaces par les messages du produit.
debut = c.index("void trame(Writer &w, uint32_t n, uint32_t ms, const capt::Partie &p, bool hasRep, uint32_t rep, uint32_t sautes) {\n")
fin = c.index("void reply(Writer &w, uint32_t n, uint32_t ms, const Reply &r) {\n")
c = c[:debut] + IMPL + c[fin:]

# Liste blanche : celle du produit.
debut = c.index("const char *remoteRefusal(const char *cmd) {\n")
fin = c.index("// ===========================================================================\n//  Lignes de l'hote : prefixe id=, assemblage\n")
c = c[:debut] + REFUS + c[fin:]
open("src/produit/json_out_produit.cpp", "w").write(c)
print("json_out_produit.{h,cpp} ecrits")
EOF
```

Sortie attendue :

```
json_out_produit.{h,cpp} ecrits
```

- [ ] **Étape 5 : Lancer les tests : ils passent**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
  pire message du produit : 564 octets (budget 896)
test_json_produit : 75 verifications, 0 echecs
tests hote : OK
```

- [ ] **Étape 6 : Committer**

```bash
git add src/produit/json_out_produit.h src/produit/json_out_produit.cpp tools/tests/test_json_produit.cpp tools/tests/test_hote.sh
git commit -m "$(cat <<'FIN'
Ajouter les briques du protocole compagnon du produit

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 8 : Profil produit de `json_check.py` et document du protocole

**Fichiers :**
- Modifier : `tools/json_check.py`
- Créer : `tools/tests/test_json_check_produit.py`
- Créer : `docs/PROTOCOLE-JSON-PRODUIT.md`
- Modifier : `tools/tests/test_hote.sh`

**Interfaces :**
- Produit : `json_check.SCHEMAS_PRODUIT`, `PROFILS`, `AUTOMATE_BORNES`, `SURVEILLANCE_BORNES`,
  `check_line(raw, profil="sonde")`, `coherence_produit`, option `--profil sonde|produit` (défaut `sonde` :
  la sonde ne change pas) ; `docs/PROTOCOLE-JSON-PRODUIT.md` (profil par différence, 29 exemples vérifiés).

- [ ] **Étape 1 : Écrire le test du profil produit**

```python
#!/usr/bin/env python3
"""Tests du profil produit de tools/json_check.py (spec produit 7, 9.1) :
python3 -m unittest discover -s tools/tests -p test_json_check_produit.py -v"""
import contextlib
import copy
import io
import json
import os
import re
import sys
import unittest

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ICI, ".."))
import json_check  # noqa: E402

RACINE = os.path.join(ICI, "..", "..")
PROTOCOLE = os.path.join(RACINE, "docs", "PROTOCOLE-JSON-PRODUIT.md")


def msg(t, bloc=None, **champs):
    d = {"v": 1, "t": t, "n": 1, "ms": 10}
    if bloc:
        d["bloc"] = bloc
    d.update(champs)
    return d


BOOT = {"boot": "3FA2C901", "up_s": 12}
AUTOMATE = {k: lo for k, (lo, _) in json_check.AUTOMATE_BORNES.items()}
AUTOMATE.update(delai_moteur_ms=3000, lissage_calme_ms=700, lissage_plafond_ms=3000)
SURV = {k: lo for k, (lo, _) in json_check.SURVEILLANCE_BORNES.items()}
SURV.update(temp_alerte_c=70, temp_hyst_c=5)

# Un message valide de chaque (t, bloc) du profil produit, dans l'ordre des champs du firmware.
VALIDES = {
    ("hello", "base"): msg(
        "hello", "base", rev=4, fw="0.1.0-1a2b3c4", fw_desc="0.1.0-1a2b3c4", date="Sep 29 2026", heure="20:02:11",
        env="produit", build="produit", reseau_build="thread", puce="esp32c6", idf="v5.5.5", arduino="3.3.12",
        boot="3FA2C901", reset="mise_sous_tension", reset_n=1, up_s=12,
        session={"transport": "udp", "periode_ms": 2000, "compteurs_ms": 0, "reseau_ms": 30000, "bail_s": 30,
                 "trames": False, "log": False},
        limites={"ligne_max": 1024, "cmd_max": 127}),
    ("hello", "identite"): msg(
        "hello", "identite", boot="3FA2C901", mac="F0F5BD012345",
        id={"fabricant": "Djoko-CLI", "produit": "Module hotte Haier", "serie": "HOTTE-F0F5BD012345", "nom": "Hotte",
            "hw": 1, "hw_txt": "C6 SuperMini, pilote simule"},
        appareil="hotte", caps=["hotte", "matter", "udp", "cle", "log", "trames_d", "alim", "thermique", "essai", "simule"],
        thermique={"temp_max_pose_c": None}),
    ("config", None): msg(
        "config", automate=AUTOMATE, surveillance=SURV, ligne={"pilote": "simule", "mode": "repete", "annexe": False},
        simu={"latence_ms": 150, "periode_ms": 500, "trame_ms": 60, "annexe_ms": 200, "prolongee_ms": 900000,
              "fin": "eteinte", "inconnues": "rien", "refuse": False, "hors_service": False}),
    ("etat", "hotte"): msg("etat", "hotte", **BOOT, marche="armee", moteur=2, lampe=True, confiance="confirme",
                           source="fil", age_ms=120),
    ("etat", "automate"): msg("etat", "automate", **BOOT, mode="repete", annexe=False, pilote=True,
                              ventilo={"cible": "eteint", "origine": "app"}, lampe=None,
                              appui={"touche": "v2", "essai": 0, "depuis_ms": 40}, delai_moteur_ms=0,
                              marche_autorisee=False),
    ("etat", "matter"): msg("etat", "matter", **BOOT, demarre=True, mis_en_service=True, fabriques=1, abonnements=2,
                            ignore_ms=0, maxint_s=20, role_demarrage="routeur", tx_dbm=20, derniere=True, ecritures=14,
                            reflets=9, verrou_occupe=0, ignores=0),
    ("etat", "thermique"): msg("etat", "thermique", **BOOT, temp_c=45, temp_max_c=46, temp_max_pose_c=61,
                               alerte=False, alertes=0, lectures_ratees=0, seuil_c=70),
    ("etat", "alim"): msg("etat", "alim", **BOOT, hotte=None, module={"mv": 4460, "min_s_mv": 4390, "min_mv": 4380,
                                                                       "alerte": False}, alertes_actives=False),
    ("etat", "sys"): msg("etat", "sys", **BOOT, sys={"heap": 120000, "heap_min": 90000, "heap_bloc": 40000,
                                                     "pile_boucle": 3100, "boucle_max_ms": 12, "json_perdus": 0,
                                                     "json_trop_longs": 0, "rejets": 0}),
    ("compteurs", "hotte"): msg(
        "compteurs", "hotte", appuis_panneau=12, appuis_module=30, reussies=14, annulees=1, remplacees=2, abandons=0,
        echecs={"non_confirme": 0, "sans_lecture": 0, "collision": 1, "garde": 0, "pilote": 0, "duree": 0},
        nouveaux_essais=1, transitions_inconnues=0, anomalies=0, marche_inconnue=0, prolongee_perimee=0,
        lignes_muettes=0, actions_perdues=0),
    ("compteurs", "alim"): msg("compteurs", "alim", hotte_passages=0, module_passages=0),
    ("compteurs", "radio"): msg("compteurs", "radio", role="router", tx_dbm=20, sensibilite_dbm=-120,
                                lien={"avec": "routeur", "rssi_moyen_dbm": -71, "lq_in": 3, "lq_out": 3},
                                tx_total=812, tx_retry=20, tx_echecs=0, changements_parent=0, changements_role=2),
    ("hb", None): msg("hb", boot="3FA2C901", up_s=12, json_perdus=0),
    ("fin", None): msg("fin", cause="bail"),
    ("reponse", None): msg("reponse", id=42, etape="fin", cmd="hotte ventilo v2", ok=True, code="accepte", duree_ms=1,
                           suite="sequence"),
    ("log", None): msg("log", src="hotte", niv="notice", txt="[hotte] sequence ventilo : ok (1 appuis, 820 ms)"),
    ("reseau", "ip"): msg(
        "reseau", "ip", frais_ms=1200, srp={"nom": "ESP-HOTTE-012345"},
        adresses=[{"adr": "fd11:22::1a2b:3c4d:5e6f:7081", "type": "omr", "pref": True}],
        udp={"port": 5480, "ouvert": True, "empreinte": "630DCD29", "sessions": 1, "provisoire": False, "rx": 12,
             "rejets": 0, "rx_perdus": 0, "defis": 1, "tx": 230, "tx_perdus": 0, "tx_erreurs": 0, "tampons_libres": 51,
             "tampons_min": 38, "rafale_tx": 0}),
    ("hotte", None): msg("hotte", avant={"marche": "armee", "moteur": 0, "lampe": False},
                         apres={"marche": "armee", "moteur": 2, "lampe": False}, origine="panneau", source="fil",
                         confiance="confirme"),
    ("sequence", None): msg("sequence", id=42, origine="app", sujet="ventilo", issue="ok", cause=None, duree_ms=820,
                            appuis=1),
    ("alerte", None): msg("alerte", sujet="temperature", etape="debut", valeur=71, seuil=70, unite="c"),
    ("trame_d", None): msg("trame_d", t_ms=1234, origine="carte", sens="vers_panneau", octets="18"),
}


def verifier(obj):
    brut = b"\x1e" + json.dumps(obj, separators=(",", ":")).encode("ascii")
    _, errs, warns = json_check.check_line(brut, "produit")
    return errs, warns


def lancer(*argv):
    sortie = io.StringIO()
    with contextlib.redirect_stdout(sortie):
        code = json_check.main(list(argv))
    return code, sortie.getvalue()


class SchemasProduit(unittest.TestCase):
    def test_messages_valides(self):
        self.assertEqual(set(VALIDES), set(json_check.SCHEMAS_PRODUIT))
        for cle, obj in VALIDES.items():
            errs, warns = verifier(obj)
            self.assertEqual((errs, warns), ([], []), cle)

    def test_profil_sonde_ignore_le_produit(self):
        # Sous le profil de la sonde, un bloc du produit est inconnu.
        brut = b"\x1e" + json.dumps(VALIDES[("etat", "hotte")], separators=(",", ":")).encode()
        _, errs, _ = json_check.check_line(brut)
        self.assertTrue(any("bloc 'hotte' inconnu" in e for e in errs), errs)

    def test_build_et_caps(self):
        o = copy.deepcopy(VALIDES[("hello", "base")])
        o["build"] = "sonde"
        self.assertTrue(verifier(o)[0])
        o = copy.deepcopy(VALIDES[("hello", "identite")])
        o["caps"].append("injection")  # jamais dans le produit (7.3)
        self.assertTrue(any("injection" in e for e in verifier(o)[0]))

    def test_sequence_coherence(self):
        for issue, cause, ok in (("ok", None, True), ("ok", "garde", False), ("echec", "collision", True),
                                 ("echec", None, False), ("echec", "duree", False), ("abandon", "duree", True),
                                 ("abandon", "sans_lecture", True), ("abandon", "garde", False),
                                 ("annulee", None, True), ("remplacee", None, True)):
            o = copy.deepcopy(VALIDES[("sequence", None)])
            o["issue"], o["cause"] = issue, cause
            self.assertEqual(not verifier(o)[0], ok, (issue, cause))
        o = copy.deepcopy(VALIDES[("sequence", None)])
        o["id"] = None  # ordre de Maison, ou d'une autre session
        self.assertEqual(verifier(o), ([], []))

    def test_reponse_accepte_suite_sequence(self):
        o = copy.deepcopy(VALIDES[("reponse", None)])
        del o["suite"]
        self.assertTrue(any("accepte va avec suite sequence" in e for e in verifier(o)[0]))
        o["suite"] = "injection"  # celle de la sonde
        self.assertTrue(verifier(o)[0])

    def test_alerte_et_trame_d(self):
        o = copy.deepcopy(VALIDES[("alerte", None)])
        o["unite"] = "mv"
        self.assertTrue(any("attendu c" in e for e in verifier(o)[0]))
        o = copy.deepcopy(VALIDES[("trame_d", None)])
        o["sens"] = "vers_carte"
        self.assertTrue(any("trame_d : origine carte" in e for e in verifier(o)[0]))
        o = copy.deepcopy(VALIDES[("trame_d", None)])
        o["octets"] = "00" * 9
        self.assertTrue(verifier(o)[0])

    def test_config_bornes(self):
        o = copy.deepcopy(VALIDES[("config", None)])
        o["automate"]["confirmation_ms"] = 199
        self.assertTrue(any("automate.confirmation_ms 199 hors bornes" in e for e in verifier(o)[0]))
        o = copy.deepcopy(VALIDES[("config", None)])
        o["automate"]["lissage_calme_ms"] = 2000
        o["automate"]["lissage_plafond_ms"] = 1000
        self.assertTrue(any("lissage_calme_ms au-dela" in e for e in verifier(o)[0]))
        o = copy.deepcopy(VALIDES[("config", None)])
        o["surveillance"]["temp_alerte_c"] = 90
        self.assertTrue(verifier(o)[0])

    def test_radio_inconnue(self):
        # OpenThread pas encore lu : role null, rien d'autre.
        self.assertEqual(verifier(msg("compteurs", "radio", role=None)), ([], []))


class BornesProduit(unittest.TestCase):
    def bornes(self, chemin, n):
        with open(os.path.join(RACINE, chemin), encoding="utf-8") as f:
            texte = f.read()
        c = {m[0]: (int(m[1]), int(m[2])) for m in re.findall(r'\{"(\w+)", &Params::\w+, (\d+), (\d+)\}', texte)}
        self.assertEqual(len(c), n)
        return c

    def test_memes_bornes_que_le_firmware(self):
        self.assertEqual(self.bornes("src/produit/hotte_etat.cpp", 12), json_check.AUTOMATE_BORNES)
        self.assertEqual(self.bornes("src/produit/surveillance.cpp", 6), json_check.SURVEILLANCE_BORNES)


class ExemplesProduit(unittest.TestCase):
    def test_exemples_du_protocole(self):
        exemples = json_check.examples(PROTOCOLE)
        self.assertGreaterEqual(len(exemples), 20)
        code, out = lancer("--strict", "--profil", "produit", "--exemples", PROTOCOLE)
        self.assertEqual(code, 0, out)
        types = {json.loads(e[1:])["t"] for e in exemples}
        self.assertTrue({"hello", "config", "etat", "compteurs", "reponse", "hotte", "sequence", "alerte", "trame_d",
                         "reseau", "log", "hb", "fin"} <= types, types)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer les tests Python : ils échouent**

```bash
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

Attendu : **échec**, avec :

```
AttributeError: module 'json_check' has no attribute 'AUTOMATE_BORNES'
FAILED (errors=1)
```

- [ ] **Étape 3 : Ajouter le profil produit à `tools/json_check.py`**

Depuis la racine du dépôt :

```bash
python3 - <<'EOF'
#!/usr/bin/env python3
"""Ajoute le profil du produit a tools/json_check.py (spec produit 7, 9.1) :
schemas SCHEMAS_PRODUIT, option --profil sonde|produit, coherences du produit.
Chaque texte remplace doit se trouver exactement une fois."""
import sys

P = "tools/json_check.py"
c = open(P, encoding="utf-8").read()


def remplacer(avant, apres):
    global c
    n = c.count(avant)
    if n != 1:
        sys.exit(f"{P} : {n} occurrence(s) de {avant[:70]!r} (1 attendue)")
    c = c.replace(avant, apres)


remplacer(
    '"""Verifie des lignes machine de la sonde contre docs/PROTOCOLE-JSON.md (v1, profil hotte).\n',
    '"""Verifie des lignes machine de la sonde (docs/PROTOCOLE-JSON.md) ou du module\n'
    "produit (docs/PROTOCOLE-JSON-PRODUIT.md, option --profil produit) : v1, profil hotte.\n",
)
remplacer(
    "  options : --strict (avertissements comptes comme erreurs), -q (resume seul),\n",
    "  python3 tools/json_check.py --profil produit --exemples docs/PROTOCOLE-JSON-PRODUIT.md\n"
    "  options : --strict (avertissements comptes comme erreurs), -q (resume seul),\n",
)

PRODUIT = r'''
# ---------------------------------------------------------------------------
#  Profil du produit (sous-projet 2 : docs/PROTOCOLE-JSON-PRODUIT.md)
# ---------------------------------------------------------------------------

# Bornes des reglages de l'automate et de la surveillance (kChamps de
# src/produit/hotte_etat.cpp et src/produit/surveillance.cpp ; test_json_check_produit le verifie).
AUTOMATE_BORNES = {
    "delai_moteur_ms": (1000, 60000),
    "entre_appuis_ms": (100, 5000),
    "confirmation_ms": (200, 5000),
    "fraicheur_ms": (200, 120000),
    "lissage_calme_ms": (300, 2000),
    "lissage_plafond_ms": (1000, 10000),
    "ordre_calme_ms": (50, 1000),
    "sequence_max_ms": (5000, 60000),
    "attente_etat_ms": (1000, 60000),
    "prolongee_max_ms": (900000, 3600000),
    "ignore_demarrage_ms": (0, 10000),
    "sonde_vitesse": (0, 1),
}
SURVEILLANCE_BORNES = {
    "temp_alerte_c": (20, 85),
    "temp_hyst_c": (1, 10),
    "alim_hotte_min_mv": (3000, 6000),
    "alim_module_min_mv": (3000, 5000),
    "alim_hyst_mv": (20, 500),
    "alim_alertes": (0, 1),
}
MARCHE = Enum("eteinte", "armee", "prolongee", "inconnue")
MOTEUR = Int(0, 3)
CONFIANCE = Enum("confirme", "deduit", "presume", "inconnu")
SOURCE = Enum("fil", "annexe", "deduit", "nvs")
MODE_ETAT = Enum("repete", "changements", "appuis")
CANAL = Enum("app", "matter")
TEMP = Int(-40, 150)
CAUSES_ECHEC = {"non_confirme", "sans_lecture", "collision", "garde", "pilote"}
CAUSES_ABANDON = {"sans_lecture", "pilote", "duree"}
ETAT_COURT = Obj({"marche": MARCHE, "moteur": MOTEUR, "lampe": Null(BOOL)})
VOIE = Null(Obj({"mv": U32, "min_s_mv": U32, "min_mv": U32, "alerte": BOOL}))


def _bloc(**champs):
    return Obj(dict({"boot": BOOT, "up_s": U32}, **champs))


SCHEMAS_PRODUIT = {
    ("hello", "base"): Obj(
        dict(
            SCHEMAS[("hello", "base")].fields,
            build=Enum("produit"),
            reseau_build=Enum("thread"),
        )
    ),
    ("hello", "identite"): Obj(
        {
            "boot": BOOT,
            "mac": Null(Hex(6)),
            "id": SCHEMAS[("hello", "identite")].fields["id"],
            "appareil": Enum("hotte"),
            "caps": Arr(Enum("hotte", "matter", "udp", "cle", "log", "trames_d", "alim", "thermique", "essai", "simule")),
            "thermique": Obj({"temp_max_pose_c": Null(TEMP)}),
        }
    ),
    ("config", None): Obj(
        {
            "automate": Obj({k: U32 for k in AUTOMATE_BORNES}),
            "surveillance": Obj({k: U32 for k in SURVEILLANCE_BORNES}),
            "ligne": Obj({"pilote": Enum("simule", "A", "B", "C"), "mode": MODE_ETAT, "annexe": BOOL}),
            "simu": Null(
                Obj(
                    {
                        "latence_ms": Int(0, 5000),
                        "periode_ms": Int(100, 60000),
                        "trame_ms": Int(10, 1000),
                        "annexe_ms": Int(50, 5000),
                        "prolongee_ms": Int(60000, 3600000),
                        "fin": Enum("eteinte", "armee"),
                        "inconnues": Enum("rien", "direct"),
                        "refuse": BOOL,
                        "hors_service": BOOL,
                    }
                )
            ),
        }
    ),
    ("etat", "hotte"): _bloc(
        marche=MARCHE, moteur=MOTEUR, lampe=Null(BOOL), confiance=CONFIANCE, source=SOURCE, age_ms=Null(U32)
    ),
    ("etat", "automate"): _bloc(
        mode=MODE_ETAT,
        annexe=BOOL,
        pilote=BOOL,
        ventilo=Null(Obj({"cible": Enum("eteint", "v1", "v2", "v3"), "origine": CANAL})),
        lampe=Null(Obj({"cible": BOOL, "origine": CANAL})),
        appui=Null(Obj({"touche": Enum("marche", "lumiere", "v1", "v2", "v3"), "essai": Int(0, 1), "depuis_ms": U32})),
        delai_moteur_ms=U32,
        marche_autorisee=BOOL,
    ),
    ("etat", "matter"): _bloc(
        demarre=BOOL,
        mis_en_service=BOOL,
        fabriques=U8,
        abonnements=U32,
        ignore_ms=U32,
        maxint_s=Int(0, 3600),
        role_demarrage=Enum("routeur", "med"),
        tx_dbm=Null(Int(-128, 127)),
        derniere=BOOL,
        ecritures=U32,
        reflets=U32,
        verrou_occupe=U32,
        ignores=U32,
    ),
    ("etat", "thermique"): _bloc(
        temp_c=Null(TEMP),
        temp_max_c=Null(TEMP),
        temp_max_pose_c=Null(TEMP),
        alerte=BOOL,
        alertes=U32,
        lectures_ratees=U32,
        seuil_c=Int(20, 85),
    ),
    ("etat", "alim"): _bloc(hotte=VOIE, module=VOIE, alertes_actives=BOOL),
    ("etat", "sys"): SCHEMAS[("etat", "sys")],
    ("compteurs", "hotte"): Obj(
        {
            "appuis_panneau": U32,
            "appuis_module": U32,
            "reussies": U32,
            "annulees": U32,
            "remplacees": U32,
            "abandons": U32,
            "echecs": counters("non_confirme", "sans_lecture", "collision", "garde", "pilote", "duree"),
            "nouveaux_essais": U32,
            "transitions_inconnues": U32,
            "anomalies": U32,
            "marche_inconnue": U32,
            "prolongee_perimee": U32,
            "lignes_muettes": U32,
            "actions_perdues": U32,
        }
    ),
    ("compteurs", "alim"): counters("hotte_passages", "module_passages"),
    ("compteurs", "radio"): Obj(
        {
            "role": Null(Enum("disabled", "detached", "child", "router", "leader")),
            "tx_dbm": Opt(Null(Int(-128, 127))),
            "sensibilite_dbm": Opt(Int(-128, 0)),
            "lien": Opt(
                Null(
                    Obj(
                        {
                            "avec": Enum("parent", "routeur"),
                            "rssi_moyen_dbm": Int(-128, 0),
                            "lq_in": Int(0, 3),
                            "lq_out": Int(0, 3),
                        }
                    )
                )
            ),
            "tx_total": Opt(U32),
            "tx_retry": Opt(U32),
            "tx_echecs": Opt(U32),
            "changements_parent": Opt(U32),
            "changements_role": Opt(U32),
        }
    ),
    ("hb", None): SCHEMAS[("hb", None)],
    ("fin", None): SCHEMAS[("fin", None)],
    ("reponse", None): Obj(dict(SCHEMAS[("reponse", None)].fields, suite=Opt(Enum("sequence")))),
    ("log", None): Obj(
        {
            "src": Enum("produit", "matter", "hotte", "reseau", "simu"),
            "niv": Enum("notice", "trace"),
            "txt": Str(191),
            "sautes": Opt(U32),
        }
    ),
    # Transport UDP sur Thread : schema du bloc ip de la ScreenBar, plus rafale_tx.
    ("reseau", "ip"): Obj(
        {
            "frais_ms": Null(U32),
            "srp": Obj({"nom": Null(Str(63))}),
            "adresses": Arr(Obj({"adr": Str(45), "type": Enum("omr", "ml_eid", "autre"), "pref": BOOL}), 4),
            "udp": Obj(dict(SCHEMAS[("reseau", "ip")].fields["udp"].fields, rafale_tx=U32)),
        }
    ),
    ("hotte", None): Obj({"avant": ETAT_COURT, "apres": ETAT_COURT, "origine": Enum("panneau", "module", "inconnue"),
                          "source": SOURCE, "confiance": CONFIANCE}),
    ("sequence", None): Obj(
        {
            "id": Null(Int(1, 999999999)),
            "origine": CANAL,
            "sujet": Enum("ventilo", "lampe"),
            "issue": Enum("ok", "annulee", "echec", "abandon", "remplacee"),
            "cause": Null(Enum(*(CAUSES_ECHEC | CAUSES_ABANDON))),
            "duree_ms": U32,
            "appuis": U8,
        }
    ),
    ("alerte", None): Obj(
        {
            "sujet": Enum("temperature", "lecture_temperature", "alim_hotte", "alim_module"),
            "etape": Enum("debut", "fin"),
            "valeur": Null(Int(-40, 100000)),
            "seuil": Null(Int(-40, 100000)),
            "unite": Enum("c", "mv"),
        }
    ),
    ("trame_d", None): Obj(
        {
            "t_ms": U32,
            "origine": Enum("carte", "panneau", "module"),
            "sens": Enum("vers_panneau", "vers_carte"),
            "octets": Hex(1, 8),
            "sautes": Opt(U32),
        }
    ),
}
PROFILS = {"sonde": SCHEMAS, "produit": SCHEMAS_PRODUIT}
'''
remplacer(
    'BLOCKED = {"hello", "etat", "compteurs", "reseau"}\nBLOCS = {t: {b for (tt, b) in SCHEMAS if tt == t} for t in BLOCKED}\n',
    PRODUIT.lstrip("\n")
    + '\nBLOCKED = {"hello", "etat", "compteurs", "reseau"}\n'
    + "BLOCS = {p: {t: {b for (tt, b) in s if tt == t} for t in BLOCKED} for p, s in PROFILS.items()}\n",
)

remplacer(
    'def coherence(t, obj, errs, warns):\n    """Regles qui lient plusieurs champs."""\n    if t == "reponse":\n',
    'def coherence(t, obj, errs, warns, profil="sonde"):\n'
    '    """Regles qui lient plusieurs champs."""\n'
    '    if profil == "produit":\n'
    "        coherence_produit(t, obj, errs, warns)\n"
    "        return\n"
    '    if t == "reponse":\n',
)
remplacer(
    "def check_line(raw):\n",
    '''def coherence_produit(t, obj, errs, warns):
    """Regles du profil du produit qui lient plusieurs champs."""
    if t == "reponse":
        code, ok, etape = obj.get("code"), obj.get("ok"), obj.get("etape")
        if code in OK_CODES and ok is not True:
            errs.append(f"reponse : code {code} avec ok {ok}")
        if code in KO_CODES and ok is not False:
            errs.append(f"reponse : code {code} avec ok {ok}")
        if etape == "debut" and code != "en_cours":
            errs.append(f"reponse : etape debut avec code {code}")
        if etape == "fin" and "duree_ms" not in obj:
            errs.append("reponse : etape fin sans duree_ms")
        if (code == "accepte") != (obj.get("suite") == "sequence"):
            errs.append(f"reponse : code {code} et suite {obj.get('suite')} (accepte va avec suite sequence)")
        if ("bail_s" in obj) != ("up_s" in obj):
            errs.append("reponse : bail_s et up_s vont ensemble")
    elif t == "config":
        for nom, bornes in (("automate", AUTOMATE_BORNES), ("surveillance", SURVEILLANCE_BORNES)):
            bloc = obj.get(nom)
            if not isinstance(bloc, dict):
                continue
            for k, (lo, hi) in bornes.items():
                v = bloc.get(k)
                if isinstance(v, int) and not isinstance(v, bool) and not lo <= v <= hi:
                    errs.append(f"config : {nom}.{k} {v} hors bornes ({lo}..{hi})")
        a = obj.get("automate")
        if isinstance(a, dict) and isinstance(a.get("lissage_calme_ms"), int) and isinstance(a.get("lissage_plafond_ms"), int):
            if a["lissage_calme_ms"] > a["lissage_plafond_ms"]:
                errs.append("config : automate.lissage_calme_ms au-dela de lissage_plafond_ms")
    elif t == "sequence":
        issue, cause = obj.get("issue"), obj.get("cause")
        if issue in ("ok", "annulee", "remplacee") and cause is not None:
            errs.append(f"sequence : issue {issue} avec cause {cause}")
        if issue == "echec" and cause not in CAUSES_ECHEC:
            errs.append(f"sequence : echec avec cause {cause}")
        if issue == "abandon" and cause not in CAUSES_ABANDON:
            errs.append(f"sequence : abandon avec cause {cause}")
    elif t == "alerte":
        sujet, unite = obj.get("sujet"), obj.get("unite")
        attendue = "c" if sujet in ("temperature", "lecture_temperature") else "mv"
        if unite != attendue:
            errs.append(f"alerte : {sujet} en {unite}, attendu {attendue}")
    elif t == "trame_d":
        o, sens = obj.get("origine"), obj.get("sens")
        if (o == "carte") != (sens == "vers_panneau"):
            errs.append(f"trame_d : origine {o} et sens {sens}")
    elif t == "hello" and obj.get("bloc") == "base":
        if obj.get("fw") != obj.get("fw_desc"):
            warns.append(f"hello : fw {obj.get('fw')!r} different de fw_desc {obj.get('fw_desc')!r}")


def check_line(raw, profil="sonde"):
''',
)
remplacer(
    "    if t in BLOCKED:\n"
    '        if len(keys) < 5 or keys[4] != "bloc":\n'
    '            errs.append(f"{t} : bloc attendu en 5e champ")\n'
    "        if bloc not in BLOCS[t]:\n"
    "            errs.append(f\"{t} : bloc {bloc!r} inconnu (attendu : {', '.join(sorted(BLOCS[t]))})\")\n",
    "    blocs = BLOCS[profil]\n"
    "    if t in BLOCKED:\n"
    '        if len(keys) < 5 or keys[4] != "bloc":\n'
    '            errs.append(f"{t} : bloc attendu en 5e champ")\n'
    "        if bloc not in blocs[t]:\n"
    "            errs.append(f\"{t} : bloc {bloc!r} inconnu (attendu : {', '.join(sorted(blocs[t]))})\")\n",
)
remplacer(
    "    schema = SCHEMAS.get((t, bloc if t in BLOCKED else None))\n",
    "    schema = PROFILS[profil].get((t, bloc if t in BLOCKED else None))\n",
)
remplacer("    coherence(t, obj, errs, warns)\n    return obj, errs, warns\n", "    coherence(t, obj, errs, warns, profil)\n    return obj, errs, warns\n")
remplacer(
    "    def __init__(self, strict, quiet, continuity=True):\n        self.strict, self.quiet, self.continuity = strict, quiet, continuity\n",
    '    def __init__(self, strict, quiet, continuity=True, profil="sonde"):\n'
    "        self.strict, self.quiet, self.continuity, self.profil = strict, quiet, continuity, profil\n",
)
remplacer("        obj, errs, warns = check_line(machine)\n", "        obj, errs, warns = check_line(machine, self.profil)\n")
remplacer(
    '    ap = argparse.ArgumentParser(description="Verifie les lignes machine de la sonde (docs/PROTOCOLE-JSON.md).")\n',
    '    ap = argparse.ArgumentParser(description="Verifie les lignes machine de la sonde ou du produit (profil hotte).")\n',
)
remplacer(
    "    args = ap.parse_args(argv)\n",
    '    ap.add_argument("--profil", choices=sorted(PROFILS), default="sonde",\n'
    '                    help="sonde (docs/PROTOCOLE-JSON.md, defaut) ou produit (docs/PROTOCOLE-JSON-PRODUIT.md)")\n'
    "    args = ap.parse_args(argv)\n",
)
remplacer(
    "    rep = Report(args.strict, args.quiet, not args.independantes)\n",
    "    rep = Report(args.strict, args.quiet, not args.independantes, args.profil)\n",
)
open(P, "w", encoding="utf-8").write(c)
print("json_check.py : profil produit ajoute")
EOF
```

Sortie attendue :

```
json_check.py : profil produit ajoute
```

- [ ] **Étape 4 : Écrire `docs/PROTOCOLE-JSON-PRODUIT.md`**

````markdown
# Protocole compagnon v1 : profil « hotte » du module produit

Ce document décrit le protocole machine du **module produit** (ESP32-C6, env
`produit`, sous-projet 2) **par différence** avec deux références :

- la ScreenBar, `benq-screenbar-halo-matter` au commit **`c58a506`**,
  [docs/PROTOCOLE-JSON.fr.md](https://github.com/Djoko-cli/benq-screenbar-halo-matter/blob/c58a506/docs/PROTOCOLE-JSON.fr.md)
  (notée « ScreenBar ») : tramage, session, enveloppe, commandes, versionnage
  et **transport UDP sur Thread** (§10) ;
- la sonde de la reconnaissance, [PROTOCOLE-JSON.md](PROTOCOLE-JSON.md)
  (notée « sonde ») : champ `appareil`, clé H1, série `HOTTE-`.

Tout ce que ce document ne dit pas est **identique** à la ScreenBar. La spec
du produit fait foi : [SPEC-PRODUIT.md](SPEC-PRODUIT.md), section 7.

- Code : `src/produit/json_out_produit.*` (briques pures, testées sur l'hôte),
  `src/produit/json_mode_produit.*` (sessions, instantanés, événements),
  `src/produit/cli_produit.cpp` (lignes de l'hôte, liste blanche),
  `src/produit/net_udp_thread.*` (transport réseau, copié de la ScreenBar),
  `src/h1_proto.*` (enveloppe H1).
- Vérification : `python3 tools/json_check.py --profil produit ...` contrôle
  toute ligne machine du produit. Les exemples de la section 7 sont vérifiés
  par `python3 tools/json_check.py --profil produit --strict --exemples
  docs/PROTOCOLE-JSON-PRODUIT.md`, lancé par `sh tools/tests/test_hote.sh`.

## 1. Ce qui ne change pas

| Référence | Ici |
|---|---|
| ScreenBar §2 à §4, §6, §9 : tramage, session, enveloppe, commandes `id=<n>`, `reponse`, versionnage | identiques ; `rev` = 4 ; tampon d'émission de l'USB de 8 Ko, comme la sonde |
| ScreenBar §10 : UDP sur Thread, port 5480, enveloppe H1, poignée de main, adresse OMR, débit plafonné à 3 000 octets/s, 24 tampons OpenThread gardés en réserve, priorité basse | identique (code copié) ; clé en NVS `produit/cle` ; découverte par le **nom d'hôte SRP** du nœud, lu dans `reseau` `ip` par l'USB |
| sonde §2 : `appareil` = `hotte`, série `HOTTE-` + MAC | identique ; une app multi-appareils distingue la sonde du produit par `hello.base.build` |

## 2. Écarts généraux

- **`hello` `base`** : `build` = `produit`, `reseau_build` = `thread`, `env` =
  `produit`.
- **`hello` `identite`** : `produit` = `Module hotte Haier`, `nom` = `Hotte`,
  `hw_txt` = `C6 SuperMini, pilote <simule|A|B|C>` ; `caps` = `hotte`, `matter`,
  `udp`, `cle`, `log`, `trames_d`, `alim`, `thermique`, `essai`, et `simule`
  dans le build simulé ; **jamais `injection`**. Un objet `thermique` porte
  `temp_max_pose_c`, le maximum de la puce depuis la pose (NVS `temp_max`),
  en degrés entiers, ou null.
- **Températures** en degrés entiers (arrondis), **tensions** en mV : le
  protocole ne porte que des entiers.
- **Clé H1** : `~/.config/hotte-produit/cle` côté Mac. `tools/hotte_udp.py cle
  <port>` la range d'après `hello.base.build`, lu par l'USB ; `--appareil
  produit` pour les sessions. La remise à zéro Matter (`decommission`) efface
  la clé, comme le départ du dernier contrôleur.

## 3. La commande `json`

Identique à la ScreenBar, avec deux écarts :

- `json trames 0|1` : événements `trame_d` (section 5.4). **Coupés au départ**,
  10 par seconde au plus ; à distance, ils se coupent seuls **60 s** après
  `json trames 1` (`hello` `base` le dit).
- Profil distant : `etat` toutes les 2 s, **`compteurs` coupés** (0 : les essais
  les demandent, `json compteurs 10000`), `reseau` toutes les 30 s, `trame_d`
  et `log` coupés. Profil USB : `etat` 1 s, `compteurs` 1 s, `reseau` 5 s.

## 4. Messages périodiques et de session

### 4.1 `config`

| Objet | Champs |
|---|---|
| `automate` | les 12 réglages de l'automate, noms de `hotte regle` (spec 4.2) |
| `surveillance` | `temp_alerte_c`, `temp_hyst_c`, `alim_hotte_min_mv`, `alim_module_min_mv`, `alim_hyst_mv`, `alim_alertes` (0 au banc : pont de mesure non monté) |
| `ligne` | `pilote` (`simule`, `A`, `B`, `C`), `mode` (`repete`, `changements`, `appuis`), `annexe` |
| `simu` | build simulé : `latence_ms`, `periode_ms`, `trame_ms`, `annexe_ms`, `prolongee_ms`, `fin` (`eteinte`, `armee`), `inconnues` (`rien`, `direct`), `refuse`, `hors_service` ; null ailleurs |

`config` est réémis à chaque session en mode machine après tout réglage
(`hotte regle`, `simu ...`, `matter ...`).

### 4.2 `etat`

Chaque bloc commence par `boot` et `up_s`, comme ceux de la sonde.

| Bloc | Champs |
|---|---|
| `hotte` | `marche` (`eteinte`, `armee`, `prolongee`, `inconnue`), `moteur` (0 à 3), `lampe` (booléen, null si inconnue), `confiance` (`confirme`, `deduit`, `presume`, `inconnu`), `source` (`fil`, `annexe`, `deduit`, `nvs`), `age_ms` (âge du dernier état **lu** ; null : rien lu depuis le démarrage) |
| `automate` | `mode`, `annexe`, `pilote` (en service), `ventilo` (null, ou `cible` `eteint`, `v1`, `v2`, `v3` et `origine` `app` ou `matter`), `lampe` (null, ou `cible` booléen et `origine`), `appui` (null, ou l'appui en vol : `touche`, `essai` 0 ou 1, `depuis_ms`), `delai_moteur_ms` (reste du délai moteur), `marche_autorisee` (arrêt du moteur lu depuis le dernier appui : règle d'or) |
| `matter` | `demarre`, `mis_en_service`, `fabriques`, `abonnements`, `ignore_ms` (reste de la fenêtre d'ignorance du démarrage), `maxint_s`, `role_demarrage` (`routeur`, `med` : au prochain démarrage), `tx_dbm` (null si illisible), `derniere` (substitution de On), `ecritures`, `reflets`, `verrou_occupe`, `ignores` |
| `thermique` | `temp_c`, `temp_max_c` (depuis le démarrage), `temp_max_pose_c` (depuis la pose), `alerte`, `alertes` (nombre), `lectures_ratees`, `seuil_c` ; températures null tant que rien n'est lu |
| `alim` | `hotte` (`+` de `CN3`, GPIO2) et `module` (après diode et fusible, GPIO3) : null, ou `mv`, `min_s_mv` (minimum de la dernière seconde), `min_mv` (depuis le démarrage), `alerte` ; `alertes_actives` |
| `sys` | l'objet `sys` de la ScreenBar, à l'identique |

### 4.3 `compteurs`

| Bloc | Champs |
|---|---|
| `hotte` | `appuis_panneau`, `appuis_module`, séquences `reussies`, `annulees`, `remplacees`, `abandons`, `echecs` par cause (`non_confirme`, `sans_lecture`, `collision`, `garde`, `pilote`, `duree`), `nouveaux_essais`, `transitions_inconnues`, `anomalies`, `marche_inconnue`, `prolongee_perimee`, `lignes_muettes`, `actions_perdues` |
| `alim` | `hotte_passages`, `module_passages` : passages sous les seuils, comptés même sans alerte |
| `radio` | `role` (rôle OpenThread réel ; null : pas encore lu, et rien d'autre), `tx_dbm`, `sensibilite_dbm`, `lien` (null, ou `avec` `parent` en MED, `routeur` voisin de meilleur RSSI en routeur : `rssi_moyen_dbm`, `lq_in`, `lq_out`), `tx_total`, `tx_retry`, `tx_echecs` (compteurs MAC), `changements_parent`, `changements_role` |

### 4.4 `reseau` `ip`

Le schéma de la ScreenBar (`frais_ms`, `srp.nom`, `adresses`, `udp.*`), plus
`udp.rafale_tx` : datagrammes de rafale émis (section 6).

## 5. Événements

### 5.1 `hotte` : changement d'état

`avant` et `apres` (`marche`, `moteur`, `lampe`), `origine` (`panneau`,
`module`, `inconnue`), `source` et `confiance` de l'état après. Un appui du
panneau est publié tout de suite (critère C2) ; pendant une séquence demandée
par Maison ou l'app, les états intermédiaires ne sont pas publiés dans Maison,
mais le compagnon les voit.

### 5.2 `sequence` : fin d'une séquence

`id` (l'entier de la commande de l'app, dans la session qui l'a envoyée ;
null pour un ordre de Maison et dans les autres sessions), `origine` (`app`,
`matter`), `sujet` (`ventilo`, `lampe`), `issue` (`ok`, `annulee`, `echec`,
`abandon`, `remplacee`), `cause` (null pour `ok`, `annulee`, `remplacee` ;
`non_confirme`, `sans_lecture`, `collision`, `garde` ou `pilote` pour un
échec ; `sans_lecture`, `pilote` ou `duree` pour un abandon), `duree_ms`,
`appuis`. L'origine d'un ordre avec id reçoit son `sequence` même hors mode
machine, comme l'`injection` de la sonde.

### 5.3 `alerte` : surveillance

`sujet` (`temperature`, `lecture_temperature`, `alim_hotte`, `alim_module`),
`etape` (`debut`, `fin`), `valeur` et `seuil` (degrés, ou mV ; null pour une
lecture de température en échec), `unite` (`c`, `mv`). **Toujours émis vers
chaque session en mode machine, sans abonnement**, et doublé d'un `log`
`notice` émis même sans `json log 1`. **Aucune action sur la hotte.**

### 5.4 `trame_d` : trame décodée du fil

`t_ms`, `origine` (`carte`, `panneau`, `module`), `sens` (`vers_panneau` pour
la carte, `vers_carte` sinon), `octets` (1 à 8, en hexa), `sautes` (lignes
sautées par le plafond de 10 par seconde, si non nul). Nom distinct du `trame`
de la sonde, qui porte des durées brutes. Dans le build simulé, le codage est
celui du banc : un appui, `A0` + le rang de la touche (`marche` 0 à `v3` 4) ;
un état, `marche << 4 | moteur << 2 | lampe`. Le codage réel viendra de
l'avenant de l'étape 6.

### 5.5 `log`

`src` : `produit`, `matter`, `hotte`, `reseau`, `simu`.

## 6. Commandes et liste blanche (spec 7.5)

| Commande | Réponse avec id | À distance |
|---|---|---|
| `hotte ventilo off\|v1\|v2\|v3\|derniere`, `hotte lampe on\|off` | `accepte`, `suite` = `sequence` ; l'événement `sequence` suit, avec le même `id` | **oui** |
| `hotte regle <nom> <valeur>` | `ok`, `usage` ou `refuse` | non |
| `reboot` | `ok`, puis redémarrage 500 ms plus tard (les valeurs NVS en attente d'abord) | **oui** (Q27) |
| `matter tx 8..20` | `ok` | **oui**, 8 à 20 dBm (Q39) |
| `matter med\|maxint\|derniere <v>`, `essai alim on\|off`, `simu ...`, `json cle ...`, `decommission` | `ok`, `usage` ou `refuse` | non |
| `radio rafale <1..30 s>` | `ok` ; datagrammes pleins non authentifiés vers la session, hors plafond de débit, dans la réserve de tampons | **seulement si `essai alim on` a été posé par l'USB** (effacé seul après 72 h) |
| famille `json` | comme la ScreenBar | liste de la ScreenBar, plus `json trames` |

Tout le reste répond `interdite` à distance, en particulier `injecte`,
`injection`, `capture` et `seuils`, qui n'existent pas dans ce build. Le code
manuel et le QR ne partent jamais à distance.

## 7. Exemples

Chaque ligne commence par `<RS>` (l'octet 0x1E) ; elles sont vérifiées par
`json_check.py --profil produit --strict --exemples`.

### 7.1 Connexion par l'USB

```
> id=1 json 1
<RS>{"v":1,"t":"hello","n":0,"ms":5210,"bloc":"base","rev":4,"fw":"0.1.0-4c1d2e3","fw_desc":"0.1.0-4c1d2e3","date":"Sep 29 2026","heure":"20:02:11","env":"produit","build":"produit","reseau_build":"thread","puce":"esp32c6","idf":"v5.5.5","arduino":"3.3.12","boot":"3FA2C901","reset":"mise_sous_tension","reset_n":1,"up_s":5,"session":{"transport":"usb","periode_ms":1000,"compteurs_ms":1000,"reseau_ms":5000,"bail_s":30,"trames":false,"log":false},"limites":{"ligne_max":1024,"cmd_max":127}}
<RS>{"v":1,"t":"hello","n":1,"ms":5211,"bloc":"identite","boot":"3FA2C901","mac":"F0F5BD012345","id":{"fabricant":"Djoko-CLI","produit":"Module hotte Haier","serie":"HOTTE-F0F5BD012345","nom":"Hotte","hw":1,"hw_txt":"C6 SuperMini, pilote simule"},"appareil":"hotte","caps":["hotte","matter","udp","cle","log","trames_d","alim","thermique","essai","simule"],"thermique":{"temp_max_pose_c":null}}
<RS>{"v":1,"t":"config","n":2,"ms":5213,"automate":{"delai_moteur_ms":3000,"entre_appuis_ms":500,"confirmation_ms":1000,"fraicheur_ms":2000,"lissage_calme_ms":700,"lissage_plafond_ms":3000,"ordre_calme_ms":150,"sequence_max_ms":15000,"attente_etat_ms":5000,"prolongee_max_ms":1200000,"ignore_demarrage_ms":2000,"sonde_vitesse":0},"surveillance":{"temp_alerte_c":70,"temp_hyst_c":5,"alim_hotte_min_mv":4500,"alim_module_min_mv":3700,"alim_hyst_mv":100,"alim_alertes":0},"ligne":{"pilote":"simule","mode":"repete","annexe":false},"simu":{"latence_ms":150,"periode_ms":500,"trame_ms":60,"annexe_ms":200,"prolongee_ms":900000,"fin":"eteinte","inconnues":"rien","refuse":false,"hors_service":false}}
<RS>{"v":1,"t":"etat","n":3,"ms":5214,"bloc":"hotte","boot":"3FA2C901","up_s":5,"marche":"eteinte","moteur":0,"lampe":false,"confiance":"confirme","source":"fil","age_ms":210}
<RS>{"v":1,"t":"etat","n":4,"ms":5215,"bloc":"automate","boot":"3FA2C901","up_s":5,"mode":"repete","annexe":false,"pilote":true,"ventilo":null,"lampe":null,"appui":null,"delai_moteur_ms":0,"marche_autorisee":true}
<RS>{"v":1,"t":"etat","n":5,"ms":5216,"bloc":"matter","boot":"3FA2C901","up_s":5,"demarre":true,"mis_en_service":true,"fabriques":1,"abonnements":1,"ignore_ms":0,"maxint_s":20,"role_demarrage":"routeur","tx_dbm":20,"derniere":true,"ecritures":6,"reflets":2,"verrou_occupe":0,"ignores":0}
<RS>{"v":1,"t":"etat","n":6,"ms":5217,"bloc":"thermique","boot":"3FA2C901","up_s":5,"temp_c":38,"temp_max_c":38,"temp_max_pose_c":null,"alerte":false,"alertes":0,"lectures_ratees":0,"seuil_c":70}
<RS>{"v":1,"t":"etat","n":7,"ms":5218,"bloc":"alim","boot":"3FA2C901","up_s":5,"hotte":{"mv":212,"min_s_mv":180,"min_mv":150,"alerte":false},"module":{"mv":204,"min_s_mv":176,"min_mv":150,"alerte":false},"alertes_actives":false}
<RS>{"v":1,"t":"etat","n":8,"ms":5219,"bloc":"sys","boot":"3FA2C901","up_s":5,"sys":{"heap":118000,"heap_min":96000,"heap_bloc":41000,"pile_boucle":3100,"boucle_max_ms":14,"json_perdus":0,"json_trop_longs":0,"rejets":0}}
<RS>{"v":1,"t":"compteurs","n":9,"ms":5220,"bloc":"hotte","appuis_panneau":0,"appuis_module":0,"reussies":0,"annulees":0,"remplacees":0,"abandons":0,"echecs":{"non_confirme":0,"sans_lecture":0,"collision":0,"garde":0,"pilote":0,"duree":0},"nouveaux_essais":0,"transitions_inconnues":0,"anomalies":0,"marche_inconnue":0,"prolongee_perimee":0,"lignes_muettes":0,"actions_perdues":0}
<RS>{"v":1,"t":"compteurs","n":10,"ms":5221,"bloc":"alim","hotte_passages":1,"module_passages":1}
<RS>{"v":1,"t":"compteurs","n":11,"ms":5222,"bloc":"radio","role":"router","tx_dbm":20,"sensibilite_dbm":-120,"lien":{"avec":"routeur","rssi_moyen_dbm":-58,"lq_in":3,"lq_out":3},"tx_total":412,"tx_retry":9,"tx_echecs":0,"changements_parent":0,"changements_role":2}
<RS>{"v":1,"t":"reseau","n":12,"ms":5223,"bloc":"ip","frais_ms":800,"srp":{"nom":"ESP-HOTTE-012345"},"adresses":[{"adr":"fd11:22::1a2b:3c4d:5e6f:7081","type":"omr","pref":true},{"adr":"fd4a:9b1c:e2d3:1:0:ff:fe00:9c00","type":"ml_eid","pref":true}],"udp":{"port":5480,"ouvert":true,"empreinte":"630DCD29","sessions":0,"provisoire":false,"rx":0,"rejets":0,"rx_perdus":0,"defis":0,"tx":0,"tx_perdus":0,"tx_erreurs":0,"tampons_libres":57,"tampons_min":57,"rafale_tx":0}}
<RS>{"v":1,"t":"reponse","n":13,"ms":5224,"id":1,"etape":"fin","cmd":"json 1","ok":true,"code":"ok","duree_ms":14,"bail_s":30,"up_s":5}
<RS>{"v":1,"t":"log","n":14,"ms":5225,"src":"produit","niv":"notice","txt":"1 jeu(x) de reglages hors bornes en NVS : valeurs par defaut"}
```

### 7.2 Un appui au panneau, puis un ordre de l'app

```
<RS>{"v":1,"t":"hotte","n":20,"ms":61020,"avant":{"marche":"armee","moteur":0,"lampe":false},"apres":{"marche":"armee","moteur":2,"lampe":false},"origine":"panneau","source":"deduit","confiance":"deduit"}
> id=7 hotte ventilo off
<RS>{"v":1,"t":"reponse","n":21,"ms":64000,"id":7,"etape":"fin","cmd":"hotte ventilo off","ok":true,"code":"accepte","duree_ms":0,"suite":"sequence"}
<RS>{"v":1,"t":"hotte","n":22,"ms":64410,"avant":{"marche":"armee","moteur":2,"lampe":false},"apres":{"marche":"armee","moteur":0,"lampe":false},"origine":"module","source":"fil","confiance":"confirme"}
<RS>{"v":1,"t":"hotte","n":23,"ms":65420,"avant":{"marche":"armee","moteur":0,"lampe":false},"apres":{"marche":"eteinte","moteur":0,"lampe":false},"origine":"module","source":"fil","confiance":"confirme"}
<RS>{"v":1,"t":"sequence","n":24,"ms":65421,"id":7,"origine":"app","sujet":"ventilo","issue":"ok","cause":null,"duree_ms":1421,"appuis":2}
<RS>{"v":1,"t":"sequence","n":25,"ms":70104,"id":null,"origine":"matter","sujet":"lampe","issue":"echec","cause":"non_confirme","duree_ms":2890,"appuis":2}
```

### 7.3 Alerte, trames, refus, fin

```
<RS>{"v":1,"t":"alerte","n":30,"ms":90000,"sujet":"temperature","etape":"debut","valeur":71,"seuil":70,"unite":"c"}
<RS>{"v":1,"t":"log","n":31,"ms":90001,"src":"produit","niv":"notice","txt":"alerte temperature debut : 71 C (seuil 70 C)"}
<RS>{"v":1,"t":"alerte","n":32,"ms":91000,"sujet":"alim_module","etape":"fin","valeur":3801,"seuil":3700,"unite":"mv"}
<RS>{"v":1,"t":"trame_d","n":33,"ms":92000,"t_ms":91990,"origine":"panneau","sens":"vers_carte","octets":"A3"}
<RS>{"v":1,"t":"trame_d","n":34,"ms":92200,"t_ms":92190,"origine":"carte","sens":"vers_panneau","octets":"18","sautes":2}
<RS>{"v":1,"t":"reponse","n":35,"ms":93000,"id":8,"etape":"fin","cmd":"injecte durees 750 750","ok":false,"code":"interdite","msg":"interdite a distance (spec 7.5) : USB seulement","duree_ms":0}
<RS>{"v":1,"t":"hb","n":36,"ms":94000,"boot":"3FA2C901","up_s":94,"json_perdus":0}
<RS>{"v":1,"t":"fin","n":37,"ms":95000,"cause":"commande"}
```

## 8. Transport réseau : UDP sur Thread

Celui de la ScreenBar (§10), code copié (`src/produit/net_udp_thread.*`) :

- le Mac joint le nœud par son **adresse OMR** ou son **nom SRP**, lus dans
  `reseau` `ip` par l'USB ; il garde sa route Thread avec l'assistant de benq,
  `tools/macos/halo-routes` au commit `c58a506` ;
- `python3 tools/hotte_udp.py --appareil produit session <nom SRP ou IPv6>
  "hotte ventilo v2"` ouvre une session, envoie l'ordre, montre les lignes ;
- la liste blanche est celle de la section 6.
````

- [ ] **Étape 5 : Lancer les tests Python : ils passent**

```bash
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

Attendu :

```
Ran 172 tests in 9.319s
OK
```

- [ ] **Étape 6 : Vérifier les lignes du produit dans `tools/tests/test_hote.sh`**

Dans `tools/tests/test_hote.sh`, remplacer :

```sh
echo "tests hote : OK"
```

par :

```sh
# Lignes realistes du produit et exemples de son protocole : conformes au profil produit.
python3 tools/json_check.py --profil produit --strict --independantes -q "$OUT/test_json_produit_lignes.txt"
python3 tools/json_check.py --profil produit --strict -q --exemples docs/PROTOCOLE-JSON-PRODUIT.md
echo "tests hote : OK"
```

- [ ] **Étape 7 : Lancer les tests hôte**

```bash
sh tools/tests/test_hote.sh
```

Attendu :

```
16 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
40 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
18 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
29 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
tests hote : OK
```

- [ ] **Étape 8 : Committer**

```bash
git add tools/json_check.py tools/tests/test_json_check_produit.py docs/PROTOCOLE-JSON-PRODUIT.md tools/tests/test_hote.sh
git commit -m "$(cat <<'FIN'
Ajouter le profil produit a json_check et le document du protocole du produit

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

### Tâche 9 : `hotte_udp.py` : clé du produit, nom SRP, résumé

**Fichiers :**
- Modifier : `tools/hotte_udp.py`
- Modifier : `tools/tests/test_hotte_udp.py`

**Interfaces :**
- Produit : `hotte_udp.APPAREILS`, `chemin_cle(appareil="sonde")`, `lire_build(lire, ecrire, ident)`,
  `cmd_cle(port, chemin=None, appareil=None)`, `resume_produit(t, b, m)`, option globale
  `--appareil sonde|produit` (avant la sous-commande).

- [ ] **Étape 1 : Ajouter les tests du produit à `tools/tests/test_hotte_udp.py`**

Depuis la racine du dépôt :

```bash
python3 - <<'EOF'
#!/usr/bin/env python3
"""Ajoute les tests du produit a tools/tests/test_hotte_udp.py (pour tools/hotte_udp.py (spec produit 4.7) : cle d'apres
'build' (hello, lu par l'USB) ou --appareil, nom SRP ou IPv6, resume du
profil produit. Chaque texte remplace doit se trouver exactement une fois."""
import sys

# ---------------------------------------------------------------------------
#  Tests
# ---------------------------------------------------------------------------
T = "tools/tests/test_hotte_udp.py"
t = open(T, encoding="utf-8").read()
AJOUT = '''

class Produit(unittest.TestCase):
    """Cle d'apres l'appareil (build de hello), resume du profil produit."""

    def test_chemin_par_appareil(self):
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop("HOTTE_CLE", None)
            self.assertTrue(hotte_udp.chemin_cle().endswith(os.path.join(".config", "hotte-sonde", "cle")))
            self.assertTrue(hotte_udp.chemin_cle("produit").endswith(os.path.join(".config", "hotte-produit", "cle")))
            with self.assertRaises(SystemExit):
                hotte_udp.chemin_cle("screenbar")
        with mock.patch.dict(os.environ, {"HOTTE_CLE": "/tmp/une-cle"}):
            self.assertEqual(hotte_udp.chemin_cle("produit"), "/tmp/une-cle")

    def usb_hello(self, build):
        """Module simule sur l'USB : 'id=<n> json hello' -> hello.base (build), puis reponse fin."""
        attente = [b"reste d'une ligne\\n> "]  # le decoupage se cale sur le premier LF

        def ecrire(octets):
            for ligne in octets.split(b"\\n")[:-1]:
                if ligne.startswith(b"id=") and ligne.endswith(b"json hello"):
                    ident = int(ligne.split(b" ")[0][3:])
                    if build:
                        attente.append(RS + compact({"v": 1, "t": "hello", "n": 1, "ms": 5, "bloc": "base",
                                                     "build": build}) + b"\\r\\n")
                    attente.append(RS + compact({"v": 1, "t": "reponse", "n": 2, "ms": 6, "id": ident, "etape": "fin",
                                                 "cmd": "json hello", "ok": True, "code": "ok", "duree_ms": 1}) + b"\\r\\n")

        def lire(_delai):
            return attente.pop(0) if attente else b""

        return lire, ecrire

    def test_lire_build(self):
        for build in ("produit", "sonde"):
            lire, ecrire = self.usb_hello(build)
            self.assertEqual(hotte_udp.lire_build(lire, ecrire, 900001), build)
        lire, ecrire = self.usb_hello(None)  # firmware sans build : rien
        self.assertIsNone(hotte_udp.lire_build(lire, ecrire, 900002))
        t = [0.0]

        def horloge():
            t[0] += 0.5
            return t[0]

        self.assertIsNone(hotte_udp.lire_build(lambda _d: b"", lambda _o: None, 900003, horloge=horloge))

    def test_resume_produit(self):
        r = hotte_udp.resume({"v": 1, "t": "sequence", "n": 4, "ms": 1, "id": 17, "origine": "app", "sujet": "ventilo",
                              "issue": "echec", "cause": "non_confirme", "duree_ms": 2400, "appuis": 2})
        self.assertEqual(r, "n=4 sequence id=17 app ventilo : echec, non_confirme (2 appuis, 2400 ms)")
        r = hotte_udp.resume({"v": 1, "t": "etat", "n": 5, "ms": 1, "bloc": "hotte", "boot": "3FA2C901", "up_s": 1,
                              "marche": "armee", "moteur": 2, "lampe": None, "confiance": "confirme", "source": "fil",
                              "age_ms": 120})
        self.assertEqual(r, "n=5 etat/hotte armee moteur 2 lampe ? (confirme, fil, lu il y a 120 ms)")
        r = hotte_udp.resume({"v": 1, "t": "alerte", "n": 6, "ms": 1, "sujet": "temperature", "etape": "debut",
                              "valeur": 71, "seuil": 70, "unite": "c"})
        self.assertEqual(r, "n=6 alerte temperature debut : 71 c (seuil 70)")
        r = hotte_udp.resume({"v": 1, "t": "hotte", "n": 7, "ms": 1,
                              "avant": {"marche": "armee", "moteur": 0, "lampe": False},
                              "apres": {"marche": "armee", "moteur": 2, "lampe": True},
                              "origine": "panneau", "source": "fil", "confiance": "confirme"})
        self.assertEqual(r, "n=7 hotte armee/0 -> armee/2 lampe on (panneau, fil)")
        self.assertIn("puce 45 C", hotte_udp.resume({"v": 1, "t": "etat", "n": 8, "ms": 1, "bloc": "thermique",
                                                      "temp_c": 45, "temp_max_c": 46, "temp_max_pose_c": 61,
                                                      "alerte": False, "seuil_c": 70}))
        # Un message de la sonde garde son resume.
        self.assertEqual(hotte_udp.resume({"v": 1, "t": "fin", "n": 9, "ms": 1, "cause": "bail"}), "n=9 fin cause bail")
'''
avant = '\n\nif __name__ == "__main__":\n    unittest.main()\n'
if t.count(avant) != 1:
    sys.exit(f"{T} : fin du fichier inattendue")
t = t.replace(avant, AJOUT + avant)
open(T, "w", encoding="utf-8").write(t)
print("test_hotte_udp.py : tests du produit ajoutes")
EOF
```

Sortie attendue :

```
test_hotte_udp.py : tests du produit ajoutes
```

- [ ] **Étape 2 : Lancer les tests Python : ils échouent**

```bash
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

Attendu : **échec**, avec :

```
AttributeError: module 'hotte_udp' has no attribute 'lire_build'
FAILED (failures=1, errors=2)
```

- [ ] **Étape 3 : Ajouter le produit à `tools/hotte_udp.py`**

Clé d'après l'appareil : `~/.config/hotte-<appareil>/cle`. `cle <port>` lit
`hello.base.build` par l'USB (`id=<n> json hello`) si `--appareil` manque ;
`session` et `enregistre` prennent `--appareil produit` (la clé signe la
poignée de main, elle se choisit avant). Les noms SRP et les adresses IPv6
passent déjà par `getaddrinfo`.

Depuis la racine du dépôt :

```bash
python3 - <<'EOF'
#!/usr/bin/env python3
"""Ajoute le produit a tools/hotte_udp.py (spec produit 4.7) : cle d'apres
'build' (hello, lu par l'USB) ou --appareil, nom SRP ou IPv6, resume du
profil produit. Chaque texte remplace doit se trouver exactement une fois."""
import sys

P = "tools/hotte_udp.py"
c = open(P, encoding="utf-8").read()


def remplacer(avant, apres):
    global c
    n = c.count(avant)
    if n != 1:
        sys.exit(f"{P} : {n} occurrence(s) de {avant[:70]!r} (1 attendue)")
    c = c.replace(avant, apres)


remplacer(
    '"""Client de banc du transport reseau de la sonde (docs/PROTOCOLE-JSON.md, section 9).\n\n'
    "Session H1 (UDP sur le Wi-Fi, port 5480) avec la cle posee par l'USB : suivre\n"
    "les lignes de la sonde, lui envoyer des commandes de la liste blanche, ou\n"
    "enregistrer une capture.\n",
    '"""Client de banc du transport reseau de la sonde (docs/PROTOCOLE-JSON.md, section 9)\n'
    "et du module produit (docs/PROTOCOLE-JSON-PRODUIT.md).\n\n"
    "Session H1 (UDP port 5480 : sur le Wi-Fi pour la sonde, sur Thread pour le\n"
    "produit) avec la cle posee par l'USB : suivre les lignes, envoyer des\n"
    "commandes de la liste blanche, ou enregistrer une capture.\n\n"
    "Appareil (--appareil sonde|produit) : il choisit le fichier de cle,\n"
    "~/.config/hotte-<appareil>/cle. Pour 'cle', il se lit dans hello.base.build\n"
    "par l'USB si --appareil manque ; pour 'session' et 'enregistre', sonde par\n"
    "defaut (la cle signe la poignee de main : elle se choisit avant).\n",
)
remplacer(
    "      sure du C6 : DTR = RTS = 0 en un seul appel). La cle est rangee dans\n"
    "      ~/.config/hotte-sonde/cle (droits 0600 ; HOTTE_CLE=<fichier> pour un\n"
    "      autre), jamais affichee : seule son empreinte l'est. Toutes les sessions\n",
    "      sure du C6 : DTR = RTS = 0 en un seul appel). La cle est rangee dans\n"
    "      ~/.config/hotte-<appareil>/cle (droits 0600 ; HOTTE_CLE=<fichier> pour\n"
    "      un autre), jamais affichee : seule son empreinte l'est. Toutes les sessions\n",
)
remplacer(
    "Hote : hotte-sonde.local (mDNS), ou l'adresse de la sonde (commande 'info').\n",
    "Hote : hotte-sonde.local (mDNS) ou l'adresse IPv4 de la sonde ; pour le\n"
    "produit, son nom SRP (<nom>.local, bloc reseau/ip) ou une adresse IPv6 OMR\n"
    "(le Mac garde sa route Thread avec l'assistant de benq, tools/macos/halo-routes).\n",
)
remplacer(
    "  python3 tools/hotte_udp.py enregistre hotte-sonde.local krona-wifi \"capture tout\" \"seuils 1 19000\" --duree 660\n",
    "  python3 tools/hotte_udp.py enregistre hotte-sonde.local krona-wifi \"capture tout\" \"seuils 1 19000\" --duree 660\n"
    "  python3 tools/hotte_udp.py --appareil produit session fd11:22::1a2b --duree 30 \"hotte ventilo v2\"\n",
)
remplacer(
    'def chemin_cle():\n    """Fichier de la cle : HOTTE_CLE, sinon ~/.config/hotte-sonde/cle."""\n'
    '    return os.path.abspath(os.environ.get("HOTTE_CLE") or os.path.expanduser("~/.config/hotte-sonde/cle"))\n',
    'APPAREILS = ("sonde", "produit")\n\n\n'
    'def chemin_cle(appareil="sonde"):\n'
    '    """Fichier de la cle : HOTTE_CLE, sinon ~/.config/hotte-<appareil>/cle."""\n'
    "    if appareil not in APPAREILS:\n"
    '        raise SystemExit(f"appareil {appareil!r} : sonde ou produit")\n'
    '    return os.path.abspath(os.environ.get("HOTTE_CLE") or os.path.expanduser(f"~/.config/hotte-{appareil}/cle"))\n',
)
remplacer(
    "def ranger_cle(hex_key, chemin):\n",
    '''def lire_build(lire, ecrire, ident, delai_s=3.0, horloge=time.time):
    """'id=<ident> json hello' sur un port deja ouvert ; rend hello.base.build
    ('sonde', 'produit'), ou None sans reponse (firmware ancien, port muet)."""
    decoupe = serie_enregistre.Decoupe()
    ecrire(b"\\x15\\n")  # Ctrl-U : efface un reste de ligne
    decoupe.feed(lire(0.2))
    ecrire(f"id={ident} json hello\\n".encode("ascii"))
    build = None
    limite = horloge() + delai_s
    while horloge() < limite:
        for genre, _, m in decoupe.feed(lire(0.2)):
            if genre != "machine":
                continue
            if m.get("t") == "hello" and m.get("bloc") == "base" and m.get("build") in APPAREILS:
                build = m["build"]
            if m.get("t") == "reponse" and m.get("id") == ident and m.get("etape") == "fin":
                return build
    return build


def ranger_cle(hex_key, chemin):
''',
)
remplacer(
    "def cmd_cle(port, chemin):\n"
    "    alea = os.urandom(32).hex().upper()\n",
    "def cmd_cle(port, chemin=None, appareil=None):\n"
    "    \"\"\"Nouvelle cle par l'USB ; chemin nul : celui de l'appareil (--appareil,\n"
    "    ou hello.base.build lu sur le port).\"\"\"\n"
    "    alea = os.urandom(32).hex().upper()\n",
)
remplacer(
    "        ident = 900000 + int.from_bytes(os.urandom(2), \"big\") % 90000\n"
    "        cle, empreinte, msg = demander_cle(serie_enregistre.lecteur(fd), serie_enregistre.ecrivain(fd), alea, ident)\n",
    "        ident = 900000 + int.from_bytes(os.urandom(2), \"big\") % 90000\n"
    "        lire, ecrire = serie_enregistre.lecteur(fd), serie_enregistre.ecrivain(fd)\n"
    "        if chemin is None:\n"
    "            if appareil is None:\n"
    "                appareil = lire_build(lire, ecrire, ident + 1)\n"
    "                if appareil is None:\n"
    "                    raise SystemExit(\"appareil inconnu (aucun hello.base.build) : preciser --appareil sonde|produit\")\n"
    "            chemin = chemin_cle(appareil)\n"
    "        cle, empreinte, msg = demander_cle(lire, ecrire, alea, ident)\n",
)
remplacer(
    '        raise SystemExit(f"{hote} : {e} (nom mDNS hotte-sonde.local, ou adresse donnee par \'info\')")\n',
    '        raise SystemExit(f"{hote} : {e} (hotte-sonde.local, nom SRP du produit, ou adresse donnee par \'info\')")\n',
)
remplacer(
    'def resume(m):\n    """Une ligne lisible par message du profil hotte."""\n    t, b = m.get("t"), m.get("bloc")\n'
    '    tete = f"n={m.get(\'n\')} {t}" + (f"/{b}" if b else "")\n',
    'def resume(m):\n    """Une ligne lisible par message du profil hotte (sonde et produit)."""\n    t, b = m.get("t"), m.get("bloc")\n'
    '    tete = f"n={m.get(\'n\')} {t}" + (f"/{b}" if b else "")\n'
    "    produit = resume_produit(t, b, m)\n"
    "    if produit is not None:\n"
    "        return tete + produit\n",
)
remplacer(
    "def ouvrir_client(hote, port, commandes, key, afficher, demander):\n",
    '''def _lampe(v):
    return "?" if v is None else "on" if v else "off"


def resume_produit(t, b, m):
    """La fin de la ligne pour un message propre au produit ; None sinon."""
    if t == "etat" and b == "hotte":
        return (f" {m.get('marche')} moteur {m.get('moteur')} lampe {_lampe(m.get('lampe'))} "
                f"({m.get('confiance')}, {m.get('source')}, lu il y a {m.get('age_ms')} ms)")
    if t == "etat" and b == "thermique":
        return (f" puce {m.get('temp_c')} C (max {m.get('temp_max_c')}, pose {m.get('temp_max_pose_c')}), "
                f"seuil {m.get('seuil_c')} C{' ALERTE' if m.get('alerte') else ''}")
    if t == "etat" and b == "alim":
        voies = []
        for k in ("hotte", "module"):
            v = m.get(k)
            voies.append(f"{k} ?" if not isinstance(v, dict) else
                         f"{k} {v.get('mv')} mV (min {v.get('min_mv')}){' ALERTE' if v.get('alerte') else ''}")
        return " " + ", ".join(voies)
    if t == "hotte":
        av, ap = m.get("avant") or {}, m.get("apres") or {}
        return (f" {av.get('marche')}/{av.get('moteur')} -> {ap.get('marche')}/{ap.get('moteur')} "
                f"lampe {_lampe(ap.get('lampe'))} ({m.get('origine')}, {m.get('source')})")
    if t == "sequence":
        cause = f", {m['cause']}" if m.get("cause") else ""
        return (f" id={m.get('id')} {m.get('origine')} {m.get('sujet')} : {m.get('issue')}{cause} "
                f"({m.get('appuis')} appuis, {m.get('duree_ms')} ms)")
    if t == "alerte":
        return f" {m.get('sujet')} {m.get('etape')} : {m.get('valeur')} {m.get('unite')} (seuil {m.get('seuil')})"
    if t == "trame_d":
        return f" {m.get('origine')} {m.get('octets')}"
    return None


def ouvrir_client(hote, port, commandes, key, afficher, demander):
''',
)
remplacer(
    "def main(argv=None):\n    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)\n",
    "def main(argv=None):\n    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)\n"
    '    ap.add_argument("--appareil", choices=APPAREILS, help="sonde ou produit (fichier de cle ; cle : lu dans hello)")\n',
)
remplacer(
    '        a.add_argument("hote", help="hotte-sonde.local, ou l\'adresse IPv4 de la sonde")\n',
    '        a.add_argument("hote", help="hotte-sonde.local, nom SRP du produit (<nom>.local), ou adresse IPv4/IPv6")\n',
)
remplacer(
    '    if args.cmd == "cle":\n        cmd_cle(args.port, chemin_cle())\n'
    "    elif args.cmd == \"session\":\n"
    "        cmd_session(args.hote, args.port, args.commandes, args.duree, args.brut, lire_cle(chemin_cle()))\n"
    "    else:\n"
    "        cmd_enregistre(args.hote, args.port, args.scenario, args.commandes, args.duree, args.dossier,\n"
    "                       lire_cle(chemin_cle()))\n",
    '    if args.cmd == "cle":\n'
    '        cmd_cle(args.port, chemin_cle(args.appareil) if os.environ.get("HOTTE_CLE") else None, args.appareil)\n'
    "    elif args.cmd == \"session\":\n"
    "        cmd_session(args.hote, args.port, args.commandes, args.duree, args.brut,\n"
    "                    lire_cle(chemin_cle(args.appareil or \"sonde\")))\n"
    "    else:\n"
    "        cmd_enregistre(args.hote, args.port, args.scenario, args.commandes, args.duree, args.dossier,\n"
    "                       lire_cle(chemin_cle(args.appareil or \"sonde\")))\n",
)
open(P, "w", encoding="utf-8").write(c)
print("hotte_udp.py : produit ajoute")

EOF
```

Sortie attendue :

```
hotte_udp.py : produit ajoute
```

- [ ] **Étape 4 : Lancer les tests Python : ils passent**

```bash
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

Attendu :

```
Ran 175 tests in 9.289s
OK
```

- [ ] **Étape 5 : Committer**

```bash
git add tools/hotte_udp.py tools/tests/test_hotte_udp.py
git commit -m "$(cat <<'FIN'
Ajouter le produit a hotte_udp.py : cle d'apres l'appareil, resume du profil produit

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
FIN
)"
git push
```

