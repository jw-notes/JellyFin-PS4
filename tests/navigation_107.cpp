// Script the actual application loop with a host display/controller adapter.
// Authentication, API calls, renderer, input filter and tab screens are real.
#include "app.h"
#include "platform.h"
#include "screenshot.h"
#include "facts.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <unistd.h>
extern int ps4_application_main();
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
static std::string movie_id;
static int frame=0;static uint32_t pad=0;
uint32_t* framebuffer(){return pixels.data();}
uint32_t buttons(){return pad;}
void present(){
 ++frame;assert(frame<440);
 if(frame==50){Item detail;std::string status;assert(facts_copy(movie_id,detail,status));assert(detail.overview=="PS4 quick-peek integration synopsis.");assert(detail.cast.find("Preview Actor")!=std::string::npos);FILE* f=fopen("build/quick-peek-live-metadata-107.bmp","wb");assert(f&&write_frame_bmp(f,pixels.data(),WIDTH,HEIGHT,WIDTH));fclose(f);}
 struct Input{int frame;uint32_t button;};
 const Input sequence[]={{8,0x4000},{24,0x1000},{80,0x2000},{112,0x2000},{128,0x240},{144,0x210},{160,0x40},{176,0x200},{192,0x800},{208,0x800},{224,0x800},{240,0x400},{256,0x400},{272,0x400},{288,0x8},{304,0x40},{320,0x40},{336,0x40},{352,0x40},{368,0x40},{384,0x4000}};
 pad=0;for(const auto& input:sequence)if(frame>=input.frame&&frame<input.frame+3)pad=input.button;
 usleep(20000);
}
bool platform_init(){render_init();return true;}
void platform_shutdown(){}
[[noreturn]] void platform_exit(int status){throw status;}
int current_user(){return 106;}
bool sign_in(){assert(false&&"Unexpected sign-in screen");return false;}
bool keyboard(const std::string&,std::string&,bool,KeyboardType){assert(false&&"Shoulder navigation must not open the keyboard");return false;}
bool player_open(const std::string&){assert(false);return false;}
std::string player_error(){return {};}
bool player_draw(){return false;}bool player_active(){return false;}bool player_has_audio(){return false;}
void player_pause(bool){}void player_seek(uint64_t){}uint64_t player_time(){return 0;}void player_close(){}
int main(int argc,char** argv){
 assert(argc==4);assert(storage_init(106));snprintf(g_server,sizeof g_server,"%s",argv[1]);assert(login(argv[2],argv[3]));Page movies;assert(fetch_page("",0,"PS4 Integration",0,movies)&&!movies.items.empty());movie_id=movies.items[0].base.id;
 std::ifstream before(data_dir+"/app.log");std::string old((std::istreambuf_iterator<char>(before)),{});
 try{ps4_application_main();assert(false);}catch(int status){assert(status==0);}
 std::ifstream file(data_dir+"/app.log");std::string log((std::istreambuf_iterator<char>(file)),{});log=log.substr(old.size());
 const char* expected[]={"Search","Settings","Home","Settings","Search","Home","Settings"};size_t at=0;
 for(auto tab:expected){auto found=log.find(std::string("Navigation: tab=")+tab,at);assert(found!=std::string::npos);at=found+strlen(tab)+16;}
 assert(frame>=384);assert(log.find("mode=3")==std::string::npos);assert(log.find("Quick peek: detail metadata loaded")!=std::string::npos);logout();
 puts("PASS quick peek from empty list metadata, fetched synopsis/cast, D-pad with trigger noise never opens Live TV; actual app loop: R1 Home/Search/Settings/Home, L1 Home/Settings/Search/Home, Options, settings navigation and exit; no keyboard required to switch tabs");
}
