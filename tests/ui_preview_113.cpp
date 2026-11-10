// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// Host-only previews of 1.13: the music library's tabs (Albums, Artists,
// Playlists, Genres, Songs), Now Playing with shuffle and the next tracks,
// and the queue screen.
#include "app.h"
#include "platform.h"
#include "ui_view.h"
#include "art.h"
#include "screenshot.h"
#include "music_viz.h"
#include "language.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>
void art_inject(const std::string& key,int w,int h,const std::vector<uint8_t>& rgba);
static std::vector<uint8_t> fake_image(int w,int h,int seed){std::vector<uint8_t> p((size_t)w*h*4);for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto q=&p[((size_t)y*w+x)*4];q[0]=(uint8_t)(x*255/w+seed*40);q[1]=(uint8_t)(y*255/h);q[2]=(uint8_t)(128+seed*20);q[3]=255;}return p;}
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
bool keyboard(const std::string&,std::string&,bool,KeyboardType){return false;}
uint32_t* framebuffer(){return pixels.data();}void present(){usleep(1000);}uint32_t buttons(){return 0;}
static void capture(const char* path){FILE* f=fopen(path,"wb");assert(f&&write_frame_bmp(f,pixels.data(),WIDTH,HEIGHT,WIDTH));fclose(f);}
static Item item(const char* id,const char* name,const char* type,int year){Item i;snprintf(i.base.id,sizeof i.base.id,"%s",id);snprintf(i.base.name,sizeof i.base.name,"%s",name);snprintf(i.base.type,sizeof i.base.type,"%s",type);i.year=year;return i;}
int main(){
 render_init();theme_apply_builtin(0);assert(storage_init(113));
 snprintf(g_userid,sizeof g_userid,"%s","u1");
 // The tab helpers wrap both ways and name every tab.
 assert(music_tab_of(MODE_MUSIC)==0&&music_tab_of(MODE_SONGS)==4&&music_tab_of(MODE_ALBUM)==-1);
 assert(music_tab_mode(-1)==MODE_SONGS&&music_tab_mode(MUSIC_TABS)==MODE_MUSIC);
 // Albums, with the tab strip.
 Page albums;const char* names[]={"Night Drive","Harbour Lights","Paper Kites","Static Bloom","Low Tide","Glass Houses","Northbound","Afterglow","Summer Static","Velvet Hour","Copper Sky","Driftwood"};
 for(int i=0;i<12;++i){char id[8];snprintf(id,sizeof id,"a%d",i);albums.items.push_back(item(id,names[i],"MusicAlbum",2000+i));if(i%3)art_inject(id,300,300,fake_image(300,300,i));}
 albums.total=12;
 draw_library(albums,"Music",0,1,false,-1,0,"",nullptr,0);capture("build/ui113-music-albums.bmp");
 // Songs: square cards, the artist under the focused one, and a letter filter beside the tabs.
 Page songs;for(int i=0;i<12;++i){char id[8];snprintf(id,sizeof id,"s%d",i);Item s=item(id,names[11-i],"Audio",0);s.artist="The Example Band";s.duration=200000+i*7000;songs.items.push_back(s);if(i%2)art_inject(id,300,300,fake_image(300,300,i+3));}
 songs.total=258;JumpBar jump;jump.show=true;jump.filter="N";
 draw_library(songs,"Music",0,2,false,-1,0,"",&jump,4);capture("build/ui113-music-songs.bmp");
 // Now Playing: shuffle on, the next three tracks.
 art_inject("np",500,500,fake_image(500,500,2));
 PlayerOsd music;music.title="Harbour Lights";music.subtitle="The Example Band";music.album="Night Drive  \xC2\xB7  2011";music.art="np";music.audio_only=true;music.started=true;music.duration=243000;music.position=84000;
 music.track=3;music.tracks=12;music.shuffle=true;music.upcoming={"Static Bloom","Copper Sky","Low Tide"};music.up_next="Static Bloom";music.viz=VIZ_OFF;
 float bands[MUSIC_VIZ_BANDS]={};
 draw_music_screen(music,bands,MUSIC_VIZ_BANDS,nullptr,0);capture("build/ui113-music-shuffle.bmp");
 // The queue screen (Options), scrolled to the highlighted track.
 for(auto n:names)music.queue_names.push_back(n);music.queue_open=true;music.queue_sel=8;
 draw_music_screen(music,bands,MUSIC_VIZ_BANDS,nullptr,0);capture("build/ui113-music-queue.bmp");
 // Search: the keyboard beside the recent searches, then live results with
 // their kinds to filter by and the highlighted one described.
 {KeyboardEditor editor("");SearchView view;view.editor=&editor;view.recents={"harbour","static bloom","example"};
  draw_search_page(view);capture("build/ui113-search-recent.bmp");
  editor.input="night";editor.cursor=5;draw_search_page(view);capture("build/ui113-search-typing.bmp");editor.input.clear();editor.cursor=0;
  editor.input="nig";editor.cursor=3;view.keys=false;view.selected=1;
  Item film=item("r1","Night Shift","Movie",2019);film.overview="A night guard at a quiet museum notices that the exhibits are never quite where he left them.";art_inject("r1",200,300,fake_image(200,300,4));
  Item show=item("r2","Nightfall Harbour","Series",2021);show.overview="A small fishing town keeps one secret too many when the lighthouse goes dark.";art_inject("r2",200,300,fake_image(200,300,5));
  Item ep=item("r3","The Night Ferry","Episode",2021);ep.series="Nightfall Harbour";ep.season=1;ep.episode=4;
  Item album=item("r4","Night Drive","MusicAlbum",2011);art_inject("r4",300,300,fake_image(300,300,6));
  Item song=item("r5","Night Drive (Reprise)","Audio",0);song.artist="The Example Band";
  view.results={film,show,ep,album,song};view.total=5;view.filters={"All","Films","Shows","Episodes","Albums","Songs"};view.filter_counts={5,1,1,1,1,1};
  draw_search_page(view);capture("build/ui113-search-results.bmp");}
 // The player's track menu: audio listed, the playing track marked.
 {for(int y=0;y<HEIGHT;++y)rect(0,y,WIDTH,1,(y*40/HEIGHT)<<16|(60+y*80/HEIGHT)<<8|90);
  PlayerOsd osd;osd.title="Night Shift";osd.subtitle="2019";osd.duration=7020000;osd.position=2460000;osd.started=true;osd.control=OSD_SCRUB;
  osd.tracks_open=true;osd.audio_names={"English - AAC - Stereo - Default","English - AC3 - 5.1","Commentary - AAC - Stereo","Deutsch - AC3 - 5.1"};osd.audio_now=0;osd.audio_sel=1;
  osd.sub_names={"Off","English - SUBRIP","English (SDH) - SUBRIP"};
  draw_player_osd(osd);draw_track_menu(osd);capture("build/ui113-player-tracks.bmp");
  osd.tracks_sub=true;osd.sub_sel=2;draw_player_osd(osd);draw_track_menu(osd);capture("build/ui113-player-subs.bmp");}
 // Downloads: one downloading, two saved (one on a USB drive).
 {std::vector<DownloadEntry> entries;DownloadEntry e1;e1.item=item("d1","The Night Ferry","Episode",2021);e1.item.series="Nightfall Harbour";e1.item.season=1;e1.item.episode=4;e1.item.duration=2580000;e1.bytes=812000000;
  DownloadEntry e2;e2.item=item("d2","Night Shift","Movie",2019);e2.item.duration=6300000;e2.item.resume=1800000;e2.bytes=2400000000ULL;e2.usb=true;art_inject("d2",200,300,fake_image(200,300,4));
  entries={e1,e2};DownloadStatus st;st.active=true;st.name="Nightfall Harbour S01E05";st.got=310000000;st.total=790000000;st.queued=3;
  draw_downloads(entries,1,st,"New downloads are saved to the USB drive.");capture("build/ui113-downloads.bmp");}
 // A USB drive's folder: folders first, then videos and music, with sizes.
 {std::vector<LocalRow> rows;auto row=[&](const char* n,const char* d,LocalKind k){LocalRow r;r.name=n;r.detail=d;r.kind=k;rows.push_back(r);};
  row("Holiday 2025","",LOCAL_FOLDER);row("Music","",LOCAL_FOLDER);row("Garden Tour.mkv","1.4 GB",LOCAL_VIDEO);row("Beach Day.mp4","812.0 MB",LOCAL_VIDEO);row("01 Morning Light.flac","31.2 MB",LOCAL_AUDIO);row("02 Harbour Wind.flac","28.7 MB",LOCAL_AUDIO);
  draw_local_browser("USB Drive","/mnt/usb0",rows,2,"");capture("build/ui113-usb.bmp");}
 puts("ui113: ok");return 0;
}
