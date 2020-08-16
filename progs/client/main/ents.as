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
	Vec3 mins;
	Vec3 maxs;

	Vec3 velocity;
	Vec3 prevVelocity;

	bool canExtrapolate;
	bool canExtrapolatePrev;
	bool linearProjectileCanDraw;

	int microSmooth;
	Vec3 microSmoothOrigin;
	Vec3 microSmoothOrigin2;

	Vec3 trailOrigin;
	int64 respawnTime;
	int64 flyStopTime;
}

array< CEntity > cgEnts( MAX_EDICTS );

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

	if( state.solid == SOLID_BMODEL ) {	
		GS::InlineModelBounds( GS::InlineModel( state.modelindex ), cent.mins, cent.maxs );
	} else if( state.solid != 0 ) {
		int x = 8 * ( state.solid & 31 );
		int zd = 8 * ( ( state.solid >> 5 ) & 31 );
		int zu = 8 * ( ( state.solid >> 10 ) & 63 ) - 32;

		cent.mins[0] = cent.mins[1] = -x;
		cent.maxs[0] = cent.maxs[1] = x;
		cent.mins[2] = -zd;
		cent.maxs[2] = zu;
	} else {
		cent.mins = Vec3(0.0f, 0.0f, 0.0f);
		cent.maxs = Vec3(0.0f, 0.0f, 0.0f);
	}
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

void DrawEntityBox( CEntity @cent ) {
	if( !cg_drawEntityBoxes.boolean )
		return;
	if( ( cent.refEnt.renderfx & RF_VIEWERMODEL ) != 0 )
		return;
	if( cent.current.solid == 0 )
		return;
	if( cg_drawEntityBoxes.integer < 2 && cent.current.solid == SOLID_BMODEL )
		return;

	Vec3 origin = cent.current.origin;
	Vec3 mins = cent.mins, maxs = cent.maxs;

	// push triggers don't move so aren't interpolated
	if( cent.current.type != ET_PUSH_TRIGGER )
		origin = cent.prev.origin + cg.lerpfrac * (cent.current.origin - cent.prev.origin);

	DrawTestBox( origin, mins, maxs );
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
	AddShellEffects( @ent, cent.effects );

	if( barrel && CGame::Scene::GrabTag( tag, @cent.refEnt, "tag_barrel2" ) ) {
		CGame::Scene::PlaceModelOnTag( @ent, @cent.refEnt, tag );
		CGame::Scene::AddEntityToScene( @ent );
		AddShellEffects( @ent, cent.effects );
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
			case ET_GIB:
				if( cg_gibs.boolean ) {
					cent.renderfx |= RF_NOSHADOW;
					UpdateGenericEnt( @cent );
					@cent.refEnt.model = @cgs.media.modIlluminatiGib;
				}
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

			case ET_FLAG_BASE:
				UpdateFlagBaseEnt( @cent );
				break;

			case ET_RADAR:
				cent.renderfx |= RF_NODEPTHTEST;
			case ET_SPRITE:
				cent.renderfx |= ( RF_NOSHADOW | RF_FULLBRIGHT );
				UpdateSpriteEnt( @cent );
				break;

			case ET_PUSH_TRIGGER:
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
					ExtrapolateLinearProjectile( @cent );
				} else {
					LerpGenericEnt( @cent );
				}
				break;

			case ET_SPRITE:
			case ET_RADAR:
				LerpSpriteEnt( @cent );
				break;

			case ET_PUSH_TRIGGER:
				break;

			default:
				break;
		}

		Vec3 origin, velocity;
		GetEntitySpatilization( number, origin, velocity );
		CGame::Sound::SetEntitySpatilization( number, origin, velocity );
	}
}

bool AddEntityReal( CEntity @cent )
{
	EntityState @state = @cent.current;

	switch( cent.type ) {
		case ET_GENERIC:
			AddGenericEnt( @cent );
			DrawEntityBox( @cent );
			EntityLoopSound( @state, ATTN_STATIC );
			return true;

		case ET_GIB:
			if( cg_gibs.boolean ) {
				AddGenericEnt( @cent );
				EntityLoopSound( @state, ATTN_STATIC );
			}
			return true;

		case ET_ITEM:
			AddItemEnt( @cent );
			DrawEntityBox( @cent );
			EntityLoopSound( @state, ATTN_IDLE );
			return true;

		case ET_PLASMA:
			AddGenericEnt( @cent );
			EntityLoopSound( @state, ATTN_STATIC );
			return true;

		case ET_PUSH_TRIGGER:
			DrawEntityBox( @cent );
			EntityLoopSound( @state, ATTN_STATIC );
			return true;

		case ET_FLAG_BASE:
			AddFlagBaseEnt( @cent );
			return true;

		case ET_SPRITE:
		case ET_RADAR:
			AddSpriteEnt( @cent );
			EntityLoopSound( @state, ATTN_STATIC );
			//canLight = true;
			return true;

		default:
			return false;
	}
	return false;
}

bool AddEntity( int entNum )
{
	CEntity @cent = @cgEnts[entNum];

	bool res = AddEntityReal( @cent );
	if( res ) {
		cent.trailOrigin = cent.refEnt.origin;
	}

	return res;
}

}
