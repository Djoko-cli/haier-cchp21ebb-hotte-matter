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

}  // namespace hotte
