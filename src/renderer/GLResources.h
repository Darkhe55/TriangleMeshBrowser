// src/renderer/GLResources.h
// OpenGL 资源 RAII 包装
#pragma once

#include <GL/glew.h>
#include <utility>
#include <stdexcept>
#include <string>

namespace prism {

template <typename Deleter>
class GLHandle {
    GLuint id_ = 0;
public:
    GLHandle() = default;
    explicit GLHandle(GLuint id) noexcept : id_(id) {}

    ~GLHandle() { if (id_) Deleter{}(id_); }

    GLHandle(const GLHandle&) = delete;
    GLHandle& operator=(const GLHandle&) = delete;

    GLHandle(GLHandle&& o) noexcept : id_(o.id_) { o.id_ = 0; }
    GLHandle& operator=(GLHandle&& o) noexcept {
        if (this != &o) {
            if (id_) Deleter{}(id_);
            id_ = o.id_;
            o.id_ = 0;
        }
        return *this;
    }

    GLuint get() const noexcept { return id_; }
    explicit operator bool() const noexcept { return id_ != 0; }

    // 与裸 GLuint 比较
    bool operator==(GLuint v) const noexcept { return id_ == v; }
    bool operator!=(GLuint v) const noexcept { return id_ != v; }

    void reset(GLuint id = 0) noexcept {
        if (id_ != id) {
            if (id_) Deleter{}(id_);
            id_ = id;
        }
    }
    void swap(GLHandle& o) noexcept { std::swap(id_, o.id_); }
};

// ---------- Deleter ----------
struct ShaderDeleter  { void operator()(GLuint p) const noexcept { if (p) glDeleteProgram(p); } };
struct TextureDeleter { void operator()(GLuint t) const noexcept { if (t) glDeleteTextures(1, &t); } };
struct VaoDeleter     { void operator()(GLuint v) const noexcept { if (v) glDeleteVertexArrays(1, &v); } };
struct BufferDeleter  { void operator()(GLuint b) const noexcept { if (b) glDeleteBuffers(1, &b); } };
struct RboDeleter     { void operator()(GLuint r) const noexcept { if (r) glDeleteRenderbuffers(1, &r); } };
struct FboDeleter     { void operator()(GLuint f) const noexcept { if (f) glDeleteFramebuffers(1, &f); } };

// ---------- Typed handles ----------
using ShaderPtr  = GLHandle<ShaderDeleter>;
using TexturePtr = GLHandle<TextureDeleter>;
using VaoPtr     = GLHandle<VaoDeleter>;
using BufferPtr  = GLHandle<BufferDeleter>;
using RboPtr     = GLHandle<RboDeleter>;
using FboPtr     = GLHandle<FboDeleter>;

// ---------- OpenGL 错误检查 ----------
inline void checkGLError(const char* tag) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        throw std::runtime_error(std::string("OpenGL error at ") + tag +
                                 ": 0x" + std::to_string(static_cast<unsigned>(err)));
    }
}

} // namespace prism
