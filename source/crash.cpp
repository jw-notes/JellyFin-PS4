// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#include "crash.h"
#include "app.h"
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#ifndef JF_BUILD_VERSION
#define JF_BUILD_VERSION "host"
#endif
static int crash_fd=-1;
static void fatal_signal(int number){
 // Keep the fatal path allocation-free and independent of the normal logger.
 // The build's own version (version.json, via build.sh); the signal's two hex
 // digits are the last characters before the newline.
 const char hex[]="0123456789abcdef";char message[]="PS4 build " JF_BUILD_VERSION " fatal signal=0x00\n";
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
 // A write to a connection the other end has closed must fail, not end the app
 // with no word in any log: the toolchain's MSG_NOSIGNAL is Linux's value, which
 // the console's FreeBSD kernel does not read as "no signal".
 signal(SIGPIPE,SIG_IGN);
 for(int number:{SIGSEGV,SIGBUS,SIGABRT,SIGILL,SIGFPE})if(signal(number,fatal_signal)==SIG_ERR)plog("Crash signal handler unavailable");
}
