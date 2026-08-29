// src/renderer/FrameBuffer.h
// 离屏 FBO 用于截图
#pragma once

#include "GLResources.h"
#include <vector>
#include <cstdint>

namespace prism {

class FrameBuffer {
public:
    FrameBuffer() = default;
    FrameBuffer(int width, int height);

    void resize(int w, int h);
    void bind()   const noexcept;
    void unbind() const noexcept;

    GLuint colorTex() const noexcept { return tex_.get(); }
    int width()  const noexcept { return width_; }
    int height() const noexcept { return height_; }

    bool valid() const noexcept { return fbo_ != 0 && tex_ != 0; }

    // 读出 RGBA 像素
    bool readPixels(std::vector<std::uint8_t>& out) const;

private:
    int width_ = 0, height_ = 0;
    FboPtr    fbo_{};
    TexturePtr tex_{};
    RboPtr    rbo_{};
    void create();
};

} // namespace prism
