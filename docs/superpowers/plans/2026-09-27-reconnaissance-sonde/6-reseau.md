# Plan de la sonde, partie 6 : reseau

> En-tête, contraintes globales, carte des fichiers et interfaces : [../2026-09-27-reconnaissance-sonde.md](../2026-09-27-reconnaissance-sonde.md). Les tâches s'exécutent dans l'ordre des numéros (17 à 21).

### Tâche 17 : Wi-Fi de la sonde (identifiants en NVS, mDNS, reconnexion), commande `wifi`, `info` enrichi

**Fichiers :**
- Créer : `src/net_wifi.h` (API du contrat, plus trois ajouts : `netWifiSsid`, `netWifiPertes`, `netWifiValides`)
- Créer : `src/net_wifi.cpp`
- Créer : `tools/tests/test_wifi.cpp`
- Modifier : `tools/tests/test_hote.sh` (deux lignes `test_wifi` avant la ligne `echo` finale)
- Modifier : `src/cli.cpp` (inclusion de `net_wifi.h`, ligne `wifi :` de `info`, `cmdWifi`, ligne `{"wifi", cmdWifi, "wifi <ssid> <mdp>"}` en fin de `kCommandes`)
- Modifier : `src/main.cpp` (inclusion, `netWifiBegin()` en fin de `setup()`, `netWifiPoll()` après `jsonPoll()`, commentaire d'en-tête)
- Tester : `sh tools/tests/test_hote.sh`, `~/.platformio/penv/bin/pio run -e sonde`

**Interfaces :**
- Consomme :
  - `src/config.h` (tâche 7) : `constexpr const char *kNvsEspace = "hotte";`, `constexpr const char *kNomMdns = "hotte-sonde";`.
  - `src/json_mode.h` (tâche 11) : `bool jsonLog(const char *src, const char *niv, const char *txt);` (`src` `reseau`, `niv` `notice` ; `false` : l'appelant écrit le texte sur l'USB).
  - `src/cli.cpp` (tâches 10 et 11) : `static char *splitWord(char *s)`, `static void cmdInfo(char *)`, `static const Commande kCommandes[]`.
  - `src/main.cpp` (tâches 10 et 11) : `setup()` finit par `cliBegin();`, `loop()` appelle `jsonPoll();` puis `vTaskDelay(1);`.
  - `tools/tests/verif.h` (tâche 8) : `VERIF`, `bilan`.
- Produit (`src/net_wifi.h`) :
  ```cpp
  void netWifiBegin();                              // NVS -> STA, WiFi.setSleep(false), mDNS kNomMdns
  void netWifiPoll();                               // reconnexion toutes les 10 s si perdu
  bool netWifiUp();
  int8_t netWifiRssi();                             // 0 si deconnecte
  void netWifiIp(char out[16]);                     // "0.0.0.0" si deconnecte
  bool netWifiSet(const char *ssid, const char *mdp);  // NVS puis reconnexion
  const char *netWifiSsid();                        // ajout : "" sans identifiants
  uint32_t netWifiPertes();                         // ajout : connexions perdues depuis le demarrage
  inline bool netWifiValides(const char *ssid, const char *mdp);  // ajout, pur (teste sur l'hote)
  ```
  Clés NVS (espace `hotte`) : `wifi_ssid` (chaîne, 32 caractères au plus), `wifi_mdp` (chaîne, absente pour un réseau ouvert).

**Décisions de cette tâche (à connaître pour les suivantes) :**
- **Sans identifiants, la radio reste éteinte** : `netWifiBegin()` ne démarre la station que si `wifi_ssid` existe. La pile lwIP ne tourne donc qu'avec le Wi-Fi (la tâche 19 n'ouvre son socket qu'après une première adresse).
- **Identifiants vérifiés avant la NVS** par `netWifiValides` (SSID de 1 à 32 caractères sans espace ; mot de passe vide, de 8 à 63 caractères, ou 64 hexa) : un mot de passe de 7 caractères écrit en NVS laisserait la sonde hors ligne à chaque démarrage, sans rien dire. `WiFi.persistent(false)` : les identifiants ne vivent que dans l'espace `hotte`.
- **Reconnexion** : le cœur Arduino relance seul après une perte ordinaire (`setAutoReconnect(true)`) ; `netWifiPoll()` relance aussi toutes les 10 s tant que la connexion manque (refus du mot de passe, point d'accès absent au démarrage, station pas démarrée), par `WiFi.disconnect(false, false, 0)` puis `WiFi.begin`.
- **Aucun rappel d'événement Wi-Fi** : `netWifiPoll()` relit `WiFi.status()` dans la tâche `loop` (spec §8.7 : un seul producteur de lignes). Les annonces (`[wifi] connecte a ...`, `[wifi] connexion a ... perdue ...`) partent par `jsonLog("reseau", "notice", ...)`, sinon en texte sur l'USB.
- **mDNS** : nom d'hôte seul (`hotte-sonde.local`, `MDNS.begin` après le démarrage de la station), pas de service DNS-SD ; `WiFi.setHostname(kNomMdns)` donne le même nom au DHCP.
- **Mot de passe** : la commande `wifi` est USB seulement (déjà refusée à distance par `jsonp::remoteRefusal`) ; Majid la tape lui-même (tâche 21), dans une console ouverte hors du dépôt (`log2file` écrirait l'écho dans un fichier du dépôt).

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_wifi.cpp` :

```cpp
// Tests hote de netWifiValides (src/net_wifi.h) : identifiants acceptes par
// la commande 'wifi <ssid> <mdp>' AVANT toute ecriture en NVS (un mot de passe
// de 7 caracteres ecrit en NVS laisserait la sonde hors ligne a chaque
// demarrage, sans rien dire).
// Lancer : sh tools/tests/test_hote.sh
#include <string>

#include "net_wifi.h"
#include "verif.h"

static void testSsid() {
  VERIF(netWifiValides("maison", "motdepasse"));
  VERIF(!netWifiValides("", "motdepasse"));                               // vide
  VERIF(netWifiValides(std::string(32, 's').c_str(), "motdepasse"));      // 32 : la limite du 802.11
  VERIF(!netWifiValides(std::string(33, 's').c_str(), "motdepasse"));
  VERIF(!netWifiValides("ma maison", "motdepasse"));                      // espace : 'wifi' coupe au premier
  VERIF(!netWifiValides("caf\xc3\xa9", "motdepasse"));                    // hors ASCII imprimable
  VERIF(!netWifiValides("tab\tx", "motdepasse"));
  VERIF(!netWifiValides(nullptr, "motdepasse"));
}

static void testMdp() {
  VERIF(netWifiValides("maison", ""));                                    // reseau ouvert
  VERIF(!netWifiValides("maison", "1234567"));                            // 7 : refuse par WPA
  VERIF(netWifiValides("maison", "12345678"));                            // 8
  VERIF(netWifiValides("maison", "phrase avec espaces"));                 // le reste de la ligne
  VERIF(netWifiValides("maison", std::string(63, 'p').c_str()));          // 63 : phrase WPA la plus longue
  VERIF(!netWifiValides("maison", std::string(64, 'p').c_str()));         // 64 non hexa
  VERIF(netWifiValides("maison", std::string(64, 'a').c_str()));          // 64 hexa : cle WPA brute
  VERIF(netWifiValides("maison", (std::string(32, '0') + std::string(32, 'F')).c_str()));
  VERIF(!netWifiValides("maison", std::string(65, 'a').c_str()));
  VERIF(!netWifiValides("maison", "mot\x01passe"));
  VERIF(!netWifiValides("maison", nullptr));
}

int main() {
  testSsid();
  testMdp();
  return bilan("test_wifi");
}
```

Puis ajouter ses deux lignes à `tools/tests/test_hote.sh` :

1. Dans `tools/tests/test_hote.sh`, remplacer :

```sh
"$OUT/test_generateur"
echo "tests hote : OK"
```

   par :

```sh
"$OUT/test_generateur"
$CXX tools/tests/test_wifi.cpp -o "$OUT/test_wifi"
"$OUT/test_wifi"
echo "tests hote : OK"
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`
Attendu : ÉCHEC avec « tools/tests/test_wifi.cpp:8:10: fatal error: 'net_wifi.h' file not found »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `src/net_wifi.h` :

```cpp
#pragma once
// ===========================================================================
//  Wi-Fi de la sonde (docs/SPEC-RECONNAISSANCE.md 8.5 et 8.7)
//
//  Station (STA) du coeur Arduino, identifiants en NVS (espace kNvsEspace,
//  cles wifi_ssid et wifi_mdp), jamais de sommeil du modem
//  (WiFi.setSleep(false) : consommation stable, une batterie USB ne se coupe
//  pas), nom mDNS kNomMdns (hotte-sonde.local), nouvel essai toutes les 10 s
//  tant que la connexion manque. Sans identifiants, la radio reste eteinte.
//
//  Tache loop seulement : l'etat de la connexion est relu par netWifiPoll()
//  (aucun rappel d'evenement ne produit de ligne, spec 8.7). Les annonces
//  (connexion, perte) partent en log 'reseau' en mode 'json log 1', sinon en
//  texte sur l'USB.
// ===========================================================================
#include <stdint.h>
#include <string.h>

void netWifiBegin();                              // NVS -> STA, WiFi.setSleep(false), mDNS kNomMdns
void netWifiPoll();                               // reconnexion toutes les 10 s si perdu
bool netWifiUp();
int8_t netWifiRssi();                             // 0 si deconnecte
void netWifiIp(char out[16]);                     // "0.0.0.0" si deconnecte
bool netWifiSet(const char *ssid, const char *mdp);  // NVS puis reconnexion
// Ajouts (info, reseau.ip) : SSID en vigueur ("" : aucun identifiant),
// connexions perdues depuis le demarrage.
const char *netWifiSsid();
uint32_t netWifiPertes();

// Identifiants acceptes par 'wifi <ssid> <mdp>', verifies avant toute
// ecriture en NVS : SSID de 1 a 32 caracteres ASCII imprimables sans espace
// (la commande coupe au premier) ; mot de passe vide (reseau ouvert), de 8 a
// 63 caracteres ASCII imprimables (phrase WPA, espaces compris), ou 64
// chiffres hexadecimaux (cle WPA brute). Pur : teste sur l'hote
// (tools/tests/test_wifi.cpp).
inline bool netWifiValides(const char *ssid, const char *mdp) {
  if (!ssid || !mdp) return false;
  const size_t ns = strlen(ssid), nm = strlen(mdp);
  if (ns < 1 || ns > 32 || (nm > 0 && nm < 8) || nm > 64) return false;
  for (size_t i = 0; i < ns; i++) {
    const unsigned char c = (unsigned char)ssid[i];
    if (c <= 0x20 || c > 0x7E) return false;
  }
  for (size_t i = 0; i < nm; i++) {
    const unsigned char c = (unsigned char)mdp[i];
    if (c < 0x20 || c > 0x7E) return false;
    const bool hexa = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (nm == 64 && !hexa) return false;
  }
  return true;
}
```

Créer `src/net_wifi.cpp` :

```cpp
// ===========================================================================
//  Wi-Fi de la sonde : station, identifiants en NVS, mDNS, reconnexion
//  (net_wifi.h). Tache loop seulement.
// ===========================================================================
#include "net_wifi.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

#include "config.h"
#include "json_mode.h"

static constexpr uint32_t kEssaiMs = 10000;  // nouvel essai tant que la connexion manque

static char sSsid[33] = "";
static char sMdp[65] = "";
static bool sRadio = false;  // station demarree (identifiants presents)
static bool sMdns = false;   // repondeur mDNS demarre
static bool sUp = false;     // connecte avec une adresse, au dernier netWifiPoll()
static uint32_t sEssaiAt = 0, sPertes = 0;

// Annonce : log 'reseau' en mode 'json log 1', sinon texte sur l'USB.
static void annonce(const char *txt) {
  if (!jsonLog("reseau", "notice", txt)) Serial.println(txt);
}

static void lireIdentifiants() {
  sSsid[0] = sMdp[0] = 0;
  Preferences p;
  // Ouverture en ecriture meme pour lire (reglages.cpp) ; isKey() d'abord :
  // interroger une cle absente logue une erreur NVS.
  if (!p.begin(kNvsEspace, false)) return;
  if (p.isKey("wifi_ssid")) p.getString("wifi_ssid", sSsid, sizeof(sSsid));
  if (p.isKey("wifi_mdp")) p.getString("wifi_mdp", sMdp, sizeof(sMdp));
  p.end();
  if (!netWifiValides(sSsid, sMdp)) sSsid[0] = sMdp[0] = 0;  // NVS abimee : comme sans identifiants
}

// Demarre la station (la premiere fois) et lance une connexion avec les
// identifiants en vigueur. Non bloquant, sauf si la station est connectee
// (WiFi.begin la deconnecte d'abord, 1 s au plus : commande 'wifi' seulement).
static void connecter() {
  sEssaiAt = millis();
  if (!sRadio) {
    WiFi.persistent(false);       // identifiants dans l'espace 'hotte' seulement
    WiFi.setHostname(kNomMdns);   // nom DHCP, avant le demarrage de la station
    WiFi.setSleep(false);         // spec 8.7 : applique au demarrage de la station
    WiFi.setAutoReconnect(true);  // le coeur relance seul apres une perte ordinaire
    if (!WiFi.mode(WIFI_STA)) return;
    sRadio = true;
  } else {
    WiFi.disconnect(false, false, 0);  // arrete un essai en cours (sinon la configuration est refusee)
  }
  if (!sMdns) sMdns = MDNS.begin(kNomMdns);  // apres le demarrage de la pile reseau
  WiFi.begin(sSsid, sMdp);
}

void netWifiBegin() {
  lireIdentifiants();
  if (sSsid[0]) connecter();
}

void netWifiPoll() {
  if (!sSsid[0]) return;  // aucun identifiant : radio eteinte
  const uint32_t now = millis();
  const bool up = netWifiUp();
  if (up != sUp) {
    sUp = up;
    char t[120];
    if (up) {
      char ip[16];
      netWifiIp(ip);
      snprintf(t, sizeof(t), "[wifi] connecte a %s : IP %s, RSSI %d dBm, %s.local", sSsid, ip, (int)WiFi.RSSI(),
               kNomMdns);
    } else {
      sPertes++;
      snprintf(t, sizeof(t), "[wifi] connexion a %s perdue : nouvel essai toutes les %lu s", sSsid,
               (unsigned long)(kEssaiMs / 1000));
    }
    annonce(t);
    sEssaiAt = now;
  }
  // Le coeur relance seul apres une perte ordinaire ; pas apres un refus
  // (mot de passe, point d'acces absent au demarrage...), ni si la station n'a
  // pas demarre : on relance ici.
  if (!up && now - sEssaiAt >= kEssaiMs) connecter();
}

bool netWifiUp() { return sRadio && WiFi.status() == WL_CONNECTED; }

int8_t netWifiRssi() { return netWifiUp() ? WiFi.RSSI() : 0; }

void netWifiIp(char out[16]) {
  if (!netWifiUp()) {
    strcpy(out, "0.0.0.0");
    return;
  }
  const IPAddress a = WiFi.localIP();
  snprintf(out, 16, "%u.%u.%u.%u", (unsigned)a[0], (unsigned)a[1], (unsigned)a[2], (unsigned)a[3]);
}

bool netWifiSet(const char *ssid, const char *mdp) {
  if (!netWifiValides(ssid, mdp)) return false;
  Preferences p;
  bool ok = p.begin(kNvsEspace, false);
  ok = ok && p.putString("wifi_ssid", ssid) == strlen(ssid);
  // Reseau ouvert : la cle disparait (un ancien mot de passe serait relu au demarrage).
  if (*mdp) ok = ok && p.putString("wifi_mdp", mdp) == strlen(mdp);
  else if (ok && p.isKey("wifi_mdp")) ok = p.remove("wifi_mdp");
  p.end();
  if (!ok) return false;
  strcpy(sSsid, ssid);
  strcpy(sMdp, mdp);
  connecter();
  sUp = false;  // changement voulu : pas une perte
  return true;
}

const char *netWifiSsid() { return sSsid; }

uint32_t netWifiPertes() { return sPertes; }
```

Brancher la commande `wifi` et `info` dans `src/cli.cpp` (quatre remplacements, dans cet ordre) :

1. Dans `src/cli.cpp`, remplacer :

```cpp
#include "json_out.h"
#include "sonde.h"
```

   par :

```cpp
#include "json_out.h"
#include "net_wifi.h"
#include "sonde.h"
```

2. Dans `src/cli.cpp`, remplacer :

```cpp
  Serial.printf("ecoute GPIO%u, injection GPIO%u (tenue basse)\n", (unsigned)kPinEcoute, (unsigned)kPinInjection);
  afficherReglages(sondeReglages());
}
```

   par :

```cpp
  Serial.printf("ecoute GPIO%u, injection GPIO%u (tenue basse)\n", (unsigned)kPinEcoute, (unsigned)kPinInjection);
  afficherReglages(sondeReglages());
  if (!*netWifiSsid()) {
    Serial.println("wifi : non configure ('wifi <ssid> <mdp>')");
  } else {
    char ip[16];
    netWifiIp(ip);
    Serial.printf("wifi : %s, %s, IP %s, RSSI %d dBm, mDNS %s.local, %lu perte(s) depuis le demarrage\n",
                  netWifiSsid(), netWifiUp() ? "connecte" : "deconnecte (nouvel essai toutes les 10 s)", ip,
                  (int)netWifiRssi(), kNomMdns, (unsigned long)netWifiPertes());
  }
}
```

3. Dans `src/cli.cpp`, remplacer :

```cpp
static void cmdJson(char *args) { jsonCommand(args, JsonCmd{false, 0, "", millis()}); }
```

   par :

```cpp
static void cmdJson(char *args) { jsonCommand(args, JsonCmd{false, 0, "", millis()}); }

// wifi <ssid> <mdp> : le mot de passe est le reste de la ligne (vide : reseau
// ouvert). Verifie avant d'ecrire en NVS ; USB seulement (liste blanche).
static void cmdWifi(char *args) {
  char *mdp = splitWord(args);
  if (!*args) {
    Serial.println("usage : wifi <ssid> <mdp>   (mdp absent : reseau ouvert)");
    return;
  }
  if (!netWifiValides(args, mdp)) {
    Serial.println("wifi : refuse (ssid de 1 a 32 caracteres sans espace ; mdp vide, de 8 a 63 caracteres, ou 64 hexa)");
    return;
  }
  if (!netWifiSet(args, mdp)) {
    Serial.println("wifi : echec de l'ecriture en NVS, rien ne change");
    return;
  }
  Serial.printf("wifi : identifiants enregistres, connexion a %s ('info' pour suivre)\n", args);
}
```

4. Dans `src/cli.cpp`, remplacer :

```cpp
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log] : mode machine"},
};
```

   par :

```cpp
  {"json", cmdJson, "json [1 [bail s]|0|etat|hello|ping|periode|compteurs|reseau|trames|log] : mode machine"},
  {"wifi", cmdWifi, "wifi <ssid> <mdp>"},
};
```

Brancher le Wi-Fi dans `src/main.cpp` (quatre remplacements) :

1. Dans `src/main.cpp`, remplacer :

```cpp
#include "json_out.h"
#include "reglages.h"
```

   par :

```cpp
#include "json_out.h"
#include "net_wifi.h"
#include "reglages.h"
```

2. Dans `src/main.cpp`, remplacer :

```cpp
  cliBegin();
}
```

   par :

```cpp
  cliBegin();
  netWifiBegin();  // identifiants en NVS : station, mDNS ; sans eux, radio eteinte
}
```

3. Dans `src/main.cpp`, remplacer :

```cpp
  jsonPoll();
  vTaskDelay(1);  // laisse tourner la tache IDLE
```

   par :

```cpp
  jsonPoll();
  netWifiPoll();
  vTaskDelay(1);  // laisse tourner la tache IDLE
```

4. Dans `src/main.cpp`, remplacer :

```cpp
//  setup() : GPIO7 bas en premier, port USB, identifiant de demarrage,
//  reglages NVS, interruption des fronts, capture RMT, console. loop() :
//  console, capture (parties -> lignes trame du mode machine, ou texte),
//  mode machine (bail, lignes periodiques), puis la tache IDLE.
```

   par :

```cpp
//  setup() : GPIO7 bas en premier, port USB, identifiant de demarrage,
//  reglages NVS, interruption des fronts, capture RMT, console, Wi-Fi.
//  loop() : console, capture (parties -> lignes trame du mode machine, ou
//  texte), mode machine (bail, lignes periodiques), Wi-Fi, puis la tache IDLE.
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh`
Attendu (fin de la sortie) :

```text
test_generateur : 20 verifications, 0 echecs
test_wifi : 19 verifications, 0 echecs
tests hote : OK
```

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|RAM:|Flash:|SUCCESS|FAILED"`
Attendu (aucun `warning`, aucune `error` ; la bibliothèque WiFi fait passer la flash de 24 à 80 %) :

```text
RAM:   [==        ]  19.0% (used 62140 bytes from 327680 bytes)
Flash: [========  ]  80.3% (used 10525xx bytes from 1310720 bytes)
========================= [SUCCESS] Took ... seconds =========================
sonde          SUCCESS   ...
```

- [ ] **Étape 5 : Commit**

```bash
git add src/net_wifi.h src/net_wifi.cpp src/cli.cpp src/main.cpp tools/tests/test_wifi.cpp tools/tests/test_hote.sh
git commit -m "Ajouter le Wi-Fi de la sonde et la commande wifi" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 18 : Enveloppe H1 copiée de la ScreenBar

**Fichiers :**
- Créer : `src/h1_proto.h`, `src/h1_proto.cpp`, `src/h1_crypto.cpp` (copies telles quelles de `benq@c58a506`, ligne de provenance en tête)
- Créer : `tools/tests/test_h1.cpp` (copie de `benq@c58a506:tools/host_tests/test_h1.cpp`, seule la ligne « Lancer : » change)
- Modifier : `tools/tests/test_hote.sh` (trois lignes : commentaire, compilation, exécution de `test_h1`)
- Tester : `sh tools/tests/test_hote.sh`, `~/.platformio/penv/bin/pio run -e sonde`

**Interfaces :**
- Consomme : `tools/tests/test_hote.sh` (tâche 7, étendu en 17 : sa dernière ligne de test est `"$OUT/test_wifi"`) ; la ScreenBar au commit `c58a506` (lecture seule).
- Produit (`src/h1_proto.h`, namespace `h1`, inchangé) : `kKeyLen` (32), `kKidHex` (8), `kHeaderMax` (56), `kDefiLen` (82), `kSlots` (2), `struct Part {const void *p; size_t n;}`, `bool hmacSha256(const uint8_t *key, size_t keyLen, const Part *parts, size_t nParts, uint8_t out[32])`, `bool keyId(const uint8_t psk[kKeyLen], char kid[kKidHex + 1])`, `void wipe(void *p, size_t n)`, `void toHex(const uint8_t *p, size_t n, char *out)`, `enum class Kind`, `struct Parsed`, `Parsed parse(const uint8_t *d, size_t n)`, `struct Peer {uint8_t ip[16]; uint16_t port; uint8_t local[16];}`, `struct Session`, `enum class Verdict`, `class Table` (`setKey`, `hasKey`, `kid`, `onSalut`, `onData`, `end`, `resume`, `seal`, `expire`, `clear`, `slot`, `provisional`, `established`). `src/h1_crypto.cpp` fournit `hmacSha256` et `sha256` par mbedTLS (SHA matériel du C6) ; `tools/tests/test_h1.cpp` les fournit par CommonCrypto.

**Décisions :**
- Copies **telles quelles**, sans rien adapter (les commentaires parlent encore de Thread et de `halo1/cle` : ils décrivent la ScreenBar, la spec §8.6 le veut ainsi ; le sous-projet 3 remettra ces fichiers en commun). Seul `test_h1.cpp` change sa ligne « Lancer : ».
- CommonCrypto est dans la `libSystem` de macOS : la ligne de compilation de `test_h1` n'a rien à lier de plus, comme dans `benq/tools/test_halo1.sh`. La spec §8.6 prévoyait un `tools/tests/test_h1.sh` à part : le contrat range tous les tests C++ dans `test_hote.sh`.

- [ ] **Étape 1 : Écrire le test qui échoue**

Copier le test de la ScreenBar, avec sa provenance et la commande de lancement de ce dépôt :

```bash
{ echo '// Copie de benq-screenbar-halo-matter@c58a506 : tools/host_tests/test_h1.cpp (adapte : commande de lancement)'; git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:tools/host_tests/test_h1.cpp; } > tools/tests/test_h1.cpp
sed -i '' 's|^// Lancer : sh tools/test_halo1.sh$|// Lancer : sh tools/tests/test_hote.sh|' tools/tests/test_h1.cpp
```

Puis ajouter ses lignes à `tools/tests/test_hote.sh` :

1. Dans `tools/tests/test_hote.sh`, remplacer :

```sh
"$OUT/test_wifi"
echo "tests hote : OK"
```

   par :

```sh
"$OUT/test_wifi"
# Enveloppe H1 : crypto de CommonCrypto (macOS, dans libSystem : rien a lier) ; mbedTLS sur la carte.
$CXX src/h1_proto.cpp tools/tests/test_h1.cpp -o "$OUT/test_h1"
"$OUT/test_h1"
echo "tests hote : OK"
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `sh tools/tests/test_hote.sh`
Attendu : ÉCHEC avec « clang++: error: no such file or directory: 'src/h1_proto.cpp' »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Copier les trois sources, telles quelles, avec leur ligne de provenance :

