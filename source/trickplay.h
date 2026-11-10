// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// Seek preview thumbnails from the server's trickplay sheets (Jellyfin 10.9+).
#pragma once
#include "app.h"
#include <stdint.h>
#include <vector>
// The film playing has these thumbnails (or none: an empty info). Forgets the last one's.
void trickplay_begin(const TrickplayInfo& info);
// No trickplay from the server: frames are taken from the file itself, at
// `url` (a path, an http(s) address, or a USB drive's URL read through
// GoldHEN's FTP server). An empty url: none.
void trickplay_begin_file(const std::string& url);
void trickplay_end();
bool trickplay_available();
// The thumbnail for `ms`, scaled for the screen (0xFFRRGGBB, w x h), copied
// out. While its sheet is still on the way, the nearest one already here, or
// false; asking fetches it.
bool trickplay_thumb(uint64_t ms,std::vector<uint32_t>& px,int& w,int& h);
#ifdef HOST_TEST
// Host screenshots: every thumbnail is this picture (THUMB_W wide, 0xFFRRGGBB).
void trickplay_preview(const std::vector<uint32_t>& px,int w,int h);
#endif
