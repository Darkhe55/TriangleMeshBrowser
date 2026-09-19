// browser/harmony/entry/src/main/cpp/platform/PlatformViewer.cpp
// 鸿蒙渲染宿主: 用内核 (Procedural / MeshRenderer / Shader / Panel) 渲染 + UI。
#include "PlatformViewer.h"
#include "InputState.h"

// PRISM_SRC 已在 target_include_directories 中, 故用相对源码根的路径
#include "model/Mesh.h"
#include "model/Procedural.h"
#include "renderer/MeshRenderer.h"
#include "renderer/Shader.h"

#ifdef PRISM_HAVE_IMGUI
#include "ui/Panel.h"
#include <imgui.h>
#include "imgui_impl_opengl3.h"
#endif

#include <GLES3/gl3.h>
#include <hilog/log.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <chrono>
#include <cmath>
#include <exception>
#include <memory>
#include <string>

#undef LOG_TAG
#define LOG_TAG "PrismViewer"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO,  0x0000, LOG_TAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0x0000, LOG_TAG, __VA_ARGS__)

namespace prism {

PlatformViewer::PlatformViewer() = default;

PlatformViewer::~PlatformViewer() {
    shutdown();
}

bool PlatformViewer::init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);          // GLES 默认逆时针为正面
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    try {
        // Shader 在 PRISM_OHOS 下按文件名从嵌入表取源码并转为 GLSL ES 3.00
        shader_ = std::make_unique<Shader>("mesh.vert", "mesh.frag");

        // 程序化几何: 无外部资源依赖, 适合验证渲染链路
        mesh_ = procedural::torus(1.0f, 0.38f, 64, 24);

        renderer_ = std::make_unique<MeshRenderer>();
        renderer_->upload(*mesh_);
    } catch (const std::exception& e) {
        LOGE("PlatformViewer init failed: %{public}s", e.what());
        return false;
    }

#ifdef PRISM_HAVE_IMGUI
    initImGui();
#endif

    spin_ = 0.f;
    lastTime_ = 0.f;
    ready_ = true;
    LOGI("PlatformViewer init ok: tris=%{public}u GL=%{public}s",
         renderer_->triangleCount(),
         reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    return true;
}

#ifdef PRISM_HAVE_IMGUI
void PlatformViewer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;        // 不写 imgui.ini (沙箱内无需持久化)
    io.LogFilename = nullptr;
    ImGui::StyleColorsDark();

    // 中文字体: 尝试加载鸿蒙系统字体 (失败则回退默认拉丁字体)
    const char* kFonts[] = {
        "/system/fonts/HarmonyOS_Sans_SC_Regular.ttf",
        "/system/fonts/HarmonyOS_Sans_SC.ttf",
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/DroidSansFallbackFull.ttf",
        "/system/fonts/DroidSansFallback.ttf",
    };
    for (const char* f : kFonts) {
        if (io.Fonts->AddFontFromFileTTF(f, 18.0f, nullptr,
                                         io.Fonts->GetGlyphRangesChineseFull()) != nullptr) {
            LOGI("ImGui CJK font loaded: %{public}s", f);
            break;
        }
    }

    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        LOGE("ImGui_ImplOpenGL3_Init failed");
        ImGui::DestroyContext();
        return;
    }

    panel_     = std::make_unique<Panel>();
    uiState_   = std::make_unique<ViewState>();
    uiRequest_ = std::make_unique<UiRequest>();
    imguiReady_ = true;
    LOGI("ImGui ready (GLES3 backend)");
}

void PlatformViewer::feedImGui(const std::vector<PointerEvent>& pointers, float scroll) {
    ImGuiIO& io = ImGui::GetIO();
    for (const PointerEvent& e : pointers) {
        io.AddMousePosEvent(e.x, e.y);
        const bool down = (e.action == PointerAction::Down);
        const bool up   = (e.action == PointerAction::Up);
        // 左键=0, 右键=1 (鸿蒙按钮位: LEFT 0x01 / RIGHT 0x02)
        if (down || up) {
            if (e.button & 0x02) io.AddMouseButtonEvent(1, down);
            else                 io.AddMouseButtonEvent(0, down);
        }
    }
    if (scroll != 0.f) io.AddMouseWheelEvent(0.f, scroll);
}

void PlatformViewer::drawUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    const std::string name = mesh_ ? mesh_->name : std::string();
    const std::uint32_t vc = mesh_ ? static_cast<std::uint32_t>(mesh_->vertices_.size()) : 0u;
    const std::uint32_t tc = mesh_ ? mesh_->triangleCount : 0u;
    const glm::vec3 bmin = mesh_ ? mesh_->bboxMin : glm::vec3(0.f);
    const glm::vec3 bmax = mesh_ ? mesh_->bboxMax : glm::vec3(0.f);
    const bool materials = mesh_ && mesh_->hasPmxMaterials();

    panel_->draw(*uiState_, *uiRequest_, name, vc, tc, bmin, bmax, materials, false);

    ImGui::Render();
}
#endif // PRISM_HAVE_IMGUI

