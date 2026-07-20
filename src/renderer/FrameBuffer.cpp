// src/renderer/FrameBuffer.cpp
#include "FrameBuffer.h"
#include <vector>
#include <cstdint>
#include <cstring>

namespace prism {

FrameBuffer::FrameBuffer(int w, int h) : width_(w), height_(h) { create(); }

void FrameBuffer::resize(int w, int h) {
    if (w == width_ && h == height_ && valid()) return;
    width_ = w; height_ = h;
    create();
}

void FrameBuffer::create() {
    fbo_.reset(); tex_.reset(); rbo_.reset();
    if (width_ <= 0 || height_ <= 0) return;

    GLuint f = 0, t = 0, r = 0;
    glGenFramebuffers(1, &f);
    glGenTextures(1, &t);
    glGenRenderbuffers(1, &r);
    fbo_.reset(f);
    tex_.reset(t);
    rbo_.reset(r);

    // color tex
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // depth rbo
    glBindRenderbuffer(GL_RENDERBUFFER, r);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);

    // fbo attach
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        // 销毁失败的资源
        fbo_.reset();
        tex_.reset();
        rbo_.reset();
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::bind()   const noexcept { glBindFramebuffer(GL_FRAMEBUFFER, fbo_.get()); }
void FrameBuffer::unbind() const noexcept { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

bool FrameBuffer::readPixels(std::vector<std::uint8_t>& out) const {
    if (!valid()) return false;
    out.assign(static_cast<size_t>(width_) * height_ * 4, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_.get());
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

} // namespace prism
