// src/app/Viewer.h
// 应用程序主类
#pragma once

#include "OrbitCamera.h"
#include "../model/Mesh.h"
#include "../model/ModelLoader.h"
#include "../renderer/Shader.h"
#include "../renderer/MeshRenderer.h"
#include "../renderer/FrameBuffer.h"
#include "../renderer/Grid.h"
#include "../ui/Panel.h"
#include <filesystem>
#include <memory>
#include <string>
#include <array>
#include <cstdint>

struct GLFWwindow;

namespace prism {

class Viewer {
public:
    Viewer(int width, int height, const std::string& title);
    ~Viewer();

    Viewer(const Viewer&)            = delete;
    Viewer& operator=(const Viewer&) = delete;

    bool loadModel(const std::filesystem::path& path);
    void generateGeometry(int idx);
    void clearModel();

    bool isRunning() const noexcept;
    void run();
    void requestClose() noexcept { running_ = false; }

private:
    // ---------- 窗口 ----------
    GLFWwindow* window_ = nullptr;
    int winW_ = 1280, winH_ = 720;
    std::string title_;
    bool running_ = true;

    // ---------- 状态 ----------
    std::unique_ptr<Mesh>         mesh_;
    MeshRenderer                  renderer_;
    FrameBuffer                   fbo_;            // 离屏截图用
    ViewState                     state_;
    Panel                         panel_;
    OrbitCamera                   cam_;
    UiRequest                     uiReq_;

    // 拖拽文件(fs::path 内部就是 UTF-16 宽字符,直接喂给 std::ifstream 不会触发 ANSI→UTF-16 转换)
    std::filesystem::path         dropFile_;

    // Shaders
    Shader                        shaderMesh_;
    Shader                        shaderLine_;
    Shader                        shaderPicker_;
    Shader                        shaderFlat_;
    Shader                        shaderGrid_;
    bool                          shadersReady_ = false;

    // Grid
    Grid                          grid_;

    // 第二个视口(双视口模式)
    std::unique_ptr<Mesh>         meshB_;
    MeshRenderer                  rendererB_;

    // ---------- 内部方法 ----------
    bool  initGL();
    void  initImGui();
    void  loadShaders();
    void  processInput();
    void  processMovement(float dt);
    void  handleUiRequest();
    void  render();

    // 子视口渲染
    void renderSceneSingle(int vpW, int vpH);
    void renderSceneSplit (int vpW, int vpH, int halfW);

    // 拾取
    void doPicking(double sx, double sy);

    // 截图
    void exportScreenshot(int vpW, int vpH);

    // 2D 覆盖层(坐标轴 gizmo)
    void drawAxisGizmo(int vpX, int vpY, int vpW, int vpH);

    // GL 错误检查
    void checkErr(const char* tag);

    // 静态 GLFW 回调
    static void dropCallback(GLFWwindow* w, int pathCount, const char* paths[]);
    static void keyCallback  (GLFWwindow* w, int key, int scancode, int action, int mods);
    static void sizeCallback (GLFWwindow* w, int w2, int h2);
    static void scrollCallback(GLFWwindow* w, double xoff, double yoff);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double x, double y);
};

} // namespace prism
