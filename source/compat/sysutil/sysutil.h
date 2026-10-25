#pragma once
// PS3 sysutil parameters for the unchanged PS3 Live TV API, answered from the
// PS4 system settings (time zone in minutes, and summer time).
#include "ppu-types.h"
#define SYSUTIL_SYSTEMPARAM_ID_TIMEZONE 4
#define SYSUTIL_SYSTEMPARAM_ID_SUMMERTIME 5
#ifdef HOST_TEST
static inline int sysUtilGetSystemParamInt(int,s32* v){*v=0;return 0;}
#else
#include <orbis/SystemService.h>
static inline int sysUtilGetSystemParamInt(int id,s32* v){int32_t value=0;int rc=sceSystemServiceParamGetInt(id,&value);*v=value;return rc<0?rc:0;}
#endif
