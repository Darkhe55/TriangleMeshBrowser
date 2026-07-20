// src/renderer/Grid.cpp
#include "Grid.h"
#include <vector>
#include <glm/glm.hpp>

namespace prism {

Grid::Grid(float halfExtent, float step)
    : halfExtent_(halfExtent), step_(step) {}

void Grid::clear() noexcept {
    vao_.reset();
    vbo_.reset();
    vertexCount_ = 0;
    uploaded_ = false;
}

void Grid::upload() {
    clear();
    if (halfExtent_ <= 0.f || step_ <= 0.f) return;

    struct V { glm::vec3 pos; glm::vec3 col; };
    std::vector<V> verts;
    verts.reserve(static_cast<size_t>((2 * halfExtent_ / step_ + 1) * 4) + 4);

    const glm::vec3 minorCol(0.30f, 0.30f, 0.36f);  // 副线
    const glm::vec3 majorCol(0.55f, 0.55f, 0.65f);  // 主线(X=0 / Z=0)

    // 网格画在 xOy 平面 (Z=0, 即地面);Z 轴朝上。
    // 平行 X 轴的线 (y = const),Z=0
    float y = -halfExtent_;
    while (y <= halfExtent_ + 1e-3f) {
        glm::vec3 c = (std::fabs(y) < step_ * 0.5f) ? majorCol : minorCol;
        verts.push_back({{-halfExtent_, y, 0.f}, c});
        verts.push_back({{ halfExtent_, y, 0.f}, c});
        y += step_;
    }
    // 平行 Y 轴的线 (x = const),Z=0
    float x = -halfExtent_;
    while (x <= halfExtent_ + 1e-3f) {
        glm::vec3 c = (std::fabs(x) < step_ * 0.5f) ? majorCol : minorCol;
        verts.push_back({{x, -halfExtent_, 0.f}, c});
        verts.push_back({{x,  halfExtent_, 0.f}, c});
        x += step_;
    }

    vertexCount_ = static_cast<std::uint32_t>(verts.size());

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    vao_.reset(vao);
    vbo_.reset(vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(V)),
                 verts.data(), GL_STATIC_DRAW);
    // layout 0 = pos, layout 1 = color
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, col)));
    glBindVertexArray(0);
    uploaded_ = true;
}

void Grid::draw() const {
    if (!uploaded_ || vertexCount_ == 0) return;
    glBindVertexArray(vao_.get());
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertexCount_));
}

} // namespace prism
