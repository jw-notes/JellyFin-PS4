#include "keyboard_view.h"
#include "platform.h"
#include "app.h"
#include <stdio.h>
#include <vector>
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
uint32_t* framebuffer(){return pixels.data();}void present(){}uint32_t buttons(){return 0;}
static void save(const char* filename){FILE*f=fopen(filename,"wb");fprintf(f,"P6\n%d %d\n255\n",WIDTH,HEIGHT);for(auto v:pixels){unsigned char b[]={(unsigned char)(v>>16),(unsigned char)(v>>8),(unsigned char)v};fwrite(b,1,3,f);}fclose(f);}
int main(){render_init();theme_apply_builtin(0);theme_load("vendor/ps3/themes/jw-notes/deep-ocean.ini");KeyboardEditor editor("http://192.168.1.141:8097",KeyboardType::Url);draw_keyboard("Jellyfin server URL (including port)",editor,false);save("build/keyboard-url-102.ppm");editor.symbols=true;draw_keyboard("Jellyfin server URL (including port)",editor,false);save("build/keyboard-symbols-102.ppm");puts("PASS rendered URL keyboard and symbol-page previews");}
