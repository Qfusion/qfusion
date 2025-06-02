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

	Scene::AddEntityToScene( @shell );
}

void AddShellEffects( CGame::Scene::Entity @ent, int effects ) {
	if( ( effects & EF_RACEGHOST ) != 0 ) {
		AddRaceGhostShell( @ent );
	}
}

void WeaponBeamEffect( CEntity @cent ) {
	CGame::Scene::Orientation projection;

	if( cent.localEffects[LEF_EV_WEAPONBEAM] == 0 ) {
		return;
	}

	// now find the projection source for the beam we will draw
	if( !cent.GetPModelProjectionSource( projection ) ) {
		projection.origin = cent.laserOrigin;
	}

	if( cent.localEffects[LEF_EV_WEAPONBEAM] == WEAP_ELECTROBOLT ) {
		LE::ElectroTrail2( projection.origin, cent.laserPoint, cent.current.team );
	} else {
		InstaPolyBeam( projection.origin, cent.laserPoint, cent.current.team );
	}

	cent.localEffects[LEF_EV_WEAPONBEAM] = 0;
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

	Scene::SpawnParticleEffect( @ef, org, dir, count );
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

	Scene::SpawnParticleEffect( @ef, org, dir, count );
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

	Scene::SpawnParticleEffect( @ef, org, dir, count );
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

	Scene::SpawnParticleEffect( @ef, start, vec3Origin, int( len / dec ) + 1 );
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

	Scene::SpawnParticleEffect( @ef, start, vec3Origin, int( len / dec ) + 1 );
}

void ImpactPuffParticles( const Vec3 &in org, const Vec3 &in dir, int count, float scale, float r, float g, float b, float a ) {
	CGame::Scene::ParticleEffect ef;
	ef.size = scale;
	ef.alphaDecay.set( 0.5, 0.8 );
	ef.color.set( r, g, b, a );
	ef.orgRand.set( -4.0f, 4.0f );
	ef.dirRand.set( 0.0f, 15.0f );
	ef.dirMAToVel = 90.0f;
	ef.velRand.set( -40.0f, 40.0f );
	ef.accel[2] = -PARTICLE_GRAVITY;

	Scene::SpawnParticleEffect( @ef, org, dir, count );
}

/*
* High velocity wall impact puffs
*/
void HighVelImpactPuffParticles( const Vec3 &in org, const Vec3 &in dir, int count, float scale, float r, float g, float b, float a ) {
	CGame::Scene::ParticleEffect ef;
	ef.size = scale;
	ef.alphaDecay.set( 0.1, 0.16 );
	ef.color.set( r, g, b, a );
	ef.orgRand.set( -4.0f, 4.0f );
	ef.dirRand.set( 0.0f, 15.0f );
	ef.dirMAToVel = 180.0f;
	ef.velRand.set( -40.0f, 40.0f );
	ef.accel[2] = -PARTICLE_GRAVITY * 2;

	Scene::SpawnParticleEffect( @ef, org, dir, count );
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

	Scene::SpawnParticleEffect( @ef, start, vec3Origin, count );
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

	Scene::SpawnParticleEffect( @ef, origin, vec3Origin, count );
}

//============================================================
//
//      PLAYER LASER BEAM EFFECTS
//
//============================================================

CEntity@ laserOwner = null;
int laserLightColor = Vec3ToColor(Vec3(0.75f, 0.75f, 0.375f));

Vec4 _LaserColor() {
    Vec4 color(1, 1, 1, 1);
    if (cg_teamColoredBeams.boolean && laserOwner !is null && (laserOwner.current.team == TEAM_ALPHA || laserOwner.current.team == TEAM_BETA)) {
        color = TeamColor(laserOwner.current.team);
    }
    return color;
}

class LaserImpactCb : GS::Weapons::ITraceImpact {
    void impact(Trace trace, Vec3 dir) override {
        if (trace.entNum < 0) {
            return;
        }

        if (laserOwner !is null) {
            // density as quantity per second
            const int TRAILTIME = int(1000.0f / 20.0f);
            if (laserOwner.localEffects[LEF_LASERBEAM_SMOKE_TRAIL] + TRAILTIME < cg.time) {
                laserOwner.localEffects[LEF_LASERBEAM_SMOKE_TRAIL] = cg.time;

                HighVelImpactPuffParticles(trace.endPos, trace.planeNormal, 8, 0.5f, 1.0f, 0.8f, 0.2f, 1.0f);

                Sound::StartFixedSound(@cgs.media.sfxLasergunHit[rand() % 3], trace.endPos, CHAN_AUTO,
                    cg_volume_effects.value, ATTN_STATIC);
            }
        }

        // it's a brush model
        EntityState@ entState = GS::GetEntityState(trace.entNum, 0);
        if (trace.entNum == 0 || entState is null || (entState.effects & EF_TAKEDAMAGE) == 0) {
            Vec4 color = _LaserColor();
            Vec3 origin = trace.endPos + trace.planeNormal * IMPACT_POINT_OFFSET;

            LE::LaserGunImpact(origin, 15.0f, dir, color);

            Scene::AddLightToScene(origin, 100.0f, laserLightColor);
            return;
        }

        // it's a player
        // TODO: add player-impact model
    }
}

