// Copie de benq-screenbar-halo-matter@c58a506 : tools/host_tests/test_json.cpp (adapte : sans halo1, verif.h, u64, trame, injection, liste blanche hotte, masque wifi, tests regroupes par cycle)
//
// Tests hote des briques du protocole compagnon (src/json_out.*), profil
// hotte : ecrivain (u64 compris), echappement, ordre des champs, tailles,
// renumerotation, evenements trame et injection, reponse, pires cas sous le
// budget de 896 octets, commande affichee (reponse.cmd, masquage), liste
// blanche a distance, lignes de l'hote, plafonds de debit, cadence, file des
// periodiques, bail, cache des reponses.
// Lancer : sh tools/tests/test_hote.sh
//
// Avec un chemin en argument, les messages realistes produits (evenements,
// reponses ; ni les essais de l'ecrivain, ni les pires cas artificiels) y sont
// ecrits (RS + JSON + LF), pour tools/json_check.py.
#include <stdio.h>
#include <string.h>

#include <initializer_list>
#include <string>

#include "capture_model.h"
#include "json_out.h"
#include "verif.h"

using namespace jsonp;

static FILE *gCapture = nullptr;
static bool gCaptureOn = false;  // seulement les messages realistes
static Writer gW;

// Ferme la ligne et la rend telle qu'elle partirait (RS ... LF).
static std::string finish(Writer &w, bool *ok = nullptr) {
  const bool good = w.finish();
  if (ok) *ok = good;
  std::string s((const char *)w.data(), w.size());
  if (good && gCapture && gCaptureOn) fwrite(s.data(), 1, s.size(), gCapture);
  return s;
}

static std::string framed(const char *json) { return std::string("\x1e") + json + "\n"; }

static void expectLine(Writer &w, const char *json, const char *what) {
  bool ok = false;
  const std::string got = finish(w, &ok);
  const std::string want = framed(json);
  if (!ok || got != want) printf("  %s :\n  obtenu  %s  attendu %s", what, got.c_str() + (got.empty() ? 0 : 1), want.c_str() + 1);
  VERIF(ok && got == want);
}

// ---------------------------------------------------------------------------
//  Ecrivain
// ---------------------------------------------------------------------------

static void testWriter() {
  gW.begin("x", 5, 7);
  expectLine(gW, "{\"v\":1,\"t\":\"x\",\"n\":5,\"ms\":7}", "enveloppe seule");

  // Ordre v, t, n, ms, puis bloc ; entiers extremes ; imbrication.
  gW.begin("etat", 4294967295u, 4294967295u);
  gW.str("bloc", "bus");
  gW.i32("neg", -2147483647 - 1);
  gW.i32("pos", 2147483647);
  gW.u32("zero", 0);
  gW.obj("o");
  gW.arr("a");
  gW.u32(nullptr, 1);
  gW.str(nullptr, "b");
  gW.obj(nullptr);
  gW.boolean("t", true);
  gW.null("z");
  gW.end();
  gW.end();
  gW.boolean("f", false);
  gW.end();
  gW.arr("vide");
  gW.end();
  expectLine(gW,
             "{\"v\":1,\"t\":\"etat\",\"n\":4294967295,\"ms\":4294967295,\"bloc\":\"bus\",\"neg\":-2147483648,"
             "\"pos\":2147483647,\"zero\":0,\"o\":{\"a\":[1,\"b\",{\"t\":true,\"z\":null}],\"f\":false},\"vide\":[]}",
             "imbrication et entiers extremes");

  // Echappement : '"' et '\', octets hors 0x20..0x7E -> '?', jamais de \u.
  gW.begin("log", 1, 2);
  gW.str("s", "a\"b\\c\x01\x1e\x7f\xc3\xa9 ~");
  gW.str("tronq", "abcdefgh", 3);
  gW.str("tronq_esc", "\"\"\"\"", 2);  // max compte les caracteres de la valeur, pas l'echappement
  gW.str("nul", nullptr);
  expectLine(gW,
             "{\"v\":1,\"t\":\"log\",\"n\":1,\"ms\":2,\"s\":\"a\\\"b\\\\c????? ~\",\"tronq\":\"abc\",\"tronq_esc\":"
             "\"\\\"\\\"\",\"nul\":null}",
             "echappement");

  // Hexa.
  const uint8_t b[3] = {0x0A, 0xFF, 0x2E};
  gW.begin("h", 0, 0);
  gW.hex("b", b, 3);
  gW.hex("vide", b, 0);
  gW.hexU32("boot", 0x3FA2C901, 8);
  gW.hexU32("pan", 0x1A2B, 4, true);
  gW.hexU32("petit", 0x5, 4, true);
  expectLine(gW,
             "{\"v\":1,\"t\":\"h\",\"n\":0,\"ms\":0,\"b\":\"0AFF2E\",\"vide\":\"\",\"boot\":\"3FA2C901\",\"pan\":\"0x1A2B\","
             "\"petit\":\"0x0005\"}",
             "hexa");

  // Mal ferme : jamais emis.
  bool ok = true;
  gW.begin("x", 0, 0);
  gW.obj("o");
  finish(gW, &ok);
  VERIF(!ok);
  gW.begin("x", 0, 0);
  gW.end();
  finish(gW, &ok);
  VERIF(!ok);

  // Taille : exactement 1024 octets (RS et LF compris) passe, 1025 non.
  for (int extra = 0; extra < 2; extra++) {
    gW.begin("x", 0, 0);
    // enveloppe : RS {"v":1,"t":"x","n":0,"ms":0 = 1 + 27 ; ,"p":"..." = 7 + len ; } LF = 2
    const size_t head = 1 + strlen("{\"v\":1,\"t\":\"x\",\"n\":0,\"ms\":0");
    const size_t pad = kLineMax - head - 7 - 2 + (size_t)extra;
    std::string s(pad, 'a');
    gW.str("p", s.c_str(), pad);
    const bool good = gW.finish();
    VERIF(good == (extra == 0) && (!good || gW.size() == kLineMax));
    VERIF(gW.size() <= kLineMax);
  }
  // Un depassement enorme reste borne et refuse.
  gW.begin("x", 0, 0);
  for (int i = 0; i < 300; i++) gW.u32("k", 4294967295u);
  VERIF(!gW.finish() && gW.overflow() && gW.size() <= kLineMax);
}

