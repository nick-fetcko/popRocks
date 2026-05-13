#version 330 core
out vec4 outColor;

uniform vec4 color;

uniform float radius;

uniform int bgr;

uniform vec2 screenSize;

void main() {    
    outColor = color;

    if (length(gl_FragCoord.xy - screenSize / 2) < radius)
        discard;

    if (bgr == 1) {
        float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
