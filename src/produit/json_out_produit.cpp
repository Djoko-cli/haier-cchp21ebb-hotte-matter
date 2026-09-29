// Copie de src/json_out.cpp de la sonde (commit 1cb2c7c), elle-meme copiee de
// benq-screenbar-halo-matter@c58a506 (adapte : profil du produit, sans
// capture ni injection ; messages et liste blanche du produit)
#include "json_out_produit.h"

#include <string.h>

namespace jsonp {

// ===========================================================================
//  Writer
// ===========================================================================

void Writer::put(char c) {
  if (len_ < kLineMax) buf_[len_++] = (uint8_t)c;
  else over_ = true;
}

void Writer::puts(const char *s) {
  while (*s) put(*s++);
}

// Virgule avant tout champ sauf le premier de son niveau, puis la cle.
void Writer::sep(const char *k) {
  if (!depth_) {
    bad_ = true;
    return;
  }
  if (!first_[depth_ - 1]) put(',');
  first_[depth_ - 1] = false;
  if (k) {
    put('"');
    puts(k);
    put('"');
    put(':');
  }
}

void Writer::num(uint32_t v, bool neg) {
  char t[10];
  uint8_t i = 0;
  do {
    t[i++] = (char)('0' + v % 10);
    v /= 10;
  } while (v);
  if (neg) put('-');
  while (i) put(t[--i]);
}

void Writer::begin(const char *type, uint32_t n, uint32_t ms) {
  len_ = 0;
  over_ = bad_ = false;
  depth_ = 1;
  first_[0] = true;
  close_[0] = '}';
  put((char)kRS);
  put('{');
  u32("v", kVersion);
  str("t", type);
  u32("n", n);
  u32("ms", ms);
}

void Writer::str(const char *k, const char *v, size_t max) {
  sep(k);
  if (!v) {
    puts("null");
    return;
  }
  put('"');
  for (size_t i = 0; v[i] && i < max; i++) {
    const uint8_t c = (uint8_t)v[i];
    if (c == '"' || c == '\\') {
      put('\\');
      put((char)c);
    } else if (c < 0x20 || c > 0x7E) {
      put('?');  // jamais de \uXXXX, jamais d'octet hors ASCII imprimable
    } else {
      put((char)c);
    }
  }
  put('"');
}

void Writer::u32(const char *k, uint32_t v) {
  sep(k);
  num(v, false);
}

void Writer::u64(const char *k, uint64_t v) {
  sep(k);
  char t[20];  // 18446744073709551615 : 20 chiffres
  uint8_t i = 0;
  do {
    t[i++] = (char)('0' + v % 10);
    v /= 10;
  } while (v);
  while (i) put(t[--i]);
}

void Writer::i32(const char *k, int32_t v) {
  sep(k);
  if (v < 0) num((uint32_t)(-(int64_t)v), true);
  else num((uint32_t)v, false);
}

void Writer::boolean(const char *k, bool v) {
  sep(k);
  puts(v ? "true" : "false");
}

void Writer::null(const char *k) {
  sep(k);
  puts("null");
}

static const char kHexDigits[] = "0123456789ABCDEF";

void Writer::hex(const char *k, const uint8_t *p, size_t n) {
  sep(k);
  put('"');
  for (size_t i = 0; i < n; i++) {
    put(kHexDigits[p[i] >> 4]);
    put(kHexDigits[p[i] & 0x0F]);
  }
  put('"');
}

void Writer::hexU32(const char *k, uint32_t v, uint8_t digits, bool prefix0x) {
  sep(k);
  put('"');
  if (prefix0x) puts("0x");
  if (digits > 8) digits = 8;
  for (int8_t i = (int8_t)digits - 1; i >= 0; i--) put(kHexDigits[(v >> (4 * i)) & 0x0F]);
  put('"');
}

void Writer::open(const char *k, char o, char c) {
  sep(k);
  put(o);
  if (depth_ >= kDepth) {
    bad_ = true;
    return;
  }
  first_[depth_] = true;
  close_[depth_] = c;
  depth_++;
}

void Writer::obj(const char *k) { open(k, '{', '}'); }
void Writer::arr(const char *k) { open(k, '[', ']'); }

void Writer::end() {
  if (depth_ <= 1) {
    bad_ = true;
    return;
  }
  depth_--;
  put(close_[depth_]);
}

bool Writer::finish() {
  if (depth_ != 1) bad_ = true;
  put('}');
  put('\n');
  depth_ = 0;
  return !over_ && !bad_;
}

bool Writer::setN(uint32_t n) {
  if (over_ || bad_ || depth_) return false;
  // RS {"v":1,"t":"<type>","n":<chiffres>,... : le type est un litteral du
  // firmware, sans '"' ; le premier ,"n": est donc le bon.
  static const char kKey[] = ",\"n\":";
  const size_t kl = sizeof(kKey) - 1;
  size_t p = 0;
  while (p + kl <= len_ && memcmp(buf_ + p, kKey, kl)) p++;
  if (p + kl > len_) return false;
  p += kl;
  size_t q = p;
  while (q < len_ && buf_[q] >= '0' && buf_[q] <= '9') q++;
  char d[10];
  size_t dn = 0;
  do {
    d[dn++] = (char)('0' + n % 10);
    n /= 10;
  } while (n);
  const size_t newLen = len_ - (q - p) + dn;
  if (q == p || newLen > kLineMax) return false;
  memmove(buf_ + p + dn, buf_ + q, len_ - q);
  for (size_t i = 0; i < dn; i++) buf_[p + i] = (uint8_t)d[dn - 1 - i];
  len_ = newLen;
  return true;
}

// ===========================================================================
//  Messages
// ===========================================================================

void heartbeat(Writer &w, uint32_t n, uint32_t ms, uint32_t boot, uint32_t upS, uint32_t lost) {
  w.begin("hb", n, ms);
  w.hexU32("boot", boot, 8);
  w.u32("up_s", upS);
  w.u32("json_perdus", lost);
}

void sessionEnd(Writer &w, uint32_t n, uint32_t ms, const char *cause) {
  w.begin("fin", n, ms);
  w.str("cause", cause);
}

void logLine(Writer &w, uint32_t n, uint32_t ms, const char *src, const char *niv, const char *txt,
             uint32_t skipped) {
  w.begin("log", n, ms);
  w.str("src", src);
  w.str("niv", niv);
  w.str("txt", txt, kLogTextMax);
  if (skipped) w.u32("sautes", skipped);
}

// ---------------------------------------------------------------------------
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

void reply(Writer &w, uint32_t n, uint32_t ms, const Reply &r) {
  w.begin("reponse", n, ms);
  w.u32("id", r.id);
  w.str("etape", r.fin ? "fin" : "debut");
  w.str("cmd", r.cmd ? r.cmd : "", kCmdTextMax);
  w.boolean("ok", r.ok);
  w.str("code", r.code);
  if (r.msg) w.str("msg", r.msg, kMsgMax);
  if (r.fin) w.u32("duree_ms", r.durMs);
  if (r.suite == Reply::SuiteSequence) w.str("suite", "sequence");
  if (r.hasLease) {
    w.u32("bail_s", r.leaseS);
    w.u32("up_s", r.upS);
  }
  if (r.key) w.str("cle", r.key, 64);
  if (r.hasKid) w.str("empreinte", r.kid, 8);
}

// ===========================================================================
//  Lignes de l'hote : commande affichee (reponse.cmd), liste blanche
// ===========================================================================

// Mot i (0, 1, ...) de s, separe par des espaces ; nullptr s'il manque.
static const char *word(const char *s, uint8_t i, size_t *len) {
  *len = 0;
  for (;;) {
    while (*s == ' ') s++;
    if (!*s) return nullptr;
    const char *w = s;
    while (*s && *s != ' ') s++;
    if (!i--) {
      *len = (size_t)(s - w);
      return w;
    }
  }
}

static bool wordIs(const char *w, size_t n, const char *k) { return w && strlen(k) == n && !strncmp(w, k, n); }

// Entier decimal, lu comme le lit l'aiguillage (strtoul : zeros de tete
// compris, au-dela de 4294967295 sature) ; false si le mot n'en est pas un
// (l'aiguillage dira usage). Sans cela, 'bail 0000000000' passerait la liste
// blanche puis vaudrait 0.
static bool wordNum(const char *w, size_t n, uint32_t *v) {
  if (!w || !n) return false;
  uint64_t x = 0;
  for (size_t i = 0; i < n; i++) {
    if (w[i] < '0' || w[i] > '9') return false;
    x = x * 10 + (uint64_t)(w[i] - '0');
    if (x > 0xFFFFFFFFull) x = 0xFFFFFFFFull + 1;  // sature, sans debordement
  }
  *v = x > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)x;
  return true;
}

