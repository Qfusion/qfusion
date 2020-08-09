namespace CGame {

class CEntity {
	EntityState current;
	EntityState prev;
	int type;
	int effects;
	int renderfx;
	Item @item;
	int64 serverFrame;
	CGame::Scene::Entity refEnt;
	ModelSkeleton @skel;

	Vec3 velocity;
	Vec3 prevVelocity;

	bool canExtrapolate;
	bool canExtrapolatePrev;
	bool linearProjectileCanDraw;

	int microSmooth;
	Vec3 microSmoothOrigin;
	Vec3 microSmoothOrigin2;

	Vec3 trailOrigin;
	int respawnTime;
}

array< CEntity > cgEnts( MAX_EDICTS );

const int ITEM_RESPAWN_TIME = 1000;

void NewPacketEntityState( const EntityState @state ) {
	CEntity @cent = cgEnts[state.number];

	if( GS::IsEventEntity( state ) ) {
		cent.prev = cent.current;
		cent.current = state;
		cent.serverFrame = CGame::Snap.serverFrame;
		cent.canExtrapolatePrev = false;
		cent.prevVelocity.clear();
	} else if( state.linearMovement ) {
		// if teleported or the trajectory type has changed, force nolerp
		if( state.teleported ||	state.linearMovement != cent.current.linearMovement 
			|| state.linearMovementTimeStamp != cent.current.linearMovementTimeStamp ) {
			cent.serverFrame = -99;
		}

		if( cent.serverFrame != CGame::OldSnap.serverFrame ) {
			// wasn't in the last update
			cent.prev = state;
		} else {
			cent.prev = cent.current;
		}
		cent.current = state;
		cent.serverFrame = CGame::Snap.serverFrame;

		cent.canExtrapolate = false;
		cent.canExtrapolatePrev = false;
		cent.linearProjectileCanDraw = UpdateLinearProjectilePosition( cent );

		cent.velocity = cent.prevVelocity = state.linearMovementVelocity;
		cent.trailOrigin = state.origin;
	} else {
		// if it moved too much force no lerping
		if(  abs( int( cent.current.origin[0] - state.origin[0] ) ) > 512
			 || abs( int( cent.current.origin[1] - state.origin[1] ) ) > 512
			 || abs( int( cent.current.origin[2] - state.origin[2] ) ) > 512 ) {
			cent.serverFrame = -99;
		}

		// some data changes will force no lerping as well
		if( state.modelindex != cent.current.modelindex
			|| state.teleported 
			|| state.linearMovement != cent.current.linearMovement ) {
			cent.serverFrame = -99;
		}

		if( cent.serverFrame != CGame::OldSnap.serverFrame ) {
			// wasn't in last update, so initialize some things
			// duplicate the current state so lerping doesn't hurt anything
			cent.prev = state;
			cent.microSmooth = 0;
		} else {
			// shuffle the last state to previous
			cent.prev = cent.current;
		}

		cent.current = state;
		cent.trailOrigin = state.origin;
		cent.prevVelocity = cent.velocity;

		cent.canExtrapolatePrev = cent.canExtrapolate;
		cent.canExtrapolate = false;
		cent.velocity.clear();
		cent.serverFrame = CGame::Snap.serverFrame;

		// set up velocities for this CEntity
		if( CGame::ExtrapolationTime > 0 &&
			( state.type == ET_PLAYER || state.type == ET_CORPSE ) ) {
			cent.velocity = cent.current.origin2;
			cent.prevVelocity = cent.prev.origin;
			cent.canExtrapolate = cent.canExtrapolatePrev = true;
		} else if( cent.prev.origin != state.origin ) {
			int snapTime = int( CGame::Snap.serverTime - CGame::OldSnap.serverTime );

			if( snapTime < 1 ) {
				snapTime = CGame::SnapFrameTime;
			}
			float scale = 1000.0f / float( snapTime );

			cent.velocity = ( cent.current.origin - cent.prev.origin ) * scale;
		}

		if( ( state.type == ET_GENERIC || state.type == ET_PLAYER
			  || state.type == ET_GIB || state.type == ET_GRENADE
			  || state.type == ET_ITEM || state.type == ET_CORPSE ) ) {
			cent.canExtrapolate = true;
		}

		if( GS::IsBrushModel( state.modelindex ) ) {
			// disable extrapolation on movers
			cent.canExtrapolate = false;
		}
	}
}

const float MIN_DRAWDISTANCE_FIRSTPERSON = 86;
const float MIN_DRAWDISTANCE_THIRDPERSON = 52;

bool UpdateLinearProjectilePosition( CEntity @cent ) {
	Vec3 origin;
	EntityState @state;
	int moveTime;
	int64 serverTime;

	@state = @cent.current;

	if( !state.linearMovement ) {
		return true;
	}

	if( /*GS::MatchPaused()*/false ) {
		serverTime = CGame::Snap.serverTime;
	} else {
		serverTime = cg.time + cg.extrapolationTime;
	}

	if( state.solid != SOLID_BMODEL ) {
		// add a time offset to counter antilag visualization
		if( !cgs.demoPlaying && cg_projectileAntilagOffset.value > 0.0f &&
			!IsViewerEntity( state.ownerNum ) && ( cgs.playerNum + 1 != CGame::PredictedPlayerState.POVnum ) ) {
			serverTime += int64( float( state.modelindex2 ) * cg_projectileAntilagOffset.value );
		}
	}

	moveTime = GS::LinearMovement( state, serverTime, origin );
	state.origin = origin;

	if( ( moveTime < 0 ) && ( state.solid != SOLID_BMODEL ) ) {
		// when flyTime is negative don't offset it backwards more than PROJECTILE_PRESTEP value
		// FIXME: is this still valid?
		float maxBackOffset;

		if( IsViewerEntity( state.ownerNum ) ) {
			maxBackOffset = ( PROJECTILE_PRESTEP - MIN_DRAWDISTANCE_FIRSTPERSON );
		} else {
			maxBackOffset = ( PROJECTILE_PRESTEP - MIN_DRAWDISTANCE_THIRDPERSON );
		}

		if( state.origin2.distance( state.origin ) > maxBackOffset ) {
			return false;
		}
	}

	return true;
}

void EntityLoopSound( EntityState @state, float attenuation ) {
	if( state.sound == 0 ) {
		return;
	}
	CGame::Sound::AddLoopSound( cgs.soundPrecache[state.sound], state.number, cg_volume_effects.value,
		IsViewerEntity( state.number ) ? attenuation : ATTN_IDLE );
}

bool GetEntitySpatilization( int entNum, Vec3 &out origin, Vec3 &out velocity ) {
	if( entNum < -1 || entNum >= MAX_EDICTS ) {
		GS::Error( "GetEntitySoundOrigin: bad entnum" );
		return false;
	}

	// hack for client side floatcam
	if( entNum == -1 ) {
		origin = CGame::Snap.playerState.pmove.origin;
		velocity = CGame::Snap.playerState.pmove.velocity;
		return false;
	}

	CEntity @cent = cgEnts[entNum];

	// normal
	if( cent.current.solid != SOLID_BMODEL ) {
		origin = cent.refEnt.origin;
		velocity = cent.velocity;
		return false;
	}

	// bmodel
	Vec3 mins, maxs;
	CModelHandle @cmodel = GS::InlineModel( cent.current.modelindex );
	GS::InlineModelBounds( cmodel, mins, maxs );
	origin = cent.refEnt.origin + 0.5 * (mins + maxs);
	velocity = cent.velocity;
	return true;
}

void AddLinkedModel( CEntity @cent ) {
	bool barrel;
	CGame::Scene::Entity ent;
	CGame::Scene::Orientation tag;
	ModelHandle @model;

	if( cent.current.modelindex2 == 0 )
		return;

	// linear projectiles can never have a linked model. Modelindex2 is used for a different purpose
	if( cent.current.linearMovement )
		return;

	@model = cgs.modelDraw[cent.current.modelindex2];
	if( @model == null )
		return;

	ent.rtype = RT_MODEL;
	ent.scale = cent.refEnt.scale;
	ent.renderfx = cent.refEnt.renderfx;
	ent.shaderTime = cent.refEnt.shaderTime;
	ent.shaderRGBA = cent.refEnt.shaderRGBA;
	@ent.model = model;
	ent.origin = cent.refEnt.origin;
	ent.origin2 = cent.refEnt.origin2;
	ent.lightingOrigin = cent.refEnt.lightingOrigin;
	ent.axis = cent.refEnt.axis;

	if( @cent.item != null && ( cent.effects & EF_AMMOBOX ) != 0 ) { // ammobox icon hack
		@ent.customShader = CGame::RegisterShader( cent.item.icon );
	}

	barrel = false;
	if( @cent.item != null && ( cent.item.type & IT_WEAPON ) != 0 ) {
		if( CGame::Scene::GrabTag( tag, @cent.refEnt, "tag_barrel" ) ) {
			barrel = true;
			CGame::Scene::PlaceModelOnTag( @ent, @cent.refEnt, tag );
		}
	} else {
		if( CGame::Scene::GrabTag( tag, @cent.refEnt, "tag_linked" ) ) {
			CGame::Scene::PlaceModelOnTag( @ent, @cent.refEnt, tag );
		}
	}

	CGame::Scene::AddEntityToScene( @ent );
	//CG_AddShellEffects( &ent, cent->effects );

	if( barrel && CGame::Scene::GrabTag( tag, @cent.refEnt, "tag_barrel2" ) ) {
		CGame::Scene::PlaceModelOnTag( @ent, @cent.refEnt, tag );
		CGame::Scene::AddEntityToScene( @ent );
		//CG_AddShellEffects( &ent, cent->effects );
	}
}

void EntAddBobEffect( CEntity @cent ) {
	double scale;
	double bob;

	scale = 0.005 + cent.current.number * 0.00001;
	bob = 4 + cos( ( cg.time + 1000 ) * scale ) * 4;

	cent.refEnt.origin2.z += bob;
	cent.refEnt.origin.z += bob;
	cent.refEnt.lightingOrigin.z += bob;
}

void EntAddTeamColorTransitionEffect( CEntity @cent ) {
	int currentcolor;
	int scaledcolor, newcolor;
	const Vec4 ac (1.0, 1.0, 1.0, 1.0);

	float f = bound( 0.0f, float( cent.current.counterNum ) / 255.0f, 1.0f );

	if( cent.current.type == ET_PLAYER || cent.current.type == ET_CORPSE ) {
		currentcolor = PlayerColorForEntity( cent.current.number );
	} else {
		currentcolor = TeamColorForEntity( cent.current.number );
	}

	Vec4 cv = ColorToVec4( currentcolor );
	int nc = Vec4ToColor( ac + f * (cv - ac) );
	cent.refEnt.shaderRGBA = COLOR_REPLACEA( nc, COLOR_A( cent.refEnt.shaderRGBA ) );
}

void UpdateGenericEnt( CEntity @cent ) {
	cent.refEnt.reset();
	cent.refEnt.shaderRGBA = TeamColorForEntity( cent.current.number );

	cent.refEnt.frame = cent.current.frame;
	cent.refEnt.oldFrame = cent.prev.frame;
	cent.refEnt.rtype = RT_MODEL;

	int modelindex = cent.current.modelindex;
	if( modelindex > 0 && modelindex < MAX_MODELS ) {
		@cent.refEnt.model = @cgs.modelDraw[modelindex];
	}

	@cent.skel = SkeletonForModel( cent.refEnt.model );
}

void UpdateItemEnt( CEntity @cent ) {
	cent.refEnt.reset();

	@cent.item = GS::FindItemByTag( cent.current.itemNum );
	if( @cent.item is null ) {
		return;
	}

	cent.effects |= cent.item.effects;

	if( cg_simpleItems.boolean && !cent.item.simpleIcon.empty() ) {
		cent.refEnt.rtype = RT_SPRITE;
		@cent.refEnt.model = null;
		@cent.skel = null;
		cent.refEnt.renderfx = RF_NOSHADOW | RF_FULLBRIGHT;
		cent.refEnt.frame = cent.refEnt.oldFrame = 0;

		cent.refEnt.radius = cg_simpleItemsSize.value <= 32 ? cg_simpleItemsSize.value : 32;
		if( cent.refEnt.radius < 1.0f ) {
			cent.refEnt.radius = 1.0f;
		}

		if( cg_simpleItems.integer == 2 ) {
			cent.effects &= ~EF_ROTATE_AND_BOB;
		}

		@cent.refEnt.customShader = CGame::RegisterShader( cent.item.simpleIcon );
	} else {
		cent.refEnt.rtype = RT_MODEL;
		cent.refEnt.frame = cent.current.frame;
		cent.refEnt.oldFrame = cent.prev.frame;

		// set up the model
		@cent.refEnt.model = cgs.modelDraw[cent.current.modelindex];
		@cent.skel = CGame::SkeletonForModel( cent.refEnt.model );
	}
}

void ExtrapolateLinearProjectile( CEntity @cent ) {
	int i;

	cent.linearProjectileCanDraw = UpdateLinearProjectilePosition( cent );

	cent.refEnt.backLerp = 1.0f;

	cent.refEnt.origin = cent.current.origin;
	cent.refEnt.origin2 = cent.refEnt.origin;
	cent.refEnt.lightingOrigin = cent.refEnt.origin;
	cent.current.angles.anglesToAxis( cent.refEnt.axis );
}

void LerpGenericEnt( CEntity @cent ) {
	int POVent = CGame::Camera::GetMainCamera().POVent;
	Vec3 ent_angles;

	cent.refEnt.backLerp = 1.0f - cg.lerpfrac;

	if( CGame::IsViewerEntity( cent.current.number ) || POVent == cent.current.number ) {
		ent_angles = CGame::PredictedPlayerState.viewAngles;
	} else {
		// interpolate angles
		ent_angles = LerpAngles( cent.prev.angles, cent.current.angles, cg.lerpfrac );
	}

	if( ent_angles.x != 0.0f || ent_angles.y != 0.0f || ent_angles.z != 0.0f ) {
		ent_angles.anglesToAxis( cent.refEnt.axis );
	} else {
		cent.refEnt.axis.identity();
	}

	if( ( cent.renderfx & RF_FRAMELERP ) != 0 ) {
		// step origin discretely, because the frames
		// do the animation properly
		Vec3 delta, move;

		// FIXME: does this still work?
		move = cent.current.origin2 - cent.current.origin;
		delta = cent.refEnt.axis * move;
		cent.refEnt.origin = cent.current.origin + cent.refEnt.backLerp * delta;
	} else if( CGame::IsViewerEntity( cent.current.number ) || POVent == cent.current.number ) {
		cent.refEnt.origin = CGame::PredictedPlayerState.pmove.origin;
		cent.refEnt.origin2 = cent.refEnt.origin;
	} else {
		if( cg.extrapolationTime != 0 && cent.canExtrapolate ) { // extrapolation
			Vec3 origin, xorigin1, xorigin2;

			float lerpfrac = cg.lerpfrac;
			if( lerpfrac < 0.0f ) lerpfrac = 0.0f;
			if( lerpfrac > 1.0f ) lerpfrac = 1.0f;

			// extrapolation with half-snapshot smoothing
			if( cg.xerpTime >= 0 || !cent.canExtrapolatePrev ) {
				xorigin1 = cent.current.origin + cg.xerpTime * cent.velocity;
			} else {
				xorigin1 = cent.current.origin + cg.xerpTime * cent.velocity;

				if( cent.canExtrapolatePrev ) {
					Vec3 oldPosition = cent.prev.origin + cg.oldXerpTime * cent.prevVelocity;
					xorigin1 = oldPosition + cg.xerpSmoothFrac * (xorigin1 - oldPosition);
				}
			}

			// extrapolation with full-snapshot smoothing
			xorigin2 = cent.current.origin + cg.xerpTime * cent.velocity;
			if( cent.canExtrapolatePrev ) {
				Vec3 oldPosition = cent.prev.origin + cg.oldXerpTime * cent.prevVelocity;
				xorigin2 = oldPosition + lerpfrac * (xorigin2 - oldPosition);
			}

			origin = xorigin1 + 0.5f * (xorigin2 - xorigin1);

			/*
			// Interpolation between 2 extrapolated positions
			if( !cent.canExtrapolatePrev )
			    VectorMA( cent.current.origin, cg.xerpTime, cent.velocity, xorigin2 );
			else
			{
			    float frac = cg.lerpfrac;
			    clamp( frac, 0.0f, 1.0f );
			    VectorLerp( cent.prevExtrapolatedOrigin, frac, cent.extrapolatedOrigin, xorigin2 );
			}
			*/

			if( cent.microSmooth == 2 ) {
				Vec3 oldsmoothorigin = cent.microSmoothOrigin2 + 0.65f * (cent.microSmoothOrigin - cent.microSmoothOrigin2);
				cent.refEnt.origin = origin + 0.5f * (oldsmoothorigin - origin);
			} else if( cent.microSmooth == 1 ) {
				cent.refEnt.origin = origin + 0.5f * (cent.microSmoothOrigin - origin);
			} else {
				cent.refEnt.origin = origin;
			}

			if( cent.microSmooth != 0 ) {
				cent.microSmoothOrigin2 = cent.microSmoothOrigin;
			}

			cent.microSmoothOrigin = origin;
			cent.microSmooth++;
			if( cent.microSmooth > 2 ) cent.microSmooth = 2;

			cent.refEnt.origin2 = cent.refEnt.origin;
		} else {   // plain interpolation
			cent.refEnt.origin = cent.prev.origin + cg.lerpfrac * (cent.current.origin - cent.prev.origin);
			cent.refEnt.origin2 = cent.refEnt.origin;
		}
	}

	cent.refEnt.lightingOrigin = cent.refEnt.origin;
}

void AddGenericEnt( CEntity @cent ) {
	if( cent.refEnt.scale <= 0.001f ) {
		return;
	}

	// if set to invisible, skip
	if( cent.current.modelindex == 0 && ( cent.effects & EF_FLAG_TRAIL ) == 0 ) {
		return;
	}

	// bobbing & auto-rotation
	if( ( cent.effects & EF_ROTATE_AND_BOB ) != 0 ) {
		EntAddBobEffect( cent );
		cent.refEnt.axis = cg.autorotateAxis;
	}

	if( ( cent.effects & EF_TEAMCOLOR_TRANSITION ) != 0 ) {
		EntAddTeamColorTransitionEffect( cent );
	}

	// render effects
	cent.refEnt.renderfx = cent.renderfx;

	Item @item = cent.item;
	if( @item != null ) {
		if( ( item.type & ( IT_HEALTH | IT_POWERUP ) ) != 0 ) {
			cent.refEnt.renderfx |= RF_NOSHADOW;
		}

		if( ( cent.effects & EF_AMMOBOX ) != 0 ) {
			// Ugly hack for the release. Armor models are way too big
			cent.refEnt.scale *= 0.9f;

			// find out the ammo box color
			auto @colorToken = @item.colorToken;
			if( colorToken.length() > 1 ) {
				cent.refEnt.shaderRGBA = ColorByIndex( ColorIndex( colorToken[1] ) );
			} else {   // set white
				cent.refEnt.shaderRGBA = COLOR_RGBA( 255, 255, 255, 255 );
			}
		}

		if( ( cent.effects & EF_GHOST ) != 0 ) {
			cent.refEnt.renderfx |= RF_ALPHAHACK | RF_GREYSCALE;
			cent.refEnt.shaderRGBA = COLOR_REPLACEA( cent.refEnt.shaderRGBA, 100 );
		} else {
			cent.refEnt.shaderRGBA = COLOR_REPLACEA( cent.refEnt.shaderRGBA, 255 );
		}

		// add shadows for items (do it before offseting for weapons)
		if( !cg_shadows.boolean || ( cent.renderfx & RF_NOSHADOW ) != 0 ) {
			cent.refEnt.renderfx |= RF_NOSHADOW;
		} else if( cg_shadows.integer == 1 ) {
			//CG_AllocShadeBox( cent.current.number, cent.refEnt.origin, item_box_mins, item_box_maxs, NULL );
			cent.refEnt.renderfx |= RF_NOSHADOW;
		}

		cent.refEnt.renderfx |= RF_MINLIGHT;

		// offset weapon items by their special tag
		if( ( cent.item.type & IT_WEAPON ) != 0 ) {
			CGame::Scene::PlaceModelOnTag( @cent.refEnt, @cent.refEnt, cgs.weaponItemTag );
		}
	} else {
		if( cent.current.solid != SOLID_BMODEL ) {
			cent.refEnt.renderfx |= RF_NOSHADOW;
		}
	}

	if( @cent.skel != null ) {
		// get space in cache, interpolate, transform, link
		@cent.refEnt.boneposes = CGame::Scene::RegisterTemporaryExternalBoneposes( @cent.skel );
		@cent.refEnt.oldBoneposes = @cent.refEnt.boneposes;
		CGame::Scene::LerpSkeletonPoses( @cent.skel, cent.refEnt.frame, cent.refEnt.oldFrame, @cent.refEnt.boneposes, 1.0 - cent.refEnt.backLerp );
		CGame::Scene::TransformBoneposes( @cent.skel, @cent.refEnt.boneposes, @cent.refEnt.boneposes );
	}

	// flags are special
	if( ( cent.effects & EF_FLAG_TRAIL ) != 0 ) {
		//CG_AddFlagModelOnTag( cent, cent.refEnt.shaderRGBA, "tag_linked" );
	}

	if( cent.current.modelindex == 0 ) {
		return;
	}

	CGame::Scene::AddEntityToScene( @cent.refEnt );

	AddLinkedModel( @cent );
}

void AddItemEnt( CEntity @cent ) {
	int msec;
	Item @item = @cent.item;

	if( @item == null ) {
		return;
	}

	// respawning items
	if( cent.respawnTime != 0 ) {
		msec = cg.time - cent.respawnTime;
	} else {
		msec = ITEM_RESPAWN_TIME;
	}

	if( msec >= 0 && msec < ITEM_RESPAWN_TIME ) {
		cent.refEnt.scale = float( msec ) / ITEM_RESPAWN_TIME;
	} else {
		cent.refEnt.scale = 1.0f;
	}

	if( cent.refEnt.rtype != RT_SPRITE ) {
		// weapons are special
		if( ( item.type & IT_WEAPON ) != 0) {
			cent.refEnt.scale *= 1.40f;
		}

		// Ugly hack for release. Armor models are way too big
		if( ( item.type & IT_ARMOR ) != 0 ) {
			cent.refEnt.scale *= 0.85f;
		}
		if( item.tag == HEALTH_SMALL ) {
			cent.refEnt.scale *= 0.85f;
		}

		// flags are special
		if( ( cent.effects & EF_FLAG_TRAIL ) != 0 ) {
			//CG_AddFlagModelOnTag( cent, cent.refEnt.shaderRGBA, NULL );
			return;
		}

		AddGenericEnt( @cent );
		return;
	} else {
		if( ( cent.effects & EF_GHOST ) != 0 ) {
			cent.refEnt.shaderRGBA = COLOR_REPLACEA( cent.refEnt.shaderRGBA, 100 );
			cent.refEnt.renderfx |= RF_GREYSCALE;
		}
	}

	// offset the item origin up
	cent.refEnt.origin.z += cent.refEnt.radius + 2;
	cent.refEnt.origin2.z += cent.refEnt.radius + 2;
	if( ( cent.effects & EF_ROTATE_AND_BOB ) != 0 ) {
		EntAddBobEffect( @cent );
	}

	cent.refEnt.axis.identity();
	CGame::Scene::AddEntityToScene( @cent.refEnt );
}

void UpdateEntities() {
	for( int i = 0; i < CGame::Snap.numEntities; i++ ) {
		auto @state = CGame::Snap.getEntityState( i );
		CEntity @cent = cgEnts[state.number];
		cent.type = state.type;
		cent.effects = state.effects;
		@cent.item = null;
		cent.renderfx = 0;

		switch( cent.type ) {
			case ET_GENERIC:
				UpdateGenericEnt( @cent );
				break;

			// projectiles with linear trajectories
			case ET_BLASTER:
			case ET_ELECTRO_WEAK:
			case ET_ROCKET:
			case ET_PLASMA:
			case ET_GRENADE:
				cent.renderfx |= ( RF_NOSHADOW | RF_FULLBRIGHT );
				UpdateGenericEnt( @cent );
				break;

			case ET_ITEM:
				UpdateItemEnt( @cent );
				break;
		}
	}
}

/*
* LerpEntities
* Interpolate the entity states positions into the entity_t structs
*/
void LerpEntities( void ) {
	for( int i = 0; i < CGame::Snap.numEntities; i++ ) {
		auto @state = CGame::Snap.getEntityState( i );
		int number = state.number;
		CEntity @cent = cgEnts[number];

		switch( cent.type ) {
			case ET_GENERIC:
			case ET_GIB:
			case ET_BLASTER:
			case ET_ELECTRO_WEAK:
			case ET_ROCKET:
			case ET_PLASMA:
			case ET_GRENADE:
			case ET_ITEM:
			case ET_PLAYER:
			case ET_CORPSE:
			case ET_FLAG_BASE:
			case ET_MONSTER_PLAYER:
			case ET_MONSTER_CORPSE:
				if( state.linearMovement ) {
					ExtrapolateLinearProjectile( cent );
				} else {
					LerpGenericEnt( cent );
				}
				break;
			default:
				break;
		}

		Vec3 origin, velocity;
		GetEntitySpatilization( number, origin, velocity );
		CGame::Sound::SetEntitySpatilization( number, origin, velocity );
	}
}

bool AddEntity( int entNum )
{
	CEntity @cent = cgEnts[entNum];
	EntityState @state = @cent.current;

	switch( cent.type ) {
		case ET_GENERIC:
			AddGenericEnt( @cent );
			EntityLoopSound( state, ATTN_STATIC );
			return true;
		case ET_ITEM:
			AddItemEnt( @cent );
			EntityLoopSound( state, ATTN_IDLE );
			return true;
		case ET_PLASMA:
			AddGenericEnt( @cent );
			EntityLoopSound( state, ATTN_STATIC );
			return true;
		default:
			return false;
	}
	return false;
}

}
