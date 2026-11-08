#!/usr/bin/env bash
# build_android.sh — 用 NDK 编译 libmidnight_ward.so（仅 arm64-v8a：现代安卓手机标准架构）
# 环境变量：NDK（必填），OUT_DIR（默认 build-android），ABIS（默认 arm64-v8a）
set -e
cd "$(dirname "$0")/.."
: "${NDK:?需要设置 NDK 环境变量指向 NDK 根目录}"
OUT_DIR="${OUT_DIR:-build-android}"
ABIS="${ABIS:-arm64-v8a}"

for ABI in $ABIS; do
    echo "== 编译 $ABI =="
    cmake -S . -B "$OUT_DIR/$ABI" \
        -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM=android-21 \
        -DANDROID_STL=c++_static \
        -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$OUT_DIR/$ABI" -- -j"$(nproc)"
done

echo "== 产物 =="
for ABI in $ABIS; do
    ls -lh "$OUT_DIR/$ABI/libmidnight_ward.so"
done
echo "OK: 全部 ABI 编译完成（单架构 arm64-v8a，体积减半）"
