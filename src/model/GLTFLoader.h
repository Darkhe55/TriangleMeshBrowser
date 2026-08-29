// src/model/GLTFLoader.h
// glTF 2.0 加载入口 (.gltf JSON / .glb 二进制容器,经 Assimp 解析)
// 提取几何 + 材质 + 纹理路径
// 轴向: glTF 规范为右手系 +Y 朝上,加载时转换为查看器的 +Z 朝上
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

std::unique_ptr<Mesh> loadGLTF(const std::filesystem::path& filepath);

} // namespace prism
