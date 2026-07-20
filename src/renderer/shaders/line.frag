#version 330 core

uniform vec3 uLineColor;

out vec4 FragColor;

void main() {
    FragColor = vec4(uLineColor, 1.0);
}
