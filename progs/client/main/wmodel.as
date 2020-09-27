namespace CGame {

enum eWeapAnim {
	WEAPANIM_NOANIM,
	WEAPANIM_STANDBY,
	WEAPANIM_ATTACK_WEAK,
	WEAPANIM_ATTACK_STRONG,
	WEAPANIM_WEAPDOWN,
	WEAPANIM_WEAPONUP,
	VWEAP_MAXANIMS
};

enum eWeapModel {
	WEAPMODEL_WEAPON,
	WEAPMODEL_EXPANSION,
	WEAPMODEL_FLASH,
	WEAPMODEL_HAND,
	WEAPMODEL_BARREL,
	WEAPMODEL_BARREL2,
	VWEAP_MAXPARTS
};

const uint WEAPONINFO_MAX_FIRE_SOUNDS = 4;
array<const String @> wmPartSufix = { "", "_expansion", "_flash", "_hand", "_barrel", "_barrel2" };

class WModelAnimSet {
	array<int> firstframe(VWEAP_MAXANIMS);
	array<int> lastframe(VWEAP_MAXANIMS);
	array<int> loopingframes(VWEAP_MAXANIMS);
	array<float> frametime(VWEAP_MAXANIMS);

    WModelAnimSet( const float defaultfps ) {
        // default wsw hand
        firstframe[WEAPANIM_STANDBY] = 0;
        lastframe[WEAPANIM_STANDBY] = 0;
        loopingframes[WEAPANIM_STANDBY] = 1;
        frametime[WEAPANIM_STANDBY] = 1000 / defaultfps;

        firstframe[WEAPANIM_ATTACK_WEAK] = 1; // attack animation (1-5)
        lastframe[WEAPANIM_ATTACK_WEAK] = 5;
        loopingframes[WEAPANIM_ATTACK_WEAK] = 0;
        frametime[WEAPANIM_ATTACK_WEAK] = 1000 / defaultfps;

        firstframe[WEAPANIM_ATTACK_STRONG] = 0;
        lastframe[WEAPANIM_ATTACK_STRONG] = 0;
        loopingframes[WEAPANIM_ATTACK_STRONG] = 1;
        frametime[WEAPANIM_ATTACK_STRONG] = 1000 / defaultfps;

        firstframe[WEAPANIM_WEAPDOWN] = 0;
        lastframe[WEAPANIM_WEAPDOWN] = 0;
        loopingframes[WEAPANIM_WEAPDOWN] = 1;
        frametime[WEAPANIM_WEAPDOWN] = 1000 / defaultfps;

        firstframe[WEAPANIM_WEAPONUP] = 6; // flipout animation (6-10)
        lastframe[WEAPANIM_WEAPONUP] = 10;
        loopingframes[WEAPANIM_WEAPONUP] = 1;
        frametime[WEAPANIM_WEAPONUP] = 1000 / defaultfps;
    }
}

class WModelInfo {
	array<ModelHandle @> model(VWEAP_MAXPARTS); // one weapon consists of several models
	array<ModelSkeleton @> skel(VWEAP_MAXPARTS);

    // barrel
	int barrelTime;
	array<float> barrelSpeed(VWEAP_MAXPARTS);

	// flash
	int flashTime;
	bool flashFade;
	float flashRadius;
	int flashColor;

	// handOffset
	Vec3 handOrigin;
	Vec3 handAngles;

    // sounds
    array<SoundHandle @> fireSounds;
    array<SoundHandle @> strongFireSounds;

	// ammo counter display
	float acDigitWidth, acDigitHeight;
	FontHandle @acFont;
	int acFontWidth;
	float acDigitAlpha;
	float acIconSize;
	float acIconAlpha;

    CGame::Scene::Orientation tag_projectionsource;

	WModelAnimSet animSet;

    bool updateRegistration( String fn ) {
        if( fn.empty() ) {
            return false;
        }

        fn = FilePath::StripExtension( fn );

        for( int p = 0; p < VWEAP_MAXPARTS; p++ ) {
            // iqm
            if( @model[p] is null ) {
                String scratch = fn + wmPartSufix[p] + ".iqm";
                @model[p] = CGame::RegisterModel( scratch );
            }

            // md3
            if( @model[p] is null ) {
                String scratch = fn + wmPartSufix[p] + ".md3";
                @model[p] = CGame::RegisterModel( scratch );
            }

            @skel[p] = null;
            if( ( p == WEAPMODEL_HAND ) && ( @model[p] !is null ) ) {
                @skel[p] = CGame::SkeletonForModel( model[p] );
            }
        }

        if( @model[WEAPMODEL_BARREL] is null ) {
            @model[WEAPMODEL_BARREL2] = null;
        }

        // load failed
        if( @model[WEAPMODEL_HAND] is null ) {
            for( int p = 0; p < VWEAP_MAXPARTS; p++ ) {
                @model[p] = null;
            }
            return false;
        }

        // load animation script for the hand model
        if( !loadAnimationScript( fn + ".cfg") )  {
            createDefaultAnimation();
        }

        // reuse the main barrel model for the second barrel if the later is not found on disk but
        // rotation speed is specified in the script
        if( barrelSpeed[WEAPMODEL_BARREL2] != 0 && @model[WEAPMODEL_BARREL2] is null ) {
           @model[WEAPMODEL_BARREL2] = @model[WEAPMODEL_BARREL];
        }

        // create a tag_projection from tag_flash, to position fire effects
        computeWeaponInfoTags();

        return true;
    }

	bool loadAnimationScript( const String &script ) {
        for( int i = 0; i < VWEAP_MAXPARTS; i++ ) {
            barrelSpeed[i] = 0;
        }

        flashTime = 0;
        flashRadius = 0;
        flashFade = true;

        fireSounds.resize( 0 );
        strongFireSounds.resize( 0  );

        handOrigin = Vec3();
        handAngles = Vec3();

        @acFont = null;

        WeaponModel @wmodel = CGame::LoadWeaponModel( script );
		if( @wmodel is null ) {
			return false;
		}

        if( wmodel.numAnims != VWEAP_MAXANIMS ) {
		    CGame::Print( StringUtils::Format( "%sERROR: incomplete WEAPON script: %s - Using default%s\n", S_COLOR_YELLOW, script, S_COLOR_WHITE ) );
            return false;
        }

        uint n;

        n = wmodel.getNumInfoLines( "barrel" );
        if( n > 0 ) {
            array<String @> @l = wmodel.getInfoLine( "barrel", n - 1 );
            if( l.length() > 0 ) {
                barrelTime = l[0].toInt();
            }
            if( l.length() > 1 ) {
                barrelSpeed[WEAPMODEL_BARREL] = l[1].toInt();
            }
            if( l.length() > 2 ) {
                barrelSpeed[WEAPMODEL_BARREL2] = l[2].toInt();
            }
        }

        n = wmodel.getNumInfoLines( "flash" );
        if( n > 0 ) {
            array<String @> @l = wmodel.getInfoLine( "flash", n - 1 );
            if( l.length() > 0 ) {
                flashTime = l[0].toInt();
            }
            if( l.length() > 1 ) {
                flashRadius = float( l[1].toInt() );
            }
            if( l.length() > 2 ) {
                if( l[2].tolower() == "no" )
                    flashFade = false;
            }
        }

        n = wmodel.getNumInfoLines( "flashColor" );
        if( n > 0 ) {
            int r = 0, g = 0, b = 0;

            array<String @> @l = wmodel.getInfoLine( "flashColor", n - 1 );
            if( l.length() > 0 ) {
                r = int( l[0].toFloat() * 255.0f );
            }
            if( l.length() > 1 ) {
                g = int( l[1].toFloat() * 255.0f );
            }
            if( l.length() > 2 ) {
                b = int( l[2].toFloat() * 255.0f );
            }

            flashColor = COLOR_RGB( r, g, b );
        }

        n = wmodel.getNumInfoLines( "handOffset" );
        if( n > 0 ) {
            int r = 0, g = 0, b = 0;

            array<String @> @l = wmodel.getInfoLine( "handOffset", n - 1 );
            if( l.length() >= 3 ) {
                for( int i = 0; i < 3; i++ )
                    handOrigin[i] = l[i].toFloat();
            }
            if( l.length() >= 6 ) {
                for( int i = 0; i < 3; i++ )
                    handAngles[i] = l[i+3].toFloat();
            }
        }

        n = wmodel.getNumInfoLines( "ammCounter" );
        if( n > 0 ) {
            array<String @> @l = wmodel.getInfoLine( "ammCounter", n - 1 );
            String @fontFamily;

            if( l.length() > 2 ) {
                @fontFamily = @l[0];
                int fontSize = l[1].toInt();
                @acFont = CGame::RegisterFont( fontFamily, QFONT_STYLE_NONE, fontSize );
            }

            if( @acFont !is null )
                acFontWidth = CGame::Screen::StringWidth( "0", @acFont, 0 );

            if( l.length >= 3 )
    			acDigitWidth = l[2].toFloat();
            if( l.length >= 4 )
    			acDigitHeight = l[3].toFloat();
            if( l.length >= 5 )
    			acDigitAlpha = l[4].toFloat();
            if( l.length >= 6 )
    			acIconSize = l[5].toFloat();
            if( l.length >= 7 )
    			acIconAlpha = l[6].toFloat();
        }

        array<const String @> soundCmds = { "firesound", "strongfiresound" };
        array<array<SoundHandle @> @> sounds = { @fireSounds, @strongFireSounds };
        for( uint i = 0; i < soundCmds.length(); i++ ) {
            n = wmodel.getNumInfoLines( soundCmds[i] );

            for( uint j = 0; j < n; j++ ) {
                array<String @> @l = wmodel.getInfoLine( soundCmds[i], j );
                if( l.length() == 0 )
                    continue;

                const String @s = @l[0];
                if( s.tolower() != "null" )
                    sounds[i].insertLast( CGame::RegisterSound( s ) );
            }
        }

		for( uint i = 0; i < wmodel.numAnims; i++ ) {
			int fps;
			wmodel.getAnim( i, animSet.firstframe[i], animSet.lastframe[i], animSet.loopingframes[i], fps );
			animSet.frametime[i] = 1000.0f / float( fps > 10 ? fps : 10 );
		}

        return true;
	}

    void createDefaultAnimation() {
        const float defaultfps = 15.0f;

        for( int i = 0; i < VWEAP_MAXPARTS; i++ ) {
            barrelSpeed[i] = 0;
        }

        // default wsw hand
        animSet = WModelAnimSet( defaultfps );
    }

    /*
    * Store the CGame::Scene::Orientation closer to the tag_flash we can create,
    * or create one using an offset we consider acceptable.
    *
    * NOTE: This tag will ignore weapon models animations. You'd have to
    * do it in realtime to use it with animations. Or be careful on not
    * moving the weapon too much
    */
    void computeWeaponInfoTags() {
        CGame::Scene::Orientation tag, tag_barrel, tag_barrel2;
        CGame::Scene::Entity ent;

        tag_projectionsource.origin.set( 16.0f, 0.0f, 8.0f );
        tag_projectionsource.axis.identity();

        if( @model[WEAPMODEL_WEAPON] is null ) {
            return;
        }

        // assign the model to an CGame::Scene::Entity, so we can build boneposes
        ent.rtype = RT_MODEL;
        @ent.model = @model[WEAPMODEL_WEAPON];
        @ent.boneposes = @cg.tempBoneposes; // assigns and builds the skeleton so we can use grabtag

        bool haveBarrel = false;
        if( @model[WEAPMODEL_BARREL] !is null && CGame::Scene::GrabTag( tag_barrel, @ent, "tag_barrel" ) ) {
            haveBarrel = true;
        }
        
        if( @model[WEAPMODEL_BARREL2] !is null ) {
            if( !haveBarrel || !CGame::Scene::GrabTag( tag_barrel2, @ent, "tag_barrel2" ) ) {
                @model[WEAPMODEL_BARREL2] = null;
            }
        }

        // try getting the tag_flash from the weapon model
        if( !CGame::Scene::GrabTag( tag_projectionsource, @ent, "tag_flash" ) && haveBarrel ) {
            // if it didn't work, try getting it from the barrel model
            // assign the model to an CGame::Scene::Entity, so we can build boneposes
            CGame::Scene::Entity ent_barrel;

            ent_barrel.rtype = RT_MODEL;
            @ent_barrel.model = @model[WEAPMODEL_BARREL];
            @ent_barrel.boneposes = @cg.tempBoneposes;

            if( CGame::Scene::GrabTag( tag, @ent_barrel, "tag_flash" ) ) {
                tag_projectionsource = CGame::Scene::MoveToTag( tag_barrel, tag );
            }
        }
    }
}

void AddWeaponFlashOnTag( CGame::Scene::Entity @weapon, WModelInfo @weaponInfo, int modelid, 
	const String @tag_flash, int effects, int64 flashTime ) {
	CGame::Scene::Orientation tag;
	CGame::Scene::Entity flash;

	if( flashTime < cg.time ) {
		return;
	}
	if( @weaponInfo.model[modelid] is null ) {
		return;
	}
	if( !CGame::Scene::GrabTag( tag, @weapon, tag_flash ) ) {
		return;
	}

    float intensity = 1.0f;
    uint8 c = 255;

	if( weaponInfo.flashFade ) {
		intensity = float( flashTime - cg.time ) / float( weaponInfo.flashTime );
		c = uint8( 255 * intensity );
	}

    flash.rtype = RT_MODEL;
    flash.shaderRGBA = COLOR_RGBA( c, c, c, c );
	@flash.model = @weaponInfo.model[modelid];
	flash.scale = weapon.scale;
	flash.renderfx = weapon.renderfx | RF_NOSHADOW;
	flash.frame = 0;
	flash.oldFrame = 0;
    flash.backLerp = weapon.backLerp;

	CGame::Scene::PlaceModelOnTag( @flash, @weapon, tag );

	if( ( effects & EF_RACEGHOST ) == 0 ) {
		CGame::Scene::AddEntityToScene( @flash );
	}

	CGame::Scene::AddLightToScene( flash.origin, weaponInfo.flashRadius * intensity,
		weaponInfo.flashColor );
}

void AddWeaponExpansionOnTag( CGame::Scene::Entity @weapon, WModelInfo @weaponInfo, int modelid, 
	const String @tag_expansion, int effects ) {
	CGame::Scene::Orientation tag;
	CGame::Scene::Entity expansion;

	if( @weaponInfo.model[modelid] is null ) {
		return;
	}
	if( !CGame::Scene::GrabTag( tag, @weapon, tag_expansion ) ) {
		return;
	}

    expansion.rtype = RT_MODEL;
    expansion.shaderRGBA = COLOR_REPLACEA( expansion.shaderRGBA, COLOR_A( weapon.shaderRGBA ) );
	@expansion.model = @weaponInfo.model[modelid];
	expansion.scale = weapon.scale;
	expansion.renderfx = weapon.renderfx;
	expansion.frame = 0;
	expansion.oldFrame = 0;
    expansion.backLerp = weapon.backLerp;

	CGame::Scene::PlaceModelOnTag( @expansion, @weapon, tag );
	
	if( ( effects & EF_RACEGHOST ) == 0 ) {
        CGame::Scene::AddEntityToScene( @expansion );
	}

	AddShellEffects( @expansion, effects );
}

void AddWeaponBarrelOnTag( CGame::Scene::Entity @weapon, WModelInfo @weaponInfo, int modelid, 
	const String @tag_barrel, const String @tag_recoil, int effects, int64 barrelTime ) {
	CGame::Scene::Orientation tag;
	Vec3 rotangles;
	CGame::Scene::Entity barrel;

	if( @weaponInfo.model[modelid] is null ) {
		return;
	}
	if( !CGame::Scene::GrabTag( tag, @weapon, tag_barrel ) ) {
		return;
	}

    barrel.rtype = RT_MODEL;
    barrel.shaderRGBA = COLOR_REPLACEA( barrel.shaderRGBA, COLOR_A( weapon.shaderRGBA ) );
	@barrel.model = @weaponInfo.model[modelid];
	barrel.scale = weapon.scale;
	barrel.renderfx = weapon.renderfx;
	barrel.frame = 0;
	barrel.oldFrame = 0;
    barrel.backLerp = weapon.backLerp;

	// rotation
	if( barrelTime > cg.time ) {
		CGame::Scene::Orientation recoil;

		float intensity = float( barrelTime - cg.time ) / float( weaponInfo.barrelTime );
		rotangles[2] = anglemod( 360.0f * weaponInfo.barrelSpeed[modelid] * intensity * intensity );

		// Check for tag_recoil
		if( CGame::Scene::GrabTag( recoil, @weapon, tag_recoil ) ) {
            tag.origin = tag.origin + intensity * (recoil.origin - tag.origin);
		}
	}

    rotangles.anglesToAxis( barrel.axis );
    barrel.origin = weapon.origin;

	// barrel requires special tagging
	CGame::Scene::PlaceRotatedModelOnTag( @barrel, @weapon, tag );

	if( ( effects & EF_RACEGHOST ) == 0 ) {
		CGame::Scene::AddEntityToScene( @barrel );
    }

	AddShellEffects( @barrel, effects );
}

/*
* Add weapon model(s) positioned at the tag
*
* @param ammo_count Current ammo count for the counter. Negative value skips rendering of the counter. 
*/
CGame::Scene::Orientation AddWeaponOnTag( CGame::Scene::Entity @ent, CGame::Scene::Orientation &in tag, int weaponid, int effects, 
	int64 flashTime, int64 barrelTime, int ammoCount ) {
    CGame::Scene::Orientation projectionSource;

	// don't try without base model or tag
	if( ent.model is null ) {
		return projectionSource;
	}

    if( weaponid <= WEAP_NONE || weaponid >= WEAP_TOTAL ) {
        return projectionSource;
    }

	auto @weaponItem = GS::FindItemByTag( weaponid );
	if( @weaponItem is null ) {
		return projectionSource;
	}

	auto @weaponInfo = cgs.weaponModelInfo[weaponid];
    if( @weaponInfo is null ) {
        return projectionSource;
    }

    CGame::Scene::Entity weapon;
    weapon.shaderRGBA = COLOR_REPLACEA( weapon.shaderRGBA, COLOR_A( ent.shaderRGBA ) );
    weapon.scale = ent.scale;
	weapon.renderfx = ent.renderfx;
	weapon.frame = 0;
	weapon.oldFrame = 0;
    weapon.backLerp = ent.backLerp;
	@weapon.model = @weaponInfo.model[WEAPMODEL_WEAPON];

	CGame::Scene::PlaceModelOnTag( @weapon, @ent, tag );

	if( ( effects & EF_RACEGHOST ) == 0 ) {
		CGame::Scene::AddEntityToScene( @weapon );
	}

	if( @weapon.model is null ) {
		return projectionSource;
	}

	AddShellEffects( @weapon, effects );

	// update projection source
    projectionSource.origin = weapon.origin;
    projectionSource.axis = weapon.axis;
	projectionSource = CGame::Scene::MoveToTag( projectionSource, weaponInfo.tag_projectionsource );

	// expansion
	if( ( effects & EF_STRONG_WEAPON ) != 0 ) {
		AddWeaponExpansionOnTag( @weapon, @weaponInfo, WEAPMODEL_EXPANSION, "tag_expansion", effects );
	}

	// barrels
	AddWeaponBarrelOnTag( @weapon, @weaponInfo, WEAPMODEL_BARREL, "tag_barrel", "tag_recoil", effects, barrelTime );
	AddWeaponBarrelOnTag( @weapon, @weaponInfo, WEAPMODEL_BARREL2, "tag_barrel2", "tag_recoil2", effects, barrelTime );

	// flash
	AddWeaponFlashOnTag( @weapon, @weaponInfo, WEAPMODEL_FLASH, "tag_flash", effects, flashTime );

/*
	auto @ammoItem = GS::FindItemByTag( weaponItem.ammo_tag );
	// ammo counter
	if( ammo_count >= 0 ) {
		CG_AddAmmoDigitOnTag( &weapon, weaponInfo, ammoItem, ammo_count % 10, "tag_ammo_digit_1" );
		CG_AddAmmoDigitOnTag( &weapon, weaponInfo, ammoItem, (ammo_count % 100) / 10, "tag_ammo_digit_10" );
		CG_AddAmmoDigitOnTag( &weapon, weaponInfo, ammoItem, ammo_count / 100, "tag_ammo_digit_100" );
	}

	// icons
	CG_AddItemIconOnTag( &weapon, weaponInfo, weaponItem, "tag_weapon_icon" );
	CG_AddItemIconOnTag( &weapon, weaponInfo, ammoItem, "tag_ammo_icon" );
*/

    return projectionSource;
}

void RegisterWeaponModels( void ) {
	// special case for weapon 0. Must always load the animation script
	@cgs.weaponModelInfo[WEAP_NONE] = WModelInfo();
    cgs.weaponModelInfo[WEAP_NONE].updateRegistration( "generic/generic.md3" );

	for( int i = WEAP_NONE+1; i < WEAP_TOTAL; i++ ) {
		@cgs.weaponModelInfo[i] = @cgs.weaponModelInfo[WEAP_NONE];

		auto @item = GS::FindItemByTag( i );
		if( @item !is null ) {
            @cgs.weaponModelInfo[i] = WModelInfo();
            cgs.weaponModelInfo[i].updateRegistration( item.getWorldModel(0) );
		}
	}
}

}
