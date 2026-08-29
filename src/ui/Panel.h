// src/ui/Panel.h
// ImGui UI 面板 - 颜色 / 光照 / 雾 / 渲染模式 / 文件菜单 / 双视口切换
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <cstdint>

namespace prism {

// 渲染模式
enum class RenderMode {
    Solid      = 0,
    Wireframe  = 1,
    SolidWire  = 2,   // 实体 + 线框叠加
};

// 双视口模式
enum class SplitMode {
    Off,         // 单视口
    DiffModels,  // 左右各加载不同模型
    DiffShading, // 左右同一模型不同光照/颜色对比
};

// 一次操作请求(由 Panel 发出,Viewer 消费)
struct UiRequest {
    // 文件
    bool openFileDialog = false;
    bool exportScreenshot = false;
    bool exportModel = false;
    // 几何
    int  generateGeometry = -1;  // -1 = none, 0..4 = procedural
    bool fitToView = false;
    bool clearModel = false;
    // 视口
    bool toggleSplit = false;
    bool resetCamera = false;
    // 错误/信息
    std::string toast;  // 临时显示
};

// PMX 材质显示控制 (仅对含材质的 PMX 模型生效)
struct PmxViewSettings {
    // 材质层开关
    bool enableTex    = true;
    bool enableToon   = true;
    bool enableSphere = true;
    // 基础调整
    float alpha = 1.0f;
    bool  colorOverride = false;
    glm::vec3 overrideColor{1.f, 1.f, 1.f};
    bool  showEdge  = false;
    float edgeWidth = 1.0f;
};

struct ViewState {
    RenderMode  mode = RenderMode::SolidWire;
    SplitMode   split = SplitMode::Off;

    // 颜色 (左视口)
    glm::vec3 baseColor   = {0.66f, 0.85f, 0.92f};  // #A8D8EA
    glm::vec3 fogColor    = {0.12f, 0.12f, 0.15f};
    glm::vec3 lightColor  = {1.00f, 0.95f, 0.90f};
    glm::vec3 fillColor   = {0.50f, 0.70f, 0.95f};
    glm::vec3 lightDir    = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.7f));
    float ambient        = 0.25f;
    float fillStrength   = 0.15f;
    float specStrength   = 0.40f;
    bool  fogEnabled     = true;
    float fogNear        = 4.0f;
    float fogFar         = 14.0f;

    // 右视口(仅 DiffShading 模式生效)
    glm::vec3 altBaseColor  = {0.95f, 0.78f, 0.62f};  // 暖色
    glm::vec3 altLightDir   = glm::normalize(glm::vec3(0.6f, -0.8f, 0.4f));
    float     altAmbient    = 0.15f;
    float     altFogNear    = 4.0f;
    float     altFogFar     = 18.0f;

    // 拾取
    std::optional<std::uint32_t> highlightedFace;

    // PMX 材质控制
    PmxViewSettings pmx;

    // 设置:鼠标反转(默认反转 X,Y 不反转)
    bool invertX = true;
    bool invertY = false;

    // 设置:WASD 飞行
    bool  enableWASD  = true;
    float moveSpeed   = 3.0f;     // 单位/秒(Shift 加速 3x)

    // 设置:辅助显示
    bool showAxisGizmo   = true;
    bool showGrid        = true;
    bool showStatusBar   = true;
};

class Panel {
public:
    // 必须在 ImGui 新帧内调用; pmxLoaded = 当前模型含 PMX 材质数据
    void draw(ViewState& s, UiRequest& req,
              const std::string& currentModelName,
              std::uint32_t vertexCount, std::uint32_t triangleCount,
              const glm::vec3& bboxMin, const glm::vec3& bboxMax,
              bool pmxLoaded);

    // 文件路径(由 openFileDialog 触发后保存)
    std::string& pickedFilePath() noexcept { return pickedPath_; }

private:
    std::string pickedPath_;
    std::string toastText_;
    float toastTimer_ = 0.f;
    void drawMenuBar(ViewState& s, UiRequest& req);
    void drawSidePanel(ViewState& s, const UiRequest& req,
                       const std::string& modelName,
                       std::uint32_t vCount, std::uint32_t tCount,
                       const glm::vec3& bboxMin, const glm::vec3& bboxMax,
                       bool pmxLoaded);
    void drawStatus(const std::string& modelName);
    void drawToast();
};

} // namespace prism
