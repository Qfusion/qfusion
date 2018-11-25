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

// i trust bomb will never have more than 1 bomb

/*enum eBombStates FIXME enum
  {
  BOMBSTATE_IDLE,
  BOMBSTATE_CARRIED,
  BOMBSTATE_DROPPING, // bomb was dropped but is still in the air
  BOMBSTATE_DROPPED,
  BOMBSTATE_PLANTING, // bomb has just been planted and we're animating
  BOMBSTATE_PLANTED,
  BOMBSTATE_ARMED,
  BOMBSTATE_EXPLODING // bomb has just exploded and we're animating
  }*/

const uint BOMBSTATE_IDLE = 0;
const uint BOMBSTATE_CARRIED = 1;
const uint BOMBSTATE_DROPPING = 2; // bomb was dropped but is still in the air
const uint BOMBSTATE_DROPPED = 3;
const uint BOMBSTATE_PLANTING = 4; // bomb has just been planted and we're animating
const uint BOMBSTATE_PLANTED = 5;
const uint BOMBSTATE_ARMED = 6;
const uint BOMBSTATE_EXPLODING_ANIM = 7; // bomb has just exploded and we're animating the sprite (falls through)
const uint BOMBSTATE_EXPLODING = 8; // bomb has just exploded and we're spamming explosions all over the place

enum ProgressType {
	ProgressType_Nothing,
	ProgressType_Planting,
	ProgressType_Defusing,
};

// FIXME enum
const uint BOMBDROP_NORMAL = 0; // dropped manually
const uint BOMBDROP_KILLED = 1; // died
const uint BOMBDROP_TEAM = 2; // changed teams

//eBombStates bombState = BOMBSTATE_IDLE; FIXME enum
uint bombState = BOMBSTATE_IDLE;
// Bot state before resetBomb()
uint oldBombState = BOMBSTATE_IDLE;

cBombSite @bombSite;

uint bombProgress;

int64 bombNextBeep;

int64 bombPickTime;
Entity @bombDropper;

// used for time of last action (eg planting)
// so things can be animated
// or when the bomb should explode
// because the times should never ever overlap
// TODO: add a check incase they do?
//       something like bombLaunchState instead of bombState = ...
//       and check if current state is *ING and set radii etc
int64 bombActionTime;

Entity @bombCarrier = null;
Vec3 bombCarrierLastPos; // so it drops in the right place when they change teams
Vec3 bombCarrierLastVel;

Entity @bombModel;
Entity @bombSprite;
Entity @bombDecal;

void show( Entity @ent )
{
	ent.svflags &= ~SVF_NOCLIENT;

	ent.linkEntity();
}

void hide( Entity @ent )
{
	ent.svflags |= SVF_NOCLIENT;
}

Vec3 getMiddle( Entity @ent )
{
	Vec3 mins, maxs;
	ent.getSize( mins, maxs );

	return 0.5 * ( mins + maxs );
}

void bombModelCreate()
{
	@bombModel = @G_SpawnEntity( "dynamite" );
	bombModel.type = ET_GENERIC;
	bombModel.setSize( BOMB_MINS, BOMB_MAXS );
	bombModel.solid = SOLID_TRIGGER;
	bombModel.light = BOMB_LIGHT_INACTIVE;
	bombModel.modelindex = modelBombModel;
	@bombModel.touch = dynamite_touch;
	@bombModel.stop = dynamite_stop;
}

// initializes bomb entities so they can be linked later
void bombInit()
{
	// i'm not setting svflags &= ~SVF_NOCLIENT yet
	// no need to link either 

	bombModelCreate();

	@bombSprite = @G_SpawnEntity( "capture_indicator_sprite" );
	bombSprite.type = ET_RADAR;
	bombSprite.solid = SOLID_NOT;
	bombSprite.frame = BOMB_ARM_DEFUSE_RADIUS; // radius
	bombSprite.modelindex = imgBombSprite;
	bombSprite.svflags |= SVF_BROADCAST;

	@bombDecal = @G_SpawnEntity( "flag_indicator_decal" );
	bombDecal.type = ET_DECAL;
	bombDecal.origin2 = VEC_UP; // normal
	bombDecal.solid = SOLID_NOT;
	bombDecal.frame = BOMB_ARM_DEFUSE_RADIUS; // radius
	bombDecal.modelindex = imgBombDecal;
	bombDecal.svflags |= SVF_TRANSMITORIGIN2; // so the normal actually gets used

}

