#include "app.h"
#include "http_diagnostics.h"
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
static int pool=-1,ssl=-1,ctx=-1,tmpl=-1;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cancel_mutex=PTHREAD_MUTEX_INITIALIZER;
static bool cancel_scope=false,cancel_requested=false;
static int cancel_request=-1;
static std::string failure_detail;
void http_cancel_arm(){pthread_mutex_lock(&cancel_mutex);cancel_scope=true;cancel_requested=false;pthread_mutex_unlock(&cancel_mutex);}
void http_cancel_disarm(){pthread_mutex_lock(&cancel_mutex);cancel_scope=false;cancel_requested=false;cancel_request=-1;pthread_mutex_unlock(&cancel_mutex);}
void http_cancel_fire(){pthread_mutex_lock(&cancel_mutex);cancel_requested=true;if(cancel_request>=0)sceHttpAbortRequest(cancel_request);pthread_mutex_unlock(&cancel_mutex);}
std::string http_failure_detail(){pthread_mutex_lock(&mutex);auto detail=failure_detail;pthread_mutex_unlock(&mutex);return detail.empty()?"No HTTP response was received. Check the server URL and network connection.":detail;}
static void result_log(const char* operation,int result){char log[160];snprintf(log,sizeof log,"network: %s result=0x%08x",operation,(unsigned)result);plog(log);}
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
    failure_detail=detail;plog(detail);
}
static int perform(int method,const char* url,const char* body,const char* token,char* out,int cap,int* count,bool artwork=false){
    if(count)*count=0;
    if(!out||cap<1)return -1;
    out[0]=0;
    pthread_mutex_lock(&mutex);
    failure_detail.clear();
    if(tmpl<0||!url||(strncmp(url,"http://",7)&&strncmp(url,"https://",8))){failure_detail="HTTP is not initialized or the server URL is invalid.";pthread_mutex_unlock(&mutex);return -1;}
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
        std::string auth="MediaBrowser Client=\"JellyFin PS4\", Device=\"PS4\", DeviceId=\""+device_id+"\", Version=\"0.4.0\"";
        sceHttpAddRequestHeader(req,"Authorization",auth.c_str(),0);
        if(token&&*token)sceHttpAddRequestHeader(req,"X-Emby-Token",token,0);
        sceHttpAddRequestHeader(req,"Accept","application/json",0);
        sceHttpAddRequestHeader(req,"Accept-Encoding","identity",0);
        sceHttpAddRequestHeader(req,"Content-Type","application/json",0);
        int rc=cancelled?-1:sceHttpSendRequest(req,body,body?strlen(body):0);
        if(rc<0){if(cancelled)failure_detail="Request cancelled.";else failed("Send request",rc,req);}
        else{
            rc=sceHttpGetStatusCode(req,&status);
            if(rc<0){failed("Read status",rc,req);status=-1;}
            else{
                int used=0,n=0;
                while(used<cap-1){n=sceHttpReadData(req,out+used,cap-1-used);if(n<=0)break;used+=n;}
                out[used]=0;if(count)*count=used;
                if(n<0){failed("Read response",n,req);status=-1;}
                if(used==cap-1){char extra;int extra_count=sceHttpReadData(req,&extra,1);if(extra_count!=0){failure_detail=extra_count<0?"The response ended unexpectedly.":"The response exceeds the application buffer limit.";plog(failure_detail.c_str());status=-1;}}
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
    pthread_mutex_unlock(&mutex);return status;
}
int http_request(int method,const char* url,const char* body,const char* token,char* out,int cap){return perform(method,url,body,token,out,cap,nullptr);}
int http_fetch_binary(const char* url,const char* token,uint8_t* out,int cap){int count=0;int st=perform(HTTP_GET,url,nullptr,token,(char*)out,cap,&count,true);return st==200?count:-1;}

int http_context_id(){return ctx;}
int http_ssl_context_id(){return ssl;}
