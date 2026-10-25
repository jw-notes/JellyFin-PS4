#include "app.h"
#include "auth.h"
#include "platform.h"
#include "art.h"
#include "facts.h"
#include "ui_view.h"
#include "input_model.h"
#include "boot_anim.h"
#include "screensaver.h"
#include "livetv_view.h"
#include "api_userdata.h"
#include <orbis/_types/pad.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "../vendor/ps3/source/ui/render/peek.h"
#ifndef JF_BUILD_VERSION
#define JF_BUILD_VERSION "host"
#endif
static bool running=true;
static int play_generation=0;
static void header(const std::string& title){app_header(title,title=="Settings"?"Settings":"Home");}
static void notice(const std::string& s){header("");face_text(64,86,s,g_theme.text,24,TextFace::Classic);present();}
static uint32_t press(){return ui_input();}
static void wait_release(){ui_input_sync();}
static uint64_t now_ms(){timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*1000+ts.tv_nsec/1000000;}
static void error_screen(){UiInputScope input_scope;wait_release();for(;;){header("Request failed");wrapped(60,160,last_error,g_theme.text);hint_bar({{'C',"Return"}});present();if(press()&ORBIS_PAD_BUTTON_CIRCLE)break;usleep(16000);}}
static void about_screen(){
 UiInputScope input_scope;int section=0,top=0;wait_release();
 for(;;){draw_about(section,top);present();auto b=press();
  if(b&ORBIS_PAD_BUTTON_CIRCLE)return;
  if(b&(ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_LEFT)&&section>0){section--;top=0;}
  if(b&(ORBIS_PAD_BUTTON_R1|ORBIS_PAD_BUTTON_RIGHT)&&section<about_sections_count()-1){section++;top=0;}
  if(b&ORBIS_PAD_BUTTON_DOWN)top+=27;if(b&ORBIS_PAD_BUTTON_UP)top-=27;
  top=std::max(0,std::min(top,about_max_scroll(section)));}
}
static int settings_screen(){
 UiInputScope input_scope;int selected=0;constexpr int rows=9,pitch=54;wait_release();
 for(;;){
  header("Settings");
  std::string labels[]={"Theme: "+std::string(theme_current_name()),"Streaming bitrate: "+std::to_string(settings.bitrate/1000000)+" Mbps",std::string("Resume playback: ")+(settings.resume?"On":"Off"),std::string("Playback: ")+(settings.direct?"Direct file (compatible MP4 only)":"Server transcode (H264 / AAC)"),std::string("Screensaver: ")+(settings.saver?"On (Artwork)":"Off"),"Screensaver delay: "+screensaver_delay_label(),"About","Sign out","Exit application"};
  for(int i=0;i<rows;i++){const bool dim=i==5&&!settings.saver;if(i==selected)rect(48,130+i*pitch,1184,48,g_theme.panel_hi);text(70,139+i*pitch,labels[i],i==selected?g_theme.accent:dim?g_theme.text_faint:g_theme.text);}
  hint_bar({{'D',"Select / change"},{'X',"Change"},{'C',"Back"},{'l',"Search"},{'r',"Home"}});present();
  auto b=press();if(b&ORBIS_PAD_BUTTON_L1)return 1;if(b&ORBIS_PAD_BUTTON_R1)return 0;
  if(b&ORBIS_PAD_BUTTON_UP)selected=(selected+rows-1)%rows;if(b&ORBIS_PAD_BUTTON_DOWN)selected=(selected+1)%rows;
  if(b&(ORBIS_PAD_BUTTON_CROSS|ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT)){
   int dir=b&ORBIS_PAD_BUTTON_LEFT?-1:1;
   if(selected==0)theme_step(dir);
   if(selected==1){int rates[]={2000000,4000000,8000000,12000000,20000000};int n=0;for(int i=0;i<5;i++)if(rates[i]==settings.bitrate)n=i;settings.bitrate=rates[(n+dir+5)%5];}
   if(selected==2)settings.resume=!settings.resume;if(selected==3)settings.direct=!settings.direct;
   if(selected==4)settings.saver=!settings.saver;if(selected==5)screensaver_delay_step(dir);
   if(selected<6&&!save_settings()){last_error="Unable to save settings";error_screen();}
   if(selected==6&&(b&ORBIS_PAD_BUTTON_CROSS))about_screen();
   if(selected==7&&(b&ORBIS_PAD_BUTTON_CROSS)){facts_stop();art_stop();page_cache_stop();livetv_stop();logout();return -1;}
   if(selected==8&&(b&ORBIS_PAD_BUTTON_CROSS)){running=false;return -1;}
  }
  if(b&ORBIS_PAD_BUTTON_CIRCLE)return -1;
 }
}
static int search_screen(std::string& query){
 UiInputScope input_scope;
 for(;;){draw_search(query);present();auto b=press();
  if(b&ORBIS_PAD_BUTTON_L1)return 0;
  if(b&ORBIS_PAD_BUTTON_R1)return 2;
  if(b&ORBIS_PAD_BUTTON_CIRCLE)return -1;
  if(b&ORBIS_PAD_BUTTON_CROSS){if(keyboard("Search Jellyfin",query)&&!query.empty())return 3;ui_input_sync();}

 }
}
static void play(Item item,int audio,int subtitle,const std::string& source,const std::string& audio_label="",const std::string& subtitle_label=""){
 UiInputScope input_scope;ScreensaverInhibit no_saver;++play_generation;page_cache_clear();std::string url,session,source_id;notice("Preparing stream...");
 if(!playback_url(item,audio,subtitle,source,url,session,source_id)){end_playback(session);error_screen();return;}
 if(!player_open(url)){end_playback(session);error_screen();return;}
 bool paused=false,started=false,play_failed=false,cancelled=false;time_t began=time(nullptr),last_report=began;bool reported=false;
 uint64_t base=settings.resume&&!settings.direct?item.resume:0;bool direct_resume_pending=settings.direct&&settings.resume&&item.resume;
 const bool live=!strcmp(item.base.type,"TvChannel"),audio_only=!strcmp(item.base.type,"Audio");
 // The PS3 player HUD: title, play state, progress and times. Left/Right move
 // a scrub target along the bar (holding accelerates); the seek happens once
 // the target settles, so a run of presses is one request to the server.
 PlayerOsd osd;osd.live=live;osd.audio_only=audio_only;osd.duration=item.duration;osd.audio=audio_label;osd.subtitles=subtitle_label;
 if(!strcmp(item.base.type,"Episode")&&!item.series.empty()){osd.title=item.series;char code[32]="";if(item.episode)snprintf(code,sizeof code,"S%d:E%d - ",item.season,item.episode);osd.subtitle=code+std::string(item.base.name);}
 else{osd.title=item.base.name;if(item.year)osd.subtitle=std::to_string(item.year);}
 uint64_t osd_until=now_ms()+5000,target=0,scrub_at=0,hold_until=0;bool scrubbing=false;int scrub_presses=0;
 wait_release();
 for(;;){
  bool got=player_draw();auto decoder_error=player_error();if(!decoder_error.empty()){last_error=decoder_error;play_failed=true;break;}
  if(got||player_has_audio())started=true;if(started&&direct_resume_pending){player_seek(item.resume);direct_resume_pending=false;}
  const uint64_t now=now_ms();uint64_t pos=base+player_time(),shown=pos;
  // While an asynchronous seek lands, keep showing where it is going.
  if(hold_until&&now<hold_until&&(pos+3000<target||pos>target+3000))shown=target;else hold_until=0;
  if(!started)face_text(57,60,"Buffering stream...",g_theme.white,20,TextFace::Classic);
  if(!started||paused||scrubbing||audio_only||now<osd_until){osd.position=shown;osd.target=target;osd.paused=paused;osd.started=started;osd.scrubbing=scrubbing;draw_player_osd(osd);}
  present();auto b=press();
  if(b&ORBIS_PAD_BUTTON_CIRCLE){cancelled=true;break;}
  if(b&ORBIS_PAD_BUTTON_CROSS){paused=!paused;player_pause(paused);osd_until=now+5000;}
  if(b&ORBIS_PAD_BUTTON_SQUARE)osd_until=now<osd_until?0:now+5000;
  if(b&(ORBIS_PAD_BUTTON_UP|ORBIS_PAD_BUTTON_DOWN|ORBIS_PAD_BUTTON_TRIANGLE))osd_until=now+5000;
  const uint32_t seek=b&(ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT|ORBIS_PAD_BUTTON_L2|ORBIS_PAD_BUTTON_R2);
  if(seek&&!live&&started){
   if(!scrubbing){target=shown;scrubbing=true;scrub_presses=0;}
   const uint64_t step=(seek&(ORBIS_PAD_BUTTON_L2|ORBIS_PAD_BUTTON_R2))?300000:scrub_presses<6?10000:scrub_presses<16?30000:60000;
   if(seek&(ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_L2))target=target>step?target-step:0;else target+=step;
   if(item.duration&&target+5000>item.duration)target=item.duration>5000?item.duration-5000:0;
   ++scrub_presses;scrub_at=now;osd_until=now+5000;
  }
  if(scrubbing&&now-scrub_at>700){
   // The transcode starts at the resume point; earlier positions are clamped to it.
   player_seek(target>base?target-base:0);if(target<base)target=base;scrubbing=false;hold_until=now+3000;osd_until=now+3000;
  }
  if(started&&!reported){reported=true;report_playback("",item,session,source_id,pos,paused);last_report=time(nullptr);}
  if(time(nullptr)-last_report>=10){report_playback("/Progress",item,session,source_id,pos,paused);last_report=time(nullptr);}
  if(started&&!paused&&!player_active()&&time(nullptr)-began>2)break;
  if(!started&&time(nullptr)-began>45){last_error="Decoder could not start this stream within 45 seconds";break;}
  usleep(1000);
 }
 uint64_t pos=base+player_time();player_close();report_playback("/Stopped",item,session,source_id,pos,paused);end_playback(session);if((!started&&!cancelled)||play_failed)error_screen();wait_release();}
