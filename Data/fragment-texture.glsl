#version 330 core
in vec2 TexCoords;
out vec4 outColor;

uniform sampler2D text;
uniform sampler3D cube;
uniform vec4 color;

uniform int hdr;
uniform float multiplier;

uniform float contrast;
uniform float gamma;
uniform float brightness;

void main() {
    vec4 sampled = texture(text, TexCoords);

    if (hdr == 1) {
        outColor = texture(cube, sampled.rgb);

        outColor.rgb = pow(outColor.rgb, vec3(1.0 / gamma));
        outColor.rgb = ((outColor.rgb - 0.5f) * max(contrast, 0)) + 0.5f;

        outColor.rgb *= multiplier;

        outColor.rgb += brightness;

        outColor.w = sampled.w * color.w;
    } else { 
        outColor = color * sampled;
    }
}