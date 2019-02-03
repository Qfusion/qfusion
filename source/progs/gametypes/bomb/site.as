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

cBombSite @siteHead = null;
uint siteCount = 0;

class PendingExplosion {
	Vec3 pos;
	int64 time;

	PendingExplosion( Vec3 p, int64 t ) {
		this.pos = p;
		this.time = t;
	}
}

const float PI = 3.14159265f;
const int64 EXPLOSION_COMEDIC_DELAY = 1000;

float max( float a, float b ) {
	return a > b ? a : b;
}

Vec3 random_point_on_hemisphere() {
	float z = random_float01();
	float r = sqrt( max( 0.0f, 1.0f - z * z ) );
	float phi = 2 * PI * random_float01();
	return Vec3( r * cos( phi ), r * sin( phi ), z );
}

class cBombSite
{
	Entity @indicator;

	String letter;

	Entity @hud;

	bool useExplosionPoints;
	Vec3[] explosionPoints;
	bool targetsUsed;

	PendingExplosion[] pendingExplosions;
	int numPendingExplosions;
	int numExploded;

	cBombSite @next;

	cBombSite( Entity @ent, bool hasTargets, int team ) {
		if( siteCount >= SITE_LETTERS.length() ) {
			G_Print( "Too many bombsites... ignoring\n" );

			return;
		}

		@this.indicator = @ent;

		this.indicator.modelindex = 0;
		this.indicator.solid = SOLID_TRIGGER;
		this.indicator.nextThink = levelTime + 1;
		this.indicator.team = 0;
		this.indicator.linkEntity();

		Vec3 origin = this.indicator.origin;
		origin.z += 128;

		this.letter = SITE_LETTERS[siteCount];

		@this.hud = @G_SpawnEntity( "hud_bomb_site" );
		this.hud.type = ET_HUD;
		this.hud.solid = SOLID_NOT;
		this.hud.origin = origin;
		this.hud.team = team;
		this.hud.svflags = SVF_BROADCAST;
		this.hud.counterNum = letter[0];
		this.hud.linkEntity();

		if( hasTargets ) {
			this.useExplosionPoints = false;
			this.targetsUsed = false;
		}
		else {
			this.useExplosionPoints = true;
			this.generateExplosionPoints();
		}

		@this.next = @siteHead;
		@siteHead = @this;

		siteCount++;
	}

	void carrierTouched() {
		bombCarrierCanPlantTime = levelTime;
		if( bombCanPlant() ) {
			Vec3 mins, maxs;
			bombCarrier.getSize( mins, maxs );
			Vec3 velocity = bombCarrier.velocity;

			if( maxs.z < 40 && levelTime - bombActionTime >= 1000 && velocity * velocity < BOMB_MAX_PLANT_SPEED * BOMB_MAX_PLANT_SPEED ) {
				bombStartPlanting( this );
			}
		}
	}

	void explode() {
		if( !this.useExplosionPoints )
			return;

		numPendingExplosions = random_uniform( SITE_EXPLOSION_POINTS / 2, SITE_EXPLOSION_POINTS );
		numExploded = 0;

		for( int i = 0; i < numPendingExplosions; i++ ) {
			Vec3 point = explosionPoints[ random_uniform( 0, explosionPoints.length() ) ];
			int64 time = EXPLOSION_COMEDIC_DELAY + int64( ( float( i ) / float( numPendingExplosions - 1 ) ) * SITE_EXPLOSION_MAX_DELAY );
			pendingExplosions[ i ] = PendingExplosion( point, time );
		}
	}

	void stepExplosion() {
		int64 t = levelTime - bombActionTime;

		if( t >= EXPLOSION_COMEDIC_DELAY ) {
			hide( @bombModel );
			hide( @bombDecal );
		}

		if( !this.useExplosionPoints ) {
			if( !targetsUsed && t >= EXPLOSION_COMEDIC_DELAY ) {
				this.indicator.useTargets( bombModel );
				targetsUsed = true;
			}
			return;
		}

		while( numExploded < numPendingExplosions && t >= pendingExplosions[ numExploded ].time ) {
			Entity @ent = @G_SpawnEntity( "func_explosive" );

			ent.origin = pendingExplosions[ numExploded ].pos;
			ent.linkEntity();

			ent.explosionEffect( BOMB_EXPLOSION_EFFECT_RADIUS );
			ent.splashDamage( @ent, 3000, 9001, 100, MOD_EXPLOSIVE );

			ent.freeEntity();

			numExploded++;
		}
	}

