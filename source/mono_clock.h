#pragma once
// Real elapsed time, for animation, input repeat and timeouts.
//
// The toolchain's <time.h> numbers its clocks the Linux way, but the PS4
// kernel is FreeBSD's: clock_gettime(CLOCK_MONOTONIC) asks it for clock 1,
// which FreeBSD calls CLOCK_VIRTUAL -- the CPU time this process has used.
// An app that mostly waits for the display uses a fraction of a core, so
// every timer built on it ran about four times slow (the boot animation's
// 4 s took 16). sceKernelGetProcessTime() is wall time since launch, in
// microseconds.
#include <stdint.h>
#ifdef HOST_TEST
#include <time.h>
static inline uint64_t mono_us(){timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*1000000+ts.tv_nsec/1000;}
#else
extern "C" uint64_t sceKernelGetProcessTime(void);
static inline uint64_t mono_us(){return sceKernelGetProcessTime();}
#endif
static inline uint64_t mono_ms(){return mono_us()/1000;}
