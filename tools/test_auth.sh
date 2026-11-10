#!/usr/bin/env bash
# JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
# GPL-3.0-or-later, with the attribution terms in NOTICE.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/util)
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n.c -o build/test_i18n.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n_tables.c -o build/test_i18n_tables.o
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-function "${flags[@]}" tests/auth_integration.cpp tests/http_host.cpp source/app.cpp source/language.cpp source/guide.cpp source/downloads.cpp source/local_media.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp vendor/ps3/source/api/api_userdata.cpp build/test_version_summary.o build/test_i18n.o build/test_i18n_tables.o -lz -o build/auth-integration-test
export JF_TEST_DATA="$PWD/build/auth-test-data"
mkdir -p "$JF_TEST_DATA"
./build/auth-integration-test "$@"
