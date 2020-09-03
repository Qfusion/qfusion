/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "cg_local.h"



//========================================================================
//
//				SKELETONS
//
//========================================================================

cgs_skeleton_t *skel_headnode;

/*
* CG_CreateBonesTreeNode
* Find out the original tree
*/
static bonenode_t *CG_CreateBonesTreeNode( cgs_skeleton_t *skel, int bone ) {
	int i, count;
	int children[SKM_MAX_BONES];
	bonenode_t *bonenode;

	bonenode = (bonenode_t *)CG_Malloc( sizeof( bonenode_t ) );
	bonenode->bonenum = bone;
	if( bone != -1 ) {
		skel->bones[bone].node = bonenode; // store a pointer in the linear array for fast first access.
	}

	// find childs of this bone
	count = 0;
	for( i = 0; i < skel->numBones; i++ ) {
		if( skel->bones[i].parent == bone ) {
			children[count] = i;
			count++;
		}
	}

	bonenode->numbonechildren = count;
	if( bonenode->numbonechildren ) {
		bonenode->bonechildren = ( bonenode_t ** )CG_Malloc( sizeof( bonenode_t * ) * bonenode->numbonechildren );
		for( i = 0; i < bonenode->numbonechildren; i++ ) {
			bonenode->bonechildren[i] = CG_CreateBonesTreeNode( skel, children[i] );
		}
	}

	return bonenode;
}

/*
* CG_SkeletonForModel
*/
cgs_skeleton_t *CG_SkeletonForModel( struct model_s *model ) {
	int i, j;
	cgs_skeleton_t *skel;
	uint8_t *buffer;
	cgs_bone_t *bone;
	bonepose_t *bonePose;
	int numBones, numFrames;

	if( !model ) {
		return NULL;
	}

	numBones = trap_R_SkeletalGetNumBones( model, &numFrames );
	if( !numBones || !numFrames ) {
		return NULL; // no bones or frames

	}
	for( skel = skel_headnode; skel; skel = skel->next ) {
		if( skel->model == model ) {
			return skel;
		}
	}

	// allocate one huge array to hold our data
	buffer = (uint8_t *)CG_Malloc( sizeof( cgs_skeleton_t ) + numBones * sizeof( cgs_bone_t ) +
								   numFrames * ( sizeof( bonepose_t * ) + numBones * sizeof( bonepose_t ) ) );

	skel = ( cgs_skeleton_t * )buffer; buffer += sizeof( cgs_skeleton_t );
	skel->bones = ( cgs_bone_t * )buffer; buffer += numBones * sizeof( cgs_bone_t );
	skel->numBones = numBones;
	skel->bonePoses = ( bonepose_t ** )buffer; buffer += numFrames * sizeof( bonepose_t * );
	skel->numFrames = numFrames;

	// register bones
	for( i = 0, bone = skel->bones; i < numBones; i++, bone++ )
		bone->parent = trap_R_SkeletalGetBoneInfo( model, i, bone->name, sizeof( bone->name ), &bone->flags );

	// register poses for all frames for all bones
	for( i = 0; i < numFrames; i++ ) {
		skel->bonePoses[i] = ( bonepose_t * )buffer; buffer += numBones * sizeof( bonepose_t );
		for( j = 0, bonePose = skel->bonePoses[i]; j < numBones; j++, bonePose++ )
			trap_R_SkeletalGetBonePose( model, j, i, bonePose );
	}

	skel->next = skel_headnode;
	skel_headnode = skel;
	skel->model = model;

	// create a bones tree that can be run from parent to children
	skel->bonetree = CG_CreateBonesTreeNode( skel, -1 );
#ifdef SKEL_PRINTBONETREE
	CG_PrintBoneTree( skel, skel->bonetree, 1 );
#endif

	return skel;
}

//========================================================================
//
//				BONEPOSES
//
//========================================================================

/*
* CG_BoneNodeFromNum
*/
static bonenode_t *CG_BoneNodeFromNum( cgs_skeleton_t *skel, int bonenum ) {
	if( bonenum < 0 || bonenum >= skel->numBones ) {
		return skel->bonetree;
	}
	return skel->bones[bonenum].node;
}

