// Host-only previews of the 1.09 Classic Home, jump bar, detail page and player
// HUD, rendered with fixture metadata (no server artwork).
#include "app.h"
#include "platform.h"
#include "ui_view.h"
#include "art.h"
#include "screenshot.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "boot_anim.h"
#include "screensaver.h"
void screensaver_preview(int index);
void art_inject(const std::string& key,int w,int h,const std::vector<uint8_t>& rgba);
#include "livetv.h"
#include "livetv_view.h"
#include <chrono>
void livetv_preview(const std::vector<JFChannel>& channels,int selected,bool show_guide);
static std::vector<uint8_t> fake_image(int w,int h,int seed){std::vector<uint8_t> p((size_t)w*h*4);for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto q=&p[((size_t)y*w+x)*4];q[0]=(uint8_t)(x*255/w+seed*40);q[1]=(uint8_t)(y*255/h);q[2]=(uint8_t)(128+seed*20);q[3]=255;}return p;}
template<class F> static double ms_per(F f,int frames){auto t0=std::chrono::steady_clock::now();for(int i=0;i<frames;++i)f();return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count()/frames;}
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
bool keyboard(const std::string&,std::string&,bool,KeyboardType){return false;}
uint32_t* framebuffer(){return pixels.data();}void present(){}uint32_t buttons(){return 0;}
static void capture(const char* path){FILE* f=fopen(path,"wb");assert(f&&write_frame_bmp(f,pixels.data(),WIDTH,HEIGHT,WIDTH));fclose(f);}
static Item item(const char* id,const char* name,const char* type,int year){Item i;snprintf(i.base.id,sizeof i.base.id,"%s",id);snprintf(i.base.name,sizeof i.base.name,"%s",name);snprintf(i.base.type,sizeof i.base.type,"%s",type);i.year=year;return i;}
int main(){
 render_init();theme_apply_builtin(0);assert(theme_load("vendor/ps3/themes/jw-notes/deep-ocean.ini"));
 HomeView home;home.loaded=true;
 HomeRow media;media.title="My Media";media.shape=SHAPE_LIBRARY;for(auto n:{"Movies","Shows","Music","Live TV"}){auto i=item(n,n,"CollectionFolder",0);media.page.items.push_back(i);}home.rows.push_back(media);
 HomeRow cont;cont.title="Continue Watching";cont.shape=SHAPE_WIDE;for(int k=0;k<6;++k){auto i=item(("c"+std::to_string(k)).c_str(),"The Long Example Episode","Episode",2020);i.series="Example Show";i.season=1;i.episode=k+1;i.duration=2700000;i.resume=900000;cont.page.items.push_back(i);}home.rows.push_back(cont);
 HomeRow latest;latest.title="Latest Movies";latest.shape=SHAPE_POSTER;for(int k=0;k<10;++k)latest.page.items.push_back(item(("m"+std::to_string(k)).c_str(),"Example Film","Movie",2010+k));home.rows.push_back(latest);
 home.focus=1;home.rows[1].col=2;home_step(home,true);draw_home(home,-1);capture("build/ui109-home.bmp");
 home.focus=2;home.rows[2].col=5;home_step(home,true);draw_home(home,-1);capture("build/ui109-home-scrolled.bmp");
 Page movies;movies.total=130;for(int k=0;k<28;++k)movies.items.push_back(item("x","Example Film","Movie",2000+k));
 JumpBar jump;jump.show=true;draw_library(movies,"Movies",0,7,false,-1,0,"",&jump);capture("build/ui109-movies.bmp");
 jump.active=true;jump.sel=13;draw_library(movies,"Movies",0,7,false,-1,0,"",&jump);capture("build/ui109-jumpbar.bmp");
 Item film=item("f","Example Film: The Sequel","Movie",2014);film.duration=7020000;film.resume=1800000;film.rating="PG-13";film.community="7.4";film.genres="Action, Adventure, Science Fiction";film.director="Jane Director";
 film.overview="A crew of explorers travel through a wormhole in space in an attempt to ensure humanity's survival. Their journey takes them past the edge of everything they know, where time itself bends.";
 for(int k=0;k<9;++k){Person p;p.id="p"+std::to_string(k);p.name="Actor Name "+std::to_string(k+1);p.role="Character "+std::to_string(k+1);film.people.push_back(p);}
 DetailView view;view.version="1080p H264";view.video="1080p · H264 · SDR";view.audio="English - AAC - Stereo - Default";view.audio_chip="AAC Stereo";view.subtitles="Off";
 draw_detail(film,view);capture("build/ui109-detail.bmp");
 view.row=2;view.col=7;view.cast_first=2;draw_detail(film,view);capture("build/ui109-detail-cast.bmp");
 for(int y=0;y<HEIGHT;++y)rect(0,y,WIDTH,1,(y*40/HEIGHT)<<16|(60+y*80/HEIGHT)<<8|90);
 PlayerOsd osd;osd.title="Example Film: The Sequel";osd.subtitle="2014";osd.audio="AAC Stereo";osd.duration=7020000;osd.position=2460000;osd.started=true;draw_player_osd(osd);capture("build/ui109-player.bmp");
 for(int y=0;y<HEIGHT;++y)rect(0,y,WIDTH,1,(y*40/HEIGHT)<<16|(60+y*80/HEIGHT)<<8|90);
 osd.scrubbing=true;osd.target=3120000;osd.paused=true;draw_player_osd(osd);capture("build/ui109-player-scrub.bmp");
 draw_about(0,0);capture("build/ui109-about.bmp");draw_about(2,120);capture("build/ui109-about-licences.bmp");
 // Boot: the emblem over black, then the handoff over Home at fixed points.
 assert(storage_init(109));boot_begin();assert(boot_active());
 for(int i=0;i<100;++i){boot_pump();usleep(16000);}capture("build/ui109-boot-emblem.bmp");
 home.focus=0;home_step(home,true);
 auto frame_at=[&](int ms,const char* path){for(int t=0;t<ms;t+=16){boot_tick();draw_home(home,-1);boot_overlay();boot_ready();usleep(16000);}capture(path);};
 frame_at(600,"build/ui109-boot-shine.bmp");frame_at(700,"build/ui109-boot-dock.bmp");frame_at(330,"build/ui109-boot-word.bmp");
 for(int t=0;t<2000&&boot_active();t+=16){boot_tick();draw_home(home,-1);boot_overlay();usleep(16000);}assert(!boot_active());
 {Item ep=item("e1","The Long Example Episode","Episode",2021);ep.series="Example Show";ep.series_id="s1";ep.season=2;ep.episode=5;ep.rating="TV-14";ep.duration=2700000;ep.overview=film.overview;
  screensaver_set_items({{ep,"Continue Watching"},{film,"Latest Movies"}});
  for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH;++x)pixels[y*WIDTH+x]=0xff000000|((40+x*80/WIDTH)<<16)|((50+y*60/HEIGHT)<<8)|(90+x*40/WIDTH);
  int column=0;for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH*2/3;++x){column=(int)(0.86f*255*(1-(float)x/(WIDTH*2/3)));uint32_t d=pixels[y*WIDTH+x];int k=255-column;pixels[y*WIDTH+x]=0xff000000|((((d>>16)&255)*k/255)<<16)|((((d>>8)&255)*k/255)<<8)|((d&255)*k/255);}
  screensaver_preview(0);capture("build/ui109-screensaver.bmp");}
 // Benchmark: Home with every card's artwork decoded.
 for(auto& row:home.rows)for(size_t c=0;c<row.page.items.size();++c){std::string key,path;home_image(row,row.page.items[c],key,path);art_inject(key,320,480,fake_image(320,480,(int)c));}
 home.focus=1;home_step(home,true);
 printf("BENCH home frame: %.2f ms
",ms_per([&]{draw_home(home,-1);},200));
 { const Item peek_item=home.rows[2].page.items[0];auto o=home_focus_rect(home);printf("BENCH peek frame: %.2f ms
",ms_per([&]{draw_quick_peek(peek_item,o,0.5f,"");},200)); }
 capture("build/ui109-home-art.bmp");
 // Live TV: the Classic channel grid and the guide.
 std::vector<JFChannel> chans(23);const char* names[]={"BBC One","ITV1","Channel 4","Sky News","ESPN","Discovery","Cartoon Network","MTV"};
 for(size_t k=0;k<chans.size();++k){auto& c=chans[k];memset(&c,0,sizeof c);snprintf(c.id,sizeof c.id,"c%zu",k);snprintf(c.name,sizeof c.name,"%s",names[k%8]);snprintf(c.number,sizeof c.number,"%zu",101+k);c.favourite=k%5==1;uint64_t now=jf_now_ticks();snprintf(c.now_title,sizeof c.now_title,"Programme %zu",k);c.now_start_ticks=now-12*600000000ULL;c.now_end_ticks=now+18*600000000ULL;}
 livetv_preview(chans,6,false);capture("build/ui109-livetv.bmp");
 livetv_preview(chans,2,true);capture("build/ui109-livetv-guide.bmp");
 puts("PASS 1.09 previews rendered (home, jump bar, detail, player, about, boot)");
}
