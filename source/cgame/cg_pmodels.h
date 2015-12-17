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
// cg_pmodels.h -- local definitions for pmodels and view weapon

//=============================================================================
//
//							SPLITMODELS
//
//=============================================================================

extern cvar_t *cg_weaponFlashes;
extern cvar_t *cg_gunx;
extern cvar_t *cg_guny;
extern cvar_t *cg_gunz;
extern cvar_t *cg_debugPlayerModels;
extern cvar_t *cg_debugWeaponModels;
extern cvar_t *cg_gunbob;
extern cvar_t *cg_gun_fov;
extern cvar_t *cg_handOffset;

enum
{
	WEAPMODEL_NOANIM,
	WEAPMODEL_STANDBY,
	WEAPMODEL_ATTACK_WEAK,
	WEAPMODEL_ATTACK_STRONG,
	WEAPMODEL_WEAPDOWN,
	WEAPMODEL_WEAPONUP,

	VWEAP_MAXANIMS
};

#define WEAPONINFO_MAX_FIRE_SOUNDS 4

//equivalent to pmodelinfo_t. Shared by different players, etc.
typedef struct weaponinfo_s
{
	char name[MAX_QPATH];
	bool inuse;

	struct	model_s	*model[WEAPMODEL_PARTS]; //one weapon consists of several models

	int firstframe[VWEAP_MAXANIMS];         //animation script
	int lastframe[VWEAP_MAXANIMS];
	int loopingframes[VWEAP_MAXANIMS];
	unsigned int frametime[VWEAP_MAXANIMS];

	orientation_t tag_projectionsource;
	byte_vec4_t outlineColor;

	// handOffset
	vec3_t handpositionOrigin;
	vec3_t handpositionAngles;

	// flash
	unsigned int flashTime;
	bool flashFade;
	float flashRadius;
	vec3_t flashColor;

	// barrel
	unsigned int barrelTime;
	float barrelSpeed;

	// sfx
	int num_fire_sounds;
	struct sfx_s *sound_fire[WEAPONINFO_MAX_FIRE_SOUNDS];
	int num_strongfire_sounds;
	struct sfx_s *sound_strongfire[WEAPONINFO_MAX_FIRE_SOUNDS];
	struct sfx_s *sound_reload;

} weaponinfo_t;

extern weaponinfo_t cg_pWeaponModelInfos[WEAP_TOTAL];

#define SKM_MAX_BONES 256

//pmodelinfo_t is the playermodel structure as originally readed
//Consider it static 'read-only', cause it is shared by different players
typedef struct pmodelinfo_s
{
	char *name;
	int sex;

	struct	model_s	*model;
	struct cg_sexedSfx_s *sexedSfx;

	int numRotators[PMODEL_PARTS];
	int rotator[PMODEL_PARTS][16];
	int rootanims[PMODEL_PARTS];

	gs_pmodel_animationset_t animSet; // animation script

	struct pmodelinfo_s *next;
} pmodelinfo_t;

typedef struct
{
	//static data
	pmodelinfo_t *pmodelinfo;
	struct skinfile_s *skin;

	//dynamic
	gs_pmodel_animationstate_t animState;

	vec3_t angles[PMODEL_PARTS];                // for rotations
	vec3_t oldangles[PMODEL_PARTS];             // for rotations

	//effects
	orientation_t projectionSource;     // for projectiles
	// weapon. Not sure about keeping it here
	unsigned int flash_time;
	unsigned int barrel_time;

} pmodel_t;

extern pmodel_t	cg_entPModels[MAX_EDICTS];      //a pmodel handle for each cg_entity

//
// cg_pmodels.c
//

//utils
void CG_AddShellEffects( entity_t *ent, int effects );
bool CG_GrabTag( orientation_t *tag, entity_t *ent, const char *tagname );
void CG_PlaceModelOnTag( entity_t *ent, entity_t *dest, orientation_t *tag );
void CG_PlaceRotatedModelOnTag( entity_t *ent, entity_t *dest, orientation_t *tag );
void CG_MoveToTag( vec3_t move_origin, 
	mat3_t move_axis, 
	const vec3_t space_origin, 
	const mat3_t space_axis, 
	const vec3_t tag_origin, 
	const mat3_t tag_axis );

//pmodels
void CG_PModelsInit( void );
void CG_ResetPModels( void );
void CG_RegisterBasePModel( void );
struct pmodelinfo_s *CG_RegisterPlayerModel( const char *filename );
void CG_LoadClientPmodel( int cenum, char *model_name, const char *skin_name );
void CG_AddPModel( centity_t *cent );
bool CG_PModel_GetProjectionSource( int entnum, orientation_t *tag_result );
void CG_UpdatePlayerModelEnt( centity_t *cent );
void CG_PModel_AddAnimation( int entNum, int loweranim, int upperanim, int headanim, int channel );
void CG_PModel_ClearEventAnimations( int entNum );

//
// cg_wmodels.c
//
struct weaponinfo_s *CG_CreateWeaponZeroModel( char *cgs_name );
struct weaponinfo_s *CG_RegisterWeaponModel( char *cgs_name, int weaponTag );
void CG_AddWeaponOnTag( entity_t *ent, orientation_t *tag, int weapon, int effects, orientation_t *projectionSource, unsigned int flash_time, unsigned int barrel_time );
struct weaponinfo_s *CG_GetWeaponInfo( int currentweapon );

//=================================================
//				VIEW WEAPON
//=================================================

typedef struct
{
	entity_t ent;

	unsigned int POVnum;
	int weapon;

	// animation
	int baseAnim;
	unsigned int baseAnimStartTime;
	int eventAnim;
	unsigned int eventAnimStartTime;

	// other effects
	orientation_t projectionSource;
} cg_viewweapon_t;

