// src/renderer/Grid.h
// XOY 平面网格 (Z=0)
#pragma once

#include "GLResources.h"
#include <cstdint>

namespace prism {

class Grid {
public:
    Grid() = default;
    // halfExtent: 网格从 -halfExtent 到 +halfExtent(每条主轴)
    // step:       相邻副线距离(主线 0 处单独加粗)
    Grid(float halfExtent, float step);

    void upload();
    void clear() noexcept;

    void draw() const;

private:
    float halfExtent_ = 5.0f;
    float step_       = 1.0f;
    VaoPtr    vao_{};
    BufferPtr vbo_{};
    std::uint32_t vertexCount_ = 0;
    bool        uploaded_ = false;
};

} // namespace prism
