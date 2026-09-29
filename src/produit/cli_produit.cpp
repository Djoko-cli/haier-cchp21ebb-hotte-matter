// Copie partielle de src/cli.cpp de la sonde (commit 1cb2c7c), elle-meme reprise
// de benq-screenbar-halo-matter@c58a506 (adapte : runLine, cliPoll et
// cliRunRemote repris ; commandes du produit)
// ===========================================================================
//  Console du produit sur l'USB (docs/SPEC-PRODUIT.md 7.5) : texte humain,
//  'help' liste tout. Les commandes interdites a distance le sont par
//  jsonp::remoteRefusal (json_out_produit), pas par la table.
//
//  Lignes de l'hote (docs/PROTOCOLE-JSON-PRODUIT.md) : un prefixe id=<n> fait
//  la semantique (runLine). Avec id, 'json ...', 'hotte ventilo|lampe|regle',
//  'simu <reglage>', 'matter <reglage>', 'essai ...', 'radio rafale' et
//  'reboot' repondent sans texte (reponse ok, accepte, usage ou refuse) ; les
//  autres commandes gardent leur texte, entre une reponse debut et une
//  reponse fin.
// ===========================================================================
#include "cli_produit.h"

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <esp_mac.h>
#include <string.h>

#include "alim.h"
#include "config_produit.h"
#include "fw_version.h"
#include "json_mode_produit.h"
#include "json_out_produit.h"
#include "matter_hotte.h"
#include "net_udp_thread.h"
#include "produit.h"

typedef void (*Handler)(char *args);  // args : ce qui suit le mot-cle (peut etre "")
struct Commande {
  const char *nom;
  Handler h;
  const char *aide;
};

// ---------------------------------------------------------------------------
//  Outils
// ---------------------------------------------------------------------------

// Detache le premier mot de s et rend le reste (jamais nul, espaces de tete sautes).
static char *splitWord(char *s) {
  char *sp = strchr(s, ' ');
  if (!sp) return s + strlen(s);
  *sp++ = 0;
  while (*sp == ' ') sp++;
  return sp;
}

// Entier decimal strict : chiffres seulement, 4294967295 au plus.
static bool lireU32(const char *s, uint32_t *v) {
  if (!*s) return false;
  uint64_t x = 0;
  for (const char *p = s; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    x = x * 10 + (uint64_t)(*p - '0');
    if (x > 0xFFFFFFFFull) return false;
  }
  *v = (uint32_t)x;
  return true;
}

// Resultat d'une commande sans texte : reponse.code et reponse.msg avec un
// id, texte humain sans id.
struct Resultat {
  bool ok;
  const char *code;  // ok, accepte, usage, refuse
  const char *msg;   // nul : rien a dire ; 120 caracteres au plus (reponse.msg)
};

static uint32_t sRebootAt = 0;  // redemarrage differe (0 : aucun) : la reponse part d'abord

// ---------------------------------------------------------------------------
//  hotte ventilo | lampe | regle
// ---------------------------------------------------------------------------

static const char kUsageHotte[] = "usage : hotte [ventilo off|v1|v2|v3|derniere | lampe on|off | regle <nom> <valeur>]";

// Ordre a l'automate (7.5) : asynchrone. Accepte : l'evenement sequence suit,
// avec l'id de la commande.
static Resultat faireOrdre(char *args, uint32_t id) {
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  if (*reste) return {false, "usage", kUsageHotte};
  const uint32_t now = millis();
  if (!strcmp(args, "ventilo")) {
    hotte::CibleVentilo c = hotte::CibleVentilo::Aucune;
    if (!strcmp(valeur, "off")) c = hotte::CibleVentilo::Eteint;
    else if (!strcmp(valeur, "v1")) c = hotte::CibleVentilo::V1;
    else if (!strcmp(valeur, "v2")) c = hotte::CibleVentilo::V2;
    else if (!strcmp(valeur, "v3")) c = hotte::CibleVentilo::V3;
    else if (!strcmp(valeur, "derniere")) c = hotte::CibleVentilo::Derniere;
    else return {false, "usage", "usage : hotte ventilo off|v1|v2|v3|derniere"};
    produitAutomate().ordreVentilo(c, jsonOrdreApp(id), hotte::Canal::App, now);
    return {true, "accepte", nullptr};
  }
  if (!strcmp(args, "lampe")) {
    if (strcmp(valeur, "on") && strcmp(valeur, "off")) return {false, "usage", "usage : hotte lampe on|off"};
    produitAutomate().ordreLampe(!strcmp(valeur, "on"), jsonOrdreApp(id), hotte::Canal::App, now);
    return {true, "accepte", nullptr};
  }
  return {false, "usage", kUsageHotte};
}

