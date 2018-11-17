#ifdef LIGHTMAP_BICUBIC

vec4 cubic( float v ) {
	vec4 n = vec4( 1.0, 2.0, 3.0, 4.0 ) - v;
	vec4 s = n * n * n;
	float x = s.x;
	float y = s.y - 4.0 * s.x;
	float z = s.z - 4.0 * s.y + 6.0 * s.x;
	float w = 6.0 - x - y - z;
	return vec4( x, y, z, w );
}

vec4 texture_bicubic( sampler2D tex, vec2 uv ) {
	vec2 size = textureSize( tex, 0 );
	uv *= size;

	vec2 fuv = fract( uv );
	uv -= fuv;

	vec4 xcubic = cubic( fuv.x );
	vec4 ycubic = cubic( fuv.y );

	vec4 c = vec4( uv.x - 0.5, uv.x + 1.5, uv.y - 0.5, uv.y + 1.5 );
	vec4 s = vec4( xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w );
	vec4 offs = ( c + vec4( xcubic.y, xcubic.w, ycubic.y, ycubic.w ) / s ) / size.xxyy;

	vec4 s0 = texture( tex, offs.xz );
	vec4 s1 = texture( tex, offs.yz );
	vec4 s2 = texture( tex, offs.xw );
	vec4 s3 = texture( tex, offs.yw );

	float sx = s.x / ( s.x + s.y );
	float sy = s.z / ( s.z + s.w );

	return mix( mix( s3, s2, sx ), mix( s1, s0, sx ), sy );
}

vec4 texturearray_bicubic( sampler2DArray tex, vec2 uv, float l ) {
	vec2 size = textureSize( tex, 0 ).xy;
	uv *= size;

	vec2 fuv = fract( uv );
	uv -= fuv;

	vec4 xcubic = cubic( fuv.x );
	vec4 ycubic = cubic( fuv.y );

	vec4 c = vec4( uv.x - 0.5, uv.x + 1.5, uv.y - 0.5, uv.y + 1.5 );
	vec4 s = vec4( xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w );
	vec4 offs = ( c + vec4( xcubic.y, xcubic.w, ycubic.y, ycubic.w ) / s ) / size.xxyy;

	vec4 s0 = texture( tex, vec3( offs.xz, l ) );
	vec4 s1 = texture( tex, vec3( offs.yz, l ) );
	vec4 s2 = texture( tex, vec3( offs.xw, l ) );
	vec4 s3 = texture( tex, vec3( offs.yw, l ) );

	float sx = s.x / ( s.x + s.y );
	float sy = s.z / ( s.z + s.w );

	return mix( mix( s3, s2, sx ), mix( s1, s0, sx ), sy );
}

#ifdef LIGHTMAP_ARRAYS
#define LightmapSampler sampler2DArray
#define LightmapAt(t, c, l) texturearray_bicubic(t, c, l)
#else
#define LightmapSampler sampler2D
#define LightmapAt(t, c, l) texture_bicubic(t, c)
#endif

#else

#ifdef LIGHTMAP_ARRAYS
#define LightmapSampler sampler2DArray
#define LightmapAt(t, c, l) qf_textureArray(t, vec3(c, l))
#else
#define LightmapSampler sampler2D
#define LightmapAt(t, c, l) qf_texture(t, c)
#endif

#endif

#ifdef APPLY_SRGB2LINEAR
# define Lightmap(t, c, l) LinearFromsRGB(myhalf3(LightmapAt(t, c, l)))
#else
# define Lightmap(t, c, l) myhalf3(LightmapAt(t, c, l))
#endif

#define Deluxemap(t, c, l) normalize(myhalf3(LightmapAt(t, c, l)) - myhalf3(0.5))
