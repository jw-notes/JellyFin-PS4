// Host-only image preview for the same CPU renderer used by the PS4 binary.
#include "app.h"
#include "platform.h"
#include <vector>
#include <stdio.h>
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
uint32_t* framebuffer(){return pixels.data();}
void present(){}
uint32_t buttons(){return 0;}
int main(){render_init();theme_apply_builtin(0);theme_load("vendor/ps3/themes/jw-notes/deep-ocean.ini");clear(g_theme.bg);rect(0,0,WIDTH,92,g_theme.panel);text(48,25,"JellyFin [PS4]",g_theme.accent,32);text(380,30,"Renderer validation",g_theme.text);text(48,120,"Original PS3 font assets and theme palette on the PS4 renderer",g_theme.text_dim);rect(48,185,1184,260,g_theme.panel_hi);text(80,215,"Movies / TV / Music",g_theme.text,32);text(80,270,u8"Latin: caf\u00e9 \u2014 \u0395\u03bb\u03bb\u03b7\u03bd\u03b9\u03ba\u03ac \u2014 \u041a\u0438\u0440\u0438\u043b\u043b\u0438\u0446\u0430",g_theme.accent);text(80,315,u8"\u65e5\u672c\u8a9e\u306e\u30c6\u30b9\u30c8",g_theme.text);text(80,380,"Search   Continue watching   Favourites   Settings",g_theme.text_dim);text(48,650,"X: open    O: back    Options: settings",g_theme.text_dim);
 FILE*f=fopen("build/renderer-preview.ppm","wb");if(!f)return 1;fprintf(f,"P6\n%d %d\n255\n",WIDTH,HEIGHT);for(auto v:pixels){unsigned char rgb[]={static_cast<unsigned char>(v>>16),static_cast<unsigned char>(v>>8),static_cast<unsigned char>(v)};fwrite(rgb,1,3,f);}fclose(f);puts("PASS PS4 CPU renderer preview generated");}
