// browser/harmony/entry/src/main/cpp/platform/EglCore.cpp
#include "EglCore.h"

#include <hilog/log.h>
#include <native_window/external_window.h>

#undef LOG_TAG
#define LOG_TAG "PrismViewer"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO,  0x0000, LOG_TAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0x0000, LOG_TAG, __VA_ARGS__)

namespace prism {

namespace {
constexpr EGLint kAttribs[] = {
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE,        8,
    EGL_GREEN_SIZE,      8,
    EGL_BLUE_SIZE,       8,
    EGL_ALPHA_SIZE,      8,
    EGL_DEPTH_SIZE,      24,
    EGL_STENCIL_SIZE,    8,
    EGL_SAMPLE_BUFFERS,  1,   // MSAA 4x
    EGL_SAMPLES,         4,
    EGL_NONE
};
} // namespace

EglCore::~EglCore() {
    destroy();
}

bool EglCore::init(void* nativeWindow, int width, int height) {
    if (!nativeWindow) {
        LOGE("EglCore::init: null native window");
        return false;
    }
    window_ = nativeWindow;
    width_  = width;
    height_ = height;

    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }
    EGLint major = 0, minor = 0;
    if (eglInitialize(display_, &major, &minor) != EGL_TRUE) {
        LOGE("eglInitialize failed");
        destroy();
        return false;
    }
    LOGI("EGL %{public}d.%{public}d initialized", major, minor);

    EGLint numConfigs = 0;
    if (eglChooseConfig(display_, kAttribs, &config_, 1, &numConfigs) != EGL_TRUE || numConfigs < 1) {
        LOGE("eglChooseConfig(MSAA) failed, retry without MSAA");
        // 回退: 无 MSAA
        constexpr EGLint kFallback[] = {
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE,        8,
            EGL_GREEN_SIZE,      8,
            EGL_BLUE_SIZE,       8,
            EGL_ALPHA_SIZE,      8,
            EGL_DEPTH_SIZE,      24,
            EGL_STENCIL_SIZE,    8,
            EGL_NONE
        };
        if (eglChooseConfig(display_, kFallback, &config_, 1, &numConfigs) != EGL_TRUE || numConfigs < 1) {
            LOGE("eglChooseConfig failed");
            destroy();
            return false;
        }
    }

    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, ctxAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext(GLES3) failed");
        destroy();
        return false;
    }

    if (!createSurface()) {
        destroy();
        return false;
    }
    if (!makeCurrent()) {
        destroy();
        return false;
    }
    return true;
}

bool EglCore::createSurface() {
    if (!window_ || display_ == EGL_NO_DISPLAY || config_ == nullptr) return false;
    if (surface_ != EGL_NO_SURFACE) {
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
    }
    // OHOS 上 EGLNativeWindowType 为 unsigned long(整型句柄), 需 reinterpret_cast 转换
    surface_ = eglCreateWindowSurface(display_, config_,
                                      reinterpret_cast<EGLNativeWindowType>(window_), nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%{public}x", eglGetError());
        return false;
    }
    eglQuerySurface(display_, surface_, EGL_WIDTH,  &width_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height_);
    LOGI("EGL surface created %{public}dx%{public}d", width_, height_);
    return true;
}

bool EglCore::makeCurrent() {
    if (display_ == EGL_NO_DISPLAY || surface_ == EGL_NO_SURFACE || context_ == EGL_NO_CONTEXT)
        return false;
    if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
        LOGE("eglMakeCurrent failed: 0x%{public}x", eglGetError());
        return false;
    }
    return true;
}

bool EglCore::swapBuffers() {
    if (display_ == EGL_NO_DISPLAY || surface_ == EGL_NO_SURFACE) return false;
    return eglSwapBuffers(display_, surface_) == EGL_TRUE;
}

bool EglCore::resize(int width, int height) {
    width_  = width;
    height_ = height;
    if (display_ == EGL_NO_DISPLAY) return false;
    // 通知 EGL 缓冲尺寸变化, 并重建表面
    return createSurface() && makeCurrent();
}

void EglCore::destroy() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface_ != EGL_NO_SURFACE) { eglDestroySurface(display_, surface_); surface_ = EGL_NO_SURFACE; }
        if (context_ != EGL_NO_CONTEXT) { eglDestroyContext(display_, context_); context_ = EGL_NO_CONTEXT; }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
    config_ = nullptr;
    window_ = nullptr;
}

} // namespace prism
