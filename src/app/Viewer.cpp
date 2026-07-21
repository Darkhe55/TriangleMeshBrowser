// src/app/Viewer.cpp
#include "Viewer.h"
#include "../utils/FileUtils.h"
#include "../utils/StbWrite.h"
#include "../utils/ImeGuard.h"
#include "../model/Procedural.h"
#include "../ui/Picker.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <cstring>
#include <vector>
#include <cstdint>
#include <ctime>

// Windows 头文件 - 必须放 GLFW 之后
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

namespace prism {

// ---------- helpers ----------
// (normalMatrix removed; mesh has uniform scale so no normal matrix needed)

static std::string shaderPath(const std::string& name) {
    return joinPath(resourcePath("shaders"), name).string();
}

// ---------- ctor / dtor ----------
Viewer::Viewer(int w, int h, const std::string& title)
    : winW_(w), winH_(h), title_(title) {
    if (!initGL()) throw std::runtime_error("Failed to init GL");
    initImGui();
    loadShaders();
    cam_.viewportW = w; cam_.viewportH = h;
    // 默认关闭 Windows IME,防止 WASD/快捷键时弹候选框遮挡
    ImeGuard::disable(window_);
}

Viewer::~Viewer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Viewer::isRunning() const noexcept {
    return running_ && window_ && !glfwWindowShouldClose(window_);
}

bool Viewer::initGL() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);   // MSAA

    window_ = glfwCreateWindow(winW_, winH_, title_.c_str(), nullptr, nullptr);
    if (!window_) { glfwTerminate(); return false; }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // vsync

    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(window_); window_ = nullptr;
        glfwTerminate();
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 回调
    glfwSetWindowUserPointer(window_, this);
    glfwSetDropCallback       (window_, &Viewer::dropCallback);
    glfwSetKeyCallback        (window_, &Viewer::keyCallback);
    glfwSetWindowSizeCallback (window_, &Viewer::sizeCallback);
    glfwSetScrollCallback     (window_, &Viewer::scrollCallback);
    glfwSetMouseButtonCallback(window_, &Viewer::mouseButtonCallback);
    glfwSetCursorPosCallback  (window_, &Viewer::cursorPosCallback);

    return true;
}

void Viewer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 4.0f;
    st.FrameRounding  = 3.0f;
    st.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.92f);
    // Scale widget sizes to match the 24px font (default 13px -> ~1.85x).
    st.ScaleAllSizes(1.5f);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // 尝试加载中文字体(Roboto/CJK 兜底)
    // 找系统字体;失败就用默认
    auto tryLoad = [&](const char* p) -> bool {
        if (fileExists(p)) {
            ImFontConfig cfg;
            cfg.OversampleH = 2; cfg.OversampleV = 2;
            io.Fonts->AddFontFromFileTTF(p, 24.0f, &cfg,
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            return true;
        }
        return false;
    };
    bool loaded = false;
    const char* candidates[] = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
    };
    for (auto* c : candidates) {
        if (!loaded && tryLoad(c)) loaded = true;
    }
    if (!loaded) {
        ImFontConfig cfg;
        cfg.SizePixels = 24.0f;
        io.Fonts->AddFontDefault(&cfg);
    }
    // ImGui 1.92: ImGui_ImplOpenGL3_CreateFontsTexture is private/renamed;
    // the backend builds the font texture on first NewFrame() automatically.
}

void Viewer::loadShaders() {
    try {
        shaderMesh_   = Shader(shaderPath("mesh.vert"),   shaderPath("mesh.frag"));
        shaderLine_   = Shader(shaderPath("line.vert"),   shaderPath("line.frag"));
        shaderPicker_ = Shader(shaderPath("picker.vert"), shaderPath("picker.frag"));
        shaderFlat_   = Shader(shaderPath("flat.vert"),   shaderPath("flat.frag"));
        shaderGrid_   = Shader(shaderPath("grid.vert"),   shaderPath("grid.frag"));
        grid_ = Grid(5.0f, 1.0f);
        grid_.upload();
        shadersReady_ = true;
    } catch (const std::exception& e) {
        std::cerr << "Shader load error: " << e.what() << "\n";
        shadersReady_ = false;
    }
}