void PlatformViewer::applyInput(InputState& input) {
    // 一次性取走事件, 再分发给 UI / 相机
    const std::vector<PointerEvent> pointers = input.takePointer();
    input.takeKeys();
    const float scroll = input.takeScroll();

#ifdef PRISM_HAVE_IMGUI
    if (imguiReady_) {
        feedImGui(pointers, scroll);
        // 相机操作由 UI 面板/后续 OrbitCamera 接管; 此处保留无 UI 时的备用路径
        return;
    }
#endif
    // 无 UI: 左键拖拽 → 轨道旋转
    for (const PointerEvent& e : pointers) {
        if (e.action == PointerAction::Move && e.button == 0x01) {
            yaw_   -= e.x * 0.01f;
            pitch_ += e.y * 0.01f;
            if (pitch_ >  1.45f) pitch_ =  1.45f;
            if (pitch_ < -1.45f) pitch_ = -1.45f;
        }
    }
    if (scroll != 0.f) {
        dist_ *= (scroll > 0.f) ? 0.92f : 1.08f;
        if (dist_ < 1.2f)  dist_ = 1.2f;
        if (dist_ > 30.0f) dist_ = 30.0f;
    }
}

void PlatformViewer::renderScene(int w, int h) {
    const float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.f;

    // 轨道相机
    const float cp = std::cos(pitch_);
    const float sp = std::sin(pitch_);
    const glm::vec3 eye(dist_ * cp * std::sin(yaw_),
                        dist_ * sp,
                        dist_ * cp * std::cos(yaw_));
    const glm::mat4 view  = glm::lookAt(eye, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    const glm::mat4 proj  = glm::perspective(glm::radians(45.f), aspect, 0.1f, 200.f);
    const glm::mat4 model = glm::rotate(glm::mat4(1.f), spin_, glm::vec3(0.f, 1.f, 0.f));
    const glm::mat3 nmat  = glm::transpose(glm::inverse(glm::mat3(model)));

    shader_->bind();
    shader_->setMat4("uModel", model);
    shader_->setMat4("uView", view);
    shader_->setMat4("uProjection", proj);
    shader_->setMat3("uNormalMatrix", nmat);

    // mesh.frag 的光照 / 材质 / 雾参数
    shader_->setVec3 ("uLightDir",      glm::normalize(glm::vec3(-0.4f, -0.75f, -0.5f)));
    shader_->setVec3 ("uLightColor",    glm::vec3(1.00f, 0.98f, 0.95f));
    shader_->setVec3 ("uViewPos",       eye);
    shader_->setVec3 ("uBaseColor",     glm::vec3(0.62f, 0.68f, 0.78f));
    shader_->setFloat("uAmbient",       0.28f);
    shader_->setVec3 ("uFillColor",     glm::vec3(0.12f, 0.14f, 0.20f));
    shader_->setFloat("uFillStrength",  0.55f);
    shader_->setVec3 ("uFogColor",      glm::vec3(0.12f, 0.12f, 0.14f));
    shader_->setFloat("uFogNear",       6.0f);
    shader_->setFloat("uFogFar",        40.0f);
    shader_->setFloat("uSpecStrength",  0.35f);
    shader_->setFloat("uAlpha",         1.0f);
    shader_->setInt  ("uColorOverride", 0);
    shader_->setInt  ("uUseTexture",    0);
    shader_->setInt  ("uUseToon",       0);
    shader_->setInt  ("uUseSphere",     0);
    shader_->setInt  ("uSphereMode",    0);

    renderer_->drawSolid();
}

void PlatformViewer::frame(int w, int h, InputState& input) {
    if (!ready_) return;

    const float now = static_cast<float>(
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
    float dt = (lastTime_ > 0.f) ? (now - lastTime_) : 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    lastTime_ = now;

    applyInput(input);
    spin_ += dt * 0.6f;

    glViewport(0, 0, w, h);
    glClearColor(0.12f, 0.12f, 0.14f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderScene(w, h);

#ifdef PRISM_HAVE_IMGUI
    if (imguiReady_) {
        ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
        drawUI();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
#endif
}

void PlatformViewer::shutdown() {
#ifdef PRISM_HAVE_IMGUI
    if (imguiReady_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
    panel_.reset();
    uiState_.reset();
    uiRequest_.reset();
#endif
    renderer_.reset();
    mesh_.reset();
    shader_.reset();
    ready_ = false;
}

} // namespace prism
