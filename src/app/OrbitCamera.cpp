// src/app/OrbitCamera.cpp
#include "OrbitCamera.h"
#include <cmath>

namespace prism {

void OrbitCamera::_applyTarget() const noexcept {
    target += _targetOffset;
    _targetOffset = glm::vec3(0.f);
}

void OrbitCamera::reset() noexcept {
    yaw = 45.f;
    pitch = 30.f;
    distance = 3.0f;
    target = glm::vec3(0.f);
    _targetOffset = glm::vec3(0.f);
    fovDeg = 45.f;
    nearZ = 0.05f;
    farZ = 1000.f;
}

void OrbitCamera::onMouseDragRotate(float dx, float dy,
                                   bool invertX, bool invertY) noexcept {
    constexpr float kSens = 0.4f;
    float sx = invertX ? -1.f : 1.f;
    float sy = invertY ? -1.f : 1.f;
    yaw   += dx * kSens * sx;
    pitch -= dy * kSens * sy;  // 屏幕 y 轴向下
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    // wrap yaw
    while (yaw   >  180.f) yaw   -= 360.f;
    while (yaw   < -180.f) yaw   += 360.f;
}

void OrbitCamera::onMouseDragPan(float dx, float dy) noexcept {
    // 屏幕像素 -> 世界单位;按当前 distance 缩放
    float dist = std::max(distance, 0.01f);
    float worldPerPixel = 2.0f * std::tan(glm::radians(fovDeg) * 0.5f) * dist
                          / static_cast<float>(std::max(viewportH, 1));
    // 先应用累积的平移量
    _applyTarget();
    glm::vec3 delta = -right()   * (dx * worldPerPixel)
                     +  up()     * (dy * worldPerPixel);
    _targetOffset += delta;
}

void OrbitCamera::onScroll(float dy) noexcept {
    constexpr float kZoom = 0.12f;
    float factor = std::exp(dy * kZoom);
    distance = std::clamp(distance * factor, 0.05f, 200.f);
}

void OrbitCamera::fitTo(const AABB& box, float padding) noexcept {
    _targetOffset = glm::vec3(0.f);
    target = box.center();
    float r = std::max(box.radius(), 1e-3f);
    // fov-based distance
    float halfFov = glm::radians(fovDeg) * 0.5f;
    distance = r * padding / std::tan(halfFov);
    distance = std::clamp(distance, 0.05f, 200.f);
}

glm::vec3 OrbitCamera::position() const noexcept {
    _applyTarget();
    // 球坐标约定:Z 轴为垂直(pitch=0 处于 xOy 平面,pitch=90 飞到 z=+distance)
    //           yaw 绕 Z 轴旋转(yaw=0 看向 +X,yaw=90 看向 +Y)
    float yawR = glm::radians(yaw);
    float pitR = glm::radians(pitch);
    float cp = std::cos(pitR), sp = std::sin(pitR);
    float cy = std::cos(yawR), sy = std::sin(yawR);
    return target + distance * glm::vec3(cp * cy, cp * sy, sp);
}

glm::mat4 OrbitCamera::view() const noexcept {
    _applyTarget();
    return glm::lookAt(position(), target, glm::vec3(0.f, 0.f, 1.f));
}

glm::mat4 OrbitCamera::projection() const noexcept {
    float aspect = static_cast<float>(std::max(viewportW, 1))
                 / static_cast<float>(std::max(viewportH, 1));
    return glm::perspective(glm::radians(fovDeg), aspect, nearZ, farZ);
}

glm::vec3 OrbitCamera::forward() const noexcept { return glm::normalize(target - position()); }
glm::vec3 OrbitCamera::right()   const noexcept { return glm::normalize(glm::cross(forward(), glm::vec3(0,0,1))); }
glm::vec3 OrbitCamera::up()      const noexcept { return glm::normalize(glm::cross(right(), forward())); }

void OrbitCamera::screenRay(double sx, double sy, glm::vec3& origin, glm::vec3& dir) const noexcept {
    // sx, sy in screen pixels (origin top-left)
    double ndcX = (2.0 * sx / std::max(viewportW, 1)) - 1.0;
    double ndcY = 1.0 - (2.0 * sy / std::max(viewportH, 1));
    glm::mat4 invVP = glm::inverse(projection() * view());
    glm::vec4 nearP = invVP * glm::vec4(ndcX, ndcY, -1.0, 1.0);
    glm::vec4 farP  = invVP * glm::vec4(ndcX, ndcY,  1.0, 1.0);
    nearP /= nearP.w;
    farP  /= farP.w;
    origin = glm::vec3(nearP);
    dir    = glm::normalize(glm::vec3(farP) - glm::vec3(nearP));
}

} // namespace prism
