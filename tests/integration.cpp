#include "app.h"
#include "api_userdata.h"
#include <cassert>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
int main(int argc,char**argv){
 assert(json_quote("a\"b\\c\n")=="\"a\\\"b\\\\c\\u000a\"");assert(encode("a b&c/")=="a%20b%26c%2F");
 char value[128];assert(json_get_string("{ \"x\" : \"escaped \\\" and \\u00e9\", \"n\":{\"Id\":\"wrong\"}, \"Id\" : \"right\" }","Id",value,sizeof value));assert(strcmp(value,"right")==0);
 JFItem parsed[4];assert(parse_jf_items("{\"Items\":[{ \"Id\" : \"1\", \"Name\" : \"A {brace}\", \"Type\" : \"Movie\", \"UserData\":{\"Id\":\"wrong\"}}]}",parsed,4)==1);assert(strcmp(parsed[0].name,"A {brace}")==0);
 assert(storage_init(42));assert(save_settings());assert(!device_id.empty());theme_apply_builtin(0);assert(theme_load("vendor/ps3/themes/golden-age.ini"));assert(g_theme.accent!=0);
 puts("PASS JSON escaping, direct child parsing, legacy item parser, storage, original theme parser");
 if(argc<4)return 0;
 snprintf(g_server,sizeof g_server,"%s",argv[1]);assert(!login(argv[2],"incorrect-password"));assert(login(argv[2],argv[3]));assert(save_session());char uid[64];snprintf(uid,sizeof uid,"%s",g_userid);g_token[0]=0;assert(load_session());assert(!strcmp(uid,g_userid));
 Page libraries;assert(fetch_page("",0,"",0,libraries));assert(!libraries.items.empty());
 Page results;assert(fetch_page("",0,"PS4 Integration",0,results));assert(results.items.size()>=1);Item movie=results.items[0],detail;assert(fetch_detail(movie,detail));assert(!strcmp(detail.base.id,movie.base.id));
 assert(jf_set_favourite(movie.base.id,true));Page favourites;assert(fetch_page("",0,"",2,favourites));bool found=false;for(auto&i:favourites.items)if(!strcmp(i.base.id,movie.base.id))found=true;assert(found);assert(jf_set_favourite(movie.base.id,false));
 assert(jf_set_played(movie.base.id,true));assert(fetch_detail(movie,detail));assert(detail.played);assert(jf_set_played(movie.base.id,false));
 std::string url,session,source;assert(playback_url(movie,-1,-1,"",url,session,source));assert(!url.empty());assert(!session.empty());assert(!source.empty());
 FILE*f=fopen("build/test-playback-url.txt","w");assert(f);fprintf(f,"%s",url.c_str());fclose(f);
 report_playback("",movie,session,source,0,false);report_playback("/Progress",movie,session,source,45000,false);report_playback("/Stopped",movie,session,source,45000,false);
 assert(fetch_detail(movie,detail));assert(detail.resume>=44000&&detail.resume<=46000);
 // Fetch real HLS through curl, then use the server's ffmpeg in the Python harness.
 char playlist[32768];assert(http_request(HTTP_GET,url.c_str(),"",g_token,playlist,sizeof playlist)==200);assert(strstr(playlist,"#EXTM3U"));
 pid_t child=fork();if(child==0){execlp("python3","python3","tools/decode_from_url.py",nullptr);_exit(127);}int child_status;waitpid(child,&child_status,0);assert(WIFEXITED(child_status)&&WEXITSTATUS(child_status)==0);

 end_playback(session);
 logout();assert(g_token[0]==0);
 puts("PASS real Jellyfin: authentication, persisted session, libraries, search, detail, favourites, watched state, HLS negotiation, progress/resume, stream retrieval + actual video/audio decode, cleanup, logout");
}
