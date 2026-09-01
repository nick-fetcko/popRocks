#version 300 es

in highp vec2 TexCoords;
out highp vec4 outColor;

uniform sampler2D text;
uniform highp vec2 screenSize;
uniform highp float yOffset;

uniform int bgr;

void main() {
    highp vec2 coords = vec2(
        TexCoords.x * screenSize.x,
        abs((TexCoords.y * screenSize.y) + yOffset)
    );

    /*
    highp vec4 sample1 = texelFetch(text, coords, 0);
    highp vec4 sample2 = texelFetch(text, coords, 1);
    highp vec4 sample3 = texelFetch(text, coords, 2);
    highp vec4 sample4 = texelFetch(text, coords, 3);

    outColor = vec4(sample1 + sample2 + sample3 + sample4) / 4.0f;
    */

    outColor = texture(text, coords / screenSize);

    outColor.w = clamp(outColor.w, 0.0, 1.0);

    if (bgr == 1) {
        highp float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
