// src/renderer/MeshRenderer.h
// 把 Mesh 包装成 GPU 资源(VAO/VBO/EBO) + 提供 draw
#pragma once

#include "GLResources.h"
#include "../model/Mesh.h"
#include <memory>

namespace prism {

class MeshRenderer {
public:
    // 从 Mesh 上传(拷贝数据),uploads = true 才传 GPU
    void upload(const Mesh& mesh);

    void drawSolid() const;
    void drawSolidRange(std::uint32_t firstIndex, std::uint32_t count) const;  // 按索引区间绘制 (PMX 材质分段)
    void drawWireframe() const;
    void drawPicker() const;     // 拾取用 - per-face flat

    GLuint vao() const noexcept { return vao_.get(); }
    GLuint pickVao() const noexcept { return pickVao_.get(); }
    std::uint32_t triangleCount() const noexcept { return triCount_; }
    bool   hasWireframe() const noexcept { return lineEbo_ != 0; }

    // 释放所有资源
    void clear() noexcept;

private:
    VaoPtr    vao_{};
    BufferPtr vbo_{};
    BufferPtr ebo_{};
    VaoPtr    lineVao_{};
    BufferPtr lineVbo_{};
    BufferPtr lineEbo_{};
    VaoPtr    pickVao_{};
    BufferPtr pickVbo_{};

    std::uint32_t triCount_ = 0;
    std::uint32_t indexCount_ = 0;
    std::uint32_t edgeIndexCount_ = 0;
    bool indexed_ = false;
};

} // namespace prism
