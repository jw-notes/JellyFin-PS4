#include "language.h"
#include "app.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifndef HOST_TEST
#include <orbis/SystemService.h>
#endif

// Strings the PS3 catalog does not have, in LANG_JA, LANG_PT_BR, LANG_DE,
// LANG_FR and LANG_ES order. A format keeps its conversions, in order, in
// every language.
struct Entry{const char* en;const char* text[LANG__COUNT-1];};
static const Entry ps4_catalog[]={
#include "language_ps4.inc"
};

const char* T(const char* en){
 if(!en||!*en)return en;
 const char* found=tr(en);if(found!=en)return found;
 const i18n_lang lang=i18n_current();if(lang==LANG_EN)return en;
 for(const auto& e:ps4_catalog)if(!strcmp(e.en,en)){const char* t=e.text[lang-1];return t&&*t?t:en;}
 return en;
}
std::string TF(const char* fmt,...){
 char out[512];va_list args;va_start(args,fmt);vsnprintf(out,sizeof out,T(fmt),args);va_end(args);return out;
}

// The console's language (the same numbers as the PS3's SYSUTIL_LANG_*, plus
// the PS4's Latin American Spanish and Canadian French).
static i18n_lang system_language(){
#ifdef HOST_TEST
 return LANG_EN;
#else
 int32_t v=1;if(sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG,&v)<0)v=1;
 if(v==20)return LANG_ES;if(v==22)return LANG_FR;
 return i18n_from_system(v);
#endif
}
void language_apply(){
 const int code=i18n_from_code(settings.language.c_str());
 i18n_set(code>=0?(i18n_lang)code:system_language());
}
// Auto, then each language in the PS3's order.
void language_step(int dir){
 int pref=settings.language=="auto"?LANG_PREF_AUTO:i18n_from_code(settings.language.c_str());if(pref<0)pref=LANG_PREF_AUTO;
 pref=(pref+dir+LANG_PREF_COUNT)%LANG_PREF_COUNT;
 settings.language=pref==LANG_PREF_AUTO?"auto":i18n_code((i18n_lang)pref);language_apply();
}
std::string language_label(){
 const int code=i18n_from_code(settings.language.c_str());
 return code>=0?i18n_native_name((i18n_lang)code):std::string(T("Auto"))+" ("+i18n_native_name(system_language())+")";
}
