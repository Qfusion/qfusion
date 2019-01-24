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

cg_static_t cgs;
cg_state_t cg;

centity_t cg_entities[MAX_EDICTS];

cvar_t *cg_showMiss;

cvar_t *cg_hand;
cvar_t *cg_clan;
cvar_t *cg_handicap;

cvar_t *cg_addDecals;

cvar_t *cg_gun;

cvar_t *cg_thirdPerson;
cvar_t *cg_thirdPersonAngle;
cvar_t *cg_thirdPersonRange;

cvar_t *cg_weaponFlashes;
cvar_t *cg_gunx;
cvar_t *cg_guny;
cvar_t *cg_gunz;
cvar_t *cg_debugPlayerModels;
cvar_t *cg_debugWeaponModels;
cvar_t *cg_gunbob;

cvar_t *cg_handOffset;
cvar_t *cg_gun_fov;
cvar_t *cg_volume_players;
cvar_t *cg_volume_effects;
cvar_t *cg_volume_announcer;
cvar_t *cg_volume_voicechats;
cvar_t *cg_projectileFireTrail;
cvar_t *cg_bloodTrail;
cvar_t *cg_showBloodTrail;
cvar_t *cg_projectileFireTrailAlpha;
cvar_t *cg_bloodTrailAlpha;
cvar_t *cg_explosionsRing;
cvar_t *cg_explosionsDust;
cvar_t *cg_outlineModels;
cvar_t *cg_outlineWorld;
cvar_t *cg_outlinePlayers;
cvar_t *cg_drawEntityBoxes;
cvar_t *cg_fov;
cvar_t *cg_zoomfov;
cvar_t *cg_voiceChats;
cvar_t *cg_projectileAntilagOffset;
cvar_t *cg_raceGhosts;
cvar_t *cg_raceGhostsAlpha;
cvar_t *cg_chatFilter;

cvar_t *cg_cartoonEffects;

cvar_t *cg_volume_hitsound;
cvar_t *cg_autoaction_demo;
cvar_t *cg_autoaction_screenshot;
cvar_t *cg_autoaction_stats;
cvar_t *cg_autoaction_spectator;
cvar_t *cg_simpleItems;
cvar_t *cg_simpleItemsSize;
cvar_t *cg_showObituaries;
cvar_t *cg_damageNumbers;
cvar_t *cg_particles;
cvar_t *cg_showClamp;

cvar_t *cg_damage_indicator;
cvar_t *cg_damage_indicator_time;

cvar_t *cg_allyColor;
cvar_t *cg_allyModel;
cvar_t *cg_allyForceModel;

cvar_t *cg_enemyColor;
cvar_t *cg_enemyModel;
cvar_t *cg_enemyForceModel;

/*
* CG_API
*/
int CG_API( void ) {
	return CGAME_API_VERSION;
}

/*
* CG_Error
*/
void CG_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

/*
* CG_Printf
*/
void CG_Printf( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}

/*
* CG_LocalPrint
*/
void CG_LocalPrint( const char *format, ... ) {
	va_list argptr;
	char msg[GAMECHAT_STRING_SIZE];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_PrintToLog( msg );

	CG_StackChatString( &cg.chat, msg );
}

/*
* CG_GS_Malloc
*
* Used only for gameshared linking
*/
static void *CG_GS_Malloc( size_t size ) {
	return CG_Malloc( size );
}

/*
* CG_GS_Free
*
* Used only for gameshared linking
*/
static void CG_GS_Free( void *data ) {
	CG_Free( data );
}

/*
* CG_GS_Trace
*/
static void CG_GS_Trace( trace_t *t, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int ignore, int contentmask, int timeDelta ) {
	assert( !timeDelta );
	CG_Trace( t, start, mins, maxs, end, ignore, contentmask );
}

/*
* CG_GS_PointContents
*/
static int CG_GS_PointContents( const vec3_t point, int timeDelta ) {
	assert( !timeDelta );
	return CG_PointContents( point );
}

/*
* CG_GS_GetEntityState
*/
static entity_state_t *CG_GS_GetEntityState( int entNum, int deltaTime ) {
	centity_t *cent;

	if( entNum == -1 ) {
		return NULL;
	}

	assert( entNum >= 0 && entNum < MAX_EDICTS );
	cent = &cg_entities[entNum];

	if( cent->serverFrame != cg.frame.serverFrame ) {
		return NULL;
	}
	return &cent->current;
}

