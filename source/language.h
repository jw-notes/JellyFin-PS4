#pragma once
// The interface language, as on the PS3 (vendor/ps3/source/i18n): every
// string the app writes is English at its use and looked up by that text.
// The PS3's catalog answers first; strings only the PS4 has come from
// language.cpp's own table. A string with no entry stays English.
//
// Only the app's own words are translated: titles, names and overviews from
// the server are shown as the server sends them.
#include "../vendor/ps3/source/i18n/i18n.h"
#include <string>

// The text in the current language.
const char* T(const char* en);
// T(fmt), then printf-formatted: TF("Latest %s", name).
std::string TF(const char* fmt,...) __attribute__((format(printf,1,2)));

// Settings > Language: "auto" follows the console, or a code ("ja", "pt-BR").
void language_apply();
void language_step(int dir);
std::string language_label();
