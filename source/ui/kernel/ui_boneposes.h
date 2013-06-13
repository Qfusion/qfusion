/*
 * UI_Boneposes.h
 *
 */

#ifndef UI_BONEPOSES_H_
#define UI_BONEPOSES_H_

namespace WSWUI
{

#define TBC_Block_Size			1024

typedef struct
{
	char name[MAX_QPATH];
	int flags;
	int parent;
} cgs_bone_t;

typedef struct cgs_skeleton_s
{
	struct model_s *model;

	int numBones;
	cgs_bone_t *bones;

	int numFrames;
	bonepose_t **bonePoses;

	struct cgs_skeleton_s *next;
} cgs_skeleton_t;

// just testing stuff
class UI_BonePoses
{
protected:
	int TBC_Size;
	int TBC_Count;
	bonepose_t *TBC;        //Temporary Boneposes Cache
	cgs_skeleton_t *skel_headnode;

public:

	UI_BonePoses();
	~UI_BonePoses();

	cgs_skeleton_t *SkeletonForModel( struct model_s *model );

	/*
	* ExpandTemporaryBoneposesCache
	* allocate more space for temporary boneposes
	*/
	void ExpandTemporaryBoneposesCache( void );

	/*
	* RegisterTemporaryExternalBoneposes
	* These boneposes are REMOVED EACH FRAME after drawing. Register
	* here only in the case you create an entity which is not UI_entity.
	*/
	bonepose_t *RegisterTemporaryExternalBoneposes( cgs_skeleton_t *skel, bonepose_t *poses );

	/*
	* UI_TransformBoneposes
	* place bones in it's final position in the skeleton
	*/
	void TransformBoneposes( cgs_skeleton_t *skel, bonepose_t *outboneposes, bonepose_t *sourceboneposes );

	/*
	* UI_SetBoneposesForTemporaryEntity
	* Sets up skeleton with inline boneposes based on frame/oldframe values
	* These boneposes will be REMOVED EACH FRAME. Use only for temporary entities,
	* UI_entities have a persistant registration method available.
	*/
	cgs_skeleton_t *SetBoneposesForTemporaryEntity( entity_t *ent );

	void ResetTemporaryBoneposesCache( void );
};


}

#endif /* UI_BONEPOSES_H_ */
