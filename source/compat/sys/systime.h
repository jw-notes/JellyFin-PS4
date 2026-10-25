#pragma once
// PS3 sys/systime.h for the unchanged PS3 Live TV API: the wall clock.
#include <time.h>
#include "ppu-types.h"
static inline int sysGetCurrentTime(u64* sec,u64* nsec){timespec ts{};clock_gettime(CLOCK_REALTIME,&ts);*sec=(u64)ts.tv_sec;*nsec=(u64)ts.tv_nsec;return 0;}