void bombSetCarrier( Entity @ent )
{
	if ( @bombCarrier != null )
	{
		bombCarrier.effects &= ~EF_CARRIER;
		bombCarrier.modelindex2 = 0;
	}

	@bombCarrier = @ent;
	bombCarrier.effects |= EF_CARRIER;
	bombCarrier.modelindex2 = modelBombBackpack;

	hide( @bombModel );
	hide( @bombSprite );

	// it's invisible but maybe still moving
	// save some physics calculations
	bombModel.moveType = MOVETYPE_NONE;

	Client @client = @bombCarrier.client;

	client.addAward( S_COLOR_GREEN + "You've got the bomb!" );
	G_AnnouncerSound( @client, sndBombTaken, attackingTeam, true, null );

	bombState = BOMBSTATE_CARRIED;
}

void bombDrop( uint dropType )
{
	Vec3 start = bombCarrier.origin;
	Vec3 end, velocity;

	switch ( dropType )
	{
		case BOMBDROP_NORMAL:
		case BOMBDROP_KILLED: // TODO XXX FIXME
			{
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
				if( dropType == BOMBDROP_KILLED ) {
					velocity.z *= 0.5;
				}
				break;
			}

			/*case BOMBDROP_KILLED:
			// to avoid issues with a player dropping it then
			// someone else picking it up and immediately dying
			@bombDropper = null;

			end = start;
			velocity = bombCarrier.velocity;

			break;*/

		case BOMBDROP_TEAM:
			@bombDropper = null;

			// current pos/velocity are outdated
			start = end = bombCarrierLastPos;
			velocity = bombCarrierLastVel;

			break;
	}

	Trace trace;
	trace.doTrace( start, BOMB_MINS, BOMB_MAXS, end, bombCarrier.entNum, MASK_SOLID );

	Vec3 origin = trace.endPos;

	// gets set to none in bombSetCarrier
	bombModel.moveType = MOVETYPE_TOSS;

	@bombModel.owner = @bombCarrier;

	bombModel.origin = origin;
	bombModel.velocity = velocity;

	bombSprite.origin = origin;

	show( @bombModel );
	show( @bombSprite );

	bombCarrier.effects &= ~EF_CARRIER;
	bombCarrier.modelindex2 = 0;

	@bombCarrier = null;

	bombState = BOMBSTATE_DROPPING;
}

void bombPlant( cBombSite @site )
{
	@bombSite = @site;

	Vec3 start = bombCarrier.origin;

	Vec3 end = start;
	end.z -= 512;

	Vec3 center = bombCarrier.origin + getMiddle( @bombCarrier );

	// trace to the ground
	// should this be merged with the ground check in canPlantBomb?
	Trace trace;
	trace.doTrace( start, BOMB_MINS, BOMB_MAXS, end, bombCarrier.entNum, MASK_SOLID );

	// get moving
	Vec3 origin = trace.endPos;

	bombModel.origin = origin;
	bombSprite.origin = origin;
	bombDecal.origin = origin;

	// only show sprite etc to team
	// XXX: should it show a white decal for defenders?
	//      i think the decal should only be shown when there's a reason
	//      for them to stand on the bomb, which there isn't when it's not armed
	bombSprite.svflags |= SVF_ONLYTEAM;
	bombDecal.svflags |= SVF_ONLYTEAM;

	bombSprite.frame = bombDecal.frame = 0; // so they can expand

	// show stuff
	show( @bombModel );
	show( @bombSprite );
	show( @bombDecal );

	// make carrier look normal
	bombCarrier.effects &= ~EF_CARRIER;
	bombCarrier.modelindex2 = 0;

	// do this last unless you like null pointers
	@bombCarrier = null;

	announce( ANNOUNCEMENT_INPLACE );

	bombProgress = 0;
	bombActionTime = levelTime;
	bombState = BOMBSTATE_PLANTING;
}

void bombArm(array<Entity @> @nearby)
{
	bombActionTime = levelTime + int( cvarExplodeTime.value * 1000.0f );

	// add red dynamic light
	bombModel.light = BOMB_LIGHT_ARMED;

	bombModel.modelindex = modelBombModelActive;

	// show to defs too
	bombSprite.svflags &= ~SVF_ONLYTEAM;
	bombDecal.svflags &= ~SVF_ONLYTEAM;

	announce( ANNOUNCEMENT_ARMED );

	G_CenterPrintFormatMsg( null, "Bomb planted at %s!", bombSite.letter );

	setTeamProgress( attackingTeam, 0, ProgressType_Nothing );

	hideSiteIndicators( bombSite );

	bombProgress = 0;
	bombState = BOMBSTATE_ARMED;
}