// Entier sur 64 bits (t_us) : decimal, sans zero de tete, jusqu'a 2^64-1.
static void testU64() {
  gW.begin("x", 0, 0);
  gW.u64("zero", 0);
  gW.u64("un", 1);
  gW.u64("u32_plus_1", 4294967296ull);
  gW.u64("sur_json", 9007199254740991ull);  // 2^53-1 : entier sur en JSON
  gW.u64("max", 18446744073709551615ull);
  gW.arr("a");
  gW.u64(nullptr, 1234567890123ull);
  gW.end();
  expectLine(gW,
             "{\"v\":1,\"t\":\"x\",\"n\":0,\"ms\":0,\"zero\":0,\"un\":1,\"u32_plus_1\":4294967296,"
             "\"sur_json\":9007199254740991,\"max\":18446744073709551615,\"a\":[1234567890123]}",
             "u64");
}

// Un evenement formate une fois, renumerote pour chaque session (setN).
static void testRenumerote() {
  gW.begin("trame", 0, 4242);
  gW.u32("num", 1);
  const bool fin = gW.finish();
  VERIF(fin && gW.setN(123456));
  VERIF(std::string((const char *)gW.data(), gW.size()) ==
        framed("{\"v\":1,\"t\":\"trame\",\"n\":123456,\"ms\":4242,\"num\":1}"));
  VERIF(gW.setN(7) && std::string((const char *)gW.data(), gW.size()) ==
                          framed("{\"v\":1,\"t\":\"trame\",\"n\":7,\"ms\":4242,\"num\":1}"));
  gW.begin("trame", 0, 1);
  VERIF(!gW.setN(3));  // ligne pas fermee
}

// ---------------------------------------------------------------------------
//  Evenements de la sonde
// ---------------------------------------------------------------------------

static capt::Partie partie(uint32_t num, uint32_t part, bool fin, uint64_t tUs, bool niv0Haut,
                           std::initializer_list<uint32_t> dur) {
  capt::Partie p;
  p.num = num;
  p.part = part;
  p.fin = fin;
  p.tUs = tUs;
  p.niv0Haut = niv0Haut;
  for (uint32_t d : dur) p.dur[p.n++] = d;
  return p;
}

static void testTrame() {
  // Une reception d'une partie, mode tout : ni rep, ni sautes.
  capt::Partie p = partie(12, 0, true, 81234567, false, {1500, 750, 750, 2250});
  trame(gW, 305, 81240, p, false, 0, 0);
  expectLine(gW,
             "{\"v\":1,\"t\":\"trame\",\"n\":305,\"ms\":81240,\"num\":12,\"part\":0,\"fin\":true,\"t_us\":81234567,"
             "\"niv0\":\"bas\",\"dur_us\":[1500,750,750,2250],\"debord\":false}",
             "trame simple");
  // Partie suivante d'une longue reception, apres une perte, mode changements.
  p = partie(13, 1, false, 5000000123ull, true, {100, 200});
  p.debord = true;
  trame(gW, 306, 81241, p, true, 7, 3);
  expectLine(gW,
             "{\"v\":1,\"t\":\"trame\",\"n\":306,\"ms\":81241,\"num\":13,\"part\":1,\"fin\":false,"
             "\"t_us\":5000000123,\"niv0\":\"haut\",\"dur_us\":[100,200],\"debord\":true,\"rep\":7,\"sautes\":3}",
             "trame avec rep et sautes");
  // Mode changements : rep present meme nul ; partie vide qui clot une reception.
  p = partie(13, 2, true, 5000000423ull, true, {});
  trame(gW, 307, 81242, p, true, 0, 0);
  expectLine(gW,
             "{\"v\":1,\"t\":\"trame\",\"n\":307,\"ms\":81242,\"num\":13,\"part\":2,\"fin\":true,"
             "\"t_us\":5000000423,\"niv0\":\"haut\",\"dur_us\":[],\"debord\":false,\"rep\":0}",
             "trame vide de fin, rep nul");
}

