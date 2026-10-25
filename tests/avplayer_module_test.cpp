#include "avplayer_module.h"
#include <cassert>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
static int scenario,public_calls,file_calls,resolve_calls;static bool loaded;
static int load_public(){++public_calls;if(scenario==1){loaded=true;return 0;}return (int32_t)0x805a1001;}
static std::vector<int> modules(){return loaded?std::vector<int>{7,42}:std::vector<int>{7};}
static int load_file(const char* path,int* started){++file_calls;assert(!strcmp(path,"/system/common/lib/libSceAvPlayer.sprx"));*started=scenario==6?(int32_t)0x80020005:0;if(scenario==3)return (int32_t)0x80020002;loaded=true;return 42;}
static int resolve(int module,const char* name,void** output){++resolve_calls;*output=nullptr;if(module!=42)return -1;if(scenario==4&&!strcmp(name,"sceAvPlayerGetAudioData"))return -2;if(scenario==5&&!strcmp(name,"sceAvPlayerGetVideoDataEx"))return -3;*output=reinterpret_cast<void*>(uintptr_t(0x1000));return 0;}
static void log(const char*){}
int main(){const AvModuleOps ops={load_public,modules,load_file,resolve,log};
 for(scenario=0;scenario<7;++scenario){loaded=scenario==0||scenario==4||scenario==5;public_calls=file_calls=resolve_calls=0;AvModuleExports api;std::string error;bool ok=acquire_avplayer_module(api,ops,error);
  if(scenario==3||scenario==4||scenario==6){assert(!ok&&api.module==-1&&!error.empty());if(scenario==3){assert(error.find("805a1001")!=std::string::npos&&error.find("80020002")!=std::string::npos);}else if(scenario==6)assert(error.find("startup failed")!=std::string::npos);else assert(error.find("sceAvPlayerGetAudioData")!=std::string::npos);}
  else{assert(ok&&api.module==42);if(scenario==0)assert(public_calls==0&&file_calls==0);if(scenario==1)assert(public_calls==1&&file_calls==0);if(scenario==2)assert(public_calls==1&&file_calls==1);if(scenario==5)assert(api.symbols[11]==nullptr);int calls=resolve_calls;assert(acquire_avplayer_module(api,ops,error)&&calls==resolve_calls);}
 }
 puts("PASS AvPlayer loader: cached module, sysmodule success, rejected sysmodule with kernel fallback, full load failure, startup failure, missing required API and optional extended-frame fallback");
}
