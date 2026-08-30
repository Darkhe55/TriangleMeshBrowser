// src/model/USDLoader.h
// USD/USDZ 加载入口 (tinyusdz: USDA/USDC 解析 + Tydra RenderScene 转换)
// 支持 .usd/.usda/.usdc/.usdz; USDZ 走容器内资源解析路径
// 轴向: USD 规范默认 +Y 朝上 → 转换为查看器的 +Z 朝上;元数据声明 Z 时保持
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>

namespace prism {

std::unique_ptr<Mesh> loadUSD(const std::filesystem::path& filepath);

} // namespace prism
