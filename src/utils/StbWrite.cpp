// src/utils/StbWrite.cpp
#include "StbWrite.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace prism {

void writePNG(const std::string& path, int width, int height, const std::uint8_t* rgba) {
    if (!rgba || width <= 0 || height <= 0)
        throw std::runtime_error("writePNG: invalid args");

    // 垂直翻转(GL 原点在左下,PNG 在左上)
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
