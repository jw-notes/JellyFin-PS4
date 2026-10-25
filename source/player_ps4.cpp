#include "app.h"
#include "platform.h"
#include "avplayer_abi.h"
#include "avplayer_module.h"
#include "avplayer_source.h"
#include "avplayer_memory.h"
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/AudioOut.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <algorithm>
#include <map>
#include <stdarg.h>
static int native_player_log(void*,const char* format,va_list args){
 char message[1024];int result=vsnprintf(message,sizeof message,format,args);std::string safe=message;
 // Native diagnostics may print a complete authenticated stream URL.
 for(const char* scheme:{"http://","https://"}){size_t at=0;while((at=safe.find(scheme,at))!=std::string::npos){auto end=safe.find_first_of(" \t\r\n\"'",at);safe.replace(at,end==std::string::npos?safe.size()-at:end-at,"[stream URL]");at+=12;}}
 if(g_token[0]){size_t at=0;while((at=safe.find(g_token,at))!=std::string::npos){safe.replace(at,strlen(g_token),"[token]");at+=7;}}
 for(char& c:safe)if((unsigned char)c<32)c=' ';
 plog(("AvPlayer native: "+safe).c_str());return result;
}
static void* handle;
static void*(*av_init)(AvInit*);
static int(*av_add)(void*,const char*);
static int(*av_stop)(void*);static int(*av_close)(void*);
static bool(*av_audio)(void*,AvFrame*);static bool(*av_video)(void*,AvFrameEx*);
static bool(*av_video_basic)(void*,AvFrame*);
static AvModuleExports media_module;
static std::string active_source_url;
static std::atomic<bool> audio_received{false};
static bool(*av_active)(void*);static uint64_t(*av_time)(void*);
static int(*av_pause)(void*);static int(*av_resume)(void*);static int(*av_jump)(void*,uint64_t);
static pthread_t audio_thread;static bool thread_created=false;static std::atomic<bool> stop_audio{false};static std::atomic<int> event{0};
static uint32_t* retained_frame;
static int memory_allocate(size_t bytes,size_t alignment,int type,int64_t* output){off_t offset=0;int rc=sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),bytes,alignment,type,&offset);*output=offset;return rc;}
static int memory_map(void** pointer,size_t bytes,int protection,int64_t offset,size_t alignment){return sceKernelMapDirectMemory(pointer,bytes,protection,0,offset,alignment);}
static int memory_unmap(void* pointer,size_t bytes){return sceKernelMunmap(pointer,bytes);}
static int memory_release(int64_t offset,size_t bytes){return sceKernelReleaseDirectMemory(offset,bytes);}
static AvMemoryPool decoder_memory({memory_allocate,memory_map,memory_unmap,memory_release,plog});
static void* alloc(void*,uint32_t alignment,uint32_t size){return decoder_memory.allocate(alignment,size,false);}
static void dealloc(void*,void* pointer){decoder_memory.release(pointer);}
static void* texture_alloc(void*,uint32_t alignment,uint32_t size){return decoder_memory.allocate(alignment,size,true);}
static void texture_free(void*,void* pointer){decoder_memory.release(pointer);}
static void on_event(void*,uint32_t e,int,void*){event=e;char b[64];snprintf(b,sizeof b,"AvPlayer event=0x%x",e);plog(b);}
static void* audio_worker(void*){
 int port=-1;uint32_t rate=0;uint16_t channels=0;int16_t pcm[1024*2];size_t used=0;
 while(!stop_audio){AvFrame f{};if(!av_audio(handle,&f)){usleep(2000);continue;}auto a=f.details.audio;audio_received=true;
 if(!f.data||(a.channels!=1&&a.channels!=2)||a.rate!=48000){plog("Unsupported decoder audio format (requires 48k S16 mono/stereo)");continue;}
 if(port<0||rate!=a.rate||channels!=a.channels){if(port>=0)sceAudioOutClose(port);channels=a.channels;rate=a.rate;port=sceAudioOutOpen(current_user(),ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,0,1024,rate,channels==2?1:0);used=0;if(port<0){plog("AudioOutOpen failed");break;}}
 size_t count=a.size/sizeof(int16_t),at=0;const int16_t* src=(int16_t*)f.data;
 while(at<count&&!stop_audio){size_t n=std::min(count-at,(size_t)1024*channels-used);memcpy(pcm+used,src+at,n*2);at+=n;used+=n;if(used==1024*channels){if(sceAudioOutOutput(port,pcm)<0)plog("AudioOutOutput failed");used=0;}}
 }
 if(port>=0){sceAudioOutOutput(port,nullptr);sceAudioOutClose(port);}return nullptr;
}
static int clip(int n){return std::max(0,std::min(255,n));}
static int load_public_avplayer(){return sceSysmoduleLoadModule(ORBIS_SYSMODULE_AV_PLAYER);}
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
 if(media_module.symbols[14]){auto set_log=reinterpret_cast<int(*)(int(*)(void*,const char*,va_list),void*)>(media_module.symbols[14]);player_result("native log callback",set_log(native_player_log,nullptr));}
 sceAudioOutInit();AvInit init{};init.memory={nullptr,alloc,dealloc,texture_alloc,texture_free};init.events={nullptr,on_event};init.debug=3;init.priority=0x2bc;init.buffers=3;init.autostart=true;init.language="eng";event=0;
 handle=av_init(&init);if(!handle){last_error="PS4 decoder initialization failed";plog(last_error.c_str());return false;}plog("AvPlayer: decoder initialized");
 retained_frame=(uint32_t*)calloc(WIDTH*HEIGHT,4);if(!retained_frame){player_close();last_error="Video frame allocation failed";return false;}
 active_source_url=url;audio_received=false;
 const AvSourceApi source_api={reinterpret_cast<int(*)(void*,AvPostInit*)>(media_module.symbols[13]),reinterpret_cast<int(*)(void*,uint32_t,AvSourceDetails*)>(media_module.symbols[12]),av_add,plog};
 if(!configure_avplayer_source(source_api,handle,active_source_url,http_context_id(),http_ssl_context_id(),last_error)){player_close();return false;}

 stop_audio=false;thread_created=pthread_create(&audio_thread,nullptr,audio_worker,nullptr)==0;if(!thread_created){last_error="Unable to start audio worker";player_close();return false;}return true;
}
static bool video_frame(AvFrameEx& frame){
 if(av_video)return av_video(handle,&frame);
 AvFrame basic{};if(!av_video_basic(handle,&basic))return false;
 frame.data=basic.data;frame.timestamp=basic.timestamp;frame.details.video.width=basic.details.video.width;frame.details.video.height=basic.details.video.height;frame.details.video.aspect=basic.details.video.aspect;frame.details.video.pitch=basic.details.video.width;frame.details.video.luma_depth=frame.details.video.chroma_depth=8;return true;
}
bool player_draw(){if(!handle)return false;AvFrameEx f{};bool got=video_frame(f);if(got&&f.data){auto v=f.details.video;int w=v.width,h=v.height,pitch=v.pitch?v.pitch:w;if(w>0&&h>0&&w<=4096&&h<=2160&&pitch>=w&&v.luma_depth==8&&v.chroma_depth==8){
 std::fill(retained_frame,retained_frame+WIDTH*HEIGHT,0xff000000);int visible_w=w-v.crop_left-v.crop_right,visible_h=h-v.crop_top-v.crop_bottom;
 if(visible_w>0&&visible_h>0){float aspect=v.aspect>0?v.aspect:(float)visible_w/visible_h;int dw=WIDTH,dh=(int)(dw/aspect);if(dh>HEIGHT){dh=HEIGHT;dw=(int)(dh*aspect);}int ox=(WIDTH-dw)/2,oy=(HEIGHT-dh)/2;auto yplane=(uint8_t*)f.data;auto uv=yplane+pitch*h;
 for(int y=0;y<dh;y++){int sy=v.crop_top+y*visible_h/dh;for(int x=0;x<dw;x++){int sx=v.crop_left+x*visible_w/dw;int yy=yplane[sy*pitch+sx];int u=uv[(sy/2)*pitch+(sx&~1)]-128,vv=uv[(sy/2)*pitch+(sx&~1)+1]-128;int c=std::max(0,yy-16);int r=clip((298*c+409*vv+128)>>8),g=clip((298*c-100*u-208*vv+128)>>8),b=clip((298*c+516*u+128)>>8);retained_frame[(oy+y)*WIDTH+ox+x]=0xff000000|(r<<16)|(g<<8)|b;}}}}
 else plog("Unsupported video frame layout");}
 if(retained_frame)memcpy(framebuffer(),retained_frame,WIDTH*HEIGHT*4);return got;
}
bool player_has_audio(){return audio_received;}
bool player_active(){return handle&&av_active(handle);}
void player_pause(bool pause){if(handle){if(pause)av_pause(handle);else av_resume(handle);}}
void player_seek(uint64_t ms){if(handle&&av_jump(handle,ms)<0)last_error="This stream cannot seek to that position";}
uint64_t player_time(){return handle?av_time(handle):0;}
void player_close(){stop_audio=true;if(thread_created){pthread_join(audio_thread,nullptr);thread_created=false;}if(handle){av_stop(handle);av_close(handle);handle=nullptr;}free(retained_frame);retained_frame=nullptr;active_source_url.clear();}

std::string player_error(){return {};}
