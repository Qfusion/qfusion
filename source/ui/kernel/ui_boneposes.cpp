/*
 * UI_Boneposes.cpp
 *
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "ui_boneposes.h"

namespace WSWUI
{

UI_BonePoses::UI_BonePoses()
{
	TBC_Size = TBC_Block_Size;
	TBC = (bonepose_t *)__operator_new__( sizeof( bonepose_t ) * TBC_Size );
	TBC_Count = 0;
	skel_headnode = NULL;
}

UI_BonePoses::~UI_BonePoses()
{
	cgs_skeleton_t *skel, *next;

	for( skel = skel_headnode; skel; skel = next ) {
		next = skel->next;
		__operator_delete__(skel);
	}

	__operator_delete__(TBC);
}

cgs_skeleton_t *UI_BonePoses::SkeletonForModel( struct model_s *model )
{
	int i, j;
	cgs_skeleton_t *skel;
	uint8_t *buffer;
	cgs_bone_t *bone;
	bonepose_t *bonePose;
	int numBones, numFrames;

	if( !model )
		return NULL;

	numBones = trap::R_SkeletalGetNumBones( model, &numFrames );
	if( !numBones || !numFrames )
		return NULL; // no bones or frames

	for( skel = skel_headnode; skel; skel = skel->next )
	{
		if( skel->model == model )
			return skel;
	}

	// allocate one huge array to hold our data
	buffer = (uint8_t *)__operator_new__( sizeof( cgs_skeleton_t ) + numBones * sizeof( cgs_bone_t ) +
		numFrames * ( sizeof( bonepose_t * ) + numBones * sizeof( bonepose_t ) ) );

	skel = ( cgs_skeleton_t * )buffer; buffer += sizeof( cgs_skeleton_t );
	skel->bones = ( cgs_bone_t * )buffer; buffer += numBones * sizeof( cgs_bone_t );
	skel->numBones = numBones;
	skel->bonePoses = ( bonepose_t ** )buffer; buffer += numFrames * sizeof( bonepose_t * );
	skel->numFrames = numFrames;
	// register bones
	for( i = 0, bone = skel->bones; i < numBones; i++, bone++ )
		bone->parent = trap::R_SkeletalGetBoneInfo( model, i, bone->name, sizeof( bone->name ), &bone->flags );

	// register poses for all frames for all bones
	for( i = 0; i < numFrames; i++ )
	{
		skel->bonePoses[i] = ( bonepose_t * )buffer; buffer += numBones * sizeof( bonepose_t );
		for( j = 0, bonePose = skel->bonePoses[i]; j < numBones; j++, bonePose++ )
			trap::R_SkeletalGetBonePose( model, j, i, bonePose );
	}

	skel->next = skel_headnode;
	skel_headnode = skel;

	skel->model = model;

	return skel;
}

/*
* ExpandTemporaryBoneposesCache
* allocate more space for temporary boneposes
*/
void UI_BonePoses::ExpandTemporaryBoneposesCache( void )
{
	bonepose_t *temp;

	temp = TBC;

	TBC = (bonepose_t *)__operator_new__( sizeof( bonepose_t ) * ( TBC_Size + TBC_Block_Size ) );
	memcpy( TBC, temp, sizeof( bonepose_t ) * TBC_Size );
	TBC_Size += TBC_Block_Size;

	__operator_delete__( temp );
}

/*
* RegisterTemporaryExternalBoneposes
* These boneposes are REMOVED EACH FRAME after drawing. Register
* here only in the case you create an entity which is not UI_entity.
*/
bonepose_t *UI_BonePoses::RegisterTemporaryExternalBoneposes( cgs_skeleton_t *skel, bonepose_t *poses )
{
	bonepose_t *boneposes;
	if( ( TBC_Count + skel->numBones ) > TBC_Size )
		ExpandTemporaryBoneposesCache();

	boneposes = &TBC[TBC_Count];
	TBC_Count += skel->numBones;

	return boneposes;
}

/*
* UI_TransformBoneposes
* place bones in it's final position in the skeleton
*/
void UI_BonePoses::TransformBoneposes( cgs_skeleton_t *skel, bonepose_t *outboneposes, bonepose_t *sourceboneposes )
{
	int j;
	bonepose_t temppose;

	for( j = 0; j < (int)skel->numBones; j++ )
	{
		if( skel->bones[j].parent >= 0 )
		{
			memcpy( &temppose, &sourceboneposes[j], sizeof( bonepose_t ) );
			DualQuat_Multiply( outboneposes[skel->bones[j].parent].dualquat, temppose.dualquat, outboneposes[j].dualquat );
		}
		else
			memcpy( &outboneposes[j], &sourceboneposes[j], sizeof( bonepose_t ) );
	}
}

/*
* UI_SetBoneposesForTemporaryEntity
* Sets up skeleton with inline boneposes based on frame/oldframe values
* These boneposes will be REMOVED EACH FRAME. Use only for temporary entities,
* UI_entities have a persistant registration method available.
*/
cgs_skeleton_t *UI_BonePoses::SetBoneposesForTemporaryEntity( entity_t *ent )
{
	cgs_skeleton_t *skel;

	skel = SkeletonForModel( ent->model );
	if( skel )
	{
		if( ent->frame >= skel->numFrames )
			ent->frame = 0;
		if( ent->oldframe >= skel->numFrames )
			ent->oldframe = 0;

		ent->boneposes = RegisterTemporaryExternalBoneposes( skel, ent->boneposes );
		TransformBoneposes( skel, ent->boneposes, skel->bonePoses[ent->frame] );
		ent->oldboneposes = RegisterTemporaryExternalBoneposes( skel, ent->oldboneposes );
		TransformBoneposes( skel, ent->oldboneposes, skel->bonePoses[ent->oldframe] );
	}

	return skel;
}

void UI_BonePoses::ResetTemporaryBoneposesCache( void )
{
	TBC_Count = 0;
}

}