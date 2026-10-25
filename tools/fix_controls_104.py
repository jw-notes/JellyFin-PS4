from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/main.cpp';t=f.read_text().replace('#include "ui_view.h"','#include "ui_view.h"\n#include "input_model.h"')
a=t.index('static uint32_t press()');b=t.index('\nstatic void error_screen',a);t=t[:a]+'''static uint32_t press(){return ui_input();}
static void wait_release(){ui_input_sync();}'''+t[b:]
t=t.replace('static void error_screen(){','static void error_screen(){UiInputScope input_scope;').replace('static void settings_screen(){','static void settings_screen(){UiInputScope input_scope;').replace('static void detail_screen(const Item& initial){','static void detail_screen(const Item& initial){UiInputScope input_scope;').replace('static void play(Item item,int audio,int subtitle,const std::string& source){','static void play(Item item,int audio,int subtitle,const std::string& source){UiInputScope input_scope;')
t=t.replace('if(!playback_url(item,audio,subtitle,source,url,session,source_id)){error_screen();return;}', 'if(!playback_url(item,audio,subtitle,source,url,session,source_id)){end_playback(session);error_screen();return;}')
t=t.replace('if(got||player_active())started=true;', 'if(got||player_has_audio())started=true;')
t=t.replace('const bool', 'const bool')
t=t.replace('Page page;bool reload=true;', 'Page page;bool reload=true;int header_focus=-1;')
a=t.index(' present();auto b=press();int count=',t.index('int main()'));b=t.index('\n usleep(16000);',a)
t=t[:a]+''' present();auto b=press();int count=(int)page.items.size();
 // Route one action per input sample. Never keep a reference into stack after
 // pushing/popping a screen, and never stack the current shortcut view again.
 if(b&ORBIS_PAD_BUTTON_TOUCH_PAD){stack.resize(1);header_focus=-1;reload=true;ui_input_sync();continue;}
 if((b&ORBIS_PAD_BUTTON_OPTIONS)||(header_focus==2&&(b&ORBIS_PAD_BUTTON_CROSS))){settings_screen();header_focus=-1;reload=true;continue;}
 if((b&ORBIS_PAD_BUTTON_TRIANGLE)||(header_focus==1&&(b&ORBIS_PAD_BUTTON_CROSS))){std::string query;if(keyboard("Search Jellyfin",query)&&!query.empty()){stack.push_back({"","Search: "+query,query,0,0,0});reload=true;}header_focus=-1;ui_input_sync();continue;}
 if(header_focus==0&&(b&ORBIS_PAD_BUTTON_CROSS)){stack.resize(1);header_focus=-1;reload=true;ui_input_sync();continue;}
 if(b&ORBIS_PAD_BUTTON_CIRCLE){if(header_focus>=0){header_focus=-1;}else if(stack.size()>1){stack.pop_back();reload=true;}ui_input_sync();continue;}
 if(b&(ORBIS_PAD_BUTTON_SQUARE|ORBIS_PAD_BUTTON_L2|ORBIS_PAD_BUTTON_R2)){
  int mode=(b&ORBIS_PAD_BUTTON_R2)?3:(b&ORBIS_PAD_BUTTON_L2)?1:2;
  if(loc.mode!=mode||!loc.parent.empty()){stack.push_back({"",mode==3?"Live TV":mode==1?"Continue watching":"Favourites","",mode,0,0});reload=true;}
  header_focus=-1;ui_input_sync();continue;
 }
 if(header_focus>=0){
  if(b&ORBIS_PAD_BUTTON_LEFT)header_focus=(header_focus+2)%3;
  else if(b&ORBIS_PAD_BUTTON_RIGHT)header_focus=(header_focus+1)%3;
  else if(b&ORBIS_PAD_BUTTON_DOWN)header_focus=-1;
  continue;
 }
 if(b&ORBIS_PAD_BUTTON_UP){if(loc.selected<columns)header_focus=0;else loc.selected-=columns;}
 else if(b&ORBIS_PAD_BUTTON_DOWN)loc.selected=std::min(std::max(0,count-1),loc.selected+columns);
 else if(b&ORBIS_PAD_BUTTON_LEFT)loc.selected=std::max(0,loc.selected-1);
 else if(b&ORBIS_PAD_BUTTON_RIGHT)loc.selected=std::min(std::max(0,count-1),loc.selected+1);
 else if((b&ORBIS_PAD_BUTTON_L1)&&loc.start>=60){loc.start-=60;loc.selected=0;reload=true;}
 else if((b&ORBIS_PAD_BUTTON_R1)&&loc.start+60<page.total){loc.start+=60;loc.selected=0;reload=true;}
 else if((b&ORBIS_PAD_BUTTON_CROSS)&&count){
  auto item=page.items[loc.selected];
  if(is_container(item.base.type)){stack.push_back({item.base.id,item.base.name,"",0,0,0});reload=true;ui_input_sync();}
  else{detail_screen(item);reload=true;}
  continue;
 }
'''+t[b:]
t=t.replace('draw_library(page,loc.title,loc.start,loc.selected,home);','draw_library(page,loc.title,loc.start,loc.selected,home,header_focus);')
f.write_text(t,newline='\n')
f=p/'source/login_ui.cpp';t=f.read_text().replace('#include "ui_view.h"','#include "ui_view.h"\n#include "input_model.h"');a=t.index('static uint32_t previous_buttons');b=t.index('\nstatic uint64_t millis()',a);t=t[:a]+'''static uint32_t pressed(){return ui_input();}
static void release_buttons(){ui_input_sync();}'''+t[b:]
for signature in ['static void login_error() {','static bool password_screen() {','static bool quick_connect_screen() {','bool sign_in() {']:
 t=t.replace(signature,signature+'\n    UiInputScope input_scope;')
f.write_text(t,newline='\n')
f=p/'source/app.cpp';t=f.read_text().replace('void end_playback(const std::string& session){flush_reports();char out[512];http_request(', 'void end_playback(const std::string& session){flush_reports();char out[512];if(!session.empty())http_request(').replace('(settings.direct?"DirectPlay":"Transcode")','(settings.direct&&!live?"DirectPlay":"Transcode")');f.write_text(t,newline='\n')
