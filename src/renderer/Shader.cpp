// src/renderer/Shader.cpp
#include "Shader.h"
#include "../utils/FileUtils.h"
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <stdexcept>

#ifdef PRISM_OHOS
#include "shaders_embedded.h"   // CMake 生成: 嵌入手册着色器源码表
#endif

namespace prism {

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    compileFromFiles(vertPath, fragPath);
}

GLint Shader::uniformLocation(const char* name) const noexcept {
    return glGetUniformLocation(program_.get(), name);
}

#ifdef PRISM_OHOS
// ---------------------------------------------------------------------------
// 鸿蒙: 应用沙箱内没有稳定的资源目录, 着色器源码在编译期嵌入 libentry.so。
// 同一份 GLSL 源码需从 3.30 core 转为 GLSL ES 3.00: 改版本号 + 补精度限定符。
// ---------------------------------------------------------------------------
static std::string toGlesSource(const std::string& src, bool isFragment) {
    std::string out = src;
    static const std::string kFrom = "#version 330 core";
    const std::size_t pos = out.find(kFrom);
    if (pos == std::string::npos) return out;   // 已是 ES 版本或自定义
    out.replace(pos, kFrom.size(), "#version 300 es");
    if (isFragment) {
        const std::size_t nl = out.find('\n', pos);
        if (nl != std::string::npos) {
            out.insert(nl + 1, "precision highp float;\nprecision highp int;\n");
        }
    }
    return out;
}

static std::string baseName(const std::string& path) {
    const std::size_t p = path.find_last_of("/\\");
    return (p == std::string::npos) ? path : path.substr(p + 1);
}

static std::string slurp(const std::string& path) {
    const std::string name = baseName(path);
    const std::string src = embeddedShaderByName(name);
    if (src.empty()) {
        throw std::runtime_error("Embedded shader not found: " + name);
    }
    const bool isFragment =
        name.size() >= 5 && name.compare(name.size() - 5, 5, ".frag") == 0;
    return toGlesSource(src, isFragment);
}
#else
static std::string slurp(const std::string& path) {
    return readFileText(path);
}
#endif

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
    glDeleteShader(vs);
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