void bombDefuse(array<Entity @> @nearby)
{
	bombModel.light = BOMB_LIGHT_INACTIVE;
	bombModel.modelindex = modelBombModel;

	hide( @bombDecal );

	announce( ANNOUNCEMENT_DEFUSED );

	bombState = BOMBSTATE_IDLE;

	// print defusing players, add score/awards

	String awardMsg = playersAliveOnTeam( attackingTeam ) == 0 ? "Bomb defused!" : "Ninja defuse!";

	Entity @stop = @G_GetClient( maxClients - 1 ).getEnt();

	Vec3 bombOrigin = bombModel.origin;

	Client @client = nearby[0].client;
	cPlayer @player = @playerFromClient( @client );

	// unroll first iteration as the first name should have no prefix
	String defuseMsg = client.name;

	player.defuses++;
	GT_updateScore( @client );

	client.addAward( awardMsg );

	for( uint i = 1; i < nearby.size(); i++ )
	{
		@client = nearby[i].client;
		@player = playerFromClient( @client );

		// " and guy" if this is the last player, else ", guy"
		defuseMsg += ( i + 1 == nearby.size() ? " and " : ", " ) + client.name;

		player.defuses++;
		GT_updateScore( @client );

		client.addAward( awardMsg );
	}

	G_PrintMsg( null, defuseMsg + " defused the bomb!\n" );

	roundWonBy( defendingTeam );
}

void bombExplode()
{
	// do this first else the attackers can score 2 points when the explosion kills everyone
	roundWonBy( attackingTeam );

	bombModel.explosionEffect( BOMB_EXPLOSION_EFFECT_RADIUS );
	bombModel.splashDamage( @bombModel, 3000, 9001, 9001, MOD_EXPLOSIVE );

	bombSite.explode();

	hide( @bombModel );
	hide( @bombDecal );

	bombState = BOMBSTATE_EXPLODING_ANIM;
}

// 2lazy
void resetBomb()
{
	hide( @bombModel );
	hide( @bombSprite );
	hide( @bombDecal );

	bombSprite.frame = BOMB_ARM_DEFUSE_RADIUS;
	bombModel.light = BOMB_LIGHT_INACTIVE;
	bombModel.modelindex = modelBombModel;

	bombSprite.team = bombDecal.team = attackingTeam;

	oldBombState = bombState;
	bombState = BOMBSTATE_IDLE;
}

void bombThink()
{
	switch ( bombState )
	{
		case BOMBSTATE_IDLE:
		case BOMBSTATE_CARRIED:
			// do nothing

			break;

		case BOMBSTATE_DROPPING:
			{
				Vec3 origin = bombModel.origin;

				bombSprite.origin = origin;

				bombSprite.linkEntity();

				break;
			}

		case BOMBSTATE_DROPPED:
			bombModel.effects = EF_ROTATE_AND_BOB;
			break;

		case BOMBSTATE_PLANTING:
			{
				float frac = float( levelTime - bombActionTime ) / float( BOMB_SPRITE_RESIZE_TIME ) ;

				if ( frac >= 1.0f )
				{
					bombState = BOMBSTATE_PLANTED;

					return;
				}

				bombSprite.frame = bombDecal.frame = int( BOMB_ARM_DEFUSE_RADIUS * frac );
			}

			// fallthrough

		case BOMBSTATE_PLANTED:
			{
				array<Entity @> @nearby = nearbyPlayers( bombModel.origin, attackingTeam );
				bool progressing = nearby.size() > 0;

				if ( progressing )
				{
					bombProgress += frameTime;

					if ( bombProgress >= uint( cvarArmTime.value * 1000.0f ) ) // uint to avoid mismatch
					{
						bombArm( nearby );

						break;
					}
				}
				else
				{
					bombProgress -= min( bombProgress, frameTime );
				}

				// this needs to be done every frame...
				bombSprite.effects |= EF_TEAMCOLOR_TRANSITION;
				bombDecal.effects |= EF_TEAMCOLOR_TRANSITION;

				if ( bombProgress != 0 )
				{
					float frac = bombProgress / ( cvarArmTime.value * 1000.0f );

					int progress = int( frac * 100.0f );

					if ( !progressing )
					{
						progress = -progress;
					}

					setTeamProgress( attackingTeam, progress, ProgressType_Planting );

					bombSprite.counterNum = bombDecal.counterNum = int( frac * 255.0f );
				}
				else
				{
					bombSprite.counterNum = bombDecal.counterNum = 0;
				}

				break;
			}

		case BOMBSTATE_ARMED:
			{
				array<Entity @> @nearby = nearbyPlayers( bombModel.origin, defendingTeam );
				bool progressing = nearby.size() > 0;

				if ( progressing )
				{
					bombProgress += frameTime;

					if ( bombProgress >= uint( cvarDefuseTime.value * 1000.0f ) ) // cast to avoid mismatch
					{
						bombDefuse( nearby );

						setTeamProgress( defendingTeam, 100, ProgressType_Defusing );

						break;
					}
				}
				else
				{
					bombProgress -= min( bombProgress, frameTime );
				}

				if ( levelTime >= bombActionTime )
				{
					bombExplode();
					break;
				}

				if ( bombProgress != 0 )
				{
					int progress = int( ( bombProgress * 100.0f ) / ( cvarDefuseTime.value * 1000.0f ) );

					if ( !progressing )
					{
						progress = -progress;
					}

					setTeamProgress( defendingTeam, progress, ProgressType_Defusing );
				}

				if ( levelTime > bombNextBeep )
				{
					G_PositionedSound( bombModel.origin, CHAN_AUTO, sndBeep, ATTN_DISTANT );

					uint remainingTime = bombActionTime - levelTime;

					uint nextBeepDelta = uint( BOMB_BEEP_FRACTION * remainingTime );

					if ( nextBeepDelta > BOMB_BEEP_MAX )
					{
						nextBeepDelta = BOMB_BEEP_MAX;
					}
					else if ( nextBeepDelta < BOMB_BEEP_MIN )
					{
						nextBeepDelta = BOMB_BEEP_MIN;
					}

					bombNextBeep = levelTime + nextBeepDelta;
				}

				break;
			}

		default:
			assert( false, "bomb.as bombThink: invalid bombState" );

			break;
	}
}

