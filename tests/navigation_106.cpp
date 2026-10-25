// Script the actual application loop with a host display/controller adapter.
// Authentication, API calls, renderer, input filter and tab screens are real.
#include "app.h"
#include "platform.h"
#include "screenshot.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <unistd.h>
extern int ps4_application_main();
static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
static int frame=0;static uint32_t pad=0;
uint32_t* framebuffer(){return pixels.data();}
uint32_t buttons(){return pad;}
void present(){
 ++frame;assert(frame<190);
 if(frame==9||frame==21){FILE* f=fopen(frame==9?"build/search-navigation-106.bmp":"build/settings-navigation-106.bmp","wb");assert(f&&write_frame_bmp(f,pixels.data(),WIDTH,HEIGHT,WIDTH));fclose(f);}
 const uint32_t sequence[]={0x800,0x800,0x800,0x400,0x400,0x400,0x8,0x40,0x40,0x40,0x40,0x40,0x4000};
 pad=0;for(int i=0;i<13;++i)if(frame>=8+i*12&&frame<11+i*12)pad=sequence[i];
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
 assert(argc==4);assert(storage_init(106));snprintf(g_server,sizeof g_server,"%s",argv[1]);assert(login(argv[2],argv[3]));
 std::ifstream before(data_dir+"/app.log");std::string old((std::istreambuf_iterator<char>(before)),{});
 try{ps4_application_main();assert(false);}catch(int status){assert(status==0);}
 std::ifstream file(data_dir+"/app.log");std::string log((std::istreambuf_iterator<char>(file)),{});log=log.substr(old.size());
 const char* expected[]={"Search","Settings","Home","Settings","Search","Home","Settings"};size_t at=0;
 for(auto tab:expected){auto found=log.find(std::string("Navigation: tab=")+tab,at);assert(found!=std::string::npos);at=found+strlen(tab)+16;}
 assert(frame>=152);logout();
 puts("PASS actual app loop: R1 Home/Search/Settings/Home, L1 Home/Settings/Search/Home, Options, settings navigation and exit; no keyboard required to switch tabs");
}
