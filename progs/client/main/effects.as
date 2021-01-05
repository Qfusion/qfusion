namespace CGame {

void AddRaceGhostShell( CGame::Scene::Entity @ent ) {
	CGame::Scene::Entity shell;
	float alpha = cg_raceGhostsAlpha.value;

	alpha = bound( alpha, 0.0f, 1.0f );

	shell = ent;
	@shell.customSkin = null;

	if( ( shell.renderfx & RF_WEAPONMODEL ) != 0 ) {
		return;
	}

	@shell.customShader = cgs.media.shaderRaceGhostEffect;
	shell.renderfx |= ( RF_FULLBRIGHT | RF_NOSHADOW );

	Vec4 c = ColorToVec4( shell.shaderRGBA );
	shell.shaderRGBA = Vec4ToColor( Vec4( c[0]*alpha, c[1]*alpha, c[2]*alpha, alpha ) );

	CGame::Scene::AddEntityToScene( @shell );
}

void AddShellEffects( CGame::Scene::Entity @ent, int effects ) {
	if( ( effects & EF_RACEGHOST ) != 0 ) {
		AddRaceGhostShell( @ent );
	}
}

/*
* Wall impact puffs
*/
void SplashParticles( const Vec3 &in org, const Vec3 &in dir, float r, float g, float b, int count )
{
	CGame::Scene::ParticleEffect ef;
	ef.size = 0.75f;
	ef.alphaDecay.set( 0.5, 0.8 );
	ef.color.set( r, g, b, 1.0f );
	ef.colorRand.set( 0.1, 0.1, 0.1, 0.0f );
	ef.orgRand.set( -4.0f, 4.0f );
	ef.dirRand.set( 0.0f, 31.0f );
	ef.velRand.set( -20.0f, 20.0f );
	ef.accel[2] = -PARTICLE_GRAVITY;

	CGame::Scene::SpawnParticleEffect( @ef, org, dir, count );
}

void SplashParticles2( const Vec3 &in org, const Vec3 &in dir, float r, float g, float b, int count )
{
	CGame::Scene::ParticleEffect ef;
	ef.size = 0.75f;
	ef.alphaDecay.set( 0.5, 0.8 );
	ef.color.set( r, g, b, 1.0f );
	ef.orgRand.set( -4.0f, 4.0f );
	ef.dirRand.set( 0.0f, 7.0f );
	ef.velRand.set( -20.0f, 20.0f );
	ef.accel[2] = -PARTICLE_GRAVITY;

	CGame::Scene::SpawnParticleEffect( @ef, org, dir, count );
}

void ParticleExplosionEffect( const Vec3 &in org, const Vec3 &in dir, float r, float g, float b, int count ) {
	CGame::Scene::ParticleEffect ef;
	ef.size = 0.75f;
	ef.alphaDecay.set( 0.7, 0.95 );
	ef.color.set( r, g, b, 1.0f );
	ef.colorRand.set( 0.1, 0.1, 0.1, 0.0f );
	ef.orgRand.set( -4.0f, 4.0f );
	ef.dirRand.set( 0.0f, 31.0f );
	ef.velRand.set( -400.0f, 400.0f );
	ef.accel[2] = -PARTICLE_GRAVITY;

	CGame::Scene::SpawnParticleEffect( @ef, org, dir, count );
}

void BlasterTrail( const Vec3 &in start, const Vec3 &in end ) {
	const float dec = 3.0f;

	Vec3 move = end - start;
	float len = move.normalize();
	move *= dec;

	CGame::Scene::ParticleEffect ef;
	ef.size = 2.5f;
	ef.alphaDecay.set( 0.1, 0.3 );
	ef.color.set( 1.0f, 0.85f, 0, 0.25f );
	ef.orgRand.set( -1.0f, 1.0f );
	ef.velRand.set( -5.0f, 5.0f );
	ef.orgSpread = move;

	CGame::Scene::SpawnParticleEffect( @ef, start, vec3Origin, int( len / dec ) + 1 );
}

void ElectroWeakTrail( const Vec3 &in start, const Vec3 &in end ) {
	const float dec = 5;

	Vec3 move = end - start;
	float len = move.normalize();
	move *= dec;

	CGame::Scene::ParticleEffect ef;
	ef.size = 2.0f;
	ef.alphaDecay.set( 0.2, 0.3 );
	ef.color.set( 1.0f, 1.0f, 1.0f, 0.8f );
	ef.orgRand.set( 0.0f, 1.0f );
	ef.velRand.set( -2.0f, 2.0f );
	ef.orgSpread = move;

	CGame::Scene::SpawnParticleEffect( @ef, start, vec3Origin, int( len / dec ) + 1 );
}

void ImpactPuffParticles( const Vec3 &in org, const Vec3 &in dir, int count, float scale, float r, float g, float b, float a, ShaderHandle @shader ) {
	CGame::Scene::ParticleEffect ef;
	ef.size = scale;
	ef.alphaDecay.set( 0.5, 0.8 );
	ef.color.set( r, g, b, a );
	ef.orgRand.set( -4.0f, 4.0f );
	ef.dirRand.set( 0.0f, 15.0f );
	ef.dirMAToVel = 90.0f;
	ef.velRand.set( -40.0f, 40.0f );
	ef.accel[2] = -PARTICLE_GRAVITY;

	CGame::Scene::SpawnParticleEffect( @ef, org, dir, count );
}

/*
* High velocity wall impact puffs
*/
void HighVelImpactPuffParticles( const Vec3 &in org, const Vec3 &in dir, int count, float scale, float r, float g, float b, float a, ShaderHandle @shader ) {
	CGame::Scene::ParticleEffect ef;
	ef.size = scale;
	ef.alphaDecay.set( 0.1, 0.16 );
	ef.color.set( r, g, b, a );
	ef.orgRand.set( -4.0f, 4.0f );
	ef.dirRand.set( 0.0f, 15.0f );
	ef.dirMAToVel = 180.0f;
	ef.velRand.set( -40.0f, 40.0f );
	ef.accel[2] = -PARTICLE_GRAVITY * 2;

	CGame::Scene::SpawnParticleEffect( @ef, org, dir, count );
}

void ElectroIonsTrail( const Vec3 &in start, const Vec3 &in end, const Vec4 color ) {
	float dec = 8.0f;
	const int MAX_RING_IONS = 96;

	Vec3 move = end - start;
	float len = move.normalize();

	int count = int( len / dec ) + 1;
	if( count > MAX_RING_IONS ) {
		count = MAX_RING_IONS;
		dec = len / count;
	}
	move *= dec;

	CGame::Scene::ParticleEffect ef;
	ef.size = 0.65f;
	ef.alphaDecay.set( 0.6, 1.2 );
	ef.color = color;
	ef.colorRand.set( 0.1, 0.1, 0.1, 0.0f );
	ef.orgSpread = move;

	CGame::Scene::SpawnParticleEffect( @ef, start, vec3Origin, count );
}

/*
* FlyEffect
*/
void FlyEffect( CEntity @ent, const Vec3 &in origin ) {
	int count;
	int64 starttime;
	const float BEAMLENGTH = 16.0f;

	if( !cg_particles.boolean ) {
		return;
	}

	if( ent.flyStopTime < cg.time ) {
		starttime = cg.time;
		ent.flyStopTime = cg.time + 60000;
	} else {
		starttime = ent.flyStopTime - 60000;
	}

	int64 n = cg.time - starttime;
	if( n < 20000 ) {
		count = int( float(n) * 162 / 20000.0 );
	} else {
		n = ent.flyStopTime - cg.time;
		if( n < 20000 ) {
			count = int( float(n) * 162 / 20000.0 );
		} else {
			count = 162;
		}
	}

	CGame::Scene::ParticleEffect ef;
	ef.color.set( 0, 0, 0, 1.0f );
	ef.type = PE_FLY;
	ef.size = BEAMLENGTH;

	CGame::Scene::SpawnParticleEffect( @ef, origin, vec3Origin, count );
}

}
