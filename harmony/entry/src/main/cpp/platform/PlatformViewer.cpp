// browser/harmony/entry/src/main/cpp/platform/PlatformViewer.cpp
#include "PlatformViewer.h"
#include "InputState.h"

#include <GLES3/gl3.h>
#include <hilog/log.h>

#include <chrono>
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "PrismViewer"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO,  0x0000, LOG_TAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0x0000, LOG_TAG, __VA_ARGS__)

namespace prism {

namespace {

const char* kVertSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kFragSrc = R"(#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {0};
        glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
        LOGE("shader compile failed: %{public}s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

PlatformViewer::~PlatformViewer() {
    shutdown();
}

bool PlatformViewer::init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    GLuint vs = compile(GL_VERTEX_SHADER, kVertSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFragSrc);
    if (vs == 0 || fs == 0) return false;

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512] = {0};
        glGetProgramInfoLog(program_, sizeof(log) - 1, nullptr, log);
        LOGE("program link failed: %{public}s", log);
        return false;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    uMvpLoc_ = glGetUniformLocation(program_, "uMvp");

    const float verts[] = {
         0.0f,  0.6f, 0.0f,  1.0f, 0.35f, 0.35f,
        -0.5f, -0.4f, 0.0f,  0.35f, 1.0f, 0.35f,
         0.5f, -0.4f, 0.0f,  0.35f, 0.35f, 1.0f,
    };
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);

    lastTime_ = 0.f;
    ready_ = true;
    LOGI("PlatformViewer init ok, GL_VERSION=%{public}s",
         reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    return true;
}

void PlatformViewer::applyInput(InputState& input, float /*dt*/) {
    // 首版: 消费事件 (后续接入 OrbitCamera / ImGui)
    input.takePointer();
    input.takeKeys();
    input.takeScroll();
}

void PlatformViewer::renderScene(int w, int h) {
    glViewport(0, 0, w, h);
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.f;
    const float c = std::cos(angle_);
    const float s = std::sin(angle_);
    const float sx = (aspect < 1.f) ? aspect : 1.f;
    const float sy = (aspect > 1.f) ? 1.f / aspect : 1.f;
    const float m[16] = {
        c * sx,  s * sy, 0.f, 0.f,
       -s * sx,  c * sy, 0.f, 0.f,
        0.f,     0.f,    1.f, 0.f,
        0.f,     0.f,    0.f, 1.f,
    };
    glUseProgram(program_);
    glUniformMatrix4fv(uMvpLoc_, 1, GL_FALSE, m);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void PlatformViewer::frame(int w, int h, InputState& input) {
    if (!ready_) return;

    const float now = static_cast<float>(
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
    float dt = (lastTime_ > 0.f) ? (now - lastTime_) : 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    lastTime_ = now;

    applyInput(input, dt);
    angle_ += dt * 0.8f;
    renderScene(w, h);
}

void PlatformViewer::shutdown() {
    if (vao_)     { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (vbo_)     { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (program_) { glDeleteProgram(program_); program_ = 0; }
    ready_ = false;
}

} // namespace prism
