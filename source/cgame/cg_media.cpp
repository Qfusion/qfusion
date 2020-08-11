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

/*
* CG_RegisterSfx
*/
struct sfx_s *CG_RegisterSfx( const char *name ) {
	return trap_S_RegisterSound( name );
}

/*
* CG_PrecacheSounds
*/
void CG_PrecacheSounds( void ) {
	int i;

	cgs.media.sfxChat = CG_RegisterSfx( S_CHAT );

	for( i = 0; i < 2; i++ )
		cgs.media.sfxRic[i] = CG_RegisterSfx( va( "sounds/weapons/ric%i", i + 1 ) );

	// weapon
	for( i = 0; i < 4; i++ )
		cgs.media.sfxWeaponHit[i] = CG_RegisterSfx( va( S_WEAPON_HITS, i ) );
	cgs.media.sfxWeaponKill = CG_RegisterSfx( S_WEAPON_KILL );
	cgs.media.sfxWeaponHitTeam = CG_RegisterSfx( S_WEAPON_HIT_TEAM );
	cgs.media.sfxWeaponUp = CG_RegisterSfx( S_WEAPON_SWITCH );
	cgs.media.sfxWeaponUpNoAmmo = CG_RegisterSfx( S_WEAPON_NOAMMO );

	cgs.media.sfxWalljumpFailed = CG_RegisterSfx( "sounds/world/ft_walljump_failed" );

	cgs.media.sfxItemRespawn = CG_RegisterSfx( S_ITEM_RESPAWN );
	cgs.media.sfxPlayerRespawn = CG_RegisterSfx( S_PLAYER_RESPAWN );
	cgs.media.sfxTeleportIn = CG_RegisterSfx( S_TELEPORT );
	cgs.media.sfxTeleportOut = CG_RegisterSfx( S_TELEPORT );

	//	cgs.media.sfxJumpPad = CG_RegisterSfx ( S_JUMPPAD );
	cgs.media.sfxShellHit = CG_RegisterSfx( S_SHELL_HIT );

	// Gunblade sounds (weak is blade):
	for( i = 0; i < 3; i++ ) cgs.media.sfxGunbladeWeakShot[i] = CG_RegisterSfx( va( S_WEAPON_GUNBLADE_W_SHOT_1_to_3, i + 1 ) );
	for( i = 0; i < 3; i++ ) cgs.media.sfxBladeFleshHit[i] = CG_RegisterSfx( va( S_WEAPON_GUNBLADE_W_HIT_FLESH_1_to_3, i + 1 ) );
	for( i = 0; i < 2; i++ ) cgs.media.sfxBladeWallHit[i] = CG_RegisterSfx( va( S_WEAPON_GUNBLADE_W_HIT_WALL_1_to_2, i + 1 ) );
	cgs.media.sfxGunbladeStrongShot = CG_RegisterSfx( S_WEAPON_GUNBLADE_S_SHOT );
	for( i = 0; i < 3; i++ ) cgs.media.sfxGunbladeStrongHit[i] = CG_RegisterSfx( va( S_WEAPON_GUNBLADE_S_HIT_1_to_2, i + 1 ) );

	// Riotgun sounds :
	cgs.media.sfxRiotgunWeakHit = CG_RegisterSfx( S_WEAPON_RIOTGUN_W_HIT );
	cgs.media.sfxRiotgunStrongHit = CG_RegisterSfx( S_WEAPON_RIOTGUN_S_HIT );

	// Grenade launcher sounds :
	for( i = 0; i < 2; i++ ) cgs.media.sfxGrenadeWeakBounce[i] = CG_RegisterSfx( va( S_WEAPON_GRENADE_W_BOUNCE_1_to_2, i + 1 ) );
	for( i = 0; i < 2; i++ ) cgs.media.sfxGrenadeStrongBounce[i] = CG_RegisterSfx( va( S_WEAPON_GRENADE_S_BOUNCE_1_to_2, i + 1 ) );
	cgs.media.sfxGrenadeWeakExplosion = CG_RegisterSfx( S_WEAPON_GRENADE_W_HIT );
	cgs.media.sfxGrenadeStrongExplosion = CG_RegisterSfx( S_WEAPON_GRENADE_S_HIT );

	// Rocket launcher sounds :
	cgs.media.sfxRocketLauncherWeakHit = CG_RegisterSfx( S_WEAPON_ROCKET_W_HIT );
	cgs.media.sfxRocketLauncherStrongHit = CG_RegisterSfx( S_WEAPON_ROCKET_S_HIT );

	// Plasmagun sounds :
	cgs.media.sfxPlasmaWeakHit = CG_RegisterSfx( S_WEAPON_PLASMAGUN_W_HIT );
	cgs.media.sfxPlasmaStrongHit = CG_RegisterSfx( S_WEAPON_PLASMAGUN_S_HIT );

	// Lasergun sounds
	cgs.media.sfxLasergunWeakHum = CG_RegisterSfx( S_WEAPON_LASERGUN_W_HUM );
	cgs.media.sfxLasergunWeakQuadHum = CG_RegisterSfx( S_WEAPON_LASERGUN_W_QUAD_HUM );
	cgs.media.sfxLasergunWeakStop = CG_RegisterSfx( S_WEAPON_LASERGUN_W_STOP );
	cgs.media.sfxLasergunStrongHum = CG_RegisterSfx( S_WEAPON_LASERGUN_S_HUM );
	cgs.media.sfxLasergunStrongQuadHum = CG_RegisterSfx( S_WEAPON_LASERGUN_S_QUAD_HUM );
	cgs.media.sfxLasergunStrongStop = CG_RegisterSfx( S_WEAPON_LASERGUN_S_STOP );
	cgs.media.sfxLasergunHit[0] = CG_RegisterSfx( S_WEAPON_LASERGUN_HIT_0 );
	cgs.media.sfxLasergunHit[1] = CG_RegisterSfx( S_WEAPON_LASERGUN_HIT_1 );
	cgs.media.sfxLasergunHit[2] = CG_RegisterSfx( S_WEAPON_LASERGUN_HIT_2 );

	cgs.media.sfxElectroboltHit = CG_RegisterSfx( S_WEAPON_ELECTROBOLT_HIT );

	cgs.media.sfxQuadFireSound = CG_RegisterSfx( S_QUAD_FIRE );

	//VSAY sounds
	cgs.media.sfxVSaySounds[VSAY_GENERIC] = CG_RegisterSfx( S_CHAT );
	cgs.media.sfxVSaySounds[VSAY_NEEDHEALTH] = CG_RegisterSfx( S_VSAY_NEEDHEALTH );
	cgs.media.sfxVSaySounds[VSAY_NEEDWEAPON] = CG_RegisterSfx( S_VSAY_NEEDWEAPON );
	cgs.media.sfxVSaySounds[VSAY_NEEDARMOR] = CG_RegisterSfx( S_VSAY_NEEDARMOR );
	cgs.media.sfxVSaySounds[VSAY_AFFIRMATIVE] = CG_RegisterSfx( S_VSAY_AFFIRMATIVE );
	cgs.media.sfxVSaySounds[VSAY_NEGATIVE] = CG_RegisterSfx( S_VSAY_NEGATIVE );
	cgs.media.sfxVSaySounds[VSAY_YES] = CG_RegisterSfx( S_VSAY_YES );
	cgs.media.sfxVSaySounds[VSAY_NO] = CG_RegisterSfx( S_VSAY_NO );
	cgs.media.sfxVSaySounds[VSAY_ONDEFENSE] = CG_RegisterSfx( S_VSAY_ONDEFENSE );
	cgs.media.sfxVSaySounds[VSAY_ONOFFENSE] = CG_RegisterSfx( S_VSAY_ONOFFENSE );
	cgs.media.sfxVSaySounds[VSAY_OOPS] = CG_RegisterSfx( S_VSAY_OOPS );
	cgs.media.sfxVSaySounds[VSAY_SORRY] = CG_RegisterSfx( S_VSAY_SORRY );
	cgs.media.sfxVSaySounds[VSAY_THANKS] = CG_RegisterSfx( S_VSAY_THANKS );
	cgs.media.sfxVSaySounds[VSAY_NOPROBLEM] = CG_RegisterSfx( S_VSAY_NOPROBLEM );
	cgs.media.sfxVSaySounds[VSAY_YEEHAA] = CG_RegisterSfx( S_VSAY_YEEHAA );
	cgs.media.sfxVSaySounds[VSAY_GOODGAME] = CG_RegisterSfx( S_VSAY_GOODGAME );
	cgs.media.sfxVSaySounds[VSAY_DEFEND] = CG_RegisterSfx( S_VSAY_DEFEND );
	cgs.media.sfxVSaySounds[VSAY_ATTACK] = CG_RegisterSfx( S_VSAY_ATTACK );
	cgs.media.sfxVSaySounds[VSAY_NEEDBACKUP] = CG_RegisterSfx( S_VSAY_NEEDBACKUP );
	cgs.media.sfxVSaySounds[VSAY_BOOO] = CG_RegisterSfx( S_VSAY_BOOO );
	cgs.media.sfxVSaySounds[VSAY_NEEDDEFENSE] = CG_RegisterSfx( S_VSAY_NEEDDEFENSE );
	cgs.media.sfxVSaySounds[VSAY_NEEDOFFENSE] = CG_RegisterSfx( S_VSAY_NEEDOFFENSE );
	cgs.media.sfxVSaySounds[VSAY_NEEDHELP] = CG_RegisterSfx( S_VSAY_NEEDHELP );
	cgs.media.sfxVSaySounds[VSAY_ROGER] = CG_RegisterSfx( S_VSAY_ROGER );
	cgs.media.sfxVSaySounds[VSAY_ARMORFREE] = CG_RegisterSfx( S_VSAY_ARMORFREE );
	cgs.media.sfxVSaySounds[VSAY_AREASECURED] = CG_RegisterSfx( S_VSAY_AREASECURED );
	cgs.media.sfxVSaySounds[VSAY_BOOMSTICK] = CG_RegisterSfx( S_VSAY_BOOMSTICK );
	cgs.media.sfxVSaySounds[VSAY_GOTOPOWERUP] = CG_RegisterSfx( S_VSAY_GOTOPOWERUP );
	cgs.media.sfxVSaySounds[VSAY_GOTOQUAD] = CG_RegisterSfx( S_VSAY_GOTOQUAD );
	cgs.media.sfxVSaySounds[VSAY_OK] = CG_RegisterSfx( S_VSAY_OK );
	cgs.media.sfxVSaySounds[VSAY_DEFEND_A] = CG_RegisterSfx( S_VSAY_DEFEND_A );
	cgs.media.sfxVSaySounds[VSAY_ATTACK_A] = CG_RegisterSfx( S_VSAY_ATTACK_A );
	cgs.media.sfxVSaySounds[VSAY_DEFEND_B] = CG_RegisterSfx( S_VSAY_DEFEND_B );
	cgs.media.sfxVSaySounds[VSAY_ATTACK_B] = CG_RegisterSfx( S_VSAY_ATTACK_B );
}

