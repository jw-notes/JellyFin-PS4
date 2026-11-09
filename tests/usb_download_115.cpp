// Downloads to a USB drive the app can only reach through GoldHEN's FTP server:
// the production downloads.cpp, local_media.cpp and player_mpv.cpp against
// tests/fake_goldhen_ftp.py, which behaves as the console's server was measured
// to. The HTTP side is this file's: it serves a real media file with the
// interruptions each check asks for. See tools/test_usb_download_115.sh.
//   usb-download-115 <ftp address> <folder served as /mnt/usb0> <command log> <media file> <mov file> <server pid>
#include "app.h"
#include "platform.h"
#include "downloads.h"
#include "local_media.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iterator>
#include <sstream>
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
uint32_t* framebuffer(){return pixels.data();}
static int failures=0;
#define CHECK(cond,...) do{if(cond)printf("PASS ");else{printf("FAIL ");++failures;}printf(__VA_ARGS__);puts("");fflush(stdout);}while(0)
static std::string read_all(const std::string& path){std::ifstream in(path,std::ios::binary);return std::string((std::istreambuf_iterator<char>(in)),{});}
static bool exists(const std::string& path){struct stat sb;return stat(path.c_str(),&sb)==0;}

// ---- the server's side -------------------------------------------------------
static std::string media;
static uint64_t stop_after=0;        // the connection drops once this much has come, once
static bool ignore_range=false;      // the server answers a resume with the whole file
static uint64_t last_offset=0;int http_calls=0;
int http_init(){return 0;}void http_end(){}
int http_download_file(const char*,const char*,const std::function<bool(uint64_t,uint64_t)>&){return -1;}
int http_download_resume(const char* url,const char* path,uint64_t offset,const std::function<bool(uint64_t,uint64_t)>& progress){
 ++http_calls;last_offset=offset;FILE* out=fopen(path,offset?"ab":"wb");if(!out)return -1;fwrite(media.data()+offset,1,media.size()-offset,out);fclose(out);(void)url;return progress(media.size(),media.size())?200:-1;
}
int ffmpeg_download_file(const char*,const char*,const std::function<bool(uint64_t,uint64_t)>&){return -1;}
int http_download_to(const char*,DownloadSink& sink,uint64_t offset,const std::function<bool(uint64_t,uint64_t)>& progress){
 ++http_calls;last_offset=offset;const uint64_t base=ignore_range?0:offset;
 if(!sink.begin(base))return -1;
 uint64_t at=base;bool ok=true,stopped=false;
 while(at<media.size()){
  if(stop_after&&at>=stop_after){stop_after=0;ok=false;break;}
  size_t chunk=std::min<uint64_t>(256*1024,media.size()-at);
  if(!sink.write(media.data()+at,chunk)){ok=false;break;}
  at+=chunk;if(!progress(at,media.size())){ok=false;stopped=true;break;}
  usleep(20000);
 }
 const bool whole=ok&&at==media.size();
 const bool closed=sink.finish(whole);
 (void)stopped;return whole&&closed?200:-1;
}
int http_context_id(){return 0;}int http_ssl_context_id(){return 0;}
std::string http_failure_detail(){return "The test's connection dropped.";}
static const char* movie_json="{\"Name\":\"Test Film\",\"Id\":\"00112233445566778899aabbccddeeff\",\"Type\":\"Movie\",\"ProductionYear\":2026,\"Container\":\"mkv\",\"RunTimeTicks\":600000000,\"MediaSources\":[{\"Size\":0}]}";
int http_request(int,const char*,const char*,const char*,char* out,int cap){snprintf(out,cap,"%s",movie_json);return 200;}
int http_fetch_binary(const char*,const char*,uint8_t*,int){return -1;}

// ---- the checks ---------------------------------------------------------------
static Item film(){Item i;snprintf(i.base.id,sizeof i.base.id,"00112233445566778899aabbccddeeff");snprintf(i.base.type,sizeof i.base.type,"Movie");snprintf(i.base.name,sizeof i.base.name,"Test Film");i.year=2026;return i;}
static bool finished_more(int before,int seconds=60){for(int i=0;i<seconds*20;++i){const auto s=download_status();if(s.finished>before&&!s.active)return true;usleep(50000);}return false;}
static bool settled(int seconds=60){for(int i=0;i<seconds*20;++i){const auto s=download_status();if(!s.active&&!s.queued)return true;usleep(50000);}return false;}
static void clear_drive(const std::string& root){for(const auto& e:downloads_list())download_delete(e);(void)root;}

