/*
Copyright (C) 2006 Pekka Lampila ("Medar"), Damien Deville ("Pb")
and German Garcia Fernandez ("Jal") for Chasseur de bots association.


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
* Warsow hud scripts
*/

#include "cg_local.h"

extern cvar_t *cg_debugHUD;
extern cvar_t *cg_clientHUD;
extern cvar_t *cg_specHUD;

cvar_t *cg_strafeHUD;

//=============================================================================

enum { DEFAULTSCALE=0, NOSCALE, SCALEBYWIDTH, SCALEBYHEIGHT };

typedef struct
{
	const char *name;
	int value;
} constant_numeric_t;

static const constant_numeric_t cg_numeric_constants[] = {
	{ "NOTSET", STAT_NOTSET },

	// teams
	{ "TEAM_SPECTATOR", TEAM_SPECTATOR },
	{ "TEAM_PLAYERS", TEAM_PLAYERS },
	{ "TEAM_ALPHA", TEAM_ALPHA },
	{ "TEAM_BETA", TEAM_BETA },
	{ "TEAM_ALLY", TEAM_ALLY },
	{ "TEAM_ENEMY", TEAM_ENEMY },

	// align
	{ "LEFT", 1 },
	{ "CENTER", 2 },
	{ "RIGHT", 3 },
	{ "TOP", 1 },
	{ "MIDDLE", 2 },
	{ "BOTTOM", 3 },

	{ "WIDTH", 800 },
	{ "HEIGHT", 600 },

	// scale
	{ "DEFAULTSCALE", DEFAULTSCALE },
	{ "NOSCALE", NOSCALE },
	{ "SCALEBYWIDTH", SCALEBYWIDTH },
	{ "SCALEBYHEIGHT", SCALEBYHEIGHT },

	// match states
	{ "MATCH_STATE_NONE", MATCH_STATE_NONE },
	{ "MATCH_STATE_WARMUP", MATCH_STATE_WARMUP },
	{ "MATCH_STATE_COUNTDOWN", MATCH_STATE_COUNTDOWN },
	{ "MATCH_STATE_PLAYTIME", MATCH_STATE_PLAYTIME },
	{ "MATCH_STATE_POSTMATCH", MATCH_STATE_POSTMATCH },
	{ "MATCH_STATE_WAITEXIT", MATCH_STATE_WAITEXIT },

	// weapon
	{ "WEAP_GUNBLADE", WEAP_GUNBLADE },
	{ "WEAP_MACHINEGUN", WEAP_MACHINEGUN },
	{ "WEAP_RIOTGUN", WEAP_RIOTGUN },
	{ "WEAP_GRENADELAUNCHER", WEAP_GRENADELAUNCHER },
	{ "WEAP_ROCKETLAUNCHER", WEAP_ROCKETLAUNCHER },
	{ "WEAP_PLASMAGUN", WEAP_PLASMAGUN },
	{ "WEAP_LASERGUN", WEAP_LASERGUN },
	{ "WEAP_ELECTROBOLT", WEAP_ELECTROBOLT },

	{ "NOGUN", 0 },
	{ "GUN", 1 },

	// player movement types
	{ "PMOVE_TYPE_NORMAL", PM_NORMAL },
	{ "PMOVE_TYPE_SPECTATOR", PM_SPECTATOR },
	{ "PMOVE_TYPE_GIB", PM_GIB },
	{ "PMOVE_TYPE_FREEZE", PM_FREEZE },
	{ "PMOVE_TYPE_CHASECAM", PM_CHASECAM },

	// config strings
	{ "TEAM_SPECTATOR_NAME", CS_TEAM_SPECTATOR_NAME },
	{ "TEAM_PLAYERS_NAME", CS_TEAM_PLAYERS_NAME },
	{ "TEAM_ALPHA_NAME", CS_TEAM_ALPHA_NAME },
	{ "TEAM_BETA_NAME", CS_TEAM_BETA_NAME },

	{ "BombProgress_Nothing", BombProgress_Nothing },
	{ "BombProgress_Planting", BombProgress_Planting },
	{ "BombProgress_Defusing", BombProgress_Defusing },

	{ "RoundType_Normal", RoundType_Normal },
	{ "RoundType_MatchPoint", RoundType_MatchPoint },
	{ "RoundType_Overtime", RoundType_Overtime },
	{ "RoundType_OvertimeMatchPoint", RoundType_OvertimeMatchPoint },

	{ NULL, 0 }
};

//=============================================================================

static int CG_GetStatValue( const void *parameter ) {
	assert( (intptr_t)parameter >= 0 && (intptr_t)parameter < MAX_STATS );

	return cg.predictedPlayerState.stats[(intptr_t)parameter];
}

static int CG_GetLayoutStatFlag( const void *parameter ) {
	return ( cg.predictedPlayerState.stats[STAT_LAYOUTS] & (intptr_t)parameter ) ? 1 : 0;
}

static int CG_GetPOVnum( const void *parameter ) {
	return ( cg.predictedPlayerState.POVnum != cgs.playerNum + 1 ) ? cg.predictedPlayerState.POVnum : STAT_NOTSET;
}

static float _getspeed( void ) {
	vec3_t hvel;

	VectorSet( hvel, cg.predictedPlayerState.pmove.velocity[0], cg.predictedPlayerState.pmove.velocity[1], 0 );

	return VectorLength( hvel );
}

static int CG_GetSpeed( const void *parameter ) {
	return (int)_getspeed();
}

static int CG_GetSpeedVertical( const void *parameter ) {
	return cg.predictedPlayerState.pmove.velocity[2];
}

static int CG_GetFPS( const void *parameter ) {
#define FPSSAMPLESCOUNT 32
#define FPSSAMPLESMASK ( FPSSAMPLESCOUNT - 1 )
	int i;
	int fps;
	static int frameTimes[FPSSAMPLESCOUNT];
	float avFrameTime;

	if( cg_showFPS->modified ) {
		memset( frameTimes, 0, sizeof( frameTimes ) );
		cg_showFPS->modified = false;
	}

	if( cg_showFPS->integer == 1 ) {
		frameTimes[cg.frameCount & FPSSAMPLESMASK] = trap_R_GetAverageFrametime();
	} else {
		frameTimes[cg.frameCount & FPSSAMPLESMASK] = cg.realFrameTime;
	}

	for( avFrameTime = 0.0f, i = 0; i < FPSSAMPLESCOUNT; i++ ) {
		avFrameTime += frameTimes[( cg.frameCount - i ) & FPSSAMPLESMASK];
	}
	avFrameTime /= FPSSAMPLESCOUNT;
	fps = (int)( 1000.0f / avFrameTime + 0.5f );

	return fps;
}

static int CG_GetPowerupTime( const void *parameter ) {
	int powerup = (intptr_t)parameter;
	return cg.predictedPlayerState.inventory[powerup];
}

static int CG_GetMatchState( const void *parameter ) {
	return GS_MatchState();
}

static int CG_GetMatchDuration( const void *parameter ) {
	return GS_MatchDuration();
}

static int CG_GetOvertime( const void *parameter ) {
	return GS_MatchExtended();
}

static int CG_GetTeamBased( const void *parameter ) {
	return GS_TeamBasedGametype();
}

static int CG_InvidualGameType( const void *parameter ) {
	return GS_InvidualGameType();
}

static int CG_RaceGameType( const void *parameter ) {
	return GS_RaceGametype();
}

static int CG_Paused( const void *parameter ) {
	return GS_MatchPaused();
}

static int CG_GetZoom( const void *parameter ) {
	return ( !cg.view.thirdperson && ( cg.predictedPlayerState.pmove.stats[PM_STAT_ZOOMTIME] != 0 ) );
}

static int CG_GetVidWidth( const void *parameter ) {
	return cgs.vidWidth;
}

static int CG_GetVidHeight( const void *parameter ) {
	return cgs.vidHeight;
}

static int CG_GetCvar( const void *parameter ) {
	return trap_Cvar_Value( (const char *)parameter );
}

static int CG_GetDamageIndicatorDirValue( const void *parameter ) {
	float frac = 0;
	int index = (intptr_t)parameter;

	if( cg.damageBlends[index] > cg.time && !cg.view.thirdperson ) {
		frac = ( cg.damageBlends[index] - cg.time ) / 300.0f;
		clamp( frac, 0.0f, 1.0f );
	}

	return frac * 1000;
}

static int CG_GetCurrentWeaponInventoryData( const void *parameter ) {
	gs_weapon_definition_t *weapondef = GS_GetWeaponDef( cg.predictedPlayerState.stats[STAT_WEAPON] );
	firedef_t *firedef;
	int result;

	switch( (intptr_t)parameter ) {
		case 0: // AMMO_ITEM
		default:
			firedef = GS_FiredefForPlayerState( &cg.predictedPlayerState, cg.predictedPlayerState.stats[STAT_WEAPON] );
			result = firedef->ammo_id;
			break;
		case 1: // STRONG AMMO COUNT
			result = cg.predictedPlayerState.inventory[weapondef->firedef.ammo_id];
			break;
		case 2: // LOW AMMO THRESHOLD
			result = weapondef->firedef.ammo_low;
			break;
	}

	return result;
}

/**
 * Returns whether the weapon should be displayed in the weapon list on the HUD
 * (if the player either has the weapon ammo for it).
 *
 * @param weapon weapon item ID
 * @return whether to display the weapon
 */
static bool CG_IsWeaponInList( int weapon ) {
	bool hasWeapon = cg.predictedPlayerState.inventory[weapon] != 0;
	bool hasAmmo = cg.predictedPlayerState.inventory[weapon - WEAP_GUNBLADE + AMMO_GUNBLADE];

	if( weapon == WEAP_GUNBLADE ) { // gunblade always has 1 ammo when it's strong, but the player doesn't necessarily have it
		return hasWeapon;
	}

	return hasWeapon || hasAmmo;
}

static int CG_GetWeaponCount( const void *parameter ) {
	int i, n = 0;
	for( i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
		if( CG_IsWeaponInList( i ) ) {
			n++;
		}
	}
	return n;
}

static int CG_GetPmoveType( const void *parameter ) {
	// the real pmove type of the client, which is chasecam or spectator when playing a demo
	return cg.frame.playerState.pmove.pm_type;
}

static int CG_IsDemoPlaying( const void *parameter ) {
	return ( cgs.demoPlaying ? 1 : 0 );
}

static int CG_DownloadInProgress( const void *parameter ) {
	const char *str;

	str = trap_Cvar_String( "cl_download_name" );
	if( str[0] ) {
		return 1;
	}
	return 0;
}

// ch : backport some of racesow hud elements
/*********************************************************************************
lm: edit for race mod,
    adds bunch of vars to the hud.

*********************************************************************************/

//lm: for readability
enum race_index {
	mouse_x,
	mouse_y,
	jumpspeed,
	move_an,
	diff_an,
	strafe_an,
	max_index
};

static int CG_GetRaceVars( const void* parameter ) {
	int index = (intptr_t)parameter;
	int iNum;
	vec3_t hor_vel, view_dir, an;

	if( GS_MatchState() != MATCH_STATE_WARMUP && !GS_RaceGametype() ) {
		return 0;
	}

	switch( index ) {
		case diff_an:

			// difference of look and move angles
			hor_vel[0] = cg.predictedPlayerState.pmove.velocity[0];
			hor_vel[1] = cg.predictedPlayerState.pmove.velocity[1];
			hor_vel[2] = 0;
			VecToAngles( hor_vel, an );
			AngleVectors( cg.predictedPlayerState.viewangles, view_dir, NULL, NULL );
			iNum = Q_rint( 100 * ( cg.predictedPlayerState.viewangles[YAW] - an[YAW] ) );
			while( iNum > 18000 )
				iNum -= 36000;
			while( iNum < -18000 )
				iNum += 36000;

			// ch : check if player is moving backwards so iNum wont wrap around
			if( DotProduct( hor_vel, view_dir ) >= 0.0 ) {
				return iNum;
			} else if( iNum < 0 ) {
				return 18000 + iNum;
			} else {
				return -18000 + iNum;
			}

		case strafe_an:

			// optimal strafing angle
			iNum = Q_rint( 100 * ( acos( ( 320 - 0.32f * (float)cg.realFrameTime ) / CG_GetSpeed( 0 ) ) * 180 / M_PI - 45 ) ); //maybe need to check if speed below 320 is allowed for acos
			if( iNum > 0 ) {
				return iNum;
			} else {
				return 0;
			}
		case move_an:

			// angle of current moving direction
			hor_vel[0] = cg.predictedPlayerState.pmove.velocity[0];
			hor_vel[1] = cg.predictedPlayerState.pmove.velocity[1];
			hor_vel[2] = 0;
			VecToAngles( hor_vel, an );
			iNum = Q_rint( 100 * an[YAW] );
			while( iNum > 18000 )
				iNum -= 36000;
			while( iNum < -18000 )
				iNum += 36000;
			return iNum;
		case mouse_x:
			return Q_rint( 100 * cg.predictedPlayerState.viewangles[YAW] );
		case mouse_y:
			return Q_rint( 100 * cg.predictedPlayerState.viewangles[PITCH] );
		default:
			return STAT_NOTSET;
	}
}

static int CG_GetAccel( const void* parameter ) {
#define ACCEL_SAMPLE_COUNT 16
#define ACCEL_SAMPLE_MASK ( ACCEL_SAMPLE_COUNT - 1 )
	int i;
	float t, dt;
	float accel;
	float newSpeed;
	static float oldSpeed = 0.0;
	static float oldTime = 0.0;
	static float accelHistory[ACCEL_SAMPLE_COUNT] = {0.0};
	static int sampleCount = 0;

	t = cg.realTime * 0.001f;
	dt = t - oldTime;
	if( dt > 0.0 ) {
		// raw acceleration
		newSpeed = _getspeed();
		accel = ( newSpeed - oldSpeed ) / dt;
		accelHistory[sampleCount & ACCEL_SAMPLE_MASK] = accel;
		sampleCount++;
		oldSpeed = newSpeed;
		oldTime = t;
	}

	// average accel for n frames (TODO: emphasis on later frames)
	accel = 0.0f;
	for( i = 0; i < ACCEL_SAMPLE_COUNT; i++ )
		accel += accelHistory[i];
	accel /= (float)( ACCEL_SAMPLE_COUNT );

	if( GS_MatchState() != MATCH_STATE_WARMUP && !GS_RaceGametype() ) {
		return 0;
	}

	return (int)accel;
}

