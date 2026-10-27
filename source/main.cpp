#include "app.h"
#include "thread_priority.h"
#include "mono_clock.h"
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
#include "buffering.h"
#include "music_viz.h"
#include "language.h"
#include "updater.h"
#include "../vendor/ps3/source/player/core/picture_size.h"
#include <orbis/_types/pad.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <list>
#include <functional>
#include "../vendor/ps3/source/ui/render/peek.h"
#ifndef JF_BUILD_VERSION
#define JF_BUILD_VERSION "host"
#endif
static bool running=true;
static int play_generation=0;
// `title` is English: the tab is picked by it, then it is shown translated.
static void header(const std::string& title){app_header(T(title.c_str()),title=="Settings"?"Settings":"Home");}
static void notice(const std::string& s){header("");face_text(64,86,T(s.c_str()),g_theme.text,24,TextFace::Classic);present();}
static uint32_t press(){return ui_input();}
static void wait_release(){ui_input_sync();}
static uint64_t now_ms(){return mono_ms();}
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
// ---- Settings ----------------------------------------------------------------
// The PS3's two levels: categories, then a category's rows. Each row is
// English at its use (translated when drawn); its value, X and Left / Right
// are the switches below.
enum SettingSection { SEC_PLAYBACK, SEC_DISPLAY, SEC_ACCOUNT, SEC_SYSTEM, SEC__COUNT };
enum SettingId { SET_QUALITY, SET_HD1080, SET_RESUME, SET_METHOD, SET_THEME, SET_LANGUAGE, SET_SAVER, SET_SAVER_DELAY, SET_SIGN_OUT, SET_UPDATE, SET_ABOUT, SET_DEBUG, SET_EXIT };
struct SettingDef { SettingId id; SettingSection section; const char* label; const char* description; bool action; };
static const SettingDef SETTING_ROWS[]={
 {SET_QUALITY,SEC_PLAYBACK,"Quality","The highest bitrate the server streams at. Lower it if playback\nstops to buffer; raise it for a sharper picture on a fast network.",false},
 {SET_HD1080,SEC_PLAYBACK,"1080p Playback (Alpha)","Ask the server for full 1920x1080 video instead of 720p.\nSharper, but needs more bandwidth. Still experimental.",false},
 {SET_RESUME,SEC_PLAYBACK,"Resume playback","Start films and episodes where you left off.\nOff always plays them from the beginning.",false},
 {SET_METHOD,SEC_PLAYBACK,"Playback method","Auto plays files the PS4 can decode straight from the server, and has the\nserver convert the rest. Always transcode converts everything (slower to start).",false},
 {SET_THEME,SEC_DISPLAY,"Theme","The interface colour theme.\nJellywave is the default; extra .ini themes appear here too.",false},
 {SET_LANGUAGE,SEC_DISPLAY,"Language","The language the menus are shown in.\nAuto follows the PS4's own language setting.",false},
 {SET_SAVER,SEC_DISPLAY,"Screensaver","After a few idle minutes, Artwork fills the screen with backdrops\nfrom Home. Off never starts it.",false},
 {SET_SAVER_DELAY,SEC_DISPLAY,"Screensaver Delay","How long the menus wait, untouched, before the screensaver\nstarts: 30 seconds to 30 minutes.",false},
 {SET_SIGN_OUT,SEC_ACCOUNT,"Log Out","Sign out of this Jellyfin account.\nYou will need to log in again to browse your library.",true},
 {SET_UPDATE,SEC_SYSTEM,"Software Update","Checked at every launch. X checks again, and downloads a new version\nto /data/pkg when there is one; install it from the Package Installer.",true},
 {SET_ABOUT,SEC_SYSTEM,"About","The version, who made the app, and the licences of\neverything in it.",true},
 {SET_DEBUG,SEC_SYSTEM,"Debug Logging","Write a diagnostic log to /data/JellyFin-PS4-logs (never passwords\nor tokens). Turn it on when reporting a problem.",false},
 {SET_EXIT,SEC_SYSTEM,"Exit application","Close Jellyfin [PS] and return to the PS4 home screen.",true},
};
static const struct { const char* label; SettingsIcon icon; } SETTING_SECTIONS[SEC__COUNT]={{"Playback",SICON_PLAYBACK},{"Display",SICON_DISPLAY},{"Account",SICON_ACCOUNT},{"System",SICON_SYSTEM}};
static std::vector<const SettingDef*> section_rows(int section){std::vector<const SettingDef*> rows;for(const auto& r:SETTING_ROWS)if(r.section==section)rows.push_back(&r);return rows;}
static SettingsCategoryView section_view(int section){
 SettingsCategoryView v;v.label=T(SETTING_SECTIONS[section].label);v.icon=SETTING_SECTIONS[section].icon;
 for(const auto* r:section_rows(section)){if(!v.summary.empty())v.summary+="  \xC2\xB7  ";v.summary+=T(r->label);}
 return v;
}
static SettingsRowView setting_view(const SettingDef& r){
 SettingsRowView v;v.label=T(r.label);v.description=T(r.description);v.action=r.action;
 switch(r.id){
 case SET_QUALITY:v.value=TF("%d Mbps",settings.bitrate/1000000);break;
 case SET_HD1080:v.value=T(settings.hd1080?"On":"Off");v.lit=settings.hd1080;break;
 case SET_RESUME:v.value=T(settings.resume?"On":"Off");v.lit=settings.resume;break;
 case SET_METHOD:v.value=T(settings.method==PLAY_DIRECT_FILE?"Direct file":settings.method==PLAY_TRANSCODE?"Always transcode":"Auto");break;
 case SET_THEME:v.value=theme_current_name();break;
 case SET_LANGUAGE:v.value=language_label();break;
 case SET_SAVER:v.value=T(settings.saver?"Artwork":"Off");v.lit=settings.saver;break;
 case SET_SAVER_DELAY:v.value=screensaver_delay_label();v.dim=!settings.saver;break;
 case SET_DEBUG:v.value=T(settings.debug_log?"On":"Off");v.lit=settings.debug_log;break;
 case SET_UPDATE:{const int s=update_state();v.value=s==UPD_CHECKING?T("Checking..."):s==UPD_CURRENT?T("Up to date"):s==UPD_AVAILABLE?TF("Version %s available",update_version().c_str()):s==UPD_FAILED?T("Check failed"):"";v.lit=s==UPD_AVAILABLE;v.action=false;break;}
 default:break;
 }
 return v;
}
// Left / Right (and X on a value row). Returns false when nothing changed.
static bool setting_step(SettingId id,int dir){
 switch(id){
 case SET_QUALITY:{const int rates[]={2000000,4000000,8000000,12000000,20000000};int n=0;for(int i=0;i<5;i++)if(rates[i]==settings.bitrate)n=i;settings.bitrate=rates[(n+dir+5)%5];break;}
 case SET_HD1080:settings.hd1080=!settings.hd1080;break;
 case SET_RESUME:settings.resume=!settings.resume;break;
 case SET_METHOD:settings.method=(settings.method+dir+3)%3;break;
 case SET_THEME:theme_step(dir);return true;   // the theme keeps its own file
 case SET_LANGUAGE:language_step(dir);break;
 case SET_SAVER:settings.saver=!settings.saver;break;
 case SET_SAVER_DELAY:if(!settings.saver)return false;screensaver_delay_step(dir);break;
 case SET_DEBUG:if(settings.debug_log)plog("Debug logging turned off");settings.debug_log=!settings.debug_log;if(settings.debug_log)plog("Debug logging turned on");break;
 default:return false;
 }
 if(!save_settings()){last_error=T("Unable to save settings");error_screen();}
 return true;
}
static bool confirm(const char* question,const char* detail,const char* yes,const std::function<void()>& under){
 wait_release();for(;;){under();draw_confirm(T(question),T(detail),yes);present();const auto b=press();if(b&ORBIS_PAD_BUTTON_CROSS){wait_release();return true;}if(b&ORBIS_PAD_BUTTON_CIRCLE){wait_release();return false;}}
}
// Software Update: offer the new version, download it with a progress bar,
// then say where it is and how to install it. `under` redraws the screen
// beneath the panel.
static void update_screen(const std::function<void()>& under){
 UiInputScope input_scope;wait_release();
 // The offer.
 for(;;){under();draw_update_panel(TF("Version %s is available",update_version().c_str()),T("Download it now? You install it afterwards from the Package Installer."),-1,"Download","Not now");present();
  const auto b=press();if(b&ORBIS_PAD_BUTTON_CIRCLE){wait_release();return;}if(b&ORBIS_PAD_BUTTON_CROSS)break;}
 for(;;){
  update_download_start();
  while(update_download_state()==DL_RUNNING){
   uint64_t got=0,total=0;update_download_progress(got,total);
   char sizes[64];snprintf(sizes,sizeof sizes,total?"%.1f MB / %.1f MB":"%.1f MB",got/1048576.0,total/1048576.0);
   under();draw_update_panel(T("Downloading update"),sizes,total?(float)got/total:0.0f,nullptr,"Cancel");present();
   if(press()&ORBIS_PAD_BUTTON_CIRCLE)update_download_cancel();
  }
  const int s=update_download_state();if(s==DL_IDLE){wait_release();return;}   // cancelled
  const bool ok=s==DL_DONE;wait_release();
  for(;;){
   under();
   if(ok)draw_update_panel(T("Update downloaded"),std::string(T("Saved to"))+" " UPDATE_FILE "\n"+T("Install it from the Package Installer, then start Jellyfin [PS] again."),-1,"OK",nullptr);
   else{draw_update_panel(T("Download failed"),T(update_error().c_str()),-1,"Try again","Close");}
   present();const auto b=press();
   if(ok&&(b&(ORBIS_PAD_BUTTON_CROSS|ORBIS_PAD_BUTTON_CIRCLE))){wait_release();return;}
   if(!ok&&(b&ORBIS_PAD_BUTTON_CIRCLE)){wait_release();return;}
   if(!ok&&(b&ORBIS_PAD_BUTTON_CROSS))break;
  }
 }
}
// Returns the tab L1 / R1 asked for (0 Home, 1 Search), else -1.
static int settings_screen(){
 UiInputScope input_scope;int section=-1,category=0,row=0;wait_release();
 for(;;){
  if(section<0){
   std::vector<SettingsCategoryView> categories;for(int i=0;i<SEC__COUNT;++i)categories.push_back(section_view(i));
   const std::string account=std::string(T("Account"))+": "+(g_username[0]?g_username:T("(unknown)"))+"  \xC2\xB7  "+(g_server[0]?g_server:T("(no server)"));
   std::string version=JF_BUILD_VERSION;if(version.size()>1&&version[0]=='0')version.erase(0,1);
   draw_settings_categories(categories,category,account,TF("JellyFin [PS] %s \xC2\xB7 built %s",version.c_str(),__DATE__));present();
   const auto b=press();if(b&ORBIS_PAD_BUTTON_L1)return 1;if(b&ORBIS_PAD_BUTTON_R1)return 0;
   if(b&ORBIS_PAD_BUTTON_UP)category=(category+SEC__COUNT-1)%SEC__COUNT;if(b&ORBIS_PAD_BUTTON_DOWN)category=(category+1)%SEC__COUNT;
   if(b&(ORBIS_PAD_BUTTON_CROSS|ORBIS_PAD_BUTTON_RIGHT)){section=category;row=0;wait_release();}
   if(b&ORBIS_PAD_BUTTON_CIRCLE)return -1;
   continue;
  }
  const auto defs=section_rows(section);std::vector<SettingsRowView> rows;for(const auto* d:defs)rows.push_back(setting_view(*d));
  const auto category_view=section_view(section);
  draw_settings_rows(category_view,rows,row);present();
  const auto b=press();if(b&ORBIS_PAD_BUTTON_L1)return 1;if(b&ORBIS_PAD_BUTTON_R1)return 0;
  const int n=(int)defs.size();
  if(b&ORBIS_PAD_BUTTON_UP)row=(row+n-1)%n;if(b&ORBIS_PAD_BUTTON_DOWN)row=(row+1)%n;
  if(b&ORBIS_PAD_BUTTON_CIRCLE){section=-1;wait_release();continue;}
  const SettingDef& d=*defs[row];
  if(b&(ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT))setting_step(d.id,b&ORBIS_PAD_BUTTON_LEFT?-1:1);
  if(b&ORBIS_PAD_BUTTON_CROSS){
   if(!d.action){setting_step(d.id,1);continue;}
   if(d.id==SET_ABOUT){about_screen();wait_release();}
   if(d.id==SET_UPDATE){if(update_state()==UPD_AVAILABLE)update_screen([&]{draw_settings_rows(category_view,rows,row);});else update_check_start();}
   if(d.id==SET_SIGN_OUT&&confirm("Log out of this account?","You'll need to sign in again to browse your library.","Log Out",[&]{draw_settings_rows(category_view,rows,row);})){facts_stop();art_stop();page_cache_stop();livetv_stop();logout();return -1;}
   if(d.id==SET_EXIT){running=false;return -1;}
  }
 }
}
static int search_screen(std::string& query){
 UiInputScope input_scope;
 for(;;){draw_search(query);present();auto b=press();
  if(b&ORBIS_PAD_BUTTON_L1)return 0;
  if(b&ORBIS_PAD_BUTTON_R1)return 2;
  if(b&ORBIS_PAD_BUTTON_CIRCLE)return -1;
  if(b&ORBIS_PAD_BUTTON_CROSS){if(keyboard(T("Search Jellyfin"),query)&&!query.empty())return 3;ui_input_sync();}

 }
}
// How playback ended: stopped (Circle, an error), the end of the stream, or a
// skip to the next / previous track of an album.
enum PlayEnd { PLAY_STOPPED, PLAY_ENDED, PLAY_NEXT, PLAY_PREVIOUS };
// Where a track sits in its album, for the Now Playing screen.
struct MusicQueue { int track=0,tracks=0; std::string up_next; };
static PlayEnd play(Item item,int audio,int subtitle,const std::string& source,const std::string& audio_label="",const std::string& subtitle_label="",const MusicQueue* queue=nullptr){
 UiInputScope input_scope;ScreensaverInhibit no_saver;++play_generation;page_cache_clear();std::string url,session,source_id;
 const bool live=!strcmp(item.base.type,"TvChannel"),audio_only=!strcmp(item.base.type,"Audio");
 // The PS3 player HUD: title, play state, progress and times. Left/Right move
 // a scrub target along the bar (holding accelerates); the seek happens once
 // the target settles, so a run of presses is one request to the server.
 PlayerOsd osd;osd.live=live;osd.audio_only=audio_only;osd.duration=item.duration;osd.audio=audio_label;osd.subtitles=subtitle_label;
 if(!strcmp(item.base.type,"Episode")&&!item.series.empty()){osd.title=item.series;char code[32]="";if(item.episode)snprintf(code,sizeof code,"S%d:E%d - ",item.season,item.episode);osd.subtitle=code+std::string(item.base.name);}
 else if(audio_only){osd.title=item.base.name;osd.subtitle=item.artist;osd.album=item.album;if(item.year)osd.album+=(osd.album.empty()?"":"  \xC2\xB7  ")+std::to_string(item.year);if(queue){osd.track=queue->track;osd.tracks=queue->tracks;osd.up_next=queue->up_next;}}
 else{osd.title=item.base.name;if(item.year)osd.subtitle=std::to_string(item.year);}
 // Music: the track's own cover, else its album's.
 if(audio_only){load_art(item.base.id);if(!item.album_id.empty())load_art(item.album_id);music_viz_reset();}
 // The PS3 buffering screen from Play to the first frame. Preparing (the
 // server's PlaybackInfo) runs on a worker while the ring turns.
 buffering_begin(item,osd.subtitle.empty()||audio_only?osd.title:osd.title+"  \xC2\xB7  "+osd.subtitle);
 if(settings.debug_log)plog(("Playback: starting; "+resource_report()).c_str());
 const uint64_t play_pressed=mono_ms();bool prepared=false;buffering_run([&]{prepared=playback_url(item,audio,subtitle,source,url,session,source_id);});
 plog(("Playback: PlaybackInfo took "+std::to_string(mono_ms()-play_pressed)+" ms"+(settings.hd1080?" (1080p)":"")).c_str());
 if(!prepared){buffering_finish(false);end_playback(session);error_screen();return PLAY_STOPPED;}
 buffering_step(T("CONNECTING"),false,T("Cancel"));
 if(!player_open(url)){buffering_finish(false);end_playback(session);error_screen();return PLAY_STOPPED;}
 // A channel played directly gets 15 seconds before it falls back to the server.
 const bool remote=playback_is_remote();const int start_limit=remote?15:45;
 bool paused=false,started=false,play_failed=false,cancelled=false,finished=false;PlayEnd skip=PLAY_STOPPED;time_t began=time(nullptr),last_report=began;bool reported=false;
 // A transcode starts at the resume point; the original file is sought to it.
 const bool is_static=playback_is_static();
 uint64_t base=settings.resume&&!is_static?item.resume:0;bool direct_resume_pending=is_static&&settings.resume&&item.resume;
 uint64_t osd_until=now_ms()+5000,target=0,scrub_at=0,hold_until=0;bool scrubbing=false;int scrub_presses=0;
 bool picture=false,picture_move=false;uint64_t viz_toast_until=0;
 uint64_t reconnect_at=0,reconnect_window=0;int reconnects=0;
 wait_release();
 for(;;){
  bool got=player_draw();auto decoder_error=player_error();if(!decoder_error.empty()){last_error=decoder_error;play_failed=true;break;}
  if(got||player_has_audio())started=true;
  if(buffering_active()){
   if(started){
    // Hold the first frame while the ring closes and the screen fades, as
    // the PS3 does, so nothing plays under it.
    plog(("Playback: playing "+std::to_string(mono_ms()-play_pressed)+" ms after Play").c_str());
    player_pause(true);buffering_finish(true);player_pause(false);osd_until=now_ms()+5000;continue;
   }
   if(player_loaded()){buffering_step(T("BUFFERING"),true,T("Cancel"));buffering_progress((float)(player_cache_seconds()/2.0));}
   buffering_frame();present();
   if(press()&ORBIS_PAD_BUTTON_CIRCLE){cancelled=true;break;}
   if(time(nullptr)-began>start_limit){last_error=T("Decoder could not start this stream within 45 seconds");break;}
   continue;
  }
  if(started&&direct_resume_pending){player_seek(item.resume);direct_resume_pending=false;}
  const uint64_t now=now_ms();uint64_t pos=base+player_time(),shown=pos;
  // While an asynchronous seek lands, keep showing where it is going.
  if(hold_until&&now<hold_until&&(pos+3000<target||pos>target+3000))shown=target;else hold_until=0;
  osd.position=shown;osd.target=target;osd.paused=paused;osd.started=started;osd.scrubbing=scrubbing;
  if(audio_only){osd.art=art_state(item.base.id)==1||item.album_id.empty()?item.base.id:item.album_id;float bands[MUSIC_VIZ_BANDS];music_viz_bands(bands);
   static float wave[688];music_viz_wave(wave,688);osd.viz=settings.viz;osd.viz_toast=now<viz_toast_until;draw_music_screen(osd,bands,MUSIC_VIZ_BANDS,wave,688);}
  else if(paused||scrubbing||now<osd_until)draw_player_osd(osd);
  // Reconnecting after a stalled picture, until the first new frame.
  if(reconnect_at&&!player_last_frame_ms()){const std::string r=std::string(T("Reconnecting"))+"...";face_text((WIDTH-face_text_width(r,20,TextFace::Classic))/2,HEIGHT/2-10,r,g_theme.white,20,TextFace::Classic);}
  if(picture){
   auto shift=[](int v,const char* pos,const char* neg){if(!v)return std::string(T("Centred"));const int a=v<0?-v:v;char n[24];snprintf(n,sizeof n," %d.%d%%",a/2,(a%2)*5);return std::string(T(v>0?pos:neg))+n;};
   const char* shape=settings.pic_aspect==PIC_FILL?"Fill":settings.pic_aspect==PIC_STRETCH?"Stretch":"Fit";
   draw_picture_banner(picture_move?std::string(T("Picture position"))+"   "+shift(settings.pic_x,"Right","Left")+"   "+shift(settings.pic_y,"Up","Down")+"   "+T(shape)
    :std::string(T("Picture size"))+"   "+T("Width")+" "+std::to_string(settings.pic_w)+"%   "+T("Height")+" "+std::to_string(settings.pic_h)+"%   "+T(shape),picture_move);
  }
  present();auto b=press();
  // L1 (PS3): the picture's size, position and shape -- Fit / Fill / Stretch --
  // kept for every video, because they correct for the screen, not the film.
  // While the mode is open it has the buttons to itself.
  if(!audio_only&&started&&(picture||(b&ORBIS_PAD_BUTTON_L1))){
   if(!picture){picture=true;picture_move=false;osd_until=0;}
   else if(b&(ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_CROSS|ORBIS_PAD_BUTTON_CIRCLE)){picture=false;save_settings();}
   else{
    if(b&ORBIS_PAD_BUTTON_SQUARE)settings.pic_aspect=pic_aspect_next(settings.pic_aspect);
    if(b&ORBIS_PAD_BUTTON_TRIANGLE){settings.pic_aspect=PIC_FIT;settings.pic_w=settings.pic_h=PIC_DEFAULT;settings.pic_x=settings.pic_y=0;}
    if(b&ORBIS_PAD_BUTTON_R1)picture_move=!picture_move;
    const int dx=(b&ORBIS_PAD_BUTTON_RIGHT)?1:(b&ORBIS_PAD_BUTTON_LEFT)?-1:0,dy=(b&ORBIS_PAD_BUTTON_UP)?1:(b&ORBIS_PAD_BUTTON_DOWN)?-1:0;
    if(picture_move){settings.pic_x=pic_shift_clamp(settings.pic_x+dx);settings.pic_y=pic_shift_clamp(settings.pic_y+dy);}
    else{settings.pic_w=pic_clamp(settings.pic_w+dx);settings.pic_h=pic_clamp(settings.pic_h+dy);}
   }
   player_picture(settings.pic_aspect,settings.pic_w,settings.pic_h,settings.pic_x,settings.pic_y);
   b=0;
  }
  if(b&ORBIS_PAD_BUTTON_CIRCLE){cancelled=true;break;}
  // An album: R1 is the next track; L1 restarts this one, or goes back a track
  // in its first few seconds.
  if(audio_only&&osd.tracks>1&&started){
   if((b&ORBIS_PAD_BUTTON_R1)&&osd.track<osd.tracks){skip=PLAY_NEXT;break;}
   if(b&ORBIS_PAD_BUTTON_L1){if(pos>base+3000||osd.track<=1){player_seek(0);target=base;hold_until=now+1500;}else{skip=PLAY_PREVIOUS;break;}}
  }
  if(b&ORBIS_PAD_BUTTON_CROSS){paused=!paused;player_pause(paused);osd_until=now+5000;}
  // Music: Square chooses the visualizer (kept for next time); video: it hides the bar.
  if((b&ORBIS_PAD_BUTTON_SQUARE)&&audio_only){settings.viz=(settings.viz+1)%VIZ_COUNT;save_settings();viz_toast_until=now+2000;}
  else if(b&ORBIS_PAD_BUTTON_SQUARE)osd_until=now<osd_until?0:now+5000;
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
  if(time(nullptr)-last_report>=10){report_playback("/Progress",item,session,source_id,pos,paused);last_report=time(nullptr);
   // Every 30 s: free memory, so a leak shows before it runs out.
   if(settings.debug_log&&(last_report-began)%30<10)plog(("Playback: "+resource_report()).c_str());}
  if(started&&!paused&&!player_active()&&time(nullptr)-began>2){finished=true;break;}
  // Live TV watchdog. A stream that jumps -- segments expired while it lagged,
  // a provider's stitched ad break -- can leave the picture stopped while the
  // sound goes on (its timestamps went backwards). No new frame for 6 s while
  // playing: reconnect, which starts again at the live edge. Three times a
  // minute at most, so a dead channel cannot loop.
  if(live&&started&&!paused&&!audio_only){const uint64_t last=player_last_frame_ms();
   if(last&&now>last+6000&&(!reconnect_at||now>reconnect_at+8000)){
    if(now>reconnect_window+60000){reconnect_window=now;reconnects=0;}
    if(reconnects<3){++reconnects;reconnect_at=now;plog("Live TV: picture stalled for 6 s; reconnecting");player_close();if(!player_open(url)){last_error=player_error();play_failed=true;break;}}}}
  if(!started&&time(nullptr)-began>start_limit){last_error=T("Decoder could not start this stream within 45 seconds");break;}
  usleep(1000);
 }
 if(buffering_active())buffering_finish(false);
 if(picture)save_settings();
 uint64_t pos=base+player_time();player_close();finish_playback(item,session,source_id,pos,paused);
 // A channel's own stream would not play: try it once more through the server.
 if(remote&&!started&&!cancelled){plog(("Live TV: direct stream did not start ("+last_error+"); through the server").c_str());playback_force_server_once();return play(item,audio,subtitle,source,audio_label,subtitle_label,queue);}
 if((!started&&!cancelled)||play_failed)error_screen();wait_release();
 if(play_failed)return PLAY_STOPPED;return skip!=PLAY_STOPPED?skip:finished?PLAY_ENDED:PLAY_STOPPED;}
