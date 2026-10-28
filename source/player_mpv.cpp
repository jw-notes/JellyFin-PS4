// Uses the PS4 MPV/FFmpeg build from dragonflylee/switchfin's ps4 branch.
// The application UI and Jellyfin session/negotiation code remain shared.
#include "app.h"
#include "platform.h"
#include "mono_clock.h"
#include <mpv/client.h>
#include <mpv/render.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <unistd.h>
static mpv_handle* player=nullptr;
static mpv_render_context* renderer=nullptr;
static bool started=false,ended=false,got_audio=false;
static std::atomic<bool> got_video{false};
static double position=0,cached=0;
// When the stream was opened, for the start-up timings in the log.
static uint64_t opened_ms=0;
static std::string failure;

// ---- the video frames --------------------------------------------------------
// MPV's frames are rendered on a thread of their own the moment MPV says one
// is due (its update callback), not from the UI loop: there they waited for
// the UI's own present and its vsync, and frames that missed their time were
// dropped (29% of a 720p film in the 1.10 test). Three buffers: the newest
// finished one, the one the UI is copying, and the one being rendered.
// Each frame is rendered at the video's own size fitted to the screen, so a
// 1280x692 picture is a colour conversion with no scaling at all.
struct VideoFrame{uint32_t* px=nullptr;int w=0,h=0;};
static VideoFrame frames[3];
static int newest=-1,reading=-1;
static pthread_t render_thread;
static bool render_running=false,render_stop=false,render_due=false;
static pthread_mutex_t frame_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t frame_wake=PTHREAD_COND_INITIALIZER;
static uint64_t render_count=0,render_total_us=0,render_max_us=0;
// A picture other than the plain fit (L1 mode) is shaped by MPV on the full screen.
static std::atomic<bool> picture_default{true};
// When the newest frame was rendered: Live TV's watchdog notices a picture
// that has stopped while the sound goes on.
static std::atomic<uint64_t> last_frame_ms{0};
// player_reload() asked MPV to replace the stream: its old file's end is expected.
static std::atomic<bool> replacing{false};
// The video thread is inside MPV (updating or rendering a frame).
static std::atomic<bool> render_busy{false};
static void on_update(void*){pthread_mutex_lock(&frame_lock);render_due=true;pthread_cond_signal(&frame_wake);pthread_mutex_unlock(&frame_lock);}
// The picture's size on the 1280x720 screen, aspect kept.
static void fitted_size(int& w,int& h){
 int64_t dw=0,dh=0;mpv_get_property(player,"video-params/dw",MPV_FORMAT_INT64,&dw);mpv_get_property(player,"video-params/dh",MPV_FORMAT_INT64,&dh);
 if(dw<=0||dh<=0||!picture_default){w=WIDTH;h=HEIGHT;return;}
 if(dw*HEIGHT>dh*WIDTH){w=WIDTH;h=(int)(dh*WIDTH/dw);}else{h=HEIGHT;w=(int)(dw*HEIGHT/dh);}
 w=std::max(2,std::min(WIDTH,w&~1));h=std::max(2,std::min(HEIGHT,h&~1));
}
static void* render_loop(void*){
 for(;;){
  pthread_mutex_lock(&frame_lock);while(!render_due&&!render_stop)pthread_cond_wait(&frame_wake,&frame_lock);
  if(render_stop){pthread_mutex_unlock(&frame_lock);return nullptr;}
  render_due=false;int slot=0;while(slot==newest||slot==reading)++slot;pthread_mutex_unlock(&frame_lock);
  render_busy=true;
  if(!(mpv_render_context_update(renderer)&MPV_RENDER_UPDATE_FRAME)){render_busy=false;continue;}
  const uint64_t t0=mono_us();VideoFrame& f=frames[slot];fitted_size(f.w,f.h);
  int size[]={f.w,f.h};size_t stride=(size_t)f.w*4;
  // Never sleep until the frame's display time: after a stream's timestamps
  // jump back (a Pluto ad break) that time lay far ahead, the thread slept
  // there, the picture stopped, and closing the player waited on it forever.
  int no_block=0;
  mpv_render_param params[]={{MPV_RENDER_PARAM_SW_SIZE,size},{MPV_RENDER_PARAM_SW_FORMAT,(void*)"bgr0"},{MPV_RENDER_PARAM_SW_STRIDE,&stride},{MPV_RENDER_PARAM_SW_POINTER,f.px},{MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME,&no_block},{MPV_RENDER_PARAM_INVALID,nullptr}};
  const int rc=mpv_render_context_render(renderer,params);mpv_render_context_report_swap(renderer);render_busy=false;
  if(rc<0)continue;
  const uint64_t took=mono_us()-t0;++render_count;render_total_us+=took;render_max_us=std::max(render_max_us,took);
  if(!started)continue;
  pthread_mutex_lock(&frame_lock);newest=slot;pthread_mutex_unlock(&frame_lock);last_frame_ms=mono_ms();
  if(started&&!got_video){got_video=true;plog(("MPV: first frame after "+std::to_string(mono_ms()-opened_ms)+" ms").c_str());}
 }
}
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
  else if(e->event_id==MPV_EVENT_FILE_LOADED){started=true;mpv_request_log_messages(player,settings.debug_log?"info":"error");plog(("MPV: stream loaded after "+std::to_string(mono_ms()-opened_ms)+" ms").c_str());}
  else if(e->event_id==MPV_EVENT_END_FILE&&replacing&&e->data&&((mpv_event_end_file*)e->data)->reason==MPV_END_FILE_REASON_STOP){replacing=false;}
  else if(e->event_id==MPV_EVENT_END_FILE){auto end=(mpv_event_end_file*)e->data;ended=true;replacing=false;if(end&&end->reason==MPV_END_FILE_REASON_ERROR){failure=std::string("Playback failed: ")+mpv_error_string(end->error);plog(failure.c_str());}}
  else if(e->event_id==MPV_EVENT_LOG_MESSAGE){auto log=(mpv_event_log_message*)e->data;
   // While opening only the parts that say why a stream failed: the network, FFmpeg and the demuxers.
   if(log->log_level>MPV_LOG_LEVEL_INFO&&log->prefix&&strcmp(log->prefix,"ffmpeg")&&strncmp(log->prefix,"stream",6)&&strncmp(log->prefix,"demux",5)&&strcmp(log->prefix,"lavf")&&strcmp(log->prefix,"cplayer"))continue;
   plog((std::string("MPV: ")+(log->prefix?std::string(log->prefix)+": ":"")+safe_log(log->text?log->text:"")).c_str());}
  else if(e->event_id==MPV_EVENT_PROPERTY_CHANGE){auto prop=(mpv_event_property*)e->data;if(!prop->data)continue;
   if(!strcmp(prop->name,"time-pos")&&prop->format==MPV_FORMAT_DOUBLE)position=*(double*)prop->data;
   else if(!strcmp(prop->name,"audio-params")&&prop->format==MPV_FORMAT_NODE)got_audio=true;
   else if(!strcmp(prop->name,"demuxer-cache-duration")&&prop->format==MPV_FORMAT_DOUBLE)cached=*(double*)prop->data;
  }
 }
}
// Live TV's reconnect: the stream loaded again in the same player. Tearing
// MPV down and creating a new one there crashed the app (twice, both right
// after a Pluto ad break had broken the stream's timestamps); loadfile's
// replace is MPV's own well-trodden path. The old file's end (reason STOP)
// is not the end of playback.
bool player_reload(const std::string& url){
 if(!player)return player_open(url);
 // started: off until the new stream has loaded, so a last frame of the old
 // one does not count as the new picture (it hid "Reconnecting" at once).
 replacing=true;failure.clear();ended=false;started=false;got_video=false;last_frame_ms=0;opened_ms=mono_ms();
 pthread_mutex_lock(&frame_lock);newest=-1;pthread_mutex_unlock(&frame_lock);
 const char* command[]={"loadfile",url.c_str(),"replace",nullptr};const int rc=mpv_command_async(player,0,command);
 if(rc<0){replacing=false;last_error=std::string("Could not reopen stream: ")+mpv_error_string(rc);return false;}
 plog("MPV: reloading the stream in the same player");return true;
}
bool player_open(const std::string& url){
 player_close();failure.clear();position=cached=0;started=ended=got_audio=false;got_video=false;newest=reading=-1;render_count=render_total_us=render_max_us=0;last_frame_ms=0;
 plog("MPV: creating player");player=mpv_create();if(!player){last_error="Could not create the MPV player.";return false;}
 // A Live TV channel played straight from its provider: no server referrer,
 // a browser's user agent (some providers turn others away), and its best
 // variant within about 4.5 Mbps (a 720p one): the screen is 720p, and a
 // 1080p channel cost 25 ms a frame to shrink, half its frames dropped.
 const bool remote=url.rfind(std::string(g_server)+"/",0)!=0;
 // Some channels have one heavy variant only (1080p, 50-60 fps): the decoder
 // may drop frames when it falls behind rather than the picture stalling, and
 // skips deblocking on frames nothing refers to. A channel at 23.237.104.106
 // lost half its frames without this.
 if(remote){option("user-agent","Mozilla/5.0 (PlayStation 4) AppleWebKit/605.1.15");option("hls-bitrate",std::to_string(std::min(settings.bitrate,4500000)).c_str());
  option("framedrop","decoder+vo");option("vd-lavc-skiploopfilter","nonref");
  // No kept-alive HTTPS connections between HLS segments: after a Pluto ad
  // break ("keepalive request failed", "mbedtls_ssl_read returned -0x0")
  // the app died as the stream was reloaded, tearing those down.
  option("demuxer-lavf-o","http_persistent=0");}
 else{option("referrer",g_server);option("user-agent","JellyfinPS4/0.8");}option("config","no");option("terminal","no");option("idle","yes");option("vo","libmpv");
 // Verbose until the stream has loaded: when one will not open, the reason is
 // in FFmpeg's detail, not in the one-line "loading failed".
 mpv_request_log_messages(player,settings.debug_log?"v":"error");
#ifdef HOST_TEST
 option("ao","null");option("hwdec","no");option("ytdl","no");
#else
 option("ao","sdl");option("audio-channels","stereo");option("audio-samplerate","48000");
 // Isolate the experimental Videodec2 path after a process crash on hardware.
 // Use the software path until that decoder is proven on the target firmware.
 option("hwdec","no");option("vd","h264,");
#endif
 option("vd-lavc-threads","6");option("network-timeout","15");option("osd-level","0");
 // Memory: the read-ahead is capped, and the back buffer (kept for seeking
 // backwards, 50 MiB by default) mostly given up -- a 1080p stream ran out
 // of memory with both at their defaults.
 option("demuxer-max-bytes","32MiB");option("demuxer-max-back-bytes","4MiB");
 // Scaling, when the picture is not already the screen's size: fast-bilinear
 // (a 240p channel upscaled with bicubic took 34 ms a frame). With 1080p (software decoding) the cheapest settings: skip
 // deblocking on frames nothing refers to, and fast-bilinear downscaling.
 option("sws-scaler","fast-bilinear");
 if(settings.hd1080){option("vd-lavc-skiploopfilter","nonref");option("vd-lavc-fast","yes");option("sws-scaler","fast-bilinear");}
 // Music shows its own Now Playing screen: never an embedded cover as video.
 option("audio-display","no");
 option("volume",std::to_string(std::max(0,std::min(100,settings.volume))).c_str());
 plog("MPV: initializing software decoder/audio backend");int rc=mpv_initialize(player);if(rc<0){last_error=std::string("Could not initialize MPV: ")+mpv_error_string(rc);player_close();return false;}
 mpv_observe_property(player,0,"time-pos",MPV_FORMAT_DOUBLE);mpv_observe_property(player,0,"audio-params",MPV_FORMAT_NODE);mpv_observe_property(player,0,"demuxer-cache-duration",MPV_FORMAT_DOUBLE);
 mpv_render_param params[]={{MPV_RENDER_PARAM_API_TYPE,(void*)MPV_RENDER_API_TYPE_SW},{MPV_RENDER_PARAM_INVALID,nullptr}};
 plog("MPV: creating software video renderer");rc=mpv_render_context_create(&renderer,player,params);if(rc<0){last_error=std::string("Could not initialize video rendering: ")+mpv_error_string(rc);player_close();return false;}
 for(auto& f:frames){void* memory=nullptr;if(posix_memalign(&memory,64,WIDTH*HEIGHT*4)){last_error="Video frame allocation failed.";player_close();return false;}f.px=(uint32_t*)memory;f.w=f.h=0;}
 render_stop=render_due=false;render_running=pthread_create(&render_thread,nullptr,render_loop,nullptr)==0;
 if(!render_running){last_error="Could not start the video thread.";player_close();return false;}
 mpv_render_context_set_update_callback(renderer,on_update,nullptr);
 player_picture(settings.pic_aspect,settings.pic_w,settings.pic_h,settings.pic_x,settings.pic_y);
 opened_ms=mono_ms();const char* command[]={"loadfile",url.c_str(),"replace",nullptr};rc=mpv_command_async(player,0,command);
 if(rc<0){last_error=std::string("Could not open stream: ")+mpv_error_string(rc);player_close();return false;}
 plog("MPV: opening stream with FFmpeg software decoding; hardware decoding disabled");return true;
}
// The newest frame, centred with black bars, under whatever the UI draws next.
bool player_draw(){
 events();
 pthread_mutex_lock(&frame_lock);reading=newest;pthread_mutex_unlock(&frame_lock);
 auto fb=framebuffer();
 if(reading<0){std::fill(fb,fb+WIDTH*HEIGHT,0xff000000);return got_video;}
 const VideoFrame& f=frames[reading];const int x0=(WIDTH-f.w)/2,y0=(HEIGHT-f.h)/2;
 if(y0>0){std::fill(fb,fb+y0*WIDTH,0xff000000);std::fill(fb+(y0+f.h)*WIDTH,fb+WIDTH*HEIGHT,0xff000000);}
 for(int y=0;y<f.h;++y){uint32_t* dst=fb+(size_t)(y0+y)*WIDTH;const uint32_t* src=f.px+(size_t)y*f.w;
  if(x0>0){std::fill(dst,dst+x0,0xff000000);std::fill(dst+x0+f.w,dst+WIDTH,0xff000000);}
  for(int x=0;x<f.w;++x)dst[x0+x]=src[x]|0xff000000;}
 pthread_mutex_lock(&frame_lock);reading=-1;pthread_mutex_unlock(&frame_lock);
 return got_video;
}
void player_picture(int aspect,int w,int h,int x,int y){
 if(!player)return;picture_default=aspect==0&&w==100&&h==100&&x==0&&y==0;
 char v[32];
 mpv_set_property_string(player,"panscan",aspect==1?"1.0":"0.0");mpv_set_property_string(player,"keepaspect",aspect==2?"no":"yes");
 snprintf(v,sizeof v,"%.3f",w/100.0);mpv_set_property_string(player,"video-scale-x",v);
 snprintf(v,sizeof v,"%.3f",h/100.0);mpv_set_property_string(player,"video-scale-y",v);
 // MPV pans in fractions of the picture, down positive; ours is half-percents of the screen, up positive.
 snprintf(v,sizeof v,"%.4f",x/200.0);mpv_set_property_string(player,"video-pan-x",v);
 snprintf(v,sizeof v,"%.4f",-y/200.0);mpv_set_property_string(player,"video-pan-y",v);
}
void player_volume(int volume){if(player){double v=std::max(0,std::min(100,volume));mpv_set_property(player,"volume",MPV_FORMAT_DOUBLE,&v);}}
bool player_loaded(){events();return started;}
uint64_t player_last_frame_ms(){return last_frame_ms;}
double player_cache_seconds(){events();return cached>0?cached:0;}
bool player_has_audio(){events();return started&&got_audio;}
bool player_active(){events();return player&&!ended;}
std::string player_error(){events();return failure;}
void player_pause(bool paused){if(player){int flag=paused;mpv_set_property(player,"pause",MPV_FORMAT_FLAG,&flag);}}
void player_seek(uint64_t ms){if(player){std::string time=std::to_string(ms/1000.0);const char* command[]={"seek",time.c_str(),"absolute+exact",nullptr};mpv_command_async(player,0,command);}}
uint64_t player_time(){events();return position>0?(uint64_t)(position*1000):0;}
void player_close(){
 // How well the decoder kept up: frames it dropped, and frames shown late.
 if(player&&got_video){int64_t decoder=0,output=0;mpv_get_property(player,"decoder-frame-drop-count",MPV_FORMAT_INT64,&decoder);mpv_get_property(player,"frame-drop-count",MPV_FORMAT_INT64,&output);
  char line[192];snprintf(line,sizeof line,"MPV: closing; decoder dropped %lld frames, output dropped %lld; %llu frames rendered, %.1f ms average, %.1f ms longest",(long long)decoder,(long long)output,
   (unsigned long long)render_count,render_count?render_total_us/1000.0/render_count:0.0,render_max_us/1000.0);plog(line);}
 // The render thread first: nothing may render while the context goes away.
 if(renderer)mpv_render_context_set_update_callback(renderer,nullptr,nullptr);
 if(render_running){pthread_mutex_lock(&frame_lock);render_stop=true;pthread_cond_signal(&frame_wake);pthread_mutex_unlock(&frame_lock);
  // Each step logged once it takes long: a close that hangs shows where.
  const uint64_t asked=mono_ms();bool slow=false;
  while(render_busy&&!slow){if(mono_ms()-asked>2000){slow=true;plog("MPV: the video thread is still rendering after 2 s");}else usleep(2000);}
  pthread_join(render_thread,nullptr);render_running=false;if(slow)plog("MPV: the video thread stopped");}
 for(auto& f:frames){free(f.px);f.px=nullptr;}newest=reading=-1;
 // MPV itself goes on a thread of its own: tearing it down waits on its
 // network reads, and a Live TV reconnect while a Pluto ad break kept
 // resetting the stream froze the app there. Up to 3 s here, then it is
 // left to finish alone.
 if(renderer||player){
  struct Teardown{mpv_render_context* renderer;mpv_handle* player;std::shared_ptr<std::atomic<bool>> done;};
  auto done=std::make_shared<std::atomic<bool>>(false);auto job=new Teardown{renderer,player,done};renderer=nullptr;player=nullptr;
  pthread_t thread;
  auto run=[](void* p)->void*{auto t=(Teardown*)p;if(t->renderer)mpv_render_context_free(t->renderer);if(t->player)mpv_terminate_destroy(t->player);*t->done=true;delete t;return nullptr;};
  if(pthread_create(&thread,nullptr,run,job)!=0){run(job);return;}
  pthread_detach(thread);
  const uint64_t began=mono_ms();while(!*done&&mono_ms()-began<3000)usleep(5000);
  if(!*done)plog("MPV: the old player is still closing after 3 s; going on without it");
 }
}
