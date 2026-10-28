#include "guide.h"
#include "app.h"
#include "platform.h"
#include "thread_priority.h"
#include "http.h"
#include "jellyfin_api.h"
#include "language.h"
#include <zlib.h>
#include <pthread.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <vector>

namespace {
struct Show { time_t start,stop; std::string title; };
// Shows by channel key (key() of its name), sorted by start.
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
std::map<std::string,std::vector<Show>> shows;
time_t updated=0;int n_channels=0,n_shows=0;
std::string failure;
std::atomic<int> state{GUIDE_IDLE};
std::string gz_path,tsv_path,m3u_path;
constexpr time_t KEEP_BEFORE=3600,KEEP_AFTER=36*3600,STALE=12*3600;

// "ABC KGTV (720p)" -> "abckgtv": letters and digits, no bracketed tags, no
// trailing "hd" -- the same channel in a playlist and in a guide.
std::string key(const std::string& name){
 std::string s;int depth=0;
 for(char ch:name){const unsigned char c=ch;
  if(c=='('||c=='['){++depth;continue;}if(c==')'||c==']'){if(depth)--depth;continue;}if(depth)continue;
  if(isalnum(c))s+=(char)tolower(c);else if(c>=0x80)s+=(char)c;}
 if(s.size()>4&&s.compare(s.size()-2,2,"hd")==0)s.resize(s.size()-2);
 return s;
}
void utf8(std::string& o,unsigned long cp){
 if(cp<0x80)o+=(char)cp;else if(cp<0x800){o+=(char)(0xC0|cp>>6);o+=(char)(0x80|(cp&63));}
 else if(cp<0x10000){o+=(char)(0xE0|cp>>12);o+=(char)(0x80|((cp>>6)&63));o+=(char)(0x80|(cp&63));}
 else{o+=(char)(0xF0|cp>>18);o+=(char)(0x80|((cp>>12)&63));o+=(char)(0x80|((cp>>6)&63));o+=(char)(0x80|(cp&63));}
}
std::string unxml(const std::string& in){
 std::string o;o.reserve(in.size());
 for(size_t i=0;i<in.size();++i){
  if(in[i]!='&'){o+=in[i];continue;}
  const size_t e=in.find(';',i);if(e==std::string::npos||e-i>9){o+='&';continue;}
  const std::string ent=in.substr(i+1,e-i-1);
  if(ent=="amp")o+='&';else if(ent=="lt")o+='<';else if(ent=="gt")o+='>';else if(ent=="quot")o+='"';else if(ent=="apos")o+='\'';
  else if(ent.size()>1&&ent[0]=='#')utf8(o,ent[1]=='x'||ent[1]=='X'?strtoul(ent.c_str()+2,nullptr,16):strtoul(ent.c_str()+1,nullptr,10));
  else o+=in.substr(i,e-i+1);
  i=e;}
 return o;
}
std::string attr(const std::string& tag,const char* name){
 const std::string want=std::string(" ")+name+"=\"";const size_t at=tag.find(want);if(at==std::string::npos)return "";
 const size_t from=at+want.size(),to=tag.find('"',from);return to==std::string::npos?"":unxml(tag.substr(from,to-from));
}
// The text of each <tag ...>text</tag> in a block.
std::vector<std::string> inner(const std::string& block,const char* tag){
 std::vector<std::string> out;const std::string open=std::string("<")+tag,close=std::string("</")+tag+">";
 for(size_t at=block.find(open);at!=std::string::npos;at=block.find(open,at+1)){
  const char next=at+open.size()<block.size()?block[at+open.size()]:0;if(next!='>'&&next!=' ')continue;
  const size_t from=block.find('>',at);if(from==std::string::npos)break;const size_t to=block.find(close,from);if(to==std::string::npos)break;
  out.push_back(unxml(block.substr(from+1,to-from-1)));}
 return out;
}
long long days_from_civil(int y,int m,int d){
 y-=m<=2;const long long era=(y>=0?y:y-399)/400;const int yoe=(int)(y-era*400),doy=(153*(m+(m>2?-3:9))+2)/5+d-1,doe=yoe*365+yoe/4-yoe/100+doy;
 return era*146097+doe-719468;
}
// XMLTV's "20261028120000 +0100" as a Unix time.
time_t xmltv_time(const std::string& s){
 if(s.size()<12)return 0;auto num=[&](size_t at,size_t n){return atoi(s.substr(at,n).c_str());};
 time_t t=(time_t)(days_from_civil(num(0,4),num(4,2),num(6,2))*86400+num(8,2)*3600+num(10,2)*60+(s.size()>=14?num(12,2):0));
 const size_t sign=s.find_first_of("+-",12);
 if(sign!=std::string::npos&&s.size()>=sign+5){const int off=num(sign+1,2)*3600+num(sign+3,2)*60;t+=s[sign]=='+'?-off:off;}
 return t;
}

// Reads an XMLTV file (gzipped or not) a piece at a time, keeping the shows
// inside the window. Returns the channels with any.
bool parse(const std::string& path,std::map<std::string,std::vector<Show>>& out,int& channels,int& count){
 gzFile g=gzopen(path.c_str(),"rb");if(!g){failure="Could not open the downloaded guide.";return false;}
 const time_t now=time(nullptr);
 std::map<std::string,std::vector<std::string>> names;   // channel id -> display names
 std::map<std::string,std::vector<Show>> by_id;
 std::string buf;size_t pos=0;bool eof=false;static char chunk[65536];count=0;
 auto more=[&]{const int n=gzread(g,chunk,sizeof chunk);if(n<=0){eof=true;return;}if(pos>(1u<<20)){buf.erase(0,pos);pos=0;}buf.append(chunk,n);};
 for(;;){
  const size_t c=buf.find("<channel",pos),p=buf.find("<programme",pos),at=std::min(c,p);
  if(at==std::string::npos){if(eof)break;if(buf.size()>pos+16)pos=buf.size()-16;more();continue;}
  const bool prog=at==p;const char* close=prog?"</programme>":"</channel>";
  const size_t end=buf.find(close,at);
  if(end==std::string::npos){if(eof)break;more();continue;}
  const std::string block=buf.substr(at,end+strlen(close)-at);pos=end+strlen(close);
  const std::string open=block.substr(0,block.find('>')+1);
  if(!prog){const std::string id=attr(open,"id");if(!id.empty())for(auto& n:inner(block,"display-name"))names[id].push_back(n);continue;}
  const time_t start=xmltv_time(attr(open,"start")),stop=xmltv_time(attr(open,"stop"));
  if(!start||stop<=now-KEEP_BEFORE||start>=now+KEEP_AFTER||count>=200000)continue;
  const auto titles=inner(block,"title");if(titles.empty())continue;
  by_id[attr(open,"channel")].push_back({start,stop?stop:start+1800,titles[0]});++count;
 }
 gzclose(g);
 channels=0;
 for(auto& [id,list]:by_id){
  std::sort(list.begin(),list.end(),[](const Show& a,const Show& b){return a.start<b.start;});
  std::vector<std::string> keys;for(auto& n:names[id])keys.push_back(key(n));keys.push_back(key(id));
  bool used=false;for(auto& k:keys)if(!k.empty()&&!out.count(k)){out[k]=list;used=true;}
  channels+=used;
 }
 if(!count){failure="The guide has no shows for the coming day.";return false;}
 return true;
}
void save_tsv(){
 FILE* f=fopen(tsv_path.c_str(),"w");if(!f)return;
 fprintf(f,"#guide\t%lld\t%d\t%d\n",(long long)updated,n_channels,n_shows);
 for(auto& [k,list]:shows)for(auto& s:list){std::string t=s.title;for(char& c:t)if(c=='\t'||c=='\n'||c=='\r')c=' ';fprintf(f,"%s\t%lld\t%lld\t%s\n",k.c_str(),(long long)s.start,(long long)s.stop,t.c_str());}
 fclose(f);
}
void load_tsv(){
 FILE* f=fopen(tsv_path.c_str(),"r");if(!f)return;
 // A channel's shows are written once for each of its names: the header
 // holds the real counts.
 char line[1024];long long when=0;int channels=0,count=0;
 if(!fgets(line,sizeof line,f)||sscanf(line,"#guide\t%lld\t%d\t%d",&when,&channels,&count)!=3){fclose(f);return;}
 std::map<std::string,std::vector<Show>> got;
 while(fgets(line,sizeof line,f)){
  char* a=strchr(line,'\t');if(!a)continue;*a++=0;char* b=strchr(a,'\t');if(!b)continue;*b++=0;char* c=strchr(b,'\t');if(!c)continue;*c++=0;
  c[strcspn(c,"\r\n")]=0;got[line].push_back({(time_t)atoll(a),(time_t)atoll(b),c});}
 fclose(f);
 pthread_mutex_lock(&lock);shows.swap(got);updated=(time_t)when;n_channels=channels;n_shows=count;pthread_mutex_unlock(&lock);
}
std::string json_strings(const std::string& j,const char* name,std::vector<std::string>& out){
 const std::string want=std::string("\"")+name+"\":\"";
 for(size_t at=j.find(want);at!=std::string::npos;at=j.find(want,at+1)){
  std::string v;for(size_t i=at+want.size();i<j.size()&&j[i]!='"';++i){if(j[i]=='\\'&&i+1<j.size())++i;v+=j[i];}
  out.push_back(v);}
 return "";
}
// The guide the playlist names, through Jellyfin's tuner setup (admins only):
// a web XMLTV listings provider, else a playlist's url-tvg / x-tvg-url.
std::string discover(){
 std::vector<char> body(256*1024);
 // A non-admin gets 403 (not 401), which does not sign the session out.
 const int status=http_request(HTTP_GET,(std::string(g_server)+"/System/Configuration/livetv").c_str(),"",g_token,body.data(),(int)body.size());
 if(status!=200){plog(("Guide: the tuner setup is not readable by this account (HTTP "+std::to_string(status)+")").c_str());return "";}
 const std::string j(body.data());
 std::vector<std::string> paths,urls;json_strings(j,"Path",paths);json_strings(j,"Url",urls);
 for(auto& p:paths)if(p.rfind("http",0)==0){plog("Guide: using the server's XMLTV listings address");return p;}
 for(auto& u:urls){
  if(u.rfind("http",0)!=0)continue;
  if(http_download_file(u.c_str(),m3u_path.c_str(),[](uint64_t,uint64_t){return true;})!=200)continue;
  FILE* f=fopen(m3u_path.c_str(),"r");if(!f)continue;char head[4096]={};fread(head,1,sizeof head-1,f);fclose(f);remove(m3u_path.c_str());
  const std::string first(head,strcspn(head,"\r\n"));
  for(const char* tag:{"url-tvg=\"","x-tvg-url=\""}){const size_t at=first.find(tag);if(at==std::string::npos)continue;
   std::string v=first.substr(at+strlen(tag));v=v.substr(0,v.find('"'));v=v.substr(0,v.find_first_of(", "));
   if(v.rfind("http",0)==0){plog("Guide: the playlist names its guide");return v;}}
 }
 plog("Guide: the playlist names no guide");return "";
}
void* worker(void*){
 background_priority();
 std::string url=settings.guide_url;
 if(url.empty()){
  const std::string found=discover();
  if(!found.empty()&&found!=settings.guide_found){settings.guide_found=found;save_settings();}
  url=settings.guide_found;
 }
 if(url.empty()){failure="No guide address. Enter one under Guide Address, or use a playlist that names its guide (url-tvg).";state=GUIDE_FAILED;return nullptr;}
 plog("Guide: downloading");
 int status=http_download_file(url.c_str(),gz_path.c_str(),[](uint64_t,uint64_t){return true;});
 if(status!=200)status=ffmpeg_download_file(url.c_str(),gz_path.c_str(),[](uint64_t,uint64_t){return true;});
 if(status!=200){failure="The guide did not download (HTTP "+std::to_string(status)+").";plog(("Guide: "+failure).c_str());state=GUIDE_FAILED;return nullptr;}
 std::map<std::string,std::vector<Show>> got;int channels=0,count=0;
 if(!parse(gz_path,got,channels,count)){plog(("Guide: "+failure).c_str());state=GUIDE_FAILED;return nullptr;}
 remove(gz_path.c_str());
 pthread_mutex_lock(&lock);shows.swap(got);updated=time(nullptr);n_channels=channels;n_shows=count;save_tsv();pthread_mutex_unlock(&lock);
 plog(("Guide: updated, "+std::to_string(channels)+" channels, "+std::to_string(count)+" shows").c_str());
 failure.clear();state=GUIDE_IDLE;return nullptr;
}
}  // namespace

