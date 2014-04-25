#ifdef VERTEX_SHADER

qf_attribute vec4 a_Position;
qf_attribute vec4 a_SVector;
qf_attribute vec4 a_Normal;
qf_attribute vec4 a_Color;
qf_attribute vec2 a_TexCoord;
# ifdef NUM_LIGHTMAPS
qf_attribute qf_lmvec01 a_LightmapCoord01;
# if NUM_LIGHTMAPS > 2
qf_attribute qf_lmvec23 a_LightmapCoord23;
# endif // NUM_LIGHTMAPS > 2
#endif // NUM_LIGHTMAPS

#endif // VERTEX_SHADER
