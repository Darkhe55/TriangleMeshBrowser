// src/model/Mesh.h
// 统一网格数据结构 - 顶点/索引/包围盒
#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace prism {

struct VertexPN  {
    glm::vec3 position;
    glm::vec3 normal;
};

struct VertexPNF {  // 带 face id (用于面片拾取)
    glm::vec3 position;
    glm::vec3 normal;
    std::uint32_t faceId;
};

struct Mesh {
    std::string name;

    // 两种互斥的顶点表达:
    // - indexed_ = true:  vertices_+indices_  (共享顶点)
    // - indexed_ = false: flat_             (每3顶点一个三角面,法线=面法线)
    bool indexed_ = false;
    std::vector<VertexPN>  vertices_;
    std::vector<std::uint32_t> indices_;

    // 拾取专用
    std::vector<VertexPNF> pickVertices_;   // 3 * triangleCount

    glm::vec3 bboxMin{0.f}, bboxMax{0.f};
    std::uint32_t triangleCount = 0;

    bool empty() const noexcept { return triangleCount == 0; }

    void computeNormals();           // 已有顶点情况
    void computeFaceNormals();       // flat 模式
    void computeBBox();
    void centerAndScale(float targetRadius = 1.0f);
    void buildPickData();
    void clear() noexcept;
};

} // namespace prism
