#version 450

layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 uv;

layout(location=0) out vec3 fragColor;
layout(location=1) out vec3 fragPosWorld;
layout(location=2) out vec3 fragNormalWorld;
layout(location=3) out vec2 fragUV;

struct PointLight {
    vec4 positon; // ignore w
    vec4 color;   // w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
    vec4 ambientLightColor; // w is intensity
    PointLight pointLights[10];
    int numLights;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    vec4 positionWorldSpace = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * positionWorldSpace;

    fragNormalWorld = normalize(mat3(push.normalMatrix) * normal);
    fragPosWorld = positionWorldSpace.xyz;
    fragColor = color;
    fragUV = uv;
}