```bash
for f in src/h1_proto.h src/h1_proto.cpp src/h1_crypto.cpp; do { echo "// Copie de benq-screenbar-halo-matter@c58a506 : $f"; git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:$f; } > $f; done
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `sh tools/tests/test_hote.sh`
Attendu (fin de la sortie) :

```text
test_wifi : 19 verifications, 0 echecs
test_h1 : 121 verification(s), 0 echec(s)
tests hote : OK
```

Lancer (copies conformes) : `for f in src/h1_proto.h src/h1_proto.cpp src/h1_crypto.cpp; do git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:$f > "${TMPDIR:-/tmp}/ref_h1"; tail -n +2 $f | cmp -s - "${TMPDIR:-/tmp}/ref_h1" && echo "$f : identique a c58a506"; done`
Attendu : les trois lignes `src/h1_proto.h : identique a c58a506`, `src/h1_proto.cpp : identique a c58a506`, `src/h1_crypto.cpp : identique a c58a506`.

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|RAM:|Flash:|SUCCESS|FAILED"`
Attendu : `SUCCESS`, aucun `warning` ; RAM et flash inchangées (19,0 %, 80,3 % : rien n'appelle encore H1).

- [ ] **Étape 5 : Commit**

```bash
git add src/h1_proto.h src/h1_proto.cpp src/h1_crypto.cpp tools/tests/test_h1.cpp tools/tests/test_hote.sh
git commit -m "Copier l'enveloppe H1 de la ScreenBar" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 19 : Transport UDP sur le Wi-Fi (sockets lwIP, sessions H1, clé), `json_mode` multi-origines, profil réseau du protocole

**Fichiers :**
- Créer : `src/net_udp_wifi.h`, `src/net_udp_wifi.cpp` (réécrits d'après `benq@c58a506:src/net_udp.{h,cpp}` : contenu complet ci-dessous)
- Modifier : `src/json_mode.h` (en-tête ; `jsonRemoteReset`, `jsonNoteRemoteRx`, `jsonRemoteAdmit`)
- Modifier : `src/json_mode.cpp` (25 remplacements : émission réseau, cache des réponses, `eventRoom`, `caps`, bloc `reseau.ip`, sessions réseau, `json cle`, `json 0` qui libère la place)
- Modifier : `src/cli.h` (fichier entier : `cliRunRemote`)
- Modifier : `src/cli.cpp` (ligne de provenance, inclusion, ligne `udp :` de `info`, `jsonRemoteAdmit` dans `runLine`, `cliRunRemote`)
- Modifier : `src/main.cpp` (inclusion, `netUdpBegin()`, `netUdpPoll()`, commentaire d'en-tête)
- Modifier : `tools/json_check.py` (schéma `("reseau", "ip")`, cohérence `wifi`)
- Modifier : `tools/tests/test_json_check.py` (message valide `reseau/ip`, `test_reseau_ip`)
- Modifier : `docs/PROTOCOLE-JSON.md` (dix remplacements, §4.6 `reseau`, nouvelle section 9 « Transport réseau »)
- Tester : `python3 -m unittest discover -s tools/tests -p test_json_check.py -v`, `sh tools/tests/test_hote.sh`, `python3 -m unittest discover -s tools/tests`, `~/.platformio/penv/bin/pio run -e sonde`, `~/.platformio/penv/bin/pio run -e generateur`

Les originaux se lisent par `git -C /Users/Majid/Documents/Dev/esp32/benq show c58a506:src/net_udp.cpp` (et `src/net_udp.h`, `src/json_mode.cpp` : `jsonRemoteReset`, `jsonRemoteAdmit`, `keyCommand`, `eventRoom`, `cacheReply` ; `src/cli.cpp` : `cliRunRemote`, et l'appel à `jsonRemoteAdmit` dans `runLine`).

**Interfaces :**
- Consomme :
  - `src/net_wifi.h` (tâche 17) : `bool netWifiUp();`, `int8_t netWifiRssi();`, `void netWifiIp(char out[16]);`, `uint32_t netWifiPertes();`.
  - `src/h1_proto.h` (tâche 18) : `h1::Table`, `h1::parse`, `h1::Peer`, `h1::Parsed`, `h1::Verdict`, `h1::Kind`, `h1::hmacSha256`, `h1::keyId`, `h1::wipe`, `h1::toHex`, `h1::kKeyLen`, `h1::kKidHex`, `h1::kHeaderMax`, `h1::kDefiLen`, `h1::kSlots`.
  - `src/json_out.h` (tâche 8) : `Writer` (`u32`, `i32`, `str`, `null`, `boolean`, `obj`, `arr`, `end`), `Reply` (`key`, `hasKid`, `kid`), `ReplyCache` (`clear`, `put`, `find`), `Item::NetIp`, `Queue` (`clear`, `has`), `RateCap`, `Cadence`, `copyCmd`, `remoteRefusal` (liste blanche hotte : déjà complète), `kOrigins` (3), `kUsb` (0), `kLineMax`, `kCmdMax`, `kCmdTextMax`.
  - `src/json_mode.cpp` (tâche 11) : points d'accroche `room()`, `emit()`, `pushNet()`, `produce()` (`default:`), `helloIdentity()` (`caps`), branche `cle` de `jsonCommand`, `lose(Sink &, uint32_t)`, `Sink` (`trameNum`, `trameSautee`, `trameCap`, `logCap`, `cadence`, `replies`), `kLogCap`, `kTramesParSeconde`.
  - `src/cli.cpp` (tâche 11) : `static void runLine(char *line, bool tooLong)` (la ligne `const JsonCmd c{hasId, id, shown, t0};`).
  - `src/config.h` : `kNvsEspace`, `kNomMdns`, `kPortUdp` (5480).
- Produit :
  - `src/net_udp_wifi.h` : l'API du contrat, à la lettre : `void netUdpBegin(); void netUdpPoll(); bool netUdpSend(uint8_t slot, const uint8_t *json, size_t len); uint8_t netUdpFreeSlots(); void netUdpEnd(uint8_t slot); void netUdpResume(uint8_t slot); void netUdpJson(jsonp::Writer &w, uint32_t now); enum class NetKeyResult : uint8_t { Ok, Crypto, Nvs, Load }; NetKeyResult netUdpKeyNew(const uint8_t appRandom[32], char keyHex[65], char kid[9]); bool netUdpKeyErase(); bool netUdpKid(char kid[9]);`.
  - `src/json_mode.h` : `void jsonRemoteReset(uint8_t origin); void jsonNoteRemoteRx(uint8_t origin); bool jsonRemoteAdmit(uint32_t id, const char *shown);` (le reste du contrat était déjà là, sauf `jsonInjection` : tâche 23).
  - `src/cli.h` : `void cliRunRemote(uint8_t origin, char *line, bool tooLong = false);` (écart au contrat : troisième paramètre, voir les décisions).
  - `tools/json_check.py` : `SCHEMAS[("reseau", "ip")]`.
  - Pour la tâche 23 : les commandes `injection on|off` et `injecte ...` passent déjà la liste blanche ; à distance, elles arrivent dans `runLine` avec `remote` vrai et un `id` (elles doivent répondre sans texte, comme `capture` et `seuils`) ; un événement `injection` vers une session réseau passe par `send(o)` comme les autres.

**Décisions de cette tâche (à connaître pour les suivantes) :**
- **Socket** : `lwip_socket(AF_INET, SOCK_DGRAM)` lié à `INADDR_ANY:5480`, non bloquant. Il s'ouvre dès qu'une clé existe **et** que le Wi-Fi a eu une adresse (sans station, la pile lwIP ne tourne pas : un `socket()` avant elle planterait), puis reste ouvert tant que la clé existe ; il se ferme à `json cle efface`. Pas de drapeau `SONDE_UDP` (la spec §8.6 en parlait) : le transport est toujours dans l'env `sonde`, et l'env `generateur` ne compile pas ces fichiers.
- **Réception dans la tâche `loop`** (pas de file FreeRTOS comme sur Thread) : `lwip_recvfrom(..., MSG_DONTWAIT)`, 4 datagrammes par tour au plus ; lwIP en garde 6 en attente (`CONFIG_LWIP_UDP_RECVMBOX_SIZE`). Tampon de 257 octets : un datagramme de plus de 256 octets se reconnaît et se jette (`udp.rx_perdus`).
- **`h1::Peer`** : `ip` = IPv4 mappée `::ffff:a.b.c.d`, `port` ; `local` reste nul (une seule interface, lwIP choisit la source). La file d'émission garde l'IPv4 sur 4 octets.
- **Émission** : file de 32 datagrammes de 1 078 octets au plus (~35 Ko de RAM statique : la RAM passe de 19 à 30 %), 4 par tour, 50 000 octets/s, crédit de 8 192, 4 s au plus en file. `sendto` refusé avec `ENOMEM`/`EAGAIN` : nouvel essai au tour suivant ; autre refus : perdu. Wi-Fi coupé : la file attend (4 s au plus).
- **`eventRoom`** (ScreenBar) : une ligne `trame` ou `log` vers une session réseau ne part que s'il reste deux datagrammes libres ; sinon perdue (trou de `n`, `lignes_perdues`). Sur l'USB, rien ne change.
- **Bloc `reseau.ip`** : le schéma de la ScreenBar entier (`frais_ms` = 0, `srp` = null, `adresses` avec l'IPv4 de type `autre`, `udp` complet dont `tampons_libres` et `tampons_min` null), plus `mdns.nom` et `wifi` (`connecte`, `rssi_dbm`, `ip`, `pertes`). Il part aussi sur l'USB (`json 1`, `json etat`, toutes les `reseau_ms`) : l'instantané de `json 1` gagne une ligne, et une session USB reçoit désormais un `reseau` toutes les 5 s.
- **`cliRunRemote(origin, line, tooLong = false)`** : le contrat n'a que deux paramètres, mais la ScreenBar en a trois, et `net_udp_wifi.cpp` doit dire qu'une charge dépassait 127 octets (refus `trop_long` avec son `id`). Le troisième a une valeur par défaut : un appel à deux paramètres compile.
- **`json cle ...`** : `keyCommand` de la ScreenBar, clé en NVS `hotte/cle` ; sans `id`, `json cle nouvelle` renvoie vers `tools/hotte_udp.py cle <port>` (tâche 20).
- **`caps`** : `["sonde","injection","trames","log","udp","cle","mdns"]`. `reseau_build` reste `aucun` (pas de réseau Matter).

- [ ] **Étape 1 : Écrire le test qui échoue**

Dans `tools/tests/test_json_check.py`, un message `reseau/ip` valide rejoint `VALIDES` (le test `test_messages_valides` exige un message par schéma), puis un test de ses bornes :

1. Dans `tools/tests/test_json_check.py`, remplacer :

```python
    ("log", None): msg("log", src="capture", niv="notice", txt="[capture] echec du RMT"),
}
```

   par :

```python
    ("log", None): msg("log", src="capture", niv="notice", txt="[capture] echec du RMT"),
    ("reseau", "ip"): msg(
        "reseau", "ip", frais_ms=0, srp=None, adresses=[{"adr": "192.168.1.42", "type": "autre", "pref": True}],
        udp={"port": 5480, "ouvert": True, "empreinte": "630DCD29", "sessions": 1, "provisoire": False, "rx": 12,
             "rejets": 0, "rx_perdus": 0, "defis": 1, "tx": 230, "tx_perdus": 0, "tx_erreurs": 0,
             "tampons_libres": None, "tampons_min": None},
        mdns={"nom": "hotte-sonde.local"}, wifi={"connecte": True, "rssi_dbm": -58, "ip": "192.168.1.42", "pertes": 0}),
}
```

2. Dans `tools/tests/test_json_check.py`, remplacer :

```python
    def test_log(self):
```

   par :

```python
    def test_reseau_ip(self):
        cle = ("reseau", "ip")
        ip = VALIDES[cle]
        # Wi-Fi coupe : ni RSSI, ni IP, aucune adresse ; sans cle : socket ferme, empreinte null.
        coupe = dict(ip["wifi"], connecte=False, rssi_dbm=None, ip=None)
        sans_cle = dict(ip["udp"], ouvert=False, empreinte=None, sessions=0)
        self.assertEqual(self.modifie(cle, adresses=[], wifi=coupe, udp=sans_cle), ([], []))
        self.assertTrue(self.modifie(cle, wifi=dict(ip["wifi"], connecte=False))[0])     # deconnecte avec un RSSI
        self.assertTrue(self.modifie(cle, wifi=dict(coupe, connecte=True))[0])          # connecte sans IP
        self.assertTrue(self.modifie(cle, wifi=dict(ip["wifi"], rssi_dbm=5))[0])        # RSSI positif
        self.assertTrue(self.modifie(cle, wifi=dict(ip["wifi"], ip="192.168.1"))[0])    # IPv4 incomplete
        self.assertTrue(self.modifie(cle, udp=dict(ip["udp"], sessions=3))[0])          # 2 sessions H1 au plus
        self.assertTrue(self.modifie(cle, udp=dict(ip["udp"], empreinte="630dcd29"))[0])
        self.assertTrue(self.modifie(cle, sans_mdns=None)[0])
        self.assertTrue(self.modifie(cle, srp={"nom": 5})[0])
        ident = ("hello", "identite")
        caps = ["sonde", "injection", "trames", "log", "udp", "cle", "mdns"]
        self.assertEqual(self.modifie(ident, caps=caps), ([], []))

    def test_log(self):
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_json_check.py -v`
Attendu : ÉCHEC (`FAILED (failures=2)`) avec « AssertionError: Items in the first set but not the second: ('reseau', 'ip') » et « ["reseau : bloc 'ip' inconnu (attendu : )"] »

- [ ] **Étape 3 : Écrire l'implémentation minimale (vérificateur)**

1. Dans `tools/json_check.py`, remplacer :

```python
    ("log", None): Obj(
        {
            "src": Enum("sonde", "capture", "injection", "reseau"),
            "niv": Enum("notice", "trace"),
            "txt": Str(191),
            "sautes": Opt(U32),
        }
    ),
}
```

   par :

```python
    ("log", None): Obj(
        {
            "src": Enum("sonde", "capture", "injection", "reseau"),
            "niv": Enum("notice", "trace"),
            "txt": Str(191),
            "sautes": Opt(U32),
        }
    ),
    # Transport UDP sur le Wi-Fi : schema du bloc ip de la ScreenBar (srp toujours
    # null, tampons OpenThread null), plus mdns et wifi.
    ("reseau", "ip"): Obj(
        {
            "frais_ms": Null(U32),
            "srp": Null(Obj({"nom": Null(Str(63))})),
            "adresses": Arr(Obj({"adr": Str(45), "type": Enum("omr", "ml_eid", "autre"), "pref": BOOL}), 4),
            "udp": Obj(
                {
                    "port": Int(1, 65535),
                    "ouvert": BOOL,
                    "empreinte": Null(Str(8, r"^[0-9A-F]{8}$")),
                    "sessions": Int(0, 2),
                    "provisoire": BOOL,
                    "rx": U32,
                    "rejets": U32,
                    "rx_perdus": U32,
                    "defis": U32,
                    "tx": U32,
                    "tx_perdus": U32,
                    "tx_erreurs": U32,
                    "tampons_libres": Null(U32),
                    "tampons_min": Null(U32),
                }
            ),
            "mdns": Obj({"nom": Str(63)}),
            "wifi": Obj(
                {
                    "connecte": BOOL,
                    "rssi_dbm": Null(Int(-128, 0)),
                    "ip": Null(Str(15, r"^[0-9]{1,3}(\.[0-9]{1,3}){3}$")),
                    "pertes": U32,
                }
            ),
        }
    ),
}
```

2. Dans `tools/json_check.py`, remplacer :

```python
    elif t == "hello" and obj.get("bloc") == "base":
```

   par :

```python
    elif t == "reseau" and obj.get("bloc") == "ip":
        w = obj.get("wifi")
        if isinstance(w, dict) and isinstance(w.get("connecte"), bool):
            vus = [k for k in ("rssi_dbm", "ip") if w.get(k) is not None]
            if w["connecte"] and len(vus) != 2:
                errs.append("reseau/ip : wifi connecte sans rssi_dbm ou sans ip")
            if not w["connecte"] and vus:
                errs.append(f"reseau/ip : wifi deconnecte avec {', '.join(vus)}")
    elif t == "hello" and obj.get("bloc") == "base":
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_json_check.py -v`
Attendu : « Ran 17 tests » puis « OK »

- [ ] **Étape 5 : Déclarer l'API du transport (`src/net_udp_wifi.h`)**

On écrit d'abord l'en-tête (l'API du contrat), puis ses appelants en deux
morceaux, `json_mode` (étape 6), puis `cli` et `main` (étape 8), en compilant
après chacun ; l'implémentation vient en dernier (étape 10). Tant qu'elle
manque, chaque fichier compile, et seule l'édition de liens échoue, sur les
fonctions du transport : c'est le test qui échoue.

Créer `src/net_udp_wifi.h` :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/net_udp.h (adapte : UDP sur le Wi-Fi par les sockets lwIP, IPv4, port kPortUdp, NVS hotte/cle, files et plafonds de la sonde, sans #if MATTER_NET_THREAD)
#pragma once
// ===========================================================================
//  Transport reseau du protocole compagnon (docs/PROTOCOLE-JSON.md, section 9 ;
//  ScreenBar section 10)
//
//  Socket UDP de lwIP sur le Wi-Fi, IPv4, port kPortUdp (5480), enveloppe H1
//  (h1_proto.h) identique a la ScreenBar, cle partagee en NVS (hotte/cle).
//  L'app (Mac) joint la sonde par son nom mDNS (hotte-sonde.local) ou son
//  adresse IPv4.
//
//  Tout se passe dans la tache loop (spec 8.7 : un seul producteur) :
//  - reception : recvfrom non bloquant, au plus 4 datagrammes par tour (la
//    file de reception de lwIP en garde 6) ; poignee de main, verification,
//    commandes (cli.cpp : cliRunRemote) ;
//  - emission : datagrammes fermes (en-tete H1 + JSON) dans une file de 32,
//    remis a lwIP par sendto non bloquant, au plus 4 par tour ; debit moyen
//    plafonne a 50 000 octets/s, credit de 8 192 ; pas partis en 4 s :
//    perdus et comptes. Le DEFI d'une poignee de main passe devant, hors
//    plafond. Jamais d'attente : file pleine, la ligne est perdue et comptee
//    par l'appelant.
//  - le socket s'ouvre des qu'une cle existe et que le Wi-Fi a une adresse
//    (la pile lwIP ne demarre qu'avec la station) ; il se ferme a
//    l'effacement de la cle.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "json_out.h"

// setup(), apres netWifiBegin() : cle lue en NVS.
void netUdpBegin();
// loop(), apres netWifiPoll() : socket, datagrammes recus (poignees de main,
// commandes), oubli des sessions, emission.
void netUdpPoll();

// Ligne machine (objet JSON sans RS ni LF) pour la session etablie slot
// (0..h1::kSlots-1) : enveloppe H1 et mise en file. false : pas de session,
// ou file pleine (ligne perdue, comptee par l'appelant).
bool netUdpSend(uint8_t slot, const uint8_t *json, size_t len);
// Places libres dans la file d'emission (partagee par les sessions) ; 0 socket ferme.
uint8_t netUdpFreeSlots();
// 'json 0' execute pour la session slot : sa place revient au prochain client
// sans attendre 30 s de silence (h1::Table::end).
void netUdpEnd(uint8_t slot);
// Nouvelle commande admise de la session slot : elle sert de nouveau
// (h1::Table::resume).
void netUdpResume(uint8_t slot);

// Bloc 'reseau' 'ip' : schema de la ScreenBar (srp null, tampons null), plus
// mdns et wifi.
void netUdpJson(jsonp::Writer &w, uint32_t now);

// Cle (json cle, USB seulement). netUdpKeyNew : cle = HMAC-SHA256(alea de
// l'app, alea de la sonde), ecrite en NVS, rendue une fois en hexa (keyHex,
// 64 + 1) avec son empreinte (kid, 8 + 1) ; toutes les sessions tombent.
enum class NetKeyResult : uint8_t {
  Ok,
  Crypto,  // cle non calculee : rien ne change
  Nvs,     // cle non ecrite : rien ne change
  Load,    // cle ecrite en NVS mais pas chargee : transport coupe jusqu'au redemarrage
};
NetKeyResult netUdpKeyNew(const uint8_t appRandom[32], char keyHex[65], char kid[9]);
// Efface la cle (NVS et memoire) : plus de transport reseau. false : NVS en
// echec (la cle est quand meme retiree de la memoire).
bool netUdpKeyErase();
bool netUdpKid(char kid[9]);  // false : aucune cle
```

Rien ne l'inclut encore : la compilation vient avec ses appelants.

- [ ] **Étape 6 : Brancher `json_mode` sur le transport (sessions réseau, cache des réponses, clé)**

`src/json_mode.h` (deux remplacements) :

1. Dans `src/json_mode.h`, remplacer :

```cpp
//  Une session par transport (origine, jsonp::kUsb puis, en tache 19, une
//  par session reseau) : ses reglages, son n, sa file, ses pertes. Les
//  evenements partent vers chaque session en mode machine, formates pour
//  elle ; une reponse, vers l'origine de sa commande seulement.
```

   par :

```cpp
//  Une session par transport (origine : jsonp::kUsb, puis une par session
//  reseau etablie, net_udp_wifi.cpp) : ses reglages, son n, sa file, ses
//  pertes. Les evenements partent vers chaque session en mode machine,
//  formates pour elle ; une reponse, vers l'origine de sa commande seulement.
```

2. Dans `src/json_mode.h`, remplacer :

```cpp
// --- Transport reseau (cli.cpp ; etendu en tache 19) ------------------------

// Origine de la commande en cours : jsonp::kUsb hors cliRunRemote().
void jsonSetOrigin(uint8_t origin);
uint8_t jsonOrigin();
```

   par :

```cpp
// --- Transport reseau (net_udp_wifi.cpp, cli.cpp) --------------------------

// Origine de la commande en cours : jsonp::kUsb hors cliRunRemote().
void jsonSetOrigin(uint8_t origin);
uint8_t jsonOrigin();
// Session reseau neuve dans cet emplacement (origine), ou partie (oubliee,
// remplacee, cle changee) : son etat de session repart de zero, sans rien
// emettre (n continue).
void jsonRemoteReset(uint8_t origin);
void jsonNoteRemoteRx(uint8_t origin);  // message au MAC juste recu (bail)
// Origine reseau, ligne avec id, avant tout le reste (cadence comprise), shown
// etant la commande telle que la reponse la montre (reponse.cmd) :
//  - meme id, meme commande, reponse en cache : elle repart, rien n'est execute ;
//  - reponse differee de cet id encore en file : rien (elle partira) ;
//  - id deja traite (hors cache, ou autre commande) : reponse deja_traite ;
//  - sinon l'id est neuf (le plus haut id de la session avance) : false, a traiter.
bool jsonRemoteAdmit(uint32_t id, const char *shown);
```

`src/json_mode.cpp` (vingt-cinq remplacements, dans cet ordre ; chaque ancien texte est unique dans le fichier) :

1. Dans `src/json_mode.cpp`, remplacer :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/json_mode.cpp (adapte : profil hotte, instantanes de la sonde, evenements trame et log ; sans lampe, LED, Matter ni livraison ; sessions reseau en tache 19)
```

   par :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/json_mode.cpp (adapte : profil hotte, instantanes de la sonde, evenements trame et log ; sans lampe, LED, Matter ni livraison ; sessions reseau sur UDP/Wi-Fi)
```

2. Dans `src/json_mode.cpp`, remplacer :

```cpp
#include "fw_version.h"
#include "sonde.h"
```

   par :

```cpp
#include "fw_version.h"
#include "h1_proto.h"
#include "net_udp_wifi.h"
#include "sonde.h"
```

3. Dans `src/json_mode.cpp`, remplacer :

```cpp
  uint32_t t0;    // durAtSend : duree_ms mesuree a l'envoi (instantane)
  bool durAtSend;
  char cmd[kCmdTextMax + 1];
};

// Une session par transport (origine). L'USB (kUsb) existe toujours ; les
// origines reseau viennent avec le transport UDP (tache 19).
```

   par :

```cpp
  uint32_t t0;    // durAtSend : duree_ms mesuree a l'envoi (instantane)
  bool durAtSend;
  bool cache;     // gardee pour un id repete (reseau) ; pas un refus deja_traite
  char cmd[kCmdTextMax + 1];
};

// Une session par transport (origine). L'USB (kUsb) existe toujours ; une
// origine reseau n'a de sens que tant que sa session H1 est etablie
// (net_udp_wifi.cpp : jsonRemoteReset a chaque changement).
```

4. Dans `src/json_mode.cpp`, remplacer :

```cpp
  uint32_t loopMaxMs = 0;            // plus long tour de loop() depuis le bloc sys emis
```

   par :

```cpp
  uint32_t loopMaxMs = 0;            // plus long tour de loop() depuis le bloc sys emis
  uint32_t topId = 0;                // reseau : plus haut id admis dans cette session (jsonRemoteAdmit)
```

