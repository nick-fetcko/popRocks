#version 330 core
out vec4 outColor;

uniform vec4 color;

uniform int bgr;

void main() {    
    outColor = color;

    if (bgr == 1) {
        float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
