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

cvar_t *cg_predict;
cvar_t *cg_predict_optimize;
cvar_t *cg_showMiss;

cvar_t *model;
cvar_t *skin;
cvar_t *hand;
cvar_t *clan;
cvar_t *handicap;

cvar_t *cg_addDecals;

//cvar_t *cg_footSteps;

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

cvar_t *developer;

cvar_t *cg_handOffset;
cvar_t *cg_gun_fov;
cvar_t *cg_gun_alpha;
cvar_t *cg_volume_players;
cvar_t *cg_volume_effects;
cvar_t *cg_volume_announcer;
cvar_t *cg_volume_voicechats;
cvar_t *cg_grenadeTrail;
cvar_t *cg_rocketTrail;
cvar_t *cg_rocketFireTrail;
cvar_t *cg_bloodTrail;
cvar_t *cg_showBloodTrail;
cvar_t *cg_grenadeTrailAlpha;
cvar_t *cg_rocketTrailAlpha;
cvar_t *cg_rocketFireTrailAlpha;
cvar_t *cg_bloodTrailAlpha;
cvar_t *cg_explosionsRing;
cvar_t *cg_explosionsDust;
cvar_t *cg_gibs;
cvar_t *cg_outlineModels;
cvar_t *cg_outlineWorld;
cvar_t *cg_outlinePlayers;
cvar_t *cg_drawEntityBoxes;
cvar_t *cg_fov;
cvar_t *cg_oldMovement;
cvar_t *cg_noAutohop;
cvar_t *cg_zoomSens;
cvar_t *cg_predictLaserBeam;
cvar_t *cg_voiceChats;
cvar_t *cg_shadows;
cvar_t *cg_showSelfShadow;
cvar_t *cg_laserBeamSubdivisions;
cvar_t *cg_projectileAntilagOffset;
cvar_t *cg_raceGhosts;
cvar_t *cg_raceGhostsAlpha;
cvar_t *cg_chatBeep;
cvar_t *cg_chatFilter;
cvar_t *cg_chatFilterTV;

cvar_t *cg_cartoonEffects;
cvar_t *cg_cartoonHitEffect;

cvar_t *cg_volume_hitsound;
cvar_t *cg_autoaction_demo;
cvar_t *cg_autoaction_screenshot;
cvar_t *cg_autoaction_stats;
cvar_t *cg_autoaction_spectator;
cvar_t *cg_simpleItems;
cvar_t *cg_simpleItemsSize;
cvar_t *cg_showObituaries;
cvar_t *cg_particles;
cvar_t *cg_showhelp;
cvar_t *cg_scoreboardStats;
cvar_t *cg_showClamp;

cvar_t *cg_damage_kick;
cvar_t *cg_damage_indicator;
cvar_t *cg_damage_indicator_time;
cvar_t *cg_pickup_flash;

cvar_t *cg_weaponAutoSwitch;

// force models
cvar_t *cg_teamPLAYERSmodel;
cvar_t *cg_teamALPHAmodel;
cvar_t *cg_teamBETAmodel;

cvar_t *cg_teamPLAYERSskin;
cvar_t *cg_teamALPHAskin;
cvar_t *cg_teamBETAskin;

cvar_t *cg_teamPLAYERScolor;
cvar_t *cg_teamALPHAcolor;
cvar_t *cg_teamBETAcolor;

cvar_t *cg_forceMyTeamAlpha;
cvar_t *cg_forceTeamPlayersTeamBeta;
cvar_t *cg_teamColoredBeams;
//cvar_t *cg_teamColorBeamMinimum;

cvar_t *cg_ebbeam_old;
cvar_t *cg_ebbeam_width;
cvar_t *cg_ebbeam_alpha;
cvar_t *cg_ebbeam_time;
cvar_t *cg_instabeam_width;
cvar_t *cg_instabeam_alpha;
cvar_t *cg_instabeam_time;
cvar_t *cg_lgbeam_old;

cvar_t *cg_playList;
cvar_t *cg_playListShuffle;

cvar_t *cg_flashWindowCount;

/*
* CG_API
*/
int CG_API( void )
{
	return CGAME_API_VERSION;
}

/*
* CG_Error
*/
void CG_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

/*
* CG_Printf
*/
void CG_Printf( const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}

/*
* CG_LocalPrint
*/
#define LOCALPRINT_MSG_SIZE 1024
void CG_LocalPrint( bool team, const char *format, ... )
{
	va_list	argptr;
	char msg[LOCALPRINT_MSG_SIZE];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	if( con_chatCGame->integer )
	{
		trap_PrintToLog( msg );
	}
	else
	{
		trap_Print( msg );
	}

	CG_StackChatString( &cg.chat, msg );
}

