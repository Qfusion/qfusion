/*
Copyright (C) 2017 Victor Luchits

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

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_ascript.h"

angelwrap_api_t *module_angelExport = NULL;
void gs_asemptyfunc( void ) {}

#ifdef __cplusplus
extern "C" {
#endif

void GS_asInitializeExport( void );

#ifdef __cplusplus
};
#endif

//=======================================================================

static const gs_asEnumVal_t asConfigstringEnumVals[] =
{
	ASLIB_ENUM_VAL( CS_MODMANIFEST ),
	ASLIB_ENUM_VAL( CS_MESSAGE ),
	ASLIB_ENUM_VAL( CS_MAPNAME ),
	ASLIB_ENUM_VAL( CS_AUDIOTRACK ),
	ASLIB_ENUM_VAL( CS_HOSTNAME ),
	ASLIB_ENUM_VAL( CS_TVSERVER ),
	ASLIB_ENUM_VAL( CS_SKYBOX ),
	ASLIB_ENUM_VAL( CS_STATNUMS ),
	ASLIB_ENUM_VAL( CS_POWERUPEFFECTS ),
	ASLIB_ENUM_VAL( CS_GAMETYPETITLE ),
	ASLIB_ENUM_VAL( CS_GAMETYPENAME ),
	ASLIB_ENUM_VAL( CS_GAMETYPEVERSION ),
	ASLIB_ENUM_VAL( CS_GAMETYPEAUTHOR ),
	ASLIB_ENUM_VAL( CS_AUTORECORDSTATE ),
	ASLIB_ENUM_VAL( CS_SCB_PLAYERTAB_LAYOUT ),
	ASLIB_ENUM_VAL( CS_SCB_PLAYERTAB_TITLES ),
	ASLIB_ENUM_VAL( CS_TEAM_ALPHA_NAME ),
	ASLIB_ENUM_VAL( CS_TEAM_BETA_NAME ),
	ASLIB_ENUM_VAL( CS_MAXCLIENTS ),
	ASLIB_ENUM_VAL( CS_MAPCHECKSUM ),
	ASLIB_ENUM_VAL( CS_MATCHNAME ),
	ASLIB_ENUM_VAL( CS_MATCHSCORE ),
	ASLIB_ENUM_VAL( CS_ACTIVE_CALLVOTE ),

	ASLIB_ENUM_VAL( CS_MODELS ),
	ASLIB_ENUM_VAL( CS_SOUNDS ),
	ASLIB_ENUM_VAL( CS_IMAGES ),
	ASLIB_ENUM_VAL( CS_SKINFILES ),
	ASLIB_ENUM_VAL( CS_LIGHTS ),
	ASLIB_ENUM_VAL( CS_ITEMS ),
	ASLIB_ENUM_VAL( CS_PLAYERINFOS ),
	ASLIB_ENUM_VAL( CS_GAMECOMMANDS ),
	ASLIB_ENUM_VAL( CS_LOCATIONS ),
	ASLIB_ENUM_VAL( CS_GENERAL ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asEffectEnumVals[] =
{
	ASLIB_ENUM_VAL( EF_ROTATE_AND_BOB ),
	ASLIB_ENUM_VAL( EF_SHELL ),
	ASLIB_ENUM_VAL( EF_STRONG_WEAPON ),
	ASLIB_ENUM_VAL( EF_QUAD ),
	ASLIB_ENUM_VAL( EF_REGEN ),
	ASLIB_ENUM_VAL( EF_CARRIER ),
	ASLIB_ENUM_VAL( EF_BUSYICON ),
	ASLIB_ENUM_VAL( EF_FLAG_TRAIL ),
	ASLIB_ENUM_VAL( EF_TAKEDAMAGE ),
	ASLIB_ENUM_VAL( EF_TEAMCOLOR_TRANSITION ),
	ASLIB_ENUM_VAL( EF_EXPIRING_QUAD ),
	ASLIB_ENUM_VAL( EF_EXPIRING_SHELL ),
	ASLIB_ENUM_VAL( EF_EXPIRING_REGEN ),
	ASLIB_ENUM_VAL( EF_GODMODE ),

	ASLIB_ENUM_VAL( EF_PLAYER_STUNNED ),
	ASLIB_ENUM_VAL( EF_PLAYER_HIDENAME ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asMatchStateEnumVals[] =
{
	//ASLIB_ENUM_VAL( MATCH_STATE_NONE ), // I see no point in adding it
	ASLIB_ENUM_VAL( MATCH_STATE_WARMUP ),
	ASLIB_ENUM_VAL( MATCH_STATE_COUNTDOWN ),
	ASLIB_ENUM_VAL( MATCH_STATE_PLAYTIME ),
	ASLIB_ENUM_VAL( MATCH_STATE_POSTMATCH ),
	ASLIB_ENUM_VAL( MATCH_STATE_WAITEXIT ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asHUDStatEnumVals[] =
{
	ASLIB_ENUM_VAL( STAT_PROGRESS_SELF ),
	ASLIB_ENUM_VAL( STAT_PROGRESS_OTHER ),
	ASLIB_ENUM_VAL( STAT_PROGRESS_ALPHA ),
	ASLIB_ENUM_VAL( STAT_PROGRESS_BETA ),
	ASLIB_ENUM_VAL( STAT_IMAGE_SELF ),
	ASLIB_ENUM_VAL( STAT_IMAGE_OTHER ),
	ASLIB_ENUM_VAL( STAT_IMAGE_ALPHA ),
	ASLIB_ENUM_VAL( STAT_IMAGE_BETA ),
	ASLIB_ENUM_VAL( STAT_TIME_SELF ),
	ASLIB_ENUM_VAL( STAT_TIME_BEST ),
	ASLIB_ENUM_VAL( STAT_TIME_RECORD ),
	ASLIB_ENUM_VAL( STAT_TIME_ALPHA ),
	ASLIB_ENUM_VAL( STAT_TIME_BETA ),
	ASLIB_ENUM_VAL( STAT_MESSAGE_SELF ),
	ASLIB_ENUM_VAL( STAT_MESSAGE_OTHER ),
	ASLIB_ENUM_VAL( STAT_MESSAGE_ALPHA ),
	ASLIB_ENUM_VAL( STAT_MESSAGE_BETA ),
	ASLIB_ENUM_VAL( STAT_IMAGE_CLASSACTION1 ),
	ASLIB_ENUM_VAL( STAT_IMAGE_CLASSACTION2 ),
	ASLIB_ENUM_VAL( STAT_IMAGE_DROP_ITEM ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asTeamEnumVals[] =
{
	ASLIB_ENUM_VAL( TEAM_SPECTATOR ),
	ASLIB_ENUM_VAL( TEAM_PLAYERS ),
	ASLIB_ENUM_VAL( TEAM_ALPHA ),
	ASLIB_ENUM_VAL( TEAM_BETA ),
	ASLIB_ENUM_VAL( GS_MAX_TEAMS ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asEntityTypeEnumVals[] =
{
	ASLIB_ENUM_VAL( ET_GENERIC ),
	ASLIB_ENUM_VAL( ET_PLAYER ),
	ASLIB_ENUM_VAL( ET_CORPSE ),
	ASLIB_ENUM_VAL( ET_BEAM ),
	ASLIB_ENUM_VAL( ET_PORTALSURFACE ),
	ASLIB_ENUM_VAL( ET_PUSH_TRIGGER ),
	ASLIB_ENUM_VAL( ET_GIB ),
	ASLIB_ENUM_VAL( ET_BLASTER ),
	ASLIB_ENUM_VAL( ET_ELECTRO_WEAK ),
	ASLIB_ENUM_VAL( ET_ROCKET ),
	ASLIB_ENUM_VAL( ET_GRENADE ),
	ASLIB_ENUM_VAL( ET_PLASMA ),
	ASLIB_ENUM_VAL( ET_SPRITE ),
	ASLIB_ENUM_VAL( ET_ITEM ),
	ASLIB_ENUM_VAL( ET_LASERBEAM ),
	ASLIB_ENUM_VAL( ET_CURVELASERBEAM ),
	ASLIB_ENUM_VAL( ET_FLAG_BASE ),
	ASLIB_ENUM_VAL( ET_MINIMAP_ICON ),
	ASLIB_ENUM_VAL( ET_DECAL ),
	ASLIB_ENUM_VAL( ET_ITEM_TIMER ),
	ASLIB_ENUM_VAL( ET_PARTICLES ),
	ASLIB_ENUM_VAL( ET_SPAWN_INDICATOR ),
	ASLIB_ENUM_VAL( ET_RADAR ),

	ASLIB_ENUM_VAL( ET_EVENT ),
	ASLIB_ENUM_VAL( ET_SOUNDEVENT ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asSolidEnumVals[] =
{
	ASLIB_ENUM_VAL( SOLID_NOT ),
	ASLIB_ENUM_VAL( SOLID_TRIGGER ),
	ASLIB_ENUM_VAL( SOLID_YES ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asPMoveFeaturesVals[] =
{
	ASLIB_ENUM_VAL( PMFEAT_CROUCH ),
	ASLIB_ENUM_VAL( PMFEAT_WALK ),
	ASLIB_ENUM_VAL( PMFEAT_JUMP ),
	ASLIB_ENUM_VAL( PMFEAT_DASH ),
	ASLIB_ENUM_VAL( PMFEAT_WALLJUMP ),
	ASLIB_ENUM_VAL( PMFEAT_FWDBUNNY ),
	ASLIB_ENUM_VAL( PMFEAT_AIRCONTROL ),
	ASLIB_ENUM_VAL( PMFEAT_ZOOM ),
	ASLIB_ENUM_VAL( PMFEAT_GHOSTMOVE ),
	ASLIB_ENUM_VAL( PMFEAT_CONTINOUSJUMP ),
	ASLIB_ENUM_VAL( PMFEAT_ITEMPICK ),
	ASLIB_ENUM_VAL( PMFEAT_GUNBLADEAUTOATTACK ),
	ASLIB_ENUM_VAL( PMFEAT_WEAPONSWITCH ),
	ASLIB_ENUM_VAL( PMFEAT_CORNERSKIMMING ),
	ASLIB_ENUM_VAL( PMFEAT_CROUCHSLIDING ),
	ASLIB_ENUM_VAL( PMFEAT_ALL ),
	ASLIB_ENUM_VAL( PMFEAT_DEFAULT ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asItemTypeEnumVals[] =
{
	ASLIB_ENUM_VAL( IT_WEAPON ),
	ASLIB_ENUM_VAL( IT_AMMO ),
	ASLIB_ENUM_VAL( IT_ARMOR ),
	ASLIB_ENUM_VAL( IT_POWERUP ),
	ASLIB_ENUM_VAL( IT_HEALTH ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asWeaponTagEnumVals[] =
{
	ASLIB_ENUM_VAL( WEAP_NONE ),
	ASLIB_ENUM_VAL( WEAP_GUNBLADE ),
	ASLIB_ENUM_VAL( WEAP_MACHINEGUN ),
	ASLIB_ENUM_VAL( WEAP_RIOTGUN ),
	ASLIB_ENUM_VAL( WEAP_GRENADELAUNCHER ),
	ASLIB_ENUM_VAL( WEAP_ROCKETLAUNCHER ),
	ASLIB_ENUM_VAL( WEAP_PLASMAGUN ),
	ASLIB_ENUM_VAL( WEAP_LASERGUN ),
	ASLIB_ENUM_VAL( WEAP_ELECTROBOLT ),
	ASLIB_ENUM_VAL( WEAP_INSTAGUN ),
	ASLIB_ENUM_VAL( WEAP_TOTAL ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asAmmoTagEnumVals[] =
{
	ASLIB_ENUM_VAL( AMMO_NONE ),
	ASLIB_ENUM_VAL( AMMO_GUNBLADE ),
	ASLIB_ENUM_VAL( AMMO_BULLETS ),
	ASLIB_ENUM_VAL( AMMO_SHELLS ),
	ASLIB_ENUM_VAL( AMMO_GRENADES ),
	ASLIB_ENUM_VAL( AMMO_ROCKETS ),
	ASLIB_ENUM_VAL( AMMO_PLASMA ),
	ASLIB_ENUM_VAL( AMMO_LASERS ),
	ASLIB_ENUM_VAL( AMMO_BOLTS ),
	ASLIB_ENUM_VAL( AMMO_INSTAS ),

	ASLIB_ENUM_VAL( AMMO_WEAK_GUNBLADE ),
	ASLIB_ENUM_VAL( AMMO_WEAK_BULLETS ),
	ASLIB_ENUM_VAL( AMMO_WEAK_SHELLS ),
	ASLIB_ENUM_VAL( AMMO_WEAK_GRENADES ),
	ASLIB_ENUM_VAL( AMMO_WEAK_ROCKETS ),
	ASLIB_ENUM_VAL( AMMO_WEAK_PLASMA ),
	ASLIB_ENUM_VAL( AMMO_WEAK_LASERS ),
	ASLIB_ENUM_VAL( AMMO_WEAK_BOLTS ),
	ASLIB_ENUM_VAL( AMMO_WEAK_INSTAS ),

	ASLIB_ENUM_VAL( AMMO_TOTAL ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asArmorTagEnumVals[] =
{
	ASLIB_ENUM_VAL( ARMOR_NONE ),
	ASLIB_ENUM_VAL( ARMOR_GA ),
	ASLIB_ENUM_VAL( ARMOR_YA ),
	ASLIB_ENUM_VAL( ARMOR_RA ),
	ASLIB_ENUM_VAL( ARMOR_SHARD ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asHealthTagEnumVals[] =
{
	ASLIB_ENUM_VAL( HEALTH_NONE ),
	ASLIB_ENUM_VAL( HEALTH_SMALL ),
	ASLIB_ENUM_VAL( HEALTH_MEDIUM ),
	ASLIB_ENUM_VAL( HEALTH_LARGE ),
	ASLIB_ENUM_VAL( HEALTH_MEGA ),
	ASLIB_ENUM_VAL( HEALTH_ULTRA ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asPowerupTagEnumVals[] =
{
	ASLIB_ENUM_VAL( POWERUP_NONE ),
	ASLIB_ENUM_VAL( POWERUP_QUAD ),
	ASLIB_ENUM_VAL( POWERUP_SHELL ),
	ASLIB_ENUM_VAL( POWERUP_REGEN ),

	ASLIB_ENUM_VAL( POWERUP_TOTAL ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asMiscItemTagEnumVals[] =
{
	ASLIB_ENUM_VAL( AMMO_PACK_WEAK ),
	ASLIB_ENUM_VAL( AMMO_PACK_STRONG ),
	ASLIB_ENUM_VAL( AMMO_PACK ),
	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asClientStateEnumVals[] =
{
	ASLIB_ENUM_VAL( CS_FREE ),
	ASLIB_ENUM_VAL( CS_ZOMBIE ),
	ASLIB_ENUM_VAL( CS_CONNECTING ),
	ASLIB_ENUM_VAL( CS_CONNECTED ),
	ASLIB_ENUM_VAL( CS_SPAWNED ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asSoundChannelEnumVals[] =
{
	ASLIB_ENUM_VAL( CHAN_AUTO ),
	ASLIB_ENUM_VAL( CHAN_PAIN ),
	ASLIB_ENUM_VAL( CHAN_VOICE ),
	ASLIB_ENUM_VAL( CHAN_ITEM ),
	ASLIB_ENUM_VAL( CHAN_BODY ),
	ASLIB_ENUM_VAL( CHAN_MUZZLEFLASH ),
	ASLIB_ENUM_VAL( CHAN_FIXED ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asContentsEnumVals[] =
{
	ASLIB_ENUM_VAL( CONTENTS_SOLID ),
	ASLIB_ENUM_VAL( CONTENTS_LAVA ),
	ASLIB_ENUM_VAL( CONTENTS_SLIME ),
	ASLIB_ENUM_VAL( CONTENTS_WATER ),
	ASLIB_ENUM_VAL( CONTENTS_FOG ),
	ASLIB_ENUM_VAL( CONTENTS_AREAPORTAL ),
	ASLIB_ENUM_VAL( CONTENTS_PLAYERCLIP ),
	ASLIB_ENUM_VAL( CONTENTS_MONSTERCLIP ),
	ASLIB_ENUM_VAL( CONTENTS_TELEPORTER ),
	ASLIB_ENUM_VAL( CONTENTS_JUMPPAD ),
	ASLIB_ENUM_VAL( CONTENTS_CLUSTERPORTAL ),
	ASLIB_ENUM_VAL( CONTENTS_DONOTENTER ),
	ASLIB_ENUM_VAL( CONTENTS_ORIGIN ),
	ASLIB_ENUM_VAL( CONTENTS_BODY ),
	ASLIB_ENUM_VAL( CONTENTS_CORPSE ),
	ASLIB_ENUM_VAL( CONTENTS_DETAIL ),
	ASLIB_ENUM_VAL( CONTENTS_STRUCTURAL ),
	ASLIB_ENUM_VAL( CONTENTS_TRANSLUCENT ),
	ASLIB_ENUM_VAL( CONTENTS_TRIGGER ),
	ASLIB_ENUM_VAL( CONTENTS_NODROP ),
	ASLIB_ENUM_VAL( MASK_ALL ),
	ASLIB_ENUM_VAL( MASK_SOLID ),
	ASLIB_ENUM_VAL( MASK_PLAYERSOLID ),
	ASLIB_ENUM_VAL( MASK_DEADSOLID ),
	ASLIB_ENUM_VAL( MASK_MONSTERSOLID ),
	ASLIB_ENUM_VAL( MASK_WATER ),
	ASLIB_ENUM_VAL( MASK_OPAQUE ),
	ASLIB_ENUM_VAL( MASK_SHOT ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asSurfFlagEnumVals[] =
{
	ASLIB_ENUM_VAL( SURF_NODAMAGE ),
	ASLIB_ENUM_VAL( SURF_SLICK ),
	ASLIB_ENUM_VAL( SURF_SKY ),
	ASLIB_ENUM_VAL( SURF_LADDER ),
	ASLIB_ENUM_VAL( SURF_NOIMPACT ),
	ASLIB_ENUM_VAL( SURF_NOMARKS ),
	ASLIB_ENUM_VAL( SURF_FLESH ),
	ASLIB_ENUM_VAL( SURF_NODRAW ),
	ASLIB_ENUM_VAL( SURF_HINT ),
	ASLIB_ENUM_VAL( SURF_SKIP ),
	ASLIB_ENUM_VAL( SURF_NOLIGHTMAP ),
	ASLIB_ENUM_VAL( SURF_POINTLIGHT ),
	ASLIB_ENUM_VAL( SURF_METALSTEPS ),
	ASLIB_ENUM_VAL( SURF_NOSTEPS ),
	ASLIB_ENUM_VAL( SURF_NONSOLID ),
	ASLIB_ENUM_VAL( SURF_LIGHTFILTER ),
	ASLIB_ENUM_VAL( SURF_ALPHASHADOW ),
	ASLIB_ENUM_VAL( SURF_NODLIGHT ),
	ASLIB_ENUM_VAL( SURF_DUST ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asSVFlagEnumVals[] =
{
	ASLIB_ENUM_VAL( SVF_NOCLIENT ),
	ASLIB_ENUM_VAL( SVF_PORTAL ),
	ASLIB_ENUM_VAL( SVF_TRANSMITORIGIN2 ),
	ASLIB_ENUM_VAL( SVF_SOUNDCULL ),
	ASLIB_ENUM_VAL( SVF_FAKECLIENT ),
	ASLIB_ENUM_VAL( SVF_BROADCAST ),
	ASLIB_ENUM_VAL( SVF_CORPSE ),
	ASLIB_ENUM_VAL( SVF_PROJECTILE ),
	ASLIB_ENUM_VAL( SVF_ONLYTEAM ),
	ASLIB_ENUM_VAL( SVF_FORCEOWNER ),
	ASLIB_ENUM_VAL( SVF_ONLYOWNER ),
	ASLIB_ENUM_VAL( SVF_FORCETEAM ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asMeaningsOfDeathEnumVals[] =
{
	ASLIB_ENUM_VAL( MOD_GUNBLADE_W ),
	ASLIB_ENUM_VAL( MOD_GUNBLADE_S ),
	ASLIB_ENUM_VAL( MOD_MACHINEGUN_W ),
	ASLIB_ENUM_VAL( MOD_MACHINEGUN_S ),
	ASLIB_ENUM_VAL( MOD_RIOTGUN_W ),
	ASLIB_ENUM_VAL( MOD_RIOTGUN_S ),
	ASLIB_ENUM_VAL( MOD_GRENADE_W ),
	ASLIB_ENUM_VAL( MOD_GRENADE_S ),
	ASLIB_ENUM_VAL( MOD_ROCKET_W ),
	ASLIB_ENUM_VAL( MOD_ROCKET_S ),
	ASLIB_ENUM_VAL( MOD_PLASMA_W ),
	ASLIB_ENUM_VAL( MOD_PLASMA_S ),
	ASLIB_ENUM_VAL( MOD_ELECTROBOLT_W ),
	ASLIB_ENUM_VAL( MOD_ELECTROBOLT_S ),
	ASLIB_ENUM_VAL( MOD_INSTAGUN_W ),
	ASLIB_ENUM_VAL( MOD_INSTAGUN_S ),
	ASLIB_ENUM_VAL( MOD_LASERGUN_W ),
	ASLIB_ENUM_VAL( MOD_LASERGUN_S ),
	ASLIB_ENUM_VAL( MOD_GRENADE_SPLASH_W ),
	ASLIB_ENUM_VAL( MOD_GRENADE_SPLASH_S ),
	ASLIB_ENUM_VAL( MOD_ROCKET_SPLASH_W ),
	ASLIB_ENUM_VAL( MOD_ROCKET_SPLASH_S ),
	ASLIB_ENUM_VAL( MOD_PLASMA_SPLASH_W ),
	ASLIB_ENUM_VAL( MOD_PLASMA_SPLASH_S ),

	// World damage
	ASLIB_ENUM_VAL( MOD_WATER ),
	ASLIB_ENUM_VAL( MOD_SLIME ),
	ASLIB_ENUM_VAL( MOD_LAVA ),
	ASLIB_ENUM_VAL( MOD_CRUSH ),
	ASLIB_ENUM_VAL( MOD_TELEFRAG ),
	ASLIB_ENUM_VAL( MOD_FALLING ),
	ASLIB_ENUM_VAL( MOD_SUICIDE ),
	ASLIB_ENUM_VAL( MOD_EXPLOSIVE ),

	// probably not used
	ASLIB_ENUM_VAL( MOD_BARREL ),
	ASLIB_ENUM_VAL( MOD_BOMB ),
	ASLIB_ENUM_VAL( MOD_EXIT ),
	ASLIB_ENUM_VAL( MOD_SPLASH ),
	ASLIB_ENUM_VAL( MOD_TARGET_LASER ),
	ASLIB_ENUM_VAL( MOD_TRIGGER_HURT ),
	ASLIB_ENUM_VAL( MOD_HIT ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asKeyiconEnumVals[] =
{
	ASLIB_ENUM_VAL( KEYICON_FORWARD ),
	ASLIB_ENUM_VAL( KEYICON_BACKWARD ),
	ASLIB_ENUM_VAL( KEYICON_LEFT ),
	ASLIB_ENUM_VAL( KEYICON_RIGHT ),
	ASLIB_ENUM_VAL( KEYICON_FIRE ),
	ASLIB_ENUM_VAL( KEYICON_JUMP ),
	ASLIB_ENUM_VAL( KEYICON_CROUCH ),
	ASLIB_ENUM_VAL( KEYICON_SPECIAL ),
	ASLIB_ENUM_VAL( KEYICON_TOTAL ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asAxisEnumVals[] =
{
	ASLIB_ENUM_VAL( PITCH ),
	ASLIB_ENUM_VAL( YAW ),
	ASLIB_ENUM_VAL( ROLL ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asButtonEnumVals[] =
{
	ASLIB_ENUM_VAL( BUTTON_NONE ),
	ASLIB_ENUM_VAL( BUTTON_ATTACK ),
	ASLIB_ENUM_VAL( BUTTON_WALK ),
	ASLIB_ENUM_VAL( BUTTON_SPECIAL ),
	ASLIB_ENUM_VAL( BUTTON_USE ),
	ASLIB_ENUM_VAL( BUTTON_ZOOM ),
	ASLIB_ENUM_VAL( BUTTON_BUSYICON ),
	ASLIB_ENUM_VAL( BUTTON_ANY ),

	ASLIB_ENUM_VAL_NULL
};

//=======================================================================

static const gs_asEnum_t asGameEnums[] =
{
	{ "configstrings_e", asConfigstringEnumVals },
	{ "state_effects_e", asEffectEnumVals },
	{ "matchstates_e", asMatchStateEnumVals },
	{ "hudstats_e", asHUDStatEnumVals },
	{ "teams_e", asTeamEnumVals },
	{ "entitytype_e", asEntityTypeEnumVals },
	{ "solid_e", asSolidEnumVals },
	{ "pmovefeats_e", asPMoveFeaturesVals },
	{ "itemtype_e", asItemTypeEnumVals },

	{ "weapon_tag_e", asWeaponTagEnumVals },
	{ "ammo_tag_e", asAmmoTagEnumVals },
	{ "armor_tag_e", asArmorTagEnumVals },
	{ "health_tag_e", asHealthTagEnumVals },
	{ "powerup_tag_e", asPowerupTagEnumVals },
	{ "otheritems_tag_e", asMiscItemTagEnumVals },

	{ "client_statest_e", asClientStateEnumVals },
	{ "sound_channels_e", asSoundChannelEnumVals },
	{ "contents_e", asContentsEnumVals },
	{ "surfaceflags_e", asSurfFlagEnumVals },
	{ "serverflags_e", asSVFlagEnumVals },
	{ "meaningsofdeath_e", asMeaningsOfDeathEnumVals },
	{ "keyicon_e", asKeyiconEnumVals },

	{ "axis_e", asAxisEnumVals },
	{ "button_e", asButtonEnumVals },

	ASLIB_ENUM_VAL_NULL
};

/*
* GS_asRegisterEnums
*/
void GS_asRegisterEnums( asIScriptEngine *asEngine, const gs_asEnum_t *asEnums, const char *nameSpace ) {
	int i, j;
	const gs_asEnum_t *asEnum;
	const gs_asEnumVal_t *asEnumVal;

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( nameSpace );
	} else {
		asEngine->SetDefaultNamespace( "" );
	}

	for( i = 0, asEnum = asEnums; asEnum->name != NULL; i++, asEnum++ ) {
		asEngine->RegisterEnum( asEnum->name );

		for( j = 0, asEnumVal = asEnum->values; asEnumVal->name != NULL; j++, asEnumVal++ )
			asEngine->RegisterEnumValue( asEnum->name, asEnumVal->name, asEnumVal->value );
	}

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( "" );
	}
}

