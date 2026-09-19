// browser/harmony/entry/src/main/cpp/platform/EglCore.h
// HarmonyOS EGL 上下文封装: 由 XComponent 提供的 NativeWindow 创建 GLES3 表面。
#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cstdint>

namespace prism {

class EglCore {
public:
    EglCore() = default;
    ~EglCore();

    EglCore(const EglCore&) = delete;
    EglCore& operator=(const EglCore&) = delete;

    // 用 XComponent 的 native window 初始化 EGL 并创建 GLES3 上下文/表面
    bool init(void* nativeWindow, int width, int height);
    // 绑定上下文到当前线程
    bool makeCurrent();
    // 交换缓冲
    bool swapBuffers();
    // 销毁全部 EGL 资源
    void destroy();
    // NativeWindow 尺寸变化时重建表面
    bool resize(int width, int height);

    bool valid() const noexcept { return context_ != EGL_NO_CONTEXT && surface_ != EGL_NO_SURFACE; }
    int  width()  const noexcept { return width_; }
    int  height() const noexcept { return height_; }

private:
    bool createSurface();

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLConfig  config_  = nullptr;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    void*      window_  = nullptr;   // OHNativeWindow*
    int        width_   = 0;
    int        height_  = 0;
};

} // namespace prism
