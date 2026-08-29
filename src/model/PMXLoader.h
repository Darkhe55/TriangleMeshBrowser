// src/model/PMXLoader.h
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

// 支持 PMX 2.0 / 2.1 (MikuMikuDance 模型格式)
// 仅提取几何信息:顶点位置 / 法线 / 三角面索引
std::unique_ptr<Mesh> loadPMX(const std::filesystem::path& path);

} // namespace prism