static int CG_GetScoreboardShown( const void *parameter ) {
	return CG_IsScoreboardShown() ? 1 : 0;
}

static int CG_GetOverlayMenuState( const void *parameter ) {
	if( trap_SCR_IsOverlayMenuShown() ) {
		return 2;
	}

	if( trap_SCR_HaveOverlayMenu() ) {
		return 1;
	}

	return 0;
}

typedef struct
{
	const char *name;
	int ( *func )( const void *parameter );
	const void *parameter;
} reference_numeric_t;

static const reference_numeric_t cg_numeric_references[] =
{
	// stats
	{ "HEALTH", CG_GetStatValue, (void *)STAT_HEALTH },
	{ "WEAPON_ITEM", CG_GetStatValue, (void *)STAT_WEAPON },
	{ "PENDING_WEAPON", CG_GetStatValue, (void *)STAT_PENDING_WEAPON },

	{ "PICKUP_ITEM", CG_GetStatValue, (void *)STAT_PICKUP_ITEM },

	{ "SCORE", CG_GetStatValue, (void *)STAT_SCORE },
	{ "RESPAWN_TIME", CG_GetStatValue, (void *)STAT_NEXT_RESPAWN },

	{ "POINTED_PLAYER", CG_GetStatValue, (void *)STAT_POINTED_PLAYER },
	{ "POINTED_TEAMPLAYER", CG_GetStatValue, (void *)STAT_POINTED_TEAMPLAYER },

	{ "TEAM_ALPHA_SCORE", CG_GetStatValue, (void *)STAT_TEAM_ALPHA_SCORE },
	{ "TEAM_BETA_SCORE", CG_GetStatValue, (void *)STAT_TEAM_BETA_SCORE },

	{ "PROGRESS", CG_GetStatValue, (void *)STAT_PROGRESS },
	{ "PROGRESS_TYPE", CG_GetStatValue, (void *)STAT_PROGRESS_TYPE },

	{ "ROUND_TYPE", CG_GetStatValue, (void *)STAT_ROUND_TYPE },

	{ "CARRYING_BOMB", CG_GetStatValue, (void *)STAT_CARRYING_BOMB },
	{ "CAN_CHANGE_LOADOUT", CG_GetStatValue, (void *)STAT_CAN_CHANGE_LOADOUT },

	{ "ALPHA_PLAYERS_ALIVE", CG_GetStatValue, (void *)STAT_ALPHA_PLAYERS_ALIVE },
	{ "ALPHA_PLAYERS_TOTAL", CG_GetStatValue, (void *)STAT_ALPHA_PLAYERS_TOTAL },
	{ "BETA_PLAYERS_ALIVE", CG_GetStatValue, (void *)STAT_BETA_PLAYERS_ALIVE },
	{ "BETA_PLAYERS_TOTAL", CG_GetStatValue, (void *)STAT_BETA_PLAYERS_TOTAL },

	// inventory grabs
	{ "AMMO_ITEM", CG_GetCurrentWeaponInventoryData, (void *)0 },
	{ "AMMO", CG_GetCurrentWeaponInventoryData, (void *)1 },
	{ "LOW_AMMO", CG_GetCurrentWeaponInventoryData, (void *)2 },
	{ "WEAPON_COUNT", CG_GetWeaponCount, NULL },

	// other
	{ "CHASING", CG_GetPOVnum, NULL },
	{ "SPEED", CG_GetSpeed, NULL },
	{ "SPEED_VERTICAL", CG_GetSpeedVertical, NULL },
	{ "FPS", CG_GetFPS, NULL },
	{ "MATCH_STATE", CG_GetMatchState, NULL },
	{ "MATCH_DURATION", CG_GetMatchDuration, NULL },
	{ "OVERTIME", CG_GetOvertime, NULL },
	{ "MATCH_POINT", CG_GetOvertime, NULL },
	{ "TEAMBASED", CG_GetTeamBased, NULL },
	{ "INDIVIDUAL", CG_InvidualGameType, NULL },
	{ "RACE", CG_RaceGameType, NULL },
	{ "PAUSED", CG_Paused, NULL },
	{ "ZOOM", CG_GetZoom, NULL },
	{ "VIDWIDTH", CG_GetVidWidth, NULL },
	{ "VIDHEIGHT", CG_GetVidHeight, NULL },
	{ "SCOREBOARD", CG_GetScoreboardShown, NULL },
	{ "PMOVE_TYPE", CG_GetPmoveType, NULL },
	{ "DEMOPLAYING", CG_IsDemoPlaying, NULL },
	{ "INSTANTRESPAWN", CG_GetLayoutStatFlag, (void *)STAT_LAYOUT_INSTANTRESPAWN },
	{ "QUICKMENU", CG_GetOverlayMenuState, NULL },

	{ "POWERUP_QUAD_TIME", CG_GetPowerupTime, (void *)POWERUP_QUAD },
	{ "POWERUP_WARSHELL_TIME", CG_GetPowerupTime, (void *)POWERUP_SHELL },
	{ "POWERUP_REGEN_TIME", CG_GetPowerupTime, (void *)POWERUP_REGEN },

	{ "DAMAGE_INDICATOR_TOP", CG_GetDamageIndicatorDirValue, (void *)0 },
	{ "DAMAGE_INDICATOR_RIGHT", CG_GetDamageIndicatorDirValue, (void *)1 },
	{ "DAMAGE_INDICATOR_BOTTOM", CG_GetDamageIndicatorDirValue, (void *)2 },
	{ "DAMAGE_INDICATOR_LEFT", CG_GetDamageIndicatorDirValue, (void *)3 },

// ch : backport racesow hud elements
//lm: race stuff
	{ "MOUSE_X", CG_GetRaceVars, (void *)mouse_x },
	{ "MOUSE_Y", CG_GetRaceVars, (void *)mouse_y },
	{ "ACCELERATION", CG_GetAccel, NULL },
	{ "MOVEANGLE", CG_GetRaceVars, (void *)move_an  },
	{ "STRAFEANGLE", CG_GetRaceVars, (void *)strafe_an },
	{ "DIFF_ANGLE", CG_GetRaceVars, (void *)diff_an },

	// cvars
	{ "SHOW_FPS", CG_GetCvar, "cg_showFPS" },
	{ "SHOW_OBITUARIES", CG_GetCvar, "cg_showObituaries" },
	{ "SHOW_PICKUP", CG_GetCvar, "cg_showPickup" },
	{ "SHOW_POINTED_PLAYER", CG_GetCvar, "cg_showPointedPlayer" },
	{ "SHOW_PRESSED_KEYS", CG_GetCvar, "cg_showPressedKeys" },
	{ "SHOW_SPEED", CG_GetCvar, "cg_showSpeed" },
	{ "SHOW_TIMER", CG_GetCvar, "cg_showTimer" },
	{ "SHOW_AWARDS", CG_GetCvar, "cg_showAwards" },
	{ "SHOW_R_SPEEDS", CG_GetCvar, "r_speeds" },
	{ "SHOW_STRAFE", CG_GetCvar, "cg_strafeHUD" },

	{ "DOWNLOAD_IN_PROGRESS", CG_DownloadInProgress, NULL },
	{ "DOWNLOAD_PERCENT", CG_GetCvar, "cl_download_percent" },

	{ "CHAT_MODE", CG_GetCvar, "con_messageMode" },

	{ NULL, NULL, NULL }
};

//=============================================================================

static struct qfontface_s *CG_GetLayoutCursorFont( void );

#define MAX_OBITUARIES 32

typedef enum { OBITUARY_NONE, OBITUARY_NORMAL, OBITUARY_TEAM, OBITUARY_SUICIDE, OBITUARY_ACCIDENT } obituary_type_t;

typedef struct obituary_s
{
	obituary_type_t type;
	int64_t time;
	char victim[MAX_INFO_VALUE];
	int victim_team;
	char attacker[MAX_INFO_VALUE];
	int attacker_team;
	int mod;
} obituary_t;

static obituary_t cg_obituaries[MAX_OBITUARIES];
static int cg_obituaries_current = -1;

/*
* CG_SC_PrintObituary
*/
void CG_SC_PrintObituary( const char *format, ... ) {
	va_list argptr;
	char msg[GAMECHAT_STRING_SIZE];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );

	CG_StackChatString( &cg.chat, msg );
}

/*
* CG_SC_ResetObituaries
*/
void CG_SC_ResetObituaries( void ) {
	memset( cg_obituaries, 0, sizeof( cg_obituaries ) );
	cg_obituaries_current = -1;
}

static const char * obituaries[] = {
	"2.3'ED",
	"ABOLISHED",
	"ABUSED",
	"AIRED OUT",
	"ANNIHILATED",
	"AXED",
	"BAKED",
	"BATTERED",
	"BELITTLED",
	"BIGGED ON",
	"BINNED",
	"BLASTED",
	"BLESSED",
	"BODYBAGGED",
	"BROKE UP WITH",
	"BURIED",
	"BUTTERED",
	"CANCELED",
	"CANNED",
	"CAPPED",
	"CARAMELISED",
	"CARBONATED",
	"CASHED OUT",
	"CLUBBED",
	"CONSUMED",
	"CRIPPLED",
	"CRUSHED",
	"CULLED",
	"CURED",
	"DEBUGGED",
	"DECIMATED",
	"DEEPWATER HORIZONED",
	"DEFACED",
	"DEFAMED",
	"DEFLATED",
	"DELETED",
	"DEMOLISHED",
	"DEPOSITED",
	"DESTROYED",
	"DISASSEMBLED",
	"DISCIPLINED",
	"DISMANTLED",
	"DIVORCED",
	"DRENCHED",
	"DRILLED INTO",
	"DUMPED ON",
	"DUMPED",
	"DUMPSTERED",
	"DUNKED",
	"EDUCATED",
	"ELABORATED ON",
	"ENDED",
	"ERASED",
	"EXECUTED",
	"EXTERMINATED",
	"FILED",
	"FLUSHED",
	"FOLDED",
	"FORKED",
	"FRAGGED",
	"FUCKED",
	"GRAVEDUG",
	"GUTTED",
	"HOSED",
	"HUMILIATED",
	"ICED",
	"IMPREGNATED",
	"INVADED",
	"LANDFILLED",
	"LARDED",
	"LEFT CLICKED ON",
	"LET GO",
	"LIQUIDATED",
	"LYNCHED",
	"MASSACRED",
	"MAULED",
	"MURDERED",
	"MUTILATED",
	"NEUTRALIZED",
	"NUKED",
	"NULLIFIED",
	"OBLITERATED",
	"PACKED",
	"PAID RESPECTS TO",
	"PILED",
	"PISSED ON",
	"PLANTED",
	"PLOUGHED",
	"POUNDED",
	"PULVERISED",
	"PURGED",
	"PUT DOWN",
	"PYONGYANGED",
	"RAZED",
	"RECYCLED",
	"REDUCED",
	"REFORMED",
	"REFRIGERATED",
	"REHABILITATED",
	"REMOVED",
	"RESPECTED",
	"REVERSED INTO",
	"REVISED",
	"ROLLED",
	"RUINED",
	"SACKED",
	"SANDED",
	"SANDED",
	"SHAFTED",
	"SHARPENED",
	"SHATTERED",
	"SHAVED",
	"SHITTED ON",
	"SKEWERED",
	"SLAMMED",
	"SLAPPED",
	"SLAUGHTERED",
	"SLICED",
	"SMASHED",
	"SPREAD",
	"SQUASHED",
	"STAMPED OUT",
	"STOMPED",
	"STYLED ON",
	"SUNK",
	"SWAMPED",
	"SWIPED LEFT ON",
	"TARNISHED",
	"TERMINATED",
	"TOILET STORED",
	"TOOK CARE OF",
	"TOPPLED",
	"TOSSED",
	"TRASHED",
	"TRUMPED",
	"UNFOLLOWED",
	"VAPED",
	"VIOLATED",
	"VULKANIZED",
	"WASTED",
	"WAXED",
	"WEEDED OUT",
	"WHACKED",
	"WHIPPED",
	"WIPED OUT",
	"WITNESSED",
	"WRECKED",
};

static const char * prefixes[] = {
	"",
	"",
	"",
	"",
	"SHIT",
	"ASS",
	"PILE",
	"PLANTER",
};

static const char * RandomObituary() {
	return obituaries[ rand() % ( sizeof( obituaries ) / sizeof( obituaries[ 0 ] ) ) ];
}

static const char * RandomPrefix() {
	return prefixes[ rand() % ( sizeof( prefixes ) / sizeof( prefixes[ 0 ] ) ) ];
}