static void testInjection() {
  const uint32_t dur[4] = {750, 750, 750, 2250};
  const uint32_t relu[4] = {752, 748, 751, 2249};
  InjectionEv e;
  e.id = 42;
  e.cmd = "injecte durees 750 750 750 2250";
  e.dur = dur;
  e.n = 4;
  e.attenteUs = 20412;
  e.relu = relu;
  e.nRelu = 4;
  injection(gW, 900, 123456, e);
  expectLine(gW,
             "{\"v\":1,\"t\":\"injection\",\"n\":900,\"ms\":123456,\"id\":42,\"cmd\":\"injecte durees 750 750 750 2250\","
             "\"resultat\":\"ok\",\"niv0\":\"bas\",\"dur_us\":[750,750,750,2250],\"attente_us\":20412,"
             "\"relu_us\":[752,748,751,2249]}",
             "injection ok");
  // Sans id (console) : null ; le bus ne s'est pas tu : rien d'emis, rien de relu.
  InjectionEv d;
  d.cmd = "injecte durees 750";
  d.resultat = "delai";
  d.dur = dur;
  d.n = 1;
  d.attenteUs = 1000000;
  injection(gW, 901, 124456, d);
  expectLine(gW,
             "{\"v\":1,\"t\":\"injection\",\"n\":901,\"ms\":124456,\"id\":null,\"cmd\":\"injecte durees 750\","
             "\"resultat\":\"delai\",\"niv0\":\"bas\",\"dur_us\":[750],\"attente_us\":1000000,\"relu_us\":[]}",
             "injection delai sans id");
  // cmd tronquee a 40 caracteres (espace final compris) ; collision : relu plus court que l'emission.
  e.id = 43;
  e.cmd = "injecte durees 750 750 750 2250 750 750 750 2250";
  e.resultat = "collision";
  e.nRelu = 2;
  injection(gW, 902, 125000, e);
  expectLine(gW,
             "{\"v\":1,\"t\":\"injection\",\"n\":902,\"ms\":125000,\"id\":43,\"cmd\":\"injecte durees 750 750 750 2250 "
             "750 750 \",\"resultat\":\"collision\",\"niv0\":\"bas\",\"dur_us\":[750,750,750,2250],\"attente_us\":20412,"
             "\"relu_us\":[752,748]}",
             "injection collision, cmd tronquee");
}

static void testReply() {
  Reply r;
  r.id = 1;
  r.cmd = "json 1";
  r.durMs = 12;
  r.hasLease = true;
  r.leaseS = 30;
  r.upS = 83;
  reply(gW, 11, 83523, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":11,\"ms\":83523,\"id\":1,\"etape\":\"fin\",\"cmd\":\"json 1\",\"ok\":true,"
             "\"code\":\"ok\",\"duree_ms\":12,\"bail_s\":30,\"up_s\":83}",
             "reponse json 1");
  r = Reply();
  r.id = 5;
  r.cmd = "injecte durees 750 750";
  r.code = "accepte";
  r.durMs = 1;
  r.suite = Reply::SuiteInjection;
  reply(gW, 71, 95002, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":71,\"ms\":95002,\"id\":5,\"etape\":\"fin\",\"cmd\":\"injecte durees 750 750\","
             "\"ok\":true,\"code\":\"accepte\",\"duree_ms\":1,\"suite\":\"injection\"}",
             "reponse accepte, suite injection");
  r = Reply();
  r.id = 6;
  r.cmd = "injecte durees 750";
  r.ok = false;
  r.code = "refuse";
  r.msg = "non armee";
  reply(gW, 72, 95010, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":72,\"ms\":95010,\"id\":6,\"etape\":\"fin\",\"cmd\":\"injecte durees 750\","
             "\"ok\":false,\"code\":\"refuse\",\"msg\":\"non armee\",\"duree_ms\":0}",
             "reponse refus");
  r = Reply();
  r.id = 8;
  r.fin = false;
  r.cmd = "stats";
  r.code = "en_cours";
  reply(gW, 530, 180002, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":530,\"ms\":180002,\"id\":8,\"etape\":\"debut\",\"cmd\":\"stats\","
             "\"ok\":true,\"code\":\"en_cours\"}",
             "reponse debut");
  r.fin = true;
  r.code = "execute";
  r.durMs = 7;
  reply(gW, 531, 180009, r);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":531,\"ms\":180009,\"id\":8,\"etape\":\"fin\",\"cmd\":\"stats\","
             "\"ok\":true,\"code\":\"execute\",\"duree_ms\":7}",
             "reponse fin");
}

