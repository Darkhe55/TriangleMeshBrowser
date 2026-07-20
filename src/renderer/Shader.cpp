// src/renderer/Shader.cpp
#include "Shader.h"
#include "../utils/FileUtils.h"
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <stdexcept>

namespace prism {

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    compileFromFiles(vertPath, fragPath);
}

GLint Shader::uniformLocation(const char* name) const noexcept {
    return glGetUniformLocation(program_.get(), name);
}

static std::string slurp(const std::string& path) {
    return readFileText(path);  // FileUtils 提供
}

static GLuint compileShader(GLenum type, const std::string& src, const std::string& tag) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        glGetShaderInfoLog(s, len, nullptr, log.data());
        glDeleteShader(s);
        throw std::runtime_error("Shader compile failed (" + tag + "):\n" + log);
    }
    return s;
}

void Shader::compileFromFiles(const std::string& vert, const std::string& frag) {
    std::string vsSrc = slurp(vert);
    std::string fsSrc = slurp(frag);
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc, vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc, frag);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);   // linked, no longer needed
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        glDeleteProgram(prog);
        throw std::runtime_error("Shader link failed:\n" + log);
    }
    program_.reset(prog);
    // warm up location cache
    locMat4_ = uniformLocation("uProjection");
    locMat3_ = uniformLocation("uNormalMatrix");
    locVec3_ = uniformLocation("uBaseColor");
    locFloat_ = uniformLocation("uAmbient");
    locUInt_ = uniformLocation("uHighlightFace");
    locInt_ = uniformLocation("uMode");
}

void Shader::setMat4(const char* name, const glm::mat4& m) const noexcept {
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(m));
}
void Shader::setMat3(const char* name, const glm::mat3& m) const noexcept {
    glUniformMatrix3fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(m));
}
void Shader::setVec3(const char* name, const glm::vec3& v) const noexcept {
    glUniform3f(uniformLocation(name), v.x, v.y, v.z);
}
void Shader::setFloat(const char* name, float f) const noexcept {
    glUniform1f(uniformLocation(name), f);
}
void Shader::setUInt(const char* name, GLuint u) const noexcept {
    glUniform1ui(uniformLocation(name), u);
}
void Shader::setInt(const char* name, int i) const noexcept {
    glUniform1i(uniformLocation(name), i);
}

} // namespace prism
