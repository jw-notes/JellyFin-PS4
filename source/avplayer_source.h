#pragma once
#include "avplayer_abi.h"
#include <string>
struct AvSourceApi {
    int(*post_init)(void*,AvPostInit*);
    int(*add_source_ex)(void*,uint32_t,AvSourceDetails*);
    int(*add_source)(void*,const char*);
    void(*log)(const char*);
};
bool configure_avplayer_source(const AvSourceApi& api,void* player,const std::string& owned_url,int http,int ssl,std::string& error);
