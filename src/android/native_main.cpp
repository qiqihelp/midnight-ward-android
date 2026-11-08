// native_main.cpp — Android 原生入口（NativeActivity + native_app_glue）。
// 全部逻辑与渲染都在 src/core（平台无关，桌面已通过 ASan 全量测试），
// 本文件只做：事件循环 / 触摸与按键转发 / 帧缓冲上屏 / 资源与存档路径。
#include <android_native_app_glue.h>
#include "core/app.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/window.h>
#include <android/configuration.h>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>

#define LOG_TAG "MidnightWard"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static App* g_app = nullptr;
static android_app* g_android_app = nullptr;
static bool g_hasWindow = false;
static float g_density = 2.f;

static double nowSec() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static std::vector<uint8_t> loadAsset(const char* name) {
    std::vector<uint8_t> out;
    if (!g_app) return out;
    // 通过全局 app 拿不到 assetManager —— 用 android_app 静态指针
    android_app* app = g_android_app;
    if (!app || !app->activity || !app->activity->assetManager) return out;
    AAsset* a = AAssetManager_open(app->activity->assetManager, name,
                                   AASSET_MODE_BUFFER);
    if (!a) {
        LOGE("asset missing: %s", name);
        return out;
    }
    off_t n = AAsset_getRemainingLength(a);
    if (n > 0 && n < 64 * 1024 * 1024) {
        out.resize((size_t)n);
        if (AAsset_read(a, out.data(), (size_t)n) != (int)n) out.clear();
    }
    AAsset_close(a);
    return out;
}

static void ensureApp(android_app* state) {
    if (g_app) return;
    App::Env env;
    env.loadAsset = loadAsset;
    if (state->activity && state->activity->internalDataPath)
        env.savePath = std::string(state->activity->internalDataPath) + "/save.bin";
    int w = 1280, h = 720;
    if (state->window) {
        w = ANativeWindow_getWidth(state->window);
        h = ANativeWindow_getHeight(state->window);
    }
    int dens = state->config ? AConfiguration_getDensity(state->config) : 0;
    if (dens <= 0 || dens > 640) dens = 320;
    g_density = (float)dens / 160.f;
    if (g_density < 1.f) g_density = 1.f;
    if (g_density > 4.f) g_density = 4.f;
    env.w = w > 0 ? w : 1280;
    env.h = h > 0 ? h : 720;
    env.density = g_density;
    try {
        g_app = new App(env);
    } catch (...) {
        LOGE("App 构造失败");
        g_app = nullptr;
    }
    if (g_app) {
        LOGI("App ready %dx%d density=%.2f font=%d map=%d", env.w, env.h,
             env.density, (int)g_app->fontOk(), (int)g_app->mapValid());
    }
}

static void handle_cmd(android_app* state, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            g_hasWindow = state->window != nullptr;
            if (state->window)
                ANativeWindow_setBuffersGeometry(state->window, 0, 0,
                                                 WINDOW_FORMAT_RGBA_8888);
            ensureApp(state);
            break;
        }
        case APP_CMD_TERM_WINDOW:
            g_hasWindow = false;
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED: {
            if (g_app && state->window) {
                int w = ANativeWindow_getWidth(state->window);
                int h = ANativeWindow_getHeight(state->window);
                int dens = state->config ? AConfiguration_getDensity(state->config) : 0;
                if (dens > 0 && dens <= 640) {
                    float d = (float)dens / 160.f;
                    if (d >= 1.f && d <= 4.f) g_density = d;
                }
                if (w > 0 && h > 0) g_app->resize(w, h, g_density);
            }
            break;
        }
        case APP_CMD_PAUSE:
            if (g_app) g_app->onPause();
            break;
        case APP_CMD_RESUME:
            if (g_app) g_app->onResume();
            break;
        case APP_CMD_DESTROY:
            if (g_app) {
                g_app->onPause();
                delete g_app;
                g_app = nullptr;
            }
            g_hasWindow = false;
            break;
        default:
            break;
    }
}

