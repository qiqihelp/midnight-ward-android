cmake_minimum_required(VERSION 3.22.1)
project(midnight_ward LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# 纯 C++ 游戏核心 + Android 原生入口
add_library(midnight_ward SHARED
    src/core/fb.cpp
    src/core/font.cpp
    src/core/levelmap.cpp
    src/core/player.cpp
    src/core/game.cpp
    src/core/textures.cpp
    src/core/raycaster.cpp
    src/core/hud.cpp
    src/core/app.cpp
    src/android/native_main.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

target_include_directories(midnight_ward PRIVATE
    src
    ${ANDROID_NDK}/sources/android/native_app_glue)

target_compile_options(midnight_ward PRIVATE
    -Wall -Wextra -Wno-unused-parameter -fexceptions -frtti -fvisibility=hidden)

find_library(log-lib log)

target_link_libraries(midnight_ward
    android
    ${log-lib})

# 保留 NativeActivity 入口符号；兼容 16KB 页设备（Android 15+）
target_link_options(midnight_ward PRIVATE
    -u ANativeActivity_onCreate
    -Wl,-z,max-page-size=16384)