//======================================================================

/*
* CG_RegisterModel
*/
struct model_s *CG_RegisterModel( const char *name ) {
	struct model_s *model;

	model = trap_R_RegisterModel( name );

	// precache bones
	if( trap_R_SkeletalGetNumBones( model, NULL ) ) {
		CG_SkeletonForModel( model );
	}

	return model;
}

/*
* CG_PrecacheModels
*/
void CG_PrecacheModels( void ) {
	//	cgs.media.modGrenadeExplosion = CG_RegisterModel( PATH_GRENADE_EXPLOSION_MODEL );
	cgs.media.modRocketExplosion = CG_RegisterModel( PATH_ROCKET_EXPLOSION_MODEL );
	cgs.media.modPlasmaExplosion = CG_RegisterModel( PATH_PLASMA_EXPLOSION_MODEL );

	//	cgs.media.modBoltExplosion = CG_RegisterModel( "models/weapon_hits/electrobolt/hit_electrobolt.md3" );
	//	cgs.media.modInstaExplosion = CG_RegisterModel( "models/weapon_hits/instagun/hit_instagun.md3" );
	//	cgs.media.modTeleportEffect = CG_RegisterModel( "models/misc/telep.md3" );

	cgs.media.modDash = CG_RegisterModel( "models/effects/dash_burst.md3" );
	cgs.media.modHeadStun = CG_RegisterModel( "models/effects/head_stun.md3" );

	cgs.media.modBulletExplode = CG_RegisterModel( PATH_BULLET_EXPLOSION_MODEL );
	cgs.media.modBladeWallHit = CG_RegisterModel( PATH_GUNBLADEBLAST_IMPACT_MODEL );
	cgs.media.modBladeWallExplo = CG_RegisterModel( PATH_GUNBLADEBLAST_EXPLOSION_MODEL );
	cgs.media.modElectroBoltWallHit = CG_RegisterModel( PATH_ELECTROBLAST_IMPACT_MODEL );
	cgs.media.modInstagunWallHit = CG_RegisterModel( PATH_INSTABLAST_IMPACT_MODEL );
	cgs.media.modLasergunWallExplo = CG_RegisterModel( PATH_LASERGUN_IMPACT_MODEL );

	// gibs model
	cgs.media.modIlluminatiGibs = CG_RegisterModel( "models/objects/gibs/illuminati/illuminati1.md3" );
}

