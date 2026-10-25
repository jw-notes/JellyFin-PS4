#pragma once
// Reuse the PS3 transport interface while giving the PS4 a larger JSON budget.
// Large libraries and provider-generated media-source lists exceed 384 KiB.
#include "../../vendor/ps3/source/net/http.h"
#undef RESPONSE_SIZE
#define RESPONSE_SIZE (8*1024*1024)
