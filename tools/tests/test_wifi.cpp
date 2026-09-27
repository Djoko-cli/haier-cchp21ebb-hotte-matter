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
