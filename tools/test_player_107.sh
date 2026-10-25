#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/util)
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-function "${flags[@]}" tests/player_mpv_107.cpp tests/http_host.cpp source/player_mpv.cpp source/app.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp build/test_version_summary.o $(pkg-config --cflags --libs mpv) -o build/player-mpv-107
export JF_TEST_DATA="$PWD/build/player-test-107"
mkdir -p "$JF_TEST_DATA"
./build/player-mpv-107 "$@"