static void testEvents() {
  logLine(gW, 640, 200100, "capture", "notice", "[reglages] 1 valeur hors bornes en NVS : valeur par defaut", 0);
  expectLine(gW,
             "{\"v\":1,\"t\":\"log\",\"n\":640,\"ms\":200100,\"src\":\"capture\",\"niv\":\"notice\",\"txt\":\"[reglages] "
             "1 valeur hors bornes en NVS : valeur par defaut\"}",
             "log");
  heartbeat(gW, 641, 202000, 0x3FA2C901, 202, 0);
  expectLine(gW, "{\"v\":1,\"t\":\"hb\",\"n\":641,\"ms\":202000,\"boot\":\"3FA2C901\",\"up_s\":202,\"json_perdus\":0}", "hb");
  sessionEnd(gW, 662, 231400, "bail");
  expectLine(gW, "{\"v\":1,\"t\":\"fin\",\"n\":662,\"ms\":231400,\"cause\":\"bail\"}", "fin");
}

// Reponses de 'json cle' : cle (une fois) et empreinte.
static void testReponsesCle() {
  Reply k;
  k.id = 3;
  k.cmd = "json cle nouvelle";
  k.durMs = 4;
  k.key = "20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F";
  k.hasKid = true;
  k.kid = "630DCD29";
  reply(gW, 40, 5000, k);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":40,\"ms\":5000,\"id\":3,\"etape\":\"fin\",\"cmd\":"
             "\"json cle nouvelle\",\"ok\":true,\"code\":\"ok\",\"duree_ms\":4,"
             "\"cle\":\"20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F\",\"empreinte\":\"630DCD29\"}",
             "reponse json cle nouvelle");
  k = Reply();
  k.id = 4;
  k.cmd = "json cle";
  k.hasKid = true;  // sans cle : empreinte null
  reply(gW, 41, 5001, k);
  expectLine(gW,
             "{\"v\":1,\"t\":\"reponse\",\"n\":41,\"ms\":5001,\"id\":4,\"etape\":\"fin\",\"cmd\":\"json cle\","
             "\"ok\":true,\"code\":\"ok\",\"duree_ms\":0,\"empreinte\":null}",
             "reponse json cle sans cle");
}

// ---------------------------------------------------------------------------
//  Pires cas : chaque message tient dans le budget de 896 octets
// ---------------------------------------------------------------------------

static void checkBudget(const char *what, size_t *worst) {
  bool ok = false;
  const std::string s = finish(gW, &ok);
  if (!ok || s.size() > kBudget) printf("  %s : %zu octets (budget %zu)\n", what, s.size(), kBudget);
  VERIF(ok && s.size() <= kBudget);
  if (s.size() > *worst) *worst = s.size();
}

