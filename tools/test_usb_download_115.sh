#!/usr/bin/env bash
# JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
# GPL-3.0-or-later, with the attribution terms in NOTICE.
# See tests/usb_download_115.cpp. Arguments: a short real video file, and a video
# with its index at the end (a .mov). Nothing here touches the console.
set -euo pipefail
cd "$(dirname "$0")/.."
media="${1:?a short video file}"
mov="${2:?a .mov with its index at the end}"
flags=(-DHOST_TEST -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/util)
mkdir -p build
gcc "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/test_version_summary.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n.c -o build/test_i18n.o
gcc "${flags[@]}" -c vendor/ps3/source/i18n/i18n_tables.c -o build/test_i18n_tables.o
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-function "${flags[@]}" tests/usb_download_115.cpp source/player_mpv.cpp source/app.cpp source/language.cpp source/guide.cpp source/downloads.cpp source/local_media.cpp source/playback_model.cpp source/auth.cpp source/core.cpp source/theme.cpp vendor/ps3/source/api/media_sources.cpp vendor/ps3/source/api/api_userdata.cpp build/test_version_summary.o $(pkg-config --cflags --libs mpv) build/test_i18n.o build/test_i18n_tables.o -lz -lpthread -o build/usb-download-115
drive="$(mktemp -d)"
export JF_TEST_DATA="$(mktemp -d)"
log="$drive.log"
: > "$log"
port=2$(( RANDOM % 9000 + 1000 ))
ln -s "$(realpath "$mov")" "$drive/$(basename "$mov")"
python3 tests/fake_goldhen_ftp.py "$port" "$drive" "$log" &
server=$!
trap 'kill "$server" 2>/dev/null || true; rm -rf "$drive" "$log" "$JF_TEST_DATA"' EXIT
sleep 0.5
./build/usb-download-115 "127.0.0.1:$port" "$drive" "$log" "$media" "$(basename "$mov")" "$server"
