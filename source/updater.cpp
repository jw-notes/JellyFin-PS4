#include "updater.h"
#include "thread_priority.h"
#include "app.h"
#include "json_lite.h"
#include <mbedtls/sha256.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <vector>
#ifndef JF_BUILD_VERSION
#define JF_BUILD_VERSION "host"
#endif

static std::atomic<int> state{UPD_IDLE},download{DL_IDLE};
static std::atomic<bool> cancel{false};
static std::atomic<uint64_t> got_bytes{0},total_bytes{0};
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static std::string version,error,asset_url,asset_sha256;
static uint64_t asset_size=0;
static pthread_t check_thread,download_thread;
static bool check_joinable=false,download_joinable=false;
static int check_delay=0;

// "v1.11", "01.10" -> {1, 11}, {1, 10}: the numbers, compared in order.
static std::vector<int> numbers(const std::string& v){
 std::vector<int> out;int n=-1;
 for(char c:v){if(c>='0'&&c<='9'){n=(n<0?0:n*10)+(c-'0');}else if(n>=0){out.push_back(n);n=-1;}}
 if(n>=0)out.push_back(n);return out;
}
static bool newer(const std::string& release,const std::string& running){
 const auto a=numbers(release),b=numbers(running);
 for(size_t i=0;i<std::max(a.size(),b.size());++i){const int x=i<a.size()?a[i]:0,y=i<b.size()?b[i]:0;if(x!=y)return x>y;}
 return false;
}
static void fail(const std::string& why){pthread_mutex_lock(&lock);error=why;pthread_mutex_unlock(&lock);plog(("Update: "+why).c_str());}

static void* check_worker(void*){
 background_priority();
 if(check_delay>0)usleep((useconds_t)check_delay*1000);
 std::vector<char> body(256*1024);
 const std::string url="https://api.github.com/repos/" UPDATE_REPO "/releases/latest";
 const int status=http_request(HTTP_GET,url.c_str(),"",nullptr,body.data(),(int)body.size());
 // No release yet (404) is not an error: this build is the newest there is.
 if(status==404){plog("Update: no release published yet");state=UPD_CURRENT;return nullptr;}
 if(status!=200){plog(("Update: GitHub answered HTTP "+std::to_string(status)+(status<0?" ("+http_failure_detail()+")":"")).c_str());
  fail(status<0?"Could not reach GitHub.":"GitHub did not answer the update check.");state=UPD_FAILED;return nullptr;}
 const char* j=body.data();const char* e=j+strlen(j);char tag[64]={};
 const char* t=find_top_value(j,e,"tag_name");if(!t||!read_json_string(t,e,tag,sizeof tag)||!tag[0]){fail("The release has no version.");state=UPD_FAILED;return nullptr;}
 // The package among the release's files, with its size and SHA-256 (GitHub
 // publishes a digest for each file it stores).
 std::string pkg_url,sha;uint64_t size=0;
 const char* assets=find_top_value(j,e,"assets");
 if(assets&&*assets=='['){const char* p=assets+1;
  while(p<e){p=skip_ws(p,e);if(p>=e||*p==']')break;if(*p==','){++p;continue;}const char* end=object_end(p,e);if(!end)break;
   char name[256]={},link[1024]={},digest[128]={};
   const char* v=find_top_value(p,end,"name");if(v)read_json_string(v,end,name,sizeof name);
   const size_t n=strlen(name);
   if(n>4&&!strcmp(name+n-4,".pkg")){
    if((v=find_top_value(p,end,"browser_download_url")))read_json_string(v,end,link,sizeof link);
    if((v=find_top_value(p,end,"size")))size=(uint64_t)read_json_int(v,end,0);
    if((v=find_top_value(p,end,"digest"))&&read_json_string(v,end,digest,sizeof digest)&&!strncmp(digest,"sha256:",7))sha=digest+7;
    pkg_url=link;break;}
   p=end;}}
 const std::string release=tag;
 plog(("Update: latest release "+release+", this build " JF_BUILD_VERSION+(pkg_url.empty()?", no .pkg attached":"")).c_str());
 if(!newer(release,JF_BUILD_VERSION)){state=UPD_CURRENT;return nullptr;}
 if(pkg_url.empty()){fail("The new release has no package to download.");state=UPD_FAILED;return nullptr;}
 pthread_mutex_lock(&lock);
 version=release[0]=='v'||release[0]=='V'?release.substr(1):release;asset_url=pkg_url;asset_size=size;asset_sha256=sha;
 pthread_mutex_unlock(&lock);
 state=UPD_AVAILABLE;return nullptr;
}
void update_check_start(int delay_ms){
 if(state==UPD_CHECKING)return;
 if(check_joinable){pthread_join(check_thread,nullptr);check_joinable=false;}
 check_delay=delay_ms;state=UPD_CHECKING;pthread_mutex_lock(&lock);error.clear();pthread_mutex_unlock(&lock);
 check_joinable=pthread_create(&check_thread,nullptr,check_worker,nullptr)==0;
 if(!check_joinable){fail("Could not start the update check.");state=UPD_FAILED;}
}
int update_state(){return state;}
std::string update_version(){pthread_mutex_lock(&lock);auto v=version;pthread_mutex_unlock(&lock);return v;}
std::string update_error(){pthread_mutex_lock(&lock);auto v=error;pthread_mutex_unlock(&lock);return v;}

