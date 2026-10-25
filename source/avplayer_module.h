#pragma once
#include <array>
#include <string>
#include <vector>
struct AvModuleExports {
    int module = -1;
    // GetVideoDataEx is optional; the basic frame API is always required.
    std::array<void*,15> symbols{};
};
struct AvModuleOps {
    int (*load_public)();
    std::vector<int> (*loaded_modules)();
    int (*load_file)(const char*,int*);
    int (*resolve)(int,const char*,void**);
    void (*log)(const char*);
};
bool acquire_avplayer_module(AvModuleExports& exports,const AvModuleOps& ops,std::string& error);
