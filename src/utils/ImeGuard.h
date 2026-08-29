// src/utils/ImeGuard.h
// Windows IME 开关守护:非文本输入场景关闭输入法,文本输入时恢复
#pragma once

struct GLFWwindow;

namespace prism {

class ImeGuard {
public:
    // 关闭 IME,保留之前的 HIMC
    static void disable(GLFWwindow* w);

    // 恢复 disable 之前保存的 IME context
    static void enable(GLFWwindow* w);

    // 根据 wantIme 切换
    static void setEnabled(GLFWwindow* w, bool wantIme);

    // 根据 ImGui 是否在处理 InputText 自动切换(在 NewFrame 之后调用)
    static void syncWithImGui(GLFWwindow* w);

    // 当前是否启用了 IME
    static bool isEnabled() noexcept;
};

} // namespace prism
