// src/model/PLYLoader.cpp
#include "PLYLoader.h"
#include "../utils/FileUtils.h"
#include <glm/glm.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <stdexcept>

namespace prism {

struct PlyProperty {
    std::string name;
    std::string type;  // float / double / int / uchar
    int offset = 0;    // 相对顶点起始的字节偏移
    int size   = 0;    // 字节
    int list   = 0;    // 0=标量;否则是 list 的 count 字段属性索引
};

static int typeSize(const std::string& t) {
    if (t == "float" || t == "float32" || t == "int"  || t == "int32"
     || t == "uint" || t == "uint32")                                   return 4;
    if (t == "double"|| t == "float64")                                return 8;
    if (t == "uchar" || t == "uint8"  || t == "char"|| t == "int8")      return 1;
    if (t == "ushort"|| t == "uint16" || t == "short"|| t == "int16")    return 2;
    return 4;
}

std::unique_ptr<Mesh> loadPLY(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open PLY: " + wideToUtf8(path.wstring()));
    std::string name = getFileName(path);

    // --- header ---
    std::string line, format = "ascii";
    std::uint32_t vCount = 0, fCount = 0;
    std::vector<PlyProperty> vProps;
    std::vector<PlyProperty> fProps;
    std::string currentElement;

    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "end_header") break;
        std::istringstream ls(line);
        std::string tok; ls >> tok;
        if (tok == "format") {
            std::string v; ls >> v;
            format = v;  // ascii / binary_little_endian / binary_big_endian
        } else if (tok == "element") {
            std::string e; std::uint32_t c;
            ls >> e >> c;
            currentElement = e;
            if (e == "vertex") vCount = c;
            else if (e == "face") fCount = c;
        } else if (tok == "property") {
            std::string typeOrList, name2;
            ls >> typeOrList >> name2;
            PlyProperty p;
            p.name = name2;
            if (typeOrList == "list") {
                std::string countType, itemType;
                ls >> countType >> itemType;
                p.list = 1;  // 简化:假定 count 是 uchar
                p.type = itemType;
                p.size = typeSize(itemType);
            } else {
                p.type = typeOrList;
                p.size = typeSize(typeOrList);
            }
            if (currentElement == "vertex") {
                p.offset = 0;  // 重新计算
                vProps.push_back(p);
            } else if (currentElement == "face") {
                fProps.push_back(p);
            }
        }
    }

    if (vCount == 0) throw std::runtime_error("PLY: no vertices: " + wideToUtf8(path.wstring()));

    // 算 vertex 属性偏移
    int off = 0;
    for (auto& p : vProps) {
        p.offset = off;
        off += p.size;
    }
    int vStride = off;

    auto findProp = [](const std::vector<PlyProperty>& ps, const std::string& n) -> int {
        for (size_t i = 0; i < ps.size(); ++i) if (ps[i].name == n) return static_cast<int>(i);
        return -1;
    };
    int piX = findProp(vProps, "x");
    int piY = findProp(vProps, "y");
    int piZ = findProp(vProps, "z");
    int piNX = findProp(vProps, "nx");
    int piNY = findProp(vProps, "ny");
    int piNZ = findProp(vProps, "nz");
    if (piX < 0 || piY < 0 || piZ < 0)
        throw std::runtime_error("PLY: missing x/y/z: " + wideToUtf8(path.wstring()));

    auto mesh = std::make_unique<Mesh>();
    mesh->name = name;
    mesh->indexed_ = true;
    mesh->vertices_.reserve(vCount);
    mesh->indices_.reserve(fCount * 3);
    bool hasNormals = (piNX >= 0 && piNY >= 0 && piNZ >= 0);

    if (format == "ascii") {
        // vertices
        for (std::uint32_t i = 0; i < vCount; ++i) {
            std::getline(f, line);
            std::istringstream ls(line);
            std::vector<double> vals(vProps.size());
            for (size_t k = 0; k < vProps.size(); ++k) ls >> vals[k];
            VertexPN v;
            v.position = {static_cast<float>(vals[piX]),
                          static_cast<float>(vals[piY]),
                          static_cast<float>(vals[piZ])};
            v.normal = hasNormals
                ? glm::vec3(static_cast<float>(vals[piNX]),
                            static_cast<float>(vals[piNY]),
                            static_cast<float>(vals[piNZ]))
                : glm::vec3(0.f);
            mesh->vertices_.push_back(v);
        }
        // faces
        for (std::uint32_t i = 0; i < fCount; ++i) {
            std::getline(f, line);
            std::istringstream ls(line);
            int n; ls >> n;
            for (int k = 0; k < n; ++k) {
                std::uint32_t idx; ls >> idx;
                mesh->indices_.push_back(idx);
            }
        }
    } else if (format == "binary_little_endian") {
        std::vector<std::uint8_t> buf(vStride);
        for (std::uint32_t i = 0; i < vCount; ++i) {
            f.read(reinterpret_cast<char*>(buf.data()), vStride);
            auto get = [&](int idx) -> float {
                const auto& p = vProps[idx];
                if (p.type == "float" || p.type == "float32") {
                    float v; std::memcpy(&v, buf.data() + p.offset, 4); return v;
                }
                if (p.type == "double" || p.type == "float64") {
                    double v; std::memcpy(&v, buf.data() + p.offset, 8); return static_cast<float>(v);
                }
                if (p.type == "int" || p.type == "int32") {
                    int32_t v; std::memcpy(&v, buf.data() + p.offset, 4); return static_cast<float>(v);
                }
                if (p.type == "uchar" || p.type == "uint8") {
                    uint8_t v = buf[p.offset]; return static_cast<float>(v);
                }
                if (p.type == "short" || p.type == "int16") {
                    int16_t v; std::memcpy(&v, buf.data() + p.offset, 2); return static_cast<float>(v);
                }
                return 0.f;
            };
            VertexPN v;
            v.position = {get(piX), get(piY), get(piZ)};
            v.normal = hasNormals
                ? glm::vec3(get(piNX), get(piNY), get(piNZ))
                : glm::vec3(0.f);
            mesh->vertices_.push_back(v);
        }
        // faces — 简化:vertex_indices 是 list, count 假定是 uchar
        for (std::uint32_t i = 0; i < fCount; ++i) {
            std::uint8_t n = 0;
            f.read(reinterpret_cast<char*>(&n), 1);
            for (int k = 0; k < n; ++k) {
                std::uint32_t idx;
                f.read(reinterpret_cast<char*>(&idx), 4);
                mesh->indices_.push_back(idx);
            }
        }
    } else {
        throw std::runtime_error("PLY: unsupported format: " + format);
    }

    if (mesh->indices_.empty())
        throw std::runtime_error("PLY: no faces: " + wideToUtf8(path.wstring()));
    mesh->triangleCount = static_cast<std::uint32_t>(mesh->indices_.size() / 3);
    if (!hasNormals) mesh->computeNormals();
    mesh->computeBBox();
    return mesh;
}

} // namespace prism
