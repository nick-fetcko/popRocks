// Name: Smoke

#define M_PI       3.14159265358979323846   // pi
#define DIV 240.0f

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

// From https://stackoverflow.com/a/17479300
highp uint hash( highp uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );

    return x;
}

// Compound versions of the hashing algorithm I whipped together.
highp uint hash( highp uvec2 v ) { return hash( v.x ^ hash(v.y)                         ); }
highp uint hash( highp uvec3 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z)             ); }
highp uint hash( highp uvec4 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) ); }

// Construct a float with a range of [-1:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
highp float floatConstruct( highp uint m ) {
    const highp uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const highp uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    highp float f = uintBitsToFloat( m ); // Range [1:2]
    //f -= floor(f);
    f -= 1.0;                              // Range [0:1]
    return (2.0 * f) - 1.0;                // Range [-1:1]
}

// Pseudo-random value with a range of [-1:1].
highp float random( highp float x ) { return floatConstruct(hash(floatBitsToUint(x))); }
highp float random( highp vec2  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
highp float random( highp vec3  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
highp float random( highp vec4  v ) { return floatConstruct(hash(floatBitsToUint(v))); }

highp vec3 applyEffect(highp vec2 coords, highp vec2 screenSize) {
    highp vec2 centered = vec2((coords.x - 0.5) * 2.0, (coords.y - 0.5) * 2.0);

    highp float angle = atan(centered.y, centered.x) - M_PI;

    highp vec2 uv = coords;
    uv -= 0.5; // Center it
    uv = vec2(
        uv.x * cos(effectTimeDelta / DIV * effectRotation) - uv.y * sin(effectTimeDelta / DIV * effectRotation),
        uv.x * sin(effectTimeDelta / DIV * effectRotation) + uv.y * cos(effectTimeDelta / DIV * effectRotation)
    );
    uv += 0.5; // Bring it back from the center, to the corner
    uv *= screenSize;

    uv += vec2(
        random(
            vec3(coords, randomX)
        ) * effectIntensity * effectEnabled + effectXOffset * effectTimeDelta + cos(angle) * effectRadiation * effectTimeDelta + centered.x * -effectHorizontalSpread * effectTimeDelta,
        random(
            vec3(coords, randomY)
        ) * effectIntensity * effectEnabled - effectYOffset * effectTimeDelta + sin(angle) * effectRadiation * effectTimeDelta + centered.y * -effectVerticalSpread * effectTimeDelta
    );

    return vec3(uv, 0.0f);
}