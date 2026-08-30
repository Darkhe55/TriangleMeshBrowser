// src/model/Mesh.h
// 统一网格数据结构 - 顶点/索引/包围盒/PMX 材质数据
#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include "PmxData.h"

namespace prism {

struct VertexPN  {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv{0.f};   // 仅 PMX 模型有值,其余格式为 0
};

struct VertexPNF {  // 带 face id (用于面片拾取)
    glm::vec3 position;
    glm::vec3 normal;
    std::uint32_t faceId;
};

struct VertexPC {   // 点云 (位置 + 颜色)
    glm::vec3 position;
    glm::vec3 color{1.f};
};

struct Mesh {
    std::string name;

    // 两种互斥的顶点表达:
    // - indexed_ = true:  vertices_+indices_  (共享顶点)
    // - indexed_ = false: flat_             (每3顶点一个三角面,法线=面法线)
    // 点云格式 (LAS/LAZ/E57) 另用 pointCloud = true: vertices_ 存点坐标,无三角形索引。
    bool indexed_ = false;
    std::vector<VertexPN>  vertices_;
    std::vector<std::uint32_t> indices_;

    // 点云专属 (仅 pointCloud = true 时有意义)
    bool pointCloud = false;
    std::vector<glm::vec3> pointColors;   // 与 vertices_ 等长;为空则无颜色属性

    // 拾取专用
    std::vector<VertexPNF> pickVertices_;   // 3 * triangleCount

    glm::vec3 bboxMin{0.f}, bboxMax{0.f};
    std::uint32_t triangleCount = 0;

    // centerAndScale 实际应用的缩放系数(边缘等模型空间量换算用)
    float scaleApplied = 1.f;

    // 材质数据 (PMX / FBX / glTF / GLB; 无材质的格式为空)
    std::vector<std::string>  pmxTexturePaths;  // 纹理路径列表(材质按索引引用)
    std::vector<PmxMaterial>  pmxMaterials;     // 面索引范围连续且总和 = indices_.size()
    std::vector<PmxBone>      pmxBones;         // 仅 PMX 有值

    bool empty() const noexcept { return vertices_.empty(); }
    bool hasPmxMaterials() const noexcept { return !pmxMaterials.empty(); }  // 含材质(任一格式)
    std::uint32_t pointCount() const noexcept {
        return pointCloud ? static_cast<std::uint32_t>(vertices_.size()) : 0;
    }

    void computeNormals();           // 已有顶点情况
    void computeFaceNormals();       // flat 模式
    void computeBBox();
    void centerAndScale(float targetRadius = 1.0f);
    void buildPickData();
    void clear() noexcept;
};

} // namespace prism
