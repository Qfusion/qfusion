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
#include <map>

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
	ASLIB_ENUM_VAL( EF_CARRIER ),
	ASLIB_ENUM_VAL( EF_BUSYICON ),
	ASLIB_ENUM_VAL( EF_FLAG_TRAIL ),
	ASLIB_ENUM_VAL( EF_TAKEDAMAGE ),
	ASLIB_ENUM_VAL( EF_TEAMCOLOR_TRANSITION ),
	ASLIB_ENUM_VAL( EF_EXPIRING_QUAD ),
	ASLIB_ENUM_VAL( EF_EXPIRING_SHELL ),
	ASLIB_ENUM_VAL( EF_GODMODE ),
	ASLIB_ENUM_VAL( EF_REGEN ),
	ASLIB_ENUM_VAL( EF_EXPIRING_REGEN ),
	ASLIB_ENUM_VAL( EF_GHOST ),

	ASLIB_ENUM_VAL( EF_NOPORTALENTS ),
	ASLIB_ENUM_VAL( EF_PLAYER_STUNNED ),
	ASLIB_ENUM_VAL( EF_PLAYER_HIDENAME ),

	// these ones can be only set from client side
	ASLIB_ENUM_VAL( EF_AMMOBOX ),
	ASLIB_ENUM_VAL( EF_RACEGHOST ),
	ASLIB_ENUM_VAL( EF_OUTLINE ),
	ASLIB_ENUM_VAL( EF_GHOSTITEM ),

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

