// src/ui/Picker.cpp
#include "Picker.h"
#include <glm/glm.hpp>
#include <limits>
#include <optional>

namespace prism {

// Möller-Trumbore ray-triangle intersection
static float rayTri(const glm::vec3& o, const glm::vec3& d,
                    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    const float EPS = 1e-7f;
    glm::vec3 e1 = b - a;
    glm::vec3 e2 = c - a;
    glm::vec3 h = glm::cross(d, e2);
    float det = glm::dot(e1, h);
    if (std::fabs(det) < EPS) return -1.f;
    float invDet = 1.f / det;
    glm::vec3 s = o - a;
    float u = invDet * glm::dot(s, h);
    if (u < 0.f || u > 1.f) return -1.f;
    glm::vec3 q = glm::cross(s, e1);
    float v = invDet * glm::dot(d, q);
    if (v < 0.f || u + v > 1.f) return -1.f;
    float t = invDet * glm::dot(e2, q);
    return t > 0.f ? t : -1.f;
}

std::optional<PickHit> Picker::pick(const OrbitCamera& cam,
                                    const Mesh& mesh,
                                    double sx, double sy) {
    glm::vec3 ro, rd;
    cam.screenRay(sx, sy, ro, rd);

    float bestT = std::numeric_limits<float>::infinity();
    std::uint32_t bestF = 0;
    bool found = false;

    if (mesh.indexed_) {
        const auto& V = mesh.vertices_;
        const auto& I = mesh.indices_;
        for (std::uint32_t f = 0; f < mesh.triangleCount; ++f) {
            std::uint32_t i0 = I[3 * f + 0];
            std::uint32_t i1 = I[3 * f + 1];
            std::uint32_t i2 = I[3 * f + 2];
            if (i0 >= V.size() || i1 >= V.size() || i2 >= V.size()) continue;
            float t = rayTri(ro, rd, V[i0].position, V[i1].position, V[i2].position);
            if (t > 0.f && t < bestT) { bestT = t; bestF = f; found = true; }
        }
    } else {
        const auto& V = mesh.vertices_;
        for (std::uint32_t f = 0; f < mesh.triangleCount; ++f) {
            const auto& a = V[3 * f + 0].position;
            const auto& b = V[3 * f + 1].position;
            const auto& c = V[3 * f + 2].position;
            float t = rayTri(ro, rd, a, b, c);
            if (t > 0.f && t < bestT) { bestT = t; bestF = f; found = true; }
        }
    }
    if (!found) return std::nullopt;
    return PickHit{bestF, bestT};
}

} // namespace prism
