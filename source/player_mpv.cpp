// Uses the PS4 MPV/FFmpeg build from dragonflylee/switchfin's ps4 branch.
// The application UI and Jellyfin session/negotiation code remain shared.
#include "app.h"
#include "platform.h"
#include "mono_clock.h"
#include "music_viz.h"
#include "local_media.h"
#include "subtitles.h"
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/stream_cb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <unistd.h>
#include <sys/stat.h>
// The subtitle faces, written where libass can load them. Their own copies,
// in a namespace: render.cpp has Roboto Condensed under the same name.
namespace subtitle_faces {
#include "../vendor/ps3/source/gfx/opensans_bold.h"
#include "../vendor/ps3/source/gfx/notosans_bold.h"
#include "../vendor/ps3/source/gfx/robotocondensed_bold.h"
}
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
static std::atomic<uint64_t> first_frame_ms{0};
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
  if(started&&!got_video){got_video=true;first_frame_ms=mono_ms();plog(("MPV: first frame after "+std::to_string(mono_ms()-opened_ms)+" ms").c_str());}
 }
}
static std::string safe_log(std::string line){
 for(const char* scheme:{"http://","https://"}){size_t at=0;while((at=line.find(scheme,at))!=std::string::npos){auto end=line.find_first_of(" \t\r\n\"'",at);line.replace(at,end==std::string::npos?line.size()-at:end-at,"[stream URL]");at+=12;}}
 if(g_token[0]){size_t at=0;while((at=line.find(g_token,at))!=std::string::npos){line.replace(at,strlen(g_token),"[token]");at+=7;}}
 for(char& c:line)if((unsigned char)c<32)c=' ';
 return line;
}
static bool option(const char* key,const char* value){int rc=mpv_set_option_string(player,key,value);if(rc<0)plog((std::string("MPV: option ")+key+" failed: "+mpv_error_string(rc)).c_str());return rc>=0;}
// ---- the sound ---------------------------------------------------------------
// The audio track the next player_open() starts on (0: MPV chooses).
static int next_audio_track=0;
void player_prefer_audio(int track){next_audio_track=track;}
// A file's tag may be either form of the language's code: list them all for MPV.
static std::string language_codes(const char* code){
 static const char* const known[][2]={{"en","en,eng"},{"ja","ja,jpn"},{"pt","pt,por"},{"de","de,ger,deu"},{"fr","fr,fre,fra"},{"es","es,spa"}};
 for(const auto& row:known)if(!strncmp(code,row[0],2))return row[1];
 return code;
}
// ---- subtitles ---------------------------------------------------------------
// The console's libass has no fonts of its own (no fontconfig): it gets the
// three faces of subtitles.h from a folder, written once. Without them text
// subtitles draw nothing at all.
static std::string prop(const char* name);static std::string prop(const std::string& name);
// sub-add's reply: a subtitle file that will not load is not the film failing.
static const uint64_t SUB_ADD_REPLY=0x5AB5;
struct SideFile{PlayerSubtitleFile file;bool loaded=false;};
static std::vector<SideFile> side_files,next_side_files;
static int next_sub_track=-1;static std::string next_sub_url,pending_sub_url;static int64_t next_sub_delay=0;
void player_prefer_subtitle(int track,const std::string& url,int64_t delay_ms){next_sub_track=track;next_sub_url=url;next_sub_delay=delay_ms;}
static std::string subtitle_fonts_dir(){
#ifdef HOST_TEST
 const std::string dir=data_dir+"/fonts";
#else
 const std::string dir="/data/JellyFin-PS4/fonts";
#endif
 static bool ready=false;if(ready)return dir;
 mkdir(dir.c_str(),0755);
 const struct{const char* name;const unsigned char* data;unsigned size;} faces[]={
  {"OpenSans-Bold.ttf",subtitle_faces::OpenSans_Bold_ttf,subtitle_faces::OpenSans_Bold_ttf_len},
  {"NotoSans-Bold.ttf",subtitle_faces::NotoSans_Bold_ttf,subtitle_faces::NotoSans_Bold_ttf_len},
  {"RobotoCondensed-Bold.ttf",subtitle_faces::RobotoCondensed_Bold_ttf,subtitle_faces::RobotoCondensed_Bold_ttf_len}};
 bool all=true;
 for(const auto& face:faces){const std::string path=dir+"/"+face.name;if(local_file_size(path)==face.size)continue;
  const std::string part=path+".part";FILE* out=fopen(part.c_str(),"wb");bool wrote=out&&fwrite(face.data,1,face.size,out)==face.size;if(out&&fclose(out)!=0)wrote=false;
  if(wrote&&rename(part.c_str(),path.c_str())==0){plog(("Subtitles: wrote "+path).c_str());continue;}
  remove(part.c_str());all=false;plog(("Subtitles: could not write "+path).c_str());}
 ready=all;return dir;
}
static void subtitle_options(){
 option("sub-font-provider","none");
 // Files beside a video are the app's to list (local_subtitle_files), not MPV's
 // to load and show by themselves: it would for a local path, not for USB.
 option("sub-auto","no");option("sub-fonts-dir",subtitle_fonts_dir().c_str());
 // Text subtitles in the chosen face and look; ASS keeps its own styles.
 option("sub-font",subtitle_font_family(settings.sub_font));option("sub-bold","yes");
 const SubtitleLook look=subtitle_look(settings.sub_colour);
 option("sub-color",look.fill);option("sub-border-color",look.outline);option("sub-border-size",look.outline_size);
 // A file from the server is added once the stream has loaded; until then, and
 // for "none", nothing from inside the file (a track it flags default would show).
 pending_sub_url=next_sub_url;side_files.swap(next_side_files);next_side_files.clear();
 if(!side_files.empty())plog(("MPV: "+std::to_string(side_files.size())+" subtitle file(s) beside it").c_str());
 if(next_sub_track==0||!next_sub_url.empty())option("sid","no");else if(next_sub_track>0)option("sid",std::to_string(next_sub_track).c_str());
 if(next_sub_delay){char delay[32];snprintf(delay,sizeof delay,"%.3f",next_sub_delay/1000.0);option("sub-delay",delay);}
 plog(("MPV: subtitles "+std::string(!next_sub_url.empty()?"from the server's file":next_sub_track==0?"off":next_sub_track>0?"track "+std::to_string(next_sub_track):"as the file says")+", "+subtitle_font_family(settings.sub_font)+", "+subtitle_colour_label(settings.sub_colour)).c_str());
 next_sub_track=-1;next_sub_url.clear();next_sub_delay=0;
}
// MPV's codec names, as a viewer knows them.
static std::string subtitle_codec(std::string codec){
 if(codec=="subrip")return "SRT";if(codec=="hdmv_pgs_subtitle")return "PGS";if(codec=="dvd_subtitle")return "DVD";if(codec=="webvtt")return "VTT";if(codec=="mov_text")return "Text";
 for(char& c:codec)c=(char)toupper((unsigned char)c);return codec;
}
// Files beside the one playing: listed by the menu, each loaded the first time it is chosen.
void player_subtitle_files(const std::vector<PlayerSubtitleFile>& files){next_side_files.clear();for(const auto& f:files)next_side_files.push_back({f,false});}
static std::string stream_url(const std::string& url);
std::vector<PlayerAudioTrack> player_subtitle_tracks(){
 std::vector<PlayerAudioTrack> tracks;if(!player)return tracks;
 int64_t count=0;mpv_get_property(player,"track-list/count",MPV_FORMAT_INT64,&count);
 for(int n=0;n<count;++n){const std::string at="track-list/"+std::to_string(n)+"/";if(prop(at+"type")!="sub")continue;
  // A file beside it, once loaded, keeps the name it was listed with.
  const std::string from=prop(at+"external-filename");const SideFile* side=nullptr;for(const auto& s:side_files)if(s.loaded&&!from.empty()&&stream_url(s.file.url)==from)side=&s;
  if(side){PlayerAudioTrack track;track.id=atoi(prop(at+"id").c_str());track.label=side->file.label;track.selected=prop(at+"selected")=="yes";tracks.push_back(track);continue;}
  std::string label=prop(at+"lang");auto add=[&](const std::string& part){if(part.empty())return;if(!label.empty())label+=" - ";label+=part;};
  add(subtitle_codec(prop(at+"codec")));add(prop(at+"title"));if(prop(at+"forced")=="yes")add("Forced");
  PlayerAudioTrack track;track.id=atoi(prop(at+"id").c_str());track.label=safe_log(label.empty()?"Subtitles":label);track.selected=prop(at+"selected")=="yes";tracks.push_back(track);}
 for(size_t i=0;i<side_files.size();++i)if(!side_files[i].loaded){PlayerAudioTrack track;track.id=-(int)i-1;track.label=side_files[i].file.label;tracks.push_back(track);}
 return tracks;
}
void player_select_subtitle(int track){
 if(!player)return;
 // A file beside it: loaded now, and shown once it has.
 if(track<0&&-track-1<(int)side_files.size()){SideFile& side=side_files[-track-1];side.loaded=true;const std::string url=stream_url(side.file.url);
  const char* add[]={"sub-add",url.c_str(),"select",side.file.label.c_str(),side.file.lang.c_str(),nullptr};const int rc=mpv_command_async(player,SUB_ADD_REPLY,add);
  plog(("MPV: subtitles from the file beside it ("+side.file.label+")"+(rc<0?std::string(" failed: ")+mpv_error_string(rc):std::string())).c_str());return;}
 const int rc=track>0?[&]{int64_t id=track;return mpv_set_property(player,"sid",MPV_FORMAT_INT64,&id);}():mpv_set_property_string(player,"sid","no");
 plog(("MPV: subtitles "+(track>0?"track "+std::to_string(track):std::string("off"))+(rc<0?std::string(" failed: ")+mpv_error_string(rc):std::string())).c_str());
}
// The file has an audio track but MPV could not decode or output it.
static bool had_audio_track=false;
static std::atomic<bool> audio_failed{false};
static unsigned underruns=0,desyncs=0;
static std::string last_audio_params,last_output_params;
static std::string prop(const char* name){char* value=mpv_get_property_string(player,name);if(!value)return {};std::string out=value;mpv_free(value);return out;}
static std::string prop(const std::string& name){return prop(name.c_str());}
static std::string or_unknown(const std::string& value){return value.empty()?"unknown":value;}
// "English - AC3 - 5.1 - Director Commentary", for the track menu.
static std::string track_label(int n){
 const std::string at="track-list/"+std::to_string(n)+"/";std::string codec=prop(at+"codec"),lang=prop(at+"lang"),title=prop(at+"title"),channels=prop(at+"demux-channels");
 for(char& c:codec)c=(char)toupper((unsigned char)c);
 // A file may give only a count: MPV then says "unknown6".
 const std::string count=prop(at+"demux-channel-count");
 if(channels.empty()||channels.rfind("unknown",0)==0)channels=count=="1"?"Mono":count=="2"?"Stereo":count=="6"?"5.1":count=="8"?"7.1":count.empty()?"":count+"ch";
 else if(channels=="stereo")channels="Stereo";else if(channels=="mono")channels="Mono";else if(channels=="5.1(side)")channels="5.1";else if(channels=="7.1(wide)")channels="7.1";
 std::string out=lang;
 auto add=[&](const std::string& part){if(part.empty())return;if(!out.empty())out+=" - ";out+=part;};
 add(codec);add(channels);add(title);return out.empty()?"Audio":out;
}
std::vector<PlayerAudioTrack> player_audio_tracks(){
 std::vector<PlayerAudioTrack> tracks;if(!player)return tracks;
 int64_t count=0;mpv_get_property(player,"track-list/count",MPV_FORMAT_INT64,&count);
 for(int n=0;n<count;++n){const std::string at="track-list/"+std::to_string(n)+"/";if(prop(at+"type")!="audio")continue;
  PlayerAudioTrack track;track.id=atoi(prop(at+"id").c_str());track.label=safe_log(track_label(n));track.selected=prop(at+"selected")=="yes";tracks.push_back(track);}
 return tracks;
}
void player_select_audio(int track){
 if(!player)return;
 // The new track's decoder has yet to prove itself. got_audio stays: a track of
 // the same format changes no audio-params, so nothing would set it again.
 audio_failed=false;
 int64_t id=track;const int rc=mpv_set_property(player,"aid",MPV_FORMAT_INT64,&id);
 plog(("MPV: switching to audio track "+std::to_string(track)+(rc<0?std::string(" failed: ")+mpv_error_string(rc):std::string())).c_str());
}
// What is in the file, as MPV sees it: the same whether it came from the server or a USB drive.
static void log_streams(){
 int64_t count=0;mpv_get_property(player,"track-list/count",MPV_FORMAT_INT64,&count);had_audio_track=false;
 plog(("MPV: container "+or_unknown(prop("file-format"))+", "+std::to_string(count)+" tracks").c_str());
 for(int n=0;n<count;++n){const std::string at="track-list/"+std::to_string(n)+"/",type=prop(at+"type");if(type=="audio")had_audio_track=true;if(type!="audio"&&type!="video"&&type!="sub")continue;
  std::string line="MPV: track "+prop(at+"id")+" "+type+" codec="+or_unknown(prop(at+"codec"));
  if(type=="audio")line+=" channels="+or_unknown(prop(at+"demux-channel-count"))+" layout="+or_unknown(prop(at+"demux-channels"))+" rate="+or_unknown(prop(at+"demux-samplerate"))+" lang="+or_unknown(prop(at+"lang"));
  else if(type=="sub")line+=" lang="+or_unknown(prop(at+"lang"))+(prop(at+"external")=="yes"?" external":"")+(prop(at+"forced")=="yes"?" forced":"");
  else line+=" "+or_unknown(prop(at+"demux-w"))+"x"+or_unknown(prop(at+"demux-h"));
  if(prop(at+"default")=="yes")line+=" default";if(prop(at+"selected")=="yes")line+=" SELECTED";plog(safe_log(line).c_str());}
 if(had_audio_track)return;
 plog("MPV: this file has no audio track");
}
// What the decoder gives and what goes to the sound port.
static void log_audio_chain(){
 const std::string in=prop("audio-params/format")+" "+prop("audio-params/channel-count")+"ch "+prop("audio-params/channels")+" "+prop("audio-params/samplerate")+" Hz";
 if(in!=last_audio_params){last_audio_params=in;plog(("Audio: decoder "+or_unknown(prop("audio-codec-name"))+" ("+or_unknown(prop("audio-codec"))+") gives "+in).c_str());
#ifndef HOST_TEST
  // Mixing more than two channels into stereo can pass full scale: limit it.
  const int channels=atoi(prop("audio-params/channel-count").c_str());audio_limiter_enable(channels>2);plog(channels>2?"Audio: downmix to stereo by the mixer, limiter on":"Audio: no downmix, limiter off");
  plog(audio_port_report().c_str());
#endif
 }
 const std::string out=prop("audio-out-params/format")+" "+prop("audio-out-params/channel-count")+"ch "+prop("audio-out-params/channels")+" "+prop("audio-out-params/samplerate")+" Hz";
 if(!prop("audio-out-params/format").empty()&&out!=last_output_params){last_output_params=out;plog(("Audio: output "+or_unknown(prop("current-ao"))+" "+out+" (PCM; no Dolby bitstream)").c_str());}
}
bool player_audio_failed(){
 if(audio_failed&&had_audio_track)return true;
 // MPV gives a track up by deselecting it, should its message ever change.
 if(!started||!had_audio_track||got_audio||!first_frame_ms||mono_ms()-first_frame_ms<4000)return false;
 return prop("current-tracks/audio/id").empty();
}
static void events(){
 if(!player)return;
 for(;;){auto e=mpv_wait_event(player,0);if(e->event_id==MPV_EVENT_NONE)break;
  if(e->event_id==MPV_EVENT_COMMAND_REPLY&&e->reply_userdata==SUB_ADD_REPLY){plog(e->error<0?(std::string("MPV: the subtitle file could not be loaded: ")+mpv_error_string(e->error)).c_str():"MPV: subtitle file loaded");}
  else if(e->event_id==MPV_EVENT_COMMAND_REPLY&&e->error<0){failure=std::string("Playback command failed: ")+mpv_error_string(e->error);ended=true;}
  else if(e->event_id==MPV_EVENT_FILE_LOADED){started=true;mpv_request_log_messages(player,settings.debug_log?"info":"error");plog(("MPV: stream loaded after "+std::to_string(mono_ms()-opened_ms)+" ms").c_str());if(!pending_sub_url.empty()){const char* add[]={"sub-add",pending_sub_url.c_str(),"select",nullptr};mpv_command_async(player,SUB_ADD_REPLY,add);pending_sub_url.clear();}
   if(settings.debug_log)log_streams();else{int64_t count=0;mpv_get_property(player,"track-list/count",MPV_FORMAT_INT64,&count);had_audio_track=false;for(int n=0;n<count;++n)if(prop("track-list/"+std::to_string(n)+"/type")=="audio")had_audio_track=true;}}
  else if(e->event_id==MPV_EVENT_END_FILE&&replacing&&e->data&&((mpv_event_end_file*)e->data)->reason==MPV_END_FILE_REASON_STOP){replacing=false;}
  else if(e->event_id==MPV_EVENT_END_FILE){auto end=(mpv_event_end_file*)e->data;ended=true;replacing=false;
   plog(("MPV: file ended at "+std::to_string(position)+" s, "+(!end?"no reason":end->reason==MPV_END_FILE_REASON_EOF?"end of file":end->reason==MPV_END_FILE_REASON_ERROR?"error":"stopped")).c_str());if(end&&end->reason==MPV_END_FILE_REASON_ERROR){failure=std::string("Playback failed: ")+mpv_error_string(end->error);plog(failure.c_str());}}
  else if(e->event_id==MPV_EVENT_LOG_MESSAGE){auto log=(mpv_event_log_message*)e->data;
   // While opening only the parts that say why a stream failed: the network, FFmpeg and the demuxers.
   if(log->text){if(strstr(log->text,"underrun"))++underruns;if(strstr(log->text,"desynchronisation"))++desyncs;
    // A video decoder's failure says the same words, under "vd".
    const bool video_side=log->prefix&&!strcmp(log->prefix,"vd");
    if((strstr(log->text,"Failed to initialize a decoder")&&!video_side)||strstr(log->text,"Could not open/initialize audio device"))audio_failed=true;}
   if(log->log_level>MPV_LOG_LEVEL_INFO&&log->prefix&&strcmp(log->prefix,"ffmpeg")&&strncmp(log->prefix,"stream",6)&&strncmp(log->prefix,"demux",5)&&strcmp(log->prefix,"lavf")&&strcmp(log->prefix,"cplayer"))continue;
   plog((std::string("MPV: ")+(log->prefix?std::string(log->prefix)+": ":"")+safe_log(log->text?log->text:"")).c_str());}
  else if(e->event_id==MPV_EVENT_PROPERTY_CHANGE){auto prop=(mpv_event_property*)e->data;if(!prop->data)continue;
   if(!strcmp(prop->name,"time-pos")&&prop->format==MPV_FORMAT_DOUBLE)position=*(double*)prop->data;
   else if(!strcmp(prop->name,"audio-params")&&prop->format==MPV_FORMAT_NODE){got_audio=true;if(settings.debug_log)log_audio_chain();
#ifndef HOST_TEST
    else audio_limiter_enable(atoi(::prop("audio-params/channel-count").c_str())>2);
#endif
   }
   else if(!strcmp(prop->name,"audio-out-params")&&prop->format==MPV_FORMAT_NODE){if(settings.debug_log)log_audio_chain();}
   else if(!strcmp(prop->name,"demuxer-cache-duration")&&prop->format==MPV_FORMAT_DOUBLE)cached=*(double*)prop->data;
  }
 }
}
// Live TV's reconnect: the stream loaded again in the same player. Tearing
// MPV down and creating a new one there crashed the app (twice, both right
// after a Pluto ad break had broken the stream's timestamps); loadfile's
// replace is MPV's own well-trodden path. The old file's end (reason STOP)
// is not the end of playback.
// A USB file read through GoldHEN's FTP server goes through our reader, not
// FFmpeg's ftp:// (local_media.cpp says why): the same address, under our own scheme.
static std::string stream_url(const std::string& url){return local_is_drive_url(url)?"jfusb"+url.substr(3):url;}
static int usb_stream_open(void*,char* uri,mpv_stream_cb_info* info){
 void* reader=ftp_reader_open(local_url_path(std::string("ftp")+(uri+5)));
 if(!reader){plog("MPV: could not open the USB file through FTP");return MPV_ERROR_LOADING_FAILED;}
 info->cookie=reader;
 info->read_fn=[](void* r,char* into,uint64_t room)->int64_t{return ftp_reader_read(r,into,room);};
 info->seek_fn=[](void* r,int64_t to)->int64_t{return to<0||ftp_reader_seek(r,(uint64_t)to)<0?MPV_ERROR_GENERIC:to;};
 info->size_fn=[](void* r)->int64_t{return ftp_reader_size(r);};
 info->close_fn=[](void* r){ftp_reader_close(r);};
 info->cancel_fn=[](void* r){ftp_reader_cancel(r);};
 return 0;
}
bool player_reload(const std::string& url){
 if(!player)return player_open(url);
 // started: off until the new stream has loaded, so a last frame of the old
 // one does not count as the new picture (it hid "Reconnecting" at once).
 replacing=true;failure.clear();ended=false;started=false;got_video=false;last_frame_ms=0;opened_ms=mono_ms();
 pthread_mutex_lock(&frame_lock);newest=-1;pthread_mutex_unlock(&frame_lock);
 const std::string target=stream_url(url);const char* command[]={"loadfile",target.c_str(),"replace",nullptr};const int rc=mpv_command_async(player,0,command);
 if(rc<0){replacing=false;last_error=std::string("Could not reopen stream: ")+mpv_error_string(rc);return false;}
 plog("MPV: reloading the stream in the same player");return true;
}
bool player_open(const std::string& url){
 player_close();failure.clear();position=cached=0;started=ended=got_audio=false;got_video=false;newest=reading=-1;render_count=render_total_us=render_max_us=0;last_frame_ms=0;
 had_audio_track=false;audio_failed=false;first_frame_ms=0;underruns=desyncs=0;last_audio_params.clear();last_output_params.clear();
#ifndef HOST_TEST
 audio_stats_reset();
#endif
 plog("MPV: creating player");player=mpv_create();if(!player){last_error="Could not create the MPV player.";return false;}
 // A Live TV channel played straight from its provider: no server referrer,
 // a browser's user agent (some providers turn others away), and its best
 // variant within about 4.5 Mbps (a 720p one): the screen is 720p, and a
 // 1080p channel cost 25 ms a frame to shrink, half its frames dropped.
 const bool on_drive=url.find("://")==std::string::npos||local_is_drive_url(url);
 const bool remote=!on_drive&&url.rfind(std::string(g_server)+"/",0)!=0;
 // Some channels have one heavy variant only (1080p, 50-60 fps): the decoder
 // may drop frames when it falls behind rather than the picture stalling, and
 // skips deblocking on frames nothing refers to. A channel at 23.237.104.106
 // lost half its frames without this.
 if(remote){option("user-agent","Mozilla/5.0 (PlayStation 4) AppleWebKit/605.1.15");option("hls-bitrate",std::to_string(std::min(settings.bitrate,4500000)).c_str());
  option("framedrop","decoder+vo");option("vd-lavc-skiploopfilter","nonref");
  // No kept-alive HTTPS connections between HLS segments: after a Pluto ad
  // break ("keepalive request failed", "mbedtls_ssl_read returned -0x0")
  // the app died as the stream was reloaded, tearing those down.
  option("demuxer-lavf-o","http_persistent=0");
  // Opening a channel probes every one of its variants (Pluto: five video,
  // seven audio) with up to 5 MB each. Free memory fell with each reconnect
  // (228, 208 MB) and the app died loading the next: probe 1 MB, 1.5 s.
  option("demuxer-lavf-probesize","1000000");option("demuxer-lavf-analyzeduration","1.5");}
 else{option("referrer",g_server);option("user-agent","JellyfinPS4/0.8");}option("config","no");option("terminal","no");option("idle","yes");option("vo","libmpv");
 // Verbose until the stream has loaded: when one will not open, the reason is
 // in FFmpeg's detail, not in the one-line "loading failed".
 mpv_request_log_messages(player,settings.debug_log?"v":"error");
#ifdef HOST_TEST
 {const char* ao=getenv("JF_TEST_AO");option("ao",ao?ao:"null");const char* file=getenv("JF_TEST_AO_FILE");if(file)option("ao-pcm-file",file);const char* ad=getenv("JF_TEST_AD");if(ad)option("ad",ad);}option("hwdec","no");option("ytdl","no");
#else
 option("ao","sdl");
 // Isolate the experimental Videodec2 path after a process crash on hardware.
 // Use the software path until that decoder is proven on the target firmware.
 option("hwdec","no");option("vd","h264,");
#endif
 // Stereo for the console's sound port. Every source of more channels goes
 // through one mixer: left the default, MPV asks the AC-3 / E-AC-3 decoder to
 // mix down instead, and that scales the result to fit, measured 6.9 dB quieter
 // than the same 5.1 mix in AAC (and than Jellyfin's own transcode of it). The
 // mixer's loud passages are held by the limiter in music_viz.cpp. Passthrough
 // is not possible here: ao_sdl sends the console PCM only.
 option("audio-channels","stereo");option("audio-samplerate","48000");option("ad-lavc-downmix","no");option("audio-normalize-downmix","no");
 // A file played from the console or a USB drive has no server to choose its
 // track: prefer the console's language, or the track already chosen.
 if(next_audio_track>0)option("aid",std::to_string(next_audio_track).c_str());
 else if(on_drive&&g_audio_lang[0])option("alang",language_codes(g_audio_lang).c_str());
 if(next_audio_track>0||on_drive)plog(("MPV: audio track "+(next_audio_track>0?std::to_string(next_audio_track):"by language "+language_codes(g_audio_lang))).c_str());
 next_audio_track=0;
 option("vd-lavc-threads","6");option("network-timeout","15");option("osd-level","0");
 // Memory: the read-ahead is capped, and the back buffer (kept for seeking
 // backwards, 50 MiB by default) mostly given up -- a 1080p stream ran out
 // of memory with both at their defaults.
 option("demuxer-max-bytes",remote?"16MiB":"32MiB");option("demuxer-max-back-bytes",remote?"2MiB":"4MiB");
 // Read ahead, as with FFmpeg's ftp://: mpv would take our reader for a local file.
 if(local_is_drive_url(url))option("cache","yes");
 // Scaling, when the picture is not already the screen's size: fast-bilinear
 // (a 240p channel upscaled with bicubic took 34 ms a frame). With 1080p (software decoding) the cheapest settings: skip
 // deblocking on frames nothing refers to, and fast-bilinear downscaling.
 option("sws-scaler","fast-bilinear");
 if(settings.hd1080){option("vd-lavc-skiploopfilter","nonref");option("vd-lavc-fast","yes");option("sws-scaler","fast-bilinear");}
 // Music shows its own Now Playing screen: never an embedded cover as video.
 option("audio-display","no");
 option("volume",std::to_string(std::max(0,std::min(100,settings.volume))).c_str());
 subtitle_options();
 plog("MPV: initializing software decoder/audio backend");int rc=mpv_initialize(player);if(rc<0){last_error=std::string("Could not initialize MPV: ")+mpv_error_string(rc);player_close();return false;}
 mpv_observe_property(player,0,"time-pos",MPV_FORMAT_DOUBLE);mpv_observe_property(player,0,"audio-params",MPV_FORMAT_NODE);mpv_observe_property(player,0,"audio-out-params",MPV_FORMAT_NODE);mpv_observe_property(player,0,"demuxer-cache-duration",MPV_FORMAT_DOUBLE);
 mpv_render_param params[]={{MPV_RENDER_PARAM_API_TYPE,(void*)MPV_RENDER_API_TYPE_SW},{MPV_RENDER_PARAM_INVALID,nullptr}};
 plog("MPV: creating software video renderer");rc=mpv_render_context_create(&renderer,player,params);if(rc<0){last_error=std::string("Could not initialize video rendering: ")+mpv_error_string(rc);player_close();return false;}
 for(auto& f:frames){void* memory=nullptr;if(posix_memalign(&memory,64,WIDTH*HEIGHT*4)){last_error="Video frame allocation failed.";player_close();return false;}f.px=(uint32_t*)memory;f.w=f.h=0;}
 render_stop=render_due=false;render_running=pthread_create(&render_thread,nullptr,render_loop,nullptr)==0;
 if(!render_running){last_error="Could not start the video thread.";player_close();return false;}
 mpv_render_context_set_update_callback(renderer,on_update,nullptr);
 player_picture(settings.pic_aspect,settings.pic_w,settings.pic_h,settings.pic_x,settings.pic_y);
 mpv_stream_cb_add_ro(player,"jfusb",nullptr,usb_stream_open);
 const std::string target=stream_url(url);
 opened_ms=mono_ms();const char* command[]={"loadfile",target.c_str(),"replace",nullptr};rc=mpv_command_async(player,0,command);
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
uint64_t player_duration_ms(){if(!player)return 0;double seconds=0;return mpv_get_property(player,"duration",MPV_FORMAT_DOUBLE,&seconds)>=0&&seconds>0?(uint64_t)(seconds*1000):0;}
void player_seek(uint64_t ms){if(player){std::string time=std::to_string(ms/1000.0);plog(("MPV: seek to "+time+" s from "+std::to_string(position)+" s").c_str());const char* command[]={"seek",time.c_str(),"absolute+exact",nullptr};mpv_command_async(player,0,command);}}
uint64_t player_time(){events();return position>0?(uint64_t)(position*1000):0;}
void player_close(){
 // How well the decoder kept up: frames it dropped, and frames shown late.
 if(player&&got_video){int64_t decoder=0,output=0;mpv_get_property(player,"decoder-frame-drop-count",MPV_FORMAT_INT64,&decoder);mpv_get_property(player,"frame-drop-count",MPV_FORMAT_INT64,&output);
  char line[192];snprintf(line,sizeof line,"MPV: closing; decoder dropped %lld frames, output dropped %lld; %llu frames rendered, %.1f ms average, %.1f ms longest",(long long)decoder,(long long)output,
   (unsigned long long)render_count,render_count?render_total_us/1000.0/render_count:0.0,render_max_us/1000.0);plog(line);}
 // How the sound held up: sync with the picture, gaps, and the level that reached the speakers.
 if(player&&(got_audio||had_audio_track)){char line[200];double avsync=0,total=0;mpv_get_property(player,"avsync",MPV_FORMAT_DOUBLE,&avsync);mpv_get_property(player,"total-avsync-change",MPV_FORMAT_DOUBLE,&total);
  snprintf(line,sizeof line,"Audio: closing; decoded=%s A-V %.3f s, total correction %.3f s, %u underruns, %u desync warnings",got_audio?"yes":"NO",avsync,total,underruns,desyncs);plog(line);
#ifndef HOST_TEST
  plog(audio_stats_report().c_str());
#endif
 }
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
