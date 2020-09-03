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

// cg_boneposes.h -- definitions for skeletons

//=============================================================================
//
//							BONEPOSES
//
//=============================================================================

void CG_AddEntityToScene( entity_t *ent );
cgs_skeleton_t *CG_SkeletonForModel( struct model_s *model );

bonepose_t *CG_RegisterTemporaryExternalBoneposes( cgs_skeleton_t *skel );
bonepose_t *CG_RegisterTemporaryExternalBoneposes2( int numBones );
cgs_skeleton_t *CG_SetBoneposesForTemporaryEntity( entity_t *ent );
void CG_InitTemporaryBoneposesCache( void );
void CG_ResetTemporaryBoneposesCache( void );
void CG_FreeTemporaryBoneposesCache( void );
void CG_RecurseBlendSkeletalBone( cgs_skeleton_t *skel, bonepose_t *inboneposes, 
	bonepose_t *outboneposes, int root, float frac );
bool CG_LerpBoneposes( cgs_skeleton_t *skel, bonepose_t *curboneposes, bonepose_t *oldboneposes,
					   bonepose_t *outboneposes, float frontlerp );
bool CG_LerpSkeletonPoses( cgs_skeleton_t *skel, int curframe, int oldframe,
						   bonepose_t *outboneposes, float frontlerp );
void CG_TransformBoneposes( cgs_skeleton_t *skel, bonepose_t *boneposes,
							bonepose_t *sourceboneposes );
void CG_RotateBonePose( const vec3_t angles, bonepose_t *outboneposes, int rotator );
void CG_RotateBonePoses( const vec3_t angles, bonepose_t *outboneposes, int *rotators, int numRotators );
bool CG_SkeletalPoseGetAttachment(
	orientation_t *orient, cgs_skeleton_t *skel,
								   bonepose_t *boneposes, const char *bonename );
