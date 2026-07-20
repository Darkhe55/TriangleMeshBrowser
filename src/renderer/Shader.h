// src/renderer/Shader.h
// Shader 编译/链接封装
#pragma once

#include "GLResources.h"
#include <glm/glm.hpp>
#include <string>

namespace prism {

class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertPath, const std::string& fragPath);

    // 不可拷贝 - GL 资源 ID
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept            = default;
    Shader& operator=(Shader&& o) noexcept = default;

    void bind() const noexcept { glUseProgram(program_.get()); }

    // uniform 设置
    void setMat4 (const char* name, const glm::mat4& m) const noexcept;
    void setMat3 (const char* name, const glm::mat3& m) const noexcept;
    void setVec3 (const char* name, const glm::vec3& v) const noexcept;
    void setFloat(const char* name, float f)                const noexcept;
    void setUInt (const char* name, GLuint u)               const noexcept;
    void setInt  (const char* name, int i)                  const noexcept;

    GLuint id() const noexcept { return program_.get(); }
    bool  valid() const noexcept { return program_ != 0; }

private:
    ShaderPtr program_{};
    GLint     locMat4_  = -1;
    GLint     locMat3_  = -1;
    GLint     locVec3_  = -1;
    GLint     locFloat_ = -1;
    GLint     locUInt_  = -1;
    GLint     locInt_   = -1;

    GLint uniformLocation(const char* name) const noexcept;
    void   compileFromFiles(const std::string& vert, const std::string& frag);
};

} // namespace prism
