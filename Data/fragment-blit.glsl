#version 330 core

in vec2 TexCoords;
out vec4 outColor;

uniform sampler2DMS text;
uniform vec2 screenSize;
uniform float yOffset;

void main() {
    ivec2 coords = ivec2(
        TexCoords.x * screenSize.x,
        abs((TexCoords.y * screenSize.y) + yOffset)
    );

    vec4 sample1 = texelFetch(text, coords, 0);
    vec4 sample2 = texelFetch(text, coords, 1);
    vec4 sample3 = texelFetch(text, coords, 2);
    vec4 sample4 = texelFetch(text, coords, 3);

    outColor = vec4(sample1 + sample2 + sample3 + sample4) / 4.0f;

    outColor.w = clamp(outColor.w, 0.0, 1.0);
}