#pragma once
// Background work -- artwork downloads and decoding, page prefetch, Home's
// rows, item facts, the boot loader -- runs below the UI thread, so a busy
// worker cannot delay a frame. On the PS4 a larger number is a lower
// priority: 700 is the default, 767 the lowest.
#ifdef HOST_TEST
static inline void background_priority(){}
#else
#include <orbis/libkernel.h>
static inline void background_priority(){scePthreadSetprio(scePthreadSelf(),760);}
#endif
