#pragma once
#include "app.h"
#include <functional>
// The Live TV page (PS3 Classic layout). Runs until the viewer leaves:
// -1 Back, 0 Home, 1 Search, 2 Settings, -2 unavailable (last_error set).
// `play` plays a channel.
int livetv_screen(const std::function<void(const Item&)>& play);
// Stops the worker (sign-out, exit).
void livetv_stop();
