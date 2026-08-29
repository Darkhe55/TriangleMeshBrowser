// src/model/PmxData.h
// PMX 模型的材质 / 纹理 / 骨骼数据结构
#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>

namespace prism {

struct PmxMaterial {
    std::string name;
    glm::vec3 diffuse{1.f};
    float     alpha = 1.f;
    glm::vec3 specular{0.f};
    float     specularCoef = 0.f;
    glm::vec3 ambient{0.f};
    std::uint8_t drawFlags = 0;       // bit0 = 绘制边缘
    glm::vec3 edgeColor{0.f};
    float     edgeAlpha = 1.f;
    float     edgeSize  = 1.f;
    int       texIndex  = -1;         // 漫反射贴图 (指向纹理列表)
    int       sphIndex  = -1;         // Sphere 贴图
    int       sphMode   = 0;          // 0=关 1=乘 2=加 3=子纹理(按乘处理)
    int       toonFlag  = 0;          // 0=纹理引用 1=共享 toon
    int       toonIndex = -1;         // toonFlag=0 时指向纹理列表
    std::uint32_t firstIndex = 0;     // 在面索引数组中的起始位置
    std::uint32_t indexCount = 0;     // 本材质占用的索引个数 (3 的倍数)

    bool hasEdge() const noexcept { return (drawFlags & 0x01) != 0; }
};

struct PmxBone {
    std::string name;
    glm::vec3 position{0.f};
    std::int32_t parent = -1;
};

} // namespace prism