/*
* CG_SC_Obituary
*/
void CG_SC_Obituary( void ) {
	char message[128];
	char message2[128];
	cg_clientInfo_t *victim, *attacker;
	int victimNum = atoi( trap_Cmd_Argv( 1 ) );
	int attackerNum = atoi( trap_Cmd_Argv( 2 ) );
	int mod = atoi( trap_Cmd_Argv( 3 ) );
	int victim_gender = GENDER_MALE;
	obituary_t *current;

	// wsw : jal : extract gender from their player model info, if any
	if( victimNum >= 0 && victimNum < MAX_EDICTS && cg_entPModels[victimNum].pmodelinfo ) {
		victim_gender = cg_entPModels[victimNum].pmodelinfo->sex;
	}

	victim = &cgs.clientInfo[victimNum - 1];

	if( attackerNum ) {
		attacker = &cgs.clientInfo[attackerNum - 1];
	} else {
		attacker = NULL;
	}

	cg_obituaries_current++;
	if( cg_obituaries_current >= MAX_OBITUARIES ) {
		cg_obituaries_current = 0;
	}
	current = &cg_obituaries[cg_obituaries_current];

	current->time = cg.time;
	if( victim ) {
		Q_strncpyz( current->victim, victim->name, sizeof( current->victim ) );
		current->victim_team = cg_entities[victimNum].current.team;
	}
	if( attacker ) {
		Q_strncpyz( current->attacker, attacker->name, sizeof( current->attacker ) );
		current->attacker_team = cg_entities[attackerNum].current.team;
	}
	current->mod = mod;

	GS_Obituary( victim, victim_gender, attacker, mod, message, message2 );

	if( attackerNum ) {
		if( victimNum != attackerNum ) {
			// teamkill
			if( cg_entities[attackerNum].current.team == cg_entities[victimNum].current.team &&
				GS_TeamBasedGametype() ) {
				current->type = OBITUARY_TEAM;
				if( cg_showObituaries->integer & CG_OBITUARY_CONSOLE ) {
					CG_LocalPrint( "%s%s%s %s %s%s %s%s%s\n", S_COLOR_RED, "TEAMFRAG:", S_COLOR_WHITE, victim->name,
								   S_COLOR_WHITE, message, attacker->name, S_COLOR_WHITE, message2 );
				}

				if( ISVIEWERENTITY( attackerNum ) && ( cg_showObituaries->integer & CG_OBITUARY_CENTER ) ) {
					char name[MAX_NAME_BYTES + 2];
					Q_strncpyz( name, victim->name, sizeof( name ) );
					Q_strupr( name );
					Q_strncatz( name, S_COLOR_WHITE, sizeof( name ) );
					CG_CenterPrint( va( "YOU TEAM%s%s %s", RandomPrefix(), RandomObituary(), name ) );
				}
			} else {   // good kill
				current->type = OBITUARY_NORMAL;
				if( cg_showObituaries->integer & CG_OBITUARY_CONSOLE ) {
					CG_LocalPrint( "%s %s%s %s%s%s\n", victim->name, S_COLOR_WHITE, message, attacker->name, S_COLOR_WHITE,
								   message2 );
				}

				if( ISVIEWERENTITY( attackerNum ) && ( cg_showObituaries->integer & CG_OBITUARY_CENTER ) ) {
					char name[MAX_NAME_BYTES + 2];
					Q_strncpyz( name, victim->name, sizeof( name ) );
					Q_strupr( name );
					Q_strncatz( name, S_COLOR_WHITE, sizeof( name ) );
					CG_CenterPrint( va( "YOU %s%s %s", RandomPrefix(), RandomObituary(), name ) );
				}
			}
		} else {   // suicide
			current->type = OBITUARY_SUICIDE;
			if( cg_showObituaries->integer & CG_OBITUARY_CONSOLE ) {
				CG_LocalPrint( "%s %s%s\n", victim->name, S_COLOR_WHITE, message );
			}
		}
	} else {   // world accidents
		current->type = OBITUARY_ACCIDENT;
		if( cg_showObituaries->integer & CG_OBITUARY_CONSOLE ) {
			CG_LocalPrint( "%s %s%s\n", victim->name, S_COLOR_WHITE, message );
		}
	}
}

static void CG_DrawObituaries( int x, int y, int align, struct qfontface_s *font, vec4_t color, int width, int height,
							   int internal_align, unsigned int icon_size ) {
	const int icon_padding = 4;
	int i, num, skip, next, w, num_max;
	unsigned line_height;
	int xoffset, yoffset;
	obituary_t *obr;
	struct shader_s *pic;
	vec4_t teamcolor;

	if( !( cg_showObituaries->integer & CG_OBITUARY_HUD ) ) {
		return;
	}

	line_height = max( (unsigned)trap_SCR_FontHeight( font ), icon_size );
	num_max = height / line_height;

	if( width < (int)icon_size || !num_max ) {
		return;
	}

	next = cg_obituaries_current + 1;
	if( next >= MAX_OBITUARIES ) {
		next = 0;
	}

	num = 0;
	i = next;
	do {
		if( cg_obituaries[i].type != OBITUARY_NONE && cg.time - cg_obituaries[i].time <= 5000 ) {
			num++;
		}
		if( ++i >= MAX_OBITUARIES ) {
			i = 0;
		}
	} while( i != next );

	if( num > num_max ) {
		skip = num - num_max;
		num = num_max;
	} else {
		skip = 0;
	}

	y = CG_VerticalAlignForHeight( y, align, height );
	x = CG_HorizontalAlignForWidth( x, align, width );

	xoffset = 0;
	yoffset = 0;

	i = next;
	do {
		obr = &cg_obituaries[i];
		if( ++i >= MAX_OBITUARIES ) {
			i = 0;
		}

		if( obr->type == OBITUARY_NONE || cg.time - obr->time > 5000 ) {
			continue;
		}

		if( skip > 0 ) {
			skip--;
			continue;
		}

		switch( obr->mod ) {
			case MOD_GUNBLADE_W:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_GUNBLADE - 1] );
				break;
			case MOD_GUNBLADE_S:
				pic = CG_MediaShader( cgs.media.shaderGunbladeBlastIcon );
				break;
			case MOD_MACHINEGUN:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_MACHINEGUN - 1] );
				break;
			case MOD_RIOTGUN:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_RIOTGUN - 1] );
				break;
			case MOD_GRENADE:
			case MOD_GRENADE_SPLASH:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_GRENADELAUNCHER - 1] );
				break;
			case MOD_ROCKET:
			case MOD_ROCKET_SPLASH:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_ROCKETLAUNCHER - 1] );
				break;
			case MOD_PLASMA:
			case MOD_PLASMA_SPLASH:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_PLASMAGUN - 1] );
				break;
			case MOD_ELECTROBOLT:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_ELECTROBOLT - 1] );
				break;
			case MOD_LASERGUN:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_LASERGUN - 1] );
				break;
			default:
				pic = CG_MediaShader( cgs.media.shaderWeaponIcon[WEAP_GUNBLADE - 1] ); // FIXME
				break;
		}

		w = 0;
		if( obr->type != OBITUARY_ACCIDENT ) {
			w += min( trap_SCR_strWidth( obr->attacker, font, 0 ), ( width - icon_size ) / 2 );
		}
		w += icon_padding;
		w += icon_size;
		w += icon_padding;
		w += min( trap_SCR_strWidth( obr->victim, font, 0 ), ( width - icon_size ) / 2 );

		if( internal_align == 1 ) {
			// left
			xoffset = 0;
		} else if( internal_align == 2 ) {
			// center
			xoffset = ( width - w ) / 2;
		} else {
			// right
			xoffset = width - w;
		}

		int obituary_y = y + yoffset + ( line_height - trap_SCR_FontHeight( font ) ) / 2;
		if( obr->type != OBITUARY_ACCIDENT ) {
			if( obr->attacker_team == TEAM_ALPHA || obr->attacker_team == TEAM_BETA ) {
				CG_TeamColor( obr->attacker_team, teamcolor );
			} else {
				Vector4Set( teamcolor, 255, 255, 255, 255 );
			}
			trap_SCR_DrawStringWidth( x + xoffset, obituary_y,
									  ALIGN_LEFT_TOP, COM_RemoveColorTokensExt( obr->attacker, true ), ( width - icon_size ) / 2,
									  font, teamcolor );
			xoffset += min( trap_SCR_strWidth( obr->attacker, font, 0 ), ( width - icon_size ) / 2 );
		}

		if( obr->victim_team == TEAM_ALPHA || obr->victim_team == TEAM_BETA ) {
			CG_TeamColor( obr->victim_team, teamcolor );
		} else {
			Vector4Set( teamcolor, 255, 255, 255, 255 );
		}
		trap_SCR_DrawStringWidth( x + xoffset + icon_size + 2 * icon_padding, obituary_y,
			ALIGN_LEFT_TOP, COM_RemoveColorTokensExt( obr->victim, true ), ( width - icon_size ) / 2, font, teamcolor );

		trap_R_DrawStretchPic( x + xoffset + icon_padding, y + yoffset + ( line_height - icon_size ) / 2, icon_size,
							   icon_size, 0, 0, 1, 1, colorWhite, pic );

		yoffset += line_height;
	} while( i != next );
}

//=============================================================================

#define AWARDS_OVERSHOOT_DURATION 0.2f
#define AWARDS_OVERSHOOT_FREQUENCY 6.0f
#define AWARDS_OVERSHOOT_DECAY 10.0f

void CG_ClearAwards( void ) {
	// reset awards
	cg.award_head = 0;
	memset( cg.award_times, 0, sizeof( cg.award_times ) );
}

static void CG_DrawAwards( int x, int y, int align, struct qfontface_s *font, vec4_t color ) {
	int i, count, current;
	int yoffset;

	if( !cg_showAwards->integer ) {
		return;
	}

	if( !cg.award_head ) {
		return;
	}

	for( count = 0; count < MAX_AWARD_LINES; count++ ) {
		current = ( ( cg.award_head - 1 ) - count );
		if( current < 0 ) {
			break;
		}

		if( cg.award_times[current % MAX_AWARD_LINES] + MAX_AWARD_DISPLAYTIME < cg.time ) {
			break;
		}

		if( !cg.award_lines[current % MAX_AWARD_LINES][0] ) {
			break;
		}
	}

	if( !count ) {
		return;
	}

	y = CG_VerticalAlignForHeight( y, align, trap_SCR_FontHeight( font ) * MAX_AWARD_LINES );

	for( i = count; i > 0; i-- ) {
		current = ( cg.award_head - i ) % MAX_AWARD_LINES;
		const char *str = cg.award_lines[ current ];

		yoffset = trap_SCR_FontHeight( font ) * ( MAX_AWARD_LINES - i );

		trap_SCR_DrawStringWidth( x, y + yoffset, align, str, 0, font, color );
	}
}

//=============================================================================
//	STATUS BAR PROGRAMS
//=============================================================================

typedef float ( *opFunc_t )( const float a, float b );

// we will always operate with floats so we don't have to code 2 different numeric paths
// it's not like using float or ints would make a difference in this simple-scripting case.

static float CG_OpFuncAdd( const float a, const float b ) {
	return a + b;
}

static float CG_OpFuncSubtract( const float a, const float b ) {
	return a - b;
}

static float CG_OpFuncMultiply( const float a, const float b ) {
	return a * b;
}

static float CG_OpFuncDivide( const float a, const float b ) {
	return a / b;
}

static float CG_OpFuncAND( const float a, const float b ) {
	return (int)a & (int)b;
}

static float CG_OpFuncOR( const float a, const float b ) {
	return (int)a | (int)b;
}

static float CG_OpFuncXOR( const float a, const float b ) {
	return (int)a ^ (int)b;
}

static float CG_OpFuncCompareEqual( const float a, const float b ) {
	return ( a == b );
}

static float CG_OpFuncCompareNotEqual( const float a, const float b ) {
	return ( a != b );
}

static float CG_OpFuncCompareGreater( const float a, const float b ) {
	return ( a > b );
}

static float CG_OpFuncCompareGreaterOrEqual( const float a, const float b ) {
	return ( a >= b );
}

static float CG_OpFuncCompareSmaller( const float a, const float b ) {
	return ( a < b );
}

static float CG_OpFuncCompareSmallerOrEqual( const float a, const float b ) {
	return ( a <= b );
}

static float CG_OpFuncCompareAnd( const float a, const float b ) {
	return ( a && b );
}

static float CG_OpFuncCompareOr( const float a, const float b ) {
	return ( a || b );
}

typedef struct cg_layoutoperators_s
{
	const char *name;
	opFunc_t opFunc;
} cg_layoutoperators_t;

static cg_layoutoperators_t cg_LayoutOperators[] =
{
	{
		"+",
		CG_OpFuncAdd
	},

	{
		"-",
		CG_OpFuncSubtract
	},

	{
		"*",
		CG_OpFuncMultiply
	},

	{
		"/",
		CG_OpFuncDivide
	},

	{
		"&",
		CG_OpFuncAND
	},

	{
		"|",
		CG_OpFuncOR
	},

	{
		"^",
		CG_OpFuncXOR
	},

	{
		"==",
		CG_OpFuncCompareEqual
	},

	{
		"!=",
		CG_OpFuncCompareNotEqual
	},

	{
		">",
		CG_OpFuncCompareGreater
	},

	{
		">=",
		CG_OpFuncCompareGreaterOrEqual
	},

	{
		"<",
		CG_OpFuncCompareSmaller
	},

	{
		"<=",
		CG_OpFuncCompareSmallerOrEqual
	},

	{
		"&&",
		CG_OpFuncCompareAnd
	},

	{
		"||",
		CG_OpFuncCompareOr
	},

	{
		NULL,
		NULL
	},
};

/*
* CG_OperatorFuncForArgument
*/
static opFunc_t CG_OperatorFuncForArgument( const char *token ) {
	cg_layoutoperators_t *op;

	while( *token == ' ' )
		token++;

	for( op = cg_LayoutOperators; op->name; op++ ) {
		if( !Q_stricmp( token, op->name ) ) {
			return op->opFunc;
		}
	}

	return NULL;
}

//=============================================================================

static const char *CG_GetStringArg( struct cg_layoutnode_s **argumentsnode );
static float CG_GetNumericArg( struct cg_layoutnode_s **argumentsnode );

//=============================================================================

static int layout_cursor_scale = DEFAULTSCALE;
static int layout_cursor_x = 400;
static int layout_cursor_y = 300;
static int layout_cursor_width = 100;
static int layout_cursor_height = 100;
static int layout_cursor_align = ALIGN_LEFT_TOP;
static vec4_t layout_cursor_color = { 1, 1, 1, 1 };
static vec3_t layout_cursor_rotation = { 0, 0, 0 };

