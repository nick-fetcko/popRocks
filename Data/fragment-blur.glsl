#version 330 core
in vec2 texCoords;
out vec4 outColor;

uniform float intensity;
uniform float timeDelta;
uniform sampler2DMS texture;
uniform vec2 screenSize;

void main() {
    ivec2 coords = ivec2(
        screenSize.x * texCoords.x,
        screenSize.y * texCoords.y
    );

    vec4 sample1 = texelFetch(texture, coords, 0);
    vec4 sample2 = texelFetch(texture, coords, 1);
    vec4 sample3 = texelFetch(texture, coords, 2);
    vec4 sample4 = texelFetch(texture, coords, 3);

    vec4 sampled = vec4(sample1 + sample2 + sample3 + sample4) / 4.0f;
    
    sampled.w -= (1.0 / intensity) * timeDelta;
    outColor = sampled;
}