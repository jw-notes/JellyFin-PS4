#include "app.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
int main(int argc,char** argv){
 assert(argc==4);assert(storage_init(104));snprintf(g_server,sizeof g_server,"%s",argv[1]);assert(login(argv[2],argv[3]));
 Page channels;assert(fetch_page("",0,"",3,channels));assert(!channels.items.empty());
 Item channel=channels.items[0];assert(!strcmp(channel.base.type,"TvChannel"));
 std::string url,session,source;assert(playback_url(channel,-1,-1,"",url,session,source));assert(!url.empty()&&!source.empty());
 FILE* f=fopen("build/test-live-url-104.txt","w");assert(f);fputs(url.c_str(),f);fclose(f);
 char playlist[32768];assert(http_request(HTTP_GET,url.c_str(),"",g_token,playlist,sizeof playlist)==200);assert(strstr(playlist,"#EXTM3U"));
 report_playback("",channel,session,source,0,false);
 pid_t child=fork();if(child==0){execlp("python3","python3","tools/decode_from_url.py","build/test-live-url-104.txt",nullptr);_exit(127);}int status;waitpid(child,&status,0);assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);
 report_playback("/Stopped",channel,session,source,0,false);end_playback(session);logout();
 puts("PASS real Live TV: channel discovery, source opening, negotiated HLS retrieval, three seconds of video/audio decode and stream cleanup");
}