bool Viewer::loadModel(const std::filesystem::path& path) {
    try {
        auto m = ModelLoader::load(path);
        m->centerAndScale(1.0f);
        m->buildPickData();
        renderer_.upload(*m);
        mesh_ = std::move(m);
        AABB box; box.min = mesh_->bboxMin; box.max = mesh_->bboxMax;
        cam_.fitTo(box, 1.6f);
        state_.highlightedFace.reset();
        return true;
    } catch (const std::exception& e) {
        // toast 文本用 UTF-8(wstring → UTF-8 转换), 避免窄字符串里塞宽字符
        std::string msg = "加载失败: ";
        msg += e.what();
        uiReq_.toast = std::move(msg);
        return false;
    }
}

void Viewer::generateGeometry(int idx) {
    std::unique_ptr<Mesh> m;
    switch (idx) {
        case 0: m = procedural::cube();     break;
        case 1: m = procedural::sphere();   break;
        case 2: m = procedural::cylinder(); break;
        case 3: m = procedural::torus();    break;
        case 4: m = procedural::cone();     break;
        default: return;
    }
    m->centerAndScale(1.0f);
    m->buildPickData();
    renderer_.upload(*m);
    mesh_ = std::move(m);
    AABB box; box.min = mesh_->bboxMin; box.max = mesh_->bboxMax;
    cam_.fitTo(box, 1.6f);
    state_.highlightedFace.reset();
    uiReq_.toast = std::string("生成: ") + procedural::allNames()[idx];
}

void Viewer::clearModel() {
    mesh_.reset();
    renderer_.clear();
    state_.highlightedFace.reset();
}

// ---------- callbacks ----------
void Viewer::dropCallback(GLFWwindow* w, int pathCount, const char* paths[]) {
    auto* self = reinterpret_cast<Viewer*>(glfwGetWindowUserPointer(w));
    if (pathCount <= 0 || !paths[0]) return;
    // GLFW 在 Windows 上给的是 ANSI(CP_ACP)字节,直接喂给 std::ifstream 会触发
    // 一次 ANSI→UTF-16 转换,某些字符(包括很多 GBK 码点)会触发 ERROR_NO_UNICODE_TRANSLATION
    // (1113, "no mapping for the Unicode character exists")。
    // 先用 MultiByteToWideChar 转成宽字符,再构造 fs::path,这样 std::ifstream 内部直接拿
    // UTF-16 路径,不经过 ANSI→UTF-16 转换,中文/日文路径都能开。
    self->dropFile_ = std::filesystem::path(ansiToWide(paths[0]));
}

void Viewer::keyCallback(GLFWwindow* w, int key, int /*scancode*/, int action, int mods) {
    auto* self = reinterpret_cast<Viewer*>(glfwGetWindowUserPointer(w));
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_F && (mods & GLFW_MOD_SHIFT)) {
        if (self->mesh_) {
            AABB box; box.min = self->mesh_->bboxMin; box.max = self->mesh_->bboxMax;
            self->cam_.fitTo(box, 1.4f);
        }
    } else if (key == GLFW_KEY_F) {
        self->cam_.reset();
        if (self->mesh_) {
            AABB box; box.min = self->mesh_->bboxMin; box.max = self->mesh_->bboxMax;
            self->cam_.fitTo(box, 1.6f);
        }
    } else if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(w, GLFW_TRUE);
    }
}

void Viewer::sizeCallback(GLFWwindow* w, int w2, int h2) {
    auto* self = reinterpret_cast<Viewer*>(glfwGetWindowUserPointer(w));
    self->winW_ = w2; self->winH_ = h2;
    self->cam_.viewportW = w2; self->cam_.viewportH = h2;
    glViewport(0, 0, w2, h2);
    self->fbo_.resize(w2, h2);
}

