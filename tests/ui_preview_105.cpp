#include "app.h"
#include "platform.h"
#include "ui_view.h"
#include "art.h"
#include "screenshot.h"
#include <vector>
#include <cassert>
#include <stdio.h>
#include <string.h>
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
uint32_t* framebuffer(){return pixels.data();}void present(){}uint32_t buttons(){return 0;}
static void capture(const char* name){FILE*f=fopen(name,"wb");assert(f);assert(write_frame_bmp(f,pixels.data(),WIDTH,HEIGHT,WIDTH));assert(!fclose(f));}
int main(int argc,char**argv){
 render_init();theme_apply_builtin(0);assert(theme_load("vendor/ps3/themes/jw-notes/deep-ocean.ini"));
 // Host-only visual fixtures. These are not compiled into the installed app.
 const char* ids[]={"550219413e9d8b0f954793eeb82b5e26","8ad65f31c4477b23c8c827e23e27d817","75e6f768d3d791fc460db69c5f0e0ace","74339f69d25cd16c14e89896888186cc","910ade7bbbe5e4b09df9cc76eb0e2f45","ebbae22184afb6c0093e24b87fe1f860"};
 if(argc>1){snprintf(g_server,sizeof g_server,"%s",argv[1]);for(const auto& id:ids)load_art(id);}
 const char* titles[]={"2 Fast 2 Furious","8 Mile","10 Cloverfield Lane","28 Days Later","28 Years Later","101 Dalmatians"};
 art_wait();
 Page library;library.total=6;
 for(int i=0;i<6;i++){Item item;snprintf(item.base.id,sizeof item.base.id,"%s",ids[i]);snprintf(item.base.name,sizeof item.base.name,"%s",titles[i]);snprintf(item.base.type,sizeof item.base.type,"Movie");item.overview="Local rendering preview using artwork from the connected library. The installed application displays the title, synopsis, media versions and track choices returned by your Jellyfin server.";item.duration=7200000;item.resume=i==1?1800000:0;library.items.push_back(item);}
 draw_library(library,"Movies",0,1,false);capture("build/library-preview-105.bmp");
 library.items[1].cast="Eminem, Kim Basinger, Brittany Murphy";library.items[1].year=2002;library.items[1].rating="R";library.items[1].genres="Drama";
 draw_quick_peek(library.items[1],1,false,1.0f);capture("build/quick-peek-preview-105.bmp");
 draw_library(library,"Movies",0,1,false);draw_quick_peek(library.items[1],1,false,0.3f);capture("build/quick-peek-turn-preview-105.bmp");
 draw_detail(library.items[1],"1080p H264","English - AAC stereo","Off",0);capture("build/detail-preview-105.bmp");
 Page home;home.total=3;for(int i=0;i<3;i++){auto item=library.items[i];snprintf(item.base.name,sizeof item.base.name,"%s",i==0?"Movies":i==1?"Shows":"Music");snprintf(item.base.type,sizeof item.base.type,"CollectionFolder");home.items.push_back(item);}
 draw_library(home,"Libraries",0,1,true);capture("build/home-preview-105.bmp");
 const uint32_t colors[]={0xffff0000,0xff00ff00,0xff0000ff,0xffffffff};FILE*f=fopen("build/screenshot-encoder-test.bmp","wb");assert(f&&write_frame_bmp(f,colors,2,2,2));fclose(f);
 art_stop();
 puts("PASS shared library/detail/home renderer previews and reused BMP screenshot writer");
}