static void testWorstCases() {
  const uint32_t M = 4294967295u;
  size_t worst = 0;
  // trame : 110 durees de 32767 (1 MHz) puis de 65534 (500 kHz), compteurs au
  // maximum, t_us a 2^53-1, rep et sautes au maximum.
  capt::Partie p;
  p.num = p.part = M;
  p.fin = false;
  p.tUs = 9007199254740991ull;
  p.niv0Haut = true;
  p.debord = false;
  p.n = capt::kDurMax;
  for (uint32_t d : {32767u, 65534u}) {
    for (uint16_t i = 0; i < p.n; i++) p.dur[i] = d;
    trame(gW, M, M, p, true, M, M);
    checkBudget("trame", &worst);
  }
  printf("  pire trame : %zu octets (budget %zu)\n", worst, kBudget);
  // injection, a la borne maximale admise (total_max_us <= 200 000 us, tol_us
  // <= 500 us) : 64 durees de somme 199 000 us, 15 de 10000 et 49 de 1000 (le
  // plus de chiffres possible sous 200 000 us), relues a +500 us (+tol_us),
  // id, attente, n et ms au maximum.
  uint32_t dur[64], relu[64];
  uint32_t somme = 0;
  for (int i = 0; i < 64; i++) {
    dur[i] = i < 15 ? 10000 : 1000;
    relu[i] = dur[i] + 500;
    somme += dur[i];
  }
  VERIF(somme == 199000);
  // Pire cas artificiel : cmd de 40 '"', tous echappes (une commande 'injecte'
  // acceptee n'en contient jamais) ; relu_us est coupe pour tenir le budget.
  const std::string q(200, '"');
  InjectionEv e;
  e.id = kIdMax;
  e.cmd = q.c_str();
  e.resultat = "collision";
  e.niv0Haut = true;
  e.dur = dur;
  e.n = 64;
  e.attenteUs = M;
  e.relu = relu;
  e.nRelu = 64;
  injection(gW, M, M, e);
  checkBudget("injection", &worst);
  // Commande reelle de 40 caracteres, a la meme borne : relu_us n'est jamais
  // tronque (64 relues), et la ligne tient le budget.
  e.cmd = "injecte durees 10000 10000 10000 10000 10";
  injection(gW, M, M, e);
  {
    bool ok = false;
    const std::string s = finish(gW, &ok);
    size_t virgules = 0;
    for (size_t i = s.find("\"relu_us\""); i < s.size(); i++) virgules += s[i] == ',';
    VERIF(ok && s.size() <= kBudget && virgules + 1 == 64 && s.find(",10500,1500,") != std::string::npos);
  }
  // Durees absurdes (bogue de l'appelant) : relu_us tronque au budget, dur_us
  // borne a kInjDurMax, la ligne reste emise (1024 octets au plus).
  uint32_t big[100];
  for (uint32_t &d : big) d = M;
  e.dur = e.relu = big;
  e.n = e.nRelu = 100;
  injection(gW, M, M, e);
  bool ok = false;
  const std::string s = finish(gW, &ok);
  VERIF(ok && s.size() <= kLineMax && s.find("\"relu_us\":[]") != std::string::npos);
  // reponse : cmd et msg a leur longueur maximale, entierement echappes, tous les champs.
  const std::string bs(200, '\\');
  Reply rp;
  rp.id = kIdMax;
  rp.cmd = q.c_str();
  rp.ok = false;
  rp.code = "deja_traite";
  rp.msg = bs.c_str();
  rp.durMs = M;
  rp.suite = Reply::SuiteInjection;
  rp.hasLease = true;
  rp.leaseS = rp.upS = M;
  rp.key = "20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F";
  rp.hasKid = true;
  rp.kid = "630DCD29";
  reply(gW, M, M, rp);
  checkBudget("reponse", &worst);
  // log : 191 caracteres, tous echappes.
  const std::string lq(400, '\\');
  logLine(gW, M, M, "capture", "notice", lq.c_str(), M);
  checkBudget("log", &worst);
  heartbeat(gW, M, M, M, M, M);
  checkBudget("hb", &worst);
  sessionEnd(gW, M, M, "commande");
  checkBudget("fin", &worst);
  printf("  pire cas des evenements : %zu octets (budget %zu)\n", worst, kBudget);
}

// ---------------------------------------------------------------------------
//  Commande affichee (reponse.cmd) et liste blanche a distance
// ---------------------------------------------------------------------------

// reponse.cmd : tronquee a 40 caracteres, jamais un secret.
static void testCommandeAffichee() {
  char shown[kCmdTextMax + 1];
  copyCmd(shown, "injecte durees 750 750 750 2250 et encore des mots pour depasser");
  VERIF(strlen(shown) == kCmdTextMax && !strncmp(shown, "injecte durees 750 750 750 2250 et encor", kCmdTextMax));
  // Jamais l'alea de 'json cle nouvelle'.
  copyCmd(shown, "json cle nouvelle 000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "json cle nouvelle");
  copyCmd(shown, "json cle efface");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "json cle efface");
  // Jamais le mot de passe du Wi-Fi, meme tronque par copyCmd.
  copyCmd(shown, "wifi maison motdepasse-secret");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifi maison");
  copyCmd(shown, "wifi 0123456789ABCDEF0123456789ABCDEF motdepasse");
  VERIF_EGAL_STR(shown, "wifi 0123456789ABCDEF0123456789ABCDEF mo");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifi 0123456789ABCDEF0123456789ABCDEF");
  // Reseau ouvert (sans mdp), 'wifi' seul, autre commande : inchanges.
  copyCmd(shown, "wifi maison");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifi maison");
  copyCmd(shown, "wifi");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifi");
  copyCmd(shown, "wifis maison secret");
  maskCmd(shown);
  VERIF_EGAL_STR(shown, "wifis maison secret");
}