void Viewer::scrollCallback(GLFWwindow* w, double /*xoff*/, double yoff) {
    auto* self = reinterpret_cast<Viewer*>(glfwGetWindowUserPointer(w));
    self->cam_.onScroll(static_cast<float>(yoff));
}

namespace {
bool s_dragging = false;
double s_lastX = 0, s_lastY = 0;
int s_dragButton = -1;
}

void Viewer::mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/) {
    auto* self = reinterpret_cast<Viewer*>(glfwGetWindowUserPointer(w));
    if (ImGui::GetIO().WantCaptureMouse) {
        s_dragging = false; return;
    }
    if (action == GLFW_PRESS) {
        s_dragging = true;
        s_dragButton = button;
        glfwGetCursorPos(w, &s_lastX, &s_lastY);
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            // 拾取 (左键单击 = 旋转;但单击和拖拽难区分)
            // 简单实现:按下时记录,松开时若 < 4px 移动 = 点击 → 拾取
        }
    } else if (action == GLFW_RELEASE) {
        if (s_dragging && s_dragButton == GLFW_MOUSE_BUTTON_LEFT) {
            double x, y;
            glfwGetCursorPos(w, &x, &y);
            if (std::fabs(x - s_lastX) < 4.0 && std::fabs(y - s_lastY) < 4.0) {
                self->doPicking(x, y);
            }
        }
        s_dragging = false; s_dragButton = -1;
    }
}

void Viewer::cursorPosCallback(GLFWwindow* w, double x, double y) {
    if (!s_dragging) return;
    auto* self = reinterpret_cast<Viewer*>(glfwGetWindowUserPointer(w));
    double dx = x - s_lastX;
    double dy = y - s_lastY;
    s_lastX = x; s_lastY = y;
    if (s_dragButton == GLFW_MOUSE_BUTTON_LEFT) {
        self->cam_.onMouseDragRotate(static_cast<float>(dx), static_cast<float>(dy),
                                     self->state_.invertX, self->state_.invertY);
    } else if (s_dragButton == GLFW_MOUSE_BUTTON_MIDDLE ||
               s_dragButton == GLFW_MOUSE_BUTTON_RIGHT) {
        self->cam_.onMouseDragPan(static_cast<float>(dx), static_cast<float>(dy));
    }
}

void Viewer::doPicking(double sx, double sy) {
    if (!mesh_) return;
    auto hit = Picker::pick(cam_, *mesh_, sx, sy);
    if (hit.has_value()) {
        state_.highlightedFace = hit->faceId;
    } else {
        state_.highlightedFace.reset();
    }
}

// ---------- main loop ----------
void Viewer::processInput() {
    glfwPollEvents();
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        running_ = false; return;
    }
    if (!dropFile_.empty()) {
        loadModel(dropFile_);
        dropFile_.clear();
    }
    if (glfwGetKey(window_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS &&
        glfwGetKey(window_, GLFW_KEY_O) == GLFW_PRESS) {
        uiReq_.openFileDialog = true;
    }
}

// WASD/QE 飞行(平移 target,水平方向 = 相机水平 forward/right,Shift 加速 3x)
void Viewer::processMovement(float dt) {
    if (!state_.enableWASD || dt <= 0.f) return;
    if (ImGui::GetIO().WantCaptureKeyboard) return;  // ImGui 焦点时不抢

    auto pressed = [this](int key) {
        return glfwGetKey(window_, key) == GLFW_PRESS;
    };

    glm::vec3 move(0.f);
    if (pressed(GLFW_KEY_W) || pressed(GLFW_KEY_UP))    move.z -= 1.f;
    if (pressed(GLFW_KEY_S) || pressed(GLFW_KEY_DOWN))  move.z += 1.f;
    if (pressed(GLFW_KEY_D) || pressed(GLFW_KEY_RIGHT)) move.x += 1.f;
    if (pressed(GLFW_KEY_A) || pressed(GLFW_KEY_LEFT))  move.x -= 1.f;
    if (pressed(GLFW_KEY_E) || pressed(GLFW_KEY_SPACE)) move.y += 1.f;
    if (pressed(GLFW_KEY_Q)) move.y -= 1.f;

    if (glm::length(move) < 1e-4f) return;
    move = glm::normalize(move);

    // 相机水平 forward / right(忽略 Z 分量,让 WASD 始终在 xOy 水平面上)
    // Z 轴朝上,垂直升降用 Z
    glm::vec3 fwdH  = glm::normalize(glm::vec3(cam_.forward().x, cam_.forward().y, 0.f));
    glm::vec3 rgtH  = cam_.right();
    glm::vec3 world = fwdH * (-move.z) + rgtH * move.x + glm::vec3(0.f, 0.f, move.y);

    float speed = state_.moveSpeed;
    if (pressed(GLFW_KEY_LEFT_SHIFT) || pressed(GLFW_KEY_RIGHT_SHIFT)) speed *= 3.0f;

    cam_.target += world * (speed * dt);
}

