// Downloads: see downloads.h. Files are written as <name>.part and renamed
// when complete, so a half-downloaded file is never listed or played; the
// item's JSON is written beside it last. A paused, stopped or interrupted
// download keeps its .part and carries on from it (an HTTP Range request);
// the queue is kept in the user's folder, so it carries on after a restart.
#include "downloads.h"
#include "sandbox_ps4.h"
#include "thread_priority.h"
#include "json_unescape.h"
#include "language.h"
#include "mono_clock.h"
#include "local_media.h"
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <deque>
#include <set>
#ifdef HOST_TEST
#include <sys/statvfs.h>
#endif

#ifdef HOST_TEST
// The host has no sandbox to leave (sandbox_ps4.cpp is console-only).
namespace sandbox { bool leave(){return false;} void restore(){} bool lifted(){return false;} }
#else
extern "C" int orbis_syscall(int num, ...);  // vendor/goldhen_sdk/source/Syscall.c
#endif
namespace {
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
std::deque<Item> queue;
DownloadStatus status;
std::string current_id;
std::set<std::string> done_ids;bool done_known=false;
// cancel: the viewer stopped this one (its .part goes); pausing and stopping keep it.
std::atomic<bool> cancel{false},stopping{false},paused{false};
bool started=false;pthread_t worker;
std::string usb_problem="Downloads are saved to the console.";

std::string console_dir(){
#ifdef HOST_TEST
 return data_dir+"/downloads";
#else
 return "/data/JellyFin-PS4/downloads";
#endif
}
// The first USB slot with a drive in it (a mounted slot lists something).
std::string first_usb(){
 for(int i=0;i<8;++i){const std::string slot="/mnt/usb"+std::to_string(i);DIR* d=opendir(slot.c_str());if(!d)continue;
  bool any=false;while(auto e=readdir(d))if(strcmp(e->d_name,".")&&strcmp(e->d_name,"..")){any=true;break;}closedir(d);if(any)return slot;}
 return "";
}
std::vector<std::string> folders(){
 std::vector<std::string> out{console_dir()};
 if(sandbox::lifted())for(int i=0;i<8;++i)out.push_back("/mnt/usb"+std::to_string(i)+"/JellyFin");
 return out;
}
// Where the next file goes: the USB drive when chosen and reachable.
std::string target_dir(bool& usb){
 usb=false;
 if(settings.download_usb&&sandbox::lifted()){const std::string drive=first_usb();if(!drive.empty()){usb=true;return drive+"/JellyFin";}}
 return console_dir();
}
// Free space where `dir` lives, and its file system ("msdosfs" is FAT32).
// False when it cannot be told (the download then goes ahead).
bool free_space(const std::string& dir,uint64_t& bytes,std::string& fs){
#ifdef HOST_TEST
 struct statvfs s;if(statvfs(dir.c_str(),&s)!=0)return false;bytes=(uint64_t)s.f_bavail*s.f_frsize;fs="host";return true;
#else
 // FreeBSD's statfs (syscall 396): f_bsize at 16, f_bavail at 48, the type
 // name at 280. Its version stamp is checked, so a different layout is
 // never read as numbers.
 alignas(8) unsigned char buf[1024]={};
 if(orbis_syscall(396,dir.c_str(),buf)!=0)return false;
 uint32_t version;memcpy(&version,buf,4);if(version!=0x20030518)return false;
 uint64_t bsize;int64_t avail;memcpy(&bsize,buf+16,8);memcpy(&avail,buf+48,8);
 if(!bsize||avail<0)return false;
 bytes=bsize*(uint64_t)avail;char name[17]={};memcpy(name,buf+280,16);fs=name;return true;
#endif
}
// A file name the console's and FAT / exFAT drives both accept.
std::string safe_name(std::string s){
 for(char& c:s)if(strchr("\\/:*?\"<>|",c)||(unsigned char)c<32)c='_';
 while(!s.empty()&&(s.back()==' '||s.back()=='.'))s.pop_back();
 if(s.size()>120)s.resize(120);
 return s.empty()?"Untitled":s;
}
std::string file_name(const Item& item,const std::string& json){
 std::string name=item.base.name;
 if(!strcmp(item.base.type,"Episode")){char code[24]="";if(item.episode)snprintf(code,sizeof code," S%02dE%02d",item.season,item.episode);name=(item.series.empty()?std::string():item.series)+code+" - "+name;}
 else if(!strcmp(item.base.type,"Audio")&&!item.artist.empty())name=item.artist+" - "+name;
 else if(item.year)name+=" ("+std::to_string(item.year)+")";
 // The extension from the item's container ("mkv", "mov,mp4,m4a" -> "mp4"); the player probes anyway.
 std::string ext;const char* end=json.c_str()+json.size();const char* c=find_top_value(json.c_str(),end,"Container");
 if(c&&*c=='"'){char b[64]={};json_unescape(c+1,end,b,sizeof b);ext=b;ext=ext.find("mp4")!=std::string::npos?"mp4":ext.substr(0,ext.find(','));}
 if(ext.empty()||ext.size()>5)ext=!strcmp(item.base.type,"Audio")?"mp3":"mkv";
 return safe_name(name)+" ["+std::string(item.base.id).substr(0,8)+"]."+ext;
}
// The file's size as the server lists it (its first media source), 0 unknown.
uint64_t listed_size(const std::string& json){
 const auto at=json.find("\"Size\":");if(at==std::string::npos)return 0;
 return strtoull(json.c_str()+at+7,nullptr,10);
}
bool write_text(const std::string& path,const std::string& text){FILE* f=fopen(path.c_str(),"wb");if(!f)return false;const bool ok=fwrite(text.data(),1,text.size(),f)==text.size();return fclose(f)==0&&ok;}
std::string read_text(const std::string& path){std::string out;FILE* f=fopen(path.c_str(),"rb");if(!f)return out;char buf[8192];size_t n;while((n=fread(buf,1,sizeof buf,f))>0)out.append(buf,n);fclose(f);return out;}
// stat() sizes are wrong on the console (local_media.h): seek to the end instead.
uint64_t file_size(const std::string& path){return local_file_size(path);}
std::string gb(uint64_t bytes){char b[24];snprintf(b,sizeof b,"%.1f GB",bytes/1e9);return b;}

// The queue, kept in the user's folder: one item a line, its fields by tabs.
std::string queue_path(){return std::string(jf_data_path("download-queue.txt"));}
void save_queue_locked(){
 std::string text=paused?"#paused\n":"";
 auto add=[&](const Item& i){std::string name=i.base.name,series=i.series;for(auto* s:{&name,&series})for(char& c:*s)if(c=='\t'||c=='\n')c=' ';
  text+=std::string(i.base.id)+"\t"+i.base.type+"\t"+name+"\t"+series+"\t"+std::to_string(i.season)+"\t"+std::to_string(i.episode)+"\t"+std::to_string(i.year)+"\n";};
 for(const auto& i:queue)add(i);
 write_text(queue_path(),text);
}
void load_queue(){
 const std::string text=read_text(queue_path());size_t at=0;
 while(at<text.size()){size_t end=text.find('\n',at);if(end==std::string::npos)end=text.size();const std::string line=text.substr(at,end-at);at=end+1;
  if(line=="#paused"){paused=true;continue;}
  std::vector<std::string> f;size_t s=0;for(;;){const size_t t=line.find('\t',s);f.push_back(line.substr(s,t==std::string::npos?std::string::npos:t-s));if(t==std::string::npos)break;s=t+1;}
  if(f.size()<7||f[0].empty())continue;
  Item i;snprintf(i.base.id,sizeof i.base.id,"%s",f[0].c_str());snprintf(i.base.type,sizeof i.base.type,"%s",f[1].c_str());snprintf(i.base.name,sizeof i.base.name,"%s",f[2].c_str());
  i.series=f[3];i.season=atoi(f[4].c_str());i.episode=atoi(f[5].c_str());i.year=atoi(f[6].c_str());queue.push_back(i);}
 status.queued=(int)queue.size();status.paused=paused;
}
void fail(const Item& item,const std::string& why){pthread_mutex_lock(&lock);status.error=std::string(item.base.name)+": "+why;pthread_mutex_unlock(&lock);plog(("Download: failed, "+why).c_str());}

// One file. Returns true when it is finished with (done, failed or cancelled);
// false when it was paused or the app is stopping, so it goes back in the queue.
bool run(const Item& item){
 bool usb=false;const std::string dir=target_dir(usb);
 mkdir(dir.c_str(),0700);
 const std::string json=item_json(item.base.id);
 if(json.empty()){fail(item,T("Could not reach the server for"));return true;}
 const std::string path=dir+"/"+file_name(item,json),part=path+".part",token=encode(g_token);
 // Room for it (5 GB is kept free on the console, as the PS3 keeps 10 GB of
 // its drive), and FAT32's 4 GB limit on a file.
 const uint64_t size=listed_size(json),have=file_size(part);uint64_t free_bytes=0;std::string fs;
 if(size&&free_space(dir,free_bytes,fs)){
  const uint64_t keep=usb?256ULL<<20:5000000000ULL,need=size>have?size-have:0;
  if(usb&&fs=="msdosfs"&&size>4294967295ULL){fail(item,T("Too big for this FAT32 drive (4 GB a file)."));return true;}
  if(free_bytes<need+keep){fail(item,TF("Not enough space: needs %s, %s free.",gb(need+keep).c_str(),gb(free_bytes).c_str()));return true;}
 }
 plog(("Download: "+std::string(item.base.name)+(usb?" to USB":" to the console")+(have?" (resuming)":"")).c_str());
 auto progress=[&](uint64_t got,uint64_t total){pthread_mutex_lock(&lock);status.got=got;status.total=total?total:size;pthread_mutex_unlock(&lock);return !cancel&&!stopping&&!paused;};
 // The original file. A user the server does not let download gets the same
 // file through the stream endpoint, as direct play does. A dropped
 // connection is tried again from where it stopped, three times.
 const std::string download=std::string(g_server)+"/Items/"+encode(item.base.id)+"/Download?ApiKey="+token;
 const std::string stream=std::string(g_server)+(!strcmp(item.base.type,"Audio")?"/Audio/":"/Videos/")+encode(item.base.id)+"/stream?Static=true&ApiKey="+token;
 std::string url=download;int st=-1;
 for(int attempt=0;attempt<4;++attempt){
  st=http_download_resume(url.c_str(),part.c_str(),file_size(part),progress);
  if(st==200||cancel||stopping||paused)break;
  if(st==401||st==403){if(url==download){url=stream;remove(part.c_str());continue;}break;}
  if(st>0&&st!=416&&st<500)break;
  if(st==416)remove(part.c_str());
  plog("Download: interrupted; trying again from where it stopped");
  for(int t=0;t<50&&!cancel&&!stopping&&!paused;++t)usleep(100000);
 }
 if(paused||stopping){plog(paused?"Download: paused":"Download: stopped; it carries on next time");return false;}
 if(cancel){remove(part.c_str());plog("Download: cancelled");return true;}
 if(st!=200){fail(item,st>0?"HTTP "+std::to_string(st):http_failure_detail());return true;}
 if(rename(part.c_str(),path.c_str())!=0||!write_text(path+".json",json)){remove(part.c_str());fail(item,T("Could not save"));return true;}
 pthread_mutex_lock(&lock);done_ids.insert(item.base.id);++status.finished;pthread_mutex_unlock(&lock);
 plog("Download: complete");return true;
}
void* loop(void*){
 background_priority();
 for(;;){
  pthread_mutex_lock(&lock);
  while((queue.empty()||paused)&&!stopping)pthread_cond_wait(&wake,&lock);
  if(stopping){pthread_mutex_unlock(&lock);return nullptr;}
  const Item item=queue.front();current_id=item.base.id;
  status.active=true;status.name=item.base.name;status.got=status.total=0;status.queued=(int)queue.size()-1;cancel=false;
  pthread_mutex_unlock(&lock);
  const bool finished=run(item);
  pthread_mutex_lock(&lock);
  // Finished with: off the queue. Paused or stopping: it stays first, its .part kept.
  if(finished&&!queue.empty()&&!strcmp(queue.front().base.id,item.base.id))queue.pop_front();
  status.active=false;current_id.clear();status.queued=(int)queue.size();save_queue_locked();
  pthread_mutex_unlock(&lock);
 }
}
void start_worker_locked(){if(!started)started=pthread_create(&worker,nullptr,loop,nullptr)==0;pthread_cond_signal(&wake);}
void scan_done(){
 std::set<std::string> ids;for(const auto& e:downloads_list())ids.insert(e.item.base.id);
 pthread_mutex_lock(&lock);done_ids.insert(ids.begin(),ids.end());done_known=true;pthread_mutex_unlock(&lock);
}
}  // namespace