// hotte regle <nom> <valeur> : automate (4.2) ou surveillance ; verifie avant la NVS.
static Resultat faireRegle(char *args) {
  static char msg[jsonp::kMsgMax + 1];
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  uint32_t v = 0;
  if (!*args || !lireU32(valeur, &v) || *reste)
    return {false, "usage", "usage : hotte regle <nom> <valeur> ('hotte' : noms et valeurs)"};
  uint32_t lo = 0, hi = 0;
  hotte::Params a = produitAutomate().params();
  switch (hotte::reglerParam(&a, args, v)) {
    case hotte::Reglage::Ok:
      if (!produitReglerAutomate(a)) return {false, "refuse", "hotte regle : applique mais pas enregistre (NVS)"};
      jsonConfigChanged();
      return {true, "ok", nullptr};
    case hotte::Reglage::HorsBornes:
      hotte::bornesParam(args, &lo, &hi);
      snprintf(msg, sizeof(msg), "hotte regle : %s %lu hors bornes (%lu..%lu ; calme <= plafond)", args,
               (unsigned long)v, (unsigned long)lo, (unsigned long)hi);
      return {false, "usage", msg};
    case hotte::Reglage::NomInconnu: break;
  }
  surv::Params s = produitSurveillance().params();
  switch (surv::reglerParam(&s, args, v)) {
    case surv::Reglage::Ok:
      if (!produitReglerSurveillance(s)) return {false, "refuse", "hotte regle : applique mais pas enregistre (NVS)"};
      jsonConfigChanged();
      return {true, "ok", nullptr};
    case surv::Reglage::HorsBornes:
      surv::bornesParam(args, &lo, &hi);
      snprintf(msg, sizeof(msg), "hotte regle : %s %lu hors bornes (%lu..%lu ; hysteresis < seuil)", args,
               (unsigned long)v, (unsigned long)lo, (unsigned long)hi);
      return {false, "usage", msg};
    case surv::Reglage::NomInconnu: break;
  }
  snprintf(msg, sizeof(msg), "hotte regle : parametre %s inconnu ('hotte' : noms)", args);
  return {false, "usage", msg};
}

static void afficherHotte() {
  const uint32_t now = millis();
  const hotte::Automate &a = produitAutomate();
  const hotte::Etat &e = a.etat();
  const hotte::Diagnostic d = a.diagnostic(now);
  Serial.printf("hotte : marche %s, moteur %s, lampe %s ; confiance %s, source %s", hotte::texte(e.marche),
                hotte::texte(e.moteur), e.lampeConnue ? (e.lampe ? "allumee" : "eteinte") : "inconnue",
                hotte::texte(e.confiance), hotte::texte(e.source));
  if (e.luMs) Serial.printf(", lu il y a %lu ms\n", (unsigned long)(now - e.luMs));
  else Serial.println(", rien lu depuis le demarrage");
  Serial.printf("  ligne %s, lecture annexe %s, pilote %s ; derniere vitesse %s ; marche %s\n",
                hotte::texte(a.mode()), a.annexe() ? "oui" : "non", d.piloteEnService ? "en service" : "HORS SERVICE",
                hotte::texte(a.derniereVitesse()), d.marcheAutorisee ? "permise (arret lu)" : "attend un arret lu");
  if (d.ventilo) Serial.printf("  ordre ventilo : %s\n", d.cibleEteint ? "eteint" : hotte::texte(d.cibleMoteur));
  if (d.lampe) Serial.printf("  ordre lampe : %s\n", d.cibleLampe ? "on" : "off");
  if (d.enVol) Serial.printf("  appui en vol : %s, essai %u\n", hotte::texte(d.touche), (unsigned)d.essai);
  const hotte::Compteurs &c = d.c;
  Serial.printf("  appuis panneau %lu, module %lu ; sequences ok %lu, annulees %lu, remplacees %lu, abandons %lu, "
                "nouveaux essais %lu\n",
                (unsigned long)c.appuisPanneau, (unsigned long)c.appuisModule, (unsigned long)c.reussies,
                (unsigned long)c.annulees, (unsigned long)c.remplacees, (unsigned long)c.abandons,
                (unsigned long)c.nouveauxEssais);
  Serial.print("  echecs :");
  for (uint8_t i = 1; i < hotte::kNbCauses; i++)
    Serial.printf(" %s %lu", hotte::texte((hotte::Cause)i), (unsigned long)c.echecs[i]);
  Serial.println();
  Serial.print("  reglages :");
  for (uint8_t i = 0; i < hotte::kNbParams; i++)
    Serial.printf(" %s %lu", hotte::nomParam(i), (unsigned long)hotte::valeurParam(a.params(), i));
  const surv::Params &s = produitSurveillance().params();
  for (uint8_t i = 0; i < surv::kNbParams; i++)
    Serial.printf(" %s %lu", surv::nomParam(i), (unsigned long)surv::valeurParam(s, i));
  Serial.println();
}

