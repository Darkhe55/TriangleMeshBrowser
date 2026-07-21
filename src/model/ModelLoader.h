// src/model/ModelLoader.h
// 统一入口 - 根据扩展名分发
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace prism {

class ModelLoader {
public:
    // 自动按扩展名分发(用 fs::path 内部直接拿 wide-char 路径,兼容中文路径)
    static std::unique_ptr<Mesh> load(const std::filesystem::path& filepath);

    // 支持的扩展名
    static const std::vector<std::string>& supportedExtensions();
};

} // namespace prism
