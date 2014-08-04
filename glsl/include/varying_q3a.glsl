qf_varying vec3 v_Position;

#ifdef APPLY_DRAWFLAT
qf_varying myhalf v_NormalZ;
#endif

#ifdef APPLY_TC_GEN_REFLECTION
#define APPLY_CUBEMAP
#endif

#ifdef APPLY_CUBEMAP
qf_varying vec3 v_TexCoord;
#else
qf_varying vec2 v_TexCoord;
#endif

#ifdef NUM_LIGHTMAPS
qf_varying qf_lmvec01 v_LightmapTexCoord01;
#if NUM_LIGHTMAPS > 2 
qf_varying qf_lmvec23 v_LightmapTexCoord23;
#endif
#endif

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
qf_varying vec2 v_FogCoord;
#endif

#if defined(APPLY_SOFT_PARTICLE)
qf_varying float v_Depth;
#endif
