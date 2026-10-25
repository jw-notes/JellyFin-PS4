#!/usr/bin/env bash
set -e
cd /mnt/d/Coding-Projects/PS4/JellyFin-PS4
command -v pkg-config
pkg-config --modversion libavformat libavcodec libswscale libswresample || true
ls /usr/include/x86_64-linux-gnu/libavformat/avformat.h 2>/dev/null || true
