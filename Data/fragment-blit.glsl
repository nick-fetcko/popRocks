#version 330 core

in vec2 TexCoords;
out vec4 outColor;

uniform sampler2DMS text;
uniform vec2 screenSize;
uniform vec2 windowSize;
uniform float yOffset;

uniform int bgr;
uniform int vignette;

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

    if (vignette == 1) {
        vec2 screenCoords = (gl_FragCoord.xy - (screenSize - windowSize) / 2.0f) / windowSize;
        screenCoords *= 1.0f - screenCoords.yx;

        float dist = screenCoords.x * screenCoords.y * 27.0f;
        dist = clamp(pow(dist, 0.25f), 0.0f, 1.0f);

        outColor.w = clamp(outColor.w * dist, 0.0f, 1.0f);
    } else {
        outColor.w = clamp(outColor.w, 0.0f, 1.0f);
    }

    if (bgr == 1) {
        float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
