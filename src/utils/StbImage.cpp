// src/utils/StbImage.cpp
#include "StbImage.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdexcept>

namespace prism {

std::vector<std::uint8_t> loadImageRGBA(const fs::path& path, int& width, int& height) {
    const std::vector<std::uint8_t> buf = readFileBinary(path);
    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(buf.data(),
                                        static_cast<int>(buf.size()),
                                        &w, &h, &comp, 4);
    if (!px) throw std::runtime_error("Cannot decode image: " + wideToUtf8(path.wstring()));
    std::vector<std::uint8_t> out(px, px + static_cast<size_t>(w) * h * 4);
    stbi_image_free(px);
    width = w;
    height = h;
    return out;
}

} // namespace prism
