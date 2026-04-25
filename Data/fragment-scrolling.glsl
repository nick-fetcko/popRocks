#version 330 core
in vec2 TexCoords;
out vec4 outColor;

uniform sampler2D text;
uniform vec4 color;

uniform float maxWidth;
uniform vec2 screenSize;
uniform vec2 origin;

uniform int bleedEdge;

void main() {
    vec4 sampled = texture(text, TexCoords);
    float distance = abs(gl_FragCoord.x - (origin.x + maxWidth / 2)) - maxWidth / 2;

    outColor = color * sampled;

    if (distance > 0)
        outColor.w *= 1.0f - (distance / bleedEdge /* Fade out over bleedEdge pixels */);
}