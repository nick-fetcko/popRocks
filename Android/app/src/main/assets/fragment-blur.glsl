#version 320 es

in highp vec2 texCoords;
out highp vec4 outColor;

uniform highp float intensity;
uniform highp float timeDelta;
uniform sampler2D text;
uniform highp vec2 screenSize;

uniform int premultipliedAlpha;
uniform int bgr;

highp vec3 applyEffect(highp vec2 coords, highp vec2 screenSize);

void main() {
    highp vec3 effected = applyEffect(texCoords, screenSize);

    highp ivec2 coords = ivec2(
        screenSize.x * texCoords.x * effected.z + effected.x,
        screenSize.y * texCoords.y * effected.z + effected.y
    );

    highp vec4 sampled = texelFetch(text, coords, 0);
    //highp vec4 sampled = texture(text, coords / screenSize);
    
    sampled.w = clamp(sampled.w - (1.0 / intensity) * timeDelta, 0.0, 1.0);

    if (premultipliedAlpha == 1) {
        // Pre-multiply alpha
        sampled.rgb *= sampled.w;

        // Blend with black
        sampled.rgb -= (1.0 - sampled.w);

        // Now that we're blended,
        // we don't need alpha.
        sampled.w = 1.0f;
    }

    outColor = sampled;

    if (bgr == 1) {
        highp float temp = outColor.r;
        outColor.r = outColor.b;
        outColor.b = temp;
    }
}
