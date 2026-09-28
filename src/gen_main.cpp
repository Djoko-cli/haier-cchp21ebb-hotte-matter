// ===========================================================================
//  Generateur du banc de validation (docs/SPEC-RECONNAISSANCE.md section 10)
//
//  Second C6 (env generateur) : imite la ligne D, un bus de 5 V a drain
//  ouvert, avec les motifs a contenu connu de motifs.cpp. Sortie sur GPIO7
//  (kPinGenerateur) par le pilote RMT d'IDF en emission (driver/rmt_tx.h,
//  encodeur copie, 1 MHz). Montage : docs/WIRING.md section 10 (GPIO7 ->
//  4,7k -> base du NPN, 10k base-emetteur, collecteur sur la ligne, pull-up
//  vers le 5V). Logique inversee : GPIO haut = ligne basse, GPIO bas = ligne
//  relachee (haute).
//
//  Console USB (115200) :
//    motif <nom> [n] [pause_ms]   n trames (defaut 1000, rafale 1 ; 0 : sans
//                                 fin), pause apres chaque trame (defaut :
//                                 celle du motif, motif::pauseUs)
//    impulsions <bas_us> <periode_ms> [n]
//                                 impulsion basse de bas_us toutes les
//                                 periode_ms, n fois (defaut 0 : sans fin) ;
//                                 collision du critere 5 (docs/BANC.md 9)
//    stop                         arrete et relache la ligne (GPIO7 bas)
//    etat                         motif, trame, pause, niveau de la ligne
//    help                         commandes et motifs
//  A chaque trame emise : 'motif <nom> trame <index>' (index depuis 0) ;
//  a chaque impulsion : 'impulsion <index>'. Jamais d'attente : sans la place
//  de la ligne entiere dans le tampon d'emission de l'USB (console fermee), la
//  ligne est sautee et comptee (ligne 'fini'), car elle attendrait 2 s et
//  etirerait d'autant la pause.
//
//  Deroulement : prise du repos (un symbole bref au niveau du repos, qui y
//  laisse la ligne : eot_level), pause, trame 0, pause, trame 1, ... Chaque
//  trame est UNE transaction RMT (durees au tick pres). Pendant la pause, la
//  ligne reste au repos ; la tache loop la compte depuis la fin de la
//  transaction (rappel on_trans_done) et attend activement ses 2 dernieres
//  ms : la pause vaut la valeur demandee plus quelques dizaines de us. Apres
//  la derniere trame, la ligne reste au repos du motif ; 'stop' la relache.
//
//  Rafale : 100 000 segments de 100 us (10 s) en une transaction en boucle
//  materielle (gen::kRafaleBoucle symboles rejoues gen::kRafaleTours fois,
//  en un seul lot : aucune relance par l'interruption, donc aucun trou).
// ===========================================================================
#include <Arduino.h>
#include <driver/rmt_tx.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include <string.h>

#include "config.h"
#include "fw_version.h"
#include "gen_symboles.h"
#include "motifs.h"

