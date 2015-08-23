#define DRAWFLAT_NORMAL_STEP	0.5		// floor or ceiling if < abs(normal.z)

#if defined(APPLY_FOG_COLOR)
#define APPLY_ENV_MODULATE_COLOR
#else

#if defined(APPLY_RGB_DISTANCERAMP) || defined(APPLY_RGB_CONST) || defined(APPLY_RGB_VERTEX) || defined(APPLY_RGB_ONE_MINUS_VERTEX) || defined(APPLY_RGB_GEN_DIFFUSELIGHT)
#define APPLY_ENV_MODULATE_COLOR
#else

#if defined(APPLY_ALPHA_DISTANCERAMP) || defined(APPLY_ALPHA_CONST) || defined(APPLY_ALPHA_VERTEX) || defined(APPLY_ALPHA_ONE_MINUS_VERTEX)
#define APPLY_ENV_MODULATE_COLOR
#endif

#endif

#endif
