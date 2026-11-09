// Subtitles drawn by the console: the production playback_url() and
// player_mpv.cpp against a real Jellyfin and real files, with what the app
// draws counted in its own framebuffer.
//   subtitles-116 <server> <user> <password> <media dir> <idmap: "file id" lines>
// The media (tools/test_subtitles_116.sh makes it): a dark 20 s picture with
// subtitles from 1 s to 19 s. "Subs Text.mkv": a white SubRip line (track 2)
// and an ASS line styled red in Arial, which is not bundled (track 3).
// "Subs PGS.mkv": a white PGS box. "Subs External.mkv": SubRip beside it.
// "Subs HEVC.mkv": HEVC, so the server converts it, with SubRip and PGS.
#include "app.h"
#include "platform.h"
#include "subtitles.h"
#include "local_media.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <map>
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
uint32_t* framebuffer(){return pixels.data();}
static int failures=0;
#define CHECK(cond,...) do{if(cond)printf("PASS ");else{printf("FAIL ");++failures;}printf(__VA_ARGS__);puts("");fflush(stdout);}while(0)

// Pixels in the lower third of what the app drew: white, soft yellow, red.
struct Ink{int white=0,yellow=0,red=0,grey=0;bool any()const{return white+yellow+red>400;}};
static Ink ink(){
 Ink out;for(int y=HEIGHT*2/3;y<HEIGHT;++y)for(int x=0;x<WIDTH;++x){const uint32_t p=pixels[y*WIDTH+x];const int r=(p>>16)&255,g=(p>>8)&255,b=p&255;
  if(r>200&&g>200&&b>200)++out.white;else if(r>200&&g>180&&b>90&&b<190)++out.yellow;else if(r>200&&g<80&&b<80)++out.red;else if(r>140&&r<=200&&abs(r-g)<12&&abs(g-b)<12)++out.grey;}
 return out;
}
// Plays until the stream's clock passes `at` ms, and what was drawn then.
static Ink drawn_at(uint64_t at,bool* played=nullptr){
 bool video=false;for(int i=0;i<40000;++i){if(player_draw())video=true;if(!player_error().empty())break;if(video&&player_time()>=at)break;usleep(1000);}
 // A few more frames, so a subtitle added after loading is in.
 for(int i=0;i<80;++i){player_draw();usleep(1000);}
 if(played)*played=video&&player_error().empty();return ink();
}
static Item film(const std::map<std::string,std::string>& ids,const char* name){Item i;snprintf(i.base.id,sizeof i.base.id,"%s",ids.at(name).c_str());snprintf(i.base.type,sizeof i.base.type,"Movie");i.duration=20000;return i;}
struct Plan{bool is_static;int track;std::string sub_url,url;};
static Plan negotiate(const Item& item,int sub){
 std::string url,session,source;assert(playback_url(item,-1,sub,"",url,session,source));end_playback(session);
 return {playback_is_static(),playback_subtitle_track(),playback_subtitle_url(),url};
}
// As main.cpp's play(): the plan to the player, the stream opened.
static Ink play(const Item& item,int sub,uint64_t at,Plan* out=nullptr,bool* played=nullptr){
 const Plan p=negotiate(item,sub);if(out)*out=p;
 const uint64_t start=!p.is_static&&settings.resume?item.resume:0;
 player_prefer_audio(0);player_prefer_subtitle(p.track,p.sub_url,-(int64_t)start);assert(player_open(p.url));
 const Ink i=drawn_at(at,played);player_close();usleep(100000);return i;
}
static std::string param(const std::string& url,const char* name){const std::string key=std::string(name)+"=";auto at=url.find("?"+key);if(at==std::string::npos)at=url.find("&"+key);if(at==std::string::npos)return "";at+=key.size()+1;return url.substr(at,url.find('&',at)-at);}

