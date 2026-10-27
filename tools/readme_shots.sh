#!/usr/bin/env bash
# Render the README's screenshots on the host (build/readme/*.bmp).
# Usage: tools/readme_shots.sh [playlist.m3u]   -- the playlist names the Live TV channels.
set -euo pipefail
cd "$(dirname "$0")/.."
# The Live TV page includes the controller's button codes: that one header (it
# needs only stdint.h) is copied into a test-only folder for the host build.
mkdir -p build build/readme build/readme_inc/orbis/_types
cp tools/OpenOrbis/PS4Toolchain/include/orbis/_types/pad.h build/readme_inc/orbis/_types/pad.h
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util -Ibuild/readme_inc)
for c in vendor/ps3/source/api/version_summary.c vendor/ps3/source/api/livetv_prefs.c vendor/ps3/source/i18n/i18n.c vendor/ps3/source/i18n/i18n_tables.c; do
 gcc "${flags[@]}" -c "$c" -o "build/readme_$(basename "${c%.c}").o"
done
g++ -std=c++17 -O2 -Wall -Wno-unused-function "${flags[@]}" tests/readme_shots.cpp tests/http_host.cpp \
 source/render.cpp source/ui_view.cpp source/buffering.cpp source/music_viz.cpp source/language.cpp source/boot_anim.cpp \
 source/screenshot.cpp source/art.cpp source/app.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp \
 source/screensaver.cpp source/livetv_view.cpp source/input.cpp source/input_model.cpp \
 vendor/ps3/source/api/media_sources.cpp vendor/ps3/source/api/api_livetv.cpp vendor/ps3/source/api/livetv.cpp vendor/ps3/source/api/api_userdata.cpp \
 build/readme_*.o -lpthread -o build/readme-shots
export JF_TEST_DATA="$PWD/build/readme-data"
mkdir -p "$JF_TEST_DATA"
./build/readme-shots "$@"
