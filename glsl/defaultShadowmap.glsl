// shadowmapping GLSL shader

#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/rgbdepth.glsl"

#ifndef MAX_SHADOWS
#define MAX_SHADOWS 1
#endif

varying vec4 v_ShadowProjVector[MAX_SHADOWS];

#ifdef VERTEX_SHADER
// Vertex shader

#include "include/attributes.glsl"
#include "include/vtransform.glsl"

uniform mat4 u_ShadowmapMatrix[MAX_SHADOWS];

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal;
	vec2 TexCoord = a_TexCoord;

	TransformVerts(Position, Normal, TexCoord);

	gl_Position = u_ModelViewProjectionMatrix * Position;

	for (int i = 0; i < MAX_SHADOWS; i++)
	{
		v_ShadowProjVector[i] = u_ShadowmapMatrix[i] * Position;
		// a trick whish allows us not to perform the
		// 'shadowmaptc = (shadowmaptc + vec3 (1.0)) * vec3 (0.5)'
		// computation in the fragment shader
		v_ShadowProjVector[i].xyz = (v_ShadowProjVector[i].xyz + vec3(v_ShadowProjVector[i].w)) * 0.5;
	}
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER
// Fragment shader

#ifdef APPLY_RGB_SHADOW
uniform sampler2D u_ShadowmapTexture[MAX_SHADOWS];
# define qfShadow2D(t,v) step(v.z, decodedepthmacro(texture2D(t, v.xy)))
#else
uniform sampler2DShadow u_ShadowmapTexture[MAX_SHADOWS];
# define qfShadow2D(t,v) shadow2D(t,v)
#endif

uniform myhalf u_ShadowAlpha;
uniform float u_ShadowProjDistance[MAX_SHADOWS];
uniform vec4 u_ShadowmapTextureParams[MAX_SHADOWS];

void main(void)
{
	myhalf finalcolor = myhalf(1.0);

	for (int i = 0; i < MAX_SHADOWS; i++)
	{
		vec3 shadowmaptc = vec3 (v_ShadowProjVector[i].xyz / v_ShadowProjVector[i].w);

		// this keeps shadows from appearing on surfaces behind frustum's nearplane
		myhalf d = step(v_ShadowProjVector[i].w, 0.0);

		//shadowmaptc = (shadowmaptc + vec3 (1.0)) * vec3 (0.5);
		shadowmaptc.xy = shadowmaptc.xy * u_ShadowmapTextureParams[i].xy; // .x - texture width
		shadowmaptc.z = clamp(shadowmaptc.z, 0.0, 1.0);
		shadowmaptc.xy = vec2(clamp(shadowmaptc.x, 0.0, u_ShadowmapTextureParams[i].x), clamp(shadowmaptc.y, 0.0, u_ShadowmapTextureParams[i].y));

		vec2 ShadowMap_TextureScale = u_ShadowmapTextureParams[i].zw;

		myhalf f;

		#ifdef APPLY_DITHER

		# ifdef APPLY_PCF
		#  define texval(x, y) myhalf(qfShadow2D(u_ShadowmapTexture[i], vec3(center + vec2(x, y)*ShadowMap_TextureScale, shadowmaptc.z)))

		// this method can be described as a 'dithered pinwheel' (4 texture lookups)
		// which is a combination of the 'pinwheel' filter suggested by eihrul and dithered 4x4 PCF,
		// described here: http://http.developer.nvidia.com/GPUGems/gpugems_ch11.html 

		vec2 offset_dither = mod(floor(gl_FragCoord.xy), 2.0);
		offset_dither.y += offset_dither.x;  // y ^= x in floating point
		offset_dither.y *= step(offset_dither.y, 1.1);

		vec2 center = (shadowmaptc.xy + offset_dither.xy) * ShadowMap_TextureScale;
		myhalf group1 = texval(-0.4,  1.0);
		myhalf group2 = texval(-1.0, -0.4);
		myhalf group3 = texval( 0.4, -1.0);
		myhalf group4 = texval( 1.0,  0.4);

		f = dot(myhalf4(0.25), myhalf4(group1, group2, group3, group4));
		# else
		f = myhalf(qfShadow2D(u_ShadowmapTexture[i], vec3(shadowmaptc.xy*ShadowMap_TextureScale, shadowmaptc.z)));
		# endif // APPLY_PCF
	
		#else
		// an essay by eihrul:
		// now think of bilinear filtering as a 1x1 weighted box filter
		// that is, it's sampling over a 2x2 area, but only collecting the portion of each pixel it actually steps on
		// with a linear shadowmap filter, you are getting that, like normal bilinear sampling
		// only its doing the shadowmap test on each pixel first, to generate a new little 2x2 area, then its doing
		// the bilinear filtering on that
		// so now if you consider your 2x2 filter you have
		// each of those taps is actually using linear filtering as you've configured it
		// so you are literally sampling almost 16 pixels as is and all you are getting for it is 2x2
		// the trick is to realize that in essence you could instead be sampling a 4x4 area of pixels
		// and running a 3x3 weighted box filter on it
		// but you would need some way to get the shadowmap to simply return the 4 pixels covered by each
		// tap, rather than the filtered result
		// which is what the ARB_texture_gather extension is for
		// NOTE: we're using emulation of texture_gather now

		# ifdef APPLY_PCF
		# define texval(off) qfShadow2D(u_ShadowmapTexture[i], vec3(off,shadowmaptc.z))
		
		vec2 offset = fract(shadowmaptc.xy - 0.5);
		vec4 size = vec4(offset + 1.0, 2.0 - offset), weight = (vec4(2.0 - 1.0 / size.xy, 1.0 / size.zw - 1.0) + (shadowmaptc.xy - offset).xyxy)*ShadowMap_TextureScale.xyxy;
		f = (1.0/9.0)*dot(size.zxzx*size.wwyy, vec4(texval(weight.zw), texval(weight.xw), texval(weight.zy), texval(weight.xy)));
		
		#else
		
		f = myhalf(qfShadow2D(u_ShadowmapTexture[i], vec3(shadowmaptc.xy * ShadowMap_TextureScale, shadowmaptc.z)));
		
		#endif // APPLY_PCF
		
		#endif // APPLY_DITHER

		finalcolor *= clamp(max(max(f, d), u_ShadowAlpha), 0.0, 1.0);
	}

	gl_FragColor = vec4(vec3(finalcolor),1.0);
}

#endif // FRAGMENT_SHADER