// fixes sprite/decal changing colour at the end of a round
// and the exploding animation from stopping
void bombAltThink()
{
	switch( bombState )
	{
		case BOMBSTATE_DROPPING:
			{
				Vec3 origin = bombModel.origin;

				bombSprite.origin = origin;

				bombSprite.linkEntity();

				break;
			}

		case BOMBSTATE_PLANTED:
			bombSprite.effects |= EF_TEAMCOLOR_TRANSITION;
			bombDecal.effects |= EF_TEAMCOLOR_TRANSITION;

			if ( bombProgress != 0 )
			{
				float frac = bombProgress / ( cvarArmTime.value * 1000.0f );

				bombSprite.counterNum = bombDecal.counterNum = int( frac * 255.0f );
			}
			else
			{
				bombSprite.counterNum = bombDecal.counterNum = 0;
			}

			break;

		case BOMBSTATE_EXPLODING_ANIM:
			{
				float frac = float( levelTime - bombActionTime ) / float( BOMB_SPRITE_RESIZE_TIME ) ;

				if ( frac >= 1.0f )
				{
					hide( @bombSprite );

					bombState = BOMBSTATE_EXPLODING;

					return;
				}

				bombSprite.frame = int( BOMB_ARM_DEFUSE_RADIUS * ( 1.0f - frac ) );
			}

			// fallthrough

		case BOMBSTATE_EXPLODING:
			// TODO: i guess this would fit better as a routine in cBombSite
			if ( bombSite.useExplosionPoints )
			{
				while ( @bombSite.pendingExplosionHead != null && bombSite.pendingExplosionHead.explodeTime <= levelTime )
				{
					Entity @ent = @G_SpawnEntity( "func_explosive" );

					ent.origin = bombSite.explosionPoints[bombSite.pendingExplosionHead.pointIndex];
					ent.linkEntity();

					ent.explosionEffect( BOMB_EXPLOSION_EFFECT_RADIUS );
					ent.splashDamage( @ent, 3000, 9001, 9001, MOD_EXPLOSIVE );

					ent.freeEntity();

					@bombSite.pendingExplosionHead = @bombSite.pendingExplosionHead.next;
				}
			}

			break;

		default:
			break;
	}
}


// returns first player after target upto stop who is within
// BOMB_ARM_DEFUSE_RADIUS units of origin and is on team
// returns null if nobody is found
array<Entity @> @nearbyPlayers( Vec3 origin, int team )
{
	array<Entity @> @inradius = G_FindInRadius( origin, BOMB_ARM_DEFUSE_RADIUS );
	array<Entity @> filtered( 0 );

	uint count = 0;

	for( uint i = 0; i < inradius.size(); i++ ) {
		Entity @target = inradius[i];
		if( @target.client == null ) {
			continue;
		}
		if( target.team != team || target.isGhosting() || !entCanSee( target, origin ) ) {
			continue;
		}

		filtered.resize( count + 1 );
		@filtered[count++] = target;
	}

	return filtered;
}

