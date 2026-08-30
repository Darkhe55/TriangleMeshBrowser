// src/model/Mesh.cpp
#include "Mesh.h"
#include <algorithm>
#include <limits>

namespace prism {

void Mesh::clear() noexcept {
    name.clear();
    vertices_.clear();
    indices_.clear();
    pointColors.clear();
    pointCloud = false;
    pickVertices_.clear();
    bboxMin = bboxMax = glm::vec3(0.f);
    triangleCount = 0;
    indexed_ = false;
    scaleApplied = 1.f;
    pmxTexturePaths.clear();
    pmxMaterials.clear();
    pmxBones.clear();
}

void Mesh::computeBBox() {
    if (vertices_.empty()) {
        bboxMin = bboxMax = glm::vec3(0.f);
        return;
    }
    bboxMin = glm::vec3( std::numeric_limits<float>::infinity());
    bboxMax = glm::vec3(-std::numeric_limits<float>::infinity());
    for (const auto& v : vertices_) {
        bboxMin = glm::min(bboxMin, v.position);
        bboxMax = glm::max(bboxMax, v.position);
    }
}

void Mesh::centerAndScale(float targetRadius) {
    computeBBox();
    glm::vec3 center = (bboxMin + bboxMax) * 0.5f;
    glm::vec3 extent = bboxMax - bboxMin;
    float maxDim = std::max({extent.x, extent.y, extent.z, 1e-6f});
    float scale = (targetRadius * 2.0f) / maxDim;
    scaleApplied = scale;
    for (auto& v : vertices_) {
        v.position = (v.position - center) * scale;
    }
    for (auto& v : pickVertices_) {
        v.position = (v.position - center) * scale;
    }
    bboxMin = (bboxMin - center) * scale;
    bboxMax = (bboxMax - center) * scale;
}

void Mesh::computeFaceNormals() {
    if (indexed_) return;  // 仅 flat 模式
    for (size_t i = 0; i + 2 < vertices_.size(); i += 3) {
        auto& a = vertices_[i + 0].position;
        auto& b = vertices_[i + 1].position;
        auto& c = vertices_[i + 2].position;
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        vertices_[i + 0].normal = n;
        vertices_[i + 1].normal = n;
        vertices_[i + 2].normal = n;
    }
}

void Mesh::computeNormals() {
    // 给 indexed 模式用:累加面法线到顶点法线
    if (!indexed_ || indices_.size() < 3) return;
    for (auto& v : vertices_) v.normal = glm::vec3(0.f);
    for (size_t i = 0; i + 2 < indices_.size(); i += 3) {
        auto ia = indices_[i + 0], ib = indices_[i + 1], ic = indices_[i + 2];
        if (ia >= vertices_.size() || ib >= vertices_.size() || ic >= vertices_.size()) continue;
        glm::vec3 a = vertices_[ia].position;
        glm::vec3 b = vertices_[ib].position;
        glm::vec3 c = vertices_[ic].position;
        glm::vec3 n = glm::cross(b - a, c - a);
        vertices_[ia].normal += n;
        vertices_[ib].normal += n;
        vertices_[ic].normal += n;
    }
    for (auto& v : vertices_) {
        v.normal = glm::length(v.normal) > 1e-8f
            ? glm::normalize(v.normal)
            : glm::vec3(0.f, 0.f, 1.f);
    }
}

void Mesh::buildPickData() {
    pickVertices_.clear();
    if (pointCloud) return;   // 点云无三角面,不参与拾取
    pickVertices_.reserve(static_cast<size_t>(triangleCount) * 3);

    if (indexed_) {
        for (std::uint32_t f = 0; f < triangleCount; ++f) {
            std::uint32_t i0 = indices_[3 * f + 0];
            std::uint32_t i1 = indices_[3 * f + 1];
            std::uint32_t i2 = indices_[3 * f + 2];
            const auto& a = vertices_[i0];
            const auto& b = vertices_[i1];
            const auto& c = vertices_[i2];
            pickVertices_.push_back({a.position, a.normal, f});
            pickVertices_.push_back({b.position, b.normal, f});
            pickVertices_.push_back({c.position, c.normal, f});
        }
    } else {
        for (std::uint32_t f = 0; f < triangleCount; ++f) {
            const auto& a = vertices_[3 * f + 0];
            const auto& b = vertices_[3 * f + 1];
            const auto& c = vertices_[3 * f + 2];
            pickVertices_.push_back({a.position, a.normal, f});
            pickVertices_.push_back({b.position, b.normal, f});
            pickVertices_.push_back({c.position, c.normal, f});
        }
    }
}

} // namespace prism
