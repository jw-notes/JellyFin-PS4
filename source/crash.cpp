#include "crash.h"
#include "app.h"
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
static int crash_fd=-1;
static void fatal_signal(int number){
 // Keep the fatal path allocation-free and independent of the normal logger.
 const char hex[]="0123456789abcdef";char message[]="PS4 build 01.08 fatal signal=0x00\n";
 message[sizeof message-4]=hex[(number>>4)&15];message[sizeof message-3]=hex[number&15];
 if(crash_fd>=0){write(crash_fd,message,sizeof message-1);fsync(crash_fd);}
 signal(number,SIG_DFL);if(raise(number)!=0)_Exit(128+number);
}
void install_crash_diagnostics(int user){
 char path[256];
#ifdef HOST_TEST
 (void)user;
 snprintf(path,sizeof path,"%s/crash.log",data_dir.c_str());
#else
 snprintf(path,sizeof path,"/data/JellyFin-PS4-logs/user-%08x-crash.log",user);
#endif
 crash_fd=open(path,O_WRONLY|O_CREAT|O_APPEND,0644);
 for(int number:{SIGSEGV,SIGABRT,SIGILL,SIGFPE})if(signal(number,fatal_signal)==SIG_ERR)plog("Crash signal handler unavailable");
}
