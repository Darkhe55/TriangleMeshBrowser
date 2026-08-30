// src/utils/StbWrite.cpp
#include "StbWrite.h"
// STB_IMAGE_WRITE_STATIC: stbi_write_* 编译为文件局部符号 (static),
// 避免与 tinyusdz 静态库 internal 的 image-writer.cc.obj 导出同名
// stbi_write_* (extern) 冲突 (LNK2005)。writePNG 在同一翻译单元仍可调用。
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace prism {

void writePNG(const std::string& path, int width, int height, const std::uint8_t* rgba) {
    if (!rgba || width <= 0 || height <= 0)
        throw std::runtime_error("writePNG: invalid args");

    // 垂直翻转
    std::vector<std::uint8_t> flipped(static_cast<size_t>(width) * height * 4);
    size_t rowBytes = static_cast<size_t>(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::memcpy(flipped.data() + (height - 1 - y) * rowBytes,
                    rgba + y * rowBytes,
                    rowBytes);
    }

    int ok = stbi_write_png(path.c_str(), width, height, 4,
                            flipped.data(), static_cast<int>(rowBytes));
    if (!ok) throw std::runtime_error("Failed to write PNG: " + path);
}

} // namespace prism
