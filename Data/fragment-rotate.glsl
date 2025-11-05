#version 330 core
in vec4 color;
out vec4 outColor;

uniform float multiplier;
uniform int normalize;

void main() {    
    outColor = color;

    // Scale down to 0-1 so blending doesn't break
    if (normalize == 1)
        outColor.rgb /= multiplier;
}