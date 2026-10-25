from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/http_ps4.cpp';t=f.read_text();t=t.replace('static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;', '''static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cancel_mutex=PTHREAD_MUTEX_INITIALIZER;
static bool cancel_scope=false, cancel_requested=false;
static int cancel_request=-1;
void http_cancel_arm(){pthread_mutex_lock(&cancel_mutex);cancel_scope=true;cancel_requested=false;pthread_mutex_unlock(&cancel_mutex);}
void http_cancel_disarm(){pthread_mutex_lock(&cancel_mutex);cancel_scope=false;cancel_requested=false;cancel_request=-1;pthread_mutex_unlock(&cancel_mutex);}
void http_cancel_fire(){pthread_mutex_lock(&cancel_mutex);cancel_requested=true;if(cancel_request>=0)sceHttpAbortRequest(cancel_request);pthread_mutex_unlock(&cancel_mutex);}
''')
t=t.replace('JellyFinPS4/0.1','JellyFinPS4/0.2').replace('Version=\\"0.1.0\\"','Version=\\"0.2.0\\"')
a=t.index('void http_end()');b=t.index('\nstatic int perform',a)
t=t[:a]+'''static void http_cleanup_log(const char* operation,int result){char log[128];snprintf(log,sizeof log,"shutdown: %s result=0x%08x",operation,(unsigned)result);plog(log);}
void http_end(){
 pthread_mutex_lock(&mutex);
 if(tmpl>=0){http_cleanup_log("HTTP template",sceHttpDeleteTemplate(tmpl));tmpl=-1;}
 if(ctx>=0){http_cleanup_log("HTTP context",sceHttpTerm(ctx));ctx=-1;}
 if(ssl>=0){http_cleanup_log("SSL",sceSslTerm(ssl));ssl=-1;}
 if(pool>=0){sceNetPoolDestroy(pool);pool=-1;plog("shutdown: network pool released");}
 pthread_mutex_unlock(&mutex);
}'''+t[b:]
t=t.replace('if(req>=0){\n  std::string auth=', '''if(req>=0){
  pthread_mutex_lock(&cancel_mutex);
  if(cancel_scope)cancel_request=req;
  const bool cancelled=cancel_scope&&cancel_requested;
  pthread_mutex_unlock(&cancel_mutex);
  std::string auth=''')
t=t.replace('int rc=sceHttpSendRequest(req,body,body?strlen(body):0);','int rc=cancelled?-1:sceHttpSendRequest(req,body,body?strlen(body):0);')
t=t.replace('  sceHttpDeleteRequest(req);','  pthread_mutex_lock(&cancel_mutex);\n  if(cancel_request==req)cancel_request=-1;\n  sceHttpDeleteRequest(req);\n  pthread_mutex_unlock(&cancel_mutex);')
f.write_text(t,newline='\n')
f=p/'source/platform.h';t=f.read_text().replace('void platform_shutdown();','void platform_shutdown();\n[[noreturn]] void platform_exit(int status);');f.write_text(t,newline='\n')
f=p/'source/platform_ps4.cpp';t=f.read_text().replace('#include <orbis/Sysmodule.h>','#include <orbis/Sysmodule.h>\n#include <orbis/SystemService.h>\n#include <stdio.h>')
t=t.replace('static OrbisKernelEqueue flip_queue;', 'static OrbisKernelEqueue flip_queue;\nstatic bool queue_created=false,buffers_registered=false,memory_allocated=false;')
t=t.replace(' sceUserServiceInitialize(nullptr);',' sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);\n sceUserServiceInitialize(nullptr);')
t=t.replace(' if(sceKernelMapDirectMemory', ' memory_allocated=true;\n if(sceKernelMapDirectMemory')
t=t.replace(' if(sceKernelCreateEqueue(&flip_queue,"JellyFin flip")<0||sceVideoOutAddFlipEvent(flip_queue,video,nullptr)<0)return false;', ''' buffers_registered=true;
 if(sceKernelCreateEqueue(&flip_queue,"JellyFin flip")<0)return false;
 queue_created=true;
 if(sceVideoOutAddFlipEvent(flip_queue,video,nullptr)<0)return false;''')
a=t.index('void platform_shutdown()');b=t.index('\nuint32_t buttons()',a)
t=t[:a]+'''static void shutdown_result(const char* operation,int result){char log[128];snprintf(log,sizeof log,"shutdown: %s result=0x%08x",operation,(unsigned)result);plog(log);}
void platform_shutdown(){
 plog("shutdown: releasing controller and display");
 if(pad>=0){shutdown_result("controller close",scePadClose(pad));pad=-1;}
 bool display_released=true;
 if(video>=0){
  // Do not free scanout memory while a flip is queued or buffers are registered.
  for(int i=0;i<100&&sceVideoOutIsFlipPending(video)>0;i++)usleep(10000);
  if(buffers_registered){shutdown_result("video buffers unregister",sceVideoOutUnregisterBuffers(video,0));buffers_registered=false;}
  int result=sceVideoOutClose(video);shutdown_result("video close",result);display_released=result>=0;video=-1;
 }
 if(queue_created){shutdown_result("flip queue delete",sceKernelDeleteEqueue(flip_queue));queue_created=false;}
 if(display_released){
  bool unmapped=true;
  if(memory){int result=sceKernelMunmap(memory,8*1024*1024);shutdown_result("display unmap",result);unmapped=result>=0;if(unmapped)memory=nullptr;}
  if(unmapped&&memory_allocated){shutdown_result("display memory release",sceKernelReleaseDirectMemory(memory_offset,8*1024*1024));memory_allocated=false;}
 }else plog("shutdown: display memory retained for process cleanup after close failure");
}
[[noreturn]] void platform_exit(int status){
 plog("shutdown: requesting normal exit to PS4 home screen");
 sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
 int result=sceSystemServiceLoadExec("exit",nullptr);
 // LoadExec normally never returns. Do not return from the homebrew entrypoint.
 shutdown_result("system exit returned",result);
 _Exit(status);
}'''+t[b:]
f.write_text(t,newline='\n')
