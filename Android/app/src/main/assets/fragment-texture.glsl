#version 300 es
in highp vec2 TexCoords;
out highp vec4 outColor;

uniform highp sampler2D text;
uniform highp sampler3D cube;
uniform highp vec4 color;

uniform int hdr;
uniform int expand;
uniform highp float multiplier;

uniform highp float contrast;
uniform highp float gamma;
uniform highp float brightness;

uniform int bgr;

void main() {
    highp vec4 sampled = texture(text, TexCoords);

    if (hdr == 1) {
        outColor = texture(cube, sampled.rgb);
        //outColor = sampled;
        outColor.rgb = pow(outColor.rgb, vec3(1.0 / gamma));
        outColor.rgb = ((outColor.rgb - 0.5f) * max(contrast, 0.0)) + 0.5f;

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
        highp float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
