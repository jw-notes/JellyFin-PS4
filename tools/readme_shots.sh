#!/usr/bin/env bash
# JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
# GPL-3.0-or-later, with the attribution terms in NOTICE.
# Render the README's screenshots on the host (build/readme/*.bmp).
# Usage: tools/readme_shots.sh [playlist.m3u]   -- the playlist names the Live TV channels
# (tests/readme_channels.m3u, made-up channels, when none is given).
set -euo pipefail
cd "$(dirname "$0")/.."
# The Live TV page includes the controller's button codes: that one header (it
# needs only stdint.h) is copied into a test-only folder for the host build.
mkdir -p build build/readme build/readme_inc/orbis/_types
cp tools/OpenOrbis/PS4Toolchain/include/orbis/_types/pad.h build/readme_inc/orbis/_types/pad.h
# The version shown in About and Settings, as the PS4 build passes it (tools/build.sh).
APP_VER=$(python3 -c 'import json;print(json.load(open("version.json"))["app_version"])')
flags=("-DJF_BUILD_VERSION=\"$APP_VER\"" -DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util -Ibuild/readme_inc)
for c in vendor/ps3/source/api/version_summary.c vendor/ps3/source/api/livetv_prefs.c vendor/ps3/source/i18n/i18n.c vendor/ps3/source/i18n/i18n_tables.c; do
 gcc "${flags[@]}" -c "$c" -o "build/readme_$(basename "${c%.c}").o"
done
g++ -std=c++17 -O2 -Wall -Wno-unused-function "${flags[@]}" tests/readme_shots.cpp tests/http_host.cpp \
 source/render.cpp source/ui_view.cpp source/trickplay.cpp source/buffering.cpp source/music_viz.cpp source/language.cpp source/boot_anim.cpp \
 source/screenshot.cpp source/art.cpp source/lockup.cpp source/app.cpp source/guide.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp \
 source/screensaver.cpp source/livetv_view.cpp source/input.cpp source/input_model.cpp \
 source/keyboard_model.cpp source/keyboard_view.cpp source/downloads.cpp source/local_media.cpp \
 vendor/ps3/source/api/media_sources.cpp vendor/ps3/source/api/api_livetv.cpp vendor/ps3/source/api/livetv.cpp vendor/ps3/source/api/api_userdata.cpp \
 build/readme_*.o -lz -lpthread -o build/readme-shots
export JF_TEST_DATA="$PWD/build/readme-data"
mkdir -p "$JF_TEST_DATA"
./build/readme-shots "$@"
