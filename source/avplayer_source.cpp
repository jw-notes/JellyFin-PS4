#include "avplayer_source.h"
#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include "http_diagnostics.h"
static std::string code(int result){char text[16];snprintf(text,sizeof text,"0x%08x",(unsigned)result);return text;}
bool configure_avplayer_source(const AvSourceApi& api,void* player,const std::string& url,int http,int ssl,std::string& error){
    if(!player||url.empty()||!api.add_source){error="Invalid playback source.";return false;}
    if(http<0||ssl<0||!api.post_init){error="PS4 network playback context is unavailable.";api.log(error.c_str());return false;}
    AvPostInit post{};post.demux_video_buffer_size=4*1024*1024;post.http_context=(uint32_t)http;post.ssl_context=(uint32_t)ssl;
    int result=api.post_init(player,&post);
    api.log(("AvPlayer: network PostInit result="+code(result)).c_str());
    if(result<0){error="PS4 network playback initialization failed ("+code(result)+").";return false;}
    auto path=url.substr(0,url.find_first_of("?#"));std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return (char)tolower(c);});
    const bool hls=path.size()>=5&&path.compare(path.size()-5,5,".m3u8")==0;
    api.log(("AvPlayer: source target="+http_log_target(url.c_str())).c_str());
    api.log((std::string("AvPlayer: opening ")+(hls?"HLS":"MP4")+" source; URL bytes="+std::to_string(url.size())).c_str());
    if(api.add_source_ex){AvSourceDetails source{};source.uri={url.c_str(),(uint32_t)url.size()};source.source_type=hls?8:1;result=api.add_source_ex(player,0,&source);}
    else result=api.add_source(player,url.c_str());
    api.log(("AvPlayer: add source result="+code(result)).c_str());
    if(result<0){error=std::string("PS4 could not open the ")+(hls?"HLS":"MP4")+" stream ("+code(result)+").";return false;}
    return true;
}