static void CG_RecurseBlendSkeletalBone_r( bonepose_t *inboneposes, bonepose_t *outboneposes, bonenode_t *bonenode, float frac ) {
	int i;
	bonepose_t *inbone, *outbone;

	if( bonenode->bonenum != -1 ) {
		inbone = inboneposes + bonenode->bonenum;
		outbone = outboneposes + bonenode->bonenum;
		if( frac == 1 ) {
			memcpy( &outboneposes[bonenode->bonenum], &inboneposes[bonenode->bonenum], sizeof( bonepose_t ) );
		} else {
			// blend current node pose
			DualQuat_Lerp( inbone->dualquat, outbone->dualquat, frac, outbone->dualquat );
		}
	}

	for( i = 0; i < bonenode->numbonechildren; i++ ) {
		if( bonenode->bonechildren[i] ) {
			CG_RecurseBlendSkeletalBone_r( inboneposes, outboneposes, bonenode->bonechildren[i], frac );
		}
	}
}

/*
 * CG_RecurseBlendSkeletalBone
 * Combine 2 different poses in one from a given root bone
 */
void CG_RecurseBlendSkeletalBone( cgs_skeleton_t *skel, bonepose_t *inboneposes, bonepose_t *outboneposes, int root, float frac )
{
	bonenode_t *node = CG_BoneNodeFromNum( skel, root );
	CG_RecurseBlendSkeletalBone_r( inboneposes, outboneposes, node, frac );
}

/*
* CG_TransformBoneposes
* Transform boneposes to parent bone space (mount the skeleton)
*/
void CG_TransformBoneposes( cgs_skeleton_t *skel, bonepose_t *outboneposes, bonepose_t *sourceboneposes ) {
	int j;
	bonepose_t temppose;

	for( j = 0; j < (int)skel->numBones; j++ ) {
		if( skel->bones[j].parent >= 0 ) {
			memcpy( &temppose, &sourceboneposes[j], sizeof( bonepose_t ) );
			DualQuat_Multiply( outboneposes[skel->bones[j].parent].dualquat, temppose.dualquat, outboneposes[j].dualquat );
		} else if( outboneposes != sourceboneposes ) {
			memcpy( &outboneposes[j], &sourceboneposes[j], sizeof( bonepose_t ) );
		}
	}
}

/*
* CG_LerpBoneposes
* Interpolate between 2 poses. It doesn't matter where they come
* from nor if they are previously transformed or not
*/
bool CG_LerpBoneposes( cgs_skeleton_t *skel, bonepose_t *curboneposes, bonepose_t *oldboneposes, bonepose_t *outboneposes, float frontlerp ) {
	int i;

	assert( curboneposes && oldboneposes && outboneposes );
	assert( skel && skel->numBones && skel->numFrames );

	if( frontlerp == 1 ) {
		memcpy( outboneposes, curboneposes, sizeof( bonepose_t ) * skel->numBones );
	} else if( frontlerp == 0 ) {
		memcpy( outboneposes, oldboneposes, sizeof( bonepose_t ) * skel->numBones );
	} else {
		// lerp all bone poses
		for( i = 0; i < (int)skel->numBones; i++, curboneposes++, oldboneposes++, outboneposes++ ) {
			DualQuat_Lerp( oldboneposes->dualquat, curboneposes->dualquat, frontlerp, outboneposes->dualquat );
		}
	}

	return true;
}

/*
* CG_LerpSkeletonPoses
* Interpolate between 2 frame poses in a skeleton
*/
bool CG_LerpSkeletonPoses( cgs_skeleton_t *skel, int curframe, int oldframe, bonepose_t *outboneposes, float frontlerp ) {
	if( !skel ) {
		return false;
	}

	if( curframe >= skel->numFrames || curframe < 0 ) {
		CG_Printf( S_COLOR_YELLOW "CG_LerpSkeletonPoses: out of bounds frame: %i [%i]\n", curframe, skel->numFrames );
		curframe = 0;
	}

	if( oldframe >= skel->numFrames || oldframe < 0 ) {
		CG_Printf( S_COLOR_YELLOW "CG_LerpSkeletonPoses: out of bounds oldframe: %i [%i]\n", oldframe, skel->numFrames );
		oldframe = 0;
	}

	if( curframe == oldframe ) {
		memcpy( outboneposes, skel->bonePoses[curframe], sizeof( bonepose_t ) * skel->numBones );
		return true;
	}

	return CG_LerpBoneposes( skel, skel->bonePoses[curframe], skel->bonePoses[oldframe], outboneposes, frontlerp );
}

