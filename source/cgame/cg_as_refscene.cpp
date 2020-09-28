/*
Copyright (C) 2020 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "cg_as_local.h"

//=======================================================================

static const gs_asEnumVal_t asRenderfxFlagsEnumVals[] = {
	ASLIB_ENUM_VAL( RF_MINLIGHT ),
	ASLIB_ENUM_VAL( RF_FULLBRIGHT ),
	ASLIB_ENUM_VAL( RF_FRAMELERP ),
	ASLIB_ENUM_VAL( RF_NOSHADOW ),
	ASLIB_ENUM_VAL( RF_VIEWERMODEL ),
	ASLIB_ENUM_VAL( RF_WEAPONMODEL ),
	ASLIB_ENUM_VAL( RF_CULLHACK ),
	ASLIB_ENUM_VAL( RF_FORCENOLOD ),
	ASLIB_ENUM_VAL( RF_NOPORTALENTS ),
	ASLIB_ENUM_VAL( RF_ALPHAHACK ),
	ASLIB_ENUM_VAL( RF_GREYSCALE ),
	ASLIB_ENUM_VAL( RF_NODEPTHTEST ),
	ASLIB_ENUM_VAL( RF_NOCOLORWRITE ),

	ASLIB_ENUM_VAL_NULL,
};

static const gs_asEnumVal_t asEntTypeEnumVals[] = {
	ASLIB_ENUM_VAL( RT_MODEL ),
	ASLIB_ENUM_VAL( RT_SPRITE ),
	ASLIB_ENUM_VAL( RT_PORTALSURFACE ),

	ASLIB_ENUM_VAL_NULL,
};

const gs_asEnum_t asCGameRefSceneEnums[] = {
	{ "cg_entrenderfx_e", asRenderfxFlagsEnumVals },
	{ "cg_entreftype_e", asEntTypeEnumVals },

	ASLIB_ENUM_VAL_NULL,
};

//=======================================================================

typedef struct {
	entity_t ent;
	int		 asRefCount;
} asrefentity_t;

static asrefentity_t *objectRefEntity_Factory( void )
{
	asrefentity_t *e = (asrefentity_t *)CG_Malloc( sizeof( asrefentity_t ) );
	e->ent.scale = 1;
	Matrix3_Identity( e->ent.axis );
	Vector4Set( e->ent.shaderRGBA, 255, 255, 255, 255 );
	e->asRefCount = 1;
	return e;
}

static asrefentity_t *objectRefEntity_FactoryCopy( const asrefentity_t *other )
{
	asrefentity_t *e = (asrefentity_t *)CG_Malloc( sizeof( asrefentity_t ) );
	e->ent = other->ent;
	e->asRefCount = 1;
	return e;
}

static void objectRefEntity_Addref( asrefentity_t *e )
{
	e->asRefCount++;
}

void objectRefEntity_Release( asrefentity_t *e )
{
	if( --e->asRefCount <= 0 ) {
		CG_Free( e );
	}
}

static void asrefentity_Reset( asrefentity_t *e )
{
	memset( &e->ent, 0, sizeof( e->ent ) );
	e->ent.scale = 1;
	Matrix3_Identity( e->ent.axis );
	Vector4Set( e->ent.shaderRGBA, 255, 255, 255, 255 );
}

static asrefentity_t *asrefentity_Assign( asrefentity_t *other, asrefentity_t *self )
{
	memcpy( &self->ent, &other->ent, sizeof( entity_t ) );
	return self;
}

static const gs_asBehavior_t asrefentity_ObjectBehaviors[] = {
	{ asBEHAVE_FACTORY, ASLIB_FUNCTION_DECL( Entity @, f, () ), asFUNCTION( objectRefEntity_Factory ),
		asCALL_CDECL },
	{ asBEHAVE_FACTORY, ASLIB_FUNCTION_DECL( Entity @, f, ( const Entity &in ) ),
		asFUNCTION( objectRefEntity_FactoryCopy ), asCALL_CDECL },
	{ asBEHAVE_ADDREF, ASLIB_FUNCTION_DECL( void, f, () ), asFUNCTION( objectRefEntity_Addref ), asCALL_CDECL_OBJLAST },
	{ asBEHAVE_RELEASE, ASLIB_FUNCTION_DECL( void, f, () ), asFUNCTION( objectRefEntity_Release ),
		asCALL_CDECL_OBJLAST },

	ASLIB_BEHAVIOR_NULL,
};

static const gs_asMethod_t asrefentity_Methods[] = {
	{ ASLIB_FUNCTION_DECL( void, reset, () ), asFUNCTION( asrefentity_Reset ),
		asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Entity &, opAssign, ( const Entity &in ) ), asFUNCTION( asrefentity_Assign ),
		asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL,
};

static const gs_asProperty_t asrefentity_Properties[] = {
	{ ASLIB_PROPERTY_DECL( int, rtype ), ASLIB_FOFFSET( asrefentity_t, ent.rtype ) },
	{ ASLIB_PROPERTY_DECL( int, renderfx ), ASLIB_FOFFSET( asrefentity_t, ent.renderfx ) },
	{ ASLIB_PROPERTY_DECL( ModelHandle @, model ), ASLIB_FOFFSET( asrefentity_t, ent.model ) },

	{ ASLIB_PROPERTY_DECL( int, frame ), ASLIB_FOFFSET( asrefentity_t, ent.frame ) },
	{ ASLIB_PROPERTY_DECL( Mat3, axis ), ASLIB_FOFFSET( asrefentity_t, ent.axis ) },
	{ ASLIB_PROPERTY_DECL( Vec3, origin ), ASLIB_FOFFSET( asrefentity_t, ent.origin ) },
	{ ASLIB_PROPERTY_DECL( Vec3, origin2 ), ASLIB_FOFFSET( asrefentity_t, ent.origin2 ) },
	{ ASLIB_PROPERTY_DECL( Vec3, lightingOrigin ), ASLIB_FOFFSET( asrefentity_t, ent.lightingOrigin ) },
	{ ASLIB_PROPERTY_DECL( Boneposes @, boneposes ), ASLIB_FOFFSET( asrefentity_t, ent.boneposes ) },
	{ ASLIB_PROPERTY_DECL( Boneposes @, oldBoneposes ), ASLIB_FOFFSET( asrefentity_t, ent.oldboneposes ) },

	{ ASLIB_PROPERTY_DECL( int, shaderRGBA ), ASLIB_FOFFSET( asrefentity_t, ent.shaderRGBA ) },

	{ ASLIB_PROPERTY_DECL( ShaderHandle @, customShader ), ASLIB_FOFFSET( asrefentity_t, ent.customShader ) },
	{ ASLIB_PROPERTY_DECL( int64, shaderTime ), ASLIB_FOFFSET( asrefentity_t, ent.shaderTime ) },

	{ ASLIB_PROPERTY_DECL( int, oldFrame ), ASLIB_FOFFSET( asrefentity_t, ent.oldframe ) },
	{ ASLIB_PROPERTY_DECL( float, backLerp ), ASLIB_FOFFSET( asrefentity_t, ent.backlerp ) },

	{ ASLIB_PROPERTY_DECL( float, scale ), ASLIB_FOFFSET( asrefentity_t, ent.scale ) },
	{ ASLIB_PROPERTY_DECL( float, radius ), ASLIB_FOFFSET( asrefentity_t, ent.radius ) },
	{ ASLIB_PROPERTY_DECL( float, rotation ), ASLIB_FOFFSET( asrefentity_t, ent.rotation ) },

	{ ASLIB_PROPERTY_DECL( SkinHandle @, customSkin ), ASLIB_FOFFSET( asrefentity_t, ent.customSkin ) },

	ASLIB_PROPERTY_NULL,
};

static const gs_asClassDescriptor_t asRefEntityClassDescriptor = {
	"Entity",					 /* name */
	asOBJ_REF,					 /* object type flags */
	sizeof( asrefentity_t ),	 /* size */
	NULL,						 /* funcdefs */
	asrefentity_ObjectBehaviors, /* object behaviors */
	asrefentity_Methods,		 /* methods */
	asrefentity_Properties,		 /* properties */
	NULL, NULL,					 /* string factory hack */
};