/*
* CG_GS_GetConfigString
*/
static const char *CG_GS_GetConfigString( int index ) {
	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return NULL;
	}

	return cgs.configStrings[ index ];
}

/*
* CG_InitGameShared
*
* Give gameshared access to some utilities
*/
static void CG_InitGameShared( void ) {
	char cstring[MAX_CONFIGSTRING_CHARS];
	int maxclients;
	gs_module_api_t api;

	trap_GetConfigString( CS_MAXCLIENTS, cstring, MAX_CONFIGSTRING_CHARS );
	maxclients = atoi( cstring );
	if( maxclients < 1 || maxclients > MAX_CLIENTS ) {
		maxclients = MAX_CLIENTS;
	}

	memset( &api, 0, sizeof( api ) );
	api.PredictedEvent = CG_PredictedEvent;
	api.Error = CG_Error;
	api.Printf = CG_Printf;
	api.Malloc = CG_GS_Malloc;
	api.Free = CG_GS_Free;
	api.Trace = CG_GS_Trace;
	api.GetEntityState = CG_GS_GetEntityState;
	api.PointContents = CG_GS_PointContents;
	api.PMoveTouchTriggers = CG_Predict_TouchTriggers;
	api.GetConfigString = CG_GS_GetConfigString;
	api.GetAngelExport = NULL;

	GS_InitModule( GS_MODULE_CGAME, maxclients, &api );
}

/*
* CG_CopyString
*/
char *_CG_CopyString( const char *in, const char *filename, int fileline ) {
	char *out;

	out = ( char * )trap_MemAlloc( strlen( in ) + 1, filename, fileline );
	strcpy( out, in );
	return out;
}

/*
* CG_RegisterWeaponModels
*/
static void CG_RegisterWeaponModels( void ) {
	for( int i = 0; i < cgs.numWeaponModels; i++ ) {
		cgs.weaponInfos[i] = CG_RegisterWeaponModel( cgs.weaponModels[i], i );
	}

	// special case for weapon 0. Must always load the animation script
	if( !cgs.weaponInfos[0] ) {
		cgs.weaponInfos[0] = CG_CreateWeaponZeroModel( cgs.weaponModels[0] );
	}
}

/*
* CG_RegisterModels
*/
static void CG_RegisterModels( void ) {
	int i;
	const char *name;

	if( cgs.precacheModelsStart == MAX_MODELS ) {
		return;
	}

	if( cgs.precacheModelsStart == 0 ) {
		name = cgs.configStrings[CS_WORLDMODEL];
		if( name[0] ) {
			if( !CG_LoadingItemName( name ) ) {
				return;
			}
			CG_LoadingString( name );
			trap_R_RegisterWorldModel( name );
		}

		CG_LoadingString( "models" );

		cgs.numWeaponModels = 1;
		Q_strncpyz( cgs.weaponModels[0], "generic/generic.md3", sizeof( cgs.weaponModels[0] ) );

		cgs.precacheModelsStart = 1;
	}

	for( i = cgs.precacheModelsStart; i < MAX_MODELS; i++ ) {
		name = cgs.configStrings[CS_MODELS + i];

		if( !name[0] ) {
			cgs.precacheModelsStart = MAX_MODELS;
			break;
		}

		cgs.precacheModelsStart = i;

		if( name[0] == '#' ) {
			// special player weapon model
			if( cgs.numWeaponModels >= WEAP_TOTAL ) {
				continue;
			}

			if( !CG_LoadingItemName( name ) ) {
				return;
			}

			Q_strncpyz( cgs.weaponModels[cgs.numWeaponModels], name + 1, sizeof( cgs.weaponModels[cgs.numWeaponModels] ) );
			cgs.numWeaponModels++;
		} else if( name[0] == '$' ) {
			if( !CG_LoadingItemName( name ) ) {
				return;
			}

			// indexed pmodel
			cgs.pModelsIndex[i] = CG_RegisterPlayerModel( name + 1 );
		} else {
			if( !CG_LoadingItemName( name ) ) {
				return;
			}
			cgs.modelDraw[i] = CG_RegisterModel( name );
		}
	}

	if( cgs.precacheModelsStart != MAX_MODELS ) {
		return;
	}

	CG_RegisterMediaModels();
	CG_RegisterBasePModel(); // never before registering the weapon models
	CG_RegisterWeaponModels();

	// precache forcemodels if defined
	CG_RegisterForceModels();

	// create a tag to offset the weapon models when seen in the world as items
	VectorSet( cgs.weaponItemTag.origin, 0, 0, 0 );
	Matrix3_Copy( axis_identity, cgs.weaponItemTag.axis );
	VectorMA( cgs.weaponItemTag.origin, -14, &cgs.weaponItemTag.axis[AXIS_FORWARD], cgs.weaponItemTag.origin );
}