// ---------------------------------------------------------------------------
//  simu ... (build simule, USB seulement)
// ---------------------------------------------------------------------------

#if PILOTE_SIMULE
static bool lireTouche(const char *s, hotte::Touche *t) {
  static const hotte::Touche kTouches[] = {hotte::Touche::Marche, hotte::Touche::Lumiere, hotte::Touche::V1,
                                           hotte::Touche::V2, hotte::Touche::V3};
  for (hotte::Touche x : kTouches)
    if (!strcmp(s, hotte::texte(x))) {
      *t = x;
      return true;
    }
  return false;
}

// eteinte | armee | v1..v3 | prol-v1..prol-v3, puis [lampe]
static bool lireEtatHotte(char *s, sim::EtatHotte *e) {
  char *lampe = splitWord(s);
  char *reste = splitWord(lampe);
  if (*reste || (*lampe && strcmp(lampe, "lampe"))) return false;
  sim::EtatHotte x;
  x.lampe = *lampe != 0;
  const bool prol = !strncmp(s, "prol-", 5);
  const char *v = prol ? s + 5 : s;
  if (!strcmp(s, "eteinte")) {
    x.marche = hotte::Marche::Eteinte;
  } else if (!strcmp(s, "armee")) {
    x.marche = hotte::Marche::Armee;
  } else if (!strcmp(v, "v1") || !strcmp(v, "v2") || !strcmp(v, "v3")) {
    x.marche = prol ? hotte::Marche::Prolongee : hotte::Marche::Armee;
    x.moteur = (hotte::Moteur)(v[1] - '0');
  } else {
    return false;
  }
  *e = x;
  return sim::etatHotteValide(x);
}

static const char kUsageSimu[] =
    "usage : simu [mode repete|changements|appuis | annexe 0|1 | latence|periode|prolongee <ms> | fin eteinte|armee"
    " | inconnues rien|direct | appui <touche> | coupure | illisible|sans_effet|perdu|delai <n> | collision <n> [prend]"
    " | refuse 0|1 | hs 0|1 | apres_demarrage <etat> [lampe] | auto <s>]";

