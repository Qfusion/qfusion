namespace CGame {
    
class CMedia {
    ModelHandle @modIlluminatiGib;
    ModelHandle @modFlag;
    ModelHandle @modHeadStun;
    ModelHandle @modDash;

	ModelHandle @modRocketExplosion;
	ModelHandle @modPlasmaExplosion;

	ModelHandle @modBulletExplode;
	ModelHandle @modBladeWallHit;
	ModelHandle @modBladeWallExplo;

	ModelHandle @modElectroBoltWallHit;
	ModelHandle @modInstagunWallHit;

	ModelHandle @modLasergunWallExplo;

    ShaderHandle @shaderLaser;
    ShaderHandle @shaderFlagFlare;
    ShaderHandle @shaderRaceGhostEffect;
    ShaderHandle @shaderChatBalloon;
	ShaderHandle @shaderWaterBubble;
    ShaderHandle @shaderSmokePuff;
    ShaderHandle @shaderSmokePuff1, shaderSmokePuff2, shaderSmokePuff3;

    ShaderHandle @shaderStrongRocketFireTrailPuff;
    ShaderHandle @shaderWeakRocketFireTrailPuff;
    ShaderHandle @shaderTeleporterSmokePuff;
    ShaderHandle @shaderGrenadeTrailSmokePuff;
    ShaderHandle @shaderRocketTrailSmokePuff;
    ShaderHandle @shaderBloodTrailPuff;
    ShaderHandle @shaderBloodTrailLiquidPuff;
    ShaderHandle @shaderBloodImpactPuff;
    ShaderHandle @shaderTeamMateIndicator;
    ShaderHandle @shaderTeamCarrierIndicator;
    ShaderHandle @shaderTeleportShellGfx;

    ShaderHandle @shaderElectroBeamA;
    ShaderHandle @shaderElectroBeamAAlpha;
    ShaderHandle @shaderElectroBeamABeta;
    ShaderHandle @shaderElectroBeamB;
    ShaderHandle @shaderElectroBeamBAlpha;
    ShaderHandle @shaderElectroBeamBBeta;
    ShaderHandle @shaderElectroBeamRing;
    ShaderHandle @shaderInstaBeam;
    ShaderHandle @shaderLaserGunBeam;
    ShaderHandle @shaderRocketExplosion;
    ShaderHandle @shaderRocketExplosionRing;

    ShaderHandle @shaderBladeMark;
    ShaderHandle @shaderBulletMark;
    ShaderHandle @shaderExplosionMark;
    ShaderHandle @shaderPlasmaMark;
    ShaderHandle @shaderElectroboltMark;
    ShaderHandle @shaderInstagunMark;

    array<ShaderHandle @> shaderVSayIcon(eVSays::VSAY_TOTAL);

    SoundHandle @sfxItemRespawn;
    SoundHandle @sfxPlayerRespawn;
    SoundHandle @sfxTeleportIn;
    SoundHandle @sfxTeleportOut;
	SoundHandle @sfxWeaponUp;
	SoundHandle @sfxWeaponUpNoAmmo;
    SoundHandle @sfxQuadFireSound;
    SoundHandle @sfxShellHit;
    SoundHandle @sfxWalljumpFailed;
    array<SoundHandle @> sfxRic(2);

	// Gunblade sounds :
	array<SoundHandle @> sfxGunbladeWeakShot(3);
	SoundHandle @sfxGunbladeStrongShot;
	array<SoundHandle @> sfxBladeFleshHit(3);
	array<SoundHandle @> sfxBladeWallHit(2);
	array<SoundHandle @> sfxGunbladeStrongHit(3);

	// Grenade launcher sounds :
	array<SoundHandle @>sfxGrenadeWeakBounce(2);
	array<SoundHandle @>sfxGrenadeStrongBounce(2);
	SoundHandle @sfxGrenadeWeakExplosion;
	SoundHandle @sfxGrenadeStrongExplosion;
 
    // Rocket launcher sounds :
    SoundHandle @sfxRocketLauncherWeakHit;
    SoundHandle @sfxRocketLauncherStrongHit;

	// Lasergun sounds
	SoundHandle @sfxLasergunWeakHum;
	SoundHandle @sfxLasergunWeakQuadHum;
	SoundHandle @sfxLasergunWeakStop;
	SoundHandle @sfxLasergunStrongHum;
	SoundHandle @sfxLasergunStrongQuadHum;
	SoundHandle @sfxLasergunStrongStop;
	array<SoundHandle @>sfxLasergunHit(3);

	SoundHandle @sfxElectroboltHit;

    array<SoundHandle @> sfxVSaySounds(eVSays::VSAY_TOTAL);

    void PrecacheShaders() {
        @shaderLaser = CGame::RegisterShader( "gfx/misc/laser" );
        @shaderFlagFlare = CGame::RegisterShader( PATH_FLAG_FLARE_SHADER );
        @shaderRaceGhostEffect = CGame::RegisterShader( "gfx/raceghost" );
        @shaderChatBalloon = CGame::RegisterShader( PATH_BALLONCHAT_ICON );
	    @shaderWaterBubble = CGame::RegisterShader( "gfx/misc/waterBubble" );
        @shaderSmokePuff = CGame::RegisterShader( "gfx/misc/smokepuff" );
        @shaderSmokePuff1 = CGame::RegisterShader( "gfx/misc/smokepuff1" );
        @shaderSmokePuff2 = CGame::RegisterShader( "gfx/misc/smokepuff2" );
        @shaderSmokePuff3 = CGame::RegisterShader( "gfx/misc/smokepuff3" );

        @shaderStrongRocketFireTrailPuff = CGame::RegisterShader( "gfx/misc/strong_rocket_fire" );
        @shaderWeakRocketFireTrailPuff = CGame::RegisterShader( "gfx/misc/strong_rocket_fire" );
        @shaderTeleporterSmokePuff = CGame::RegisterShader( "TeleporterSmokePuff" );
        @shaderGrenadeTrailSmokePuff = CGame::RegisterShader( "gfx/grenadetrail_smoke_puf" );
        @shaderRocketTrailSmokePuff = CGame::RegisterShader( "gfx/misc/rocketsmokepuff" );
        @shaderBloodTrailPuff = CGame::RegisterShader( "gfx/misc/bloodtrail_puff" );
        @shaderBloodTrailLiquidPuff = CGame::RegisterShader( "gfx/misc/bloodtrailliquid_puff" );
        @shaderBloodImpactPuff = CGame::RegisterShader( "gfx/misc/bloodimpact_puff" );
        @shaderTeamMateIndicator = CGame::RegisterShader( "gfx/indicators/teammate_indicator" );
        @shaderTeamCarrierIndicator = CGame::RegisterShader( "gfx/indicators/teamcarrier_indicator" );
        @shaderTeleportShellGfx = CGame::RegisterShader( "gfx/misc/teleportshell" );

        @shaderElectroBeamA = CGame::RegisterShader( "gfx/misc/electro2a" );
        @shaderElectroBeamAAlpha = CGame::RegisterShader( "gfx/misc/electro2a_alpha" );
        @shaderElectroBeamABeta = CGame::RegisterShader( "gfx/misc/electro2a_beta" );
        @shaderElectroBeamB = CGame::RegisterShader( "gfx/misc/electro2b" );
        @shaderElectroBeamBAlpha = CGame::RegisterShader( "gfx/misc/electro2b_alpha" );
        @shaderElectroBeamBBeta = CGame::RegisterShader( "gfx/misc/electro2b_beta" );
        @shaderElectroBeamRing = CGame::RegisterShader( "gfx/misc/beamring.tga" );
        @shaderInstaBeam = CGame::RegisterShader( "gfx/misc/instagun" );
        @shaderLaserGunBeam = CGame::RegisterShader( "gfx/misc/laserbeam" );
        @shaderRocketExplosion = CGame::RegisterShader( PATH_ROCKET_EXPLOSION_SPRITE );
        @shaderRocketExplosionRing = CGame::RegisterShader( PATH_ROCKET_EXPLOSION_RING_SPRITE );

        @shaderBladeMark = CGame::RegisterShader( "gfx/decals/d_blade_hit" );
        @shaderBulletMark = CGame::RegisterShader( "gfx/decals/d_bullet_hit" );
        @shaderExplosionMark = CGame::RegisterShader( "gfx/decals/d_explode_hit" );
        @shaderPlasmaMark = CGame::RegisterShader( "gfx/decals/d_plasma_hit" );
        @shaderElectroboltMark = CGame::RegisterShader( "gfx/decals/d_electrobolt_hit" );
        @shaderInstagunMark = CGame::RegisterShader( "gfx/decals/d_instagun_hit" );

        // VSAY icons
        @shaderVSayIcon[VSAY_GENERIC] = CGame::RegisterShader( PATH_VSAY_GENERIC_ICON );
        @shaderVSayIcon[VSAY_NEEDHEALTH] = CGame::RegisterShader( PATH_VSAY_NEEDHEALTH_ICON );
        @shaderVSayIcon[VSAY_NEEDWEAPON] = CGame::RegisterShader( PATH_VSAY_NEEDWEAPON_ICON );
        @shaderVSayIcon[VSAY_NEEDARMOR] = CGame::RegisterShader( PATH_VSAY_NEEDARMOR_ICON );
        @shaderVSayIcon[VSAY_AFFIRMATIVE] = CGame::RegisterShader( PATH_VSAY_AFFIRMATIVE_ICON );
        @shaderVSayIcon[VSAY_NEGATIVE] = CGame::RegisterShader( PATH_VSAY_NEGATIVE_ICON );
        @shaderVSayIcon[VSAY_YES] = CGame::RegisterShader( PATH_VSAY_YES_ICON );
        @shaderVSayIcon[VSAY_NO] = CGame::RegisterShader( PATH_VSAY_NO_ICON );
        @shaderVSayIcon[VSAY_ONDEFENSE] = CGame::RegisterShader( PATH_VSAY_ONDEFENSE_ICON );
        @shaderVSayIcon[VSAY_ONOFFENSE] = CGame::RegisterShader( PATH_VSAY_ONOFFENSE_ICON );
        @shaderVSayIcon[VSAY_OOPS] = CGame::RegisterShader( PATH_VSAY_OOPS_ICON );
        @shaderVSayIcon[VSAY_SORRY] = CGame::RegisterShader( PATH_VSAY_SORRY_ICON );
        @shaderVSayIcon[VSAY_THANKS] = CGame::RegisterShader( PATH_VSAY_THANKS_ICON );
        @shaderVSayIcon[VSAY_NOPROBLEM] = CGame::RegisterShader( PATH_VSAY_NOPROBLEM_ICON );
        @shaderVSayIcon[VSAY_YEEHAA] = CGame::RegisterShader( PATH_VSAY_YEEHAA_ICON );
        @shaderVSayIcon[VSAY_GOODGAME] = CGame::RegisterShader( PATH_VSAY_GOODGAME_ICON );
        @shaderVSayIcon[VSAY_DEFEND] = CGame::RegisterShader( PATH_VSAY_DEFEND_ICON );
        @shaderVSayIcon[VSAY_ATTACK] = CGame::RegisterShader( PATH_VSAY_ATTACK_ICON );
        @shaderVSayIcon[VSAY_NEEDBACKUP] = CGame::RegisterShader( PATH_VSAY_NEEDBACKUP_ICON );
        @shaderVSayIcon[VSAY_BOOO] = CGame::RegisterShader( PATH_VSAY_BOOO_ICON );
        @shaderVSayIcon[VSAY_NEEDDEFENSE] = CGame::RegisterShader( PATH_VSAY_NEEDDEFENSE_ICON );
        @shaderVSayIcon[VSAY_NEEDOFFENSE] = CGame::RegisterShader( PATH_VSAY_NEEDOFFENSE_ICON );
        @shaderVSayIcon[VSAY_NEEDHELP] = CGame::RegisterShader( PATH_VSAY_NEEDHELP_ICON );
        @shaderVSayIcon[VSAY_ROGER] = CGame::RegisterShader( PATH_VSAY_ROGER_ICON );
        @shaderVSayIcon[VSAY_ARMORFREE] = CGame::RegisterShader( PATH_VSAY_ARMORFREE_ICON );
        @shaderVSayIcon[VSAY_AREASECURED] = CGame::RegisterShader( PATH_VSAY_AREASECURED_ICON );
        @shaderVSayIcon[VSAY_BOOMSTICK] = CGame::RegisterShader( PATH_VSAY_BOOMSTICK_ICON );
        @shaderVSayIcon[VSAY_GOTOPOWERUP] = CGame::RegisterShader( PATH_VSAY_GOTOPOWERUP_ICON );
        @shaderVSayIcon[VSAY_GOTOQUAD] = CGame::RegisterShader( PATH_VSAY_GOTOQUAD_ICON );
        @shaderVSayIcon[VSAY_OK] = CGame::RegisterShader( PATH_VSAY_OK_ICON );
        @shaderVSayIcon[VSAY_DEFEND_A] = CGame::RegisterShader( PATH_VSAY_DEFEND_A_ICON );
        @shaderVSayIcon[VSAY_ATTACK_A] = CGame::RegisterShader( PATH_VSAY_ATTACK_A_ICON );
        @shaderVSayIcon[VSAY_DEFEND_B] = CGame::RegisterShader( PATH_VSAY_DEFEND_B_ICON );
        @shaderVSayIcon[VSAY_ATTACK_B] = CGame::RegisterShader( PATH_VSAY_ATTACK_B_ICON );
    }

    void PrecacheModels() {
        @modIlluminatiGib = CGame::RegisterModel( "models/objects/gibs/illuminati/illuminati1.md3" );
        @modFlag = CGame::RegisterModel( PATH_FLAG_MODEL );
        @modHeadStun = CGame::RegisterModel( "models/effects/head_stun.md3" );
        @modDash = CGame::RegisterModel( "models/effects/dash_burst.md3" );

        @modBulletExplode = CGame::RegisterModel( PATH_BULLET_EXPLOSION_MODEL );
        @modBladeWallHit = CGame::RegisterModel( PATH_GUNBLADEBLAST_IMPACT_MODEL );
        @modBladeWallExplo = CGame::RegisterModel( PATH_GUNBLADEBLAST_EXPLOSION_MODEL );
        @modElectroBoltWallHit = CGame::RegisterModel( PATH_ELECTROBLAST_IMPACT_MODEL );
        @modInstagunWallHit = CGame::RegisterModel( PATH_INSTABLAST_IMPACT_MODEL );
        @modLasergunWallExplo = CGame::RegisterModel( PATH_LASERGUN_IMPACT_MODEL );
    }

    void PrecacheSounds() {
        @sfxItemRespawn = CGame::RegisterSound( S_ITEM_RESPAWN );
        @sfxPlayerRespawn = CGame::RegisterSound( S_PLAYER_RESPAWN );
        @sfxTeleportIn = CGame::RegisterSound( S_TELEPORT );
        @sfxTeleportOut = CGame::RegisterSound( S_TELEPORT );
        @sfxWeaponUp = CGame::RegisterSound( S_WEAPON_SWITCH );
        @sfxWeaponUpNoAmmo = CGame::RegisterSound( S_WEAPON_NOAMMO );
        @sfxQuadFireSound = CGame::RegisterSound( S_QUAD_FIRE );
        @sfxShellHit = CGame::RegisterSound( S_SHELL_HIT );
        @sfxWalljumpFailed = CGame::RegisterSound( "sounds/world/ft_walljump_failed" );

    	for( int i = 0; i < 2; i++ ) {
            @sfxGrenadeWeakBounce[i] = CGame::RegisterSound( StringUtils::Format( S_WEAPON_GRENADE_W_BOUNCE_1_to_2, i + 1 ) );
            @sfxGrenadeStrongBounce[i] = CGame::RegisterSound( StringUtils::Format( S_WEAPON_GRENADE_S_BOUNCE_1_to_2, i + 1 ) );
        }

        for( int i = 0; i < 2; i++ ) {
            @sfxRic[i] = CGame::RegisterSound( StringUtils::Format( "sounds/weapons/ric%s", i + 1 ) );
        }

	// Gunblade sounds (weak is blade):
        for( int i = 0; i < 3; i++ ) 
            @sfxGunbladeWeakShot[i] = CGame::RegisterSound( StringUtils::Format( S_WEAPON_GUNBLADE_W_SHOT_1_to_3, i + 1 ) );
        for( int i = 0; i < 3; i++ ) 
            @sfxBladeFleshHit[i] = CGame::RegisterSound( StringUtils::Format( S_WEAPON_GUNBLADE_W_HIT_FLESH_1_to_3, i + 1 ) );
        for( int i = 0; i < 2; i++ )
            @sfxBladeWallHit[i] = CGame::RegisterSound( StringUtils::Format( S_WEAPON_GUNBLADE_W_HIT_WALL_1_to_2, i + 1 ) );
        @sfxGunbladeStrongShot = CGame::RegisterSound( S_WEAPON_GUNBLADE_S_SHOT );
        for( int i = 0; i < 3; i++ )
            @sfxGunbladeStrongHit[i] = CGame::RegisterSound( StringUtils::Format( S_WEAPON_GUNBLADE_S_HIT_1_to_2, i + 1 ) );

        // Rocket launcher sounds :
        @sfxRocketLauncherWeakHit = CGame::RegisterSound( S_WEAPON_ROCKET_W_HIT );
        @sfxRocketLauncherStrongHit = CGame::RegisterSound( S_WEAPON_ROCKET_S_HIT );

        // Lasergun sounds
        @sfxLasergunWeakHum = CGame::RegisterSound( S_WEAPON_LASERGUN_W_HUM );
        @sfxLasergunWeakQuadHum = CGame::RegisterSound( S_WEAPON_LASERGUN_W_QUAD_HUM );
        @sfxLasergunWeakStop = CGame::RegisterSound( S_WEAPON_LASERGUN_W_STOP );
        @sfxLasergunStrongHum = CGame::RegisterSound( S_WEAPON_LASERGUN_S_HUM );
        @sfxLasergunStrongQuadHum = CGame::RegisterSound( S_WEAPON_LASERGUN_S_QUAD_HUM );
        @sfxLasergunStrongStop = CGame::RegisterSound( S_WEAPON_LASERGUN_S_STOP );
        @sfxLasergunHit[0] = CGame::RegisterSound( S_WEAPON_LASERGUN_HIT_0 );
        @sfxLasergunHit[1] = CGame::RegisterSound( S_WEAPON_LASERGUN_HIT_1 );
        @sfxLasergunHit[2] = CGame::RegisterSound( S_WEAPON_LASERGUN_HIT_2 );

        @sfxElectroboltHit = CGame::RegisterSound( S_WEAPON_ELECTROBOLT_HIT );

        //VSAY sounds
        @sfxVSaySounds[VSAY_GENERIC] = CGame::RegisterSound( S_CHAT );
        @sfxVSaySounds[VSAY_NEEDHEALTH] = CGame::RegisterSound( S_VSAY_NEEDHEALTH );
        @sfxVSaySounds[VSAY_NEEDWEAPON] = CGame::RegisterSound( S_VSAY_NEEDWEAPON );
        @sfxVSaySounds[VSAY_NEEDARMOR] = CGame::RegisterSound( S_VSAY_NEEDARMOR );
        @sfxVSaySounds[VSAY_AFFIRMATIVE] = CGame::RegisterSound( S_VSAY_AFFIRMATIVE );
        @sfxVSaySounds[VSAY_NEGATIVE] = CGame::RegisterSound( S_VSAY_NEGATIVE );
        @sfxVSaySounds[VSAY_YES] = CGame::RegisterSound( S_VSAY_YES );
        @sfxVSaySounds[VSAY_NO] = CGame::RegisterSound( S_VSAY_NO );
        @sfxVSaySounds[VSAY_ONDEFENSE] = CGame::RegisterSound( S_VSAY_ONDEFENSE );
        @sfxVSaySounds[VSAY_ONOFFENSE] = CGame::RegisterSound( S_VSAY_ONOFFENSE );
        @sfxVSaySounds[VSAY_OOPS] = CGame::RegisterSound( S_VSAY_OOPS );
        @sfxVSaySounds[VSAY_SORRY] = CGame::RegisterSound( S_VSAY_SORRY );
        @sfxVSaySounds[VSAY_THANKS] = CGame::RegisterSound( S_VSAY_THANKS );
        @sfxVSaySounds[VSAY_NOPROBLEM] = CGame::RegisterSound( S_VSAY_NOPROBLEM );
        @sfxVSaySounds[VSAY_YEEHAA] = CGame::RegisterSound( S_VSAY_YEEHAA );
        @sfxVSaySounds[VSAY_GOODGAME] = CGame::RegisterSound( S_VSAY_GOODGAME );
        @sfxVSaySounds[VSAY_DEFEND] = CGame::RegisterSound( S_VSAY_DEFEND );
        @sfxVSaySounds[VSAY_ATTACK] = CGame::RegisterSound( S_VSAY_ATTACK );
        @sfxVSaySounds[VSAY_NEEDBACKUP] = CGame::RegisterSound( S_VSAY_NEEDBACKUP );
        @sfxVSaySounds[VSAY_BOOO] = CGame::RegisterSound( S_VSAY_BOOO );
        @sfxVSaySounds[VSAY_NEEDDEFENSE] = CGame::RegisterSound( S_VSAY_NEEDDEFENSE );
        @sfxVSaySounds[VSAY_NEEDOFFENSE] = CGame::RegisterSound( S_VSAY_NEEDOFFENSE );
        @sfxVSaySounds[VSAY_NEEDHELP] = CGame::RegisterSound( S_VSAY_NEEDHELP );
        @sfxVSaySounds[VSAY_ROGER] = CGame::RegisterSound( S_VSAY_ROGER );
        @sfxVSaySounds[VSAY_ARMORFREE] = CGame::RegisterSound( S_VSAY_ARMORFREE );
        @sfxVSaySounds[VSAY_AREASECURED] = CGame::RegisterSound( S_VSAY_AREASECURED );
        @sfxVSaySounds[VSAY_BOOMSTICK] = CGame::RegisterSound( S_VSAY_BOOMSTICK );
        @sfxVSaySounds[VSAY_GOTOPOWERUP] = CGame::RegisterSound( S_VSAY_GOTOPOWERUP );
        @sfxVSaySounds[VSAY_GOTOQUAD] = CGame::RegisterSound( S_VSAY_GOTOQUAD );
        @sfxVSaySounds[VSAY_OK] = CGame::RegisterSound( S_VSAY_OK );
        @sfxVSaySounds[VSAY_DEFEND_A] = CGame::RegisterSound( S_VSAY_DEFEND_A );
        @sfxVSaySounds[VSAY_ATTACK_A] = CGame::RegisterSound( S_VSAY_ATTACK_A );
        @sfxVSaySounds[VSAY_DEFEND_B] = CGame::RegisterSound( S_VSAY_DEFEND_B );
        @sfxVSaySounds[VSAY_ATTACK_B] = CGame::RegisterSound( S_VSAY_ATTACK_B );        
    }
}

}