static struct qfontface_s *layout_cursor_font;
static char layout_cursor_font_name[MAX_QPATH];
static int layout_cursor_font_size;
static int layout_cursor_font_style;
struct qfontface_s *(*layout_cursor_font_regfunc)( const char *, int, unsigned int );
static bool layout_cursor_font_dirty = true;

enum
{
	LNODE_NUMERIC,
	LNODE_STRING,
	LNODE_REFERENCE_NUMERIC,
	LNODE_COMMAND
};

//=============================================================================
// Commands' Functions
//=============================================================================

static bool CG_IsWeaponSelected( int weapon ) {
	if( cg.view.playerPrediction && cg.predictedWeaponSwitch && cg.predictedWeaponSwitch != cg.predictedPlayerState.stats[STAT_PENDING_WEAPON] ) {
		return ( weapon == cg.predictedWeaponSwitch );
	}

	return ( weapon == cg.predictedPlayerState.stats[STAT_PENDING_WEAPON] );
}

static struct shader_s *CG_GetWeaponIcon( int weapon ) {
	int currentWeapon = cg.predictedPlayerState.stats[STAT_WEAPON];
	int weaponState = cg.predictedPlayerState.weaponState;

	if( weapon == WEAP_GUNBLADE && cg.predictedPlayerState.inventory[AMMO_GUNBLADE] ) {
		if( currentWeapon != WEAP_GUNBLADE || ( weaponState != WEAPON_STATE_REFIRESTRONG && weaponState != WEAPON_STATE_REFIRE ) ) {
			return CG_MediaShader( cgs.media.shaderGunbladeBlastIcon );
		}
	}

	return CG_MediaShader( cgs.media.shaderWeaponIcon[weapon - WEAP_GUNBLADE] );
}

constexpr float SELECTED_WEAPON_Y_OFFSET = 0.0125;

static void CG_DrawWeaponIcons( int x, int y, int offx, int offy, int iw, int ih, int align ) {
	int num_weapons = 0;
	for( int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
		if( CG_IsWeaponInList( i ) ) {
			num_weapons++;
		}
	}

	int padx = offx - iw;
	int pady = offy - ih;
	int total_width = max( 0, num_weapons * offx - padx );
	int total_height = max( 0, num_weapons * offy - pady );

	int drawn_weapons = 0;
	for( int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
		if( !CG_IsWeaponInList( i ) )
			continue;

		int curx = CG_HorizontalAlignForWidth( x + offx * drawn_weapons, align, total_width );
		int cury = CG_VerticalAlignForHeight( y + offy * drawn_weapons, align, total_height );

		if( CG_IsWeaponSelected( i ) )
			cury -= SELECTED_WEAPON_Y_OFFSET * cgs.vidHeight;

		trap_R_DrawStretchPic( curx, cury, iw, ih, 0, 0, 1, 1, colorWhite, CG_GetWeaponIcon( i ) );

		drawn_weapons++;
	}
}

static void CG_DrawWeaponAmmos( int x, int y, int offx, int offy, int fontsize, int align ) {
	/*
	 * we don't draw ammo text for GB
	 * so all the loops in this function skip it
	 */
	int num_weapons = 0;
	for( int i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ ) {
		if( CG_IsWeaponInList( i ) ) {
			num_weapons++;
		}
	}

	int total_width = max( 0, num_weapons * offx );

	int drawn_weapons = 1;
	for( int i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ ) {
		if( !CG_IsWeaponInList( i ) )
			continue;

		int curx = CG_HorizontalAlignForWidth( x + offx * drawn_weapons, align, total_width );
		int cury = y + offy * drawn_weapons;

		if( CG_IsWeaponSelected( i ) )
			cury -= SELECTED_WEAPON_Y_OFFSET * cgs.vidHeight;

		int ammo = cg.predictedPlayerState.inventory[ AMMO_GUNBLADE + i - WEAP_GUNBLADE ];

		trap_SCR_DrawString( curx, cury, ALIGN_RIGHT_BOTTOM, va( "%i", ammo ), CG_GetLayoutCursorFont(), layout_cursor_color );

		drawn_weapons++;
	}
}

static bool CG_LFuncDrawTimer( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	char time[64];
	int min, sec, milli;

	milli = (int)CG_GetNumericArg( &argumentnode );
	if( milli < 0 ) {
		return true;
	}

	// stat is in milliseconds/100.0f
	min = milli / 600;
	milli -= min * 600;
	sec = milli / 10;
	milli -= sec * 10;

	// we want MM:SS:m
	Q_snprintfz( time, sizeof( time ), "%02d:%02d.%1d", min, sec, milli );
	trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align, time, CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawPicVar( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int min, max, val, firstimg, lastimg, imgcount;
	static char filefmt[MAX_QPATH], filenm[MAX_QPATH], *ptr;
	int x, y, filenr;
	int cnt = 1;

	// get tje arguments
	val      = (int)( CG_GetNumericArg( &argumentnode ) );
	min      = (int)( CG_GetNumericArg( &argumentnode ) );
	max      = (int)( CG_GetNumericArg( &argumentnode ) );
	firstimg = (int)( CG_GetNumericArg( &argumentnode ) );
	lastimg  = (int)( CG_GetNumericArg( &argumentnode ) );

	if( min > max ) { // swap min and max and count downwards
		int t = min;
		min = max;
		max = t;
		cnt = -cnt;
	}
	if( firstimg > lastimg ) { // swap firstimg and lastimg and count the other way around
		int t = firstimg;
		firstimg = lastimg;
		lastimg = t;
		cnt = -cnt;
	}

	if( val < min ) {
		val = min;
	}
	if( val > max ) {
		val = max;
	}
	val -= min;
	max -= min;
	min = 0;

	imgcount = lastimg - firstimg + 1;

	if( ( max != 0 ) && ( imgcount != 0 ) ) { // Check for division by 0
		filenr =  (int)( ( (double)val / ( (double)max / imgcount ) ) );
	} else {
		filenr = 0;
	}
	if( filenr >= imgcount ) {
		filenr = ( imgcount - 1 );
	}
	if( filenr < 0 ) {
		filenr = 0;
	}

	if( cnt < 0 ) {
		filenr = ( imgcount - filenr ) - 1;
	}
	filenr += firstimg;

	filefmt[0] = '\0';
	Q_strncpyz( filefmt, CG_GetStringArg( &argumentnode ), sizeof( filenm ) );
	ptr = filefmt;
	while( ( ptr[0] ) && ( ptr[1] ) ) {
		if( ( ptr[0] == '#' ) && ( ptr[1] == '#' ) ) {
			ptr[0] = '%';
			ptr[1] = 'd';
			break; // Only replace first occurance?
		}
		ptr++;
	}
	if( ( ptr[0] != '%' ) && ( ptr[1] != 'd' ) ) {
		CG_Printf( "WARNING 'CG_LFuncDrawPicVar' Invalid file string parameter, no '##' present!" );
		return false;
	}
	Q_snprintfz( filenm, sizeof( filenm ), filefmt, filenr );
	x = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_align, layout_cursor_width );
	y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_align, layout_cursor_height );
	trap_R_DrawStretchPic( x, y, layout_cursor_width, layout_cursor_height, 0, 0, 1, 1, layout_cursor_color, trap_R_RegisterPic( filenm ) );
	return true;
}

static bool CG_LFuncDrawPicByIndex( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int value = (int)CG_GetNumericArg( &argumentnode );
	int x, y;

	if( value >= 0 && value < MAX_IMAGES ) {
		if( cgs.configStrings[CS_IMAGES + value][0] ) {
			x = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_align, layout_cursor_width );
			y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_align, layout_cursor_height );
			trap_R_DrawStretchPic( x, y, layout_cursor_width, layout_cursor_height, 0, 0, 1, 1, layout_cursor_color, trap_R_RegisterPic( cgs.configStrings[CS_IMAGES + value] ) );
			return true;
		}
	}

	return false;
}

static bool CG_LFuncDrawPicByItemIndex( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int itemindex = (int)CG_GetNumericArg( &argumentnode );
	int x, y;
	gsitem_t    *item;

	item = GS_FindItemByTag( itemindex );
	if( !item ) {
		return false;
	}
	x = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_align, layout_cursor_width );
	y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_align, layout_cursor_height );
	trap_R_DrawStretchPic( x, y, layout_cursor_width, layout_cursor_height, 0, 0, 1, 1, layout_cursor_color, trap_R_RegisterPic( item->icon ) );
	return true;
}

static bool CG_LFuncDrawPicByName( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int x, y;

	x = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_align, layout_cursor_width );
	y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_align, layout_cursor_height );
	trap_R_DrawStretchPic( x, y, layout_cursor_width, layout_cursor_height, 0, 0, 1, 1, layout_cursor_color, trap_R_RegisterPic( CG_GetStringArg( &argumentnode ) ) );
	return true;
}

static bool CG_LFuncDrawSubPicByName( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int x, y;
	struct shader_s *shader;
	float s1, t1, s2, t2;

	x = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_align, layout_cursor_width );
	y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_align, layout_cursor_height );

	shader = trap_R_RegisterPic( CG_GetStringArg( &argumentnode ) );

	s1 = CG_GetNumericArg( &argumentnode );
	t1 = CG_GetNumericArg( &argumentnode );
	s2 = CG_GetNumericArg( &argumentnode );
	t2 = CG_GetNumericArg( &argumentnode );

	trap_R_DrawStretchPic( x, y, layout_cursor_width, layout_cursor_height, s1, t1, s2, t2, layout_cursor_color, shader );
	return true;
}

static bool CG_LFuncDrawRotatedPicByName( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int x, y;
	struct shader_s *shader;
	float angle;

	x = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_align, layout_cursor_width );
	y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_align, layout_cursor_height );

	shader = trap_R_RegisterPic( CG_GetStringArg( &argumentnode ) );

	angle = CG_GetNumericArg( &argumentnode );

	trap_R_DrawRotatedStretchPic( x, y, layout_cursor_width, layout_cursor_height, 0, 0, 1, 1, angle, layout_cursor_color, shader );
	return true;
}

static bool CG_LFuncDrawModelByIndex( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	struct model_s *model;
	int value = (int)CG_GetNumericArg( &argumentnode );

	if( value >= 0 && value < MAX_MODELS ) {
		model = value > 1 ? CG_RegisterModel( cgs.configStrings[CS_MODELS + value] ) : NULL;
		CG_DrawHUDModel( layout_cursor_x, layout_cursor_y, layout_cursor_align, layout_cursor_width, layout_cursor_height, model, NULL, layout_cursor_rotation[YAW] );
		return true;
	}

	return false;
}

static bool CG_LFuncDrawModelByName( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	struct model_s *model;
	struct shader_s *shader;
	const char *shadername;

	model = CG_RegisterModel( CG_GetStringArg( &argumentnode ) );
	shadername = CG_GetStringArg( &argumentnode );
	shader = Q_stricmp( shadername, "NULL" ) ? trap_R_RegisterPic( shadername ) : NULL;
	CG_DrawHUDModel( layout_cursor_x, layout_cursor_y, layout_cursor_align, layout_cursor_width, layout_cursor_height, model, shader, layout_cursor_rotation[YAW] );
	return true;
}

static bool CG_LFuncDrawModelByItemIndex( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int i;
	gsitem_t    *item;
	struct model_s *model;
	int itemindex = (int)CG_GetNumericArg( &argumentnode );

	item = GS_FindItemByTag( itemindex );
	if( !item ) {
		return false;
	}
	for( i = 0; i < MAX_ITEM_MODELS; i++ ) {
		if( item->world_model[i] != NULL ) {
			model = itemindex >= 1 ? CG_RegisterModel( item->world_model[i] ) : NULL;
			CG_DrawHUDModel( layout_cursor_x, layout_cursor_y, layout_cursor_align, layout_cursor_width, layout_cursor_height, model, NULL, layout_cursor_rotation[YAW] );
		}
	}
	return true;
}

static bool CG_LFuncScale( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	layout_cursor_scale = (int)CG_GetNumericArg( &argumentnode );
	return true;
}

#define SCALE_X( n ) ( ( layout_cursor_scale == NOSCALE ) ? ( n ) : ( ( layout_cursor_scale == SCALEBYHEIGHT ) ? ( n ) * cgs.vidHeight / 600.0f : ( n ) * cgs.vidWidth / 800.0f ) )
#define SCALE_Y( n ) ( ( layout_cursor_scale == NOSCALE ) ? ( n ) : ( ( layout_cursor_scale == SCALEBYWIDTH ) ? ( n ) * cgs.vidWidth / 800.0f : ( n ) * cgs.vidHeight / 600.0f ) )

static bool CG_LFuncCursor( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float x, y;

	x = CG_GetNumericArg( &argumentnode );
	x = SCALE_X( x );
	y = CG_GetNumericArg( &argumentnode );
	y = SCALE_Y( y );

	layout_cursor_x = Q_rint( x );
	layout_cursor_y = Q_rint( y );
	return true;
}

static bool CG_LFuncCursorX( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float x;

	x = CG_GetNumericArg( &argumentnode );
	x = SCALE_X( x );

	layout_cursor_x = Q_rint( x );
	return true;
}

static bool CG_LFuncCursorY( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float y;

	y = CG_GetNumericArg( &argumentnode );
	y = SCALE_Y( y );

	layout_cursor_y = Q_rint( y );
	return true;
}

static bool CG_LFuncMoveCursor( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float x, y;

	x = CG_GetNumericArg( &argumentnode );
	x = SCALE_X( x );
	y = CG_GetNumericArg( &argumentnode );
	y = SCALE_Y( y );

	layout_cursor_x += Q_rint( x );
	layout_cursor_y += Q_rint( y );
	return true;
}