static Resultat faireSimu(char *args) {
  static char msg[jsonp::kMsgMax + 1];
  sim::PiloteSimule &p = produitSimu();
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  const uint32_t now = millis();
  sim::Reglages r = p.reglages();
  uint32_t n = 0;
  if (!strcmp(args, "appui") && !*reste) {
    hotte::Touche t;
    if (!lireTouche(valeur, &t)) return {false, "usage", "usage : simu appui marche|lumiere|v1|v2|v3"};
    p.appuiPanneau(t, now);
    return {true, "ok", nullptr};
  }
  if (!strcmp(args, "auto") && !*reste && lireU32(valeur, &n) && n <= 3600) {
    produitSimuAuto(n);  // banc B11 : 0 arrete
    if (!n) return {true, "ok", "endurance arretee"};
    snprintf(msg, sizeof(msg), "endurance : un appui du panneau toutes les %lu s", (unsigned long)n);
    return {true, "ok", msg};
  }
  if (!strcmp(args, "coupure") && !*valeur) {
    p.coupure(now);
    return {true, "ok", "hotte simulee : coupure secteur, revenue eteinte"};
  }
  if (!strcmp(args, "apres_demarrage")) {
    sim::EtatHotte e;
    char etat[40];
    snprintf(etat, sizeof(etat), "%s%s%s", valeur, *reste ? " " : "", reste);
    if (!lireEtatHotte(etat, &e)) return {false, "usage", "usage : simu apres_demarrage eteinte|armee|v1..v3|prol-v1..v3 [lampe]"};
    if (!produitSimuApresDemarrage(e)) return {false, "refuse", "simu apres_demarrage : echec de l'ecriture en NVS"};
    return {true, "ok", "hotte simulee : cet etat au prochain demarrage, une fois"};
  }
  if (*reste && strcmp(args, "collision")) return {false, "usage", kUsageSimu};
  if (!strcmp(args, "mode")) {
    if (!strcmp(valeur, "repete")) r.mode = hotte::ModeEtat::Repete;
    else if (!strcmp(valeur, "changements")) r.mode = hotte::ModeEtat::Changements;
    else if (!strcmp(valeur, "appuis")) r.mode = hotte::ModeEtat::AppuisSeuls;
    else return {false, "usage", "usage : simu mode repete|changements|appuis"};
  } else if (!strcmp(args, "annexe") && (!strcmp(valeur, "0") || !strcmp(valeur, "1"))) {
    r.annexe = !strcmp(valeur, "1");
  } else if (!strcmp(args, "latence") && lireU32(valeur, &n)) {
    r.latenceMs = n;
  } else if (!strcmp(args, "periode") && lireU32(valeur, &n)) {
    r.periodeMs = n;
  } else if (!strcmp(args, "prolongee") && lireU32(valeur, &n)) {
    r.prolongeeMs = n;
  } else if (!strcmp(args, "fin") && (!strcmp(valeur, "eteinte") || !strcmp(valeur, "armee"))) {
    r.finArmee = !strcmp(valeur, "armee");
  } else if (!strcmp(args, "inconnues") && (!strcmp(valeur, "rien") || !strcmp(valeur, "direct"))) {
    r.inconnues = !strcmp(valeur, "direct") ? sim::Inconnues::Direct : sim::Inconnues::Rien;
  } else if ((!strcmp(args, "illisible") || !strcmp(args, "sans_effet") || !strcmp(args, "perdu") ||
              !strcmp(args, "delai") || !strcmp(args, "collision")) &&
             lireU32(valeur, &n) && n <= 255) {
    const bool prend = !strcmp(reste, "prend");
    if (*reste && !prend) return {false, "usage", "usage : simu collision <n> [prend]"};
    if (!strcmp(args, "illisible")) p.illisibles((uint8_t)n);
    else if (!strcmp(args, "sans_effet")) p.sansEffet((uint8_t)n);
    else if (!strcmp(args, "perdu")) p.perdus((uint8_t)n);
    else if (!strcmp(args, "delai")) p.delais((uint8_t)n);
    else p.collisions((uint8_t)n, prend);
    return {true, "ok", nullptr};
  } else if (!strcmp(args, "refuse") && (!strcmp(valeur, "0") || !strcmp(valeur, "1"))) {
    p.refuser(!strcmp(valeur, "1"));
    jsonConfigChanged();
    return {true, "ok", nullptr};
  } else if (!strcmp(args, "hs") && (!strcmp(valeur, "0") || !strcmp(valeur, "1"))) {
    p.horsService(!strcmp(valeur, "1"), now);
    jsonConfigChanged();
    return {true, "ok", nullptr};
  } else {
    return {false, "usage", kUsageSimu};
  }
  if (!sim::reglagesValides(r)) {
    snprintf(msg, sizeof(msg), "simu %s : hors bornes (latence 0..5000, periode 100..60000, prolongee 60000..3600000)",
             args);
    return {false, "usage", msg};
  }
  if (!produitReglerSimu(r)) return {false, "refuse", "simu : applique mais pas enregistre (NVS)"};
  return {true, "ok", nullptr};
}

