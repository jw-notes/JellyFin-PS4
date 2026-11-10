#!/usr/bin/env bash
# JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
# GPL-3.0-or-later, with the attribution terms in NOTICE.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util)
g++ -std=c++17 -O2 "${flags[@]}" tests/crash_108.cpp source/crash.cpp -o build/crash-test-108-bin
./build/crash-test-108-bin
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n.c -o build/test_i18n.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n_tables.c -o build/test_i18n_tables.o
g++ -std=c++17 -O2 "${flags[@]}" tests/classic_preview_108.cpp tests/http_host.cpp source/render.cpp source/ui_view.cpp source/trickplay.cpp source/boot_anim.cpp source/screenshot.cpp source/art.cpp source/lockup.cpp source/app.cpp source/language.cpp source/guide.cpp source/downloads.cpp source/local_media.cpp source/keyboard_model.cpp source/keyboard_view.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp build/test_version_summary.o build/test_i18n.o build/test_i18n_tables.o -lz -o build/classic-preview-108
./build/classic-preview-108 "$@"
