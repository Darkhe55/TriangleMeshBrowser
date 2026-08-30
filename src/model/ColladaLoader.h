// src/model/ColladaLoader.h
// Collada 格式加载入口 (.dae,经 Assimp 解析)
// 提取几何 + 材质 + 纹理路径
// 轴向: Collada 规范为右手系 +Y 朝上,加载时转换为查看器的 +Z 朝上
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

std::unique_ptr<Mesh> loadDAE(const std::filesystem::path& filepath);

} // namespace prism
