#version 330 core

in vec3 vFragPos;
in vec3 vNormal;
in vec3 vViewNormal;
in vec2 vUv;

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

// PMX 材质控制 (非 PMX 模型全为 0)
uniform float uAlpha;              // 全局透明度乘数
uniform int   uColorOverride;      // 1 = 用 uOverrideColor 替换基础色
uniform vec3  uOverrideColor;
uniform int   uUseTexture;         // Tex 漫反射贴图
uniform sampler2D uTex;
uniform int   uUseToon;            // Toon 渐变贴图 (按光照强度采样)
uniform sampler2D uToonTex;
uniform int   uUseSphere;          // Sphere 球面贴图
uniform int   uSphereMode;         // 1=乘 2=加 (3 按乘处理)
uniform sampler2D uSphereTex;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);   // 从面片指向光源
    vec3 V = normalize(uViewPos - vFragPos);
    vec3 H = normalize(L + V);

    // 基础色: 可被全局颜色覆盖,再乘漫反射贴图
    vec3 base = uBaseColor;
    if (uColorOverride == 1) base = uOverrideColor;
    if (uUseTexture == 1)    base *= texture(uTex, vUv).rgb;

    // 主光漫反射
    float diff = max(dot(N, L), 0.0);

    // 背面补光 — 防止背面纯黑
    float backLight = max(dot(-N, L), 0.0) * uFillStrength;

    float lum = uAmbient + diff * 0.7 + backLight;

    // Blinn-Phong 高光
    float spec = pow(max(dot(N, H), 0.0), 64.0);

    vec3 color = base * lum
               + uFillColor * backLight
               + uLightColor * spec * uSpecStrength;

    // Toon: 按光照强度查渐变贴图,乘到颜色上
    if (uUseToon == 1) {
        color *= texture(uToonTex, vec2(0.5, clamp(lum, 0.0, 1.0))).rgb;
    }

    // Sphere: 视空间法线 xy 作球面映射
    if (uUseSphere == 1) {
        vec2 suv = normalize(vViewNormal).xy * 0.5 + 0.5;
        vec3 sph = texture(uSphereTex, suv).rgb;
        if (uSphereMode == 2) color += sph;
        else                  color *= sph;
    }

    // 深度雾效
    float dist = length(uViewPos - vFragPos);
    float fogFactor = clamp((dist - uFogNear) / max(uFogFar - uFogNear, 1e-4), 0.0, 1.0);
    color = mix(color, uFogColor, fogFactor);

    FragColor = vec4(color, uAlpha);
}