void Viewer::handleUiRequest() {
    if (uiReq_.openFileDialog) {
        uiReq_.openFileDialog = false;
        // GLFW 没有原生文件对话框;用 tinyfd? 设计文档没要求 → 用命令行参数 或
        // 把文件拖到窗口。简化:用 stb 或者直接用 Windows GetOpenFileName
        // 用 Windows API:
        OPENFILENAMEA ofn;
        char szFile[260] = {0};
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "3D Models\0*.obj;*.stl;*.ply;*.off\0All\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            // 同 drop: ANSI→wide→fs::path, 避免 std::ifstream 内部转码失败
            loadModel(std::filesystem::path(ansiToWide(szFile)));
        }
    }
    if (uiReq_.exportScreenshot) {
        uiReq_.exportScreenshot = false;
        exportScreenshot(winW_, winH_);
    }
    if (uiReq_.generateGeometry >= 0) {
        generateGeometry(uiReq_.generateGeometry);
        uiReq_.generateGeometry = -1;
    }
    if (uiReq_.fitToView && mesh_) {
        AABB box; box.min = mesh_->bboxMin; box.max = mesh_->bboxMax;
        cam_.fitTo(box, 1.4f);
        uiReq_.fitToView = false;
    }
    if (uiReq_.clearModel) {
        clearModel();
        uiReq_.clearModel = false;
    }
    if (uiReq_.resetCamera) {
        cam_.reset();
        uiReq_.resetCamera = false;
    }
}