// Music plays on through its album: R1 / L1 skip, and when a track ends the
// next one starts by itself.
static void play_music(const Item& first){
 Page album;if(first.album_id.empty()||!fetch_album_tracks(first.album_id,album)||album.items.empty())album.items={first};
 int at=-1;for(size_t i=0;i<album.items.size();++i)if(!strcmp(album.items[i].base.id,first.base.id))at=(int)i;
 if(at<0){album.items.insert(album.items.begin(),first);at=0;}
 const int count=(int)album.items.size();
 for(;;){
  Item track=album.items[at];track.resume=!strcmp(track.base.id,first.base.id)?first.resume:0;
  if(track.album.empty())track.album=first.album;if(track.album_id.empty())track.album_id=first.album_id;if(track.artist.empty())track.artist=first.artist;
  MusicQueue queue;queue.track=at+1;queue.tracks=count;if(at+1<count)queue.up_next=album.items[at+1].base.name;
  const PlayEnd end=play(track,-1,-1,"","","",&queue);
  if(end==PLAY_PREVIOUS)at=std::max(0,at-1);
  else if((end==PLAY_NEXT||end==PLAY_ENDED)&&at+1<count)++at;
  else break;
 }
}
// "English - EAC3 - 5.1 - Default" -> "EAC3 5.1", for the detail page's chip.
static std::string audio_chip(const std::string& label){
 std::vector<std::string> parts;size_t at=0;for(;;){auto next=label.find(" - ",at);parts.push_back(label.substr(at,next==std::string::npos?std::string::npos:next-at));if(next==std::string::npos)break;at=next+3;}
 std::string out;for(size_t i=0;i<parts.size();++i){if((i==0&&parts.size()>1)||parts[i]=="Default"||parts[i]=="Forced"||parts[i]=="External"||parts[i].empty())continue;if(!out.empty())out+=" ";out+=parts[i];}return out;
}
// Returns true with `person` set when a cast card asked for that actor's filmography.
static bool detail_screen(const Item& initial,Person& person){
 UiInputScope input_scope;Item item=initial;notice("Loading item...");const uint64_t asked=mono_ms();if(!fetch_detail(initial,item)){error_screen();return false;}
 plog(("Detail: loaded in "+std::to_string(mono_ms()-asked)+" ms").c_str());
 static JFMediaSources sources;memset(&sources,0,sizeof sources);jellyfin_parse_media_sources(responseBuffer,&sources);request_detail_art(item);
 int source=0,sub_slot=-1;auto default_audio=[&](){auto t=sources.n_sources?&sources.source[source].tracks:nullptr;return t&&t->default_audio>=0&&t->default_audio<t->n_audio?t->default_audio:0;};int audio_slot=default_audio();
 DetailView view;wait_release();
 for(;;){
  auto tracks=sources.n_sources?&sources.source[source].tracks:nullptr;
  view.audio=tracks&&tracks->n_audio?tracks->audio[audio_slot].label:T("Server default");
  view.subtitles=tracks&&sub_slot>=0?tracks->subs[sub_slot].label:T("Off");
  view.version=sources.n_sources?sources.source[source].label:T("Server default");
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
     if(!strcmp(target.base.type,"Audio"))play_music(target);else play(target,tracks&&tracks->n_audio&&audio_slot!=default_audio()?tracks->audio[audio_slot].index:-1,tracks&&sub_slot>=0?tracks->subs[sub_slot].index:-1,sources.n_sources?sources.source[source].id:"",audio_name.empty()?view.audio:audio_name,sub_name);
     // Resume point and watched state change with playback.
     Item fresh;if(fetch_detail(item,fresh)){fresh.people=fresh.people.empty()?item.people:fresh.people;item=fresh;request_detail_art(item);}
     view.col=0;wait_release();continue;
    }
    if(action==ACT_FAVOURITE){if(jf_set_favourite(item.base.id,!item.favourite)){item.favourite=!item.favourite;page_cache_clear();}else{last_error=T("Favourite update failed");error_screen();}}
    if(action==ACT_WATCHED){if(jf_set_played(item.base.id,!item.played)){item.played=!item.played;page_cache_clear();if(item.played)item.resume=0;view.col=std::min(view.col,(int)detail_actions(item).size()-1);}else{last_error=T("Watched update failed");error_screen();}}
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
static uint64_t peek_time(){return mono_us();}
template<class Background> static bool quick_peek_screen(const Item& initial,const PeekOrigin& origin,Background background){
 UiInputScope input_scope;peek_anim animation{};peek_open(&animation,peek_time());
 Item item=initial;load_art(item.base.id);facts_request(item);
 // The page underneath does not change while the peek is up: draw it once.
 background();std::vector<uint32_t> under(framebuffer(),framebuffer()+WIDTH*HEIGHT);
 // Use already fetched library facts and cached artwork: opening never waits
 // for a detail request. Input stays with the peek until it has closed.
 while(peek_tick(&animation,peek_time())!=PEEK_OFF){
  std::string status;facts_copy(item.base.id,item,status);draw_quick_peek(item,origin,peek_progress(&animation,peek_time()),status,under.data());present();auto b=press();
  if(animation.phase==PEEK_CLOSING)continue;
  if(b&(ORBIS_PAD_BUTTON_TRIANGLE|ORBIS_PAD_BUTTON_CIRCLE|ORBIS_PAD_BUTTON_UP|ORBIS_PAD_BUTTON_DOWN|ORBIS_PAD_BUTTON_LEFT|ORBIS_PAD_BUTTON_RIGHT|ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1)){peek_close(&animation,peek_time());continue;}
  if(b&ORBIS_PAD_BUTTON_CROSS)return true;
  usleep(1000);
 }
 return false;
}
// Classic Home rows, in the PS3 order: My Media, Continue Watching, Next Up,
// then Latest for the first movie, show and music library.
// One Home row's request, run on its own thread.
struct RowJob{HomeRowKind kind;Item library;std::string title;HomeShape shape;Page page{};bool ok=false,threaded=false;pthread_t thread{};
 RowJob(HomeRowKind k,const Item& l,const std::string& t,HomeShape s):kind(k),library(l),title(t),shape(s){}};
static void* row_worker(void* p){background_priority();auto job=(RowJob*)p;job->ok=fetch_home_row(job->kind,job->library,job->page);return nullptr;}
static void row_start(RowJob& job){job.threaded=pthread_create(&job.thread,nullptr,row_worker,&job)==0;if(!job.threaded)row_worker(&job);}
static void row_join(RowJob& job){if(job.threaded)pthread_join(job.thread,nullptr);job.threaded=false;}
static bool load_home(HomeView& home,bool quiet=false){
 if(!quiet)notice("Loading Home...");
 // The rows are separate requests: they run side by side rather than one
 // after another. Continue Watching and Next Up need nothing from the
 // library list, so they start before it.
 std::list<RowJob> jobs;
 jobs.emplace_back(HOME_CONTINUE,Item{},T("Continue Watching"),SHAPE_WIDE);row_start(jobs.back());
 jobs.emplace_back(HOME_NEXTUP,Item{},T("Next Up"),SHAPE_WIDE);row_start(jobs.back());
 Page views;if(!fetch_page("",0,"",0,views)){for(auto& job:jobs)row_join(job);return false;}
 bool movies=false,shows=false,music=false;
 for(const auto& library:views.items){
  bool want=(library.collection=="movies"&&!movies)||(library.collection=="tvshows"&&!shows)||(library.collection=="music"&&!music);if(!want)continue;
  movies|=library.collection=="movies";shows|=library.collection=="tvshows";music|=library.collection=="music";
  jobs.emplace_back(HOME_LATEST,library,TF("Latest %s",library.base.name),library.collection=="music"?SHAPE_SQUARE:SHAPE_POSTER);row_start(jobs.back());
 }
 HomeView next;next.focus=home.focus;next.header=home.header;
 auto add=[&](const std::string& title,HomeShape shape,Page& page){if(page.items.empty())return;HomeRow row;row.title=title;row.shape=shape;row.page=page;for(const auto& old:home.rows)if(old.title==title)row.col=old.col;next.rows.push_back(row);};
 add(T("My Media"),SHAPE_LIBRARY,views);
 for(auto& job:jobs){row_join(job);if(job.ok)add(job.title,job.shape,job.page);}
 // The screensaver's slides, as on the PS3: Continue Watching, Next Up, then Latest.
 std::vector<SaverItem> saver;for(const auto& r:next.rows)if(r.shape!=SHAPE_LIBRARY)for(size_t c=0;c<r.page.items.size()&&c<4;++c)saver.push_back({r.page.items[c],r.title});screensaver_set_items(saver);
 next.loaded=true;next.focus=std::min(next.focus,(int)next.rows.size()-1);if(next.rows.empty())next.focus=-1;
 home=next;home_step(home,true);return true;
}
static Location library_location(const Item& library){
 if(library.collection=="livetv")return {"",library.base.name,"",3,0,0};
 Location next{library.base.id,library.base.name,"",0,0,0};next.letters=library.collection!="playlists";return next;
}
int main(){if(!platform_init()){platform_shutdown();platform_exit(1);}plog("Jellyfin [PS] PS4 build " JF_BUILD_VERSION " (MPV/FFmpeg)");theme_load_setting();language_apply();boot_begin();boot_pump();if(http_init()<0){boot_leave();last_error=T("PS4 networking initialization failed. See app.log.");error_screen();app_shutdown();http_end();platform_shutdown();platform_exit(2);}bool authenticated=load_session();boot_pump();update_check_start(8000);std::vector<Location> stack;stack.push_back({"","Home","",0,0,0});Page page;bool reload=true;int header_focus=-1,pending_tab=-1;
 HomeView home;int home_generation=-1;JumpBar jump;
 bool update_offered=false;
 std::vector<uint32_t> boot_still;
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
  Person person;if(detail_screen(item,person)){Location next{person.id,person.name,"",4,0,0};next.breadcrumb=T("Filmography");stack.push_back(next);}
  reload=true;ui_input_sync();
 };
 while(running){if(!authenticated||!g_token[0]||g_auth_expired){boot_leave();facts_stop();art_stop();page_cache_stop();authenticated=sign_in();if(!authenticated)break;stack.resize(1);reload=true;home_generation=-1;}if(pending_tab>=0){
  int tab=pending_tab;pending_tab=-1;plog((std::string("Navigation: tab=")+(tab==0?"Home":tab==1?"Search":"Settings")).c_str());
  if(tab==0){stack.resize(1);header_focus=-1;reload=true;}
  else if(tab==1){std::string query;int next=search_screen(query);if(next==3){stack.push_back({"",TF("Search: %s",query.c_str()),query,0,0,0});reload=true;header_focus=-1;}else if(next>=0)pending_tab=next;}
  else{int next=settings_screen();if(next>=0)pending_tab=next;header_focus=-1;reload=true;}
  ui_input_sync();continue;
 }
 const bool at_home=stack.size()==1;
 if(at_home){
  // Rows are kept while browsing; Continue Watching and Next Up change with playback.
  if(reload&&(home_generation!=play_generation||!home.loaded)){
   // On a cold start the rows load behind the boot animation.
   // On a cold start the artwork on screen is fetched and sized here too, while
   // the mark breathes, so the handoff draws nothing new.
   bool ok=false;if(boot_active())boot_run([&]{ok=load_home(home,true);if(ok)home_prepare_art(home,1500);});else ok=load_home(home);
   if(ok)home_generation=play_generation;else{boot_leave();if(g_auth_expired)continue;error_screen();home.loaded=true;}
  }
  reload=false;jump={};
  // A newer version found by the startup check: offer it once, over Home.
  if(!update_offered&&!boot_active()&&update_state()==UPD_AVAILABLE){update_offered=true;update_screen([&]{draw_home(home,home.focus<0?home.header:-1);});ui_input_sync();}
  home_step(home,false);if(!boot_active())home_request_art(home);
  const int focused_tab=home.focus<0?home.header:-1;boot_tick();
  if(boot_active()){
   // Home's first full drawing is the slow one (every font size and card for
   // the first time: 133 ms on the console). It is made here, on the last frame
   // the mark still has the screen to itself, where a pause shows nothing; the
   // overlay then covers it. The handoff copies that still -- artwork arriving
   // meanwhile waits for the end -- and redraws just the header, where the
   // wordmark is unfolding.
   if(boot_still.empty()){draw_home(home,focused_tab);boot_still.assign(framebuffer(),framebuffer()+WIDTH*HEIGHT);}
   else if(!boot_covers_home()){memcpy(framebuffer(),boot_still.data(),boot_still.size()*4);draw_home_header(focused_tab);}
  }
  else{if(!boot_still.empty()){boot_still.clear();boot_still.shrink_to_fit();}draw_home(home,focused_tab);}
  boot_overlay();present();boot_ready();
  auto b=press();
  // During the handoff any press skips to the end rather than acting on Home.
  if(boot_active()){if(b)boot_skip();usleep(1000);continue;}if(b){char trace[128];snprintf(trace,sizeof trace,"Controls: events=0x%06x home_row=%d header=%d",b,home.focus,focused_tab);plog(trace);}
  const int rows=(int)home.rows.size();
  if(b&ORBIS_PAD_BUTTON_TOUCH_PAD){home.loaded=false;page_cache_clear();reload=true;ui_input_sync();continue;}
  if(b&(ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1)){pending_tab=(b&ORBIS_PAD_BUTTON_L1)?2:1;ui_input_sync();continue;}
  if(b&ORBIS_PAD_BUTTON_OPTIONS){pending_tab=2;ui_input_sync();continue;}
  if(b&ORBIS_PAD_BUTTON_SQUARE){stack.push_back({"",T("Favourites"),"",2,0,0});reload=true;ui_input_sync();continue;}
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
 auto& loc=stack.back();if(reload){if(!page_ready(loc.parent,loc.start,loc.query,loc.mode,loc.letter))notice(TF("Loading %s...",loc.title.c_str()));if(!fetch_page(loc.parent,loc.start,loc.query,loc.mode,page,loc.letter)){error_screen();page={};}
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
  if(loc.mode!=2||!loc.parent.empty()){stack.push_back({"",T("Favourites"),"",2,0,0});reload=true;}
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
 }boot_leave();livetv_stop();player_close();facts_stop();art_stop();update_shutdown();app_shutdown();http_end();platform_shutdown();platform_exit(0);}
