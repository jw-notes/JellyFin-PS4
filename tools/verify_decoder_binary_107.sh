#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
llvm-nm-18 -A tools/ps4-portlibs-107/opt/pacbrew/ps4/openorbis/usr/lib/libavcodec.a | grep -E 'ff_h264_videodec2_decoder|videodec2_load|videodec2_dmem_alloc' > logs/ps4-hardware-symbols-107.txt
strings build/JellyFin-PS4.elf | grep -E 'videodec2:|libSceVideodec2.sprx|build 01.07|MPV: opening' > logs/ps4-player-binary-107.txt

