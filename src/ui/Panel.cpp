// src/ui/Panel.cpp
#include "Panel.h"
#include "../model/Procedural.h"
#include "../utils/I18n.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <cstring>

namespace prism {

void Panel::draw(ViewState& s, UiRequest& req,
                 const std::string& currentModelName,
                 std::uint32_t vertexCount, std::uint32_t triangleCount,
                 const glm::vec3& bboxMin, const glm::vec3& bboxMax,
                 bool materialsLoaded, bool hasToonSphere) {
    drawMenuBar(s, req, materialsLoaded);
    drawSidePanel(s, req, currentModelName, vertexCount, triangleCount, bboxMin, bboxMax,
                  materialsLoaded, hasToonSphere);
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

void Panel::drawMenuBar(ViewState& s, UiRequest& req, bool materialsLoaded) {
    using i18n::tr;
    if (!ImGui::BeginMainMenuBar()) return;

    // -- 设置按钮(主菜单最左) --
    if (ImGui::Button(tr("menu.settings"))) {
        ImGui::OpenPopup("##settings_popup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(tr("menu.settings"));
    if (ImGui::BeginPopup("##settings_popup")) {
        if (ImGui::MenuItem(tr("settings.showAxis"),  nullptr, s.showAxisGizmo))  s.showAxisGizmo  = !s.showAxisGizmo;
        if (ImGui::MenuItem(tr("settings.showGrid"), nullptr, s.showGrid))     s.showGrid       = !s.showGrid;
        if (ImGui::MenuItem(tr("settings.showStatusBar"),  nullptr, s.showStatusBar))  s.showStatusBar  = !s.showStatusBar;
        ImGui::Separator();
        ImGui::TextDisabled("%s", tr("settings.mouseRotation"));
        if (ImGui::MenuItem(tr("settings.invertX"),   nullptr, s.invertX)) s.invertX = !s.invertX;
        if (ImGui::MenuItem(tr("settings.invertY"),   nullptr, s.invertY)) s.invertY = !s.invertY;
        ImGui::Separator();
        ImGui::TextDisabled("%s", tr("settings.wasd"));
        if (ImGui::MenuItem(tr("settings.enableWasd"), nullptr, s.enableWASD)) s.enableWASD = !s.enableWASD;
        ImGui::SliderFloat(tr("settings.moveSpeed"), &s.moveSpeed, 0.1f, 20.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(tr("settings.wasdTip"));
        ImGui::Separator();
        ImGui::TextDisabled("%s", tr("settings.language"));
        if (ImGui::MenuItem("中文",    nullptr, i18n::language() == i18n::Language::Zh))
            i18n::setLanguage(i18n::Language::Zh);
        if (ImGui::MenuItem("English", nullptr, i18n::language() == i18n::Language::En))
            i18n::setLanguage(i18n::Language::En);
        ImGui::EndPopup();
    }
    ImGui::Separator();

    if (ImGui::BeginMenu(tr("menu.file"))) {
        if (ImGui::MenuItem(tr("file.open"), "Ctrl+O")) {
            req.openFileDialog = true;
        }
        if (ImGui::MenuItem(tr("file.exportScreenshot"))) {
            req.exportScreenshot = true;
        }
        if (ImGui::MenuItem(tr("file.exportModel"))) {
            req.exportModel = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr("file.closeModel"))) {
            req.clearModel = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr("menu.geometry"))) {
        const int n = procedural::allCount();
        static const char* const geomKeys[] = {
            "geom.0", "geom.1", "geom.2", "geom.3", "geom.4"};
        for (int i = 0; i < n; ++i) {
            // 按索引取多语言名称 (顺序与 procedural::allNames 一致)
            if (ImGui::MenuItem(tr(geomKeys[i]))) {
                req.generateGeometry = i;
            }
        }
        ImGui::EndMenu();
    }

    // 控制台子板块显示开关列表 (控制右侧面板各折叠区的显示)
    if (ImGui::BeginMenu(tr("menu.console"))) {
        ImGui::MenuItem(tr("section.modelInfo"),     nullptr, &s.sections.modelInfo);
        ImGui::MenuItem(tr("section.colors"),        nullptr, &s.sections.colors);
        ImGui::MenuItem(tr("section.lighting"),      nullptr, &s.sections.lighting);
        ImGui::MenuItem(tr("section.fog"),           nullptr, &s.sections.fog);
        ImGui::BeginDisabled(s.split != SplitMode::DiffShading);
        ImGui::MenuItem(tr("section.rightLighting"), nullptr, &s.sections.rightLighting);
        ImGui::EndDisabled();
        ImGui::MenuItem(tr("section.picking"),       nullptr, &s.sections.picking);
        ImGui::BeginDisabled(!materialsLoaded);
        ImGui::MenuItem(tr("section.pmx"),           nullptr, &s.sections.pmx);
        ImGui::EndDisabled();
        ImGui::MenuItem(tr("section.view"),          nullptr, &s.sections.view);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr("menu.help"))) {
        ImGui::MenuItem(tr("help.leftDrag"));
        ImGui::MenuItem(tr("help.panDrag"));
        ImGui::MenuItem(tr("help.scroll"));
        ImGui::MenuItem(tr("help.shortcut"));
        ImGui::MenuItem(tr("help.pick"));
        ImGui::MenuItem(tr("help.drop"));
        ImGui::MenuItem(tr("help.wasd"));
        ImGui::MenuItem(tr("help.qe"));
        ImGui::MenuItem(tr("help.space"));
        ImGui::MenuItem(tr("help.shiftWasd"));
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void Panel::drawSidePanel(ViewState& s, UiRequest& req,
                          const std::string& modelName,
                          std::uint32_t vCount, std::uint32_t tCount,
                          const glm::vec3& bboxMin, const glm::vec3& bboxMax,
                          bool materialsLoaded, bool hasToonSphere) {
    using i18n::tr;
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

    ImGui::Begin(tr("console.title"));
    // 可滚动子窗口 (WantCaptureMouse 据此屏蔽滚轮缩放,见 Viewer::scrollCallback)
    ImGui::BeginChild("##scroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if (s.sections.modelInfo &&
        ImGui::CollapsingHeader(tr("section.modelInfo"), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (modelName.empty())
            ImGui::TextDisabled("%s", tr("info.noModel"));
        else {
            ImGui::Text(tr("info.file"), modelName.c_str());
            ImGui::Text(tr("info.vertices"), vCount);
            ImGui::Text(tr("info.triangles"), tCount);
            ImGui::Text("%s", tr("info.bbox"));
            ImGui::Text("  min(%.2f, %.2f, %.2f)", bboxMin.x, bboxMin.y, bboxMin.z);
            ImGui::Text("  max(%.2f, %.2f, %.2f)", bboxMax.x, bboxMax.y, bboxMax.z);
        }
    }

    if (s.sections.colors &&
        ImGui::CollapsingHeader(tr("section.colors"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3(tr("colors.baseColor"), &s.baseColor.x);
        ImGui::ColorEdit3(tr("colors.fogColor"),  &s.fogColor.x);
    }

    if (s.sections.lighting &&
        ImGui::CollapsingHeader(tr("section.lighting"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3(tr("lighting.lightColor"), &s.lightColor.x);
        ImGui::ColorEdit3(tr("lighting.fillColor"),  &s.fillColor.x);
        ImGui::SliderFloat(tr("lighting.ambient"),  &s.ambient, 0.f, 1.f);
        ImGui::SliderFloat(tr("lighting.fill"),     &s.fillStrength, 0.f, 1.f);
        ImGui::SliderFloat(tr("lighting.specular"), &s.specStrength, 0.f, 1.f);
        ImGui::SliderFloat3(tr("lighting.lightDir"), &s.lightDir.x, -1.f, 1.f);
        if (glm::length(s.lightDir) > 1e-4f) s.lightDir = glm::normalize(s.lightDir);
    }

    if (s.sections.fog &&
        ImGui::CollapsingHeader(tr("section.fog"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox(tr("fog.enable"), &s.fogEnabled);
        ImGui::BeginDisabled(!s.fogEnabled);
        ImGui::SliderFloat(tr("fog.near"), &s.fogNear, 0.1f, 20.f);
        ImGui::SliderFloat(tr("fog.far"),  &s.fogFar,  s.fogNear + 0.1f, 100.f);
        ImGui::EndDisabled();
    }

    if (s.split == SplitMode::DiffShading && s.sections.rightLighting) {
        if (ImGui::CollapsingHeader(tr("section.rightLighting"), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3(tr("right.baseColor"), &s.altBaseColor.x);
            ImGui::SliderFloat (tr("right.ambient"), &s.altAmbient, 0.f, 1.f);
            ImGui::SliderFloat3(tr("right.lightDir"), &s.altLightDir.x, -1.f, 1.f);
            if (glm::length(s.altLightDir) > 1e-4f) s.altLightDir = glm::normalize(s.altLightDir);
            ImGui::SliderFloat(tr("right.fogNear"), &s.altFogNear, 0.1f, 20.f);
            ImGui::SliderFloat(tr("right.fogFar"),  &s.altFogFar,  s.altFogNear + 0.1f, 100.f);
        }
    }

    if (s.sections.picking && ImGui::CollapsingHeader(tr("section.picking"))) {
        if (s.highlightedFace.has_value()) {
            ImGui::Text(tr("picking.face"), s.highlightedFace.value());
            if (ImGui::Button(tr("picking.clear"))) s.highlightedFace.reset();
        } else {
            ImGui::TextDisabled("%s", tr("picking.hint"));
        }
    }

    // 材质控制 (加载含材质的模型时显示: PMX / FBX / glTF / GLB)
    if (materialsLoaded && s.sections.pmx &&
        ImGui::CollapsingHeader(tr("section.pmx"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("%s", tr("pmx.layers"));
        ImGui::Checkbox(tr("pmx.tex"),    &s.pmx.enableTex);
        // Toon/Sphere 仅对具备该数据的格式显示 (PMX)
        if (hasToonSphere) {
            ImGui::Checkbox(tr("pmx.toon"),   &s.pmx.enableToon);
            ImGui::Checkbox(tr("pmx.sphere"), &s.pmx.enableSphere);
        }
        ImGui::Separator();
        ImGui::TextDisabled("%s", tr("pmx.adjust"));
        ImGui::SliderFloat(tr("pmx.alpha"), &s.pmx.alpha, 0.0f, 1.0f);
        ImGui::Checkbox(tr("pmx.colorOverride"), &s.pmx.colorOverride);
        if (s.pmx.colorOverride) {
            ImGui::ColorEdit3(tr("pmx.overrideColor"), &s.pmx.overrideColor.x);
        }
        ImGui::Checkbox(tr("pmx.edge"), &s.pmx.showEdge);
        if (s.pmx.showEdge) {
            ImGui::SliderFloat(tr("pmx.edgeWidth"), &s.pmx.edgeWidth, 0.0f, 3.0f);
        }
    }

    // 视图控制 (原菜单栏"视图"功能移入此处)
    if (s.sections.view &&
        ImGui::CollapsingHeader(tr("section.view"), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button(tr("view.reset"))) req.resetCamera = true;
        ImGui::SameLine();
        if (ImGui::Button(tr("view.fit"))) req.fitToView = true;
        ImGui::TextDisabled("%s", tr("view.splitMode"));
        const char* splitLabels[] = { tr("view.split.off"),
                                      tr("view.split.models"),
                                      tr("view.split.shading") };
        for (int i = 0; i < 3; ++i) {
            if (i > 0) ImGui::SameLine();
            const bool cur = (static_cast<int>(s.split) == i);
            if (ImGui::RadioButton(splitLabels[i], cur)) {
                s.split = static_cast<SplitMode>(i);
            }
        }
        ImGui::TextDisabled("%s", tr("view.renderMode"));
        const char* modeLabels[] = { tr("view.mode.solid"),
                                     tr("view.mode.wire"),
                                     tr("view.mode.solidwire") };
        for (int i = 0; i < 3; ++i) {
            if (i > 0) ImGui::SameLine();
            const bool cur = (static_cast<int>(s.mode) == i);
            if (ImGui::RadioButton(modeLabels[i], cur)) {
                s.mode = static_cast<RenderMode>(i);
            }
        }
        // 点云 (有顶点无三角面) 才显示点大小滑条
        if (vCount > 0 && tCount == 0) {
            ImGui::SliderFloat(tr("view.pointSize"), &s.pointSize, 1.0f, 10.0f);
        }
    }

    ImGui::EndChild();  // ##scroll
    ImGui::End();
}

void Panel::drawStatus(const std::string& modelName) {
    using i18n::tr;
    const float displayW = ImGui::GetIO().DisplaySize.x;
    const float statusH  = 32.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetIO().DisplaySize.y - statusH),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displayW, statusH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_NoInputs);
    // 左侧标题,右侧模型名 (截断过长名称)
    std::string name = modelName.empty() ? tr("status.noModel") : modelName;
    constexpr size_t kMaxName = 60;
    if (name.size() > kMaxName) name = name.substr(0, kMaxName - 3) + "...";
    ImGui::Text("%s", tr("status.appName"));
    ImGui::SameLine(displayW * 0.30f);
    ImGui::TextDisabled("|  %s", name.c_str());
    ImGui::End();
}

} // namespace prism
