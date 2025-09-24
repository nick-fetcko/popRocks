#version 330 core

// Name: No Effect

#define M_PI       3.14159265358979323846   // pi

uniform float randomX;
uniform float randomY;

uniform float effectIntensity;
uniform float effectXOffset;
uniform float effectYOffset;
uniform float effectRadiation;
uniform float effectTimeDelta;
uniform float effectHorizontalSpread;
uniform float effectVerticalSpread;

vec2 applyEffect(vec2 coords) {
    vec2 centered = vec2((coords.x - 0.5) * 2, (coords.y - 0.5) * 2);

    float angle = atan(centered.y, centered.x) - M_PI;

    return vec2(
        effectXOffset * effectTimeDelta + cos(angle) * effectRadiation * effectTimeDelta + centered.x * -effectHorizontalSpread * effectTimeDelta,
        effectYOffset * effectTimeDelta + sin(angle) * effectRadiation * effectTimeDelta + centered.y * -effectVerticalSpread * effectTimeDelta
    );
}