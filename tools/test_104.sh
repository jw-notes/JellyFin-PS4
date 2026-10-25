#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -Wall -Wextra -Isource -Ivendor/ps3/source/api -Ivendor/ps3/source/util tests/playback_input_104.cpp source/avplayer_source.cpp source/playback_model.cpp source/input_model.cpp -o build/playback-input-104
./build/playback-input-104
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/util)
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-function "${flags[@]}" tests/live_integration_104.cpp tests/http_host.cpp source/app.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp build/test_version_summary.o -o build/live-integration-104
export JF_TEST_DATA="$PWD/build/live-test-data-104"
mkdir -p "$JF_TEST_DATA"
./build/live-integration-104 "$@"