//=======================================================================

/*
* GS_asRegisterFuncdefs
*/
void GS_asRegisterFuncdefs( asIScriptEngine *asEngine, const gs_asFuncdef_t *asFuncdefs, const char *nameSpace ) {
	const gs_asFuncdef_t *asFuncdef;

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( nameSpace );
	} else {
		asEngine->SetDefaultNamespace( "" );
	}

	for( asFuncdef = asFuncdefs; asFuncdef->declaration != NULL; asFuncdef++ ) {
		asEngine->RegisterFuncdef( asFuncdef->declaration );
	}

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( "" );
	}
}

//=======================================================================

// CLASS: Trace
typedef struct
{
	trace_t trace;
} astrace_t;

void objectTrace_DefaultConstructor( astrace_t *self ) {
	memset( &self->trace, 0, sizeof( trace_t ) );
}

void objectTrace_CopyConstructor( astrace_t *other, astrace_t *self ) {
	self->trace = other->trace;
}

static bool objectTrace_doTrace4D( asvec3_t *start, asvec3_t *mins, asvec3_t *maxs, asvec3_t *end, int ignore, int contentMask, int timeDelta, astrace_t *self ) {
	if( !start || !end ) { // should never happen unless the coder explicitly feeds null
		gs.api.Printf( "* WARNING: gametype plug-in script attempted to call method 'trace.doTrace' with a null vector pointer\n* Tracing skept" );
		return false;
	}

	gs.api.Trace( &self->trace, start->v, mins ? mins->v : vec3_origin, maxs ? maxs->v : vec3_origin, end->v, ignore, contentMask, 0 );

	if( self->trace.startsolid || self->trace.allsolid ) {
		return true;
	}

	return ( self->trace.ent != -1 ) ? true : false;
}

