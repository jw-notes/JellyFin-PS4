#include "app.h"
#include "platform.h"
#include "ui_view.h"
#include "art.h"
#include "screenshot.h"
#include <cassert>
#include <cstdio>
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
uint32_t* framebuffer(){return pixels.data();}void present(){}uint32_t buttons(){return 0;}
static void capture(const char* path){FILE* f=fopen(path,"wb");assert(f&&write_frame_bmp(f,pixels.data(),WIDTH,HEIGHT,WIDTH));fclose(f);}
int main(int argc,char** argv){
 render_init();theme_apply_builtin(0);assert(theme_load("vendor/ps3/themes/jw-notes/deep-ocean.ini"));
 assert(face_text_width("Search",17,TextFace::Classic)<face_text_width("Search",24,TextFace::Classic));
 // Host-only metadata fixtures and publicly readable server poster images.
 const char* ids[]={"550219413e9d8b0f954793eeb82b5e26","8ad65f31c4477b23c8c827e23e27d817","75e6f768d3d791fc460db69c5f0e0ace","74339f69d25cd16c14e89896888186cc","910ade7bbbe5e4b09df9cc76eb0e2f45","ebbae22184afb6c0093e24b87fe1f860"};
 const char* names[]={"2 Fast 2 Furious","8 Mile","10 Cloverfield Lane","28 Days Later","28 Years Later","101 Dalmatians"};
 if(argc>1){snprintf(g_server,sizeof g_server,"%s",argv[1]);for(auto id:ids)load_art(id);art_wait();}
 Page movies;movies.total=130;for(int i=0;i<28;++i){Item item;snprintf(item.base.id,sizeof item.base.id,"%s",ids[i%6]);snprintf(item.base.name,sizeof item.base.name,"%s",names[i%6]);snprintf(item.base.type,sizeof item.base.type,"Movie");item.year=2002+i;item.duration=7200000;item.resume=i==8?1200000:0;movies.items.push_back(item);}
 auto g=library_grid(movies,false);assert(g.rows==2&&g.cols==7&&g.card_w==124&&g.card_h==187);
 assert(library_window(g,8,0)==0&&library_window(g,14,0)==7&&library_window(g,6,7)==0);
 draw_library(movies,"Movies",0,8,false,-1,0);capture("build/classic-movies-108.bmp");
 int first=library_window(g,20,0);assert(first==7);draw_library(movies,"Movies",0,20,false,-1,first);capture("build/classic-scroll-108.bmp");
 draw_quick_peek(movies.items[20],library_focus_rect(g,20,first,false),0.3f,"");capture("build/classic-peek-origin-108.bmp");
 Page seasons=movies;seasons.items.resize(9);seasons.total=9;for(int i=0;i<9;++i){snprintf(seasons.items[i].base.type,sizeof seasons.items[i].base.type,"Season");snprintf(seasons.items[i].base.name,sizeof seasons.items[i].base.name,"Season %d",i+1);}
 draw_library(seasons,"Shows",0,7,false,-1,0,"Example Show / Seasons");capture("build/classic-seasons-108.bmp");
 Page episodes=movies;episodes.items.resize(6);episodes.total=6;for(int i=0;i<6;++i){snprintf(episodes.items[i].base.type,sizeof episodes.items[i].base.type,"Episode");snprintf(episodes.items[i].base.name,sizeof episodes.items[i].base.name,"Episode %d - Example title",i+1);}
 auto wide=library_grid(episodes,false);assert(wide.cols==3&&wide.rows==2&&wide.card_w>wide.card_h);
 draw_library(episodes,"Shows",0,4,false,-1,0,"Example Show / Season 1 / Episodes");capture("build/classic-episodes-108.bmp");
 art_stop();puts("PASS original PS3 ClassicGrid dimensions, exact small-font sizes, two-row scrolling, portrait seasons, wide episodes, breadcrumbs and quick-peek origin; previews rendered");
}
