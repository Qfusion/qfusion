/*
   Copyright (C) 2009-2010 Chasseur de bots

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

enum BombState {
	BombState_Idle,
	BombState_Carried,
	BombState_Dropped,
	BombState_Planting,
	BombState_Planted,
	BombState_Exploding,
}

enum BombDrop {
	BombDrop_Normal, // dropped manually
	BombDrop_Killed, // died
	BombDrop_Team, // changed teams
}

const int BOMB_HUD_OFFSET = 32;

BombState bombState = BombState_Idle;

cBombSite @bombSite;

int64 bombNextBeep;

int64 bombPickTime;
Entity @bombDropper;

// used for time of last action (eg planting) or when the bomb should explode
int64 bombActionTime;

Entity @bombCarrier = null; // also used for the planter
int64 bombCarrierCanPlantTime = -1;
Vec3 bombCarrierLastPos; // so it drops in the right place when they change teams
Vec3 bombCarrierLastVel;

Entity @defuser = null;
uint defuseProgress;

Entity @bombModel;
Entity @bombDecal;
Entity @bombHud;

void show( Entity @ent ) {
	ent.svflags &= ~SVF_NOCLIENT;
	ent.linkEntity();
}

void hide( Entity @ent ) {
	ent.svflags |= SVF_NOCLIENT;
}

Vec3 getMiddle( Entity @ent ) {
	Vec3 mins, maxs;
	ent.getSize( mins, maxs );
	return 0.5f * ( mins + maxs );
}

void bombModelCreate() {
	@bombModel = @G_SpawnEntity( "dynamite" );
	bombModel.type = ET_GENERIC;
	bombModel.setSize( BOMB_MINS, BOMB_MAXS );
	bombModel.solid = SOLID_TRIGGER;
	bombModel.light = BOMB_LIGHT_INACTIVE;
	bombModel.modelindex = modelBombModel;
	@bombModel.touch = dynamite_touch;
	@bombModel.stop = dynamite_stop;
}

void bombInit() {
	bombModelCreate();

	// don't set ~SVF_NOCLIENT yet
	@bombDecal = @G_SpawnEntity( "flag_indicator_decal" );
	bombDecal.type = ET_DECAL;
	bombDecal.origin2 = VEC_UP; // normal
	bombDecal.solid = SOLID_NOT;
	bombDecal.modelindex = imgBombDecal;
	bombDecal.svflags |= SVF_TRANSMITORIGIN2; // so the normal actually gets used

	@bombHud = @G_SpawnEntity( "hud_bomb" );
	bombHud.type = ET_HUD;
	bombHud.solid = SOLID_NOT;
	bombHud.svflags |= SVF_BROADCAST;

	bombActionTime = -1;
}

void bombPickUp() {
	bombCarrier.effects |= EF_CARRIER;
	bombCarrier.modelindex2 = modelBombBackpack;

	hide( @bombModel );
	hide( @bombDecal );
	hide( @bombHud );

	bombModel.moveType = MOVETYPE_NONE;
	bombModel.effects &= ~EF_ROTATE_AND_BOB;

	bombState = BombState_Carried;
}

void bombSetCarrier( Entity @ent, bool no_sound ) {
	if( @bombCarrier != null ) {
		bombCarrier.effects &= ~EF_CARRIER;
		bombCarrier.modelindex2 = 0;
	}

	@bombCarrier = @ent;
	bombPickUp();

	Client @client = @bombCarrier.client;
	client.addAward( S_COLOR_GREEN + "You've got the bomb!" );
	if( !no_sound ) {
		G_AnnouncerSound( @client, sndBombTaken, attackingTeam, true, null );
	}
}

void bombDrop( BombDrop drop_reason ) {
	Vec3 start = bombCarrier.origin;
	Vec3 end, velocity;

	switch( drop_reason ) {
		case BombDrop_Normal:
		case BombDrop_Killed: {
			bombPickTime = levelTime + BOMB_DROP_RETAKE_DELAY;
			@bombDropper = @bombCarrier;

			// throw from the player's eye
			start.z += bombCarrier.viewHeight;

			// aim it up by 10 degrees like nades
			Vec3 angles = bombCarrier.angles;

			Vec3 forward, right, up;
			angles.angleVectors( forward, right, up );

			// put it a little infront of the player
			Vec3 offset( 24.0, 0.0, -16.0 );
			end.x = start.x + forward.x * offset.x;
			end.y = start.y + forward.y * offset.x;
			end.z = start.z + forward.z * offset.x + offset.z;

			velocity = bombCarrier.velocity + forward * 200;
			velocity.z = BOMB_THROW_SPEED;
			if( drop_reason == BombDrop_Killed ) {
				velocity.z *= 0.5;
			}
		} break;

		case BombDrop_Team: {
			@bombDropper = null;

			// current pos/velocity are outdated
			start = end = bombCarrierLastPos;
			velocity = bombCarrierLastVel;
		} break;
	}

	Trace trace;
	trace.doTrace( start, BOMB_MINS, BOMB_MAXS, end, bombCarrier.entNum, MASK_SOLID );

	bombModel.moveType = MOVETYPE_TOSS;
	@bombModel.owner = @bombCarrier;
	bombModel.origin = trace.endPos;
	bombModel.velocity = velocity;
	show( @bombModel );
	hide( @bombDecal );

	bombCarrier.effects &= ~EF_CARRIER;
	bombCarrier.modelindex2 = 0;

	@bombCarrier = null;

	bombState = BombState_Dropped;
}

void bombStartPlanting( cBombSite @site ) {
	@bombSite = @site;

	Vec3 start = bombCarrier.origin;

	Vec3 end = start;
	end.z -= 512;

	Vec3 center = bombCarrier.origin + getMiddle( @bombCarrier );

	// trace to the ground
	// should this be merged with the ground check in canPlantBomb?
	Trace trace;
	trace.doTrace( start, BOMB_MINS, BOMB_MAXS, end, bombCarrier.entNum, MASK_SOLID );

	// show stuff
	bombModel.origin = trace.endPos;
	show( @bombModel );

	bombDecal.origin = trace.endPos;
	bombDecal.svflags |= SVF_ONLYTEAM;
	bombDecal.frame = 0;
	show( @bombDecal );

	bombHud.origin = trace.endPos + Vec3( 0, 0, BOMB_HUD_OFFSET );
	bombHud.svflags |= SVF_ONLYTEAM;
	bombHud.frame = BombDown_Planting;
	show( @bombHud );

	// make carrier look normal
	bombCarrier.effects &= ~EF_CARRIER;
	bombCarrier.modelindex2 = 0;

	bombActionTime = levelTime;
	bombState = BombState_Planting;

	G_Sound( @bombModel, 0, sndPlantStart, ATTN_NORM );
}

void bombPlanted() {
	bombActionTime = levelTime + int( cvarExplodeTime.value * 1000.0f );

	// add red dynamic light
	bombModel.light = BOMB_LIGHT_ARMED;
	bombModel.modelindex = modelBombModelActive;

	// show to defs too
	bombDecal.svflags &= ~SVF_ONLYTEAM;
	bombHud.svflags &= ~SVF_ONLYTEAM;

	announce( Announcement_Armed );

	G_CenterPrintFormatMsg( null, "Bomb planted at %s!", bombSite.letter );

	@bombCarrier = null;
	defuseProgress = 0;
	bombState = BombState_Planted;
}

void bombDefused() {
	bombModel.light = BOMB_LIGHT_INACTIVE;
	bombModel.modelindex = modelBombModel;

	hide( @bombDecal );
	hide( @bombHud );

	announce( Announcement_Defused );

	bombState = BombState_Idle;

	Client @client = @defuser.client;
	cPlayer @player = @playerFromClient( @client );
	player.defuses++;
	GT_updateScore( @client );

	client.addAward( "Bomb defused!" );
	G_PrintMsg( null, client.name + " defused the bomb!\n" );

	roundWonBy( defendingTeam );

	@defuser = null;
}

void bombExplode() {
	// do this first else the attackers can score 2 points when the explosion kills everyone
	roundWonBy( attackingTeam );

	bombSite.explode();

	bombState = BombState_Exploding;
	@defuser = null;

	G_Sound( @bombModel, 0, sndGoodGame, ATTN_DISTANT );

}

void resetBomb() {
	hide( @bombModel );
	hide( @bombDecal );

	bombModel.light = BOMB_LIGHT_INACTIVE;
	bombModel.modelindex = modelBombModel;

	bombDecal.team = bombHud.team = attackingTeam;

	bombState = BombState_Idle;
}

void bombThink() {
	switch( bombState ) {
		case BombState_Planting: {
			if( !entCanSee( bombCarrier, bombModel.origin ) || bombCarrier.origin.distance( bombModel.origin ) > BOMB_ARM_DEFUSE_RADIUS ) {
				setTeamProgress( attackingTeam, 0, BombProgress_Nothing );
				bombPickUp();
				break;
			}

			float frac = float( levelTime - bombActionTime ) / ( cvarArmTime.value * 1000.0f );
			if( frac >= 1.0f ) {
				setTeamProgress( attackingTeam, 0, BombProgress_Nothing );
				bombPlanted();
				break;
			}

			float decal_radius_frac = min( 1.0f, float( levelTime - bombActionTime ) / float( BOMB_SPRITE_RESIZE_TIME ) );
			bombDecal.frame = int( BOMB_ARM_DEFUSE_RADIUS * decal_radius_frac );
			bombDecal.effects |= EF_TEAMCOLOR_TRANSITION;
			bombDecal.counterNum = int( frac * 255.0f );
			if( frac != 0 )
				setTeamProgress( attackingTeam, int( frac * 100.0f ), BombProgress_Planting );
		} break;

		case BombState_Planted: {
			if( @defuser == null )
				@defuser = firstNearbyTeammate( bombModel.origin, defendingTeam );

			if( @defuser != null ) {
				if( !entCanSee( defuser, bombModel.origin ) || defuser.origin.distance( bombModel.origin ) > BOMB_ARM_DEFUSE_RADIUS ) {
					@defuser = null;
				}
			}

			if( @defuser == null && defuseProgress > 0 ) {
				defuseProgress = 0;
				setTeamProgress( defendingTeam, 0, BombProgress_Nothing );
			}
			else {
				defuseProgress += frameTime;
				float frac = defuseProgress / ( cvarDefuseTime.value * 1000.0f );
				if( frac >= 1.0f ) {
					bombDefused();
					setTeamProgress( defendingTeam, 100, BombProgress_Defusing );
					break;
				}

				setTeamProgress( defendingTeam, int( frac * 100.0f ), BombProgress_Defusing );
			}

			if( levelTime >= bombActionTime ) {
				bombExplode();
				break;
			}

			if( levelTime > bombNextBeep ) {
				G_PositionedSound( bombModel.origin, CHAN_AUTO, sndBeep, ATTN_DISTANT );

				uint remainingTime = bombActionTime - levelTime;

				uint nextBeepDelta = uint( BOMB_BEEP_FRACTION * remainingTime );

				if( nextBeepDelta > BOMB_BEEP_MAX ) {
					nextBeepDelta = BOMB_BEEP_MAX;
				}
				else if( nextBeepDelta < BOMB_BEEP_MIN ) {
					nextBeepDelta = BOMB_BEEP_MIN;
				}

				bombNextBeep = levelTime + nextBeepDelta;
			}
		} break;
	}
}

// fixes sprite/decal changing colour at the end of a round
// and the exploding animation from stopping
void bombPostRoundThink() {
	switch( bombState ) {
		case BombState_Planting: {
			bombDecal.effects |= EF_TEAMCOLOR_TRANSITION;

			float frac = float( levelTime - bombActionTime ) / ( cvarArmTime.value * 1000.0f );
			bombDecal.counterNum = int( frac * 255.0f );
		} break;

		case BombState_Exploding:
			bombSite.stepExplosion();
			break;
	}
}

Entity @ firstNearbyTeammate( Vec3 origin, int team ) {
	array< Entity @ > @nearby = G_FindInRadius( origin, BOMB_ARM_DEFUSE_RADIUS );
	for( uint i = 0; i < nearby.size(); i++ ) {
		Entity @target = nearby[i];
		if( @target.client == null ) {
			continue;
		}
		if( target.team != team || target.isGhosting() || !entCanSee( target, origin ) ) {
			continue;
		}

		return target;
	}

	return null;
}

bool bombCanPlant() {
	// check carrier is moving slowly enough
	// comparing squared length because it's faster

	Vec3 velocity = bombCarrier.velocity;

	if( velocity * velocity > BOMB_MAX_PLANT_SPEED * BOMB_MAX_PLANT_SPEED ) {
		return false;
	}

	// check carrier is on the ground
	// XXX: old bomb checked if they were < 32 units above ground
	Trace trace;

	Vec3 start = bombCarrier.origin;

	Vec3 end = start;
	end.z -= 1; // const this? max height for planting

	Vec3 mins, maxs;
	bombCarrier.getSize( mins, maxs );

	trace.doTrace( start, mins, maxs, end, bombCarrier.entNum, MASK_SOLID );

	if( trace.startSolid ) {
		return false;
	}

	// check carrier is on level ground
	// trace.planeNormal and VEC_UP are both unit vectors
	// XXX: old bomb had a check that did the same thing in a different way
	//      imo the old way is really bad - it traced a ray down and forwards
	//      but it removes some plant spots (wbomb1 B ramp) and makes some
	//      others easier (shepas A rail, reactors B upper)

	// fast dot product with ( 0, 0, 1 )
	if( trace.planeNormal.z < BOMB_MIN_DOT_GROUND ) {
		return false;
	}

	return true;
}

void bombGiveToRandom() {
	Team @team = @G_GetTeam( attackingTeam );

	uint n = getCarrierCount( attackingTeam );
	bool hasCarriers = cvarEnableCarriers.boolean && n > 0;
	if( !hasCarriers )
		n = team.numPlayers;

	int carrierIdx = random_uniform( 0, n );
	int seenCarriers = 0;

	for( int i = 0; @team.ent( i ) != null; i++ ) {
		Entity @ent = @team.ent( i );
		Client @client = @ent.client;

		cPlayer @player = @playerFromClient( @client );

		if( !hasCarriers || player.isCarrier ) {
			if( seenCarriers == carrierIdx ) {
				bombSetCarrier( @ent, true );

				G_CenterPrintFormatMsg( null, "%s has the bomb!", client.name );

				break;
			}

			seenCarriers++;
		}
	}
}

bool entCanSee( Entity @ent, Vec3 point ) {
	Vec3 center = ent.origin + getMiddle( @ent );

	Trace trace;
	return !trace.doTrace( center, vec3Origin, vec3Origin, point, ent.entNum, MASK_SOLID );
}

// move the camera around the site?
void bombLookAt( Entity @ent ) {
	Entity @target = @bombSite.indicator;

	array<Entity @> @targets = bombSite.indicator.findTargets();
	for( uint i = 0; i < targets.size(); i++ ) {
		if( targets[i].classname == "func_explosive" ) {
			@target = targets[i];
			break;
		}
	}

	Vec3 center = target.origin + getMiddle( @target );
	center.z -= 50; // this tilts the camera down (not by 50 degrees...)

	Vec3 bombOrigin = bombModel.origin;

	float diff = center.z - bombOrigin.z;

	if( diff > 8 ) {
		bombOrigin.z += diff / 2;
	}

	Vec3 dir = bombOrigin - center;

	float dist = dir.length();

	dir *= 1.0 / dist; // save a sqrt? Vec3 has no /=...

	Vec3 end = center + dir * ( dist + BOMB_DEAD_CAMERA_DIST );

	Trace trace;
	bool didHit = trace.doTrace( bombOrigin, vec3Origin, vec3Origin, end, -1, MASK_SOLID );

	Vec3 origin = trace.endPos;

	if( trace.fraction != 1 ) {
		origin += 8 * trace.planeNormal;
	}

	Vec3 viewDir = center - origin;
	Vec3 angles = viewDir.toAngles();

	ent.moveType = MOVETYPE_STOP;
	ent.origin = origin;
	ent.angles = angles;

	ent.linkEntity();
}

// ent stuff

void dynamite_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags ) {
	if( match.getState() != MATCH_STATE_PLAYTIME ) {
		return;
	}

	if( bombState != BombState_Dropped ) {
		return;
	}

	if( @other.client == null ) {
		return;
	}

	if( other.team != attackingTeam ) {
		return;
	}

	// dead players can't carry
	if( other.isGhosting() || other.health < 0 ) {
		return;
	}

	// did this guy drop it recently?
	if( levelTime < bombPickTime && @other == @bombDropper ) {
		return;
	}

	bombSetCarrier( @other, false );
}

void dynamite_stop( Entity @ent ) {
	if( bombState == BombState_Dropped ) {
		bombModel.effects = EF_ROTATE_AND_BOB;

		bombHud.origin = bombModel.origin + Vec3( 0, 0, BOMB_HUD_OFFSET );
		bombHud.team = attackingTeam;
		bombHud.svflags |= SVF_ONLYTEAM;
		bombHud.frame = BombDown_Dropped;
		show( @bombHud );
	}
}
