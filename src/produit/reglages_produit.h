#pragma once
// ===========================================================================
//  NVS du produit, espace "produit" (docs/SPEC-PRODUIT.md 4.4) : reglages de
//  l'automate et de la surveillance (bornes appliquees au chargement),
//  derniere vitesse, dernier etat publie, maximum de temperature depuis la
//  pose, essai de 24 h, et, dans le build simule, la hotte simulee. Les
//  reglages Matter (med, tx_dbm, maxint, derniere) sont dans matter_hotte.cpp,
//  la cle H1 dans net_udp_thread.cpp. Tache loop seulement.
// ===========================================================================
#include "hotte_etat.h"
#include "surveillance.h"
#if PILOTE_SIMULE
#include "pilote_simule.h"
#endif

// Jeu hors bornes (ou de taille inattendue) : tous ses defauts. Rend le
// nombre de jeux remplaces (0, 1 ou 2).
uint8_t nvsChargerParams(hotte::Params *a, surv::Params *s);
bool nvsSauverParams(const hotte::Params &a);
bool nvsSauverSurveillance(const surv::Params &s);
bool nvsLireDerniere(hotte::Moteur *m);  // faux : absente (V2 par defaut)
bool nvsEcrireDerniere(hotte::Moteur m);
bool nvsLireEtatPub(hotte::Etat *e);     // faux : absent ou mal forme
bool nvsEcrireEtatPub(const hotte::Etat &e);
bool nvsLireTempMax(int32_t *dixiemes);  // faux : absent (aucune pose)
bool nvsEcrireTempMax(int32_t dixiemes);
bool nvsEffacerTempMax();
uint16_t nvsLireEssaiMin();              // minutes restantes ; 0 : pas d'essai
bool nvsEcrireEssaiMin(uint16_t min);    // 0 : efface
#if PILOTE_SIMULE
bool nvsLireSimu(sim::Reglages *r);      // faux : absent ou hors bornes (defauts)
bool nvsSauverSimu(const sim::Reglages &r);
bool nvsPrendreSimuDem(sim::EtatHotte *e);  // lu PUIS efface (une fois)
bool nvsEcrireSimuDem(const sim::EtatHotte &e);
#endif
