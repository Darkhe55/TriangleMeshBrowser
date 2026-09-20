// browser/harmony/entry/src/main/cpp/napi_init.cpp
// HarmonyOS 入口: 注册 NAPI 模块, 接管 XComponent 的 surface/输入事件,
// 在独立渲染线程上运行 EGL + OpenGL ES 3.0 渲染循环。
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <napi/native_api.h>
#include <hilog/log.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "platform/EglCore.h"
#include "platform/InputState.h"
#include "platform/PlatformViewer.h"

#undef LOG_TAG
#define LOG_TAG "PrismViewer"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO,  0x0000, LOG_TAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0x0000, LOG_TAG, __VA_ARGS__)

namespace {

prism::EglCore        g_egl;
prism::InputState     g_input;
prism::PlatformViewer g_viewer;

std::mutex            g_mutex;          // 保护 EGL/线程生命周期
std::atomic<bool>     g_running{false};
std::thread           g_renderThread;
OH_NativeXComponent*  g_xcomponent = nullptr;

// ---------------- 渲染线程 ----------------
void renderLoop() {
    if (!g_egl.makeCurrent()) {
        LOGE("renderLoop: eglMakeCurrent failed");
        return;
    }
    if (!g_viewer.init()) {
        LOGE("renderLoop: viewer init failed");
        return;
    }

    while (g_running.load()) {
        const int w = g_egl.width();
        const int h = g_egl.height();
        if (w <= 0 || h <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        g_viewer.frame(w, h, g_input);
        g_egl.swapBuffers();
    }
    g_viewer.shutdown();
    LOGI("renderLoop exited");
}

void startRenderThread() {
    if (g_running.exchange(true)) return;
    g_renderThread = std::thread(renderLoop);
}

void stopRenderThread() {
    if (!g_running.exchange(false)) return;
    if (g_renderThread.joinable()) g_renderThread.join();
}

// ---------------- XComponent 回调 ----------------
void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window) {
    uint64_t w = 0, h = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &w, &h);
    LOGI("OnSurfaceCreated %{public}llux%{public}llu",
         static_cast<unsigned long long>(w), static_cast<unsigned long long>(h));

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_egl.init(window, static_cast<int>(w), static_cast<int>(h))) {
        LOGE("EglCore init failed");
        return;
    }
    startRenderThread();
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window) {
    uint64_t w = 0, h = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &w, &h);
    LOGI("OnSurfaceChanged %{public}llux%{public}llu",
         static_cast<unsigned long long>(w), static_cast<unsigned long long>(h));
    std::lock_guard<std::mutex> lock(g_mutex);
    g_egl.resize(static_cast<int>(w), static_cast<int>(h));
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* /*component*/, void* /*window*/) {
    LOGI("OnSurfaceDestroyed");
    stopRenderThread();
    std::lock_guard<std::mutex> lock(g_mutex);
    g_egl.destroy();
}

void DispatchTouchEventCB(OH_NativeXComponent* component, void* window) {
    OH_NativeXComponent_TouchEvent touchEvent;
    if (OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent) != 0) return;
    if (touchEvent.numPoints == 0) return;

    prism::PointerAction action = prism::PointerAction::Move;
    switch (touchEvent.type) {
        case OH_NATIVEXCOMPONENT_DOWN:   action = prism::PointerAction::Down;   break;
        case OH_NATIVEXCOMPONENT_UP:     action = prism::PointerAction::Up;     break;
        case OH_NATIVEXCOMPONENT_MOVE:   action = prism::PointerAction::Move;   break;
        case OH_NATIVEXCOMPONENT_CANCEL: action = prism::PointerAction::Cancel; break;
        default: return;
    }

    // 多指手势(双指捏合缩放 / 双指旋转)需要全部触点, 因此这里推送所有点,
    // 并以数组下标作为触点 id 交给渲染线程做手势识别。
    uint32_t count = touchEvent.numPoints;
    if (count > OH_MAX_TOUCH_POINTS_NUMBER) count = OH_MAX_TOUCH_POINTS_NUMBER;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& p = touchEvent.touchPoints[i];
        g_input.pushPointer(p.x, p.y, action, OH_NATIVEXCOMPONENT_LEFT_BUTTON,
                            static_cast<int>(i));
    }
}

void DispatchMouseEventCB(OH_NativeXComponent* component, void* window) {
    OH_NativeXComponent_MouseEvent e;
    if (OH_NativeXComponent_GetMouseEvent(component, window, &e) != 0) return;

    prism::PointerAction action = prism::PointerAction::Move;
    switch (e.action) {
        case OH_NATIVEXCOMPONENT_MOUSE_PRESS:   action = prism::PointerAction::Down; break;
        case OH_NATIVEXCOMPONENT_MOUSE_RELEASE: action = prism::PointerAction::Up;   break;
        case OH_NATIVEXCOMPONENT_MOUSE_MOVE:    action = prism::PointerAction::Move; break;
        default: return;
    }
    g_input.pushPointer(e.x, e.y, action, static_cast<int>(e.button));
}

void DispatchHoverEventCB(OH_NativeXComponent* /*component*/, bool /*isHover*/) {}

