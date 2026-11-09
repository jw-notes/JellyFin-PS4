#!/usr/bin/env bash
# See tests/subtitles_116.cpp.
#   test_subtitles_116.sh <server> <user> <password> <media dir> <idmap>
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/util)
mkdir -p build
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n.c -o build/test_i18n.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n_tables.c -o build/test_i18n_tables.o
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-function "${flags[@]}" tests/subtitles_116.cpp tests/http_host.cpp source/player_mpv.cpp source/app.cpp source/language.cpp source/guide.cpp source/downloads.cpp source/local_media.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp vendor/ps3/source/api/api_userdata.cpp build/test_version_summary.o $(pkg-config --cflags --libs mpv) build/test_i18n.o build/test_i18n_tables.o -lz -lpthread -o build/subtitles-116
export JF_TEST_DATA="$PWD/build/subtitles-test-116"
rm -rf "$JF_TEST_DATA"; mkdir -p "$JF_TEST_DATA"
./build/subtitles-116 "$@"
