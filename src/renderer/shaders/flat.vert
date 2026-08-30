#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in vec3 aColor;   // 点云顶点色 (无颜色属性时未启用,值无效)

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform float uEdgeExpand;   // 沿法线外扩距离 (PMX 边缘反壳绘制用)

out vec3 vNormal;
out vec3 vColor;

void main() {
    vNormal = uNormalMatrix * aNormal;
    vColor = aColor;
    vec3 pos = aPos + aNormal * uEdgeExpand;
    gl_Position = uProjection * uView * uModel * vec4(pos, 1.0);
}
