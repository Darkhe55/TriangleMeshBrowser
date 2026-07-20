#version 330 core

in vec3 vColor;
in float vViewDist;

uniform float uFogEnabled;
uniform float uFogNear;
uniform float uFogFar;
uniform vec3  uFogColor;

out vec4 FragColor;

void main() {
    vec3 color = vColor;
    if (uFogEnabled > 0.5) {
        float fogF = clamp((vViewDist - uFogNear) / max(uFogFar - uFogNear, 1e-4), 0.0, 1.0);
        color = mix(color, uFogColor, fogF);
    }
    FragColor = vec4(color, 1.0);
}
