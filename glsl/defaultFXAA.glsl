// FXAA shader
// Original algorithm and code by Timothy Lottes

#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL 1.0/8.0
#define FXAA_REDUCE_MIN 1.0/128.0

#include "include/uniforms.glsl"

varying vec2 v_TexCoord;

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

uniform sampler2D u_BaseTexture;

void main(void)
{
#define Tex() texture2D(u_BaseTexture, v_TexCoord).rgb
#define TexOfs(ofs) texture2D(u_BaseTexture, v_TexCoord + ofs).rgb
#define TexOfsInv(ofs) texture2D(u_BaseTexture, v_TexCoord + ofs*invViewportSize).rgb

	vec2 invViewportSize = vec2(1.0) / vec2(u_Viewport.zw);
	myhalf3 rgbNW = TexOfsInv(vec2(-1.0,-1.0));
	myhalf3 rgbNE = TexOfsInv(vec2(1.0,-1.0));
	myhalf3 rgbSW = TexOfsInv(vec2(-1.0,1.0));
	myhalf3 rgbSE = TexOfsInv(vec2(1.0,1.0));
	myhalf3 rgbM = Tex();
	
	myhalf3 luma = myhalf3(0.299, 0.587, 0.114);
	myhalf lumaNW = dot(rgbNW, luma);
	myhalf lumaNE = dot(rgbNE, luma);
	myhalf lumaSW = dot(rgbSW, luma);
	myhalf lumaSE = dot(rgbSE, luma);
	myhalf lumaM  = dot(rgbM,  luma);
	
	myhalf lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	myhalf lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	
	myhalf2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
	
	myhalf dirReduce = max(
		(lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
		FXAA_REDUCE_MIN);
	  
	myhalf rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);
	
	dir = min(myhalf2( FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
		  max(myhalf2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
		  dir * rcpDirMin)) * myhalf2(invViewportSize);
		
	myhalf3 rgbA = (1.0/2.0) * (
		TexOfs(dir * (1.0/3.0 - 0.5)) +
		TexOfs(dir * (2.0/3.0 - 0.5)));
	myhalf3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
		TexOfs(dir * (0.0/3.0 - 0.5)) +
		TexOfs(dir * (3.0/3.0 - 0.5)));
	myhalf lumaB = dot(rgbB, luma);

	if((lumaB < lumaMin) || (lumaB > lumaMax)){
		gl_FragColor.rgb = vec3(rgbA);
	}else{
		gl_FragColor.rgb = vec3(rgbB);
	}
}

#endif // FRAGMENT_SHADER
