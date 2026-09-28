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

// Raison du refus : la regle en cause et, pour un octet interdit, sa position
// et sa valeur, jamais le mot de passe (28/09 : un mot de passe colle refuse
// sans dire pourquoi, accepte une fois tape a la main).
static void testRaison() {
  char t[200];
  VERIF(!netWifiRefus("maison", "motdepasse", t, sizeof(t)) && t[0] == 0);
  VERIF(netWifiRefus("maison", "1234567", t, sizeof(t)) && strstr(t, "7 caracteres"));
  VERIF(netWifiRefus("maison", std::string(65, 'a').c_str(), t, sizeof(t)) && strstr(t, "65 caracteres"));
  VERIF(netWifiRefus("maison", std::string(64, 'p').c_str(), t, sizeof(t)) && strstr(t, "hexa"));
  // Espace insecable (Option-Espace sur un Mac francais) : octets C2 A0.
  VERIF(netWifiRefus("maison", "secretXYZ\xc2\xa0", t, sizeof(t)) && strstr(t, "octet 0xC2 en position 10"));
  VERIF(!strstr(t, "secretXYZ"));
  VERIF(netWifiRefus("maison", "secretXYZ\t", t, sizeof(t)) && strstr(t, "octet 0x09 en position 10"));
  VERIF(netWifiRefus("caf\xc3\xa9", "motdepasse", t, sizeof(t)) && strstr(t, "ssid") && strstr(t, "octet 0xC3 en position 4"));
  VERIF(netWifiRefus(std::string(33, 's').c_str(), "motdepasse", t, sizeof(t)) && strstr(t, "33 caracteres"));
  // Sans tampon : le verdict seul.
  VERIF(netWifiRefus("maison", "1234567", nullptr, 0));
  VERIF(!netWifiRefus("maison", "12345678", nullptr, 0));
}

int main() {
  testSsid();
  testMdp();
  testRaison();
  return bilan("test_wifi");
}
