// src/utils/StbWrite.h
// 用 stb_image_write 写 PNG
#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace prism {

// 把 RGBA 缓冲写到 PNG (自动垂直翻转)
// 失败抛 runtime_error
void writePNG(const std::string& path, int width, int height, const std::uint8_t* rgba);

} // namespace prism
