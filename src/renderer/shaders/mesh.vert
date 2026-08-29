#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vFragPos;
out vec3 vNormal;
out vec3 vViewNormal;  // 视空间法线 (Sphere 贴图用)
out vec2 vUv;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    vNormal  = uNormalMatrix * aNormal;
    vViewNormal = mat3(uView) * vNormal;
    vUv = aUv;
    gl_Position = uProjection * uView * worldPos;
}