static void afficherSimu() {
  const sim::PiloteSimule &p = produitSimu();
  const sim::EtatHotte h = p.hotte();
  const sim::Reglages &r = p.reglages();
  Serial.printf("hotte simulee : marche %s, moteur %s, lampe %s%s%s", hotte::texte(h.marche), hotte::texte(h.moteur),
                h.lampe ? "allumee" : "eteinte", p.refuse() ? ", REFUSE les appuis du module" : "",
                p.horsService() ? ", pilote HORS SERVICE" : "");
  if (produitSimuAutoS()) Serial.printf(", endurance : un appui toutes les %lu s", (unsigned long)produitSimuAutoS());
  Serial.println();
  Serial.printf("  mode %s, annexe %s, latence %lu ms, periode %lu ms, trame %lu ms, prolongee %lu ms (fin %s), "
                "inconnues %s\n",
                hotte::texte(r.mode), r.annexe ? "oui" : "non", (unsigned long)r.latenceMs, (unsigned long)r.periodeMs,
                (unsigned long)r.trameMs, (unsigned long)r.prolongeeMs, r.finArmee ? "armee" : "eteinte",
                sim::texte(r.inconnues));
  ligne::Diagnostic d;
  p.diagnostic(&d);
  Serial.printf("  emis %lu, refus %lu, collisions %lu, delais %lu, etats lus %lu, annexe %lu, anomalies %lu, "
                "appuis vus %lu, perdus %lu\n",
                (unsigned long)d.appuisEmis, (unsigned long)d.refusGarde, (unsigned long)d.collisions,
                (unsigned long)d.delais, (unsigned long)d.etatsLus, (unsigned long)d.lecturesAnnexe,
                (unsigned long)d.anomalies, (unsigned long)d.appuisVus, (unsigned long)d.evenementsPerdus);
}
#endif

// ---------------------------------------------------------------------------
//  matter <reglage>, essai, radio rafale
// ---------------------------------------------------------------------------

static const char kUsageMatter[] = "usage : matter [med 0|1 | tx 8..20 | maxint 0|10..3600 | derniere 0|1 | journal]";

static Resultat faireMatter(char *args) {
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  uint32_t v = 0;
  bool saved = false;
  if (*reste || !lireU32(valeur, &v)) return {false, "usage", kUsageMatter};
  bool ok = false;
  const char *apres = nullptr;
  if (!strcmp(args, "med")) {
    ok = matterReglerMed(v, &saved);
    apres = "role applique au prochain demarrage ('reboot')";
  } else if (!strcmp(args, "tx")) {
    ok = matterReglerTx(v, &saved);
  } else if (!strcmp(args, "maxint")) {
    ok = matterReglerMaxint(v, &saved);
    apres = "pour les abonnements neufs";
  } else if (!strcmp(args, "derniere")) {
    ok = v <= 1 && matterReglerDerniere(v == 1, &saved);
  }
  if (!ok) return {false, "usage", kUsageMatter};
  jsonConfigChanged();
  if (!saved) return {false, "refuse", "matter : applique mais pas enregistre (NVS)"};
  return {true, "ok", apres};
}

static Resultat faireEssai(char *args) {
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  if (strcmp(args, "alim") || (strcmp(valeur, "on") && strcmp(valeur, "off")) || *reste)
    return {false, "usage", "usage : essai alim on|off"};
  if (!produitReglerEssai(!strcmp(valeur, "on"))) return {false, "refuse", "essai : echec de l'ecriture en NVS"};
  return {true, "ok", strcmp(valeur, "on") ? "essai de 24 h desarme" : "essai de 24 h arme pour 72 h : 'radio rafale' permise a distance"};
}

static Resultat faireRafale(char *args) {
  char *valeur = splitWord(args);
  char *reste = splitWord(valeur);
  uint32_t s = 0;
  if (strcmp(args, "rafale") || !lireU32(valeur, &s) || *reste || s < 1 || s > 30)
    return {false, "usage", "usage : radio rafale <1..30 s>"};
  if (!produitEssai()) return {false, "refuse", "radio rafale : seulement pendant l'essai de 24 h ('essai alim on')"};
  const uint8_t o = jsonOrigin();
  if (o == jsonp::kUsb || !netUdpRafale((uint8_t)(o - 1), s))
    return {false, "refuse", "radio rafale : depuis une session reseau etablie seulement"};
  return {true, "ok", nullptr};
}

// ---------------------------------------------------------------------------
//  Commandes
// ---------------------------------------------------------------------------

static void imprimer(const Resultat &r) {
  if (r.msg) Serial.println(r.msg);
  else Serial.println(r.ok ? "ok" : r.code);
}

