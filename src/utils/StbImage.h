// src/utils/StbImage.h
// 用 stb_image 读取图片为 RGBA
#pragma once

#include "FileUtils.h"
#include <cstdint>
#include <vector>

namespace prism {

// 读取图片文件并解码为 RGBA8,失败抛 runtime_error
// (内部经 readFileBinary + stbi_load_from_memory,支持中文路径)
std::vector<std::uint8_t> loadImageRGBA(const fs::path& path, int& width, int& height);

} // namespace prism
