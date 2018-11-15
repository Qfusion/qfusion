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

class cPendingExplosion
{
	uint pointIndex;
	int64 explodeTime;

	cPendingExplosion @next;

	cPendingExplosion( uint index, int64 time )
	{
		this.pointIndex = index;
		this.explodeTime = time;
	}
}

class cBombSite
{
	Entity @indicator;

	String letter;

	Entity @model;
	Entity @sprite;

	bool useExplosionPoints;
	Vec3[] explosionPoints;

	cPendingExplosion @pendingExplosionHead;

	cBombSite @next;

	cBombSite( Entity @ent, bool hasTargets )
	{
		if ( siteCount >= SITE_LETTERS.length() )
		{
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

		@this.model = @G_SpawnEntity( "capture_indicator_model" );
		this.model.type = ET_GENERIC;
		this.model.solid = SOLID_TRIGGER; // so bots can touch it
		this.model.origin = origin;
		this.model.modelindex = G_ModelIndex( "models/objects/bomb/spot_indicator_" + this.letter + ".md3", true );
		this.model.svflags &= ~SVF_NOCLIENT;
		this.model.effects = EF_ROTATE_AND_BOB;
		this.model.linkEntity();

		@this.sprite = @G_SpawnEntity( "capture_indicator_sprite" );
		this.sprite.type = ET_RADAR;
		this.sprite.solid = SOLID_NOT;
		this.sprite.origin = origin;
		this.sprite.modelindex = G_ImageIndex( "gfx/bomb/radar_" + this.letter );
		this.sprite.svflags = ( this.sprite.svflags & ~SVF_NOCLIENT ) | SVF_BROADCAST;
		this.sprite.frame = BOMB_ARM_DEFUSE_RADIUS;
		this.sprite.linkEntity();

		if ( hasTargets )
		{
			this.useExplosionPoints = false;
		}
		else
		{
			G_Print( "Bomb site " + this.letter + " has no targets. Consider adding them for more control over the bomb's explosion.\n" );

			this.useExplosionPoints = true;
			this.computeExplosionPoints();
		}

		@this.next = @siteHead;
		@siteHead = @this;

		siteCount++;
	}

	void carrierTouched()
	{
		if ( bombCanPlant() )
		{
			bombPlant( this );
		}
	}

	void explode()
	{
		if ( this.useExplosionPoints )
		{
			for ( int i = 0; i < SITE_EXPLOSION_POINTS; i++ )
			{
				if ( random() < SITE_EXPLOSION_PROBABILITY )
				{
					addPendingExplosion( i, levelTime + int( random() * SITE_EXPLOSION_MAX_DELAY ) );
				}
			}
		}
		else
		{
			this.indicator.useTargets( bombModel );
		}
	}

	void addPendingExplosion( uint index, int64 time )
	{
		cPendingExplosion @explosion = @cPendingExplosion( index, time );

		// check if there's no head

		if ( @pendingExplosionHead == null )
		{
			@pendingExplosionHead = @explosion;

			return;
		}

		// unroll first interation as there is no last node

		if ( time < pendingExplosionHead.explodeTime )
		{
			@explosion.next = @pendingExplosionHead;
			@pendingExplosionHead = @explosion;

			return;
		}

		cPendingExplosion @last = @this.pendingExplosionHead; // at last...

		for ( cPendingExplosion @node = @this.pendingExplosionHead.next; @node != null; @node = @node.next )
		{
			if ( time < node.explodeTime )
			{
				@explosion.next = @node;
				@last.next = @explosion;

				return;
			}

			@last = @node;
		}

		// explosion is later than all currently in linked list
		// put this one on the end

		@last.next = @explosion;
	}

	void computeExplosionPoints()
	{
		this.explosionPoints.resize( SITE_EXPLOSION_POINTS );

		Vec3 origin = this.indicator.origin;
		origin.z += 96;

		for ( int i = 0; i < SITE_EXPLOSION_POINTS; i++ )
		{
			// generate vector pointing in a random direction but with z >= 0
			Vec3 dir(
				brandom( -1, 1 ),
				brandom( -1, 1 ),
				random() // 0..1
			);

			// i guess this is faster than normalizing then multiplying
			float maxDist = SITE_EXPLOSION_MAX_DIST / dir.length();

			// trace a ray in the direction of dir with the
			// distance capped by SITE_EXPLOSION_MAX_DIST

			Trace trace;
			trace.doTrace( origin, vec3Origin, vec3Origin, origin + dir * maxDist, this.indicator.entNum, MASK_SOLID );

			// pick a random point along the line
			this.explosionPoints[i] = origin + random() * ( trace.endPos - origin );
		}
	}
}

cBombSite @getSiteFromIndicator( Entity @ent )
{
	for ( cBombSite @site = @siteHead; @site != null; @site = @site.next )
	{
		if ( @site.indicator == @ent )
		{
			return @site;
		}
	}

	assert( false, "site.as getSiteFromIndicator: couldn't find a site" );

	return null; // shut up compiler
}

void resetBombSites()
{
	@siteHead = null;
	siteCount = 0;
}

void misc_capture_area_indicator( Entity @ent )
{
	@ent.think = misc_capture_area_indicator_think;

	// drop to floor?
	if ( ent.spawnFlags & 1 == 0 )
	{
		Vec3 start, end, mins( -16, -16, -24 ), maxs( 16, 16, 32 );

		start = end = ent.origin;

		start.z += 16;
		end.z -= 512;

		Trace trace;
		trace.doTrace( start, mins, maxs, end, ent.entNum, MASK_SOLID );

		if ( trace.startSolid )
		{
			G_Print( ent.classname + " at " + vec3ToString(ent.origin) + " is in a solid, removing...\n" );

			ent.freeEntity();

			return;
		}

		ent.origin = trace.endPos;
	}

	cBombSite( @ent, ent.target != "" );
}

void misc_capture_area_indicator_think( Entity @ent )
{
	// if AS had static this could be approx 1 bajillion times
	// faster on subsequent calls

	array<Entity @> @triggers = @ent.findTargeting();

	// we are being targeted, never think again
	if ( !triggers.empty() )
	{
		return;
	}

	ent.nextThink = levelTime + 1;

	if ( roundState != ROUNDSTATE_ROUND )
	{
		return;
	}

	if ( bombState != BOMBSTATE_CARRIED )
	{
		return;
	}

	if ( !bombCanPlant() )
	{
		return;
	}

	Vec3 origin = ent.origin;
	Vec3 carrierOrigin = bombCarrier.origin;

	if ( origin.distance( carrierOrigin ) > BOMB_AUTODROP_DISTANCE )
	{
		return;
	}

	origin.z += 96;

	Vec3 center = carrierOrigin + getMiddle( @bombCarrier );

	Trace trace;
	if ( !trace.doTrace( origin, vec3Origin, vec3Origin, center, bombCarrier.entNum, MASK_SOLID ) )
	{
		// let's plant it

		cBombSite @site = @getSiteFromIndicator( @ent );

		// we know site isn't null but for debugging purposes...
		assert( @site != null, "site.as trigger_capture_area_touch: @site == null" );

		site.carrierTouched();
	}
}

void trigger_capture_area( Entity @ent )
{
	@ent.think = trigger_capture_area_think;
	@ent.touch = trigger_capture_area_touch;
	ent.setupModel( ent.model ); // set up the brush model
	ent.solid = SOLID_TRIGGER;
	ent.linkEntity();

	// give time for indicator to load
	// old bomb did 1s but this seems to be enough
	ent.nextThink = levelTime + 1;
}

void trigger_capture_area_think( Entity @ent )
{
	array<Entity @> @targets = ent.findTargets();
	if ( targets.empty() )
	{
		G_Print( "trigger_capture_area at " + vec3ToString(ent.origin) + " has no target, removing...\n" );
		ent.freeEntity();
	}
}

// honey you're touching something, you're touchin' me
void trigger_capture_area_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
	// i'm under your thumb, under your spell, can't you see?

	if ( @other.client == null )
	{
		return;
	}

	// IF I COULD ONLY REACH YOU

	if ( roundState != ROUNDSTATE_ROUND )
	{
		return;
	}

	// IF I COULD MAKE YOU SMILE

	if ( bombState != BOMBSTATE_CARRIED || @other != @bombCarrier )
	{
		return;
	}

	// IF I COULD ONLY REACH YOU

	if ( match.getState() != MATCH_STATE_PLAYTIME )
	{
		return;
	}

	// THAT WOULD REALLY BE A

	// we know target isn't null because we checked earlier

	cBombSite @site = null;

	array<Entity @> @targets = ent.findTargets();
	@site = getSiteFromIndicator( targets[0] );

	// we know point isn't null either but for debugging purposes...
	assert( @site != null, "site.as trigger_capture_area_touch: @site == null" );

	// BREAKTHROUGH

	site.carrierTouched();
}
