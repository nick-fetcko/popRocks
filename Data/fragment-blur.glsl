#version 330 core

in vec2 texCoords;
out vec4 outColor;

uniform float intensity;
uniform float timeDelta;
uniform sampler2DMS texture;
uniform vec2 screenSize;

vec3 applyEffect(vec2 coords, vec2 screenSize);

void main() {
    vec3 effected = applyEffect(texCoords, screenSize);

    ivec2 coords = ivec2(
        screenSize.x * texCoords.x * effected.z + effected.x,
        screenSize.y * texCoords.y * effected.z + effected.y
    );

    vec4 sample1 = texelFetch(texture, coords, 0);
    vec4 sample2 = texelFetch(texture, coords, 1);
    vec4 sample3 = texelFetch(texture, coords, 2);
    vec4 sample4 = texelFetch(texture, coords, 3);

    vec4 sampled = vec4(sample1 + sample2 + sample3 + sample4) / 4.0f;
    
    sampled.w = clamp(sampled.w - (1.0 / intensity) * timeDelta, 0.0, 1.0);

    outColor = sampled;
}