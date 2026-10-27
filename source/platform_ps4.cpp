#include "app.h"
#include "platform.h"
#include "crash.h"
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <map>
static int video=-1,pad=-1,user=0,frame_index=0;static uint32_t* buffers[2];static void* memory;static off_t memory_offset;
static OrbisKernelEqueue flip_queue;
static bool queue_created=false,buffers_registered=false,memory_allocated=false;
int current_user(){return user;}
// Free memory, for the log during playback: a leak shows as a number that
// falls. (The toolchain declares the call without its pointer, so it is bound
// here under its own name.)
extern "C" int32_t flexible_memory_free(size_t* size) __asm__("sceKernelAvailableFlexibleMemorySize");
std::string resource_report(){
 size_t free_bytes=0;return flexible_memory_free(&free_bytes)>=0?"free memory "+std::to_string(free_bytes>>20)+" MB":"free memory unknown";
}
// clock_gettime comes from the console's own libkernel, which numbers its
// clocks the FreeBSD way, while the toolchain's <time.h> (and so FFmpeg and
// the C library) use Linux's numbers: CLOCK_MONOTONIC (1) reached the kernel
// as CLOCK_VIRTUAL, this process's CPU time, about a quarter of real time
// here. Every caller is linked through this (--wrap=clock_gettime), which
// translates the Linux ids before the call. See also mono_clock.h.
extern "C" int __real_clock_gettime(int id,struct timespec* ts);
extern "C" int __wrap_clock_gettime(int id,struct timespec* ts){
 switch(id){
 case 1:case 4:id=4;break;   // MONOTONIC, MONOTONIC_RAW -> CLOCK_MONOTONIC
 case 2:id=15;break;         // PROCESS_CPUTIME_ID
 case 3:id=14;break;         // THREAD_CPUTIME_ID
 case 7:id=5;break;          // BOOTTIME -> CLOCK_UPTIME
 default:break;              // REALTIME is 0 in both
 }
 return __real_clock_gettime(id,ts);
}
bool platform_init(){
 sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
 sceUserServiceInitialize(nullptr);if(sceUserServiceGetInitialUser(&user)<0)return false;
 if(!storage_init(user))return false;
 install_crash_diagnostics(user);
 scePadInit();pad=scePadOpen(user,0,0,nullptr);if(pad<0)return false;
 video=sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN,0,0,nullptr);if(video<0)return false;
 const size_t size=8*1024*1024;
 if(sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),size,0x200000,3,&memory_offset)<0)return false;
 memory_allocated=true;
 if(sceKernelMapDirectMemory(&memory,size,0x33,0,memory_offset,0x200000)<0)return false;
 buffers[0]=(uint32_t*)memory;buffers[1]=buffers[0]+WIDTH*HEIGHT;
 OrbisVideoOutBufferAttribute attr{};sceVideoOutSetBufferAttribute(&attr,0x80000000,1,0,WIDTH,HEIGHT,WIDTH);
 if(sceVideoOutRegisterBuffers(video,0,(void*const*)buffers,2,&attr)<0)return false;
 buffers_registered=true;
 if(sceKernelCreateEqueue(&flip_queue,"JellyFin flip")<0)return false;
 queue_created=true;
 if(sceVideoOutAddFlipEvent(flip_queue,video,nullptr)<0)return false;
 sceVideoOutSetFlipRate(video,0);
 render_init();
 return true;
}
static void shutdown_result(const char* operation,int result){char log[128];snprintf(log,sizeof log,"shutdown: %s result=0x%08x",operation,(unsigned)result);plog(log);}
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
}
uint32_t buttons(){OrbisPadData p{};return scePadReadState(pad,&p)>=0&&p.connected?p.buttons:0;}
// Everything is drawn into this ordinary (cached) memory and copied to the
// display buffer once per frame. The display buffers are write-combined
// graphics memory: fast to write, but every read -- and every blend, glow,
// fade and shadow reads the pixel under it -- went uncached, which kept a
// simple boot frame at 13-16 ms of the 16.7 available.
static uint32_t* canvas=nullptr;
uint32_t* framebuffer(){
 if(!canvas){void* memory=nullptr;if(posix_memalign(&memory,64,(size_t)WIDTH*HEIGHT*4)==0){canvas=(uint32_t*)memory;memset(canvas,0,(size_t)WIDTH*HEIGHT*4);}else return buffers[frame_index];}
 return canvas;
}
void present(){if(canvas)memcpy(buffers[frame_index],canvas,(size_t)WIDTH*HEIGHT*4);int rc=sceVideoOutSubmitFlip(video,frame_index,1,0);if(rc>=0){while(sceVideoOutIsFlipPending(video)>0){OrbisKernelEvent ev;int n;sceKernelWaitEqueue(flip_queue,&ev,1,&n,nullptr);}}frame_index^=1;}
