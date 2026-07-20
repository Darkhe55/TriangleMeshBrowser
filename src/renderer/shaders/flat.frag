#version 330 core

in vec3 vNormal;

uniform vec3 uColor;
uniform vec3 uLightDir;
uniform float uAmbient;
uniform float uDiffuse;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float diff = max(dot(N, L), 0.0);
    vec3 color = uColor * (uAmbient + diff * uDiffuse);
    FragColor = vec4(color, 1.0);
}
