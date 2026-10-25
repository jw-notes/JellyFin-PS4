#include "app.h"
#include "auth.h"
#include "playback_model.h"
#include "api_userdata.h"
#include "json_unescape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <atomic>
#include <algorithm>
#include <map>
#include <set>
#include <deque>
#include <vector>
static std::string live_stream_id;
Settings settings;
std::string device_id, data_dir, last_error;
static std::string diagnostic_log_path;
volatile bool g_auth_expired=false;
static std::string field(const char* j,const char* k) {char b[2048]={}; const char* e=j+strlen(j);auto p=find_top_value(j,e,k);if(p&&*p=='"')json_unescape(p+1,e,b,sizeof b);return b;}
static uint64_t number(const char* j,const char* k){auto e=j+strlen(j);auto p=find_top_value(j,e,k);return p?(uint64_t)read_json_int(p,e,0):0;}
std::string json_quote(const std::string& s){std::string o="\"";const char hex[]="0123456789abcdef";for(unsigned char c:s){if(c=='"'||c=='\\'){o+='\\';o+=c;}else if(c<32){o+="\\u00";o+=hex[c>>4];o+=hex[c&15];}else o+=c;}return o+'"';}
std::string encode(const std::string& s){std::string o;const char hex[]="0123456789ABCDEF";for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')o+=c;else{o+='%';o+=hex[c>>4];o+=hex[c&15];}}return o;}
const char *jf_data_path(const char* name){static std::string p;p=data_dir+"/"+name;return p.c_str();}
static void append_log(const std::string& path,const char* message,bool shared=false){
    FILE* file=fopen(path.c_str(),"a");
    if(file){if(shared)fchmod(fileno(file),0644);fprintf(file,"%lld %s\n",(long long)time(nullptr),message);fclose(file);}
}
void plog(const char* message){
    if(data_dir.empty())return;
    append_log(data_dir+"/app.log",message);
    // Only diagnostic messages reach this file: no passwords, tokens or
    // Quick Connect secrets are logged. The private session folder stays 0700.
    if(!diagnostic_log_path.empty())append_log(diagnostic_log_path,message,true);
}
void crash_log(const char* m){plog(m);}
static bool write_atomic(const char* name,const std::string& text){std::string dest=jf_data_path(name),tmp=dest+".tmp";FILE*f=fopen(tmp.c_str(),"wb");if(!f)return false;chmod(tmp.c_str(),0600);bool ok=fwrite(text.data(),1,text.size(),f)==text.size();if(fflush(f)||fsync(fileno(f)))ok=false;if(fclose(f))ok=false;return ok&&rename(tmp.c_str(),dest.c_str())==0;}
static std::string read_file(const char* n){FILE*f=fopen(jf_data_path(n),"rb");if(!f)return {};char b[8192];size_t z=fread(b,1,sizeof(b)-1,f);fclose(f);b[z]=0;return b;}
bool storage_init(int user){
#ifdef HOST_TEST
 const char* base=getenv("JF_TEST_DATA");data_dir=base?base:"/tmp/jellyfin-ps4-test";mkdir(data_dir.c_str(),0700);
#else
 mkdir("/data/JellyFin-PS4",0700);char dir[128];snprintf(dir,sizeof dir,"/data/JellyFin-PS4/user-%08x",user);data_dir=dir;mkdir(data_dir.c_str(),0700);
 if(mkdir("/data/JellyFin-PS4-logs",0755)==0)chmod("/data/JellyFin-PS4-logs",0755);
 char log_path[160];snprintf(log_path,sizeof log_path,"/data/JellyFin-PS4-logs/user-%08x.log",user);
 diagnostic_log_path=log_path;
#endif
 device_id=read_file("device-id");if(device_id.empty()){char id[80];snprintf(id,sizeof id,"ps4-%08x-%llx-%llx",user,(long long)time(nullptr),(long long)clock());device_id=id;if(!write_atomic("device-id",device_id))return false;}
 auto j=read_file("settings.json");if(!j.empty()){auto b=number(j.c_str(),"bitrate");if(b>=1000000&&b<=20000000)settings.bitrate=(int)b;settings.resume=number(j.c_str(),"resume")!=0;settings.direct=number(j.c_str(),"direct")!=0;if(j.find("\"saver\"")!=std::string::npos)settings.saver=number(j.c_str(),"saver")!=0;auto delay=number(j.c_str(),"saver_delay");if(delay>=30&&delay<=3600)settings.saver_delay=(int)delay;}return true;}
bool save_settings(){return write_atomic("settings.json","{\"bitrate\":"+std::to_string(settings.bitrate)+",\"resume\":"+std::to_string(settings.resume)+",\"direct\":"+std::to_string(settings.direct)+",\"saver\":"+std::to_string(settings.saver)+",\"saver_delay\":"+std::to_string(settings.saver_delay)+"}");}
bool save_session(){return write_atomic("session.json","{\"server\":"+json_quote(g_server)+",\"username\":"+json_quote(g_username)+",\"token\":"+json_quote(g_token)+",\"userid\":"+json_quote(g_userid)+"}");}
bool load_session(){auto j=read_file("session.json");snprintf(g_server,sizeof g_server,"%s",field(j.c_str(),"server").c_str());snprintf(g_username,sizeof g_username,"%s",field(j.c_str(),"username").c_str());snprintf(g_token,sizeof g_token,"%s",field(j.c_str(),"token").c_str());snprintf(g_userid,sizeof g_userid,"%s",field(j.c_str(),"userid").c_str());return g_token[0]&&g_server[0]&&g_userid[0];}
static int request(int method,const std::string& path,const std::string& body=""){int st=http_request(method,(std::string(g_server)+path).c_str(),body.c_str(),g_token,responseBuffer,RESPONSE_SIZE);if(st<200||st>=300)last_error=st<0?http_failure_detail():"Server request failed (HTTP "+std::to_string(st)+")";return st;}
bool finish_login(const char* response, const std::string& fallback_username) {
    const auto token = field(response, "AccessToken");
    const char* end = response + strlen(response);
    const char* user = find_top_value(response, end, "User");
    const char* user_end = user ? object_end(user, end) : nullptr;
    const std::string user_json = user_end ? std::string(user, user_end) : "";
    const auto uid = field(user_json.c_str(), "Id");
    auto username = field(user_json.c_str(), "Name");
    if (username.empty()) username = fallback_username;
    if (token.empty() || uid.empty() || token.size() >= sizeof g_token || uid.size() >= sizeof g_userid || username.size() >= sizeof g_username) {
        last_error = "Invalid authentication response"; return false;
    }
    snprintf(g_token, sizeof g_token, "%s", token.c_str());
    snprintf(g_userid, sizeof g_userid, "%s", uid.c_str());
    snprintf(g_username, sizeof g_username, "%s", username.c_str());
    g_auth_expired = false;
    if (!save_session()) plog("Signed in; session could not be saved");
    return true;
}
bool login(const std::string& username, const std::string& password) {
    g_token[0] = g_userid[0] = 0;
    int status = request(HTTP_POST, "/Users/AuthenticateByName", "{\"Username\":" + json_quote(username) + ",\"Pw\":" + json_quote(password) + "}");
    return status == 200 && finish_login(responseBuffer, username);
}
void logout(){request(HTTP_POST,"/Sessions/Logout");g_token[0]=g_userid[0]=0;save_session();g_auth_expired=false;}
static std::string first_string(const char* j,const char* e,const char* k){auto a=find_top_value(j,e,k);char b[512]={};if(a&&*a=='[')read_json_string(skip_ws(a+1,e),e,b,sizeof b);return b;}
static std::string decimal(const char* j,const char* e,const char* k){auto p=find_top_value(j,e,k);if(!p||(*p!='-'&&(*p<'0'||*p>'9')))return {};char b[16];snprintf(b,sizeof b,"%.1f",strtod(p,nullptr));return b;}
static Item parse_item(const char* j){Item i;snprintf(i.base.id,sizeof i.base.id,"%s",field(j,"Id").c_str());snprintf(i.base.name,sizeof i.base.name,"%s",field(j,"Name").c_str());snprintf(i.base.type,sizeof i.base.type,"%s",field(j,"Type").c_str());i.overview=field(j,"Overview");i.duration=number(j,"RunTimeTicks")/10000;auto e=j+strlen(j);auto u=find_top_value(j,e,"UserData");if(u){i.resume=number(u,"PlaybackPositionTicks")/10000;auto f=find_top_value(u,e,"IsFavorite");i.favourite=f&&read_json_bool(f,e);f=find_top_value(u,e,"Played");i.played=f&&read_json_bool(f,e);}
 i.collection=field(j,"CollectionType");i.year=(int)number(j,"ProductionYear");i.rating=field(j,"OfficialRating");i.community=decimal(j,e,"CommunityRating");
 i.series=field(j,"SeriesName");i.series_id=field(j,"SeriesId");i.season=(int)number(j,"ParentIndexNumber");i.episode=(int)number(j,"IndexNumber");i.tagline=first_string(j,e,"Taglines");
 auto backdrops=find_top_value(j,e,"BackdropImageTags");i.backdrop=backdrops&&*backdrops=='['&&*skip_ws(backdrops+1,e)=='"';
 // The detail page shows every billed actor as a card (PS3 Classic); the
 // short cast string is kept for the quick peek.
 auto people=find_top_value(j,e,"People");int actors=0;
 if(people&&*people=='['){auto p=people+1;while(p<e&&i.people.size()<24){p=skip_ws(p,e);if(p>=e||*p==']')break;if(*p==','){++p;continue;}auto next=object_end(p,e);if(!next)break;std::string person(p,next);auto type=field(person.c_str(),"Type"),name=field(person.c_str(),"Name");
  if(type=="Director"&&i.director.empty())i.director=name;
  if(type=="Actor"&&!name.empty()){if(actors<6){if(actors)i.cast+=", ";i.cast+=name;}++actors;Person cast;cast.id=field(person.c_str(),"Id");cast.name=name;cast.role=field(person.c_str(),"Role");cast.image=!field(person.c_str(),"PrimaryImageTag").empty();i.people.push_back(cast);}
  p=next;}}
 auto genres=find_top_value(j,e,"Genres");if(genres&&*genres=='['){auto p=genres+1;for(int n=0;n<3&&p<e;){p=skip_ws(p,e);if(p>=e||*p==']')break;if(*p==','){++p;continue;}if(*p!='"')break;char genre[128];if(!read_json_string(p,e,genre,sizeof genre))break;if(n++)i.genres+=", ";i.genres+=genre;auto q=p+1;bool esc=false;while(q<e&&(esc||*q!='"')){esc=!esc&&*q=='\\';++q;}p=q+1;}}
 return i;}
static bool parse_items(const char* json,Page& next,std::string& error){
 auto end=json+strlen(json);auto a=find_top_value(json,end,"Items");next.total=(int)number(json,"TotalRecordCount");if(!a||*a!='['){error="Invalid library response";return false;}a++;
 while(a<end&&next.items.size()<60){a=skip_ws(a,end);if(*a==']')break;if(*a==','){a++;continue;}auto b=object_end(a,end);if(!b){error="Truncated library response";return false;}std::string obj(a,b);auto i=parse_item(obj.c_str());if(i.base.id[0])next.items.push_back(i);a=b;}if(next.total==0)next.total=(int)next.items.size();return true;
}
static bool parse_items(Page& next){return parse_items(responseBuffer,next,last_error);}
static std::string page_path(const std::string& parent,int start,const std::string& query,int mode,const std::string& letter){
 // Cards need only their name, year, runtime, synopsis and genres. Cast lists
 // belong to the detail and quick-peek requests: for 60 films they made the
 // response many times larger. Track/source lists are left out likewise.
 std::string path="/Users/"+encode(g_userid)+"/Items?Limit=60&StartIndex="+std::to_string(start)+"&Fields=Overview,Genres&EnableImageTypes=Primary,Backdrop&ImageTypeLimit=1&EnableTotalRecordCount=true";
 if(parent.empty()&&query.empty()&&mode==0)path="/Users/"+encode(g_userid)+"/Views";
 // An actor's filmography, opened from a cast card on the detail page.
 else if(mode==4)path+="&Recursive=true&PersonIds="+encode(parent)+"&IncludeItemTypes=Movie,Series&SortBy=ProductionYear,SortName&SortOrder=Descending";
 else if(!parent.empty())path+="&ParentId="+encode(parent)+"&SortBy=SortName&SortOrder=Ascending";
 // The PS3 jump bar: '#' lists names before A, a letter lists names starting with it.
 if(letter=="#")path+="&NameLessThan=A";else if(!letter.empty())path+="&NameStartsWith="+encode(letter);
 if(!query.empty())path+="&Recursive=true&SearchTerm="+encode(query);
 if(mode==1)path+="&Recursive=true&Filters=IsResumable&SortBy=DatePlayed&SortOrder=Descending";
 if(mode==2)path+="&Recursive=true&Filters=IsFavorite";
 if(mode==3)path="/LiveTv/Channels?UserId="+encode(g_userid)+"&Limit=60&StartIndex="+std::to_string(start);
 return path;
}
// Library pages already seen or fetched ahead, keyed by request path. A
// worker fetches the next/previous page while the current one is browsed, so
// paging through a large library does not wait on the server each time.
static pthread_mutex_t page_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t page_changed=PTHREAD_COND_INITIALIZER;
static std::map<std::string,Page> page_cache;
static std::deque<std::string> page_order,prefetch_queue;
static std::set<std::string> prefetch_pending;
static pthread_t page_thread;static bool page_running=false,page_stopping=false;
static void cache_page(const std::string& path,const Page& page){
 if(!page_cache.count(path))page_order.push_back(path);page_cache[path]=page;
 while(page_order.size()>16){page_cache.erase(page_order.front());page_order.pop_front();}
}
static void* prefetch_worker(void*){
 std::vector<uint8_t> body(RESPONSE_SIZE+1);
 for(;;){
  pthread_mutex_lock(&page_lock);while(prefetch_queue.empty()&&!page_stopping)pthread_cond_wait(&page_changed,&page_lock);
  if(page_stopping){pthread_mutex_unlock(&page_lock);return nullptr;}
  std::string path=prefetch_queue.front();prefetch_queue.pop_front();const std::string url=std::string(g_server)+path,token=g_token;pthread_mutex_unlock(&page_lock);
  int count=http_fetch_binary(url.c_str(),token.c_str(),body.data(),RESPONSE_SIZE);Page page;std::string error;bool ok=false;
  if(count>0){body[count]=0;ok=parse_items((const char*)body.data(),page,error);}
  pthread_mutex_lock(&page_lock);if(ok&&!page_stopping)cache_page(path,page);prefetch_pending.erase(path);pthread_cond_broadcast(&page_changed);pthread_mutex_unlock(&page_lock);
  plog(ok?"Library: next page fetched ahead":"Library: page prefetch failed");
 }
}
void prefetch_page(const std::string& parent,int start,const std::string& query,int mode,const std::string& letter){
 if(start<0||(parent.empty()&&query.empty()&&mode==0))return;
 const auto path=page_path(parent,start,query,mode,letter);
 pthread_mutex_lock(&page_lock);
 if(!page_cache.count(path)&&!prefetch_pending.count(path)){
  if(!page_running){page_stopping=false;page_running=pthread_create(&page_thread,nullptr,prefetch_worker,nullptr)==0;}
  if(page_running){while(prefetch_queue.size()>=4){prefetch_pending.erase(prefetch_queue.front());prefetch_queue.pop_front();}prefetch_pending.insert(path);prefetch_queue.push_back(path);pthread_cond_signal(&page_changed);}
 }
 pthread_mutex_unlock(&page_lock);
}
bool page_ready(const std::string& parent,int start,const std::string& query,int mode,const std::string& letter){
 const auto path=page_path(parent,start,query,mode,letter);pthread_mutex_lock(&page_lock);bool ready=page_cache.count(path)>0;pthread_mutex_unlock(&page_lock);return ready;
}
void page_cache_clear(){pthread_mutex_lock(&page_lock);page_cache.clear();page_order.clear();pthread_mutex_unlock(&page_lock);}
void page_cache_stop(){
 pthread_mutex_lock(&page_lock);bool join=page_running;page_stopping=true;prefetch_queue.clear();pthread_cond_broadcast(&page_changed);pthread_mutex_unlock(&page_lock);
 if(join)pthread_join(page_thread,nullptr);
 pthread_mutex_lock(&page_lock);page_running=page_stopping=false;prefetch_pending.clear();page_cache.clear();page_order.clear();pthread_mutex_unlock(&page_lock);
}
bool fetch_page(const std::string& parent,int start,const std::string& query,int mode,Page& page,const std::string& letter){
 const auto path=page_path(parent,start,query,mode,letter);
 pthread_mutex_lock(&page_lock);
 // A page being fetched ahead is about to land: wait for it, not a second request.
 while(prefetch_pending.count(path)&&!page_stopping)pthread_cond_wait(&page_changed,&page_lock);
 auto cached=page_cache.find(path);if(cached!=page_cache.end()){page=cached->second;pthread_mutex_unlock(&page_lock);return true;}
 pthread_mutex_unlock(&page_lock);
 if(request(HTTP_GET,path)!=200)return false;
 Page next;if(!parse_items(next))return false;page=next;
 pthread_mutex_lock(&page_lock);cache_page(path,page);pthread_mutex_unlock(&page_lock);return true;
}
bool fetch_home_row(HomeRowKind kind,const Item& library,Page& page){
 const std::string fields="&Fields=Overview,Genres&EnableImageTypes=Primary,Backdrop&ImageTypeLimit=1";std::string path;
 if(kind==HOME_CONTINUE)path="/Users/"+encode(g_userid)+"/Items/Resume?Limit=16&Recursive=true&MediaTypes=Video"+fields;
 else if(kind==HOME_NEXTUP)path="/Shows/NextUp?userId="+encode(g_userid)+"&Limit=16"+fields;
 else{const char* type=library.collection=="tvshows"?"Series":library.collection=="music"?"MusicAlbum":"Movie";path="/Users/"+encode(g_userid)+"/Items?ParentId="+encode(library.base.id)+"&IncludeItemTypes="+type+"&Recursive=true&SortBy=DateCreated&SortOrder=Descending&Limit=16"+fields;}
 if(request(HTTP_GET,path)!=200)return false;
 Page next;if(!parse_items(next))return false;page=next;return true;
}
bool fetch_detail(const Item& i,Item& detail){if(request(HTTP_GET,"/Users/"+encode(g_userid)+"/Items/"+encode(i.base.id))!=200)return false;detail=parse_item(responseBuffer);return detail.base.id[0];}
bool parse_preview_item(const char* json,Item& result){result=parse_item(json);return result.base.id[0];}
bool playback_url(const Item& i,int audio,int sub,const std::string& selected_source,std::string& url,std::string& session,std::string& source_id){
 const std::string profile="{\"Name\":\"JellyFin PS4\",\"MaxStreamingBitrate\":"+std::to_string(settings.bitrate)+",\"DirectPlayProfiles\":[],\"TranscodingProfiles\":[{\"Container\":\"ts\",\"Type\":\"Video\",\"VideoCodec\":\"h264\",\"AudioCodec\":\"aac\",\"Protocol\":\"hls\",\"Context\":\"Streaming\",\"MaxAudioChannels\":\"2\",\"MinSegments\":\"2\",\"BreakOnNonKeyFrames\":true},{\"Container\":\"mp4\",\"Type\":\"Audio\",\"AudioCodec\":\"aac\",\"Protocol\":\"http\",\"Context\":\"Streaming\"}],\"CodecProfiles\":[{\"Type\":\"Video\",\"Codec\":\"h264\",\"Conditions\":[{\"Condition\":\"LessThanEqual\",\"Property\":\"VideoBitDepth\",\"Value\":\"8\",\"IsRequired\":true},{\"Condition\":\"LessThanEqual\",\"Property\":\"Width\",\"Value\":\"1280\",\"IsRequired\":true},{\"Condition\":\"LessThanEqual\",\"Property\":\"Height\",\"Value\":\"720\",\"IsRequired\":true}]},{\"Type\":\"VideoAudio\",\"Codec\":\"aac\",\"Conditions\":[{\"Condition\":\"Equals\",\"Property\":\"AudioSampleRate\",\"Value\":\"48000\",\"IsRequired\":true}]}],\"SubtitleProfiles\":[{\"Format\":\"srt\",\"Method\":\"Encode\"},{\"Format\":\"ass\",\"Method\":\"Encode\"},{\"Format\":\"pgssub\",\"Method\":\"Encode\"}]}";
 const bool live=!strcmp(i.base.type,"TvChannel");
 live_stream_id.clear();session.clear();source_id.clear();url.clear();
 std::string body="{\"UserId\":"+json_quote(g_userid)+",\"ItemId\":"+json_quote(i.base.id)+",\"DeviceProfile\":"+profile+",\"EnableDirectPlay\":false,\"EnableDirectStream\":false,\"EnableTranscoding\":true,\"AutoOpenLiveStream\":true,\"MaxAudioChannels\":2,\"MaxStreamingBitrate\":"+std::to_string(settings.bitrate)+",\"StartTimeTicks\":"+std::to_string(!live&&settings.resume?i.resume*10000:0)+",\"SubtitleStreamIndex\":"+std::to_string(sub);
 if(!selected_source.empty())body+=",\"MediaSourceId\":"+json_quote(selected_source);
 if(audio>=0)body+=",\"AudioStreamIndex\":"+std::to_string(audio);
 body+="}";
 const std::string endpoint="/Items/"+encode(i.base.id)+"/PlaybackInfo?UserId="+encode(g_userid)+"&AutoOpenLiveStream=true&IsPlayback=true";
 if(request(HTTP_POST,endpoint,body)!=200)return false;
 std::string reply=responseBuffer;
 plog(playback_response_summary(reply).c_str());
 session=field(reply.c_str(),"PlaySessionId");
 std::string source=select_playback_source(reply,selected_source);
 if(live&&source.empty()&&field(reply.c_str(),"ErrorCode")=="NoCompatibleStream"){
  plog("Live TV: profiled playback returned no source; requesting the channel source catalogue");
  if(request(HTTP_GET,"/Items/"+encode(i.base.id)+"/PlaybackInfo?UserId="+encode(g_userid))!=200)return false;
  reply=responseBuffer;plog(playback_response_summary(reply).c_str());source=select_playback_source(reply,"");
  auto catalog_session=field(reply.c_str(),"PlaySessionId");if(!catalog_session.empty())session=catalog_session;
 }

 if(source.empty()){
  std::string reason=field(reply.c_str(),"ErrorCode");
  last_error=(live?"The server returned no playable Live TV sources":"The server returned no playable media sources")+std::string(reason.empty()?".":" ("+reason+").");
  plog(last_error.c_str());return false;
 }
 auto end=source.c_str()+source.size();auto opening=find_top_value(source.c_str(),end,"RequiresOpening");
 auto token=field(source.c_str(),"OpenToken");
 if(opening&&read_json_bool(opening,end)&&field(source.c_str(),"LiveStreamId").empty()){
  if(token.empty()){last_error="The live source requires opening, but the server did not provide an open token.";plog(last_error.c_str());return false;}
  std::string open_body=body;open_body.pop_back();open_body+=",\"OpenToken\":"+json_quote(token)+",\"PlaySessionId\":"+json_quote(session)+"}";
  plog("Live TV: opening the server-provided live source");
  if(request(HTTP_POST,"/LiveStreams/Open",open_body)!=200)return false;
  auto opened_end=responseBuffer+strlen(responseBuffer);auto media=find_top_value(responseBuffer,opened_end,"MediaSource");auto media_end=media?object_end(media,opened_end):nullptr;
  if(!media_end){last_error="The server did not return an opened live source.";plog(last_error.c_str());return false;}
  source.assign(media,media_end);
 }
 source_id=field(source.c_str(),"Id");live_stream_id=field(source.c_str(),"LiveStreamId");url=field(source.c_str(),"TranscodingUrl");
 end=source.c_str()+source.size();auto support=find_top_value(source.c_str(),end,"SupportsDirectStream");
 plog((std::string("Playback source: id=")+source_id+" live="+(!live_stream_id.empty()?"yes":"no")+" transcoding_url="+(!url.empty()?"yes":"no")).c_str());
 if(settings.direct&&!live){url=std::string(g_server)+(strcmp(i.base.type,"Audio")==0?"/Audio/":"/Videos/")+encode(i.base.id)+"/stream?Static=true&MediaSourceId="+encode(source_id)+"&api_key="+encode(g_token);}
 else if(url.empty()){
  auto transcode=find_top_value(source.c_str(),end,"SupportsTranscoding");
  if(live&&!live_stream_id.empty()&&!source_id.empty()&&transcode&&read_json_bool(transcode,end)){
   url=std::string(g_server)+"/Videos/"+encode(i.base.id)+"/master.m3u8?MediaSourceId="+encode(source_id)+"&LiveStreamId="+encode(live_stream_id)+"&PlaySessionId="+encode(session)+"&DeviceId="+encode(device_id)+"&VideoCodec=h264&AudioCodec=aac&VideoBitrate="+std::to_string(std::max(1000000,settings.bitrate-192000))+"&AudioBitrate=192000&SegmentContainer=ts&MinSegments=2";
   plog("Live TV: using the opened source with Jellyfin's H264/AAC HLS endpoint");
  }
  auto direct=field(source.c_str(),"DirectStreamUrl");
  if(url.empty()&&!direct.empty()&&support&&read_json_bool(support,end))url=direct;
  if(url.empty()){last_error=live?"The live source opened, but the server did not offer a compatible H264/AAC stream.":"The server did not offer a compatible H264/AAC transcode.";plog(last_error.c_str());return false;}
 }
 if(!url.empty()&&url[0]=='/')url=std::string(g_server)+url;
 // AvPlayer makes its own HTTP requests and cannot inherit X-Emby-Token
 // from our API transport. Authenticate same-server URLs when the server
 // has omitted the key; never attach our token to an external provider URL.
 const std::string base=std::string(g_server)+"/";
 if(url.rfind(base,0)==0&&url.find("api_key=")==std::string::npos){url+=(url.find('?')==std::string::npos?"?":"&");url+="api_key="+encode(g_token);}
 if(!settings.direct||live){url+=(url.find('?')==std::string::npos?"?":"&");url+="MaxWidth=1280&MaxHeight=720&AudioSampleRate=48000&MaxAudioChannels=2";}
 if(url.rfind("http://",0)!=0&&url.rfind("https://",0)!=0){last_error="Unsupported playback URL";return false;}
 return true;
}

static pthread_t report_thread;
static bool report_joinable=false;
static std::atomic<bool> report_busy{false};
struct ReportJob {std::string url,body,token;};
static void* send_report(void* p){auto job=(ReportJob*)p;char response[512];http_request(HTTP_POST,job->url.c_str(),job->body.c_str(),job->token.c_str(),response,sizeof response);delete job;report_busy=false;return nullptr;}
static void flush_reports(){if(report_joinable){pthread_join(report_thread,nullptr);report_joinable=false;}}
void report_playback(const char* event,const Item& i,const std::string& session,const std::string& source,uint64_t ms,bool paused){
 bool live=!strcmp(i.base.type,"TvChannel");
 std::string body="{\"ItemId\":"+json_quote(i.base.id)+",\"MediaSourceId\":"+json_quote(source)+",\"PlaySessionId\":"+json_quote(session)+",\"PositionTicks\":"+std::to_string(live?0:ms*10000)+",\"IsPaused\":"+(paused?"true":"false")+",\"CanSeek\":"+(live?"false":"true")+",\"PlayMethod\":\""+(settings.direct&&!live?"DirectPlay":"Transcode")+"\"";
 if(!live_stream_id.empty())body+=",\"LiveStreamId\":"+json_quote(live_stream_id);
 body+="}";
 std::string url=std::string(g_server)+"/Sessions/Playing"+event;
 if(!strcmp(event,"/Progress")){
  if(report_busy)return;
  flush_reports();auto job=new ReportJob{url,body,g_token};report_busy=true;
  if(pthread_create(&report_thread,nullptr,send_report,job)==0)report_joinable=true;
  else{delete job;report_busy=false;plog("Unable to start progress report worker");}
 }else{flush_reports();char small[512];http_request(HTTP_POST,url.c_str(),body.c_str(),g_token,small,sizeof small);}
}
void end_playback(const std::string& session){flush_reports();char out[512];if(!session.empty())http_request(HTTP_DELETE,(std::string(g_server)+"/Videos/ActiveEncodings?DeviceId="+encode(device_id)+"&PlaySessionId="+encode(session)).c_str(),"",g_token,out,sizeof out);
 if(!live_stream_id.empty()){http_request(HTTP_POST,(std::string(g_server)+"/LiveStreams/Close?LiveStreamId="+encode(live_stream_id)).c_str(),"",g_token,out,sizeof out);live_stream_id.clear();}}


void app_shutdown(){ page_cache_stop(); flush_reports(); plog("Application workers stopped"); }
