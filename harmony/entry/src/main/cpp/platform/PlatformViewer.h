// browser/harmony/entry/src/main/cpp/platform/PlatformViewer.h
// HarmonyOS 渲染宿主: 在 XComponent 的 EGL/GLES3 上下文上驱动渲染帧。
// 负责 GL 状态初始化、每帧渲染与输入处理; 复用桌面工程平台无关的渲染/模型内核。
#pragma once

#include <cstdint>

namespace prism {

class InputState;

class PlatformViewer {
public:
    PlatformViewer() = default;
    ~PlatformViewer();

    PlatformViewer(const PlatformViewer&) = delete;
    PlatformViewer& operator=(const PlatformViewer&) = delete;

    // 在渲染线程 eglMakeCurrent 之后调用
    bool init();
    // 渲染一帧 (w/h 为当前 surface 像素尺寸)
    void frame(int w, int h, InputState& input);
    // 释放 GL 资源
    void shutdown();

private:
    void applyInput(InputState& input, float dt);
    void renderScene(int w, int h);

    bool  ready_ = false;
    float angle_ = 0.f;
    float lastTime_ = 0.f;

    // 演示几何 (首版验证渲染管线; 后续由 MeshRenderer 接管)
    std::uint32_t vao_ = 0;
    std::uint32_t vbo_ = 0;
    std::uint32_t program_ = 0;
    int uMvpLoc_ = -1;
};

} // namespace prism