5. Dans `src/json_mode.cpp`, remplacer :

```cpp
static Sink sSinks[kOrigins];
static uint8_t sOrigin = kUsb;  // origine de la commande en cours
```

   par :

```cpp
static Sink sSinks[kOrigins];
static ReplyCache sCache[kOrigins - 1];  // origines reseau seulement
static uint8_t sOrigin = kUsb;  // origine de la commande en cours
```

6. Dans `src/json_mode.cpp`, remplacer :

```cpp
// Place d'emission libre sur ce transport : octets du tampon de HWCDC (USB).
// Transport reseau : tache 19 (d'ici la, aucune place).
static int room(uint8_t o) {
  if (o == kUsb) return Serial.availableForWrite();
  return 0;
}

// Ecrit la ligne fermee de sW sur ce transport, entiere ou pas du tout.
static bool emit(uint8_t o) {
  const size_t len = sW.size();
  if (o != kUsb || Serial.availableForWrite() < (int)len) return false;
  Serial.write(sW.data(), len);
  return true;
}
```

   par :

```cpp
// Place d'emission libre sur ce transport : octets du tampon de HWCDC (USB),
// ou places de la file des datagrammes x une ligne (reseau).
static int room(uint8_t o) {
  if (o == kUsb) return Serial.availableForWrite();
  return (int)netUdpFreeSlots() * (int)kLineMax;
}

// Ecrit la ligne fermee de sW sur ce transport, entiere ou pas du tout.
static bool emit(uint8_t o) {
  const size_t len = sW.size();
  if (o == kUsb) {
    if (Serial.availableForWrite() < (int)len) return false;
    Serial.write(sW.data(), len);
    return true;
  }
  // Un datagramme : l'objet JSON seul, sans RS ni LF (ScreenBar 10.2).
  return len >= 2 && netUdpSend((uint8_t)(o - 1), sW.data() + 1, len - 2);
}
```

7. Dans `src/json_mode.cpp`, remplacer :

```cpp
// Texte emis par le protocole lui-meme sur l'USB (fin de bail, invite) : meme
```

   par :

```cpp
// Evenement frequent (trame, log) vers une session reseau : seulement s'il
// reste ensuite la place d'une ligne periodique ou d'une reponse (deux
// datagrammes libres). Sinon perdu, n consomme et compte.
static bool eventRoom(uint8_t o) {
  if (!remote(o) || room(o) >= 2 * (int)kLineMax) return true;
  sSinks[o].n++;
  lose(sSinks[o]);
  return false;
}

// Texte emis par le protocole lui-meme sur l'USB (fin de bail, invite) : meme
```

8. Dans `src/json_mode.cpp`, remplacer :

```cpp
  sW.str(nullptr, "trames");
  sW.str(nullptr, "log");
  sW.end();
}
```

   par :

```cpp
  sW.str(nullptr, "trames");
  sW.str(nullptr, "log");
  sW.str(nullptr, "udp");
  sW.str(nullptr, "cle");
  sW.str(nullptr, "mdns");
  sW.end();
}
```

9. Dans `src/json_mode.cpp`, remplacer :

```cpp
// Une ligne de la file de la session o : formatee maintenant, avec les
// valeurs du moment.
static void produce(uint8_t o, const Queued &q, uint32_t now) {
  if (!claim()) return;
  Sink &k = sSinks[o];
  switch (q.item) {
```

   par :

```cpp
// Une reponse vient de partir vers une origine reseau : gardee pour un id repete.
static void cacheReply(uint8_t o, const Reply &r) {
  if (remote(o) && r.fin) sCache[o - 1].put(r);
}

// Une ligne de la file de la session o : formatee maintenant, avec les
// valeurs du moment.
static void produce(uint8_t o, const Queued &q, uint32_t now) {
  if (!claim()) return;
  Sink &k = sSinks[o];
  Reply sent;
  bool isReply = false;
  switch (q.item) {
```

10. Dans `src/json_mode.cpp`, remplacer :

```cpp
    case Item::Compteurs: compteurs(o, now); break;
    case Item::Heartbeat: heartbeat(sW, k.n, now, sBoot, upS(), k.lost); break;
```

   par :

```cpp
    case Item::Compteurs: compteurs(o, now); break;
    case Item::NetIp:
      sW.begin("reseau", k.n, now);
      sW.str("bloc", "ip");
      netUdpJson(sW, now);
      break;
    case Item::Heartbeat: heartbeat(sW, k.n, now, sBoot, upS(), k.lost); break;
```

11. Dans `src/json_mode.cpp`, remplacer :

```cpp
      reply(sW, k.n, now, r);
      p.used = false;
      break;
    }
    default:  // bloc absent de ce build (etat.injection : tache 23 ; reseau.ip : tache 19)
      sBusy = false;
      return;
  }
  // Le maximum n'est remis a 0 que s'il est parti : perdue, la ligne suivante le porte.
  if (send(o) && q.item == Item::EtatSys) k.loopMaxMs = 0;
}
```

   par :

```cpp
      reply(sW, k.n, now, r);
      p.used = false;
      sent = r;
      isReply = p.cache;
      break;
    }
    default:  // bloc absent de ce build (etat.injection : tache 23)
      sBusy = false;
      return;
  }
  // Le maximum n'est remis a 0 que s'il est parti : perdue, la ligne suivante le porte.
  if (send(o) && q.item == Item::EtatSys) k.loopMaxMs = 0;
  // Perdue ou non, une reponse est donnee pour cet id : un id repete la renvoie
  // (sauf un refus deja_traite, jamais garde).
  if (isReply) cacheReply(o, sent);
}
```

12. Dans `src/json_mode.cpp`, remplacer :

```cpp
// Bloc reseau.ip : avec le transport reseau (tache 19). Rien avant.
static void pushNet(uint8_t, uint32_t, bool) {}
```

   par :

```cpp
static void pushNet(uint8_t o, uint32_t now, bool session) { push(o, Item::NetIp, now, session); }
```

13. Dans `src/json_mode.cpp`, remplacer :

```cpp
// Ecrit la reponse tout de suite (perdue et comptee si elle ne tient pas).
static void replyEmit(uint8_t o, const Reply &r) {
  if (claim()) {
    reply(sW, sSinks[o].n, millis(), r);
    send(o);
  } else {
    // Ligne deja en cours de formatage (jamais vu) : perdue, n consomme et compte.
    sSinks[o].n++;
    lose(sSinks[o]);
  }
}
```

   par :

```cpp
// Ecrit la reponse tout de suite (perdue et comptee si elle ne tient pas).
// Perdue ou non, elle est donnee pour cet id : gardee (reseau) pour un renvoi.
static void replyEmit(uint8_t o, const Reply &r, bool cache) {
  if (claim()) {
    reply(sW, sSinks[o].n, millis(), r);
    send(o);
  } else {
    // Ligne deja en cours de formatage (jamais vu) : perdue, n consomme et compte.
    sSinks[o].n++;
    lose(sSinks[o]);
  }
  if (cache) cacheReply(o, r);
}
```

14. Dans `src/json_mode.cpp`, remplacer :

```cpp
static void replyQueue(uint8_t o, const Reply &r, uint32_t t0, bool durAtSend) {
```

   par :

```cpp
static void replyQueue(uint8_t o, const Reply &r, uint32_t t0, bool durAtSend, bool cache = true) {
```

15. Dans `src/json_mode.cpp`, remplacer :

```cpp
    p.t0 = t0;
    p.durAtSend = durAtSend;
    copyCmd(p.cmd, r.cmd);
```

   par :

```cpp
    p.t0 = t0;
    p.durAtSend = durAtSend;
    p.cache = cache;
    copyCmd(p.cmd, r.cmd);
```

16. Dans `src/json_mode.cpp`, remplacer :

```cpp
    now.upS = upS();
  }
  replyEmit(o, now);
}

// Reponse immediate. Reseau : la cle n'y part jamais ; sans place, ou derriere
// une reponse deja en file (l'ordre des reponses est garde), elle attend dans
// la file de la session au lieu d'etre perdue.
static void replyTo(uint8_t o, const Reply &r) {
  Reply out = r;
  if (remote(o)) out.key = nullptr;
  if (remote(o) && (room(o) < (int)kLineMax || sSinks[o].q.has(Item::Reply))) {
    replyQueue(o, out, 0, false);
    return;
  }
  replyEmit(o, out);
}
```

   par :

```cpp
    now.upS = upS();
  }
  replyEmit(o, now, cache);
}

// Reponse immediate. Reseau : la cle n'y part jamais ; sans place, ou derriere
// une reponse deja en file (l'ordre des reponses est garde), elle attend dans
// la file de la session au lieu d'etre perdue.
static void replyTo(uint8_t o, const Reply &r, bool cache = true) {
  Reply out = r;
  if (remote(o)) out.key = nullptr;
  if (remote(o) && (room(o) < (int)kLineMax || sSinks[o].q.has(Item::Reply))) {
    replyQueue(o, out, 0, false, cache);
    return;
  }
  replyEmit(o, out, cache);
}
```

17. Dans `src/json_mode.cpp`, remplacer :

```cpp
uint8_t jsonOrigin() { return sOrigin; }

void jsonCountRejected() {
```

   par :

```cpp
uint8_t jsonOrigin() { return sOrigin; }

void jsonRemoteReset(uint8_t origin) {
  if (origin == kUsb || origin >= kOrigins) return;
  // Sur place (un Sink temporaire couterait pres de 2 Ko de pile). n continue :
  // numero de ligne du transport depuis le demarrage ; le reste repart des
  // valeurs par defaut.
  Sink &k = sSinks[origin];
  k.machine = false;
  k.periodMs = kPeriodDefault;
  k.countersMs = kCountersDefault;
  k.netMs = kNetDefault;
  k.leaseS = kLeaseDefault;
  k.frames = true;
  k.log = false;
  k.nextEtat = k.nextCpt = k.nextNet = k.nextHb = 0;
  k.lastRx = k.lastCmd = 0;
  k.lost = k.tooLong = k.rejected = 0;
  k.loopMaxMs = 0;
  k.topId = 0;
  k.trameNum = 0;
  k.trameSautee = false;
  k.q.clear();
  for (PendingReply &p : k.replies) p.used = false;
  k.trameCap = RateCap(kTramesParSeconde);
  k.logCap = RateCap(kLogCap);
  k.cadence = Cadence();
  sCache[origin - 1].clear();
}

void jsonNoteRemoteRx(uint8_t origin) {
  if (origin != kUsb && origin < kOrigins) sSinks[origin].lastRx = millis();
}

bool jsonRemoteAdmit(uint32_t id, const char *shown) {
  const uint8_t o = sOrigin;
  if (o == kUsb || o >= kOrigins) return false;
  Sink &k = sSinks[o];
  // Reponse differee de cet id encore en file (instantane) : elle partira.
  for (const PendingReply &p : k.replies)
    if (p.used && p.r.id == id) return true;
  const Reply *cached = sCache[o - 1].find(id);
  if (cached && !strcmp(cached->cmd, shown)) {
    // Meme id, meme commande : la reponse perdue repart, rien n'est reexecute.
    Reply out = *cached;
    char cmd[kCmdTextMax + 1];
    copyCmd(cmd, cached->cmd);  // put() va reecrire l'entree : plus de pointeur dedans
    out.cmd = cmd;
    replyTo(o, out);
    return true;
  }
  if (cached || id <= k.topId) {
    // id deja traite, reponse plus en cache (ou autre commande sous le meme id) :
    // jamais de nouvelle execution. Refus non garde (le cache reste celui de l'id).
    Reply r;
    r.id = id;
    r.cmd = shown;
    r.ok = false;
    r.code = "deja_traite";
    r.msg = "id deja traite (reponse plus disponible) : rien n'est reexecute";
    replyTo(o, r, false);
    return true;
  }
  k.topId = id;
  // Nouvelle commande : la session sert de nouveau, meme apres un 'json 0'
  // (qui, s'il est cette commande, la termine ensuite a son tour).
  netUdpResume((uint8_t)(o - 1));
  return false;
}

void jsonCountRejected() {
```

18. Dans `src/json_mode.cpp`, remplacer :

```cpp
  out.printf("  sonde    : %lu ligne(s) perdue(s), %lu trame(s) sautee(s), %lu rejet(s), toutes sessions\n",
```

   par :

```cpp
  for (uint8_t o = 1; o < kOrigins; o++) {
    const Sink &r = sSinks[o];
    if (!r.n && !r.machine) continue;
    out.printf("  reseau %u : mode machine %s ; n = %lu, %lu perdue(s), %lu refusee(s)\n", (unsigned)o,
               r.machine ? "actif" : "coupe", (unsigned long)r.n, (unsigned long)r.lost, (unsigned long)r.rejected);
  }
  out.printf("  sonde    : %lu ligne(s) perdue(s), %lu trame(s) sautee(s), %lu rejet(s), toutes sessions\n",
```

19. Dans `src/json_mode.cpp`, remplacer :

```cpp
// Periode : 0 (coupe, si permis) ou lo..60000 ms.
static bool setPeriod(char *p, uint32_t lo, bool zeroOk, uint32_t *out, uint32_t *next, uint32_t now) {
  uint32_t v;
  if (!parseU32(nextWord(p), &v) || *nextWord(p) || (!v && !zeroOk) || (v && (v < lo || v > 60000))) return false;
  *out = v;
  *next = now + v;
  return true;
}
```

   par :

```cpp
// Periode : 0 (coupe, si permis) ou lo..60000 ms.
static bool setPeriod(char *p, uint32_t lo, bool zeroOk, uint32_t *out, uint32_t *next, uint32_t now) {
  uint32_t v;
  if (!parseU32(nextWord(p), &v) || *nextWord(p) || (!v && !zeroOk) || (v && (v < lo || v > 60000))) return false;
  *out = v;
  *next = now + v;
  return true;
}

// 'json cle ...' (ScreenBar 10.4) : USB seulement. La liste blanche la refuse
// deja au reseau ; refusee ici aussi (defense en profondeur : la reponse de
// 'nouvelle' porte la cle).
static void keyCommand(char *p, const JsonCmd &c) {
  if (sOrigin != kUsb) {
    replyNow(c, false, "interdite", "json cle : USB seulement");
    return;
  }
  const char *w = nextWord(p);
  char kid[9] = {};
  if (!*w) {
    const bool has = netUdpKid(kid);
    Reply r;
    r.id = c.id;
    r.cmd = c.cmd;
    r.durMs = millis() - c.t0;
    r.hasKid = true;
    r.kid = has ? kid : nullptr;
    if (c.hasId) jsonReply(r);
    else if (has) Serial.printf("json cle : empreinte %s (transport reseau actif, port UDP %u)\n", kid, kPortUdp);
    else Serial.println("json cle : aucune cle, transport reseau coupe");
    return;
  }
  if (!strcmp(w, "nouvelle")) {
    const char *hex = nextWord(p);
    uint8_t appRandom[32];
    bool hexOk = strlen(hex) == 64 && !*nextWord(p);
    for (uint8_t i = 0; hexOk && i < 32; i++) {
      auto nib = [](char ch) -> int {
        return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
      };
      const int hi = nib(hex[2 * i]), lo = nib(hex[2 * i + 1]);
      if (hi < 0 || lo < 0) hexOk = false;
      else appRandom[i] = (uint8_t)(hi << 4 | lo);
    }
    if (!c.hasId) {
      // La cle ne s'affiche jamais en texte : 'pio device monitor' enregistre la
      // session (log2file) dans un fichier a la racine du depot.
      h1::wipe(appRandom, sizeof(appRandom));
      Serial.println("json cle nouvelle : reservee a l'outil (ligne avec id=, la cle part dans la reponse) :");
      Serial.println("  python3 tools/hotte_udp.py cle <port>");
      return;
    }
    if (!hexOk) {
      h1::wipe(appRandom, sizeof(appRandom));
      replyNow(c, false, "usage", "json cle nouvelle <64 hexa majuscules> (alea de l'app)");
      return;
    }
    // La reponse est la seule copie de la cle : pas de cle neuve si elle ne peut
    // pas partir tout de suite (tampon d'emission USB plein).
    if (Serial.availableForWrite() < (int)kLineMax) {
      h1::wipe(appRandom, sizeof(appRandom));
      replyNow(c, false, "refuse", "tampon USB plein : rien n'est change, reessayer");
      return;
    }
    char keyHex[65];
    const NetKeyResult res = netUdpKeyNew(appRandom, keyHex, kid);
    h1::wipe(appRandom, sizeof(appRandom));
    if (res == NetKeyResult::Crypto || res == NetKeyResult::Nvs) {
      replyNow(c, false, "refuse",
               res == NetKeyResult::Nvs ? "cle non ecrite (NVS) : ancienne cle gardee"
                                        : "cle non creee (crypto) : ancienne cle gardee");
      return;
    }
    Reply r;
    r.id = c.id;
    r.cmd = c.cmd;
    r.durMs = millis() - c.t0;
    r.msg = res == NetKeyResult::Ok ? "nouvelle cle : les sessions reseau tombent"
                                    : "cle ecrite mais pas chargee : transport reseau coupe jusqu'au redemarrage";
    r.key = keyHex;
    r.hasKid = true;
    r.kid = kid;
    jsonReply(r);
    h1::wipe(keyHex, sizeof(keyHex));
    return;
  }
  if (!strcmp(w, "efface") && !*nextWord(p)) {
    const bool ok = netUdpKeyErase();
    if (c.hasId) {
      Reply r;
      r.id = c.id;
      r.cmd = c.cmd;
      r.ok = ok;
      r.code = ok ? "ok" : "refuse";
      r.msg = ok ? "cle effacee : transport reseau coupe" : "effacement NVS en echec (cle retiree de la memoire)";
      r.durMs = millis() - c.t0;
      r.hasKid = true;
      r.kid = nullptr;
      jsonReply(r);
    } else {
      Serial.println(ok ? "json cle : cle effacee, transport reseau coupe"
                        : "json cle : effacement NVS en echec (cle retiree de la memoire)");
    }
    return;
  }
  replyNow(c, false, "usage", "json cle [nouvelle <64 hexa> | efface]");
  if (!c.hasId) Serial.println("Usage : json cle [efface]   ('json cle nouvelle' : tools/hotte_udp.py cle <port>)");
}
```

20. Dans `src/json_mode.cpp`, remplacer :

```cpp
      "trames 0|1 | log 0|1]";
```

   par :

```cpp
      "trames 0|1 | log 0|1 | cle]";
```

21. Dans `src/json_mode.cpp`, remplacer :

```cpp
      replyNow(c, true, "ok", k.machine ? nullptr : "deja en mode humain");
      if (k.machine) leaveMachine(o, false, now);
      else if (!c.hasId) Serial.println("json : mode machine deja coupe");
      return;
```

   par :

```cpp
      replyNow(c, true, "ok", k.machine ? nullptr : "deja en mode humain");
      if (k.machine) leaveMachine(o, false, now);
      else if (!c.hasId) Serial.println("json : mode machine deja coupe");
      // Le client s'en va : sa place revient au suivant sans attendre 30 s (ScreenBar 10.4).
      if (rem) netUdpEnd((uint8_t)(o - 1));
      return;
```

22. Dans `src/json_mode.cpp`, remplacer :

```cpp
  } else if (!strcmp(sub, "cle")) {
    // Cle du transport reseau (spec 8.5) : avec le transport UDP (tache 19).
    replyNow(c, false, "refuse", "json cle : transport reseau absent de ce firmware");
    if (!c.hasId) Serial.println("json cle : transport reseau absent de ce firmware (USB seulement)");
    return;
  } else {
```

   par :

```cpp
  } else if (!strcmp(sub, "cle")) {
    keyCommand(p, c);
    return;
  } else {
```

23. Dans `src/json_mode.cpp`, remplacer :

```cpp
    if (k.trameSautee) {
      k.trameCap.skip();  // aucun n consomme ; la suivante produite porte 'sautes'
      sSautes++;
      continue;
    }
    if (!claim()) continue;
```

   par :

```cpp
    if (k.trameSautee) {
      k.trameCap.skip();  // aucun n consomme ; la suivante produite porte 'sautes'
      sSautes++;
      continue;
    }
    if (!eventRoom(o) || !claim()) continue;
```

24. Dans `src/json_mode.cpp`, remplacer :

```cpp
    if (!claim()) continue;  // ligne en cours (jamais attendu) : en texte sur l'USB
    k.logCap.take();
```

   par :

```cpp
    if (!eventRoom(o)) continue;
    if (!claim()) continue;  // ligne en cours (jamais attendu) : en texte sur l'USB
    k.logCap.take();
```

25. Dans `src/json_mode.cpp`, remplacer :

```cpp
  // L'USB d'abord ; les sessions reseau, qui partageront la file des
  // datagrammes et son debit, a tour de role.
```

   par :

```cpp
  // L'USB d'abord ; les sessions reseau, qui partagent la file des
  // datagrammes et son debit, a tour de role (deux instantanes simultanes
  // avancent ensemble).
```

- [ ] **Étape 7 : Compiler : seule l'édition de liens échoue**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -oE "(warning|error): .*|undefined reference to .[a-zA-Z]+|\[(SUCCESS|FAILED)\]" | LC_ALL=C sort -u`
Attendu : ÉCHEC, sans aucun `warning` ni erreur de compilation ; l'éditeur de liens ne trouve que les fonctions du transport appelées par `json_mode.cpp` :

