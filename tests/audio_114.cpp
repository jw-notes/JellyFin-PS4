// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// Audio: the production playback_url() and player_mpv.cpp against a real server
// and real files, on the host's libmpv writing what it would play to a PCM file.
//   audio-114 <server> <user> <password> <media dir> <idmap: "file id" lines>
// Without a server it runs only the checks that need none.
// The host's MPV and FFmpeg are newer than the PS4's: the log lines from the
// console (tools/fetch_console_logs.py) are what confirm levels there.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include "app.h"
#include "platform.h"
#include "audio_limiter.h"
#include "local_media.h"
#include "playback_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <map>
#include <sstream>
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
uint32_t* framebuffer(){return pixels.data();}
static int failures=0;
#define CHECK(cond,...) do{if(cond)printf("PASS ");else{printf("FAIL ");++failures;}printf(__VA_ARGS__);puts("");}while(0)

static void limiter_checks(){
 StereoLimiter limiter;std::vector<float> quiet(9600*2),copy;for(size_t i=0;i<quiet.size();++i)quiet[i]=0.9f*sinf(i*0.01f);copy=quiet;limiter.process(quiet.data(),9600);
 CHECK(quiet==copy&&limiter.limited_frames==0,"limiter: below the ceiling, bit-identical");
 std::vector<float> loud(48000*2);for(size_t i=0;i<loud.size();i+=2)loud[i]=loud[i+1]=1.8f*sinf(i*0.02f);limiter.reset();limiter.process(loud.data(),48000);
 float peak=0;for(float v:loud)peak=std::max(peak,fabsf(v));
 CHECK(peak<=StereoLimiter::CEILING+1e-5f&&limiter.limited_frames>0,"limiter: 1.8 in, %.3f out (ceiling %.2f)",peak,StereoLimiter::CEILING);
 std::vector<float> after(96000*2,0.1f);limiter.process(after.data(),96000);
 CHECK(limiter.gain==1.0f&&after.back()==0.1f,"limiter: back to unity after the passage (gain %.6f)",limiter.gain);
 // One peak in a long quiet stretch must not leave the rest of it altered.
 {StereoLimiter once;std::vector<float> stretch(48000*2*10,0.2f);stretch[1000]=1.5f;once.process(stretch.data(),48000*10);
  CHECK(once.limited_frames<48000*2&&stretch.back()==0.2f,"limiter: one peak changes under 2 s of a 10 s stretch (%llu frames)",(unsigned long long)once.limited_frames);}
}
static void model_checks(){
 const std::string source=R"({"Id":"a","Container":"mkv","DefaultAudioStreamIndex":3,"MediaStreams":[
 {"Type":"Video","Index":0,"Codec":"h264","Width":1280,"Height":534,"BitDepth":8},
 {"Type":"Audio","Index":1,"Codec":"aac","Channels":2,"IsDefault":true},
 {"Type":"Subtitle","Index":2,"Codec":"srt"},
 {"Type":"Audio","Index":3,"Codec":"ac3","Channels":6,"ChannelLayout":"5.1","IsExternal":false},
 {"Type":"Audio","Index":4,"Codec":"eac3","Channels":6},
 {"Type":"Audio","Index":9,"Codec":"aac","IsExternal":true}]})";
 CHECK(playback_audio_ordinal(source,1)==1,"ordinal: first audio stream is track 1");
 CHECK(playback_audio_ordinal(source,3)==2,"ordinal: a subtitle between does not count");
 CHECK(playback_audio_ordinal(source,4)==3,"ordinal: third audio stream");
 CHECK(playback_audio_ordinal(source,9)==-1&&playback_audio_ordinal(source,2)==-1&&playback_audio_ordinal(source,77)==-1,"ordinal: external, non-audio and unknown streams are not in the file");
 CHECK(playback_default_audio(source)==3&&playback_default_audio("{}")==-1,"default audio index");
 CHECK(playback_url_param("/v/master.m3u8?a=1&TranscodeReasons=X,Y&b=2","TranscodeReasons")=="X,Y"&&playback_url_param("/v?a=1","b").empty()&&playback_url_param("","b").empty(),"URL parameter");
 auto report=playback_source_report(source,3);std::string joined;for(auto& l:report)joined+=l+"\n";
 CHECK(joined.find("container=mkv")!=std::string::npos&&joined.find("codec=ac3 channels=6 layout=5.1")!=std::string::npos&&joined.find("SELECTED")!=std::string::npos&&joined.find("srt")==std::string::npos,"source report");
}
// Reads the WAV MPV wrote: stereo 48 kHz, in whichever sample format it chose.
struct Level{double rms_db=-120,peak_db=-120,seconds=0;};
static Level measure(const std::string& file){
 Level out;FILE* f=fopen(file.c_str(),"rb");if(!f)return out;
 char id[4];uint32_t size=0;fseek(f,12,SEEK_SET);unsigned tag=0,bits=0,channels=2;bool found=false;
 while(fread(id,1,4,f)==4&&fread(&size,4,1,f)==1){
  if(!memcmp(id,"fmt ",4)){uint8_t format[40]={};const size_t got=fread(format,1,std::min<uint32_t>(size,40),f);(void)got;tag=format[0]|format[1]<<8;channels=format[2]|format[3]<<8;bits=format[14]|format[15]<<8;if(tag==0xFFFE)tag=format[24]|format[25]<<8;if(size>40)fseek(f,size-40,SEEK_CUR);continue;}
  if(!memcmp(id,"data",4)){found=true;break;}
  fseek(f,size,SEEK_CUR);}
 if(!found||!bits){fclose(f);return out;}
 const unsigned bytes=bits/8;std::vector<uint8_t> buffer(bytes*4096);double sum=0,peak=0;uint64_t count=0;size_t n;
 while((n=fread(buffer.data(),bytes,4096,f))>0){for(size_t i=0;i<n;++i){double v=0;const uint8_t* p=buffer.data()+i*bytes;
   if(tag==3&&bytes==4){float s;memcpy(&s,p,4);v=s;}else if(bytes==2){int16_t s;memcpy(&s,p,2);v=s/32768.0;}else if(bytes==4){int32_t s;memcpy(&s,p,4);v=s/2147483648.0;}
   sum+=v*v;peak=std::max(peak,fabs(v));}count+=n;}
 fclose(f);if(count){out.rms_db=10*log10(sum/count+1e-20);out.peak_db=20*log10(peak+1e-20);out.seconds=count/(double)channels/48000.0;}return out;
}
static std::string pcm_path(){return std::string(getenv("JF_TEST_DATA"))+"/out.wav";}
static void use_pcm(const char* decoders=nullptr){setenv("JF_TEST_AO","pcm",1);setenv("JF_TEST_AO_FILE",pcm_path().c_str(),1);if(decoders)setenv("JF_TEST_AD",decoders,1);else unsetenv("JF_TEST_AD");}
// Runs until the end of the file (the PCM output is not paced) or the time limit.
static bool run_to_end(int limit_ms,bool& got_audio,bool& got_video,bool& failed){
 got_audio=got_video=failed=false;
 for(int waited=0;waited<limit_ms;waited+=5){if(player_draw())got_video=true;if(!player_error().empty())return false;if(player_has_audio())got_audio=true;if(player_audio_failed())failed=true;if(!player_active())return true;usleep(5000);}
 return false;
}
static Level play_file(const std::string& url,int track,bool* audio=nullptr){
 use_pcm();player_prefer_audio(track);remove(pcm_path().c_str());assert(player_open(url));bool a,v,f;run_to_end(120000,a,v,f);player_close();if(audio)*audio=a;usleep(150000);return measure(pcm_path());
}
static void local_checks(const std::string& media){
 const char* expected_near[]={"AAC-Stereo.mkv","AAC-5.1.mkv","Dolby-AC3-5.1.mkv","Dolby-EAC3-5.1.mkv"};std::map<std::string,Level> levels;
 for(auto name:expected_near){bool audio=false;Level l=play_file(media+"/"+name,0,&audio);levels[name]=l;CHECK(audio&&l.seconds>55,"local %s: decoded, %.1f s, RMS %.1f dBFS, peak %.1f dBFS",name,l.seconds,l.rms_db,l.peak_db);}
 const double ac3_gap=fabs(levels["Dolby-AC3-5.1.mkv"].rms_db-levels["AAC-5.1.mkv"].rms_db),eac3_gap=fabs(levels["Dolby-EAC3-5.1.mkv"].rms_db-levels["AAC-5.1.mkv"].rms_db);
 CHECK(ac3_gap<1.5&&eac3_gap<1.5,"AC-3 and E-AC-3 are within 1.5 dB of the same mix in AAC 5.1 (%.1f, %.1f dB)",ac3_gap,eac3_gap);
 {bool audio=false;Level l=play_file(media+"/original-dolby-5.1.ac3",0,&audio);CHECK(audio&&l.seconds>700,"raw .ac3 on a drive: decoded %.0f s, RMS %.1f dBFS, peak %.1f dBFS",l.seconds,l.rms_db,l.peak_db);}
 // Multi-track: English before the French default; the menu lists all; in-place switch.
 use_pcm();remove(pcm_path().c_str());setenv("JF_TEST_AO","null",1);snprintf(g_audio_lang,4,"en");player_prefer_audio(0);assert(player_open(media+"/Multi-Track.mkv"));
 for(int i=0;i<4000&&!player_loaded();++i){player_draw();usleep(1000);}
 auto tracks=player_audio_tracks();std::string names;int selected=-1;for(size_t i=0;i<tracks.size();++i){names+=tracks[i].label+" | ";if(tracks[i].selected)selected=(int)i;}
 printf("     menu: %s\n",names.c_str());
 CHECK(tracks.size()==4,"Multi-Track: 4 audio tracks listed");
 CHECK(selected==1,"Multi-Track on a USB-style path: the English AC-3 track starts, not the French default (selected %d)",selected);
 player_select_audio(tracks[2].id);bool switched=false;for(int i=0;i<4000;++i){player_draw();auto now=player_audio_tracks();if(now.size()==4&&now[2].selected&&player_has_audio()){switched=true;break;}usleep(1000);}
 CHECK(switched,"Multi-Track: switched in place to E-AC-3 with sound");
 player_seek(30000);bool sought=false;for(int i=0;i<5000;++i){player_draw();auto t=player_time();if(t>=29000&&t<45000){sought=true;break;}usleep(1000);}
 CHECK(sought&&player_error().empty(),"Multi-Track: seeks after switching");
 player_close();
 // An audio codec the player has no decoder for: picture on, reported, no failure of playback.
 use_pcm("mp3,-");setenv("JF_TEST_AO","null",1);player_prefer_audio(0);assert(player_open(media+"/Dolby-AC3-5.1.mkv"));bool video=false,reported=false,ok=true;
 for(int i=0;i<9000;++i){if(player_draw())video=true;if(!player_error().empty())ok=false;if(player_audio_failed()){reported=true;if(video)break;}usleep(1000);}
 player_close();unsetenv("JF_TEST_AD");
 CHECK(reported,"undecodable audio is reported");CHECK(video&&ok,"undecodable audio: the picture still plays");
 // Picture-only control: a good file must not report a failure.
 setenv("JF_TEST_AO","null",1);player_prefer_audio(0);assert(player_open(media+"/AAC-Stereo.mkv"));bool false_alarm=false;for(int i=0;i<6000;++i){player_draw();if(player_audio_failed())false_alarm=true;usleep(1000);}player_close();
 CHECK(!false_alarm,"a good file never reports an audio failure");
}
static void browse_checks(const std::string& media){
 setenv("JF_USB_ROOT","/nonexistent",1);std::vector<LocalEntry> entries;CHECK(list_local_folder(media,entries),"USB browsing lists the folder");
 std::map<std::string,int> kinds;for(auto& e:entries)kinds[e.name]=e.kind;
 CHECK(kinds.count("original-dolby-5.1.ac3")&&kinds["original-dolby-5.1.ac3"]==LOCAL_AUDIO,".ac3 is listed as audio");
 CHECK(kinds.count("Dolby-EAC3-5.1.mkv")&&kinds["Dolby-EAC3-5.1.mkv"]==LOCAL_VIDEO,".mkv is listed as video");
}
// A real FTP server (GoldHEN's on the console, read only), as the sandboxed app uses it.
static void ftp_checks(const char* address){
 setenv("JF_USB_FTP",address,1);
 auto drives=usb_drives();std::string names;for(auto& d:drives)names+=d+" ";
 CHECK(!drives.empty(),"FTP: drives found: %s",names.c_str());if(drives.empty()){unsetenv("JF_USB_FTP");return;}
 std::vector<LocalEntry> root;CHECK(list_local_folder(drives[0],root)&&!root.empty(),"FTP: drive listed, %zu shown entries",root.size());
 std::vector<LocalEntry> entries;const std::string folder=drives[0]+"/Jellyfin-Audio-Tests";CHECK(list_local_folder(folder,entries)&&!entries.empty(),"FTP: test folder listed, %zu entries",entries.size());
 for(auto& e:entries)printf("     %s kind=%d bytes=%llu\n",e.name.c_str(),e.kind,(unsigned long long)e.bytes);
 for(auto& e:entries){if(e.name!="Dolby-AC3-5.1.mkv"&&e.name!="Dolby-EAC3-5.1.mkv"&&e.name!="AAC-5.1.mkv"&&e.name!="AAC-Stereo.mkv")continue;
  const Item item=local_item(e);CHECK(local_is_drive_url(item.local),"FTP: %s plays from the drive URL",e.name.c_str());
  use_pcm();setenv("JF_TEST_AO","null",1);player_prefer_audio(0);assert(player_open(item.local));bool audio=false,video=false;
  for(int i=0;i<15000&&!(audio&&video&&player_time()>1500);++i){if(player_draw())video=true;if(player_has_audio())audio=true;if(!player_error().empty())break;usleep(1000);}
  CHECK(audio&&video&&player_error().empty(),"FTP: %s: picture and sound, at %.1f s",e.name.c_str(),player_time()/1000.0);
  player_seek(40000);bool sought=false;for(int i=0;i<15000;++i){player_draw();const auto t=player_time();if(t>=39000&&t<55000){sought=true;break;}if(!player_error().empty())break;usleep(1000);}
  CHECK(sought&&player_error().empty(),"FTP: %s: seeks to 40 s (at %.1f s)",e.name.c_str(),player_time()/1000.0);
  const uint64_t length=player_duration_ms();CHECK(length>59000&&length<61000,"FTP: %s: the player knows its length (%.1f s), for the bar and the seek limit",e.name.c_str(),length/1000.0);
  // Back and forward again, and it keeps playing from where it landed.
  bool steady=true;for(uint64_t to:{10000ull,50000ull,5000ull}){player_seek(to);bool landed=false;for(int i=0;i<15000&&!landed;++i){player_draw();const auto t=player_time();landed=t+1000>=to&&t<to+6000;usleep(1000);}
   const uint64_t from=player_time();for(int i=0;i<1500;++i){player_draw();usleep(1000);}steady=steady&&landed&&player_active()&&player_error().empty()&&player_time()>from;}
  CHECK(steady,"FTP: %s: seeks back and forward, and carries on playing",e.name.c_str());player_close();}
 // Its index at the end: reached by jumping to the end and coming back, which
 // FFmpeg's own ftp:// could not do. Far from the start, too, for a seek back.
 for(auto& e:entries){if(e.name!="tears_of_steel_720p.mov")continue;
  const Item item=local_item(e);use_pcm();setenv("JF_TEST_AO","null",1);player_prefer_audio(0);assert(player_open(item.local));bool audio=false,video=false;
  for(int i=0;i<30000&&!(audio&&video&&player_time()>1500);++i){if(player_draw())video=true;if(player_has_audio())audio=true;if(!player_error().empty())break;usleep(1000);}
  CHECK(audio&&video&&player_error().empty(),"FTP: %s: picture and sound, at %.1f s, length %.1f s",e.name.c_str(),player_time()/1000.0,player_duration_ms()/1000.0);
  bool steady=true;for(uint64_t to:{300000ull,600000ull,30000ull}){player_seek(to);bool landed=false;for(int i=0;i<30000&&!landed;++i){player_draw();const auto t=player_time();landed=t+2000>=to&&t<to+60000;if(!player_error().empty())break;usleep(1000);}
   const uint64_t from=player_time();for(int i=0;i<1500;++i){player_draw();usleep(1000);}steady=steady&&landed&&player_active()&&player_error().empty()&&player_time()>from;printf("     after seek to %.0f s: at %.1f s\n",to/1000.0,player_time()/1000.0);}
  CHECK(steady,"FTP: %s: seeks forward and back, and carries on playing",e.name.c_str());player_close();}
 // 27 subtitle files beside the film: all listed, any of them chosen and shown.
 for(auto& e:entries){if(e.name!="tears_of_steel_720p.mov")continue;
  const Item item=local_item(e);const auto files=local_subtitle_files(item.local);std::string names;for(auto& f:files)names+=f.label+" | ";printf("     beside it: %s\n",names.c_str());
  CHECK(files.size()>=20,"FTP: %zu subtitle files beside %s found",files.size(),e.name.c_str());
  std::vector<PlayerSubtitleFile> side;for(const auto& f:files)side.push_back({f.url,f.label,f.lang});
  setenv("JF_TEST_AO","null",1);player_prefer_subtitle(-1,"",0);player_subtitle_files(side);player_prefer_audio(0);assert(player_open(item.local));
  for(int i=0;i<20000&&!(player_draw()&&player_time()>500);++i)usleep(1000);
  bool all=true;for(const char* want:{"English - SRT (en)","German - SRT","Russian - SRT"}){auto list=player_subtitle_tracks();int id=0;for(auto& t:list)if(t.label==want)id=t.id;if(!id){all=false;continue;}
   player_select_subtitle(id);bool shown=false;for(int i=0;i<8000&&!shown;++i){player_draw();for(auto& t:player_subtitle_tracks())if(t.label==want&&t.selected&&t.id>0)shown=true;usleep(1000);}
   printf("     %s: %s\n",want,shown?"loaded and selected":"NOT selected");all=all&&shown;}
  CHECK(all&&player_error().empty(),"FTP: English, German and Russian files each loaded and selected in turn, playback carrying on");player_close();}
 unsetenv("JF_USB_FTP");
}
static void server_checks(const std::string& media,const std::map<std::string,std::string>& ids){
 auto item_for=[&](const std::string& name){Item item;snprintf(item.base.id,sizeof item.base.id,"%s",ids.at(name).c_str());snprintf(item.base.type,sizeof item.base.type,"Movie");return item;};
 std::string url,session,source;
 for(const char* name:{"AAC-Stereo.mkv","AAC-5.1.mkv","Dolby-AC3-5.1.mkv","Dolby-EAC3-5.1.mkv"}){
  assert(playback_url(item_for(name),-1,-1,"",url,session,source));const bool direct=playback_is_static();bool audio=false;Level l=play_file(url,playback_audio_track(),&audio);end_playback(session);
  CHECK(direct&&audio&&l.seconds>55,"server %s: direct play, track %d, RMS %.1f dBFS, peak %.1f dBFS",name,playback_audio_track(),l.rms_db,l.peak_db);}
 // A default that is not the page's: the English AC-3, directly, with the player on track 2.
 snprintf(g_audio_lang,4,"en");assert(playback_url(item_for("Multi-Track.mkv"),-1,-1,"",url,session,source));
 CHECK(playback_is_static()&&playback_audio_track()==2,"server Multi-Track: the page's English track, directly, as player track %d",playback_audio_track());end_playback(session);
 const std::string id_as_source=source;
 for(int stream:{1,2,3,4}){assert(playback_url(item_for("Multi-Track.mkv"),stream,-1,id_as_source,url,session,source));const bool ok=playback_is_static()&&playback_audio_track()==stream;
  bool audio=false;Level l=play_file(url,playback_audio_track(),&audio);end_playback(session);CHECK(ok&&audio,"server Multi-Track stream %d: direct as track %d, RMS %.1f dBFS",stream,playback_audio_track(),l.rms_db);}
 // Video the profile refuses: the server's H264/AAC stereo.
 {assert(playback_url(item_for("HEVC-AC3-5.1.mkv"),-1,-1,"",url,session,source));const bool hls=url.find("master.m3u8")!=std::string::npos&&url.find("AudioCodec=aac")!=std::string::npos;bool audio=false;Level l=play_file(url,playback_audio_track(),&audio);end_playback(session);
  CHECK(!playback_is_static()&&hls&&audio,"server HEVC + AC-3: transcode to AAC, RMS %.1f dBFS, %.1f s",l.rms_db,l.seconds);}
 // The player's fallback: the same Dolby file forced to a transcode.
 {playback_force_transcode_once();assert(playback_url(item_for("Dolby-AC3-5.1.mkv"),-1,-1,"",url,session,source));CHECK(!playback_is_static()&&url.find("master.m3u8")!=std::string::npos&&url.find("AudioCodec=aac")!=std::string::npos,"fallback: a forced transcode asks for H264/AAC");
  bool audio=false;Level l=play_file(url,0,&audio);end_playback(session);CHECK(audio&&l.seconds>10,"fallback: plays with sound, RMS %.1f dBFS, %.1f s",l.rms_db,l.seconds);
  assert(playback_url(item_for("Dolby-AC3-5.1.mkv"),-1,-1,"",url,session,source));CHECK(playback_is_static(),"fallback: applies to one playback only");end_playback(session);}
 // Multi-track forced: the page's track survives the second request.
 {playback_force_transcode_once();assert(playback_url(item_for("Multi-Track.mkv"),-1,-1,"",url,session,source));CHECK(!playback_is_static()&&url.find("AudioStreamIndex=2")!=std::string::npos,"fallback: a forced transcode keeps the page's track");end_playback(session);}
 // DTS / TrueHD as chosen tracks: the profile does not list them, so the server converts.
 for(const char* name:{"H264-DTS-5.1.mkv","H264-TrueHD-5.1.mkv"}){assert(playback_url(item_for(name),1,-1,ids.at(name),url,session,source));CHECK(!playback_is_static(),"server %s chosen: transcoded (%s)",name,playback_url_param(url,"TranscodeReasons").c_str());end_playback(session);}
 logout();
}
int main(int argc,char** argv){
 // JF_TEST_THREAD_STACK=<KB>: new threads (MPV's, FFmpeg's) get that stack, as
 // small as the console's, while this one keeps the host's.
 if(const char* kb=getenv("JF_TEST_THREAD_STACK")){pthread_attr_t attr;pthread_attr_init(&attr);pthread_attr_setstacksize(&attr,(size_t)atoi(kb)*1024);pthread_setattr_default_np(&attr);}
 setvbuf(stdout,nullptr,_IOLBF,0);
 limiter_checks();model_checks();
 // audio-114 --ftp <address>: only the checks against a real FTP server.
 if(argc==3&&!strcmp(argv[1],"--ftp")){settings.debug_log=true;assert(storage_init(114));ftp_checks(argv[2]);printf(failures?"FAILURES: %d\n":"ALL PASSED (FTP)\n",failures);return failures?1:0;}
 if(argc<5){printf(failures?"FAILURES: %d\n":"ALL PASSED (no server)\n",failures);return failures?1:0;}
 settings.debug_log=true;assert(storage_init(114));
 if(argc>6)ftp_checks(argv[6]);
 browse_checks(argv[4]);local_checks(argv[4]);
 snprintf(g_server,sizeof g_server,"%s",argv[1]);assert(login(argv[2],argv[3]));
 std::map<std::string,std::string> ids;{std::ifstream in(argv[5]);std::string name,id;while(in>>name>>id)ids[name]=id;}
 server_checks(argv[4],ids);
 printf(failures?"FAILURES: %d\n":"ALL PASSED\n",failures);return failures?1:0;
}
