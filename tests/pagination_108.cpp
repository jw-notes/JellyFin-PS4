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
 ++frame;assert(frame<330);
 struct Input{int frame;uint32_t button;};
 const Input sequence[]={{8,0x4000},{20,0x20},{28,0x20},{36,0x20},{44,0x20},{52,0x20},{60,0x20},{76,0x40},{88,0x40},{100,0x40},{112,0x40},{124,0x40},{136,0x40},{148,0x40},{160,0x40},{172,0x40},{184,0x10},{196,0x40},{208,0x100000},{220,0x8},{232,0x40},{244,0x40},{256,0x40},{268,0x40},{280,0x40},{292,0x4000}};
 pad=0;for(const auto& input:sequence)if(frame>=input.frame&&frame<input.frame+2)pad=input.button;
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
 assert(frame>=292);assert(log.find("selected=59 mode=0")!=std::string::npos);logout();
 puts("PASS actual classic grid navigation: partial last row, next page, previous page, next page again, Home, Settings and exit");
}