/*
* CG_RegisterSounds
*/
static void CG_RegisterSounds( void ) {
	int i;
	const char *name;

	if( cgs.precacheSoundsStart == MAX_SOUNDS ) {
		return;
	}

	if( !cgs.precacheSoundsStart ) {
		CG_LoadingString( "sounds" );

		cgs.precacheSoundsStart = 1;
	}

	for( i = cgs.precacheSoundsStart; i < MAX_SOUNDS; i++ ) {
		name = cgs.configStrings[CS_SOUNDS + i];
		if( !name[0] ) {
			cgs.precacheSoundsStart = MAX_SOUNDS;
			break;
		}

		cgs.precacheSoundsStart = i;

		if( name[0] != '*' ) {
			if( !CG_LoadingItemName( name ) ) {
				return;
			}
			cgs.soundPrecache[i] = trap_S_RegisterSound( name );
		}
	}

	if( cgs.precacheSoundsStart != MAX_SOUNDS ) {
		return;
	}

	CG_RegisterMediaSounds();
}

/*
* CG_RegisterShaders
*/
static void CG_RegisterShaders( void ) {
	int i;
	const char *name;

	if( cgs.precacheShadersStart == MAX_IMAGES ) {
		return;
	}

	if( !cgs.precacheShadersStart ) {
		CG_LoadingString( "shaders" );

		cgs.precacheShadersStart = 1;
	}

	for( i = cgs.precacheShadersStart; i < MAX_IMAGES; i++ ) {
		name = cgs.configStrings[CS_IMAGES + i];
		if( !name[0] ) {
			cgs.precacheShadersStart = MAX_IMAGES;
			break;
		}

		cgs.precacheShadersStart = i;

		if( !CG_LoadingItemName( name ) ) {
			return;
		}

		if( strstr( name, "correction/" ) ) { // HACK HACK HACK -- for color correction LUTs
			cgs.imagePrecache[i] = trap_R_RegisterLinearPic( name );
		} else {
			cgs.imagePrecache[i] = trap_R_RegisterPic( name );
		}
	}

	if( cgs.precacheShadersStart != MAX_IMAGES ) {
		return;
	}

	CG_RegisterMediaShaders();
}

/*
* CG_RegisterSkinfiles
*/
static void CG_RegisterSkinFiles( void ) {
	int i;
	const char *name;

	if( cgs.precacheSkinsStart == MAX_SKINFILES ) {
		return;
	}

	if( !cgs.precacheSkinsStart ) {
		CG_LoadingString( "skins" );

		cgs.precacheSkinsStart = 1;
	}

	for( i = cgs.precacheSkinsStart; i < MAX_SKINFILES; i++ ) {
		name = cgs.configStrings[CS_SKINFILES + i];
		if( !name[0] ) {
			cgs.precacheSkinsStart = MAX_SKINFILES;
			break;
		}

		cgs.precacheSkinsStart = i;

		if( !CG_LoadingItemName( name ) ) {
			return;
		}

		cgs.skinPrecache[i] = trap_R_RegisterSkinFile( name );
	}

	cgs.precacheSkinsStart = MAX_SKINFILES;
}

/*
* CG_RegisterClients
*/
static void CG_RegisterClients( void ) {
	int i;
	const char *name;

	if( cgs.precacheClientsStart == MAX_CLIENTS ) {
		return;
	}

	if( !cgs.precacheClientsStart ) {
		CG_LoadingString( "clients" );
	}

	for( i = cgs.precacheClientsStart; i < MAX_CLIENTS; i++ ) {
		name = cgs.configStrings[CS_PLAYERINFOS + i];
		cgs.precacheClientsStart = i;

		if( !name[0] ) {
			continue;
		}
		if( !CG_LoadingItemName( name ) ) {
			return;
		}

		CG_LoadClientInfo( i );
	}

	cgs.precacheClientsStart = MAX_CLIENTS;
}

