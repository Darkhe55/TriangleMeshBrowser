// src/utils/ImeGuard.h
// Windows IME guard - 防止输入法在非文本输入场景干扰 (例如 WASD 时弹候选框)
//
// 用法:
//   窗口创建后立即 ImeGuard::disable(window);  // 全局关闭 IME
//   文本输入激活前 ImeGuard::enable(window);   // 恢复 IME
//   文本输入结束  ImeGuard::disable(window);  // 再次关闭
//   或者每帧调 ImeGuard::syncWithImGui(window) - 自动开/关
#pragma once

struct GLFWwindow;

namespace prism {

class ImeGuard {
public:
    // 关闭 IME,保留之前的 HIMC
    static void disable(GLFWwindow* w);

    // 恢复 disable 之前保存的 IME context
    static void enable(GLFWwindow* w);

    // 简化:根据 wantIme 切换
    static void setEnabled(GLFWwindow* w, bool wantIme);

    // 自动:根据 ImGui 是否在处理 InputText 切换
    // (在 NewFrame 之后调;现在没 InputText 永远 disable)
    static void syncWithImGui(GLFWwindow* w);

    // 当前是否启用了 IME
    static bool isEnabled() noexcept;
};

} // namespace prism
