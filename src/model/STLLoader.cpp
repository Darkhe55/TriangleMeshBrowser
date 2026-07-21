// src/model/STLLoader.cpp
#include "STLLoader.h"
#include "../utils/FileUtils.h"
#include <glm/glm.hpp>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdint>

namespace prism {

// ASCII STL:每三角面:
//   solid name
//   facet normal nx ny nz
//     outer loop
//       vertex x y z
//       vertex x y z
//       vertex x y z
//     endloop
//   endfacet
//   ...
//   endsolid name
static std::unique_ptr<Mesh> loadSTL_ASCII(std::istream& in, const std::string& name) {
    auto mesh = std::make_unique<Mesh>();
    mesh->name = name;
    mesh->indexed_ = false;
    mesh->vertices_.reserve(1024);

    std::string tok;
    glm::vec3 normal(0.f, 0.f, 1.f);
    glm::vec3 verts[3];
    int vIdx = 0;

    while (in >> tok) {
        if (tok == "facet") {
            in >> tok >> normal.x >> normal.y >> normal.z;
            vIdx = 0;
        } else if (tok == "vertex") {
            if (vIdx < 3) {
                in >> verts[vIdx].x >> verts[vIdx].y >> verts[vIdx].z;
                mesh->vertices_.push_back({verts[vIdx], normal});
                ++vIdx;
            } else {
                // 防御性:多余 vertex
                glm::vec3 dummy;
                in >> dummy.x >> dummy.y >> dummy.z;
            }
        } else if (tok == "endsolid") {
            break;
        }
    }
    if (mesh->vertices_.size() % 3 != 0) {
        throw std::runtime_error("STL ASCII: invalid vertex count: " + name);
    }
    mesh->triangleCount = static_cast<std::uint32_t>(mesh->vertices_.size() / 3);
    mesh->computeBBox();
    return mesh;
}

static std::unique_ptr<Mesh> loadSTL_Binary(std::ifstream& f, const std::string& name, std::uint32_t triCount) {
    auto mesh = std::make_unique<Mesh>();
    mesh->name = name;
    mesh->indexed_ = false;
    mesh->vertices_.reserve(static_cast<size_t>(triCount) * 3);

    for (std::uint32_t i = 0; i < triCount; ++i) {
        float data[12];
        std::uint16_t attr;
        f.read(reinterpret_cast<char*>(data), 48);
        if (!f) throw std::runtime_error("STL Binary: truncated at triangle " + std::to_string(i) + " in " + name);
        f.read(reinterpret_cast<char*>(&attr), 2);
        if (!f) throw std::runtime_error("STL Binary: truncated attribute at " + std::to_string(i) + " in " + name);

        glm::vec3 n{data[0], data[1], data[2]};
        glm::vec3 a{data[3],  data[4],  data[5]};
        glm::vec3 b{data[6],  data[7],  data[8]};
        glm::vec3 c{data[9],  data[10], data[11]};
        mesh->vertices_.push_back({a, n});
        mesh->vertices_.push_back({b, n});
        mesh->vertices_.push_back({c, n});
    }
    mesh->triangleCount = triCount;
    mesh->computeBBox();
    return mesh;
}

std::unique_ptr<Mesh> loadSTL(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open STL: " + wideToUtf8(path.wstring()));
    std::string name = getFileName(path);

    // 探测 ASCII / Binary
    char header[6] = {0};
    f.read(header, 5);
    if (std::memcmp(header, "solid", 5) == 0) {
        // 可能是 ASCII。但很多 Binary 文件也以 "solid" 开头
        // 进一步检查:跳到 token "facet" 之前是否还有非 ASCII 字符
        f.seekg(0, std::ios::end);
        std::streampos end = f.tellg();
        std::streampos dataSize = end - std::streampos(84);

        // 跳到第 80 字节处尝试读 triCount
        f.seekg(80, std::ios::beg);
        std::uint32_t triCount = 0;
        f.read(reinterpret_cast<char*>(&triCount), 4);
        std::streampos expected = std::streampos(84) + std::streampos(static_cast<std::int64_t>(triCount) * 50);
        if (end == expected) {
            // 几乎肯定是 Binary
            f.seekg(80, std::ios::beg);
            return loadSTL_Binary(f, name, triCount);
        }
        // 当 ASCII 处理
        f.clear();
        f.seekg(0, std::ios::beg);
        return loadSTL_ASCII(f, name);
    }
    // 直接 Binary
    f.seekg(80, std::ios::beg);
    std::uint32_t triCount = 0;
    f.read(reinterpret_cast<char*>(&triCount), 4);
    f.seekg(80, std::ios::beg);
    return loadSTL_Binary(f, name, triCount);
}

} // namespace prism
