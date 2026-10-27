// Host-only previews of 1.10: the header clock and user picture, "Ends at"
// on the detail page, quick peek and player bar, the buffering screen and
// the music Now Playing screen with its visualizer.
#include "app.h"
#include "platform.h"
#include "ui_view.h"
#include "art.h"
#include "screenshot.h"
#include "buffering.h"
#include "music_viz.h"
#include "language.h"
#include "boot_anim.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>
void art_inject(const std::string& key,int w,int h,const std::vector<uint8_t>& rgba);
static std::vector<uint8_t> fake_image(int w,int h,int seed){std::vector<uint8_t> p((size_t)w*h*4);for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto q=&p[((size_t)y*w+x)*4];q[0]=(uint8_t)(x*255/w+seed*40);q[1]=(uint8_t)(y*255/h);q[2]=(uint8_t)(128+seed*20);q[3]=255;}return p;}
template<class F> static double ms_per(F f,int frames){auto t0=std::chrono::steady_clock::now();for(int i=0;i<frames;++i)f();return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count()/frames;}
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
bool keyboard(const std::string&,std::string&,bool,KeyboardType){return false;}
uint32_t* framebuffer(){return pixels.data();}void present(){usleep(16000);}uint32_t buttons(){return 0;}
static void capture(const char* path){FILE* f=fopen(path,"wb");assert(f&&write_frame_bmp(f,pixels.data(),WIDTH,HEIGHT,WIDTH));fclose(f);}
static Item item(const char* id,const char* name,const char* type,int year){Item i;snprintf(i.base.id,sizeof i.base.id,"%s",id);snprintf(i.base.name,sizeof i.base.name,"%s",name);snprintf(i.base.type,sizeof i.base.type,"%s",type);i.year=year;return i;}
static void hold(int ms){for(int t=0;t<ms;t+=16){buffering_frame();usleep(16000);}}
static void tone(int frames){
 // Bass, a mid and a high partial, moving: something for every band.
 static int n=0;std::vector<float> lr(1024*2);
 for(int k=0;k<frames;++k){for(int i=0;i<1024;++i,++n){float t=n/48000.0f;float v=0.5f*sinf(6.2831853f*80*t)+0.3f*sinf(6.2831853f*(600+200*sinf(t))*t)+0.15f*sinf(6.2831853f*5000*t);lr[i*2]=lr[i*2+1]=v;}music_viz_push(lr.data(),1024);float b[MUSIC_VIZ_BANDS];music_viz_bands(b);}
}
int main(){
 render_init();theme_apply_builtin(0);assert(storage_init(110));
 // Header: the clock and date at the right, the signed-in user's picture.
 snprintf(g_userid,sizeof g_userid,"%s","u1");
 HomeView home;home.loaded=true;HomeRow media;media.title="My Media";media.shape=SHAPE_LIBRARY;for(auto n:{"Movies","Shows","Music"}){auto i=item(n,n,"CollectionFolder",0);media.page.items.push_back(i);}home.rows.push_back(media);
 home.focus=0;home_step(home,true);
 {std::vector<uint8_t> none;art_inject("user:u1",0,0,none);}draw_home(home,-1);capture("build/ui110-header-silhouette.bmp");
 art_inject("user:u1",96,96,fake_image(96,96,3));draw_home(home,-1);capture("build/ui110-header-avatar.bmp");
 std::string t,d;console_clock(0,t,d);printf("clock now: %s %s, in 90 min: %s\n",d.c_str(),t.c_str(),ends_at(90*60000).c_str());
 // Detail page and quick peek: when a resumed film would finish.
 Item film=item("f","Example Film: The Sequel","Movie",2014);film.duration=7020000;film.resume=1800000;film.rating="PG-13";film.community="7.4";film.genres="Action, Adventure";film.overview="A crew of explorers travel through a wormhole in space in an attempt to ensure humanity's survival.";
 DetailView view;view.version="1080p H264";view.video="1080p · H264 · SDR";view.audio="English - AAC - Stereo - Default";view.audio_chip="AAC Stereo";view.subtitles="Off";
 draw_detail(film,view);capture("build/ui110-detail-ends.bmp");
 draw_home(home,-1);draw_quick_peek(film,{100,200,140,210},1.0f,"");capture("build/ui110-peek-ends.bmp");
 for(int y=0;y<HEIGHT;++y)rect(0,y,WIDTH,1,(y*40/HEIGHT)<<16|(60+y*80/HEIGHT)<<8|90);
 PlayerOsd osd;osd.title=film.base.name;osd.subtitle="2014";osd.audio="AAC Stereo";osd.duration=7020000;osd.position=2460000;osd.started=true;draw_player_osd(osd);capture("build/ui110-player-ends.bmp");
 // Buffering: the backdrop from the detail page, the ring and the labels.
 art_inject("f:backdrop",1280,720,fake_image(1280,720,1));
 buffering_begin(film,"Example Film: The Sequel  \xC2\xB7  2014");hold(400);capture("build/ui110-buffer-preparing.bmp");
 buffering_step("CONNECTING",false,"Cancel");hold(600);capture("build/ui110-buffer-connecting.bmp");
 buffering_step("BUFFERING",true,"Cancel");buffering_progress(0.47f);hold(500);capture("build/ui110-buffer-47.bmp");
 printf("BENCH buffering frame: %.2f ms\n",ms_per([]{buffering_frame();},120));
 buffering_finish(true);assert(!buffering_active());
 // Without artwork: a pool of the accent behind the ring.
 Item channel=item("ch","BBC One","TvChannel",0);buffering_begin(channel,"BBC One");buffering_step("CONNECTING",false,"Cancel");hold(500);capture("build/ui110-buffer-noart.bmp");buffering_finish(false);
 // Music: the cover, the bars, title / artist / album and progress.
 Item song=item("s1","Midnight City","Audio",2011);song.artist="M83";song.album="Hurry Up, We're Dreaming";song.duration=243000;
 art_inject("s1",500,500,fake_image(500,500,2));
 PlayerOsd music;music.title=song.base.name;music.subtitle=song.artist;music.album=song.album+"  \xC2\xB7  2011";music.art="s1";music.audio_only=true;music.started=true;music.duration=song.duration;music.position=84000;
 music_viz_reset();tone(24);float bands[MUSIC_VIZ_BANDS];music_viz_bands(bands);
 int lit=0;for(float b:bands)if(b>0.05f)++lit;printf("visualizer: %d of %d bands lit\n",lit,MUSIC_VIZ_BANDS);assert(lit>=3);
 draw_music_screen(music,bands,MUSIC_VIZ_BANDS,nullptr,0);capture("build/ui110-music.bmp");
 printf("BENCH music frame: %.2f ms\n",ms_per([&]{draw_music_screen(music,bands,MUSIC_VIZ_BANDS,nullptr,0);},120));
 // Paused: no new audio, so the bars fall to the floor.
 usleep(200000);for(int i=0;i<60;++i)music_viz_bands(bands);float top=0;for(float b:bands)top=std::max(top,b);assert(top<0.05f);
 music.paused=true;draw_music_screen(music,bands,MUSIC_VIZ_BANDS,nullptr,0);capture("build/ui110-music-paused.bmp");
 // An album: the track position, Up Next and the L1 / R1 hints.
 music.paused=false;music.track=3;music.tracks=12;music.up_next="Wait";tone(6);music_viz_bands(bands);draw_music_screen(music,bands,MUSIC_VIZ_BANDS,nullptr,0);capture("build/ui110-music-album.bmp");
 // Quick peek: mid-flip on the front face, and its cost per frame with the page under it.
 art_inject("f",320,480,fake_image(320,480,4));draw_home(home,-1);std::vector<uint32_t> under(pixels);
 const PeekOrigin origin{340,128,140,210};
 draw_quick_peek(film,origin,0.25f,"",under.data());capture("build/ui110-peek-flip.bmp");
 float u=0;printf("BENCH peek frame (animating): %.2f ms\n",ms_per([&]{draw_quick_peek(film,origin,u,"",under.data());u+=0.01f;if(u>1)u=0;},200));
 // The loading shimmer over cards whose artwork has not arrived.
 HomeView loading;loading.loaded=true;HomeRow latest;latest.title="Latest Movies";latest.shape=SHAPE_POSTER;for(int k=0;k<8;++k)latest.page.items.push_back(item(("pending"+std::to_string(k)).c_str(),"Loading Film","Movie",2020));loading.rows.push_back(latest);
 loading.focus=0;home_step(loading,true);draw_home(loading,-1);capture("build/ui110-shimmer.bmp");
 // Languages: the PS3 catalog plus the PS4's own strings.
 i18n_set(LANG_JA);draw_detail(film,view);capture("build/ui110-detail-ja.bmp");
 assert(strcmp(T("Settings"),"Settings")&&strcmp(T("Skip 5 min"),"Skip 5 min"));printf("ja: %s / %s / %s\n",T("Settings"),T("Skip 5 min"),TF("Track %d of %d",3,12).c_str());
 i18n_set(LANG_DE);draw_music_screen(music,bands,MUSIC_VIZ_BANDS,nullptr,0);capture("build/ui110-music-de.bmp");printf("de: %s / %s\n",T("Previous"),ends_at(60000).c_str());
 i18n_set(LANG_EN);
 // Settings: the categories, then one category's rows with its description, and the log-out question.
 {std::vector<SettingsCategoryView> cats={{"Playback","Quality  \xC2\xB7  Resume playback  \xC2\xB7  Playback method",SICON_PLAYBACK},{"Display","Theme  \xC2\xB7  Language  \xC2\xB7  Screensaver  \xC2\xB7  Screensaver Delay",SICON_DISPLAY},{"Account","Log Out",SICON_ACCOUNT},{"System","About  \xC2\xB7  Exit application",SICON_SYSTEM}};
  draw_settings_categories(cats,1,"Account: alex  \xC2\xB7  http://192.168.1.100:8096","JellyFin [PS] 1.10 \xC2\xB7 built Oct 27 2026");capture("build/ui110-settings.bmp");
  std::vector<SettingsRowView> rows(4);rows[0]={"Theme","Jellywave","The interface colour theme.\nJellywave is the default; extra .ini themes appear here too."};rows[1]={"Language","Auto (English)","The language the menus are shown in.\nAuto follows the PS4's own language setting."};
  rows[2]={"Screensaver","Artwork","x",true};rows[3]={"Screensaver Delay","3 min","x"};draw_settings_rows(cats[1],rows,1);capture("build/ui110-settings-display.bmp");
  std::vector<SettingsRowView> acct(1);acct[0]={"Log Out","","Sign out of this Jellyfin account.\nYou will need to log in again to browse your library.",false,false,true};
  draw_settings_rows(cats[2],acct,0);draw_confirm("Log out of this account?","You'll need to sign in again to browse your library.","Log Out");capture("build/ui110-settings-confirm.bmp");}
 // The L1 picture banner over a frame, and Software Update's offer and progress.
 for(int y=0;y<HEIGHT;++y)rect(0,y,WIDTH,1,(y*40/HEIGHT)<<16|(60+y*80/HEIGHT)<<8|90);
 draw_picture_banner("Picture size   Width 96%   Height 100%   Fill",false);capture("build/ui110-picture.bmp");
 draw_home(home,-1);draw_update_panel("Version 1.11 is available","Download it now? You install it afterwards from the Package Installer.",-1,"Download","Not now");capture("build/ui110-update-offer.bmp");
 draw_home(home,-1);draw_update_panel("Downloading update","12.4 MB / 32.0 MB",12.4f/32.0f,nullptr,"Cancel");capture("build/ui110-update-progress.bmp");
 // The visualizers, on fresh audio: Aurora, Halo and Scope (Bars is above), with the banner.
 {music.paused=false;music.up_next="";music.tracks=0;std::vector<float> wv(688);
  const char* files[]={"","build/ui110-viz-aurora.bmp","build/ui110-viz-halo.bmp","build/ui110-viz-scope.bmp","build/ui110-viz-off.bmp"};
  for(int v=VIZ_AURORA;v<VIZ_COUNT;++v){tone(8);music_viz_bands(bands);music_viz_wave(wv.data(),688);music.viz=v;music.viz_toast=v==VIZ_HALO;
   draw_music_screen(music,bands,MUSIC_VIZ_BANDS,wv.data(),688);capture(files[v]);}
  music.viz=VIZ_AURORA;music.viz_toast=false;printf("BENCH music frame (aurora): %.2f ms\n",ms_per([&]{draw_music_screen(music,bands,MUSIC_VIZ_BANDS,wv.data(),688);},60));
  music.viz=VIZ_HALO;printf("BENCH music frame (halo): %.2f ms\n",ms_per([&]{draw_music_screen(music,bands,MUSIC_VIZ_BANDS,wv.data(),688);},60));
  music.viz=VIZ_SCOPE;printf("BENCH music frame (scope): %.2f ms\n",ms_per([&]{draw_music_screen(music,bands,MUSIC_VIZ_BANDS,wv.data(),688);},60));}
 // The boot animation: the mark over black, then mid-handoff over Home.
 boot_begin();for(int i=0;i<110;++i)boot_pump();capture("build/ui110-boot-mark.bmp");
 for(int t=0;t<1300;t+=16){boot_tick();draw_home(home,-1);boot_overlay();boot_ready();present();}capture("build/ui110-boot-dock.bmp");
 for(int t=0;t<4000&&boot_active();t+=16){boot_tick();draw_home(home,-1);boot_overlay();present();}
 puts("PASS 1.10 previews rendered (header, ends at, buffering, music, peek, shimmer, languages, picture, update, boot)");
}
