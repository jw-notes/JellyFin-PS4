from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/player_ps4.cpp';t=f.read_text().replace('#include "avplayer_module.h"','#include "avplayer_module.h"\n#include "avplayer_source.h"')
t=t.replace('static AvModuleExports media_module;', 'static AvModuleExports media_module;\nstatic std::string active_source_url;\nstatic std::atomic<bool> audio_received{false};')
t=t.replace('auto a=f.details.audio;', 'auto a=f.details.audio;audio_received=true;')
a=t.index(' int source_result=av_add(');b=t.index('\n stop_audio=false;',a)
t=t[:a]+''' active_source_url=url;audio_received=false;
 const AvSourceApi source_api={reinterpret_cast<int(*)(void*,AvPostInit*)>(media_module.symbols[13]),reinterpret_cast<int(*)(void*,uint32_t,AvSourceDetails*)>(media_module.symbols[12]),av_add,plog};
 if(!configure_avplayer_source(source_api,handle,active_source_url,http_context_id(),http_ssl_context_id(),last_error)){player_close();return false;}
'''+t[b:]
t=t.replace('bool player_active(){','bool player_has_audio(){return audio_received;}\nbool player_active(){').replace('retained_frame=nullptr;}','retained_frame=nullptr;active_source_url.clear();}')
f.write_text(t,newline='\n')
f=p/'source/platform.h';t=f.read_text().replace('bool player_active();','bool player_active();\nbool player_has_audio();');f.write_text(t,newline='\n')
f=p/'tools/build.sh';t=f.read_text().replace('source/avplayer_module.cpp','source/avplayer_module.cpp source/avplayer_source.cpp');f.write_text(t,newline='\n')