bool bombCanPlant()
{
	// check carrier is moving slowly enough
	// comparing squared length because it's faster

	Vec3 velocity = bombCarrier.velocity;

	if ( velocity * velocity > BOMB_MAX_PLANT_SPEED * BOMB_MAX_PLANT_SPEED )
	{
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

	if ( trace.startSolid )
	{
		return false;
	}

	// check carrier is on level ground
	// trace.planeNormal and VEC_UP are both unit vectors
	// XXX: old bomb had a check that did the same thing in a different way
	//      imo the old way is really bad - it traced a ray down and forwards
	//      but it removes some plant spots (wbomb1 B ramp) and makes some
	//      others easier (shepas A rail, reactors B upper)

	// fast dot product with ( 0, 0, 1 )
	if ( trace.planeNormal.z < BOMB_MIN_DOT_GROUND )
	{
		return false;
	}

	return true;
}

void bombGiveToRandom()
{
	Team @team = @G_GetTeam( attackingTeam );

	if ( cvarEnableCarriers.boolean )
	{
		uint carrierCount = getCarrierCount( attackingTeam );

		if ( carrierCount != 0 )
		{
			// -1 so it starts from 0
			int playerNum = int( random() * ( carrierCount - 1 ) );

			for ( int i = 0, carrier = 0; @team.ent( i ) != null; i++ )
			{
				Entity @ent = @team.ent( i );
				Client @client = @ent.client;

				cPlayer @player = @playerFromClient( @client );

				if ( player.isCarrier )
				{
					if ( carrier == playerNum )
					{
						bombSetCarrier( @ent );

						G_CenterPrintFormatMsg( null, "%s has the bomb!", client.name );

						break;
					}

					carrier++;
				}
			}

			return;
		}

		// if carrierCount == 0 then fallthrough as if they were disabled
	}

	int playerNum = int( random() * team.numPlayers );

	for ( int i = 0; @team.ent( i ) != null; i++ )
	{
		if ( i == playerNum )
		{
			Entity @ent = @team.ent( i );

			bombSetCarrier( @ent );

			G_CenterPrintFormatMsg( null, "%s has the bomb!", ent.client.name );

			break;
		}
	}
}

bool entCanSee( Entity @ent, Vec3 point )
{
	Vec3 center = ent.origin + getMiddle( @ent );

	Trace trace;
	return !trace.doTrace( center, vec3Origin, vec3Origin, point, ent.entNum, MASK_SOLID );
}

// move the camera around the site?
void bombLookAt( Entity @ent )
{
	Entity @target = @bombSite.indicator;

	array<Entity @> @targets = bombSite.indicator.findTargets();
	for( uint i = 0; i < targets.size(); i++ ) {
		if ( targets[i].classname == "func_explosive" ) {
			@target = targets[i];
			break;
		}
	}

	Vec3 center = target.origin + getMiddle( @target );
	center.z -= 50; // this tilts the camera down (not by 50 degrees...)

	Vec3 bombOrigin = bombModel.origin;

	float diff = center.z - bombOrigin.z;

	if ( diff > 8 )
	{
		bombOrigin.z += diff / 2;
	}

	Vec3 dir = bombOrigin - center;

	float dist = dir.length();

	dir *= 1.0 / dist; // save a sqrt? Vec3 has no /=...

	Vec3 end = center + dir * ( dist + BOMB_DEAD_CAMERA_DIST );

	Trace trace;
	bool didHit = trace.doTrace( bombOrigin, vec3Origin, vec3Origin, end, -1, MASK_SOLID );

	Vec3 origin = trace.endPos;

	if ( trace.fraction != 1 )
	{
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

void dynamite_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
	if ( match.getState() != MATCH_STATE_PLAYTIME )
	{
		return;
	}

	if ( bombState != BOMBSTATE_DROPPED && bombState != BOMBSTATE_DROPPING )
	{
		return;
	}

	if ( @other.client == null )
	{
		return;
	}

	if ( other.team != attackingTeam )
	{
		return;
	}

	// dead players can't carry
	if ( other.isGhosting() || other.health < 0 )
	{
		return;
	}

	// did this guy drop it recently?
	if ( levelTime < bombPickTime && @other == @bombDropper )
	{
		return;
	}

	bombSetCarrier( @other );
}

void dynamite_stop( Entity @ent )
{
	if ( bombState == BOMBSTATE_DROPPING )
	{
		bombState = BOMBSTATE_DROPPED;

		Vec3 origin = bombModel.origin;

		bombSprite.origin = origin;
	}
}
