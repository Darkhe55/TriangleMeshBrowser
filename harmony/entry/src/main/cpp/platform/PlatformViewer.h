// browser/harmony/entry/src/main/cpp/platform/PlatformViewer.h
// HarmonyOS 渲染宿主: 在 XComponent 的 EGL/GLES3 上下文上驱动渲染帧。
// 复用桌面工程平台无关的模型/渲染/UI 内核 (Procedural / MeshRenderer / Shader / Panel)。
#pragma once

#include "InputState.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
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

    // ---------------- 与 ArkTS 壳交互 ----------------
    // 以下方法均为线程安全, 可在 UI 线程(NAPI)与渲染线程之间调用。

    // ArkTS 文件选择器返回后, 提交沙箱内的文件路径 (保留原扩展名)
    void submitModelPath(const std::string& path);

    // ArkTS 轮询: 是否有一项"打开文件"请求待处理 (取出即清除)
    bool takeOpenFileRequest();

    // ArkTS 轮询: 是否生成了新截图 (取出即清除, 返回沙箱内 .png 路径)
    bool takeScreenshotResult(std::string& outPath);

    // ArkTS 告知应用沙箱目录 (截图/导出写入此处)
    void setSandboxDir(const std::string& dir);

    // 最近一次错误信息 (取出即清除)
    std::string takeLastError();

private:
    // UI 请求 -> 具体动作 (渲染线程)
    void handleUiRequest();
    void consumePendingModel();
    void requestScreenshot();
    void exportScreenshot(int w, int h);
    void exportModelToFile();
    void generateGeometry(int kind);
    void fitCamera();
    void clearModel();
    void setToast(const std::string& msg);

    void applyInput(InputState& input);
    // 触摸手势: 单指拖拽旋转, 双指捏合缩放 + 旋转
    void applyGestures(const std::vector<PointerEvent>& pointers, float scroll);
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

    // ---- 跨线程状态 ----
    std::mutex        ioMutex_;                    // 保护下列跨线程字符串
    std::atomic<bool> openFileRequested_{false};   // Native -> ArkTS: 请求选文件
    std::atomic<bool> screenshotDone_{false};      // Native -> ArkTS: 截图已写出
    std::string       screenshotPath_;             // 受 ioMutex_ 保护
    std::string       sandboxDir_;                 // 受 ioMutex_ 保护
    std::string       pendingModelPath_;           // 受 ioMutex_ 保护
    std::string       lastError_;                  // 受 ioMutex_ 保护
    bool              hasPendingModel_ = false;    // 受 ioMutex_ 保护
    bool              screenshotRequested_ = false; // 渲染线程内使用

    // 轨道相机
    float yaw_   = 0.7f;
    float pitch_ = 0.45f;
    float dist_  = 4.2f;
    float spin_  = 0.f;      // 模型自转角
    float lastTime_ = 0.f;

    // ---- 触摸手势状态 (仅渲染线程访问) ----
    std::map<int, std::pair<float, float>> touches_;   // 触点 id -> 屏幕坐标
    float lastPinchDist_  = 0.f;    // 上一帧双指间距
    float lastPinchAngle_ = 0.f;    // 上一帧双指连线角度
    bool  pinchActive_    = false;  // 是否已建立双指基线
    float lastDragX_ = 0.f;         // 上一帧单指位置
    float lastDragY_ = 0.f;
    bool  dragValid_ = false;
};

} // namespace prism
