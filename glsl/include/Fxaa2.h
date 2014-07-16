#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL 1.0/8.0
#define FXAA_REDUCE_MIN 1.0/128.0

vec4 FxaaPixelShader(
    vec2 pos,
    vec4 fxaaConsolePosPos,
    sampler2D tex,
    sampler2D fxaaConsole360TexExpBiasNegOne,
    sampler2D fxaaConsole360TexExpBiasNegTwo,
    vec2 fxaaQualityRcpFrame,
    vec4 fxaaConsoleRcpFrameOpt,
    vec4 fxaaConsoleRcpFrameOpt2,
    vec4 fxaaConsole360RcpFrameOpt2,
    float fxaaQualitySubpix,
    float fxaaQualityEdgeThreshold,
    float fxaaQualityEdgeThresholdMin,
    float fxaaConsoleEdgeSharpness,
    float fxaaConsoleEdgeThreshold,
    float fxaaConsoleEdgeThresholdMin,
    vec4 fxaaConsole360ConstDir
) {
#define TexOfs(ofs) qf_texture(tex, pos + (ofs)).rgb
#define TexOfsInv(ofs) qf_texture(tex, pos + (ofs) * fxaaQualityRcpFrame).rgb

	const vec3 luma = vec3(0.299, 0.587, 0.114);
	vec4 lumas = vec4(
		dot(TexOfsInv(vec2(-1.0, -1.0)), luma),
		dot(TexOfsInv(vec2(1.0, -1.0)), luma),
		dot(TexOfsInv(vec2(-1.0, 1.0)), luma),
		dot(TexOfsInv(vec2(1.0, 1.0)), luma));
	float lumaM = dot(qf_texture(tex, pos).rgb, luma);

	float lumaMin = min(lumaM, min(min(lumas.x, lumas.y), min(lumas.z, lumas.w)));
	float lumaMax = max(lumaM, max(max(lumas.x, lumas.y), max(lumas.z, lumas.w)));

	vec2 dir = vec2(-((lumas.x + lumas.y) - (lumas.z + lumas.w)), (lumas.x + lumas.z) - (lumas.y + lumas.w));
	float dirMin = min(abs(dir.x), abs(dir.y)) + max(dot(lumas, vec4(0.25 * FXAA_REDUCE_MUL)), FXAA_REDUCE_MIN);
	dir = min(vec2(FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX), dir / dirMin)) * fxaaQualityRcpFrame;

	vec3 rgbA = 0.5 * (TexOfs(dir * (1.0 / 3.0 - 0.5)) + TexOfs(dir * (2.0 / 3.0 - 0.5)));
	vec3 rgbB = rgbA * 0.5 + 0.25 * (TexOfs(dir * -0.5) + TexOfs(dir * 0.5));
	float lumaB = dot(rgbB, luma);

	return vec4(mix(rgbA, rgbB, min(step(lumaMin, lumaB), step(lumaB, lumaMax))), 1.0);
}
