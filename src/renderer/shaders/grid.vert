#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;
out float vViewDist;

void main() {
    vColor = aColor;
    vec4 viewPos = uView * uModel * vec4(aPos, 1.0);
    vViewDist = -viewPos.z;  // camera looks down -Z
    gl_Position = uProjection * viewPos;
}