//=======================================================================

static void objectOrientation_DefaultConstructor( orientation_t *o )
{
	memset( o, 0, sizeof( orientation_t ) );
	Matrix3_Identity( o->axis );
}

static void objectOrientation_CopyConstructor( orientation_t *other, orientation_t *o )
{
	memcpy( o, other, sizeof( orientation_t ) );
}

static void objectOrientation_Vec3Mat3Constructor( asvec3_t *v, asmat3_t *m, orientation_t *o )
{
	VectorCopy( v->v, o->origin );
	Matrix3_Copy( m->m, o->axis );
}

static const gs_asBehavior_t asOrientation_ObjectBehaviors[] = {
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, () ), asFUNCTION( objectOrientation_DefaultConstructor ), asCALL_CDECL_OBJLAST, },
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( const Orientation &in ) ), asFUNCTION( objectOrientation_CopyConstructor ), asCALL_CDECL_OBJLAST, },
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( const Vec3 &in, const Mat3 &in) ), asFUNCTION( objectOrientation_Vec3Mat3Constructor ), asCALL_CDECL_OBJLAST, },

	ASLIB_BEHAVIOR_NULL,
};

static const gs_asMethod_t asOrientation_Methods[] = { ASLIB_METHOD_NULL };

static const gs_asProperty_t asOrientation_Properties[] = { 
	{ ASLIB_PROPERTY_DECL( Vec3, origin ), ASLIB_FOFFSET( orientation_t, origin ) },
	{ ASLIB_PROPERTY_DECL( Mat3, axis ), ASLIB_FOFFSET( orientation_t, axis ) },
	ASLIB_PROPERTY_NULL,
};

