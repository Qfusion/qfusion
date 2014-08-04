#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/rgbdepth.glsl"

qf_varying float v_Depth;

void main(void)
{
#ifdef APPLY_RGB_SHADOW_16BIT
	qf_FragColor = encodedepthmacro16(v_Depth);
#else
	qf_FragColor = encodedepthmacro(v_Depth);
#endif
}
