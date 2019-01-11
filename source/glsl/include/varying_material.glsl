qf_varying vec2 v_TexCoord;

qf_varying vec3 v_Position;

#if defined(APPLY_SPECULAR)
qf_varying vec3 v_EyeVector;
#endif

qf_varying mat3 v_StrMatrix; // directions of S/T/R texcoords (tangent, binormal, normal)
