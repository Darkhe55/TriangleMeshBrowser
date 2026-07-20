#version 330 core

in vec3 vFragPos;
in vec3 vNormal;

uniform vec3  uLightDir;     // 主光源方向(指向光源)
uniform vec3  uLightColor;   // 光源颜色
uniform vec3  uViewPos;      // 相机位置
uniform vec3  uBaseColor;    // 基础面片颜色
uniform float uAmbient;      // 环境光强度 [0,1]
uniform vec3  uFillColor;    // 补光颜色
uniform float uFillStrength; // 补光强度 [0,1]
uniform vec3  uFogColor;     // 雾色(=背景色)
uniform float uFogNear;      // 雾起始距离
uniform float uFogFar;       // 雾结束距离
uniform float uSpecStrength; // 高光强度

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);   // 从面片指向光源
    vec3 V = normalize(uViewPos - vFragPos);
    vec3 H = normalize(L + V);

    // 主光漫反射
    float diff = max(dot(N, L), 0.0);

    // 背面补光 — 防止背面纯黑
    float backLight = max(dot(-N, L), 0.0) * uFillStrength;

    // Blinn-Phong 高光
    float spec = pow(max(dot(N, H), 0.0), 64.0);

    vec3 color = uBaseColor * (uAmbient + diff * 0.7 + backLight)
               + uFillColor * backLight
               + uLightColor * spec * uSpecStrength;

    // 深度雾效
    float dist = length(uViewPos - vFragPos);
    float fogFactor = clamp((dist - uFogNear) / max(uFogFar - uFogNear, 1e-4), 0.0, 1.0);
    color = mix(color, uFogColor, fogFactor);

    FragColor = vec4(color, 1.0);
}
