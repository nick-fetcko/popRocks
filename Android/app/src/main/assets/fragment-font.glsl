#version 300 es
in highp vec2 TexCoords;
out highp vec4 outColor;

uniform highp sampler2D text;
uniform highp vec4 color;

void main() {
    highp vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    outColor = color * sampled;
}  