int main(int argc,char** argv){
 assert(argc==7);setenv("JF_USB_FTP",argv[1],1);const std::string root=argv[2],log=argv[3];media=read_all(argv[4]);assert(!media.empty());const std::string mov=argv[5];const pid_t server=atoi(argv[6]);
 settings.debug_log=true;assert(storage_init(115));snprintf(g_server,sizeof g_server,"http://test");snprintf(g_token,sizeof g_token,"token");
 downloads_startup();settings.download_usb=true;
 const std::string dir=root+"/JellyFin",name="Test Film (2026) [00112233].mkv",file=dir+"/"+name;
 CHECK(usb_drives().size()==1&&usb_status().empty(),"a drive reached through FTP: downloads will go to it, no warning shown");

 int before=download_status().finished;
 stop_after=media.size()/3;
 assert(download_item(film()));
 // Interrupted part way: the worker waits five seconds and carries on from there.
 CHECK(finished_more(before),"download finishes, after a dropped connection");
 CHECK(http_calls==2&&last_offset>=media.size()/3&&last_offset<media.size(),"the retry resumed at %llu, the .part's size on the drive",(unsigned long long)last_offset);
 CHECK(read_all(file)==media,"the file on the drive is byte-identical to the original (%zu bytes)",media.size());
 CHECK(!exists(file+".part")&&exists(file+".json"),"no .part left, its description beside it");
 const std::string commands=read_all(log);
 CHECK(commands.find("APPE ")!=std::string::npos&&commands.find("TRUNCATED")==std::string::npos,"resumed with APPE; never REST with STOR, which empties the file on this server");
 CHECK(download_state("00112233445566778899aabbccddeeff")==DLS_DONE,"the item shows as downloaded");

 auto entries=downloads_list();
 CHECK(entries.size()==1&&entries[0].usb&&entries[0].bytes==media.size()&&local_is_drive_url(entries[0].item.local),"Downloads lists it, marked USB, right size, drive URL");
 assert(!entries.empty());
 const auto calls=read_all(log).size();downloads_list();downloads_list();
 CHECK(read_all(log).size()==calls,"asking again does not go back to the server");

 const Item entry=entries[0].item;
 setenv("JF_TEST_AO","null",1);player_prefer_audio(0);assert(player_open(entry.local));bool audio=false,video=false;
 for(int i=0;i<15000&&!(audio&&video&&player_time()>1000);++i){if(player_draw())video=true;if(player_has_audio())audio=true;if(!player_error().empty())break;usleep(1000);}
 CHECK(audio&&video,"the download plays from the drive: picture and sound");
 player_seek(30000);bool sought=false;for(int i=0;i<15000&&!sought;++i){player_draw();const auto t=player_time();sought=t>=29000&&t<45000;usleep(1000);}
 CHECK(sought,"and seeks to 30 s");player_close();

 Item played=entry;played.duration=3600000;download_played(played,600000);
 entries=downloads_list();CHECK(exists(file+".resume")&&!entries.empty()&&entries[0].item.resume==600000,"its resume point is kept on the drive, and read back");
 download_played(played,10000);CHECK(!exists(file+".resume"),"and cleared when stopped near the start");
 Item browsed=entry;snprintf(browsed.base.id,sizeof browsed.base.id,"local:0001");browsed.duration=3600000;download_played(browsed,600000);
 CHECK(!exists(file+".resume"),"a file merely browsed to has nothing written beside it");

 before=download_status().finished;assert(!entries.empty());CHECK(download_delete(entries[0])&&!exists(file)&&!exists(file+".json"),"deleting takes the file and its description");
 CHECK(downloads_list().empty(),"and Downloads no longer lists it");

 // The server ignores the range when resuming: the .part is started again, not added to.
 before=download_status().finished;stop_after=media.size()/4;ignore_range=true;assert(download_item(film()));
 CHECK(finished_more(before)&&read_all(file)==media,"a server that ignores the range: started over, byte-identical");
 ignore_range=false;clear_drive(root);

 // Stopped by the viewer: the part goes.
 download_pause(false);before=download_status().finished;assert(download_item(film()));
 for(int i=0;i<200&&!exists(file+".part");++i)usleep(10000);usleep(200000);
 download_pause(true);{bool idle=false;for(int i=0;i<400&&!idle;++i){idle=!download_status().active;usleep(50000);}CHECK(idle,"pausing stops the upload");}const uint64_t kept=ftp_file_size("/mnt/usb0/JellyFin/"+name+".part");CHECK(kept>0,"keeping %llu bytes",(unsigned long long)kept);
 download_pause(false);CHECK(finished_more(before)&&read_all(file)==media,"resuming finishes it, byte-identical");clear_drive(root);
 before=download_status().finished;assert(download_item(film()));
 for(int i=0;i<300&&!exists(file+".part");++i)usleep(10000);usleep(100000);download_cancel();
 CHECK(settled(),"cancelling stops the upload");usleep(200000);CHECK(!exists(file+".part")&&!exists(file),"and removes the part");

 // A drive that fills up: it says so and stops, with nothing listed.
 {std::ofstream quota(root+"/.quota");quota<<media.size()/2;}
 assert(download_item(film()));CHECK(settled(90),"a full drive ends the download");
 CHECK(!download_status().error.empty()&&!exists(file),"with an error and no finished file: %s",download_status().error.c_str());
 remove((root+"/.quota").c_str());remove((file+".part").c_str());

 // A .mov with its index at the end, the case FFmpeg's own ftp:// cannot play.
 const std::string movie_url=local_file_url("/mnt/usb0/"+mov);
 assert(player_open(movie_url));audio=video=false;
 for(int i=0;i<30000&&!(audio&&video&&player_time()>1500);++i){if(player_draw())video=true;if(player_has_audio())audio=true;if(!player_error().empty())break;usleep(1000);}
 CHECK(audio&&video&&player_error().empty(),"%s: picture and sound through the reader, at %.1f s",mov.c_str(),player_time()/1000.0);
 player_close();

 // FTP off in GoldHEN's settings: a warning, and downloads go to the console.
 kill(server,SIGTERM);for(int i=0;i<130&&!usb_drives().empty();++i)usleep(50000);
 CHECK(usb_drives().empty()&&!usb_status().empty(),"the server gone: no drive, and a warning (%s)",usb_status().c_str());
 before=download_status().finished;assert(download_item(film()));
 CHECK(finished_more(before),"the download still finishes");
 CHECK(exists(data_dir+"/downloads/"+name),"on the console");
 CHECK(http_calls>0&&read_all(data_dir+"/downloads/"+name)==media,"byte-identical");
 clear_drive(root);
 downloads_stop();
 printf(failures?"FAILURES: %d\n":"ALL PASSED\n",failures);return failures?1:0;
}