static const gs_asClassDescriptor_t asOrientationClassDescriptor = {
	"Orientation",															 /* name */
	asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS, /* object type flags */
	sizeof( orientation_t ),												 /* size */
	NULL,																	 /* funcdefs */
	asOrientation_ObjectBehaviors,											 /* object behaviors */
	asOrientation_Methods,													 /* methods */
	asOrientation_Properties,												 /* properties */
	NULL, NULL,																 /* string factory hack */
};

static const gs_asClassDescriptor_t asBoneposesClassDescriptor = {
	"Boneposes",			   /* name */
	asOBJ_REF | asOBJ_NOCOUNT, /* object type flags */
	sizeof( bonepose_t * ),	   /* size */
	NULL,					   /* funcdefs */
	NULL,					   /* object behaviors */
	NULL,					   /* methods */
	NULL,					   /* properties */
	NULL, NULL,				   /* string factory hack */
};

//=======================================================================

const gs_asClassDescriptor_t *const asCGameRefSceneClassesDescriptors[] = {
	&asRefEntityClassDescriptor,
	&asOrientationClassDescriptor,
	&asBoneposesClassDescriptor,

	NULL,
};

//=======================================================================

static bool asFunc_CG_GrabTag( orientation_t *tag, entity_t *ent, asstring_t *tagname ) {
	return CG_GrabTag( tag, ent, tagname->buffer );
}

orientation_t asFunc_CG_MoveToTag( orientation_t *space, orientation_t *tag )
{
	orientation_t o;
	CG_MoveToTag( o.origin, o.axis, space->origin, space->axis, tag->origin, tag->axis );
	return o;
}

static void asFunc_CG_SpawnPolyQuad( const asvec3_t *v1, const asvec3_t *v2, const asvec3_t *v3, const asvec3_t *v4,
	float stx, float sty, const asvec4_t *color, int64_t dietime, int64_t fadetime, struct shader_s *shader, int tag ) {
	CG_SpawnPolyQuad( v1->v, v2->v, v3->v, v4->v, stx, sty, color->v, dietime, fadetime, shader, tag );
}

static void asFunc_CG_SpawnPolyBeam( const asvec3_t *start, const asvec3_t *end, const asvec4_t *color, int width,
	int64_t dietime, int64_t fadetime, struct shader_s *shader, int shaderlength, int tag ) {
	CG_SpawnPolyBeam( start->v, end->v, color->v, width, dietime, fadetime, shader, shaderlength, tag );
}

