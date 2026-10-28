#include "app.h"
#include "thread_priority.h"
#include "auth.h"
#include "playback_model.h"
#include "api_userdata.h"
#include "json_unescape.h"
#include <stdio.h>
#include <ctype.h>
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
static bool last_static=false,last_remote=false,force_server=false;
bool playback_is_static(){return last_static;}
bool playback_is_remote(){return last_remote;}
void playback_force_server_once(){force_server=true;}
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
    // Settings > System > Debug Logging turns the log off.
    if(data_dir.empty()||!settings.debug_log)return;
    append_log(data_dir+"/app.log",message);
    // Only diagnostic messages reach this file: no passwords, tokens or
    // Quick Connect secrets are logged. The private session folder stays 0700.
    if(!diagnostic_log_path.empty())append_log(diagnostic_log_path,message,true);
}
void crash_log(const char* m){plog(m);}
static bool write_atomic(const char* name,const std::string& text){std::string dest=jf_data_path(name),tmp=dest+".tmp";FILE*f=fopen(tmp.c_str(),"wb");if(!f)return false;chmod(tmp.c_str(),0600);bool ok=fwrite(text.data(),1,text.size(),f)==text.size();if(fflush(f)||fsync(fileno(f)))ok=false;if(fclose(f))ok=false;return ok&&rename(tmp.c_str(),dest.c_str())==0;}
static std::string read_file(const char* n){FILE*f=fopen(jf_data_path(n),"rb");if(!f)return {};char b[8192];size_t z=fread(b,1,sizeof(b)-1,f);fclose(f);b[z]=0;return b;}
static int storage_user=0;   // the PS4 user whose folder this is (Share Sign-in)
bool storage_init(int user){
 storage_user=user;
#ifdef HOST_TEST
 const char* base=getenv("JF_TEST_DATA");data_dir=base?base:"/tmp/jellyfin-ps4-test";mkdir(data_dir.c_str(),0700);
#else
 mkdir("/data/JellyFin-PS4",0700);char dir[128];snprintf(dir,sizeof dir,"/data/JellyFin-PS4/user-%08x",user);data_dir=dir;mkdir(data_dir.c_str(),0700);
 if(mkdir("/data/JellyFin-PS4-logs",0755)==0)chmod("/data/JellyFin-PS4-logs",0755);
 char log_path[160];snprintf(log_path,sizeof log_path,"/data/JellyFin-PS4-logs/user-%08x.log",user);
 diagnostic_log_path=log_path;
#endif
 device_id=read_file("device-id");if(device_id.empty()){char id[80];snprintf(id,sizeof id,"ps4-%08x-%llx-%llx",user,(long long)time(nullptr),(long long)clock());device_id=id;if(!write_atomic("device-id",device_id))return false;}
 auto j=read_file("settings.json");if(!j.empty()){auto b=number(j.c_str(),"bitrate");if(b>=1000000&&b<=20000000)settings.bitrate=(int)b;if(j.find("\"resume\"")!=std::string::npos)settings.resume=number(j.c_str(),"resume")!=0;
  // Settings older than 1.11 ("v" < 11) had Resume off by default: it is on now, once.
  if(number(j.c_str(),"v")<11)settings.resume=true;if(j.find("\"method\"")!=std::string::npos){auto m=number(j.c_str(),"method");settings.method=m<=PLAY_TRANSCODE?(int)m:PLAY_AUTO;}else settings.method=number(j.c_str(),"direct")?PLAY_DIRECT_FILE:PLAY_AUTO;if(j.find("\"saver\"")!=std::string::npos)settings.saver=number(j.c_str(),"saver")!=0;auto delay=number(j.c_str(),"saver_delay");if(delay>=30&&delay<=3600)settings.saver_delay=(int)delay;auto language=field(j.c_str(),"language");if(!language.empty())settings.language=language;if(j.find("\"hd1080\"")!=std::string::npos)settings.hd1080=number(j.c_str(),"hd1080")!=0;
  if(j.find("\"debug\"")!=std::string::npos)settings.debug_log=number(j.c_str(),"debug")!=0;
  settings.viz=(int)(number(j.c_str(),"viz")%5);
  settings.home_order=field(j.c_str(),"home_order");
  if(j.find("\"guide_auto\"")!=std::string::npos)settings.guide_auto=number(j.c_str(),"guide_auto")!=0;
  settings.guide_url=field(j.c_str(),"guide_url");settings.guide_found=field(j.c_str(),"guide_found");
  if(j.find("\"skip_intro\"")!=std::string::npos)settings.skip_intro=(int)number(j.c_str(),"skip_intro")%3;
  if(j.find("\"skip_outro\"")!=std::string::npos)settings.skip_outro=(int)number(j.c_str(),"skip_outro")%3;
  if(j.find("\"volume\"")!=std::string::npos)settings.volume=std::max(0,std::min(100,(int)number(j.c_str(),"volume")));
  if(j.find("\"pic_w\"")!=std::string::npos){settings.pic_aspect=(int)number(j.c_str(),"pic_aspect")%3;settings.pic_w=std::max(50,std::min(110,(int)number(j.c_str(),"pic_w")));settings.pic_h=std::max(50,std::min(110,(int)number(j.c_str(),"pic_h")));
   // Shifts are stored offset by 100: the JSON reader here takes no minus sign.
   settings.pic_x=std::max(-40,std::min(40,(int)number(j.c_str(),"pic_x")-100));settings.pic_y=std::max(-40,std::min(40,(int)number(j.c_str(),"pic_y")-100));}}return true;}
