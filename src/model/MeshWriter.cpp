// src/model/MeshWriter.cpp
#include "MeshWriter.h"
#include "../utils/FileUtils.h"
#include <glm/glm.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <vector>

namespace prism {

namespace {

// 面迭代: 兼容索引与扁平两种存储
std::uint32_t idxAt(const Mesh& m, std::uint32_t k) {
    return m.indexed_ ? m.indices_[k] : k;
}

// 三角面法线(顶点法线为 0 时兜底)
glm::vec3 faceNormal(const Mesh& m, std::uint32_t i0, std::uint32_t i1, std::uint32_t i2) {
    const glm::vec3& a = m.vertices_[i0].position;
    const glm::vec3& b = m.vertices_[i1].position;
    const glm::vec3& c = m.vertices_[i2].position;
    glm::vec3 n = glm::cross(b - a, c - a);
    return glm::length(n) > 1e-12f ? glm::normalize(n) : glm::vec3(0.f, 0.f, 1.f);
}

std::ofstream openOut(const std::filesystem::path& path, bool binary) {
    std::ofstream f(path, binary ? (std::ios::binary) : (std::ios::out));
    if (!f) throw std::runtime_error("Cannot create file: " + wideToUtf8(path.wstring()));
    return f;
}

} // namespace

void MeshWriter::save(const Mesh& mesh, const std::filesystem::path& path) {
    if (mesh.empty()) throw std::runtime_error("Mesh is empty, nothing to export");
    const std::string ext = getExtension(path);
    if (ext == ".obj") { writeOBJ(mesh, path); return; }
    if (ext == ".stl") { writeSTL(mesh, path); return; }
    if (ext == ".ply") { writePLY(mesh, path, false); return; }  // 默认 ASCII
    if (ext == ".off") { writeOFF(mesh, path); return; }
    throw std::runtime_error("Unsupported export format: " + ext +
                             " (supported: .obj .stl .ply .off)");
}

void MeshWriter::writeOBJ(const Mesh& mesh, const std::filesystem::path& path) {
    std::ofstream f = openOut(path, false);
    f << "# Exported by PrismViewer\n";
    for (const auto& v : mesh.vertices_) {
        f << "v " << v.position.x << ' ' << v.position.y << ' ' << v.position.z << '\n';
    }
    for (const auto& v : mesh.vertices_) {
        f << "vn " << v.normal.x << ' ' << v.normal.y << ' ' << v.normal.z << '\n';
    }
    const std::uint32_t total = mesh.triangleCount * 3;
    for (std::uint32_t k = 0; k + 2 < total; k += 3) {
        // OBJ 索引 1-based
        const std::uint32_t a = idxAt(mesh, k) + 1;
        const std::uint32_t b = idxAt(mesh, k + 1) + 1;
        const std::uint32_t c = idxAt(mesh, k + 2) + 1;
        f << "f " << a << "//" << a << ' ' << b << "//" << b << ' ' << c << "//" << c << '\n';
    }
    if (!f) throw std::runtime_error("Failed to write OBJ: " + wideToUtf8(path.wstring()));
}

void MeshWriter::writeSTL(const Mesh& mesh, const std::filesystem::path& path) {
    std::ofstream f = openOut(path, true);
    char header[80] = {0};
    std::strncpy(header, "PrismViewer export", sizeof(header) - 1);
    f.write(header, 80);
    const std::uint32_t triCount = mesh.triangleCount;
    f.write(reinterpret_cast<const char*>(&triCount), 4);

    const std::uint16_t attr = 0;
    for (std::uint32_t t = 0; t < triCount; ++t) {
        const std::uint32_t i0 = idxAt(mesh, 3 * t + 0);
        const std::uint32_t i1 = idxAt(mesh, 3 * t + 1);
        const std::uint32_t i2 = idxAt(mesh, 3 * t + 2);
        const glm::vec3 n = faceNormal(mesh, i0, i1, i2);
        f.write(reinterpret_cast<const char*>(&n.x), 12);
        f.write(reinterpret_cast<const char*>(&mesh.vertices_[i0].position.x), 12);
        f.write(reinterpret_cast<const char*>(&mesh.vertices_[i1].position.x), 12);
        f.write(reinterpret_cast<const char*>(&mesh.vertices_[i2].position.x), 12);
        f.write(reinterpret_cast<const char*>(&attr), 2);
    }
    if (!f) throw std::runtime_error("Failed to write STL: " + wideToUtf8(path.wstring()));
}

void MeshWriter::writePLY(const Mesh& mesh, const std::filesystem::path& path, bool binary) {
    std::ofstream f = openOut(path, binary);
    f << "ply\n";
    f << (binary ? "format binary_little_endian 1.0\n" : "format ascii 1.0\n");
    f << "comment Exported by PrismViewer\n";
    f << "element vertex " << mesh.vertices_.size() << "\n";
    f << "property float x\nproperty float y\nproperty float z\n";
    f << "property float nx\nproperty float ny\nproperty float nz\n";
    f << "element face " << mesh.triangleCount << "\n";
    f << "property list uchar int vertex_indices\n";
    f << "end_header\n";

    if (!binary) {
        for (const auto& v : mesh.vertices_) {
            f << v.position.x << ' ' << v.position.y << ' ' << v.position.z << ' '
              << v.normal.x << ' ' << v.normal.y << ' ' << v.normal.z << '\n';
        }
        const std::uint32_t total = mesh.triangleCount * 3;
        for (std::uint32_t k = 0; k + 2 < total; k += 3) {
            f << "3 " << idxAt(mesh, k) << ' ' << idxAt(mesh, k + 1) << ' '
              << idxAt(mesh, k + 2) << '\n';
        }
    } else {
        // x64 原生即小端,直接按字节写出
        for (const auto& v : mesh.vertices_) {
            f.write(reinterpret_cast<const char*>(&v.position.x), 12);
            f.write(reinterpret_cast<const char*>(&v.normal.x), 12);
        }
        const std::uint32_t total = mesh.triangleCount * 3;
        for (std::uint32_t k = 0; k + 2 < total; k += 3) {
            const std::uint8_t n = 3;
            const std::int32_t a = static_cast<std::int32_t>(idxAt(mesh, k));
            const std::int32_t b = static_cast<std::int32_t>(idxAt(mesh, k + 1));
            const std::int32_t c = static_cast<std::int32_t>(idxAt(mesh, k + 2));
            f.write(reinterpret_cast<const char*>(&n), 1);
            f.write(reinterpret_cast<const char*>(&a), 4);
            f.write(reinterpret_cast<const char*>(&b), 4);
            f.write(reinterpret_cast<const char*>(&c), 4);
        }
    }
    if (!f) throw std::runtime_error("Failed to write PLY: " + wideToUtf8(path.wstring()));
}

void MeshWriter::writeOFF(const Mesh& mesh, const std::filesystem::path& path) {
    std::ofstream f = openOut(path, false);
    f << "OFF\n";
    f << mesh.vertices_.size() << ' ' << mesh.triangleCount << " 0\n";
    for (const auto& v : mesh.vertices_) {
        f << v.position.x << ' ' << v.position.y << ' ' << v.position.z << '\n';
    }
    const std::uint32_t total = mesh.triangleCount * 3;
    for (std::uint32_t k = 0; k + 2 < total; k += 3) {
        f << "3 " << idxAt(mesh, k) << ' ' << idxAt(mesh, k + 1) << ' '
          << idxAt(mesh, k + 2) << '\n';
    }
    if (!f) throw std::runtime_error("Failed to write OFF: " + wideToUtf8(path.wstring()));
}

} // namespace prism