void OnKeyEventCB(OH_NativeXComponent* component, void* /*window*/) {
    OH_NativeXComponent_KeyEvent* keyEvent = nullptr;
    if (OH_NativeXComponent_GetKeyEvent(component, &keyEvent) != 0 || keyEvent == nullptr) return;

    OH_NativeXComponent_KeyAction action;
    OH_NativeXComponent_KeyCode code;
    if (OH_NativeXComponent_GetKeyEventAction(keyEvent, &action) != 0) return;
    if (OH_NativeXComponent_GetKeyEventCode(keyEvent, &code) != 0) return;

    const bool pressed = (action == OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN);
    g_input.pushKey(static_cast<int>(code), pressed);
}

OH_NativeXComponent_Callback g_surfaceCallback;
OH_NativeXComponent_MouseEvent_Callback g_mouseCallback;

// ---------------- ArkTS <-> Native 桥接 ----------------
// ArkTS 侧以轮询方式与 Native 协作 (避免 threadsafe function 的复杂度):
//   1. ArkTS 每 ~200ms 调 pollOpenFileRequest(); true 则弹系统文件选择器
//   2. 选中后把文件复制进应用沙箱, 调 submitModelPath(沙箱路径)
//   3. 截图由 Native 写入沙箱, ArkTS 轮询 pollScreenshotPath() 取回并存入媒体库

napi_value JsPollOpenFileRequest(napi_env env, napi_callback_info /*info*/) {
    napi_value result = nullptr;
    napi_get_boolean(env, g_viewer.takeOpenFileRequest(), &result);
    return result;
}

napi_value JsSubmitModelPath(napi_env env, napi_callback_info info) {
    std::size_t argc = 1;
    napi_value argv[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc < 1 || argv[0] == nullptr) return nullptr;

    std::size_t len = 0;
    if (napi_get_value_string_utf8(env, argv[0], nullptr, 0, &len) != napi_ok || len == 0) {
        return nullptr;
    }
    std::string path(len + 1, '\0');
    if (napi_get_value_string_utf8(env, argv[0], path.data(), len + 1, &len) != napi_ok) {
        return nullptr;
    }
    path.resize(len);
    g_viewer.submitModelPath(path);
    return nullptr;
}

napi_value JsSetSandboxDir(napi_env env, napi_callback_info info) {
    std::size_t argc = 1;
    napi_value argv[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc < 1 || argv[0] == nullptr) return nullptr;

    std::size_t len = 0;
    if (napi_get_value_string_utf8(env, argv[0], nullptr, 0, &len) != napi_ok) return nullptr;
    std::string dir(len + 1, '\0');
    if (napi_get_value_string_utf8(env, argv[0], dir.data(), len + 1, &len) != napi_ok) {
        return nullptr;
    }
    dir.resize(len);
    g_viewer.setSandboxDir(dir);
    return nullptr;
}

napi_value JsPollScreenshotPath(napi_env env, napi_callback_info /*info*/) {
    std::string path;
    g_viewer.takeScreenshotResult(path);      // 无则保持空串
    napi_value result = nullptr;
    napi_create_string_utf8(env, path.c_str(), path.size(), &result);
    return result;
}

napi_value JsPollLastError(napi_env env, napi_callback_info /*info*/) {
    const std::string err = g_viewer.takeLastError();
    napi_value result = nullptr;
    napi_create_string_utf8(env, err.c_str(), err.size(), &result);
    return result;
}

// ---------------- NAPI 模块注册 ----------------
napi_value Init(napi_env env, napi_value exports) {
    napi_value exportInstance = nullptr;
    if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
        LOGE("get NATIVE_XCOMPONENT_OBJ failed");
        return exports;
    }
    if (napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&g_xcomponent)) != napi_ok) {
        LOGE("unwrap XComponent failed");
        return exports;
    }

    g_surfaceCallback.OnSurfaceCreated  = OnSurfaceCreatedCB;
    g_surfaceCallback.OnSurfaceChanged  = OnSurfaceChangedCB;
    g_surfaceCallback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    g_surfaceCallback.DispatchTouchEvent = DispatchTouchEventCB;
    OH_NativeXComponent_RegisterCallback(g_xcomponent, &g_surfaceCallback);

    g_mouseCallback.DispatchMouseEvent = DispatchMouseEventCB;
    g_mouseCallback.DispatchHoverEvent = DispatchHoverEventCB;
    OH_NativeXComponent_RegisterMouseEventCallback(g_xcomponent, &g_mouseCallback);

    OH_NativeXComponent_RegisterKeyEventCallback(g_xcomponent, OnKeyEventCB);

    // 暴露给 ArkTS 壳的方法
    napi_property_descriptor props[] = {
        {"pollOpenFileRequest", nullptr, JsPollOpenFileRequest, nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"submitModelPath",     nullptr, JsSubmitModelPath,     nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"setSandboxDir",       nullptr, JsSetSandboxDir,       nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"pollScreenshotPath",  nullptr, JsPollScreenshotPath,  nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"pollLastError",       nullptr, JsPollLastError,       nullptr, nullptr, nullptr,
         napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(props) / sizeof(props[0]), props);

    LOGI("NAPI Init done");
    return exports;
}

napi_module g_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = nullptr,
    .reserved = {0},
};

} // namespace

extern "C" __attribute__((constructor)) void RegisterEntryModule() {
    napi_module_register(&g_module);
}