static bool CG_LFuncSize( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float x, y;

	x = CG_GetNumericArg( &argumentnode );
	x = SCALE_X( x );
	y = CG_GetNumericArg( &argumentnode );
	y = SCALE_Y( y );

	layout_cursor_width = Q_rint( x );
	layout_cursor_height = Q_rint( y );
	return true;
}

static bool CG_LFuncSizeWidth( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float x;

	x = CG_GetNumericArg( &argumentnode );
	x = SCALE_X( x );

	layout_cursor_width = Q_rint( x );
	return true;
}

static bool CG_LFuncSizeHeight( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float y;

	y = CG_GetNumericArg( &argumentnode );
	y = SCALE_Y( y );

	layout_cursor_height = Q_rint( y );
	return true;
}

static bool CG_LFuncColor( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int i;
	for( i = 0; i < 4; i++ ) {
		layout_cursor_color[i] = CG_GetNumericArg( &argumentnode );
		clamp( layout_cursor_color[i], 0, 1 );
	}
	return true;
}

static bool CG_LFuncColorToTeamColor( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_TeamColor( CG_GetNumericArg( &argumentnode ), layout_cursor_color );
	return true;
}

static bool CG_LFuncColorAlpha( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	layout_cursor_color[3] = CG_GetNumericArg( &argumentnode );
	return true;
}

static bool CG_LFuncRotationSpeed( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int i;
	for( i = 0; i < 3; i++ ) {
		layout_cursor_rotation[i] = CG_GetNumericArg( &argumentnode );
		clamp( layout_cursor_rotation[i], 0, 999 );
	}
	return true;
}

static bool CG_LFuncAlign( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int v, h;

	h = (int)CG_GetNumericArg( &argumentnode );
	v = (int)CG_GetNumericArg( &argumentnode );
	if( h < 1 ) {
		h = 1;
	}
	if( v < 1 ) {
		v = 1;
	}
	layout_cursor_align = ( h - 1 ) + ( 3 * ( v - 1 ) );
	return true;
}

static struct qfontface_s *CG_GetLayoutCursorFont( void ) {
	struct qfontface_s *font;

	if( !layout_cursor_font_dirty ) {
		return layout_cursor_font;
	}
	if( !layout_cursor_font_regfunc ) {
		layout_cursor_font_regfunc = trap_SCR_RegisterFont;
	}

	font = layout_cursor_font_regfunc( layout_cursor_font_name, layout_cursor_font_style, layout_cursor_font_size );
	if( font ) {
		layout_cursor_font = font;
	} else {
		layout_cursor_font = cgs.fontSystemSmall;
	}
	layout_cursor_font_dirty = false;

	return layout_cursor_font;
}

static bool CG_LFuncFontFamily( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *fontname = CG_GetStringArg( &argumentnode );

	if( !Q_stricmp( fontname, "con_fontSystem" ) ) {
		Q_strncpyz( layout_cursor_font_name, SYSTEM_FONT_FAMILY, sizeof( layout_cursor_font_name ) );
	} else if( !Q_stricmp( fontname, "con_fontSystemMono" ) ) {
		Q_strncpyz( layout_cursor_font_name, SYSTEM_FONT_FAMILY_MONO, sizeof( layout_cursor_font_name ) );
	} else {
		Q_strncpyz( layout_cursor_font_name, fontname, sizeof( layout_cursor_font_name ) );
	}
	layout_cursor_font_dirty = true;
	layout_cursor_font_regfunc = trap_SCR_RegisterFont;

	return true;
}

static bool CG_LFuncSpecialFontFamily( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *fontname = CG_GetStringArg( &argumentnode );

	Q_strncpyz( layout_cursor_font_name, fontname, sizeof( layout_cursor_font_name ) );
	layout_cursor_font_regfunc = trap_SCR_RegisterSpecialFont;
	layout_cursor_font_dirty = true;

	return true;
}

static bool CG_LFuncFontSize( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	struct cg_layoutnode_s *charnode = argumentnode;
	const char *fontsize = CG_GetStringArg( &charnode );

	if( !Q_stricmp( fontsize, "con_fontsystemtiny" ) ) {
		layout_cursor_font_size = cgs.fontSystemTinySize;
	} else if( !Q_stricmp( fontsize, "con_fontsystemsmall" ) ) {
		layout_cursor_font_size = cgs.fontSystemSmallSize;
	} else if( !Q_stricmp( fontsize, "con_fontsystemmedium" ) ) {
		layout_cursor_font_size = cgs.fontSystemMediumSize;
	} else if( !Q_stricmp( fontsize, "con_fontsystembig" ) ) {
		layout_cursor_font_size = cgs.fontSystemBigSize;
	} else {
		layout_cursor_font_size = (int)ceilf( CG_GetNumericArg( &argumentnode ) );
	}

	clamp_low( layout_cursor_font_size, 1 );

	layout_cursor_font_dirty = true;

	return true;
}

static bool CG_LFuncFontStyle( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *fontstyle = CG_GetStringArg( &argumentnode );

	if( !Q_stricmp( fontstyle, "normal" ) ) {
		layout_cursor_font_style = QFONT_STYLE_NONE;
	} else if( !Q_stricmp( fontstyle, "italic" ) ) {
		layout_cursor_font_style = QFONT_STYLE_ITALIC;
	} else if( !Q_stricmp( fontstyle, "bold" ) ) {
		layout_cursor_font_style = QFONT_STYLE_BOLD;
	} else if( !Q_stricmp( fontstyle, "bold-italic" ) ) {
		layout_cursor_font_style = QFONT_STYLE_BOLD | QFONT_STYLE_ITALIC;
	} else {
		CG_Printf( "WARNING 'CG_LFuncFontStyle' Unknown font style '%s'", fontstyle );
		return false;
	}

	layout_cursor_font_dirty = true;

	return true;
}

static bool CG_LFuncDrawObituaries( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int internal_align = (int)CG_GetNumericArg( &argumentnode );
	int icon_size = (int)CG_GetNumericArg( &argumentnode );

	CG_DrawObituaries( layout_cursor_x, layout_cursor_y, layout_cursor_align, CG_GetLayoutCursorFont(), layout_cursor_color,
					   layout_cursor_width, layout_cursor_height, internal_align, icon_size * cgs.vidHeight / 600 );
	return true;
}

