#pragma once
#include "app.h"
#include <functional>
// The Live TV page (PS3 Classic layout). Runs until the viewer leaves:
// -1 Back, 0 Home, 1 Search, 2 Settings, -2 unavailable (last_error set).
// `play` plays a channel, given the ones before and after it in the list
// (null at either end), and returns -1 / +1 to change to one of those
// (L1 / R1 in the player), 0 when it is done.
using LiveTvPlay=std::function<int(const Item&,const Item* prev,const Item* next)>;
int livetv_screen(const LiveTvPlay& play);
// Stops the worker (sign-out, exit).
void livetv_stop();