static const gs_asEnumVal_t asGameStatEnumVals[] =
{
	ASLIB_ENUM_VAL( GAMESTAT_FLAGS ),
	ASLIB_ENUM_VAL( GAMESTAT_MATCHSTATE ),
	ASLIB_ENUM_VAL( GAMESTAT_MATCHSTART ),
	ASLIB_ENUM_VAL( GAMESTAT_MATCHDURATION ),
	ASLIB_ENUM_VAL( GAMESTAT_CLOCKOVERRIDE ),
	ASLIB_ENUM_VAL( GAMESTAT_MAXPLAYERSINTEAM ),
	ASLIB_ENUM_VAL( GAMESTAT_COLORCORRECTION ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asGameStatFlagsEnumVals[] =
{
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_PAUSED ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_WAITING ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_INSTAGIB ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_MATCHEXTENDED ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_FALLDAMAGE ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_HASCHALLENGERS ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_INHIBITSHOOTING ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_ISTEAMBASED ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_ISRACE ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_COUNTDOWN ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_SELFDAMAGE ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_INFINITEAMMO ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_CANFORCEMODELS ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_CANSHOWMINIMAP ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_TEAMONLYMINIMAP ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_MMCOMPATIBLE ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_ISTUTORIAL ),
	ASLIB_ENUM_VAL( GAMESTAT_FLAG_CANDROPWEAPON ),

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
	ASLIB_ENUM_VAL( ET_MONSTER_PLAYER ),
	ASLIB_ENUM_VAL( ET_MONSTER_CORPSE ),

	ASLIB_ENUM_VAL( ET_EVENT ),
	ASLIB_ENUM_VAL( ET_SOUNDEVENT ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asEntityEventEnumVals[] =
{
	ASLIB_ENUM_VAL( EV_NONE ),
	ASLIB_ENUM_VAL( EV_WEAPONACTIVATE ),
	ASLIB_ENUM_VAL( EV_FIREWEAPON ),
	ASLIB_ENUM_VAL( EV_ELECTROTRAIL ),
	ASLIB_ENUM_VAL( EV_INSTATRAIL ),
	ASLIB_ENUM_VAL( EV_FIRE_RIOTGUN ),
	ASLIB_ENUM_VAL( EV_FIRE_BULLET ),
	ASLIB_ENUM_VAL( EV_SMOOTHREFIREWEAPON ),
	ASLIB_ENUM_VAL( EV_NOAMMOCLICK ),
	ASLIB_ENUM_VAL( EV_DASH ),
	ASLIB_ENUM_VAL( EV_WALLJUMP ),
	ASLIB_ENUM_VAL( EV_WALLJUMP_FAILED ),
	ASLIB_ENUM_VAL( EV_DOUBLEJUMP ),
	ASLIB_ENUM_VAL( EV_JUMP ),
	ASLIB_ENUM_VAL( EV_JUMP_PAD ),
	ASLIB_ENUM_VAL( EV_FALL ),
	ASLIB_ENUM_VAL( EV_WEAPONDROP ),
	ASLIB_ENUM_VAL( EV_ITEM_RESPAWN ),
	ASLIB_ENUM_VAL( EV_PAIN ),
	ASLIB_ENUM_VAL( EV_DIE ),
	ASLIB_ENUM_VAL( EV_GIB ),
	ASLIB_ENUM_VAL( EV_PLAYER_RESPAWN ),
	ASLIB_ENUM_VAL( EV_PLAYER_TELEPORT_IN ),
	ASLIB_ENUM_VAL( EV_PLAYER_TELEPORT_OUT ),
	ASLIB_ENUM_VAL( EV_GESTURE ),
	ASLIB_ENUM_VAL( EV_DROP ),
	ASLIB_ENUM_VAL( EV_SPOG ),
	ASLIB_ENUM_VAL( EV_BLOOD ),
	ASLIB_ENUM_VAL( EV_BLADE_IMPACT ),
	ASLIB_ENUM_VAL( EV_GUNBLADEBLAST_IMPACT ),
	ASLIB_ENUM_VAL( EV_GRENADE_BOUNCE ),
	ASLIB_ENUM_VAL( EV_GRENADE_EXPLOSION ),
	ASLIB_ENUM_VAL( EV_ROCKET_EXPLOSION ),
	ASLIB_ENUM_VAL( EV_PLASMA_EXPLOSION ),
	ASLIB_ENUM_VAL( EV_BOLT_EXPLOSION ),
	ASLIB_ENUM_VAL( EV_INSTA_EXPLOSION ),
	ASLIB_ENUM_VAL( EV_FREE2 ),
	ASLIB_ENUM_VAL( EV_FREE3 ),
	ASLIB_ENUM_VAL( EV_FREE4 ),
	ASLIB_ENUM_VAL( EV_EXPLOSION1 ),
	ASLIB_ENUM_VAL( EV_EXPLOSION2 ),
	ASLIB_ENUM_VAL( EV_BLASTER ),
	ASLIB_ENUM_VAL( EV_SPARKS ),
	ASLIB_ENUM_VAL( EV_BULLET_SPARKS ),
	ASLIB_ENUM_VAL( EV_SEXEDSOUND ),
	ASLIB_ENUM_VAL( EV_VSAY ),
	ASLIB_ENUM_VAL( EV_LASER_SPARKS ),
	ASLIB_ENUM_VAL( EV_FIRE_SHOTGUN ),
	ASLIB_ENUM_VAL( EV_PNODE ),
	ASLIB_ENUM_VAL( EV_GREEN_LASER ),
	ASLIB_ENUM_VAL( EV_PLAT_HIT_TOP ),
	ASLIB_ENUM_VAL( EV_PLAT_HIT_BOTTOM ),
	ASLIB_ENUM_VAL( EV_PLAT_START_MOVING ),
	ASLIB_ENUM_VAL( EV_DOOR_HIT_TOP ),
	ASLIB_ENUM_VAL( EV_DOOR_HIT_BOTTOM ),
	ASLIB_ENUM_VAL( EV_DOOR_START_MOVING ),
	ASLIB_ENUM_VAL( EV_BUTTON_FIRE ),
	ASLIB_ENUM_VAL( EV_TRAIN_STOP ),
	ASLIB_ENUM_VAL( EV_TRAIN_START ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asSolidEnumVals[] =
{
	ASLIB_ENUM_VAL( SOLID_NOT ),
	ASLIB_ENUM_VAL( SOLID_TRIGGER ),
	ASLIB_ENUM_VAL( SOLID_YES ),
	ASLIB_ENUM_VAL( SOLID_BMODEL ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asPMoveStatEnumVals[] =
{
	ASLIB_ENUM_VAL( PM_STAT_FEATURES ),
	ASLIB_ENUM_VAL( PM_STAT_NOUSERCONTROL ),
	ASLIB_ENUM_VAL( PM_STAT_KNOCKBACK ),
	ASLIB_ENUM_VAL( PM_STAT_CROUCHTIME ),
	ASLIB_ENUM_VAL( PM_STAT_ZOOMTIME ),
	ASLIB_ENUM_VAL( PM_STAT_DASHTIME ),
	ASLIB_ENUM_VAL( PM_STAT_WJTIME ),
	ASLIB_ENUM_VAL( PM_STAT_NOAUTOATTACK ),
	ASLIB_ENUM_VAL( PM_STAT_STUN ),
	ASLIB_ENUM_VAL( PM_STAT_MAXSPEED ),
	ASLIB_ENUM_VAL( PM_STAT_JUMPSPEED ),
	ASLIB_ENUM_VAL( PM_STAT_DASHSPEED ),
	ASLIB_ENUM_VAL( PM_STAT_FWDTIME ),
	ASLIB_ENUM_VAL( PM_STAT_CROUCHSLIDETIME ),
	ASLIB_ENUM_VAL( PM_STAT_SIZE ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asPMoveTypeEnumVals[] =
{
	ASLIB_ENUM_VAL( PM_NORMAL ),
	ASLIB_ENUM_VAL( PM_SPECTATOR ),
	ASLIB_ENUM_VAL( PM_GIB ),
	ASLIB_ENUM_VAL( PM_FREEZE ),
	ASLIB_ENUM_VAL( PM_CHASECAM ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asPMoveFlagEnumVals[] =
{
	ASLIB_ENUM_VAL( PMF_WALLJUMPCOUNT ),
	ASLIB_ENUM_VAL( PMF_JUMP_HELD ),
	ASLIB_ENUM_VAL( PMF_ON_GROUND ),
	ASLIB_ENUM_VAL( PMF_TIME_WATERJUMP ),
	ASLIB_ENUM_VAL( PMF_TIME_LAND ),
	ASLIB_ENUM_VAL( PMF_TIME_TELEPORT ),
	ASLIB_ENUM_VAL( PMF_NO_PREDICTION ),
	ASLIB_ENUM_VAL( PMF_DASHING ),
	ASLIB_ENUM_VAL( PMF_SPECIAL_HELD ),
	ASLIB_ENUM_VAL( PMF_WALLJUMPING ),
	ASLIB_ENUM_VAL( PMF_DOUBLEJUMPED ),
	ASLIB_ENUM_VAL( PMF_JUMPPAD_TIME ),
	ASLIB_ENUM_VAL( PMF_CROUCH_SLIDING ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asPMoveFeaturesEnumVals[] =
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
	ASLIB_ENUM_VAL( SURF_NOWALLJUMP ),

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

static const gs_asEnumVal_t asSlideMoveEnumVals[] =
{
	ASLIB_ENUM_VAL( SLIDEMOVEFLAG_MOVED ),
	ASLIB_ENUM_VAL( SLIDEMOVEFLAG_BLOCKED ),
	ASLIB_ENUM_VAL( SLIDEMOVEFLAG_TRAPPED ),
	ASLIB_ENUM_VAL( SLIDEMOVEFLAG_WALL_BLOCKED ),
	ASLIB_ENUM_VAL( SLIDEMOVEFLAG_PLANE_TOUCHED ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asStatEnumVals[] = {
	ASLIB_ENUM_VAL( STAT_LAYOUTS ),
	ASLIB_ENUM_VAL( STAT_HEALTH ),
	ASLIB_ENUM_VAL( STAT_ARMOR ),
	ASLIB_ENUM_VAL( STAT_WEAPON ),
	ASLIB_ENUM_VAL( STAT_WEAPON_TIME ),
	ASLIB_ENUM_VAL( STAT_PENDING_WEAPON ),
	ASLIB_ENUM_VAL( STAT_PICKUP_ITEM ),
	ASLIB_ENUM_VAL( STAT_SCORE ),
	ASLIB_ENUM_VAL( STAT_TEAM ),
	ASLIB_ENUM_VAL( STAT_REALTEAM ),
	ASLIB_ENUM_VAL( STAT_NEXT_RESPAWN ),
	ASLIB_ENUM_VAL( STAT_POINTED_PLAYER ),
	ASLIB_ENUM_VAL( STAT_POINTED_TEAMPLAYER ),
	ASLIB_ENUM_VAL( STAT_TEAM_ALPHA_SCORE ),
	ASLIB_ENUM_VAL( STAT_TEAM_BETA_SCORE ),
	ASLIB_ENUM_VAL( STAT_LAST_KILLER ),
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

	ASLIB_ENUM_VAL_NULL,
};

//=======================================================================

static const gs_asEnum_t asGameEnums[] =
{
	{ "configstrings_e", asConfigstringEnumVals },
	{ "state_effects_e", asEffectEnumVals },
	{ "matchstates_e", asMatchStateEnumVals },
	{ "gamestats_e", asGameStatEnumVals },
	{ "gamestatflags_e", asGameStatFlagsEnumVals },
	{ "teams_e", asTeamEnumVals },
	{ "entitytype_e", asEntityTypeEnumVals },
	{ "entityevent_e", asEntityEventEnumVals },
	{ "solid_e", asSolidEnumVals },
	{ "pmovestats_e", asPMoveStatEnumVals },
	{ "pmovefeats_e", asPMoveFeaturesEnumVals },
	{ "pmovetype_e", asPMoveTypeEnumVals },
	{ "pmoveflag_e", asPMoveFlagEnumVals },
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

	{ "slidemoveflags_e", asSlideMoveEnumVals },
	{ "stat_e", asStatEnumVals },

	ASLIB_ENUM_VAL_NULL,
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
	self->trace.fraction = 1.0f;
	self->trace.ent = -1;
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
static std::map<entity_state_t *, int> esRefCounters;

static const gs_asFuncdef_t asEntityState_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static entity_state_t *objectEntityState_Factory( void ) {
	entity_state_t *state = (entity_state_t *)gs.api.Malloc( sizeof( entity_state_t ) );
	memset( state, 0, sizeof( *state ) );
	esRefCounters[state] = 1;
	return state;
}

static void objectEntityState_AddRef( entity_state_t *state ) {
	auto it = esRefCounters.find( state );
	if( it == esRefCounters.end() ) {
		return;
	}
	it->second++;
}

static void objectEntityState_Release( entity_state_t *state ) {
	auto it = esRefCounters.find( state );
	if( it == esRefCounters.end() ) {
		return;
	}
	if( --(it->second) == 0 ) {
		gs.api.Free( state );
		esRefCounters.erase( it );
	}
}

static const gs_asBehavior_t asEntityState_ObjectBehaviors[] =
{
	{ asBEHAVE_FACTORY, ASLIB_FUNCTION_DECL(EntityState @, f, ()), asFUNCTION( objectEntityState_Factory ), asCALL_CDECL },
	{ asBEHAVE_ADDREF, ASLIB_FUNCTION_DECL(void, f, ()), asFUNCTION( objectEntityState_AddRef ), asCALL_CDECL_OBJLAST },
	{ asBEHAVE_RELEASE, ASLIB_FUNCTION_DECL(void, f, ()), asFUNCTION( objectEntityState_Release ), asCALL_CDECL_OBJLAST },

	ASLIB_BEHAVIOR_NULL
};

static int objectEntityState_GetNumber( entity_state_t *state ) {
	return state->number;
}

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

static int objectEntityState_GetEvent( unsigned int idx, entity_state_t *state ) {
	if( idx >= sizeof(state->events)/sizeof(state->events[0]) ) {
		return -1;
	}
	return state->events[idx];
}

static void objectEntityState_SetEvent( unsigned int idx, int value, entity_state_t *state ) {
	if( idx >= sizeof(state->events)/sizeof(state->events[0]) ) {
		return;
	}
	state->events[idx] = value;
}

static int objectEntityState_GetEventParm( unsigned int idx, entity_state_t *state ) {
	if( idx >= sizeof(state->eventParms)/sizeof(state->eventParms[0]) ) {
		return -1;
	}
	return state->eventParms[idx];
}

static void objectEntityState_SetEventParm( unsigned int idx, int value, entity_state_t *state ) {
	if( idx >= sizeof(state->eventParms)/sizeof(state->eventParms[0]) ) {
		return;
	}
	state->eventParms[idx] = value;
}

static void objectEntityState_Assign( entity_state_t *other, entity_state_t *state ) {
	int number = other->number;
	
	auto it = esRefCounters.find( state );
	if( it == esRefCounters.end() ) {
		// not a script-allocated state, keep the number
		number = state->number;
	}
	
	*state = *other;
	state->number = number;
}

static const gs_asMethod_t asEntityState_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( Vec3, &opAssign, ( const EntityState &in ) ), asFUNCTION( objectEntityState_Assign ), asCALL_CDECL_OBJLAST },

	{ ASLIB_FUNCTION_DECL( int, get_number, ( ) const ), asFUNCTION( objectEntityState_GetNumber ), asCALL_CDECL_OBJLAST },
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
	{ ASLIB_FUNCTION_DECL( int, get_events, ( uint index ) const ), asFUNCTION( objectEntityState_GetEvent ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_events, ( uint index, int value ) ), asFUNCTION( objectEntityState_SetEvent ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_eventParms, ( uint index ) const ), asFUNCTION( objectEntityState_GetEventParm ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_eventParms, ( uint index, int value ) ), asFUNCTION( objectEntityState_SetEventParm ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const gs_asProperty_t asEntityState_Properties[] =
{
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
	{ ASLIB_PROPERTY_DECL( bool, linearMovement ), ASLIB_FOFFSET( entity_state_t, linearMovement ) },
	{ ASLIB_PROPERTY_DECL( uint, linearMovementDuration ), ASLIB_FOFFSET( entity_state_t, linearMovementDuration ) },
	{ ASLIB_PROPERTY_DECL( int64, linearMovementTimeStamp ), ASLIB_FOFFSET( entity_state_t, linearMovementTimeStamp ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asEntityStateClassDescriptor =
{
	"EntityState",              /* name */
	asOBJ_REF,                  /* object type flags */
	sizeof( entity_state_t ),   /* size */
	asEntityState_Funcdefs,     /* funcdefs */
	asEntityState_ObjectBehaviors,/* object behaviors */
	asEntityState_Methods,      /* methods */
	asEntityState_Properties,   /* properties */
	
	NULL, NULL                  /* string factory hack */
};

//=======================================================================

// CLASS: UserCmd

static const gs_asFuncdef_t asUserCmd_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static void objectUserCmd_DefaultConstructor( usercmd_t *cmd ) {
	memset( cmd, 0, sizeof( usercmd_t ) );
}

static void objectUserCmd_CopyConstructor( usercmd_t *other, usercmd_t *cmd ) {
	*cmd = *other;
}

static const gs_asBehavior_t asUserCmd_ObjectBehaviors[] =
{
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( ) ), asFUNCTION( objectUserCmd_DefaultConstructor ), asCALL_CDECL_OBJLAST },
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( const UserCmd &in ) ), asFUNCTION( objectUserCmd_CopyConstructor ), asCALL_CDECL_OBJLAST },

	ASLIB_BEHAVIOR_NULL
};

static const gs_asMethod_t asUserCmd_Methods[] =
{
	ASLIB_METHOD_NULL
};

static const gs_asProperty_t asUserCmd_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( int8, msec ), ASLIB_FOFFSET( usercmd_t, msec ) },
	{ ASLIB_PROPERTY_DECL( uint32, buttons ), ASLIB_FOFFSET( usercmd_t, buttons ) },
	{ ASLIB_PROPERTY_DECL( int64, serverTimeStamp ), ASLIB_FOFFSET( usercmd_t, serverTimeStamp ) },
	{ ASLIB_PROPERTY_DECL( int8, forwardmove ), ASLIB_FOFFSET( usercmd_t, forwardmove ) },
	{ ASLIB_PROPERTY_DECL( int8, sidemove ), ASLIB_FOFFSET( usercmd_t, sidemove ) },
	{ ASLIB_PROPERTY_DECL( int8, upmove ), ASLIB_FOFFSET( usercmd_t, upmove ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asUserCmdClassDescriptor =
{
	"UserCmd",               /* name */
	asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CK, /* object type flags */
	sizeof( usercmd_t ),     /* size */
	asUserCmd_Funcdefs,      /* funcdefs */
	asUserCmd_ObjectBehaviors,/* object behaviors */
	asUserCmd_Methods,       /* methods */
	asUserCmd_Properties,    /* properties */

	NULL, NULL               /* string factory hack */
};

//=======================================================================

// CLASS: PMoveState
static std::map<pmove_state_t *, int> pmsRefCounters;

static const gs_asFuncdef_t asPMoveState_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static pmove_state_t *objectPMoveState_Factory( void ) {
	pmove_state_t *state = (pmove_state_t *)gs.api.Malloc( sizeof( pmove_state_t ) );
	memset( state, 0, sizeof( *state ) );
	pmsRefCounters[state] = 1;
	return state;
}

static void objectPMoveState_AddRef( pmove_state_t *state ) {
	auto it = pmsRefCounters.find( state );
	if( it == pmsRefCounters.end() ) {
		return;
	}
	it->second++;
}

static void objectPMoveState_Release( pmove_state_t *state ) {
	auto it = pmsRefCounters.find( state );
	if( it == pmsRefCounters.end() ) {
		return;
	}
	if( --(it->second) == 0 ) {
		gs.api.Free( state );
		pmsRefCounters.erase( it );
	}
}

static const gs_asBehavior_t asPMoveState_ObjectBehaviors[] =
{
	{ asBEHAVE_FACTORY, ASLIB_FUNCTION_DECL(PMoveState @, f, ()), asFUNCTION( objectPMoveState_Factory ), asCALL_CDECL },
	{ asBEHAVE_ADDREF, ASLIB_FUNCTION_DECL(void, f, ()), asFUNCTION( objectPMoveState_AddRef ), asCALL_CDECL_OBJLAST },
	{ asBEHAVE_RELEASE, ASLIB_FUNCTION_DECL(void, f, ()), asFUNCTION( objectPMoveState_Release ), asCALL_CDECL_OBJLAST },

	ASLIB_BEHAVIOR_NULL
};

static void objectPMoveState_Assign( pmove_state_t *other, pmove_state_t *state ) {
	*state = *other;
}

static asvec3_t objectPMoveState_GetOrigin( pmove_state_t *state ) {
	asvec3_t origin;
	VectorCopy( state->origin, origin.v );
	return origin;
}

static void objectPMoveState_SetOrigin( asvec3_t *vec, pmove_state_t *state ) {
	VectorCopy( vec->v, state->origin );
}

static asvec3_t objectPMoveState_GetVelocity( pmove_state_t *state ) {
	asvec3_t velocity;
	VectorCopy( state->velocity, velocity.v );
	return velocity;
}

static void objectPMoveState_SetVelocity( asvec3_t *vec, pmove_state_t *state ) {
	VectorCopy( vec->v, state->velocity );
}

static int16_t objectPMoveState_GetStat( unsigned int idx, pmove_state_t *state ) {
	if( idx >= PM_STAT_SIZE ) {
		return 0;
	}
	return state->stats[idx];
}

static void objectPMoveState_SetStat( unsigned int idx, int16_t value, pmove_state_t *state ) {
	if( idx >= PM_STAT_SIZE ) {
		return;
	}
	state->stats[idx] = value;
}

static int objectPMoveState_GetPmFlags( pmove_state_t *state ) {
	return state->pm_flags;
}

static void objectPMoveState_SetPmFlags( int value, pmove_state_t *state ) {
	if( gs.module == GS_MODULE_GAME ) {
		state->pm_flags = value;
		return;
	}

	// the no_prediction bit can't be changed client-side
	int mask = PMF_NO_PREDICTION;
	int preserve = state->pm_flags & mask;
	state->pm_flags = (value & ~mask) | preserve;
}

static const gs_asMethod_t asPMoveState_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( Vec3, &opAssign, ( const PMoveState &in ) ), asFUNCTION( objectPMoveState_Assign ), asCALL_CDECL_OBJLAST },

	{ ASLIB_FUNCTION_DECL( Vec3, get_origin, ( ) const ), asFUNCTION( objectPMoveState_GetOrigin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_origin, ( const Vec3 &in ) ), asFUNCTION( objectPMoveState_SetOrigin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_velocity, ( ) const ), asFUNCTION( objectPMoveState_GetVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_velocity, ( const Vec3 &in ) ), asFUNCTION( objectPMoveState_SetVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int16, get_stats, ( uint index ) const ), asFUNCTION( objectPMoveState_GetStat ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_stats, ( uint index, int16 value ) ), asFUNCTION( objectPMoveState_SetStat ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_pm_flags, () const ), asFUNCTION( objectPMoveState_GetPmFlags ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_pm_flags, ( int value ) ), asFUNCTION( objectPMoveState_SetPmFlags ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const gs_asProperty_t asPMoveState_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( int, pm_type ), ASLIB_FOFFSET( pmove_state_t, pm_type ) },
	{ ASLIB_PROPERTY_DECL( int, pm_time ), ASLIB_FOFFSET( pmove_state_t, pm_time ) },
	{ ASLIB_PROPERTY_DECL( int, gravity ), ASLIB_FOFFSET( pmove_state_t, gravity ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asPMoveStateClassDescriptor =
{
	"PMoveState",               /* name */
	asOBJ_REF,                  /* object type flags */
	sizeof( pmove_state_t ),    /* size */
	asPMoveState_Funcdefs,      /* funcdefs */
	asPMoveState_ObjectBehaviors,/* object behaviors */
	asPMoveState_Methods,       /* methods */
	asPMoveState_Properties,    /* properties */

	NULL, NULL                  /* string factory hack */
};

//=======================================================================

// CLASS: PlayerState

static const gs_asFuncdef_t asPlayerState_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t asPlayerState_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static asvec3_t objectPlayerState_GetViewAngles( player_state_t *ps ) {
	asvec3_t angles;
	VectorCopy( ps->viewangles, angles.v );
	return angles;
}

static void objectPlayerState_SetViewAngles( asvec3_t *vec, player_state_t *ps ) {
	VectorCopy( vec->v, ps->viewangles );
}

static int objectPlayerState_GetEvent( unsigned int idx, player_state_t *ps ) {
	if( idx >= sizeof(ps->event)/sizeof(ps->event[0]) ) {
		return -1;
	}
	return ps->event[idx];
}

static void objectPlayerState_SetEvent( unsigned int idx, int value, player_state_t *ps ) {
	if( idx >= sizeof(ps->event)/sizeof(ps->event[0]) ) {
		return;
	}
	ps->event[idx] = value;
}

static int objectPlayerState_GetEventParm( unsigned int idx, player_state_t *ps ) {
	if( idx >= sizeof(ps->eventParm)/sizeof(ps->eventParm[0]) ) {
		return -1;
	}
	return ps->eventParm[idx];
}

static void objectPlayerState_SetEventParm( unsigned int idx, int value, player_state_t *ps ) {
	if( idx >= sizeof(ps->eventParm)/sizeof(ps->eventParm[0]) ) {
		return;
	}
	ps->eventParm[idx] = value;
}

static pmove_state_t *objectPlayerState_GetPMove( player_state_t *ps ) {
	return &ps->pmove;
}

static int objectPlayerState_GetInventory( unsigned int idx, player_state_t *ps ) {
	if( idx >= MAX_ITEMS ) {
		return 0;
	}
	return ps->inventory[idx];
}

static void objectPlayerState_SetInventory( unsigned int idx, int value, player_state_t *ps ) {
	if( idx >= MAX_ITEMS ) {
		return;
	}
	ps->inventory[idx] = value;
}

static int16_t objectPlayerState_GetStat( unsigned int idx, player_state_t *ps ) {
	if( idx >= PS_MAX_STATS ) {
		return 0;
	}
	return ps->stats[idx];
}

static void objectPlayerState_SetStat( unsigned int idx, int16_t value, player_state_t *ps ) {
	if( idx >= PS_MAX_STATS ) {
		return;
	}
	ps->stats[idx] = value;
}

static const gs_asMethod_t asPlayerState_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( Vec3, get_viewAngles, ( ) const ), asFUNCTION( objectPlayerState_GetViewAngles ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_viewAngles, ( const Vec3 &in ) ), asFUNCTION( objectPlayerState_SetViewAngles ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_events, ( uint index ) const ), asFUNCTION( objectPlayerState_GetEvent ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_events, ( uint index, int value ) ), asFUNCTION( objectPlayerState_SetEvent ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_eventParms, ( uint index ) const ), asFUNCTION( objectPlayerState_GetEventParm ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_eventParms, ( uint index, int value ) ), asFUNCTION( objectPlayerState_SetEventParm ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( PMoveState &, get_pmove, () const ), asFUNCTION( objectPlayerState_GetPMove ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_inventory, ( uint index ) const ), asFUNCTION( objectPlayerState_GetInventory ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_inventory, ( uint index, int value ) ), asFUNCTION( objectPlayerState_SetInventory ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int16, get_stats, ( uint index ) const ), asFUNCTION( objectPlayerState_GetStat ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_stats, ( uint index, int16 value ) ), asFUNCTION( objectPlayerState_SetStat ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const gs_asProperty_t asPlayerState_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( uint, POVnum ), ASLIB_FOFFSET( player_state_t, POVnum ) },
	{ ASLIB_PROPERTY_DECL( uint, playerNum ), ASLIB_FOFFSET( player_state_t, playerNum ) },
	{ ASLIB_PROPERTY_DECL( float, viewHeight ), ASLIB_FOFFSET( player_state_t, viewheight ) },
	{ ASLIB_PROPERTY_DECL( float, fov ), ASLIB_FOFFSET( player_state_t, fov ) },
	{ ASLIB_PROPERTY_DECL( uint, plrkeys ), ASLIB_FOFFSET( player_state_t, plrkeys ) },
	{ ASLIB_PROPERTY_DECL( uint8, weaponState ), ASLIB_FOFFSET( player_state_t, weaponState ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asPlayerStateClassDescriptor =
{
	"PlayerState",              /* name */
	asOBJ_REF | asOBJ_NOCOUNT,  /* object type flags */
	sizeof( player_state_t ),   /* size */
	asPlayerState_Funcdefs,     /* funcdefs */
	asPlayerState_ObjectBehaviors,/* object behaviors */
	asPlayerState_Methods,      /* methods */
	asPlayerState_Properties,   /* properties */

	NULL, NULL                  /* string factory hack */
};

//=======================================================================

// CLASS: PMove

static const gs_asFuncdef_t asPMove_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t asPMove_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};


static void objectPMove_GetSize( asvec3_t *mins, asvec3_t *maxs, pmove_t *pmove ) {
	VectorCopy( pmove->maxs, maxs->v );
	VectorCopy( pmove->mins, mins->v );
}

static void objectPMove_SetSize( asvec3_t *mins, asvec3_t *maxs, pmove_t *pmove ) {
	VectorCopy( mins->v, pmove->mins );
	VectorCopy( maxs->v, pmove->maxs );
}

static asvec3_t objectPMove_GetGroundPlaneNormal( pmove_t *pmove ) {
	asvec3_t asvec;
	VectorCopy( pmove->groundplane.normal, asvec.v );
	return asvec;
}

static void objectPMove_SetGroundPlaneNormal( asvec3_t *norm, pmove_t *pmove ) {
	VectorCopy( norm->v, pmove->groundplane.normal );
}

static int objectPMove_GetTouchEnt( unsigned int idx, pmove_t *pmove ) {
	if( idx >= MAXTOUCH ) {
		return -1;
	}
	return pmove->touchents[idx];
}

static void objectPMove_SetTouchEnt( unsigned int idx, int entNum, pmove_t *pmove ) {
	if( idx >= MAXTOUCH ) {
		return;
	}
	pmove->touchents[idx] = entNum;
}

static void objectPMove_AddTouchEnt( int entNum, pmove_t *pm ) {
	int i;

	if( pm->numtouch >= MAXTOUCH || entNum < 0 ) {
		return;
	}

	// see if it is already added
	for( i = 0; i < pm->numtouch; i++ ) {
		if( pm->touchents[i] == entNum ) {
			return;
		}
	}

	// add it
	pm->touchents[pm->numtouch] = entNum;
	pm->numtouch++;
}

static void objectPMove_TouchTriggers( player_state_t *ps, asvec3_t *prevOrigin, pmove_t *pmove ) {
	gs.api.PMoveTouchTriggers( pmove, ps, prevOrigin->v );
}

static asvec3_t objectPMove_GetOrigin( pmove_t *pmove) {
	asvec3_t origin;
	VectorCopy( pmove->origin, origin.v );
	return origin;
}

static void objectPMove_SetOrigin( asvec3_t *vec, pmove_t *pmove ) {
	VectorCopy( vec->v, pmove->origin );
}

static asvec3_t objectPMove_GetVelocity( pmove_t *pmove ) {
	asvec3_t origin;
	VectorCopy( pmove->velocity, origin.v );
	return origin;
}

static void objectPMove_SetVelocity( asvec3_t *vec, pmove_t *pmove ) {
	VectorCopy( vec->v, pmove->velocity );
}

static const gs_asMethod_t asPMove_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( void, getSize, ( Vec3 & out, Vec3 & out ) ), asFUNCTION( objectPMove_GetSize ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, setSize, ( const Vec3 &in, const Vec3 &in ) ), asFUNCTION( objectPMove_SetSize ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_groundPlaneNormal, () const ), asFUNCTION( objectPMove_GetGroundPlaneNormal ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_groundPlaneNormal, ( const Vec3 &in ) ), asFUNCTION( objectPMove_SetGroundPlaneNormal ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_touchEnts, ( uint index ) const ), asFUNCTION( objectPMove_GetTouchEnt ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_touchEnts, ( uint index, int entNum ) ), asFUNCTION( objectPMove_SetTouchEnt ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, touchTriggers, ( PlayerState @, const Vec3 &in prevOrigin ) ), asFUNCTION( objectPMove_TouchTriggers ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, addTouchEnt, ( int entNum ) ), asFUNCTION( objectPMove_AddTouchEnt ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, slideMove, () ), asFUNCTION( PM_SlideMove ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_origin, () const ), asFUNCTION( objectPMove_GetOrigin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_origin, ( const Vec3 &in ) ), asFUNCTION( objectPMove_SetOrigin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_velocity, () const ), asFUNCTION( objectPMove_GetVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_velocity, ( const Vec3 &in ) ), asFUNCTION( objectPMove_SetVelocity ), asCALL_CDECL_OBJLAST },
	ASLIB_METHOD_NULL,
};

static const gs_asProperty_t asPMove_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( bool, skipCollision ), ASLIB_FOFFSET( pmove_t, skipCollision ) },
	{ ASLIB_PROPERTY_DECL( int, numTouchEnts ), ASLIB_FOFFSET( pmove_t, numtouch ) },
	{ ASLIB_PROPERTY_DECL( float, step ), ASLIB_FOFFSET( pmove_t, step ) },
	{ ASLIB_PROPERTY_DECL( int, groundEntity ), ASLIB_FOFFSET( pmove_t, groundentity ) },
	{ ASLIB_PROPERTY_DECL( int, groundSurfFlags ), ASLIB_FOFFSET( pmove_t, groundsurfFlags ) },
	{ ASLIB_PROPERTY_DECL( int, groundContents ), ASLIB_FOFFSET( pmove_t, groundcontents ) },
	{ ASLIB_PROPERTY_DECL( int, waterType ), ASLIB_FOFFSET( pmove_t, watertype ) },
	{ ASLIB_PROPERTY_DECL( int, waterLevel ), ASLIB_FOFFSET( pmove_t, waterlevel ) },
	{ ASLIB_PROPERTY_DECL( int, contentMask ), ASLIB_FOFFSET( pmove_t, contentmask ) },
	{ ASLIB_PROPERTY_DECL( bool, ladder ), ASLIB_FOFFSET( pmove_t, ladder ) },
	{ ASLIB_PROPERTY_DECL( float, groundPlaneDist ), ASLIB_FOFFSET( pmove_t, groundplane.dist ) },
	{ ASLIB_PROPERTY_DECL( int16, groundPlaneType ), ASLIB_FOFFSET( pmove_t, groundplane.type ) },
	{ ASLIB_PROPERTY_DECL( int16, groundPlaneSignBits ), ASLIB_FOFFSET( pmove_t, groundplane.signbits ) },
	{ ASLIB_PROPERTY_DECL( float, remainingTime ), ASLIB_FOFFSET( pmove_t, remainingTime ) },
	{ ASLIB_PROPERTY_DECL( float, slideBounce ), ASLIB_FOFFSET( pmove_t, slideBounce ) },
	{ ASLIB_PROPERTY_DECL( int, passEnt ), ASLIB_FOFFSET( pmove_t, passEnt ) },
	ASLIB_PROPERTY_NULL,
};

static const gs_asClassDescriptor_t asPMoveClassDescriptor =
{
	"PMove",				   /* name */
	asOBJ_REF | asOBJ_NOCOUNT, /* object type flags */
	sizeof( pmove_t ),		   /* size */
	asPMove_Funcdefs,		   /* funcdefs */
	asPMove_ObjectBehaviors,   /* object behaviors */
	asPMove_Methods,		   /* methods */
	asPMove_Properties,		   /* properties */
	NULL, NULL,				   /* string factory hack */
};

//=======================================================================

// CLASS: GameState

static const gs_asFuncdef_t asGameState_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t asGameState_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static int64_t objectGameState_GetStat( unsigned int idx, game_state_t *state ) {
	if( idx >= MAX_GAME_STATS ) {
		return -1;
	}
	return state->stats[idx];
}

static void objectGameState_SetStat( unsigned int idx, int64_t value, game_state_t *state ) {
	if( idx >= MAX_GAME_STATS ) {
		return;
	}
	state->stats[idx] = value;
}

static const gs_asMethod_t asGameState_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( int64, get_stats, ( uint index ) const ), asFUNCTION( objectGameState_GetStat ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_stats, ( uint index, int64 value ) ), asFUNCTION( objectGameState_SetStat ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const gs_asProperty_t asGameState_Properties[] =
{
	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asGameStateClassDescriptor =
{
	"GameState",                /* name */
	asOBJ_REF | asOBJ_NOCOUNT,  /* object type flags */
	sizeof( game_state_t ),     /* size */
	asGameState_Funcdefs,       /* funcdefs */
	asGameState_ObjectBehaviors,/* object behaviors */
	asGameState_Methods,        /* methods */
	asGameState_Properties,     /* properties */

	NULL, NULL                  /* string factory hack */
};


//=======================================================================

// CLASS: FireDef

static const gs_asFuncdef_t asFiredef_Funcdefs[] = {
	ASLIB_FUNCDEF_NULL,
};

static const gs_asBehavior_t asFiredef_ObjectBehaviors[] = {
	ASLIB_BEHAVIOR_NULL,
};

static const gs_asMethod_t asFiredef_Methods[] = {
	ASLIB_METHOD_NULL,
};

static const gs_asProperty_t asFiredef_Properties[] = {
	// ammo def
	{ ASLIB_PROPERTY_DECL( int, fireMode ), ASLIB_FOFFSET( firedef_t, fire_mode ) },
	{ ASLIB_PROPERTY_DECL( int, ammoID ), ASLIB_FOFFSET( firedef_t, ammo_id ) },
	{ ASLIB_PROPERTY_DECL( int, usagCount ), ASLIB_FOFFSET( firedef_t, usage_count ) },
	{ ASLIB_PROPERTY_DECL( int, projectileCount ), ASLIB_FOFFSET( firedef_t, projectile_count ) },

	// timings
	{ ASLIB_PROPERTY_DECL( uint, weaponupTime ), ASLIB_FOFFSET( firedef_t, weaponup_time ) },
	{ ASLIB_PROPERTY_DECL( uint, weapondownTime ), ASLIB_FOFFSET( firedef_t, weapondown_time ) },
	{ ASLIB_PROPERTY_DECL( uint, reloadTime ), ASLIB_FOFFSET( firedef_t, reload_time ) },
	{ ASLIB_PROPERTY_DECL( uint, cooldownTime ), ASLIB_FOFFSET( firedef_t, cooldown_time ) },
	{ ASLIB_PROPERTY_DECL( uint, timeout ), ASLIB_FOFFSET( firedef_t, timeout ) },
	{ ASLIB_PROPERTY_DECL( bool, smoothRefire ), ASLIB_FOFFSET( firedef_t, smooth_refire ) },

	// damages
	{ ASLIB_PROPERTY_DECL( float, damage ), ASLIB_FOFFSET( firedef_t, damage ) },
	{ ASLIB_PROPERTY_DECL( float, selfDamage ), ASLIB_FOFFSET( firedef_t, selfdamage ) },
	{ ASLIB_PROPERTY_DECL( int, knockback ), ASLIB_FOFFSET( firedef_t, knockback ) },
	{ ASLIB_PROPERTY_DECL( int, stun ), ASLIB_FOFFSET( firedef_t, stun ) },
	{ ASLIB_PROPERTY_DECL( int, splashRadius ), ASLIB_FOFFSET( firedef_t, splash_radius ) },
	{ ASLIB_PROPERTY_DECL( int, minDamage ), ASLIB_FOFFSET( firedef_t, mindamage ) },
	{ ASLIB_PROPERTY_DECL( int, minKnockback ), ASLIB_FOFFSET( firedef_t, minknockback ) },

	// projectile def
	{ ASLIB_PROPERTY_DECL( int, speed ), ASLIB_FOFFSET( firedef_t, speed ) },
	{ ASLIB_PROPERTY_DECL( int, spread ), ASLIB_FOFFSET( firedef_t, spread ) },
	{ ASLIB_PROPERTY_DECL( int, vspread ), ASLIB_FOFFSET( firedef_t, v_spread ) },

	// ammo amounts
	{ ASLIB_PROPERTY_DECL( int, weaponPickup ), ASLIB_FOFFSET( firedef_t, weapon_pickup ) },
	{ ASLIB_PROPERTY_DECL( int, ammoPickup ), ASLIB_FOFFSET( firedef_t, ammo_pickup ) },
	{ ASLIB_PROPERTY_DECL( int, ammoMax ), ASLIB_FOFFSET( firedef_t, ammo_max ) },
	{ ASLIB_PROPERTY_DECL( int, ammoLow ), ASLIB_FOFFSET( firedef_t, ammo_low ) },

	ASLIB_PROPERTY_NULL,
};

static const gs_asClassDescriptor_t asFiredefClassDescriptor = {
	"Firedef",				     /* name */
	asOBJ_REF | asOBJ_NOCOUNT,	 /* object type flags */
	sizeof( firedef_t ),		 /* size */
	asFiredef_Funcdefs,			 /* funcdefs */
	asFiredef_ObjectBehaviors,   /* object behaviors */
	asFiredef_Methods,		     /* methods */
	asFiredef_Properties,	     /* properties */

	NULL, NULL /* string factory hack */
};

//=======================================================================

static const gs_asClassDescriptor_t asCModelHandleClassDescriptor = {
	"CModelHandle",								   /* name */
	asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE, /* object type flags */
	sizeof( void * ),							   /* size */
	NULL,										   /* funcdefs */
	NULL,										   /* object behaviors */
	NULL,										   /* methods */
	NULL,										   /* properties */
	NULL, NULL									   /* string factory hack */
};

//=======================================================================

static const gs_asClassDescriptor_t * const asGameClassesDescriptors[] =
{
	&asTraceClassDescriptor,
	&asItemClassDescriptor,
	&asEntityStateClassDescriptor,
	&asUserCmdClassDescriptor,
	&asPMoveStateClassDescriptor,
	&asPlayerStateClassDescriptor,
	&asPMoveClassDescriptor,
	&asGameStateClassDescriptor,
	&asFiredefClassDescriptor,
	&asCModelHandleClassDescriptor,

	NULL
};

//=======================================================================

static void GS_asPrint( asstring_t *str ) {
	gs.api.Printf( "%s", str->buffer );
}

static void GS_asError( asstring_t *str ) {
	gs.api.Error( "%s", str->buffer );
}

static int GS_asPointContents( asvec3_t *vec ) {
	return gs.api.PointContents( vec->v, 0 );
}

static int GS_asPointContents4D( asvec3_t *vec, int timeDelta ) {
	return gs.api.PointContents( vec->v, timeDelta );
}

static void GS_asPredictedEvent( int entityNumber, int event, int param ) {
	gs.api.PredictedEvent( entityNumber, event, param );
}

static void GS_asRoundUpToHullSize( asvec3_t *inmins, asvec3_t *inmaxs, asvec3_t *outmins, asvec3_t *outmaxs ) {
	VectorCopy( inmins->v, outmins->v );
	VectorCopy( inmaxs->v, outmaxs->v );
	gs.api.RoundUpToHullSize( outmins->v, outmaxs->v );
}

static asvec3_t GS_asClipVelocity( asvec3_t *in, asvec3_t *normal, float overbounce ) {
	asvec3_t out;
	GS_ClipVelocity( in->v, normal->v, out.v, overbounce );
	return out;
}

static void GS_asGetPlayerStandSize( asvec3_t *mins, asvec3_t *maxs ) {
	VectorCopy( playerbox_stand_mins, mins->v );
	VectorCopy( playerbox_stand_maxs, maxs->v );
}

static void GS_asGetPlayerCrouchSize( asvec3_t *mins, asvec3_t *maxs ) {
	VectorCopy( playerbox_crouch_mins, mins->v );
	VectorCopy( playerbox_crouch_maxs, maxs->v );
}

static void GS_asGetPlayerGibSize( asvec3_t *mins, asvec3_t *maxs ) {
	VectorCopy( playerbox_gib_mins, mins->v );
	VectorCopy( playerbox_gib_maxs, maxs->v );
}

static float GS_asGetPlayerStandViewHeight( void ) {
	return float( playerbox_stand_viewheight );
}

static float GS_asGetPlayerCrouchViewHeight( void ) {
	return float( playerbox_crouch_viewheight );
}

static float GS_asGetPlayerGibViewHeight( void ) {
	return float( playerbox_gib_viewheight );
}

static entity_state_t *GS_asGetEntityState( int number, int deltaTime ) {
	return gs.api.GetEntityState( number, deltaTime );
}

static int GS_asDirToByte( asvec3_t *vec ) {
	return DirToByte( vec->v );
}

static bool GS_asIsEventEntity( entity_state_t *state )
{
	return ISEVENTENTITY( state );
}

static bool GS_asIsBrushModel( int x )
{
	return ( x > 0 ) && ( x < gs.api.NumInlineModels() );
}

static struct cmodel_s *GS_asInlineModel( int x )
{
	return gs.api.InlineModel( x );
}

static void GS_asInlineModelBounds( struct cmodel_s *model, asvec3_t *mins, asvec3_t *maxs )
{
	return gs.api.InlineModelBounds( model, mins->v, maxs->v );
}

static const gs_asglobfuncs_t asGameGlobalFunctions[] = {
	{ "void Print( const String &in )", asFUNCTION( GS_asPrint ), NULL },
	{ "void Error( const String &in )", asFUNCTION( GS_asError ), NULL },

	{ "int PointContents( const Vec3 &in )", asFUNCTION( GS_asPointContents ), NULL },
	{ "int PointContents4D( const Vec3 &in, int timeDelta )", asFUNCTION( GS_asPointContents4D ), NULL },
	{ "void PredictedEvent( int entityNumber, int event, int param )", asFUNCTION( GS_asPredictedEvent ), NULL },
	{ "void RoundUpToHullSize( const Vec3 &in inmins, const Vec3 &in inmaxs, Vec3 &out mins, Vec3 &out maxs )", asFUNCTION( GS_asRoundUpToHullSize ), NULL },
	{ "Vec3 ClipVelocity( const Vec3 &in, const Vec3 &in, float overbounce )", asFUNCTION( GS_asClipVelocity ), NULL },

	{ "void GetPlayerStandSize( Vec3 & out, Vec3 & out )", asFUNCTION( GS_asGetPlayerStandSize ), NULL },
	{ "void GetPlayerCrouchSize( Vec3 & out, Vec3 & out )", asFUNCTION( GS_asGetPlayerCrouchSize ), NULL },
	{ "void GetPlayerGibSize( Vec3 & out, Vec3 & out )", asFUNCTION( GS_asGetPlayerGibSize ), NULL },

	{ "float GetPlayerStandViewHeight()", asFUNCTION( GS_asGetPlayerStandViewHeight ), NULL },
	{ "float GetPlayerCrouchHeight()", asFUNCTION( GS_asGetPlayerCrouchViewHeight ), NULL },
	{ "float GetPlayerGibHeight()", asFUNCTION( GS_asGetPlayerGibViewHeight ), NULL },

	{ "EntityState @GetEntityState( int number, int deltaTime = 0 )", asFUNCTION( GS_asGetEntityState ), NULL },
	{ "const Firedef @FiredefForPlayerState( const PlayerState @state, int checkWeapon )", asFUNCTION( GS_FiredefForPlayerState ), NULL },
	
	{ "int DirToByte( const Vec3 &in )", asFUNCTION( GS_asDirToByte ), NULL },

	{ "bool IsEventEntity( const EntityState @ )", asFUNCTION( GS_asIsEventEntity ), NULL },
	{ "bool IsBrushModel( int modelindex )", asFUNCTION( GS_asIsBrushModel ), NULL },

	{ "CModelHandle InlineModel( int modNum )", asFUNCTION( GS_asInlineModel ), NULL },
	{ "void InlineModelBounds( CModelHandle handle, Vec3 & out, Vec3 & out )", asFUNCTION( GS_asInlineModelBounds ), NULL },

	{ NULL },
};

//=======================================================================

static int asPS_MAX_STATS = PS_MAX_STATS;
static int asMAX_GAME_STATS = MAX_GAME_STATS;
static int asMAX_EVENTS = MAX_EVENTS;
static int asMAX_TOUCHENTS = MAXTOUCH;

static float asBASEGRAVITY = BASEGRAVITY;
static float asGRAVITY = GRAVITY;
static float asGRAVITY_COMPENSATE = GRAVITY_COMPENSATE;
static int asZOOMTIME = ZOOMTIME;
static float asSTEPSIZE = STEPSIZE;
static float asSLIDEMOVE_PLANEINTERACT_EPSILON = SLIDEMOVE_PLANEINTERACT_EPSILON;
static float asPROJECTILE_PRESTEP = PROJECTILE_PRESTEP;

static int asMAX_CLIENTS = MAX_CLIENTS;
static int asMAX_EDICTS = MAX_EDICTS;
static int asMAX_LIGHTSTYLES = MAX_LIGHTSTYLES;
static int asMAX_MODELS = MAX_MODELS;
static int asMAX_IMAGES = MAX_IMAGES;
static int asMAX_SKINFILES = MAX_SKINFILES;
static int asMAX_ITEMS = MAX_ITEMS;
static int asMAX_GENERAL = MAX_GENERAL;
static int asMAX_MMPLAYERINFOS = MAX_MMPLAYERINFOS;
static int asMAX_CONFIGSTRINGS = MAX_CONFIGSTRINGS;

static const gs_asglobproperties_t asGameGlobalConstants[] = {
	{ "const int PS_MAX_STATS", &asPS_MAX_STATS },
	{ "const int MAX_GAME_STATS", &asMAX_GAME_STATS },
	{ "const int MAX_EVENTS", &asMAX_EVENTS },
	{ "const int MAX_TOUCHENTS", &asMAX_TOUCHENTS },
	{ "const float BASEGRAVITY", &asBASEGRAVITY },
	{ "const float GRAVITY", &asGRAVITY },
	{ "const float GRAVITY_COMPENSATE", &asGRAVITY_COMPENSATE },
	{ "const int ZOOMTIME", &asZOOMTIME },
	{ "const float STEPSIZE", &asSTEPSIZE },
	{ "const float SLIDEMOVE_PLANEINTERACT_EPSILON", &asSLIDEMOVE_PLANEINTERACT_EPSILON },
	{ "const float PROJECTILE_PRESTEP", &asPROJECTILE_PRESTEP },
	{ "const int MAX_CLIENTS", &asMAX_CLIENTS },
	{ "const int MAX_EDICTS", &asMAX_EDICTS },
	{ "const int MAX_LIGHTSTYLES", &asMAX_LIGHTSTYLES },
	{ "const int MAX_MODELS", &asMAX_MODELS },
	{ "const int MAX_IMAGES", &asMAX_IMAGES },
	{ "const int MAX_SKINFILES", &asMAX_SKINFILES },
	{ "const int MAX_ITEMS", &asMAX_ITEMS },
	{ "const int MAX_GENERAL", &asMAX_GENERAL },
	{ "const int MAX_MMPLAYERINFOS", &asMAX_MMPLAYERINFOS },
	{ "const int MAX_CONFIGSTRINGS", &asMAX_CONFIGSTRINGS },

	{ NULL },
};

static const gs_asglobproperties_t asGameGlobalProperties[] =
{
	{ "GameState gameState", &gs.gameState },
	{ NULL },
};

//=======================================================================

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

	GS_asRegisterGlobalProperties( asEngine, asGameGlobalConstants, NULL );

	GS_asRegisterGlobalProperties( asEngine, asGameGlobalProperties, "GS" );

	GS_asRegisterGlobalFunctions( asEngine, asGameGlobalFunctions, "GS" );

	GS_asRegisterObjectClasses( asEngine, asGameClassesDescriptors, NULL );
}
