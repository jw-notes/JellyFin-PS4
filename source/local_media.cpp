// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// Media on USB drives: see local_media.h.
#include "local_media.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <atomic>
#include <ctype.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <new>
#ifndef HOST_TEST
#include "sandbox_ps4.h"
#endif

namespace {
std::string usb_root(){
#ifdef HOST_TEST
 const char* root=getenv("JF_USB_ROOT");return root?root:"/tmp/jf-usb";
#else
 return "/mnt";
#endif
}
std::string lower_ext(const std::string& name){const auto dot=name.rfind('.');if(dot==std::string::npos)return {};std::string e=name.substr(dot+1);for(char& c:e)c=(char)tolower((unsigned char)c);return e;}
LocalKind kind_of(const std::string& name){
 static const char* video[]={"mkv","mp4","m4v","mov","avi","ts","m2ts","mts","webm","wmv","mpg","mpeg","flv","3gp"};
 static const char* audio[]={"mp3","flac","m4a","aac","ogg","oga","opus","wav","wma","alac","aiff","aif","ac3","eac3","ec3","mka"};
 const std::string e=lower_ext(name);
 for(auto v:video)if(e==v)return LOCAL_VIDEO;
 for(auto a:audio)if(e==a)return LOCAL_AUDIO;
 return LOCAL_OTHER;
}
// Natural order: "Track 2" before "Track 10".
bool natural_less(const std::string& a,const std::string& b){
 size_t i=0,j=0;
 while(i<a.size()&&j<b.size()){
  if(isdigit((unsigned char)a[i])&&isdigit((unsigned char)b[j])){
   size_t i2=i,j2=j;while(i2<a.size()&&isdigit((unsigned char)a[i2]))++i2;while(j2<b.size()&&isdigit((unsigned char)b[j2]))++j2;
   const std::string na=a.substr(i,i2-i),nb=b.substr(j,j2-j);const auto sa=na.find_first_not_of('0'),sb=nb.find_first_not_of('0');
   const std::string ta=sa==std::string::npos?"":na.substr(sa),tb=sb==std::string::npos?"":nb.substr(sb);
   if(ta.size()!=tb.size())return ta.size()<tb.size();if(ta!=tb)return ta<tb;i=i2;j=j2;continue;}
  const int ca=tolower((unsigned char)a[i]),cb=tolower((unsigned char)b[j]);if(ca!=cb)return ca<cb;++i;++j;}
 return a.size()-i<b.size()-j;
}

// ---- through GoldHEN's FTP server ----------------------------------------------
// Inside the sandbox /mnt/usb0..7 cannot be opened. When the escape at launch
// failed, the drives are still there for GoldHEN's own FTP server, which runs
// outside it: folders are listed from it here, files are played from it
// through this file's reader, and downloads are written to it. It answers only
// while FTP is on in GoldHEN's settings.
//
// Measured on GoldHEN's server v2.2: ABOR and EPSV answer 502; REST with RETR
// reads from anywhere; closing a transfer's data connection gets 426; APPE
// appends. REST with STOR does NOT write at the offset: it empties the file.
#ifdef HOST_TEST
std::string ftp_address(){const char* a=getenv("JF_USB_FTP");return a?a:"";}
bool ftp_mode(){return !ftp_address().empty();}
#else
std::string ftp_address(){return "127.0.0.1:2121";}
bool ftp_mode(){return !sandbox::lifted();}
#endif
int connect_to(const std::string& host,const std::string& port){
 struct addrinfo hints{},*found=nullptr;hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;hints.ai_flags=AI_NUMERICHOST;
 if(getaddrinfo(host.c_str(),port.c_str(),&hints,&found)!=0||!found)return -1;
 const int fd=socket(found->ai_family,found->ai_socktype,found->ai_protocol);
 if(fd>=0&&connect(fd,found->ai_addr,found->ai_addrlen)!=0){close(fd);freeaddrinfo(found);return -1;}
 freeaddrinfo(found);
 // No SIGPIPE from this socket. The toolchain's headers are Linux's and leave
 // the option out; on the console's FreeBSD kernel it is 0x800.
#if defined(SO_NOSIGPIPE)
 if(fd>=0){const int on=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&on,sizeof on);}
#elif !defined(HOST_TEST)
 if(fd>=0){const int on=1;setsockopt(fd,SOL_SOCKET,0x800,&on,sizeof on);}
#endif
 return fd;
}
// On the console the toolchain's MSG_NOSIGNAL (Linux's 0x4000) means something
// else to the kernel, so it is not passed there; the socket option and the
// app ignoring SIGPIPE (crash.cpp) stand in for it.
#if !defined(MSG_NOSIGNAL) || !defined(HOST_TEST)
#undef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
// Up to 3 s of silence ends a read, unless told otherwise.
int read_some(int fd,char* into,int room,int wait_ms=3000){struct pollfd wait{};wait.fd=fd;wait.events=POLLIN;if(poll(&wait,1,wait_ms)<=0)return -1;return (int)recv(fd,into,room,0);}
struct FtpRow{std::string name;bool folder=false;uint64_t bytes=0;};
class Ftp{
public:
 ~Ftp(){shut();}
 void shut(){if(control>=0)close(control);control=-1;pending.clear();}
 bool connected()const{return control>=0;}
 bool binary(){return command("TYPE I")/100==2;}
 // A file's size. 0: missing, or it could not be told.
 uint64_t size(const std::string& path){return command("SIZE "+path)==213&&last.size()>4?strtoull(last.c_str()+4,nullptr,10):0;}
 // The data connection for the next transfer, -1 when there is none. The server
 // names its address on the network; on the console the loopback reaches it too.
 int passive(){
  if(command("PASV")!=227)return -1;
  const auto open_at=last.find('(');int a,b,c,d,high,low;if(open_at==std::string::npos||sscanf(last.c_str()+open_at+1,"%d,%d,%d,%d,%d,%d",&a,&b,&c,&d,&high,&low)!=6)return -1;
  const std::string port=std::to_string(high*256+low);
  const int data=connect_to(host,port);return data>=0?data:connect_to(std::to_string(a)+"."+std::to_string(b)+"."+std::to_string(c)+"."+std::to_string(d),port);
 }
 bool open(){
  const std::string address=ftp_address();const auto colon=address.find(':');if(colon==std::string::npos)return false;
  host=address.substr(0,colon);control=connect_to(host,address.substr(colon+1));if(control<0)return false;
  if(reply()/100!=2)return false;
  const int user=command("USER anonymous");if(user==331)return command("PASS anonymous")/100==2;return user/100==2;
 }
 // A folder's rows. False when it cannot be listed.
 bool list(const std::string& dir,std::vector<FtpRow>& rows){
  rows.clear();if(command("CWD "+dir)/100!=2)return false;
  const int data=passive();if(data<0)return false;
  const int started=command("LIST");if(started/100!=1&&started/100!=2){close(data);return false;}
  std::string text;std::vector<char> buffer(4096);int n;while((n=read_some(data,buffer.data(),buffer.size()))>0)text.append(buffer.data(),n);close(data);
  if(started/100==1&&reply()/100!=2)return false;
  size_t at=0;while(at<text.size()){auto end=text.find('\n',at);if(end==std::string::npos)end=text.size();std::string line=text.substr(at,end-at);at=end+1;if(!line.empty()&&line.back()=='\r')line.pop_back();
   // "-rwxrwxrwx 1 -1 -1 14384367 Nov  9 2026 AAC-5.1.mkv": the name, spaces and all, is what follows the eighth field.
   size_t pos=0;std::string size;bool whole=true;for(int field=0;field<8;++field){while(pos<line.size()&&line[pos]==' ')++pos;const auto stop=line.find(' ',pos);if(stop==std::string::npos){whole=false;break;}if(field==4)size=line.substr(pos,stop-pos);pos=stop;}
   if(!whole)continue;while(pos<line.size()&&line[pos]==' ')++pos;
   FtpRow row;row.name=line.substr(pos);if(row.name.empty()||row.name=="."||row.name=="..")continue;row.folder=line[0]=='d';row.bytes=strtoull(size.c_str(),nullptr,10);rows.push_back(row);}
  return true;
 }
 int reply(int wait_ms=3000){
  // A reply ends with the line that has its code and a space ("211-" goes on).
  for(;;){auto end=pending.find('\n');
   while(end==std::string::npos){char buffer[1024];const int n=control<0?-1:read_some(control,buffer,sizeof buffer,wait_ms);if(n<=0)return -1;pending.append(buffer,n);end=pending.find('\n');}
   last=pending.substr(0,end);pending.erase(0,end+1);
   if(last.size()>=4&&isdigit((unsigned char)last[0])&&isdigit((unsigned char)last[1])&&isdigit((unsigned char)last[2])&&last[3]==' ')return atoi(last.c_str());}
 }
 int command(const std::string& text){const std::string line=text+"\r\n";if(control<0||send(control,line.data(),line.size(),MSG_NOSIGNAL)!=(ssize_t)line.size())return -1;return reply();}
private:
 int control=-1;std::string host,pending,last;
};
// Which slots have a drive, kept by a thread of its own: the UI asks every two
// seconds, and must not wait on a network conversation to draw its next frame.
pthread_mutex_t ftp_lock=PTHREAD_MUTEX_INITIALIZER;
std::vector<std::string> ftp_drives;
std::atomic<int> ftp_holds{0};
std::string ftp_sweep_note;
void ftp_sweep(){
 std::vector<std::string> found;Ftp ftp;const bool open=ftp.open();
 if(open)for(int i=0;i<8;++i){const std::string slot="/mnt/usb"+std::to_string(i);std::vector<FtpRow> rows;if(ftp.list(slot,rows)&&!rows.empty())found.push_back(slot);}
 const std::string note=(open?"USB: ":"USB: GoldHEN's FTP server does not answer at ")+(open?std::to_string(found.size())+" drive(s) through GoldHEN's FTP server at ":std::string())+ftp_address()+(open?"":"; turn FTP on in GoldHEN's settings to see USB drives");
 pthread_mutex_lock(&ftp_lock);ftp_drives=found;const bool changed=note!=ftp_sweep_note;ftp_sweep_note=note;pthread_mutex_unlock(&ftp_lock);
 if(changed)plog(note.c_str());
}
void* ftp_watch(void*){for(;;){sleep(5);if(ftp_holds.load()==0)ftp_sweep();}return nullptr;}
void ftp_start(){static bool started=false;if(started)return;started=true;ftp_sweep();pthread_t thread;if(pthread_create(&thread,nullptr,ftp_watch,nullptr)==0)pthread_detach(thread);}
std::string url_path(const std::string& path){std::string out;char hex[4];for(unsigned char c:path){if(isalnum(c)||c=='/'||c=='-'||c=='_'||c=='.'||c=='~')out+=(char)c;else{snprintf(hex,sizeof hex,"%%%02X",c);out+=hex;}}return out;}