/*
* CG_RotateBonePose
*/
void CG_RotateBonePose( const vec3_t angles, bonepose_t *outboneposes, int rotator )
{
	dualquat_t quat_rotator;
	bonepose_t temppose;
	vec3_t tempangles;
	bonepose_t *bonepose = outboneposes + rotator;

	tempangles[0] = -angles[YAW];
	tempangles[1] = -angles[PITCH];
	tempangles[2] = -angles[ROLL];

	DualQuat_FromAnglesAndVector( tempangles, vec3_origin, quat_rotator );

	memcpy( &temppose, bonepose, sizeof( bonepose_t ) );

	DualQuat_Multiply( quat_rotator, temppose.dualquat, bonepose->dualquat );
}

/*
 * CG_RotateBonePoses
 */
void CG_RotateBonePoses( const const vec3_t angles, bonepose_t *outboneposes, int *rotators, int numRotators )
{
	dualquat_t	quat_rotator;
	vec3_t		tempangles;

	if( numRotators == 0 ) {
		return;
	}

	float scale = 1.0f / (float)numRotators;
	tempangles[0] = -angles[YAW] * scale;
	tempangles[1] = -angles[PITCH] * scale;
	tempangles[2] = -angles[ROLL] * scale;

	DualQuat_FromAnglesAndVector( tempangles, vec3_origin, quat_rotator );

	for( int i = 0; i < numRotators; i++ ) {
		int			rotator = rotators[i];
		bonepose_t *bonepose = outboneposes + rotator;
		bonepose_t	temppose = *bonepose;

		DualQuat_Multiply( quat_rotator, temppose.dualquat, bonepose->dualquat );
	}
}

/*
* CG_TagMask
* Use alternative names for tag bones
*/
static cg_tagmask_t *CG_TagMask( const char *maskname, cgs_skeleton_t *skel ) {
	cg_tagmask_t *tagmask;

	if( !skel ) {
		return NULL;
	}

	for( tagmask = skel->tagmasks; tagmask; tagmask = tagmask->next ) {
		if( !Q_stricmp( tagmask->tagname, maskname ) ) {
			return tagmask;
		}
	}

	return NULL;
}

/*
* CG_SkeletalPoseGetAttachment
* Get the tag from the interpolated and transformed pose
*/
bool CG_SkeletalPoseGetAttachment( orientation_t *orient, cgs_skeleton_t *skel,
								   bonepose_t *boneposes, const char *bonename ) {
	int i;
	quat_t quat;
	cgs_bone_t *bone;
	bonepose_t *bonepose;
	cg_tagmask_t *tagmask;

	if( !boneposes || !skel ) {
		CG_Printf( "CG_SkeletalPoseLerpAttachment: Wrong model or boneposes %s\n", bonename );
		return false;
	}

	tagmask = CG_TagMask( bonename, skel );

	// find the appropriate attachment bone
	if( tagmask ) {
		bone = skel->bones;
		for( i = 0; i < skel->numBones; i++, bone++ ) {
			if( !Q_stricmp( bone->name, tagmask->bonename ) ) {
				break;
			}
		}
	} else {
		bone = skel->bones;
		for( i = 0; i < skel->numBones; i++, bone++ ) {
			if( !Q_stricmp( bone->name, bonename ) ) {
				break;
			}
		}
	}

	if( i == skel->numBones ) {
		//if( developer && developer->integer )
		//	CG_Printf( S_COLOR_YELLOW "CG_SkeletalPoseLerpAttachment: no such bone %s\n", bonename );
		return false;
	}

	// get the desired bone
	bonepose = boneposes + i;

	// copy the inverted bone into the tag
	Quat_Inverse( &bonepose->dualquat[0], quat ); // inverse the tag direction
	Quat_ToMatrix3( quat, orient->axis );
	DualQuat_GetVector( bonepose->dualquat, orient->origin );

	// normalize each axis
	Matrix3_Normalize( orient->axis );

	// do the offseting if having a tagmask
	if( tagmask ) {
		// we want to place a rotated model over this tag, not to rotate the tag,
		// because all rotations would move. So we create a new orientation for the
		// model and we position the new orientation in tag space
		if( tagmask->rotate[YAW] || tagmask->rotate[PITCH] || tagmask->rotate[ROLL] ) {
			orientation_t modOrient, newOrient;

			VectorCopy( tagmask->offset, modOrient.origin );
			AnglesToAxis( tagmask->rotate, modOrient.axis );

			VectorCopy( vec3_origin, newOrient.origin );
			Matrix3_Identity( newOrient.axis );

			CG_MoveToTag( newOrient.origin, newOrient.axis,
						  orient->origin, orient->axis,
						  modOrient.origin, modOrient.axis
						  );

			Matrix3_Copy( newOrient.axis, orient->axis );
			VectorCopy( newOrient.origin, orient->origin );
		} else {
			// offset
			for( i = 0; i < 3; i++ ) {
				if( tagmask->offset[i] ) {
					VectorMA( orient->origin, tagmask->offset[i], &orient->axis[i * 3], orient->origin );
				}
			}
		}
	}

	return true;
}