typedef struct
{
	char *buf;
	size_t buf_size;
	void *privatep;
	void (*done_cb)(int status, const char *resp);
} cg_asyncrequest_t;

/*
* CG_AsyncGetRequest_ReadCb
*/
static size_t CG_AsyncGetRequest_ReadCb( const void *buf, size_t numb, float percentage, 
	int status, const char *contentType, void *privatep )
{
	char *newbuf;
	cg_asyncrequest_t *req = ( cg_asyncrequest_t * )privatep;

	if( status < 0 || status >= 300 ) {
		return 0;
	}

	newbuf = ( char * )CG_Malloc( req->buf_size + numb + 1 );
	memcpy( newbuf, req->buf, req->buf_size - 1 );
	memcpy( newbuf + req->buf_size - 1, buf, numb );
	newbuf[numb] = '\0'; // EOF

	CG_Free( req->buf );
	req->buf = newbuf;
	req->buf_size = req->buf_size + numb + 1;

	return numb;
}

/*
* CG_AsyncGetRequest_DoneCb
*/
static void CG_AsyncGetRequest_DoneCb( int status, const char *contentType, void *privatep )
{
	cg_asyncrequest_t *req = ( cg_asyncrequest_t * )privatep;

	req->done_cb( status, req->buf );

	CG_Free( req->buf );
	CG_Free( req );
}

/*
* CG_AsyncGetRequest
*/
int CG_AsyncGetRequest( const char *resource, void (*done_cb)(int status, const char *resp), void *privatep )
{
	char url[1024];
	cg_asyncrequest_t *req;
	
	trap_GetBaseServerURL( url, sizeof( url ) );
	Q_strncatz( url, resource, sizeof( url ) );

	req = ( cg_asyncrequest_t * )CG_Malloc( sizeof( *req ) );
	req->buf_size = 1;
	req->buf = ( char * )CG_Malloc( 1 );
	*req->buf = '\0';
	req->privatep = privatep;
	req->done_cb = done_cb;
	
	return trap_AsyncStream_PerformRequest( url, "GET", "", 10, 
		CG_AsyncGetRequest_ReadCb, CG_AsyncGetRequest_DoneCb, (void *)req );
}

/*
* CG_GS_Malloc
*
* Used only for gameshared linking
*/
static void *CG_GS_Malloc( size_t size )
{
	return CG_Malloc( size );
}

/*
* CG_GS_Free
*
* Used only for gameshared linking
*/
static void CG_GS_Free( void *data )
{
	CG_Free( data );
}

/*
* CG_GS_Trace
*/
static void CG_GS_Trace( trace_t *t, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int contentmask, int timeDelta )
{
	assert( !timeDelta );
	CG_Trace( t, start, mins, maxs, end, ignore, contentmask );
}

/*
* CG_GS_PointContents
*/
static int CG_GS_PointContents( vec3_t point, int timeDelta )
{
	assert( !timeDelta );
	return CG_PointContents( point );
}

/*
* CG_GS_RoundUpToHullSize
*/
static void CG_GS_RoundUpToHullSize( vec3_t mins, vec3_t maxs )
{
	trap_CM_RoundUpToHullSize( mins, maxs, NULL );
}

/*
* CG_GS_GetEntityState
*/
static entity_state_t *CG_GS_GetEntityState( int entNum, int deltaTime )
{
	centity_t *cent;

	if( entNum == -1 )
		return NULL;

	assert( entNum >= 0 && entNum < MAX_EDICTS );
	cent = &cg_entities[entNum];

	if( cent->serverFrame != cg.frame.serverFrame )
		return NULL;
	return &cent->current;
}

/*
* CG_GS_GetConfigString
*/
static const char *CG_GS_GetConfigString( int index )
{
	if( index < 0 || index >= MAX_CONFIGSTRINGS )
		return NULL;

	return cgs.configStrings[ index ];
}

/*
* CG_InitGameShared
*
* Give gameshared access to some utilities
*/
static void CG_InitGameShared( void )
{
	char cstring[MAX_CONFIGSTRING_CHARS];

	memset( &gs, 0, sizeof( gs_state_t ) );
	gs.module = GS_MODULE_CGAME;
	trap_GetConfigString( CS_MAXCLIENTS, cstring, MAX_CONFIGSTRING_CHARS );
	gs.maxclients = atoi( cstring );
	if( gs.maxclients < 1 || gs.maxclients > MAX_CLIENTS )
		gs.maxclients = MAX_CLIENTS;

	module_PredictedEvent = CG_PredictedEvent;
	module_Error = CG_Error;
	module_Printf = CG_Printf;
	module_Malloc = CG_GS_Malloc;
	module_Free = CG_GS_Free;
	module_Trace = CG_GS_Trace;
	module_GetEntityState = CG_GS_GetEntityState;
	module_PointContents = CG_GS_PointContents;
	module_RoundUpToHullSize = CG_GS_RoundUpToHullSize;
	module_PMoveTouchTriggers = CG_Predict_TouchTriggers;
	module_GetConfigString = CG_GS_GetConfigString;

	GS_InitWeapons();
}