// ---- the player's reader --------------------------------------------------------
// It runs on MPV's and FFmpeg's threads, whose stacks are small on the console:
// a 64 KB buffer kept on the stack here crashed the app on its first seek (and
// does on the host with a 64 KB stack). Buffers live on the heap.
// FFmpeg's own ftp:// gives up going back after it has reached a file's end, so a
// QuickTime or MP4 file with its index at the end loads and then plays nothing.
// This is all the FTP a player needs: a read that picks up where it is, and a seek.
struct FtpReader{
 Ftp ftp;std::string path;int data=-1;uint64_t at=0,total=0;std::atomic<bool> cancelled{false};std::vector<char> scrap;
 ~FtpReader(){if(data>=0)close(data);}
 bool connect(){ftp.shut();if(!ftp.open()||!ftp.binary())return false;const uint64_t size=ftp.size(path);if(size)total=size;return size!=0;}
 bool begin(){
  if(!ftp.connected()&&!connect()){plog("USB reader: could not reconnect to GoldHEN's FTP server");return false;}
  const int fd=ftp.passive();if(fd<0){plog("USB reader: no data connection");return false;}
  if(ftp.command("REST "+std::to_string(at))!=350||ftp.command("RETR "+path)/100!=1){close(fd);plog(("USB reader: could not read from "+std::to_string(at)).c_str());return false;}
  return (data=fd)>=0;
 }
 // Ends a transfer early. Without its reply the conversation is out of step,
 // so the connection goes too; the next read makes a new one.
 void stop(){if(data<0)return;close(data);data=-1;if(ftp.reply(2000)<0)ftp.shut();}
 int64_t read(char* into,uint64_t room){
  if(room>(1u<<20))room=1u<<20;
  for(int attempt=0;attempt<3;++attempt){
   if(cancelled)return -1;
   if(at>=total)return 0;
   if(data<0&&!begin()){ftp.shut();continue;}
   // Silent for 15 s is a failure; a cancel is noticed within a tenth of a second.
   int waited=0,ready=0;struct pollfd wait{};wait.fd=data;wait.events=POLLIN;
   while(!(ready=poll(&wait,1,100))&&!cancelled&&(waited+=100)<15000){}
   if(cancelled)return -1;
   if(ready<=0){plog("USB reader: no data for 15 s; reconnecting");stop();ftp.shut();continue;}
   const ssize_t got=recv(data,into,room,0);
   if(got>0){at+=got;return got;}
   // Closed: the end of the file, or the connection was lost short of it.
   close(data);data=-1;if(ftp.reply(2000)<0)ftp.shut();
   if(at>=total)return 0;
  }
  plog(("USB reader: gave up reading at "+std::to_string(at)).c_str());return -1;
 }
 int64_t seek(uint64_t to){
  if(to==at)return (int64_t)to;
  // A small jump forward is cheaper read than asked for again.
  if(data>=0&&to>at&&to-at<=256*1024){scrap.resize(65536);while(at<to){const int64_t got=read(scrap.data(),std::min<uint64_t>(scrap.size(),to-at));if(got<=0)return -1;}return (int64_t)to;}
  if(settings.debug_log)plog(("USB reader: from "+std::to_string(at)+" to "+std::to_string(to)).c_str());
  stop();at=to;return (int64_t)to;
 }
};
}  // namespace

