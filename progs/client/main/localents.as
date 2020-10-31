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

	switch( int( floor( crandom() * 3.0f ) ) ) {
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

void DustCircle( const Vec3 &in pos, const Vec3 &in dir, float radius, int count )
{
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
				ent.shaderRGBA = Vec4ToColor( Vec4( le.color[0] * fade, le.color[1] * fade, le.color[2] * fade, 1.0f ) );
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

		ent.lightingOrigin = ent.origin;
		le.velocity += time * le.accel;

		CGame::Scene::AddEntityToScene( @ent );
	}
}

}

}