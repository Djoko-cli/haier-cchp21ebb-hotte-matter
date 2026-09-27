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
