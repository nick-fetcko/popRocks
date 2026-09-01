// Name: No Effect

#define M_PI       3.14159265358979323846   // pi

uniform highp float randomX;
uniform highp float randomY;

uniform highp float effectIntensity;
uniform highp float effectXOffset;
uniform highp float effectYOffset;
uniform highp float effectRadiation;
uniform highp float effectTimeDelta;
uniform highp float effectHorizontalSpread;
uniform highp float effectVerticalSpread;
uniform highp float effectRotation;
uniform highp float effectEnabled;

highp vec3 applyEffect(highp vec2 coords, highp vec2 screenSize) {
    highp vec2 centered = vec2((coords.x - 0.5) * 2.0, (coords.y - 0.5) * 2.0);

    highp float angle = atan(centered.y, centered.x) - M_PI;

    return vec3(
        effectXOffset * effectTimeDelta + cos(angle) * effectRadiation * effectTimeDelta + centered.x * -effectHorizontalSpread * effectTimeDelta,
        -effectYOffset * effectTimeDelta + sin(angle) * effectRadiation * effectTimeDelta + centered.y * -effectVerticalSpread * effectTimeDelta,
        1.0f
    );
}