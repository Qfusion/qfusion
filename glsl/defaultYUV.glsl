// YUV -> RGB color-space conversion shader

#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/yuv.glsl"

qf_varying vec2 v_TexCoord;

#ifdef VERTEX_SHADER

#include "include/attributes.glsl"
#include "include/vtransform.glsl"

void main(void)
{
	gl_Position = u_ModelViewProjectionMatrix * a_Position;
	v_TexCoord = a_TexCoord;
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER
// Fragment shader

uniform sampler2D u_YUVTextureY;
uniform sampler2D u_YUVTextureU;
uniform sampler2D u_YUVTextureV;

void main(void)
{
	qf_FragColor = vec4(YUV2RGB_HDTV(vec3(
		qf_texture(u_YUVTextureY, v_TexCoord).r,
		qf_texture(u_YUVTextureU, v_TexCoord).r, 
		qf_texture(u_YUVTextureV, v_TexCoord).r
	)), 1.0);
}

#endif // FRAGMENT_SHADER
