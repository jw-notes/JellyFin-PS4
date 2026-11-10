// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once
#include <stdint.h>
#include <stdio.h>
bool write_frame_bmp(FILE* output,const uint32_t* frame,unsigned width,unsigned height,unsigned pitch);
void capture_screen(const uint32_t* frame);
