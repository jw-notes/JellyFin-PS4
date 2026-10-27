#include "app.h"
#include "http_diagnostics.h"
#include "mono_clock.h"
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#include <orbis/Http.h>
extern "C" int32_t sceSslInit(size_t);
extern "C" int32_t sceSslTerm(int32_t);
extern "C" int32_t bounded_http_receive(int32_t,uint32_t) __asm__("sceHttpSetRecvTimeOut");
#include <orbis/Sysmodule.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <map>
#include <functional>
#include <vector>
static int pool=-1,ssl=-1,ctx=-1,tmpl=-1;
// Requests run side by side: Home's rows, the artwork workers and the page
// prefetch each have their own connection. `mutex` guards only the template's
// lifetime: http_end() waits for the requests in flight before tearing down.
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t idle=PTHREAD_COND_INITIALIZER;
static int in_flight=0;
static pthread_mutex_t cancel_mutex=PTHREAD_MUTEX_INITIALIZER;
static bool cancel_scope=false,cancel_requested=false;
static int cancel_request=-1;
// Each thread reads the failure of its own last request.
static std::map<pthread_t,std::string> failure_details;
static void set_failure(const std::string& detail){pthread_mutex_lock(&mutex);failure_details[pthread_self()]=detail;pthread_mutex_unlock(&mutex);}
void http_cancel_arm(){pthread_mutex_lock(&cancel_mutex);cancel_scope=true;cancel_requested=false;pthread_mutex_unlock(&cancel_mutex);}
void http_cancel_disarm(){pthread_mutex_lock(&cancel_mutex);cancel_scope=false;cancel_requested=false;cancel_request=-1;pthread_mutex_unlock(&cancel_mutex);}
void http_cancel_fire(){pthread_mutex_lock(&cancel_mutex);cancel_requested=true;if(cancel_request>=0)sceHttpAbortRequest(cancel_request);pthread_mutex_unlock(&cancel_mutex);}
std::string http_failure_detail(){pthread_mutex_lock(&mutex);auto found=failure_details.find(pthread_self());std::string detail=found==failure_details.end()?"":found->second;pthread_mutex_unlock(&mutex);return detail.empty()?"No HTTP response was received. Check the server URL and network connection.":detail;}
static void result_log(const char* operation,int result){char log[160];snprintf(log,sizeof log,"network: %s result=0x%08x",operation,(unsigned)result);plog(log);}

