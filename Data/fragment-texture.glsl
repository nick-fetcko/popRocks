#version 330 core
in vec2 TexCoords;
out vec4 outColor;

uniform sampler2D text;
uniform vec4 color;

void main() {    
    vec4 sampled = texture(text, TexCoords);
    outColor = color * sampled;
}