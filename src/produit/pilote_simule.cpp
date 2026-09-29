// ===========================================================================
//  Pilote de ligne simule : voir pilote_simule.h. Pur (teste sur l'hote).
// ===========================================================================
#include "pilote_simule.h"

namespace sim {

using hotte::Marche;
using hotte::Moteur;
using hotte::Touche;

bool etatHotteValide(const EtatHotte &e) {
  if (e.marche == Marche::Inconnue) return false;
  if (e.marche == Marche::Eteinte && e.moteur != Moteur::Arret) return false;
  return !(e.marche == Marche::Prolongee && e.moteur == Moteur::Arret);
}

const char *texte(Inconnues i) { return i == Inconnues::Direct ? "direct" : "rien"; }

bool reglagesValides(const Reglages &r) {
  return (uint8_t)r.mode <= (uint8_t)hotte::ModeEtat::AppuisSeuls && r.latenceMs <= 5000 && r.periodeMs >= 100 &&
         r.periodeMs <= 60000 && r.trameMs >= 10 && r.trameMs <= 1000 && r.annexeMs >= 50 && r.annexeMs <= 5000 &&
         r.prolongeeMs >= 60000 && r.prolongeeMs <= 3600000 && (uint8_t)r.inconnues <= (uint8_t)Inconnues::Direct &&
         r.delaiMoteurMs >= 1000 && r.delaiMoteurMs <= 60000;
}

uint16_t versMot(const EtatHotte &e) {
  return (uint16_t)(0x5300 | (uint16_t)e.marche | (uint16_t)e.moteur << 2 | (uint16_t)(e.lampe ? 1 : 0) << 4);
}

bool depuisMot(uint16_t v, EtatHotte *out) {
  if ((v & 0xFFE0) != 0x5300) return false;
  EtatHotte e;
  e.marche = (Marche)(v & 3);
  e.moteur = (Moteur)(v >> 2 & 3);
  e.lampe = v >> 4 & 1;
  if (!etatHotteValide(e)) return false;
  *out = e;
  return true;
}

PiloteSimule::PiloteSimule(const Reglages &r) : r_(r) {}

void PiloteSimule::regler(const Reglages &r) { r_ = r; }

void PiloteSimule::poserHotte(const EtatHotte &e, uint32_t nowMs) {
  h_ = e;
  if (h_.marche == Marche::Prolongee) debutProlMs_ = nowMs;
}

void PiloteSimule::demarrer(uint32_t nowMs) {
  for (Tache &t : taches_) t.actif = false;
  tete_ = nb_ = 0;
  enCours_ = false;
  luUnEtat_ = false;
  lu_ = hotte::Etat();
  changementVu_ = false;
  changementLuMs_ = nowMs;
  dernierEtatMs_ = derniereAnnexeMs_ = nowMs;
}

void PiloteSimule::pousser(const ligne::Evenement &e) {
  if (nb_ == kFile) {
    d_.evenementsPerdus++;
    return;
  }
  file_[(tete_ + nb_) % kFile] = e;
  nb_++;
}

void PiloteSimule::pousserTrame(ligne::Trame::Origine o, uint8_t octet, uint32_t nowMs) {
  if (nbT_ == kTrames) {  // la plus ancienne cede
    teteT_ = (uint8_t)((teteT_ + 1) % kTrames);
    nbT_--;
  }
  ligne::Trame &t = trames_[(teteT_ + nbT_) % kTrames];
  t.tMs = nowMs;
  t.origine = o;
  t.n = 1;
  t.octets[0] = octet;
  nbT_++;
}

bool PiloteSimule::trame(ligne::Trame *out) {
  if (!nbT_) return false;
  *out = trames_[teteT_];
  teteT_ = (uint8_t)((teteT_ + 1) % kTrames);
  nbT_--;
  return true;
}

bool PiloteSimule::evenement(ligne::Evenement *out) {
  if (!nb_) return false;
  *out = file_[tete_];
  tete_ = (uint8_t)((tete_ + 1) % kFile);
  nb_--;
  return true;
}

void PiloteSimule::planifier(const Tache &t) {
  for (Tache &s : taches_)
    if (!s.actif) {
      s = t;
      s.actif = true;
      return;
    }
  d_.evenementsPerdus++;  // agenda plein : n'arrive pas avec un appui a la fois
}

void PiloteSimule::noterLu(const hotte::Etat &e, uint32_t nowMs) {
  if (luUnEtat_ && e.moteur != lu_.moteur) {
    changementVu_ = true;
    changementLuMs_ = nowMs;
  }
  lu_ = e;
  luUnEtat_ = true;
}

void PiloteSimule::emettreEtat(uint32_t nowMs) {
  dernierEtatMs_ = nowMs;
  ligne::Evenement ev;
  ev.tMs = nowMs;
  if (illisibles_) {
    illisibles_--;
    ev.type = ligne::Evenement::Anomalie;
    ev.codeAnomalie = ligne::kAnoIllisible;
    d_.anomalies++;
    pousser(ev);
    return;
  }
  pousserTrame(ligne::Trame::Carte,
               (uint8_t)((uint8_t)h_.marche << 4 | (uint8_t)h_.moteur << 2 | (h_.lampe ? 1 : 0)), nowMs);
  ev.type = ligne::Evenement::EtatLu;
  ev.etat.marche = h_.marche;
  ev.etat.moteur = h_.moteur;
  ev.etat.lampe = h_.lampe;
  ev.etat.lampeConnue = true;
  ev.etat.confiance = hotte::Confiance::Confirme;
  ev.etat.source = hotte::Source::Fil;
  ev.etat.depuisMs = ev.etat.luMs = nowMs;
  noterLu(ev.etat, nowMs);
  d_.etatsLus++;
  pousser(ev);
}

void PiloteSimule::emettreAnnexe(uint32_t nowMs) {
  derniereAnnexeMs_ = nowMs;
  ligne::Evenement ev;
  ev.type = ligne::Evenement::EtatLu;
  ev.tMs = nowMs;
  ev.etat.marche = Marche::Inconnue;  // la lecture annexe ne lit pas le voyant marche
  ev.etat.moteur = h_.moteur;
  ev.etat.lampe = h_.lampe;
  ev.etat.lampeConnue = true;
  ev.etat.confiance = hotte::Confiance::Deduit;
  ev.etat.source = hotte::Source::Annexe;
  ev.etat.depuisMs = ev.etat.luMs = nowMs;
  noterLu(ev.etat, nowMs);
  d_.lecturesAnnexe++;
  pousser(ev);
}

void PiloteSimule::changement(uint32_t nowMs) {
  if (r_.mode != hotte::ModeEtat::AppuisSeuls) emettreEtat(nowMs);
}

void PiloteSimule::carte(Touche t, uint32_t nowMs) {
  const EtatHotte avant = h_;
  if (t == Touche::Lumiere) {
    h_.lampe = !h_.lampe;
  } else if (t == Touche::Marche) {
    switch (h_.marche) {
      case Marche::Eteinte: h_.marche = Marche::Armee; break;
      case Marche::Armee:
        if (h_.moteur == Moteur::Arret) {
          h_.marche = Marche::Eteinte;
        } else {
          h_.marche = Marche::Prolongee;
          debutProlMs_ = nowMs;
        }
        break;
      case Marche::Prolongee:
        if (r_.inconnues == Inconnues::Direct) {
          h_.marche = Marche::Eteinte;
          h_.moteur = Moteur::Arret;
        }
        break;
      case Marche::Inconnue: break;
    }
  } else {
    const Moteur v = hotte::moteurDe(t);
    switch (h_.marche) {
      case Marche::Eteinte: break;  // une vitesse sans marche ne fait rien
      case Marche::Armee: h_.moteur = h_.moteur == v ? Moteur::Arret : v; break;
      case Marche::Prolongee:
        if (h_.moteur == v) {
          h_.marche = Marche::Armee;
          h_.moteur = Moteur::Arret;
        } else if (r_.inconnues == Inconnues::Direct) {
          h_.moteur = v;
        }
        break;
      case Marche::Inconnue: break;
    }
  }
  if (h_.marche != avant.marche || h_.moteur != avant.moteur || h_.lampe != avant.lampe) changement(nowMs);
}

bool PiloteSimule::changeMoteur(Touche t) const {
  if (t == Touche::Marche || t == Touche::Lumiere) return false;
  const hotte::Transition tr = hotte::appliquer(lu_, t);
  return !tr.observee || tr.apres.moteur != lu_.moteur;  // dans le doute, oui
}

ligne::Admission PiloteSimule::appuyer(Touche t, uint32_t idAppui, uint32_t nowMs) {
  Tache fin;
  fin.type = kFin;
  fin.id = idAppui;
  if (hs_) {
    fin.t = nowMs;
    fin.res = hotte::ResultatAppui::Erreur;
    planifier(fin);
    return ligne::Admission::Acceptee;
  }
  if (enCours_) return ligne::Admission::Occupee;
  // Garde-fou 6 : jamais "marche" moteur tournant, ni sans etat lu.
  if (t == Touche::Marche && (!luUnEtat_ || lu_.moteur != Moteur::Arret)) {
    d_.refusGarde++;
    return ligne::Admission::Refusee;
  }
  // Garde-fou 7 : delai entre deux changements du moteur vus sur le fil.
  if (changeMoteur(t) && changementVu_ && nowMs - changementLuMs_ < r_.delaiMoteurMs) {
    d_.refusGarde++;
    return ligne::Admission::Refusee;
  }
  enCours_ = true;
  if (delais_) {  // pas de silence dans attente_max_ms : rien emis
    delais_--;
    d_.delais++;
    fin.t = nowMs + 1;
    fin.res = hotte::ResultatAppui::Delai;
    planifier(fin);
    return ligne::Admission::Acceptee;
  }
  d_.appuisEmis++;
  fin.t = nowMs + r_.trameMs;
  bool recue = true, echo = true;
  if (perdus_) {
    perdus_--;
    recue = echo = false;
  } else if (collisions_) {
    collisions_--;
    d_.collisions++;
    fin.res = hotte::ResultatAppui::Collision;
    echo = false;  // trame coupee : pas d'echo lisible
    recue = collisionPrend_;
  }
  if (sansEffet_) {
    sansEffet_--;
    recue = false;
  }
  if (refuse_) recue = false;
  if (echo) {
    Tache e;
    e.type = kEcho;
    e.t = fin.t;
    e.touche = t;
    e.origine = hotte::Origine::Module;
    planifier(e);
  }
  planifier(fin);
  if (recue) {
    Tache c;
    c.type = kCarte;
    c.t = fin.t + r_.latenceMs;
    c.touche = t;
    planifier(c);
  }
  return ligne::Admission::Acceptee;
}

void PiloteSimule::appuiPanneau(Touche t, uint32_t nowMs) {
  Tache e;
  e.type = kEcho;
  e.t = nowMs + r_.trameMs;
  e.touche = t;
  e.origine = hotte::Origine::Panneau;
  planifier(e);
  Tache c;
  c.type = kCarte;
  c.t = e.t + r_.latenceMs;
  c.touche = t;
  planifier(c);
}

void PiloteSimule::coupure(uint32_t nowMs) {
  for (Tache &t : taches_)
    if (t.actif && t.type == kCarte) t.actif = false;  // la carte repart de zero
  h_ = EtatHotte();
  changement(nowMs);
}

void PiloteSimule::horsService(bool on, uint32_t nowMs) {
  if (hs_ == on) return;
  hs_ = on;
  ligne::Evenement ev;
  ev.type = ligne::Evenement::Service;
  ev.tMs = nowMs;
  ev.enService = !on;
  pousser(ev);
}

void PiloteSimule::poll(uint32_t nowMs) {
  // Taches echues, dans l'ordre du temps.
  for (;;) {
    Tache *prochaine = nullptr;
    for (Tache &t : taches_)
      if (t.actif && (int32_t)(nowMs - t.t) >= 0 && (!prochaine || (int32_t)(t.t - prochaine->t) < 0))
        prochaine = &t;
    if (!prochaine) break;
    const Tache t = *prochaine;
    prochaine->actif = false;
    if (t.type == kCarte) {
      carte(t.touche, t.t);
    } else if (t.type == kEcho) {
      pousserTrame(t.origine == hotte::Origine::Module ? ligne::Trame::Module : ligne::Trame::Panneau,
                   (uint8_t)(0xA0 | (uint8_t)t.touche), t.t);
      ligne::Evenement ev;
      ev.type = ligne::Evenement::Appui;
      ev.tMs = t.t;
      ev.touche = t.touche;
      ev.origine = t.origine;
      d_.appuisVus++;
      pousser(ev);
    } else {
      enCours_ = false;
      ligne::Evenement ev;
      ev.type = ligne::Evenement::FinAppui;
      ev.tMs = t.t;
      ev.idAppui = t.id;
      ev.resultat = t.res;
      pousser(ev);
    }
  }
  // Fin de la marche prolongee (15 min) : eteinte, ou armee (Q2).
  if (h_.marche == Marche::Prolongee && nowMs - debutProlMs_ >= r_.prolongeeMs) {
    h_.marche = r_.finArmee ? Marche::Armee : Marche::Eteinte;
    h_.moteur = Moteur::Arret;
    changement(nowMs);
  }
  if (r_.mode == hotte::ModeEtat::Repete && nowMs - dernierEtatMs_ >= r_.periodeMs) emettreEtat(nowMs);
  if (r_.annexe && nowMs - derniereAnnexeMs_ >= r_.annexeMs) emettreAnnexe(nowMs);
}

}  // namespace sim
