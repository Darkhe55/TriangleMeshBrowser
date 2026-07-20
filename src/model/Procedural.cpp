// src/model/Procedural.cpp
#include "Procedural.h"
#include <glm/glm.hpp>
#include <cmath>
#include <vector>

namespace prism::procedural {

static std::unique_ptr<Mesh> make(const std::string& name) {
    auto m = std::make_unique<Mesh>();
    m->name = name;
    m->indexed_ = true;
    return m;
}

std::unique_ptr<Mesh> cube(float s) {
    auto m = make("Cube");
    float h = s * 0.5f;
    // 6 面 × 4 顶点 = 24 顶点(每面单独法线);Z 轴朝上,z=+h 为顶面
    glm::vec3 p[8] = {
        {-h,-h,-h}, { h,-h,-h}, { h, h,-h}, {-h, h,-h},
        {-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h}
    };
    struct Face { int v[4]; glm::vec3 n; };
    Face faces[6] = {
        {{0,3,2,1}, { 0, 0,-1}},  // bottom (z=-h)
        {{4,5,6,7}, { 0, 0, 1}},  // top    (z=+h)
        {{0,4,7,3}, {-1, 0, 0}},  // left
        {{1,2,6,5}, { 1, 0, 0}},  // right
        {{2,3,7,6}, { 0, 1, 0}},  // back  (y=+h)
        {{0,1,5,4}, { 0,-1, 0}},  // front (y=-h)
    };
    for (auto& f : faces) {
        for (int k = 0; k < 4; ++k)
            m->vertices_.push_back({p[f.v[k]], f.n});
        std::uint32_t base = static_cast<std::uint32_t>(m->vertices_.size()) - 4;
        m->indices_.push_back(base + 0);
        m->indices_.push_back(base + 1);
        m->indices_.push_back(base + 2);
        m->indices_.push_back(base + 0);
        m->indices_.push_back(base + 2);
        m->indices_.push_back(base + 3);
    }
    m->triangleCount = 12;
    m->computeBBox();
    return m;
}

std::unique_ptr<Mesh> sphere(float r, int slices, int stacks) {
    auto m = make("Sphere");
    m->vertices_.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));
    for (int j = 0; j <= stacks; ++j) {
        float v = static_cast<float>(j) / stacks;
        float phi = v * 3.14159265358979f;
        for (int i = 0; i <= slices; ++i) {
            float u = static_cast<float>(i) / slices;
            float theta = u * 6.28318530717958f;
            glm::vec3 pos{
                r * std::sin(phi) * std::cos(theta),
                r * std::sin(phi) * std::sin(theta),
                r * std::cos(phi)
            };
            m->vertices_.push_back({pos, glm::normalize(pos)});
        }
    }
    for (int j = 0; j < stacks; ++j) {
        for (int i = 0; i < slices; ++i) {
            std::uint32_t a = static_cast<std::uint32_t>(j * (slices + 1) + i);
            std::uint32_t b = a + slices + 1;
            m->indices_.push_back(a); m->indices_.push_back(b); m->indices_.push_back(a + 1);
            m->indices_.push_back(b); m->indices_.push_back(b + 1); m->indices_.push_back(a + 1);
        }
    }
    m->triangleCount = static_cast<std::uint32_t>(m->indices_.size() / 3);
    m->computeBBox();
    return m;
}

std::unique_ptr<Mesh> cylinder(float r, float h, int slices) {
    auto m = make("Cylinder");
    float halfH = h * 0.5f;
    // 侧面:两层(Z 轴为圆柱高度方向)
    for (int j = 0; j < 2; ++j) {
        float z = (j == 0) ? -halfH : halfH;
        for (int i = 0; i <= slices; ++i) {
            float theta = (float)i / slices * 6.28318530717958f;
            glm::vec3 p{r * std::cos(theta), r * std::sin(theta), z};
            glm::vec3 n = glm::normalize(glm::vec3(p.x, p.y, 0));
            m->vertices_.push_back({p, n});
        }
    }
    int ringSize = slices + 1;
    for (int i = 0; i < slices; ++i) {
        std::uint32_t a = static_cast<std::uint32_t>(i);
        std::uint32_t b = a + ringSize;
        m->indices_.push_back(a); m->indices_.push_back(b); m->indices_.push_back(a + 1);
        m->indices_.push_back(b); m->indices_.push_back(b + 1); m->indices_.push_back(a + 1);
    }
    // 顶/底 (中心扇形)
    std::uint32_t topCenterIdx = static_cast<std::uint32_t>(m->vertices_.size());
    m->vertices_.push_back({{0, 0, halfH}, {0, 0, 1}});
    std::uint32_t topStart = static_cast<std::uint32_t>(ringSize);  // 上层起点
    for (int i = 0; i < slices; ++i) {
        m->indices_.push_back(topCenterIdx);
        m->indices_.push_back(topStart + i);
        m->indices_.push_back(topStart + i + 1);
    }
    std::uint32_t botCenterIdx = static_cast<std::uint32_t>(m->vertices_.size());
    m->vertices_.push_back({{0, 0, -halfH}, {0, 0, -1}});
    for (int i = 0; i < slices; ++i) {
        m->indices_.push_back(botCenterIdx);
        m->indices_.push_back(i + 1);
        m->indices_.push_back(i);
    }
    m->triangleCount = static_cast<std::uint32_t>(m->indices_.size() / 3);
    m->computeBBox();
    return m;
}