static bool objectTrace_doTrace( asvec3_t *start, asvec3_t *mins, asvec3_t *maxs, asvec3_t *end, int ignore, int contentMask, astrace_t *self ) {
	return objectTrace_doTrace4D( start, mins, maxs, end, ignore, contentMask, 0, self );
}

static asvec3_t objectTrace_getEndPos( astrace_t *self ) {
	asvec3_t asvec;

	VectorCopy( self->trace.endpos, asvec.v );
	return asvec;
}

static asvec3_t objectTrace_getPlaneNormal( astrace_t *self ) {
	asvec3_t asvec;

	VectorCopy( self->trace.plane.normal, asvec.v );
	return asvec;
}

static const gs_asFuncdef_t astrace_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t astrace_ObjectBehaviors[] =
{
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( ) ), asFUNCTION( objectTrace_DefaultConstructor ), asCALL_CDECL_OBJLAST },
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( const Trace &in ) ), asFUNCTION( objectTrace_CopyConstructor ), asCALL_CDECL_OBJLAST },

	ASLIB_BEHAVIOR_NULL
};

static const gs_asMethod_t astrace_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( bool, doTrace, ( const Vec3 &in, const Vec3 &in, const Vec3 &in, const Vec3 &in, int ignore, int contentMask ) const ), asFUNCTION( objectTrace_doTrace ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, doTrace4D, ( const Vec3 &in, const Vec3 &in, const Vec3 &in, const Vec3 &in, int ignore, int contentMask, int timeDelta ) const ), asFUNCTION( objectTrace_doTrace4D ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_endPos, ( ) const ), asFUNCTION( objectTrace_getEndPos ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_planeNormal, ( ) const ), asFUNCTION( objectTrace_getPlaneNormal ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const gs_asProperty_t astrace_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( const bool, allSolid ), ASLIB_FOFFSET( astrace_t, trace.allsolid ) },
	{ ASLIB_PROPERTY_DECL( const bool, startSolid ), ASLIB_FOFFSET( astrace_t, trace.startsolid ) },
	{ ASLIB_PROPERTY_DECL( const float, fraction ), ASLIB_FOFFSET( astrace_t, trace.fraction ) },
	{ ASLIB_PROPERTY_DECL( const int, surfFlags ), ASLIB_FOFFSET( astrace_t, trace.surfFlags ) },
	{ ASLIB_PROPERTY_DECL( const int, contents ), ASLIB_FOFFSET( astrace_t, trace.contents ) },
	{ ASLIB_PROPERTY_DECL( const int, entNum ), ASLIB_FOFFSET( astrace_t, trace.ent ) },
	{ ASLIB_PROPERTY_DECL( const float, planeDist ), ASLIB_FOFFSET( astrace_t, trace.plane.dist ) },
	{ ASLIB_PROPERTY_DECL( const int16, planeType ), ASLIB_FOFFSET( astrace_t, trace.plane.type ) },
	{ ASLIB_PROPERTY_DECL( const int16, planeSignBits ), ASLIB_FOFFSET( astrace_t, trace.plane.signbits ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asTraceClassDescriptor =
{
	"Trace",                    /* name */
	asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CK,   /* object type flags */
	sizeof( astrace_t ),        /* size */
	astrace_Funcdefs,           /* funcdefs */
	astrace_ObjectBehaviors,    /* object behaviors */
	astrace_Methods,            /* methods */
	astrace_Properties,         /* properties */

	NULL, NULL                  /* string factory hack */
};

//=======================================================================

// CLASS: Item
static asstring_t *objectGItem_getClassName( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->classname, self->classname ? strlen( self->classname ) : 0 );
}

static asstring_t *objectGItem_getName( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->name, self->name ? strlen( self->name ) : 0 );
}

