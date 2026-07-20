// src/model/Procedural.h
// 几何体生成器
#pragma once

#include "Mesh.h"
#include <memory>
#include <string>

namespace prism::procedural {

std::unique_ptr<Mesh> cube(float size = 1.0f);
std::unique_ptr<Mesh> sphere(float radius = 1.0f, int slices = 32, int stacks = 16);
std::unique_ptr<Mesh> cylinder(float radius = 1.0f, float height = 2.0f, int slices = 32);
std::unique_ptr<Mesh> torus(float major = 1.0f, float minor = 0.3f, int majorSeg = 48, int minorSeg = 16);
std::unique_ptr<Mesh> cone(float radius = 1.0f, float height = 2.0f, int slices = 32);

// 名称列表 (用于 UI 菜单)
const char* const* allNames() noexcept;
int                allCount()  noexcept;

} // namespace prism::procedural
