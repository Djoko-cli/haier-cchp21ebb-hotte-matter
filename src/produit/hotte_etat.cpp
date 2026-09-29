// ===========================================================================
//  Automate de la hotte : voir hotte_etat.h. Pur (teste sur l'hote).
// ===========================================================================
#include "hotte_etat.h"

#include <string.h>

namespace hotte {

namespace {

struct Champ {
  const char *nom;
  uint32_t Params::*m;
  uint32_t lo, hi;
};
// Bornes du 4.2. Ordre : celui du tableau des parametres.
const Champ kChamps[kNbParams] = {
    {"delai_moteur_ms", &Params::delaiMoteurMs, 1000, 60000},
    {"entre_appuis_ms", &Params::entreAppuisMs, 100, 5000},
    {"confirmation_ms", &Params::confirmationMs, 200, 5000},
    {"fraicheur_ms", &Params::fraicheurMs, 200, 120000},
    {"lissage_calme_ms", &Params::lissageCalmeMs, 300, 2000},
    {"lissage_plafond_ms", &Params::lissagePlafondMs, 1000, 10000},
    {"ordre_calme_ms", &Params::ordreCalmeMs, 50, 1000},
    {"sequence_max_ms", &Params::sequenceMaxMs, 5000, 60000},
    {"attente_etat_ms", &Params::attenteEtatMs, 1000, 60000},
    {"prolongee_max_ms", &Params::prolongeeMaxMs, 900000, 3600000},
    {"ignore_demarrage_ms", &Params::ignoreDemarrageMs, 0, 10000},
    {"sonde_vitesse", &Params::sondeVitesse, 0, 1},
};

const Champ *champ(const char *nom) {
  if (!nom) return nullptr;
  for (const Champ &c : kChamps)
    if (!strcmp(c.nom, nom)) return &c;
  return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
//  Etat, table, textes
// ---------------------------------------------------------------------------

bool memeEtat(const Etat &a, const Etat &b) {
  return a.marche == b.marche && a.moteur == b.moteur && a.lampe == b.lampe && a.lampeConnue == b.lampeConnue;
}

Etat etatMiseSousTension(uint32_t nowMs) {
  Etat e;
  e.marche = Marche::Eteinte;
  e.moteur = Moteur::Arret;
  e.lampe = false;
  e.lampeConnue = true;
  e.confiance = Confiance::Deduit;
  e.source = Source::Deduit;
  e.depuisMs = nowMs;
  e.luMs = 0;
  return e;
}

Moteur moteurDe(Touche t) {
  switch (t) {
    case Touche::V1: return Moteur::V1;
    case Touche::V2: return Moteur::V2;
    case Touche::V3: return Moteur::V3;
    default: return Moteur::Arret;
  }
}

Touche toucheDe(Moteur m) {
  switch (m) {
    case Moteur::V1: return Touche::V1;
    case Moteur::V2: return Touche::V2;
    case Moteur::V3: return Touche::V3;
    default: return Touche::Marche;
  }
}

Transition appliquer(const Etat &avant, Touche t) {
  Transition r{avant, true};
  Etat &a = r.apres;
  if (t == Touche::Lumiere) {  // la lampe bascule, independante de la marche (etape -1)
    if (avant.lampeConnue) a.lampe = !avant.lampe;
    return r;
  }
  const Moteur m = avant.moteur;
  if (t == Touche::Marche) {
    switch (avant.marche) {
      case Marche::Eteinte:
        a.marche = Marche::Armee;
        return r;
      case Marche::Armee:
        // Moteur arrete : eteinte. Moteur en Vx : marche prolongee (observe en
        // V3 ; suppose identique en V1 et V2, code comme observe : Q30).
        a.marche = m == Moteur::Arret ? Marche::Eteinte : Marche::Prolongee;
        return r;
      case Marche::Prolongee:  // inconnu (Q2)
        a.marche = Marche::Inconnue;
        r.observee = false;
        return r;
      case Marche::Inconnue:
        r.observee = false;
        return r;
    }
    return r;
  }
  const Moteur v = moteurDe(t);
  switch (avant.marche) {
    case Marche::Eteinte:  // une vitesse sans marche ne fait rien (etape -1)
      return r;
    case Marche::Armee:
      // Vitesse active : armee, moteur arrete (observe en V3 ; Q30). Autre : Vy.
      a.moteur = m == v ? Moteur::Arret : v;
      return r;
    case Marche::Prolongee:
      if (m == v) {  // vitesse active : armee (observe, 31,2 s)
        a.marche = Marche::Armee;
        a.moteur = Moteur::Arret;
        return r;
      }
      a.marche = Marche::Inconnue;  // autre vitesse pendant la prolongation : inconnu (Q2)
      a.moteur = v;
      r.observee = false;
      return r;
    case Marche::Inconnue:
      if (m != Moteur::Arret && m == v) {  // armee comme prolongee vont a armee
        a.marche = Marche::Armee;
        a.moteur = Moteur::Arret;
        return r;
      }
      if (m != Moteur::Arret) a.moteur = v;  // armee : Vy ; prolongee : inconnu
      r.observee = false;
      return r;
  }
  return r;
}

const char *texte(Touche t) {
  static const char *const k[] = {"marche", "lumiere", "v1", "v2", "v3"};
  return (uint8_t)t < 5 ? k[(uint8_t)t] : "?";
}
const char *texte(Moteur m) {
  static const char *const k[] = {"arret", "v1", "v2", "v3"};
  return (uint8_t)m < 4 ? k[(uint8_t)m] : "?";
}
const char *texte(Marche m) {
  static const char *const k[] = {"eteinte", "armee", "prolongee", "inconnue"};
  return (uint8_t)m < 4 ? k[(uint8_t)m] : "?";
}
const char *texte(Confiance c) {
  static const char *const k[] = {"confirme", "deduit", "presume", "inconnu"};
  return (uint8_t)c < 4 ? k[(uint8_t)c] : "?";
}
const char *texte(Source s) {
  static const char *const k[] = {"fil", "annexe", "deduit", "nvs"};
  return (uint8_t)s < 4 ? k[(uint8_t)s] : "?";
}
const char *texte(Origine o) {
  static const char *const k[] = {"panneau", "module", "inconnue"};
  return (uint8_t)o < 3 ? k[(uint8_t)o] : "?";
}
const char *texte(ModeEtat m) {
  static const char *const k[] = {"repete", "changements", "appuis"};
  return (uint8_t)m < 3 ? k[(uint8_t)m] : "?";
}
const char *texte(ResultatAppui r) {
  static const char *const k[] = {"ok", "collision", "delai", "garde", "erreur"};
  return (uint8_t)r < 5 ? k[(uint8_t)r] : "?";
}
// ---------------------------------------------------------------------------
//  Reglages
// ---------------------------------------------------------------------------

bool paramsValides(const Params &p) {
  for (const Champ &c : kChamps)
    if (p.*c.m < c.lo || p.*c.m > c.hi) return false;
  return p.lissageCalmeMs <= p.lissagePlafondMs;
}

const char *nomParam(uint8_t i) { return i < kNbParams ? kChamps[i].nom : nullptr; }

uint32_t valeurParam(const Params &p, uint8_t i) { return i < kNbParams ? p.*kChamps[i].m : 0; }

bool bornesParam(const char *nom, uint32_t *lo, uint32_t *hi) {
  const Champ *c = champ(nom);
  if (!c) return false;
  *lo = c->lo;
  *hi = c->hi;
  return true;
}

Reglage reglerParam(Params *p, const char *nom, uint32_t v) {
  const Champ *c = champ(nom);
  if (!c) return Reglage::NomInconnu;
  Params q = *p;
  q.*c->m = v;
  if (!paramsValides(q)) return Reglage::HorsBornes;
  *p = q;
  return Reglage::Ok;
}

bool chargerParams(const void *octets, size_t n, Params *out) {
  Params p;
  if (octets && n == sizeof(Params)) memcpy(&p, octets, sizeof(Params));
  const bool ok = octets && n == sizeof(Params) && paramsValides(p);
  *out = ok ? p : Params();
  return ok;
}

// ===========================================================================
//  Automate (5.1 a 5.9)
// ===========================================================================

namespace {

bool vitesse(Touche t) { return t == Touche::V1 || t == Touche::V2 || t == Touche::V3; }

bool fiable(Confiance c) { return c == Confiance::Confirme || c == Confiance::Deduit; }

// Confiance apres une deduction par la table (appui vu sur le fil).
Confiance deduite(Confiance avant, bool observee) {
  if (avant == Confiance::Inconnu) return Confiance::Inconnu;
  if (!observee || avant == Confiance::Presume) return Confiance::Presume;
  return Confiance::Deduit;
}

// Un etat complet lu s'accorde avec e sur ce que les deux connaissent.
bool accordFil(const Etat &lu, const Etat &e) {
  if (lu.moteur != e.moteur) return false;
  if (lu.marche != Marche::Inconnue && e.marche != Marche::Inconnue && lu.marche != e.marche) return false;
  return !lu.lampeConnue || !e.lampeConnue || lu.lampe == e.lampe;
}

// Une lecture annexe ne juge que ce que la touche change : la lampe ou le moteur.
bool accordAnnexe(const Etat &lu, const Etat &e, Touche t) {
  if (t == Touche::Lumiere) return lu.lampeConnue && e.lampeConnue && lu.lampe == e.lampe;
  return lu.moteur == e.moteur;
}

}  // namespace

const char *texte(CibleVentilo c) {
  static const char *const k[] = {"aucune", "eteint", "v1", "v2", "v3", "derniere"};
  return (uint8_t)c < 6 ? k[(uint8_t)c] : "?";
}
const char *texte(Issue i) {
  static const char *const k[] = {"ok", "annulee", "echec", "abandon", "remplacee"};
  return (uint8_t)i < 5 ? k[(uint8_t)i] : "?";
}
const char *texte(Cause c) {
  static const char *const k[] = {"aucune", "non_confirme", "sans_lecture", "collision", "garde", "pilote", "duree"};
  return (uint8_t)c < kNbCauses ? k[(uint8_t)c] : "?";
}
const char *texte(Canal c) { return c == Canal::App ? "app" : "matter"; }
const char *texte(Sujet s) { return s == Sujet::Lampe ? "lampe" : "ventilo"; }
const char *texte(Evt e) {
  static const char *const k[] = {"marche_inconnue", "prolongee_perimee", "ligne_muette", "deduction_dementie",
                                  "sonde_vitesse"};
  return (uint8_t)e < 5 ? k[(uint8_t)e] : "?";
}

// ---------------------------------------------------------------------------
//  Automate : entrees
// ---------------------------------------------------------------------------

Automate::Automate(const Params &p) : p_(p) {}

void Automate::configurer(const Params &p) { p_ = p; }

void Automate::configurerLigne(ModeEtat mode, bool annexe) {
  mode_ = mode;
  annexe_ = annexe;
  muette_ = false;
}

void Automate::demarrer(Demarrage cause, const Etat *etatPublieNvs, Moteur derniereVitesse, uint32_t nowMs) {
  const Params p = p_;
  const ModeEtat m = mode_;
  const bool a = annexe_;
  *this = Automate(p);  // tout repart de zero, sauf les reglages et la ligne
  mode_ = m;
  annexe_ = a;
  demarrageMs_ = nowMs;
  derniere_ = derniereVitesse == Moteur::Arret ? Moteur::V2 : derniereVitesse;
  if (cause == Demarrage::MiseSousTension) {
    // H-coupure (5.7, Q20) : la hotte revient toujours eteinte. Rien n'est
    // lu : "marche" attend un arret du moteur lu (regle 3).
    etat_ = etatMiseSousTension(nowMs);
  } else if (etatPublieNvs) {
    etat_ = *etatPublieNvs;  // la hotte a pu continuer a tourner
    etat_.confiance = Confiance::Presume;
    etat_.source = Source::Nvs;
    etat_.depuisMs = nowMs;
    etat_.luMs = 0;
  } else {
    etat_ = Etat();
    etat_.depuisMs = nowMs;
  }
  moteurLu_ = etat_.moteur;
  changementMoteurMs_ = nowMs;  // prudence : un changement du moteur a pu preceder le demarrage
  aPublier_ = etat_.confiance != Confiance::Inconnu;
}

void Automate::ordreVentilo(CibleVentilo c, uint32_t idOrdre, Canal canal, uint32_t nowMs) {
  if (c == CibleVentilo::Aucune) return;
  const bool eteint = c == CibleVentilo::Eteint;
  Moteur m = Moteur::Arret;
  if (c == CibleVentilo::Derniere) m = derniere_;
  else if (c == CibleVentilo::V1) m = Moteur::V1;
  else if (c == CibleVentilo::V2) m = Moteur::V2;
  else if (c == CibleVentilo::V3) m = Moteur::V3;
  if (ov_.actif && eteint == cibleEteint_ && m == cibleMoteur_) {
    // Meme cible (5.5) : rien ne repart ; le nouvel ordre reprend la sequence.
    Action a;
    a.type = Action::FinSequence;
    a.sujet = Sujet::Ventilo;
    a.idOrdre = ov_.id;
    a.canal = ov_.canal;
    a.issue = Issue::Remplacee;
    a.dureeMs = nowMs - ov_.recuMs;
    a.appuis = ov_.appuis;
    pousser(a);
    c_.remplacees++;
    ov_.id = idOrdre;
    ov_.canal = canal;
    ov_.recuMs = nowMs;
    ov_.appuis = 0;
    return;
  }
  finOrdre(Sujet::Ventilo, Issue::Remplacee, Cause::Aucune, nowMs);
  ov_ = Ordre();
  ov_.actif = true;
  ov_.id = idOrdre;
  ov_.canal = canal;
  ov_.recuMs = nowMs;
  cibleEteint_ = eteint;
  cibleMoteur_ = m;
  reprise_[(int)Sujet::Ventilo] = Reprise();
}

void Automate::ordreLampe(bool allumee, uint32_t idOrdre, Canal canal, uint32_t nowMs) {
  if (ol_.actif && allumee == cibleLampe_) {
    Action a;
    a.type = Action::FinSequence;
    a.sujet = Sujet::Lampe;
    a.idOrdre = ol_.id;
    a.canal = ol_.canal;
    a.issue = Issue::Remplacee;
    a.dureeMs = nowMs - ol_.recuMs;
    a.appuis = ol_.appuis;
    pousser(a);
    c_.remplacees++;
    ol_.id = idOrdre;
    ol_.canal = canal;
    ol_.recuMs = nowMs;
    ol_.appuis = 0;
    return;
  }
  finOrdre(Sujet::Lampe, Issue::Remplacee, Cause::Aucune, nowMs);
  ol_ = Ordre();
  ol_.actif = true;
  ol_.id = idOrdre;
  ol_.canal = canal;
  ol_.recuMs = nowMs;
  cibleLampe_ = allumee;
  reprise_[(int)Sujet::Lampe] = Reprise();
}

void Automate::surEtatLu(const Etat &lu, uint32_t nowMs) {
  const bool sur = fiable(etat_.confiance);
  Etat n = etat_;
  n.moteur = lu.moteur;
  if (lu.marche != Marche::Inconnue) {
    n.marche = lu.marche;
  } else if (!sur || (lu.moteur == Moteur::Arret && etat_.marche == Marche::Prolongee) ||
             (lu.moteur != Moteur::Arret && etat_.marche == Marche::Eteinte)) {
    // Lecture annexe : le voyant marche n'est pas lu, et la deduction ne tient plus.
    n.marche = Marche::Inconnue;
  }
  if (lu.lampeConnue) {
    n.lampe = lu.lampe;
    n.lampeConnue = true;
  } else if (!sur) {
    n.lampeConnue = false;
  }
  // Confirme : etat complet lu, ou lecture partielle qui s'accorde avec un
  // etat deja confirme sans appui ni anomalie depuis. Sinon, Deduit : le
  // moteur et la lampe sont lus, le voyant marche vient de la table.
  const bool complet = lu.marche != Marche::Inconnue && lu.lampeConnue;
  const bool garde = etat_.confiance == Confiance::Confirme && memeEtat(n, etat_);
  n.confiance = complet || garde ? Confiance::Confirme : Confiance::Deduit;
  n.source = lu.source;
  n.depuisMs = nowMs;
  n.luMs = nowMs;
  if (lu.moteur != moteurLu_) {
    changementMoteurMs_ = nowMs;
    moteurLu_ = lu.moteur;
  }
  arretLu_ = lu.moteur == Moteur::Arret;  // lu apres le dernier appui : un appui remet a faux
  muette_ = false;
  if (vol_.actif && vol_.fini) {
    if (lu.source == Source::Fil) {
      vol_.filLu = true;
      vol_.fil = lu;
      vol_.filMs = nowMs;
    } else {
      vol_.annexeLue = true;
      vol_.annexeEtat = lu;
      vol_.annexeMs = nowMs;
    }
  }
  const bool notre = vol_.actif && accordFil(lu, vol_.attendu) && !accordFil(lu, vol_.avant);
  changer(n, notre ? Origine::Module : Origine::Inconnue, nowMs);
}

void Automate::surAppui(Touche t, Origine o, uint32_t nowMs) {
  arretLu_ = false;
  const Transition tr = appliquer(etat_, t);
  if (o == Origine::Module) {  // echo de notre propre trame
    if (vol_.actif && !vol_.echo && t == vol_.touche) vol_.echo = true;
    // Appuis seuls : le voyant marche ne se connait que par l'echo et la
    // table. Ailleurs, l'etat lu tranchera ; le moteur et la lampe viennent
    // toujours d'une lecture.
    if (mode_ != ModeEtat::AppuisSeuls || tr.apres.marche == etat_.marche) return;
    Etat n = etat_;
    n.marche = tr.apres.marche;
    n.confiance = deduite(etat_.confiance, tr.observee);
    n.source = Source::Deduit;
    n.depuisMs = nowMs;
    if (!tr.observee) c_.transitionsInconnues++;
    changer(n, Origine::Module, nowMs);
    return;
  }
  c_.appuisPanneau++;
  Etat n = tr.apres;
  n.confiance = deduite(etat_.confiance, tr.observee);
  n.source = Source::Deduit;
  n.depuisMs = nowMs;
  if (!tr.observee) c_.transitionsInconnues++;
  changer(n, Origine::Panneau, nowMs);
  annulerTout(nowMs);  // le panneau est prioritaire (5.6)
  pousserPublier();    // publie tout de suite (C2)
}

void Automate::surFinAppui(uint32_t idAppui, ResultatAppui r, uint32_t nowMs) {
  if (!vol_.actif || vol_.fini || vol_.id != idAppui) return;
  vol_.fini = true;
  vol_.res = r;
  vol_.finMs = nowMs;
  if (r == ResultatAppui::Erreur) piloteOk_ = false;
}

void Automate::surAnomalie(uint16_t, uint32_t nowMs) {
  c_.anomalies++;
  arretLu_ = false;
  if (fiable(etat_.confiance)) etat_.confiance = Confiance::Presume;  // valeurs publiees inchangees
  if (vol_.actif) {
    vol_.anomalie = true;
    vol_.anomalieMs = nowMs;
  }
}

void Automate::surPilote(bool enService, uint32_t) { piloteOk_ = enService; }

// ---------------------------------------------------------------------------
//  Automate : sorties
// ---------------------------------------------------------------------------

void Automate::pousser(const Action &a) {
  if (nb_ == kFile) {
    c_.actionsPerdues++;
    return;
  }
  file_[(tete_ + nb_) % kFile] = a;
  nb_++;
}

void Automate::pousserPublier() {
  aPublier_ = false;
  if (etat_.confiance == Confiance::Inconnu) return;  // rien de nouveau n'est publie (5.1)
  for (uint8_t i = 0; i < nb_; i++)
    if (file_[(tete_ + i) % kFile].type == Action::Publier) return;  // il publiera l'etat du moment
  Action a;
  a.type = Action::Publier;
  pousser(a);
}

void Automate::changer(const Etat &n, Origine o, uint32_t nowMs) {
  const Etat avant = etat_;
  etat_ = n;
  if (n.moteur != Moteur::Arret && fiable(n.confiance)) derniere_ = n.moteur;  // toutes origines
  if (n.moteur != avant.moteur) changementMoteurMs_ = nowMs;
  if (memeEtat(avant, n)) return;
  Action a;
  a.type = Action::Changement;
  a.avant = avant;
  a.etat = n;
  a.origine = o;
  pousser(a);
  aPublier_ = true;
}

void Automate::finOrdre(Sujet s, Issue i, Cause c, uint32_t nowMs) {
  Ordre &o = s == Sujet::Ventilo ? ov_ : ol_;
  if (!o.actif) return;
  Action a;
  a.type = Action::FinSequence;
  a.sujet = s;
  a.idOrdre = o.id;
  a.canal = o.canal;
  a.issue = i;
  a.cause = c;
  a.dureeMs = nowMs - o.recuMs;
  a.appuis = o.appuis;
  pousser(a);
  switch (i) {
    case Issue::Ok: c_.reussies++; break;
    case Issue::Annulee: c_.annulees++; break;
    case Issue::Remplacee: c_.remplacees++; break;
    case Issue::Abandon: c_.abandons++; break;
    case Issue::Echec: c_.echecs[(uint8_t)c < kNbCauses ? (uint8_t)c : 0]++; break;
  }
  o = Ordre();
  if (vol_.actif && vol_.sujet == s) vol_.orphelin = true;  // l'appui en vol finit, puis plus rien
  reprise_[(int)s] = Reprise();
  if (i != Issue::Remplacee) pousserPublier();  // l'etat reel, en fin de sequence
}

void Automate::annulerTout(uint32_t nowMs) {
  finOrdre(Sujet::Ventilo, Issue::Annulee, Cause::Aucune, nowMs);
  finOrdre(Sujet::Lampe, Issue::Annulee, Cause::Aucune, nowMs);
}

void Automate::verifierTemps(uint32_t nowMs) {
  // Ligne muette, en mode repete seulement : aucune repetition depuis fraicheur_ms.
  if (mode_ == ModeEtat::Repete && fiable(etat_.confiance) && !muette_) {
    const uint32_t ref = etat_.luMs ? etat_.luMs : demarrageMs_;
    if (nowMs - ref >= p_.fraicheurMs) {
      etat_.confiance = Confiance::Presume;
      muette_ = true;
      arretLu_ = false;
      c_.lignesMuettes++;
      c_.anomalies++;
      Action a;
      a.type = Action::Evenement;
      a.evt = Evt::LigneMuette;
      pousser(a);
    }
  }
  // Marche prolongee sans lecture depuis prolongee_max_ms : l'etat publie ne
  // change pas, mais rien ne se planifie plus sur lui (5.9).
  if (etat_.marche == Marche::Prolongee && fiable(etat_.confiance)) {
    const uint32_t ref = etat_.luMs ? etat_.luMs : etat_.depuisMs;
    if (nowMs - ref >= p_.prolongeeMaxMs) {
      etat_.confiance = Confiance::Presume;
      c_.prolongeePerimee++;
      Action a;
      a.type = Action::Evenement;
      a.evt = Evt::ProlongeePerimee;
      pousser(a);
    }
  }
  if (vol_.actif && vol_.fini) {
    Cause c = Cause::Aucune;
    const Verdict v = juger(nowMs, &c);
    if (v != Verdict::Attendre) appliquerVerdict(v, c, nowMs);
  }
  const Sujet sujets[2] = {Sujet::Ventilo, Sujet::Lampe};
  for (const Sujet s : sujets) {
    Ordre &o = s == Sujet::Ventilo ? ov_ : ol_;
    if (!o.actif) continue;
    if (nowMs - o.recuMs >= p_.sequenceMaxMs) {
      finOrdre(s, Issue::Abandon, Cause::Duree, nowMs);
    } else if (o.bloqueMs && nowMs - o.bloqueMs >= p_.attenteEtatMs) {
      finOrdre(s, Issue::Abandon, piloteOk_ ? Cause::SansLecture : Cause::Pilote, nowMs);
    }
  }
}

Automate::Verdict Automate::juger(uint32_t nowMs, Cause *cause) const {
  const Vol &v = vol_;
  *cause = Cause::Aucune;
  if (v.res == ResultatAppui::Delai) return Verdict::NonPris;  // rien emis
  if (v.res == ResultatAppui::Garde) {
    *cause = Cause::Garde;
    return Verdict::Echec;
  }
  if (v.res == ResultatAppui::Erreur) {
    *cause = Cause::Pilote;
    return Verdict::Echec;
  }
  const bool collision = v.res == ResultatAppui::Collision;
  const uint32_t conf = p_.confirmationMs;
  const bool marche = v.touche == Touche::Marche;
  // 1. Etat complet lu sur le fil apres la fin de l'appui.
  if (v.filLu) {
    if (accordFil(v.fil, v.attendu)) return Verdict::Reussi;
    if (!accordFil(v.fil, v.avant)) return Verdict::Autre;  // un appui du panneau
    // Inchange, lu au-dela du delai de reponse de la carte, apres toute anomalie.
    if (v.filMs - v.finMs >= conf && (!v.anomalie || (int32_t)(v.filMs - v.anomalieMs) > 0))
      return Verdict::NonPris;
  }
  // 2. Lecture annexe apres la fin de l'appui (moteur ou lampe).
  if (v.annexeLue && !marche) {
    if (accordAnnexe(v.annexeEtat, v.attendu, v.touche)) return Verdict::Reussi;
    if (!accordAnnexe(v.annexeEtat, v.avant, v.touche)) return Verdict::Autre;
    if (v.annexeMs - v.finMs >= conf) return Verdict::NonPris;
  }
  // 3. Sans lecture decisive.
  if (mode_ == ModeEtat::AppuisSeuls && marche) {
    // Le voyant marche ne se lit pas : l'echo de notre trame et la table.
    // Jamais de nouvel essai pour "marche" dans ce mode.
    if (!collision && v.echo) return Verdict::Reussi;
    if (nowMs - v.finMs < conf) return Verdict::Attendre;
    *cause = collision ? Cause::Collision : Cause::SansLecture;
    return Verdict::Echec;
  }
  if (mode_ == ModeEtat::Changements && !collision && v.echo && !v.filLu && !v.anomalie &&
      nowMs - v.finMs >= conf)
    return Verdict::NonPris;  // notre trame est passee, la carte n'a rien emis
  // La lecture suivante doit venir : la repetition (repete) ou la lecture
  // annexe (moteur, lampe). Au-dela, echec.
  uint32_t limite = conf;
  if (mode_ == ModeEtat::Repete) limite += p_.fraicheurMs;
  else if (annexe_ && !marche) limite += conf;
  if (nowMs - v.finMs < limite) return Verdict::Attendre;
  *cause = collision ? Cause::Collision : (v.filLu || v.annexeLue) ? Cause::NonConfirme : Cause::SansLecture;
  return Verdict::Echec;
}

void Automate::appliquerVerdict(Verdict v, Cause c, uint32_t nowMs) {
  const Vol vol = vol_;
  vol_ = Vol();
  if (vol.res != ResultatAppui::Delai) {
    aEmis_ = true;
    dernierFinMs_ = vol.finMs;
  }
  const int s = (int)vol.sujet;
  switch (v) {
    case Verdict::Reussi:
      reprise_[s] = Reprise();
      return;
    case Verdict::Autre:  // un autre etat : un appui du panneau (5.6)
      annulerTout(nowMs);
      return;
    case Verdict::Echec:
      if (!vol.orphelin) finOrdre(vol.sujet, Issue::Echec, c, nowMs);
      return;
    case Verdict::NonPris:
      break;
    case Verdict::Attendre:
      return;
  }
  if (vol.orphelin) return;
  if (mode_ == ModeEtat::AppuisSeuls && etat_.marche != vol.avant.marche) {
    Etat n = etat_;  // la deduction de l'echo n'a pas eu lieu
    n.marche = vol.avant.marche;
    changer(n, Origine::Module, nowMs);
  }
  const bool delai = vol.res == ResultatAppui::Delai;
  if (vol.sonde && !delai) {
    // Sonde de vitesse sans effet : la hotte etait eteinte (une vitesse sans
    // marche ne fait rien). La suite : marche, puis la vitesse.
    Etat n = etat_;
    n.marche = Marche::Eteinte;
    n.source = Source::Deduit;
    n.depuisMs = nowMs;
    changer(n, Origine::Module, nowMs);
    Action a;
    a.type = Action::Evenement;
    a.evt = Evt::SondeVitesse;
    pousser(a);
    return;
  }
  const bool permis = delai || !(mode_ == ModeEtat::AppuisSeuls && vol.touche == Touche::Marche);
  if (vol.essai == 0 && permis) {  // un nouvel essai, un seul, l'echec etant prouve (regle 2)
    reprise_[s].actif = true;
    reprise_[s].touche = vol.touche;
    repriseImmediate_ = delai;
    c_.nouveauxEssais++;
    return;
  }
  // Deuxieme echec prouve. Une vitesse depuis "armee" deduite qui ne prend
  // pas : la deduction du voyant marche est dementie.
  if (vitesse(vol.touche) && vol.avant.marche == Marche::Armee && vol.avant.moteur == Moteur::Arret &&
      etat_.confiance != Confiance::Confirme) {
    Etat n = etat_;
    n.marche = Marche::Inconnue;
    changer(n, Origine::Inconnue, nowMs);
    Action a;
    a.type = Action::Evenement;
    a.evt = Evt::DeductionDementie;
    pousser(a);
  }
  finOrdre(vol.sujet, Issue::Echec, Cause::NonConfirme, nowMs);
}

bool Automate::delaiMoteurOk(uint32_t nowMs) const { return nowMs - changementMoteurMs_ >= p_.delaiMoteurMs; }

Automate::Plan Automate::planVentilo(uint32_t nowMs, Touche *t, bool *sonde) {
  *sonde = false;
  if (!ov_.actif) return Plan::Rien;
  const Etat &e = etat_;
  if (!fiable(e.confiance) || !piloteOk_) return Plan::Bloque;
  if (cibleEteint_) {
    if (e.moteur != Moteur::Arret) {  // vitesse active, arret du moteur lu, puis marche
      *t = toucheDe(e.moteur);
      return delaiMoteurOk(nowMs) ? Plan::Presser : Plan::Attente;
    }
    switch (e.marche) {
      case Marche::Eteinte:
        return Plan::Atteint;
      case Marche::Inconnue: {  // rien sur le voyant marche ; C1 tient (moteur et lampe)
        c_.marcheInconnue++;
        Action a;
        a.type = Action::Evenement;
        a.evt = Evt::MarcheInconnue;
        pousser(a);
        return Plan::Atteint;
      }
      case Marche::Armee:
        if (!arretLu_) return Plan::Bloque;  // regle 3
        *t = Touche::Marche;
        return Plan::Presser;
      case Marche::Prolongee:  // prolongee moteur arrete : incoherent, on attend une lecture
        return Plan::Bloque;
    }
    return Plan::Bloque;
  }
  const Moteur y = cibleMoteur_;
  if (e.moteur == y) return Plan::Atteint;  // prolongee-Vy comprise : elle continue (Q38)
  if (e.moteur != Moteur::Arret) {
    // Prolongee-Vx : vitesse active d'abord (Vx, armee, puis Vy) ; sinon Vy directement.
    *t = e.marche == Marche::Prolongee ? toucheDe(e.moteur) : toucheDe(y);
    return delaiMoteurOk(nowMs) ? Plan::Presser : Plan::Attente;
  }
  switch (e.marche) {
    case Marche::Armee:
      *t = toucheDe(y);
      return delaiMoteurOk(nowMs) ? Plan::Presser : Plan::Attente;
    case Marche::Eteinte:
      if (!arretLu_) return Plan::Bloque;  // regle 3, y compris apres une mise sous tension
      *t = Touche::Marche;
      return Plan::Presser;
    case Marche::Inconnue:  // sonde de vitesse (regle 7), si elle est permise (Q6)
      if (!p_.sondeVitesse || !arretLu_) return Plan::Bloque;
      *t = toucheDe(y);
      *sonde = true;
      return delaiMoteurOk(nowMs) ? Plan::Presser : Plan::Attente;
    case Marche::Prolongee:
      return Plan::Bloque;
  }
  return Plan::Bloque;
}

Automate::Plan Automate::planLampe(Touche *t) const {
  if (!ol_.actif) return Plan::Rien;
  const Etat &e = etat_;
  // Une bascule appuyee a l'aveugle ferait l'inverse une fois sur deux.
  if (!fiable(e.confiance) || !e.lampeConnue || !piloteOk_) return Plan::Bloque;
  if (e.lampe == cibleLampe_) return Plan::Atteint;
  *t = Touche::Lumiere;
  return Plan::Presser;
}

Action Automate::presser(Sujet s, Touche t, bool sonde, uint32_t nowMs) {
  Ordre &o = s == Sujet::Ventilo ? ov_ : ol_;
  Reprise &r = reprise_[(int)s];
  vol_ = Vol();
  vol_.actif = true;
  vol_.sujet = s;
  vol_.touche = t;
  if (++idAppui_ == 0) idAppui_ = 1;
  vol_.id = idAppui_;
  vol_.debutMs = nowMs;
  vol_.avant = etat_;
  if (sonde) {  // hypothese a confirmer : la hotte etait armee
    vol_.sonde = true;
    vol_.attendu = etat_;
    vol_.attendu.marche = Marche::Armee;
    vol_.attendu.moteur = moteurDe(t);
  } else {
    vol_.attendu = appliquer(etat_, t).apres;
  }
  vol_.essai = r.actif && r.touche == t ? 1 : 0;
  r = Reprise();
  repriseImmediate_ = false;
  arretLu_ = false;
  o.appuis++;
  c_.appuisModule++;
  Action a;
  a.type = Action::Appuyer;
  a.touche = t;
  a.idAppui = vol_.id;
  return a;
}

bool Automate::planifier(uint32_t nowMs, Action *out) {
  const Sujet sujets[2] = {Sujet::Ventilo, Sujet::Lampe};  // le ventilateur d'abord
  for (const Sujet s : sujets) {
    Ordre &o = s == Sujet::Ventilo ? ov_ : ol_;
    if (!o.actif) continue;
    if (vol_.actif && !vol_.orphelin && vol_.sujet == s) continue;  // son verdict d'abord
    Touche t = Touche::Marche;
    bool sonde = false;
    const Plan pl = s == Sujet::Ventilo ? planVentilo(nowMs, &t, &sonde) : planLampe(&t);
    if (pl != Plan::Bloque) o.bloqueMs = 0;
    else if (!o.bloqueMs) o.bloqueMs = nowMs ? nowMs : 1;
    if (pl == Plan::Atteint) {
      finOrdre(s, Issue::Ok, Cause::Aucune, nowMs);
      continue;
    }
    if (pl != Plan::Presser || vol_.actif) continue;  // un appui a la fois
    if (aEmis_ && !repriseImmediate_ && nowMs - dernierFinMs_ < p_.entreAppuisMs) continue;
    *out = presser(s, t, sonde, nowMs);
    return true;
  }
  return false;
}

Action Automate::suivante(uint32_t nowMs) {
  if (!nb_) verifierTemps(nowMs);
  if (!nb_) {
    Action a;
    if (planifier(nowMs, &a)) return a;
  }
  if (!nb_ && aPublier_ && !ov_.actif && !ol_.actif) pousserPublier();
  if (!nb_) return Action();
  Action a = file_[tete_];
  tete_ = (uint8_t)((tete_ + 1) % kFile);
  nb_--;
  if (a.type == Action::Publier) a.etat = etat_;  // l'etat du moment
  return a;
}

Diagnostic Automate::diagnostic(uint32_t nowMs) const {
  Diagnostic d;
  d.c = c_;
  d.ventilo = ov_.actif;
  d.cibleEteint = cibleEteint_;
  d.cibleMoteur = cibleMoteur_;
  d.idVentilo = ov_.id;
  d.canalVentilo = ov_.canal;
  d.lampe = ol_.actif;
  d.cibleLampe = cibleLampe_;
  d.idLampe = ol_.id;
  d.canalLampe = ol_.canal;
  d.enVol = vol_.actif;
  d.touche = vol_.touche;
  d.essai = vol_.essai;
  d.volDepuisMs = vol_.debutMs;
  const uint32_t ecoule = nowMs - changementMoteurMs_;
  d.delaiMoteurResteMs = ecoule >= p_.delaiMoteurMs ? 0 : p_.delaiMoteurMs - ecoule;
  d.piloteEnService = piloteOk_;
  d.marcheAutorisee = arretLu_;
  return d;
}

}  // namespace hotte
