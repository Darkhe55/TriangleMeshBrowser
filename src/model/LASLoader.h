// src/model/LASLoader.h
// LAS/LAZ 点云加载入口 (经 laszip 解析, LAZ 自动识别压缩)
// 产出 pointCloud = true 的 Mesh;LAS 规范为右手系 +Z 朝上,与查看器一致不转换
// 颜色: 含 RGB 的点格式(2/3/5/7/8)读取 16-bit RGB, 全黑则丢弃颜色属性
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

std::unique_ptr<Mesh> loadLAS(const std::filesystem::path& filepath);

} // namespace prism
