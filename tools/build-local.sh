#!/usr/bin/env bash
# JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
# GPL-3.0-or-later, with the attribution terms in NOTICE.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/build.sh > logs/build-latest.log 2>&1