int main(int argc,char** argv){
 if(argc<6){puts("usage: subtitles-116 <server> <user> <password> <media dir> <idmap>");return 2;}
 settings.debug_log=true;assert(storage_init(116));setenv("JF_TEST_AO","null",1);
 snprintf(g_server,sizeof g_server,"%s",argv[1]);assert(login(argv[2],argv[3]));
 std::map<std::string,std::string> ids;{std::ifstream in(argv[5]);std::string name,id;while(in>>name>>id)ids[name]=id;}
 const std::string media=argv[4];settings.resume=false;

 // ---- the console draws them -------------------------------------------------
 settings.sub_console=true;settings.sub_font=SUBFONT_OPENSANS;settings.sub_colour=SUBCOLOUR_WHITE;
 Plan p;bool played=false;
 Ink none=play(film(ids,"Subs_Text.mkv"),-1,2500,&p,&played);
 CHECK(played&&p.is_static&&p.track==0&&none.white<100,"no subtitles chosen: direct play, nothing drawn (%d white)",none.white);
 Ink srt=play(film(ids,"Subs_Text.mkv"),2,2500,&p);
 CHECK(p.is_static&&p.track==1,"SubRip inside the file: direct play now, not a transcode (player track %d)",p.track);
 CHECK(srt.white>800&&srt.yellow<100,"SubRip drawn by the console, white (%d white pixels)",srt.white);
 Ink ass=play(film(ids,"Subs_Text.mkv"),3,2500,&p);
 CHECK(p.is_static&&p.track==2&&ass.red>800,"ASS keeps its own red, in a bundled face for its Arial (%d red pixels)",ass.red);
 Ink pgs=play(film(ids,"Subs_PGS.mkv"),2,2500,&p);
 CHECK(p.is_static&&p.track==1&&pgs.white>20000,"PGS drawn by the console, direct play (%d white pixels)",pgs.white);
 Ink ext=play(film(ids,"Subs_External.mkv"),0,2500,&p);
 CHECK(p.is_static&&!p.sub_url.empty()&&ext.white>800,"SubRip beside the file: from the server's URL, direct play (%d white pixels)",ext.white);
 Ink hevc_text=play(film(ids,"Subs_HEVC.mkv"),2,2500,&p);
 CHECK(!p.is_static&&!p.sub_url.empty()&&param(p.url,"SubtitleMethod").empty()&&hevc_text.white>800,"converted video: SubRip from the server's file, drawn here, not burned in (%d white)",hevc_text.white);
 Ink hevc_pgs=play(film(ids,"Subs_HEVC.mkv"),3,2500,&p);
 CHECK(!p.is_static&&param(p.url,"SubtitleMethod")=="Encode"&&param(p.url,"SubtitleStreamIndex")=="3"&&hevc_pgs.white>20000,"converted video with PGS: burned in by the server, track 3 asked for (%d white)",hevc_pgs.white);

 // A converted stream that starts part way: its clock starts at 0, the
 // subtitle file's at the film's start. From 15 s, the line (to 19 s) shows at
 // first and is gone by 6 s in; wrongly offset, it would still be there.
 settings.resume=true;{Item late=film(ids,"Subs_HEVC.mkv");late.resume=15000;
  Ink early=play(late,2,1500,&p);Ink after=play(late,2,6000);
  CHECK(!p.is_static&&early.white>800&&after.white<100,"resumed at 15 s: the subtitle file lines up with the converted stream (%d, then %d white)",early.white,after.white);}
 settings.resume=false;

 // The face and look.
 settings.sub_colour=SUBCOLOUR_YELLOW;Ink yellow=play(film(ids,"Subs_Text.mkv"),2,2500);
 CHECK(yellow.yellow>800&&yellow.white<200,"Soft Yellow (%d yellow, %d white)",yellow.yellow,yellow.white);
 settings.sub_colour=SUBCOLOUR_GREY;Ink grey=play(film(ids,"Subs_Text.mkv"),2,2500);
 CHECK(grey.grey>800&&grey.white<srt.white/2,"Soft Grey draws, translucent: light grey, not the opaque white (%d grey, %d white; White gave %d white)",grey.grey,grey.white,srt.white);
 settings.sub_colour=SUBCOLOUR_WHITE;
 int widths[SUBFONT_COUNT];for(int f=0;f<SUBFONT_COUNT;++f){settings.sub_font=f;widths[f]=play(film(ids,"Subs_Text.mkv"),2,2500).white;}
 CHECK(widths[0]>800&&widths[1]>800&&widths[2]>800&&widths[0]!=widths[1]&&widths[1]!=widths[2]&&widths[0]!=widths[2],"all three faces draw, each differently (Open Sans %d, Noto Sans %d, Roboto Condensed %d white)",widths[0],widths[1],widths[2]);
 settings.sub_font=SUBFONT_OPENSANS;
 for(const char* face:{"OpenSans-Bold.ttf","NotoSans-Bold.ttf","RobotoCondensed-Bold.ttf"}){std::ifstream f(data_dir+"/fonts/"+face,std::ios::binary|std::ios::ate);CHECK(f&&f.tellg()>10000,"face written for libass: %s (%lld bytes)",face,(long long)(f?(long long)f.tellg():0));}

 // ---- the server burns them in (Draw Subtitles: Server) ----------------------
 settings.sub_console=false;
 Ink burned=play(film(ids,"Subs_Text.mkv"),2,2500,&p);
 CHECK(!p.is_static&&param(p.url,"SubtitleMethod")=="Encode"&&param(p.url,"SubtitleStreamIndex")=="2"&&burned.white>800,"Server: converted with SubRip burned in (%d white)",burned.white);
 Ink burned_ass=play(film(ids,"Subs_Text.mkv"),3,2500,&p);
 CHECK(param(p.url,"SubtitleStreamIndex")=="3"&&burned_ass.red>800,"Server: the ASS track asked for is the one burned in (%d red)",burned_ass.red);
 settings.sub_console=true;

 // ---- a file with no server: its own tracks, switched in place ----------------
 player_prefer_subtitle(-1,"",0);player_prefer_audio(0);assert(player_open(media+"/Subs Text.mkv"));
 for(int i=0;i<5000&&!player_loaded();++i){player_draw();usleep(1000);}
 auto subs=player_subtitle_tracks();std::string names;for(auto& s:subs)names+=s.label+" | ";printf("     menu: %s\n",names.c_str());
 CHECK(subs.size()==2,"USB file: both subtitle tracks listed");
 if(subs.size()==2){player_select_subtitle(subs[1].id);Ink red=drawn_at(2500);CHECK(red.red>800,"USB file: switched in place to the ASS track (%d red)",red.red);
  player_select_subtitle(0);for(int i=0;i<300;++i){player_draw();usleep(1000);}Ink off=ink();CHECK(off.red<100&&off.white<100,"USB file: and off again");}
 player_close();

 // A file beside it ("Subs External.en.srt"): listed by name, loaded when chosen,
 // and switched between in place like the file's own.
 {const auto files=local_subtitle_files(media+"/Subs External.mkv");
  CHECK(files.size()==1&&files[0].label=="English - SRT"&&files[0].lang=="en","USB file: the subtitle file beside it is found and named (%s)",files.empty()?"none":files[0].label.c_str());
  std::vector<PlayerSubtitleFile> side;for(const auto& f:files)side.push_back({f.url,f.label,f.lang});
  player_prefer_subtitle(-1,"",0);player_subtitle_files(side);player_prefer_audio(0);assert(player_open(media+"/Subs External.mkv"));
  Ink before=drawn_at(2000);auto list=player_subtitle_tracks();
  CHECK(list.size()==1&&list[0].id<0&&before.white<100,"USB file: it is in the menu, not shown until chosen (%d white)",before.white);
  if(!list.empty()){player_select_subtitle(list[0].id);for(int i=0;i<600;++i){player_draw();usleep(1000);}const Ink on=ink();list=player_subtitle_tracks();
   CHECK(on.white>800&&list.size()==1&&list[0].id>0&&list[0].selected&&list[0].label=="English - SRT","USB file: chosen, it loads and shows (%d white), still named the same",on.white);
   player_select_subtitle(0);for(int i=0;i<300;++i){player_draw();usleep(1000);}const Ink off=ink();
   if(!list.empty())player_select_subtitle(list[0].id);for(int i=0;i<300;++i){player_draw();usleep(1000);}const Ink again=ink();
   CHECK(off.white<100&&again.white>800,"USB file: off, and back on in place (%d, then %d white)",off.white,again.white);}
  player_close();}

 logout();
 printf(failures?"FAILURES: %d\n":"ALL PASSED\n",failures);return failures?1:0;
}
