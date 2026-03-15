#version 450

layout(location=0) in vec3 fragColor;
layout(location=1) in vec3 fragPosWorld;
layout(location=2) in vec3 fragNormalWorld;
layout(location=3) in vec2 fragUV;

layout(location=0) out vec4 outColor;

struct PointLight {
    vec4 position; // ignore w
    vec4 color;    // w is intensity
};

struct DirectionalLight {
    vec4 direction; // xyz = direction, w unused
    vec4 color;     // rgb = color, w = intensity
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
    vec4 ambientLightColor; // w is intensity
    PointLight pointLights[10];

    int numLights;
    vec3 _pad0; // std140 padding (C++ ile ayný)

    DirectionalLight sunLight;
} ubo;

// texture
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    vec3 N = normalize(fragNormalWorld);

    vec3 cameraPosWorld = ubo.inverseViewMatrix[3].xyz;
    vec3 V = normalize(cameraPosWorld - fragPosWorld);

    // texture sample
    vec3 albedo = texture(texSampler, fragUV).rgb;
    vec3 baseColor = albedo * fragColor;

    // ---- Ambient ----
    vec3 diffuseLight = ubo.ambientLightColor.rgb * ubo.ambientLightColor.a;
    vec3 specularLight = vec3(0.0);

    // ---- Directional (Sun) ----
    vec3 Ld = normalize(-ubo.sunLight.direction.xyz); // ýþýðýn geliþ yönü
    float NdotLd = max(dot(N, Ld), 0.0);
    vec3 sunIntensity = ubo.sunLight.color.rgb * ubo.sunLight.color.a;

    diffuseLight += sunIntensity * NdotLd;

    // Blinn-Phong specular for sun
    vec3 Hs = normalize(Ld + V);
    float sunSpec = pow(max(dot(N, Hs), 0.0), 32.0);
    specularLight += sunIntensity * sunSpec;

    // ---- Point lights ----
    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];

        vec3 toLight = light.position.xyz - fragPosWorld;
        float dist = length(toLight);
        vec3 L = toLight / max(dist, 0.0001);

        // Þehir ölçeðine uygun attenuation (daha yumuþak)
        // Ýstersen bu katsayýlarý dist ölçeðine göre oynarýz.
        float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

        float NdotL = max(dot(N, L), 0.0);
        vec3 intensity = light.color.rgb * light.color.a * attenuation;

        diffuseLight += intensity * NdotL;

        vec3 H = normalize(L + V);
        float blinnTerm = pow(max(dot(N, H), 0.0), 32.0);
        specularLight += intensity * blinnTerm;
    }

    vec3 lit = diffuseLight * baseColor + specularLight * baseColor;
    outColor = vec4(lit, 1.0);
}
