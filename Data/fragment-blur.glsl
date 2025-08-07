#version 330 core
in vec2 TexCoords;
out vec4 outColor;

uniform float intensity;
uniform float timeDelta;
uniform sampler2D text;

void main() {    
    vec4 sampled = texture(text, TexCoords);
    sampled.w -= (1.0 / intensity) * timeDelta;
    outColor = sampled;
}