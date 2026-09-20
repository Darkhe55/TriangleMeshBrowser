// browser/harmony/entry/src/main/cpp/platform/PlatformViewer.cpp
// 鸿蒙渲染宿主: 用内核 (Procedural / MeshRenderer / Shader / Panel) 渲染 + UI。
//
// 与 ArkTS 壳的分工:
//   ArkTS 负责系统级交互 (文件选择器 / 媒体库), Native 负责渲染与模型解析。
//   选文件走 "ArkTS 选择 -> 复制进应用沙箱 -> 把沙箱路径交给 Native" 的路径,
//   这样 Native 侧可沿用按扩展名分发的 ModelLoader::load(path), 无需改内核。
#include "PlatformViewer.h"
#include "InputState.h"

// PRISM_SRC 已在 target_include_directories 中, 故用相对源码根的路径
#include "model/Mesh.h"
#include "model/ModelLoader.h"
#include "model/MeshWriter.h"
#include "model/Procedural.h"
#include "renderer/MeshRenderer.h"
#include "renderer/Shader.h"
#include "utils/I18n.h"
#include "utils/StbWrite.h"      // prism::writePNG (stb_image_write 的唯一实现来源)

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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#undef LOG_TAG
#define LOG_TAG "PrismViewer"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO,  0x0000, LOG_TAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0x0000, LOG_TAG, __VA_ARGS__)

