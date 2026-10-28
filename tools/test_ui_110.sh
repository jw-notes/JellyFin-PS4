#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util)
mkdir -p build
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n.c -o build/test_i18n.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n_tables.c -o build/test_i18n_tables.o
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-function "${flags[@]}" tests/ui_preview_110.cpp tests/http_host.cpp source/render.cpp source/ui_view.cpp source/buffering.cpp source/music_viz.cpp source/language.cpp source/boot_anim.cpp source/screenshot.cpp source/art.cpp source/app.cpp source/guide.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp build/test_version_summary.o build/test_i18n.o build/test_i18n_tables.o -lz -lpthread -o build/ui-preview-110
export JF_TEST_DATA="$PWD/build/ui-test-110"
mkdir -p "$JF_TEST_DATA"
./build/ui-preview-110 "$@"