void LaserBeamEffect(CEntity@ cent)
{
    int i, j;
    SoundHandle@ sound = null;
    float range;
    bool firstPerson;
    Trace trace;
    CGame::Scene::Orientation projectsource;
    Vec4 color = _LaserColor();
    Vec3 laserOrigin, laserAngles, laserPoint, laserDir;
    auto @cur = @cent.current;

    if (cent.localEffects[LEF_LASERBEAM] <= cg.time) {
        if (cent.localEffects[LEF_LASERBEAM] != 0) {
            if (!cent.laserCurved) {
                @sound = @cgs.media.sfxLasergunStrongStop;
            } else {
                @sound = @cgs.media.sfxLasergunWeakStop;
            }

            if (IsViewerEntity(cur.number)) {
                Sound::StartGlobalSound(sound, CHAN_AUTO, cg_volume_effects.value);
            } else {
                Sound::StartRelativeSound(sound, cur.number, CHAN_AUTO, cg_volume_effects.value, ATTN_NORM);
            }
        }
        cent.localEffects[LEF_LASERBEAM] = 0;
        return;
    }

    @laserOwner = cent;

    // interpolate the positions
    firstPerson = (IsViewerEntity(cur.number) && !Camera::GetMainCamera().thirdPerson);

    if (firstPerson) {
        auto @pps = @PredictedPlayerState;
        laserOrigin = pps.pmove.origin;
        laserOrigin.z += pps.viewHeight;
        laserAngles = pps.viewAngles;
        laserDir = laserAngles.anglesToForward();

        laserPoint = cent.laserPointOld + cg.lerpfrac * (cent.laserPoint - cent.laserPointOld);
    } else {
        laserOrigin = cent.laserOriginOld + cg.lerpfrac * (cent.laserOrigin - cent.laserOriginOld);
        laserPoint = cent.laserPointOld + cg.lerpfrac * (cent.laserPoint - cent.laserPointOld);
        if (!cent.laserCurved) {
            // make up the angles from the start and end points (s->angles is not so precise)
            laserDir = (laserPoint - laserOrigin);
            laserDir.normalize();
        } else {
            for (i = 0; i < 3; i++)
                laserAngles[i] = LerpAngle(cent.prev.angles[i], cur.angles[i], cg.lerpfrac);
            laserDir = laserAngles.anglesToForward();
        }
    }

    auto impactCb = LaserImpactCb();

    if (!cent.laserCurved) {
        range = GS::Weapons::getWeaponDef(WEAP_LASERGUN).fireDef.timeout;

        if ((cur.effects & EF_QUAD) != 0) {
            @sound = @cgs.media.sfxLasergunStrongQuadHum;
        } else {
            @sound = @cgs.media.sfxLasergunStrongHum;
        }

        // trace the beam: for tracing we use the real beam origin
        trace = GS::Weapons::TraceLaserBeam(laserOrigin, laserDir, range, cur.number, 0, @impactCb);

        // draw the beam: for drawing we use the weapon projection source (already handles the case of viewer entity)
        if (cgEnts[cur.number].GetPModelProjectionSource(projectsource)) {
            laserOrigin = projectsource.origin;
        }

        Scene::KillPolysByTag(cur.number);

        for (int phase = 0; phase < 3; phase++) {
            ElectroPolyboardBeam(laserOrigin, trace.endPos, cg_laserBeamSubdivisions.integer,
                float(phase), range, color, cur.number, firstPerson);
        }
    } else {
        float subdivisions = float(cg_laserBeamSubdivisions.integer);
        Vec3 from, dir, end, blendPoint;
        int passthrough = cur.number;
        Vec3 tmpangles, blendAngles;

        range = GS::Weapons::getWeaponDef(WEAP_LASERGUN).fireDefWeak.timeout;

        if ((cur.effects & EF_QUAD) != 0) {
            @sound = @cgs.media.sfxLasergunWeakQuadHum;
        } else {
            @sound = @cgs.media.sfxLasergunWeakHum;
        }

        // trace the beam: for tracing we use the real beam origin
        trace = GS::Weapons::TraceCurveLaserBeam(laserOrigin, laserAngles, laserPoint, cur.number, 0, @impactCb);

        // draw the beam: for drawing we use the weapon projection source (already handles the case of viewer entity)
        if (!cgEnts[cur.number].GetPModelProjectionSource(projectsource)) {
            projectsource.origin = laserOrigin;
        }

        if (subdivisions < GS::Weapons::CURVELASERBEAM_SUBDIVISIONS) {
            subdivisions = float(GS::Weapons::CURVELASERBEAM_SUBDIVISIONS);
        }

        Scene::KillPolysByTag(cur.number);

        blendPoint = laserPoint;
        from = projectsource.origin;
        dir = blendPoint - projectsource.origin;
        blendAngles = dir.toAngles();

        for (i = 1; i <= int(subdivisions); i++) {
            float frac = ((range / subdivisions) * float(i)) / float(range);

            for (j = 0; j < 3; j++)
                tmpangles[j] = LerpAngle(laserAngles[j], blendAngles[j], frac);

            dir = tmpangles.anglesToForward();
            end = projectsource.origin + dir * (range * frac);

            trace = GS::Weapons::TraceLaserBeam(from, dir, (from - end).length(), passthrough, 0, null);
            LaserGunPolyBeam(from, trace.endPos, color, cur.number);
            if (trace.fraction != 1.0f) {
                break;
            }

            passthrough = trace.entNum;
            from = trace.endPos;
        }
    }

    // enable continuous flash on the weapon owner
    if (cg_weaponFlashes.boolean) {
        auto @weaponInfo = @cgs.weaponModelInfo[WEAP_LASERGUN];
        cgEnts[cur.number].pmodel.flashTime = cg.time + weaponInfo.flashTime;
    }

    if (sound !is null) {
        if (IsViewerEntity(cur.number)) {
            Sound::AddLoopSound(sound, cur.number, cg_volume_effects.value, ATTN_NONE);
        } else {
            Sound::AddLoopSound(sound, cur.number, cg_volume_effects.value, ATTN_STATIC);
        }
    }

    @laserOwner = null;
}

}