// ---- name lookups for FFmpeg ----------------------------------------------------
// FFmpeg resolves a stream's host with the C library's getaddrinfo, whose DNS
// does not work in this app: every channel addressed by name failed ("Failed
// to resolve hostname ... System error"), and a live stream retrying it for
// every segment ran the process out of file descriptors within minutes. The
// linker sends the call here (--wrap=getaddrinfo): names are resolved by the
// console's own resolver -- the one the HTTP library uses -- and handed back
// through the C library as a numeric host. Numeric addresses, and the question
// "is this numeric?" (AI_NUMERICHOST), stay with the C library: FFmpeg's TLS
// asks it to decide whether to send the host name (SNI), and a server refuses
// the handshake without one.
static bool dotted(const char* s){unsigned a,b,c,d;char end;return sscanf(s,"%u.%u.%u.%u%c",&a,&b,&c,&d,&end)==4&&a<256&&b<256&&c<256&&d<256;}
extern "C" int __real_getaddrinfo(const char* node,const char* service,const struct addrinfo* hints,struct addrinfo** res);
extern "C" int __wrap_getaddrinfo(const char* node,const char* service,const struct addrinfo* hints,struct addrinfo** res){
    if(!node||!*node||pool<0||dotted(node)||(hints&&(hints->ai_flags&AI_NUMERICHOST))||!strcmp(node,"localhost"))return __real_getaddrinfo(node,service,hints,res);
    const int rc=EAI_FAIL;
    // A live stream asks for the same host with every segment: answers are kept
    // for five minutes, and each host is logged once.
    struct Known{uint32_t address;uint64_t until;};
    static std::map<std::string,Known> known;static pthread_mutex_t known_lock=PTHREAD_MUTEX_INITIALIZER;
    const uint64_t now=mono_us();uint32_t s_addr=0;bool fresh=false;
    pthread_mutex_lock(&known_lock);auto hit=known.find(node);if(hit!=known.end()&&hit->second.until>now)s_addr=hit->second.address;pthread_mutex_unlock(&known_lock);
    if(!s_addr){
        const OrbisNetId resolver=sceNetResolverCreate("jellyfin-dns",pool,0);if(resolver<0)return rc;
        OrbisNetInAddr address{};const int found=sceNetResolverStartNtoa(resolver,node,&address,5000000,2,0);sceNetResolverDestroy(resolver);
        if(found<0||!address.s_addr){char line[160];snprintf(line,sizeof line,"network: %s did not resolve (0x%08x)",node,(unsigned)found);plog(line);return rc;}
        s_addr=address.s_addr;
        pthread_mutex_lock(&known_lock);fresh=known.find(node)==known.end();if(known.size()>64)known.clear();known[node]={s_addr,now+300000000ull};pthread_mutex_unlock(&known_lock);
    }
    const unsigned char* b=(const unsigned char*)&s_addr;char ip[16];snprintf(ip,sizeof ip,"%u.%u.%u.%u",b[0],b[1],b[2],b[3]);
    struct addrinfo numeric{};if(hints)numeric=*hints;numeric.ai_family=AF_INET;numeric.ai_flags|=AI_NUMERICHOST;
    if(fresh)plog((std::string("network: resolved ")+node+" with the system resolver").c_str());
    return __real_getaddrinfo(ip,service,&numeric,res);
}
int http_init(){
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NETCTL);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP);
    result_log("NetInit",sceNetInit());
    result_log("NetCtlInit",sceNetCtlInit());
    OrbisNetCtlInfo info{};
    int state=sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS,&info);
    if(state>=0)plog((std::string("network: console IP=")+info.ip_address).c_str());else result_log("IP address",state);
    pool=sceNetPoolCreate("JellyFin PS4",2*1024*1024,0);if(pool<0){result_log("pool create",pool);return -1;}
    ssl=sceSslInit(2*1024*1024);if(ssl<0){result_log("SSL init",ssl);return -1;}
    ctx=sceHttpInit(pool,ssl,4*1024*1024);if(ctx<0){result_log("HTTP init",ctx);return -1;}
    // The PS3 client makes direct connections. Do the same for explicit server
    // URLs rather than inheriting the console browser's automatic proxy setup.
    tmpl=sceHttpCreateTemplate(ctx,"JellyFinPS4/0.4",2,0);if(tmpl<0){result_log("template create",tmpl);return -1;}
    sceHttpSetConnectTimeOut(tmpl,15000000);
    sceHttpSetResolveTimeOut(tmpl,10000000);
    sceHttpSetSendTimeOut(tmpl,30000000);
    return 0;
}
void http_end(){
    pthread_mutex_lock(&mutex);
    while(in_flight>0)pthread_cond_wait(&idle,&mutex);
    if(tmpl>=0){result_log("template delete",sceHttpDeleteTemplate(tmpl));tmpl=-1;}
    if(ctx>=0){result_log("HTTP terminate",sceHttpTerm(ctx));ctx=-1;}
    if(ssl>=0){result_log("SSL terminate",sceSslTerm(ssl));ssl=-1;}
    if(pool>=0){sceNetPoolDestroy(pool);pool=-1;plog("network: pool released");}
    pthread_mutex_unlock(&mutex);
}
static void failed(const char* stage,int code,int req=-1){
    char detail[256];int os_error=0;
    int rc=req>=0?sceHttpGetLastErrno(req,&os_error):-1;
    if(rc>=0&&os_error)snprintf(detail,sizeof detail,"%s failed (PS4 error 0x%08x, network error %d).",stage,(unsigned)code,os_error);
    else snprintf(detail,sizeof detail,"%s failed (PS4 error 0x%08x).",stage,(unsigned)code);
    set_failure(detail);plog(detail);
}
static int perform(int method,const char* url,const char* body,const char* token,char* out,int cap,int* count,bool artwork=false){
    if(count)*count=0;
    if(!out||cap<1)return -1;
    out[0]=0;
    set_failure("");
    pthread_mutex_lock(&mutex);
    if(tmpl<0||!url||(strncmp(url,"http://",7)&&strncmp(url,"https://",8))){failure_details[pthread_self()]="HTTP is not initialized or the server URL is invalid.";pthread_mutex_unlock(&mutex);return -1;}
    ++in_flight;pthread_mutex_unlock(&mutex);
    const char* verb=method==HTTP_POST?"POST":method==HTTP_DELETE?"DELETE":"GET";
    const std::string target=http_log_target(url);
    plog((std::string("HTTP ")+verb+" target="+target).c_str());
    int con=sceHttpCreateConnectionWithURL(tmpl,url,false),req=-1,status=-1;
    if(con<0)failed("Create connection",con);
    else{
        req=sceHttpCreateRequestWithURL2(con,verb,url,body?strlen(body):0);
        if(req<0)failed("Create request",req);
    }
    if(req>=0){
        if(artwork){sceHttpSetConnectTimeOut(req,4000000);sceHttpSetResolveTimeOut(req,2000000);sceHttpSetSendTimeOut(req,4000000);bounded_http_receive(req,4000000);}
        pthread_mutex_lock(&cancel_mutex);
        if(cancel_scope)cancel_request=req;
        const bool cancelled=cancel_scope&&cancel_requested;
        pthread_mutex_unlock(&cancel_mutex);
        // Jellyfin's sign-in header goes to the Jellyfin server only (any server
        // while one is still being chosen): GitHub answers it with 401.
        if(!g_server[0]||!strncmp(url,g_server,strlen(g_server))){
            std::string auth="MediaBrowser Client=\"JellyFin PS4\", Device=\"PS4\", DeviceId=\""+device_id+"\", Version=\"0.4.0\"";
            sceHttpAddRequestHeader(req,"Authorization",auth.c_str(),0);
        }
        if(token&&*token)sceHttpAddRequestHeader(req,"X-Emby-Token",token,0);
        sceHttpAddRequestHeader(req,"Accept","application/json",0);
        sceHttpAddRequestHeader(req,"Accept-Encoding","identity",0);
        sceHttpAddRequestHeader(req,"Content-Type","application/json",0);
        int rc=cancelled?-1:sceHttpSendRequest(req,body,body?strlen(body):0);
        if(rc<0){if(cancelled)set_failure("Request cancelled.");else failed("Send request",rc,req);}
        else{
            rc=sceHttpGetStatusCode(req,&status);
            if(rc<0){failed("Read status",rc,req);status=-1;}
            else{
                int used=0,n=0;
                while(used<cap-1){n=sceHttpReadData(req,out+used,cap-1-used);if(n<=0)break;used+=n;}
                out[used]=0;if(count)*count=used;
                if(n<0){failed("Read response",n,req);status=-1;}
                if(used==cap-1){char extra;int extra_count=sceHttpReadData(req,&extra,1);if(extra_count!=0){const char* why=extra_count<0?"The response ended unexpectedly.":"The response exceeds the application buffer limit.";set_failure(why);plog(why);status=-1;}}
            }
        }
        pthread_mutex_lock(&cancel_mutex);
        if(cancel_request==req)cancel_request=-1;
        sceHttpDeleteRequest(req);
        pthread_mutex_unlock(&cancel_mutex);
    }
    if(con>=0)sceHttpDeleteConnection(con);
    if(status==401&&token&&*token)g_auth_expired=true;
    plog((std::string("HTTP ")+verb+" status="+std::to_string(status)+" target="+target).c_str());
    pthread_mutex_lock(&mutex);if(--in_flight==0)pthread_cond_broadcast(&idle);pthread_mutex_unlock(&mutex);return status;
}
int http_request(int method,const char* url,const char* body,const char* token,char* out,int cap){return perform(method,url,body,token,out,cap,nullptr);}
int http_fetch_binary(const char* url,const char* token,uint8_t* out,int cap){int count=0;int st=perform(HTTP_GET,url,nullptr,token,(char*)out,cap,&count,true);return st==200?count:-1;}

