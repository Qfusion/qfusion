#ifdef LIGHTMAP_ARRAYS
#define LightmapSampler sampler2DArray
#define Lightmap(t, c, l) qf_textureArray(t, vec3(c, l))
#else
#define LightmapSampler sampler2D
#define Lightmap(t, c, l) qf_texture(t, c)
#endif
