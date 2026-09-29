#pragma once
// ===========================================================================
//  Coeur du produit, tenu par main_produit.cpp (tache loop seulement) :
//  automate, pilote de ligne, surveillance, reglages en NVS.
// ===========================================================================
#include "hotte_etat.h"
#include "pilote_ligne.h"
#include "surveillance.h"
#if PILOTE_SIMULE
#include "pilote_simule.h"
#endif

hotte::Automate &produitAutomate();
ligne::Pilote &produitPilote();
surv::Surveillance &produitSurveillance();
// Jeux de reglages hors bornes lus en NVS au demarrage, remplaces par les
// defauts : annonces a chaque ouverture de session machine.
uint8_t produitHorsBornes();
// 'hotte regle' : reglages deja valides, appliques puis enregistres en NVS.
// false : NVS en echec (appliques quand meme).
bool produitReglerAutomate(const hotte::Params &p);
bool produitReglerSurveillance(const surv::Params &p);
// Ecrit tout de suite ce qui attend en NVS (derniere_v, etat_pub, temp_max) :
// avant 'reboot' et 'decommission' (4.4).
void produitEcrireEnAttente();
// Essai de 24 h ('essai alim on|off', USB) : autorise 'radio rafale' a
// distance ; s'efface seul apres 72 h de fonctionnement cumule.
bool produitEssai();
bool produitReglerEssai(bool on);
uint32_t produitEssaiResteMin();
#if PILOTE_SIMULE
sim::PiloteSimule &produitSimu();
// Reglages de la hotte simulee : appliques au pilote et a l'automate (mode,
// lecture annexe, delai moteur), enregistres en NVS (simu).
bool produitReglerSimu(const sim::Reglages &r);
// Etat de la hotte simulee au prochain demarrage, une fois (NVS simu_dem).
bool produitSimuApresDemarrage(const sim::EtatHotte &e);
// Endurance (banc B11) : un appui du panneau toutes les periodeS secondes,
// dans un cycle qui revient a eteinte, lampe eteinte ; 0 : arret. Pas en NVS.
void produitSimuAuto(uint32_t periodeS);
uint32_t produitSimuAutoS();
#endif