static int32_t handle_input(android_app* state, AInputEvent* event) {
    if (!g_app) return 0;
    (void)state;
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        if (AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN
            && AKeyEvent_getKeyCode(event) == AKEYCODE_BACK) {
            return g_app->back() ? 1 : 0; // 游戏中吞掉；其余界面交给系统退出
        }
        return 0;
    }
    if (type != AINPUT_EVENT_TYPE_MOTION) return 0;
    // NDK 只提供 AMotionEvent_getAction，手动拆掩码与指针索引
    int32_t action = AMotionEvent_getAction(event);
    int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
    int32_t index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                  >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    switch (actionMasked) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            int32_t id = AMotionEvent_getPointerId(event, index);
            g_app->touchDown(id, AMotionEvent_getX(event, index),
                             AMotionEvent_getY(event, index));
            return 1;
        }
        case AMOTION_EVENT_ACTION_MOVE: {
            size_t n = AMotionEvent_getPointerCount(event);
            for (size_t i = 0; i < n; i++) {
                int32_t id = AMotionEvent_getPointerId(event, (int)i);
                g_app->touchMove(id, AMotionEvent_getX(event, (int)i),
                                 AMotionEvent_getY(event, (int)i));
            }
            return 1;
        }
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            int32_t id = AMotionEvent_getPointerId(event, index);
            g_app->touchUp(id, AMotionEvent_getX(event, index),
                           AMotionEvent_getY(event, index));
            return 1;
        }
        case AMOTION_EVENT_ACTION_CANCEL:
            g_app->touchCancel();
            return 1;
        default:
            return 1;
    }
}

static void drawFrame(android_app* state) {
    if (!g_app || !state->window) return;
    g_app->render();
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(state->window, &buf, nullptr) != 0) return;
    do {
        if (!buf.bits) break;
        int w = buf.width, h = buf.height;
        if (w <= 0 || h <= 0) break;
        if (w != g_app->width() || h != g_app->height()) {
            g_app->resize(w, h, g_density);
            g_app->render();
        }
        const uint8_t* src = g_app->pixels();
        if (!src) break;
        size_t copy = (size_t)w * 4;
        int32_t stride = buf.stride * 4;
        uint8_t* dstBase = (uint8_t*)buf.bits;
        for (int y = 0; y < h; y++) {
            std::memcpy(dstBase + (size_t)y * stride, src + (size_t)y * copy, copy);
        }
    } while (false);
    ANativeWindow_unlockAndPost(state->window);
}

// 排空所有待处理事件（ALooper_pollAll 已被新 NDK 移除，改用 pollOnce 循环）。
// 返回 false 表示应用请求退出。
static bool drainEvents(android_app* state, int timeoutMillis) {
    while (true) {
        int events = 0;
        struct android_poll_source* source = nullptr;
        int r = ALooper_pollOnce(timeoutMillis, nullptr, &events,
                                 (void**)&source);
        if (r >= 0 && source) {
            source->process(state, source);
            if (state->destroyRequested) return false;
            timeoutMillis = 0; // 继续排空剩余事件
            continue;
        }
        if (r >= 0) { // 有回调处理了事件但无 source
            if (state->destroyRequested) return false;
            timeoutMillis = 0;
            continue;
        }
        // ALOOPER_POLL_WAKE / TIMEOUT / ERROR：本次排空结束
        return !state->destroyRequested;
    }
}

void android_main(struct android_app* state) {
    g_android_app = state;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;
    // 全屏沉浸 + 屏幕常亮
    if (state->activity) {
        ANativeActivity_setWindowFlags(state->activity,
                                       AWINDOW_FLAG_FULLSCREEN
                                           | AWINDOW_FLAG_KEEP_SCREEN_ON,
                                       0);
    }
    LOGI("native_main 启动（纯 C++ 引擎）");

    double lastFrame = nowSec();
    const double frameInterval = 1.0 / 30.0;
    try {
        while (true) {
            // 有窗口时保持 30fps 帧率节拍；无窗口时阻塞等待事件
            double remaining = frameInterval - (nowSec() - lastFrame);
            int timeout = (g_app && g_hasWindow)
                              ? (remaining > 0.001 ? (int)(remaining * 1000.0) : 0)
                              : -1;
            if (!drainEvents(state, timeout)) break;
            if (g_app && g_hasWindow) {
                double now = nowSec();
                double dt = now - lastFrame;
                if (dt >= frameInterval) {
                    g_app->tick((float)dt);
                    drawFrame(state);
                    lastFrame = now;
                }
            } else {
                lastFrame = nowSec();
            }
        }
    } catch (const std::exception& e) {
        LOGE("异常退出: %s", e.what());
    } catch (...) {
        LOGE("未知异常退出");
    }
    if (g_app) {
        g_app->onPause();
        delete g_app;
        g_app = nullptr;
    }
    g_android_app = nullptr;
}