static asstring_t *objectGItem_getShortName( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->shortname, self->shortname ? strlen( self->shortname ) : 0 );
}

static asstring_t *objectGItem_getModelName( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->world_model[0], self->world_model[0] ? strlen( self->world_model[0] ) : 0 );
}

static asstring_t *objectGItem_getModel2Name( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->world_model[1], self->world_model[1] ? strlen( self->world_model[1] ) : 0 );
}

static asstring_t *objectGItem_getIconName( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->icon, self->icon ? strlen( self->icon ) : 0 );
}

static asstring_t *objectGItem_getSimpleItemName( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->simpleitem, self->simpleitem ? strlen( self->simpleitem ) : 0 );
}

static asstring_t *objectGItem_getPickupSoundName( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->pickup_sound, self->pickup_sound ? strlen( self->pickup_sound ) : 0 );
}

static asstring_t *objectGItem_getColorToken( gsitem_t *self ) {
	return module_angelExport->asStringFactoryBuffer( self->color, self->color ? strlen( self->color ) : 0 );
}

static bool objectGItem_isPickable( gsitem_t *self ) {
	return ( self && ( self->flags & ITFLAG_PICKABLE ) ) ? true : false;
}

static bool objectGItem_isUsable( gsitem_t *self ) {
	return ( self && ( self->flags & ITFLAG_USABLE ) ) ? true : false;
}