// A large GET written to a file as it arrives (the update package): never
// held in memory. `progress` hears the bytes so far and the total (0 when the
// server does not say) and returns false to stop. Redirects (GitHub sends
// release downloads elsewhere) are followed by the HTTP library.
int http_download_file(const char* url,const char* path,const std::function<bool(uint64_t,uint64_t)>& progress){
    set_failure("");
    pthread_mutex_lock(&mutex);
    if(tmpl<0||!url||(strncmp(url,"http://",7)&&strncmp(url,"https://",8))){failure_details[pthread_self()]="HTTP is not initialized or the address is invalid.";pthread_mutex_unlock(&mutex);return -1;}
    ++in_flight;pthread_mutex_unlock(&mutex);
    const std::string target=http_log_target(url);plog((std::string("HTTP download target=")+target).c_str());
    int con=sceHttpCreateConnectionWithURL(tmpl,url,false),req=-1,status=-1;
    if(con<0)failed("Create connection",con);
    else{req=sceHttpCreateRequestWithURL2(con,"GET",url,0);if(req<0)failed("Create request",req);}
    if(req>=0){
        sceHttpAddRequestHeader(req,"Accept","application/octet-stream",0);
        int rc=sceHttpSendRequest(req,nullptr,0);
        if(rc<0)failed("Send request",rc,req);
        else if((rc=sceHttpGetStatusCode(req,&status))<0){failed("Read status",rc,req);status=-1;}
        else if(status==200){
            int32_t result=0;size_t length=0;uint64_t total=sceHttpGetResponseContentLength(req,&result,&length)>=0&&result==0?length:0,got=0;
            FILE* out=fopen(path,"wb");
            if(!out){set_failure(std::string("Could not write ")+path+".");status=-1;}
            else{
                std::vector<char> buffer(256*1024);int n=0;bool stopped=false;
                while((n=sceHttpReadData(req,buffer.data(),buffer.size()))>0){
                    if(fwrite(buffer.data(),1,n,out)!=(size_t)n){set_failure(std::string("Could not write ")+path+" (is the drive full?).");status=-1;break;}
                    got+=n;if(!progress(got,total)){stopped=true;set_failure("Cancelled.");status=-1;break;}
                }
                if(n<0&&!stopped){failed("Read response",n,req);status=-1;}
                if(fclose(out))status=-1;
                if(status==200&&total&&got!=total){set_failure("The download ended early.");status=-1;}
            }
        }
        sceHttpDeleteRequest(req);
    }
    if(con>=0)sceHttpDeleteConnection(con);
    plog((std::string("HTTP download status=")+std::to_string(status)+" target="+target).c_str());
    pthread_mutex_lock(&mutex);if(--in_flight==0)pthread_cond_broadcast(&idle);pthread_mutex_unlock(&mutex);return status;
}
int http_context_id(){return ctx;}
int http_ssl_context_id(){return ssl;}
