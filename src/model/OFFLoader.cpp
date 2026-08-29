// src/model/OFFLoader.cpp
#include "OFFLoader.h"
#include "../utils/FileUtils.h"
#include <glm/glm.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>

namespace prism {

std::unique_ptr<Mesh> loadOFF(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open OFF: " + wideToUtf8(path.wstring()));

    auto mesh = std::make_unique<Mesh>();
    mesh->name = getFileName(path);
    mesh->indexed_ = true;

    std::string header;
    f >> header;
    if (header != "OFF" && header != "COFF" && header != "NOFF" && header != "CNOFF")
        throw std::runtime_error("Not an OFF file: " + wideToUtf8(path.wstring()));

    std::uint32_t vCount = 0, fCount = 0, eCount = 0;
    f >> vCount >> fCount >> eCount;

    mesh->vertices_.reserve(vCount);
    mesh->indices_.reserve(fCount * 3);

    for (std::uint32_t i = 0; i < vCount; ++i) {
        glm::vec3 p;
        f >> p.x >> p.y >> p.z;
        // COFF: 后面跟 r g b [a]; NOFF: 后面跟 normal; CNOFF: color+normal
        if (header != "OFF") {
            glm::vec3 extra{};
            f >> extra.x >> extra.y >> extra.z;
            // COFF/CNOFF: 再读颜色
            if (header == "COFF" || header == "CNOFF") {
                glm::vec3 col;
                f >> col.x >> col.y >> col.z;
            }
            // NOFF/CNOFF: extra 是法线
            if (header == "NOFF" || header == "CNOFF") {
                mesh->vertices_.push_back({p, extra});
            } else {
                mesh->vertices_.push_back({p, glm::vec3(0.f)});
            }
        } else {
            mesh->vertices_.push_back({p, glm::vec3(0.f)});
        }
    }

    for (std::uint32_t i = 0; i < fCount; ++i) {
        int n; f >> n;
        std::vector<std::uint32_t> faceVerts;
        faceVerts.reserve(8);
        for (int k = 0; k < n; ++k) {
            std::uint32_t idx; f >> idx;
            faceVerts.push_back(idx);
        }
        // 颜色 (COFF/CNOFF): face 后跟 r g b [a]
        if (header == "COFF" || header == "CNOFF") {
            glm::vec3 col;
            f >> col.x >> col.y >> col.z;
        }
        for (size_t k = 1; k + 1 < faceVerts.size(); ++k) {
            mesh->indices_.push_back(faceVerts[0]);
            mesh->indices_.push_back(faceVerts[k]);
            mesh->indices_.push_back(faceVerts[k + 1]);
        }
    }

    if (mesh->vertices_.empty() || mesh->indices_.empty())
        throw std::runtime_error("OFF empty: " + wideToUtf8(path.wstring()));
    mesh->triangleCount = static_cast<std::uint32_t>(mesh->indices_.size() / 3);

    bool needCompute = false;
    for (const auto& v : mesh->vertices_) {
        if (glm::length(v.normal) < 1e-6f) { needCompute = true; break; }
    }
    if (needCompute) mesh->computeNormals();

    mesh->computeBBox();
    return mesh;
}

} // namespace prism