static bool CG_LFuncDrawAwards( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawAwards( layout_cursor_x, layout_cursor_y, layout_cursor_align, CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawClock( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawClock( layout_cursor_x, layout_cursor_y, layout_cursor_align, CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawHelpMessage( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	// hide this one when scoreboard is up
	if( !CG_IsScoreboardShown() ) {
		if( !cgs.demoPlaying ) {
			int i;
			int y = layout_cursor_y;
			int font_height = trap_SCR_FontHeight( CG_GetLayoutCursorFont() );
			const char *helpmessage = "";
			vec4_t color;
			bool showhelp = cg_showhelp->integer;

			// scale alpha to text appears more faint if the player's moving
			Vector4Copy( layout_cursor_color, color );

			for( i = 0; i < 3; i++ ) {
				int x = layout_cursor_x;

				switch( i ) {
					case 0:
						helpmessage = "";
						if( showhelp ) {
							if( cg.helpmessage[0] ) {
								helpmessage = cg.helpmessage;
							} else if( cg.matchmessage ) {
								helpmessage = cg.matchmessage;
							}
						}
						break;
					case 1:
						if( !cg.motd ) {
							return true;
						}
						helpmessage = "Message of the day:";
						break;
					case 2:
						helpmessage = cg.motd;
						break;
					default:
						return true;
				}

				if( helpmessage[0] ) {
					y += trap_SCR_DrawMultilineString( x, y, helpmessage, layout_cursor_align,
													   layout_cursor_width, 0, CG_GetLayoutCursorFont(), color ) * font_height;
				}
			}
		}
	}
	return true;
}

static bool CG_LFuncDrawTeamMates( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawTeamMates();
	return true;
}

static bool CG_LFuncDrawDamageNumbers( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawDamageNumbers();
	return true;
}

static bool CG_LFuncDrawBombIndicators( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawBombHUD();
	return true;
}

static bool CG_LFuncDrawPointed( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawPlayerNames( CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawString( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *string = CG_GetStringArg( &argumentnode );

	if( !string || !string[0] ) {
		return false;
	}
	trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align,
						 string, CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawStringRepeat_x( const char *string, int num_draws ) {
	int i;
	char temps[1024];
	size_t pos, string_len;

	if( !string || !string[0] ) {
		return false;
	}
	if( !num_draws ) {
		return false;
	}

	string_len = strlen( string );

	pos = 0;
	for( i = 0; i < num_draws; i++ ) {
		if( pos + string_len >= sizeof( temps ) ) {
			break;
		}
		memcpy( temps + pos, string, string_len );
		pos += string_len;
	}
	temps[pos] = '\0';

	trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align, temps, CG_GetLayoutCursorFont(), layout_cursor_color );

	return true;
}

static bool CG_LFuncDrawStringRepeat( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *string = CG_GetStringArg( &argumentnode );
	int num_draws = CG_GetNumericArg( &argumentnode );
	return CG_LFuncDrawStringRepeat_x( string, num_draws );
}

static bool CG_LFuncDrawStringRepeatConfigString( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *string = CG_GetStringArg( &argumentnode );
	int index = (int)CG_GetNumericArg( &argumentnode );

	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		CG_Printf( "WARNING 'CG_LFuncDrawStringRepeatConfigString' Bad stat_string index" );
		return false;
	}

	int num_draws = atoi( cgs.configStrings[index] );
	return CG_LFuncDrawStringRepeat_x( string, num_draws );
}

static bool CG_LFuncDrawBindString( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char * fmt = CG_GetStringArg( &argumentnode );
	const char * command = CG_GetStringArg( &argumentnode );

	char keys[ 128 ];
	CG_GetBoundKeysString( command, keys, sizeof( keys ) );
	char buf[ 1024 ];
	Q_snprintfz( buf, sizeof( buf ), fmt, keys );

	trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align, buf, CG_GetLayoutCursorFont(), layout_cursor_color );

	return true;
}

static bool CG_LFuncDrawItemNameFromIndex( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	gsitem_t    *item;
	int itemindex = CG_GetNumericArg( &argumentnode );

	item = GS_FindItemByTag( itemindex );
	if( !item || !item->name ) {
		return false;
	}
	trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align, item->name, CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawConfigstring( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int index = (int)CG_GetNumericArg( &argumentnode );

	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		CG_Printf( "WARNING 'CG_LFuncDrawConfigstring' Bad stat_string index" );
		return false;
	}
	trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align,
						 cgs.configStrings[index], CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawCleanConfigstring( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int index = (int)CG_GetNumericArg( &argumentnode );

	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		CG_Printf( "WARNING 'CG_LFuncDrawCleanConfigstring' Bad stat_string index" );
		return false;
	}
	trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align,
						 COM_RemoveColorTokensExt( cgs.configStrings[index], true ), CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawPlayerName( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int index = (int)CG_GetNumericArg( &argumentnode ) - 1;

	if( ( index >= 0 && index < gs.maxclients ) && cgs.clientInfo[index].name[0] ) {
		vec4_t color;
		VectorCopy( colorWhite, color );
		color[3] = layout_cursor_color[3];
		trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align,
							 cgs.clientInfo[index].name, CG_GetLayoutCursorFont(), color );
		return true;
	}
	return false;
}

static bool CG_LFuncDrawCleanPlayerName( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int index = (int)CG_GetNumericArg( &argumentnode ) - 1;

	if( ( index >= 0 && index < gs.maxclients ) && cgs.clientInfo[index].name[0] ) {
		trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align,
							 COM_RemoveColorTokensExt( cgs.clientInfo[index].name, true ), CG_GetLayoutCursorFont(), layout_cursor_color );
		return true;
	}
	return false;
}

static bool CG_LFuncDrawNumeric( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int value = (int)CG_GetNumericArg( &argumentnode );

	trap_SCR_DrawString( layout_cursor_x, layout_cursor_y, layout_cursor_align, va( "%i", value ), CG_GetLayoutCursorFont(), layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawBar( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int value = (int)CG_GetNumericArg( &argumentnode );
	int maxvalue = (int)CG_GetNumericArg( &argumentnode );
	CG_DrawHUDRect( layout_cursor_x, layout_cursor_y, layout_cursor_align,
					layout_cursor_width, layout_cursor_height, value, maxvalue,
					layout_cursor_color, NULL );
	return true;
}

static bool CG_LFuncDrawPicBar( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int value = (int)CG_GetNumericArg( &argumentnode );
	int maxvalue = (int)CG_GetNumericArg( &argumentnode );

	CG_DrawHUDRect( layout_cursor_x, layout_cursor_y, layout_cursor_align,
					layout_cursor_width, layout_cursor_height, value, maxvalue,
					layout_cursor_color, trap_R_RegisterPic( CG_GetStringArg( &argumentnode ) ) );
	return true;
}

static bool CG_LFuncDrawWeaponIcon( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int weapon = cg.predictedPlayerState.stats[STAT_WEAPON];
	int x, y;

	if( weapon < WEAP_GUNBLADE || weapon >= WEAP_TOTAL ) {
		return false;
	}

	x = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_align, layout_cursor_width );
	y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_align, layout_cursor_height );
	trap_R_DrawStretchPic( x, y, layout_cursor_width, layout_cursor_height, 0, 0, 1, 1, layout_cursor_color, CG_GetWeaponIcon( weapon ) );
	return true;
}

static void CG_LFuncsWeaponIcons( struct cg_layoutnode_s *argumentnode ) {
	int offx, offy, w, h;

	offx = (int)( CG_GetNumericArg( &argumentnode ) * cgs.vidWidth / 800 );
	offy = (int)( CG_GetNumericArg( &argumentnode ) * cgs.vidHeight / 600 );
	w = (int)( CG_GetNumericArg( &argumentnode ) * cgs.vidWidth / 800 );
	h = (int)( CG_GetNumericArg( &argumentnode ) * cgs.vidHeight / 600 );

	CG_DrawWeaponIcons( layout_cursor_x, layout_cursor_y, offx, offy, w, h, layout_cursor_align );
}

static bool CG_LFuncDrawWeaponIcons( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_LFuncsWeaponIcons( argumentnode );
	return true;
}

static bool CG_LFuncDrawWeaponStrongAmmo( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int offx, offy, fontsize;

	offx = (int)( CG_GetNumericArg( &argumentnode ) * cgs.vidWidth / 800 );
	offy = (int)( CG_GetNumericArg( &argumentnode ) * cgs.vidHeight / 600 );
	fontsize = (int)CG_GetNumericArg( &argumentnode );

	CG_DrawWeaponAmmos( layout_cursor_x, layout_cursor_y, offx, offy, fontsize, layout_cursor_align );

	return true;
}

static bool CG_LFuncDrawCrossHair( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawCrosshair();
	return true;
}

static bool CG_LFuncDrawKeyState( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *key = CG_GetStringArg( &argumentnode );

	CG_DrawKeyState( layout_cursor_x, layout_cursor_y, layout_cursor_width, layout_cursor_height, layout_cursor_align, key );
	return true;
}

static bool CG_LFuncDrawNet( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawNet( layout_cursor_x, layout_cursor_y, layout_cursor_width, layout_cursor_height, layout_cursor_align, layout_cursor_color );
	return true;
}

static bool CG_LFuncDrawChat( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int padding_x, padding_y;
	struct shader_s *shader;

	padding_x = (int)( CG_GetNumericArg( &argumentnode ) ) * cgs.vidWidth / 800;
	padding_y = (int)( CG_GetNumericArg( &argumentnode ) ) * cgs.vidHeight / 600;
	shader = trap_R_RegisterPic( CG_GetStringArg( &argumentnode ) );

	CG_DrawChat( &cg.chat, layout_cursor_x, layout_cursor_y, layout_cursor_font_name, CG_GetLayoutCursorFont(), layout_cursor_font_size,
				 layout_cursor_width, layout_cursor_height, padding_x, padding_y, layout_cursor_color, shader );
	return true;
}

static bool CG_LFuncIf( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	return (int)CG_GetNumericArg( &argumentnode ) != 0;
}

static bool CG_LFuncIfNot( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments ) {
	return (int)CG_GetNumericArg( &argumentnode ) == 0;
}


typedef struct cg_layoutcommand_s
{
	const char *name;
	bool ( *func )( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments );
	int numparms;
	const char *help;
	bool precache;
} cg_layoutcommand_t;

static const cg_layoutcommand_t cg_LayoutCommands[] =
{
	{
		"setScale",
		CG_LFuncScale,
		1,
		"Sets the cursor scaling method.",
		false
	},

	{
		"setCursor",
		CG_LFuncCursor,
		2,
		"Sets the cursor position to x and y coordinates.",
		false
	},

	{
		"setCursorX",
		CG_LFuncCursorX,
		1,
		"Sets the cursor x position.",
		false
	},

	{
		"setCursorY",
		CG_LFuncCursorY,
		1,
		"Sets the cursor y position.",
		false
	},

	{
		"moveCursor",
		CG_LFuncMoveCursor,
		2,
		"Moves the cursor position by dx and dy.",
		false
	},

	{
		"setAlign",
		CG_LFuncAlign,
		2,
		"Changes align setting. Parameters: horizontal alignment, vertical alignment",
		false
	},

	{
		"setSize",
		CG_LFuncSize,
		2,
		"Sets width and height. Used for pictures and models.",
		false
	},

	{
		"setWidth",
		CG_LFuncSizeWidth,
		1,
		"Sets width. Used for pictures and models.",
		false
	},

	{
		"setHeight",
		CG_LFuncSizeHeight,
		1,
		"Sets height. Used for pictures and models.",
		false
	},

	{
		"setFontFamily",
		CG_LFuncFontFamily,
		1,
		"Sets font by font family. Accepts 'con_fontSystem', as a shortcut to default game font family.",
		false
	},

	{
		"setSpecialFontFamily",
		CG_LFuncSpecialFontFamily,
		1,
		"Sets font by font family. The font will not overriden by the fallback font used when CJK is detected.",
		false
	},

	{
		"setFontSize",
		CG_LFuncFontSize,
		1,
		"Sets font by font name. Accepts 'con_fontSystemSmall', 'con_fontSystemMedium' and 'con_fontSystemBig' as shortcuts to default game fonts sizes.",
		false
	},

	{
		"setFontStyle",
		CG_LFuncFontStyle,
		1,
		"Sets font style. Possible values are: 'normal', 'italic', 'bold' and 'bold-italic'.",
		false
	},

	{
		"setColor",
		CG_LFuncColor,
		4,
		"Sets color setting in RGBA mode. Used for text and pictures",
		false
	},

	{
		"setColorToTeamColor",
		CG_LFuncColorToTeamColor,
		1,
		"Sets cursor color to the color of the team provided in the argument",
		false
	},

	{
		"setColorAlpha",
		CG_LFuncColorAlpha,
		1,
		"Changes the alpha value of the current color",
		false
	},

	{
		"setRotationSpeed",
		CG_LFuncRotationSpeed,
		3,
		"Sets rotation speeds as vector. Used for models",
		false
	},

	{
		"drawObituaries",
		CG_LFuncDrawObituaries,
		2,
		"Draws graphical death messages",
		false
	},

	{
		"drawAwards",
		CG_LFuncDrawAwards,
		0,
		"Draws award messages",
		false
	},

	{
		"drawClock",
		CG_LFuncDrawClock,
		0,
		"Draws clock",
		false
	},

	{
		"drawHelpString",
		CG_LFuncDrawHelpMessage,
		0,
		"Draws the help message",
		false
	},

	{
		"drawPlayerName",
		CG_LFuncDrawPlayerName,
		1,
		"Draws the name of the player with id provided by the argument, colored with color tokens, white by default",
		false
	},

	{
		"drawCleanPlayerName",
		CG_LFuncDrawCleanPlayerName,
		1,
		"Draws the name of the player with id provided by the argument, using the current color",
		false
	},

	{
		"drawPointing",
		CG_LFuncDrawPointed,
		0,
		"Draws the name of the player in the crosshair",
		false
	},

	{
		"drawTeamMates",
		CG_LFuncDrawTeamMates,
		0,
		"Draws indicators where team mates are",
		false
	},

	{
		"drawDamageNumbers",
		CG_LFuncDrawDamageNumbers,
		0,
		"Draws damage numbers",
		false
	},

	{
		"drawBombIndicators",
		CG_LFuncDrawBombIndicators,
		0,
		"Draws bomb HUD",
		false
	},

	{
		"drawStatString",
		CG_LFuncDrawConfigstring,
		1,
		"Draws configstring of argument id",
		false
	},

	{
		"drawCleanStatString",
		CG_LFuncDrawCleanConfigstring,
		1,
		"Draws configstring of argument id, ignoring color codes",
		false
	},

	{
		"drawItemName",
		CG_LFuncDrawItemNameFromIndex,
		1,
		"Draws the name of the item with given item index",
		false
	},

	{
		"drawString",
		CG_LFuncDrawString,
		1,
		"Draws the string in the argument",
		false
	},

	{
		"drawStringNum",
		CG_LFuncDrawNumeric,
		1,
		"Draws numbers as text",
		false
	},

	{
		"drawStringRepeat",
		CG_LFuncDrawStringRepeat,
		2,
		"Draws argument string multiple times",
		false
	},

	{
		"drawStringRepeatConfigString",
		CG_LFuncDrawStringRepeatConfigString,
		2,
		"Draws argument string multiple times",
		false
	},

	{
		"drawBindString",
		CG_LFuncDrawBindString,
		2,
		"Draws a string with %s replaced by a key name",
		false
	},

	{
		"drawBar",
		CG_LFuncDrawBar,
		2,
		"Draws a bar of size setting, the bar is filled in proportion to the arguments",
		false
	},

	{
		"drawPicBar",
		CG_LFuncDrawPicBar,
		3,
		"Draws a picture of size setting, is filled in proportion to the 2 arguments (value, maxvalue). 3rd argument is the picture path",
		false
	},

	{
		"drawCrosshair",
		CG_LFuncDrawCrossHair,
		0,
		"Draws the game crosshair",
		false
	},

	{
		"drawKeyState",
		CG_LFuncDrawKeyState,
		1,
		"Draws icons showing if the argument key is pressed. Possible arg: forward, backward, left, right, fire, jump, crouch, special",
		false
	},

	{
		"drawNetIcon",
		CG_LFuncDrawNet,
		0,
		"Draws the disconnection icon",
		false
	},

	{
		"drawChat",
		CG_LFuncDrawChat,
		3,
		"Draws the game chat messages",
		false
	},

	{
		"drawPicByIndex",
		CG_LFuncDrawPicByIndex,
		1,
		"Draws a pic with argument as imageIndex",
		true
	},

	{
		"drawPicByItemIndex",
		CG_LFuncDrawPicByItemIndex,
		1,
		"Draws a item icon pic with argument as itemIndex",
		false
	},

	{
		"drawPicByName",
		CG_LFuncDrawPicByName,
		1,
		"Draws a pic with argument being the file path",
		true
	},

	{
		"drawSubPicByName",
		CG_LFuncDrawSubPicByName,
		5,
		"Draws a part of a pic with arguments being the file path and the texture coordinates",
		true
	},

	{
		"drawRotatedPicByName",
		CG_LFuncDrawRotatedPicByName,
		2,
		"Draws a pic with arguments being the file path and the rotation",
		true
	},

	{
		"drawModelByIndex",
		CG_LFuncDrawModelByIndex,
		1,
		"Draws a model with argument being the modelIndex",
		true
	},

	{
		"drawModelByName",
		CG_LFuncDrawModelByName,
		2,
		"Draws a model with argument being the path to the model file",
		true
	},

	{
		"drawModelByItemIndex",
		CG_LFuncDrawModelByItemIndex,
		1,
		"Draws a item model with argument being the item index",
		false
	},

	{
		"drawWeaponIcons",
		CG_LFuncDrawWeaponIcons,
		4,
		"Draws the icons of weapon/ammo owned by the player, arguments are offset x, offset y, size x, size y",
		false
	},

	{
		"drawWeaponStrongAmmo",
		CG_LFuncDrawWeaponStrongAmmo,
		3,
		"Draws the amount of strong ammo owned by the player,  arguments are offset x, offset y, fontsize",
		false
	},

	{
		"drawWeaponIcon",
		CG_LFuncDrawWeaponIcon,
		0,
		"Draws the icon of the current weapon",
		false
	},

	{
		"if",
		CG_LFuncIf,
		1,
		"Conditional expression. Argument accepts operations >, <, ==, >=, <=, etc",
		false
	},

	{
		"ifnot",
		CG_LFuncIfNot,
		1,
		"Negative conditional expression. Argument accepts operations >, <, ==, >=, <=, etc",
		false
	},

	{
		"endif",
		NULL,
		0,
		"End of conditional expression block",
		false
	},

	{
		"drawTimer",
		CG_LFuncDrawTimer,
		1,
		"Draws a timer clock for the race gametype",
		false
	},
	{
		"drawPicVar",
		CG_LFuncDrawPicVar,
		6,
		"Draws a picture from a sequence, depending on the value of a given parameter. Parameters: minval, maxval, value, firstimg, lastimg, imagename (replacing ## by the picture number, no leading zeros), starting at 0)",
		false
	},

	{
		NULL,
		NULL,
		0,
		NULL,
		false
	}
};

void Cmd_CG_PrintHudHelp_f( void ) {
	const cg_layoutcommand_t *cmd;
	cg_layoutoperators_t *op;
	int i;
	const gsitem_t *item;
	char *name, *p;

	CG_Printf( "- %sHUD scripts commands\n-------------------------------------%s\n", S_COLOR_YELLOW, S_COLOR_WHITE );
	for( cmd = cg_LayoutCommands; cmd->name; cmd++ ) {
		CG_Printf( "- cmd: %s%s%s expected arguments: %s%i%s\n- desc: %s%s%s\n",
				   S_COLOR_YELLOW, cmd->name, S_COLOR_WHITE,
				   S_COLOR_YELLOW, cmd->numparms, S_COLOR_WHITE,
				   S_COLOR_BLUE, cmd->help, S_COLOR_WHITE );
	}
	CG_Printf( "\n" );

	CG_Printf( "- %sHUD scripts operators\n------------------------------------%s\n", S_COLOR_YELLOW, S_COLOR_WHITE );
	CG_Printf( "- " );
	for( op = cg_LayoutOperators; op->name; op++ ) {
		CG_Printf( "%s%s%s, ", S_COLOR_YELLOW, op->name, S_COLOR_WHITE );
	}
	CG_Printf( "\n\n" );

	CG_Printf( "- %sHUD scripts CONSTANT names\n-------------------------------%s\n", S_COLOR_YELLOW, S_COLOR_WHITE );
	for( item = &itemdefs[1]; item->classname; item++ ) {
		name = Q_strupr( CG_CopyString( item->name ) );
		p = name;
		while( ( p = strchr( p, ' ' ) ) ) {
			*p = '_';
		}

		CG_Printf( "%sITEM_%s%s, ", S_COLOR_YELLOW, name, S_COLOR_WHITE );
	}
	for( i = 0; cg_numeric_constants[i].name != NULL; i++ ) {
		CG_Printf( "%s%s%s, ", S_COLOR_YELLOW, cg_numeric_constants[i].name, S_COLOR_WHITE );
	}
	CG_Printf( "\n\n" );

	CG_Printf( "- %sHUD scripts REFERENCE names\n------------------------------%s\n", S_COLOR_YELLOW, S_COLOR_WHITE );
	for( i = 0; cg_numeric_references[i].name != NULL; i++ ) {
		CG_Printf( "%s%s%s, ", S_COLOR_YELLOW, cg_numeric_references[i].name, S_COLOR_WHITE );
	}
	CG_Printf( "\n" );
}


//=============================================================================


typedef struct cg_layoutnode_s
{
	bool ( *func )( struct cg_layoutnode_s *commandnode, struct cg_layoutnode_s *argumentnode, int numArguments );
	int type;
	char *string;
	int integer;
	float value;
	opFunc_t opFunc;
	struct cg_layoutnode_s *parent;
	struct cg_layoutnode_s *next;
	struct cg_layoutnode_s *ifthread;
	bool precache;
} cg_layoutnode_t;

/*
* CG_GetStringArg
*/
static const char *CG_GetStringArg( struct cg_layoutnode_s **argumentsnode ) {
	struct cg_layoutnode_s *anode = *argumentsnode;

	if( !anode || anode->type == LNODE_COMMAND ) {
		CG_Error( "'CG_LayoutGetIntegerArg': bad arg count" );
	}

	// we can return anything as string
	*argumentsnode = anode->next;
	return anode->string;
}

/*
* CG_GetNumericArg
* can use recursion for mathematical operations
*/
static float CG_GetNumericArg( struct cg_layoutnode_s **argumentsnode ) {
	struct cg_layoutnode_s *anode = *argumentsnode;
	float value;

	if( !anode || anode->type == LNODE_COMMAND ) {
		CG_Error( "'CG_LayoutGetIntegerArg': bad arg count" );
	}

	if( anode->type != LNODE_NUMERIC && anode->type != LNODE_REFERENCE_NUMERIC ) {
		CG_Printf( "WARNING: 'CG_LayoutGetIntegerArg': arg %s is not numeric", anode->string );
	}

	*argumentsnode = anode->next;
	if( anode->type == LNODE_REFERENCE_NUMERIC ) {
		value = cg_numeric_references[anode->integer].func( cg_numeric_references[anode->integer].parameter );
	} else {
		value = anode->value;
	}

	// recurse if there are operators
	if( anode->opFunc != NULL ) {
		value = anode->opFunc( value, CG_GetNumericArg( argumentsnode ) );
	}

	return value;
}

/*
* CG_LayoutParseCommandNode
* alloc a new node for a command
*/
static cg_layoutnode_t *CG_LayoutParseCommandNode( const char *token ) {
	int i = 0;
	const cg_layoutcommand_t *command = NULL;
	cg_layoutnode_t *node;

	for( i = 0; cg_LayoutCommands[i].name; i++ ) {
		if( !Q_stricmp( token, cg_LayoutCommands[i].name ) ) {
			command = &cg_LayoutCommands[i];
			break;
		}
	}

	if( command == NULL ) {
		return NULL;
	}

	node = ( cg_layoutnode_t * )CG_Malloc( sizeof( cg_layoutnode_t ) );
	node->type = LNODE_COMMAND;
	node->integer = command->numparms;
	node->value = 0.0f;
	node->string = CG_CopyString( command->name );
	node->func = command->func;
	node->ifthread = NULL;
	node->precache = command->precache;

	return node;
}

/*
* CG_LayoutParseArgumentNode
* alloc a new node for an argument
*/
static cg_layoutnode_t *CG_LayoutParseArgumentNode( const char *token ) {
	cg_layoutnode_t *node;
	int type = LNODE_NUMERIC;
	char tokcopy[MAX_TOKEN_CHARS], *p;
	const char *valuetok;
	static char tmpstring[8];
	const gsitem_t *item;

	// find what's it
	if( !token ) {
		return NULL;
	}

	valuetok = token;

	if( token[0] == '%' ) { // it's a stat parm
		int i;
		type = LNODE_REFERENCE_NUMERIC;
		valuetok++; // skip %

		// replace stat names by values
		for( i = 0; cg_numeric_references[i].name != NULL; i++ ) {
			if( !Q_stricmp( valuetok, cg_numeric_references[i].name ) ) {
				Q_snprintfz( tmpstring, sizeof( tmpstring ), "%i", i );
				valuetok = tmpstring;
				break;
			}
		}
		if( cg_numeric_references[i].name == NULL ) {
			CG_Printf( "Warning: HUD: %s is not valid numeric reference\n", valuetok );
			valuetok--;
			valuetok = "0";
		}
	} else if( token[0] == '#' ) {   // it's a integer constant
		int i;
		type = LNODE_NUMERIC;
		valuetok++; // skip #

		// replace constants names by values
		if( !strncmp( valuetok, "ITEM_", strlen( "ITEM_" ) ) ) {
			Q_strncpyz( tokcopy, valuetok, sizeof( tokcopy ) );
			valuetok = tokcopy;

			p = tokcopy;
			while( ( p = strchr( p, '_' ) ) ) {
				*p = ' ';
			}
			if( ( item = GS_FindItemByName( valuetok + strlen( "ITEM_" ) ) ) ) {
				Q_snprintfz( tmpstring, sizeof( tmpstring ), "%i", item->tag );
				valuetok = tmpstring;
			}
			if( item == NULL ) {
				CG_Printf( "Warning: HUD: %s is not valid numeric constant\n", valuetok );
				valuetok = "0";
			}
		} else {
			for( i = 0; cg_numeric_constants[i].name != NULL; i++ ) {
				if( !Q_stricmp( valuetok, cg_numeric_constants[i].name ) ) {
					Q_snprintfz( tmpstring, sizeof( tmpstring ), "%i", cg_numeric_constants[i].value );
					valuetok = tmpstring;
					break;
				}
			}
			if( cg_numeric_constants[i].name == NULL ) {
				CG_Printf( "Warning: HUD: %s is not valid numeric constant\n", valuetok );
				valuetok = "0";
			}
		}

#if 0 // not used yet at least
	} else if( token[0] == '$' ) {   // it's a string constant
		int i;
		type = LNODE_STRING;
		valuetok++; // skip $

		// replace stat names by values
		for( i = 0; cg_string_constants[i].name != NULL; i++ ) {
			if( !Q_stricmp( valuetok, cg_string_constants[i].name ) ) {
				Q_snprintfz( tmpstring, sizeof( tmpstring ), "%s", cg_string_constants[i].value );
				valuetok = tmpstring;
				break;
			}
		}
#endif
	} else if( token[0] == '\\' ) {
		valuetok = ++token;
		type = LNODE_STRING;
	} else if( token[0] < '0' && token[0] > '9' && token[0] != '.' ) {
		type = LNODE_STRING;
	}

	// alloc
	node = ( cg_layoutnode_t * )CG_Malloc( sizeof( cg_layoutnode_t ) );
	node->type = type;
	node->integer = atoi( valuetok );
	node->value = atof( valuetok );
	node->string = CG_CopyString( token );
	node->func = NULL;
	node->ifthread = NULL;
	node->precache = false;

	// return it
	return node;
}

/*
* CG_LayoutCathegorizeToken
*/
static int CG_LayoutCathegorizeToken( char *token ) {
	int i = 0;

	for( i = 0; cg_LayoutCommands[i].name; i++ ) {
		if( !Q_stricmp( token, cg_LayoutCommands[i].name ) ) {
			return LNODE_COMMAND;
		}
	}

	if( token[0] == '%' ) { // it's a numerical reference
		return LNODE_REFERENCE_NUMERIC;
	} else if( token[0] == '#' ) {   // it's a numerical constant
		return LNODE_NUMERIC;
#if 0
	} else if( token[0] == '$' ) {   // it's a string constant
		return LNODE_STRING;
#endif
	} else if( token[0] < '0' && token[0] > '9' && token[0] != '.' ) {
		return LNODE_STRING;
	}

	return LNODE_NUMERIC;
}

/*
* CG_RecurseFreeLayoutThread
* recursive for freeing "if" subtrees
*/
static void CG_RecurseFreeLayoutThread( cg_layoutnode_t *rootnode ) {
	cg_layoutnode_t *node;

	if( !rootnode ) {
		return;
	}

	while( rootnode ) {
		node = rootnode;
		rootnode = rootnode->parent;

		if( node->ifthread ) {
			CG_RecurseFreeLayoutThread( node->ifthread );
		}

		if( node->string ) {
			CG_Free( node->string );
		}

		CG_Free( node );
	}
}

/*
* CG_LayoutFixCommasInToken
* commas are accepted in the scripts. They actually do nothing, but are good for readability
*/
static bool CG_LayoutFixCommasInToken( char **ptr, char **backptr ) {
	char *token;
	char *back;
	int offset, count;
	bool stepback = false;

	token = *ptr;
	back = *backptr;

	if( !token || !strlen( token ) ) {
		return false;
	}

	// check that sizes match (quotes are removed from tokens)
	offset = count = strlen( token );
	back = *backptr;
	while( count-- ) {
		if( *back == '"' ) {
			count++;
			offset++;
		}
		back--;
	}

	back = *backptr - offset;
	while( offset ) {
		if( *back == '"' ) {
			offset--;
			back++;
			continue;
		}

		if( *token != *back ) {
			CG_Printf( "Token and Back mismatch %c - %c\n", *token, *back );
		}

		if( *back == ',' ) {
			*back = ' ';
			stepback = true;
		}

		offset--;
		token++;
		back++;
	}

	return stepback;
}

/*
* CG_RecurseParseLayoutScript
* recursive for generating "if" subtrees
*/
static cg_layoutnode_t *CG_RecurseParseLayoutScript( char **ptr, int level ) {
	cg_layoutnode_t *command = NULL;
	cg_layoutnode_t *argumentnode = NULL;
	cg_layoutnode_t *node = NULL;
	cg_layoutnode_t *rootnode = NULL;
	int expecArgs = 0, numArgs = 0;
	int token_type;
	bool add;
	char *token, *s_tokenback;

	if( !ptr ) {
		return NULL;
	}

	if( !*ptr || !*ptr[0] ) {
		return NULL;
	}

	while( *ptr ) {
		s_tokenback = *ptr;

		token = COM_Parse( ptr );
		while( *token == ' ' ) token++; // eat up whitespaces
		if( !Q_stricmp( ",", token ) ) {
			continue;                            // was just a comma
		}
		if( CG_LayoutFixCommasInToken( &token, ptr ) ) {
			*ptr = s_tokenback; // step back
			continue;
		}

		if( !*token ) {
			continue;
		}
		if( !strlen( token ) ) {
			continue;
		}

		add = false;
		token_type = CG_LayoutCathegorizeToken( token );

		// if it's an operator, we don't create a node, but add the operation to the last one
		if( CG_OperatorFuncForArgument( token ) != NULL ) {
			if( !node ) {
				CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" Operator hasn't any prior argument\n", level, token );
				continue;
			}
			if( node->type == LNODE_COMMAND || node->type == LNODE_STRING ) {
				CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" Operator was assigned to a command node\n", level, token );
			} else {
				expecArgs++; // we now expect one extra argument (not counting the operator one)

			}
			node->opFunc = CG_OperatorFuncForArgument( token );
			continue; // skip and continue
		}

		if( expecArgs > numArgs ) {
			// we are expecting an argument
			switch( token_type ) {
				case LNODE_NUMERIC:
				case LNODE_STRING:
				case LNODE_REFERENCE_NUMERIC:
					break;
				case LNODE_COMMAND:
				{
					CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" is not a valid argument for \"%s\"\n", level, token, command ? command->string : "" );
					continue;
				}
				break;
				default:
				{
					CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i) skip and continue: Unrecognized token \"%s\"\n", level, token );
					continue;
				}
				break;
			}
		} else {
			if( token_type != LNODE_COMMAND ) {
				// we are expecting a command
				CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): unrecognized command \"%s\"\n", level, token );
				continue;
			}

			// special case: endif commands interrupt the thread and are not saved
			if( !Q_stricmp( token, "endif" ) ) {
				//finish the last command properly
				if( command ) {
					command->integer = expecArgs;
				}
				return rootnode;
			}

			// special case: last command was "if", we create a new sub-thread and ignore the new command
			if( command && ( !Q_stricmp( command->string, "if" ) || !Q_stricmp( command->string, "ifnot" ) ) ) {
				*ptr = s_tokenback; // step back one token
				command->ifthread = CG_RecurseParseLayoutScript( ptr, level + 1 );
			}
		}

		// things look fine, proceed creating the node
		switch( token_type ) {
			case LNODE_NUMERIC:
			case LNODE_STRING:
			case LNODE_REFERENCE_NUMERIC:
			{
				node = CG_LayoutParseArgumentNode( token );
				if( !node ) {
					CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" is not a valid argument for \"%s\"\n", level, token, command ? command->string : "" );
					break;
				}
				numArgs++;
				add = true;
			}
			break;
			case LNODE_COMMAND:
			{
				node = CG_LayoutParseCommandNode( token );
				if( !node ) {
					CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" is not a valid command\n", level, token );
					break; // skip and continue
				}

				// expected arguments could have been extended by the operators
				if( command ) {
					command->integer = expecArgs;
				}

				// move on into the new command
				command = node;
				argumentnode = NULL;
				numArgs = 0;
				expecArgs = command->integer;
				add = true;
			}
			break;
			default:
				break;
		}

		if( add == true ) {
			if( command && command == rootnode ) {
				if( !argumentnode ) {
					argumentnode = node;
				}
			}

			if( rootnode ) {
				rootnode->next = node;
			}
			node->parent = rootnode;
			rootnode = node;

			// precache arguments by calling the function at load time
			if( command && expecArgs == numArgs && command->func && command->precache ) {
				Vector4Set( layout_cursor_color, 0, 0, 0, 0 );
				layout_cursor_x = -layout_cursor_width - 1;
				layout_cursor_y = -layout_cursor_height - 1;
				layout_cursor_width = 0;
				layout_cursor_height = 0;
				command->func( command, argumentnode, numArgs );
			}
		}
	}

	if( level > 0 ) {
		CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): If without endif\n", level );
	}

	return rootnode;
}