void guide_start(){
 gz_path=jf_data_path("guide.download");tsv_path=jf_data_path("guide.tsv");m3u_path=jf_data_path("guide-playlist.m3u");
 load_tsv();
 if(settings.guide_auto&&time(nullptr)-updated>STALE)guide_update_now();
}
void guide_update_now(){
 int idle=GUIDE_IDLE,failed=GUIDE_FAILED;
 if(!state.compare_exchange_strong(idle,GUIDE_UPDATING)&&!state.compare_exchange_strong(failed,GUIDE_UPDATING))return;
 pthread_t thread;if(pthread_create(&thread,nullptr,worker,nullptr)==0)pthread_detach(thread);else{failure="Could not start the update.";state=GUIDE_FAILED;}
}
int guide_state(){return state;}
std::string guide_status(){
 if(state==GUIDE_UPDATING)return T("Updating the guide...");
 if(state==GUIDE_FAILED)return failure;
 pthread_mutex_lock(&lock);const time_t when=updated;const int ch=n_channels,count=n_shows;pthread_mutex_unlock(&lock);
 if(!when)return T("Not downloaded yet.");
 const long ago=(long)(time(nullptr)-when);
 const std::string age=ago<3600?TF("%d min ago",(int)std::max(1L,ago/60)):ago<86400?TF("%d h ago",(int)(ago/3600)):TF("%d days ago",(int)(ago/86400));
 return TF("Updated %s",age.c_str())+"  \xC2\xB7  "+TF("%d channels",ch)+"  \xC2\xB7  "+TF("%d shows",count);
}
std::string guide_address(bool* from_playlist){
 if(from_playlist)*from_playlist=settings.guide_url.empty()&&!settings.guide_found.empty();
 return settings.guide_url.empty()?settings.guide_found:settings.guide_url;
}
bool guide_now_next(const std::string& channel_name,GuideNow& out){
 const std::string k=key(channel_name);const time_t now=time(nullptr);
 pthread_mutex_lock(&lock);
 auto it=shows.find(k);bool any=false;
 if(it!=shows.end()){
  const auto& list=it->second;
  auto at=std::upper_bound(list.begin(),list.end(),now,[](time_t t,const Show& s){return t<s.stop;});
  if(at!=list.end()){any=true;
   if(at->start<=now){out.now=at->title;out.now_start=at->start;out.now_stop=at->stop;++at;}
   if(at!=list.end()){out.next=at->title;out.next_start=at->start;}}
 }
 pthread_mutex_unlock(&lock);return any;
}
