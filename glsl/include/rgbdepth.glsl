#ifndef decodedepthmacro
// Lifted from Darkplaces shader program
#define decodedepthmacro(d) dot((d).rgb, vec3(1.0, 255.0 / 65536.0, 255.0 / 16777215.0))
#define encodedepthmacro(d) (vec4(d, d*256.0, d*65536.0, 0.0) - floor(vec4(d, d*256.0, d*65536.0, 0.0)))
#endif
