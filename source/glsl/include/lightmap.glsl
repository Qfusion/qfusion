#ifdef LIGHTMAP_ARRAYS
#define LightmapSampler sampler2DArray
#define LightmapAt(t, c, l) qf_textureArray(t, vec3(c, l))
#else
#define LightmapSampler sampler2D
#define LightmapAt(t, c, l) qf_texture(t, c)
#endif

#ifdef APPLY_SRGB2LINEAR
# define Lightmap(t, c, l) LinearFromsRGB(myhalf3(LightmapAt(t, c, l)))
#else
# define Lightmap(t, c, l) myhalf3(LightmapAt(t, c, l))
#endif

#define Deluxemap(t, c, l) normalize(myhalf3(LightmapAt(t, c, l)) - myhalf3(0.5))