static void* download_worker(void*){
 background_priority();
 pthread_mutex_lock(&lock);const std::string url=asset_url,sha=asset_sha256;const uint64_t size=asset_size;pthread_mutex_unlock(&lock);
 mkdir(UPDATE_DIR,0777);
 const std::string part=std::string(UPDATE_FILE)+".part";
 const int status=http_download_file(url.c_str(),part.c_str(),[&](uint64_t got,uint64_t total){got_bytes=got;total_bytes=total?total:size;return !cancel.load();});
 if(status!=200){if(!cancel)fail(http_failure_detail());remove(part.c_str());download=cancel?DL_IDLE:DL_FAILED;return nullptr;}
 struct stat st{};
 if(stat(part.c_str(),&st)||(size&&(uint64_t)st.st_size!=size)){fail("The download is the wrong size.");remove(part.c_str());download=DL_FAILED;return nullptr;}
 // The SHA-256 GitHub published for the file, against the file now on disk.
 if(!sha.empty()){
  mbedtls_sha256_context ctx;mbedtls_sha256_init(&ctx);mbedtls_sha256_starts_ret(&ctx,0);
  FILE* f=fopen(part.c_str(),"rb");std::vector<unsigned char> buf(256*1024);size_t n;
  while(f&&(n=fread(buf.data(),1,buf.size(),f))>0)mbedtls_sha256_update_ret(&ctx,buf.data(),n);
  if(f)fclose(f);unsigned char digest[32];mbedtls_sha256_finish_ret(&ctx,digest);mbedtls_sha256_free(&ctx);
  char hex[65];for(int i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",digest[i]);
  if(strcasecmp(hex,sha.c_str())){fail("The download did not match its checksum.");remove(part.c_str());download=DL_FAILED;return nullptr;}
 }
 remove(UPDATE_FILE);
 if(rename(part.c_str(),UPDATE_FILE)){fail("Could not save the package.");download=DL_FAILED;return nullptr;}
 plog("Update: package saved to " UPDATE_FILE);download=DL_DONE;return nullptr;
}
void update_download_start(){
 if(download==DL_RUNNING||state!=UPD_AVAILABLE)return;
 if(download_joinable){pthread_join(download_thread,nullptr);download_joinable=false;}
 cancel=false;got_bytes=0;total_bytes=0;download=DL_RUNNING;pthread_mutex_lock(&lock);error.clear();pthread_mutex_unlock(&lock);
 download_joinable=pthread_create(&download_thread,nullptr,download_worker,nullptr)==0;
 if(!download_joinable){fail("Could not start the download.");download=DL_FAILED;}
}
void update_download_cancel(){cancel=true;}
int update_download_state(){return download;}
void update_download_progress(uint64_t& got,uint64_t& total){got=got_bytes;total=total_bytes;}
void update_shutdown(){
 cancel=true;
 if(check_joinable){pthread_join(check_thread,nullptr);check_joinable=false;}
 if(download_joinable){pthread_join(download_thread,nullptr);download_joinable=false;}
}