/*
* CG_RegisterVariables
*/
static void CG_RegisterVariables( void ) {
	cg_showMiss =       trap_Cvar_Get( "cg_showMiss", "0", 0 );

	cg_debugPlayerModels =  trap_Cvar_Get( "cg_debugPlayerModels", "0", CVAR_CHEAT | CVAR_ARCHIVE );
	cg_debugWeaponModels =  trap_Cvar_Get( "cg_debugWeaponModels", "0", CVAR_CHEAT | CVAR_ARCHIVE );

	cg_hand =           trap_Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_handicap =       trap_Cvar_Get( "handicap", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_clan =           trap_Cvar_Get( "clan", "", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_fov =        trap_Cvar_Get( "fov", "100", CVAR_ARCHIVE );
	cg_zoomfov =    trap_Cvar_Get( "zoomfov", "75", CVAR_ARCHIVE );

	cg_addDecals =      trap_Cvar_Get( "cg_decals", "1", CVAR_ARCHIVE );

	cg_thirdPerson =    trap_Cvar_Get( "cg_thirdPerson", "0", CVAR_CHEAT );
	cg_thirdPersonAngle =   trap_Cvar_Get( "cg_thirdPersonAngle", "0", 0 );
	cg_thirdPersonRange =   trap_Cvar_Get( "cg_thirdPersonRange", "90", 0 );

	cg_gun =        trap_Cvar_Get( "cg_gun", "1", CVAR_ARCHIVE );
	cg_gunx =       trap_Cvar_Get( "cg_gunx", "0", CVAR_ARCHIVE );
	cg_guny =       trap_Cvar_Get( "cg_guny", "0", CVAR_ARCHIVE );
	cg_gunz =       trap_Cvar_Get( "cg_gunz", "0", CVAR_ARCHIVE );
	cg_gunbob =     trap_Cvar_Get( "cg_gunbob", "1", CVAR_ARCHIVE );

	cg_gun_fov =        trap_Cvar_Get( "cg_gun_fov", "75", CVAR_ARCHIVE );
	cg_weaponFlashes =  trap_Cvar_Get( "cg_weaponFlashes", "2", CVAR_ARCHIVE );

	// wsw
	cg_volume_players = trap_Cvar_Get( "cg_volume_players", "1.0", CVAR_ARCHIVE );
	cg_volume_effects = trap_Cvar_Get( "cg_volume_effects", "1.0", CVAR_ARCHIVE );
	cg_volume_announcer =   trap_Cvar_Get( "cg_volume_announcer", "1.0", CVAR_ARCHIVE );
	cg_volume_hitsound =    trap_Cvar_Get( "cg_volume_hitsound", "1.0", CVAR_ARCHIVE );
	cg_volume_voicechats =  trap_Cvar_Get( "cg_volume_voicechats", "1.0", CVAR_ARCHIVE );
	cg_handOffset =     trap_Cvar_Get( "cg_handOffset", "5", CVAR_ARCHIVE );
	cg_projectileFireTrail =    trap_Cvar_Get( "cg_projectileFireTrail", "140", CVAR_ARCHIVE );
	cg_bloodTrail =     trap_Cvar_Get( "cg_bloodTrail", "10", CVAR_ARCHIVE );
	cg_showBloodTrail = trap_Cvar_Get( "cg_showBloodTrail", "1", CVAR_ARCHIVE );
	cg_projectileFireTrailAlpha =   trap_Cvar_Get( "cg_projectileFireTrailAlpha", "0.45", CVAR_ARCHIVE );
	cg_bloodTrailAlpha =    trap_Cvar_Get( "cg_bloodTrailAlpha", "1.0", CVAR_ARCHIVE );
	cg_explosionsRing = trap_Cvar_Get( "cg_explosionsRing", "0", CVAR_ARCHIVE );
	cg_explosionsDust =    trap_Cvar_Get( "cg_explosionsDust", "0", CVAR_ARCHIVE );
	cg_outlineModels =  trap_Cvar_Get( "cg_outlineModels", "1", CVAR_ARCHIVE );
	cg_outlineWorld =   trap_Cvar_Get( "cg_outlineWorld", "0", CVAR_CHEAT );
	cg_outlinePlayers = trap_Cvar_Get( "cg_outlinePlayers", "1", CVAR_ARCHIVE );
	cg_drawEntityBoxes =    trap_Cvar_Get( "cg_drawEntityBoxes", "0", CVAR_DEVELOPER );
	cg_showObituaries = trap_Cvar_Get( "cg_showObituaries", va( "%i", CG_OBITUARY_HUD | CG_OBITUARY_CENTER ), CVAR_ARCHIVE );
	cg_damageNumbers = trap_Cvar_Get( "cg_damageNumbers", "1", CVAR_ARCHIVE );
	cg_autoaction_demo =    trap_Cvar_Get( "cg_autoaction_demo", "0", CVAR_ARCHIVE );
	cg_autoaction_screenshot =  trap_Cvar_Get( "cg_autoaction_screenshot", "0", CVAR_ARCHIVE );
	cg_autoaction_stats =   trap_Cvar_Get( "cg_autoaction_stats", "0", CVAR_ARCHIVE );
	cg_autoaction_spectator = trap_Cvar_Get( "cg_autoaction_spectator", "0", CVAR_ARCHIVE );
	cg_simpleItems =    trap_Cvar_Get( "cg_simpleItems", "0", CVAR_ARCHIVE );
	cg_simpleItemsSize =    trap_Cvar_Get( "cg_simpleItemsSize", "16", CVAR_ARCHIVE );
	cg_particles =      trap_Cvar_Get( "cg_particles", "1", CVAR_ARCHIVE );

	cg_cartoonEffects =     trap_Cvar_Get( "cg_cartoonEffects", "7", CVAR_ARCHIVE );

	cg_damage_indicator =   trap_Cvar_Get( "cg_damage_indicator", "1", CVAR_ARCHIVE );
	cg_damage_indicator_time =  trap_Cvar_Get( "cg_damage_indicator_time", "25", CVAR_ARCHIVE );

	cg_voiceChats =     trap_Cvar_Get( "cg_voiceChats", "1", CVAR_ARCHIVE );

	cg_projectileAntilagOffset = trap_Cvar_Get( "cg_projectileAntilagOffset", "1.0", CVAR_ARCHIVE );

	cg_raceGhosts =     trap_Cvar_Get( "cg_raceGhosts", "0", CVAR_ARCHIVE );
	cg_raceGhostsAlpha =    trap_Cvar_Get( "cg_raceGhostsAlpha", "0.25", CVAR_ARCHIVE );

	cg_chatFilter =     trap_Cvar_Get( "cg_chatFilter", "0", CVAR_ARCHIVE );

	// developer cvars
	cg_showClamp = trap_Cvar_Get( "cg_showClamp", "0", CVAR_DEVELOPER );

	cg_allyColor = trap_Cvar_Get( "cg_allyColor", "0", CVAR_ARCHIVE );
	cg_allyModel = trap_Cvar_Get( "cg_allyModel", "bigvic", CVAR_ARCHIVE );
	cg_allyForceModel = trap_Cvar_Get( "cg_allyForceModel", "1", CVAR_ARCHIVE | CVAR_READONLY );
	cg_allyModel->modified = true;
	cg_allyForceModel->modified = true;

	cg_enemyColor = trap_Cvar_Get( "cg_enemyColor", "1", CVAR_ARCHIVE );
	cg_enemyModel = trap_Cvar_Get( "cg_enemyModel", "padpork", CVAR_ARCHIVE );
	cg_enemyForceModel = trap_Cvar_Get( "cg_enemyForceModel", "1", CVAR_ARCHIVE | CVAR_READONLY );
	cg_enemyModel->modified = true;
	cg_enemyForceModel->modified = true;

	cg_strafeHUD = trap_Cvar_Get( "cg_strafeHUD", "0", CVAR_ARCHIVE );

	trap_Cvar_Get( "cg_loadout", "", CVAR_ARCHIVE | CVAR_USERINFO );
}

/*
* CG_ValidateItemDef
*
* Compares name and tag against the itemlist to make sure cgame and game lists match
*/
void CG_ValidateItemDef( int tag, char *name ) {
	const gsitem_t *item;

	item = GS_FindItemByName( name );
	if( !item ) {
		CG_Error( "Client/Server itemlist missmatch (Game and Cgame version/mod differs). Item '%s' not found\n", name );
	}

	if( item->tag != tag ) {
		CG_Error( "Client/Server itemlist missmatch (Game and Cgame version/mod differs).\n" );
	}
}

/*
* CG_OverrideWeapondef
*
* Compares name and tag against the itemlist to make sure cgame and game lists match
*/
void CG_OverrideWeapondef( int index, const char *cstring ) {
	int weapon, i;
	int firemode = FIRE_MODE_WEAK;
	gs_weapon_definition_t *weapondef;
	firedef_t *firedef;

	weapon = index;
	if( index >= ( MAX_WEAPONDEFS / 2 ) ) {
		weapon -= ( MAX_WEAPONDEFS / 2 );
		firemode = FIRE_MODE_STRONG;
	}

	weapondef = GS_GetWeaponDef( weapon );
	if( !weapondef ) {
		CG_Error( "CG_OverrideWeapondef: Invalid weapon index\n" );
	}

	firedef = ( firemode == FIRE_MODE_STRONG ) ? &weapondef->firedef : &weapondef->firedef_weak;

	i = sscanf( cstring, "%7i %7i %7u %7u %7u %7u %7u %7i %7i %7i",
				&firedef->usage_count,
				&firedef->projectile_count,
				&firedef->weaponup_time,
				&firedef->weapondown_time,
				&firedef->reload_time,
				&firedef->cooldown_time,
				&firedef->timeout,
				&firedef->speed,
				&firedef->spread,
				&firedef->v_spread
				);

	if( i != 10 ) {
		CG_Error( "CG_OverrideWeapondef: Bad configstring: %s \"%s\" (%i)\n", weapondef->name, cstring, i );
	}
}

/*
* CG_ValidateItemList
*/
static void CG_ValidateItemList( void ) {
	int i;
	int cs;

	for( i = 0; i < MAX_ITEMS; i++ ) {
		cs = CS_ITEMS + i;
		if( cgs.configStrings[cs][0] ) {
			CG_ValidateItemDef( i, cgs.configStrings[cs] );
		}
	}

	for( i = 0; i < MAX_WEAPONDEFS; i++ ) {
		cs = CS_WEAPONDEFS + i;
		if( cgs.configStrings[cs][0] ) {
			CG_OverrideWeapondef( i, cgs.configStrings[cs] );
		}
	}
}

/*
* CG_Precache
*/
void CG_Precache( void ) {
	if( cgs.precacheDone ) {
		return;
	}

	cgs.precacheStart = cgs.precacheCount;
	cgs.precacheStartMsec = trap_Milliseconds();

	CG_RegisterModels();
	if( cgs.precacheModelsStart < MAX_MODELS ) {
		return;
	}

	CG_RegisterSounds();
	if( cgs.precacheSoundsStart < MAX_SOUNDS ) {
		return;
	}

	CG_RegisterShaders();
	if( cgs.precacheShadersStart < MAX_IMAGES ) {
		return;
	}

	CG_RegisterSkinFiles();
	if( cgs.precacheSkinsStart < MAX_SKINFILES ) {
		return;
	}

	CG_RegisterClients();
	if( cgs.precacheClientsStart < MAX_CLIENTS ) {
		return;
	}

	cgs.precacheDone = true;
}

/*
* CG_RegisterConfigStrings
*/
static void CG_RegisterConfigStrings( void ) {
	int i;
	const char *cs;

	cgs.precacheCount = cgs.precacheTotal = 0;

	for( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		trap_GetConfigString( i, cgs.configStrings[i], MAX_CONFIGSTRING_CHARS );

		cs = cgs.configStrings[i];
		if( !cs[0] ) {
			continue;
		}

		if( i == CS_WORLDMODEL ) {
			cgs.precacheTotal++;
		} else if( i >= CS_MODELS && i < CS_MODELS + MAX_MODELS ) {
			cgs.precacheTotal++;
		} else if( i >= CS_SOUNDS && i < CS_SOUNDS + MAX_SOUNDS ) {
			cgs.precacheTotal++;
		} else if( i >= CS_IMAGES && i < CS_IMAGES + MAX_IMAGES ) {
			cgs.precacheTotal++;
		} else if( i >= CS_SKINFILES && i < CS_SKINFILES + MAX_SKINFILES ) {
			cgs.precacheTotal++;
		} else if( i >= CS_PLAYERINFOS && i < CS_PLAYERINFOS + MAX_CLIENTS ) {
			cgs.precacheTotal++;
		}
	}

	// backup initial configstrings for CG_Reset
	memcpy( &cgs.baseConfigStrings[0][0], &cgs.configStrings[0][0], MAX_CONFIGSTRINGS*MAX_CONFIGSTRING_CHARS );

	CG_SC_AutoRecordAction( cgs.configStrings[CS_AUTORECORDSTATE] );
}

/*
* CG_Reset
*/
void CG_Reset( void ) {
	memcpy( &cgs.configStrings[0][0], &cgs.baseConfigStrings[0][0], MAX_CONFIGSTRINGS*MAX_CONFIGSTRING_CHARS );

	CG_ResetClientInfos();

	CG_ResetPModels();

	CG_ResetKickAngles();
	CG_ResetColorBlend();
	CG_ResetDamageIndicator();

	CG_SC_ResetObituaries();

	CG_ClearDecals();
	CG_ClearPolys();
	CG_ClearEffects();
	CG_ClearLocalEntities();

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	CG_ClearInputState();

	CG_ClearPointedNum();

	CG_ClearAwards();

	cg.time = 0;
	cg.realTime = 0;

	chaseCam.key_pressed = false;

	// reset prediction optimization
	cg.predictFrom = 0;

	memset( cg_entities, 0, sizeof( cg_entities ) );
}

/*
* CG_Init
*/
void CG_Init( const char *serverName, unsigned int playerNum,
			  int vidWidth, int vidHeight, float pixelRatio,
			  bool demoplaying, const char *demoName, bool pure,
			  unsigned snapFrameTime, int protocol, const char *demoExtension,
			  int sharedSeed, bool gameStart ) {
	CG_InitGameShared();

	memset( &cg, 0, sizeof( cg_state_t ) );
	memset( &cgs, 0, sizeof( cg_static_t ) );

	memset( cg_entities, 0, sizeof( cg_entities ) );
#ifdef PURE_CHEAT
	CG_Printf( S_COLOR_MAGENTA "Hi, I'm an unpure bitch 7\n" );
#endif

	srand( time( NULL ) );

	// save server name
	cgs.serverName = CG_CopyString( serverName );

	// save local player number
	cgs.playerNum = playerNum;

	// save current width and height
	cgs.vidWidth = vidWidth;
	cgs.vidHeight = vidHeight;
	cgs.pixelRatio = pixelRatio;

	// demo
	cgs.demoPlaying = demoplaying;
	cgs.demoName = demoName;
	Q_strncpyz( cgs.demoExtension, demoExtension, sizeof( cgs.demoExtension ) );

	// whether to only allow pure files
	cgs.pure = pure;

	// game protocol number
	cgs.gameProtocol = protocol;
	cgs.snapFrameTime = snapFrameTime;

	cgs.hasGametypeMenu = false; // this will update as soon as we receive configstrings

	CG_asInitScriptEngine();

	CG_asLoadGameScript();

	CG_InitInput();

	CG_RegisterVariables();
	CG_InitTemporaryBoneposesCache();
	CG_PModelsInit();
	CG_WModelsInit();

	CG_ScreenInit();

	CG_ClearLocalEntities();

	CG_InitDamageNumbers();

	// get configstrings
	CG_RegisterConfigStrings();

	// register fonts here so loading screen works
	CG_RegisterFonts();
	cgs.shaderWhite = trap_R_RegisterPic( "$whiteimage" );

	CG_RegisterCGameCommands();

	CG_ValidateItemList();

	CG_LoadStatusBar();

	CG_LoadingString( "" );

	CG_ClearDecals();
	CG_ClearPolys();
	CG_ClearEffects();

	CG_InitChat( &cg.chat );

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	cg.firstFrame = true; // think of the next frame in CG_NewFrameSnap as of the first one

	// now that we're done with precaching, let the autorecord actions do something
	CG_ConfigString( CS_AUTORECORDSTATE, cgs.configStrings[CS_AUTORECORDSTATE] );

	CG_DemocamInit();
}

void CG_ResizeWindow( int width, int height ) {
	cgs.vidWidth = width;
	cgs.vidHeight = height;
}

/*
* CG_Shutdown
*/
void CG_Shutdown( void ) {
	CG_FreeLocalEntities();
	CG_DemocamShutdown();
	CG_ScreenShutdown();
	CG_UnregisterCGameCommands();
	CG_PModelsShutdown();
	CG_FreeTemporaryBoneposesCache();
	CG_ShutdownInput();
	CG_asUnloadGameScript();
	CG_asShutdownScriptEngine();
}