bool save_settings(){return write_atomic("settings.json","{\"v\":11,\"bitrate\":"+std::to_string(settings.bitrate)+",\"resume\":"+std::to_string(settings.resume)+",\"method\":"+std::to_string(settings.method)+",\"saver\":"+std::to_string(settings.saver)+",\"saver_delay\":"+std::to_string(settings.saver_delay)+",\"language\":"+json_quote(settings.language)+",\"hd1080\":"+std::to_string(settings.hd1080)+",\"debug\":"+std::to_string(settings.debug_log)+",\"pic_aspect\":"+std::to_string(settings.pic_aspect)+",\"pic_w\":"+std::to_string(settings.pic_w)+",\"pic_h\":"+std::to_string(settings.pic_h)+",\"pic_x\":"+std::to_string(settings.pic_x+100)+",\"pic_y\":"+std::to_string(settings.pic_y+100)+",\"viz\":"+std::to_string(settings.viz)+",\"volume\":"+std::to_string(settings.volume)+",\"home_order\":"+json_quote(settings.home_order)+",\"skip_intro\":"+std::to_string(settings.skip_intro)+",\"skip_outro\":"+std::to_string(settings.skip_outro)+",\"guide_auto\":"+std::to_string(settings.guide_auto)+",\"guide_url\":"+json_quote(settings.guide_url)+",\"guide_found\":"+json_quote(settings.guide_found)+"}");}
// Share Sign-in (PS3): one PS4 user lends their sign-in to the console's
// users with none of their own. It is a copy of their session beside the
// users' folders, with whose it is; theirs keeps it up to date.
static const char* SHARED_SESSION="../shared-session.json";
bool g_session_shared=false;
static std::string session_text(int owner){return "{\"server\":"+json_quote(g_server)+",\"username\":"+json_quote(g_username)+",\"token\":"+json_quote(g_token)+",\"userid\":"+json_quote(g_userid)+(owner?",\"owner\":"+std::to_string(owner):"")+"}";}
static bool apply_session(const std::string& j){snprintf(g_server,sizeof g_server,"%s",field(j.c_str(),"server").c_str());snprintf(g_username,sizeof g_username,"%s",field(j.c_str(),"username").c_str());snprintf(g_token,sizeof g_token,"%s",field(j.c_str(),"token").c_str());snprintf(g_userid,sizeof g_userid,"%s",field(j.c_str(),"userid").c_str());return g_token[0]&&g_server[0]&&g_userid[0];}
static int shared_owner(){auto j=read_file(SHARED_SESSION);return j.empty()?0:(int)number(j.c_str(),"owner");}
int share_state(){if(g_session_shared)return SHARE_USING;const int owner=shared_owner();return !owner?SHARE_OFF:owner==storage_user?SHARE_MINE:SHARE_OTHER;}
bool share_set(bool on){
 const int state=share_state();if(state==SHARE_USING||state==SHARE_OTHER)return false;
 if(!on){remove(jf_data_path(SHARED_SESSION));plog("Account: sign-in no longer shared");return true;}
 if(!g_token[0])return false;plog("Account: sign-in shared with this console's users");return write_atomic(SHARED_SESSION,session_text(storage_user));
}
bool save_session(){
 // Using someone else's: nothing of theirs is written to this user's folder.
 if(g_session_shared)return true;
 // Shared, and signed in again (a new token): the copy follows.
 if(shared_owner()==storage_user){if(g_token[0])write_atomic(SHARED_SESSION,session_text(storage_user));else remove(jf_data_path(SHARED_SESSION));}
 return write_atomic("session.json",session_text(0));
}
// A user who logged out of a shared sign-in has said no to it: "no-shared"
// in their folder, until they choose it again on the sign-in screen.
std::string shared_name(){
 if(!shared_owner()||shared_owner()==storage_user)return "";
 auto j=read_file(SHARED_SESSION);return field(j.c_str(),"token").empty()?"":field(j.c_str(),"username");
}
bool share_use(){
 if(shared_name().empty()||!apply_session(read_file(SHARED_SESSION)))return false;
 g_session_shared=true;remove(jf_data_path("no-shared"));plog("Account: using the console's shared sign-in");return true;
}
bool load_session(){
 g_session_shared=false;if(apply_session(read_file("session.json")))return true;
 // No sign-in of this user's own: one shared on the console, if any (and not refused).
 if(read_file("no-shared").empty())return share_use();
 return false;
}
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
// A shared sign-in is only put down here, not logged out: it is someone else's.
void logout(){if(g_session_shared){g_session_shared=false;g_token[0]=g_userid[0]=0;g_auth_expired=false;write_atomic("no-shared","1");plog("Account: stopped using the shared sign-in");return;}request(HTTP_POST,"/Sessions/Logout");g_token[0]=g_userid[0]=0;save_session();g_auth_expired=false;}
static std::string first_string(const char* j,const char* e,const char* k){auto a=find_top_value(j,e,k);char b[512]={};if(a&&*a=='[')read_json_string(skip_ws(a+1,e),e,b,sizeof b);return b;}
static std::string decimal(const char* j,const char* e,const char* k){auto p=find_top_value(j,e,k);if(!p||(*p!='-'&&(*p<'0'||*p>'9')))return {};char b[16];snprintf(b,sizeof b,"%.1f",strtod(p,nullptr));return b;}
static Item parse_item(const char* j){Item i;snprintf(i.base.id,sizeof i.base.id,"%s",field(j,"Id").c_str());snprintf(i.base.name,sizeof i.base.name,"%s",field(j,"Name").c_str());snprintf(i.base.type,sizeof i.base.type,"%s",field(j,"Type").c_str());i.overview=field(j,"Overview");i.duration=number(j,"RunTimeTicks")/10000;auto e=j+strlen(j);auto u=find_top_value(j,e,"UserData");if(u){i.resume=number(u,"PlaybackPositionTicks")/10000;auto f=find_top_value(u,e,"IsFavorite");i.favourite=f&&read_json_bool(f,e);f=find_top_value(u,e,"Played");i.played=f&&read_json_bool(f,e);}
 i.collection=field(j,"CollectionType");i.year=(int)number(j,"ProductionYear");i.rating=field(j,"OfficialRating");i.community=decimal(j,e,"CommunityRating");
 i.series=field(j,"SeriesName");i.series_id=field(j,"SeriesId");i.season=(int)number(j,"ParentIndexNumber");i.episode=(int)number(j,"IndexNumber");i.tagline=first_string(j,e,"Taglines");
 i.album=field(j,"Album");i.album_id=field(j,"AlbumId");i.artist=field(j,"AlbumArtist");if(i.artist.empty())i.artist=first_string(j,e,"Artists");
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
 // Film, show and music libraries list their films / series / albums
 // wherever they sit in its folders, as Jellyfin's own clients do. The sort
 // ends on DateCreated so items with the same name keep their order between
 // pages: with SortName alone the server could put a film on two pages.
 else if(mode==MODE_MOVIES||mode==MODE_SHOWS||mode==MODE_MUSIC)path+="&ParentId="+encode(parent)+"&Recursive=true&IncludeItemTypes="+(mode==MODE_MOVIES?"Movie":mode==MODE_SHOWS?"Series":"MusicAlbum")+"&SortBy=SortName,ProductionYear,DateCreated&SortOrder=Ascending";
 // An album's tracks by album, not folder: tracks in disc folders (CD1, CD2)
 // or elsewhere left a folder listing empty.
 else if(mode==MODE_ALBUM)path+="&AlbumIds="+encode(parent)+"&Recursive=true&IncludeItemTypes=Audio&SortBy=ParentIndexNumber,IndexNumber,SortName&SortOrder=Ascending";
 else if(!parent.empty())path+="&ParentId="+encode(parent)+"&SortBy=SortName,DateCreated&SortOrder=Ascending";
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
 background_priority();
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
 // Home loads its rows side by side, so each uses its own buffer and error.
 std::vector<char> body(1024*1024);std::string error;
 if(http_request(HTTP_GET,(std::string(g_server)+path).c_str(),"",g_token,body.data(),(int)body.size())!=200)return false;
 Page next;if(!parse_items(body.data(),next,error))return false;page=next;return true;
}
bool fetch_album_tracks(const std::string& album_id,Page& page){
 const std::string path="/Users/"+encode(g_userid)+"/Items?AlbumIds="+encode(album_id)+"&IncludeItemTypes=Audio&Recursive=true&SortBy=ParentIndexNumber,IndexNumber,SortName&SortOrder=Ascending&Limit=60";
 if(request(HTTP_GET,path)!=200)return false;Page next;if(!parse_items(next))return false;page=next;return true;
}
int segments_supported(){
 static std::string asked;static int known=-1;
 const std::string who=std::string(g_server)+"|"+g_userid;if(asked==who)return known;
 known=-1;
 // Media segments arrived in 10.10.
 if(request(HTTP_GET,"/System/Info/Public")==200){const std::string v=field(responseBuffer,"Version");int major=0,minor=0;
  if(sscanf(v.c_str(),"%d.%d",&major,&minor)==2&&(major<10||(major==10&&minor<10))){known=0;asked=who;plog(("Segments: server "+v+" is older than 10.10").c_str());return known;}}
 const int status=request(HTTP_GET,"/Plugins");
 if(status==200){
  std::string all=responseBuffer;for(char& c:all)c=(char)tolower((unsigned char)c);
  // Any plugin that makes intro / credits segments: Intro Skipper, Chapter
  // Segments, EDL / segment providers.
  known=all.find("intro skipper")!=std::string::npos||all.find("introskipper")!=std::string::npos||all.find("segment")!=std::string::npos?1:0;
 }
 asked=who;plog(known==1?"Segments: a segment plugin is installed":known==0?"Segments: no segment plugin on the server":"Segments: can't tell (plugin list needs an admin account)");
 return known;
}
std::vector<MediaSegment> fetch_segments(const std::string& item_id){
 std::vector<MediaSegment> out;
 if(request(HTTP_GET,"/MediaSegments/"+encode(item_id)+"?includeSegmentTypes=Intro&includeSegmentTypes=Outro")!=200)return out;
 const char* j=responseBuffer;const char* e=j+strlen(j);auto a=find_top_value(j,e,"Items");if(!a||*a!='[')return out;++a;
 while(a<e){a=skip_ws(a,e);if(a>=e||*a==']')break;if(*a==','){++a;continue;}auto b=object_end(a,e);if(!b)break;
  const std::string obj(a,b);const std::string type=field(obj.c_str(),"Type");
  // Ticks are 100 ns.
  MediaSegment s;s.outro=type=="Outro";s.start=number(obj.c_str(),"StartTicks")/10000;s.end=number(obj.c_str(),"EndTicks")/10000;
  if((type=="Intro"||type=="Outro")&&s.end>s.start+1000)out.push_back(s);a=b;}
 return out;
}
bool fetch_adjacent_episodes(const Item& episode,Item& prev,Item& next,bool& has_prev,bool& has_next){
 has_prev=has_next=false;if(episode.series_id.empty())return false;
 // AdjacentTo: the episode and its neighbours in the whole series' order.
 const std::string path="/Shows/"+encode(episode.series_id)+"/Episodes?userId="+encode(g_userid)+"&AdjacentTo="+encode(episode.base.id)+"&Fields=Overview";
 if(request(HTTP_GET,path)!=200)return false;Page page;if(!parse_items(page))return false;
 int at=-1;for(size_t i=0;i<page.items.size();++i)if(!strcmp(page.items[i].base.id,episode.base.id))at=(int)i;
 if(at<0)return false;
 if(at>0){prev=page.items[at-1];has_prev=true;}
 if(at+1<(int)page.items.size()){next=page.items[at+1];has_next=true;}
 return true;
}
bool fetch_detail(const Item& i,Item& detail){if(request(HTTP_GET,"/Users/"+encode(g_userid)+"/Items/"+encode(i.base.id))!=200)return false;detail=parse_item(responseBuffer);return detail.base.id[0];}
bool parse_preview_item(const char* json,Item& result){result=parse_item(json);return result.base.id[0];}
bool playback_url(const Item& i,int audio,int sub,const std::string& selected_source,std::string& url,std::string& session,std::string& source_id){
 // 720p by default; 1080p (Settings > Playback) lifts the profile's ceiling and the URL's.
 const std::string max_w=settings.hd1080?"1920":"1280",max_h=settings.hd1080?"1080":"720";
 const std::string profile="{\"Name\":\"JellyFin PS4\",\"MaxStreamingBitrate\":"+std::to_string(settings.bitrate)+",\"DirectPlayProfiles\":[{\"Container\":\"mp4,m4v,mkv,mov\",\"Type\":\"Video\",\"VideoCodec\":\"h264\",\"AudioCodec\":\"aac,mp3,ac3,eac3,flac,opus,vorbis\"},{\"Container\":\"mp3,flac,m4a,mp4,aac,ogg,oga,opus,wav,webma,webm\",\"Type\":\"Audio\"}],\"TranscodingProfiles\":[{\"Container\":\"ts\",\"Type\":\"Video\",\"VideoCodec\":\"h264\",\"AudioCodec\":\"aac\",\"Protocol\":\"hls\",\"Context\":\"Streaming\",\"MaxAudioChannels\":\"2\",\"MinSegments\":\"2\",\"BreakOnNonKeyFrames\":true},{\"Container\":\"mp4\",\"Type\":\"Audio\",\"AudioCodec\":\"aac\",\"Protocol\":\"http\",\"Context\":\"Streaming\"}],\"CodecProfiles\":[{\"Type\":\"Video\",\"Codec\":\"h264\",\"Conditions\":[{\"Condition\":\"LessThanEqual\",\"Property\":\"VideoBitDepth\",\"Value\":\"8\",\"IsRequired\":true},{\"Condition\":\"LessThanEqual\",\"Property\":\"Width\",\"Value\":\""+max_w+"\",\"IsRequired\":true},{\"Condition\":\"LessThanEqual\",\"Property\":\"Height\",\"Value\":\""+max_h+"\",\"IsRequired\":true}]},{\"Type\":\"VideoAudio\",\"Codec\":\"aac\",\"Conditions\":[{\"Condition\":\"Equals\",\"Property\":\"AudioSampleRate\",\"Value\":\"48000\",\"IsRequired\":true}]}],\"SubtitleProfiles\":[{\"Format\":\"srt\",\"Method\":\"Encode\"},{\"Format\":\"ass\",\"Method\":\"Encode\"},{\"Format\":\"pgssub\",\"Method\":\"Encode\"}]}";
 const bool live=!strcmp(i.base.type,"TvChannel");
 live_stream_id.clear();session.clear();source_id.clear();url.clear();last_static=last_remote=false;
 // Live TV: most channels are a plain HTTP stream (an M3U tuner) the player
 // can open itself -- no server transcode to start, and the provider's best
 // variant rather than whichever it lists first. The source is then asked
 // for unopened, so the server starts nothing; other tuners are opened below.
 const bool live_direct=!strcmp(i.base.type,"TvChannel")&&settings.method!=PLAY_TRANSCODE&&!force_server;force_server=false;
 // Auto: the server may send a file the player decodes itself as it is
 // (direct play: no transcode to wait for), or copy its H264 video and
 // convert only the audio (direct stream). Not with a chosen audio track or
 // burned-in subtitles, which only the server can apply, nor for Live TV.
 const bool may_direct=!live&&settings.method==PLAY_AUTO&&audio<0&&sub<0;
 const bool may_copy=!live&&settings.method!=PLAY_TRANSCODE;
 std::string body="{\"UserId\":"+json_quote(g_userid)+",\"ItemId\":"+json_quote(i.base.id)+",\"DeviceProfile\":"+profile+",\"EnableDirectPlay\":"+std::string(may_direct?"true":"false")+",\"EnableDirectStream\":"+(may_copy?"true":"false")+",\"EnableTranscoding\":true,\"AutoOpenLiveStream\":"+std::string(live_direct?"false":"true")+",\"MaxAudioChannels\":2,\"MaxStreamingBitrate\":"+std::to_string(settings.bitrate)+",\"StartTimeTicks\":"+std::to_string(!live&&settings.resume?i.resume*10000:0)+",\"SubtitleStreamIndex\":"+std::to_string(sub);
 if(!selected_source.empty())body+=",\"MediaSourceId\":"+json_quote(selected_source);
 if(audio>=0)body+=",\"AudioStreamIndex\":"+std::to_string(audio);
 body+="}";
 const std::string endpoint="/Items/"+encode(i.base.id)+"/PlaybackInfo?UserId="+encode(g_userid)+"&AutoOpenLiveStream="+(live_direct?"false":"true")+"&IsPlayback=true";
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
 if(live_direct){
  const std::string path=field(source.c_str(),"Path"),protocol=field(source.c_str(),"Protocol");
  if(protocol=="Http"&&(path.rfind("http://",0)==0||path.rfind("https://",0)==0)){
   source_id=field(source.c_str(),"Id");url=path;last_remote=true;
   // Which provider, not the whole address: enough to tell failing hosts apart.
   const size_t host_end=path.find('/',path.find("//")+2);
   plog(("Live TV: playing the channel's own stream directly from "+path.substr(0,host_end==std::string::npos?path.size():host_end)).c_str());return true;
  }
  plog(("Live TV: source protocol "+(protocol.empty()?std::string("unknown"):protocol)+"; through the server").c_str());
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
 auto direct_play=find_top_value(source.c_str(),end,"SupportsDirectPlay");
 const bool server_direct=may_direct&&url.empty()&&direct_play&&read_json_bool(direct_play,end);
 if((settings.method==PLAY_DIRECT_FILE||server_direct)&&!live){last_static=true;plog(server_direct?"Playback: direct play (the original file)":"Playback: direct file (setting)");url=std::string(g_server)+(strcmp(i.base.type,"Audio")==0?"/Audio/":"/Videos/")+encode(i.base.id)+"/stream?Static=true&MediaSourceId="+encode(source_id)+"&api_key="+encode(g_token);}
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
 if(!last_static){url+=(url.find('?')==std::string::npos?"?":"&");url+="MaxWidth="+max_w+"&MaxHeight="+max_h+"&AudioSampleRate=48000&MaxAudioChannels=2";}
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
 std::string body="{\"ItemId\":"+json_quote(i.base.id)+",\"MediaSourceId\":"+json_quote(source)+",\"PlaySessionId\":"+json_quote(session)+",\"PositionTicks\":"+std::to_string(live?0:ms*10000)+",\"IsPaused\":"+(paused?"true":"false")+",\"CanSeek\":"+(live?"false":"true")+",\"PlayMethod\":\""+(last_static&&!live?"DirectPlay":"Transcode")+"\"";
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
void finish_playback(const Item& i,const std::string& session,const std::string& source,uint64_t ms,bool paused){
 flush_reports();
 const bool live=!strcmp(i.base.type,"TvChannel");
 struct Job{std::string stopped_url,stopped_body,encodings_url,close_url,token;};
 auto job=new Job;job->token=g_token;
 job->stopped_url=std::string(g_server)+"/Sessions/Playing/Stopped";
 job->stopped_body="{\"ItemId\":"+json_quote(i.base.id)+",\"MediaSourceId\":"+json_quote(source)+",\"PlaySessionId\":"+json_quote(session)+",\"PositionTicks\":"+std::to_string(live?0:ms*10000)+",\"IsPaused\":"+(paused?"true":"false")+(live_stream_id.empty()?"":",\"LiveStreamId\":"+json_quote(live_stream_id))+"}";
 if(!session.empty())job->encodings_url=std::string(g_server)+"/Videos/ActiveEncodings?DeviceId="+encode(device_id)+"&PlaySessionId="+encode(session);
 if(!live_stream_id.empty())job->close_url=std::string(g_server)+"/LiveStreams/Close?LiveStreamId="+encode(live_stream_id);
 live_stream_id.clear();
 pthread_t thread;
 if(pthread_create(&thread,nullptr,[](void* p)->void*{auto j=(Job*)p;char out[512];
   http_request(HTTP_POST,j->stopped_url.c_str(),j->stopped_body.c_str(),j->token.c_str(),out,sizeof out);
   if(!j->encodings_url.empty())http_request(HTTP_DELETE,j->encodings_url.c_str(),"",j->token.c_str(),out,sizeof out);
   if(!j->close_url.empty())http_request(HTTP_POST,j->close_url.c_str(),"",j->token.c_str(),out,sizeof out);
   delete j;return nullptr;},job)==0)pthread_detach(thread);
 else{char out[512];http_request(HTTP_POST,job->stopped_url.c_str(),job->stopped_body.c_str(),job->token.c_str(),out,sizeof out);delete job;end_playback(session);}
}
void end_playback(const std::string& session){flush_reports();char out[512];if(!session.empty())http_request(HTTP_DELETE,(std::string(g_server)+"/Videos/ActiveEncodings?DeviceId="+encode(device_id)+"&PlaySessionId="+encode(session)).c_str(),"",g_token,out,sizeof out);
 if(!live_stream_id.empty()){http_request(HTTP_POST,(std::string(g_server)+"/LiveStreams/Close?LiveStreamId="+encode(live_stream_id)).c_str(),"",g_token,out,sizeof out);live_stream_id.clear();}}


void app_shutdown(){ page_cache_stop(); flush_reports(); plog("Application workers stopped"); }
