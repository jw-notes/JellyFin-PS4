#pragma once
#include <stdint.h>
#include <stdio.h>
bool write_frame_bmp(FILE* output,const uint32_t* frame,unsigned width,unsigned height,unsigned pitch);
void capture_screen(const uint32_t* frame);