void Viewer::renderSceneSingle(int vpW, int vpH) {
    glViewport(0, 0, vpW, vpH);
    glm::vec3 fog = state_.fogEnabled ? state_.fogColor : state_.baseColor;
    float fogNear = state_.fogEnabled ? state_.fogNear  : 1e9f;
    float fogFar  = state_.fogEnabled ? state_.fogFar   : 1e9f + 1.f;

    // Grid 网格(画在 mesh 之前,深度测试不通过会被遮)
    if (state_.showGrid) {
        shaderGrid_.bind();
        shaderGrid_.setMat4("uModel", glm::mat4(1.f));
        shaderGrid_.setMat4("uView",  cam_.view());
        shaderGrid_.setMat4("uProjection", cam_.projection());
        shaderGrid_.setFloat("uFogEnabled", state_.fogEnabled ? 1.f : 0.f);
        shaderGrid_.setFloat("uFogNear",    fogNear);
        shaderGrid_.setFloat("uFogFar",     fogFar);
        shaderGrid_.setVec3 ("uFogColor",   fog);
        glLineWidth(1.0f);
        glEnable(GL_BLEND);
        grid_.draw();
    }

    if (state_.mode == RenderMode::Solid || state_.mode == RenderMode::SolidWire) {
        shaderMesh_.bind();
        shaderMesh_.setMat4 ("uModel", glm::mat4(1.f));
        shaderMesh_.setMat4 ("uView",  cam_.view());
        shaderMesh_.setMat4 ("uProjection", cam_.projection());
        shaderMesh_.setMat3 ("uNormalMatrix", glm::mat3(1.f));
        shaderMesh_.setVec3 ("uLightDir",  state_.lightDir);
        shaderMesh_.setVec3 ("uLightColor", state_.lightColor);
        shaderMesh_.setVec3 ("uViewPos",   cam_.position());
        shaderMesh_.setVec3 ("uBaseColor", state_.baseColor);
        shaderMesh_.setFloat("uAmbient",   state_.ambient);
        shaderMesh_.setVec3 ("uFillColor", state_.fillColor);
        shaderMesh_.setFloat("uFillStrength", state_.fillStrength);
        shaderMesh_.setVec3 ("uFogColor",  fog);
        shaderMesh_.setFloat("uFogNear",   fogNear);
        shaderMesh_.setFloat("uFogFar",    fogFar);
        shaderMesh_.setFloat("uSpecStrength", state_.specStrength);
        if (mesh_) renderer_.drawSolid();
    }
    if (state_.mode == RenderMode::Wireframe || state_.mode == RenderMode::SolidWire) {
        if (state_.mode == RenderMode::Wireframe) {
            glClear(GL_DEPTH_BUFFER_BIT);  // 线框最前
        }
        shaderLine_.bind();
        shaderLine_.setMat4("uModel", glm::mat4(1.f));
        shaderLine_.setMat4("uView",  cam_.view());
        shaderLine_.setMat4("uProjection", cam_.projection());
        shaderLine_.setVec3("uLineColor", state_.mode == RenderMode::Wireframe
                              ? state_.baseColor : glm::vec3(0.f, 0.f, 0.f));
        if (mesh_ && renderer_.hasWireframe()) {
            glLineWidth(1.5f);
            renderer_.drawWireframe();
        }
    }
    // 拾取高亮(只画高亮面)
    if (mesh_ && state_.highlightedFace.has_value()) {
        shaderPicker_.bind();
        shaderPicker_.setMat4("uModel", glm::mat4(1.f));
        shaderPicker_.setMat4("uView",  cam_.view());
        shaderPicker_.setMat4("uProjection", cam_.projection());
        shaderPicker_.setMat3("uNormalMatrix", glm::mat3(1.f));
        shaderPicker_.setUInt("uHighlightFace", state_.highlightedFace.value());
        shaderPicker_.setVec3("uHighlightColor", glm::vec3(1.f, 0.7f, 0.1f));
        shaderPicker_.setVec3("uBaseColor", state_.baseColor);
        shaderPicker_.setFloat("uAmbient", state_.ambient);
        shaderPicker_.setVec3("uLightDir", state_.lightDir);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.f, -1.f);
        renderer_.drawPicker();
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void Viewer::renderSceneSplit(int vpW, int vpH, int halfW) {
    // 左视口 - 主光
    glViewport(0, 0, halfW, vpH);
    glScissor(0, 0, halfW, vpH);
    glEnable(GL_SCISSOR_TEST);
    renderSceneSingle(halfW, vpH);
    glDisable(GL_SCISSOR_TEST);

    // 右视口 - alt 光照/颜色
    glViewport(halfW, 0, vpW - halfW, vpH);
    glScissor(halfW, 0, vpW - halfW, vpH);
    glEnable(GL_SCISSOR_TEST);

    if (state_.split == SplitMode::DiffShading) {
        // 同一 mesh 不同参数
        glm::vec3 fog = state_.fogEnabled ? state_.fogColor : state_.altBaseColor;
        float fogNear = state_.fogEnabled ? state_.altFogNear : 1e9f;
        float fogFar  = state_.fogEnabled ? state_.altFogFar  : 1e9f + 1.f;
        shaderMesh_.bind();
        shaderMesh_.setMat4("uModel", glm::mat4(1.f));
        shaderMesh_.setMat4("uView", cam_.view());
        shaderMesh_.setMat4("uProjection", cam_.projection());
        shaderMesh_.setMat3("uNormalMatrix", glm::mat3(1.f));
        shaderMesh_.setVec3("uLightDir",  state_.altLightDir);
        shaderMesh_.setVec3("uLightColor", state_.lightColor);
        shaderMesh_.setVec3("uViewPos",   cam_.position());
        shaderMesh_.setVec3("uBaseColor", state_.altBaseColor);
        shaderMesh_.setFloat("uAmbient",   state_.altAmbient);
        shaderMesh_.setVec3("uFillColor", state_.fillColor);
        shaderMesh_.setFloat("uFillStrength", 0.3f);
        shaderMesh_.setVec3("uFogColor",  fog);
        shaderMesh_.setFloat("uFogNear",   fogNear);
        shaderMesh_.setFloat("uFogFar",    fogFar);
        shaderMesh_.setFloat("uSpecStrength", 0.3f);
        if (mesh_) renderer_.drawSolid();

        // 分隔线
        shaderLine_.bind();
        shaderLine_.setMat4("uModel", glm::mat4(1.f));
        shaderLine_.setMat4("uView",  cam_.view());
        shaderLine_.setMat4("uProjection", cam_.projection());
        shaderLine_.setVec3("uLineColor", glm::vec3(0.4f, 0.8f, 1.0f));
        glLineWidth(2.f);
    } else if (state_.split == SplitMode::DiffModels) {
        // 右侧画 meshB(没加载则画同 mesh 但 alt 颜色)
        if (!meshB_) {
            meshB_ = procedural::torus();
            meshB_->centerAndScale(1.0f);
            meshB_->buildPickData();
            rendererB_.upload(*meshB_);
        }
        shaderMesh_.bind();
        shaderMesh_.setMat4("uModel", glm::mat4(1.f));
        shaderMesh_.setMat4("uView", cam_.view());
        shaderMesh_.setMat4("uProjection", cam_.projection());
        shaderMesh_.setMat3("uNormalMatrix", glm::mat3(1.f));
        shaderMesh_.setVec3("uLightDir",  state_.lightDir);
        shaderMesh_.setVec3("uLightColor", state_.lightColor);
        shaderMesh_.setVec3("uViewPos",   cam_.position());
        shaderMesh_.setVec3("uBaseColor", state_.altBaseColor);
        shaderMesh_.setFloat("uAmbient",   state_.ambient);
        shaderMesh_.setVec3("uFillColor", state_.fillColor);
        shaderMesh_.setFloat("uFillStrength", state_.fillStrength);
        shaderMesh_.setVec3("uFogColor",  state_.fogColor);
        shaderMesh_.setFloat("uFogNear",   state_.fogNear);
        shaderMesh_.setFloat("uFogFar",    state_.fogFar);
        shaderMesh_.setFloat("uSpecStrength", state_.specStrength);
        rendererB_.drawSolid();
    }
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, vpW, vpH);
}

