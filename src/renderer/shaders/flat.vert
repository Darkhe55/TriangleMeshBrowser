#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform float uEdgeExpand;   // 沿法线外扩距离 (PMX 边缘反壳绘制用)

out vec3 vNormal;

void main() {
    vNormal = uNormalMatrix * aNormal;
    vec3 pos = aPos + aNormal * uEdgeExpand;
    gl_Position = uProjection * uView * uModel * vec4(pos, 1.0);
}
