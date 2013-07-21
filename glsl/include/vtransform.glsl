void TransformVerts(inout vec4 Position, inout vec3 Normal, inout vec2 TexCoord)
{
#ifdef NUM_BONE_INFLUENCES
	QF_VertexDualQuatsTransform(NUM_BONE_INFLUENCES, Position, Normal);
#endif

#ifdef APPLY_DEFORMVERTS
	QF_DeformVerts(Position, Normal, TexCoord);
#endif

#ifdef APPLY_INSTANCED_TRANSFORMS
	QF_InstancedTransform(Position, Normal);
#endif
}

void TransformVerts(inout vec4 Position, inout vec3 Normal, inout vec3 Tangent, inout vec2 TexCoord)
{
#ifdef NUM_BONE_INFLUENCES
	QF_VertexDualQuatsTransform(NUM_BONE_INFLUENCES, Position, Normal, Tangent);
#endif

#ifdef APPLY_DEFORMVERTS
	QF_DeformVerts(Position, Normal, TexCoord);
#endif

#ifdef APPLY_INSTANCED_TRANSFORMS
	QF_InstancedTransform(Position, Normal);
#endif
}
