#ifndef decodedepthmacro24
#define decodedepthmacro24(d) dot((d).rgb, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0))
#define encodedepthmacro24(d) fract(d * vec4(1.0, 255.0, 65025.0, 0.0))
// Use green for near because it has one more bit of precision
#define decodedepthmacro16(d) dot((d).rgb, vec3(1.0, 1.0 / 961.0, 1.0 / 31.0))
#define encodedepthmacro16(d) fract(d * vec4(1.0, 961.0, 31.0, 0.0))
#endif
