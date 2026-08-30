// src/renderer/MeshRenderer.cpp
#include "MeshRenderer.h"
#include <glm/glm.hpp>
#include <unordered_set>
#include <vector>
#include <cstdint>

namespace prism {

void MeshRenderer::clear() noexcept {
    vao_.reset();
    vbo_.reset();
    ebo_.reset();
    lineVao_.reset();
    lineEbo_.reset();
    pickVao_.reset();
    pickVbo_.reset();
    pointVao_.reset();
    pointVbo_.reset();
    triCount_ = indexCount_ = edgeIndexCount_ = pointCount_ = 0;
    indexed_ = false;
    pointHasColor_ = false;
}

static void computeEdgeIndices(const std::vector<std::uint32_t>& triIndices,
                                std::vector<std::uint32_t>& outEdges) {
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(triIndices.size());
    outEdges.reserve(triIndices.size());
    auto key = [](std::uint32_t a, std::uint32_t b) -> std::uint64_t {
        if (a > b) std::swap(a, b);
        return (static_cast<std::uint64_t>(a) << 32) | b;
    };
    for (size_t i = 0; i + 2 < triIndices.size(); i += 3) {
        std::uint32_t v[3] = { triIndices[i], triIndices[i + 1], triIndices[i + 2] };
        for (int k = 0; k < 3; ++k) {
            std::uint32_t a = v[k], b = v[(k + 1) % 3];
            std::uint64_t kk = key(a, b);
            if (seen.insert(kk).second) {
                outEdges.push_back(a);
                outEdges.push_back(b);
            }
        }
    }
}

void MeshRenderer::upload(const Mesh& mesh) {
    clear();
    triCount_  = mesh.triangleCount;
    indexed_   = mesh.indexed_;
    indexCount_ = static_cast<std::uint32_t>(mesh.indices_.size());
    edgeIndexCount_ = 0;

    // 点云: 单独上传位置+颜色,早退(无三角形/线框/拾取)
    if (mesh.pointCloud) {
        pointCount_ = static_cast<std::uint32_t>(mesh.vertices_.size());
        if (pointCount_ == 0) return;
        const bool hasColor = mesh.pointColors.size() == mesh.vertices_.size();
        pointHasColor_ = hasColor;
        std::vector<VertexPC> pts;
        pts.reserve(mesh.vertices_.size());
        for (size_t i = 0; i < mesh.vertices_.size(); ++i) {
            VertexPC p;
            p.position = mesh.vertices_[i].position;
            p.color = hasColor ? mesh.pointColors[i] : glm::vec3(1.f);
            pts.push_back(p);
        }
        GLuint pvao = 0, pvbo = 0;
        glGenVertexArrays(1, &pvao);
        glGenBuffers(1, &pvbo);
        pointVao_.reset(pvao);
        pointVbo_.reset(pvbo);
        glBindVertexArray(pvao);
        glBindBuffer(GL_ARRAY_BUFFER, pvbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(pts.size() * sizeof(VertexPC)),
                     pts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPC),
                              reinterpret_cast<void*>(offsetof(VertexPC, position)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPC),
                              reinterpret_cast<void*>(offsetof(VertexPC, color)));
        glBindVertexArray(0);
        return;
    }

    if (triCount_ == 0) return;

    // --- main VAO: pos + normal ---
    GLuint vao = 0, vbo = 0, ebo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    vao_.reset(vao);
    vbo_.reset(vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices_.size() * sizeof(VertexPN)),
                 mesh.vertices_.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN),
                          reinterpret_cast<void*>(offsetof(VertexPN, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN),
                          reinterpret_cast<void*>(offsetof(VertexPN, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPN),
                          reinterpret_cast<void*>(offsetof(VertexPN, uv)));

    if (indexed_) {
        glGenBuffers(1, &ebo);
        ebo_.reset(ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.indices_.size() * sizeof(std::uint32_t)),
                     mesh.indices_.data(), GL_STATIC_DRAW);
    }
    glBindVertexArray(0);

    // --- line VAO: 仅 position, GL_LINES 边索引 ---
    std::vector<std::uint32_t> edges;
    if (indexed_) {
        computeEdgeIndices(mesh.indices_, edges);
    } else {
        // flat 模式:每 3 顶点一个面,边 = (0,1)(1,2)(2,0)
        edges.reserve(mesh.vertices_.size() * 2);
        for (std::uint32_t i = 0; i < mesh.vertices_.size(); i += 3) {
            edges.push_back(i);     edges.push_back(i + 1);
            edges.push_back(i + 1); edges.push_back(i + 2);
            edges.push_back(i + 2); edges.push_back(i);
        }
    }
    if (!edges.empty()) {
        edgeIndexCount_ = static_cast<std::uint32_t>(edges.size());
        GLuint lvao = 0, lebo = 0;
        glGenVertexArrays(1, &lvao);
        glGenBuffers(1, &lebo);
        lineVao_.reset(lvao);
        lineEbo_.reset(lebo);

        glBindVertexArray(lvao);
        // 共享主 VBO,避免顶点数据重复占用显存与重复上传耗时 (线框只用 position 属性)
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN),
                              reinterpret_cast<void*>(offsetof(VertexPN, position)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(edges.size() * sizeof(std::uint32_t)),
                     edges.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);
    }

    // --- picker VAO: pos + normal + faceId ---
    if (!mesh.pickVertices_.empty()) {
        GLuint pvao = 0, pvbo = 0;
        glGenVertexArrays(1, &pvao);
        glGenBuffers(1, &pvbo);
        pickVao_.reset(pvao);
        pickVbo_.reset(pvbo);

        glBindVertexArray(pvao);
        glBindBuffer(GL_ARRAY_BUFFER, pvbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.pickVertices_.size() * sizeof(VertexPNF)),
                     mesh.pickVertices_.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNF),
                              reinterpret_cast<void*>(offsetof(VertexPNF, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNF),
                              reinterpret_cast<void*>(offsetof(VertexPNF, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(VertexPNF),
                               reinterpret_cast<void*>(offsetof(VertexPNF, faceId)));
        glBindVertexArray(0);
    }
}

void MeshRenderer::drawSolid() const {
    if (!vao_) return;
    glBindVertexArray(vao_.get());
    if (indexed_) {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(indexCount_));
    }
}

void MeshRenderer::drawSolidRange(std::uint32_t first, std::uint32_t count) const {
    if (!vao_) return;
    glBindVertexArray(vao_.get());
    if (indexed_) {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(static_cast<std::uintptr_t>(first) * sizeof(std::uint32_t)));
    } else {
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(first), static_cast<GLsizei>(count));
    }
}

void MeshRenderer::drawWireframe() const {
    if (!lineVao_ || edgeIndexCount_ == 0) return;
    glBindVertexArray(lineVao_.get());
    glDrawElements(GL_LINES, static_cast<GLsizei>(edgeIndexCount_), GL_UNSIGNED_INT, nullptr);
}

void MeshRenderer::drawPicker() const {
    if (!pickVao_) return;
    glBindVertexArray(pickVao_.get());
    // pickVertices_ 每三角面 3 顶点
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triCount_ * 3));
}

void MeshRenderer::drawPoints() const {
    if (!pointVao_ || pointCount_ == 0) return;
    glBindVertexArray(pointVao_.get());
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pointCount_));
}

} // namespace prism
