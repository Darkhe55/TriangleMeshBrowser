// src/model/OBJLoader.h
#pragma once

#include "Mesh.h"
#include <filesystem>
#include <memory>
#include <string>

namespace prism {

// 解析 OBJ 文件。失败抛 runtime_error
std::unique_ptr<Mesh> loadOBJ(const std::filesystem::path& path);

} // namespace prism
