// src/model/MeshWriter.h
// 网格导出: OBJ / STL(二进制) / PLY(ASCII/二进制小端) / OFF
#pragma once

#include "Mesh.h"
#include <filesystem>

namespace prism {

struct MeshWriter {
    // 按扩展名分发,不支持的格式抛 runtime_error
    static void save(const Mesh& mesh, const std::filesystem::path& path);

    static void writeOBJ(const Mesh& mesh, const std::filesystem::path& path);
    static void writeSTL(const Mesh& mesh, const std::filesystem::path& path);
    static void writePLY(const Mesh& mesh, const std::filesystem::path& path, bool binary);
    static void writeOFF(const Mesh& mesh, const std::filesystem::path& path);
};

} // namespace prism