namespace prism {

namespace {

// 生成 prism_shot_YYYYMMDD_HHMMSS.png
std::string shotFileName() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf), "prism_shot_%04d%02d%02d_%02d%02d%02d.png",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

// 取路径最后一段 (用于 toast 显示文件名)
std::string baseNameOf(const std::string& path) {
    const std::size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

} // namespace

PlatformViewer::PlatformViewer() = default;

PlatformViewer::~PlatformViewer() {
    shutdown();
}

// ---------------- 与 ArkTS 壳交互 ----------------

void PlatformViewer::submitModelPath(const std::string& path) {
    if (path.empty()) return;
    std::lock_guard<std::mutex> lk(ioMutex_);
    pendingModelPath_ = path;
    hasPendingModel_ = true;
    LOGI("submitModelPath: %{public}s", path.c_str());
}

bool PlatformViewer::takeOpenFileRequest() {
    return openFileRequested_.exchange(false);
}

bool PlatformViewer::takeScreenshotResult(std::string& outPath) {
    if (!screenshotDone_.load()) return false;
    std::lock_guard<std::mutex> lk(ioMutex_);
    outPath = screenshotPath_;
    screenshotDone_.store(false);
    return !outPath.empty();
}

void PlatformViewer::setSandboxDir(const std::string& dir) {
    std::lock_guard<std::mutex> lk(ioMutex_);
    sandboxDir_ = dir;
    LOGI("sandbox dir: %{public}s", dir.c_str());
}

std::string PlatformViewer::takeLastError() {
    std::lock_guard<std::mutex> lk(ioMutex_);
    std::string e = lastError_;
    lastError_.clear();
    return e;
}

// ---------------- 渲染线程内的动作 ----------------

void PlatformViewer::setToast(const std::string& msg) {
#ifdef PRISM_HAVE_IMGUI
    if (uiRequest_) {
        uiRequest_->toast = msg;
    }
#else
    (void)msg;
#endif
}

void PlatformViewer::requestScreenshot() {
    screenshotRequested_ = true;   // 在 frame() 渲染完成后读取像素
}

void PlatformViewer::exportScreenshot(int w, int h) {
    if (w <= 0 || h <= 0) return;

    std::string dir;
    {
        std::lock_guard<std::mutex> lk(ioMutex_);
        dir = sandboxDir_;
    }
    if (dir.empty()) {
        LOGE("exportScreenshot: sandbox dir not set");
        std::lock_guard<std::mutex> lk(ioMutex_);
        lastError_ = i18n::tr("toast.shotFailed");
        return;
    }

    const std::size_t stride = static_cast<std::size_t>(w) * 4u;
    std::vector<std::uint8_t> pixels(stride * static_cast<std::size_t>(h));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    const std::string path = dir + "/" + shotFileName();
    // writePNG 内部完成 OpenGL(左下原点) -> PNG(左上原点) 的垂直翻转
    try {
        writePNG(path, w, h, pixels.data());
    } catch (const std::exception& e) {
        LOGE("exportScreenshot: %{public}s", e.what());
        std::lock_guard<std::mutex> lk(ioMutex_);
        lastError_ = std::string(i18n::tr("toast.shotFailed")) + e.what();
        return;
    }

    LOGI("screenshot saved: %{public}s", path.c_str());
    {
        std::lock_guard<std::mutex> lk(ioMutex_);
        screenshotPath_ = path;
    }
    screenshotDone_.store(true);
}

void PlatformViewer::exportModelToFile() {
    if (!mesh_) {
        setToast(i18n::tr("toast.exportNoModel"));
        return;
    }
    std::string dir;
    {
        std::lock_guard<std::mutex> lk(ioMutex_);
        dir = sandboxDir_;
    }
    if (dir.empty()) {
        setToast(std::string(i18n::tr("toast.exportFailed")) + "sandbox dir not set");
        return;
    }
    // 默认导出 OBJ; ArkTS 侧如需其它格式可再扩展
    const std::string path = dir + "/" + baseNameOf(mesh_->name) + ".obj";
    try {
        MeshWriter::save(*mesh_, std::filesystem::path(path));
        setToast(std::string(i18n::tr("toast.exported")) + baseNameOf(path));
        LOGI("model exported: %{public}s", path.c_str());
    } catch (const std::exception& e) {
        setToast(std::string(i18n::tr("toast.exportFailed")) + e.what());
        LOGE("exportModelToFile failed: %{public}s", e.what());
    }
}

void PlatformViewer::generateGeometry(int kind) {
    try {
        std::unique_ptr<Mesh> m;
        switch (kind) {
            case 0: m = procedural::cube(1.2f);                        break;
            case 1: m = procedural::sphere(1.0f, 48, 24);              break;
            case 2: m = procedural::cylinder(0.8f, 2.0f, 48);          break;
            case 3: m = procedural::torus(1.0f, 0.38f, 64, 24);        break;
            case 4: m = procedural::cone(1.0f, 2.0f, 48);              break;
            default: return;
        }
        if (!m) return;
        mesh_ = std::move(m);
        renderer_->upload(*mesh_);
        fitCamera();
        setToast(std::string(i18n::tr("toast.generate")) + mesh_->name);
        LOGI("generateGeometry kind=%{public}d tris=%{public}u",
             kind, renderer_->triangleCount());
    } catch (const std::exception& e) {
        setToast(std::string(i18n::tr("toast.loadFailed")) + e.what());
        LOGE("generateGeometry failed: %{public}s", e.what());
    }
}

void PlatformViewer::fitCamera() {
    if (!mesh_) return;
    const glm::vec3 c = (mesh_->bboxMin + mesh_->bboxMax) * 0.5f;
    const glm::vec3 d = mesh_->bboxMax - mesh_->bboxMin;
    float radius = 0.5f * glm::length(d);
    if (radius < 1e-4f) radius = 1.0f;
    dist_ = radius * 3.2f;
    if (dist_ < 1.2f) dist_ = 1.2f;
    if (dist_ > 500.f) dist_ = 500.f;
    // 半径仅用于推导距离; 相机朝向原点, 因此把模型中心平移量记录到 yaw 保持简单
    (void)c;
}

void PlatformViewer::clearModel() {
    mesh_.reset();
    renderer_.reset();
    renderer_ = std::make_unique<MeshRenderer>();
    setToast(i18n::tr("toast.cleared"));
    LOGI("model cleared");
}

// UI 面板的请求在这里落地
void PlatformViewer::handleUiRequest() {
#ifdef PRISM_HAVE_IMGUI
    if (!uiRequest_) return;

    if (uiRequest_->openFileDialog) {
        uiRequest_->openFileDialog = false;
        // Native 无法直接弹出系统选择器: 置位后由 ArkTS 侧轮询并打开
        openFileRequested_.store(true);
        LOGI("openFileDialog requested -> hand to ArkTS");
    }
    if (uiRequest_->exportScreenshot) {
        uiRequest_->exportScreenshot = false;
        requestScreenshot();
    }
    if (uiRequest_->exportModel) {
        uiRequest_->exportModel = false;
        exportModelToFile();
    }
    if (uiRequest_->generateGeometry >= 0) {
        const int kind = uiRequest_->generateGeometry;
        uiRequest_->generateGeometry = -1;
        generateGeometry(kind);
    }
    if (uiRequest_->fitToView) {
        uiRequest_->fitToView = false;
        fitCamera();
    }
    if (uiRequest_->clearModel) {
        uiRequest_->clearModel = false;
        clearModel();
    }
    if (uiRequest_->resetCamera) {
        uiRequest_->resetCamera = false;
        yaw_ = 0.7f; pitch_ = 0.45f;
        fitCamera();
    }
#endif
}

// 处理 ArkTS 交回来的待加载模型 (渲染线程, 有 GL 上下文)
void PlatformViewer::consumePendingModel() {
    std::string path;
    {
        std::lock_guard<std::mutex> lk(ioMutex_);
        if (!hasPendingModel_) return;
        path = pendingModelPath_;
        hasPendingModel_ = false;
    }

    try {
        auto m = ModelLoader::load(std::filesystem::path(path));
        if (!m) {
            std::lock_guard<std::mutex> lk(ioMutex_);
            lastError_ = std::string(i18n::tr("toast.loadFailed")) + baseNameOf(path);
            return;
        }
        mesh_ = std::move(m);
        renderer_->upload(*mesh_);
        fitCamera();
        setToast(std::string(i18n::tr("toast.loaded")) + baseNameOf(path));
        LOGI("model loaded: %{public}s tris=%{public}u",
             baseNameOf(path).c_str(), renderer_->triangleCount());
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lk(ioMutex_);
        lastError_ = std::string(i18n::tr("toast.loadFailed")) + e.what();
        LOGE("load model failed: %{public}s", e.what());
    }
}

// ---------------- 初始化 ----------------

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
        fitCamera();
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

// ---------------- 输入 ----------------

void PlatformViewer::applyInput(InputState& input) {
    // 一次性取走事件, 再分发给 UI / 相机
    const std::vector<PointerEvent> pointers = input.takePointer();
    const std::vector<KeyEventData> keys    = input.takeKeys();
    const float scroll = input.takeScroll();

#ifdef PRISM_HAVE_IMGUI
    if (imguiReady_) {
        feedImGui(pointers, scroll);

        // 快捷键: O 打开文件 (不方便点面板时的兜底)
        for (const KeyEventData& k : keys) {
            // OHOS keycode: KEYCODE_O = 2017
            if (k.pressed && k.code == 2017) {
                openFileRequested_.store(true);
                LOGI("key O pressed -> open file request");
            }
        }

        // ImGui 捕获鼠标时(指针停在面板上)不做手势, 否则交给轨道相机
        if (!ImGui::GetIO().WantCaptureMouse) {
            applyGestures(pointers, scroll);
        }
        return;
    }
#endif
    // 无 UI: 手势直接驱动相机
    applyGestures(pointers, scroll);
}

// 单指拖拽 = 轨道旋转; 双指张开/捏合 = 缩放; 双指转动 = 绕 Y 轴旋转
void PlatformViewer::applyGestures(const std::vector<PointerEvent>& pointers, float scroll) {
    // 维护活动触点表
    for (const PointerEvent& e : pointers) {
        if (e.action == PointerAction::Up || e.action == PointerAction::Cancel) {
            touches_.erase(e.id);
        } else {
            touches_[e.id] = std::make_pair(e.x, e.y);
        }
    }

    if (touches_.size() >= 2) {
        // ---- 双指: 捏合缩放 + 旋转 ----
        auto it = touches_.begin();
        const float ax = it->second.first;
        const float ay = it->second.second;
        ++it;
        const float bx = it->second.first;
        const float by = it->second.second;

        const float dx = bx - ax;
        const float dy = by - ay;
        const float d   = std::sqrt(dx * dx + dy * dy);
        const float ang = std::atan2(dy, dx);

        if (pinchActive_ && d > 1.f && lastPinchDist_ > 1.f) {
            // 两指距离变大 -> 拉近观察
            dist_ *= (lastPinchDist_ / d);
            if (dist_ < 0.2f)  dist_ = 0.2f;
            if (dist_ > 500.f) dist_ = 500.f;

            float dAng = ang - lastPinchAngle_;
            while (dAng >  3.14159265f) dAng -= 6.28318531f;
            while (dAng < -3.14159265f) dAng += 6.28318531f;
            yaw_ -= dAng * 0.5f;
        }
        lastPinchDist_  = d;
        lastPinchAngle_ = ang;
        pinchActive_    = true;
        dragValid_      = false;
        return;
    }

    pinchActive_    = false;
    lastPinchDist_  = 0.f;
    lastPinchAngle_ = 0.f;

    if (touches_.size() == 1) {
        // ---- 单指: 轨道旋转 ----
        const auto& p = touches_.begin()->second;
        if (dragValid_) {
            yaw_   -= (p.first  - lastDragX_) * 0.006f;
            pitch_ += (p.second - lastDragY_) * 0.006f;
            if (pitch_ >  1.45f) pitch_ =  1.45f;
            if (pitch_ < -1.45f) pitch_ = -1.45f;
        }
        lastDragX_ = p.first;
        lastDragY_ = p.second;
        dragValid_ = true;
    } else {
        dragValid_ = false;
    }

    if (scroll != 0.f) {
        dist_ *= (scroll > 0.f) ? 0.92f : 1.08f;
        if (dist_ < 0.2f)  dist_ = 0.2f;
        if (dist_ > 500.f) dist_ = 500.f;
    }
}

// ---------------- 渲染 ----------------

void PlatformViewer::renderScene(int w, int h) {
    const float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.f;

    // 轨道相机
    const float cp = std::cos(pitch_);
    const float sp = std::sin(pitch_);
    const glm::vec3 eye(dist_ * cp * std::sin(yaw_),
                        dist_ * sp,
                        dist_ * cp * std::cos(yaw_));
    const glm::mat4 view  = glm::lookAt(eye, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    const glm::mat4 proj  = glm::perspective(glm::radians(45.f), aspect, 0.1f, 2000.f);
    const glm::mat4 model = glm::rotate(glm::mat4(1.f), spin_, glm::vec3(0.f, 1.f, 0.f));
    const glm::mat3 nmat  = glm::transpose(glm::inverse(glm::mat3(model)));

    if (!renderer_ || !mesh_) return;

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
    shader_->setFloat("uFogNear",       dist_ * 2.0f);
    shader_->setFloat("uFogFar",        dist_ * 12.0f);
    shader_->setFloat("uSpecStrength",  0.35f);
    shader_->setFloat("uAlpha",         1.0f);
    shader_->setInt  ("uColorOverride", 0);
    shader_->setInt  ("uUseTexture",    0);
    shader_->setInt  ("uUseToon",       0);
    shader_->setInt  ("uUseSphere",     0);
    shader_->setInt  ("uSphereMode",    0);

    if (mesh_->pointCloud)          renderer_->drawPoints();
    else                            renderer_->drawSolid();
}

void PlatformViewer::frame(int w, int h, InputState& input) {
    if (!ready_) return;

    const float now = static_cast<float>(
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
    float dt = (lastTime_ > 0.f) ? (now - lastTime_) : 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    lastTime_ = now;

    // ArkTS 交回的文件 / UI 面板请求 (渲染线程, GL 上下文已 current)
    consumePendingModel();
    handleUiRequest();

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

    // 截图必须在同一帧绘制完成后读取默认帧缓冲
    if (screenshotRequested_) {
        screenshotRequested_ = false;
        exportScreenshot(w, h);
    }
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
