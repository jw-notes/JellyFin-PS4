#!/usr/bin/env bash
# JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
# GPL-3.0-or-later, with the attribution terms in NOTICE.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util)
mkdir -p build
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n.c -o build/test_i18n.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n_tables.c -o build/test_i18n_tables.o
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-function "${flags[@]}" tests/ui_preview_113.cpp tests/http_host.cpp source/render.cpp source/ui_view.cpp source/trickplay.cpp source/buffering.cpp source/music_viz.cpp source/language.cpp source/boot_anim.cpp source/screenshot.cpp source/art.cpp source/lockup.cpp source/app.cpp source/guide.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp source/keyboard_model.cpp source/keyboard_view.cpp source/downloads.cpp source/local_media.cpp vendor/ps3/source/api/media_sources.cpp build/test_version_summary.o build/test_i18n.o build/test_i18n_tables.o -lz -lpthread -o build/ui-preview-113
export JF_TEST_DATA="$PWD/build/ui-test-113"
mkdir -p "$JF_TEST_DATA"
./build/ui-preview-113 "$@"
