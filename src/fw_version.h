// Copie de benq-screenbar-halo-matter@c58a506 : src/fw_version.h (adapte : version 0.1.0 de la sonde, commentaires)
#pragma once

// ===========================================================================
//  Version du firmware (C et C++ : app_desc.c l'inclut aussi)
//
//  FW_VERSION, FW_GIT_REV et FW_ENV viennent de platformio.ini
//  (build_src_flags, donc pour src/ seulement) : la version a la main, la
//  revision par tools/git_rev.py, le nom de l'env PlatformIO par $PIOENV. Les
//  valeurs ci-dessous ne servent qu'a une compilation hors PlatformIO.
//
//  0.1.0 : premiere sonde de la ligne D (protocole compagnon v1, profil hotte).
//
//  FW_VERSION_FULL ("0.1.0-903414e", "-dirty" si l'arbre differe du commit)
//  est la version affichee au demarrage et par 'info', et celle du descripteur
//  d'application (esp_app_desc.version, src/app_desc.c), que hello rapporte
//  en fw et fw_desc.
// ===========================================================================

#ifndef FW_VERSION
#define FW_VERSION "0.1.0"
#endif
#ifndef FW_GIT_REV
#define FW_GIT_REV "nogit"
#endif
#ifndef FW_ENV
#define FW_ENV "inconnu"  // env PlatformIO ("sonde", "generateur"), rapporte par hello.env
#endif
#define FW_VERSION_FULL FW_VERSION "-" FW_GIT_REV