//========================================================================
//
//		TMP BONEPOSES
//
//========================================================================

#define TBC_Block_Size      1024
static int TBC_Size;

bonepose_t *TBC;        //Temporary Boneposes Cache
static int TBC_Count;


/*
* CG_InitTemporaryBoneposesCache
* allocate space for temporary boneposes
*/
void CG_InitTemporaryBoneposesCache( void ) {
	TBC_Size = TBC_Block_Size;
	TBC = ( bonepose_t * )CG_Malloc( sizeof( bonepose_t ) * TBC_Size );
	TBC_Count = 0;
}

/*
* CG_ExpandTemporaryBoneposesCache
*/
static void CG_ExpandTemporaryBoneposesCache( int num ) {
	bonepose_t *temp;

	temp = TBC;

	TBC = ( bonepose_t * )CG_Malloc( sizeof( bonepose_t ) * ( TBC_Size + fmax( num, TBC_Block_Size ) ) );
	memcpy( TBC, temp, sizeof( bonepose_t ) * TBC_Size );
	TBC_Size += fmax( num, TBC_Block_Size );

	CG_Free( temp );
}

/*
* CG_ResetTemporaryBoneposesCache
*/
void CG_ResetTemporaryBoneposesCache( void ) {
	TBC_Count = 0;
}

/*
 * CG_RegisterTemporaryExternalBoneposes
 * These boneposes are RESET after drawing EACH FRAME
 */
bonepose_t *CG_RegisterTemporaryExternalBoneposes( cgs_skeleton_t *skel )
{
	return CG_RegisterTemporaryExternalBoneposes2( skel->numBones );
}

/*
* CG_RegisterTemporaryExternalBoneposes
* These boneposes are RESET after drawing EACH FRAME
*/
bonepose_t *CG_RegisterTemporaryExternalBoneposes2( int numBones ) {
	bonepose_t *boneposes;
	if( ( TBC_Count + numBones ) > TBC_Size ) {
		CG_ExpandTemporaryBoneposesCache( numBones );
	}

	boneposes = &TBC[TBC_Count];
	TBC_Count += numBones;

	return boneposes;
}

/*
* CG_SetBoneposesForTemporaryEntity
* Sets up skeleton with inline boneposes based on frame/oldframe values
* These boneposes will be RESET after drawing EACH FRAME.
*/
cgs_skeleton_t *CG_SetBoneposesForTemporaryEntity( entity_t *ent ) {
	cgs_skeleton_t *skel;

	skel = CG_SkeletonForModel( ent->model );
	if( skel ) {
		// get space in cache, interpolate, transform, link
		ent->boneposes = CG_RegisterTemporaryExternalBoneposes( skel );
		CG_LerpSkeletonPoses( skel, ent->frame, ent->oldframe, ent->boneposes, 1.0 - ent->backlerp );
		CG_TransformBoneposes( skel, ent->boneposes, ent->boneposes );
		ent->oldboneposes = ent->boneposes;
	}

	return skel;
}

/*
* CG_FreeTemporaryBoneposesCache
*/
void CG_FreeTemporaryBoneposesCache( void ) {
	CG_Free( TBC );
	TBC_Size = 0;
	TBC_Count = 0;
}