static bool objectGItem_isDropable( gsitem_t *self ) {
	return ( self && ( self->flags & ITFLAG_DROPABLE ) ) ? true : false;
}

static const gs_asFuncdef_t asitem_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t asitem_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const gs_asMethod_t asitem_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( const String @, get_classname, ( ) const ), asFUNCTION( objectGItem_getClassName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_name, ( ) const ), asFUNCTION( objectGItem_getName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_shortName, ( ) const ), asFUNCTION( objectGItem_getShortName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_model, ( ) const ), asFUNCTION( objectGItem_getModelName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_model2, ( ) const ), asFUNCTION( objectGItem_getModel2Name ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_icon, ( ) const ), asFUNCTION( objectGItem_getIconName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_simpleIcon, ( ) const ), asFUNCTION( objectGItem_getSimpleItemName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_pickupSound, ( ) const ), asFUNCTION( objectGItem_getPickupSoundName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_colorToken, ( ) const ), asFUNCTION( objectGItem_getColorToken ), asCALL_CDECL_OBJLAST },

	{ ASLIB_FUNCTION_DECL( bool, isPickable, ( ) const ), asFUNCTION( objectGItem_isPickable ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, isUsable, ( ) const ), asFUNCTION( objectGItem_isUsable ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, isDropable, ( ) const ), asFUNCTION( objectGItem_isDropable ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const gs_asProperty_t asitem_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( const int, tag ), ASLIB_FOFFSET( gsitem_t, tag ) },
	{ ASLIB_PROPERTY_DECL( const uint, type ), ASLIB_FOFFSET( gsitem_t, type ) },
	{ ASLIB_PROPERTY_DECL( const int, flags ), ASLIB_FOFFSET( gsitem_t, flags ) },
	{ ASLIB_PROPERTY_DECL( const int, quantity ), ASLIB_FOFFSET( gsitem_t, quantity ) },
	{ ASLIB_PROPERTY_DECL( const int, inventoryMax ), ASLIB_FOFFSET( gsitem_t, inventory_max ) },
	{ ASLIB_PROPERTY_DECL( const int, ammoTag ), ASLIB_FOFFSET( gsitem_t, ammo_tag ) },
	{ ASLIB_PROPERTY_DECL( const int, weakAmmoTag ), ASLIB_FOFFSET( gsitem_t, weakammo_tag ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asItemClassDescriptor =
{
	"Item",                     /* name */
	asOBJ_REF | asOBJ_NOCOUNT,  /* object type flags */
	sizeof( gsitem_t ),         /* size */
	asitem_Funcdefs,            /* funcdefs */
	asitem_ObjectBehaviors,     /* object behaviors */
	asitem_Methods,             /* methods */
	asitem_Properties,          /* properties */

	NULL, NULL                  /* string factory hack */
};

//=======================================================================

// CLASS: EntityState

void objectEntityState_DefaultConstructor( entity_state_t *state ) {
	memset( state, 0, sizeof( entity_state_t ) );
}

void objectEntityState_CopyConstructor( entity_state_t *other, entity_state_t *state ) {
	*state = *other;
}

static const gs_asFuncdef_t asEntityState_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t asEntityState_ObjectBehaviors[] =
{
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( ) ), asFUNCTION( objectEntityState_DefaultConstructor ), asCALL_CDECL_OBJLAST },
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( const Trace &in ) ), asFUNCTION( objectEntityState_CopyConstructor ), asCALL_CDECL_OBJLAST },
	
	ASLIB_BEHAVIOR_NULL
};