void Viewer::exportScreenshot(int vpW, int vpH) {
    // 在 FBO 里渲染当前场景,再读像素
    fbo_.resize(vpW, vpH);
    fbo_.bind();
    glViewport(0, 0, vpW, vpH);
    glClearColor(state_.fogColor.x, state_.fogColor.y, state_.fogColor.z, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (state_.split == SplitMode::Off) {
        renderSceneSingle(vpW, vpH);
    } else {
        renderSceneSplit(vpW, vpH, vpW / 2);
    }
    std::vector<std::uint8_t> pixels;
    fbo_.readPixels(pixels);
    fbo_.unbind();

    // 写 PNG:时间戳命名
    auto t = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(t);
    std::tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    char name[64];
    std::strftime(name, sizeof(name), "prism_shot_%Y%m%d_%H%M%S.png", &tmv);
    try {
        writePNG(name, vpW, vpH, pixels.data());
        uiReq_.toast = std::string("已保存: ") + name;
    } catch (const std::exception& e) {
        uiReq_.toast = std::string("截图失败: ") + e.what();
    }
}

void Viewer::render() {
    glClearColor(state_.fogColor.x, state_.fogColor.y, state_.fogColor.z, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (state_.split == SplitMode::Off) {
        renderSceneSingle(winW_, winH_);
    } else {
        renderSceneSplit(winW_, winH_, winW_ / 2);
    }
}

void Viewer::run() {
    auto last = std::chrono::high_resolution_clock::now();
    while (isRunning()) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;   // 防止窗口拖动 / 暂停后大跳

        processInput();
        processMovement(dt);
        handleUiRequest();

        // UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        std::string name = mesh_ ? mesh_->name : "";
        std::uint32_t vc = mesh_ ? static_cast<std::uint32_t>(mesh_->vertices_.size()) : 0;
        std::uint32_t tc = mesh_ ? mesh_->triangleCount : 0;
        glm::vec3 bbMin = mesh_ ? mesh_->bboxMin : glm::vec3(0);
        glm::vec3 bbMax = mesh_ ? mesh_->bboxMax : glm::vec3(0);
        panel_.draw(state_, uiReq_, name, vc, tc, bbMin, bbMax);

        // 双视口标签
        if (state_.split != SplitMode::Off) {
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            dl->AddText(ImVec2(10, 50),  IM_COL32(200,230,255,255), "主视口");
            float halfW = static_cast<float>(winW_) * 0.5f;
            dl->AddText(ImVec2(halfW + 10.0f, 50.0f),
                        IM_COL32(255,210,170,255),
                        state_.split == SplitMode::DiffShading ? "对比: 不同光照" : "对比: 不同模型");
        }

        // 左上角 3D 坐标轴 gizmo
        drawAxisGizmo(0, 0, winW_, winH_);

        // IME 智能切换:仅当 ImGui 正在处理文本输入 (InputText/InputTextMultiline 等)
        // 时启用 IME;其余时间 (WASD / 快捷键 / 滑条) 自动关闭,防止候选框遮挡。
        ImeGuard::syncWithImGui(window_);

        ImGui::Render();

        render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);

        (void)last;  // dt 已计算
    }
}

