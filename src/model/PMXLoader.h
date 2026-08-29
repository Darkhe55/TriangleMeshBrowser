// src/model/PMXLoader.h
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

// 支持 PMX 2.0 / 2.1 (MikuMikuDance 模型格式)
// 提取几何(顶点/法线/UV/三角面) + 纹理路径 + 材质 + 骨骼(名称/位置)
std::unique_ptr<Mesh> loadPMX(const std::filesystem::path& path);

} // namespace prism
