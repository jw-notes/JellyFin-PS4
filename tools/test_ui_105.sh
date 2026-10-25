#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util)
g++ -std=c++17 -O2 -Wall -Wextra -Isource tests/avplayer_module_test.cpp source/avplayer_module.cpp -o build/avplayer-module-test
./build/avplayer-module-test
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
g++ -std=c++17 -O2 -Wno-unused-function "${flags[@]}" tests/ui_preview_105.cpp tests/http_host.cpp source/render.cpp source/ui_view.cpp source/screenshot.cpp source/art.cpp source/app.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp build/test_version_summary.o -o build/ui-preview-105
./build/ui-preview-105 "$@"