static void cmdHelp(char *args);  // apres la table, qu'elle parcourt

static void cmdInfo(char *) {
  uint8_t mac[8] = {};
  Serial.printf("firmware %s (env %s), IDF %s, Arduino %s\n", FW_VERSION_FULL, FW_ENV, esp_get_idf_version(),
                ESP_ARDUINO_VERSION_STR);
  if (esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK)
    Serial.printf("MAC %02X:%02X:%02X:%02X:%02X:%02X, serie HOTTE-%02X%02X%02X%02X%02X%02X\n", mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("pilote %s ; GPIO%u (injection) tenue basse ; ecoute GPIO%u\n", PILOTE_LIGNE_TEXTE,
                (unsigned)kPinInjectionD, (unsigned)kPinEcouteD);
  char kid[9];
  if (netUdpKid(kid)) Serial.printf("udp : port %u, cle %s\n", (unsigned)kPortUdp, kid);
  else Serial.printf("udp : port %u ferme, aucune cle ('python3 tools/hotte_udp.py cle <port>')\n", (unsigned)kPortUdp);
  Serial.printf("essai de 24 h : %s\n", produitEssai() ? "arme" : "non");
  matterHotteStatut(Serial);
}

static void cmdHotte(char *args) {
  if (!*args) {
    afficherHotte();
    return;
  }
  char *reste = splitWord(args);
  if (!strcmp(args, "regle")) {
    imprimer(faireRegle(reste));
    return;
  }
  // Remet le mot detache : faireOrdre relit "ventilo v2".
  char ligne[48];
  snprintf(ligne, sizeof(ligne), "%s %s", args, reste);
  const Resultat r = faireOrdre(ligne, 0);
  if (!r.ok) Serial.println(r.msg);
  else Serial.println("hotte : ordre accepte, la sequence suit");
}

#if PILOTE_SIMULE
static void cmdSimu(char *args) {
  if (*args) {
    const Resultat r = faireSimu(args);
    if (r.msg) Serial.println(r.msg);
    if (!r.ok) return;
  }
  afficherSimu();
}
#endif

static void cmdMatter(char *args) {
  if (!*args) {
    matterHotteStatut(Serial);
    return;
  }
  if (!strcmp(args, "journal")) {
    matterHotteJournal(Serial);
    return;
  }
  imprimer(faireMatter(args));
}

static void cmdAlim(char *args) {
  if (!strcmp(args, "arret")) alimArreter();
  else if (!strcmp(args, "reprise") && !alimReprendre()) Serial.println("alim : reprise de l'ADC en echec");
  else if (*args && strcmp(args, "reprise")) Serial.println("usage : alim [arret | reprise]");
  alimStatut(Serial, produitSurveillance());
}

static void cmdThermique(char *args) {
  if (strcmp(args, "raz")) {
    Serial.println("usage : thermique raz   (a la pose : maximum depuis la pose remis a zero)");
    return;
  }
  produitSurveillance().maxPoseRaz();
  Serial.println("thermique : maximum depuis la pose remis a zero");
}

static void cmdEssai(char *args) { imprimer(faireEssai(args)); }

static void cmdRadio(char *args) { imprimer(faireRafale(args)); }

static void cmdJson(char *args) { jsonCommand(args, JsonCmd{false, 0, "", millis()}); }

static void cmdDecommission(char *) {
  Serial.println("decommission : cle H1 effacee, noeud retire de toutes ses fabriques, redemarrage");
  produitEcrireEnAttente();
  matterHotteDecommission();
}

static void cmdReboot(char *) {
  Serial.println("redemarrage (valeurs NVS en attente ecrites d'abord)");
  sRebootAt = millis() + 200;
}

static const Commande kCommandes[] = {
  {"help", cmdHelp, "liste des commandes"},
  {"info", cmdInfo, "version, MAC, pilote, cle, Matter"},
  {"hotte", cmdHotte, "hotte [ventilo off|v1|v2|v3|derniere | lampe on|off | regle <nom> <valeur>]"},
#if PILOTE_SIMULE
  {"simu", cmdSimu, "simu [reglage | appui | coupure | panne | apres_demarrage] : hotte simulee"},
#endif
  {"matter", cmdMatter, "matter [med 0|1 | tx 8..20 | maxint s | derniere 0|1 | journal]"},
  {"alim", cmdAlim, "alim [arret | reprise] : tensions et temperature de la puce"},
  {"thermique", cmdThermique, "thermique raz : maximum depuis la pose remis a zero"},
  {"essai", cmdEssai, "essai alim on|off : essai de 24 h (radio rafale permise a distance)"},
  {"radio", cmdRadio, "radio rafale <s> : rafale d'emission (essai de 24 h, session reseau)"},
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log|cle] : mode machine"},
  {"decommission", cmdDecommission, "remise a zero Matter et effacement de la cle H1 (USB)"},
  {"reboot", cmdReboot, "redemarrage (valeurs NVS en attente ecrites d'abord)"},
};