/*
* CG_CopyString
*/
char *_CG_CopyString( const char *in, const char *filename, int fileline )
{
	char *out;

	out = ( char * )trap_MemAlloc( strlen( in ) + 1, filename, fileline );
	strcpy( out, in );
	return out;
}

/*
* CG_InitL10n
*/
static void CG_InitL10n( void )
{
	trap_L10n_ClearDomain();
	trap_L10n_LoadLangPOFile( "l10n/cgame" );
}

/*
* CG_TranslateString
*/
const char *CG_TranslateString( const char *string )
{
	const char *translation;

	translation = trap_L10n_TranslateString( string );
	if( !translation ) {
		return string;
	}
	return translation;
}

/*
* CG_RegisterWeaponModels
*/
static void CG_RegisterWeaponModels( void )
{
	int i;

	for( i = 0; i < cgs.numWeaponModels; i++ )
		cgs.weaponInfos[i] = CG_RegisterWeaponModel( cgs.weaponModels[i], i );

	// special case for weapon 0. Must always load the animation script
	if( !cgs.weaponInfos[0] )
		cgs.weaponInfos[0] = CG_CreateWeaponZeroModel( cgs.weaponModels[0] );
}

/*
* CG_RegisterModels
*/
static void CG_RegisterModels( void )
{
	int i;
	char *name;

	name = cgs.configStrings[CS_WORLDMODEL];
	if( name[0] )
	{
		CG_LoadingItemName( name );
		CG_LoadingString( name );
		trap_R_RegisterWorldModel( name );
	}

	CG_LoadingString( "models" );

	cgs.numWeaponModels = 1;
	Q_strncpyz( cgs.weaponModels[0], "generic/generic.md3", sizeof( cgs.weaponModels[0] ) );

	for( i = 1; i < MAX_MODELS; i++ )
	{
		name = cgs.configStrings[CS_MODELS+i];
		if( !name[0] )
			break;

		if( name[0] == '#' )
		{
			// special player weapon model
			if( cgs.numWeaponModels < WEAP_TOTAL )
			{
				Q_strncpyz( cgs.weaponModels[cgs.numWeaponModels], name+1, sizeof( cgs.weaponModels[cgs.numWeaponModels] ) );
				cgs.numWeaponModels++;
				CG_LoadingItemName( name );
			}
		}
		else if( name[0] == '$' )
		{
			// indexed pmodel
			cgs.pModelsIndex[i] = CG_RegisterPlayerModel( name+1 );
			CG_LoadingItemName( name );
		}
		else
		{
			CG_LoadingItemName( name );
			cgs.modelDraw[i] = CG_RegisterModel( name );
		}
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
static void CG_RegisterSounds( void )
{
	int i;
	char *name;

	CG_LoadingString( "sounds" );

	for( i = 1; i < MAX_SOUNDS; i++ )
	{
		name = cgs.configStrings[CS_SOUNDS+i];
		if( !name[0] )
			break;

		if( name[0] != '*' )
		{
			CG_LoadingItemName( name );
			cgs.soundPrecache[i] = trap_S_RegisterSound( name );
		}
	}

	CG_RegisterMediaSounds();
}

/*
* CG_RegisterShaders
*/
static void CG_RegisterShaders( void )
{
	int i;
	char *name;

	CG_LoadingString( "shaders" );

	for( i = 1; i < MAX_IMAGES; i++ )
	{
		name = cgs.configStrings[CS_IMAGES+i];
		if( !name[0] )
			break;

		CG_LoadingItemName( name );
		cgs.imagePrecache[i] = trap_R_RegisterPic( name );
	}

	CG_RegisterMediaShaders();
}

/*
* CG_RegisterSkinfiles
*/
static void CG_RegisterSkinFiles( void )
{
	int i;
	char *name;

	CG_LoadingString( "skins" );

	for( i = 1; i < MAX_SKINFILES; i++ )
	{
		name = cgs.configStrings[CS_SKINFILES+i];
		if( !name[0] )
			break;

		CG_LoadingItemName( name );
		cgs.skinPrecache[i] = trap_R_RegisterSkinFile( name );
	}
}

/*
* CG_RegisterClients
*/
static void CG_RegisterClients( void )
{
	int i;
	char *name;

	CG_LoadingString( "clients" );

	for( i = 0; i < gs.maxclients; i++ )
	{
		name = cgs.configStrings[CS_PLAYERINFOS+i];
		if( !name[0] )
			continue;

		CG_LoadingItemName( name );
		CG_LoadClientInfo( &cgs.clientInfo[i], name, i );
	}
}

/*
* CG_RegisterLightStyles
*/
static void CG_RegisterLightStyles( void )
{
	int i;
	char *name;

	CG_LoadingString( "lightstyles" );

	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		name = cgs.configStrings[CS_LIGHTS+i];
		if( !name[0] )
			continue;

		CG_LoadingItemName( name );
		CG_SetLightStyle( i );
	}
}

/*
* CG_RegisterVariables
*/
static void CG_RegisterVariables( void )
{
	cg_predict =	    trap_Cvar_Get( "cg_predict", "1", 0 );
	cg_predict_optimize = trap_Cvar_Get( "cg_predict_optimize", "1", 0 );
	cg_showMiss =	    trap_Cvar_Get( "cg_showMiss", "0", 0 );

	cg_debugPlayerModels =	trap_Cvar_Get( "cg_debugPlayerModels", "0", CVAR_CHEAT|CVAR_ARCHIVE );
	cg_debugWeaponModels =	trap_Cvar_Get( "cg_debugWeaponModels", "0", CVAR_CHEAT|CVAR_ARCHIVE );

	model =		    trap_Cvar_Get( "model", DEFAULT_PLAYERMODEL, CVAR_USERINFO | CVAR_ARCHIVE );
	skin =		    trap_Cvar_Get( "skin", DEFAULT_PLAYERSKIN, CVAR_USERINFO | CVAR_ARCHIVE );
	hand =		    trap_Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	handicap =		trap_Cvar_Get( "handicap", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	clan =		    trap_Cvar_Get( "clan", "", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_oldMovement =	trap_Cvar_Get( "cg_oldMovement", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_noAutohop =	trap_Cvar_Get( "cg_noAutohop", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_fov =	    trap_Cvar_Get( "fov", "100", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_zoomSens =	    trap_Cvar_Get( "zoomsens", "0", CVAR_ARCHIVE );

	cg_addDecals =	    trap_Cvar_Get( "cg_decals", "1", CVAR_ARCHIVE );
	//cg_footSteps =	    trap_Cvar_Get( "cg_footSteps", "1", 0 );

	cg_thirdPerson =	trap_Cvar_Get( "cg_thirdPerson", "0", CVAR_CHEAT );
	cg_thirdPersonAngle =	trap_Cvar_Get( "cg_thirdPersonAngle", "0", 0 );
	cg_thirdPersonRange =	trap_Cvar_Get( "cg_thirdPersonRange", "90", 0 );

	//skelmod
	cg_gun =		trap_Cvar_Get( "cg_gun", "1", CVAR_ARCHIVE );
	cg_gunx =		trap_Cvar_Get( "cg_gunx", "0", CVAR_ARCHIVE );
	cg_guny =		trap_Cvar_Get( "cg_guny", "0", CVAR_ARCHIVE );
	cg_gunz =		trap_Cvar_Get( "cg_gunz", "0", CVAR_ARCHIVE );
	cg_gunbob =		trap_Cvar_Get( "cg_gunbob", "1", CVAR_ARCHIVE );

	cg_gun_fov =		trap_Cvar_Get( "cg_gun_fov", "90", CVAR_ARCHIVE );
	cg_gun_alpha =		trap_Cvar_Get( "cg_gun_alpha", "1", CVAR_ARCHIVE );
	cg_weaponFlashes =	trap_Cvar_Get( "cg_weaponFlashes", "2", CVAR_ARCHIVE );

	// wsw
	cg_volume_players =	trap_Cvar_Get( "cg_volume_players", "1.0", CVAR_ARCHIVE );
	cg_volume_effects =	trap_Cvar_Get( "cg_volume_effects", "1.0", CVAR_ARCHIVE );
	cg_volume_announcer =	trap_Cvar_Get( "cg_volume_announcer", "1.0", CVAR_ARCHIVE );
	cg_volume_hitsound =	trap_Cvar_Get( "cg_volume_hitsound", "1.0", CVAR_ARCHIVE );
	cg_volume_voicechats =	trap_Cvar_Get( "cg_volume_voicechats", "1.0", CVAR_ARCHIVE );
	cg_handOffset =		trap_Cvar_Get( "cg_handOffset", "5", CVAR_ARCHIVE );
	cg_rocketTrail =	trap_Cvar_Get( "cg_rocketTrail", "40", CVAR_ARCHIVE );
	cg_rocketFireTrail =	trap_Cvar_Get( "cg_rocketFireTrail", "90", CVAR_ARCHIVE );
	cg_grenadeTrail =	trap_Cvar_Get( "cg_grenadeTrail", "20", CVAR_ARCHIVE );
	cg_bloodTrail =		trap_Cvar_Get( "cg_bloodTrail", "10", CVAR_ARCHIVE );
	cg_showBloodTrail =	trap_Cvar_Get( "cg_showBloodTrail", "1", CVAR_ARCHIVE );
	cg_rocketTrailAlpha =	trap_Cvar_Get( "cg_rocketTrailAlpha", "0.35", CVAR_ARCHIVE );
	cg_rocketFireTrailAlpha =	trap_Cvar_Get( "cg_rocketFireTrailAlpha", "0.45", CVAR_ARCHIVE );
	cg_grenadeTrailAlpha =	trap_Cvar_Get( "cg_grenadeTrailAlpha", "0.5", CVAR_ARCHIVE );
	cg_bloodTrailAlpha =	trap_Cvar_Get( "cg_bloodTrailAlpha", "1.0", CVAR_ARCHIVE );
	cg_explosionsRing =	trap_Cvar_Get( "cg_explosionsRing", "0", CVAR_ARCHIVE );
	cg_explosionsDust =    trap_Cvar_Get( "cg_explosionsDust", "0", CVAR_ARCHIVE );
	cg_gibs =		trap_Cvar_Get( "cg_gibs", "1", CVAR_ARCHIVE );
	cg_outlineModels =	trap_Cvar_Get( "cg_outlineModels", "1", CVAR_ARCHIVE );
	cg_outlineWorld =	trap_Cvar_Get( "cg_outlineWorld", "0", CVAR_ARCHIVE );
	cg_outlinePlayers =	trap_Cvar_Get( "cg_outlinePlayers", "1", CVAR_ARCHIVE );
	cg_drawEntityBoxes =	trap_Cvar_Get( "cg_drawEntityBoxes", "0", CVAR_DEVELOPER );
	cg_showObituaries =	trap_Cvar_Get( "cg_showObituaries", va( "%i", CG_OBITUARY_HUD|CG_OBITUARY_CENTER ), CVAR_ARCHIVE );
	cg_autoaction_demo =	trap_Cvar_Get( "cg_autoaction_demo", "0", CVAR_ARCHIVE );
	cg_autoaction_screenshot =  trap_Cvar_Get( "cg_autoaction_screenshot", "0", CVAR_ARCHIVE );
	cg_autoaction_stats =	trap_Cvar_Get( "cg_autoaction_stats", "0", CVAR_ARCHIVE );
	cg_autoaction_spectator = trap_Cvar_Get( "cg_autoaction_spectator", "0", CVAR_ARCHIVE );
	cg_simpleItems =	trap_Cvar_Get( "cg_simpleItems", "0", CVAR_ARCHIVE );
	cg_simpleItemsSize =	trap_Cvar_Get( "cg_simpleItemsSize", "12", CVAR_ARCHIVE );
	cg_particles =		trap_Cvar_Get( "cg_particles", "1", CVAR_ARCHIVE );
	cg_showhelp =		trap_Cvar_Get( "cg_showhelp", "1", CVAR_ARCHIVE );
	cg_predictLaserBeam =	trap_Cvar_Get( "cg_predictLaserBeam", "1", CVAR_ARCHIVE );
	cg_showSelfShadow =	trap_Cvar_Get( "cg_showSelfShadow", "0", CVAR_ARCHIVE );

	cg_cartoonEffects =		trap_Cvar_Get( "cg_cartoonEffects", "7", CVAR_ARCHIVE );
	cg_cartoonHitEffect =	trap_Cvar_Get( "cg_cartoonHitEffect", "0", CVAR_ARCHIVE );

	cg_damage_kick =	trap_Cvar_Get( "cg_damage_kick", "0", CVAR_ARCHIVE );
	cg_damage_indicator =	trap_Cvar_Get( "cg_damage_indicator", "1", CVAR_ARCHIVE );
	cg_damage_indicator_time =	trap_Cvar_Get( "cg_damage_indicator_time", "50", CVAR_ARCHIVE );
	cg_pickup_flash =	trap_Cvar_Get( "cg_pickup_flash", "0", CVAR_ARCHIVE );

	cg_weaponAutoSwitch =	trap_Cvar_Get( "cg_weaponAutoSwitch", "2", CVAR_ARCHIVE );

	cg_voiceChats =		trap_Cvar_Get( "cg_voiceChats", "1", CVAR_ARCHIVE );
	cg_shadows =		trap_Cvar_Get( "cg_shadows", "1", CVAR_ARCHIVE );

	cg_laserBeamSubdivisions = trap_Cvar_Get( "cg_laserBeamSubdivisions", "10", CVAR_ARCHIVE );
	cg_projectileAntilagOffset = trap_Cvar_Get( "cg_projectileAntilagOffset", "1.0", CVAR_ARCHIVE );

	cg_raceGhosts =		trap_Cvar_Get( "cg_raceGhosts", "0", CVAR_ARCHIVE );
	cg_raceGhostsAlpha =	trap_Cvar_Get( "cg_raceGhostsAlpha", "0.25", CVAR_ARCHIVE );

	cg_chatBeep =		trap_Cvar_Get( "cg_chatBeep", "1", CVAR_ARCHIVE );
	cg_chatFilter =		trap_Cvar_Get( "cg_chatFilter", "0", CVAR_ARCHIVE );
	cg_chatFilterTV =	trap_Cvar_Get( "cg_chatFilterTV", "2", CVAR_ARCHIVE );

	cg_scoreboardStats =	trap_Cvar_Get( "cg_scoreboardStats", "1", CVAR_ARCHIVE );

	// developer cvars
	developer =		trap_Cvar_Get( "developer", "0", CVAR_CHEAT );
	cg_showClamp =		trap_Cvar_Get( "cg_showClamp", "0", CVAR_DEVELOPER );

	//team models
	cg_teamPLAYERSmodel =	trap_Cvar_Get( "cg_teamPLAYERSmodel", "", CVAR_ARCHIVE );
	cg_teamPLAYERSskin =	trap_Cvar_Get( "cg_teamPLAYERSskin", "default", CVAR_ARCHIVE );
	cg_teamPLAYERScolor =	trap_Cvar_Get( "cg_teamPLAYERScolor", "", CVAR_ARCHIVE );
	cg_teamPLAYERSmodel->modified = qtrue;
	cg_teamPLAYERSskin->modified = qtrue;
	cg_teamPLAYERScolor->modified = qtrue;

	cg_teamALPHAmodel =	trap_Cvar_Get( "cg_teamALPHAmodel", "", CVAR_ARCHIVE );
	cg_teamALPHAskin =	trap_Cvar_Get( "cg_teamALPHAskin", "default", CVAR_ARCHIVE );
	cg_teamALPHAcolor =	trap_Cvar_Get( "cg_teamALPHAcolor", DEFAULT_TEAMALPHA_COLOR, CVAR_ARCHIVE );
	cg_teamALPHAmodel->modified = qtrue;
	cg_teamALPHAskin->modified = qtrue;
	cg_teamALPHAcolor->modified = qtrue;

	cg_teamBETAmodel =	trap_Cvar_Get( "cg_teamBETAmodel", "", CVAR_ARCHIVE );
	cg_teamBETAskin =	trap_Cvar_Get( "cg_teamBETAskin", "default", CVAR_ARCHIVE );
	cg_teamBETAcolor =	trap_Cvar_Get( "cg_teamBETAcolor", DEFAULT_TEAMBETA_COLOR, CVAR_ARCHIVE );
	cg_teamBETAmodel->modified = qtrue;
	cg_teamBETAskin->modified = qtrue;
	cg_teamBETAcolor->modified = qtrue;

	cg_forceMyTeamAlpha =		trap_Cvar_Get( "cg_forceMyTeamAlpha", "0", CVAR_ARCHIVE );
	cg_forceTeamPlayersTeamBeta =	trap_Cvar_Get( "cg_forceTeamPlayersTeamBeta", "0", CVAR_ARCHIVE );

	// dmh - learn0more's team colored beams
	cg_teamColoredBeams = trap_Cvar_Get( "cg_teamColoredBeams", "1", CVAR_ARCHIVE );

	cg_ebbeam_old = trap_Cvar_Get( "cg_ebbeam_old", "0", CVAR_ARCHIVE );
	cg_ebbeam_width = trap_Cvar_Get( "cg_ebbeam_width", "64", CVAR_ARCHIVE );
	cg_ebbeam_alpha = trap_Cvar_Get( "cg_ebbeam_alpha", "0.4", CVAR_ARCHIVE );
	cg_ebbeam_time = trap_Cvar_Get( "cg_ebbeam_time", "0.6", CVAR_ARCHIVE );
	cg_lgbeam_old = trap_Cvar_Get( "cg_lgbeam_old", "0", CVAR_ARCHIVE );

	cg_instabeam_width = trap_Cvar_Get( "cg_instabeam_width", "7", CVAR_ARCHIVE );
	cg_instabeam_alpha = trap_Cvar_Get( "cg_instabeam_alpha", "0.4", CVAR_ARCHIVE );
	cg_instabeam_time = trap_Cvar_Get( "cg_instabeam_time", "0.4", CVAR_ARCHIVE );

	cg_showminimap = trap_Cvar_Get( "cg_showMiniMap", "0", CVAR_ARCHIVE );
	cg_showitemtimers = trap_Cvar_Get( "cg_showItemTimers", "3", CVAR_ARCHIVE );
	cg_placebo =  trap_Cvar_Get( "cg_placebo", "0", CVAR_ARCHIVE );
	cg_strafeHUD = trap_Cvar_Get( "cg_strafeHUD", "0", CVAR_ARCHIVE );

	cg_playList = trap_Cvar_Get( "cg_playList", S_PLAYLIST_MATCH, CVAR_ARCHIVE );
	cg_playListShuffle = trap_Cvar_Get( "cg_playListShuffle", "1", CVAR_ARCHIVE );

	cg_flashWindowCount = trap_Cvar_Get( "cg_flashWindowCount", "4", CVAR_ARCHIVE );
}

/*
* CG_ValidateItemDef
*
* Compares name and tag against the itemlist to make sure cgame and game lists match
*/
void CG_ValidateItemDef( int tag, char *name )
{
	gsitem_t *item;

	item = GS_FindItemByName( name );
	if( !item )
		CG_Error( "Client/Server itemlist missmatch (Game and Cgame version/mod differs). Item '%s' not found\n", name );

	if( item->tag != tag )
		CG_Error( "Client/Server itemlist missmatch (Game and Cgame version/mod differs).\n" );
}

/*
* CG_OverrideWeapondef
*
* Compares name and tag against the itemlist to make sure cgame and game lists match
*/
void CG_OverrideWeapondef( int index, const char *cstring )
{
	int weapon, i;
	int firemode = FIRE_MODE_WEAK;
	gs_weapon_definition_t *weapondef;
	firedef_t *firedef;

	weapon = index;
	if( index >= (MAX_WEAPONDEFS / 2) )
	{
		weapon -= (MAX_WEAPONDEFS / 2);
		firemode = FIRE_MODE_STRONG;
	}

	weapondef = GS_GetWeaponDef( weapon );
	if( !weapondef )
		CG_Error( "CG_OverrideWeapondef: Invalid weapon index\n" );

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

	if( i != 10 )
		CG_Error( "CG_OverrideWeapondef: Bad configstring: %s \"%s\" (%i)\n", weapondef->name, cstring, i );
}

/*
* CG_ValidateItemList
*/
static void CG_ValidateItemList( void )
{
	int i;

	for( i = CS_ITEMS; i < CS_ITEMS+MAX_ITEMS; i++ )
	{
		if( cgs.configStrings[i][0] )
		{
			CG_LoadingItemName( cgs.configStrings[i] );
			CG_ValidateItemDef( i - CS_ITEMS, cgs.configStrings[i] );
		}
	}

	for( i = CS_WEAPONDEFS; i < CS_WEAPONDEFS + MAX_WEAPONDEFS; i++ )
	{
		if( cgs.configStrings[i][0] )
		{
			CG_LoadingItemName( cgs.configStrings[i] );
			CG_OverrideWeapondef( i - CS_WEAPONDEFS, cgs.configStrings[i] );
		}
	}
}

/*
* CG_UpdateTVServerString
*/
void CG_UpdateTVServerString( void )
{
	// if we got the server settings configstring, update our local copy of the data
	if( cgs.configStrings[CS_TVSERVER][0] )
	{
		char *settings = cgs.configStrings[CS_TVSERVER];

		cgs.tv = atoi( COM_Parse( &settings ) ) == 0 ? false : true;
		if( cgs.demoPlaying )
			cgs.tv = false;		// ignore the TV bit in demos
	}
}

/*
* CG_RegisterConfigStrings
*/
static void CG_RegisterConfigStrings( void )
{
	int i;
	const char *cs;

	cg.precacheCount = cg.precacheTotal = 0;

	for( i = 0; i < CS_GENERAL; i++ )
	{
		trap_GetConfigString( i, cgs.configStrings[i], MAX_CONFIGSTRING_CHARS );

		cs = cgs.configStrings[i];
		if( !cs[0] ) {
			continue;
		}

		if( i == CS_WORLDMODEL )
		{
			cg.precacheTotal++;
		}
		else if( i >= CS_MODELS )
		{
			if( i >= CS_LOCATIONS && i < CS_LOCATIONS + MAX_LOCATIONS )
				continue;

			if( ( i >= CS_SOUNDS && i < CS_SOUNDS + MAX_SOUNDS ) && ( cs[0] == '*' ) )
				continue;

			cg.precacheTotal++;
		}
	}

	// if we got the server settings configstring, update our local copy of the data
	CG_UpdateTVServerString();

	GS_SetGametypeName( cgs.configStrings[CS_GAMETYPENAME] );

	trap_Cmd_ExecuteText( EXEC_NOW, va( "exec configs/client/%s.cfg silent", gs.gametypeName ) );

	CG_SC_AutoRecordAction( cgs.configStrings[i] );
}

/*
* CG_StartBackgroundTrack
*/
void CG_StartBackgroundTrack( void )
{
	char *string;
	char intro[MAX_QPATH], loop[MAX_QPATH];

	string = cgs.configStrings[CS_AUDIOTRACK];
	Q_strncpyz( intro, COM_Parse( &string ), sizeof( intro ) );
	Q_strncpyz( loop, COM_Parse( &string ), sizeof( loop ) );

	if( intro[0] )
		trap_S_StartBackgroundTrack( intro, loop );
	else if( cg_playList->string[0] )
		trap_S_StartBackgroundTrack( cg_playList->string, cg_playListShuffle->integer ? "1" : "0" );
}

/*
* CG_Reset
*/
void CG_Reset( void )
{
	CG_ResetPModels();

	CG_ResetKickAngles();
	CG_ResetColorBlend();
	CG_ResetDamageIndicator();
	CG_ResetItemTimers();

	CG_ClearDecals();
	CG_ClearPolys();
	CG_ClearEffects();
	CG_ClearLocalEntities();

	CG_DemocamReset();

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	cg.time = 0;
	cg.realTime = 0;

	// reset prediction optimization
	cg.predictFrom = 0;

	memset( cg_entities, 0, sizeof( cg_entities ) );
}

/*
* CG_Init
*/
void CG_Init( const char *serverName, unsigned int playerNum, int vidWidth, int vidHeight, 
			 qboolean demoplaying, const char *demoName, qboolean pure, 
			 unsigned int snapFrameTime, int protocol, int sharedSeed )
{
	CG_InitGameShared();

	memset( &cg, 0, sizeof( cg_state_t ) );
	memset( &cgs, 0, sizeof( cg_static_t ) );

	memset( cg_entities, 0, sizeof( cg_entities ) );
#ifdef PURE_CHEAT
	CG_Printf( S_COLOR_MAGENTA"Hi, I'm an unpure bitch 7\n" );
#endif

	// save server name
	cgs.serverName = CG_CopyString( serverName );

	// save local player number
	cgs.playerNum = playerNum;

	// save current width and height
	cgs.vidWidth = vidWidth;
	cgs.vidHeight = vidHeight;

	// demo
	cgs.demoPlaying = demoplaying == qtrue;
	cgs.demoName = demoName;

	// whether to only allow pure files
	cgs.pure = pure == qtrue;

	// whether we are connected to a tv-server
	cgs.tv = false;
	cgs.tvRequested = false;

	// game protocol number
	cgs.gameProtocol = protocol;
	cgs.snapFrameTime = snapFrameTime;

	cgs.initialSharedSeed = sharedSeed;
	cg.sharedSeed = cgs.initialSharedSeed;

	cgs.hasGametypeMenu = false; // this will update as soon as we receive configstrings

	CG_RegisterVariables();
	CG_InitTemporaryBoneposesCache();
	CG_PModelsInit();

	CG_ScreenInit();

	// get configstrings
	CG_RegisterConfigStrings();

	// register fonts here so loading screen works
	CG_RegisterFonts();
	cgs.shaderWhite = trap_R_RegisterPic( "$whiteimage" );

	// l10n
	CG_InitL10n();

	CG_RegisterLevelMinimap();

	CG_RegisterModels();
	CG_RegisterSounds();
	CG_RegisterShaders();
	CG_RegisterSkinFiles();
	CG_RegisterClients();

	CG_RegisterCGameCommands();

	CG_ValidateItemList();

	CG_LoadStatusBar();

	CG_LoadingString( "" );

	CG_ClearDecals();
	CG_ClearPolys();
	CG_ClearEffects();
	CG_ClearLocalEntities();

	CG_InitChat( &cg.chat );

	CG_RegisterLightStyles();

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	cgs.precacheDone = true;

	cgs.demoTutorial = cgs.demoPlaying && (strstr( cgs.demoName, "tutorials/" ) != NULL);

	cg.firstFrame = true; // think of the next frame in CG_NewFrameSnap as of the first one

	// now that we're done with precaching, let the autorecord actions do something
	CG_ConfigString( CS_AUTORECORDSTATE, cgs.configStrings[CS_AUTORECORDSTATE] );

	CG_DemocamInit();
}

/*
* CG_Shutdown
*/
void CG_Shutdown( void )
{
	CG_DemocamShutdown();
	CG_ScreenShutdown();
	CG_UnregisterCGameCommands();
}

//======================================================================

#ifndef CGAME_HARD_LINKED
// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

void Com_Printf( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}
#endif
