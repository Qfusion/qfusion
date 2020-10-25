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
array<LocalEntity> localEntities(MAX_LOCAL_ENTITIES);
List::List freeLocalEntities;
List::List activeLocalEntities;

void Init() {
    freeLocalEntities.Init();
    activeLocalEntities.Init();
    for( int i = 0; i < MAX_LOCAL_ENTITIES; i++ ) {
        freeLocalEntities.PushBack( any( @localEntities[i] ) );
    }
}

LocalEntity @Alloc( eLocalEntityType type, float r, float g, float b, float a ) {
	LocalEntity @le;

	if( !freeLocalEntities.Empty() ) { // take a free decal if possible
        any v = freeLocalEntities.Back().value;
		v.retrieve( @le );
	} else {              // grab the oldest one otherwise
        any v = activeLocalEntities.Front().value;
        v.retrieve( @le );
	}

    le.refEnt.reset();
    le.type = type;
    le.start = cg.time;
    le.color = Vec4( r, g, b, a );
	return @le;
}

LocalEntity @AllocModel( eLocalEntityType type, const Vec3 &in origin, const Vec3 &in angles, int frames,
    float r, float g, float b, float a, float light, float lr, float lg, float lb, ModelHandle @model, ShaderHandle @shader ) {
	LocalEntity @le;

	@le = Alloc( type, r, g, b, a );
	le.frames = frames;
	le.light = light;
    le.lightcolor = Vec3( lr, lg, lb );

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

	le.refEnt.rtype = RT_SPRITE;
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

}

}