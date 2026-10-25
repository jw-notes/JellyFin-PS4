#include "avplayer_source.h"
#include "playback_model.h"
#include "input_model.h"
#include <cassert>
#include <cstring>
#include <cstdio>
static int stage=0,failure=0;static unsigned kind=0;
static void log_line(const char*){}
static int post(void*,AvPostInit* p){assert(stage++==0);assert(p->http_context==17&&p->ssl_context==29);assert(p->demux_video_buffer_size>=1024*1024);return failure;}
static int add(void*,const char*){assert(stage++==1);kind=99;return failure;}
static int add_ex(void*,uint32_t uri,AvSourceDetails* s){assert(stage++==1);assert(uri==0);assert(s->uri.length==strlen(s->uri.name));kind=s->source_type;return failure;}
int main(){
 AvSourceApi api{post,add_ex,add,log_line};std::string error;void* player=(void*)1;
 assert(configure_avplayer_source(api,player,"http://server/master.M3U8?token=secret",17,29,error));assert(kind==8);
 stage=0;assert(configure_avplayer_source(api,player,"http://server/stream.mp4",17,29,error));assert(kind==1);
 stage=0;api.add_source_ex=nullptr;assert(configure_avplayer_source(api,player,"http://server/master.m3u8",17,29,error));assert(kind==99);
 stage=0;failure=(int)0x806a0002;assert(!configure_avplayer_source(api,player,"http://server/stream.mp4",17,29,error));assert(stage==1&&error.find("806a0002")!=std::string::npos);
 stage=0;failure=0;assert(!configure_avplayer_source(api,player,"http://server/stream.mp4",-1,29,error));assert(stage==0);
 assert(select_playback_source("{\"MediaSources\":[{\"RequiresOpening\":true,\"OpenToken\":\"opaque\"}]}","").find("opaque")!=std::string::npos);
 assert(select_playback_source("{\"MediaSources\":[{\"Id\":\"stub\",\"Path\":\"gelato://stub/test\"},{\"Id\":\"real\"}]}","").find("real")!=std::string::npos);
 assert(select_playback_source("{\"MediaSources\":[{\"Id\":\"a\"},{\"Id\":\"b\"}]}","b")=="{\"Id\":\"b\"}");
 assert(select_playback_source("{\"MediaSources\":[]}","").empty());
 assert(playback_response_summary("{\"MediaSources\":[],\"ErrorCode\":\"NoCompatibleStream\"}").find("NoCompatibleStream")!=std::string::npos);
 UiInputFilter f;constexpr uint32_t cross=1<<14,right=1<<5;
 f.sync(0,100);assert(f.update(cross,110)==cross);assert(f.update(cross,150)==0);
 assert(f.update(0,160)==0);assert(f.update(cross,170)==cross); // rapid deliberate taps must remain responsive
 f.update(0,200);f.update(0,266);assert(f.update(cross,280)==cross);
 f.sync(cross,300);assert(f.update(cross,400)==0); // screen transition consumes held confirm
 f.sync(0,500);assert(f.update(right,510)==right);assert(f.update(right,859)==0);assert(f.update(right,860)==right);assert(f.update(right,969)==0);assert(f.update(right,970)==right);
 puts("PASS source network context / HLS ABI, opening-only live sources, server errors, held-button suppression, rapid taps, screen transitions and directional repeat");
}
