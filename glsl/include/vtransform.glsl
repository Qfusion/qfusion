#ifdef NUM_BONE_INFLUENCES

attribute vec4 a_BonesIndices, a_BonesWeights;

uniform vec4 u_DualQuats[MAX_UNIFORM_BONES*2];

#define DUAL_QUAT_TRANSFORM_TANGENT
#include "dualquat_overload.glsl"

#undef DUAL_QUAT_TRANSFORM_TANGENT
#include "dualquat_overload.glsl"

#endif // NUM_BONE_INFLUENCES

void TransformVerts(inout vec4 Position, inout vec3 Normal, inout vec2 TexCoord)
{
#ifdef NUM_BONE_INFLUENCES
	VertexDualQuatsTransform(Position, Normal);
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
	VertexDualQuatsTransform(Position, Normal, Tangent);
#endif

#ifdef APPLY_DEFORMVERTS
	QF_DeformVerts(Position, Normal, TexCoord);
#endif

#ifdef APPLY_INSTANCED_TRANSFORMS
	QF_InstancedTransform(Position, Normal);
#endif
}
