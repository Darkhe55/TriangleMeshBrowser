#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in uint aFaceId;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vNormal;
flat out uint vFaceId;

void main() {
    vNormal  = uNormalMatrix * aNormal;
    vFaceId  = aFaceId;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
