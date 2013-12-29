// Qfusion distortion GLSL shader

#include "include/common.glsl"
#include "include/uniforms.glsl"

#include "include/greyscale.glsl"

qf_varying vec4 v_TexCoord;
qf_varying vec4 v_ProjVector;
#ifdef APPLY_EYEDOT
qf_varying vec3 v_EyeVector;
#endif

#ifdef VERTEX_SHADER
// Vertex shader

#include "include/attributes.glsl"
#include "include/vtransform.glsl"
#include "include/rgbgen.glsl"

#ifdef APPLY_EYEDOT
uniform float u_FrontPlane;
#endif

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;
	vec3 Tangent = a_SVector.xyz;
	float TangentDir = a_SVector.w;
	myhalf4 inColor = myhalf4(a_Color);

	TransformVerts(Position, Normal, TexCoord);

	qf_FrontColor = vec4(VertexRGBGen(Position, Normal, inColor));

	v_TexCoord.st = TextureMatrix2x3Mul(u_TextureMatrix, TexCoord);

	vec2 textureMatrix3_[3];
	textureMatrix3_[0] =  u_TextureMatrix[0];
	textureMatrix3_[1] =  u_TextureMatrix[1];
	textureMatrix3_[2] = -u_TextureMatrix[2];
	v_TexCoord.pq = TextureMatrix2x3Mul(textureMatrix3_, TexCoord);

#ifdef APPLY_EYEDOT
	mat3 v_StrMatrix;
	v_StrMatrix[0] = Tangent;
	v_StrMatrix[2] = Normal;
	v_StrMatrix[1] = TangentDir * cross(Normal, Tangent);

	vec3 EyeVectorWorld = (u_ViewOrigin - Position.xyz) * u_FrontPlane;
	v_EyeVector = EyeVectorWorld * v_StrMatrix;
#endif

	gl_Position = u_ModelViewProjectionMatrix * Position;
	v_ProjVector = gl_Position;
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER
// Fragment shader

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

#endif // FRAGMENT_SHADER