	void generateExplosionPoints() {
		this.explosionPoints.resize( SITE_EXPLOSION_POINTS );
		this.pendingExplosions.resize( SITE_EXPLOSION_POINTS );

		Vec3 origin = this.indicator.origin;
		origin.z += 96;

		for( int i = 0; i < SITE_EXPLOSION_POINTS; i++ ) {
			Vec3 dir = random_point_on_hemisphere();
			Vec3 end = origin + dir * SITE_EXPLOSION_MAX_DIST;

			Trace trace;
			trace.doTrace( origin, vec3Origin, vec3Origin, end, this.indicator.entNum, MASK_SOLID );

			// pick a random point along the line
			this.explosionPoints[i] = origin + random_float01() * ( trace.endPos - origin );
		}
	}
}

cBombSite @getSiteFromIndicator( Entity @ent ) {
	for( cBombSite @site = @siteHead; @site != null; @site = @site.next ) {
		if( @site.indicator == @ent ) {
			return @site;
		}
	}

	assert( false, "site.as getSiteFromIndicator: couldn't find a site" );

	return null; // shut up compiler
}

void resetBombSites() {
	@siteHead = null;
	siteCount = 0;
}

void misc_capture_area_indicator( Entity @ent ) {
	@ent.think = misc_capture_area_indicator_think;

	// drop to floor?
	if( ent.spawnFlags & 1 == 0 ) {
		Vec3 start, end, mins( -16, -16, -24 ), maxs( 16, 16, 32 );

		start = end = ent.origin;

		start.z += 16;
		end.z -= 512;

		Trace trace;
		trace.doTrace( start, mins, maxs, end, ent.entNum, MASK_SOLID );

		if( trace.startSolid ) {
			G_Print( ent.classname + " at " + vec3ToString(ent.origin) + " is in a solid, removing...\n" );

			ent.freeEntity();

			return;
		}

		ent.origin = trace.endPos;
	}

	cBombSite( @ent, ent.target != "", defendingTeam );
}

void misc_capture_area_indicator_think( Entity @ent ) {
	// if AS had static this could be approx 1 bajillion times
	// faster on subsequent calls

	array<Entity @> @triggers = @ent.findTargeting();

	// we are being targeted, never think again
	if( !triggers.empty() ) {
		return;
	}

	ent.nextThink = levelTime + 1;

	if( roundState != RoundState_Round ) {
		return;
	}

	if( bombState != BombState_Carried ) {
		return;
	}

	if( !bombCanPlant() ) {
		return;
	}

	Vec3 origin = ent.origin;
	Vec3 carrierOrigin = bombCarrier.origin;

	if( origin.distance( carrierOrigin ) > BOMB_AUTODROP_DISTANCE ) {
		return;
	}

	origin.z += 96;

	Vec3 center = carrierOrigin + getMiddle( @bombCarrier );

	Trace trace;
	if( !trace.doTrace( origin, vec3Origin, vec3Origin, center, bombCarrier.entNum, MASK_SOLID ) ) {
		// let's plant it

		cBombSite @site = @getSiteFromIndicator( @ent );

		// we know site isn't null but for debugging purposes...
		assert( @site != null, "site.as trigger_capture_area_touch: @site == null" );

		site.carrierTouched();
	}
}

void trigger_capture_area( Entity @ent ) {
	@ent.think = trigger_capture_area_think;
	@ent.touch = trigger_capture_area_touch;
	ent.setupModel( ent.model ); // set up the brush model
	ent.solid = SOLID_TRIGGER;
	ent.linkEntity();

	// give time for indicator to load
	// old bomb did 1s but this seems to be enough
	ent.nextThink = levelTime + 1;
}

void trigger_capture_area_think( Entity @ent ) {
	array<Entity @> @targets = ent.findTargets();
	if( targets.empty() ) {
		G_Print( "trigger_capture_area at " + vec3ToString(ent.origin) + " has no target, removing...\n" );
		ent.freeEntity();
	}
}

void trigger_capture_area_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags ) {
	if( @other.client == null ) {
		return;
	}

	if( roundState != RoundState_Round ) {
		return;
	}

	if( bombState != BombState_Carried || @other != @bombCarrier ) {
		return;
	}

	if( match.getState() != MATCH_STATE_PLAYTIME ) {
		return;
	}

	cBombSite @site = null;

	array<Entity @> @targets = ent.findTargets();
	@site = getSiteFromIndicator( targets[0] );
	site.carrierTouched();
}
