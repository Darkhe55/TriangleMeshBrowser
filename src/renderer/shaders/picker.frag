#version 330 core

in vec3 vNormal;
flat in uint vFaceId;

uniform uint uHighlightFace;   // 当前高亮的面 id (UINT_MAX = 不高亮)
uniform vec3 uHighlightColor;  // 高亮颜色
uniform vec3 uBaseColor;       // 基础颜色
uniform float uAmbient;
uniform vec3 uLightDir;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float diff = max(dot(N, L), 0.0);

    vec3 color = uBaseColor * (uAmbient + diff * 0.8);

    if (vFaceId == uHighlightFace) {
        // 高亮面 — 加亮 + 边框式调色
        color = mix(color, uHighlightColor, 0.6) + uHighlightColor * 0.3;
    }

    FragColor = vec4(color, 1.0);
}
