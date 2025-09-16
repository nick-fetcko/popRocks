#version 330 core

// Name: No Effect

uniform float randomX;
uniform float randomY;

uniform float effectIntensity;
uniform float effectXOffset;
uniform float effectYOffset;

vec2 applyEffect(vec2 coords) {
    return vec2(
        effectXOffset,
        effectYOffset
    );
}