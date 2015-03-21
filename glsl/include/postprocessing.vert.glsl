#include "common.glsl"
#include "uniforms.glsl"
#include "attributes.glsl"

qf_varying vec2 v_TexCoord;

void main(void)
{
    gl_Position = u_ModelViewProjectionMatrix * a_Position;
	v_TexCoord = TextureMatrix2x3Mul(u_TextureMatrix, a_Position.xy);
	v_TexCoord.y = 1.0 - v_TexCoord.y;
}