static asvec3_t objectEntityState_GetOrigin( entity_state_t *state ) {
	asvec3_t origin;
	VectorCopy( state->origin, origin.v );
	return origin;
}

static void objectEntityState_SetOrigin( asvec3_t *vec, entity_state_t *state ) {
	VectorCopy( vec->v, state->origin );
}

static asvec3_t objectEntityState_GetOrigin2( entity_state_t *state ) {
	asvec3_t origin;
	VectorCopy( state->origin2, origin.v );
	return origin;
}

static void objectEntityState_SetOrigin2( asvec3_t *vec, entity_state_t *state ) {
	VectorCopy( vec->v, state->origin2 );
}

static asvec3_t objectEntityState_GetOrigin3( entity_state_t *state ) {
	asvec3_t origin;
	VectorCopy( state->origin3, origin.v );
	return origin;
}

static void objectEntityState_SetOrigin3( asvec3_t *vec, entity_state_t *state ) {
	VectorCopy( vec->v, state->origin3 );
}

static asvec3_t objectEntityState_GetAngles( entity_state_t *state ) {
	asvec3_t angles;
	VectorCopy( state->angles, angles.v );
	return angles;
}

static void objectEntityState_SetAngles( asvec3_t *vec, entity_state_t *state ) {
	VectorCopy( vec->v, state->angles );
}

