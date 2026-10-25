#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
sdk="$PWD/tools/OpenOrbis/PS4Toolchain"
libs="$PWD/tools/ps4-portlibs-107/opt/pacbrew/ps4/openorbis/usr"
flags="--target=x86_64-pc-freebsd12-elf -isysroot $sdk -isystem $sdk/include -O2 -fPIC -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE"
cd tools/ps4-portlibs-107/sources/zlib-1.3.1
CC="clang-18 $flags" AR=llvm-ar-18 RANLIB=llvm-ranlib-18 ./configure --static --prefix="$libs"
make -j8 libz.a
cp libz.a "$libs/lib/"
cp zlib.h zconf.h "$libs/include/"
cd ../bzip2-1.0.8
make -j8 CC=clang-18 AR=llvm-ar-18 RANLIB=llvm-ranlib-18 CFLAGS="$flags -Wall" libbz2.a
cp libbz2.a "$libs/lib/"
cp bzlib.h "$libs/include/"
