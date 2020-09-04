/*
Copyright (C) 1997-2001 Id Software, Inc.

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

/*
==========================================================================

- SPLITMODELS -

==========================================================================
*/

// - Adding the weapon models in split pieces
// by Jalisk0

#include "cg_local.h"


//======================================================================
//						weaponinfo Registering
//======================================================================

weaponinfo_t cg_pWeaponModelInfos[WEAP_TOTAL];

static const char *wmPartSufix[] = { "", "_expansion", "_flash", "_hand", "_barrel", "_barrel2", NULL };

/*
* CG_vWeap_ParseAnimationScript
*
* script:
* 0 = first frame
* 1 = lastframe/number of frames
* 2 = looping frames
* 3 = frame time
*
* keywords:
* "islastframe":Will read the second value of each animation as lastframe (usually means numframes)
* "rotationscale": value witch will scale the barrel rotation speed
* "ammoCounter": digital ammo counter parameters: font_family font_size digit_width digit_height digit_alpha icon_size icon_alpha
*/
static bool CG_vWeap_ParseAnimationScript( weaponinfo_t *weaponinfo, const char *filename ) {
	uint8_t *buf;
	char *ptr, *token;
	int rounder, counter, i;
	bool debug = true;
	int anim_data[4][VWEAP_MAXANIMS];
	int length, filenum;

	rounder = 0;
	counter = 1; // reserve 0 for 'no animation'

	// set some defaults
	for( i = 0; i < VWEAP_MAXPARTS; i++ )
		weaponinfo->barrelSpeed[i] = 0;
	weaponinfo->flashFade = true;

	if( !cg_debugWeaponModels->integer ) {
		debug = false;
	}

	// load the file
	length = trap_FS_FOpenFile( filename, &filenum, FS_READ );
	if( length == -1 ) {
		return false;
	}
	if( !length ) {
		trap_FS_FCloseFile( filenum );
		return false;
	}
	buf = ( uint8_t * )CG_Malloc( length + 1 );
	trap_FS_Read( buf, length, filenum );
	trap_FS_FCloseFile( filenum );

	if( !buf ) {
		CG_Free( buf );
		return false;
	}

	if( debug ) {
		CG_Printf( "%sLoading weapon animation script:%s%s\n", S_COLOR_BLUE, filename, S_COLOR_WHITE );
	}

	memset( anim_data, 0, sizeof( anim_data ) );

	//proceed
	ptr = ( char * )buf;
	while( ptr ) {
		token = COM_ParseExt( &ptr, true );
		if( !token[0] ) {
			break;
		}

		//see if it is keyword or number
		if( *token < '0' || *token > '9' ) {
			if( !Q_stricmp( token, "barrel" ) ) {
				if( debug ) {
					CG_Printf( "%sScript: barrel:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				// time
				i = atoi( COM_ParseExt( &ptr, false ) );
				weaponinfo->barrelTime = (unsigned int)( i > 0 ? i : 0 );

				// speed
				weaponinfo->barrelSpeed[WEAPMODEL_BARREL] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->barrelSpeed[WEAPMODEL_BARREL2] = atof( COM_ParseExt( &ptr, false ) );

				if( debug ) {
					CG_Printf( "%s time:%" PRIi64 ", speed:%.2f%s\n", S_COLOR_BLUE, weaponinfo->barrelTime, weaponinfo->barrelSpeed[WEAPMODEL_BARREL], S_COLOR_WHITE );
				}
			} else if( !Q_stricmp( token, "flash" ) ) {
				if( debug ) {
					CG_Printf( "%sScript: flash:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				// time
				i = atoi( COM_ParseExt( &ptr, false ) );
				weaponinfo->flashTime = (unsigned int)( i > 0 ? i : 0 );

				// radius
				i = atoi( COM_ParseExt( &ptr, false ) );
				weaponinfo->flashRadius = (float)( i > 0 ? i : 0 );

				// fade
				token = COM_ParseExt( &ptr, false );
				if( !Q_stricmp( token, "no" ) ) {
					weaponinfo->flashFade = false;
				}

				if( debug ) {
					CG_Printf( "%s time:%i, radius:%i, fade:%s%s\n", S_COLOR_BLUE, (int)weaponinfo->flashTime, (int)weaponinfo->flashRadius, weaponinfo->flashFade ? "YES" : "NO", S_COLOR_WHITE );
				}
			} else if( !Q_stricmp( token, "flashColor" ) ) {
				if( debug ) {
					CG_Printf( "%sScript: flashColor:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				weaponinfo->flashColor[0] = atof( token = COM_ParseExt( &ptr, false ) );
				weaponinfo->flashColor[1] = atof( token = COM_ParseExt( &ptr, false ) );
				weaponinfo->flashColor[2] = atof( token = COM_ParseExt( &ptr, false ) );

				if( debug ) {
					CG_Printf( "%s%f %f %f%s\n", S_COLOR_BLUE,
							   weaponinfo->flashColor[0], weaponinfo->flashColor[1], weaponinfo->flashColor[2],
							   S_COLOR_WHITE );
				}
			} else if( !Q_stricmp( token, "handOffset" ) ) {
				if( debug ) {
					CG_Printf( "%sScript: handPosition:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				weaponinfo->handpositionOrigin[FORWARD] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionOrigin[RIGHT] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionOrigin[UP] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[PITCH] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[YAW] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[ROLL] = atof( COM_ParseExt( &ptr, false ) );

				if( debug ) {
					CG_Printf( "%s%f %f %f %f %f %f%s\n", S_COLOR_BLUE,
							   weaponinfo->handpositionOrigin[0], weaponinfo->handpositionOrigin[1], weaponinfo->handpositionOrigin[2],
							   weaponinfo->handpositionAngles[0], weaponinfo->handpositionAngles[1], weaponinfo->handpositionAngles[2],
							   S_COLOR_WHITE );
				}

			} else if( !Q_stricmp( token, "firesound" ) ) {
				if( debug ) {
					CG_Printf( "%sScript: firesound:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}
				if( weaponinfo->num_fire_sounds >= WEAPONINFO_MAX_FIRE_SOUNDS ) {
					if( debug ) {
						CG_Printf( S_COLOR_BLUE "too many firesounds defined. Max is %i" S_COLOR_WHITE "\n", WEAPONINFO_MAX_FIRE_SOUNDS );
					}
					break;
				}

				token = COM_ParseExt( &ptr, false );
				if( Q_stricmp( token, "NULL" ) ) {
					weaponinfo->sound_fire[weaponinfo->num_fire_sounds] = trap_S_RegisterSound( token );
					if( weaponinfo->sound_fire[weaponinfo->num_fire_sounds] != NULL ) {
						weaponinfo->num_fire_sounds++;
					}
				}
				if( debug ) {
					CG_Printf( "%s%s%s\n", S_COLOR_BLUE, token, S_COLOR_WHITE );
				}
			} else if( !Q_stricmp( token, "strongfiresound" ) ) {
				if( debug ) {
					CG_Printf( "%sScript: strongfiresound:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}
				if( weaponinfo->num_strongfire_sounds >= WEAPONINFO_MAX_FIRE_SOUNDS ) {
					if( debug ) {
						CG_Printf( S_COLOR_BLUE "too many strongfiresound defined. Max is %i" S_COLOR_WHITE "\n", WEAPONINFO_MAX_FIRE_SOUNDS );
					}
					break;
				}

				token = COM_ParseExt( &ptr, false );
				if( Q_stricmp( token, "NULL" ) ) {
					weaponinfo->sound_strongfire[weaponinfo->num_strongfire_sounds] = trap_S_RegisterSound( token );
					if( weaponinfo->sound_strongfire[weaponinfo->num_strongfire_sounds] != NULL ) {
						weaponinfo->num_strongfire_sounds++;
					}
				}
				if( debug ) {
					CG_Printf( "%s%s%s\n", S_COLOR_BLUE, token, S_COLOR_WHITE );
				}
			} else if( !Q_stricmp( token, "ammoCounter" ) ) {
				char fontName[MAX_TOKEN_CHARS];
				char fontSize;

				if( debug ) {
					CG_Printf( "%sScript: ammoCounter:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				token = COM_ParseExt( &ptr, false );
				Q_strncpyz( fontName, token, sizeof( fontName ) );

				token = COM_ParseExt( &ptr, false );
				fontSize = atoi( token );

				if( fontName[0] && fontSize ) {
					weaponinfo->acFont = trap_SCR_RegisterFont( fontName, QFONT_STYLE_NONE, fontSize );
				} else {
					weaponinfo->acFont = NULL;
				}
				if( weaponinfo->acFont ) {
					weaponinfo->acFontWidth = trap_SCR_strWidth( "0", weaponinfo->acFont, 0 );
				}

				weaponinfo->acDigitWidth = atof( token = COM_ParseExt( &ptr, false ) );
				weaponinfo->acDigitHeight = atof( token = COM_ParseExt( &ptr, false ) );
				weaponinfo->acDigitAlpha = atof( token = COM_ParseExt( &ptr, false ) );
				weaponinfo->acIconSize = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->acIconAlpha = atof( COM_ParseExt( &ptr, false ) );

				if( debug ) {
					CG_Printf( S_COLOR_BLUE "%s %i %f %f %f %f %f" S_COLOR_WHITE "\n",
						fontName, fontSize, weaponinfo->acDigitWidth, weaponinfo->acDigitHeight, 
						weaponinfo->acDigitAlpha, weaponinfo->acIconSize, weaponinfo->acIconAlpha );
				}
			} else if( token[0] && debug ) {
				CG_Printf( "%signored: %s%s\n", S_COLOR_YELLOW, token, S_COLOR_WHITE );
			}
		} else {
			//frame & animation values
			i = (int)atoi( token );
			if( debug ) {
				if( rounder == 0 ) {
					CG_Printf( "%sScript: %s", S_COLOR_BLUE, S_COLOR_WHITE );
				}
				CG_Printf( "%s%i - %s", S_COLOR_BLUE, i, S_COLOR_WHITE );
			}
			anim_data[rounder][counter] = i;
			rounder++;
			if( rounder > 3 ) {
				rounder = 0;
				if( debug ) {
					CG_Printf( "%s anim: %i%s\n", S_COLOR_BLUE, counter, S_COLOR_WHITE );
				}
				counter++;
				if( counter == VWEAP_MAXANIMS ) {
					break;
				}
			}
		}
	}

	CG_Free( buf );

	if( counter < VWEAP_MAXANIMS ) {
		CG_Printf( "%sERROR: incomplete WEAPON script: %s - Using default%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );
		return false;
	}

	//reorganize to make my life easier
	for( i = 0; i < VWEAP_MAXANIMS; i++ ) {
		weaponinfo->firstframe[i] = anim_data[0][i];
		weaponinfo->lastframe[i] = anim_data[1][i];
		weaponinfo->loopingframes[i] = anim_data[2][i];

		if( anim_data[3][i] < 10 ) { //never allow less than 10 fps
			anim_data[3][i] = 10;
		}

		weaponinfo->frametime[i] = 1000 / anim_data[3][i];
	}

	return true;
}

/*
* CG_LoadHandAnimations
*/
static void CG_CreateHandDefaultAnimations( weaponinfo_t *weaponinfo ) {
	int i;
	float defaultfps = 15.0f;

	for( i = 0; i < VWEAP_MAXPARTS; i++ )
		weaponinfo->barrelSpeed[i] = 0;

	// default wsw hand
	weaponinfo->firstframe[WEAPANIM_STANDBY] = 0;
	weaponinfo->lastframe[WEAPANIM_STANDBY] = 0;
	weaponinfo->loopingframes[WEAPANIM_STANDBY] = 1;
	weaponinfo->frametime[WEAPANIM_STANDBY] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_ATTACK_WEAK] = 1; // attack animation (1-5)
	weaponinfo->lastframe[WEAPANIM_ATTACK_WEAK] = 5;
	weaponinfo->loopingframes[WEAPANIM_ATTACK_WEAK] = 0;
	weaponinfo->frametime[WEAPANIM_ATTACK_WEAK] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_ATTACK_STRONG] = 0;
	weaponinfo->lastframe[WEAPANIM_ATTACK_STRONG] = 0;
	weaponinfo->loopingframes[WEAPANIM_ATTACK_STRONG] = 1;
	weaponinfo->frametime[WEAPANIM_ATTACK_STRONG] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_WEAPDOWN] = 0;
	weaponinfo->lastframe[WEAPANIM_WEAPDOWN] = 0;
	weaponinfo->loopingframes[WEAPANIM_WEAPDOWN] = 1;
	weaponinfo->frametime[WEAPANIM_WEAPDOWN] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_WEAPONUP] = 6; // flipout animation (6-10)
	weaponinfo->lastframe[WEAPANIM_WEAPONUP] = 10;
	weaponinfo->loopingframes[WEAPANIM_WEAPONUP] = 1;
	weaponinfo->frametime[WEAPANIM_WEAPONUP] = 1000 / defaultfps;

	return;
}

/*
* CG_ComputeWeaponInfoTags
*
* Store the orientation_t closer to the tag_flash we can create,
* or create one using an offset we consider acceptable.
*
* NOTE: This tag will ignore weapon models animations. You'd have to
* do it in realtime to use it with animations. Or be careful on not
* moving the weapon too much
*/
static void CG_ComputeWeaponInfoTags( weaponinfo_t *weaponinfo ) {
	orientation_t tag, tag_barrel, tag_barrel2;
	entity_t ent;
	bool have_barrel;

	VectorSet( weaponinfo->tag_projectionsource.origin, 16, 0, 8 );
	Matrix3_Identity( weaponinfo->tag_projectionsource.axis );

	if( !weaponinfo->model[WEAPMODEL_WEAPON] ) {
		return;
	}

	// assign the model to an entity_t, so we can build boneposes
	memset( &ent, 0, sizeof( ent ) );
	ent.rtype = RT_MODEL;
	ent.scale = 1.0f;
	ent.model = weaponinfo->model[WEAPMODEL_WEAPON];
	CG_SetBoneposesForTemporaryEntity( &ent ); // assigns and builds the skeleton so we can use grabtag

	have_barrel = false;
	if( weaponinfo->model[WEAPMODEL_BARREL] && CG_GrabTag( &tag_barrel, &ent, "tag_barrel" ) ) {
		have_barrel = true;
	}
	
	if( weaponinfo->model[WEAPMODEL_BARREL2] ) {
		if( !have_barrel || !CG_GrabTag( &tag_barrel2, &ent, "tag_barrel2" ) ) {
			weaponinfo->model[WEAPMODEL_BARREL2] = NULL;
		}
	}

	// try getting the tag_flash from the weapon model
	if( !CG_GrabTag( &weaponinfo->tag_projectionsource, &ent, "tag_flash" ) && have_barrel ) {
		// if it didn't work, try getting it from the barrel model
		// assign the model to an entity_t, so we can build boneposes
		entity_t ent_barrel;

		memset( &ent_barrel, 0, sizeof( ent_barrel ) );
		ent_barrel.rtype = RT_MODEL;
		ent_barrel.scale = 1.0f;
		ent_barrel.model = weaponinfo->model[WEAPMODEL_BARREL];
		CG_SetBoneposesForTemporaryEntity( &ent_barrel );

		if( CG_GrabTag( &tag, &ent_barrel, "tag_flash" ) ) {
			VectorCopy( vec3_origin, weaponinfo->tag_projectionsource.origin );
			Matrix3_Identity( weaponinfo->tag_projectionsource.axis );
			CG_MoveToTag( weaponinfo->tag_projectionsource.origin,
						  weaponinfo->tag_projectionsource.axis,
						  tag_barrel.origin,
						  tag_barrel.axis,
						  tag.origin,
						  tag.axis );
		}
	}
}

/*
* CG_WeaponModelUpdateRegistration
*/
static bool CG_WeaponModelUpdateRegistration( weaponinfo_t *weaponinfo, char *filename ) {
	int p;
	char scratch[MAX_QPATH];

	for( p = 0; p < VWEAP_MAXPARTS; p++ ) {
		// iqm
		if( !weaponinfo->model[p] ) {
			Q_snprintfz( scratch, sizeof( scratch ), "%s%s.iqm", filename, wmPartSufix[p] );
			weaponinfo->model[p] = CG_RegisterModel( scratch );
		}

		// md3
		if( !weaponinfo->model[p] ) {
			Q_snprintfz( scratch, sizeof( scratch ), "%s%s.md3", filename, wmPartSufix[p] );
			weaponinfo->model[p] = CG_RegisterModel( scratch );
		}

		weaponinfo->skel[p] = NULL;
		if( ( p == WEAPMODEL_HAND ) && ( weaponinfo->model[p] ) ) {
			weaponinfo->skel[p] = CG_SkeletonForModel( weaponinfo->model[p] );
		}
	}

	if( !weaponinfo->model[WEAPMODEL_BARREL] ) {
		weaponinfo->model[WEAPMODEL_BARREL2] = NULL;
	}

	// load failed
	if( !weaponinfo->model[WEAPMODEL_HAND] ) {
		for( p = 0; p < VWEAP_MAXPARTS; p++ )
			weaponinfo->model[p] = NULL;
		return false;
	}

	// load animation script for the hand model
	Q_snprintfz( scratch, sizeof( scratch ), "%s.cfg", filename );

	if( !CG_vWeap_ParseAnimationScript( weaponinfo, scratch ) ) {
		CG_CreateHandDefaultAnimations( weaponinfo );
	}

	// reuse the main barrel model for the second barrel if the later is not found on disk but
	// rotation speed is specified in the script
	if( weaponinfo->barrelSpeed[WEAPMODEL_BARREL2] && !weaponinfo->model[WEAPMODEL_BARREL2] ) {
		weaponinfo->model[WEAPMODEL_BARREL2] = weaponinfo->model[WEAPMODEL_BARREL];
	}

	// create a tag_projection from tag_flash, to position fire effects
	CG_ComputeWeaponInfoTags( weaponinfo );

	Vector4Set( weaponinfo->outlineColor, 0, 0, 0, 255 );

	if( cg_debugWeaponModels->integer ) {
		CG_Printf( "%sWEAPmodel: Loaded successful%s\n", S_COLOR_BLUE, S_COLOR_WHITE );
	}

	return true;
}

/*
* CG_RegisterWeaponModel
*/
struct weaponinfo_s *CG_RegisterWeaponModel( const char *cgs_name, int weaponTag ) {
	char filename[MAX_QPATH];
	weaponinfo_t *weaponinfo;

	if( weaponTag >= WEAP_TOTAL ) {
		if( cg_debugWeaponModels->integer ) {
			CG_Printf( "%sWEAPmodel: Failed:%s tag: %d %s\n", S_COLOR_YELLOW, cgs_name, weaponTag, S_COLOR_WHITE );
		}
		return NULL;
	}

	Q_strncpyz( filename, cgs_name, sizeof( filename ) );
	COM_StripExtension( filename );

	weaponinfo = &cg_pWeaponModelInfos[weaponTag];
	if( weaponinfo->inuse == true ) {
		return weaponinfo;
	}

	weaponinfo->inuse = CG_WeaponModelUpdateRegistration( weaponinfo, filename );
	if( !weaponinfo->inuse ) {
		if( cg_debugWeaponModels->integer ) {
			CG_Printf( "%sWEAPmodel: Failed:%s%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );
		}
		return NULL;
	}

	// find the item for this weapon and try to assign the outline color
	if( weaponTag ) {
		gsitem_t *item = GS_FindItemByTag( weaponTag );
		if( item ) {
			if( item->color && strlen( item->color ) > 1 ) {
				byte_vec4_t colorByte;

				Vector4Scale( color_table[ColorIndex( item->color[1] )], 255, colorByte );
				CG_SetOutlineColor( weaponinfo->outlineColor, colorByte );
			}
		}
	}

	return weaponinfo;
}


/*
* CG_CreateWeaponZeroModel
*
* we can't allow NULL weaponmodels to be passed to the viewweapon.
* They will produce crashes because the lack of animation script.
* We need to have at least one weaponinfo with a script to be used
* as a replacement, so, weapon 0 will have the animation script
* even if the registration failed
*/
struct weaponinfo_s *CG_CreateWeaponZeroModel( const char *filename ) {
	weaponinfo_t *weaponinfo;

	weaponinfo = &cg_pWeaponModelInfos[WEAP_NONE];
	if( weaponinfo->inuse == true ) {
		return weaponinfo;
	}

	CG_CreateHandDefaultAnimations( weaponinfo );
	Vector4Set( weaponinfo->outlineColor, 0, 0, 0, 255 );
	weaponinfo->inuse = true;

	return weaponinfo; //no checks
}


//======================================================================
//							weapons
//======================================================================

/*
* CG_GetWeaponInfo
*/
struct weaponinfo_s *CG_GetWeaponInfo( int weapon ) {
	if( weapon < 0 || ( weapon >= WEAP_TOTAL ) ) {
		weapon = WEAP_NONE;
	}

	return cgs.weaponInfos[weapon] ? cgs.weaponInfos[weapon] : cgs.weaponInfos[WEAP_NONE];
}

/*
* CG_AddWeaponFlashOnTag
*/
static void CG_AddWeaponFlashOnTag( entity_t *weapon, weaponinfo_t *weaponInfo, int modelid, 
	const char *tag_flash, int effects, int64_t flash_time ) {
	uint8_t c;
	orientation_t tag;
	entity_t flash;
	float intensity;

	if( flash_time < cg.time ) {
		return;
	}
	if( !weaponInfo->model[modelid] ) {
		return;
	}
	if( !CG_GrabTag( &tag, weapon, tag_flash ) ) {
		return;
	}

	if( weaponInfo->flashFade ) {
		intensity = (float)( flash_time - cg.time ) / (float)weaponInfo->flashTime;
		c = ( uint8_t )( 255 * intensity );
	} else {
		intensity = 1.0f;
		c = 255;
	}

	memset( &flash, 0, sizeof( flash ) );
	Vector4Set( flash.shaderRGBA, c, c, c, c );
	flash.model = weaponInfo->model[modelid];
	flash.scale = weapon->scale;
	flash.renderfx = weapon->renderfx | RF_NOSHADOW;
	flash.frame = 0;
	flash.oldframe = 0;

	CG_PlaceModelOnTag( &flash, weapon, &tag );
	
	if( !( effects & EF_RACEGHOST ) ) {
		CG_AddEntityToScene( &flash );
	}

	CG_AddLightToScene( flash.origin, weaponInfo->flashRadius * intensity,
		weaponInfo->flashColor[0], weaponInfo->flashColor[1], weaponInfo->flashColor[2] );
}

/*
* CG_AddWeaponExpansionOnTag
*/
static void CG_AddWeaponExpansionOnTag( entity_t *weapon, weaponinfo_t *weaponInfo, int modelid, 
	const char *tag_expansion, int effects ) {
	orientation_t tag;
	entity_t expansion;

	if( !weaponInfo->model[modelid] ) {
		return;
	}
	if( !CG_GrabTag( &tag, weapon, tag_expansion ) ) {
		return;
	}

	memset( &expansion, 0, sizeof( expansion ) );
	Vector4Set( expansion.shaderRGBA, 255, 255, 255, weapon->shaderRGBA[3] );
	expansion.model = weaponInfo->model[modelid];
	expansion.scale = weapon->scale;
	expansion.renderfx = weapon->renderfx;
	expansion.frame = 0;
	expansion.oldframe = 0;

	CG_PlaceModelOnTag( &expansion, weapon, &tag );
	
	CG_AddColoredOutLineEffect( &expansion, effects, 0, 0, 0, 255 );

	if( !( effects & EF_RACEGHOST ) ) {
		CG_AddEntityToScene( &expansion ); // skelmod
	}

	CG_AddShellEffects( &expansion, effects );
}

/*
* CG_AddWeaponBarrelOnTag
*/
static void CG_AddWeaponBarrelOnTag( entity_t *weapon, weaponinfo_t *weaponInfo, int modelid, 
	const char *tag_barrel, const char *tag_recoil, int effects, int64_t barrel_time ) {
	orientation_t tag;
	vec3_t rotangles = { 0, 0, 0 };
	entity_t barrel;

	if( !weaponInfo->model[modelid] ) {
		return;
	}
	if( !CG_GrabTag( &tag, weapon, tag_barrel ) ) {
		return;
	}

	memset( &barrel, 0, sizeof( barrel ) );
	Vector4Set( barrel.shaderRGBA, 255, 255, 255, weapon->shaderRGBA[3] );
	barrel.model = weaponInfo->model[modelid];
	barrel.scale = weapon->scale;
	barrel.renderfx = weapon->renderfx;
	barrel.frame = 0;
	barrel.oldframe = 0;

	// rotation
	if( barrel_time > cg.time ) {
		float intensity;
		orientation_t recoil;

		intensity =  (float)( barrel_time - cg.time ) / (float)weaponInfo->barrelTime;
		rotangles[2] = anglemod( 360.0f * weaponInfo->barrelSpeed[modelid] * intensity * intensity );

		// Check for tag_recoil
		if( CG_GrabTag( &recoil, weapon, tag_recoil ) ) {
			VectorLerp( tag.origin, intensity, recoil.origin, tag.origin );
		}
	}

	AnglesToAxis( rotangles, barrel.axis );

	// barrel requires special tagging
	CG_PlaceRotatedModelOnTag( &barrel, weapon, &tag );

	CG_AddColoredOutLineEffect( &barrel, effects, 0, 0, 0, weapon->shaderRGBA[3] );

	if( !( effects & EF_RACEGHOST ) )
		CG_AddEntityToScene( &barrel );

	CG_AddShellEffects( &barrel, effects );
}

/*
* CG_AddPolyOnTag
*/
static void CG_AddPolyOnTag( const entity_t *weapon, const orientation_t *tag, float width, float height, 
	float x_offset, float s1, float t1, float s2, float t2, const vec4_t color, float alpha, struct shader_s *shader ) {
	int i;
	vec4_t origin;
	mat3_t mat, tmat;
	poly_t p;
	vec4_t verts[4];
	vec4_t normals[4];
	vec2_t texcoords[4];
	byte_vec4_t colors[4];
	unsigned short elems[6] = { 0, 1, 2, 0, 2, 3 };

	if( !tag ) {
		return;
	}
	if( !alpha ) {
		return;
	}

	Vector4Set( origin, 0, width / 2.0, height / 2.0, 1 );

	Vector4Set( verts[0], 0, x_offset, 0, 1 );
	Vector4Set( normals[0], 0, 0, 0, 0 );
	Vector2Set( texcoords[0], s1, t1 );

	for( i = 0; i < 3; i++ )
		colors[0][i] = ( uint8_t )Q_bound( 0, color[i] * 255, 255 );
	colors[0][3] = ( uint8_t )(alpha * (float)weapon->shaderRGBA[3]);

	Vector4Set( verts[1], 0, width, 0, 1 );
	Vector4Set( normals[0], 0, 0, 0, 0 );
	Vector2Set( texcoords[1], s2, t1 );
	Vector4Copy( colors[0], colors[1] );

	Vector4Set( verts[2], 0, width, height, 1 );
	Vector4Set( normals[2], 0, 0, 0, 0 );
	Vector2Set( texcoords[2], s2, t2 );
	Vector4Copy( colors[0], colors[2] );

	Vector4Set( verts[3], 0, x_offset, height, 1 );
	Vector4Set( normals[3], 0, 0, 0, 0 );
	Vector2Set( texcoords[3], s1, t2 );
	Vector4Copy( colors[0], colors[3] );

	memset( &p, 0, sizeof( p ) );
	p.verts = verts;
	p.normals = normals;
	p.stcoords = texcoords;
	p.colors = colors;
	p.numverts = 4;
	p.elems = elems;
	p.numelems = 6;
	p.renderfx = weapon->renderfx;
	p.shader = shader;

	for( i = 0; i < 4; i++ ) {
		vec3_t vv;
		vec3_t org;

		VectorClear( org );
		Matrix3_Identity( mat );
		VectorSubtract( verts[i], origin, verts[i] );

		CG_MoveToTag( org, mat,
			weapon->origin, weapon->axis,
			tag->origin,
			tag->axis
		);

		Matrix3_Transpose( mat, tmat );
		Matrix3_TransformVector( tmat, verts[i], vv );
		VectorAdd( vv, org, verts[i] );
	}

	trap_R_AddPolyToScene( &p );
}

static int char_w, char_h;
static float char_s1, char_t1, char_s2, char_t2;
static struct shader_s *char_shader;

static void DrawCharCallback( int x, int y, int w, int h, float s1, 
	float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader ) {
	char_w = w;
	char_h = h;
	char_s1 = s1, char_t1 = t1, char_s2 = s2, char_t2 = t2;
	char_shader = (struct shader_s *)shader;
}

/*
* CG_AddAmmoDigitOnTag
*/
static void CG_AddAmmoDigitOnTag( entity_t *weapon, const weaponinfo_t *weaponInfo, 
	const gsitem_t *ammoItem, int num, const char *tag_name ) {
	float width, height;
	float x_width, x_offset;
	cg_fdrawchar_t pop;
	orientation_t tag_digit;

	if( !weaponInfo->acFont ) {
		return;
	}
	if( !weaponInfo->acDigitWidth || !weaponInfo->acDigitHeight ) {
		return;
	}
	if( !CG_GrabTag( &tag_digit, weapon, tag_name ) ) {
		return;
	}
	
	width = weaponInfo->acDigitWidth;
	height = weaponInfo->acDigitHeight;

	pop = trap_SCR_SetDrawCharIntercept( (cg_fdrawchar_t)&DrawCharCallback );
	trap_SCR_DrawRawChar( 0, 0, '0' + num, weaponInfo->acFont, colorWhite );

	trap_SCR_SetDrawCharIntercept( pop );

	x_width = weaponInfo->acFontWidth;
	x_offset = width * (1.0 - (float)char_w / x_width);

	CG_AddPolyOnTag( weapon, &tag_digit, width, height, x_offset, char_s1, char_t1, char_s2, char_t2, 
		color_table[ColorIndex( ammoItem->color[1] )], weaponInfo->acDigitAlpha, char_shader );
}

/*
* CG_AddItemIconOnTag
*/
static void CG_AddItemIconOnTag( entity_t *weapon, const weaponinfo_t *weaponInfo, 
	const gsitem_t *item, const char *tag_name ) {
	float size;
	orientation_t tag_icon;

	if( !CG_GrabTag( &tag_icon, weapon, tag_name ) ) {
		return;
	}

	size = weaponInfo->acIconSize;
	if( !size ) {
		return;
	}

	CG_AddPolyOnTag( weapon, &tag_icon, size, size, 0, 0.0, 0.0, 1.0, 1.0, 
		colorWhite, weaponInfo->acIconAlpha, trap_R_RegisterPic( item->icon ) );
}

/*
* CG_AddWeaponOnTag
*
* Add weapon model(s) positioned at the tag
*
* @param ammo_count Current ammo count for the counter. Negative value skips rendering of the counter. 
*/
void CG_AddWeaponOnTag( entity_t *ent, orientation_t *tag, int weaponid, int effects, 
	orientation_t *projectionSource, int64_t flash_time, int64_t barrel_time, int ammo_count ) {
	entity_t weapon;
	weaponinfo_t *weaponInfo;
	gsitem_t *weaponItem, *ammoItem;

	// don't try without base model or tag
	if( !ent->model || !tag ) {
		return;
	}

	weaponInfo = CG_GetWeaponInfo( weaponid );
	if( !weaponInfo ) {
		return;
	}

	weaponItem = GS_FindItemByTag( weaponid );
	if( !weaponItem ) {
		return;
	}

	ammoItem = GS_FindItemByTag( weaponItem->ammo_tag );

	//if( ent->renderfx & RF_WEAPONMODEL )
	//	effects &= ~EF_RACEGHOST;

	memset( &weapon, 0, sizeof( weapon ) );
	Vector4Set( weapon.shaderRGBA, 255, 255, 255, ent->shaderRGBA[3] );
	weapon.scale = ent->scale;
	weapon.renderfx = ent->renderfx;
	weapon.frame = 0;
	weapon.oldframe = 0;
	weapon.model = weaponInfo->model[WEAPMODEL_WEAPON];

	CG_PlaceModelOnTag( &weapon, ent, tag );

	CG_AddColoredOutLineEffect( &weapon, effects, 0, 0, 0, 255 );

	if( !( effects & EF_RACEGHOST ) ) {
		CG_AddEntityToScene( &weapon );
	}

	if( !weapon.model ) {
		return;
	}

	CG_AddShellEffects( &weapon, effects );

	// update projection source
	if( projectionSource != NULL ) {
		VectorCopy( vec3_origin, projectionSource->origin );
		Matrix3_Copy( axis_identity, projectionSource->axis );
		CG_MoveToTag( projectionSource->origin, projectionSource->axis,
					  weapon.origin, weapon.axis,
					  weaponInfo->tag_projectionsource.origin,
					  weaponInfo->tag_projectionsource.axis );
	}

	// expansion
	if( effects & EF_STRONG_WEAPON ) {
		CG_AddWeaponExpansionOnTag( &weapon, weaponInfo, WEAPMODEL_EXPANSION, "tag_expansion", effects );
	}

	// barrels
	CG_AddWeaponBarrelOnTag( &weapon, weaponInfo, WEAPMODEL_BARREL, "tag_barrel", "tag_recoil", effects, barrel_time );
	CG_AddWeaponBarrelOnTag( &weapon, weaponInfo, WEAPMODEL_BARREL2, "tag_barrel2", "tag_recoil2", effects, barrel_time );

	// flash
	CG_AddWeaponFlashOnTag( &weapon, weaponInfo, WEAPMODEL_FLASH, "tag_flash", effects, flash_time );

	// ammo counter
	if( ammo_count >= 0 ) {
		CG_AddAmmoDigitOnTag( &weapon, weaponInfo, ammoItem, ammo_count % 10, "tag_ammo_digit_1" );
		CG_AddAmmoDigitOnTag( &weapon, weaponInfo, ammoItem, (ammo_count % 100) / 10, "tag_ammo_digit_10" );
		CG_AddAmmoDigitOnTag( &weapon, weaponInfo, ammoItem, ammo_count / 100, "tag_ammo_digit_100" );
	}

	// icons
	CG_AddItemIconOnTag( &weapon, weaponInfo, weaponItem, "tag_weapon_icon" );
	CG_AddItemIconOnTag( &weapon, weaponInfo, ammoItem, "tag_ammo_icon" );
}
