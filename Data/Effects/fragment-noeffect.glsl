#version 330 core

// Name: No Effect

#define M_PI       3.14159265358979323846   // pi

uniform float randomX;
uniform float randomY;

uniform float effectIntensity;
uniform float effectXOffset;
uniform float effectYOffset;
uniform float effectRadiation;

vec2 applyEffect(vec2 coords) {
    vec2 centered = vec2((coords.x - 0.5) * 2, (coords.y - 0.5) * 2);

    float angle = atan(centered.y, centered.x) - M_PI;

    return vec2(
        effectXOffset + cos(angle) * effectRadiation,
        effectYOffset + sin(angle) * effectRadiation
    );
}