#ifndef decodedepthmacro
// Lifted from Darkplaces shader program
#define decodedepthmacro(d) dot((d).rgb, vec3(1.0, 255.0 / 65536.0, 255.0 / 16777215.0))
#define encodedepthmacro(d) fract(d * vec4(1.0, 256.0, 65536.0, 0.0))
#endif
