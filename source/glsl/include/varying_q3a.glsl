#if defined(APPLY_TC_GEN_REFLECTION)
#define APPLY_CUBEMAP
#elif defined(APPLY_TC_GEN_CELSHADE)
#define APPLY_CUBEMAP_VERTEX
#elif defined(APPLY_TC_GEN_SURROUND)
#define APPLY_SURROUNDMAP
#endif

#if defined(NUM_DLIGHTS) || defined(APPLY_CUBEMAP) || defined(APPLY_SURROUNDMAP)
qf_varying vec3 v_Position;
#endif

#if defined(APPLY_CUBEMAP) || defined(APPLY_DRAWFLAT)
qf_varying myhalf3 v_Normal;
#endif

#if defined(APPLY_CUBEMAP_VERTEX)
qf_varying vec3 v_TexCoord;
#elif !defined(APPLY_CUBEMAP) && !defined(APPLY_SURROUNDMAP)
qf_varying vec2 v_TexCoord;
#endif

#ifdef NUM_LIGHTMAPS
qf_varying qf_lmvec01 v_LightmapTexCoord01;
#if NUM_LIGHTMAPS > 2 
qf_varying qf_lmvec23 v_LightmapTexCoord23;
#endif
#ifdef LIGHTMAP_ARRAYS
qf_flat_varying vec4 v_LightmapLayer0123;
#endif
#endif

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
qf_varying vec2 v_FogCoord;
#endif

#if defined(APPLY_SOFT_PARTICLE)
qf_varying float v_Depth;
#endif