void downloads_startup(){
#ifdef HOST_TEST
 usb_problem="USB drives are not available on this host.";
#else
 usb_problem=sandbox::leave()?"":"GoldHEN 2.3 or later is needed to save to USB drives. Downloads are saved to the console.";
#endif
 pthread_mutex_lock(&lock);load_queue();pthread_mutex_unlock(&lock);
 if(!queue.empty())plog(("Download: "+std::to_string(queue.size())+" waiting from last time").c_str());
}
void downloads_resume_queue(){
 pthread_mutex_lock(&lock);if(!queue.empty())start_worker_locked();pthread_mutex_unlock(&lock);
}
std::string usb_status(){
 if(!settings.download_usb)return "";
 if(!usb_problem.empty())return usb_problem;
 if(!sandbox::lifted())return "Restart the app to save downloads to a USB drive.";
 return first_usb().empty()?"No USB drive found. Downloads are saved to the console.":"";
}
bool download_item(const Item& item){
 std::vector<Item> targets;
 if(!download_targets(item,targets)){last_error=T("Could not list what to download.");return false;}
 if(!done_known)scan_done();
 int added=0;
 pthread_mutex_lock(&lock);
 for(auto& t:targets){
  if(strcmp(t.base.type,"Movie")&&strcmp(t.base.type,"Episode")&&strcmp(t.base.type,"Audio")&&strcmp(t.base.type,"Video")&&strcmp(t.base.type,"MusicVideo"))continue;
  if(done_ids.count(t.base.id)||current_id==t.base.id)continue;
  bool waiting=false;for(const auto& q:queue)if(!strcmp(q.base.id,t.base.id))waiting=true;if(waiting)continue;
  if(t.series.empty())t.series=item.series.empty()&&!strcmp(item.base.type,"Series")?std::string(item.base.name):item.series;
  queue.push_back(t);++added;
 }
 status.error.clear();status.queued=(int)queue.size()-(status.active?1:0);save_queue_locked();
 start_worker_locked();pthread_mutex_unlock(&lock);
 if(!added){last_error=T("Already downloaded or downloading.");return false;}
 plog(("Download: "+std::to_string(added)+" queued").c_str());
 return started;
}
int download_state(const std::string& id,float* progress){
 if(!done_known)scan_done();
 pthread_mutex_lock(&lock);int state=DLS_NONE;
 if(current_id==id){state=DLS_ACTIVE;if(progress)*progress=status.total?(float)std::min(1.0,(double)status.got/status.total):-1.0f;}
 else if(done_ids.count(id))state=DLS_DONE;
 else for(const auto& q:queue)if(id==q.base.id)state=DLS_WAITING;
 pthread_mutex_unlock(&lock);return state;
}
bool download_queued_or_done(const std::string& id){return download_state(id)!=DLS_NONE;}
DownloadStatus download_status(){pthread_mutex_lock(&lock);DownloadStatus s=status;s.paused=paused;pthread_mutex_unlock(&lock);return s;}
void download_pause(bool on){
 pthread_mutex_lock(&lock);paused=on;status.paused=on;save_queue_locked();if(!on)start_worker_locked();pthread_mutex_unlock(&lock);
 plog(on?"Download: pausing":"Download: resuming");
}
void download_cancel(){pthread_mutex_lock(&lock);
 // The one downloading goes (its .part removed); the rest are dropped.
 const std::string now=current_id;queue.clear();if(!now.empty()){Item keep;snprintf(keep.base.id,sizeof keep.base.id,"%s",now.c_str());queue.push_back(keep);}
 status.queued=0;cancel=true;paused=false;status.paused=false;save_queue_locked();pthread_cond_signal(&wake);pthread_mutex_unlock(&lock);plog("Download: cancelled");}
