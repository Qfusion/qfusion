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

    ModelHandle @modIlluminatiGibs;

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
	array<SoundHandle @>sfxWeaponHit(4);
	SoundHandle @sfxWeaponKill;
	SoundHandle @sfxWeaponHitTeam;
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
 
    // Plasmagun sounds :
    SoundHandle @sfxPlasmaWeakHit;
    SoundHandle @sfxPlasmaStrongHit;

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

	// Riotgun sounds
	SoundHandle @sfxRiotgunWeakHit;
	SoundHandle @sfxRiotgunStrongHit;

	SoundHandle @sfxElectroboltHit;

    array<SoundHandle @> sfxVSaySounds(eVSays::VSAY_TOTAL);

    void PrecacheShaders() {
        @shaderLaser = RegisterShader( "gfx/misc/laser" );
        @shaderFlagFlare = RegisterShader( PATH_FLAG_FLARE_SHADER );
        @shaderRaceGhostEffect = RegisterShader( "gfx/raceghost" );
        @shaderChatBalloon = RegisterShader( PATH_BALLONCHAT_ICON );
	    @shaderWaterBubble = RegisterShader( "gfx/misc/waterBubble" );
        @shaderSmokePuff = RegisterShader( "gfx/misc/smokepuff" );
        @shaderSmokePuff1 = RegisterShader( "gfx/misc/smokepuff1" );
        @shaderSmokePuff2 = RegisterShader( "gfx/misc/smokepuff2" );
        @shaderSmokePuff3 = RegisterShader( "gfx/misc/smokepuff3" );

        @shaderStrongRocketFireTrailPuff = RegisterShader( "gfx/misc/strong_rocket_fire" );
        @shaderWeakRocketFireTrailPuff = RegisterShader( "gfx/misc/strong_rocket_fire" );
        @shaderTeleporterSmokePuff = RegisterShader( "TeleporterSmokePuff" );
        @shaderGrenadeTrailSmokePuff = RegisterShader( "gfx/grenadetrail_smoke_puf" );
        @shaderRocketTrailSmokePuff = RegisterShader( "gfx/misc/rocketsmokepuff" );
        @shaderBloodTrailPuff = RegisterShader( "gfx/misc/bloodtrail_puff" );
        @shaderBloodTrailLiquidPuff = RegisterShader( "gfx/misc/bloodtrailliquid_puff" );
        @shaderBloodImpactPuff = RegisterShader( "gfx/misc/bloodimpact_puff" );
        @shaderTeamMateIndicator = RegisterShader( "gfx/indicators/teammate_indicator" );
        @shaderTeamCarrierIndicator = RegisterShader( "gfx/indicators/teamcarrier_indicator" );
        @shaderTeleportShellGfx = RegisterShader( "gfx/misc/teleportshell" );

        @shaderElectroBeamA = RegisterShader( "gfx/misc/electro2a" );
        @shaderElectroBeamAAlpha = RegisterShader( "gfx/misc/electro2a_alpha" );
        @shaderElectroBeamABeta = RegisterShader( "gfx/misc/electro2a_beta" );
        @shaderElectroBeamB = RegisterShader( "gfx/misc/electro2b" );
        @shaderElectroBeamBAlpha = RegisterShader( "gfx/misc/electro2b_alpha" );
        @shaderElectroBeamBBeta = RegisterShader( "gfx/misc/electro2b_beta" );
        @shaderElectroBeamRing = RegisterShader( "gfx/misc/beamring.tga" );
        @shaderInstaBeam = RegisterShader( "gfx/misc/instagun" );
        @shaderLaserGunBeam = RegisterShader( "gfx/misc/laserbeam" );
        @shaderRocketExplosion = RegisterShader( PATH_ROCKET_EXPLOSION_SPRITE );
        @shaderRocketExplosionRing = RegisterShader( PATH_ROCKET_EXPLOSION_RING_SPRITE );

        @shaderBladeMark = RegisterShader( "gfx/decals/d_blade_hit" );
        @shaderBulletMark = RegisterShader( "gfx/decals/d_bullet_hit" );
        @shaderExplosionMark = RegisterShader( "gfx/decals/d_explode_hit" );
        @shaderPlasmaMark = RegisterShader( "gfx/decals/d_plasma_hit" );
        @shaderElectroboltMark = RegisterShader( "gfx/decals/d_electrobolt_hit" );
        @shaderInstagunMark = RegisterShader( "gfx/decals/d_instagun_hit" );

        // VSAY icons
        @shaderVSayIcon[VSAY_GENERIC] = RegisterShader( PATH_VSAY_GENERIC_ICON );
        @shaderVSayIcon[VSAY_NEEDHEALTH] = RegisterShader( PATH_VSAY_NEEDHEALTH_ICON );
        @shaderVSayIcon[VSAY_NEEDWEAPON] = RegisterShader( PATH_VSAY_NEEDWEAPON_ICON );
        @shaderVSayIcon[VSAY_NEEDARMOR] = RegisterShader( PATH_VSAY_NEEDARMOR_ICON );
        @shaderVSayIcon[VSAY_AFFIRMATIVE] = RegisterShader( PATH_VSAY_AFFIRMATIVE_ICON );
        @shaderVSayIcon[VSAY_NEGATIVE] = RegisterShader( PATH_VSAY_NEGATIVE_ICON );
        @shaderVSayIcon[VSAY_YES] = RegisterShader( PATH_VSAY_YES_ICON );
        @shaderVSayIcon[VSAY_NO] = RegisterShader( PATH_VSAY_NO_ICON );
        @shaderVSayIcon[VSAY_ONDEFENSE] = RegisterShader( PATH_VSAY_ONDEFENSE_ICON );
        @shaderVSayIcon[VSAY_ONOFFENSE] = RegisterShader( PATH_VSAY_ONOFFENSE_ICON );
        @shaderVSayIcon[VSAY_OOPS] = RegisterShader( PATH_VSAY_OOPS_ICON );
        @shaderVSayIcon[VSAY_SORRY] = RegisterShader( PATH_VSAY_SORRY_ICON );
        @shaderVSayIcon[VSAY_THANKS] = RegisterShader( PATH_VSAY_THANKS_ICON );
        @shaderVSayIcon[VSAY_NOPROBLEM] = RegisterShader( PATH_VSAY_NOPROBLEM_ICON );
        @shaderVSayIcon[VSAY_YEEHAA] = RegisterShader( PATH_VSAY_YEEHAA_ICON );
        @shaderVSayIcon[VSAY_GOODGAME] = RegisterShader( PATH_VSAY_GOODGAME_ICON );
        @shaderVSayIcon[VSAY_DEFEND] = RegisterShader( PATH_VSAY_DEFEND_ICON );
        @shaderVSayIcon[VSAY_ATTACK] = RegisterShader( PATH_VSAY_ATTACK_ICON );
        @shaderVSayIcon[VSAY_NEEDBACKUP] = RegisterShader( PATH_VSAY_NEEDBACKUP_ICON );
        @shaderVSayIcon[VSAY_BOOO] = RegisterShader( PATH_VSAY_BOOO_ICON );
        @shaderVSayIcon[VSAY_NEEDDEFENSE] = RegisterShader( PATH_VSAY_NEEDDEFENSE_ICON );
        @shaderVSayIcon[VSAY_NEEDOFFENSE] = RegisterShader( PATH_VSAY_NEEDOFFENSE_ICON );
        @shaderVSayIcon[VSAY_NEEDHELP] = RegisterShader( PATH_VSAY_NEEDHELP_ICON );
        @shaderVSayIcon[VSAY_ROGER] = RegisterShader( PATH_VSAY_ROGER_ICON );
        @shaderVSayIcon[VSAY_ARMORFREE] = RegisterShader( PATH_VSAY_ARMORFREE_ICON );
        @shaderVSayIcon[VSAY_AREASECURED] = RegisterShader( PATH_VSAY_AREASECURED_ICON );
        @shaderVSayIcon[VSAY_BOOMSTICK] = RegisterShader( PATH_VSAY_BOOMSTICK_ICON );
        @shaderVSayIcon[VSAY_GOTOPOWERUP] = RegisterShader( PATH_VSAY_GOTOPOWERUP_ICON );
        @shaderVSayIcon[VSAY_GOTOQUAD] = RegisterShader( PATH_VSAY_GOTOQUAD_ICON );
        @shaderVSayIcon[VSAY_OK] = RegisterShader( PATH_VSAY_OK_ICON );
        @shaderVSayIcon[VSAY_DEFEND_A] = RegisterShader( PATH_VSAY_DEFEND_A_ICON );
        @shaderVSayIcon[VSAY_ATTACK_A] = RegisterShader( PATH_VSAY_ATTACK_A_ICON );
        @shaderVSayIcon[VSAY_DEFEND_B] = RegisterShader( PATH_VSAY_DEFEND_B_ICON );
        @shaderVSayIcon[VSAY_ATTACK_B] = RegisterShader( PATH_VSAY_ATTACK_B_ICON );
    }

    void PrecacheModels() {
        @modRocketExplosion = RegisterModel( PATH_ROCKET_EXPLOSION_MODEL );
        @modPlasmaExplosion = RegisterModel( PATH_PLASMA_EXPLOSION_MODEL );

        @modIlluminatiGib = RegisterModel( "models/objects/gibs/illuminati/illuminati1.md3" );
        @modFlag = RegisterModel( PATH_FLAG_MODEL );
        @modHeadStun = RegisterModel( "models/effects/head_stun.md3" );
        @modDash = RegisterModel( "models/effects/dash_burst.md3" );

        @modBulletExplode = RegisterModel( PATH_BULLET_EXPLOSION_MODEL );
        @modBladeWallHit = RegisterModel( PATH_GUNBLADEBLAST_IMPACT_MODEL );
        @modBladeWallExplo = RegisterModel( PATH_GUNBLADEBLAST_EXPLOSION_MODEL );
        @modElectroBoltWallHit = RegisterModel( PATH_ELECTROBLAST_IMPACT_MODEL );
        @modInstagunWallHit = RegisterModel( PATH_INSTABLAST_IMPACT_MODEL );
        @modLasergunWallExplo = RegisterModel( PATH_LASERGUN_IMPACT_MODEL );

        @modIlluminatiGibs = RegisterModel( "models/objects/gibs/illuminati/illuminati1.md3" );
    }

    void PrecacheSounds() {
        @sfxItemRespawn = RegisterSound( S_ITEM_RESPAWN );
        @sfxPlayerRespawn = RegisterSound( S_PLAYER_RESPAWN );
        @sfxTeleportIn = RegisterSound( S_TELEPORT );
        @sfxTeleportOut = RegisterSound( S_TELEPORT );
        for( int i = 0; i < 4; i++ )
            @sfxWeaponHit[i] = RegisterSound( StringUtils::Format( S_WEAPON_HITS, i ) );
        @sfxWeaponKill = RegisterSound( S_WEAPON_KILL );
        @sfxWeaponHitTeam = RegisterSound( S_WEAPON_HIT_TEAM );        
        @sfxWeaponUp = RegisterSound( S_WEAPON_SWITCH );
        @sfxWeaponUpNoAmmo = RegisterSound( S_WEAPON_NOAMMO );
        @sfxQuadFireSound = RegisterSound( S_QUAD_FIRE );
        @sfxShellHit = RegisterSound( S_SHELL_HIT );
        @sfxWalljumpFailed = RegisterSound( "sounds/world/ft_walljump_failed" );

    	for( int i = 0; i < 2; i++ ) {
            @sfxGrenadeWeakBounce[i] = RegisterSound( StringUtils::Format( S_WEAPON_GRENADE_W_BOUNCE_1_to_2, i + 1 ) );
            @sfxGrenadeStrongBounce[i] = RegisterSound( StringUtils::Format( S_WEAPON_GRENADE_S_BOUNCE_1_to_2, i + 1 ) );
        }

        for( int i = 0; i < 2; i++ ) {
            @sfxRic[i] = RegisterSound( StringUtils::Format( "sounds/weapons/ric%s", i + 1 ) );
        }

	    // Gunblade sounds (weak is blade):
        for( int i = 0; i < 3; i++ ) 
            @sfxGunbladeWeakShot[i] = RegisterSound( StringUtils::Format( S_WEAPON_GUNBLADE_W_SHOT_1_to_3, i + 1 ) );
        for( int i = 0; i < 3; i++ ) 
            @sfxBladeFleshHit[i] = RegisterSound( StringUtils::Format( S_WEAPON_GUNBLADE_W_HIT_FLESH_1_to_3, i + 1 ) );
        for( int i = 0; i < 2; i++ )
            @sfxBladeWallHit[i] = RegisterSound( StringUtils::Format( S_WEAPON_GUNBLADE_W_HIT_WALL_1_to_2, i + 1 ) );
        @sfxGunbladeStrongShot = RegisterSound( S_WEAPON_GUNBLADE_S_SHOT );
        for( int i = 0; i < 3; i++ )
            @sfxGunbladeStrongHit[i] = RegisterSound( StringUtils::Format( S_WEAPON_GUNBLADE_S_HIT_1_to_2, i + 1 ) );

        // Plasmagun sounds
        @sfxPlasmaWeakHit = RegisterSound( S_WEAPON_PLASMAGUN_W_HIT );
        @sfxPlasmaStrongHit = RegisterSound( S_WEAPON_PLASMAGUN_S_HIT );

        // Rocket launcher sounds
        @sfxRocketLauncherWeakHit = RegisterSound( S_WEAPON_ROCKET_W_HIT );
        @sfxRocketLauncherStrongHit = RegisterSound( S_WEAPON_ROCKET_S_HIT );

        // Lasergun sounds
        @sfxLasergunWeakHum = RegisterSound( S_WEAPON_LASERGUN_W_HUM );
        @sfxLasergunWeakQuadHum = RegisterSound( S_WEAPON_LASERGUN_W_QUAD_HUM );
        @sfxLasergunWeakStop = RegisterSound( S_WEAPON_LASERGUN_W_STOP );
        @sfxLasergunStrongHum = RegisterSound( S_WEAPON_LASERGUN_S_HUM );
        @sfxLasergunStrongQuadHum = RegisterSound( S_WEAPON_LASERGUN_S_QUAD_HUM );
        @sfxLasergunStrongStop = RegisterSound( S_WEAPON_LASERGUN_S_STOP );
        @sfxLasergunHit[0] = RegisterSound( S_WEAPON_LASERGUN_HIT_0 );
        @sfxLasergunHit[1] = RegisterSound( S_WEAPON_LASERGUN_HIT_1 );
        @sfxLasergunHit[2] = RegisterSound( S_WEAPON_LASERGUN_HIT_2 );

        // Riotgun sounds
        @sfxRiotgunWeakHit = RegisterSound( S_WEAPON_RIOTGUN_W_HIT );
        @sfxRiotgunStrongHit = RegisterSound( S_WEAPON_RIOTGUN_S_HIT );

        // Electobolt sounds
        @sfxElectroboltHit = RegisterSound( S_WEAPON_ELECTROBOLT_HIT );

        //VSAY sounds
        @sfxVSaySounds[VSAY_GENERIC] = RegisterSound( S_CHAT );
        @sfxVSaySounds[VSAY_NEEDHEALTH] = RegisterSound( S_VSAY_NEEDHEALTH );
        @sfxVSaySounds[VSAY_NEEDWEAPON] = RegisterSound( S_VSAY_NEEDWEAPON );
        @sfxVSaySounds[VSAY_NEEDARMOR] = RegisterSound( S_VSAY_NEEDARMOR );
        @sfxVSaySounds[VSAY_AFFIRMATIVE] = RegisterSound( S_VSAY_AFFIRMATIVE );
        @sfxVSaySounds[VSAY_NEGATIVE] = RegisterSound( S_VSAY_NEGATIVE );
        @sfxVSaySounds[VSAY_YES] = RegisterSound( S_VSAY_YES );
        @sfxVSaySounds[VSAY_NO] = RegisterSound( S_VSAY_NO );
        @sfxVSaySounds[VSAY_ONDEFENSE] = RegisterSound( S_VSAY_ONDEFENSE );
        @sfxVSaySounds[VSAY_ONOFFENSE] = RegisterSound( S_VSAY_ONOFFENSE );
        @sfxVSaySounds[VSAY_OOPS] = RegisterSound( S_VSAY_OOPS );
        @sfxVSaySounds[VSAY_SORRY] = RegisterSound( S_VSAY_SORRY );
        @sfxVSaySounds[VSAY_THANKS] = RegisterSound( S_VSAY_THANKS );
        @sfxVSaySounds[VSAY_NOPROBLEM] = RegisterSound( S_VSAY_NOPROBLEM );
        @sfxVSaySounds[VSAY_YEEHAA] = RegisterSound( S_VSAY_YEEHAA );
        @sfxVSaySounds[VSAY_GOODGAME] = RegisterSound( S_VSAY_GOODGAME );
        @sfxVSaySounds[VSAY_DEFEND] = RegisterSound( S_VSAY_DEFEND );
        @sfxVSaySounds[VSAY_ATTACK] = RegisterSound( S_VSAY_ATTACK );
        @sfxVSaySounds[VSAY_NEEDBACKUP] = RegisterSound( S_VSAY_NEEDBACKUP );
        @sfxVSaySounds[VSAY_BOOO] = RegisterSound( S_VSAY_BOOO );
        @sfxVSaySounds[VSAY_NEEDDEFENSE] = RegisterSound( S_VSAY_NEEDDEFENSE );
        @sfxVSaySounds[VSAY_NEEDOFFENSE] = RegisterSound( S_VSAY_NEEDOFFENSE );
        @sfxVSaySounds[VSAY_NEEDHELP] = RegisterSound( S_VSAY_NEEDHELP );
        @sfxVSaySounds[VSAY_ROGER] = RegisterSound( S_VSAY_ROGER );
        @sfxVSaySounds[VSAY_ARMORFREE] = RegisterSound( S_VSAY_ARMORFREE );
        @sfxVSaySounds[VSAY_AREASECURED] = RegisterSound( S_VSAY_AREASECURED );
        @sfxVSaySounds[VSAY_BOOMSTICK] = RegisterSound( S_VSAY_BOOMSTICK );
        @sfxVSaySounds[VSAY_GOTOPOWERUP] = RegisterSound( S_VSAY_GOTOPOWERUP );
        @sfxVSaySounds[VSAY_GOTOQUAD] = RegisterSound( S_VSAY_GOTOQUAD );
        @sfxVSaySounds[VSAY_OK] = RegisterSound( S_VSAY_OK );
        @sfxVSaySounds[VSAY_DEFEND_A] = RegisterSound( S_VSAY_DEFEND_A );
        @sfxVSaySounds[VSAY_ATTACK_A] = RegisterSound( S_VSAY_ATTACK_A );
        @sfxVSaySounds[VSAY_DEFEND_B] = RegisterSound( S_VSAY_DEFEND_B );
        @sfxVSaySounds[VSAY_ATTACK_B] = RegisterSound( S_VSAY_ATTACK_B );        
    }
}

}
