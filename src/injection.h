#pragma once
// ===========================================================================
//  Injection sur la ligne D, voie 1 (docs/SPEC-RECONNAISSANCE.md 8.4)
//
//  RMT en emission sur GPIO7 (etage d'injection, docs/WIRING.md section 6 :
//  GPIO7 haut = bus tire bas), attente du silence, surveillance des fronts,
//  arret sur collision, evenement 'injection'. Garde-fous purs :
//  injection_regles.h. Tout se passe dans la tache loop.
// ===========================================================================
#include "injection_regles.h"
#include "json_out.h"

// Parametres de l'avenant et etage declare monte (NVS, reglages.h). Premier
// appel : encodeur RMT cree (false : injection impossible). Appels suivants
// ('injection monte', 'injection regle') : valeurs mises a jour ; etage non
// monte : injection desarmee. Une demande deja acceptee garde ses valeurs.
bool injectionBegin(const inj::Params &p, bool montee);
// Demande analysee (inj::analyser) : Refus::Aucun si elle est acceptee (elle
// part des que le bus se tait, evenement 'injection' ensuite, vers l'origine
// jsonOrigin() de la commande avec son id) ; sinon le refus (non montee, non
// armee, delai minimal ; une injection deja en attente compte comme delai).
inj::Refus injectionDemander(const inj::Demande &d, uint32_t id, const char *cmd, uint32_t nowMs);
void injectionPoll();                      // attente de silence, emission, surveillance, evenement
void injectionJson(jsonp::Writer &w, uint32_t nowMs);   // bloc etat.injection
inj::Etat &injectionEtat();
const inj::Params &injectionParams();
// Ajouts : armement ('injection on' : refuse si l'etage n'est pas declare
// monte) et desarmement ('injection off') ; le desarmement seul apres
// arme_max_s est annonce en log 'injection'.
inj::Refus injectionArmer(uint32_t nowMs);
void injectionDesarmer();