// Transport reseau : liste blanche du profil hotte.
static void testRemote() {
  struct Case {
    const char *cmd;
    bool ok;
  };
  static const Case kCases[] = {
      {"json 1", true},
      {"json 1 bail 30", true},
      {"json 1 bail 10", true},
      {"json 1 bail 120", true},
      {"json 1 bail 0", false},
      {"json 1 bail 9", false},
      {"json 1 bail 121", false},
      {"json 1 bail x", true},  // usage, dit par l'aiguillage
      {"json 1 bail 0000000000", false},
      {"json 1 bail 0000000600", false},
      {"json 1 bail 00000000030", true},
      {"json 1 bail 99999999999", false},
      {"json periode 0000000200", false},
      {"json compteurs 0000000200", false},
      {"json reseau 00000001000", false},
      {"json periode 00000002000", true},
      {"json 0", true},
      {"json etat", true},
      {"json hello", true},
      {"json ping", true},
      {"json periode 2000", true},
      {"json periode 1999", false},
      {"json periode 0", false},
      {"json compteurs 0", true},
      {"json compteurs 999", false},
      {"json compteurs 1000", true},
      {"json compteurs 60000", true},
      {"json reseau 0", true},
      {"json reseau 9999", false},
      {"json reseau 10000", true},
      {"json trames 1", true},
      {"json trames 0", true},
      {"json log 0", true},
      {"json", false},
      {"json cle", false},
      {"json cle efface", false},
      {"json cle nouvelle 00", false},
      {"capture on", true},
      {"capture off", true},
      {"capture tout", true},
      {"capture  changements", true},
      {"capture", false},
      {"capture on 2", false},
      {"capture pause", false},
      {"seuils 2 8000", true},
      {"seuils 0", true},
      {"seuils", false},
      {"injection on", true},
      {"injection off", true},
      {"injection", false},
      {"injection monte 1", false},
      {"injection on 1", false},
      {"injecte durees 750 750", true},
      {"injecte", false},
      {"wifi maison secret", false},
      {"info", false},
      {"bus", false},
      {"stats", false},
      {"help", false},
      {"reboot", false},
      {"lampe on", false},
      {"led test", false},
      {"", false},
      {"   ", false},
  };
  for (const Case &c : kCases) {
    const char *why = remoteRefusal(c.cmd);
    if ((why == nullptr) != c.ok) printf("  remoteRefusal('%s') : %s\n", c.cmd, why ? why : "permise");
    VERIF((why == nullptr) == c.ok);
    VERIF(!why || strlen(why) <= kMsgMax);
  }
}

// ---------------------------------------------------------------------------
//  Lignes de l'hote
// ---------------------------------------------------------------------------

static bool idOf(const char *in, uint32_t *id, std::string *rest) {
  char buf[160];
  snprintf(buf, sizeof(buf), "%s", in);
  char *r = nullptr;
  const bool ok = parseIdPrefix(buf, id, &r);
  *rest = r;
  return ok;
}

static void testIdPrefix() {
  uint32_t id = 0;
  std::string rest;
  VERIF(idOf("id=17 capture tout", &id, &rest) && id == 17 && rest == "capture tout");
  VERIF(idOf("  id=1   json 1", &id, &rest) && id == 1 && rest == "json 1");
  VERIF(idOf("id=999999999 json ping", &id, &rest) && id == 999999999 && rest == "json ping");
  VERIF(idOf("id=5", &id, &rest) && id == 5 && rest.empty());
  VERIF(idOf("id=007 x", &id, &rest) && id == 7);
  VERIF(!idOf("id=0 json 1", &id, &rest) && rest == "id=0 json 1");
  VERIF(!idOf("id=1000000000 json 1", &id, &rest));
  VERIF(!idOf("id=0000000001 x", &id, &rest));
  VERIF(!idOf("id= 5 x", &id, &rest));
  VERIF(!idOf("id=17capture", &id, &rest));
  VERIF(!idOf("id=-3 x", &id, &rest));
  VERIF(!idOf("capture id=3", &id, &rest));
  VERIF(!idOf("ID=3 x", &id, &rest));
}

static std::string feedAll(LineAssembler &a, const char *bytes, size_t n, bool machine, int *lines,
                           std::string *echo) {
  std::string last;
  for (size_t i = 0; i < n; i++) {
    const LineAssembler::Ev e = a.feed((uint8_t)bytes[i], machine);
    if (e == LineAssembler::Ev::Echo && echo) *echo += bytes[i];
    if (e == LineAssembler::Ev::Line) {
      last = a.text();
      last += a.tooLong() ? "|trop long" : "";
      (*lines)++;
      a.reset();
    }
  }
  return last;
}

