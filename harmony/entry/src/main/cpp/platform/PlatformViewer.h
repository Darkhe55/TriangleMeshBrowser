// browser/harmony/entry/src/main/cpp/platform/PlatformViewer.h
// HarmonyOS 渲染宿主: 在 XComponent 的 EGL/GLES3 上下文上驱动渲染帧。
// 复用桌面工程平台无关的模型/渲染/UI 内核 (Procedural / MeshRenderer / Shader / Panel)。
#pragma once

#include "InputState.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace prism {

class Mesh;
class MeshRenderer;
class Shader;

#ifdef PRISM_HAVE_IMGUI
class Panel;
struct ViewState;
struct UiRequest;
#endif

class PlatformViewer {
public:
    PlatformViewer();
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
    void applyInput(InputState& input);
    void renderScene(int w, int h);

#ifdef PRISM_HAVE_IMGUI
    void initImGui();
    void feedImGui(const std::vector<PointerEvent>& pointers, float scroll);
    void drawUI();
#endif

    bool ready_ = false;

    // 内核对象 (完整定义在 .cpp, 前置声明避免头文件扩散)
    std::unique_ptr<Mesh>         mesh_;
    std::unique_ptr<MeshRenderer> renderer_;
    std::unique_ptr<Shader>       shader_;

#ifdef PRISM_HAVE_IMGUI
    std::unique_ptr<ViewState>    uiState_;
    std::unique_ptr<UiRequest>    uiRequest_;
    std::unique_ptr<Panel>        panel_;
    bool  imguiReady_ = false;
#endif

    // 轨道相机
    float yaw_   = 0.7f;
    float pitch_ = 0.45f;
    float dist_  = 4.2f;
    float spin_  = 0.f;      // 模型自转角
    float lastTime_ = 0.f;
};

} // namespace prism
