// src/model/ThreeMFLoader.h
// 3MF 格式加载入口 (.3mf 3D Manufacturing Format,经 Assimp 解析)
// 提取几何 + 材质 + 纹理路径
// 轴向: 3MF 规范为右手系 +Z 朝上(毫米),保持原样不转换
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

std::unique_ptr<Mesh> load3MF(const std::filesystem::path& filepath);

} // namespace prism
