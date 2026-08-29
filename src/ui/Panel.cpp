// src/ui/Panel.cpp
#include "Panel.h"
#include "../model/Procedural.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <cstring>

namespace prism {

void Panel::draw(ViewState& s, UiRequest& req,
                 const std::string& currentModelName,
                 std::uint32_t vertexCount, std::uint32_t triangleCount,
                 const glm::vec3& bboxMin, const glm::vec3& bboxMax) {
    drawMenuBar(s, req);
    drawSidePanel(s, req, currentModelName, vertexCount, triangleCount, bboxMin, bboxMax);
    drawStatus(currentModelName);

    if (!req.toast.empty()) {
        toastText_  = req.toast;
        toastTimer_ = 2.5f;
        req.toast.clear();
    }
    drawToast();
}

void Panel::drawToast() {
    if (toastTimer_ <= 0.f) return;
    toastTimer_ -= ImGui::GetIO().DeltaTime;
    // 位置:状态栏上方
    const float margin = 20.0f;
    const float fromBottom = 48.0f;
    ImGui::SetNextWindowPos(
        ImVec2(margin, ImGui::GetIO().DisplaySize.y - fromBottom),
        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("##toast", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoInputs);
    ImGui::Text("%s", toastText_.c_str());
    ImGui::End();
}

void Panel::drawMenuBar(ViewState& s, UiRequest& req) {
    if (!ImGui::BeginMainMenuBar()) return;

    // -- 设置按钮(主菜单最左) --
    if (ImGui::Button("设置")) {
        ImGui::OpenPopup("##settings_popup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("设置");
    if (ImGui::BeginPopup("##settings_popup")) {
        if (ImGui::MenuItem("显示坐标轴",  nullptr, s.showAxisGizmo))  s.showAxisGizmo  = !s.showAxisGizmo;
        if (ImGui::MenuItem("显示 XOY 网格", nullptr, s.showGrid))     s.showGrid       = !s.showGrid;
        if (ImGui::MenuItem("显示状态栏",  nullptr, s.showStatusBar))  s.showStatusBar  = !s.showStatusBar;
        ImGui::Separator();
        ImGui::TextDisabled("鼠标旋转");
        if (ImGui::MenuItem("反转 X 轴",   nullptr, s.invertX)) s.invertX = !s.invertX;
        if (ImGui::MenuItem("反转 Y 轴",   nullptr, s.invertY)) s.invertY = !s.invertY;
        ImGui::Separator();
        ImGui::TextDisabled("WASD 飞行");
        if (ImGui::MenuItem("启用 WASD 移动", nullptr, s.enableWASD)) s.enableWASD = !s.enableWASD;
        ImGui::SliderFloat("速度 (单位/秒)", &s.moveSpeed, 0.1f, 20.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("按住 Shift = 3x 加速");
        ImGui::EndPopup();
    }
    ImGui::Separator();

    if (ImGui::BeginMenu("文件")) {
        if (ImGui::MenuItem("打开模型...", "Ctrl+O")) {
            req.openFileDialog = true;
        }
        if (ImGui::MenuItem("导出截图 (PNG)...")) {
            req.exportScreenshot = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("关闭模型")) {
            req.clearModel = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("几何")) {
        const char* const* names = procedural::allNames();
        int n = procedural::allCount();
        for (int i = 0; i < n; ++i) {
            if (ImGui::MenuItem(names[i])) {
                req.generateGeometry = i;
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("视图")) {
        if (ImGui::MenuItem("复位相机", "F")) {
            req.resetCamera = true;
        }
        if (ImGui::MenuItem("框选模型", "Shift+F")) {
            req.fitToView = true;
        }
        ImGui::Separator();
        const char* splitLabels[] = {"单视口", "对比:不同模型", "对比:不同光照"};
        for (int i = 0; i < 3; ++i) {
            bool cur = (static_cast<int>(s.split) == i);
            if (ImGui::MenuItem(splitLabels[i], nullptr, cur)) {
                s.split = static_cast<SplitMode>(i);
            }
        }
        ImGui::Separator();
        const char* modeLabels[] = {"实体", "线框", "实体+线框"};
        for (int i = 0; i < 3; ++i) {
            bool cur = (static_cast<int>(s.mode) == i);
            if (ImGui::MenuItem(modeLabels[i], nullptr, cur)) {
                s.mode = static_cast<RenderMode>(i);
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("帮助")) {
        ImGui::MenuItem("左键拖拽:旋转");
        ImGui::MenuItem("中键/右键拖拽:平移");
        ImGui::MenuItem("滚轮:缩放");
        ImGui::MenuItem("F: 复位相机 / Shift+F: 框选");
        ImGui::MenuItem("点击模型: 高亮三角面");
        ImGui::MenuItem("拖文件到窗口: 直接打开");
        ImGui::MenuItem("WASD / 方向键: 平移视点");
        ImGui::MenuItem("Q / E: 上下");
        ImGui::MenuItem("Space: 上升 (E 等价)");
        ImGui::MenuItem("Shift + WASD: 3x 加速");
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void Panel::drawSidePanel(ViewState& s, const UiRequest& /*req*/,
                          const std::string& modelName,
                          std::uint32_t vCount, std::uint32_t tCount,
                          const glm::vec3& bboxMin, const glm::vec3& bboxMax) {
    // 自适应布局:宽度 = max(340, 屏幕宽 * 0.22),封顶 560;高度 = 屏幕高 - 菜单条 - 状态栏
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float menuH    = ImGui::GetFrameHeight();
    const float statusH  = 32.0f;                   // 底部状态栏预留
    const float margin   = 12.0f;
    float panelW = display.x * 0.22f;
    if (panelW < 340.0f) panelW = 340.0f;
    if (panelW > 560.0f) panelW = 560.0f;
    const float panelH = display.y - menuH - statusH - margin * 2.0f;

    ImGui::SetNextWindowPos(
        ImVec2(display.x - panelW - margin, menuH + margin),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);

    ImGui::Begin("控制台");
    // 可滚动子窗口
    ImGui::BeginChild("##scroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if (ImGui::CollapsingHeader("模型信息", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (modelName.empty())
            ImGui::TextDisabled("(无模型)");
        else {
            ImGui::Text("文件: %s", modelName.c_str());
            ImGui::Text("顶点数: %u", vCount);
            ImGui::Text("面  数: %u", tCount);
            ImGui::Text("包围盒:");
            ImGui::Text("  min(%.2f, %.2f, %.2f)", bboxMin.x, bboxMin.y, bboxMin.z);
            ImGui::Text("  max(%.2f, %.2f, %.2f)", bboxMax.x, bboxMax.y, bboxMax.z);
        }
    }

    if (ImGui::CollapsingHeader("颜色", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("面片颜色", &s.baseColor.x);
        ImGui::ColorEdit3("雾色",     &s.fogColor.x);
    }

    if (ImGui::CollapsingHeader("光照", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("光源色",   &s.lightColor.x);
        ImGui::ColorEdit3("补光色",   &s.fillColor.x);
        ImGui::SliderFloat("环境光",  &s.ambient, 0.f, 1.f);
        ImGui::SliderFloat("补光",    &s.fillStrength, 0.f, 1.f);
        ImGui::SliderFloat("高光",    &s.specStrength, 0.f, 1.f);
        ImGui::SliderFloat3("光源方向", &s.lightDir.x, -1.f, 1.f);
        if (glm::length(s.lightDir) > 1e-4f) s.lightDir = glm::normalize(s.lightDir);
    }

    if (ImGui::CollapsingHeader("雾效", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("启用雾效", &s.fogEnabled);
        ImGui::BeginDisabled(!s.fogEnabled);
        ImGui::SliderFloat("雾近", &s.fogNear, 0.1f, 20.f);
        ImGui::SliderFloat("雾远", &s.fogFar,  s.fogNear + 0.1f, 100.f);
        ImGui::EndDisabled();
    }

    if (s.split == SplitMode::DiffShading) {
        if (ImGui::CollapsingHeader("右视口光照", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("右面片色",   &s.altBaseColor.x);
            ImGui::SliderFloat ("右环境光", &s.altAmbient, 0.f, 1.f);
            ImGui::SliderFloat3("右光源方向", &s.altLightDir.x, -1.f, 1.f);
            if (glm::length(s.altLightDir) > 1e-4f) s.altLightDir = glm::normalize(s.altLightDir);
            ImGui::SliderFloat("右雾近", &s.altFogNear, 0.1f, 20.f);
            ImGui::SliderFloat("右雾远", &s.altFogFar,  s.altFogNear + 0.1f, 100.f);
        }
    }

    if (ImGui::CollapsingHeader("拾取")) {
        if (s.highlightedFace.has_value()) {
            ImGui::Text("高亮面: #%u", s.highlightedFace.value());
            if (ImGui::Button("取消高亮")) s.highlightedFace.reset();
        } else {
            ImGui::TextDisabled("(点击模型高亮三角面)");
        }
    }

    ImGui::EndChild();  // ##scroll
    ImGui::End();
}

void Panel::drawStatus(const std::string& modelName) {
    const float displayW = ImGui::GetIO().DisplaySize.x;
    const float statusH  = 32.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetIO().DisplaySize.y - statusH),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displayW, statusH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_NoInputs);
    // 左侧标题,右侧模型名
    std::string name = modelName.empty() ? "未加载" : modelName;
    // 截断过长模型名
    constexpr size_t kMaxName = 60;
    if (name.size() > kMaxName) name = name.substr(0, kMaxName - 3) + "...";
    ImGui::Text("  棱镜模型查看器");
    ImGui::SameLine(displayW * 0.30f);
    ImGui::TextDisabled("|  %s", name.c_str());
    ImGui::End();
}

} // namespace prism
