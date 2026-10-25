#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host-sdk
if [[ ! -e build/host-sdk/orbis ]]; then ln -s ../../tools/OpenOrbis/PS4Toolchain/include/orbis build/host-sdk/orbis; fi
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util -Ibuild/host-sdk)
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
common=(tests/http_host.cpp source/app.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp vendor/ps3/source/api/api_userdata.cpp build/test_version_summary.o)
g++ -std=c++17 -O2 "${flags[@]}" tests/large_library_106.cpp "${common[@]}" -o build/large-library-106
python3 tools/run_large_library_106.py
g++ -std=c++17 -O2 "${flags[@]}" -Dmain=ps4_application_main -c source/main.cpp -o build/main-navigation-106.o
g++ -std=c++17 -O2 "${flags[@]}" tests/navigation_106.cpp build/main-navigation-106.o source/input.cpp source/input_model.cpp source/render.cpp source/ui_view.cpp source/screenshot.cpp source/art.cpp source/facts.cpp "${common[@]}" -o build/navigation-106
export JF_TEST_DATA="$PWD/build/nav-test-106"
mkdir -p "$JF_TEST_DATA"
./build/navigation-106 "$@"