static void cmdHelp(char *) {
  Serial.println("=== Commandes ===");
  for (const Commande &c : kCommandes) Serial.printf("  %-12s %s\n", c.nom, c.aide);
}

// Decoupe le mot-cle, cherche dans kCommandes ; false : inconnue.
static bool executer(char *line) {
  while (*line == ' ') line++;
  size_t n = strlen(line);
  while (n && line[n - 1] == ' ') line[--n] = 0;
  if (!*line) return true;
  char *args = splitWord(line);
  for (const Commande &c : kCommandes) {
    if (!strcmp(line, c.nom)) {
      c.h(args);
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
//  Lignes de l'hote. runLine et cliPoll sont repris de src/cli.cpp de la sonde.
// ---------------------------------------------------------------------------

static bool firstWordIs(const char *s, const char *w) {
  const size_t n = strlen(w);
  return !strncmp(s, w, n) && (s[n] == ' ' || !s[n]);
}

static char *afterWord(char *s) {
  while (*s && *s != ' ') s++;
  while (*s == ' ') s++;
  return s;
}

// Commande a reponse seule (sans texte), avec id : rend vrai et remplit r.
static bool sansTexte(char *cmd, uint32_t id, jsonp::Reply *r) {
  Resultat res{};
  if (firstWordIs(cmd, "hotte") && *afterWord(cmd)) {
    char *args = afterWord(cmd);
    if (firstWordIs(args, "regle")) res = faireRegle(afterWord(args));
    else res = faireOrdre(args, id);
#if PILOTE_SIMULE
  } else if (firstWordIs(cmd, "simu") && *afterWord(cmd)) {
    res = faireSimu(afterWord(cmd));
#endif
  } else if (firstWordIs(cmd, "matter") && *afterWord(cmd) && !firstWordIs(afterWord(cmd), "journal")) {
    res = faireMatter(afterWord(cmd));
  } else if (firstWordIs(cmd, "essai")) {
    res = faireEssai(afterWord(cmd));
  } else if (firstWordIs(cmd, "radio")) {
    res = faireRafale(afterWord(cmd));
  } else if (firstWordIs(cmd, "reboot") && !*afterWord(cmd)) {
    sRebootAt = millis() + 500;  // la reponse part d'abord (datagramme compris)
    res = {true, "ok", "redemarrage dans 500 ms"};
  } else {
    return false;
  }
  r->ok = res.ok;
  r->code = res.code;
  r->msg = res.msg;
  // Ordre accepte : l'evenement sequence suivra, avec cet id.
  if (res.ok && !strcmp(res.code, "accepte")) r->suite = jsonp::Reply::SuiteSequence;
  return true;
}

// Une ligne de l'hote : prefixe id=<n> retire avant l'aiguillage, refus sans
// execution (trop longue, cadence, interdite a distance), puis :
//  - sans id : comme toujours (texte) ;
//  - avec id : famille json (reponse seule) ; commandes sans texte (reponse
//    ok, accepte, usage ou refuse) ; sinon commande a texte entre reponse
//    debut et reponse fin.
// A distance, jsonRemoteAdmit passe avant tout refus (id deja vu).
static void runLine(char *line, bool tooLong) {
  const uint32_t t0 = millis();
  const bool remote = jsonOrigin() != jsonp::kUsb;
  uint32_t id = 0;
  char *cmd = line;
  const bool hasId = jsonp::parseIdPrefix(line, &id, &cmd);
  while (*cmd == ' ') cmd++;
  size_t len = strlen(cmd);
  while (len && cmd[len - 1] == ' ') cmd[--len] = 0;
  if (remote && !hasId) {
    jsonCountRejected();  // a distance, toujours un id : sans lui, aucune reponse possible
    return;
  }
  if (!hasId && !*cmd && !tooLong) return;  // ligne vide (Ctrl-U compris) : rien
  char shown[jsonp::kCmdTextMax + 1];
  jsonp::copyCmd(shown, cmd);  // avant que l'aiguillage ne coupe la ligne en mots
  jsonp::maskCmd(shown);       // jamais l'alea de 'json cle nouvelle' dans la reponse
  const JsonCmd c{hasId, id, shown, t0};
  if (remote && jsonRemoteAdmit(id, shown)) return;
  if (tooLong) {
    jsonRefuse(c, "trop_long", "ligne de plus de 127 octets : rien n'est execute");
    return;
  }
  if ((hasId || jsonMachine() || remote) && !jsonCadenceOk(t0)) {
    jsonRefuse(c, "cadence", "plus de 20 lignes par seconde : rien n'est execute");
    return;
  }
  if (remote) {
    if (const char *why = jsonp::remoteRefusal(cmd, produitEssai())) {
      jsonRefuse(c, "interdite", why);
      jsonAfterCommand();
      return;
    }
  }
  if (!hasId) {
    if (!executer(cmd)) Serial.printf("commande inconnue : %s ('help')\n", cmd);
    jsonAfterCommand();
    return;
  }
  jsonp::Reply r;
  r.id = id;
  r.cmd = shown;
  if (!*cmd) {
    r.ok = false;
    r.code = "inconnue";
    r.msg = "commande vide";
    jsonReply(r);
    return;
  }
  if (firstWordIs(cmd, "json")) {
    jsonCommand(afterWord(cmd), c);
  } else if (sansTexte(cmd, id, &r)) {
    r.durMs = millis() - t0;
    jsonReply(r);
  } else {
    r.fin = false;
    r.code = "en_cours";
    jsonReply(r);  // la commande va ecrire son texte
    const bool known = executer(cmd);
    if (!known) Serial.printf("commande inconnue : %s ('help')\n", cmd);
    r.fin = true;
    r.ok = known;
    r.code = known ? "execute" : "inconnue";
    r.durMs = millis() - t0;
    jsonReplyEnd(r);
  }
  jsonAfterCommand();
}

void cliRunRemote(uint8_t origin, char *line, bool tooLong) {
  // Jamais l'USB par ce chemin (il echappe a la liste blanche).
  if (origin == jsonp::kUsb || origin >= jsonp::kOrigins) return;
  const uint8_t prev = jsonOrigin();
  jsonSetOrigin(origin);
  if (jsonOrigin() == origin) runLine(line, tooLong);
  jsonSetOrigin(prev);
}

static jsonp::LineAssembler sLine;  // 127 caracteres au plus, prefixe id= compris

void cliBegin() {
  Serial.println("Tape 'help' pour la liste des commandes.");
  Serial.print("> ");
}

void cliPoll() {
  if (sRebootAt && (int32_t)(millis() - sRebootAt) >= 0) {
    produitEcrireEnAttente();  // 4.4 : ce qui attend en NVS, d'abord
    ESP.restart();
  }
  while (Serial.available()) {
    const uint8_t c = (uint8_t)Serial.read();
    jsonNoteRx();
    const bool machine = jsonMachine();
    switch (sLine.feed(c, machine)) {
      case jsonp::LineAssembler::Ev::Echo:
        if (!machine) Serial.print((char)c);
        break;
      case jsonp::LineAssembler::Ev::Erase:
        if (!machine) Serial.print("\b \b");
        break;
      case jsonp::LineAssembler::Ev::Clear:
        if (!machine)
          for (uint8_t i = 0; i < sLine.cleared(); i++) Serial.print("\b \b");
        break;
      case jsonp::LineAssembler::Ev::Line: {
        if (!machine) Serial.println();
        const bool tooLong = sLine.tooLong();
        runLine(sLine.text(), tooLong);
        sLine.reset();
        if (!machine && !jsonMachine()) {
          Serial.flush();  // l'USB CDC du C6 perd des octets si on enchaine trop vite
          Serial.print("> ");
        }
        break;
      }
      case jsonp::LineAssembler::Ev::None: break;
    }
  }
}
