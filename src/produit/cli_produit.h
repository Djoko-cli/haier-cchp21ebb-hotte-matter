// Copie de src/cli.h de la sonde (commit 1cb2c7c) (adapte : produit)
#pragma once
#include <stdint.h>

void cliBegin();
void cliPoll();  // lignes de l'USB ; redemarrage differe ('reboot')
// Ligne recue d'une session reseau etablie (net_udp_thread.cpp) : meme
// aiguillage que l'USB, liste blanche du produit comprise (7.5).
void cliRunRemote(uint8_t origin, char *line, bool tooLong);
