namespace CGame {

namespace LE {

enum eLocalEntityType
{
	LE_FREE,
	LE_NO_FADE,
	LE_RGB_FADE,
	LE_ALPHA_FADE,
	LE_SCALE_ALPHA_FADE,
	LE_INVERSESCALE_ALPHA_FADE,
	LE_LASER,

	LE_EXPLOSION_TRACER,
	LE_DASH_SCALE,
	LE_PUFF_SCALE,
	LE_PUFF_SHRINK
};

class LocalEntity {
    int type;
	Vec4 color;

	int64 start;

	float light;
	Vec3 lightcolor;

	Vec3 velocity;
	Vec3 avelocity;
	Vec3 angles;
	Vec3 accel;

	int bounce;     // is activator and bounceability value at once
	int frames;

	ModelSkeleton @skel;
    CGame::Scene::Entity refEnt;
}

const int MAX_LOCAL_ENTITIES = 512;
const int FADEINFRAMES = 2;

const Vec3 debrisMaxs( 4, 4, 8 );
const Vec3 debrisMins( -4, -4, 0 );

array<LocalEntity> localEntities(MAX_LOCAL_ENTITIES);
List::List freeLocalEntities;
List::List activeLocalEntities;

void Init() {
    freeLocalEntities.clear();
    activeLocalEntities.clear();
    for( int i = 0; i < MAX_LOCAL_ENTITIES; i++ ) {
        freeLocalEntities.pushBack( any( @localEntities[i] ) );
    }
}

LocalEntity @Alloc( eLocalEntityType type, float r, float g, float b, float a ) {
	List::Element @el;

	if( !freeLocalEntities.empty() ) { // take a free decal if possible
		@el = freeLocalEntities.popBack();
	} else {              // grab the oldest one otherwise
		@el = activeLocalEntities.popFront();
	}

	activeLocalEntities.pushBack( el.value );

	LocalEntity @le;
	el.value.retrieve( @le );

	le.bounce = 0;
	le.frames = 0;
	le.light = 0.0f;
    le.type = type;
    le.start = cg.time;
    le.color.set( r, g, b, a );
	le.velocity.clear();
	le.accel.clear();
	le.avelocity.clear();

	le.refEnt.reset();
	le.refEnt.shaderRGBA = Vec4ToColor( le.color );

	return @le;
}

LocalEntity @AllocModel( eLocalEntityType type, const Vec3 &in origin, const Vec3 &in angles, int frames,
    float r, float g, float b, float a, float light, float lr, float lg, float lb, ModelHandle @model, ShaderHandle @shader ) {
	LocalEntity @le;

	@le = Alloc( type, r, g, b, a );
	le.frames = frames;
	le.light = light;
    le.lightcolor.set( lr, lg, lb );

	le.refEnt.rtype = RT_MODEL;
	le.refEnt.renderfx = RF_NOSHADOW;
	@le.refEnt.model = @model;
	@le.refEnt.customShader = @shader;
	le.refEnt.shaderTime = cg.time;
    le.refEnt.origin = origin;
    le.angles = angles;
    angles.anglesToAxis( le.refEnt.axis );

	return @le;
}

LocalEntity @AllocSprite( eLocalEntityType type, const Vec3 &in origin, float radius, int frames,
    float r, float g, float b, float a, float light, float lr, float lg, float lb, ShaderHandle @shader ) {
	LocalEntity @le;

	@le = Alloc( type, r, g, b, a );
	le.frames = frames;
	le.light = light;
    le.lightcolor = Vec3( lr, lg, lb );

	le.refEnt.rtype = RT_SPRITE;
	le.refEnt.radius = radius;
	le.refEnt.renderfx = RF_NOSHADOW;
	@le.refEnt.customShader = @shader;
	le.refEnt.shaderTime = cg.time;
    le.refEnt.origin = origin;
 
	return @le;
}

LocalEntity @AllocLaser( const Vec3 &in start, const Vec3 &in end, float radius, int frames,
    float r, float g, float b, float a, ShaderHandle @shader ) {
	LocalEntity @le;

	@le = Alloc( LE_LASER, r, g, b, a );
	le.frames = frames;

	le.refEnt.shaderRGBA = Vec4ToColor( Vec4( r, g, b, a ) );
	le.refEnt.radius = radius;
	@le.refEnt.customShader = @shader;
	le.refEnt.shaderTime = cg.time;
    le.refEnt.origin = start;
    le.refEnt.origin2 = end;

	return @le;
}

void SpawnSprite( const Vec3 &in origin, const Vec3 &in velocity, const Vec3 &in accel,
    float radius, int time, int bounce, bool expandEffect, bool shrinkEffect,
    float r, float g, float b, float a, float light, float lr, float lg, float lb, ShaderHandle @shader ) {
	LocalEntity @le;
	eLocalEntityType type = LE_ALPHA_FADE;

	if( radius == 0.0f || @shader is null ) {
		return;
	}

	int numFrames = int( float( time * 10.0f ) );
	if( numFrames == 0 ) {
		return;
	}

	if( expandEffect && !shrinkEffect ) {
		type = LE_SCALE_ALPHA_FADE;
	}

	if( !expandEffect && shrinkEffect ) {
		type = LE_INVERSESCALE_ALPHA_FADE;
	}

	@le = AllocSprite( type, origin, radius, numFrames,
        r, g, b, a,
        light, lr, lg, lb,
        @shader );
    le.velocity = velocity;
    le.accel = accel;
	le.bounce = bounce;
	le.refEnt.rotation = float( rand() % 360 );
}

void Explosion_Puff( const Vec3 &in pos, float radius, int frame ) {
	LocalEntity @le;
	ShaderHandle @shader = cgs.media.shaderSmokePuff1;

	switch( rand() % 3 ) {
		case 1:
			@shader = @cgs.media.shaderSmokePuff2;
			break;
		case 2:
			@shader = @cgs.media.shaderSmokePuff3;
			break;
		case 0:
		default:
			@shader = @cgs.media.shaderSmokePuff1;
			break;
	}

	Vec3 local_pos = pos + Vec3( crandom() * 4.0f, crandom() * 4.0f, crandom() * 4.0f );

	@le = AllocSprite( LE_PUFF_SCALE, local_pos, radius, frame,
						1.0f, 1.0f, 1.0f, 1.0f,
						0, 0, 0, 0,
						@shader );
	le.refEnt.rotation = float( rand() % 360 );
}

void Explosion_Puff_2( const Vec3 &in pos, const Vec3 &in vel, float radius ) {
	if( radius == 0.0f ) {
		radius = floor( 35.0f + crandom() * 5 );
	}

	LocalEntity @le = AllocSprite( LE_PUFF_SHRINK, pos, radius, 7,
						1.0f, 1.0f, 1.0f, 0.2f,
						0, 0, 0, 0,
						@cgs.media.shaderSmokePuff3 );
	le.velocity = vel;
}

void ElectroRings( const Vec3 &in start, const Vec3 &in end, const Vec4 &in color ) {
	const float space = 15.0f;

	Vec3 dir = end - start;
	float len = dir.normalize();
	if( len == 0.0f ) {
		return;
	}

	int numrings = int( len / space ) + 1;
	float timeFrac = 0.6f / float( numrings );
	for( int i = 0; i < numrings; i++ ) {
		int t = int( ( float( i ) * timeFrac + 7.5f + ( i * 0.20f ) ) * cg_ebbeam_time.value );
		float l = i * space;

		Vec3 origin = start + l * dir;
		LocalEntity @le = AllocSprite( LE_ALPHA_FADE, origin, 4.25f, t,
							color.x, color.y, color.z, color.w, 0, 0, 0, 0,
							@cgs.media.shaderElectroBeamRing );
		le.refEnt.rotation = rand() % 360;
	}
}

void ElectroTrail2( const Vec3 &in start, const Vec3 &in end, int team ) {
	Vec4 color( 1.0f, 1.0f, 1.0f, 1.0f );

	if( cg_ebbeam_time.value < 0.05f ) {
		return;
	}

	if( cg_teamColoredBeams.boolean && ( ( team == TEAM_ALPHA ) || ( team == TEAM_BETA ) ) ) {
		color = TeamColor( team );
	}

	ElectroPolyBeam( start, end, team );
	ElectroRings( start, end, color );
	ElectroIonsTrail( start, end, color );
}

void ImpactSmokePuff( const Vec3 &in origin, const Vec3 &in dir, float radius, float alpha, int time, int speed ) {
	const float SMOKEPUFF_MAXVIEWDIST = 700.0f;

	if( ( GS::PointContents( origin ) & MASK_WATER ) != 0 ) {
		return;
	}

	Vec3 local_dir = dir;
	local_dir.normalize();

	//offset the origin by half of the radius
	Vec3 local_origin = origin + radius * 0.5f * local_dir;

	LocalEntity @le = AllocSprite( LE_SCALE_ALPHA_FADE, local_origin, radius + crandom(), time,
						1, 1, 1, alpha, 0, 0, 0, 0, @cgs.media.shaderSmokePuff );
	le.refEnt.rotation = rand() % 360;
	le.velocity = local_dir * speed;
}

void BulletExplosion( const Vec3 &in pos, const Vec3 &in dir ) {
	LocalEntity @le = AllocModel( LE_ALPHA_FADE, pos, dir.toAngles(), 3, //3 frames for weak
		1, 1, 1, 1, //full white no inducted alpha
		0, 0, 0, 0, //dlight
		@cgs.media.modBulletExplode,
		null );

	le.refEnt.rotation = rand() % 360;
	le.refEnt.scale = 1.0f;
	if( cg_particles.boolean ) {
		ImpactSmokePuff( pos, dir, 2, 0.6f, 6, 8 );
	}
}

void BulletExplosion( const Vec3 &in pos, const Trace &in tr ) {
	Vec3 dir = tr.planeNormal;
	Vec3 angles = dir.toAngles();

	if( tr.entNum > 0 && cgEnts[tr.entNum].current.type == ET_PLAYER ) {
		return;
	}
	
	if( ( tr.surfFlags & SURF_FLESH ) != 0 || ( tr.entNum > 0 && cgEnts[tr.entNum].current.type == ET_CORPSE ) ) {
		LocalEntity @le = AllocModel( LE_ALPHA_FADE, pos, angles, 3, //3 frames for weak
							1, 0, 0, 1, //full white no inducted alpha
							0, 0, 0, 0, //dlight
							@cgs.media.modBulletExplode,
							null );
		le.refEnt.rotation = rand() % 360;
		le.refEnt.scale = 1.0f;
		if( IsViewerEntity( tr.entNum ) ) {
			le.refEnt.renderfx |= RF_VIEWERMODEL;
		}
	} else if( cg_particles.boolean && ( tr.surfFlags & SURF_DUST ) != 0 ) {
		// throw particles on dust
		ImpactSmokePuff( tr.endPos, tr.planeNormal, 4, 0.6f, 6, 8 );
	} else {
		LocalEntity @le = AllocModel( LE_ALPHA_FADE, pos, angles, 3, //3 frames for weak
							1, 1, 1, 1, //full white no inducted alpha
							0, 0, 0, 0, //dlight
							cgs.media.modBulletExplode,
							null );
		le.refEnt.rotation = rand() % 360;
		le.refEnt.scale = 1.0f;

		if( cg_particles.boolean ) {
			ImpactSmokePuff( tr.endPos, tr.planeNormal, 2, 0.6f, 6, 8 );
		}

		if( ( tr.surfFlags & SURF_NOMARKS ) == 0 ) {
			CGame::Scene::SpawnDecal( pos, dir, random() * 360, 8, 1, 1, 1, 1, 10, 1, false, @cgs.media.shaderBulletMark );
		}
	}
}

void BubbleTrail( const Vec3 &in start, const Vec3 &in end, int dist ) {
	Vec3 vec = end - start;
	float len = vec.normalize();
	if( len == 0.0f ) {
		return;
	}

	vec *= dist;
	auto @shader = @cgs.media.shaderWaterBubble;

	Vec3 move = start;
	for( int i = 0; i < len; i += dist ) {
		LocalEntity @le = AllocSprite( LE_ALPHA_FADE, move, 3, 10,
							 1, 1, 1, 1,
							 0, 0, 0, 0,
							 shader );
		le.velocity.set( crandom() * 5, crandom() * 5, crandom() * 5 + 6 );
		move += vec;
	}
}

void PlasmaExplosion( const Vec3 &in pos, const Vec3 &in dir, int fire_mode, float radius ) {
	LocalEntity @le;
	const float model_radius = PLASMA_EXPLOSION_MODEL_RADIUS;

	Vec3 angles = dir.toAngles();
	Vec3 origin = pos + IMPACT_POINT_OFFSET * dir;

	if( fire_mode == FIRE_MODE_STRONG ) {
		@le = AllocModel( LE_ALPHA_FADE, origin, angles, 4,
							1, 1, 1, 1,
							150, 0, 0.75, 0,
							@cgs.media.modPlasmaExplosion,
							null );
		le.refEnt.scale = radius / model_radius;
	} else {
		@le = AllocModel( LE_ALPHA_FADE, origin, angles, 4,
							1, 1, 1, 1,
							80, 0, 0.75, 0,
							@cgs.media.modPlasmaExplosion,
							null );
		le.refEnt.scale = radius / model_radius;
	}

	le.refEnt.rotation = rand() % 360;

	CGame::Scene::SpawnDecal( pos, dir, 90, 16,
				   1, 1, 1, 1, 4, 1, true,
				   @cgs.media.shaderPlasmaMark );
}

void BoltExplosionMode( const Vec3 &in pos, const Vec3 &in dir, int fire_mode, int surfFlags ) {
	if( CGame::Scene::SpawnDecal( pos, dir, random() * 360, 12,
		1, 1, 1, 1, 10, 1, true, @cgs.media.shaderElectroboltMark ) == 0 ) {
		if( ( surfFlags & ( SURF_SKY | SURF_NOMARKS | SURF_NOIMPACT ) ) != 0 ) {
			return;
		}
	}

	Vec3 angles = dir.toAngles();
	Vec3 origin = pos + IMPACT_POINT_OFFSET * dir;

	LocalEntity @le = AllocModel( LE_INVERSESCALE_ALPHA_FADE, pos, angles, 6, // 6 is time
						1, 1, 1, 1, //full white no inducted alpha
						250, 0.75, 0.75, 0.75, //white dlight
						@cgs.media.modElectroBoltWallHit, null );

	le.refEnt.rotation = rand() % 360;
	le.refEnt.scale = ( fire_mode == FIRE_MODE_STRONG ) ? 1.5f : 1.0f;

	// add white energy particles on the impact
	ImpactPuffParticles( origin, dir, 15, 0.75f, 1, 1, 1, 1 );

	CGame::Sound::StartFixedSound( @cgs.media.sfxElectroboltHit, origin, CHAN_AUTO,
							cg_volume_effects.value, ATTN_STATIC );
}

void InstaExplosionMode( const Vec3 &in pos, const Vec3 &in dir, int fire_mode, int surfFlags, int owner ) {
	int team = -1;
	Vec4 tcolor( 0.65f, 0.0f, 0.26f, 1.0f );

	if( cg_teamColoredInstaBeams.boolean && ( owner > 0 ) && ( owner < GS::maxClients + 1 ) ) {
		team = cgEnts[owner].current.team;
	}

	if( ( team == TEAM_ALPHA ) || ( team == TEAM_BETA ) ) {
		tcolor = TeamColor( team );
		tcolor.x *= 0.65f;
		tcolor.y *= 0.65f;
		tcolor.z *= 0.65f;
	}

	Vec3 angles = dir.toAngles();
	Vec3 origin = pos + IMPACT_POINT_OFFSET * dir;

	if( CGame::Scene::SpawnDecal( pos, dir, random() * 360, 12,
						tcolor[0], tcolor[1], tcolor[2], 1.0f,
						10, 1, true, @cgs.media.shaderInstagunMark ) == 0 ) {
		if( ( surfFlags & ( SURF_SKY | SURF_NOMARKS | SURF_NOIMPACT ) ) != 0 ) {
			return;
		}
	}

	LocalEntity @le = AllocModel( LE_ALPHA_FADE, origin, angles, 6, // 6 is time
						tcolor[0], tcolor[1], tcolor[2], 1,
						250, 0.65, 0.65, 0.65, //white dlight
						@cgs.media.modInstagunWallHit, null );

	le.refEnt.rotation = rand() % 360;
	le.refEnt.scale = ( fire_mode == FIRE_MODE_STRONG ) ? 1.5f : 1.0f;

	// add white energy particles on the impact
	ImpactPuffParticles( origin, dir, 15, 0.75f, 1, 1, 1, 1 );

	CGame::Sound::StartFixedSound( cgs.media.sfxElectroboltHit, origin, CHAN_AUTO,
							cg_volume_effects.value, ATTN_STATIC );
}

void RocketExplosionMode( const Vec3 pos, const Vec3 dir, int fire_mode, float radius ) {
	const float expvelocity = 8.0f;

	Vec3 angles = dir.toAngles();

	if( fire_mode == FIRE_MODE_STRONG ) {
		//trap_S_StartSound ( pos, 0, 0, CG_RegisterSfx (cgs.media.sfxRocketLauncherStrongHit), cg_volume_effects.value, ATTN_NORM, 0 );
		CGame::Scene::SpawnDecal( pos, dir, random() * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, @cgs.media.shaderExplosionMark );
	} else {
		//trap_S_StartSound ( pos, 0, 0, CG_RegisterSfx (cgs.media.sfxRocketLauncherWeakHit), cg_volume_effects.value, ATTN_NORM, 0 );
		CGame::Scene::SpawnDecal( pos, dir, random() * 360, radius * 0.25, 1, 1, 1, 1, 10, 1, false, @cgs.media.shaderExplosionMark );
	}

	// animmap shader of the explosion
	Vec3 origin = pos +radius * 0.12f * dir;
	LocalEntity @le = AllocSprite( LE_ALPHA_FADE, origin, radius * 0.5f, 8,
						1, 1, 1, 1,
						radius * 4, 0.8f, 0.6f, 0, // orange dlight
						@cgs.media.shaderRocketExplosion );

	Vec3 vec ( crandom() * expvelocity, crandom() * expvelocity, crandom() * expvelocity );
	le.velocity = vec + dir * expvelocity;
	le.refEnt.rotation = rand() % 360;

	if( cg_explosionsRing.boolean ) {
		// explosion ring sprite
		origin = pos + radius * 0.20f * dir;
		le = AllocSprite( LE_ALPHA_FADE, origin, radius, 3,
							 1, 1, 1, 1,
							 0, 0, 0, 0, // no dlight
							 cgs.media.shaderRocketExplosionRing );

		le.refEnt.rotation = rand() % 360;
	}

	if( cg_explosionsDust.integer == 1 ) {
		// dust ring parallel to the contact surface
		ExplosionsDust( pos, dir, radius );
	}

	// Explosion particles
	ParticleExplosionEffect( pos, dir, 1, 0.5, 0, 32 );

	if( fire_mode == FIRE_MODE_STRONG ) {
		CGame::Sound::StartFixedSound( @cgs.media.sfxRocketLauncherStrongHit, pos, CHAN_AUTO, cg_volume_effects.value, ATTN_DISTANT );
	} else {
		CGame::Sound::StartFixedSound( @cgs.media.sfxRocketLauncherWeakHit, pos, CHAN_AUTO, cg_volume_effects.value, ATTN_DISTANT );
	}

	//jalfixme: add sound at water?
}

void BladeImpact( const Vec3 &in pos, const Vec3 &in dir ) {
	int POVent = CGame::Camera::GetMainCamera().POVent;

	//find what are we hitting
	Vec3 local_pos( pos );
	Vec3 local_dir = dir;
	local_dir.normalize();
	Vec3 end = pos - local_dir;

	Trace trace;
	trace.doTrace( local_pos, vec3Origin, vec3Origin, end, POVent, MASK_SHOT );
	if( trace.fraction == 1.0 ) {
		return;
	}

	Vec3 angles = dir.toAngles();
	Vec3 origin = pos + IMPACT_POINT_OFFSET * dir;

	if( ( trace.surfFlags & SURF_FLESH ) != 0 ||
		( trace.entNum > 0 && cgEnts[trace.entNum].current.type == ET_PLAYER )
		|| ( trace.entNum > 0 && cgEnts[trace.entNum].current.type == ET_CORPSE ) ) {
		LocalEntity @le = AllocModel( LE_ALPHA_FADE, origin, angles, 3, //3 frames for weak
							1, 1, 1, 1, //full white no inducted alpha
							0, 0, 0, 0, //dlight
							@cgs.media.modBladeWallHit, null );
		le.refEnt.rotation = rand() % 360;
		le.refEnt.scale = 1.0f;

		CGame::Sound::StartFixedSound( @cgs.media.sfxBladeFleshHit[rand() % 3], origin, CHAN_AUTO,
								cg_volume_effects.value, ATTN_NORM );
	} else if( ( trace.surfFlags & SURF_DUST ) != 0 ) {
		// throw particles on dust
		SplashParticles( trace.endPos, trace.planeNormal, 0.30f, 0.30f, 0.25f, 30 );

		//fixme? would need a dust sound
		CGame::Sound::StartFixedSound( @cgs.media.sfxBladeWallHit[rand() % 2], origin, CHAN_AUTO,
								cg_volume_effects.value, ATTN_NORM );
	} else {
		LocalEntity @le = AllocModel( LE_ALPHA_FADE, origin, angles, 3, //3 frames for weak
							1, 1, 1, 1, //full white no inducted alpha
							0, 0, 0, 0, //dlight
							@cgs.media.modBladeWallHit, null );
		le.refEnt.rotation = rand() % 360;
		le.refEnt.scale = 1.0f;

		SplashParticles( trace.endPos, trace.planeNormal, 0.30f, 0.30f, 0.25f, 15 );

		CGame::Sound::StartFixedSound( cgs.media.sfxBladeWallHit[rand() % 2], origin, CHAN_AUTO,
								cg_volume_effects.value, ATTN_NORM );
		if( ( trace.surfFlags & SURF_NOMARKS ) == 0 ) {
			CGame::Scene::SpawnDecal( pos, dir, random() * 10, 8, 1, 1, 1, 1, 10, 1, false, @cgs.media.shaderBladeMark );
		}
	}
}

void LaserGunImpact( const Vec3 &in pos, float radius, const Vec3 &in laser_dir, const Vec4 &in color ) {
	CGame::Scene::Entity ent;

	Vec3 ndir = laser_dir * -1.0f;
	Vec3 angles = ndir.toAngles();
	angles.z = anglemod( -360.0f * cg.time * 0.001f );

	ent.origin = pos;
	ent.renderfx = RF_FULLBRIGHT | RF_NOSHADOW;
	ent.scale = 1.45f;
	ent.shaderRGBA = COLOR_RGBA( uint8( color[0] * 255 ), uint8( color[1] * 255 ), uint8( color[2] * 255 ), uint8( color[3] * 255 ) );
	@ent.model = @cgs.media.modLasergunWallExplo;
	angles.anglesToAxis( ent.axis );

	CGame::Scene::AddEntityToScene( @ent );
}

void GunBladeBlastImpact( const Vec3 &in pos, const Vec3 &in dir, float radius ) {
	const float model_radius = GUNBLADEBLAST_EXPLOSION_MODEL_RADIUS;

	Vec3 angles = dir.toAngles();
	Vec3 origin = pos + IMPACT_POINT_OFFSET * dir;

	LocalEntity @le = AllocModel( LE_ALPHA_FADE, pos, angles, 2, //3 frames
						1, 1, 1, 1, //full white no inducted alpha
						0, 0, 0, 0, //dlight
						@cgs.media.modBladeWallHit,
						null );
	le.refEnt.rotation = rand() % 360;
	le.refEnt.scale = 1.0f; // this is the small bullet impact

	LocalEntity @le_explo = AllocModel( LE_ALPHA_FADE, origin, angles, 2 + int( radius / 16.1f ),
						1, 1, 1, 1, //full white no inducted alpha
						0, 0, 0, 0, //dlight
						@cgs.media.modBladeWallExplo,
						null );
	le_explo.refEnt.rotation = rand() % 360;
	le_explo.refEnt.scale = radius / model_radius;

	CGame::Scene::SpawnDecal( pos, dir, random() * 360, 3 + ( radius * 0.5f ), 
		1, 1, 1, 1, 10, 1, false, @cgs.media.shaderExplosionMark );
}

void ProjectileFireTrail( CEntity @cent ) {
	const float radius = 8;
	const float alpha = bound( 0.0f, cg_projectileFireTrailAlpha.value, 1.0f );

	if( !cg_projectileFireTrail.boolean ) {
		return;
	}

	// didn't move
	Vec3 vec = cent.refEnt.origin - cent.trailOrigin;
	float len = vec.normalize();
	if( len == 0.0f ) {
		return;
	}

	ShaderHandle @shader;
	if( ( cent.effects & EF_STRONG_WEAPON ) != 0 ) {
		@shader = @cgs.media.shaderStrongRocketFireTrailPuff;
	} else {
		@shader = @cgs.media.shaderWeakRocketFireTrailPuff;
	}

	// density is found by quantity per second
	int trailTime = int( 1000.0f / cg_projectileFireTrail.value );
	if( trailTime < 1 ) {
		trailTime = 1;
	}

	// we don't add more than one sprite each frame. If frame
	// ratio is too slow, people will prefer having less sprites on screen
	if( cent.localEffects[LEF_ROCKETFIRE_LAST_DROP] + trailTime < cg.time ) {
		cent.localEffects[LEF_ROCKETFIRE_LAST_DROP] = cg.time;

		LocalEntity @le = AllocSprite( LE_INVERSESCALE_ALPHA_FADE, cent.trailOrigin, radius, 4,
							1.0f, 1.0f, 1.0f, alpha,
							0, 0, 0, 0,
							@shader );
		le.velocity.set( -vec.x * 10 + crandom() * 5, -vec.y * 10 + crandom() * 5, -vec.z * 10 + crandom() * 5 );
		le.refEnt.rotation = rand() % 360;
	}
}

void ProjectileTrail( CEntity @cent ) {
	float radius = 6.5f, alpha = 0.35f;
	ShaderHandle @shader = @cgs.media.shaderSmokePuff;

	ProjectileFireTrail( @cent ); // add fire trail

	if( !cg_projectileTrail.boolean ) {
		return;
	}

	// didn't move
	Vec3 vec = cent.refEnt.origin - cent.trailOrigin;
	float len = vec.normalize();
	if( len == 0.0f ) {
		return;
	}

	// density is found by quantity per second
	int trailTime = int( 1000.0f / cg_projectileTrail.value );
	if( trailTime < 1 ) {
		trailTime = 1;
	}

	// we don't add more than one sprite each frame. If frame
	// ratio is too slow, people will prefer having less sprites on screen
	if( cent.localEffects[LEF_ROCKETTRAIL_LAST_DROP] + trailTime < cg.time ) {
		cent.localEffects[LEF_ROCKETTRAIL_LAST_DROP] = cg.time;

		int contents = GS::PointContents( cent.trailOrigin ) & GS::PointContents( cent.refEnt.origin );
		if( ( contents & MASK_WATER ) != 0 ) {
			@shader = @cgs.media.shaderWaterBubble;
			radius = 3 + crandom();
			alpha = 1.0f;
		}

		LocalEntity @le = AllocSprite( LE_PUFF_SHRINK, cent.trailOrigin, radius, 20,
							1.0f, 1.0f, 1.0f, alpha,
							0, 0, 0, 0,
							@shader );
		le.velocity.set( -vec[0] * 5 + crandom() * 5, -vec[1] * 5 + crandom() * 5, -vec[2] * 5 + crandom() * 5 + 3 );
		le.refEnt.rotation = rand() % 360;
	}
}

void BloodDamageEffect( const Vec3 &in origin, const Vec3 &in dir, int damage ) {
	float radius = 5.0f, alpha = cg_bloodTrailAlpha.value;
	const int time = 8;
	ShaderHandle @shader = cgs.media.shaderBloodImpactPuff;

	if( !cg_bloodTrail.boolean ) {
		return;
	}

	int count = bound( 1, int( damage * 0.25f ), 10 );

	if( ( GS::PointContents( origin ) & MASK_WATER ) != 0 ) {
		@shader = @cgs.media.shaderBloodTrailLiquidPuff;
		radius += ( 1 + crandom() );
		alpha = 0.5f * cg_bloodTrailAlpha.value;
	}
	alpha = bound( 0.0f, alpha, 1.0f );

	for( int i = 0; i < count; i++ ) {
		LocalEntity @le = AllocSprite( LE_PUFF_SHRINK, origin, radius + crandom(), time,
							 1, 1, 1, alpha, 0, 0, 0, 0, @shader );

		le.refEnt.rotation = rand() % 360;

		// randomize dir
		le.velocity.set(
				   -dir[0] * 5 + crandom() * 5,
				   -dir[1] * 5 + crandom() * 5,
				   -dir[2] * 5 + crandom() * 5 + 3 );
		le.velocity = dir + float( min( 6, count ) ) * le.velocity;
	}
}

void GrenadeExplosionMode( const Vec3 &in pos, const Vec3 &in dir, int fire_mode, float radius ) {
	const float expvelocity = 8.0f;
	Vec3 angles = dir.toAngles();

	//if( CG_PointContents( pos ) & MASK_WATER )
	//jalfixme: (shouldn't we do the water sound variation?)

	if( fire_mode == FIRE_MODE_STRONG ) {
		CGame::Scene::SpawnDecal( pos, dir, random() * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, @cgs.media.shaderExplosionMark );
	} else {
		CGame::Scene::SpawnDecal( pos, dir, random() * 360, radius * 0.25, 1, 1, 1, 1, 10, 1, false, @cgs.media.shaderExplosionMark );
	}

	// animmap shader of the explosion
	Vec3 origin = pos + radius * 0.15f * dir;
	LocalEntity @le = AllocSprite( LE_ALPHA_FADE, origin, radius * 0.5f, 8,
						1, 1, 1, 1,
						radius * 4, 0.75f, 0.533f, 0, // yellow dlight
						@cgs.media.shaderRocketExplosion );

	Vec3 vec( crandom() * expvelocity, crandom() * expvelocity, crandom() * expvelocity );
	le.velocity = expvelocity * dir + vec;
	le.refEnt.rotation = rand() % 360;

	// explosion ring sprite
	if( cg_explosionsRing.boolean ) {
		origin = pos + radius * 0.25f * dir;
		@le = AllocSprite( LE_ALPHA_FADE, origin, radius, 3,
							 1, 1, 1, 1,
							 0, 0, 0, 0, // no dlight
							 cgs.media.shaderRocketExplosionRing );

		le.refEnt.rotation = rand() % 360;
	}

	if( cg_explosionsDust.integer == 1 ) {
		// dust ring parallel to the contact surface
		ExplosionsDust( pos, dir, radius );
	}

	// Explosion particles
	ParticleExplosionEffect( pos, dir, 1, 0.5, 0, 32 );

	if( fire_mode == FIRE_MODE_STRONG ) {
		CGame::Sound::StartFixedSound( @cgs.media.sfxGrenadeStrongExplosion, pos, CHAN_AUTO, cg_volume_effects.value, ATTN_DISTANT );
	} else {
		CGame::Sound::StartFixedSound( @cgs.media.sfxGrenadeWeakExplosion, pos, CHAN_AUTO, cg_volume_effects.value, ATTN_DISTANT );
	}
}

void GenericExplosion( const Vec3 &in pos, const Vec3 &in dir, int fire_mode, float radius, bool decal = true ) {
	const float expvelocity = 8.0f;

	//if( CG_PointContents( pos ) & MASK_WATER )
	//jalfixme: (shouldn't we do the water sound variation?)
	if( decal ) {
		if( fire_mode == FIRE_MODE_STRONG ) {
			CGame::Scene::SpawnDecal( pos, dir, random() * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, @cgs.media.shaderExplosionMark );
		} else {
			CGame::Scene::SpawnDecal( pos, dir, random() * 360, radius * 0.25, 1, 1, 1, 1, 10, 1, false, @cgs.media.shaderExplosionMark );
		}
	}

	// animmap shader of the explosion
	Vec3 origin = pos + radius * 0.15f * dir;
	LocalEntity @le = AllocSprite( LE_ALPHA_FADE, origin, radius * 0.5f, 8,
						1, 1, 1, 1,
						radius * 4, 0.75f, 0.533f, 0, // yellow dlight
						@cgs.media.shaderRocketExplosion );

	Vec3 vec( crandom() * expvelocity, crandom() * expvelocity, crandom() * expvelocity );
	le.velocity = expvelocity * dir + vec;
	le.refEnt.rotation = rand() % 360;

	// use the rocket explosion sounds
	if( fire_mode == FIRE_MODE_STRONG ) {
		CGame::Sound::StartFixedSound( @cgs.media.sfxRocketLauncherStrongHit, pos, CHAN_AUTO, cg_volume_effects.value, ATTN_DISTANT );
	} else {
		CGame::Sound::StartFixedSound( @cgs.media.sfxRocketLauncherWeakHit, pos, CHAN_AUTO, cg_volume_effects.value, ATTN_DISTANT );
	}
}

void FlagTrail( const Vec3 &in origin, const Vec3 &in start, const Vec3 &in end, const Vec3 &in rgb ) {
	const float mass = 20;

	Vec3 dir = end - start;
	float len = dir.normalize();
	if( len == 0.0f ) {
		return;
	}

	LocalEntity @le = AllocSprite( LE_SCALE_ALPHA_FADE, origin, 8, 50 + int( 50 * random() ),
						rgb.x, rgb.y, rgb.z, 0.7f,
						0, 0, 0, 0,
						@cgs.media.shaderTeleporterSmokePuff );
	le.velocity.set( -dir[0] * 5 + crandom() * 5, -dir[1] * 5 + crandom() * 5, -dir[2] * 5 + crandom() * 5 + 3 );
	le.refEnt.rotation = rand() % 360;

	//friction and gravity
	le.accel.set( -0.2f, -0.2f, -9.8f * mass );
	le.bounce = 50;
}

void Explosion1( const Vec3 &in pos ) {
	RocketExplosionMode( pos, vec3Origin, FIRE_MODE_STRONG, 150 );
}

void Explosion2( const Vec3 &in pos ) {
	GrenadeExplosionMode( pos, vec3Origin, FIRE_MODE_STRONG, 150 );
}

void GreenLaser( const Vec3 &in start, const Vec3 &in end ) {
	AllocLaser( start, end, 2.0f, 2.0f, 0.0f, 0.85f, 0.0f, 0.3f, @cgs.media.shaderLaser );
}

void DustCircle( const Vec3 &in pos, const Vec3 &in dir, float radius, int count ) {
	if( ( GS::PointContents( pos ) & MASK_WATER ) != 0 ) {
		return; // no smoke under water :)
	}

	Vec3 dir_per2 = dir.perpendicular();
	Vec3 dir_per1 = dir ^ dir_per2;

	dir_per1.normalize();
	dir_per2.normalize();

	float sector = 6.2831f / float( count );
	for( int i = 0; i < count; i++ ) {
		float angle = sector * float( i );

		Vec3 dir_temp;
		dir_temp += sin( angle ) * dir_per1;
		dir_temp += cos( angle ) * dir_per2;

		//VectorScale(dir_temp, VectorNormalize(dir_temp),dir_temp );
		dir_temp *= crandom() * 10 + radius;
		Explosion_Puff_2( pos, dir_temp, 10.0f );
	}
}

void ExplosionsDust( const Vec3 &in  pos, const Vec3 &in  dir, float radius ) {
	const int count = 32; /* Number of sprites used to create the circle */

	if( ( GS::PointContents( pos ) & MASK_WATER ) != 0 ) {
		return; // no smoke under water :
	}

	Vec3 dir_per2 = dir.perpendicular();
	Vec3 dir_per1 = dir ^ dir_per2;

	// make a circle out of the specified number (int count) of sprites
	for( int i = 0; i < count; i++ ) {
		float angle = float( 6.2831f / count * i );
		Vec3 dir_temp;

		dir_temp += sin( angle ) * dir_per1;
		dir_temp += cos( angle ) * dir_per2;

		dir_temp *= crandom() * 8 + radius + 16.0f;

		// make the sprite smaller & alpha'd
		LocalEntity @le = AllocSprite( LE_ALPHA_FADE, pos, 10, 10,
							1.0f, 1.0f, 1.0f, 1.0f,
							0, 0, 0, 0,
							@cgs.media.shaderSmokePuff3 );
		le.velocity = dir_temp;
	}
}

void DashEffect( CEntity @cent ) {
	const float IGNORE_DASH = 6.0;

	if( ( cg_cartoonEffects.integer & 4 ) == 0 ) {
		return;
	}

	// KoFFiE: Calculate angle based on relative position of the previous origin state of the player entity
	Vec3 pos = cent.current.origin;
	Vec3 dvect = pos - cent.prev.origin;

	// ugly inline define . Ignore when difference between 2 positions was less than this value.
	if( ( dvect[0] > -IGNORE_DASH ) && ( dvect[0] < IGNORE_DASH ) &&
		( dvect[1] > -IGNORE_DASH ) && ( dvect[1] < IGNORE_DASH ) ) {
		return;
	}

	Vec3 angle = dvect.toAngles();
	angle[1] += 270; // Adjust angle
	pos[2] -= 24; // Adjust the position to ground height

	if( ( GS::PointContents( pos ) & MASK_WATER ) != 0 ) {
		return; // no smoke under water :)
	}

	LocalEntity @le = AllocModel( LE_DASH_SCALE, pos, angle, 7, //5
						1.0, 1.0, 1.0, 0.2f,
						0, 0, 0, 0,
						@cgs.media.modDash,
						null );
	le.refEnt.scale = 0.01f;
	le.refEnt.axis.z.z *= 2.0f;
}

void SpawnPlayerTeleportEffect( CEntity @cent ) {
	Vec3 teleportOrigin;
	Vec3 rgb;

	for( int j = LEF_EV_PLAYER_TELEPORT_IN; j <= LEF_EV_PLAYER_TELEPORT_OUT; j++ ) {
		if( cent.localEffects[j] != 0 ) {
			cent.localEffects[j] = 0;

			rgb.set( 0.5, 0.5, 0.5 );
			if( j == LEF_EV_PLAYER_TELEPORT_OUT ) {
				teleportOrigin = cent.teleportedFrom;
			} else {
				teleportOrigin = cent.teleportedTo;
				if( IsViewerEntity( cent.current.number ) ) {
					rgb.set( 0.1, 0.1, 0.1 );
				}
			}

			if( cg_raceGhosts.boolean && !IsViewerEntity( cent.current.number ) && GS::RaceGametype() ) {
				rgb *= cg_raceGhostsAlpha.value;
			}

			// spawn a dummy model
			LocalEntity @le = AllocModel( LE_RGB_FADE, teleportOrigin, vec3Origin, 10,
								rgb.x, rgb.y, rgb.z, 1, 0, 0, 0, 0, @cent.refEnt.model,
								@cgs.media.shaderTeleportShellGfx );

			@le.skel = @cent.skel;
			@le.refEnt.model = @cent.refEnt.model;
			le.refEnt.frame = cent.refEnt.frame;
			le.refEnt.oldFrame = cent.refEnt.frame;
			le.refEnt.backLerp = 1.0f;
			le.refEnt.axis = cent.refEnt.axis;
		}
	}
}

void SmallPileOfGibs( const Vec3 &in origin, int damage, const Vec3 &in initialVelocity, int team ) {
	if( !cg_gibs.boolean ) {
		return;
	}

	int time = 50;
	int count = bound( 15, 14 + cg_gibs.integer, 128 ); // 15 models minimum

	for( int i = 0; i < count; i++ ) {
		Vec4 color( 60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f ); // grey

		// coloring
		switch( rand() % 3 ) {
			case 0:
				// orange
				color.set( 1, 0.5, 0, 1 );
				break;
			case 1:
				// purple
				color.set( 1, 0, 1, 1 );
				break;
			case 2:
			default:
				if( ( team == TEAM_ALPHA ) || ( team == TEAM_BETA ) ) {
					// team
					Vec4 fcolor = TeamColor( team );
					for( int j = 0; j < 3; j++ ) {
						color[j] = bound( 60.0f / 255.0f, fcolor[j], 1.0f );
					}
					color[3] = fcolor[3];
				}
				break;
		}

		LocalEntity @le = AllocModel( LE_ALPHA_FADE, origin, vec3Origin, time + int( time * random() ),
							color[0], color[1], color[2], color[3],
							0, 0, 0, 0,
							@cgs.media.modIlluminatiGibs,
							null );

		// random rotation and scale variations
		Vec3 angles ( crandom() * 360, crandom() * 360, crandom() * 360 );
		angles.anglesToAxis( le.refEnt.axis );
		le.refEnt.scale = 0.8f - ( random() * 0.25 );
		le.refEnt.renderfx = RF_FULLBRIGHT | RF_NOSHADOW;

		Vec3 velocity( crandom() * 0.5, crandom() * 0.5, 0.5 + random() * 0.5 );
		velocity.normalize();
		velocity *= min( damage * 10, 300 );

		velocity.x += crandom() * bound( 0, damage, 150 );
		velocity.y += crandom() * bound( 0, damage, 150 );
		velocity.z +=  random() * bound( 0, damage, 250 );

		le.velocity = initialVelocity + velocity;
		le.avelocity[0] = random() * 1200;
		le.avelocity[1] = random() * 1200;
		le.avelocity[2] = random() * 1200;

		//friction and gravity
		le.accel.set( -0.2f, -0.2f, -900 );

		le.bounce = 75;
	}
}

void AddLocalEntities( void ) {
	float time = float( cg.frameTime ) * 0.001f;
	float backLerp = 1.0f - cg.lerpfrac;

	for( List::Element @hnode = @activeLocalEntities.hnode, iter = @hnode.next, next = @hnode; @iter != @hnode; @iter = @next ) {
		@next = @iter.next;

		LocalEntity @le;
		iter.value.retrieve( @le );

		float scale, fade, fadeIn;

		float frac = float( cg.time - le.start ) * 0.01f;
		int f = int( floor( frac ) );
		if( f < 0 ) f = 0;

		// it's time to DIE
		if( f >= le.frames - 1 ) {
			le.type = LE_FREE;
			activeLocalEntities.remove( @iter );
			freeLocalEntities.pushBack( iter.value );
			continue;
		}

		if( le.frames > 1 ) {
			scale = 1.0f - frac / float( le.frames - 1 );
			scale = bound( 0.0f, scale, 1.0f );
			fade = scale * 255.0f;

			// quick fade in, if time enough
			if( le.frames > FADEINFRAMES * 2 ) {
				float scaleIn = bound( 0.0f, frac / float ( FADEINFRAMES ), 1.0f );
				fadeIn = scaleIn * 255.0f;
			} else {
				fadeIn = 255.0f;
			}
		} else {
			scale = 1.0f;
			fade = 255.0f;
			fadeIn = 255.0f;
		}

		auto @ent = @le.refEnt;

		if( le.light != 0 && scale != 0.0f ) {
			CGame::Scene::AddLightToScene( ent.origin, le.light * scale, Vec3ToColor( le.lightcolor ) );
		}

		if( le.type == LE_LASER ) {
			//TODO
			//CG_QuickPolyBeam( ent.origin, ent.origin2, ent.radius, ent.customShader ); // wsw : jalfixme: missing the color (comes inside ent.skinnum)
			continue;
		}

		if( le.type == LE_DASH_SCALE ) {
			if( f < 1 ) {
				ent.scale = 0.15 * frac;
			} else {
				Vec3 fw = ent.axis.x, r = ent.axis.y, u = ent.axis.z;
				Vec3 angles = u.toAngles();

				u.z -= 0.052f;              //height
				if( u.z <= 0.0f ) {
					le.type = LE_FREE;
					activeLocalEntities.remove( @iter );
					freeLocalEntities.pushBack( any( @le ) );
					continue;
				}

				u.y += 0.005f * sin( deg2rad( angles[YAW] ) ); //length
				u.x += 0.005f * cos( deg2rad( angles[YAW] ) ); //length
				fw.y += 0.008f * cos( deg2rad( angles[YAW] ) ); //width
				fw.x -= 0.008f * sin( deg2rad( angles[YAW] ) ); //width
				ent.axis = Mat3( fw, r, u );
			}
		}
		if( le.type == LE_PUFF_SCALE ) {
			if( le.frames - f < 4 ) {
				ent.scale = 1.0f - 1.0f * ( frac - abs( 4 - le.frames ) ) / 4;
			}
		}
		if( le.type == LE_PUFF_SHRINK ) {
			if( frac < 3 ) {
				ent.scale = 1.0f - 0.2f * frac / 4;
			} else {
				ent.scale = 0.8 - 0.8 * ( frac - 3 ) / 3;
				le.velocity *= 0.85f;
			}
		}

		if( le.type == LE_EXPLOSION_TRACER ) {
			if( cg.time - ent.rotation > 10.0f ) {
				ent.rotation = cg.time;
				if( ent.radius - 16.0f * frac > 4 ) {
					Explosion_Puff( ent.origin, ent.radius - 16 * frac, le.frames - f );
				}
			}
		}

		switch( le.type ) {
			case LE_NO_FADE:
				break;
			case LE_RGB_FADE:
				fade = min( fade, fadeIn );
				ent.shaderRGBA = COLOR_RGBA( uint8( le.color[0] * fade ), uint8( le.color[1] * fade ), uint8( le.color[2] * fade ), 255 );
				break;
			case LE_SCALE_ALPHA_FADE:
				fade = min( fade, fadeIn );
				ent.scale = 1.0f + 1.0f / scale;
				ent.scale = min( ent.scale, 5.0f );
				ent.shaderRGBA = COLOR_REPLACEA( ent.shaderRGBA, uint8( fade * le.color[3] ) );
				break;
			case LE_INVERSESCALE_ALPHA_FADE:
				fade = min( fade, fadeIn );
				ent.scale = bound( 0.1f, scale + 0.1f, 1.0f );
				ent.shaderRGBA = COLOR_REPLACEA( ent.shaderRGBA, uint8( fade * le.color[3] ) );
				break;
			case LE_ALPHA_FADE:
				fade = min( fade, fadeIn );
				ent.shaderRGBA = COLOR_REPLACEA( ent.shaderRGBA, uint8( fade * le.color[3] ) );
				break;
			default:
				break;
		}

		ent.backLerp = backLerp;

		if( le.avelocity[0] != 0.f || le.avelocity[1] != 0.f || le.avelocity[2] != 0.f ) {
			le.angles += time * le.avelocity;
			le.angles.anglesToAxis( ent.axis );
		}

		// apply rotational friction
		if( le.bounce != 0 ) { // FIXME?
			int i;
			const float adj = 100 * 6 * time; // magic constants here

			for( i = 0; i < 3; i++ ) {
				if( le.avelocity[i] > 0.0f ) {
					le.avelocity[i] -= adj;
					if( le.avelocity[i] < 0.0f ) {
						le.avelocity[i] = 0.0f;
					}
				} else if( le.avelocity[i] < 0.0f ) {
					le.avelocity[i] += adj;
					if( le.avelocity[i] > 0.0f ) {
						le.avelocity[i] = 0.0f;
					}
				}
			}
		}

		if( le.bounce != 0 ) {
			Vec3 next_origin = ent.origin + time * le.velocity;

			Trace trace;
			trace.doTrace( ent.origin, debrisMins, debrisMaxs, next_origin, 0, MASK_SOLID );

			// remove the particle when going out of the map
			if( ( trace.contents & CONTENTS_NODROP ) != 0 || ( trace.surfFlags & SURF_SKY ) != 0 ) {
				le.frames = 0;
			} else if( trace.fraction != 1.0 ) {   // found solid
				float orig_xyzspeed = le.velocity.length();

				// Reflect velocity
				float dot =  le.velocity * trace.planeNormal;
				le.velocity -= 2.0f * dot * trace.planeNormal;

				//put new origin in the impact point, but move it out a bit along the normal
				ent.origin = trace.endPos + trace.planeNormal;

				// make sure we don't gain speed from bouncing off
				float bounce = 2.0f * le.bounce * 0.01f;
				if( bounce < 1.5f ) {
					bounce = 1.5f;
				}
				float xyzspeed = orig_xyzspeed / bounce;

				le.velocity.normalize();
				le.velocity *= xyzspeed;

				//the entity has not speed enough. Stop checks
				if( xyzspeed * time < 1.0f ) {
					//see if we have ground
					Vec3 ground_origin = ent.origin;
					ground_origin.z += ( debrisMins.z - 4 );

					Trace traceground;
					traceground.doTrace( ent.origin, debrisMins, debrisMaxs, ground_origin, 0, MASK_SOLID );
					if( traceground.fraction != 1.0 ) {
						le.bounce = 0;
						le.velocity.clear();
						le.accel.clear();
						le.avelocity.clear();
						if( le.type == LE_EXPLOSION_TRACER ) {
							// blx
							le.type = LE_FREE;
							activeLocalEntities.remove( @iter );
							freeLocalEntities.pushBack( iter.value );
							continue;
						}
					}
				}

			} else {
				ent.origin2 = ent.origin;
				ent.origin = next_origin;
			}
		} else {
			ent.origin2 = ent.origin;
			ent.origin += time * le.velocity;
		}

		if( @le.skel != null ) {
			// get space in cache, interpolate, transform, link
			@le.refEnt.boneposes = CGame::Scene::RegisterTemporaryExternalBoneposes( @le.skel );
			@le.refEnt.oldBoneposes = @le.refEnt.boneposes;
			CGame::Scene::LerpSkeletonPoses( @le.skel, le.refEnt.frame, le.refEnt.oldFrame, @le.refEnt.boneposes, 1.0 - le.refEnt.backLerp );
			CGame::Scene::TransformBoneposes( @le.skel, @le.refEnt.boneposes, @le.refEnt.boneposes );
		}

		ent.lightingOrigin = ent.origin;
		le.velocity += time * le.accel;

		CGame::Scene::AddEntityToScene( @ent );
	}
}

}

}