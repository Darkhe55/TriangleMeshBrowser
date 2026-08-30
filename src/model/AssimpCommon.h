// src/model/AssimpCommon.h
// Assimp 公共导入逻辑: 场景扁平化 + 材质/纹理解析
// 供 FBXLoader / GLTFLoader / ColladaLoader / ThreeMFLoader 复用
// 轴向: Y-up 格式加载时绕 X 轴 +90° 转为查看器的 +Z 朝上
//       (x, y, z) -> (x, -z, y)
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

// 上轴策略:
// - Convert:      规范为 +Y 朝上,一律转换 (glTF / Collada)
// - Keep:         规范为 +Z 朝上,不转换 (3MF)
// - FromMetadata: 读场景元数据 UpAxis,仅 +Y 朝上才转换 (FBX,
//                 3ds Max 导出的 Z-up FBX 保持原样)
enum class UpAxisPolicy { Convert, Keep, FromMetadata };

// 从内存缓冲经 Assimp 导入并扁平化为统一网格 (几何 + 材质 + 纹理路径)
// hint: Assimp 格式提示("fbx" / "gltf2" / "glb" / "collada" / "3mf")
// modelDir: 模型所在目录(嵌入式贴图临时 PNG 的落盘位置)
// 解析失败抛 runtime_error
std::unique_ptr<Mesh> importWithAssimp(const std::vector<std::uint8_t>& data,
                                       const std::string& hint,
                                       const std::filesystem::path& modelDir,
                                       const std::string& fileName,
                                       UpAxisPolicy axisPolicy);

} // namespace prism