static void asFunc_CG_RotateBonePoses( asvec3_t *angles, bonepose_t *boneposes, CScriptArrayInterface *arr )
{
	if( !arr ) {
		return;
	}

	int numRotators = arr->GetSize();
	if( !numRotators ) {
		return;
	}
	if( numRotators > SKM_MAX_BONES ) {
		return;
	}

	int *rotators = (int *)alloca( numRotators * sizeof( int ) );
	if( rotators == nullptr ) {
		return;
	}

	for( int i = 0; i < numRotators; i++ ) {
		rotators[i] = *( (int *)arr->At( i ) );
	}

	CG_RotateBonePoses( angles->v, boneposes, rotators, numRotators );
}

void asFunc_CG_AddLightToScene( asvec3_t *org, float radius, int color ) {
	CG_AddLightToScene( org->v, radius, 
		( color & 255 ) / 255.0f, 
		( ( color >> 8 ) & 255 ) / 255.0f, 
		( ( color >> 16 ) & 255 ) / 255.0f );
}

const gs_asglobfuncs_t asCGameRefSceneGlobalFuncs[] = {
	{ "void PlaceRotatedModelOnTag( Entity @+ ent, const Entity @+ dest, const Orientation &in )",
		asFUNCTION( CG_PlaceRotatedModelOnTag ), NULL },
	{ "void PlaceModelOnTag( Entity @+ ent, const Entity @+ dest, const Orientation &in )",
		asFUNCTION( CG_PlaceModelOnTag ), NULL },
	{ "bool GrabTag( Orientation &out, const Entity @+ ent, const String &in )", asFUNCTION( asFunc_CG_GrabTag ),
		NULL },
	{ "Orientation MoveToTag( const Orientation &in space, const Orientation &in tag )", asFUNCTION( asFunc_CG_MoveToTag ),
		NULL },

	{ "Boneposes @RegisterTemporaryExternalBoneposes( ModelSkeleton @ )",
		asFUNCTION( CG_RegisterTemporaryExternalBoneposes ), NULL },
	{ "Boneposes @RegisterTemporaryExternalBoneposes( int numBones )",
		asFUNCTION( CG_RegisterTemporaryExternalBoneposes2 ), NULL },
	{ "bool LerpSkeletonPoses( ModelSkeleton @, int frame, int oldFrame, Boneposes @ boneposes, float frac )",
		asFUNCTION( CG_LerpSkeletonPoses ), NULL },
	{ "void TransformBoneposes( ModelSkeleton @, Boneposes @ boneposes, Boneposes @ sourceBoneposes )",
		asFUNCTION( CG_TransformBoneposes ), NULL },
	{ "void RecurseBlendSkeletalBone( ModelSkeleton @, Boneposes @ inBoneposes, Boneposes @ outBoneposes, int root, float frac )",
		asFUNCTION( CG_RecurseBlendSkeletalBone ), NULL },
	{ "void RotateBonePoses( const Vec3 &in angles, Boneposes @ inBoneposes, array<int> @rotators )",
		asFUNCTION( asFunc_CG_RotateBonePoses ), NULL },

	{ "void SpawnPolyQuad( const Vec3 &in, const Vec3 &in, const Vec3 &in, const Vec3 &in, float stx, float sty, " 
	  "const Vec4 &in, int64 dieTime, int64 fadeTime, ShaderHandle @, int tag )",
		asFUNCTION( asFunc_CG_SpawnPolyQuad ), NULL },
	{ "void SpawnPolyBeam( const Vec3 &in start, const Vec3 &in end, const Vec4 &in, int width, int64 dieTime, int64 "
	  "fadeTime, ShaderHandle @, int shaderLength, int tag )",
		asFUNCTION( asFunc_CG_SpawnPolyBeam ), NULL },

	{ "void AddEntityToScene( Entity @+ ent )", asFUNCTION( CG_AddEntityToScene ), NULL },
	{ "void AddLightToScene( const Vec3 &in origin, float radius, int color )", asFUNCTION( asFunc_CG_AddLightToScene ), NULL },

	{ NULL },
};