static void testAssembler() {
  LineAssembler a;
  int lines = 0;
  std::string echo;
  const char in1[] = "id=1 json 1\r\n";
  VERIF(feedAll(a, in1, sizeof(in1) - 1, true, &lines, &echo) == "id=1 json 1" && lines == 1);
  // Mode machine : RS, echappements et octets hauts ignores.
  lines = 0;
  const char in2[] = "bu\x1e" "s\x1b\x01\xff\x80 x\n";
  VERIF(feedAll(a, in2, sizeof(in2) - 1, true, &lines, nullptr) == "bus x" && lines == 1);
  // Mode humain : tout octet est garde (sauf CR, LF, Ctrl-U, retour arriere).
  lines = 0;
  const char in3[] = "a\x01" "b\n";
  VERIF(feedAll(a, in3, sizeof(in3) - 1, false, &lines, nullptr) == std::string("a\x01" "b") && lines == 1);
  // Ctrl-U vide la ligne ; retour arriere.
  lines = 0;
  const char in4[] = "reste d'une session\x15" "\nid=2 json pinh\x08g\n";
  a.reset();
  std::string last = feedAll(a, in4, sizeof(in4) - 1, true, &lines, nullptr);
  VERIF(lines == 2 && last == "id=2 json ping");
  a.reset();
  a.feed('a', false);
  a.feed('b', false);
  VERIF(a.feed(kCtrlU, false) == LineAssembler::Ev::Clear && a.cleared() == 2 && a.length() == 0);
  VERIF(a.feed(8, false) == LineAssembler::Ev::None);
  // 127 caracteres passent, 128 marquent la ligne trop longue ; la suivante repart.
  for (int extra = 0; extra < 2; extra++) {
    a.reset();
    lines = 0;
    std::string s(kCmdMax + (size_t)extra, 'x');
    s += "\n";
    last = feedAll(a, s.c_str(), s.size(), true, &lines, nullptr);
    VERIF(lines == 1 && (extra ? last.size() == kCmdMax + strlen("|trop long") : last.size() == kCmdMax));
  }
  lines = 0;
  last = feedAll(a, "ok\n", 3, true, &lines, nullptr);
  VERIF(last == "ok");
  // Ctrl-U efface aussi le depassement.
  a.reset();
  std::string s(200, 'y');
  s += "\x15" "json\n";
  lines = 0;
  last = feedAll(a, s.c_str(), s.size(), true, &lines, nullptr);
  VERIF(last == "json");
}

// ---------------------------------------------------------------------------
//  Debit, cadence, file
// ---------------------------------------------------------------------------

static void testRate() {
  RateCap cap(50);
  uint32_t t = 0u - 300;  // a travers le retour a zero de millis()
  unsigned passed = 0;
  for (int i = 0; i < 80; i++) {
    if (cap.available(t)) {
      cap.take();
      passed++;
    } else {
      cap.skip();
    }
    t += 5;  // 80 evenements en 400 ms
  }
  VERIF(passed == 50 && cap.takeSkipped() == 30 && cap.takeSkipped() == 0);
  t += 1000;
  VERIF(cap.available(t));
  // Cadence : 20 lignes par seconde glissante.
  Cadence c;
  uint32_t now = 1000;
  int ok = 0;
  for (int i = 0; i < 25; i++) ok += c.allow(now + (uint32_t)i * 10);  // 25 lignes en 250 ms
  VERIF(ok == 20);
  VERIF(!c.allow(now + 999));
  VERIF(c.allow(now + 1000));
  VERIF(!c.allow(now + 1001));
  VERIF(c.allow(now + 1010));
}

static void testQueue() {
  Queue q;
  VERIF(q.push(Item::EtatBus, 0, true) && q.push(Item::EtatSys, 1, true) && q.size() == 2);
  VERIF(q.push(Item::EtatBus, 2, true) && q.size() == 2);  // doublon ignore
  VERIF(q.push(Item::Reply, 3, false, 1) && q.push(Item::Reply, 3, false, 2) && q.size() == 4);  // reponses jamais fondues
  VERIF(q.push(Item::EtatSys, 4, false) && q.size() == 4);  // doublon explicite
  VERIF(q.dropSession() == 1 && q.size() == 3);
  const Queued *f = q.front();
  VERIF(f && f->item == Item::EtatSys && !f->session && f->at == 4);  // promue, gardee a sa place
  q.pop();
  f = q.front();
  VERIF(f && f->item == Item::Reply && f->arg == 1);
  q.clear();
  int pushed = 0;
  for (int i = 0; i < 40; i++) pushed += q.push(Item::Reply, (uint32_t)i, false, (uint8_t)i);
  VERIF(pushed == Queue::kN && q.size() == Queue::kN);
  VERIF(!q.push(Item::Config, 50, false));
  for (int i = 0; i < 5; i++) q.pop();
  VERIF(q.push(Item::Config, 51, false) && q.has(Item::Config));
  uint8_t prev = 4;
  bool order = true;
  while (const Queued *e = q.front()) {
    if (e->item == Item::Reply) {
      order &= e->arg == prev + 1;
      prev = e->arg;
    }
    q.pop();
  }
  VERIF(order);  // FIFO a travers le tour de l'anneau
}

