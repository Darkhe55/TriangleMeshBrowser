#version 330 core

in vec3 vNormal;
in vec3 vColor;

uniform vec3 uColor;
uniform vec3 uLightDir;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uUseVertexColor;   // 1 = 用点云顶点色替代 uColor,0 = 用 uColor

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float diff = max(dot(N, L), 0.0);
    vec3 base = mix(uColor, vColor, uUseVertexColor);
    vec3 color = base * (uAmbient + diff * uDiffuse);
    FragColor = vec4(color, 1.0);
}