void usb_poll_hold(bool on){ftp_holds+=on?1:-1;}
namespace {
// The part after the video's name: "en", "eng", "ch-traditional", "persian".
std::string language_name(const std::string& tag,std::string& code){
 static const char* const names[][2]={{"en","English"},{"eng","English"},{"english","English"},{"fr","French"},{"fre","French"},{"fra","French"},{"french","French"},
  {"de","German"},{"deu","German"},{"ger","German"},{"german","German"},{"es","Spanish"},{"spa","Spanish"},{"spanish","Spanish"},{"it","Italian"},{"ita","Italian"},{"italian","Italian"},
  {"pt","Portuguese"},{"por","Portuguese"},{"portuguese","Portuguese"},{"nl","Dutch"},{"nld","Dutch"},{"dut","Dutch"},{"dutch","Dutch"},{"ru","Russian"},{"rus","Russian"},{"russian","Russian"},
  {"ja","Japanese"},{"jp","Japanese"},{"jpn","Japanese"},{"japanese","Japanese"},{"zh","Chinese"},{"ch","Chinese"},{"chi","Chinese"},{"zho","Chinese"},{"chinese","Chinese"},
  {"ko","Korean"},{"kor","Korean"},{"cs","Czech"},{"cz","Czech"},{"ces","Czech"},{"cze","Czech"},{"pl","Polish"},{"pol","Polish"},{"polish","Polish"},{"hu","Hungarian"},{"hun","Hungarian"},
  {"he","Hebrew"},{"heb","Hebrew"},{"el","Greek"},{"gk","Greek"},{"gre","Greek"},{"ell","Greek"},{"greek","Greek"},{"da","Danish"},{"dan","Danish"},{"dansk","Danish"},{"danish","Danish"},
  {"no","Norwegian"},{"nor","Norwegian"},{"nb","Norwegian"},{"sv","Swedish"},{"swe","Swedish"},{"fi","Finnish"},{"fin","Finnish"},{"hr","Croatian"},{"hrv","Croatian"},{"croatian","Croatian"},
  {"fa","Persian"},{"per","Persian"},{"fas","Persian"},{"persian","Persian"},{"id","Indonesian"},{"ind","Indonesian"},{"indonesian","Indonesian"},{"ar","Arabic"},{"ara","Arabic"},
  {"tr","Turkish"},{"tur","Turkish"},{"uk","Ukrainian"},{"ukr","Ukrainian"},{"ro","Romanian"},{"rum","Romanian"},{"ron","Romanian"},{"th","Thai"},{"tha","Thai"},{"vi","Vietnamese"},{"vie","Vietnamese"}};
 std::string head=tag,rest;const auto dash=tag.find_first_of("-_.");if(dash!=std::string::npos){head=tag.substr(0,dash);rest=tag.substr(dash+1);}
 std::string low=head;for(char& c:low)c=(char)tolower((unsigned char)c);
 for(const auto& row:names)if(low==row[0]){code=low;return std::string(row[1])+(rest.empty()?"":" ("+rest+")");}
 code.clear();if(tag.empty())return "Subtitles";std::string out=tag;out[0]=(char)toupper((unsigned char)out[0]);return out;
}
}  // namespace
std::vector<LocalSubtitleFile> local_subtitle_files(const std::string& video){
 std::vector<LocalSubtitleFile> out;
 const bool ftp=local_is_drive_url(video);const std::string path=ftp?local_url_path(video):video;
 const auto slash=path.rfind('/');if(slash==std::string::npos)return out;
 const std::string dir=path.substr(0,slash),file=path.substr(slash+1);const auto dot=file.rfind('.');const std::string stem=dot==std::string::npos?file:file.substr(0,dot);
 std::vector<std::string> names;
 if(ftp){std::vector<LocalEntry> files;if(ftp_folder_files(dir,files))for(const auto& f:files)names.push_back(f.name);}
 else if(DIR* d=opendir(dir.c_str())){while(auto e=readdir(d))names.push_back(e->d_name);closedir(d);}
 for(const auto& name:names){
  const std::string ext=lower_ext(name);if(ext!="srt"&&ext!="ass"&&ext!="ssa"&&ext!="vtt"&&ext!="sup")continue;
  // The video's own name, then nothing or ".<tag>".
  if(name.size()<stem.size()+ext.size()+1||name.compare(0,stem.size(),stem)!=0)continue;
  const std::string middle=name.substr(stem.size(),name.size()-stem.size()-ext.size()-1);if(!middle.empty()&&middle[0]!='.')continue;
  LocalSubtitleFile sub;const std::string tag=middle.empty()?"":middle.substr(1);sub.label=language_name(tag,sub.lang);
  std::string kind=ext;for(char& c:kind)c=(char)toupper((unsigned char)c);sub.label+=" - "+kind;
  sub.url=ftp?local_file_url(dir+"/"+name):dir+"/"+name;sub.tag=tag;out.push_back(sub);}
 // Two files that read the same ("Film.en.srt", "Film.eng.srt"): their own tags tell them apart.
 std::vector<std::string> labels;for(const auto& a:out)labels.push_back(a.label);
 for(size_t i=0;i<out.size();++i)if(!out[i].tag.empty()&&std::count(labels.begin(),labels.end(),labels[i])>1)out[i].label+=" ("+out[i].tag+")";
 std::sort(out.begin(),out.end(),[](const LocalSubtitleFile& a,const LocalSubtitleFile& b){return natural_less(a.label,b.label);});
 return out;
}
bool local_via_ftp(const std::string& path){return ftp_mode()&&path.rfind("/mnt/usb",0)==0;}
std::string local_file_url(const std::string& path){return "ftp://"+ftp_address()+url_path(path);}
std::string local_url_path(const std::string& url){
 const std::string prefix="ftp://"+ftp_address();std::string out;if(url.rfind(prefix,0)!=0)return out;
 for(size_t i=prefix.size();i<url.size();++i){if(url[i]=='%'&&i+2<url.size()){out+=(char)strtol(url.substr(i+1,2).c_str(),nullptr,16);i+=2;}else out+=url[i];}
 return out;
}
void* ftp_reader_open(const std::string& path){auto* reader=new(std::nothrow) FtpReader;if(!reader)return nullptr;reader->path=path;if(!reader->connect()){delete reader;return nullptr;}return reader;}
int64_t ftp_reader_read(void* reader,char* into,uint64_t room){return static_cast<FtpReader*>(reader)->read(into,room);}
int64_t ftp_reader_seek(void* reader,uint64_t to){return static_cast<FtpReader*>(reader)->seek(to);}
int64_t ftp_reader_size(void* reader){return (int64_t)static_cast<FtpReader*>(reader)->total;}
void ftp_reader_cancel(void* reader){static_cast<FtpReader*>(reader)->cancelled=true;}
void ftp_reader_close(void* reader){delete static_cast<FtpReader*>(reader);}

