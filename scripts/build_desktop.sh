#!/usr/bin/env bash
# build_desktop.sh — 桌面构建（模拟器 + 全部测试）
# 用法: scripts/build_desktop.sh [asan|fast]
set -e
cd "$(dirname "$0")/.."
MODE="${1:-asan}"
OUT=build-desktop
mkdir -p "$OUT"
SRC="src/core/fb.cpp src/core/font.cpp src/core/levelmap.cpp src/core/player.cpp \
     src/core/game.cpp src/core/textures.cpp src/core/raycaster.cpp \
     src/core/hud.cpp src/core/app.cpp src/desktop/desktop_main.cpp"
if [ "$MODE" = "asan" ]; then
    echo "== 构建（ASan + UBSan）=="
    g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
        -Wall -Wextra -Wno-unused-parameter -Isrc -Isrc/core -o "$OUT/mw" $SRC -lpthread
else
    echo "== 构建（快速，无检测器）=="
    g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
        -Isrc -Isrc/core -o "$OUT/mw" $SRC -lpthread
fi
echo "OK: $OUT/mw"