std::vector<DownloadEntry> downloads_list(){
 std::vector<DownloadEntry> out;
 for(const auto& dir:folders()){DIR* d=opendir(dir.c_str());if(!d)continue;
  while(auto e=readdir(d)){const std::string name=e->d_name;if(name.size()<6||name.compare(name.size()-5,5,".json"))continue;
   const std::string path=dir+"/"+name.substr(0,name.size()-5);const uint64_t bytes=local_file_size(path);if(!bytes)continue;
   const std::string json=read_text(dir+"/"+name);
   DownloadEntry entry;if(!parse_preview_item(json.c_str(),entry.item))continue;
   entry.path=path;entry.item.local=path;entry.bytes=bytes;entry.usb=dir.rfind("/mnt/usb",0)==0;
   // Where it was last stopped, here (download_played); else as the server had it.
   const std::string resume=read_text(path+".resume");if(!resume.empty())entry.item.resume=strtoull(resume.c_str(),nullptr,10);
   out.push_back(entry);}
  closedir(d);}
 std::sort(out.begin(),out.end(),[](const DownloadEntry& a,const DownloadEntry& b){
  const std::string ka=a.item.series.empty()?a.item.base.name:a.item.series,kb=b.item.series.empty()?b.item.base.name:b.item.series;
  if(ka!=kb)return ka<kb;if(a.item.season!=b.item.season)return a.item.season<b.item.season;return a.item.episode<b.item.episode;});
 return out;
}
void download_played(const Item& item,uint64_t position_ms){
 if(item.local.empty())return;
 // Near the end counts as watched: no resume point.
 const bool done=item.duration&&position_ms+120000>item.duration;
 if(done||position_ms<30000)remove((item.local+".resume").c_str());
 else write_text(item.local+".resume",std::to_string(position_ms));
}
bool download_delete(const DownloadEntry& entry){
 const bool ok=remove(entry.path.c_str())==0;remove((entry.path+".json").c_str());remove((entry.path+".resume").c_str());
 pthread_mutex_lock(&lock);done_ids.erase(entry.item.base.id);pthread_mutex_unlock(&lock);
 plog(ok?"Download: deleted":"Download: delete failed");return ok;
}
void downloads_stop(){
 if(started){pthread_mutex_lock(&lock);stopping=true;pthread_cond_broadcast(&wake);pthread_mutex_unlock(&lock);pthread_join(worker,nullptr);started=false;}
 sandbox::restore();
}
