from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/ui_view.cpp';t=f.read_text().replace('value.pop_back();while(!value.empty()&&((unsigned char)value.back()&0xc0)==0x80)value.pop_back();','size_t end=value.size()-1;while(end&&((unsigned char)value[end]&0xc0)==0x80)--end;value.resize(end);').replace('for(auto name:tabs){bool active=', 'for(auto name:tabs){if(tab.empty())break;bool active=').replace('text(48,697,','text(48,686,');f.write_text(t,newline='\n')
f=p/'source/art.cpp';t=f.read_text().replace('auto& dst=framebuffer()[py*WIDTH+px];int alpha=p[3];','auto& dst=framebuffer()[py*WIDTH+px];int alpha=p[3];\n  if(alpha==255){dst=0xff000000|(p[0]<<16)|(p[1]<<8)|p[2];continue;}if(!alpha)continue;');f.write_text(t,newline='\n')
f=p/'source/main.cpp';t=f.read_text().replace('#include "art.h"','#include "art.h"\n#include "ui_view.h"')
a=t.index('static void header(');b=t.index('\nstatic void notice',a);t=t[:a]+'''static void header(const std::string& title){app_header(title,title=="Settings"?"Settings":"Home");}'''+t[b:]
a=t.index('header(item.base.name);',t.index('static void detail_screen'));b=t.index('present();auto b=press();',a)
t=t[:a]+'''auto tracks=sources.n_sources?&sources.source[source].tracks:nullptr;
 std::string audio=tracks&&tracks->n_audio?tracks->audio[audio_slot].label:"Server default";
 std::string subtitle=tracks&&sub_slot>=0?tracks->subs[sub_slot].label:"Off";
 draw_detail(item,sources.n_sources?sources.source[source].label:"Server default",audio,subtitle,selected);
 '''+t[b:]
a=t.index(' header(loc.title);');b=t.index('present();auto b=press();',a)
t=t[:a]+''' bool home=stack.size()==1&&loc.parent.empty()&&loc.mode==0&&loc.query.empty();
 int columns=library_columns(home),first=library_first(loc.selected,home);
 for(int k=first;k<std::min(first+columns,(int)page.items.size());++k)load_art(page.items[k].base.id);
 draw_library(page,loc.title,loc.start,loc.selected,home);
 '''+t[b:]
t=t.replace('loc.selected-2','loc.selected-columns').replace('loc.selected+2','loc.selected+columns')
t=t.replace('if(b&ORBIS_PAD_BUTTON_OPTIONS){settings_screen();reload=true;}', 'if(b&ORBIS_PAD_BUTTON_OPTIONS){settings_screen();reload=true;}\n if(b&ORBIS_PAD_BUTTON_TOUCH_PAD){stack.resize(1);reload=true;}')
f.write_text(t,newline='\n')
f=p/'source/login_ui.cpp';t=f.read_text().replace('#include "platform.h"','#include "platform.h"\n#include "ui_view.h"');a=t.index('static void login_header(');b=t.index('\nstatic void login_notice',a);t=t[:a]+'''static void login_header(const std::string& title){app_header(title,"");}'''+t[b:];f.write_text(t,newline='\n')
f=p/'source/platform_ps4.cpp';t=f.read_text().replace('#include "platform.h"','#include "platform.h"\n#include "screenshot.h"');t=t.replace('void present(){int rc=', 'void present(){static uint32_t previous_capture=0;uint32_t now=buttons();if((now&4)&&!(previous_capture&4))capture_screen(framebuffer());previous_capture=now;int rc=');f.write_text(t,newline='\n')
f=p/'tools/build.sh';t=f.read_text().replace('source/core.cpp source/keyboard_model.cpp','source/core.cpp source/ui_view.cpp source/screenshot.cpp source/keyboard_model.cpp');f.write_text(t,newline='\n')
f=p/'tools/fetch_console_logs.py';t=f.read_text().replace("listing('/data/JellyFin-PS4')",'''listing('/data/JellyFin-PS4-logs/screenshots')
try:
 f.cwd('/data/JellyFin-PS4-logs/screenshots')
 for filename in sorted(f.nlst())[-6:]:
  name=posixpath.basename(filename)
  if name.startswith('screen-') and name.endswith('.bmp'):
   with (out/name).open('wb') as dest:f.retrbinary('RETR '+name,dest.write)
   files.append(str((out/name).relative_to(p)))
except error_perm:pass
listing('/data/JellyFin-PS4')''');f.write_text(t,newline='\n')
