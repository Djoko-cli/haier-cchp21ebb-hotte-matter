#pragma once
// ===========================================================================
//  Couche Matter du produit (docs/SPEC-PRODUIT.md 3, 4.4, 8)
//
//  Un noeud sans pont : EP1 "Hotte" (Fan 0x002B, OffLowMedHigh, sans Auto,
//  sous-classe MatterFanHotte), EP2 "Eclairage hotte" (On/Off Light).
//  Recettes reprises de benq (src/matter_bridge.cpp au commit c58a506) :
//  boite d'intentions remplie par les rappels (tache CHIP) et videe par la
//  tache loop, echo propre ecarte, reflets par updateAttributeVal sous
//  TryLockChipStack, ordres ignores au demarrage, identite posee a chaque
//  demarrage, Thread seul, role et plafond des abonnements en NVS.
//
//  Toutes les fonctions sont pour la tache loop.
// ===========================================================================
#include <Arduino.h>

#include "hotte_etat.h"
#include "json_out_produit.h"

// Cree les endpoints et demarre la pile. etatInitial : l'etat initial de
// l'automate (5.7), publie par le premier reflet (parade du piege de la NVS).
void matterHotteBegin(const hotte::Etat &etatInitial, hotte::Moteur derniereVitesse);
// Vide la boite d'intentions : ordres regroupes vers l'automate (ignores
// pendant ignore_demarrage_ms), puis reflet de l'etat publie s'il attend.
void matterHottePoll(hotte::Automate &a, uint32_t now);
// Etat a publier (Action::Publier de l'automate) : reflete au prochain tour
// ou la boite est vide et le verrou de la pile libre.
void matterHottePublier(const hotte::Etat &e);
// Derniere vitesse de l'automate : copie atomique lue par la tache CHIP pour
// la substitution de On (3.3, regle 3).
void matterHotteDerniere(hotte::Moteur m);

bool matterHotteMisEnService();
void matterHotteDecommission();  // efface la cle H1 d'abord (7.7)
void matterHotteStatut(Print &out);
void matterHotteEtat(jsonp::MatterEtat *out, uint32_t now);
void matterHotteRadio(jsonp::RadioCompteurs *out);
// Journal des ecritures brutes de Maison (banc Matter, B2) : 'matter journal'.
void matterHotteJournal(Print &out);

// Reglages (NVS produit), USB seulement ; au prochain demarrage sauf tx et derniere.
bool matterReglerMed(uint32_t v, bool *saved);     // 0 routeur (defaut), 1 MED des l'init
bool matterReglerTx(uint32_t dbm, bool *saved);    // 8..20, applique tout de suite
bool matterReglerMaxint(uint32_t s, bool *saved);  // 0, ou 10..3600
bool matterReglerDerniere(bool on, bool *saved);   // substitution de On (banc B2)

// Verrou OpenThread pour le transport reseau (net_udp_thread.cpp) : false si
// la pile n'est pas demarree ou si le verrou n'est pas libre dans totalMs
// (0 : sans attente). Sous ce verrou, AUCUN appel Matter/CHIP.
bool matterOtTryLock(uint32_t totalMs);
void matterOtUnlock();