static asvec3_t objectEntityState_GetLinearMovementVelocity( entity_state_t *state ) {
	asvec3_t angles;
	VectorCopy( state->linearMovementVelocity, angles.v );
	return angles;
}

static void objectEntityState_SetLinearMovementVelocity( asvec3_t *vec, entity_state_t *state ) {
	VectorCopy( vec->v, state->linearMovementVelocity );
}

static asvec3_t objectEntityState_GetLinearMovementBegin( entity_state_t *state ) {
	asvec3_t angles;
	VectorCopy( state->linearMovementBegin, angles.v );
	return angles;
}

static void objectEntityState_SetLinearMovementBegin( asvec3_t *vec, entity_state_t *state ) {
	VectorCopy( vec->v, state->linearMovementBegin );
}

static asvec3_t objectEntityState_GetLinearMovementEnd( entity_state_t *state ) {
	asvec3_t angles;
	VectorCopy( state->linearMovementEnd, angles.v );
	return angles;
}

static void objectEntityState_SetLinearMovementEnd( asvec3_t *vec, entity_state_t *state ) {
	VectorCopy( vec->v, state->linearMovementEnd );
}

static const gs_asMethod_t asEntityState_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( Vec3, get_origin, ( ) const ), asFUNCTION( objectEntityState_GetOrigin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_origin, ( const Vec3 &in ) ), asFUNCTION( objectEntityState_SetOrigin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_origin2, ( ) const ), asFUNCTION( objectEntityState_GetOrigin2 ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_origin2, ( const Vec3 &in ) ), asFUNCTION( objectEntityState_SetOrigin2 ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_origin3, ( ) const ), asFUNCTION( objectEntityState_GetOrigin3 ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_origin3, ( const Vec3 &in ) ), asFUNCTION( objectEntityState_SetOrigin3 ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_angles, ( ) const ), asFUNCTION( objectEntityState_GetAngles ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_angles, ( const Vec3 &in ) ), asFUNCTION( objectEntityState_SetAngles ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_linearMovementVelocity, ( ) const ), asFUNCTION( objectEntityState_GetLinearMovementVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_linearMovementVelocity, ( const Vec3 &in ) ), asFUNCTION( objectEntityState_SetLinearMovementVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_linearMovementBegin, ( ) const ), asFUNCTION( objectEntityState_GetLinearMovementBegin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_linearMovementBegin, ( const Vec3 &in ) ), asFUNCTION( objectEntityState_SetLinearMovementBegin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_linearMovementEnd, ( ) const ), asFUNCTION( objectEntityState_GetLinearMovementEnd ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_linearMovementEnd, ( const Vec3 &in ) ), asFUNCTION( objectEntityState_SetLinearMovementEnd ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const gs_asProperty_t asEntityState_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( int, number ), ASLIB_FOFFSET( entity_state_t, number ) },
	{ ASLIB_PROPERTY_DECL( uint, svFlags ), ASLIB_FOFFSET( entity_state_t, svflags ) },
	{ ASLIB_PROPERTY_DECL( int, type ), ASLIB_FOFFSET( entity_state_t, type ) },
	{ ASLIB_PROPERTY_DECL( int, solid ), ASLIB_FOFFSET( entity_state_t, solid ) },
	{ ASLIB_PROPERTY_DECL( int, modelindex ), ASLIB_FOFFSET( entity_state_t, modelindex ) },
	{ ASLIB_PROPERTY_DECL( int, modelindex2 ), ASLIB_FOFFSET( entity_state_t, modelindex2 ) },
	{ ASLIB_PROPERTY_DECL( int, bodyOwner ), ASLIB_FOFFSET( entity_state_t, bodyOwner ) },
	{ ASLIB_PROPERTY_DECL( int, channel ), ASLIB_FOFFSET( entity_state_t, channel ) },
	{ ASLIB_PROPERTY_DECL( int, frame ), ASLIB_FOFFSET( entity_state_t, frame ) },
	{ ASLIB_PROPERTY_DECL( int, ownerNum ), ASLIB_FOFFSET( entity_state_t, ownerNum ) },
	{ ASLIB_PROPERTY_DECL( uint, effects ), ASLIB_FOFFSET( entity_state_t, effects ) },
	{ ASLIB_PROPERTY_DECL( int, counterNum ), ASLIB_FOFFSET( entity_state_t, counterNum ) },
	{ ASLIB_PROPERTY_DECL( int, skinNum ), ASLIB_FOFFSET( entity_state_t, skinnum ) },
	{ ASLIB_PROPERTY_DECL( int, itemNum ), ASLIB_FOFFSET( entity_state_t, itemNum ) },
	{ ASLIB_PROPERTY_DECL( int, fireMode ), ASLIB_FOFFSET( entity_state_t, firemode ) },
	{ ASLIB_PROPERTY_DECL( int, damage ), ASLIB_FOFFSET( entity_state_t, damage ) },
	{ ASLIB_PROPERTY_DECL( int, targetNum ), ASLIB_FOFFSET( entity_state_t, targetNum ) },
	{ ASLIB_PROPERTY_DECL( int, colorRGBA ), ASLIB_FOFFSET( entity_state_t, colorRGBA ) },
	{ ASLIB_PROPERTY_DECL( int, range ), ASLIB_FOFFSET( entity_state_t, range ) },
	{ ASLIB_PROPERTY_DECL( float, attenuation ), ASLIB_FOFFSET( entity_state_t, attenuation ) },
	{ ASLIB_PROPERTY_DECL( int, weapon ), ASLIB_FOFFSET( entity_state_t, weapon ) },
	{ ASLIB_PROPERTY_DECL( bool, teleported ), ASLIB_FOFFSET( entity_state_t, teleported ) },
	{ ASLIB_PROPERTY_DECL( int, sound ), ASLIB_FOFFSET( entity_state_t, sound ) },
	{ ASLIB_PROPERTY_DECL( int, light ), ASLIB_FOFFSET( entity_state_t, light ) },
	{ ASLIB_PROPERTY_DECL( int, team ), ASLIB_FOFFSET( entity_state_t, team ) },
	{ ASLIB_PROPERTY_DECL( int, event1 ), ASLIB_FOFFSET( entity_state_t, events[0] ) },
	{ ASLIB_PROPERTY_DECL( int, event2 ), ASLIB_FOFFSET( entity_state_t, events[1] ) },
	{ ASLIB_PROPERTY_DECL( int, eventParm1 ), ASLIB_FOFFSET( entity_state_t, eventParms[0] ) },
	{ ASLIB_PROPERTY_DECL( int, eventParm2 ), ASLIB_FOFFSET( entity_state_t, eventParms[1] ) },
	{ ASLIB_PROPERTY_DECL( bool, linearMovement ), ASLIB_FOFFSET( entity_state_t, linearMovement ) },
	{ ASLIB_PROPERTY_DECL( uint, linearMovementDuration ), ASLIB_FOFFSET( entity_state_t, linearMovementDuration ) },
	{ ASLIB_PROPERTY_DECL( int64, linearMovementTimeStamp ), ASLIB_FOFFSET( entity_state_t, linearMovementTimeStamp ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asEntityStateClassDescriptor =
{
	"EntityState",              /* name */
	asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CK,   /* object type flags */
	sizeof( entity_state_t ),   /* size */
	asEntityState_Funcdefs,     /* funcdefs */
	asEntityState_ObjectBehaviors,/* object behaviors */
	asEntityState_Methods,      /* methods */
	asEntityState_Properties,   /* properties */
	
	NULL, NULL                  /* string factory hack */
};

//=======================================================================

static const gs_asClassDescriptor_t * const asGameClassesDescriptors[] =
{
	&asTraceClassDescriptor,
	&asItemClassDescriptor,
	&asEntityStateClassDescriptor,

	NULL
};

/*
* GS_asRegisterObjectClassNames
*/
void GS_asRegisterObjectClassNames( asIScriptEngine *asEngine, 
	const gs_asClassDescriptor_t *const *asClassesDescriptors, const char *nameSpace ) {
	int i;
	const gs_asClassDescriptor_t *cDescr;

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( nameSpace );
	} else {
		asEngine->SetDefaultNamespace( "" );
	}

	for( i = 0; ; i++ ) {
		if( !( cDescr = asClassesDescriptors[i] ) ) {
			break;
		}
		asEngine->RegisterObjectType( cDescr->name, cDescr->size, cDescr->typeFlags );
	}

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( "" );
	}
}

