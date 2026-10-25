#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util)
g++ -std=c++17 -O2 -Wno-unused-function "${flags[@]}" tests/render_preview.cpp tests/http_host.cpp source/render.cpp source/keyboard_model.cpp source/keyboard_view.cpp source/keyboard.cpp source/app.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp build/test_version_summary.o -o build/render-preview
./build/render-preview