#if 0
static void CG_RecursePrintLayoutThread( cg_layoutnode_t *rootnode, int level ) {
	int i;
	cg_layoutnode_t *node;

	node = rootnode;
	while( node->parent )
		node = node->parent;

	while( node ) {
		for( i = 0; i < level; i++ )
			CG_Printf( "   " );
		CG_Printf( "%s\n", node->string );

		if( node->ifthread ) {
			CG_RecursePrintLayoutThread( node->ifthread, level + 1 );
		}

		node = node->next;
	}
}
#endif

/*
* CG_ParseLayoutScript
*/
static void CG_ParseLayoutScript( char *string, cg_layoutnode_t *rootnode ) {

	CG_RecurseFreeLayoutThread( cg.statusBar );
	cg.statusBar = CG_RecurseParseLayoutScript( &string, 0 );

#if 0
	CG_RecursePrintLayoutThread( cg_layoutRootNode, 0 );
#endif
}

//=============================================================================

//=============================================================================

/*
* CG_RecurseExecuteLayoutThread
* Execution works like this: First node (on backwards) is expected to be the command, followed by arguments nodes.
* we keep a pointer to the command and run the tree counting arguments until we reach the next command,
* then we call the command function sending the pointer to first argument and the pointer to the command.
* At return we advance one node (we stopped at last argument node) so it starts again from the next command (if any).
*
* When finding an "if" command with a subtree, we execute the "if" command. In the case it
* returns any value, we recurse execute the subtree
*/
static void CG_RecurseExecuteLayoutThread( cg_layoutnode_t *rootnode ) {
	cg_layoutnode_t *argumentnode = NULL;
	cg_layoutnode_t *commandnode = NULL;
	int numArguments;

	if( !rootnode ) {
		return;
	}

	// run until the real root
	commandnode = rootnode;
	while( commandnode->parent ) {
		commandnode = commandnode->parent;
	}

	// now run backwards up to the next command node
	while( commandnode ) {
		argumentnode = commandnode->next;

		// we could trust the parser, but I prefer counting the arguments here
		numArguments = 0;
		while( argumentnode ) {
			if( argumentnode->type == LNODE_COMMAND ) {
				break;
			}

			argumentnode = argumentnode->next;
			numArguments++;
		}

		// reset
		argumentnode = commandnode->next;

		// Execute the command node
		if( commandnode->integer != numArguments ) {
			CG_Printf( "ERROR: Layout command %s: invalid argument count (expecting %i, found %i)\n", commandnode->string, commandnode->integer, numArguments );
			return;
		}
		if( commandnode->func ) {
			//special case for if commands
			if( commandnode->func( commandnode, argumentnode, numArguments ) ) {
				// execute the "if" thread when command returns a value
				if( commandnode->ifthread ) {
					CG_RecurseExecuteLayoutThread( commandnode->ifthread );
				}
			}
		}

		//move up to next command node
		commandnode = argumentnode;
		if( commandnode == rootnode ) {
			return;
		}

		while( commandnode && commandnode->type != LNODE_COMMAND ) {
			commandnode = commandnode->next;
		}
	}
}