//======================================================================

/*
* CG_MediaShader
*/
struct shader_s *CG_RegisterShader( const char *name ) {
	return trap_R_RegisterPic( name );
}


/*
* CG_PrecacheShaders
*/
void CG_PrecacheShaders( void ) {
	cgs.media.shaderParticle = CG_RegisterShader( "particle" );

	cgs.media.shaderNet = CG_RegisterShader( "gfx/hud/net" );
	cgs.media.shaderBackTile = CG_RegisterShader( "gfx/ui/backtile" );
	cgs.media.shaderSelect = CG_RegisterShader( "gfx/hud/select" );
	cgs.media.shaderChatBalloon = CG_RegisterShader( PATH_BALLONCHAT_ICON );
	cgs.media.shaderDownArrow = CG_RegisterShader( "gfx/2d/arrow_down" );

	cgs.media.shaderPlayerShadow = CG_RegisterShader( "gfx/decals/shadow" );

	cgs.media.shaderWaterBubble = CG_RegisterShader( "gfx/misc/waterBubble" );
	cgs.media.shaderSmokePuff = CG_RegisterShader( "gfx/misc/smokepuff" );

	cgs.media.shaderSmokePuff1 = CG_RegisterShader( "gfx/misc/smokepuff1" );
	cgs.media.shaderSmokePuff2 = CG_RegisterShader( "gfx/misc/smokepuff2" );
	cgs.media.shaderSmokePuff3 = CG_RegisterShader( "gfx/misc/smokepuff3" );

	cgs.media.shaderStrongRocketFireTrailPuff = CG_RegisterShader( "gfx/misc/strong_rocket_fire" );
	cgs.media.shaderWeakRocketFireTrailPuff = CG_RegisterShader( "gfx/misc/strong_rocket_fire" );
	cgs.media.shaderTeleporterSmokePuff = CG_RegisterShader( "TeleporterSmokePuff" );
	cgs.media.shaderGrenadeTrailSmokePuff = CG_RegisterShader( "gfx/grenadetrail_smoke_puf" );
	cgs.media.shaderRocketTrailSmokePuff = CG_RegisterShader( "gfx/misc/rocketsmokepuff" );
	cgs.media.shaderBloodTrailPuff = CG_RegisterShader( "gfx/misc/bloodtrail_puff" );
	cgs.media.shaderBloodTrailLiquidPuff = CG_RegisterShader( "gfx/misc/bloodtrailliquid_puff" );
	cgs.media.shaderBloodImpactPuff = CG_RegisterShader( "gfx/misc/bloodimpact_puff" );
	cgs.media.shaderTeamMateIndicator = CG_RegisterShader( "gfx/indicators/teammate_indicator" );
	cgs.media.shaderTeamCarrierIndicator = CG_RegisterShader( "gfx/indicators/teamcarrier_indicator" );
	cgs.media.shaderTeleportShellGfx = CG_RegisterShader( "gfx/misc/teleportshell" );

	cgs.media.shaderAdditiveParticleShine = CG_RegisterShader( "additiveParticleShine" );

	cgs.media.shaderBladeMark = CG_RegisterShader( "gfx/decals/d_blade_hit" );
	cgs.media.shaderBulletMark = CG_RegisterShader( "gfx/decals/d_bullet_hit" );
	cgs.media.shaderExplosionMark = CG_RegisterShader( "gfx/decals/d_explode_hit" );
	cgs.media.shaderPlasmaMark = CG_RegisterShader( "gfx/decals/d_plasma_hit" );
	cgs.media.shaderElectroboltMark = CG_RegisterShader( "gfx/decals/d_electrobolt_hit" );
	cgs.media.shaderInstagunMark = CG_RegisterShader( "gfx/decals/d_instagun_hit" );

	cgs.media.shaderElectroBeamA = CG_RegisterShader( "gfx/misc/electro2a" );
	cgs.media.shaderElectroBeamAAlpha = CG_RegisterShader( "gfx/misc/electro2a_alpha" );
	cgs.media.shaderElectroBeamABeta = CG_RegisterShader( "gfx/misc/electro2a_beta" );
	cgs.media.shaderElectroBeamB = CG_RegisterShader( "gfx/misc/electro2b" );
	cgs.media.shaderElectroBeamBAlpha = CG_RegisterShader( "gfx/misc/electro2b_alpha" );
	cgs.media.shaderElectroBeamBBeta = CG_RegisterShader( "gfx/misc/electro2b_beta" );
	cgs.media.shaderElectroBeamRing = CG_RegisterShader( "gfx/misc/beamring.tga" );
	cgs.media.shaderInstaBeam = CG_RegisterShader( "gfx/misc/instagun" );
	cgs.media.shaderLaserGunBeam = CG_RegisterShader( "gfx/misc/laserbeam" );
	cgs.media.shaderRocketExplosion = CG_RegisterShader( PATH_ROCKET_EXPLOSION_SPRITE );
	cgs.media.shaderRocketExplosionRing = CG_RegisterShader( PATH_ROCKET_EXPLOSION_RING_SPRITE );

	cgs.media.shaderLaser = CG_RegisterShader( "gfx/misc/laser" );

	// ctf
	cgs.media.shaderFlagFlare = CG_RegisterShader( PATH_FLAG_FLARE_SHADER );

	cgs.media.shaderRaceGhostEffect = CG_RegisterShader( "gfx/raceghost" );

	// Kurim : weapon icons
	cgs.media.shaderWeaponIcon[WEAP_GUNBLADE - 1] = CG_RegisterShader( PATH_GUNBLADE_ICON );
	cgs.media.shaderWeaponIcon[WEAP_MACHINEGUN - 1] = CG_RegisterShader( PATH_MACHINEGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_RIOTGUN - 1] = CG_RegisterShader( PATH_RIOTGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_GRENADELAUNCHER - 1] = CG_RegisterShader( PATH_GRENADELAUNCHER_ICON );
	cgs.media.shaderWeaponIcon[WEAP_ROCKETLAUNCHER - 1] = CG_RegisterShader( PATH_ROCKETLAUNCHER_ICON );
	cgs.media.shaderWeaponIcon[WEAP_PLASMAGUN - 1] = CG_RegisterShader( PATH_PLASMAGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_LASERGUN - 1] = CG_RegisterShader( PATH_LASERGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_ELECTROBOLT - 1] = CG_RegisterShader( PATH_ELECTROBOLT_ICON );
	cgs.media.shaderWeaponIcon[WEAP_INSTAGUN - 1] = CG_RegisterShader( PATH_INSTAGUN_ICON );

	cgs.media.shaderNoGunWeaponIcon[WEAP_GUNBLADE - 1] = CG_RegisterShader( PATH_NG_GUNBLADE_ICON );
	cgs.media.shaderNoGunWeaponIcon[WEAP_MACHINEGUN - 1] = CG_RegisterShader( PATH_NG_MACHINEGUN_ICON );
	cgs.media.shaderNoGunWeaponIcon[WEAP_RIOTGUN - 1] = CG_RegisterShader( PATH_NG_RIOTGUN_ICON );
	cgs.media.shaderNoGunWeaponIcon[WEAP_GRENADELAUNCHER - 1] = CG_RegisterShader( PATH_NG_GRENADELAUNCHER_ICON );
	cgs.media.shaderNoGunWeaponIcon[WEAP_ROCKETLAUNCHER - 1] = CG_RegisterShader( PATH_NG_ROCKETLAUNCHER_ICON );
	cgs.media.shaderNoGunWeaponIcon[WEAP_PLASMAGUN - 1] = CG_RegisterShader( PATH_NG_PLASMAGUN_ICON );
	cgs.media.shaderNoGunWeaponIcon[WEAP_LASERGUN - 1] = CG_RegisterShader( PATH_NG_LASERGUN_ICON );
	cgs.media.shaderNoGunWeaponIcon[WEAP_ELECTROBOLT - 1] = CG_RegisterShader( PATH_NG_ELECTROBOLT_ICON );
	cgs.media.shaderNoGunWeaponIcon[WEAP_INSTAGUN - 1] = CG_RegisterShader( PATH_NG_INSTAGUN_ICON );

	cgs.media.shaderGunbladeBlastIcon = CG_RegisterShader( PATH_GUNBLADE_BLAST_ICON );

	cgs.media.shaderInstagunChargeIcon[0] = CG_RegisterShader( "gfx/hud/icons/weapon/instagun_0" );
	cgs.media.shaderInstagunChargeIcon[1] = CG_RegisterShader( "gfx/hud/icons/weapon/instagun_1" );
	cgs.media.shaderInstagunChargeIcon[2] = CG_RegisterShader( "gfx/hud/icons/weapon/instagun_2" );

	// Kurim : keyicons
	cgs.media.shaderKeyIcon[KEYICON_FORWARD] = CG_RegisterShader( PATH_KEYICON_FORWARD );
	cgs.media.shaderKeyIcon[KEYICON_BACKWARD] = CG_RegisterShader( PATH_KEYICON_BACKWARD );
	cgs.media.shaderKeyIcon[KEYICON_LEFT] = CG_RegisterShader( PATH_KEYICON_LEFT );
	cgs.media.shaderKeyIcon[KEYICON_RIGHT] = CG_RegisterShader( PATH_KEYICON_RIGHT );
	cgs.media.shaderKeyIcon[KEYICON_FIRE] = CG_RegisterShader( PATH_KEYICON_FIRE );
	cgs.media.shaderKeyIcon[KEYICON_JUMP] = CG_RegisterShader( PATH_KEYICON_JUMP );
	cgs.media.shaderKeyIcon[KEYICON_CROUCH] = CG_RegisterShader( PATH_KEYICON_CROUCH );
	cgs.media.shaderKeyIcon[KEYICON_SPECIAL] = CG_RegisterShader( PATH_KEYICON_SPECIAL );

	cgs.media.shaderSbNums = CG_RegisterShader( "gfx/hud/sbnums" );

	// VSAY icons
	cgs.media.shaderVSayIcon[VSAY_GENERIC] = CG_RegisterShader( PATH_VSAY_GENERIC_ICON );
	cgs.media.shaderVSayIcon[VSAY_NEEDHEALTH] = CG_RegisterShader( PATH_VSAY_NEEDHEALTH_ICON );
	cgs.media.shaderVSayIcon[VSAY_NEEDWEAPON] = CG_RegisterShader( PATH_VSAY_NEEDWEAPON_ICON );
	cgs.media.shaderVSayIcon[VSAY_NEEDARMOR] = CG_RegisterShader( PATH_VSAY_NEEDARMOR_ICON );
	cgs.media.shaderVSayIcon[VSAY_AFFIRMATIVE] = CG_RegisterShader( PATH_VSAY_AFFIRMATIVE_ICON );
	cgs.media.shaderVSayIcon[VSAY_NEGATIVE] = CG_RegisterShader( PATH_VSAY_NEGATIVE_ICON );
	cgs.media.shaderVSayIcon[VSAY_YES] = CG_RegisterShader( PATH_VSAY_YES_ICON );
	cgs.media.shaderVSayIcon[VSAY_NO] = CG_RegisterShader( PATH_VSAY_NO_ICON );
	cgs.media.shaderVSayIcon[VSAY_ONDEFENSE] = CG_RegisterShader( PATH_VSAY_ONDEFENSE_ICON );
	cgs.media.shaderVSayIcon[VSAY_ONOFFENSE] = CG_RegisterShader( PATH_VSAY_ONOFFENSE_ICON );
	cgs.media.shaderVSayIcon[VSAY_OOPS] = CG_RegisterShader( PATH_VSAY_OOPS_ICON );
	cgs.media.shaderVSayIcon[VSAY_SORRY] = CG_RegisterShader( PATH_VSAY_SORRY_ICON );
	cgs.media.shaderVSayIcon[VSAY_THANKS] = CG_RegisterShader( PATH_VSAY_THANKS_ICON );
	cgs.media.shaderVSayIcon[VSAY_NOPROBLEM] = CG_RegisterShader( PATH_VSAY_NOPROBLEM_ICON );
	cgs.media.shaderVSayIcon[VSAY_YEEHAA] = CG_RegisterShader( PATH_VSAY_YEEHAA_ICON );
	cgs.media.shaderVSayIcon[VSAY_GOODGAME] = CG_RegisterShader( PATH_VSAY_GOODGAME_ICON );
	cgs.media.shaderVSayIcon[VSAY_DEFEND] = CG_RegisterShader( PATH_VSAY_DEFEND_ICON );
	cgs.media.shaderVSayIcon[VSAY_ATTACK] = CG_RegisterShader( PATH_VSAY_ATTACK_ICON );
	cgs.media.shaderVSayIcon[VSAY_NEEDBACKUP] = CG_RegisterShader( PATH_VSAY_NEEDBACKUP_ICON );
	cgs.media.shaderVSayIcon[VSAY_BOOO] = CG_RegisterShader( PATH_VSAY_BOOO_ICON );
	cgs.media.shaderVSayIcon[VSAY_NEEDDEFENSE] = CG_RegisterShader( PATH_VSAY_NEEDDEFENSE_ICON );
	cgs.media.shaderVSayIcon[VSAY_NEEDOFFENSE] = CG_RegisterShader( PATH_VSAY_NEEDOFFENSE_ICON );
	cgs.media.shaderVSayIcon[VSAY_NEEDHELP] = CG_RegisterShader( PATH_VSAY_NEEDHELP_ICON );
	cgs.media.shaderVSayIcon[VSAY_ROGER] = CG_RegisterShader( PATH_VSAY_ROGER_ICON );
	cgs.media.shaderVSayIcon[VSAY_ARMORFREE] = CG_RegisterShader( PATH_VSAY_ARMORFREE_ICON );
	cgs.media.shaderVSayIcon[VSAY_AREASECURED] = CG_RegisterShader( PATH_VSAY_AREASECURED_ICON );
	cgs.media.shaderVSayIcon[VSAY_BOOMSTICK] = CG_RegisterShader( PATH_VSAY_BOOMSTICK_ICON );
	cgs.media.shaderVSayIcon[VSAY_GOTOPOWERUP] = CG_RegisterShader( PATH_VSAY_GOTOPOWERUP_ICON );
	cgs.media.shaderVSayIcon[VSAY_GOTOQUAD] = CG_RegisterShader( PATH_VSAY_GOTOQUAD_ICON );
	cgs.media.shaderVSayIcon[VSAY_OK] = CG_RegisterShader( PATH_VSAY_OK_ICON );
	cgs.media.shaderVSayIcon[VSAY_DEFEND_A] = CG_RegisterShader( PATH_VSAY_DEFEND_A_ICON );
	cgs.media.shaderVSayIcon[VSAY_ATTACK_A] = CG_RegisterShader( PATH_VSAY_ATTACK_A_ICON );
	cgs.media.shaderVSayIcon[VSAY_DEFEND_B] = CG_RegisterShader( PATH_VSAY_DEFEND_B_ICON );
	cgs.media.shaderVSayIcon[VSAY_ATTACK_B] = CG_RegisterShader( PATH_VSAY_ATTACK_B_ICON );
}

