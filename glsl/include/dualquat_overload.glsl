#ifdef DUAL_QUAT_TRANSFORM_TANGENT
void VertexDualQuatsTransform(inout vec4 Position, inout vec3 Normal, inout vec3 Tangent)
#else
void VertexDualQuatsTransform(inout vec4 Position, inout vec3 Normal)
#endif
{
	ivec4 Indices = ivec4(a_BonesIndices * 2.0);
	vec4 DQReal = u_DualQuats[Indices.x];
	vec4 DQDual = u_DualQuats[Indices.x + 1];
#if NUM_BONE_INFLUENCES >= 2
	DQReal *= a_BonesWeights.x;
	DQDual *= a_BonesWeights.x;
	vec4 DQReal1 = u_DualQuats[Indices.y];
	vec4 DQDual1 = u_DualQuats[Indices.y + 1];
	float Scale = mix(-1.0, 1.0, step(0.0, dot(DQReal1, DQReal))) * a_BonesWeights.y;
	DQReal += DQReal1 * Scale;
	DQDual += DQDual1 * Scale;
#if NUM_BONE_INFLUENCES >= 3
	DQReal1 = u_DualQuats[Indices.z];
	DQDual1 = u_DualQuats[Indices.z + 1];
	Scale = mix(-1.0, 1.0, step(0.0, dot(DQReal1, DQReal))) * a_BonesWeights.z;
	DQReal += DQReal1 * Scale;
	DQDual += DQDual1 * Scale;
#if NUM_BONE_INFLUENCES >= 4
	DQReal1 = u_DualQuats[Indices.w];
	DQDual1 = u_DualQuats[Indices.w + 1];
	Scale = mix(-1.0, 1.0, step(0.0, dot(DQReal1, DQReal))) * a_BonesWeights.w;
	DQReal += DQReal1 * Scale;
	DQDual += DQDual1 * Scale;
#endif // NUM_BONE_INFLUENCES >= 4
#endif // NUM_BONE_INFLUENCES >= 3
#endif // NUM_BONE_INFLUENCES >= 2
	float Len = 1.0 / length(DQReal);
	DQReal *= Len;
	DQDual *= Len;
	Position.xyz += (cross(DQReal.xyz, cross(DQReal.xyz, Position.xyz) + Position.xyz * DQReal.w + DQDual.xyz) +
		DQDual.xyz*DQReal.w - DQReal.xyz*DQDual.w) * 2.0;
	Normal += cross(DQReal.xyz, cross(DQReal.xyz, Normal) + Normal * DQReal.w) * 2.0;
#ifdef DUAL_QUAT_TRANSFORM_TANGENT
	Tangent += cross(DQReal.xyz, cross(DQReal.xyz, Tangent) + Tangent * DQReal.w) * 2.0;
#endif
}