void Viewer::checkErr(const char* tag) { checkGLError(tag); }

// 左上角 3D 坐标轴 gizmo (跟相机联动)
void Viewer::drawAxisGizmo(int vpX, int vpY, int /*vpW*/, int /*vpH*/) {
    if (!state_.showAxisGizmo) return;
    const float menuH = ImGui::GetFrameHeight();
    const float pad   = 16.0f;
    const float r     = 36.0f;
    const ImVec2 center(vpX + pad + r, vpY + menuH + pad + r);

    glm::mat4 vp = cam_.projection() * cam_.view();
    auto project = [&](glm::vec3 dir) -> ImVec2 {
        glm::vec4 c = vp * glm::vec4(dir, 0.0f);
        return ImVec2(c.x * r, -c.y * r);  // NDC y 向上,屏幕 y 向下
    };
    ImVec2 xEnd = center + project(glm::vec3(1, 0, 0));
    ImVec2 yEnd = center + project(glm::vec3(0, 1, 0));
    ImVec2 zEnd = center + project(glm::vec3(0, 0, 1));

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    // 圆心 + 标签背景
    dl->AddCircleFilled(center, 6.0f, IM_COL32(20, 20, 28, 200));
    // 3 条轴线(末端实心圆点)
    dl->AddLine(center, xEnd, IM_COL32(255,  90,  90, 255), 2.5f);
    dl->AddLine(center, yEnd, IM_COL32( 90, 235,  90, 255), 2.5f);
    dl->AddLine(center, zEnd, IM_COL32( 90, 140, 255, 255), 2.5f);
    dl->AddCircleFilled(xEnd, 4.0f, IM_COL32(255,  90,  90, 255));
    dl->AddCircleFilled(yEnd, 4.0f, IM_COL32( 90, 235,  90, 255));
    dl->AddCircleFilled(zEnd, 4.0f, IM_COL32( 90, 140, 255, 255));
    // 标签
    dl->AddText(ImVec2(xEnd.x + 6, xEnd.y - 10), IM_COL32(255, 110, 110, 255), "X");
    dl->AddText(ImVec2(yEnd.x + 6, yEnd.y - 10), IM_COL32(110, 255, 110, 255), "Y");
    dl->AddText(ImVec2(zEnd.x + 6, zEnd.y - 10), IM_COL32(110, 160, 255, 255), "Z");
}

} // namespace prism