/*
* CG_PrecacheMinimap
*/
void CG_PrecacheMinimap( void ) {
	size_t i;
	int file;
	char *name, minimap[MAX_QPATH];

	cgs.shaderMiniMap = NULL;

	name = cgs.configStrings[CS_MAPNAME];

	for( i = 0; i < NUM_IMAGE_EXTENSIONS; i++ ) {
		Q_snprintfz( minimap, sizeof( minimap ), "minimaps/%s%s", name, IMAGE_EXTENSIONS[i] );
		file = trap_FS_FOpenFile( minimap, NULL, FS_READ );
		if( file != -1 ) {
			cgs.shaderMiniMap = trap_R_RegisterPic( minimap );
			break;
		}
	}
}

/*
* CG_PrecacheFonts
*/
void CG_PrecacheFonts( void ) {
	cvar_t *con_fontSystemFamily = trap_Cvar_Get( "con_fontSystemFamily", DEFAULT_SYSTEM_FONT_FAMILY, CVAR_ARCHIVE );
	cvar_t *con_fontSystemMonoFamily = trap_Cvar_Get( "con_fontSystemMonoFamily", DEFAULT_SYSTEM_FONT_FAMILY_MONO, CVAR_ARCHIVE );
	cvar_t *con_fontSystemSmallSize = trap_Cvar_Get( "con_fontSystemSmallSize", STR_TOSTR( DEFAULT_SYSTEM_FONT_SMALL_SIZE ), CVAR_ARCHIVE );
	cvar_t *con_fontSystemMediumSize = trap_Cvar_Get( "con_fontSystemMediumSize", STR_TOSTR( DEFAULT_SYSTEM_FONT_MEDIUM_SIZE ), CVAR_ARCHIVE );
	cvar_t *con_fontSystemBigSize = trap_Cvar_Get( "con_fontSystemBigSize", STR_TOSTR( DEFAULT_SYSTEM_FONT_BIG_SIZE ), CVAR_ARCHIVE );

	// register system fonts
	Q_strncpyz( cgs.fontSystemFamily, con_fontSystemFamily->string, sizeof( cgs.fontSystemFamily ) );
	Q_strncpyz( cgs.fontSystemMonoFamily, con_fontSystemMonoFamily->string, sizeof( cgs.fontSystemMonoFamily ) );
	if( con_fontSystemSmallSize->integer <= 0 ) {
		trap_Cvar_Set( con_fontSystemSmallSize->name, con_fontSystemSmallSize->dvalue );
	}
	if( con_fontSystemMediumSize->integer <= 0 ) {
		trap_Cvar_Set( con_fontSystemMediumSize->name, con_fontSystemMediumSize->dvalue );
	}
	if( con_fontSystemBigSize->integer <= 0 ) {
		trap_Cvar_Set( con_fontSystemBigSize->name, con_fontSystemBigSize->dvalue );
	}

	float scale = ( float )( cgs.vidHeight ) / 600.0f;

	cgs.fontSystemSmallSize = ceilf( con_fontSystemSmallSize->integer * scale );
	cgs.fontSystemSmall = trap_SCR_RegisterFont( cgs.fontSystemFamily, QFONT_STYLE_NONE, cgs.fontSystemSmallSize );
	if( !cgs.fontSystemSmall ) {
		Q_strncpyz( cgs.fontSystemFamily, DEFAULT_SYSTEM_FONT_FAMILY, sizeof( cgs.fontSystemFamily ) );
		cgs.fontSystemSmallSize = ceilf( DEFAULT_SYSTEM_FONT_SMALL_SIZE * scale );

		cgs.fontSystemSmall = trap_SCR_RegisterFont( cgs.fontSystemFamily, QFONT_STYLE_NONE, cgs.fontSystemSmallSize );
		if( !cgs.fontSystemSmall ) {
			CG_Error( "Couldn't load default font \"%s\"", cgs.fontSystemFamily );
		}
	}

	cgs.fontSystemMediumSize = ceilf( con_fontSystemMediumSize->integer * scale );
	cgs.fontSystemMedium = trap_SCR_RegisterFont( cgs.fontSystemFamily, QFONT_STYLE_NONE, cgs.fontSystemMediumSize );
	if( !cgs.fontSystemMedium ) {
		cgs.fontSystemMediumSize = ceilf( DEFAULT_SYSTEM_FONT_MEDIUM_SIZE * scale );
		cgs.fontSystemMedium = trap_SCR_RegisterFont( cgs.fontSystemFamily, QFONT_STYLE_NONE, cgs.fontSystemMediumSize );
	}

	cgs.fontSystemBigSize = ceilf( con_fontSystemBigSize->integer * scale );
	cgs.fontSystemBig = trap_SCR_RegisterFont( cgs.fontSystemFamily, QFONT_STYLE_NONE, cgs.fontSystemBigSize );
	if( !cgs.fontSystemBig ) {
		cgs.fontSystemBigSize = ceilf( DEFAULT_SYSTEM_FONT_BIG_SIZE * scale );
		cgs.fontSystemBig = trap_SCR_RegisterFont( cgs.fontSystemFamily, QFONT_STYLE_NONE, cgs.fontSystemBigSize );
	}
}
