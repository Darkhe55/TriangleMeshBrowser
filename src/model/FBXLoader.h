// src/model/FBXLoader.h
// FBX 格式加载入口 (ASCII + Binary,经 Assimp 解析)
// 提取几何 + 材质 + 纹理路径
// 轴向: 按文件自带 UpAxis 元数据,+Y 朝上时转换为查看器的 +Z 朝上
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

std::unique_ptr<Mesh> loadFBX(const std::filesystem::path& filepath);

} // namespace prism