bool ftp_make_folder(const std::string& path){Ftp ftp;return ftp.open()&&ftp.command("MKD "+path)/100==2;}
uint64_t ftp_file_size(const std::string& path){Ftp ftp;return ftp.open()&&ftp.binary()?ftp.size(path):0;}
bool ftp_delete(const std::string& path){Ftp ftp;return ftp.open()&&ftp.command("DELE "+path)/100==2;}
bool ftp_rename(const std::string& from,const std::string& to){Ftp ftp;return ftp.open()&&ftp.command("RNFR "+from)==350&&ftp.command("RNTO "+to)/100==2;}
bool ftp_read_text(const std::string& path,std::string& out){
 out.clear();Ftp ftp;if(!ftp.open()||!ftp.binary())return false;
 const int data=ftp.passive();if(data<0)return false;
 if(ftp.command("RETR "+path)/100!=1){close(data);return false;}
 std::vector<char> buffer(8192);int n;while((n=read_some(data,buffer.data(),buffer.size()))>0)out.append(buffer.data(),n);close(data);
 return n==0&&ftp.reply()/100==2;
}
struct FtpUpload{Ftp ftp;int data=-1;~FtpUpload(){if(data>=0)close(data);}};
FtpUpload* ftp_upload_open(const std::string& path,bool append){
 auto* upload=new(std::nothrow) FtpUpload;if(!upload)return nullptr;
 bool ok=upload->ftp.open()&&upload->ftp.binary()&&(upload->data=upload->ftp.passive())>=0;
 // A resumed upload is always from the file's end: APPE, never REST (see above).
 if(ok)ok=upload->ftp.command((append?"APPE ":"STOR ")+path)/100==1;
 if(!ok){delete upload;return nullptr;}
 return upload;
}
bool ftp_upload_write(FtpUpload* upload,const char* bytes,size_t count){
 while(count){
  struct pollfd wait{};wait.fd=upload->data;wait.events=POLLOUT;if(poll(&wait,1,30000)<=0)return false;
  const ssize_t sent=send(upload->data,bytes,std::min<size_t>(count,65536),MSG_NOSIGNAL);if(sent<=0)return false;
  bytes+=sent;count-=sent;}
 return true;
}
bool ftp_upload_close(FtpUpload* upload,bool finished){
 close(upload->data);upload->data=-1;
 // The server may still be writing to the drive: its reply can take a while.
 const int reply=upload->ftp.reply(60000);delete upload;return !finished||reply==226;
}
bool ftp_write_text(const std::string& path,const std::string& text){
 FtpUpload* upload=ftp_upload_open(path,false);if(!upload)return false;
 const bool wrote=ftp_upload_write(upload,text.data(),text.size());return ftp_upload_close(upload,true)&&wrote;
}
bool ftp_folder_files(const std::string& path,std::vector<LocalEntry>& out){
 out.clear();Ftp ftp;std::vector<FtpRow> rows;if(!ftp.open()||!ftp.list(path,rows))return false;
 for(const auto& row:rows){if(row.folder)continue;LocalEntry entry;entry.name=row.name;entry.path=path+"/"+row.name;entry.bytes=row.bytes;out.push_back(entry);}
 return true;
}
bool local_is_drive_url(const std::string& url){return !ftp_address().empty()&&url.rfind("ftp://"+ftp_address()+"/",0)==0;}
std::vector<std::string> usb_drives(){
 if(ftp_mode()){ftp_start();pthread_mutex_lock(&ftp_lock);auto copy=ftp_drives;pthread_mutex_unlock(&ftp_lock);return copy;}
 std::vector<std::string> out;
 for(int i=0;i<8;++i){const std::string slot=usb_root()+"/usb"+std::to_string(i);DIR* d=opendir(slot.c_str());if(!d)continue;
  bool any=false;while(auto e=readdir(d))if(strcmp(e->d_name,".")&&strcmp(e->d_name,"..")){any=true;break;}closedir(d);if(any)out.push_back(slot);}
 return out;
}
// On the console stat() cannot be trusted: the toolchain's struct stat does
// not match the kernel's, so its sizes are wrong (65984 for a 33 MB file).
// A folder is one that opens; a size is found by seeking to the end.
bool local_is_dir(const std::string& path){
#ifdef HOST_TEST
 struct stat sb;return stat(path.c_str(),&sb)==0&&S_ISDIR(sb.st_mode);
#else
 DIR* d=opendir(path.c_str());if(!d)return false;closedir(d);return true;
#endif
}
uint64_t local_file_size(const std::string& path){
#ifdef HOST_TEST
 struct stat sb;return stat(path.c_str(),&sb)==0?(uint64_t)sb.st_size:0;
#else
 FILE* f=fopen(path.c_str(),"rb");if(!f)return 0;
 const off_t size=fseeko(f,0,SEEK_END)==0?ftello(f):0;fclose(f);return size>0?(uint64_t)size:0;
#endif
}
std::string usb_drives_signature(){std::string s;for(const auto& d:usb_drives())s+=d+";";return s;}
bool list_local_folder(const std::string& path,std::vector<LocalEntry>& out){
 out.clear();
 auto by_kind=[](const LocalEntry& a,const LocalEntry& b){if((a.kind==LOCAL_FOLDER)!=(b.kind==LOCAL_FOLDER))return a.kind==LOCAL_FOLDER;return natural_less(a.name,b.name);};
 if(ftp_mode()){
  Ftp ftp;std::vector<FtpRow> rows;if(!ftp.open()||!ftp.list(path,rows))return false;
  for(const auto& row:rows){if(row.name[0]=='.'||row.name=="System Volume Information"||row.name=="$RECYCLE.BIN")continue;
   LocalEntry entry;entry.name=row.name;entry.path=path+"/"+row.name;entry.kind=row.folder?LOCAL_FOLDER:kind_of(row.name);if(entry.kind==LOCAL_OTHER)continue;entry.bytes=row.folder?0:row.bytes;out.push_back(entry);}
  std::sort(out.begin(),out.end(),by_kind);return true;}
 DIR* d=opendir(path.c_str());if(!d)return false;
 while(auto e=readdir(d)){
  const std::string name=e->d_name;if(name.empty()||name[0]=='.'||name=="System Volume Information"||name=="$RECYCLE.BIN")continue;
  LocalEntry entry;entry.name=name;entry.path=path+"/"+name;
  if(local_is_dir(entry.path))entry.kind=LOCAL_FOLDER;
  else{entry.kind=kind_of(name);if(entry.kind!=LOCAL_OTHER)entry.bytes=local_file_size(entry.path);
   // A download's description, and a file still downloading, are not media.
   if(entry.kind==LOCAL_OTHER)continue;}
  out.push_back(entry);}
 closedir(d);
 std::sort(out.begin(),out.end(),by_kind);
 return true;
}
Item local_item(const LocalEntry& entry){
 Item item;
 // A stable id from the path, so nothing asks the server about it by mistake.
 unsigned long h=5381;for(unsigned char c:entry.path)h=h*33+c;snprintf(item.base.id,sizeof item.base.id,"local:%08lx",h&0xffffffffUL);
 std::string name=entry.name;const auto dot=name.rfind('.');if(dot!=std::string::npos&&dot>0)name.resize(dot);
 snprintf(item.base.name,sizeof item.base.name,"%s",name.c_str());
 snprintf(item.base.type,sizeof item.base.type,"%s",entry.kind==LOCAL_AUDIO?"Audio":"Video");
 // Music: the folder is the album.
 if(entry.kind==LOCAL_AUDIO){const auto slash=entry.path.rfind('/');if(slash!=std::string::npos){const std::string dir=entry.path.substr(0,slash);const auto s2=dir.rfind('/');item.album=s2==std::string::npos?dir:dir.substr(s2+1);}}
 item.local=local_via_ftp(entry.path)?local_file_url(entry.path):entry.path;return item;
}
