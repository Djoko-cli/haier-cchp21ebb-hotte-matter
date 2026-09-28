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
#include <stdio.h>
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
// chiffres hexadecimaux (cle WPA brute). Refus : true, et la raison dans out
// (si out) : la regle en cause et, pour un octet interdit, sa position et sa
// valeur, jamais le mot de passe. Un copier-coller y met parfois un caractere
// invisible (espace insecable C2 A0, apostrophe typographique E2 80 99) :
// 28/09, un mot de passe colle refuse, accepte une fois tape a la main. Pur :
// teste sur l'hote (tools/tests/test_wifi.cpp).
inline bool netWifiRefus(const char *ssid, const char *mdp, char *out, size_t n) {
  if (out && n) out[0] = 0;
  if (!ssid || !mdp) {
    if (out) snprintf(out, n, "identifiants absents");
    return true;
  }
  const size_t ns = strlen(ssid), nm = strlen(mdp);
  if (ns < 1 || ns > 32) {
    if (out) snprintf(out, n, "ssid de %u caracteres (1 a 32)", (unsigned)ns);
    return true;
  }
  for (size_t i = 0; i < ns; i++) {
    const unsigned char c = (unsigned char)ssid[i];
    if (c <= 0x20 || c > 0x7E) {
      if (out)
        snprintf(out, n, "ssid : octet 0x%02X en position %u (ASCII imprimable sans espace seulement)", c,
                 (unsigned)(i + 1));
      return true;
    }
  }
  if ((nm > 0 && nm < 8) || nm > 64) {
    if (out) snprintf(out, n, "mot de passe de %u caracteres (8 a 63, ou 64 hexa)", (unsigned)nm);
    return true;
  }
  for (size_t i = 0; i < nm; i++) {
    const unsigned char c = (unsigned char)mdp[i];
    if (c < 0x20 || c > 0x7E) {
      if (out)
        snprintf(out, n,
                 "mot de passe : octet 0x%02X en position %u (ASCII imprimable seulement ; colle, il peut porter "
                 "un caractere invisible : le taper a la main)",
                 c, (unsigned)(i + 1));
      return true;
    }
  }
  for (size_t i = 0; nm == 64 && i < nm; i++) {
    const unsigned char c = (unsigned char)mdp[i];
    const bool hexa = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!hexa) {
      if (out) snprintf(out, n, "mot de passe de 64 caracteres : cle brute, chiffres hexadecimaux seulement");
      return true;
    }
  }
  return false;
}

inline bool netWifiValides(const char *ssid, const char *mdp) { return !netWifiRefus(ssid, mdp, nullptr, 0); }
