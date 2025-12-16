#version 330 core
in vec4 color;
out vec4 outColor;

uniform float multiplier;
uniform int normalized;

uniform int bgr;

void main() {    
    outColor = color;

    // Scale down to 0-1 so blending doesn't break
    if (normalized == 1)
        outColor.rgb /= multiplier;

    if (bgr == 1) {
        float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
