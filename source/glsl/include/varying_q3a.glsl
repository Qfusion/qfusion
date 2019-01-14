#if defined(APPLY_TC_GEN_REFLECTION)
#define APPLY_CUBEMAP
#elif defined(APPLY_TC_GEN_SURROUND)
#define APPLY_SURROUNDMAP
#endif

qf_varying vec3 v_Position;

#if defined(APPLY_CUBEMAP) || defined(APPLY_DRAWFLAT)
qf_varying vec3 v_Normal;
#endif

#if defined(APPLY_CUBEMAP_VERTEX)
qf_varying vec3 v_TexCoord;
#elif !defined(APPLY_CUBEMAP) && !defined(APPLY_SURROUNDMAP)
qf_varying vec2 v_TexCoord;
#endif

#if defined(APPLY_SOFT_PARTICLE)
qf_varying float v_Depth;
#endif
