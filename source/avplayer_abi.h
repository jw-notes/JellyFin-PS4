// ABI layouts derived from shadPS4's avplayer.h (GPL-2.0-or-later).
// https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/avplayer/avplayer.h
#pragma once
#include <stddef.h>
#include <stdint.h>
struct AvAudio {uint16_t channels;uint8_t reserved[2];uint32_t rate,size;char language[4];};
struct AvVideo {uint32_t width,height;float aspect;char language[4];};
union AvDetails {uint8_t reserved[16];AvAudio audio;AvVideo video;};
struct AvFrame {uint8_t* data;uint8_t reserved[4];uint64_t timestamp;AvDetails details;};
struct AvVideoEx {uint32_t width,height;float aspect;uint8_t language[4];uint32_t framerate,crop_left,crop_right,crop_top,crop_bottom,pitch;uint8_t luma_depth,chroma_depth;bool full_range;uint8_t reserved[37];};
union AvDetailsEx {AvVideoEx video;uint8_t reserved[80];};
struct AvFrameEx {void* data;uint8_t reserved[4];uint64_t timestamp;AvDetailsEx details;};
struct AvMemory {void* object;void*(*allocate)(void*,uint32_t,uint32_t);void(*deallocate)(void*,void*);void*(*texture)(void*,uint32_t,uint32_t);void(*free_texture)(void*,void*);};
struct AvFiles {void* object;int(*open)(void*,const char*);int(*close)(void*);int(*read)(void*,uint8_t*,uint64_t,uint32_t);uint64_t(*size)(void*);};
struct AvEvents {void* object;void(*event)(void*,uint32_t,int,void*);};
struct AvInit {AvMemory memory;AvFiles files;AvEvents events;uint32_t debug,priority;int buffers;bool autostart;uint8_t reserved[3];const char* language;};
static_assert(sizeof(AvInit)==120,"AvPlayerInitData ABI");
static_assert(offsetof(AvFrame,details)==24,"AvPlayerFrameInfo ABI");
static_assert(sizeof(AvFrameEx)==104,"AvPlayerFrameInfoEx ABI");

struct AvUri {const char* name;uint32_t length;};
struct AvSourceDetails {AvUri uri;uint8_t reserved1[64];uint32_t source_type;uint8_t reserved2[44];};
struct AvDecoderInit {uint32_t type;uint8_t params[28];};
struct AvPostInit {uint32_t demux_video_buffer_size;AvDecoderInit video,audio;uint32_t http_context,ssl_context;uint8_t reserved[56];};
static_assert(sizeof(AvSourceDetails)==128,"AvPlayerSourceDetails ABI");
static_assert(offsetof(AvSourceDetails,source_type)==80,"AvPlayer source type offset");
static_assert(sizeof(AvPostInit)==132&&offsetof(AvPostInit,http_context)==68,"AvPlayerPostInitData ABI");
