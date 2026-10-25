#!/bin/bash
set -euo pipefail
cd /mnt/d/Coding-Projects/PS4/JellyFin-PS4/tools
python3 get_linker.py
dpkg-deb -x lld.deb llvm-local
DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ./OpenOrbis/PS4Toolchain/bin/linux/PkgTool.Core --help || true
