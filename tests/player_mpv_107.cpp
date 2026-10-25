#include "app.h"
#include "platform.h"
#include <cassert>
#include <cstdio>
#include <set>
#include <unistd.h>
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
uint32_t* framebuffer(){return pixels.data();}
static void decoded_stream(const std::string& url){
 assert(player_open(url));std::set<uint64_t> images;
 for(int n=0;n<1600;++n){bool video=player_draw();assert(player_error().empty());if(video){uint64_t hash=0;for(int k=0;k<WIDTH*HEIGHT;k+=1021)hash=hash*131+pixels[k];images.insert(hash);}if(player_time()>2200&&player_has_audio()&&images.size()>5)break;usleep(10000);}
 assert(player_time()>2200&&player_has_audio()&&images.size()>5);
 printf("PASS production MPV adapter: changing video frames, decoded audio and advancing playback clock (%zu distinct frames)\n",images.size());
}
int main(int argc,char** argv){
 assert(argc==4);assert(storage_init(107));snprintf(g_server,sizeof g_server,"%s",argv[1]);assert(login(argv[2],argv[3]));
 Page movies;assert(fetch_page("",0,"PS4 Integration",0,movies)&&!movies.items.empty());std::string url,session,source;
 assert(playback_url(movies.items[0],-1,-1,"",url,session,source));decoded_stream(url);
 player_pause(true);usleep(100000);player_draw();auto paused=player_time();usleep(250000);player_draw();auto after=player_time();assert(after<=paused+100);
 player_pause(false);player_seek(12000);bool sought=false;for(int n=0;n<1000;++n){player_draw();assert(player_error().empty());auto now=player_time();if(now>=11500&&now<16000){sought=true;break;}usleep(10000);}assert(sought);
 player_close();end_playback(session);puts("PASS movie pause, resume, seek and decoder teardown");
 Page channels;assert(fetch_page("",0,"",3,channels)&&!channels.items.empty());assert(playback_url(channels.items[0],-1,-1,"",url,session,source));decoded_stream(url);player_close();end_playback(session);puts("PASS real Live TV through production MPV adapter");
 assert(player_open(std::string(g_server)+"/missing-stream-107.m3u8"));bool rejected=false;for(int n=0;n<1000;++n){player_draw();if(!player_error().empty()){rejected=true;break;}usleep(10000);}assert(rejected);player_close();logout();puts("PASS asynchronous source failure reaches the UI instead of a silent buffering timeout");
}
