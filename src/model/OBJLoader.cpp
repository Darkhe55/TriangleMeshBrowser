// src/model/OBJLoader.cpp
#include "OBJLoader.h"
#include "../utils/FileUtils.h"
#include <glm/glm.hpp>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <cstring>

namespace prism {

// 用 hash 合并 (pos,normal) -> 顶点索引
struct VertKey {
    glm::vec3 p;
    glm::vec3 n;
    bool operator==(const VertKey& o) const noexcept { return p == o.p && n == o.n; }
};

struct VertKeyHash {
    size_t operator()(const VertKey& k) const noexcept {
        size_t h = 0;
        auto mix = [&h](float f) {
            size_t bits;
            std::memcpy(&bits, &f, sizeof(bits));
            h ^= bits + 0x9e3779b9 + (h << 6) + (h >> 2);
        };
        mix(k.p.x); mix(k.p.y); mix(k.p.z);
        mix(k.n.x); mix(k.n.y); mix(k.n.z);
        return h;
    }
};

std::unique_ptr<Mesh> loadOBJ(const std::filesystem::path& path) {
    std::string text = readFileText(path);
    auto mesh = std::make_unique<Mesh>();
    mesh->name = getFileName(path);
    mesh->indexed_ = true;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    positions.reserve(1 << 14);
    normals.reserve(1 << 14);

    std::unordered_map<VertKey, std::uint32_t, VertKeyHash> cache;
    cache.reserve(1 << 14);
    mesh->vertices_.reserve(1 << 14);
    mesh->indices_.reserve(1 << 14);

    auto indexOf = [&](glm::vec3 p, glm::vec3 n) -> std::uint32_t {
        VertKey k{p, n};
        auto it = cache.find(k);
        if (it != cache.end()) return it->second;
        std::uint32_t idx = static_cast<std::uint32_t>(mesh->vertices_.size());
        mesh->vertices_.push_back({p, n});
        cache.emplace(k, idx);
        return idx;
    };

    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == 'v' && line.size() > 1 && (line[1] == ' ' || line[1] == '\t')) {
            glm::vec3 p;
            std::istringstream ls(line.data() + 2);
            ls >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (line.compare(0, 2, "vn") == 0 && (line[2] == ' ' || line[2] == '\t')) {
            glm::vec3 n;
            std::istringstream ls(line.data() + 3);
            ls >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (line[0] == 'f' && (line[1] == ' ' || line[1] == '\t')) {
            // 解析每个 "v/vt/vn" 或 "v//vn" 或 "v"
            std::istringstream ls(line.data() + 2);
            std::vector<std::uint32_t> faceVerts;
            faceVerts.reserve(8);
            std::string tok;
            while (ls >> tok) {
                int vi = 0, vni = 0;
                // 找 '/'
                auto p1 = tok.find('/');
                if (p1 == std::string::npos) {
                    vi = std::stoi(tok);
                } else {
                    vi = std::stoi(tok.substr(0, p1));
                    auto p2 = tok.find('/', p1 + 1);
                    if (p2 == std::string::npos) {
                        // "v/vt"
                    } else if (p2 == p1 + 1) {
                        // "v//vn"
                        if (p2 + 1 < tok.size())
                            vni = std::stoi(tok.substr(p2 + 1));
                    } else {
                        if (p1 + 1 < p2) {/* vt 忽略 */}
                        if (p2 + 1 < tok.size())
                            vni = std::stoi(tok.substr(p2 + 1));
                    }
                }
                // OBJ 是 1-based, 支持负索引
                if (vi < 0) vi = static_cast<int>(positions.size()) + vi + 1;
                if (vni < 0) vni = static_cast<int>(normals.size()) + vni + 1;
                if (vi <= 0 || static_cast<size_t>(vi - 1) >= positions.size()) continue;

                glm::vec3 pos = positions[static_cast<size_t>(vi - 1)];
                glm::vec3 nrm(0.f, 1.f, 0.f);
                if (vni > 0 && static_cast<size_t>(vni - 1) < normals.size()) {
                    nrm = normals[static_cast<size_t>(vni - 1)];
                } else {
                    // 占位,稍后 computeNormals
                    nrm = glm::vec3(0.f);
                }
                faceVerts.push_back(indexOf(pos, nrm));
            }
            // 三角扇形
            for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
                mesh->indices_.push_back(faceVerts[0]);
                mesh->indices_.push_back(faceVerts[i]);
                mesh->indices_.push_back(faceVerts[i + 1]);
            }
        }
    }

    if (mesh->vertices_.empty() || mesh->indices_.empty()) {
        throw std::runtime_error("OBJ has no triangles: " + wideToUtf8(path.wstring()));
    }
    mesh->triangleCount = static_cast<std::uint32_t>(mesh->indices_.size() / 3);

    // 存在零法线 → 重新计算
    bool needCompute = false;
    for (const auto& v : mesh->vertices_) {
        if (glm::length(v.normal) < 1e-6f) { needCompute = true; break; }
    }
    if (needCompute) mesh->computeNormals();

    mesh->computeBBox();
    return mesh;
}

} // namespace prism
