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
#define Tex() qf_texture(tex, pos).rgb
#define TexOfs(ofs) qf_texture(tex, pos + ofs).rgb
#define TexOfsInv(ofs) qf_texture(tex, pos + ofs * fxaaQualityRcpFrame).rgb

    vec3 rgbNW = TexOfsInv(vec2(-1.0,-1.0));
    vec3 rgbNE = TexOfsInv(vec2(1.0,-1.0));
    vec3 rgbSW = TexOfsInv(vec2(-1.0,1.0));
    vec3 rgbSE = TexOfsInv(vec2(1.0,1.0));
    vec3 rgbM = Tex();

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2( FXAA_SPAN_MAX, FXAA_SPAN_MAX),
         max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
         dir * rcpDirMin)) * fxaaQualityRcpFrame;

    vec3 rgbA = (1.0/2.0) * (
        TexOfs(dir * (1.0/3.0 - 0.5)) +
        TexOfs(dir * (2.0/3.0 - 0.5)));
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        TexOfs(dir * (0.0/3.0 - 0.5)) +
        TexOfs(dir * (3.0/3.0 - 0.5)));
    float lumaB = dot(rgbB, luma);

    if((lumaB < lumaMin) || (lumaB > lumaMax)){
            return vec4(rgbA, 1.0);
    }else{
            return vec4(rgbB, 1.0);
    }
}
