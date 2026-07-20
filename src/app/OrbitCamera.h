// src/app/OrbitCamera.h
// 轨道相机:左键旋转 / 滚轮缩放 / 中键+Shift 平移
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace prism {

struct AABB {
    glm::vec3 min{0.f}, max{0.f};
    glm::vec3 center() const noexcept { return (min + max) * 0.5f; }
    glm::vec3 extent() const noexcept { return max - min; }
    float     radius() const noexcept {
        return 0.5f * glm::length(extent());
    }
};

class OrbitCamera {
public:
    OrbitCamera() = default;

    // 状态
    float yaw   = 45.f;   // 度
    float pitch = 30.f;   // 度
    float distance = 3.0f;
    mutable glm::vec3 target{0.f};   // mutable: _applyTarget 在 const 上下文里延迟应用
    float fovDeg  = 45.f;
    float nearZ   = 0.05f;
    float farZ    = 1000.f;

    // 视口
    int viewportW = 1280;
    int viewportH = 720;

    // 输入
    // invertX/invertY: 翻转旋转方向(默认 X 翻转,Y 不翻)
    void onMouseDragRotate(float dx, float dy,
                           bool invertX = false, bool invertY = false) noexcept;
    void onMouseDragPan   (float dx, float dy) noexcept;
    void onScroll(float dy) noexcept;

    // 适配模型包围盒
    void fitTo(const AABB& box, float padding = 1.4f) noexcept;

    // 复位
    void reset() noexcept;

    // 矩阵
    glm::mat4 view() const noexcept;
    glm::mat4 projection() const noexcept;
    glm::vec3 position() const noexcept;
    glm::vec3 forward()   const noexcept;
    glm::vec3 right()     const noexcept;
    glm::vec3 up()        const noexcept;

    // 从屏幕坐标生成 ray(返回 origin + dir 归一化)
    void screenRay(double sx, double sy, glm::vec3& origin, glm::vec3& dir) const noexcept;

private:
    mutable glm::vec3 _targetOffset{0.f};  // 平移累积
    void _applyTarget() const noexcept;
};

} // namespace prism