// "English - EAC3 - 5.1 - Default" -> "EAC3 5.1", for the detail page's chip.
static std::string audio_chip(const std::string& label){
 std::vector<std::string> parts;size_t at=0;for(;;){auto next=label.find(" - ",at);parts.push_back(label.substr(at,next==std::string::npos?std::string::npos:next-at));if(next==std::string::npos)break;at=next+3;}
 std::string out;for(size_t i=0;i<parts.size();++i){if((i==0&&parts.size()>1)||parts[i]=="Default"||parts[i]=="Forced"||parts[i]=="External"||parts[i].empty())continue;if(!out.empty())out+=" ";out+=parts[i];}return out;
}
// Returns true with `person` set when a cast card asked for that actor's filmography.
static bool detail_screen(const Item& initial,Person& person){
 UiInputScope input_scope;Item item=initial;notice("Loading item...");if(!fetch_detail(initial,item)){error_screen();return false;}
 static JFMediaSources sources;memset(&sources,0,sizeof sources);jellyfin_parse_media_sources(responseBuffer,&sources);request_detail_art(item);
 int source=0,sub_slot=-1;auto default_audio=[&](){auto t=sources.n_sources?&sources.source[source].tracks:nullptr;return t&&t->default_audio>=0&&t->default_audio<t->n_audio?t->default_audio:0;};int audio_slot=default_audio();
 DetailView view;wait_release();
 for(;;){
  auto tracks=sources.n_sources?&sources.source[source].tracks:nullptr;
  view.audio=tracks&&tracks->n_audio?tracks->audio[audio_slot].label:"Server default";
  view.subtitles=tracks&&sub_slot>=0?tracks->subs[sub_slot].label:"Off";
  view.version=sources.n_sources?sources.source[source].label:"Server default";
  view.video=sources.n_sources?sources.source[source].summary:"";
  if(view.video.empty()&&sources.n_sources>1)view.video=sources.source[source].label;
  view.audio_chip=tracks&&tracks->n_audio?audio_chip(view.audio):"";
  const auto actions=detail_actions(item);const int cast=(int)item.people.size();
  draw_detail(item,view);present();auto b=press();
  if(b&ORBIS_PAD_BUTTON_CIRCLE)return false;
  if(b&ORBIS_PAD_BUTTON_UP&&view.row>0){view.row--;view.col=view.row==0?0:std::min(view.col,2);}
  else if(b&ORBIS_PAD_BUTTON_DOWN&&view.row<(cast?2:1)){view.row++;view.col=view.row==2?view.cast_first:std::min(view.col,2);}
  else if(b&(ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT)){
   const int dir=b&ORBIS_PAD_BUTTON_LEFT?-1:1,count=view.row==0?(int)actions.size():view.row==1?3:cast;
   view.col=std::max(0,std::min(count-1,view.col+dir));
   if(view.row==2){if(view.col<view.cast_first)view.cast_first=view.col;if(view.col>=view.cast_first+DETAIL_CAST_VISIBLE)view.cast_first=view.col-DETAIL_CAST_VISIBLE+1;}
  }
  else if(b&ORBIS_PAD_BUTTON_CROSS){
   if(view.row==0){
    const auto action=actions[std::min(view.col,(int)actions.size()-1)];
    if(action==ACT_RESUME||action==ACT_PLAY||action==ACT_START){
     Item target=item;if(action==ACT_START)target.resume=0;
     const std::string audio_name=tracks&&tracks->n_audio?audio_chip(tracks->audio[audio_slot].label):"",sub_name=tracks&&sub_slot>=0?tracks->subs[sub_slot].label:"";
     play(target,tracks&&tracks->n_audio?tracks->audio[audio_slot].index:-1,tracks&&sub_slot>=0?tracks->subs[sub_slot].index:-1,sources.n_sources?sources.source[source].id:"",audio_name.empty()?view.audio:audio_name,sub_name);
     // Resume point and watched state change with playback.
     Item fresh;if(fetch_detail(item,fresh)){fresh.people=fresh.people.empty()?item.people:fresh.people;item=fresh;request_detail_art(item);}
     view.col=0;wait_release();continue;
    }
    if(action==ACT_FAVOURITE){if(jf_set_favourite(item.base.id,!item.favourite)){item.favourite=!item.favourite;page_cache_clear();}else{last_error="Favourite update failed";error_screen();}}
    if(action==ACT_WATCHED){if(jf_set_played(item.base.id,!item.played)){item.played=!item.played;page_cache_clear();if(item.played)item.resume=0;view.col=std::min(view.col,(int)detail_actions(item).size()-1);}else{last_error="Watched update failed";error_screen();}}
   }
   else if(view.row==1){
    if(view.col==0&&sources.n_sources){source=(source+1)%sources.n_sources;audio_slot=default_audio();sub_slot=-1;}
    if(view.col==1&&tracks&&tracks->n_audio)audio_slot=(audio_slot+1)%tracks->n_audio;
    if(view.col==2&&tracks&&tracks->n_subs)sub_slot=(sub_slot+2)%(tracks->n_subs+1)-1;
   }
   else if(view.row==2&&view.col<cast){person=item.people[view.col];return true;}
  }

 }
}
struct Location {std::string parent,title,query;int mode=0,start=0,selected=0,first=0;std::string breadcrumb{};std::string letter{};bool letters=false;};
static Location child_location(const Location& current,const Item& item,bool home){
 Location next{item.base.id,home?item.base.name:current.title,"",0,0,0};
 if(!home){std::string parent=current.breadcrumb;if(!strcmp(item.base.type,"Series"))next.breadcrumb=std::string(item.base.name)+" / Seasons";
 else if(!strcmp(item.base.type,"Season")){const std::string suffix=" / Seasons";if(parent.size()>=suffix.size()&&parent.compare(parent.size()-suffix.size(),suffix.size(),suffix)==0)parent.resize(parent.size()-suffix.size());next.breadcrumb=parent+" / "+item.base.name+" / Episodes";}
 else next.breadcrumb=(parent.empty()?"":parent+" / ")+item.base.name;}
 return next;
}
static uint64_t peek_time(){timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);return (uint64_t)now.tv_sec*1000000+now.tv_nsec/1000;}
template<class Background> static bool quick_peek_screen(const Item& initial,const PeekOrigin& origin,Background background){
 UiInputScope input_scope;peek_anim animation{};peek_open(&animation,peek_time());
 Item item=initial;load_art(item.base.id);facts_request(item);
 // The page underneath does not change while the peek is up: draw it once.
 background();std::vector<uint32_t> under(framebuffer(),framebuffer()+WIDTH*HEIGHT);
 // Use already fetched library facts and cached artwork: opening never waits
 // for a detail request. Input stays with the peek until it has closed.
 while(peek_tick(&animation,peek_time())!=PEEK_OFF){
  memcpy(framebuffer(),under.data(),under.size()*4);
  std::string status;facts_copy(item.base.id,item,status);draw_quick_peek(item,origin,peek_progress(&animation,peek_time()),status);present();auto b=press();
  if(animation.phase==PEEK_CLOSING)continue;
  if(b&(ORBIS_PAD_BUTTON_TRIANGLE|ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_UP|ORBIS_PAD_BUTTON_DOWN|ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT|ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1)){peek_close(&animation,peek_time());continue;}
  if(b&ORBIS_PAD_BUTTON_CROSS)return true;
  usleep(1000);
 }
 return false;
}
// Classic Home rows, in the PS3 order: My Media, Continue Watching, Next Up,
// then Latest for the first movie, show and music library.
static bool load_home(HomeView& home,bool quiet=false){
 if(!quiet)notice("Loading Home...");Page views;if(!fetch_page("",0,"",0,views))return false;
 HomeView next;next.focus=home.focus;next.header=home.header;
 auto add=[&](const std::string& title,HomeShape shape,Page& page){if(page.items.empty())return;HomeRow row;row.title=title;row.shape=shape;row.page=page;for(const auto& old:home.rows)if(old.title==title)row.col=old.col;next.rows.push_back(row);};
 add("My Media",SHAPE_LIBRARY,views);
 Page row;if(fetch_home_row(HOME_CONTINUE,Item{},row))add("Continue Watching",SHAPE_WIDE,row);
 row={};if(fetch_home_row(HOME_NEXTUP,Item{},row))add("Next Up",SHAPE_WIDE,row);
 bool movies=false,shows=false,music=false;
 for(const auto& library:views.items){
  bool want=(library.collection=="movies"&&!movies)||(library.collection=="tvshows"&&!shows)||(library.collection=="music"&&!music);if(!want)continue;
  movies|=library.collection=="movies";shows|=library.collection=="tvshows";music|=library.collection=="music";
  row={};if(fetch_home_row(HOME_LATEST,library,row))add("Latest "+std::string(library.base.name),library.collection=="music"?SHAPE_SQUARE:SHAPE_POSTER,row);
 }
 // The screensaver's slides, as on the PS3: Continue Watching, Next Up, then Latest.
 std::vector<SaverItem> saver;for(const auto& r:next.rows)if(r.shape!=SHAPE_LIBRARY)for(size_t c=0;c<r.page.items.size()&&c<4;++c)saver.push_back({r.page.items[c],r.title});screensaver_set_items(saver);
 next.loaded=true;next.focus=std::min(next.focus,(int)next.rows.size()-1);if(next.rows.empty())next.focus=-1;
 home=next;home_step(home,true);return true;
}
static Location library_location(const Item& library){
 if(library.collection=="livetv")return {"",library.base.name,"",3,0,0};
 Location next{library.base.id,library.base.name,"",0,0,0};next.letters=library.collection!="playlists";return next;
}
int main(){if(!platform_init()){platform_shutdown();platform_exit(1);}plog("Jellyfin [PS] PS4 build " JF_BUILD_VERSION " (MPV/FFmpeg)");theme_load_setting();boot_begin();boot_pump();if(http_init()<0){boot_leave();last_error="PS4 networking initialization failed. See app.log.";error_screen();app_shutdown();http_end();platform_shutdown();platform_exit(2);}bool authenticated=load_session();boot_pump();std::vector<Location> stack;stack.push_back({"","Home","",0,0,0});Page page;bool reload=true;int header_focus=-1,pending_tab=-1;
 HomeView home;int home_generation=-1;JumpBar jump;
 // Open whatever a card holds: a navigation view, a folder or the detail page.
 auto open_item=[&](const Item& item,bool from_home){
  // Live TV is a page of its own, as on the PS3.
  if(!strcmp(item.base.id,"nav:live")||item.collection=="livetv"){
   int next=livetv_screen([&](const Item& channel){play(channel,-1,-1,"");});
   if(next==-2)error_screen();else if(next>=0)pending_tab=next;
   reload=true;ui_input_sync();return;
  }
  if(!strncmp(item.base.id,"nav:",4)){int mode=!strcmp(item.base.id,"nav:live")?3:!strcmp(item.base.id,"nav:continue")?1:2;stack.push_back({"",item.base.name,"",mode,0,0});reload=true;ui_input_sync();return;}
  if(!strcmp(item.base.type,"CollectionFolder")||!strcmp(item.base.type,"UserView")){stack.push_back(library_location(item));reload=true;ui_input_sync();return;}
  if(is_container(item.base.type)){stack.push_back(child_location(stack.back(),item,from_home));reload=true;ui_input_sync();return;}
  Person person;if(detail_screen(item,person)){Location next{person.id,person.name,"",4,0,0};next.breadcrumb="Filmography";stack.push_back(next);}
  reload=true;ui_input_sync();
 };
 while(running){if(!authenticated||!g_token[0]||g_auth_expired){boot_leave();facts_stop();art_stop();page_cache_stop();authenticated=sign_in();if(!authenticated)break;stack.resize(1);reload=true;home_generation=-1;}if(pending_tab>=0){
  int tab=pending_tab;pending_tab=-1;plog((std::string("Navigation: tab=")+(tab==0?"Home":tab==1?"Search":"Settings")).c_str());
  if(tab==0){stack.resize(1);header_focus=-1;reload=true;}
  else if(tab==1){std::string query;int next=search_screen(query);if(next==3){stack.push_back({"","Search: "+query,query,0,0,0});reload=true;header_focus=-1;}else if(next>=0)pending_tab=next;}
  else{int next=settings_screen();if(next>=0)pending_tab=next;header_focus=-1;reload=true;}
  ui_input_sync();continue;
 }
 const bool at_home=stack.size()==1;
 if(at_home){
  // Rows are kept while browsing; Continue Watching and Next Up change with playback.
  if(reload&&(home_generation!=play_generation||!home.loaded)){
   // On a cold start the rows load behind the boot animation.
   bool ok=false;if(boot_active())boot_run([&]{ok=load_home(home,true);});else ok=load_home(home);
   if(ok)home_generation=play_generation;else{boot_leave();if(g_auth_expired)continue;error_screen();home.loaded=true;}
  }
  reload=false;jump={};
  home_step(home,false);home_request_art(home);
  const int focused_tab=home.focus<0?home.header:-1;boot_tick();if(!boot_covers_home())draw_home(home,focused_tab);boot_overlay();present();boot_ready();
  auto b=press();
  // During the handoff any press skips to the end rather than acting on Home.
  if(boot_active()){if(b)boot_skip();usleep(1000);continue;}if(b){char trace[128];snprintf(trace,sizeof trace,"Controls: events=0x%06x home_row=%d header=%d",b,home.focus,focused_tab);plog(trace);}
  const int rows=(int)home.rows.size();
  if(b&ORBIS_PAD_BUTTON_TOUCH_PAD){home.loaded=false;page_cache_clear();reload=true;ui_input_sync();continue;}
  if(b&(ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1)){pending_tab=(b&ORBIS_PAD_BUTTON_L1)?2:1;ui_input_sync();continue;}
  if(b&ORBIS_PAD_BUTTON_OPTIONS){pending_tab=2;ui_input_sync();continue;}
  if(b&ORBIS_PAD_BUTTON_SQUARE){stack.push_back({"","Favourites","",2,0,0});reload=true;ui_input_sync();continue;}
  if(b&ORBIS_PAD_BUTTON_UP){if(home.focus>0)home.focus--;else if(home.focus==0){home.focus=-1;home.header=1;}}
  else if(b&ORBIS_PAD_BUTTON_DOWN){if(home.focus<rows-1)home.focus++;}
  else if(home.focus<0){
   if(b&ORBIS_PAD_BUTTON_LEFT&&home.header>0)home.header--;
   else if(b&ORBIS_PAD_BUTTON_RIGHT&&home.header<2)home.header++;
   else if(b&ORBIS_PAD_BUTTON_CROSS){if(home.header==0){if(rows)home.focus=0;}else{pending_tab=home.header;ui_input_sync();}}
   else if(b&ORBIS_PAD_BUTTON_CIRCLE&&rows)home.focus=0;
  }
  else if(home.focus<rows){
   auto& row=home.rows[home.focus];const int n=(int)row.page.items.size();
   if(b&ORBIS_PAD_BUTTON_LEFT&&row.col>0)row.col--;
   else if(b&ORBIS_PAD_BUTTON_RIGHT&&row.col<n-1)row.col++;
   else if(b&ORBIS_PAD_BUTTON_CROSS&&n){open_item(row.page.items[row.col],true);continue;}
   else if(b&ORBIS_PAD_BUTTON_TRIANGLE&&n&&row.shape!=SHAPE_LIBRARY){
    const Item item=row.page.items[row.col];const auto origin=home_focus_rect(home);
    if(quick_peek_screen(item,origin,[&]{draw_home(home,-1);}))open_item(item,true);
    continue;
   }
  }
  // present() already waits for the display: no extra sleep, or Home runs at half rate.
  continue;
 }
 auto& loc=stack.back();if(reload){if(!page_ready(loc.parent,loc.start,loc.query,loc.mode,loc.letter))notice("Loading "+loc.title+"...");if(!fetch_page(loc.parent,loc.start,loc.query,loc.mode,page,loc.letter)){error_screen();page={};}
  else{if(loc.start+60<page.total)prefetch_page(loc.parent,loc.start+60,loc.query,loc.mode,loc.letter);if(loc.start>=60)prefetch_page(loc.parent,loc.start-60,loc.query,loc.mode,loc.letter);}
  loc.selected=std::min(loc.selected,std::max(0,(int)page.items.size()-1));reload=false;wait_release();}
 jump.show=loc.letters;jump.filter=loc.letter;if(!jump.show)jump.active=false;
 auto grid=library_grid(page,false);int columns=grid.cols;loc.first=library_window(grid,loc.selected,loc.first);int first=loc.first;
 for(int k=first;k<std::min(first+columns*grid.rows,(int)page.items.size());++k)load_art(page.items[k].base.id);
 draw_library(page,loc.title,loc.start,loc.selected,false,header_focus,first,loc.breadcrumb,&jump);
 present();auto b=press();int count=(int)page.items.size();if(b){char trace[128];snprintf(trace,sizeof trace,"Controls: events=0x%06x selected=%d mode=%d header=%d",b,loc.selected,loc.mode,header_focus);plog(trace);}
 // Route one action per input sample. Never keep a reference into stack after
 // pushing/popping a screen, and never stack the current shortcut view again.
 if(jump.active){
  // PS3 jump bar: Up/Down pick a letter, X lists names starting with it.
  if(b&ORBIS_PAD_BUTTON_UP)jump.sel=(jump.sel+JUMP_ENTRIES-1)%JUMP_ENTRIES;
  else if(b&ORBIS_PAD_BUTTON_DOWN)jump.sel=(jump.sel+1)%JUMP_ENTRIES;
  else if(b&(ORBIS_PAD_BUTTON_RIGHT|ORBIS_PAD_BUTTON_CIRCLE)){jump.active=false;ui_input_sync();}
  else if(b&ORBIS_PAD_BUTTON_CROSS){loc.letter=jump_letter(jump.sel);loc.start=loc.selected=loc.first=0;jump.active=false;reload=true;ui_input_sync();continue;}
  continue;
 }
 if(b&ORBIS_PAD_BUTTON_TOUCH_PAD){stack.resize(1);header_focus=-1;reload=true;ui_input_sync();continue;}
 if(b&(ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1)){int current=loc.query.empty()?0:1;pending_tab=(current+((b&ORBIS_PAD_BUTTON_L1)?2:1))%3;header_focus=-1;ui_input_sync();continue;}
 if((b&ORBIS_PAD_BUTTON_OPTIONS)||(header_focus==2&&(b&ORBIS_PAD_BUTTON_CROSS))){pending_tab=2;ui_input_sync();continue;}
 if(header_focus==1&&(b&ORBIS_PAD_BUTTON_CROSS)){pending_tab=1;ui_input_sync();continue;}
 if((b&ORBIS_PAD_BUTTON_TRIANGLE)&&header_focus<0&&count){
  const Item item=page.items[loc.selected];const Location view=loc;const Page shown=page;
  if(quick_peek_screen(item,library_focus_rect(grid,view.selected,view.first,false),[&]{draw_library(shown,view.title,view.start,view.selected,false,-1,view.first,view.breadcrumb,&jump);}))open_item(item,false);
  continue;
 }
 if(header_focus==0&&(b&ORBIS_PAD_BUTTON_CROSS)){stack.resize(1);header_focus=-1;reload=true;ui_input_sync();continue;}
 if(b&ORBIS_PAD_BUTTON_CIRCLE){if(header_focus>=0){header_focus=-1;}else if(!loc.letter.empty()){loc.letter.clear();loc.start=loc.selected=loc.first=0;reload=true;}else{stack.pop_back();reload=true;}ui_input_sync();continue;}
 if(b&ORBIS_PAD_BUTTON_SQUARE){
  if(loc.mode!=2||!loc.parent.empty()){stack.push_back({"","Favourites","",2,0,0});reload=true;}
  header_focus=-1;ui_input_sync();continue;
 }
 if(header_focus>=0){
  if(b&ORBIS_PAD_BUTTON_LEFT)header_focus=(header_focus+2)%3;
  else if(b&ORBIS_PAD_BUTTON_RIGHT)header_focus=(header_focus+1)%3;
  else if(b&ORBIS_PAD_BUTTON_DOWN)header_focus=-1;
  continue;
 }
 if(b&ORBIS_PAD_BUTTON_UP){if(loc.selected<columns){if(loc.start>=60){loc.start-=60;loc.selected=std::min(59,(59/columns)*columns+loc.selected%columns);loc.first=0;reload=true;}else header_focus=0;}else loc.selected-=columns;}
 else if(b&ORBIS_PAD_BUTTON_DOWN){if(loc.selected+columns<count)loc.selected+=columns;else if((loc.selected/columns+1)*columns<count)loc.selected=count-1;else if(loc.start+60<page.total){loc.start+=60;loc.selected%=columns;loc.first=0;reload=true;}}
 else if(b&ORBIS_PAD_BUTTON_LEFT){
  // Left at the first column opens the letter rail on library roots (PS3).
  if(jump.show&&(loc.selected%columns==0||!count)){jump.active=true;jump.sel=!loc.letter.empty()?jump_entry(loc.letter=="#"?"":loc.letter):count?jump_entry(page.items[loc.selected].base.name):1;ui_input_sync();}
  else if(loc.selected==0&&loc.start>=60){loc.start-=60;loc.selected=59;reload=true;}else loc.selected=std::max(0,loc.selected-1);}
 else if(b&ORBIS_PAD_BUTTON_RIGHT){if(loc.selected==count-1&&loc.start+60<page.total){loc.start+=60;loc.selected=0;reload=true;}else loc.selected=std::min(std::max(0,count-1),loc.selected+1);}
 else if((b&ORBIS_PAD_BUTTON_CROSS)&&count){open_item(page.items[loc.selected],false);continue;}
 }boot_leave();livetv_stop();player_close();facts_stop();art_stop();app_shutdown();http_end();platform_shutdown();platform_exit(0);}
