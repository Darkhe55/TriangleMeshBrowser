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

// imgui 1.92: ImGuiContext 在 imgui.h 是 opaque,InputTextState 在 imgui_internal.h
#include <imgui.h>
#include <imgui_internal.h>

namespace prism {

namespace {
    // 保存 disable 之前的 HIMC
    void* s_prevHimc = nullptr;
    // 当前关联的窗口句柄
    HWND  s_hwnd     = nullptr;
    // 上一次用户希望的 IME 状态(避免每帧重复调 ImmAssociateContext)
    bool  s_enabled  = false;
}

void ImeGuard::disable(GLFWwindow* w) {
#ifdef _WIN32
    if (!w) return;
    HWND hwnd = glfwGetWin32Window(w);
    if (!hwnd) return;
    if (hwnd != s_hwnd) {
        // 第一次 disable 这窗口:记下旧的 HIMC 以便 enable 恢复
        s_prevHimc = ImmAssociateContext(hwnd, nullptr);
        s_hwnd     = hwnd;
    } else {
        // 重复 disable:只需保证关
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
        // 没 disable 过这窗口 / 没有保留的旧 context → 让系统重建
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
    if (wantIme == s_enabled) return;  // 已是目标状态,跳过
    if (wantIme) enable(w);
    else         disable(w);
}

void ImeGuard::syncWithImGui(GLFWwindow* w) {
    if (!w) return;
    // ImGui 1.92: InputTextState.ID != 0 表示有 InputText/InputTextMultiline 处于 active 编辑态
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    bool wantIme = ctx && ctx->InputTextState.ID != 0;
    setEnabled(w, wantIme);
}

bool ImeGuard::isEnabled() noexcept { return s_enabled; }

} // namespace prism