/*
* CG_ExecuteLayoutProgram
*/
void CG_ExecuteLayoutProgram( struct cg_layoutnode_s *rootnode ) {
	CG_RecurseExecuteLayoutThread( rootnode );
}

//=============================================================================

//=============================================================================



/*
* CG_LoadHUDFile
*/

// Loads the HUD-file recursively. Recursive includes now supported
// Also processes "preload" statements for graphics pre-loading
#define HUD_MAX_LVL 16 // maximum levels of recursive file loading
static char *CG_LoadHUDFile( char *path ) {
	char *rec_fn[HUD_MAX_LVL]; // Recursive filenames...
	char *rec_buf[HUD_MAX_LVL]; // Recursive file contents buffers
	char *rec_ptr[HUD_MAX_LVL]; // Recursive file position buffers
	char *token = NULL, *tmpbuf = NULL, *retbuf = NULL;
	int rec_lvl = 0, rec_plvl = -1;
	int retuse = 0, retlen = 0;
	int f, i, len;

	// Check if path is correct
	if( path == NULL ) {
		return NULL;
	}
	memset( rec_ptr, 0, sizeof( rec_ptr ) );
	memset( rec_buf, 0, sizeof( rec_buf ) );
	memset( rec_ptr, 0, sizeof( rec_ptr ) );

	// Copy the path of the file to the first recursive level filename :)
	rec_fn[rec_lvl] = ( char * )CG_Malloc( strlen( path ) + 1 );
	Q_strncpyz( rec_fn[rec_lvl], path, strlen( path ) + 1 );
	while( 1 ) {
		if( rec_lvl > rec_plvl ) {
			// We went a recursive level higher, our filename should have been filled in :)
			if( !rec_fn[rec_lvl] ) {
				rec_lvl--;
			} else if( rec_fn[rec_lvl][0] == '\0' ) {
				CG_Free( rec_fn[rec_lvl] );
				rec_fn[rec_lvl] = NULL;
				rec_lvl--;
			} else {
				// First check if this file hadn't been included already by one of the previous files
				// in the current file-stack to prevent problems :)
				for( i = 0; i < rec_lvl; i++ ) {
					if( !Q_stricmp( rec_fn[rec_lvl], rec_fn[i] ) ) {
						// Recursive file loading detected!!
						CG_Printf( "HUD: WARNING: Detected recursive file inclusion: %s\n", rec_fn[rec_lvl] );
						CG_Free( rec_fn[rec_lvl] );
						rec_fn[rec_lvl] = NULL;
					}
				}
			}

			// File was OK :)
			if( rec_fn[rec_lvl] != NULL ) {
				len = trap_FS_FOpenFile( rec_fn[rec_lvl], &f, FS_READ );
				if( len > 0 ) {
					rec_plvl = rec_lvl;
					rec_buf[rec_lvl] = ( char * )CG_Malloc( len + 1 );
					rec_buf[rec_lvl][len] = '\0';
					rec_ptr[rec_lvl] = rec_buf[rec_lvl];

					// Now read the file
					if( trap_FS_Read( rec_buf[rec_lvl], len, f ) <= 0 ) {
						CG_Free( rec_fn[rec_lvl] );
						CG_Free( rec_buf[rec_lvl] );
						rec_fn[rec_lvl] = NULL;
						rec_buf[rec_lvl] = NULL;
						if( rec_lvl > 0 ) {
							CG_Printf( "HUD: WARNING: Read error while loading file: %s\n", rec_fn[rec_lvl] );
						}
						rec_lvl--;
					}
					trap_FS_FCloseFile( f );
				} else {
					if( !len ) {
						// File was empty - still have to close
						trap_FS_FCloseFile( f );
					} else if( rec_lvl > 0 ) {
						CG_Printf( "HUD: WARNING: Could not include file: %s\n", rec_fn[rec_lvl] );
					}
					CG_Free( rec_fn[rec_lvl] );
					rec_fn[rec_lvl] = NULL;
					rec_lvl--;
				}
			} else {
				// Skip this file, go down one level
				rec_lvl--;
			}
			rec_plvl = rec_lvl;
		} else if( rec_lvl < rec_plvl ) {
			// Free previous level buffer
			if( rec_fn[rec_plvl] ) {
				CG_Free( rec_fn[rec_plvl] );
			}
			if( rec_buf[rec_plvl] ) {
				CG_Free( rec_buf[rec_plvl] );
			}
			rec_buf[rec_plvl] = NULL;
			rec_ptr[rec_plvl] = NULL;
			rec_fn[rec_plvl] = NULL;
			rec_plvl = rec_lvl;
			if( rec_lvl < 0 ) {
				// Break - end of recursive looping
				if( retbuf == NULL ) {
					CG_Printf( "HUD: ERROR: Could not load empty HUD-script: %s\n", path );
				}
				break;
			}
		}
		if( rec_lvl < 0 ) {
			break;
		}
		token = COM_ParseExt2( ( const char ** )&rec_ptr[rec_lvl], true, false );
		if( !Q_stricmp( "include", token ) ) {
			// Handle include
			token = COM_ParseExt2( ( const char ** )&rec_ptr[rec_lvl], false, false );
			if( ( ( rec_lvl + 1 ) < HUD_MAX_LVL ) && ( rec_ptr[rec_lvl] ) && ( token ) && ( token[0] != '\0' ) ) {
				// Go to next recursive level and prepare it's filename :)
				rec_lvl++;
				i = strlen( "huds/" ) + strlen( token ) + strlen( ".hud" ) + 1;
				rec_fn[rec_lvl] = ( char * )CG_Malloc( i );
				Q_snprintfz( rec_fn[rec_lvl], i, "huds/%s", token );
				COM_DefaultExtension( rec_fn[rec_lvl], ".hud", i );
				if( trap_FS_FOpenFile( rec_fn[rec_lvl], NULL, FS_READ ) < 0 ) {
					// File doesn't exist!
					CG_Free( rec_fn[rec_lvl] );
					i = strlen( "huds/inc/" ) + strlen( token ) + strlen( ".hud" ) + 1;
					rec_fn[rec_lvl] = ( char * )CG_Malloc( i );
					Q_snprintfz( rec_fn[rec_lvl], i, "huds/inc/%s", token );
					COM_DefaultExtension( rec_fn[rec_lvl], ".hud", i );
					if( trap_FS_FOpenFile( rec_fn[rec_lvl], NULL, FS_READ ) < 0 ) {
						CG_Free( rec_fn[rec_lvl] );
						rec_fn[rec_lvl] = NULL;
						rec_lvl--;
					}
				}
			}
		} else if( !Q_stricmp( "precache", token ) ) {
			// Handle graphics precaching
			if( rec_ptr[rec_lvl] == NULL ) {
				CG_Printf( "HUD: ERROR: EOF instead of file argument for preload\n" );
			} else {
				token = COM_ParseExt2( ( const char ** )&rec_ptr[rec_lvl], false, false );
				if( ( token ) && ( token[0] != '\0' ) ) {
					if( developer->integer ) {
						CG_Printf( "HUD: INFO: Precaching image '%s'\n", token );
					}
					trap_R_RegisterPic( token );
				} else {
					CG_Printf( "HUD: ERROR: Missing argument for preload\n" );
				}
			}
		} else if( ( len = strlen( token ) ) > 0 ) {
			// Normal token, add to token-pool.
			if( ( retuse + len + 1 ) >= retlen ) {
				// Enlarge token buffer by 1kb
				retlen += 1024;
				tmpbuf = ( char * )CG_Malloc( retlen );
				if( retbuf ) {
					memcpy( tmpbuf, retbuf, retuse );
					CG_Free( retbuf );
				}
				retbuf = tmpbuf;
				retbuf[retuse] = '\0';
			}
			strcat( &retbuf[retuse], token );
			retuse += len;
			strcat( &retbuf[retuse], " " );
			retuse++;
			retbuf[retuse] = '\0';
		}

		// Detect "end-of-file" of included files and go down 1 level.
		if( ( rec_lvl <= rec_plvl ) && ( !rec_ptr[rec_lvl] ) ) {
			rec_plvl = rec_lvl;
			rec_lvl--;
		}
	}
	if( retbuf == NULL ) {
		CG_Printf( "HUD: ERROR: Could not load file: %s\n", path );
	}
	return retbuf;
}

/*
* CG_LoadStatusBarFile
*/
static void CG_LoadStatusBarFile( char *path ) {
	char *opt;

	assert( path && path[0] );

	//opt = CG_OptimizeStatusBarFile( path, false );
	opt = CG_LoadHUDFile( path );

	if( opt == NULL ) {
		CG_Printf( "HUD: failed to load %s file\n", path );
		return;
	}

	// load the new status bar program
	CG_ParseLayoutScript( opt, cg.statusBar );

	// Free the opt buffer!
	CG_Free( opt );

	// set up layout font as default system font
	Q_strncpyz( layout_cursor_font_name, SYSTEM_FONT_FAMILY, sizeof( layout_cursor_font_name ) );
	layout_cursor_font_style = QFONT_STYLE_NONE;
	layout_cursor_font_size = SYSTEM_FONT_SMALL_SIZE;
	layout_cursor_font_dirty = true;
	layout_cursor_font_regfunc = trap_SCR_RegisterFont;
}

/*
* CG_LoadStatusBar
*/
void CG_LoadStatusBar( void ) {
	cvar_t *hud = ISREALSPECTATOR() ? cg_specHUD : cg_clientHUD;
	const char *default_hud = "default";
	size_t filename_size;
	char *filename;

	assert( hud );

	// buffer for filenames
	filename_size = strlen( "huds/" ) + max( strlen( default_hud ), strlen( hud->string ) ) + 4 + 1;
	filename = ( char * )alloca( filename_size );

	// always load default first. Custom second if needed
	if( cg_debugHUD && cg_debugHUD->integer ) {
		CG_Printf( "HUD: Loading default clientHUD huds/%s\n", default_hud );
	}
	Q_snprintfz( filename, filename_size, "huds/%s", default_hud );
	COM_DefaultExtension( filename, ".hud", filename_size );
	CG_LoadStatusBarFile( filename );

	if( hud->string[0] ) {
		if( Q_stricmp( hud->string, default_hud ) ) {
			if( cg_debugHUD && cg_debugHUD->integer ) {
				CG_Printf( "HUD: Loading custom clientHUD huds/%s\n", hud->string );
			}
			Q_snprintfz( filename, filename_size, "huds/%s", hud->string );
			COM_DefaultExtension( filename, ".hud", filename_size );
			CG_LoadStatusBarFile( filename );
		}
	} else {
		trap_Cvar_Set( hud->name, default_hud );
	}
}
