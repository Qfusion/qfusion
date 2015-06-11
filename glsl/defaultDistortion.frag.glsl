#include "include/common.glsl"
#include "include/uniforms.glsl"
#include_if(APPLY_GREYSCALE) "include/greyscale.glsl"

qf_varying vec4 v_TexCoord;
qf_varying vec4 v_ProjVector;
#ifdef APPLY_EYEDOT
qf_varying vec3 v_EyeVector;
#endif

#ifdef APPLY_DUDV
uniform sampler2D u_DuDvMapTexture;
#endif

#ifdef APPLY_EYEDOT
uniform sampler2D u_NormalmapTexture;
#endif
uniform sampler2D u_ReflectionTexture;
uniform sampler2D u_RefractionTexture;

void main(void)
{
	myhalf3 color;

#ifdef APPLY_DUDV
	vec3 displacement = vec3(qf_texture(u_DuDvMapTexture, vec2(v_TexCoord.pq) * vec2(0.25)));
	vec2 coord = vec2(v_TexCoord.st) + vec2(displacement) * vec2 (0.2);

	vec3 fdist = vec3 (normalize(vec3(qf_texture(u_DuDvMapTexture, coord)) - vec3 (0.5))) * vec3(0.005);
#else
	vec3 fdist = vec3(0.0);
#endif

	// get projective texcoords
	float scale = float(1.0 / float(v_ProjVector.w));
	float inv2NW = u_TextureParams.z * 0.5; // .z - inverse width
	float inv2NH = u_TextureParams.w * 0.5; // .w - inverse height
	vec2 projCoord = (vec2(v_ProjVector.xy) * scale + vec2 (1.0)) * vec2 (0.5) + vec2(fdist.xy);
	projCoord.s = float (clamp (float(projCoord.s), inv2NW, 1.0 - inv2NW));
	projCoord.t = float (clamp (float(projCoord.t), inv2NH, 1.0 - inv2NH));

	myhalf3 refr = myhalf3(0.0);
	myhalf3 refl = myhalf3(0.0);

#ifdef APPLY_EYEDOT
	// calculate dot product between the surface normal and eye vector
	// great for simulating qf_varying water translucency based on the view angle
	myhalf3 surfaceNormal = normalize(myhalf3(qf_texture(u_NormalmapTexture, coord)) - myhalf3 (0.5));
	vec3 eyeNormal = normalize(myhalf3(v_EyeVector));

	float refrdot = float(dot(surfaceNormal, eyeNormal));
	//refrdot = float (clamp (refrdot, 0.0, 1.0));
	float refldot = 1.0 - refrdot;
	// get refraction and reflection

#ifdef APPLY_REFRACTION
	refr = (myhalf3(qf_texture(u_RefractionTexture, projCoord))) * refrdot;
#endif
#ifdef APPLY_REFLECTION
	refl = (myhalf3(qf_texture(u_ReflectionTexture, projCoord))) * refldot;
#endif

#else

#ifdef APPLY_REFRACTION
	refr = (myhalf3(qf_texture(u_RefractionTexture, projCoord)));
#endif
#ifdef APPLY_REFLECTION
	refl = (myhalf3(qf_texture(u_ReflectionTexture, projCoord)));
#endif

#endif // APPLY_EYEDOT

	// add reflection and refraction
#ifdef APPLY_DISTORTION_ALPHA
	color = myhalf3(qf_FrontColor.rgb) + myhalf3(mix (refr, refl, myhalf(qf_FrontColor.a)));
#else
	color = myhalf3(qf_FrontColor.rgb) + refr + refl;
#endif

#ifdef APPLY_GREYSCALE
	qf_FragColor = vec4(vec3(Greyscale(color)),1.0);
#else
	qf_FragColor = vec4(vec3(color),1.0);
#endif
}
