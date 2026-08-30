// src/model/E57Loader.h
// E57 点云加载入口 (内置轻量解析器: pugixml 解析 XML 段 + 手写二进制段解码)
// 遍历文件内全部 Data3D 扫描, 应用各自位姿(四元数旋转+平移)合并为单一点云
// E57 为右手系 +Z 朝上, 与查看器一致不转换; 颜色按 colorLimits 归一化
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

std::unique_ptr<Mesh> loadE57(const std::filesystem::path& filepath);

} // namespace prism
