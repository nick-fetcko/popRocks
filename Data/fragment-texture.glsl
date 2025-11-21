#version 330 core
in vec2 TexCoords;
out vec4 outColor;

uniform sampler2D text;
uniform sampler3D cube;
uniform vec4 color;

uniform int hdr;
uniform int expand;
uniform float multiplier;

uniform float contrast;
uniform float gamma;
uniform float brightness;

uniform int bgr;

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
        if (expand == 1)
            outColor.rgb *= multiplier;
    }

    outColor.w = clamp(outColor.w, 0.0, 1.0);

    if (bgr == 1) {
        float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
