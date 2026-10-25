from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/player_ps4.cpp';t=f.read_text().replace('#include "avplayer_abi.h"','#include "avplayer_abi.h"\n#include "avplayer_module.h"')
t=t.replace('static bool(*av_active)', 'static bool(*av_video_basic)(void*,AvFrame*);\nstatic AvModuleExports media_module;\nstatic bool(*av_active)')
a=t.index('bool player_open(');b=t.index(' sceAudioOutInit();AvInit',a)
t=t[:a]+'''static int load_public_avplayer(){return sceSysmoduleLoadModule(ORBIS_SYSMODULE_AV_PLAYER);}
static std::vector<int> loaded_modules(){
 std::vector<OrbisKernelModule> list(256);size_t count=0;
 int rc=sceKernelGetModuleList(list.data(),list.size(),&count);
 if(rc<0){list.resize(1024);rc=sceKernelGetModuleList(list.data(),list.size(),&count);}
 if(rc<0){char message[120];snprintf(message,sizeof message,"AvPlayer: module enumeration failed 0x%08x",(unsigned)rc);plog(message);return {};}
 std::vector<int> result;for(size_t i=0;i<std::min(count,list.size());++i)result.push_back((int)list[i]);return result;
}
static int load_avplayer_file(const char* path,int* started){return (int32_t)sceKernelLoadStartModule(path,0,nullptr,0,nullptr,started);}
static void player_result(const char* stage,int rc){char message[128];snprintf(message,sizeof message,"AvPlayer: %s result=0x%08x",stage,(unsigned)rc);plog(message);}
bool player_open(const std::string& url){
 player_close();
 const AvModuleOps ops={load_public_avplayer,loaded_modules,load_avplayer_file,sceKernelDlsym,plog};
 std::string module_error;
 if(!acquire_avplayer_module(media_module,ops,module_error)){last_error=module_error;return false;}
 av_init=reinterpret_cast<decltype(av_init)>(media_module.symbols[0]);
 av_add=reinterpret_cast<decltype(av_add)>(media_module.symbols[1]);
 av_stop=reinterpret_cast<decltype(av_stop)>(media_module.symbols[2]);
 av_close=reinterpret_cast<decltype(av_close)>(media_module.symbols[3]);
 av_audio=reinterpret_cast<decltype(av_audio)>(media_module.symbols[4]);
 av_video_basic=reinterpret_cast<decltype(av_video_basic)>(media_module.symbols[5]);
 av_active=reinterpret_cast<decltype(av_active)>(media_module.symbols[6]);
 av_time=reinterpret_cast<decltype(av_time)>(media_module.symbols[7]);
 av_pause=reinterpret_cast<decltype(av_pause)>(media_module.symbols[8]);
 av_resume=reinterpret_cast<decltype(av_resume)>(media_module.symbols[9]);
 av_jump=reinterpret_cast<decltype(av_jump)>(media_module.symbols[10]);
 av_video=reinterpret_cast<decltype(av_video)>(media_module.symbols[11]);
 plog("AvPlayer: initializing decoder");
'''+t[b:]
t=t.replace('handle=av_init(&init);if(!handle){last_error="PS4 decoder initialization failed";return false;}', 'handle=av_init(&init);if(!handle){last_error="PS4 decoder initialization failed";plog(last_error.c_str());return false;}plog("AvPlayer: decoder initialized");')
t=t.replace('if(av_add(handle,url.c_str())<0){player_close();last_error="PS4 decoder rejected the stream URL";return false;}', 'int source_result=av_add(handle,url.c_str());player_result("add stream source",source_result);if(source_result<0){char message[160];snprintf(message,sizeof message,"PS4 decoder rejected the stream (0x%08x).",(unsigned)source_result);last_error=message;player_close();return false;}')
t=t.replace('bool player_draw(){if(!handle)return false;AvFrameEx f{};bool got=av_video(handle,&f);', '''static bool video_frame(AvFrameEx& frame){
 if(av_video)return av_video(handle,&frame);
 AvFrame basic{};if(!av_video_basic(handle,&basic))return false;
 frame.data=basic.data;frame.timestamp=basic.timestamp;frame.details.video.width=basic.details.video.width;frame.details.video.height=basic.details.video.height;frame.details.video.aspect=basic.details.video.aspect;frame.details.video.pitch=basic.details.video.width;frame.details.video.luma_depth=frame.details.video.chroma_depth=8;return true;
}
bool player_draw(){if(!handle)return false;AvFrameEx f{};bool got=video_frame(f);''')
f.write_text(t,newline='\n')
f=p/'tools/build.sh';t=f.read_text().replace('source/player_ps4.cpp','source/player_ps4.cpp source/avplayer_module.cpp');f.write_text(t,newline='\n')
f=p/'version.json';f.write_text('{"app_version":"01.03","release_directory":"v1.03","client_version":"0.4.0"}\n',newline='\n')