std::unique_ptr<Mesh> torus(float R, float r, int major, int minor) {
    auto m = make("Torus");
    for (int j = 0; j <= major; ++j) {
        for (int i = 0; i <= minor; ++i) {
            float u = (float)i / minor * 6.28318530717958f;
            float v = (float)j / major * 6.28318530717958f;
            float cx = (R + r * std::cos(u)) * std::cos(v);
            float cy = (R + r * std::cos(u)) * std::sin(v);
            float cz = r * std::sin(u);
            glm::vec3 p{cx, cy, cz};
            glm::vec3 n = glm::normalize(glm::vec3(std::cos(u) * std::cos(v),
                                                    std::cos(u) * std::sin(v),
                                                    std::sin(u)));
            m->vertices_.push_back({p, n});
        }
    }
    int stride = minor + 1;
    for (int j = 0; j < major; ++j) {
        for (int i = 0; i < minor; ++i) {
            std::uint32_t a = static_cast<std::uint32_t>(j * stride + i);
            std::uint32_t b = a + stride;
            m->indices_.push_back(a); m->indices_.push_back(b); m->indices_.push_back(a + 1);
            m->indices_.push_back(b); m->indices_.push_back(b + 1); m->indices_.push_back(a + 1);
        }
    }
    m->triangleCount = static_cast<std::uint32_t>(m->indices_.size() / 3);
    m->computeBBox();
    return m;
}

std::unique_ptr<Mesh> cone(float r, float h, int slices) {
    auto m = make("Cone");
    float halfH = h * 0.5f;
    glm::vec3 apex{0, 0, halfH};
    // 侧面:每 slice 一个三角形 (apex, base_i, base_{i+1})
    for (int i = 0; i < slices; ++i) {
        float t1 = (float)i / slices * 6.28318530717958f;
        float t2 = (float)(i + 1) / slices * 6.28318530717958f;
        glm::vec3 b1{r * std::cos(t1), r * std::sin(t1), -halfH};
        glm::vec3 b2{r * std::cos(t2), r * std::sin(t2), -halfH};
        glm::vec3 n = glm::normalize(glm::cross(b1 - apex, b2 - apex));
        m->vertices_.push_back({apex, n});
        m->vertices_.push_back({b1,   n});
        m->vertices_.push_back({b2,   n});
    }
    // 底面中心
    glm::vec3 botCenter{0, 0, -halfH};
    m->vertices_.push_back({botCenter, {0, 0, -1}});
    std::uint32_t cIdx = static_cast<std::uint32_t>(m->vertices_.size() - 1);
    for (int i = 0; i < slices; ++i) {
        std::uint32_t a = static_cast<std::uint32_t>(i * 3 + 1);
        std::uint32_t b = static_cast<std::uint32_t>(i * 3 + 2);
        m->indices_.push_back(cIdx); m->indices_.push_back(b); m->indices_.push_back(a);
    }
    // 侧面三角形索引
    for (int i = 0; i < slices; ++i) {
        std::uint32_t a = static_cast<std::uint32_t>(i * 3);
        m->indices_.push_back(a); m->indices_.push_back(a + 1); m->indices_.push_back(a + 2);
    }
    m->triangleCount = static_cast<std::uint32_t>(m->indices_.size() / 3);
    m->computeBBox();
    return m;
}

const char* const* allNames() noexcept {
    static const char* names[] = {"Cube", "Sphere", "Cylinder", "Torus", "Cone"};
    return names;
}
int allCount() noexcept { return 5; }

} // namespace prism::procedural