void copyCmd(char out[kCmdTextMax + 1], const char *cmd) {
  size_t i = 0;
  for (; cmd && cmd[i] && i < kCmdTextMax; i++) out[i] = cmd[i];
  out[i] = 0;
}

void maskCmd(char *shown) {
  size_t n0, n1, n2;
  const char *w0 = word(shown, 0, &n0), *w1 = word(shown, 1, &n1), *w2 = word(shown, 2, &n2);
  if (wordIs(w0, n0, "json") && wordIs(w1, n1, "cle") && wordIs(w2, n2, "nouvelle")) strcpy(shown, "json cle nouvelle");
}

const char *remoteRefusal(const char *cmd, bool essai) {
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

// ===========================================================================
//  Lignes de l'hote : prefixe id=, assemblage
// ===========================================================================

bool parseIdPrefix(char *line, uint32_t *id, char **rest) {
  *rest = line;
  char *p = line;
  while (*p == ' ') p++;
  if (strncmp(p, "id=", 3)) return false;
  p += 3;
  uint32_t v = 0;
  uint8_t digits = 0;
  while (*p >= '0' && *p <= '9') {
    if (++digits > 9) return false;
    v = v * 10 + (uint32_t)(*p++ - '0');
  }
  if (!digits || v < 1 || v > kIdMax || (*p && *p != ' ')) return false;
  while (*p == ' ') p++;
  *id = v;
  *rest = p;
  return true;
}

LineAssembler::Ev LineAssembler::feed(uint8_t c, bool machine) {
  if (c == '\r') return Ev::None;  // CRLF de l'hote : le CR est ignore
  if (c == '\n') {
    buf_[len_] = 0;
    return Ev::Line;
  }
  if (c == kCtrlU) {
    cleared_ = len_;
    len_ = 0;
    tooLong_ = false;
    return Ev::Clear;
  }
  if (c == 8 || c == 127) {  // retour arriere
    if (!len_) return Ev::None;
    len_--;
    return Ev::Erase;
  }
  if (machine && (c < 0x20 || c > 0x7E)) return Ev::None;
  if (len_ >= kCmdMax) {
    tooLong_ = true;
    return Ev::None;
  }
  buf_[len_++] = (char)c;
  return Ev::Echo;
}

char *LineAssembler::text() {
  buf_[len_] = 0;
  return buf_;
}

// ===========================================================================
//  Debit
// ===========================================================================

bool RateCap::available(uint32_t now) {
  if (!started_ || now - winAt_ >= 1000) {
    started_ = true;
    winAt_ = now;
    count_ = 0;
  }
  return count_ < limit_;
}

bool Cadence::allow(uint32_t now) {
  // at_[idx_] : la plus ancienne des kLines dernieres lignes acceptees.
  if (n_ >= kLines && now - at_[idx_] < kWindowMs) return false;
  at_[idx_] = now;
  idx_ = (uint8_t)((idx_ + 1) % kLines);
  if (n_ < kLines) n_++;
  return true;
}

// ===========================================================================
//  File
// ===========================================================================

bool Queue::has(Item item) const {
  for (uint8_t i = 0; i < n_; i++)
    if (q_[(head_ + i) % kN].item == item) return true;
  return false;
}

bool Queue::push(Item item, uint32_t now, bool session, uint8_t arg) {
  if (item != Item::Reply) {
    for (uint8_t i = 0; i < n_; i++) {
      Queued &q = q_[(head_ + i) % kN];
      if (q.item != item) continue;
      // Demande explicite : survit a la fin du mode machine, et son retard
      // part d'elle (la ligne est formatee a l'envoi : rien n'est perime).
      if (!session) {
        q.session = false;
        q.at = now;
      }
      return true;
    }
  }
  if (n_ >= kN) return false;
  q_[(head_ + n_) % kN] = Queued{item, arg, session, now};
  n_++;
  return true;
}

void Queue::pop() {
  if (!n_) return;
  head_ = (uint8_t)((head_ + 1) % kN);
  n_--;
}

uint8_t Queue::dropLate(uint32_t now, uint32_t lateMs) {
  uint8_t dropped = 0;
  while (n_) {
    const Queued &q = q_[head_];
    if (q.item == Item::Reply || now - q.at <= lateMs) break;
    pop();
    dropped++;
  }
  return dropped;
}

bool Queue::frontReady(int room) const {
  if (!n_) return false;
  const size_t need = q_[head_].item == Item::Reply ? kLineMax : 2 * kLineMax;
  return room >= 0 && (size_t)room >= need;
}

uint8_t Queue::dropSession() {
  Queued keep[kN];
  uint8_t k = 0, dropped = 0;
  for (uint8_t i = 0; i < n_; i++) {
    const Queued &q = q_[(head_ + i) % kN];
    if (q.session) dropped++;
    else keep[k++] = q;
  }
  for (uint8_t i = 0; i < k; i++) q_[i] = keep[i];
  head_ = 0;
  n_ = k;
  return dropped;
}

// ===========================================================================
//  Bail
// ===========================================================================

bool leaseExpired(uint32_t now, uint32_t lastRx, uint32_t lastCmd, uint16_t leaseS) {
  if (!leaseS) return false;
  const uint32_t last = (int32_t)(lastRx - lastCmd) > 0 ? lastRx : lastCmd;
  return now - last >= (uint32_t)leaseS * 1000u;
}

// ===========================================================================
//  Cache des reponses
// ===========================================================================

void ReplyCache::clear() {
  // Sur place : un objet temporaire coute pres d'un Ko de pile a la tache loop.
  for (Entry &e : e_) e.used = false;
  next_ = 0;
}

void ReplyCache::put(const Reply &r) {
  if (!r.fin || !r.id) return;
  // Une seule entree par id : la plus recente (un id repete ne s'execute pas,
  // mais une reponse differee peut suivre une reponse immediate du meme id).
  for (Entry &e : e_)
    if (e.used && e.r.id == r.id) e.used = false;
  Entry &e = e_[next_];
  next_ = (uint8_t)((next_ + 1) % kN);
  e.used = true;
  e.r = r;
  copyCmd(e.cmd, r.cmd ? r.cmd : "");
  e.r.cmd = e.cmd;
  e.r.msg = nullptr;
  e.r.key = nullptr;
  e.r.hasKid = false;
  e.r.kid = nullptr;
}

const Reply *ReplyCache::find(uint32_t id) const {
  if (!id) return nullptr;
  for (const Entry &e : e_)
    if (e.used && e.r.id == id) return &e.r;
  return nullptr;
}

}  // namespace jsonp
