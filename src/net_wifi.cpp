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
