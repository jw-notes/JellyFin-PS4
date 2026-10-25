#include "avplayer_module.h"
#include <stdio.h>
static const char* names[]={"sceAvPlayerInit","sceAvPlayerAddSource","sceAvPlayerStop","sceAvPlayerClose","sceAvPlayerGetAudioData","sceAvPlayerGetVideoData","sceAvPlayerIsActive","sceAvPlayerCurrentTime","sceAvPlayerPause","sceAvPlayerResume","sceAvPlayerJumpToTime","sceAvPlayerGetVideoDataEx","sceAvPlayerAddSourceEx","sceAvPlayerPostInit","sceAvPlayerSetLogCallback"};
static void result(const AvModuleOps& ops,const char* stage,int code){char message[180];snprintf(message,sizeof message,"AvPlayer: %s result=0x%08x",stage,(unsigned)code);ops.log(message);}
static bool resolve_module(int module,AvModuleExports& exports,const AvModuleOps& ops,std::string& error){
    void* init=nullptr;
    if(ops.resolve(module,names[0],&init)<0||!init)return false;
    AvModuleExports candidate;candidate.module=module;candidate.symbols[0]=init;
    for(size_t i=1;i<candidate.symbols.size();++i){
        const int rc=ops.resolve(module,names[i],&candidate.symbols[i]);
        if(i>=11){if(rc<0)candidate.symbols[i]=nullptr;continue;}
        if(rc<0||!candidate.symbols[i]){char message[180];snprintf(message,sizeof message,"AvPlayer API %s is unavailable (0x%08x).",names[i],(unsigned)rc);error=message;ops.log(message);return false;}
    }
    exports=candidate;ops.log(candidate.symbols[11]?"AvPlayer: exports resolved; extended video frames":"AvPlayer: exports resolved; basic video frames");return true;
}
static bool find_loaded(AvModuleExports& exports,const AvModuleOps& ops,std::string& error){for(int module:ops.loaded_modules())if(resolve_module(module,exports,ops,error))return true;return false;}
bool acquire_avplayer_module(AvModuleExports& exports,const AvModuleOps& ops,std::string& error){
    if(exports.module>=0&&exports.symbols[0])return true;
    if(find_loaded(exports,ops,error)){ops.log("AvPlayer: using already loaded module");return true;}
    const int public_result=ops.load_public();result(ops,"system module load",public_result);
    // Some homebrew environments refuse the public sysmodule ID even though
    // the on-console SPRX is available. Do not stop before trying that module.
    if(find_loaded(exports,ops,error))return true;
    int start_result=0;
    const int module=ops.load_file("/system/common/lib/libSceAvPlayer.sprx",&start_result);
    result(ops,"kernel module load",module);result(ops,"module start",start_result);
    if(module>=0&&start_result<0){char message[180];snprintf(message,sizeof message,"AvPlayer module startup failed (0x%08x).",(unsigned)start_result);error=message;ops.log(message);return false;}
    if(module>=0&&start_result>=0&&resolve_module(module,exports,ops,error))return true;
    // A concurrent/already loaded result must still reuse the existing handle.
    if(find_loaded(exports,ops,error))return true;
    if(error.empty()){char message[220];snprintf(message,sizeof message,"AvPlayer module could not load (system 0x%08x, kernel 0x%08x, start 0x%08x).",(unsigned)public_result,(unsigned)module,(unsigned)start_result);error=message;}
    ops.log(error.c_str());return false;
}
