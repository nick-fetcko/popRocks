#version 300 es
out highp vec4 outColor;

uniform highp vec4 color;

uniform int bgr;

void main() {    
    outColor = color;

    if (bgr == 1) {
        highp float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
