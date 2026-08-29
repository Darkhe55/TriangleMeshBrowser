// src/utils/ImeGuard.cpp
#include "ImeGuard.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#ifdef _WIN32
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif

// InputTextState 定义于 imgui_internal.h
#include <imgui.h>
#include <imgui_internal.h>

namespace prism {

namespace {
    // 保存 disable 之前的 HIMC
    void* s_prevHimc = nullptr;
    // 当前关联的窗口句柄
    HWND  s_hwnd     = nullptr;
    // 当前 IME 启用状态
    bool  s_enabled  = false;
}

void ImeGuard::disable(GLFWwindow* w) {
#ifdef _WIN32
    if (!w) return;
    HWND hwnd = glfwGetWin32Window(w);
    if (!hwnd) return;
    if (hwnd != s_hwnd) {
        // 记录旧 HIMC 以便恢复
        s_prevHimc = ImmAssociateContext(hwnd, nullptr);
        s_hwnd     = hwnd;
    } else {
        // 确保关闭
        ImmAssociateContext(hwnd, nullptr);
    }
    s_enabled = false;
#endif
}

void ImeGuard::enable(GLFWwindow* w) {
#ifdef _WIN32
    if (!w) return;
    HWND hwnd = glfwGetWin32Window(w);
    if (!hwnd) return;
    if (hwnd != s_hwnd || s_prevHimc == nullptr) {
        // 无可恢复的 context: 新建
        HIMC fresh = ImmCreateContext();
        if (fresh) {
            ImmAssociateContext(hwnd, fresh);
            s_prevHimc = fresh;
            s_hwnd     = hwnd;
        }
    } else {
        ImmAssociateContext(hwnd, static_cast<HIMC>(s_prevHimc));
    }
    s_enabled = true;
#endif
}

void ImeGuard::setEnabled(GLFWwindow* w, bool wantIme) {
    if (wantIme == s_enabled) return;
    if (wantIme) enable(w);
    else         disable(w);
}

void ImeGuard::syncWithImGui(GLFWwindow* w) {
    if (!w) return;
    // InputTextState.ID != 0 表示有文本输入处于编辑态
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    bool wantIme = ctx && ctx->InputTextState.ID != 0;
    setEnabled(w, wantIme);
}

bool ImeGuard::isEnabled() noexcept { return s_enabled; }

} // namespace prism