```text
[FAILED]
error: ld returned 1 exit status
undefined reference to `netUdpEnd
undefined reference to `netUdpFreeSlots
undefined reference to `netUdpJson
undefined reference to `netUdpKeyErase
undefined reference to `netUdpKeyNew
undefined reference to `netUdpKid
undefined reference to `netUdpSend
```

`netUdpResume` n'y est pas encore : seul `jsonRemoteAdmit` l'appelle, que rien n'appelle avant l'étape 8 (l'éditeur de liens retire les fonctions inutilisées). Toute autre ligne (`warning`, `error:` d'un fichier `.cpp`) : corriger l'étape 6 avant d'aller plus loin.

- [ ] **Étape 8 : Brancher `cli` et `main` sur le transport**

Remplacer tout le contenu de `src/cli.h` par :

```cpp
#pragma once
#include <stdint.h>
void cliBegin();                                  // tache 10
void cliPoll();                                   // tache 10 : lit l'USB, execute les lignes
// Tache 19 : ligne recue par le transport reseau (net_udp_wifi.cpp), deja
// authentifiee : origine 1..jsonp::kOrigins-1. Memes regles que l'USB avec un
// id, plus celles du reseau (ScreenBar 10.5) : id obligatoire, id repete servi
// depuis le cache des reponses, liste blanche ('interdite'). tooLong : la
// charge depassait 127 octets (refusee avec son id, trop_long).
void cliRunRemote(uint8_t origin, char *line, bool tooLong = false);
```

`src/cli.cpp` (six remplacements) :

1. Dans `src/cli.cpp`, remplacer :

```cpp
// Copie partielle de benq-screenbar-halo-matter@c58a506 : src/cli.cpp (adapte : runLine et cliPoll repris, commandes de la sonde)
```

   par :

```cpp
// Copie partielle de benq-screenbar-halo-matter@c58a506 : src/cli.cpp (adapte : runLine, cliPoll et cliRunRemote repris, commandes de la sonde)
```

2. Dans `src/cli.cpp`, remplacer :

```cpp
#include "net_wifi.h"
#include "sonde.h"
```

   par :

```cpp
#include "net_udp_wifi.h"
#include "net_wifi.h"
#include "sonde.h"
```

3. Dans `src/cli.cpp`, remplacer :

```cpp
                  (int)netWifiRssi(), kNomMdns, (unsigned long)netWifiPertes());
  }
}
```

   par :

```cpp
                  (int)netWifiRssi(), kNomMdns, (unsigned long)netWifiPertes());
  }
  char kid[9];
  if (netUdpKid(kid)) Serial.printf("udp : port %u, cle %s\n", (unsigned)kPortUdp, kid);
  else Serial.printf("udp : port %u ferme, aucune cle ('python3 tools/hotte_udp.py cle <port>')\n", (unsigned)kPortUdp);
}
```

4. Dans `src/cli.cpp`, remplacer :

```cpp
// A distance (tache 19), jsonRemoteAdmit passera avant tout refus.
static void runLine(char *line, bool tooLong) {
```

   par :

```cpp
// A distance, jsonRemoteAdmit passe avant tout refus (id deja vu).
static void runLine(char *line, bool tooLong) {
```

5. Dans `src/cli.cpp`, remplacer :

```cpp
  const JsonCmd c{hasId, id, shown, t0};
  if (tooLong) {
```

   par :

```cpp
  const JsonCmd c{hasId, id, shown, t0};
  // A distance, d'abord l'id : une commande renvoyee (reponse perdue) recoit la
  // meme reponse sans nouvelle execution, avant tout refus de cadence qui
  // ecraserait sa reponse.
  if (remote && jsonRemoteAdmit(id, shown)) return;
  if (tooLong) {
```

6. Dans `src/cli.cpp`, remplacer :

```cpp
// ---------------------------------------------------------------------------

static jsonp::LineAssembler sLine;  // 127 caracteres au plus, prefixe id= compris
```

   par :

```cpp
// ---------------------------------------------------------------------------

void cliRunRemote(uint8_t origin, char *line, bool tooLong) {
  // Jamais l'USB par ce chemin (il echappe a la liste blanche).
  if (origin == jsonp::kUsb || origin >= jsonp::kOrigins) return;
  const uint8_t prev = jsonOrigin();
  jsonSetOrigin(origin);
  if (jsonOrigin() == origin) runLine(line, tooLong);
  jsonSetOrigin(prev);
}

static jsonp::LineAssembler sLine;  // 127 caracteres au plus, prefixe id= compris
```

`src/main.cpp` (quatre remplacements) :

1. Dans `src/main.cpp`, remplacer :

```cpp
#include "net_wifi.h"
#include "reglages.h"
```

   par :

```cpp
#include "net_udp_wifi.h"
#include "net_wifi.h"
#include "reglages.h"
```

2. Dans `src/main.cpp`, remplacer :

```cpp
  netWifiBegin();  // identifiants en NVS : station, mDNS ; sans eux, radio eteinte
}
```

   par :

```cpp
  netWifiBegin();  // identifiants en NVS : station, mDNS ; sans eux, radio eteinte
  netUdpBegin();   // cle H1 en NVS (hotte/cle) : socket UDP 5480 des que le Wi-Fi a une adresse
}
```

3. Dans `src/main.cpp`, remplacer :

```cpp
  netWifiPoll();
  vTaskDelay(1);  // laisse tourner la tache IDLE
```

   par :

```cpp
  netWifiPoll();
  netUdpPoll();
  vTaskDelay(1);  // laisse tourner la tache IDLE
```

4. Dans `src/main.cpp`, remplacer :

```cpp
//  reglages NVS, interruption des fronts, capture RMT, console, Wi-Fi.
//  loop() : console, capture (parties -> lignes trame du mode machine, ou
//  texte), mode machine (bail, lignes periodiques), Wi-Fi, puis la tache IDLE.
```

   par :

```cpp
//  reglages NVS, interruption des fronts, capture RMT, console, Wi-Fi, UDP.
//  loop() : console, capture (parties -> lignes trame du mode machine, ou
//  texte), mode machine (bail, lignes periodiques), Wi-Fi, UDP (commandes
//  recues, datagrammes emis), puis la tache IDLE.
```

- [ ] **Étape 9 : Compiler : l'édition de liens échoue encore, sur les seules fonctions du transport**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -oE "(warning|error): .*|undefined reference to .[a-zA-Z]+|\[(SUCCESS|FAILED)\]" | LC_ALL=C sort -u`
Attendu : ÉCHEC, toujours sans `warning` ni erreur de compilation ; les dix fonctions de `net_udp_wifi.h` manquent (`main.cpp` appelle `netUdpBegin` et `netUdpPoll`, `cli.cpp` `netUdpKid`, et `jsonRemoteAdmit`, appelée par `runLine`, `netUdpResume`) :

```text
[FAILED]
error: ld returned 1 exit status
undefined reference to `netUdpBegin
undefined reference to `netUdpEnd
undefined reference to `netUdpFreeSlots
undefined reference to `netUdpJson
undefined reference to `netUdpKeyErase
undefined reference to `netUdpKeyNew
undefined reference to `netUdpKid
undefined reference to `netUdpPoll
undefined reference to `netUdpResume
undefined reference to `netUdpSend
```

- [ ] **Étape 10 : Écrire `src/net_udp_wifi.cpp`**

Créer `src/net_udp_wifi.cpp` :

```cpp
// Copie de benq-screenbar-halo-matter@c58a506 : src/net_udp.cpp (adapte : sockets lwIP IPv4 sur le Wi-Fi au lieu d'OpenThread, reception lue dans la tache loop, h1::Peer en IPv4 mappee, file de 32 datagrammes, 50 000 o/s, credit de 8 192, NVS hotte/cle, bloc reseau ip de la sonde)
#include "net_udp_wifi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <errno.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <string.h>

#include "cli.h"
#include "config.h"
#include "h1_proto.h"
#include "json_mode.h"
#include "net_wifi.h"

// Une origine JSON par session etablie, apres l'USB (json_mode.cpp).
static_assert(jsonp::kOrigins == h1::kSlots + 1, "une origine JSON par session H1 etablie");

static const char *const kNvsKey = "cle";  // dans l'espace kNvsEspace ("hotte")

// Commande de 127 octets et son en-tete de 56 : 256 laisse de la marge ; au-dela,
// le datagramme n'est pas pour nous (compte, jete).
static constexpr size_t kRxMax = 256;
static constexpr uint8_t kRxPerTurn = 4;  // la file de reception de lwIP en garde 6
// En-tete H1 et ligne machine sans RS ni LF : 56 + 1022 octets, sous la MTU
// de 1 500 du Wi-Fi (jamais fragmente).
static constexpr size_t kTxMax = h1::kHeaderMax + jsonp::kLineMax - 2;
static constexpr uint8_t kTxN = 32;
static constexpr uint32_t kTxStaleMs = 4000;  // pas parti dans ce delai : perdu
static constexpr uint8_t kTxPerTurn = 4;      // datagrammes remis a lwIP par tour de loop()
// Debit moyen plafonne (spec 8.5) : 50 000 octets/s, credit de 8 192 (huit
// lignes d'un Ko d'affilee). Au repos, le profil distant en consomme moins
// d'un Ko/s ; 100 trames pleines par seconde (~86 Ko/s) depasseraient le
// plafond : la file se remplit, les lignes en trop sont perdues et comptees.
static constexpr uint32_t kTxBytesPerS = 50000;
static constexpr uint32_t kTxBurst = 8192;
static constexpr uint32_t kExpireMs = 1000;
static constexpr uint32_t kOpenRetryMs = 1000;

struct TxItem {
  uint32_t at;
  bool errCounted;  // tx_erreurs compte une fois par datagramme
  uint8_t slot;     // session qui l'a scelle, kRawSlot : DEFI (sans session)
  uint16_t len;
  uint16_t port;
  uint8_t ip[4];    // IPv4 du destinataire, ordre du reseau
  uint8_t data[kTxMax];
};

static constexpr uint8_t kRawSlot = 0xFF;

static uint8_t sRx[kRxMax + 1];  // un octet de plus : un datagramme trop grand se reconnait
static TxItem sTx[kTxN];
static uint8_t sTxHead = 0, sTxN = 0;

static h1::Table sTable;
static int sSock = -1;
static uint32_t sOpenTryAt = 0, sExpireAt = 0;
static uint32_t sTxCredit = kTxBurst, sTxCreditAt = 0;

static struct {
  uint32_t rxTooBig, rxErr;  // perdus avant lecture : trop grands, erreurs de lwIP
  uint32_t rx, rejected, defis, tx, txLost, txErr;
} sSt = {};

// h1::Peer est en IPv6 (16 octets) : IPv4 mappee ::ffff:a.b.c.d (spec 8.5).
// local reste nul : une seule interface, lwIP choisit l'adresse source.
static h1::Peer peerOf(const sockaddr_in &a) {
  h1::Peer p;
  p.ip[10] = p.ip[11] = 0xFF;
  memcpy(p.ip + 12, &a.sin_addr.s_addr, 4);
  p.port = ntohs(a.sin_port);
  return p;
}

// ===========================================================================
//  File d'emission (tache loop)
// ===========================================================================

static TxItem *txTail() { return sTxN < kTxN ? &sTx[(sTxHead + sTxN) % kTxN] : nullptr; }

static void txPop() {
  if (!sTxN) return;
  sTxHead = (uint8_t)((sTxHead + 1) % kTxN);
  sTxN--;
}

static void txClear() {
  sSt.txLost += sTxN;
  sTxHead = sTxN = 0;
}

// Session partie (remplacee, oubliee) : ses datagrammes deja scelles ne
// prennent plus le debit de la suivante. Les autres gardent leur ordre.
static void txDropSlot(uint8_t slot) {
  uint8_t kept = 0;
  for (uint8_t i = 0; i < sTxN; i++) {
    const uint8_t from = (uint8_t)((sTxHead + i) % kTxN);
    if (sTx[from].slot == slot) {
      sSt.txLost++;
      continue;
    }
    const uint8_t to = (uint8_t)((sTxHead + kept) % kTxN);
    if (to != from) sTx[to] = sTx[from];
    kept++;
  }
  sTxN = kept;
}

// Datagramme sans session (DEFI) vers l'expediteur du SALUT : en tete de file,
// hors plafond de debit (82 octets, 2 par seconde au plus), pour qu'une
// poignee de main n'attende pas derriere l'instantane d'une autre session.
static void queueRaw(const h1::Peer &to, const uint8_t *d, size_t n, uint32_t now) {
  if (sTxN >= kTxN || n > kTxMax) {
    sSt.txLost++;
    return;
  }
  sTxHead = (uint8_t)((sTxHead + kTxN - 1) % kTxN);
  sTxN++;
  TxItem *t = &sTx[sTxHead];
  t->at = now;
  t->errCounted = false;
  t->slot = kRawSlot;
  t->len = (uint16_t)n;
  t->port = to.port;
  memcpy(t->ip, to.ip + 12, 4);
  memcpy(t->data, d, n);
}

bool netUdpSend(uint8_t slot, const uint8_t *json, size_t len) {
  if (slot >= h1::kSlots || len > jsonp::kLineMax - 2 || sSock < 0) return false;
  const h1::Session &s = sTable.slot(slot);
  TxItem *t = txTail();
  if (!s.used || !t) return false;
  char hdr[h1::kHeaderMax + 1];
  const size_t hn = sTable.seal(slot, json, len, hdr);
  if (!hn) return false;
  t->at = millis();
  t->errCounted = false;
  t->slot = slot;
  t->len = (uint16_t)(hn + len);
  t->port = s.peer.port;
  memcpy(t->ip, s.peer.ip + 12, 4);
  memcpy(t->data, hdr, hn);
  memcpy(t->data + hn, json, len);
  sTxN++;
  return true;
}

uint8_t netUdpFreeSlots() { return sSock >= 0 ? (uint8_t)(kTxN - sTxN) : 0; }

void netUdpEnd(uint8_t slot) { sTable.end(slot); }

void netUdpResume(uint8_t slot) { sTable.resume(slot); }

// Remet a lwIP les datagrammes en tete, sans jamais attendre.
static void txFlush() {
  // Heure lue ici, apres la mise en file : un datagramme date d'une
  // milliseconde plus tard que le debut de netUdpPoll() ne doit pas paraitre
  // vieux de 49 jours (comparaison signee).
  const uint32_t now = millis();
  while (sTxN && (int32_t)(now - sTx[sTxHead].at) > (int32_t)kTxStaleMs) {
    txPop();
    sSt.txLost++;
  }
  // Credit de debit (au plus 10 s rattrapees : pas de debordement du produit).
  const uint32_t elapsed = now - sTxCreditAt;
  sTxCreditAt = now;
  const uint32_t add = (elapsed > 10000 ? 10000 : elapsed) * kTxBytesPerS / 1000;
  sTxCredit = sTxCredit + add > kTxBurst ? kTxBurst : sTxCredit + add;
  if (!sTxN) return;
  if (sSock < 0) {
    txClear();
    return;
  }
  if (!netWifiUp()) return;  // Wi-Fi coupe : la file attend (kTxStaleMs au plus)
  for (uint8_t sent = 0; sTxN && sent < kTxPerTurn; sent++) {
    TxItem &t = sTx[sTxHead];
    if (t.slot != kRawSlot && sTxCredit < t.len) break;  // plafond : au tour suivant
    sockaddr_in to = {};
    to.sin_family = AF_INET;
    to.sin_port = htons(t.port);
    memcpy(&to.sin_addr.s_addr, t.ip, 4);
    if (lwip_sendto(sSock, t.data, t.len, 0, (const sockaddr *)&to, sizeof(to)) < 0) {
      if (!t.errCounted) sSt.txErr++;
      t.errCounted = true;
      // Tampons du Wi-Fi pleins : nouvel essai au tour suivant, jusqu'a kTxStaleMs.
      if (errno == ENOMEM || errno == EAGAIN || errno == EWOULDBLOCK) break;
      sSt.txLost++;  // refus durable (adresse, route) : perdu
      txPop();
      continue;
    }
    sSt.tx++;
    if (t.slot != kRawSlot) sTxCredit -= t.len;
    txPop();
  }
}

// ===========================================================================
//  Socket (tache loop)
// ===========================================================================

// Ouvert des qu'une cle existe et que le Wi-Fi a une adresse (la pile lwIP ne
// tourne qu'avec la station), puis tant que la cle existe (une coupure du
// Wi-Fi ne le ferme pas). Sans cle, le port reste ferme : lwIP repond "port
// injoignable".
static void syncSocket(uint32_t now) {
  const bool open = sSock >= 0;
  const bool want = sTable.hasKey() && (open || netWifiUp());
  if (want == open) return;
  if (!want) {
    lwip_close(sSock);
    sSock = -1;
    txClear();
    return;
  }
  if (sOpenTryAt && now - sOpenTryAt < kOpenRetryMs) return;
  sOpenTryAt = now ? now : 1;
  const int s = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0) return;
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(kPortUdp);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (lwip_bind(s, (const sockaddr *)&a, sizeof(a)) < 0) {
    lwip_close(s);
    return;
  }
  lwip_fcntl(s, F_SETFL, O_NONBLOCK);
  sSock = s;
}

// ===========================================================================
//  Datagrammes recus (tache loop)
// ===========================================================================

// Aleatoire de la plateforme pour nc et sid (tire seulement pour un SALUT
// admis) : vrai aleatoire, la radio Wi-Fi tourne.
static void fillRandom(void *p, size_t n) { esp_fill_random(p, n); }

// Chaque session partie (oubliee, remplacee, cle changee) : son etat JSON repart de zero.
static void resetOrigins(uint8_t mask) {
  for (uint8_t i = 0; i < h1::kSlots; i++)
    if (mask & (1u << i)) {
      txDropSlot(i);
      jsonRemoteReset((uint8_t)(i + 1));
    }
}

static void handle(const uint8_t *d, size_t len, const h1::Peer &from, uint32_t now) {
  const h1::Parsed p = h1::parse(d, len);

  if (p.kind == h1::Kind::Salut) {
    // Sans place pour le DEFI, la poignee de main en cours (celle d'un autre
    // client peut-etre) n'est pas remplacee.
    if (sTxN >= kTxN) {
      sSt.rejected++;  // pas encore authentifie : ne compte pas comme datagramme perdu
      return;
    }
    char defi[h1::kDefiLen + 1];
    if (sTable.onSalut(p, fillRandom, from, now, defi) != h1::Verdict::Ok) {
      sSt.rejected++;
      return;
    }
    sSt.defis++;
    queueRaw(from, (const uint8_t *)defi, h1::kDefiLen, now);
    return;
  }

  uint8_t slot = 0;
  bool fresh = false;
  if (sTable.onData(p, from, now, &slot, &fresh) != h1::Verdict::Ok) {
    sSt.rejected++;  // forme, sid inconnu, MAC faux, rejeu : silence
    return;
  }
  sSt.rx++;
  const uint8_t origin = (uint8_t)(slot + 1);
  if (fresh) resetOrigins((uint8_t)(1u << slot));  // session neuve, ou a la place d'une autre
  jsonNoteRemoteRx(origin);
  // La charge est une ligne de la CLI : 127 octets au plus (au-dela, refusee
  // avec son id, trop_long), ASCII imprimable (tout autre octet devient '?',
  // comme dans les chaines emises).
  char line[jsonp::kCmdMax + 1];
  const bool tooLong = p.payloadLen > jsonp::kCmdMax;
  const size_t n = tooLong ? jsonp::kCmdMax : p.payloadLen;
  for (size_t i = 0; i < n; i++) {
    const uint8_t c = p.payload[i];
    line[i] = c >= 0x20 && c <= 0x7E ? (char)c : '?';
  }
  line[n] = 0;
  cliRunRemote(origin, line, tooLong);
}

static void rxPoll(uint32_t now) {
  for (uint8_t i = 0; i < kRxPerTurn && sSock >= 0; i++) {
    sockaddr_in from = {};
    socklen_t fromLen = sizeof(from);
    const int n = lwip_recvfrom(sSock, sRx, sizeof(sRx), MSG_DONTWAIT, (sockaddr *)&from, &fromLen);
    if (n < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) sSt.rxErr++;
      return;  // rien de plus a lire ce tour-ci
    }
    if ((size_t)n > kRxMax) {
      sSt.rxTooBig++;
      continue;
    }
    if (from.sin_family != AF_INET) {
      sSt.rejected++;
      continue;
    }
    handle(sRx, (size_t)n, peerOf(from), now);
  }
}

// ===========================================================================
//  Cle (NVS hotte/cle)
// ===========================================================================

static void loadKey() {
  Preferences p;
  // Ouverture en ecriture meme pour lire (reglages.cpp) ; isKey() d'abord.
  if (!p.begin(kNvsEspace, false)) return;
  uint8_t k[h1::kKeyLen];
  const bool ok = p.isKey(kNvsKey) && p.getBytes(kNvsKey, k, sizeof(k)) == sizeof(k);
  p.end();
  if (ok) sTable.setKey(k);
  h1::wipe(k, sizeof(k));
}

// Toutes les sessions tombent (nouvelle cle, cle effacee) : leurs datagrammes aussi.
static void dropAll() {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < h1::kSlots; i++)
    if (sTable.slot(i).used) mask |= (uint8_t)(1u << i);
  resetOrigins(mask);
  txClear();
}

NetKeyResult netUdpKeyNew(const uint8_t appRandom[32], char keyHex[65], char kid[9]) {
  uint8_t card[32], key[h1::kKeyLen];
  esp_fill_random(card, sizeof(card));
  const h1::Part part = {card, sizeof(card)};
  char newKid[h1::kKidHex + 1];
  const bool made = h1::hmacSha256(appRandom, 32, &part, 1, key) && h1::keyId(key, newKid);
  h1::wipe(card, sizeof(card));
  if (!made) {
    h1::wipe(key, sizeof(key));
    return NetKeyResult::Crypto;
  }
  Preferences p;
  const bool written = p.begin(kNvsEspace, false) && p.putBytes(kNvsKey, key, sizeof(key)) == sizeof(key);
  p.end();
  if (!written) {  // NVS : ecriture atomique par entree, l'ancienne cle reste
    h1::wipe(key, sizeof(key));
    return NetKeyResult::Nvs;
  }
  dropAll();
  const bool loaded = sTable.setKey(key);
  h1::toHex(key, sizeof(key), keyHex);
  memcpy(kid, newKid, h1::kKidHex + 1);
  h1::wipe(key, sizeof(key));
  return loaded ? NetKeyResult::Ok : NetKeyResult::Load;
}

bool netUdpKeyErase() {
  Preferences p;
  bool ok = p.begin(kNvsEspace, false);
  if (ok && p.isKey(kNvsKey)) ok = p.remove(kNvsKey);
  p.end();
  // Meme si la NVS refuse : plus de cle en memoire, plus de transport jusqu'au redemarrage.
  dropAll();
  sTable.setKey(nullptr);
  return ok;
}

bool netUdpKid(char kid[9]) {
  if (!sTable.hasKey()) return false;
  memcpy(kid, sTable.kid(), h1::kKidHex + 1);
  return true;
}

// ===========================================================================
//  Cycle de vie
// ===========================================================================

void netUdpBegin() { loadKey(); }

void netUdpPoll() {
  const uint32_t now = millis();
  syncSocket(now);
  rxPoll(now);
  if (now - sExpireAt >= kExpireMs) {
    sExpireAt = now;
    resetOrigins(sTable.expire(now));
  }
  txFlush();
}

// ===========================================================================
//  JSON : bloc reseau ip
// ===========================================================================

void netUdpJson(jsonp::Writer &w, uint32_t /*now*/) {
  const bool up = netWifiUp();
  char ip[16];
  netWifiIp(ip);
  w.u32("frais_ms", 0);  // releve a la production de la ligne
  w.null("srp");         // pas de SRP : le nom se resout en mDNS (mdns.nom, cap mdns)
  w.arr("adresses");
  if (up) {
    w.obj(nullptr);
    w.str("adr", ip);
    w.str("type", "autre");
    w.boolean("pref", true);
    w.end();
  }
  w.end();
  w.obj("udp");
  w.u32("port", kPortUdp);
  w.boolean("ouvert", sSock >= 0);
  char kid[9];
  const bool hasKey = netUdpKid(kid);
  w.str("empreinte", hasKey ? kid : nullptr);  // l'app verifie qu'elle a la bonne cle
  w.u32("sessions", sTable.established());
  w.boolean("provisoire", sTable.provisional().used);
  w.u32("rx", sSt.rx);
  w.u32("rejets", sSt.rejected);
  w.u32("rx_perdus", sSt.rxTooBig + sSt.rxErr);
  w.u32("defis", sSt.defis);
  w.u32("tx", sSt.tx);
  w.u32("tx_perdus", sSt.txLost);
  w.u32("tx_erreurs", sSt.txErr);
  w.null("tampons_libres");  // tampons d'OpenThread : sans objet sur le Wi-Fi
  w.null("tampons_min");
  w.end();
  w.obj("mdns");
  char nom[40];
  snprintf(nom, sizeof(nom), "%s.local", kNomMdns);
  w.str("nom", nom);
  w.end();
  w.obj("wifi");
  w.boolean("connecte", up);
  if (up) w.i32("rssi_dbm", netWifiRssi());
  else w.null("rssi_dbm");
  w.str("ip", up ? ip : nullptr);
  w.u32("pertes", netWifiPertes());
  w.end();
}
```

- [ ] **Étape 11 : Compiler et relancer tous les tests**

Lancer : `~/.platformio/penv/bin/pio run -e sonde 2>&1 | grep -E "warning|error|RAM:|Flash:|SUCCESS|FAILED"`
Attendu (aucun `warning`) :

```text
RAM:   [===       ]  30.4% (used 99676 bytes from 327680 bytes)
Flash: [========  ]  81.5% (used 10681xx bytes from 1310720 bytes)
========================= [SUCCESS] Took ... seconds =========================
sonde          SUCCESS   ...
```

Lancer : `~/.platformio/penv/bin/pio run -e generateur 2>&1 | grep -E "warning|error|SUCCESS|FAILED"`
Attendu : `generateur     SUCCESS`, aucun `warning` (l'env n'a aucun des fichiers de cette tâche).

Lancer : `sh tools/tests/test_hote.sh | tail -1`
Attendu : « tests hote : OK »

Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « Ran 141 tests » puis « OK »

- [ ] **Étape 12 : Documenter le profil réseau (exemples vérifiés)**

`docs/PROTOCOLE-JSON.md` (dix remplacements ; l'exemple 8.1 gagne la ligne `reseau` que le firmware émet désormais, et les `caps` du transport) :

1. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
- Code : `src/json_out.*` (briques pures, testées sur l'hôte),
  `src/json_mode.*` (sessions, instantanés, événements), `src/cli.cpp`
  (lignes de l'hôte).
- Vérification : `tools/json_check.py` contrôle toute ligne machine contre ce
  profil (capture série brute, ou `.jsonl` avec `--jsonl`). Les exemples de la
  section 8 sont vérifiés par
  `python3 tools/json_check.py --strict --exemples docs/PROTOCOLE-JSON.md`,
  lancé par `sh tools/tests/test_hote.sh`.
- État : transport USB. Le transport réseau (UDP sur le Wi-Fi, ScreenBar §10)
  et l'injection viennent ensuite ; ce document s'étendra avec eux.
```

   par :

```markdown
- Code : `src/json_out.*` (briques pures, testées sur l'hôte),
  `src/json_mode.*` (sessions, instantanés, événements), `src/cli.cpp`
  (lignes de l'hôte), `src/net_wifi.*` et `src/net_udp_wifi.*` (transport
  réseau), `src/h1_proto.*` (enveloppe H1, copiée de la ScreenBar).
- Vérification : `tools/json_check.py` contrôle toute ligne machine contre ce
  profil (capture série brute, ou `.jsonl` avec `--jsonl`). Les exemples des
  sections 8 et 9 sont vérifiés par
  `python3 tools/json_check.py --strict --exemples docs/PROTOCOLE-JSON.md`,
  lancé par `sh tools/tests/test_hote.sh`.
- État : transports USB et UDP sur le Wi-Fi (section 9). L'injection vient
  ensuite ; ce document s'étendra avec elle.
```

2. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
| §5, §7, §8, §11, §12 : messages et exemples de la lampe | remplacés par les sections 4 à 8 |
```

   par :

```markdown
| §5, §7, §8, §11, §12 : messages et exemples de la lampe | remplacés par les sections 4 à 8 |
| §10 Transport réseau : UDP sur Thread, enveloppe H1, liste blanche, profil distant | UDP sur le **Wi-Fi**, en IPv4, nom mDNS ; enveloppe H1 **identique** ; liste blanche et profil de la sonde (section 9) |
```

3. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
| `json reseau <ms>` | période des `reseau` (aucun bloc tant que le transport réseau manque) | 0 ou 1000..60000, défaut **5 000** |
```

   par :

```markdown
| `json reseau <ms>` | période des `reseau` (bloc `ip`, section 4.6) | 0 ou 1000..60000, défaut **5 000** |
```

4. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
| `json cle ...` | clé du transport réseau | `refuse` tant que le transport réseau manque |
```

   par :

```markdown
| `json cle ...` | clé du transport réseau (section 9.3) | USB seulement ; `nouvelle` avec un `id` seulement |
```

5. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
Valeurs propres : `build` = `sonde`, `reseau_build` = `aucun`, `env` =
`sonde`, `session.transport` = `usb`. `fw` doit égaler `fw_desc`.
```

   par :

```markdown
Valeurs propres : `build` = `sonde`, `reseau_build` = `aucun` (pas de réseau
Matter ; le transport UDP est dit par `caps`), `env` = `sonde`,
`session.transport` = `usb` ou `udp`. `fw` doit égaler `fw_desc`.
```

6. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
| `caps` | `sonde` (capture de la ligne `D`), `injection` (présente dans ce build), `trames`, `log` ; puis `udp`, `cle`, `mdns` avec le transport réseau |
```

   par :

```markdown
| `caps` | `sonde` (capture de la ligne `D`), `injection` (présente dans ce build), `trames`, `log`, `udp` (transport UDP, section 9), `cle` (`json cle`), `mdns` (résoudre `reseau.ip.mdns.nom` au lieu de `srp.nom`) |
```

7. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
### 4.5 `hb` et `fin`

Identiques à la ScreenBar §5.6.
```

   par :

```markdown
### 4.5 `hb` et `fin`

Identiques à la ScreenBar §5.6.

### 4.6 `reseau`

Période `reseau_ms` (5 000 ms sur l'USB, 30 000 ms à distance). Un bloc,
`ip`, avec le **schéma de la ScreenBar** (§5.5, bloc `ip`), plus `mdns` et
`wifi`. Il part aussi sur l'USB : l'app y lit l'adresse, le nom et
l'empreinte de la clé avant d'ouvrir une session réseau.

| Champ | Sens |
|---|---|
| `frais_ms` | 0 : les valeurs sont lues à la production de la ligne |
| `srp` | toujours null : pas de SRP, le nom se résout en mDNS (`mdns.nom`) |
| `adresses` | l'adresse IPv4 de la station (`type` `autre`, `pref` vrai) ; vide sans Wi-Fi |
| `udp.port` | 5480 |
| `udp.ouvert` | socket ouvert : une clé existe, et le Wi-Fi a eu une adresse depuis le démarrage |
| `udp.empreinte` | empreinte de la clé (section 9.3), null sans clé |
| `udp.sessions`, `udp.provisoire` | sessions H1 établies (0..2), poignée de main en cours |
| `udp.rx`, `udp.rejets`, `udp.rx_perdus`, `udp.defis` | messages acceptés ; datagrammes refusés en silence (forme, clé, sid, MAC, rejeu, limite des `DEFI`) ; perdus avant lecture (plus de 256 octets, erreur de lwIP) ; `DEFI` émis |
| `udp.tx`, `udp.tx_perdus`, `udp.tx_erreurs` | datagrammes remis à lwIP ; perdus après leur mise en file (4 s sans départ, refus durable de lwIP, file vidée par un changement de clé, session partie) ; refus de lwIP (une fois par datagramme). Une ligne qui ne trouve pas de place dans la file compte dans le `json_perdus` de sa session et dans `compteurs.sonde.lignes_perdues` |
| `udp.tampons_libres`, `udp.tampons_min` | toujours null (tampons d'OpenThread : sans objet) |
| `mdns.nom` | `hotte-sonde.local` |
| `wifi.connecte` | station connectée, avec une adresse |
| `wifi.rssi_dbm`, `wifi.ip` | force du signal du point d'accès et adresse IPv4 ; null hors connexion |
| `wifi.pertes` | connexions perdues depuis le démarrage |

Pire cas : environ 560 octets.
```

8. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
<RS>{"v":1,"t":"hello","n":1,"ms":12032,"bloc":"identite","boot":"3FA2C901","mac":"F0F5BD012345","id":{"fabricant":"Djoko-CLI","produit":"Sonde hotte Haier","serie":"HOTTE-F0F5BD012345","nom":"Sonde hotte","hw":1,"hw_txt":"C6 SuperMini, etages v1"},"appareil":"hotte","caps":["sonde","injection","trames","log"]}
```

   par :

```markdown
<RS>{"v":1,"t":"hello","n":1,"ms":12032,"bloc":"identite","boot":"3FA2C901","mac":"F0F5BD012345","id":{"fabricant":"Djoko-CLI","produit":"Sonde hotte Haier","serie":"HOTTE-F0F5BD012345","nom":"Sonde hotte","hw":1,"hw_txt":"C6 SuperMini, etages v1"},"appareil":"hotte","caps":["sonde","injection","trames","log","udp","cle","mdns"]}
```

9. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
<RS>{"v":1,"t":"compteurs","n":6,"ms":12037,"bloc":"sonde","receptions":118,"parties":118,"blocs":118,"symboles":1947,"debord":0,"rep":0,"lignes_perdues":0,"sautes":0,"rejets":0}
<RS>{"v":1,"t":"reponse","n":7,"ms":12038,"id":1,"etape":"fin","cmd":"json 1","ok":true,"code":"ok","duree_ms":7,"bail_s":30,"up_s":12}
```

   par :

```markdown
<RS>{"v":1,"t":"compteurs","n":6,"ms":12037,"bloc":"sonde","receptions":118,"parties":118,"blocs":118,"symboles":1947,"debord":0,"rep":0,"lignes_perdues":0,"sautes":0,"rejets":0}
<RS>{"v":1,"t":"reseau","n":7,"ms":12038,"bloc":"ip","frais_ms":0,"srp":null,"adresses":[{"adr":"192.168.1.42","type":"autre","pref":true}],"udp":{"port":5480,"ouvert":true,"empreinte":"630DCD29","sessions":0,"provisoire":false,"rx":0,"rejets":0,"rx_perdus":0,"defis":0,"tx":0,"tx_perdus":0,"tx_erreurs":0,"tampons_libres":null,"tampons_min":null},"mdns":{"nom":"hotte-sonde.local"},"wifi":{"connecte":true,"rssi_dbm":-58,"ip":"192.168.1.42","pertes":0}}
<RS>{"v":1,"t":"reponse","n":8,"ms":12039,"id":1,"etape":"fin","cmd":"json 1","ok":true,"code":"ok","duree_ms":8,"bail_s":30,"up_s":12}
```

10. Dans `docs/PROTOCOLE-JSON.md`, remplacer :

```markdown
Puis `etat` et `compteurs` chaque seconde, et les `trame`.
```

   par :

```markdown
Puis `etat` et `compteurs` chaque seconde, `reseau` toutes les 5 s, et les
`trame`.
```

Puis ajouter à la fin de `docs/PROTOCOLE-JSON.md` (après la dernière ligne de l'exemple 8.6) :

````markdown

## 9. Transport réseau : UDP sur le Wi-Fi

Par différence avec la ScreenBar §10 (UDP sur Thread). Client de banc :
`tools/hotte_udp.py` (clé par l'USB, session, enregistrement).

### 9.1 Chemin et découverte

- La sonde est une **station Wi-Fi** (`src/net_wifi.*`). Les identifiants se
  posent par `wifi <ssid> <mdp>`, par l'USB seulement ; ils sont vérifiés avant
  d'être écrits dans l'espace NVS `hotte` (clés `wifi_ssid` et `wifi_mdp`). Le
  modem ne dort jamais (`WiFi.setSleep(false)`, spec §8.7). Tant que la
  connexion manque, nouvel essai toutes les 10 s. Sans identifiants, la radio
  reste éteinte.
- **IPv4** en v1. Le Mac joint la sonde par son nom mDNS,
  **`hotte-sonde.local`** (cap `mdns`, `reseau.ip.mdns.nom`), ou par son
  adresse (`reseau.ip.wifi.ip`, commande `info`). Pas de SRP : `srp` vaut null.
- Les annonces du Wi-Fi (connexion, perte) partent en `log`, `src` `reseau`,
  avec `json log 1` ; sinon en texte sur l'USB.

### 9.2 Datagrammes

- Port **5480**, socket UDP de lwIP (`src/net_udp_wifi.*`). Il s'ouvre dès
  qu'une clé existe et que le Wi-Fi a une adresse, puis reste ouvert tant que
  la clé existe. Sans clé, lwIP répond « port injoignable ».
- Un message = un datagramme : en-tête H1 puis l'objet JSON seul (sans RS ni
  LF), ou la ligne de commande de l'app. 1 078 octets au plus (56 + 1 022),
  sous la MTU de 1 500 du Wi-Fi. La sonde jette tout datagramme reçu de plus
  de 256 octets (`udp.rx_perdus`).
- Tout se passe dans la tâche `loop` : lecture non bloquante (4 datagrammes par
  tour au plus ; lwIP en garde 6 en attente), commandes, émission. `h1::Peer`
  porte l'adresse de l'app en IPv4 mappée (`::ffff:a.b.c.d`).
- Émission : file de **32 datagrammes**, partagée par les sessions, remis à
  lwIP sans attente, 4 par tour au plus.
  - Débit moyen plafonné à **50 000 octets/s**, crédit de **8 192**.
  - Pas partis en 4 s (Wi-Fi coupé) : perdus et comptés (`udp.tx_perdus`).
  - File pleine : la ligne est perdue et comptée (`json_perdus`,
    `lignes_perdues`), jamais attendue.
  - Le `DEFI` d'une poignée de main passe en tête, hors plafond.
- Un événement fréquent (`trame`, `log`) vers une session réseau n'est produit
  que s'il reste deux datagrammes libres : la place d'une ligne périodique ou
  d'une réponse après lui. Sinon, il est perdu (trou de `n`, `lignes_perdues`).
- Le reste est **identique** à la ScreenBar §10.2 :
  - `id` strictement croissants dans une session H1 ;
  - les 8 dernières `reponse` gardées par session ; un `id` renvoyé est servi
    depuis ce cache, sans nouvelle exécution ; un `id` déjà traité reçoit
    `deja_traite` ;
  - 6 s de retard admis dans la file des périodiques ; sessions servies à tour
    de rôle.

### 9.3 Authentification

**Enveloppe H1 identique** à la ScreenBar §10.4 : poignée de main `SALUT` /
`DEFI`, HMAC-SHA256, 2 sessions établies et une provisoire, fenêtre de 32,
2 `DEFI` par seconde au plus, oubli après 10 min, place libérée par `json 0`.
Code : `src/h1_proto.*` et `src/h1_crypto.cpp`, copiés tels quels.

Écarts :
- la clé est gardée en NVS dans l'espace **`hotte`**, clé **`cle`** (32
  octets) ;
- elle se crée par `id=<n> json cle nouvelle <64 hexa>`, **par l'USB
  seulement**. Sur le Mac, `python3 tools/hotte_udp.py cle <port>` le fait et
  range la clé dans `~/.config/hotte-sonde/cle` (droits 0600), sans jamais
  l'afficher : seule l'empreinte l'est ;
- ni trousseau, ni effacement à une remise à zéro Matter (pas de Matter).
  `json cle efface` coupe le transport.

Vecteurs : les mêmes que la ScreenBar §10.4, `tools/tests/test_h1.cpp` et
`tools/tests/test_hotte_udp.py`.

```
PSK  000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F
kid  630DCD29
na   A0A1A2A3A4A5A6A7A8A9AAABACADAEAF      nc  505152535455565758595A5B5C5D5E5F      sid 1234ABCD
SALUT  H1 SALUT 630DCD29 A0A1A2A3A4A5A6A7A8A9AAABACADAEAF 52D853E3FFE9E9CCEFFA98BB5304B32D
DEFI   H1 DEFI 1234ABCD 505152535455565758595A5B5C5D5E5F BFF13F71B42243E6017D2807F8E6171F
Ks   20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F
A 1  H1 1234ABCD 1 FD97A0C9E604524B49C763452D0310CE id=1 json 1
C 1  H1 1234ABCD 1 347A2E6A129BC822ECFF39BEC910451C {"v":1,"t":"hb","n":7,"ms":1234}
```

### 9.4 Commandes à distance : liste blanche

Toute ligne reçue par le réseau porte un `id` ; sans lui, elle est ignorée et
comptée dans `rejets`. Sont autorisées (`jsonp::remoteRefusal`,
`src/json_out.cpp`) :
- `json 1 [bail 10..120]`, `json 0`, `json etat`, `json hello`, `json ping` ;
- `json periode 2000..60000`, `json compteurs 0|1000..60000`,
  `json reseau 0|10000..60000`, `json trames 0|1`, `json log 0|1` ;
- `capture on|off|tout|changements`, `seuils <valeurs>` (bornés, sans texte) ;
- `injection on|off`, `injecte ...` (avec l'étage d'injection).

Tout le reste reçoit la `reponse` `interdite`, et rien n'est exécuté : en
particulier `json` seul, `json cle ...`, `wifi`, `injection monte`, `reboot`,
`bus`, `stats`, `info` et `help`.

### 9.5 Profil distant

Il s'écarte volontairement de la ScreenBar §10.6 (spec §8.5) : ici, le Wi-Fi a
de la marge, et les trames sont la raison d'être de la sonde.

| Réglage | USB | UDP (Wi-Fi) |
|---|---|---|
| `periode_ms` (`etat`) | 1 000 | 2 000 |
| `compteurs_ms` | 1 000 | 5 000 |
| `reseau_ms` | 5 000 | 30 000 |
| `trames` après `json 1` | oui | oui, sans coupure automatique |
| plafond des `trame` | 100 par seconde | 100 par seconde, puis `sautes` |
| file d'émission | tampon de 8 Ko | 32 datagrammes, 50 000 octets/s, crédit de 8 192 |

`hello.base.session.transport` vaut `udp`. Débit : moins d'un Ko/s au repos ;
motif `krona` du banc (`seuils 1 19000`, une quinzaine de lignes `trame` de
~250 octets par seconde) : ~4 Ko/s. Au plafond des trames (100 lignes pleines
par seconde, ~86 Ko/s), la file se remplit : les lignes en trop sont perdues et
comptées.

### 9.6 Exemples

Datagrammes, en texte (l'en-tête H1, puis la charge) :

```
app   -> sonde : H1 SALUT 630DCD29 A0A1A2A3A4A5A6A7A8A9AAABACADAEAF 52D853E3FFE9E9CCEFFA98BB5304B32D
sonde -> app   : H1 DEFI 1234ABCD 505152535455565758595A5B5C5D5E5F BFF13F71B42243E6017D2807F8E6171F
app   -> sonde : H1 1234ABCD 1 FD97A0C9E604524B49C763452D0310CE id=1 json 1
sonde -> app   : H1 1234ABCD 1 <mac> {"v":1,"t":"hello","n":0,...}
```

Objets portés par les datagrammes de la sonde, notés comme des lignes machine
pour être vérifiés : le `hello` d'une session réseau, un refus de la liste
blanche, un `id` déjà traité.

```
<RS>{"v":1,"t":"hello","n":0,"ms":80210,"bloc":"base","rev":4,"fw":"0.1.0-1a2b3c4","fw_desc":"0.1.0-1a2b3c4","date":"Sep 27 2026","heure":"14:02:11","env":"sonde","build":"sonde","reseau_build":"aucun","puce":"esp32c6","idf":"v5.5.5","arduino":"3.3.12","boot":"3FA2C901","reset":"mise_sous_tension","reset_n":1,"up_s":80,"session":{"transport":"udp","periode_ms":2000,"compteurs_ms":5000,"reseau_ms":30000,"bail_s":30,"trames":true,"log":false},"limites":{"ligne_max":1024,"cmd_max":127}}
<RS>{"v":1,"t":"reponse","n":41,"ms":95002,"id":6,"etape":"fin","cmd":"reboot","ok":false,"code":"interdite","msg":"interdite a distance (10.5) : USB seulement","duree_ms":0}
<RS>{"v":1,"t":"reponse","n":42,"ms":95510,"id":4,"etape":"fin","cmd":"json ping","ok":false,"code":"deja_traite","msg":"id deja traite (reponse plus disponible) : rien n'est reexecute","duree_ms":0}
```

Clé, par l'USB (`tools/hotte_udp.py cle` : l'aléa de l'app n'est jamais
renvoyé, la clé ne part qu'une fois) :

```
id=900417 json cle nouvelle 5A0C...(64 hexa)
```

```
<RS>{"v":1,"t":"reponse","n":12,"ms":5012,"id":900417,"etape":"fin","cmd":"json cle nouvelle","ok":true,"code":"ok","msg":"nouvelle cle : les sessions reseau tombent","duree_ms":3,"cle":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F","empreinte":"630DCD29"}
<RS>{"v":1,"t":"reponse","n":13,"ms":5230,"id":900418,"etape":"fin","cmd":"json cle","ok":true,"code":"ok","duree_ms":0,"empreinte":"630DCD29"}
```
````

- [ ] **Étape 13 : Relancer les vérifications**

Lancer : `python3 tools/json_check.py --strict -q --exemples docs/PROTOCOLE-JSON.md`
Attendu :

```text
30 ligne(s) machine, 0 erreur(s), 0 avertissement(s) ; 0 ligne(s) de texte, 0 fragment(s), 0 ligne(s) abimee(s) ; n : 0 trou(s) (0 ligne(s) perdue(s), 0.000 %), 0 recul(s)
plus longue ligne : 526 octets (docs/PROTOCOLE-JSON.md exemple 11)
par type : compteurs/sonde 1, config 2, etat/bus 1, etat/capture 1, etat/sys 1, fin 1, hb 1, hello/base 2, hello/identite 1, injection 1, log 2, reponse 11, reseau/ip 1, trame 4
```

Lancer : `sh tools/tests/test_hote.sh | tail -1` puis `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « tests hote : OK », puis « Ran 141 tests » et « OK »

- [ ] **Étape 14 : Commit**

```bash
git add src/net_udp_wifi.h src/net_udp_wifi.cpp src/json_mode.h src/json_mode.cpp src/cli.h src/cli.cpp src/main.cpp tools/json_check.py tools/tests/test_json_check.py docs/PROTOCOLE-JSON.md
git commit -m "Ajouter le transport UDP sur le Wi-Fi et le profil reseau du protocole" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 20 : `tools/hotte_udp.py` : clé, session et enregistrement par le réseau

**Fichiers :**
- Créer : `tools/hotte_udp.py` (adapté de `benq@c58a506:tools/halo_udp.py` : contenu complet ci-dessous)
- Créer : `tools/tests/test_hotte_udp.py`
- Tester : `python3 -m unittest discover -s tools/tests -p test_hotte_udp.py -v`, `python3 -m unittest discover -s tools/tests`

**Interfaces :**
- Consomme :
  - `tools/serie_enregistre.py` (tâche 12) : `ouvrir_port(chemin)`, `lecteur(fd)`, `ecrivain(fd)`, `PortFerme`, `Decoupe()` (`feed(octets)` → `[(genre, brut, objet)]`), `Bilan()` (`noter(obj)`, `resume()`, `abimees`), `nom_fichier(scenario, quand)`, `DOSSIER`.
  - `tools/json_check.py` (tâches 11, 19) : `jsonl_record(ligne)`, `main(argv)` (tests).
  - `tools/capture_fmt.py` (tâche 3) : `lire_jsonl(chemin)` (tests).
  - Firmware (tâche 19) : `id=<n> json cle nouvelle <64 hexa>` par l'USB (réponse avec `cle` et `empreinte`), port UDP 5480, enveloppe H1, liste blanche.
- Produit (`tools/hotte_udp.py`) :
  ```python
  PORT = 5480
  def chemin_cle() -> str                     # HOTTE_CLE, sinon ~/.config/hotte-sonde/cle
  def mac16(key, text) -> bytes
  def kid_of(key) -> str
  def texte_salut(key, kid, na) -> bytes
  def cle_session(key, na, nc, sid) -> bytes
  def lire_defi(key, kid, na, d) -> tuple[str, bytes] | None
  def sceller(ks, sens, sid, ctr, charge: bytes) -> bytes
  class Fenetre: accepter(ctr) -> bool
  def ouvrir_message(ks, sens, sid, d, fenetre) -> tuple[int, bytes] | None
  def demander_cle(lire, ecrire, alea, ident, delai_s=5.0, horloge=time.time) -> tuple[str, str, str | None]
  def ranger_cle(hex_key, chemin); def lire_cle(chemin) -> bytes; def cmd_cle(port, chemin)
  def resoudre(hote, port) -> tuple[int, tuple]
  PHRASE_INJECTION = "Majid devant la hotte"
  def confirmer(commandes, demander=input) -> bool   # False sans terminal sur l'entree standard
  class Client: ouvrir(), envoyer(ligne), tour() -> (charge, objet) | None, fermer(suite=None), stats, a_faire
  def resume(m) -> str
  def cmd_session(hote, port, commandes, duree, brut, key, afficher=print, demander=input) -> dict
  def enregistrement(rx_ms, de, charge: bytes) -> str   # ligne .jsonl du contrat
  def cmd_enregistre(hote, port, scenario, commandes, duree, dossier, key, afficher=print, demander=input) -> str
  def main(argv=None) -> int
  ```
  Ligne de commande : `cle <port>`, `session <hote> [commande ...] [--duree s] [--brut] [--port p]`, `enregistre <hote> <scenario> [commande ...] [--duree s] [--port p] [--dossier d]`.

**Décisions :**
- **Clé** dans `~/.config/hotte-sonde/cle` (0600, dossier 0700, écriture atomique), jamais affichée ; `HOTTE_CLE=<fichier>` en choisit un autre. Ni trousseau ni sous-commande `refus` (propres à Thread et à l'app de la ScreenBar).
- **Résolution** : `getaddrinfo(hote, port, AF_UNSPEC, SOCK_DGRAM)`, premier résultat, socket de sa famille (la sonde n'annonce que de l'IPv4, mais le client ne l'impose pas).
- **`injection on`** (règle 11 : toute émission vers la hotte se fait en présence de Majid, à portée de la fiche, le panneau en vue) : Majid tape lui-même la phrase exacte `Majid devant la hotte`, pour chaque `injection on` de la liste, **avant l'ouverture de la session** ; la question rappelle la règle 11. Un autre texte (`oui` compris) : rien n'est envoyé. **Seulement depuis un terminal** : sans terminal sur l'entrée standard (`sys.stdin.isatty()` faux : script, tube, agent), refus sans question, même avec la phrase ; elle n'est jamais fournie par un agent. Une question en pleine session laisserait passer le bail et les `json ping`.
- **`enregistre`** accepte des commandes, comme `serie_enregistre.py` (la spec ne montrait que `--duree`) : `"capture tout" "seuils 1 19000"` pour le critère 4. Le `.jsonl` suit le contrat, avec `"de"` = l'adresse de la sonde ; seules les lignes au MAC juste et au JSON valide y entrent. Le résumé de `logs/live.log` commence par `udp <adresse> <scenario>`.
- Tests : une vraie session UDP contre une sonde simulée sur 127.0.0.1 (poignée de main, réponses, un datagramme au MAC faux, `json 0`), environ 6 s. L'entrée standard est simulée (`Entree`, `mock.patch.object(sys, "stdin", ...)`) : terminal ou non, quel que soit le lanceur des tests.

- [ ] **Étape 1 : Écrire le test qui échoue**

Créer `tools/tests/test_hotte_udp.py` :

```python
#!/usr/bin/env python3
"""Tests de tools/hotte_udp.py sans sonde : vecteurs H1 (docs/PROTOCOLE-JSON.md
9.3, les memes que tools/tests/test_h1.cpp), fenetre contre le rejeu, fichier
de cle, cle par un USB simule, format .jsonl, confirmation de 'injection on'
(phrase exacte, dans un terminal seulement), et une vraie session UDP contre une
sonde simulee sur 127.0.0.1 :
python3 -m unittest discover -s tools/tests -p test_hotte_udp.py -v"""
import contextlib
import io
import json
import os
import socket
import stat
import sys
import tempfile
import threading
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import capture_fmt  # noqa: E402
import hotte_udp  # noqa: E402
import json_check  # noqa: E402

RS = b"\x1e"

# Vecteurs de la ScreenBar (10.4), repris en 9.3 du profil hotte.
PSK = bytes.fromhex("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F")
KID = "630DCD29"
NA = "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
NC = "505152535455565758595A5B5C5D5E5F"
SID = "1234ABCD"
SALUT = b"H1 SALUT 630DCD29 A0A1A2A3A4A5A6A7A8A9AAABACADAEAF 52D853E3FFE9E9CCEFFA98BB5304B32D"
DEFI = b"H1 DEFI 1234ABCD 505152535455565758595A5B5C5D5E5F BFF13F71B42243E6017D2807F8E6171F"
KS = bytes.fromhex("20D6D83D97ED44F2BBF8CE56389BD475CBE2B625CE6CE24768B6B4C1C625012F")
MSG_A = b"H1 1234ABCD 1 FD97A0C9E604524B49C763452D0310CE id=1 json 1"
JSON_C = b'{"v":1,"t":"hb","n":7,"ms":1234}'
HDR_C = b"H1 1234ABCD 1 347A2E6A129BC822ECFF39BEC910451C "
MAC_A_MAX = "62CA08CFED5A5FE89EB9AAE8CC3D9CB7"  # A, ctr 4294967295, charge "x"


def compact(obj):
    return json.dumps(obj, separators=(",", ":")).encode("ascii")


class Entree(io.StringIO):
    """Entree standard simulee : un terminal (Majid au clavier) ou non (script, tube, agent)."""

    def __init__(self, tty):
        super().__init__()
        self.tty = tty

    def isatty(self):
        return self.tty


def terminal(tty):
    return mock.patch.object(sys, "stdin", Entree(tty))


class Vecteurs(unittest.TestCase):
    def test_poignee_de_main(self):
        self.assertEqual(hotte_udp.kid_of(PSK), KID)
        self.assertEqual(hotte_udp.texte_salut(PSK, KID, NA), SALUT)
        self.assertEqual(hotte_udp.lire_defi(PSK, KID, NA, DEFI), (SID, KS))
        self.assertEqual(hotte_udp.cle_session(PSK, NA, NC, SID), KS)
        # DEFI d'un autre na, ou abime : ignore.
        self.assertIsNone(hotte_udp.lire_defi(PSK, KID, "B" * 32, DEFI))
        self.assertIsNone(hotte_udp.lire_defi(PSK, KID, NA, DEFI[:-1] + b"0"))
        self.assertIsNone(hotte_udp.lire_defi(PSK, KID, NA, b"H1 1234ABCD 1 " + b"0" * 32 + b" {}"))

    def test_messages(self):
        self.assertEqual(hotte_udp.sceller(KS, "A", SID, 1, b"id=1 json 1"), MSG_A)
        self.assertEqual(hotte_udp.sceller(KS, "C", SID, 1, JSON_C), HDR_C + JSON_C)
        self.assertEqual(hotte_udp.sceller(KS, "A", SID, 4294967295, b"x")[:-2].split(b" ")[3].decode(), MAC_A_MAX)
        f = hotte_udp.Fenetre()
        self.assertEqual(hotte_udp.ouvrir_message(KS, "C", SID, HDR_C + JSON_C, f), (1, JSON_C))
        self.assertIsNone(hotte_udp.ouvrir_message(KS, "C", SID, HDR_C + JSON_C, f))           # rejeu
        f = hotte_udp.Fenetre()
        self.assertIsNone(hotte_udp.ouvrir_message(KS, "A", SID, HDR_C + JSON_C, f))           # autre sens
        self.assertIsNone(hotte_udp.ouvrir_message(KS, "C", "DEADBEEF", HDR_C + JSON_C, f))    # autre session
        self.assertIsNone(hotte_udp.ouvrir_message(KS, "C", SID, HDR_C + JSON_C[:-1] + b" }", f))  # charge modifiee
        self.assertEqual(hotte_udp.ouvrir_message(KS, "C", SID, HDR_C + JSON_C, f), (1, JSON_C))  # fenetre intacte

    def test_fenetre(self):
        # Memes cas que testWindow de test_h1.cpp (fenetre de 32).
        f = hotte_udp.Fenetre()
        self.assertFalse(f.accepter(0))
        self.assertTrue(f.accepter(1))
        self.assertFalse(f.accepter(1))
        self.assertTrue(f.accepter(40))
        self.assertFalse(f.accepter(8))
        self.assertTrue(f.accepter(9))
        self.assertFalse(f.accepter(9))
        self.assertTrue(f.accepter(41))
        self.assertTrue(f.accepter(100))
        self.assertFalse(f.accepter(68))
        self.assertTrue(f.accepter(69))


class Cle(unittest.TestCase):
    def setUp(self):
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)
        self.chemin = os.path.join(self.dossier.name, "config", "hotte-sonde", "cle")

    def test_fichier(self):
        hotte_udp.ranger_cle(PSK.hex().upper(), self.chemin)
        self.assertEqual(stat.S_IMODE(os.stat(self.chemin).st_mode), 0o600)
        self.assertEqual(stat.S_IMODE(os.stat(os.path.dirname(self.chemin)).st_mode), 0o700)
        self.assertEqual(hotte_udp.lire_cle(self.chemin), PSK)
        with open(self.chemin, "w") as f:
            f.write("pas une cle\n")
        with self.assertRaises(SystemExit):
            hotte_udp.lire_cle(self.chemin)
        with self.assertRaises(SystemExit):
            hotte_udp.lire_cle(os.path.join(self.dossier.name, "absente"))

    def usb(self, reponse):
        """Sonde simulee sur l'USB : repond a 'id=<n> json cle nouvelle <alea>' par reponse(id, alea)."""
        ecrit, attente = [], [b"reste d'une ligne\n> "]

        def ecrire(octets):
            ecrit.append(octets)
            for ligne in octets.split(b"\n")[:-1]:
                if ligne.startswith(b"id="):
                    ident, _, _, _, alea = ligne.decode().split(" ")
                    attente.append(RS + compact(reponse(int(ident[3:]), alea)) + b"\r\n")

        def lire(_delai):
            return attente.pop(0) if attente else b""

        return lire, ecrire, ecrit

    def test_demander_cle(self):
        cle = "11" * 32

        def ok(ident, alea):
            self.assertRegex(alea, r"^[0-9A-F]{64}$")
            return {"v": 1, "t": "reponse", "n": 3, "ms": 9, "id": ident, "etape": "fin", "cmd": "json cle nouvelle",
                    "ok": True, "code": "ok", "msg": "nouvelle cle : les sessions reseau tombent", "duree_ms": 2,
                    "cle": cle.upper(), "empreinte": hotte_udp.kid_of(bytes.fromhex(cle))}

        lire, ecrire, ecrit = self.usb(ok)
        alea = "AB" * 32
        r = hotte_udp.demander_cle(lire, ecrire, alea, 900123)
        self.assertEqual(r, (cle.upper(), hotte_udp.kid_of(bytes.fromhex(cle)), "nouvelle cle : les sessions reseau tombent"))
        self.assertEqual(ecrit[0], b"\x15\n")
        self.assertEqual(ecrit[1], f"id=900123 json cle nouvelle {alea}\n".encode())

        def fausse_empreinte(ident, alea):
            m = ok(ident, alea)
            m["empreinte"] = "00000000"
            return m

        lire, ecrire, _ = self.usb(fausse_empreinte)
        with self.assertRaises(SystemExit) as e:
            hotte_udp.demander_cle(lire, ecrire, alea, 900124)
        self.assertIn("empreinte incoherente", str(e.exception))

        def refus(ident, _alea):
            return {"v": 1, "t": "reponse", "n": 3, "ms": 9, "id": ident, "etape": "fin", "cmd": "json cle nouvelle",
                    "ok": False, "code": "refuse", "msg": "tampon USB plein : rien n'est change, reessayer",
                    "duree_ms": 0}

        lire, ecrire, _ = self.usb(refus)
        with self.assertRaises(SystemExit) as e:
            hotte_udp.demander_cle(lire, ecrire, alea, 900125)
        self.assertIn("refuse : refuse tampon USB plein", str(e.exception))

    def test_port_absent(self):
        with self.assertRaises(SystemExit) as e:
            hotte_udp.cmd_cle("/dev/cu.inexistant-hotte", self.chemin)
        self.assertIn("/dev/cu.inexistant-hotte", str(e.exception))
        self.assertFalse(os.path.exists(self.chemin))


class Formats(unittest.TestCase):
    def test_jsonl(self):
        charge = b'{"v":1,"t":"hb","n":7,"ms":1234,"boot":"3FA2C901","up_s":1,"json_perdus":0}'
        ligne = hotte_udp.enregistrement(1790000000123, "192.168.1.42", charge)
        self.assertEqual(ligne, '{"rx_ms":1790000000123,"de":"192.168.1.42","l":' + charge.decode() + "}\n")
        de, brut = json_check.jsonl_record(ligne.encode().rstrip(b"\n"))
        self.assertEqual((de, brut), ("192.168.1.42", RS + charge))
        with tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False) as f:
            f.write(ligne)
        self.addCleanup(os.unlink, f.name)
        self.assertEqual(list(capture_fmt.lire_jsonl(f.name)), [json.loads(charge)])

    def test_confirmation(self):
        questions = []

        def repond(texte):
            def demander(q):
                questions.append(q)
                return texte
            return demander

        def ferme(_q):
            raise EOFError

        with terminal(True):
            self.assertTrue(hotte_udp.confirmer(["capture tout", "json etat"], repond("non")))
            self.assertEqual(questions, [])  # rien a confirmer : aucune question
            # La phrase exacte, tapee par Majid ; la question rappelle la regle 11.
            self.assertTrue(hotte_udp.confirmer(["injection  on"], repond("Majid devant la hotte")))
            for mot in ("Regle 11", "a portee de la fiche", "le panneau en vue", "Majid devant la hotte"):
                self.assertIn(mot, questions[0])
            self.assertFalse(hotte_udp.confirmer(["injection on"], repond("oui")))
            self.assertFalse(hotte_udp.confirmer(["injection on"], repond("majid devant la hotte")))
            self.assertFalse(hotte_udp.confirmer(["injection on"], repond("Majid devant la hotte ")))
            self.assertFalse(hotte_udp.confirmer(["injection on"], ferme))
        # Sans terminal (script, tube, agent) : refus, meme avec la phrase, et sans question.
        del questions[:]
        with terminal(False):
            self.assertFalse(hotte_udp.confirmer(["injection on"], repond("Majid devant la hotte")))
            self.assertTrue(hotte_udp.confirmer(["capture tout"], repond("")))
        self.assertEqual(questions, [])

    def test_resolution(self):
        self.assertEqual(hotte_udp.resoudre("127.0.0.1", 5480)[0], socket.AF_INET)
        self.assertEqual(hotte_udp.resoudre("::1", 5480)[0], socket.AF_INET6)
        with self.assertRaises(SystemExit):
            hotte_udp.resoudre("nom-qui-n-existe-pas.invalid", 5480)


class SondeSimulee(threading.Thread):
    """Joue la sonde sur 127.0.0.1 : poignee de main H1, une reponse par
    commande, quelques lignes du profil hotte, et un datagramme au MAC faux."""

    def __init__(self, cle):
        super().__init__(daemon=True)
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(("127.0.0.1", 0))
        self.s.settimeout(0.05)
        self.port = self.s.getsockname()[1]
        self.cle, self.kid = cle, hotte_udp.kid_of(cle)
        self.ks, self.ctr, self.n = None, 0, 0
        self.fenetre = hotte_udp.Fenetre()
        self.recues = []
        self.arret = threading.Event()

    def ligne(self, t, **champs):
        obj = {"v": 1, "t": t, "n": self.n, "ms": 5000 + self.n}
        obj.update(champs)
        self.n += 1
        return compact(obj)

    def envoyer(self, adr, charge):
        self.ctr += 1
        self.s.sendto(hotte_udp.sceller(self.ks, "C", SID, self.ctr, charge), adr)

    def reponse(self, ident, cmd, **champs):
        return self.ligne("reponse", id=ident, etape="fin", cmd=cmd, ok=True, code="ok", duree_ms=1, **champs)

    def trame(self):
        return self.ligne("trame", num=self.n, part=0, fin=True, t_us=1000000 + self.n, niv0="bas",
                          dur_us=[2000, 4000, 2000], debord=False)

    def run(self):
        while not self.arret.is_set():
            try:
                d, adr = self.s.recvfrom(2048)
            except socket.timeout:
                continue
            p = d.split(b" ")
            if len(p) == 5 and p[1] == b"SALUT":
                na = p[3].decode()
                if hotte_udp.texte_salut(self.cle, self.kid, na) != d:
                    continue
                mac = hotte_udp.mac16(self.cle, f"H1|DEFI|{self.kid}|{na}|{NC}|{SID}").hex().upper()
                self.s.sendto(f"H1 DEFI {SID} {NC} {mac}".encode(), adr)
                self.ks, self.ctr, self.fenetre = hotte_udp.cle_session(self.cle, na, NC, SID), 0, hotte_udp.Fenetre()
                continue
            r = hotte_udp.ouvrir_message(self.ks, "A", SID, d, self.fenetre) if self.ks else None
            if r is None:
                continue
            ident, cmd = r[1].decode()[3:].split(" ", 1)
            ident = int(ident)
            self.recues.append(cmd)
            if cmd == "json 1":
                self.envoyer(adr, self.ligne("hb", boot="3FA2C901", up_s=5, json_perdus=0))
                self.envoyer(adr, self.trame())
                self.envoyer(adr, self.reponse(ident, cmd, bail_s=30, up_s=5))
                self.s.sendto(b"H1 1234ABCD 99 " + b"0" * 32 + b" {}", adr)  # MAC faux : rejete
            elif cmd == "json 0":
                self.envoyer(adr, self.reponse(ident, cmd))
                self.envoyer(adr, self.ligne("fin", cause="commande"))
            else:
                self.envoyer(adr, self.reponse(ident, cmd))
                self.envoyer(adr, self.trame())


class SessionUdp(unittest.TestCase):
    def setUp(self):
        self.sonde = SondeSimulee(PSK)
        self.sonde.start()
        self.addCleanup(self.sonde.s.close)
        self.addCleanup(self.sonde.arret.set)
        self.dossier = tempfile.TemporaryDirectory()
        self.addCleanup(self.dossier.cleanup)

    def test_session(self):
        sortie = []
        stats = hotte_udp.cmd_session("127.0.0.1", self.sonde.port, ["capture tout"], 2.0, False, PSK, sortie.append)
        texte = "\n".join(sortie)
        self.assertIn(f"sonde 127.0.0.1 port {self.sonde.port}, cle {KID}", texte)
        self.assertIn("session 1234ABCD ouverte", texte)
        self.assertIn(">> id=1 json 1", texte)
        self.assertIn("n=0 hb", texte)
        self.assertIn("n=2 reponse id=1 fin ok « json 1 »", texte)
        self.assertIn(">> id=2 capture tout", texte)
        self.assertEqual(self.sonde.recues, ["json 1", "capture tout", "json 0"])
        self.assertEqual(stats["rejetees"], 1)
        self.assertEqual(stats["sessions"], 1)

    def test_enregistre(self):
        sortie = []
        chemin = hotte_udp.cmd_enregistre("127.0.0.1", self.sonde.port, "essai udp", ["capture tout"], 2.0,
                                          self.dossier.name, PSK, sortie.append)
        self.assertRegex(os.path.basename(chemin), r"^\d{4}-\d{2}-\d{2}-\d{4}-essai-udp\.jsonl$")
        with open(chemin, encoding="ascii") as f:
            recs = [json.loads(l) for l in f]
        self.assertEqual({r["de"] for r in recs}, {"127.0.0.1"})
        self.assertEqual([r["l"]["t"] for r in recs], ["hb", "trame", "reponse", "reponse", "trame", "reponse", "fin"])
        self.assertTrue(all(isinstance(r["rx_ms"], int) for r in recs))
        with contextlib.redirect_stdout(io.StringIO()) as out:
            code = json_check.main(["--strict", "--jsonl", chemin])
        self.assertEqual(code, 0, out.getvalue())
        self.assertIn("7 ligne(s) machine, 0 erreur(s)", out.getvalue())
        with open(os.path.join(self.dossier.name, "live.log"), encoding="utf-8") as f:
            journal = f.read()
        self.assertRegex(journal, r"udp 127\.0\.0\.1 essai udp fin 2 s : 7 lignes .*1 rejetee")

    def test_injection_confirmee(self):
        # 'oui' au clavier, ou la phrase sans terminal (script, agent) : rien n'est envoye.
        for tty, reponse in ((True, "oui"), (False, "Majid devant la hotte")):
            with terminal(tty), self.assertRaises(SystemExit) as e:
                hotte_udp.cmd_session("127.0.0.1", self.sonde.port, ["injection on"], 1.0, False, PSK,
                                      lambda *_: None, demander=lambda _q, r=reponse: r)
            self.assertIn("rien n'est envoye", str(e.exception))
        self.assertEqual(self.sonde.recues, [])
        # La phrase exacte, dans un terminal : la commande part apres 'json 1'.
        with terminal(True):
            hotte_udp.cmd_session("127.0.0.1", self.sonde.port, ["injection on"], 2.0, False, PSK, lambda *_: None,
                                  demander=lambda _q: "Majid devant la hotte")
        self.assertEqual(self.sonde.recues, ["json 1", "injection on", "json 0"])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Étape 2 : Lancer le test et vérifier qu'il échoue**

Lancer : `python3 -m unittest discover -s tools/tests -p test_hotte_udp.py -v`
Attendu : ÉCHEC avec « ModuleNotFoundError: No module named 'hotte_udp' »

- [ ] **Étape 3 : Écrire l'implémentation minimale**

Créer `tools/hotte_udp.py` :

```python
#!/usr/bin/env python3
# Copie de benq-screenbar-halo-matter@c58a506 : tools/halo_udp.py (adapte : cle dans ~/.config/hotte-sonde/cle, getaddrinfo AF_UNSPEC, sous-commande enregistre, confirmation tapee de 'injection on', resume du profil hotte ; sans trousseau ni 'refus')
"""Client de banc du transport reseau de la sonde (docs/PROTOCOLE-JSON.md, section 9).

Session H1 (UDP sur le Wi-Fi, port 5480) avec la cle posee par l'USB : suivre
les lignes de la sonde, lui envoyer des commandes de la liste blanche, ou
enregistrer une capture.

  cle <port serie>
      Nouvelle cle partagee : 'json cle nouvelle <alea>' par l'USB (ouverture
      sure du C6 : DTR = RTS = 0 en un seul appel). La cle est rangee dans
      ~/.config/hotte-sonde/cle (droits 0600 ; HOTTE_CLE=<fichier> pour un
      autre), jamais affichee : seule son empreinte l'est. Toutes les sessions
      reseau tombent. Le port doit etre libre.

  session <hote> [commande ...] [--duree s] [--brut] [--port p]
      Poignee de main (SALUT signe, DEFI verifie), 'json 1', puis chaque
      commande donnee (entre guillemets, sans id=), 'json ping' toutes les
      10 s. Une commande sans reponse en 2 s est renvoyee avec le meme id (2
      fois au plus : la sonde renvoie sa reponse sans reexecuter). Sonde muette
      8 s : nouvelle poignee de main. Un resume de chaque ligne (--brut : le
      JSON tel quel). Fin : 'json 0'.

  enregistre <hote> <scenario> [commande ...] [--duree s] [--port p] [--dossier d]
      Meme session ; chaque ligne acceptee est ecrite dans
      <dossier>/AAAA-MM-JJ-hhmm-<scenario>.jsonl :
        {"rx_ms": <heure du Mac en ms>, "de": "<ip>", "l": <objet recu tel quel>}
      et un resume part dans <dossier>/live.log toutes les 5 s et a la fin
      ('tail -f logs/live.log'). Fin : --duree ecoulee, ou Ctrl-C.

Hote : hotte-sonde.local (mDNS), ou l'adresse de la sonde (commande 'info').
'injection on' (regle 11 : Majid devant la hotte, a portee de la fiche, le
panneau en vue) ne part qu'apres une confirmation que Majid tape lui-meme,
dans un terminal, avant l'ouverture de la session : la phrase exacte
'Majid devant la hotte'. Sans terminal (script, tube, agent), ou sans la
phrase exacte, rien n'est envoye.

Exemples :
  python3 tools/hotte_udp.py cle /dev/cu.usbmodem101
  python3 tools/hotte_udp.py session hotte-sonde.local --duree 30
  python3 tools/hotte_udp.py enregistre hotte-sonde.local krona-wifi "capture tout" "seuils 1 19000" --duree 660
"""
import argparse
import datetime
import hashlib
import hmac
import json
import os
import socket
import sys
import tempfile
import time

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import serie_enregistre  # noqa: E402  (ouverture sure du port, decoupage, bilan)

PORT = 5480
RESUME_S = 5.0
PHRASE_INJECTION = "Majid devant la hotte"  # confirmation de 'injection on' (regle 11)
DOSSIER = serie_enregistre.DOSSIER


def chemin_cle():
    """Fichier de la cle : HOTTE_CLE, sinon ~/.config/hotte-sonde/cle."""
    return os.path.abspath(os.environ.get("HOTTE_CLE") or os.path.expanduser("~/.config/hotte-sonde/cle"))


# ---------------------------------------------------------------------------
#  Enveloppe H1 (ScreenBar 10.4 ; docs/PROTOCOLE-JSON.md 9.3)
# ---------------------------------------------------------------------------


def mac16(key, text):
    return hmac.new(key, text.encode() if isinstance(text, str) else text, hashlib.sha256).digest()[:16]


def kid_of(key):
    return hashlib.sha256(key).hexdigest().upper()[:8]


def texte_salut(key, kid, na):
    """'H1 SALUT <kid> <na> <mac_salut>' (na : 32 hexa majuscules)."""
    mac = mac16(key, f"H1|SALUT|{kid}|{na}").hex().upper()
    return f"H1 SALUT {kid} {na} {mac}".encode()


def cle_session(key, na, nc, sid):
    return hmac.new(key, f"H1|SESSION|{na}|{nc}|{sid}".encode(), hashlib.sha256).digest()


def lire_defi(key, kid, na, d):
    """(sid, Ks) si d est le DEFI au MAC juste pour ce na ; None sinon."""
    p = d.split(b" ")
    if len(p) != 5 or p[0] != b"H1" or p[1] != b"DEFI":
        return None
    sid, nc = p[2].decode("ascii", "replace"), p[3].decode("ascii", "replace")
    want = mac16(key, f"H1|DEFI|{kid}|{na}|{nc}|{sid}").hex().upper().encode()
    if not hmac.compare_digest(want, p[4]):
        return None
    return sid, cle_session(key, na, nc, sid)


def sceller(ks, sens, sid, ctr, charge):
    """'H1 <sid> <ctr> <mac> <charge>' ; sens 'A' (app -> sonde) ou 'C' (sonde -> app)."""
    mac = mac16(ks, f"{sens}|{sid}|{ctr}|".encode() + charge).hex().upper()
    return f"H1 {sid} {ctr} {mac} ".encode() + charge


class Fenetre:
    """ctr deja vus : fenetre glissante de 32 (h1::Window)."""

    def __init__(self):
        self.top, self.bits = 0, 0

    def accepter(self, ctr):
        if ctr == 0:
            return False
        if ctr > self.top:
            shift = ctr - self.top
            self.bits = 0 if shift >= 32 else (self.bits << shift) & 0xFFFFFFFF
            self.bits |= 1
            self.top = ctr
            return True
        back = self.top - ctr
        if back >= 32 or self.bits & (1 << back):
            return False
        self.bits |= 1 << back
        return True


def ouvrir_message(ks, sens, sid, d, fenetre):
    """(ctr, charge) si d est un message de la session sid, sens donne, MAC juste
    et ctr neuf (la fenetre n'avance qu'apres le MAC) ; None sinon."""
    p = d.split(b" ", 4)
    if len(p) != 5 or p[0] != b"H1" or p[1] != sid.encode() or not p[2].isdigit() or p[2].startswith(b"0"):
        return None
    want = mac16(ks, sens.encode() + b"|" + p[1] + b"|" + p[2] + b"|" + p[4]).hex().upper().encode()
    if not hmac.compare_digest(want, p[3]):
        return None
    ctr = int(p[2])
    if ctr > 0xFFFFFFFF or not fenetre.accepter(ctr):
        return None
    return ctr, p[4]


# ---------------------------------------------------------------------------
#  Cle par l'USB
# ---------------------------------------------------------------------------


def demander_cle(lire, ecrire, alea, ident, delai_s=5.0, horloge=time.time):
    """'id=<ident> json cle nouvelle <alea>' sur un port deja ouvert ; rend
    (cle en 64 hexa, empreinte, msg). SystemExit si refusee ou sans reponse."""
    decoupe = serie_enregistre.Decoupe()
    ecrire(b"\x15\n")  # Ctrl-U : efface un reste de ligne
    decoupe.feed(lire(0.2))
    ecrire(f"id={ident} json cle nouvelle {alea}\n".encode("ascii"))
    limite = horloge() + delai_s
    while horloge() < limite:
        for genre, _, m in decoupe.feed(lire(0.2)):
            if genre != "machine" or m.get("t") != "reponse" or m.get("id") != ident or m.get("etape") != "fin":
                continue
            if not m.get("ok") or "cle" not in m:
                raise SystemExit(f"refuse : {m.get('code')} {m.get('msg', '')}")
            try:
                cle = bytes.fromhex(m["cle"])
            except (TypeError, ValueError):
                cle = b""
            if len(cle) != 32 or kid_of(cle) != m.get("empreinte"):
                raise SystemExit("empreinte incoherente : cle non rangee")
            return m["cle"], m["empreinte"], m.get("msg")
    raise SystemExit("aucune reponse en 5 s (firmware sans transport reseau ? sonde occupee ?). Si la sonde a "
                     "quand meme change de cle, 'json cle' par l'USB montre une autre empreinte : relancer 'cle'.")


def ranger_cle(hex_key, chemin):
    """Ecriture atomique, 0600 (dossier 0700), sans suivre de lien symbolique."""
    d = os.path.dirname(chemin)
    os.makedirs(d, mode=0o700, exist_ok=True)
    fdk, tmp = tempfile.mkstemp(prefix=".cle.", dir=d)  # 0600, nom unique, jamais un lien
    try:
        os.fchmod(fdk, 0o600)
        os.write(fdk, (hex_key + "\n").encode())
        os.fsync(fdk)
    finally:
        os.close(fdk)
    try:
        os.replace(tmp, chemin)
    except OSError:
        os.unlink(tmp)
        raise


def lire_cle(chemin):
    try:
        with open(chemin) as f:
            key = bytes.fromhex(f.read().strip())
    except OSError:
        raise SystemExit(f"{chemin} : illisible (cle posee par 'python3 tools/hotte_udp.py cle <port>' ?)")
    except ValueError:
        raise SystemExit(f"{chemin} : cle illisible (64 hexa attendus)")
    if len(key) != 32:
        raise SystemExit(f"{chemin} : cle illisible (64 hexa attendus)")
    return key


def cmd_cle(port, chemin):
    alea = os.urandom(32).hex().upper()
    try:
        fd = serie_enregistre.ouvrir_port(port)
    except OSError as e:
        raise SystemExit(f"{port} : {e.strerror} (port tenu par 'pio device monitor' ou serie_enregistre.py ?)")
    try:
        ident = 900000 + int.from_bytes(os.urandom(2), "big") % 90000
        cle, empreinte, msg = demander_cle(serie_enregistre.lecteur(fd), serie_enregistre.ecrivain(fd), alea, ident)
    except serie_enregistre.PortFerme as e:
        raise SystemExit(f"port ferme ({e}) : la sonde a-t-elle redemarre ?")
    finally:
        os.close(fd)
    ranger_cle(cle, chemin)
    print(f"cle rangee dans {chemin} (empreinte {empreinte})")
    if msg:
        print(f"  ({msg})")


# ---------------------------------------------------------------------------
#  Session H1
# ---------------------------------------------------------------------------


def resoudre(hote, port):
    """(famille, adresse) : premier resultat de getaddrinfo, IPv4 ou IPv6."""
    try:
        ai = socket.getaddrinfo(hote, port, socket.AF_UNSPEC, socket.SOCK_DGRAM)
    except socket.gaierror as e:
        raise SystemExit(f"{hote} : {e} (nom mDNS hotte-sonde.local, ou adresse donnee par 'info')")
    return ai[0][0], ai[0][4]


def confirmer(commandes, demander=input):
    """'injection on' arme l'injection (spec 8.4). Regle 11 : toute emission vers
    la hotte se fait en presence de Majid. Pour chaque 'injection on' de la
    liste, avant tout envoi, Majid tape lui-meme la phrase exacte
    PHRASE_INJECTION. Seulement depuis un terminal : sans terminal sur l'entree
    standard (script, tube, agent), refus, sans question."""
    n = sum(1 for c in commandes if " ".join(c.split()) == "injection on")
    if not n:
        return True
    if sys.stdin is None or not sys.stdin.isatty():
        return False
    for _ in range(n):
        try:
            r = demander("Regle 11 : Majid est devant la hotte, a portee de la fiche, le panneau en vue, et "
                         "debranche au moindre doute.\n'injection on' arme l'injection de la sonde. Pour "
                         f"l'envoyer, Majid tape lui-meme : {PHRASE_INJECTION}\n> ")
        except EOFError:
            return False
        if r != PHRASE_INJECTION:
            return False
    return True


def poignee(s, key, kid, afficher):
    """SALUT signe, na neuf a chaque essai ; seul un DEFI au MAC juste pour ce na est pris."""
    for _ in range(3):
        na = os.urandom(16).hex().upper()
        try:
            s.send(texte_salut(key, kid, na))
        except OSError as e:
            afficher(f"!! envoi impossible : {e.strerror} (sonde sur le Wi-Fi ? meme reseau ?)")
            time.sleep(1)
            continue
        deadline = time.time() + 2.0  # le DEFI peut attendre la fin d'un datagramme en cours
        while time.time() < deadline:
            s.settimeout(max(0.05, deadline - time.time()))
            try:
                d = s.recv(2048)
            except socket.timeout:
                break
            except ConnectionRefusedError:
                raise SystemExit("port injoignable : pas de cle sur la sonde ('hotte_udp.py cle <port>'), "
                                 "ou Wi-Fi sans adresse ('info' par l'USB)")
            except OSError as e:
                afficher(f"!! reception : {e.strerror}")
                break
            r = lire_defi(key, kid, na, d)
            if r:
                return r
    return None


class Client:
    """Session H1 cote app sur un socket UDP connecte."""

    def __init__(self, s, key, afficher=print):
        self.s, self.key, self.kid, self.afficher = s, key, kid_of(key), afficher
        self.sid = None
        self.a_faire = []
        self.stats = {"lignes": 0, "rejetees": 0, "renvois": 0, "sessions": 0}

    def ouvrir(self):
        r = poignee(self.s, self.key, self.kid, self.afficher)
        if not r:
            return False
        self.sid, self.ks = r
        self.ctr, self.ident, self.fenetre = 0, 0, Fenetre()
        self.dernier_rx, self.derniere_cmd, self.en_vol = time.time(), 0.0, None
        self.stats["sessions"] += 1
        self.afficher(f"session {self.sid} ouverte")
        self.envoyer("json 1")
        return True

    def _emettre(self, ident, ligne):
        self.ctr += 1
        texte = f"id={ident} {ligne}"
        try:
            self.s.send(sceller(self.ks, "A", self.sid, self.ctr, texte.encode("ascii")))
        except OSError as e:
            self.afficher(f"!! envoi impossible : {e.strerror}")
        return texte

    def envoyer(self, ligne):
        self.ident += 1
        texte = self._emettre(self.ident, ligne)
        self.en_vol = {"id": self.ident, "ligne": ligne, "at": time.time(), "essais": 0}
        self.derniere_cmd = time.time()
        self.afficher(f">> {texte}")

    def tour(self):
        """Renvoi, commande suivante ou ping, puis un datagramme (0,3 s au plus).
        Rend (charge, objet ou None) pour un message accepte, sinon None."""
        now = time.time()
        if now - self.dernier_rx > 8:
            self.afficher("!! sonde muette depuis 8 s : nouvelle poignee de main")
            if not self.ouvrir():
                time.sleep(1)
                return None
            now = time.time()
        p = self.en_vol
        if p and now - p["at"] > 2:
            if p["essais"] < 2:
                p["essais"] += 1
                p["at"] = now
                self.stats["renvois"] += 1
                self.afficher(f">> (renvoi) {self._emettre(p['id'], p['ligne'])}")
            else:
                self.afficher(f"!! id={p['id']} sans reponse")
                self.en_vol = None
        if not self.en_vol:
            if self.a_faire and now - self.derniere_cmd >= 1.5:
                self.envoyer(self.a_faire.pop(0))
            elif now - self.derniere_cmd >= 10:
                self.envoyer("json ping")
        self.s.settimeout(0.3)
        try:
            d = self.s.recv(2048)
        except socket.timeout:
            return None
        except ConnectionRefusedError:
            self.afficher("!! port injoignable : cle effacee ? sonde redemarree ?")
            time.sleep(0.3)
            return None
        except OSError as e:
            self.afficher(f"!! reception : {e.strerror}")
            time.sleep(0.5)
            return None
        r = ouvrir_message(self.ks, "C", self.sid, d, self.fenetre)
        if r is None:
            self.stats["rejetees"] += 1
            return None
        charge = r[1]
        self.dernier_rx = time.time()
        self.stats["lignes"] += 1
        try:
            m = json.loads(charge)
        except ValueError:
            self.afficher(f"!! JSON illisible : {charge[:80]!r}")
            return charge, None
        if not isinstance(m, dict):
            return charge, None
        if (m.get("t") == "reponse" and m.get("etape") == "fin" and self.en_vol
                and m.get("id") == self.en_vol["id"]):
            self.en_vol = None
        return charge, m

    def fermer(self, suite=None, delai_s=1.0):
        """'json 0' ; les lignes recues jusqu'au message fin (1 s au plus) passent par suite."""
        if not self.sid:
            return
        self.a_faire = []
        self.en_vol = None
        self.envoyer("json 0")
        limite = time.time() + delai_s
        while time.time() < limite:
            r = self.tour()
            if r and suite:
                suite(*r)
            if r and r[1] and r[1].get("t") == "fin":
                break


def resume(m):
    """Une ligne lisible par message du profil hotte."""
    t, b = m.get("t"), m.get("bloc")
    tete = f"n={m.get('n')} {t}" + (f"/{b}" if b else "")
    if t == "reponse":
        extra = f" id={m.get('id')} {m.get('etape')} {m.get('code')} « {m.get('cmd')} »"
        if m.get("msg"):
            extra += f" : {m['msg']}"
        return tete + extra
    if t == "trame":
        d = m.get("dur_us") or []
        extra = f" {m.get('num')}.{m.get('part')}{' fin' if m.get('fin') else ''} {m.get('niv0')} {len(d)} duree(s)"
        for k in ("rep", "sautes"):
            if k in m:
                extra += f" {k} {m[k]}"
        return tete + extra + (" DEBORD" if m.get("debord") else "")
    if t == "compteurs":
        return tete + "".join(f" {k} {m.get(k)}" for k in ("receptions", "debord", "lignes_perdues", "sautes", "rejets"))
    if t == "etat" and b == "bus":
        return tete + f" repos {m.get('repos')} fronts {m.get('fronts')} receptions/s {m.get('receptions_s')}"
    if t == "reseau" and b == "ip":
        w, u = m.get("wifi", {}), m.get("udp", {})
        return tete + (f" wifi {w.get('ip')} {w.get('rssi_dbm')} dBm pertes {w.get('pertes')} | udp sessions "
                       f"{u.get('sessions')} rx {u.get('rx')} tx {u.get('tx')} perdus {u.get('tx_perdus')}")
    if t == "hello" and b == "base":
        return tete + f" fw {m.get('fw')} rev {m.get('rev')} session {m.get('session')}"
    if t == "injection":
        return tete + f" id={m.get('id')} {m.get('resultat')} attente {m.get('attente_us')} us"
    if t == "log":
        return tete + f" [{m.get('src')}] {m.get('txt')}"
    if t == "fin":
        return tete + f" cause {m.get('cause')}"
    return tete


def ouvrir_client(hote, port, commandes, key, afficher, demander):
    if not confirmer(commandes, demander):
        raise SystemExit(f"injection on non confirmee (phrase '{PHRASE_INJECTION}' tapee par Majid, dans un "
                         "terminal : regle 11) : rien n'est envoye")
    famille, adresse = resoudre(hote, port)
    s = socket.socket(famille, socket.SOCK_DGRAM)
    try:
        s.connect(adresse)
    except OSError as e:
        s.close()
        raise SystemExit(f"{adresse[0]} : {e.strerror}")
    afficher(f"sonde {adresse[0]} port {adresse[1]}, cle {kid_of(key)}")
    c = Client(s, key, afficher)
    c.a_faire = list(commandes)
    if not c.ouvrir():
        s.close()
        raise SystemExit("aucun DEFI en 6 s (cle differente ? sonde hors ligne ? 'info' par l'USB)")
    return c, adresse[0]


def cmd_session(hote, port, commandes, duree, brut, key, afficher=print, demander=input):
    c, _ = ouvrir_client(hote, port, commandes, key, afficher, demander)

    def montrer(charge, m):
        afficher(charge.decode("ascii", "replace") if brut or m is None else resume(m))

    debut = time.time()
    try:
        while time.time() - debut < duree:
            r = c.tour()
            if r:
                montrer(*r)
    except KeyboardInterrupt:
        pass
    c.fermer(montrer)
    c.s.close()
    st = c.stats
    afficher(f"{st['lignes']} ligne(s) recue(s), {st['rejetees']} rejetee(s), {st['renvois']} renvoi(s), "
             f"{st['sessions']} session(s)")
    return st


def enregistrement(rx_ms, de, charge):
    """Une ligne .jsonl : l'objet recu tel quel (octets ASCII du firmware)."""
    return '{"rx_ms":%d,"de":%s,"l":%s}\n' % (rx_ms, json.dumps(de), charge.decode("ascii"))


def journaliser(journal, de, scenario, etat, duree_s, bilan, stats, afficher):
    quand = datetime.datetime.now()
    ligne = (f"{quand:%Y-%m-%d %H:%M:%S} udp {de} {scenario} {etat} {duree_s:.0f} s : {bilan.resume()} ; "
             f"H1 : {stats['rejetees']} rejetee(s), {stats['renvois']} renvoi(s), {stats['sessions']} session(s)")
    with open(journal, "a", encoding="utf-8") as f:
        f.write(ligne + "\n")
    afficher(ligne)


def cmd_enregistre(hote, port, scenario, commandes, duree, dossier, key, afficher=print, demander=input):
    """Session enregistree ; rend le chemin du .jsonl."""
    c, de = ouvrir_client(hote, port, commandes, key, afficher, demander)
    os.makedirs(dossier, exist_ok=True)
    chemin = os.path.join(dossier, serie_enregistre.nom_fichier(scenario, datetime.datetime.now()))
    journal = os.path.join(dossier, "live.log")
    bilan = serie_enregistre.Bilan()
    afficher(f"enregistrement dans {chemin} (resume dans {journal})")
    with open(chemin, "w", encoding="ascii") as sortie:

        def noter(charge, m):
            valide = (isinstance(m, dict) and m.get("v") == 1 and not isinstance(m.get("v"), bool)
                      and isinstance(m.get("t"), str) and isinstance(m.get("n"), int)
                      and not isinstance(m.get("n"), bool))
            if not valide:
                bilan.abimees += 1
                return
            sortie.write(enregistrement(int(round(time.time() * 1000)), de, charge))
            bilan.noter(m)
            if m["t"] == "reponse" and m.get("etape") == "fin":
                afficher(resume(m))

        debut = time.time()
        prochain = debut + RESUME_S
        etat = "fin"
        try:
            while duree is None or time.time() - debut < duree:
                r = c.tour()
                if r:
                    noter(*r)
                if time.time() >= prochain:
                    sortie.flush()
                    journaliser(journal, de, scenario, "en cours", time.time() - debut, bilan, c.stats, afficher)
                    prochain += RESUME_S
        except KeyboardInterrupt:
            etat = "interrompu"
        duree_s = time.time() - debut
        c.fermer(noter)
        c.s.close()
    journaliser(journal, de, scenario, etat, duree_s, bilan, c.stats, afficher)
    return chemin


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("cle", help="nouvelle cle par l'USB")
    a.add_argument("port", help="port serie du C6, ex. /dev/cu.usbmodem101")
    for nom in ("session", "enregistre"):
        a = sub.add_parser(nom, help="session H1" if nom == "session" else "session H1 enregistree en .jsonl")
        a.add_argument("hote", help="hotte-sonde.local, ou l'adresse IPv4 de la sonde")
        if nom == "enregistre":
            a.add_argument("scenario", help="nom du scenario (nom du fichier)")
            a.add_argument("--dossier", default=DOSSIER, help="dossier des enregistrements (defaut : logs/ du depot)")
        a.add_argument("commandes", nargs="*", help="commandes envoyees apres 'json 1', sans id=")
        a.add_argument("--duree", type=float, default=60.0 if nom == "session" else None,
                       help="duree en secondes (session : 60 ; enregistre : jusqu'a Ctrl-C)")
        a.add_argument("--port", type=int, default=PORT)
        if nom == "session":
            a.add_argument("--brut", action="store_true", help="JSON tel quel au lieu du resume")
    args = ap.parse_args(argv)
    if args.cmd == "cle":
        cmd_cle(args.port, chemin_cle())
    elif args.cmd == "session":
        cmd_session(args.hote, args.port, args.commandes, args.duree, args.brut, lire_cle(chemin_cle()))
    else:
        cmd_enregistre(args.hote, args.port, args.scenario, args.commandes, args.duree, args.dossier,
                       lire_cle(chemin_cle()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Étape 4 : Lancer les tests et vérifier qu'ils passent**

Lancer : `python3 -m unittest discover -s tools/tests -p test_hotte_udp.py -v`
Attendu : « Ran 12 tests » (environ 6 s : trois sessions UDP réelles de 2 s) puis « OK »

Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « Ran 153 tests » puis « OK »

Lancer : `python3 tools/hotte_udp.py cle /dev/cu.inexistant; echo "code $?"`
Attendu : « /dev/cu.inexistant : No such file or directory (port tenu par 'pio device monitor' ou serie_enregistre.py ?) » puis « code 1 » (aucun port n'est ouvert)

Lancer (sans terminal, comme un agent ; la clé de test ne sert qu'à passer la lecture du fichier) : `K=$(mktemp -d)/cle && python3 -c "print('00' * 32)" > $K && HOTTE_CLE=$K python3 tools/hotte_udp.py session 127.0.0.1 "injection on" --duree 1 < /dev/null; echo "code $?"`
Attendu : « injection on non confirmee (phrase 'Majid devant la hotte' tapee par Majid, dans un terminal : regle 11) : rien n'est envoye » puis « code 1 » (entrée standard qui n'est pas un terminal : refus sans question, aucun datagramme ; la phrase par un tube l'est aussi, `test_confirmation`)

- [ ] **Étape 5 : Commit**

```bash
git add tools/hotte_udp.py tools/tests/test_hotte_udp.py
git commit -m "Ajouter le client reseau hotte_udp.py" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 20b : Procédure du banc par le Wi-Fi et sur la batterie (`docs/BANC.md` §8)

La tâche 21 est un banc avec Majid : elle n'écrit que des résultats. Sa
procédure s'écrit ici, comme celle de la tâche 16 l'est en tâche 15. Le §8 de
`docs/BANC.md` suit l'ordre de la séance : Wi-Fi et clé, trente minutes sur la
batterie de l'étape 5 (spec §12 : sonde en Wi-Fi alimentée 30 min sans
coupure, modèle noté dans `RECONNAISSANCE.md`), consommation Wi-Fi actif
(critère 6), dix minutes de `krona` (critère 4). **Le banc n'a jamais de
liaison avec la hotte** (`docs/BANC.md` §2.1, tâche 15) : le §8 le rappelle,
batterie comprise, puisque le banc va à l'emplacement de test, près de la
hotte.

**Fichiers :**
- Modifier : `docs/BANC.md` (§1 : batterie et câble ouvert ; §2.4 : renvoi vers le §8.3 ; §7 : ligne `21` de « Montage et firmwares », ligne Wi-Fi du « Critère 6 », tableau « Batterie USB » ; §8 ajouté en fin de fichier)
- Tester : contrôle du texte par `grep`, verdict de la batterie sur deux fichiers simulés, `python3 -m unittest discover -s tools/tests`

**Interfaces :**
- Consomme : `docs/BANC.md` (tâche 15 : §1, §2.1 où monter la sonde, §2.4 courant de la sonde, §7 tableaux de résultats) ; `tools/hotte_udp.py` (tâche 20 : `cle`, `session`, `enregistre`, lignes `.jsonl` `{"rx_ms", "de", "l"}`) ; champs `boot` et `up_s` des lignes `hello` et `etat`, `wifi.rssi_dbm` de `reseau/ip` (tâches 11 et 19 ; à distance, un `etat` toutes les 2 s) ; `tools/banc.py` (tâche 15) ; `tools/json_check.py --jsonl` (tâche 11) ; générateur (tâche 14 : `motif krona <n>`, `0` : sans fin, `stop`) ; `docs/RECONNAISSANCE.md` (tâche 1 : ligne « Batterie USB » de « Installation et instruments »).
- Produit : `docs/BANC.md` §8 (8.1 Wi-Fi et clé, 8.2 trente minutes sur la batterie, 8.3 consommation Wi-Fi actif, 8.4 dix minutes de `krona`), suivi à la lettre par la tâche 21, et les tableaux du §7 qu'elle remplit. Le §8 finit par le paragraphe « Si le critère échoue ... » : la tâche 23b insère le §9 après lui.

**Décisions :**
- **Batterie** : sonde branchée sur la batterie (elle redémarre), générateur en `motif krona 0`, `python3 tools/hotte_udp.py enregistre hotte-sonde.local batterie-30min "capture tout" "seuils 1 19000" --duree 1860`. Verdict sur ce seul fichier : **un seul `boot`**, `up_s` croissant jusqu'à **1 800 au moins**, **aucun trou de `rx_ms` de plus de 10 s**. Une batterie qui se coupe fait redémarrer la sonde (deux `boot`, `up_s` qui recule) ou la fait taire (trou). Modèle, date et résultat vont au §7 et dans `docs/RECONNAISSANCE.md`.
- **Consommation, Wi-Fi actif (critère 6)** : les deux méthodes du §2.4, mais depuis la batterie : testeur USB entre la batterie et le câble de la sonde ; sinon câble de la batterie ouvert, multimètre en série sur VBUS vers la broche `5V` de la sonde, **USB-C de la sonde débranché** pendant toute la mesure, **calibre 200 mA**.
- **Ordre** : la batterie avant le critère 4, qui se joue sur elle ; la consommation entre les deux.
- Le tableau du §6 et le tableau « Critère 5 » du §7 ne changent pas : la tâche 23b les remplace à partir de leur texte de la tâche 15 ; elle ajoute aussi la ligne `24` de « Montage et firmwares », après la ligne `21` posée ici.

- [ ] **Étape 1 : Vérifier que la procédure manque (le contrôle qui échoue)**

Lancer :

```bash
for m in "## 8. Par le Wi-Fi, sur la batterie (critères 4 et 6)" "### 8.2 Trente minutes sur la batterie" 'batterie-30min "capture tout" "seuils 1 19000" --duree 1860' "**un seul boot**" "### 8.3 Consommation, Wi-Fi actif (critère 6)" "| | 21 | 1 (10k) |" "### Batterie USB : 30 min en Wi-Fi (tâche 21)" "Wi-Fi actif, sur la batterie :"; do grep -q -F -- "$m" docs/BANC.md && echo "ok : $m" || echo "ABSENT : $m"; done
```

Attendu : huit lignes « ABSENT : ... ».

- [ ] **Étape 2 : Écrire la procédure**

`docs/BANC.md` (quatre remplacements, puis le §8 en fin de fichier) :

1. Dans `docs/BANC.md`, remplacer :

```markdown
- testeur USB, s'il y en a un (critère 6, §2.4) ;
```

   par :

```markdown
- testeur USB, s'il y en a un (critère 6, §2.4 et §8.3) ;
- la batterie USB de l'étape 5 (§8) et, sans testeur USB, un câble USB de
  cette batterie qu'on peut ouvrir (fils VBUS et GND accessibles, §8.3) ;
```

2. Dans `docs/BANC.md`, remplacer :

```markdown
Deux méthodes ; l'instrument est noté au §7.
```

   par :

```markdown
Deux méthodes ; l'instrument est noté au §7. Wi-Fi actif, sur la batterie :
§8.3.
```

3. Dans `docs/BANC.md`, remplacer :

```markdown
| | 16b | 2 (4,7k + 1 nF) | | | | | | | |
```

   par :

```markdown
| | 16b | 2 (4,7k + 1 nF) | | | | | | | |
| | 21 | 1 (10k) | | | | | | | |
```

4. Dans `docs/BANC.md`, remplacer :

```markdown
| | Wi-Fi actif (tâche 21) | session UDP, motif `krona` | |

### Conclusions
```

   par :

```markdown
| | batterie, Wi-Fi actif (tâche 21) ; testeur USB ou multimètre : ____ (§8.3) | session UDP, motif `krona` | |

### Batterie USB : 30 min en Wi-Fi (tâche 21)

| Date | Modèle de la batterie | Fichier `logs/` | Boots | `up_s` (début → fin) | Plus grand trou de `rx_ms` (s) | Verdict |
|---|---|---|---|---|---|---|
| | | | | | | |

### Conclusions
```

Puis ajouter à la fin de `docs/BANC.md` (après la ligne `- Limites constatées :` du §7) :

```markdown

## 8. Par le Wi-Fi, sur la batterie (critères 4 et 6)

Même montage (§2, variante 1), **toujours sans aucune liaison avec la hotte**
(§2.1) : sonde sur le boîtier détaché (hotte débranchée pendant toute la
séance, fiche XH 4 broches retirée de l'embase, les deux **OL** relevés avant
tout USB, batterie comprise), ou sur la plaque d'essai du banc. La sonde parle
au Mac par le Wi-Fi (UDP, port 5480,
[PROTOCOLE-JSON.md §9](PROTOCOLE-JSON.md#9-transport-réseau--udp-sur-le-wi-fi)).
À partir du §8.2, elle est alimentée par la **batterie USB** de l'étape 5
(spec §12), pas par le Mac : sans hôte qui lit, l'USB ne ralentit jamais la
sonde. Le générateur reste sur l'USB du Mac, console ouverte ; le fil de masse
relie les deux C6. Rien de ce qui suit ne s'écrit dans ce fichier : ni SSID,
ni mot de passe, ni clé, ni adresse IP.

Ordre : Wi-Fi et clé (§8.1), trente minutes sur la batterie (§8.2),
consommation (§8.3), dix minutes de `krona` (§8.4).

### 8.1 Une fois : Wi-Fi et clé

1. **Identifiants Wi-Fi (Majid, au clavier).** Sonde sur l'USB du Mac. Majid
   ouvre une console **hors du dépôt**, pour que `log2file` n'enregistre pas le
   mot de passe :
   `cd ~ && ~/.platformio/penv/bin/pio device monitor -p $PORT_SONDE -b 115200`.
   Il tape `wifi <ssid> <mdp>`, puis `info` quelques secondes plus tard.
   Attendu : `wifi : <ssid>, connecte, IP <a.b.c.d>, RSSI <-xx> dBm, mDNS hotte-sonde.local, 0 perte(s) depuis le demarrage`.
   Il ferme la console (Ctrl-C) pour libérer le port.
2. **Clé H1 (l'agent).** `python3 tools/hotte_udp.py cle $PORT_SONDE`.
   Attendu : `cle rangee dans /Users/<nom>/.config/hotte-sonde/cle (empreinte XXXXXXXX)`.
   La clé n'est jamais affichée.
3. **Chemin (l'agent).** `ping -c 3 hotte-sonde.local`, puis
   `python3 tools/hotte_udp.py session hotte-sonde.local --duree 15`.
   Attendu : `session XXXXXXXX ouverte`, `hello/base` avec `'transport': 'udp'`,
   `reponse id=1 fin ok « json 1 »`, un `etat` toutes les 2 s, `reseau/ip` avec
   le RSSI, et à la fin `0 rejetee(s)`.

### 8.2 Trente minutes sur la batterie

La batterie de l'étape 5 doit tenir la sonde en Wi-Fi 30 min sans se couper
(spec §12) : certaines batteries s'éteignent seules sous un faible courant.

1. **Sonde sur la batterie (Majid).** Le banc à l'emplacement de test (là où
   la sonde sera posée à l'étape 5), toujours sans liaison avec la hotte.
   Majid débranche la sonde du Mac et la branche sur la batterie : elle
   redémarre et rejoint le Wi-Fi. Après 10 s,
   `python3 tools/hotte_udp.py session hotte-sonde.local --duree 10` (l'agent)
   doit ouvrir la session.
2. **Motif sans fin (Majid).** `motif krona 0` dans la console du générateur.
3. **Enregistrement (l'agent)**, 31 min :
   `python3 tools/hotte_udp.py enregistre hotte-sonde.local batterie-30min "capture tout" "seuils 1 19000" --duree 1860`.
   À la fin, Majid tape `stop` dans la console du générateur.
4. **Verdict (l'agent)**, sur le fichier écrit :
   `python3 -c "import json,sys; L=[json.loads(x) for x in open(sys.argv[1])]; B={r['l']['boot'] for r in L if 'boot' in r['l']}; U=[r['l']['up_s'] for r in L if 'up_s' in r['l']]; T=[r['rx_ms'] for r in L]; print('boots', len(B), '; up_s', U[0], 'a', U[-1], '(croissant)' if U == sorted(U) else '(RECUL)', '; plus grand trou de rx_ms', max(b - a for a, b in zip(T, T[1:])) / 1000, 's')" logs/<fichier>.jsonl`.
   Attendu : `boots 1 ; up_s <début> a <fin> (croissant) ; plus grand trou de rx_ms <g> s`.
   La batterie convient si : **un seul boot** dans le fichier, `up_s`
   croissant jusqu'à **1 800 au moins**, et **aucun trou de `rx_ms` de plus
   de 10 s**. Sinon (la sonde a redémarré, ou s'est tue) : une autre batterie,
   et on recommence.
5. Noter le modèle de la batterie, la date et le résultat au §7 (« Batterie
   USB ») et dans [RECONNAISSANCE.md](RECONNAISSANCE.md#installation-et-instruments)
   (« Installation et instruments »).

### 8.3 Consommation, Wi-Fi actif (critère 6)

Sonde sur la batterie, pendant une session Wi-Fi avec le motif `krona`.
L'instrument est noté au §7 (critère 6). Deux méthodes :

- **Testeur USB**, s'il y en a un : entre la batterie et le câble USB de la
  sonde.
- **Sinon, multimètre en série sur VBUS**, depuis la batterie :
  1. batterie débranchée ; **l'USB-C de la sonde reste débranché pendant
     toute la mesure** : jamais deux sources sur sa broche `5V` ;
  2. un câble USB de la batterie, **ouvert** (fils VBUS et GND accessibles) :
     son fil GND sur `TP−` (le `GND` de la sonde) ; multimètre en courant
     continu, **calibre 200 mA**, borne mA sur le fil VBUS, COM sur la broche
     `5V` de la sonde ; sur le boîtier, `J2` reste ouvert ;
  3. brancher la batterie : la sonde démarre, capture active, et rejoint le
     Wi-Fi. Si le courant saute sans cesse, ou si la session tombe, la chute
     de tension dans le calibre est trop forte : passer sur l'entrée 10 A ;
  4. à la fin, débrancher la batterie, retirer le multimètre et le câble
     ouvert, puis rebrancher la batterie sur l'USB-C de la sonde.

Mesure : l'agent lance
`python3 tools/hotte_udp.py session hotte-sonde.local --duree 60`, Majid tape
`motif krona 400` dans la console du générateur et lit le courant moyen (de
l'ordre de 100 mA, spec §8.7 ; les pointes d'émission ne s'y lisent pas,
§2.4).

### 8.4 Dix minutes de `krona` (critère 4)

1. **Emplacement (Majid).** Sonde sur la batterie, à l'emplacement de test ;
   générateur sur l'USB du Mac, console ouverte ; fil de masse en place.
   Attendre 10 s après le branchement de la batterie, puis
   `python3 tools/hotte_udp.py session hotte-sonde.local --duree 10` (l'agent)
   doit ouvrir la session.
2. **Enregistrement (l'agent).**
   `python3 tools/hotte_udp.py enregistre hotte-sonde.local banc-wifi-krona "capture tout" "seuils 1 19000" --duree 660`.
   Dans un second terminal : `tail -f logs/live.log` (un résumé toutes les
   5 s).
3. **Motif (Majid).** Dès que le terminal affiche
   `reponse id=3 fin ok « seuils 1 19000 »`, Majid tape `motif krona 4500` dans
   la console du générateur : 4 500 trames de 134 ms, soit 10 min 3 s. Il
   regarde défiler `motif krona trame <index>` jusqu'à
   `motif krona fini : 4500 trames`. L'enregistrement s'arrête seul à 660 s.
4. **Verdicts (l'agent)**, sur le fichier écrit :
   - `python3 tools/json_check.py --jsonl logs/<fichier>.jsonl` : 0 erreur, et
     la ligne `n : ... (... ligne(s) perdue(s), X %)` ; il faut **X < 0,100**.
   - `python3 tools/banc.py logs/<fichier>.jsonl krona --n 4500 --decalage-us <retenu au §7>` :
     `compteurs de la sonde pendant la capture : debord +0, lignes_perdues +0, sautes +0`
     (zéro perte côté sonde) ; `critere 1` vaut 4500 / 4500 si le Wi-Fi n'a
     rien perdu.
   - RSSI pendant la capture :
     `python3 -c "import json,sys; r=[json.loads(l)['l']['wifi']['rssi_dbm'] for l in open(sys.argv[1]) if json.loads(l)['l'].get('bloc')=='ip']; print('RSSI', min(r), 'a', max(r), 'dBm sur', len(r), 'releves')" logs/<fichier>.jsonl`.
5. Noter au §7 (critère 4) : date, emplacement (en mots, sans adresse), RSSI,
   durée, hausses des compteurs, trous de `n`, verdict.

Si le critère échoue : noter le RSSI et `wifi.pertes`, rapprocher la sonde du
point d'accès et recommencer ; sinon, reprendre le risque « Wi-Fi trop
faible » du §13 de la spec (enregistrement local, relu par l'USB).
```

- [ ] **Étape 3 : Relancer le contrôle, le verdict de la batterie et les tests**

Lancer la boucle de l'étape 1.
Attendu : huit lignes « ok : ... », aucune « ABSENT : ... ».

Lancer (le verdict du §8.2 sur deux fichiers simulés de 31 min : une sonde qui tient, une sonde qui redémarre après 30 s de silence) :

```bash
python3 -c "import json; [open(f'${TMPDIR:-/tmp}/bat-{n}.jsonl', 'w').write(''.join(json.dumps({'rx_ms': 2000 * i + (30000 if n == 'ko' and i > 50 else 0), 'de': 'x', 'l': {'t': 'etat', 'boot': 'B' if n == 'ok' or i <= 50 else 'C', 'up_s': 2 * i if n == 'ok' or i <= 50 else i - 50}}) + '\n' for i in range(1, 931))) for n in ('ok', 'ko')]"
for f in ok ko; do python3 -c "import json,sys; L=[json.loads(x) for x in open(sys.argv[1])]; B={r['l']['boot'] for r in L if 'boot' in r['l']}; U=[r['l']['up_s'] for r in L if 'up_s' in r['l']]; T=[r['rx_ms'] for r in L]; print('boots', len(B), '; up_s', U[0], 'a', U[-1], '(croissant)' if U == sorted(U) else '(RECUL)', '; plus grand trou de rx_ms', max(b - a for a, b in zip(T, T[1:])) / 1000, 's')" ${TMPDIR:-/tmp}/bat-$f.jsonl; done
```

Attendu :

```text
boots 1 ; up_s 2 a 1860 (croissant) ; plus grand trou de rx_ms 2.0 s
boots 2 ; up_s 2 a 880 (RECUL) ; plus grand trou de rx_ms 32.0 s
```

Lancer : `python3 -m unittest discover -s tools/tests 2>&1 | tail -3`
Attendu : « Ran 153 tests » puis « OK » (aucun test ne change ; `test_banc.py` relit le tableau du §4, inchangé).

- [ ] **Étape 4 : Commit**

```bash
git add docs/BANC.md
git commit -m "Ecrire la procedure du banc par le Wi-Fi et sur la batterie" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Tâche 21 [BANC avec Majid] : critère 4 (dix minutes de `krona` par le Wi-Fi), batterie de 30 min, consommation Wi-Fi actif

Exécutée par l'agent principal avec Majid, jamais par un sous-agent. Montage : celui de la tâche 16 (`docs/BANC.md` §2, variante 1), qui doit être validée d'abord (critères 1 à 3 par l'USB, décalage retenu au §7). La procédure est celle de `docs/BANC.md` §8 (tâche 20b) : cette tâche n'écrit que des résultats.

**Fichiers :**
- Modifier : `docs/BANC.md` (§7 seulement : ligne `21` de « Montage et firmwares », « Critère 4 : Wi-Fi (tâche 21) », ligne Wi-Fi du « Critère 6 », « Batterie USB : 30 min en Wi-Fi (tâche 21) », « Conclusions ») ; `docs/RECONNAISSANCE.md` (« Installation et instruments », ligne « Batterie USB »)
- Tester : `tools/hotte_udp.py enregistre`, `tools/json_check.py --jsonl`, `tools/banc.py`

**Interfaces :**
- Consomme : firmware `sonde` des tâches 17 à 19 (`wifi`, `info`, `json cle nouvelle`, port UDP 5480, `hotte-sonde.local`) ; `tools/hotte_udp.py` (tâche 20) ; générateur (tâche 14 : `motif krona <n>`, `motif krona 0` sans fin, `stop`) ; `tools/banc.py <capture> krona --n N --decalage-us D` (tâche 15, qui imprime `compteurs de la sonde pendant la capture : debord +x, lignes_perdues +y, sautes +z`) ; `tools/json_check.py --jsonl` (tâche 11 : `n : ... (... ligne(s) perdue(s), X %)`) ; `docs/BANC.md` §2.1, §2.4 et §7 (tâche 15), §8 (tâche 20b).
- Produit : les résultats du critère 4, de la batterie et de la consommation Wi-Fi (critère 6) dans `docs/BANC.md` §7 ; le modèle de batterie vérifié dans `docs/RECONNAISSANCE.md` (prérequis de l'étape 5) ; la clé H1 de Majid dans `~/.config/hotte-sonde/cle` (hors dépôt), réutilisée à l'étape 5 de la spec.

**Règles pour cette tâche :**
- **Aucun secret dans le dépôt** (public) : ni SSID, ni mot de passe, ni clé, ni adresse IP. Le mot de passe du Wi-Fi est tapé par Majid lui-même ; l'agent ne le lit ni ne le recopie.
- **Le banc n'a jamais de liaison avec la hotte** (`docs/BANC.md` §2.1) : la sonde n'est sur le boîtier de mesure détaché que si la hotte reste débranchée pendant toute la séance (déplacement à l'emplacement de test compris) et si le câble de sortie est déconnecté du boîtier (fiche XH 4 broches retirée de l'embase) ; avant tout USB, batterie comprise, `TP−` vers la terre de la fiche de la hotte puis vers la vis de la carcasse : **OL**. Sinon, plaque d'essai du banc avec son propre étage d'écoute (mêmes valeurs). Rien ne touche au secteur.
- Aucun code ni document à écrire : seulement des résultats, dans les tableaux prévus par la tâche 20b.
- La batterie se juge sur un seul fichier, l'enregistrement de 1 860 s de l'étape 7 ; le critère 4 aussi, l'enregistrement de 660 s de l'étape 9.

- [ ] **Étape 1 : Vérifier le logiciel avant le banc (l'agent)**

Lancer : `sh tools/tests/test_hote.sh | tail -1`, `python3 -m unittest discover -s tools/tests 2>&1 | tail -1`, `~/.platformio/penv/bin/pio run -e sonde 2>&1 | tail -3`, `grep -c '^### 8\.' docs/BANC.md`
Attendu : « tests hote : OK », « OK », « sonde          SUCCESS », `4` (les §8.1 à §8.4 de la tâche 20b). Si l'un échoue : on s'arrête, pas de banc.

- [ ] **Étape 2 : Montage, hors tension, sans liaison avec la hotte (Majid)**

Tous les câbles USB débranchés, batterie à part. Le montage de la tâche 16, variante 1 (`docs/BANC.md` §2.1 et §2.2), sans étage d'injection. Contrôles au multimètre, hors tension, **avant de brancher le moindre USB** (batterie comprise), notés au §7 (« Montage et firmwares », ligne `21`) :

| Mesure | Attendu | Sinon |
|---|---|---|
| boîtier seulement : hotte et câble de sortie | hotte débranchée (fiche hors de la prise, à vue) pour toute la séance ; fiche XH 4 broches du câble de sortie retirée de l'embase du boîtier | ne rien brancher |
| boîtier seulement : `TP−` vers la terre de la fiche de la hotte (le trou qui reçoit la broche de terre), calibre le plus élevé | OL | une liaison avec la hotte reste : ne rien brancher, la chercher |
| boîtier seulement : `TP−` vers la vis de la carcasse, calibre le plus élevé | OL | idem |
| LIGNE vers `GND` (ohmmètre) | plus de 10 kΩ | court-circuit : revoir le câblage avant tout USB |
| GPIO6 de la sonde | relié seulement au collecteur de l'étage d'écoute et à son 10k | revoir le câblage |

- [ ] **Étape 3 : Flasher la sonde (l'agent, avec l'accord de Majid)**

Montage de l'étape 2 en place, sonde sur l'USB du Mac, générateur branché, sa console ouverte (`docs/BANC.md` §3).
Lancer : `~/.platformio/penv/bin/pio run -e sonde -t upload --upload-port $PORT_SONDE`
Attendu : `SUCCESS` ; la sonde redémarre (le `fw` affiché par `info` porte le commit de la tâche 20b).

- [ ] **Étape 4 : Identifiants Wi-Fi (Majid, au clavier)**

Majid ouvre une console hors du dépôt (pour que `log2file` n'écrive pas l'écho du mot de passe dans le dépôt) :
`cd ~ && ~/.platformio/penv/bin/pio device monitor -p $PORT_SONDE -b 115200`
Il tape `wifi <ssid> <mdp>`. Attendu : `wifi : identifiants enregistres, connexion a <ssid> ('info' pour suivre)`, puis, en quelques secondes, `[wifi] connecte a <ssid> : IP <a.b.c.d>, RSSI <-xx> dBm, hotte-sonde.local`.
Il tape `info`. Attendu, entre autres :
`wifi : <ssid>, connecte, IP <a.b.c.d>, RSSI <-xx> dBm, mDNS hotte-sonde.local, 0 perte(s) depuis le demarrage`
`udp : port 5480 ferme, aucune cle ('python3 tools/hotte_udp.py cle <port>')`
Il ferme la console (Ctrl-C) : le port doit être libre pour l'étape suivante.
En cas de `wifi : refuse (...)` : SSID avec espace, ou mot de passe de moins de 8 caractères ; `deconnecte (nouvel essai toutes les 10 s)` qui dure : mauvais mot de passe, ou réseau en 5 GHz seulement (le C6 est en 2,4 GHz).

- [ ] **Étape 5 : Clé H1 (l'agent)**

Lancer : `python3 tools/hotte_udp.py cle $PORT_SONDE`
Attendu : « cle rangee dans /Users/<nom>/.config/hotte-sonde/cle (empreinte XXXXXXXX) » et « (nouvelle cle : les sessions reseau tombent) ». Noter l'empreinte (8 hexa, pas secrète) pour la comparer à `udp.empreinte`.

- [ ] **Étape 6 : Chemin réseau (l'agent)**

Lancer : `ping -c 3 hotte-sonde.local`
Attendu : trois réponses de l'adresse vue à l'étape 4.
Lancer : `python3 tools/hotte_udp.py session hotte-sonde.local --duree 15`
Attendu : `sonde <a.b.c.d> port 5480, cle XXXXXXXX`, `session XXXXXXXX ouverte`, `n=0 hello/base fw 0.1.0-... rev 4 session {'transport': 'udp', 'periode_ms': 2000, 'compteurs_ms': 5000, 'reseau_ms': 30000, ...}`, `reponse id=1 fin ok « json 1 »`, un `etat` toutes les 2 s, une ligne `reseau/ip wifi <a.b.c.d> <-xx> dBm pertes 0 | udp sessions 1 ...`, et à la fin `... ligne(s) recue(s), 0 rejetee(s), 0 renvoi(s), 1 session(s)`.
En cas de « aucun DEFI en 6 s » : relancer l'étape 5 (clé d'une autre sonde ?), vérifier que le Mac est sur le même réseau.

- [ ] **Étape 7 : Trente minutes sur la batterie (Majid et l'agent)**

`docs/BANC.md` §8.2.
1. Majid pose le banc à l'emplacement de test (là où la sonde sera posée à l'étape 5), toujours sans liaison avec la hotte, débranche la sonde du Mac et la branche sur la **batterie USB** de l'étape 5 (le générateur reste sur le Mac, console ouverte ; fil de masse en place). Après 10 s, l'agent lance `python3 tools/hotte_udp.py session hotte-sonde.local --duree 10` : la session doit s'ouvrir.
2. Majid tape `motif krona 0` dans la console du générateur.
3. L'agent lance `python3 tools/hotte_udp.py enregistre hotte-sonde.local batterie-30min "capture tout" "seuils 1 19000" --duree 1860`, et dans un second terminal `tail -f logs/live.log`. L'enregistrement s'arrête seul à 1 860 s (31 min) et affiche le chemin de `logs/AAAA-MM-JJ-hhmm-batterie-30min.jsonl`. Majid tape alors `stop`.
4. L'agent lance le verdict du §8.2, point 4, sur ce fichier.
   Attendu : `boots 1 ; up_s <début> a <fin> (croissant) ; plus grand trou de rx_ms <g> s`, avec `<fin>` d'au moins 1800 et `<g>` d'au plus 10.
   **Batterie retenue** si les trois conditions tiennent (un seul boot, `up_s` croissant jusqu'à 1 800 au moins, aucun trou de `rx_ms` de plus de 10 s). Sinon : noter le modèle refusé, prendre une autre batterie et recommencer l'étape 7.

- [ ] **Étape 8 : Consommation, Wi-Fi actif (Majid, critère 6)**

`docs/BANC.md` §8.3, sonde sur la batterie retenue. Majid choisit l'instrument :
- **testeur USB**, s'il y en a un, entre la batterie et le câble USB de la sonde ;
- sinon **multimètre en série sur VBUS** : batterie débranchée ; USB-C de la sonde débranché pendant toute la mesure ; câble de la batterie ouvert, son GND sur `TP−`, borne mA (calibre 200 mA, courant continu) sur son VBUS, COM sur la broche `5V` de la sonde ; puis batterie branchée (entrée 10 A si le courant saute ou si la session tombe).

L'agent lance `python3 tools/hotte_udp.py session hotte-sonde.local --duree 60`, Majid tape `motif krona 400` dans la console du générateur, lit le courant moyen (attendu : de l'ordre de 100 mA, spec §8.7) et le dit à l'agent avec l'instrument. Avec le multimètre, Majid débranche ensuite la batterie, retire le multimètre et le câble ouvert, puis rebranche la batterie sur l'USB-C de la sonde.

- [ ] **Étape 9 : Dix minutes de `krona` (Majid et l'agent)**

`docs/BANC.md` §8.4.
1. Sonde sur la batterie, à l'emplacement de test (générateur sur le Mac, console ouverte ; fil de masse en place). 10 s après le dernier branchement de la batterie, l'agent relance `python3 tools/hotte_udp.py session hotte-sonde.local --duree 10` : la session doit s'ouvrir.
2. L'agent lance : `python3 tools/hotte_udp.py enregistre hotte-sonde.local banc-wifi-krona "capture tout" "seuils 1 19000" --duree 660`, et dans un second terminal `tail -f logs/live.log`.
3. Dès que le premier terminal affiche `n=... reponse id=3 fin ok « seuils 1 19000 »`, Majid tape `motif krona 4500` dans la console du générateur. Il regarde défiler `motif krona trame <index>` jusqu'à `motif krona fini : 4500 trames` (10 min 3 s).
4. Pendant ce temps, `live.log` affiche toutes les 5 s une ligne `... udp <a.b.c.d> banc-wifi-krona en cours <s> s : <n> lignes (...) ; n : 0 trou(s), 0 perdue(s), 0 recul(s) ; ... ; sonde : receptions ..., debord 0, lignes_perdues 0, sautes 0, rep 0, json_perdus 0 ; H1 : 0 rejetee(s), ...`. Si `lignes_perdues` ou `debord` monte, l'agent le note avec l'heure.
5. L'enregistrement s'arrête seul à 660 s et affiche le chemin de `logs/AAAA-MM-JJ-hhmm-banc-wifi-krona.jsonl`.

- [ ] **Étape 10 : Verdicts du critère 4 (l'agent)**

Lancer : `python3 tools/json_check.py --jsonl logs/<fichier>.jsonl | tail -3`
Attendu : « ... ligne(s) machine, 0 erreur(s) ... n : T trou(s) (P ligne(s) perdue(s), X %) » avec **X < 0.100**.
Lancer : `python3 tools/banc.py logs/<fichier>.jsonl krona --n 4500 --decalage-us <décalage retenu au §7, variante 1>`
Attendu : « compteurs de la sonde pendant la capture : debord +0, lignes_perdues +0, sautes +0 » (zéro perte côté sonde) ; « critere 1 : 4500 / 4500 trames decodees au bit pres (100.0 %) » si le Wi-Fi n'a rien perdu (une ligne perdue en route fait manquer sa trame : c'est le trou de `n` mesuré par `json_check.py`).
Lancer : `python3 -c "import json,sys; r=[json.loads(l)['l']['wifi']['rssi_dbm'] for l in open(sys.argv[1]) if json.loads(l)['l'].get('bloc')=='ip']; print('RSSI', min(r), 'a', max(r), 'dBm sur', len(r), 'releves')" logs/<fichier>.jsonl`
Attendu : `RSSI <min> a <max> dBm sur <k> releves`, k d'environ 22 (un `reseau` toutes les 30 s, plus celui de `json 1`).
**Critère 4 rempli** si : hausses de `debord`, `lignes_perdues`, `sautes` toutes à 0, **et** trous de `n` sous 0,1 %. Sinon : `docs/BANC.md` §8.4, dernier paragraphe (rapprocher du point d'accès, recommencer ; noter RSSI et `wifi.pertes`).

- [ ] **Étape 11 : Noter les résultats (l'agent)**

Dans `docs/BANC.md` §7, remplir :
- « Montage et firmwares », ligne `21` : date, où est la sonde (`boîtier` ou `plaque`), les deux **OL** (boîtier), `fw` de la sonde et du générateur, ports ;
- « Critère 4 : Wi-Fi (tâche 21) » : `| <date> | <emplacement en mots, sans adresse> | <min> à <max> | 10 min 3 s | +0 / +0 / +0 (ou les hausses) | <T> trou(s), <P> ligne(s), <X> % | OK ou ECHEC |` ;
- « Critère 6 », ligne `batterie, Wi-Fi actif (tâche 21)` : l'instrument (testeur USB ou multimètre) et le courant ;
- « Batterie USB : 30 min en Wi-Fi (tâche 21) » : date, modèle, fichier, boots, `up_s` de début et de fin, plus grand trou de `rx_ms`, verdict (une ligne par batterie essayée) ;
- « Conclusions » : une ligne sur la tenue du Wi-Fi (RSSI, pertes) et une sur la batterie retenue.

Dans `docs/RECONNAISSANCE.md`, « Installation et instruments », ligne « Batterie USB : modèle ; essai de 30 min, sonde en Wi-Fi, sans coupure (date, résultat) » : `<modèle> ; <date> : 30 min en Wi-Fi sans coupure, OK (BANC.md §7)`, ou le refus.

Ni SSID, ni adresse, ni clé. Relire le diff avant le commit : `git diff docs/BANC.md docs/RECONNAISSANCE.md`.

- [ ] **Étape 12 : Commit**

```bash
git add docs/BANC.md docs/RECONNAISSANCE.md
git commit -m "Consigner le critere 4 du banc, la batterie et la consommation en Wi-Fi" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