// Retard (500 ms) et place libre : les deux gardes de l'envoi des periodiques.
static void testQueueDrain() {
  Queue q;
  VERIF(!q.frontReady(4096) && q.dropLate(1000) == 0);
  q.push(Item::EtatBus, 0, true);
  q.push(Item::EtatSys, 100, true);
  q.push(Item::Reply, 200, false, 0);
  q.push(Item::Compteurs, 250, true);
  VERIF(q.dropLate(500) == 0 && q.size() == 4);
  VERIF(q.dropLate(700) == 2 && q.front()->item == Item::Reply);
  VERIF(q.dropLate(1000000) == 0 && q.size() == 2);  // une reponse n'est jamais perdue pour retard
  VERIF(!q.frontReady(kLineMax - 1) && q.frontReady(kLineMax));
  VERIF(!q.frontReady(-1));
  q.pop();
  VERIF(q.front()->item == Item::Compteurs && q.dropLate(1000000) == 1 && !q.size());
  q.push(Item::NetIp, 2000, true);
  VERIF(!q.frontReady(2 * kLineMax - 1) && q.frontReady(2 * kLineMax));
  q.clear();
  // Fusion : une demande explicite repart de maintenant, pas une periodique.
  q.push(Item::EtatSys, 0, true);
  q.push(Item::EtatSys, 450, false);
  VERIF(q.size() == 1 && !q.front()->session && q.front()->at == 450);
  VERIF(q.dropLate(950) == 0 && q.dropLate(951) == 1);
  q.push(Item::EtatBus, 0, true);
  q.push(Item::EtatBus, 400, true);
  VERIF(q.front()->at == 0 && q.dropLate(501) == 1);
  q.clear();
  q.push(Item::HelloBase, 0, false);
  q.push(Item::HelloBase, 300, true);
  VERIF(!q.front()->session && q.front()->at == 0);
}

static void testLease() {
  VERIF(!leaseExpired(1000000, 0, 0, 0));
  VERIF(!leaseExpired(30999, 1000, 500, 30) && leaseExpired(31000, 1000, 500, 30));
  VERIF(!leaseExpired(90000, 1000, 70000, 30) && leaseExpired(100000, 1000, 70000, 30));
  const uint32_t rx = 0xFFFFF000u;
  VERIF(!leaseExpired(rx + 29999u, rx, rx - 0x1000u, 30) && leaseExpired(rx + 30000u, rx, rx - 0x1000u, 30));
  VERIF(!leaseExpired(9999, 0, 0, 10) && leaseExpired(600000, 0, 0, 600));
}

// ---------------------------------------------------------------------------
//  Cache des reponses (sessions reseau)
// ---------------------------------------------------------------------------

// Un id repete recoit la meme reponse ; ni msg ni cle.
static void testCache() {
  ReplyCache cache;
  VERIF(!cache.find(1));
  Reply r;
  r.id = 5;
  r.cmd = "injecte durees 750 750";
  r.code = "accepte";
  r.msg = "attente du silence";
  r.suite = Reply::SuiteInjection;
  cache.put(r);
  const Reply *got = cache.find(5);
  VERIF(got && got->suite == Reply::SuiteInjection && !got->msg && !strcmp(got->cmd, r.cmd) && got->cmd != r.cmd);
  Reply debut = r;
  debut.id = 6;
  debut.fin = false;
  cache.put(debut);
  VERIF(!cache.find(6));  // etape debut jamais gardee
  for (uint32_t id = 10; id < 10 + ReplyCache::kN; id++) {
    r.id = id;
    cache.put(r);
  }
  VERIF(!cache.find(5) && cache.find(10) && cache.find(10 + ReplyCache::kN - 1));
  r.id = 12;
  r.code = "ok";
  cache.put(r);
  VERIF(cache.find(12) && !strcmp(cache.find(12)->code, "ok"));
  cache.clear();
  VERIF(!cache.find(12));
}

int main(int argc, char **argv) {
  if (argc > 1) gCapture = fopen(argv[1], "wb");
  testWriter();
  testU64();
  testRenumerote();
  gCaptureOn = true;
  testTrame();
  testInjection();
  testReply();
  testEvents();
  testReponsesCle();
  gCaptureOn = false;
  testWorstCases();
  testCommandeAffichee();
  testRemote();
  testIdPrefix();
  testAssembler();
  testRate();
  testQueue();
  testQueueDrain();
  testLease();
  testCache();
  if (gCapture) fclose(gCapture);
  return bilan("test_json");
}