namespace {

constexpr uint32_t kNDefaut = 1000;       // critere 1 du banc : 1 000 trames par motif
constexpr uint32_t kPauseMaxMs = 60000;
constexpr uint32_t kAttenteActiveUs = 2000;
constexpr size_t kLigneMax = 80;

rmt_channel_handle_t sCanal = nullptr;
rmt_encoder_handle_t sCopie = nullptr;
// Tampons lus par le pilote pendant l'emission : jamais modifies avant la fin.
rmt_symbol_word_t sSym[gen::kSymMax];
rmt_symbol_word_t sRafale[gen::kRafaleBoucle];
rmt_symbol_word_t sBref[1];

// Ecrits par le rappel (interruption), lus par la tache loop.
volatile bool sOccupe = false;   // une transaction est en cours
volatile uint32_t sFinUs = 0;    // fin de la derniere transaction (esp_timer, 32 bits bas)

enum class Etape : uint8_t { Arret, Emission, Pause, Fini };
struct Course {
  motif::Id id = motif::Id::Uart500;
  bool impulsions = false;  // commande 'impulsions' au lieu d'un motif
  uint32_t basUs = 0;       // impulsions : duree basse
  uint32_t n = 0;         // trames demandees (0 : sans fin)
  uint32_t pauseUs = 0;
  uint32_t index = 0;     // prochaine trame
  Etape etape = Etape::Arret;
  bool reposHaut = true;  // niveau de la ligne entre deux trames
  uint32_t sautees = 0;   // lignes de progression sautees (console non lue)
};
Course sC;
motif::Seg sSeg[motif::kSegMax];
gen::Sym sY[gen::kSymMax];
char sLigne[kLigneMax + 1];
size_t sLong = 0;

bool IRAM_ATTR finEmission(rmt_channel_handle_t, const rmt_tx_done_event_data_t *, void *) {
  sFinUs = (uint32_t)esp_timer_get_time();
  sOccupe = false;
  return false;
}

rmt_symbol_word_t mot(const gen::Sym &y) {
  rmt_symbol_word_t w;
  w.duration0 = y.d0;
  w.level0 = y.l0;
  w.duration1 = y.d1;
  w.level1 = y.l1;
  return w;
}

const char *texteRepos(bool haut) { return haut ? "haut (GPIO7 bas)" : "bas (GPIO7 haut)"; }

bool canalOuvrir() {
  rmt_tx_channel_config_t c = {};
  c.gpio_num = (gpio_num_t)kPinGenerateur;
  c.clk_src = RMT_CLK_SRC_DEFAULT;
  c.resolution_hz = 1000000;
  c.mem_block_symbols = gen::kMemSymboles;  // deux blocs : la boucle de la rafale et sa fin y tiennent
  c.trans_queue_depth = 2;   // une seule transaction en vol a la fois
  c.flags.init_level = 0;    // ligne relachee
  esp_err_t err = rmt_new_tx_channel(&c, &sCanal);
  if (err == ESP_OK) {
    rmt_tx_event_callbacks_t cb = {};
    cb.on_trans_done = finEmission;
    err = rmt_tx_register_event_callbacks(sCanal, &cb, nullptr);
  }
  if (err == ESP_OK) {
    rmt_copy_encoder_config_t e = {};
    err = rmt_new_copy_encoder(&e, &sCopie);
  }
  if (err == ESP_OK) err = rmt_enable(sCanal);
  if (err != ESP_OK) {
    Serial.printf("[rmt] %s : generateur inutilisable (GPIO7 reste bas)\n", esp_err_to_name(err));
    sCanal = nullptr;
    return false;
  }
  return true;
}

bool envoyer(const rmt_symbol_word_t *s, size_t n, bool ligneHauteApres, int boucles) {
  rmt_transmit_config_t t = {};
  t.loop_count = boucles;
  t.flags.eot_level = gen::niveauGpio(ligneHauteApres);
  sOccupe = true;
  const esp_err_t err = rmt_transmit(sCanal, sCopie, s, n * sizeof(rmt_symbol_word_t), &t);
  if (err != ESP_OK) {
    sOccupe = false;
    Serial.printf("[rmt] rmt_transmit : %s\n", esp_err_to_name(err));
    return false;
  }
  return true;
}

// Deux us au niveau voulu, qui y laissent la ligne (eot_level).
bool poserNiveau(bool ligneHaute) {
  const uint8_t g = gen::niveauGpio(ligneHaute);
  sBref[0] = mot(gen::Sym{1, g, 1, g});
  return envoyer(sBref, 1, ligneHaute, 0);
}

// Interrompt la transaction en cours (arret asynchrone du C6) ; la ligne reste au dernier niveau de repos.
void interrompre() {
  rmt_disable(sCanal);
  sOccupe = false;
  rmt_enable(sCanal);
}

void arreter() {
  interrompre();
  if (poserNiveau(true)) rmt_tx_wait_all_done(sCanal, 100);
  sC.etape = Etape::Arret;
}

void demarrer(motif::Id id, uint32_t n, uint32_t pauseUs) {
  if (sC.etape == Etape::Emission || sC.etape == Etape::Pause) interrompre();
  sC = Course{};
  sC.id = id;
  sC.n = n;
  sC.pauseUs = pauseUs;
  sC.reposHaut = motif::reposHaut(id);
  if (!poserNiveau(sC.reposHaut)) return;  // etape Arret
  sC.etape = Etape::Emission;              // prise du repos, puis pause, puis trame 0
  if (n) Serial.printf("motif %s : %lu trames", motif::nom(id), (unsigned long)n);
  else Serial.printf("motif %s : sans fin", motif::nom(id));
  Serial.printf(", pause %lu us, repos %s\n", (unsigned long)pauseUs, texteRepos(sC.reposHaut));
}

// 'impulsions' : pause comptee depuis la fin de chaque impulsion, soit une
// periode de bas_us + pause (plus quelques dizaines de us).
void demarrerImpulsions(uint32_t basUs, uint32_t periodeMs, uint32_t n) {
  if (sC.etape == Etape::Emission || sC.etape == Etape::Pause) interrompre();
  sC = Course{};
  sC.impulsions = true;
  sC.basUs = basUs;
  sC.n = n;
  sC.pauseUs = periodeMs * 1000 - basUs;
  sC.reposHaut = true;
  if (!poserNiveau(true)) return;  // etape Arret
  sC.etape = Etape::Emission;
  if (n) Serial.printf("impulsions : %lu", (unsigned long)n);
  else Serial.print("impulsions : sans fin");
  Serial.printf(", %lu us basses toutes les %lu ms, ligne haute entre deux\n", (unsigned long)basUs,
                (unsigned long)periodeMs);
}

// Ligne de progression (une par trame ou impulsion), entiere ou pas du tout,
// sans jamais attendre (regle de la sonde, spec 8.7) : Mac qui ne lit plus, le
// tampon d'emission de l'USB est plein et Serial.printf attendrait 2 s (HWCDC :
// 20 x 100 ms) avant de rendre la main ; la pause en serait allongee d'autant
// (banc 0 du 28/09 : une trame toutes les 2 s).
void progression() {
  char l[48];
  const int k = sC.impulsions
                    ? snprintf(l, sizeof(l), "impulsion %lu\n", (unsigned long)sC.index)
                    : snprintf(l, sizeof(l), "motif %s trame %lu\n", motif::nom(sC.id), (unsigned long)sC.index);
  if (k <= 0 || k >= (int)sizeof(l) || Serial.availableForWrite() < k) {
    sC.sautees++;
    return;
  }
  Serial.write((const uint8_t *)l, (size_t)k);
}

void avancer() {
  if (sC.etape == Etape::Emission && !sOccupe) sC.etape = Etape::Pause;
  if (sC.etape != Etape::Pause) return;
  const uint32_t ecoule = (uint32_t)esp_timer_get_time() - sFinUs;
  if (ecoule < sC.pauseUs) {
    const uint32_t reste = sC.pauseUs - ecoule;
    if (reste > kAttenteActiveUs) return;  // tour suivant de loop (tic de 1 ms)
    delayMicroseconds(reste);
  }
  if (sC.n && sC.index >= sC.n) {
    sC.etape = Etape::Fini;
    if (sC.impulsions) Serial.printf("impulsions finies : %lu, ligne haute", (unsigned long)sC.n);
    else
      Serial.printf("motif %s fini : %lu trames, ligne au repos %s", motif::nom(sC.id), (unsigned long)sC.n,
                    texteRepos(sC.reposHaut));
    if (sC.sautees) Serial.printf(" ; %lu ligne(s) de progression sautee(s), console non lue", (unsigned long)sC.sautees);
    Serial.println();
    return;
  }
  bool ok;
  if (sC.impulsions) {
    const motif::Seg s{false, sC.basUs};
    const size_t ny = gen::symboles(&s, 1, sY, gen::kSymMax);
    for (size_t i = 0; i < ny; i++) sSym[i] = mot(sY[i]);
    ok = ny && envoyer(sSym, ny, true, 0);
  } else if (sC.id == motif::Id::Rafale) {
    ok = envoyer(sRafale, gen::kRafaleBoucle, true, (int)gen::kRafaleTours);
  } else {
    const size_t ns = motif::trame(sC.id, sC.index, sSeg, motif::kSegMax);
    const size_t ny = gen::symboles(sSeg, ns, sY, gen::kSymMax);
    for (size_t i = 0; i < ny; i++) sSym[i] = mot(sY[i]);
    ok = ny && envoyer(sSym, ny, sC.reposHaut, 0);
  }
  if (!ok) {
    if (sC.impulsions) Serial.printf("impulsion %lu impossible, arret\n", (unsigned long)sC.index);
    else Serial.printf("motif %s : trame %lu impossible, arret\n", motif::nom(sC.id), (unsigned long)sC.index);
    arreter();
    return;
  }
  sC.etape = Etape::Emission;
  progression();
  sC.index++;
}

// Entier decimal strict : chiffres seulement, 4294967295 au plus.
bool lireU32(const char *s, uint32_t *v) {
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

void aide() {
  Serial.println("=== Generateur du banc ===");
  Serial.println("  motif <nom> [n] [pause_ms]  n trames (defaut 1000, rafale 1 ; 0 : sans fin), pause apres chaque trame");
  Serial.println("  impulsions <bas_us> <periode_ms> [n]  impulsion basse repetee (n : defaut 0, sans fin), critere 5");
  Serial.println("  stop                        arrete et relache la ligne (GPIO7 bas)");
  Serial.println("  etat                        motif, trame, pause, niveau de la ligne");
  Serial.println("motifs (repos, pause par defaut) :");
  for (uint8_t k = 0; k <= (uint8_t)motif::Id::Rafale; k++) {
    const motif::Id id = (motif::Id)k;
    Serial.printf("  %-12s repos %-4s %7lu us\n", motif::nom(id), motif::reposHaut(id) ? "haut" : "bas",
                  (unsigned long)motif::pauseUs(id));
  }
}

void etat() {
  if (!sCanal) {
    Serial.println("etat : RMT indisponible, GPIO7 bas");
    return;
  }
  switch (sC.etape) {
    case Etape::Arret: Serial.println("etat : arret, ligne relachee (GPIO7 bas)"); return;
    case Etape::Fini:
      if (sC.impulsions) Serial.printf("etat : fini, %lu impulsions, ligne haute\n", (unsigned long)sC.n);
      else
        Serial.printf("etat : fini, motif %s, %lu trames, ligne au repos %s\n", motif::nom(sC.id),
                      (unsigned long)sC.n, texteRepos(sC.reposHaut));
      return;
    case Etape::Emission:
    case Etape::Pause: break;
  }
  if (sC.impulsions) {
    Serial.printf("etat : impulsions de %lu us, %lu emises", (unsigned long)sC.basUs, (unsigned long)sC.index);
    if (sC.n) Serial.printf(" sur %lu", (unsigned long)sC.n);
    Serial.printf(", periode %lu us\n", (unsigned long)(sC.basUs + sC.pauseUs));
    return;
  }
  Serial.printf("etat : motif %s, %lu trames emises", motif::nom(sC.id), (unsigned long)sC.index);
  if (sC.n) Serial.printf(" sur %lu", (unsigned long)sC.n);
  Serial.printf(" (%s), pause %lu us, repos %s\n", sC.etape == Etape::Emission ? "emission" : "pause",
                (unsigned long)sC.pauseUs, texteRepos(sC.reposHaut));
}

void usageMotif() { Serial.println("usage : motif <nom> [n] [pause_ms 0..60000] ('help' : noms des motifs)"); }

void usageImpulsions() {
  Serial.println("usage : impulsions <bas_us 10..30000> <periode_ms 1..60000> [n] (1 ms de ligne haute au moins entre deux)");
}

void executer(char *ligne) {
  char *mots[5];
  int n = 0;
  char *reste = nullptr;
  for (char *m = strtok_r(ligne, " ", &reste); m; m = strtok_r(nullptr, " ", &reste)) {
    if (n == 5) {
      Serial.println("trop d'arguments");
      return;
    }
    mots[n++] = m;
  }
  if (!n) return;
  if (!strcmp(mots[0], "help")) {
    aide();
  } else if (!strcmp(mots[0], "etat")) {
    etat();
  } else if (!sCanal) {
    Serial.println("RMT indisponible : rien a faire (voir le message de demarrage)");
  } else if (!strcmp(mots[0], "stop")) {
    arreter();
    Serial.println("stop : ligne relachee (GPIO7 bas)");
  } else if (!strcmp(mots[0], "motif")) {
    motif::Id id;
    if (n < 2 || n > 4 || !motif::depuisNom(mots[1], &id)) return usageMotif();
    uint32_t nb = id == motif::Id::Rafale ? 1 : kNDefaut;
    uint32_t pauseUs = motif::pauseUs(id);
    uint32_t pauseMs = 0;
    if (n >= 3 && !lireU32(mots[2], &nb)) return usageMotif();
    if (n == 4) {
      if (!lireU32(mots[3], &pauseMs) || pauseMs > kPauseMaxMs) return usageMotif();
      pauseUs = pauseMs * 1000;
    }
    demarrer(id, nb, pauseUs);
  } else if (!strcmp(mots[0], "impulsions")) {
    uint32_t bas = 0, periode = 0, nb = 0;
    if (n < 3 || n > 4 || !lireU32(mots[1], &bas) || !lireU32(mots[2], &periode) ||
        (n == 4 && !lireU32(mots[3], &nb)) || !gen::impulsionValide(bas, periode))
      return usageImpulsions();
    demarrerImpulsions(bas, periode, nb);
  } else {
    Serial.printf("commande inconnue : %s ('help')\n", mots[0]);
  }
}

// Echo, effacement, une commande par ligne (CR ou LF ; les lignes vides sont ignorees).
void lireConsole() {
  while (Serial.available()) {
    const int c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (!sLong) continue;
      Serial.println();
      sLigne[sLong] = 0;
      sLong = 0;
      executer(sLigne);
      Serial.print("> ");
    } else if (c == 8 || c == 127) {
      if (sLong) {
        sLong--;
        Serial.print("\b \b");
      }
    } else if (c >= 32 && c < 127 && sLong < kLigneMax) {
      sLigne[sLong++] = (char)c;
      Serial.print((char)c);
    }
  }
}

}  // namespace

void setup() {
  // TOUJOURS la premiere instruction : GPIO7 bas, NPN bloque, ligne relachee.
  pinMode(kPinGenerateur, OUTPUT);
  digitalWrite(kPinGenerateur, LOW);
  Serial.begin(115200);
  Serial.printf("generateur %s (%s) : GPIO%u -> NPN -> ligne (GPIO haut = ligne basse)\n", FW_VERSION_FULL, FW_ENV,
                (unsigned)kPinGenerateur);
  gen::Sym y[gen::kRafaleBoucle];
  gen::rafale(y);
  for (size_t i = 0; i < gen::kRafaleBoucle; i++) sRafale[i] = mot(y[i]);
  canalOuvrir();
  aide();
  Serial.print("> ");
}

void loop() {
  lireConsole();
  if (sCanal) avancer();
  vTaskDelay(1);  // laisse tourner la tache IDLE
}
