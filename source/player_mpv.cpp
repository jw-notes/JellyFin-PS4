// Uses the PS4 MPV/FFmpeg build from dragonflylee/switchfin's ps4 branch.
// The application UI and Jellyfin session/negotiation code remain shared.
#include "app.h"
#include "platform.h"
#include <mpv/client.h>
#include <mpv/render.h>
#include <string.h>
#include <stdlib.h>
static mpv_handle* player=nullptr;
static mpv_render_context* renderer=nullptr;
static uint32_t* retained=nullptr;
static bool started=false,ended=false,got_audio=false,got_video=false;
static double position=0;
static std::string failure;
static std::string safe_log(std::string line){
 for(const char* scheme:{"http://","https://"}){size_t at=0;while((at=line.find(scheme,at))!=std::string::npos){auto end=line.find_first_of(" \t\r\n\"'",at);line.replace(at,end==std::string::npos?line.size()-at:end-at,"[stream URL]");at+=12;}}
 if(g_token[0]){size_t at=0;while((at=line.find(g_token,at))!=std::string::npos){line.replace(at,strlen(g_token),"[token]");at+=7;}}
 for(char& c:line)if((unsigned char)c<32)c=' ';
 return line;
}
static bool option(const char* key,const char* value){int rc=mpv_set_option_string(player,key,value);if(rc<0)plog((std::string("MPV: option ")+key+" failed: "+mpv_error_string(rc)).c_str());return rc>=0;}
static void events(){
 if(!player)return;
 for(;;){auto e=mpv_wait_event(player,0);if(e->event_id==MPV_EVENT_NONE)break;
  if(e->event_id==MPV_EVENT_COMMAND_REPLY&&e->error<0){failure=std::string("Playback command failed: ")+mpv_error_string(e->error);ended=true;}
  else if(e->event_id==MPV_EVENT_FILE_LOADED){started=true;plog("MPV: stream loaded");}
  else if(e->event_id==MPV_EVENT_END_FILE){auto end=(mpv_event_end_file*)e->data;ended=true;if(end&&end->reason==MPV_END_FILE_REASON_ERROR){failure=std::string("Playback failed: ")+mpv_error_string(end->error);plog(failure.c_str());}}
  else if(e->event_id==MPV_EVENT_LOG_MESSAGE){auto log=(mpv_event_log_message*)e->data;plog((std::string("MPV: ")+safe_log(log->text?log->text:"")).c_str());}
  else if(e->event_id==MPV_EVENT_PROPERTY_CHANGE){auto prop=(mpv_event_property*)e->data;if(!prop->data)continue;
   if(!strcmp(prop->name,"time-pos")&&prop->format==MPV_FORMAT_DOUBLE)position=*(double*)prop->data;
   else if(!strcmp(prop->name,"audio-params")&&prop->format==MPV_FORMAT_NODE)got_audio=true;
  }
 }
}
bool player_open(const std::string& url){
 player_close();failure.clear();position=0;started=ended=got_audio=got_video=false;
 plog("MPV: creating player");player=mpv_create();if(!player){last_error="Could not create the MPV player.";return false;}
 option("referrer",g_server);option("user-agent","JellyfinPS4/0.8");option("config","no");option("terminal","no");option("idle","yes");option("vo","libmpv");
 mpv_request_log_messages(player,"info");
#ifdef HOST_TEST
 option("ao","null");option("hwdec","no");option("ytdl","no");
#else
 option("ao","sdl");option("audio-channels","stereo");option("audio-samplerate","48000");
 // Isolate the experimental Videodec2 path after a process crash on hardware.
 // Use the software path until that decoder is proven on the target firmware.
 option("hwdec","no");option("vd","h264,");
#endif
 option("vd-lavc-threads","6");option("demuxer-max-bytes","64MiB");option("network-timeout","15");option("osd-level","0");
 plog("MPV: initializing software decoder/audio backend");int rc=mpv_initialize(player);if(rc<0){last_error=std::string("Could not initialize MPV: ")+mpv_error_string(rc);player_close();return false;}
 mpv_observe_property(player,0,"time-pos",MPV_FORMAT_DOUBLE);mpv_observe_property(player,0,"audio-params",MPV_FORMAT_NODE);
 mpv_render_param params[]={{MPV_RENDER_PARAM_API_TYPE,(void*)MPV_RENDER_API_TYPE_SW},{MPV_RENDER_PARAM_INVALID,nullptr}};
 plog("MPV: creating software video renderer");rc=mpv_render_context_create(&renderer,player,params);if(rc<0){last_error=std::string("Could not initialize video rendering: ")+mpv_error_string(rc);player_close();return false;}
 void* memory=nullptr;if(posix_memalign(&memory,64,WIDTH*HEIGHT*4)){last_error="Video frame allocation failed.";player_close();return false;}retained=(uint32_t*)memory;for(int i=0;i<WIDTH*HEIGHT;++i)retained[i]=0xff000000;
 const char* command[]={"loadfile",url.c_str(),"replace",nullptr};rc=mpv_command_async(player,0,command);
 if(rc<0){last_error=std::string("Could not open stream: ")+mpv_error_string(rc);player_close();return false;}
 plog("MPV: opening stream with FFmpeg software decoding; hardware decoding disabled");return true;
}
bool player_draw(){
 events();if(!renderer||!retained)return false;
 bool fresh=(mpv_render_context_update(renderer)&MPV_RENDER_UPDATE_FRAME)!=0;
 if(fresh){int size[]={WIDTH,HEIGHT};size_t stride=WIDTH*4;
  mpv_render_param params[]={{MPV_RENDER_PARAM_SW_SIZE,size},{MPV_RENDER_PARAM_SW_FORMAT,(void*)"bgr0"},{MPV_RENDER_PARAM_SW_STRIDE,&stride},{MPV_RENDER_PARAM_SW_POINTER,retained},{MPV_RENDER_PARAM_INVALID,nullptr}};
  int rc=mpv_render_context_render(renderer,params);if(rc<0){failure=std::string("Video rendering failed: ")+mpv_error_string(rc);return false;}
  for(int i=0;i<WIDTH*HEIGHT;++i)retained[i]|=0xff000000;
  mpv_render_context_report_swap(renderer);if(started)got_video=true;
 }
 memcpy(framebuffer(),retained,WIDTH*HEIGHT*4);return got_video;
}
bool player_has_audio(){events();return started&&got_audio;}
bool player_active(){events();return player&&!ended;}
std::string player_error(){events();return failure;}
void player_pause(bool paused){if(player){int flag=paused;mpv_set_property(player,"pause",MPV_FORMAT_FLAG,&flag);}}
void player_seek(uint64_t ms){if(player){std::string time=std::to_string(ms/1000.0);const char* command[]={"seek",time.c_str(),"absolute+exact",nullptr};mpv_command_async(player,0,command);}}
uint64_t player_time(){events();return position>0?(uint64_t)(position*1000):0;}
void player_close(){
 if(renderer){mpv_render_context_free(renderer);renderer=nullptr;}
 if(player){mpv_terminate_destroy(player);player=nullptr;}
 free(retained);retained=nullptr;
}
