from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/avplayer_abi.h';t=f.read_text()+'''
struct AvUri {const char* name;uint32_t length;};
struct AvSourceDetails {AvUri uri;uint8_t reserved1[64];uint32_t source_type;uint8_t reserved2[44];};
struct AvDecoderInit {uint32_t type;uint8_t params[28];};
struct AvPostInit {uint32_t demux_video_buffer_size;AvDecoderInit video,audio;uint32_t http_context,ssl_context;uint8_t reserved[56];};
static_assert(sizeof(AvSourceDetails)==128,"AvPlayerSourceDetails ABI");
static_assert(offsetof(AvSourceDetails,source_type)==80,"AvPlayer source type offset");
static_assert(sizeof(AvPostInit)==132&&offsetof(AvPostInit,http_context)==68,"AvPlayerPostInitData ABI");
''';f.write_text(t,newline='\n')
f=p/'source/avplayer_module.h';t=f.read_text().replace('array<void*,12>','array<void*,14>');f.write_text(t,newline='\n')
f=p/'source/avplayer_module.cpp';t=f.read_text().replace('"sceAvPlayerGetVideoDataEx"};','"sceAvPlayerGetVideoDataEx","sceAvPlayerAddSourceEx","sceAvPlayerPostInit"};').replace('if(i==11){','if(i>=11){');f.write_text(t,newline='\n')
f=p/'source/app.h';t=f.read_text().replace('std::string http_failure_detail();','std::string http_failure_detail();\nint http_context_id();\nint http_ssl_context_id();');f.write_text(t,newline='\n')
f=p/'source/http_ps4.cpp';t=f.read_text()+'\nint http_context_id(){return ctx;}\nint http_ssl_context_id(){return ssl;}\n';f.write_text(t,newline='\n')
