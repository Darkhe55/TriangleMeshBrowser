// src/ui/Picker.h
// Ray-triangle 拾取 (Möller-Trumbore)
#pragma once

#include "../app/OrbitCamera.h"
#include "../model/Mesh.h"
#include <cstdint>
#include <optional>

namespace prism {

struct PickHit {
    std::uint32_t faceId = 0;
    float t = 0.f;  // ray 距离
};

class Picker {
public:
    // 在 mesh 上对 ray 做拾取。返回最近命中
    static std::optional<PickHit> pick(const OrbitCamera& cam,
                                      const Mesh& mesh,
                                      double screenX, double screenY);
};

} // namespace prism
