#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export OO_PS4_TOOLCHAIN="$PWD/tools/OpenOrbis/PS4Toolchain"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
SDK="$OO_PS4_TOOLCHAIN"
RELEASE_DIR="dist/$(python3 -c 'import json;print(json.load(open("version.json"))["release_directory"])')"
mkdir -p "$RELEASE_DIR"
LD="$PWD/tools/llvm-local/usr/bin/ld.lld-18"
mkdir -p build/obj build/pkg/sce_sys build/pkg/themes dist
PORTLIBS="$PWD/tools/ps4-portlibs-107/opt/pacbrew/ps4/openorbis/usr"
BASELIBS="$PWD/tools/pacbrew-base-107/lib"
APP_VER=$(python3 -c 'import json;print(json.load(open("version.json"))["app_version"])')
flags=("-DJF_BUILD_VERSION=\"$APP_VER\"" -I"$PORTLIBS/include" --target=x86_64-pc-freebsd12-elf -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -fPIC -funwind-tables -fno-exceptions -fno-rtti -O2 -Wall -Wextra -Wno-unused-function -isysroot "$SDK" -isystem "$SDK/include" -isystem "$SDK/include/c++/v1" -Isource -Isource/compat -Ivendor/ps3/source/api -Ivendor/ps3/source/net -Ivendor/ps3/source/ui -Ivendor/ps3/source/gfx -Ivendor/ps3/source/util)
sources=(source/core.cpp source/playback_model.cpp source/input_model.cpp source/input.cpp source/ui_view.cpp source/boot_anim.cpp source/screensaver.cpp source/livetv_view.cpp source/screenshot.cpp source/keyboard_model.cpp source/keyboard_view.cpp source/keyboard.cpp source/auth.cpp source/login_ui.cpp source/render.cpp source/art.cpp source/facts.cpp source/app.cpp source/theme.cpp source/http_ps4.cpp source/platform_ps4.cpp source/crash.cpp source/player_mpv.cpp source/main.cpp vendor/ps3/source/api/media_sources.cpp vendor/ps3/source/api/api_userdata.cpp vendor/ps3/source/api/api_livetv.cpp vendor/ps3/source/api/livetv.cpp)
objects=()
for file in "${sources[@]}"; do
 obj="build/obj/$(basename "${file%.*}").o"
 echo "CXX $file"
 clang++-18 "${flags[@]}" -std=c++17 -c "$file" -o "$obj"
 objects+=("$obj")
done
clang-18 "${flags[@]}" -c vendor/ps3/source/api/version_summary.c -o build/obj/version_summary.o
clang-18 "${flags[@]}" -c vendor/ps3/source/api/livetv_prefs.c -o build/obj/livetv_prefs.o
"$LD" -m elf_x86_64 -pie --script "$SDK/link.x" --eh-frame-hdr -L"$PORTLIBS/lib" -L"$BASELIBS" -L"$SDK/lib" "$SDK/lib/crt1.o" "${objects[@]}" build/obj/version_summary.o build/obj/livetv_prefs.o -lc++ --start-group -lmpv -lavformat -lavcodec -lavfilter -lswscale -lswresample -lavutil -lass -lfribidi -lfreetype -lpng16 -lbz2 -lz -lmbedtls -lmbedx509 -lmbedcrypto -ldav1d -lsamplerate -lSDL2 -lc -lm -lpthread -lclang_rt.builtins-x86_64 --end-group -lkernel -lSceLibcInternal -lSceSysUtil -lScePigletv2VSH -lSceVideoOut -lScePad -lSceUserService -lSceSysmodule -lSceNet -lSceNetCtl -lSceHttp -lSceSsl -lSceAudioOut -lSceSystemService -o build/JellyFin-PS4.elf
"$SDK/bin/linux/create-fself" -in=build/JellyFin-PS4.elf -out=build/JellyFin-PS4.oelf --eboot build/pkg/eboot.bin --paid 0x3800000000000011
python3 tools/package.py
cd build/pkg
"$SDK/bin/linux/create-gp4" -out project.gp4 --content-id=IV0000-JFPS40000_00-JELLYFINPS400000 --files "$(find . -type f ! -name project.gp4 -printf '%P ' | sort)"
cd ../..
python3 tools/fix_gp4.py
cd build/pkg
if [[ -f /proc/sys/fs/binfmt_misc/WSLInterop ]]; then
 export DOTNET_ROLL_FORWARD=LatestMajor
 export WSLENV="${WSLENV:+$WSLENV:}DOTNET_ROLL_FORWARD"
  "$SDK/bin/windows/PkgTool.Core.exe" pkg_build project.gp4 "../../$RELEASE_DIR"
else
 "$SDK/bin/linux/PkgTool.Core" pkg_build project.gp4 "../../$RELEASE_DIR"
fi