/*
* GS_asRegisterObjectClasses
*/
void GS_asRegisterObjectClasses( asIScriptEngine *asEngine, 
	const gs_asClassDescriptor_t *const *asClassesDescriptors, const char *nameSpace ) {
	int i, j;
	const gs_asClassDescriptor_t *cDescr;

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( nameSpace );
	} else {
		asEngine->SetDefaultNamespace( "" );
	}

	// now register object and global behaviors, then methods and properties
	for( i = 0; ; i++ ) {
		if( !( cDescr = asClassesDescriptors[i] ) ) {
			break;
		}

		// funcdefs
		if( cDescr->funcdefs ) {
			for( j = 0; ; j++ ) {
				const gs_asFuncdef_t *funcdef = &cDescr->funcdefs[j];
				if( !funcdef->declaration ) {
					break;
				}
				asEngine->RegisterFuncdef( funcdef->declaration );
			}
		}

		// object behaviors
		if( cDescr->objBehaviors ) {
			for( j = 0; ; j++ ) {
				const gs_asBehavior_t *objBehavior = &cDescr->objBehaviors[j];
				if( !objBehavior->declaration ) {
					break;
				}
				asEngine->RegisterObjectBehaviour(
					cDescr->name, objBehavior->behavior, objBehavior->declaration,
					objBehavior->funcPointer, objBehavior->callConv );
			}
		}

		// object methods
		if( cDescr->objMethods ) {
			for( j = 0; ; j++ ) {
				const gs_asMethod_t *objMethod = &cDescr->objMethods[j];
				if( !objMethod->declaration ) {
					break;
				}

				asEngine->RegisterObjectMethod( cDescr->name,
					objMethod->declaration, objMethod->funcPointer,
					objMethod->callConv );
			}
		}

		// object properties
		if( cDescr->objProperties ) {
			for( j = 0; ; j++ ) {
				const gs_asProperty_t *objProperty = &cDescr->objProperties[j];
				if( !objProperty->declaration ) {
					break;
				}

				asEngine->RegisterObjectProperty( cDescr->name,
					objProperty->declaration, objProperty->offset );
			}
		}
	}

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( "" );
	}
}

/*
* GS_asRegisterGlobalFunctions
*/
void GS_asRegisterGlobalFunctions( asIScriptEngine *asEngine, 
	const gs_asglobfuncs_t *funcs, const char *nameSpace ) {
	int error;
	int count = 0, failedcount = 0;
	const gs_asglobfuncs_t *func;

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( nameSpace );
	} else {
		asEngine->SetDefaultNamespace( "" );
	}

	for( func = funcs; func->declaration; func++ ) {
		error = asEngine->RegisterGlobalFunction( func->declaration, func->pointer, asCALL_CDECL );

		if( error < 0 ) {
			failedcount++;
			continue;
		}

		count++;
	}

	// get AS function pointers
	for( func = funcs; func->declaration; func++ ) {
		if( func->asFuncPtr ) {
			*func->asFuncPtr = asEngine->GetGlobalFunctionByDecl( func->declaration );
		}
	}

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( "" );
	}
}

/*
* GS_asRegisterGlobalProperties
*/
void GS_asRegisterGlobalProperties( asIScriptEngine *asEngine, 
	const gs_asglobproperties_t *props, const char *nameSpace ) {
	int error;
	int count = 0, failedcount = 0;
	const gs_asglobproperties_t *prop;

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( nameSpace );
	} else {
		asEngine->SetDefaultNamespace( "" );
	}

	for( prop = props; prop->declaration; prop++ ) {
		error = asEngine->RegisterGlobalProperty( prop->declaration, prop->pointer );
		if( error < 0 ) {
			failedcount++;
			continue;
		}

		count++;
	}

	if( nameSpace ) {
		asEngine->SetDefaultNamespace( "" );
	}
}

//=======================================================================

/*
* GS_asInitializeExport
*/
void GS_asInitializeExport( void ) {
	module_angelExport = NULL;

	if( gs.api.GetAngelExport ) {
		module_angelExport = gs.api.GetAngelExport();
	}
}

/*
* GS_asInitializeEngine
*/
void GS_asInitializeEngine( asIScriptEngine *asEngine ) {
	// register global variables
	GS_asRegisterEnums( asEngine, asGameEnums, NULL );

	// first register all class names so methods using custom classes work
	GS_asRegisterObjectClassNames( asEngine, asGameClassesDescriptors, NULL );

	// register classes
	GS_asRegisterObjectClasses( asEngine, asGameClassesDescriptors, NULL );
}
