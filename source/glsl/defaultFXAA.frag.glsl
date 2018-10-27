#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;

uniform sampler2D u_BaseTexture;

#ifdef APPLY_FXAA3

#define FXAA_PC 1
#if (!defined(GL_ES) && (QF_GLSL_VERSION >= 130)) || (defined(GL_ES) && (QF_GLSL_VERSION >= 300))
# define FXAA_GLSL_130 1
#else
# define FXAA_GLSL_120 1
#endif
#define FXAA_GREEN_AS_LUMA 1
#define FXAA_QUALITY_X_PRESET 23
#define FXAA_GATHER4_ALPHA 1
#include "include/Fxaa3_11.h"

#else

#include "include/Fxaa2.h"

#endif

void main(void)
{
    // Only used on FXAA Quality.
    // Choose the amount of sub-pixel aliasing removal.
    // This can effect sharpness.
    //   1.00 - upper limit (softer)
    //   0.75 - default amount of filtering
    //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
    //   0.25 - almost off
    //   0.00 - completely off
    float QualitySubpix = 0.75;

    // The minimum amount of local contrast required to apply algorithm.
    //   0.333 - too little (faster)
    //   0.250 - low quality
    //   0.166 - default
    //   0.125 - high quality 
    //   0.033 - very high quality (slower)
    float QualityEdgeThreshold = 0.166;
    float QualityEdgeThresholdMin = 0.0;

    vec4 ConsolePosPos = vec4(0.0,0.0,0.0,0.0);
    vec4 ConsoleRcpFrameOpt = vec4(0.0,0.0,0.0,0.0);
    vec4 ConsoleRcpFrameOpt2 = vec4(0.0,0.0,0.0,0.0);
    vec4 Console360RcpFrameOpt2 = vec4(0.0,0.0,0.0,0.0);
    float ConsoleEdgeSharpness = 8.0;
    float ConsoleEdgeThreshold = 0.125;
    float ConsoleEdgeThresholdMin = 0.05;
    vec4  Console360ConstDir = vec4(1.0, -1.0, 0.25, -0.25);

    qf_FragColor = FxaaPixelShader(v_TexCoord, ConsolePosPos, u_BaseTexture, u_BaseTexture, u_BaseTexture, 
        u_TextureParams.zw, ConsoleRcpFrameOpt, ConsoleRcpFrameOpt2, Console360RcpFrameOpt2, 
        QualitySubpix, QualityEdgeThreshold, QualityEdgeThresholdMin, ConsoleEdgeSharpness, 
        ConsoleEdgeThreshold, ConsoleEdgeThresholdMin, Console360ConstDir);
}
