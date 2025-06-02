
namespace CGame {
	
/*
* LeadWaterSplash
*/
void LeadWaterSplash( const Trace &in tr ) {
	int contents = tr.contents;
	Vec3 pos = tr.endPos;
	Vec3 dir = tr.planeNormal;

	if( ( contents & CONTENTS_WATER ) != 0 ) {
		SplashParticles( pos, dir, 0.47f, 0.48f, 0.8f, 8 );
	} else if( ( contents & CONTENTS_SLIME ) != 0 ) {
		SplashParticles( pos, dir, 0.0f, 1.0f, 0.0f, 8 );
	} else if( ( contents & CONTENTS_LAVA ) != 0 ) {
		SplashParticles( pos, dir, 1.0f, 0.67f, 0.0f, 8 );
	}
}

void BulletImpact(const Trace &in tr) {
    // bullet impact
    LE::BulletExplosion(tr.endPos, tr);

    // throw particles on dust
    if (cg_particles.boolean && (tr.surfFlags & SURF_DUST) != 0) {
        SplashParticles(tr.endPos, tr.planeNormal, 0.30f, 0.30f, 0.25f, 1);
    }

    // spawn decal
    CGame::Scene::SpawnDecal(tr.endPos, tr.planeNormal, float(random() * 360.0),  
       8, 1, 1, 1, 1, 8, 1, false,
       cgs.media.shaderBulletMark);
}

/*
* LeadBubbleTrail
*/
void LeadBubbleTrail(Trace tr, const Vec3 &in waterStart) {
    // If went through water, determine where the end is and make a bubble trail
    Vec3 dir = tr.endPos - waterStart;
    dir.normalize();
    Vec3 pos = tr.endPos + dir * -2.0;

    if( (GS::PointContents(pos) & MASK_WATER) != 0 ) {
        tr.endPos = pos;
    } else {
        tr.doTrace(pos, Vec3(0,0,0), Vec3(0,0,0), waterStart, tr.entNum, MASK_WATER);
    }

    pos = waterStart + tr.endPos;
    pos *= 0.5;

    LE::BubbleTrail(waterStart, tr.endPos, 32);
}

void Event_FireMachinegun(const Vec3 &in origin, const Vec3 &in fv, const Vec3 &in rv, 
	const Vec3 &in uv, int weapon, int fireMode, int seed, int owner) {
    double r, u;
    double alpha, s;
    Trace tr, wtr;
    const GS::Weapons::WeaponDef@ weaponDef = GS::Weapons::getWeaponDef(weapon);
    const GS::Weapons::FireDef@ fireDef = (fireMode != 0) ? weaponDef.fireDef : weaponDef.fireDefWeak;
    int range = fireDef.timeout;
    int hspread = fireDef.spread;
    int vspread = fireDef.vSpread;

    // circle shape
	Rand rnd = Rand( seed );
    alpha = M_PI * rnd.Cdouble(); // [-PI ..+PI]
    s = abs( rnd.Cdouble()); // [0..1]
    r = s * cos(alpha) * hspread;
    u = s * sin(alpha) * vspread;

	bool water = GS::Weapons::TraceBullet4D(tr, wtr, origin, fv, rv, uv, r, u, range, owner, 0);
    if (water) {
        if (wtr.endPos != origin) {
            LeadWaterSplash(wtr);
        }
    }

    if (tr.entNum != -1 && (tr.surfFlags & SURF_NOIMPACT) == 0) {
        BulletImpact(tr);

        if (!water) {
            if ((tr.surfFlags & SURF_FLESH) != 0 ||
                (tr.entNum > 0 && cgEnts[tr.entNum].current.type == ET_PLAYER) ||
                (tr.entNum > 0 && cgEnts[tr.entNum].current.type == ET_CORPSE)) {
                // flesh impact sound
            } else {
                ImpactPuffParticles(tr.endPos, tr.planeNormal, 1, 0.7, 1, 0.7, 0.0, 1.0);

                CGame::Sound::StartFixedSound(
                    @cgs.media.sfxRic[rand() % 2],
                    tr.endPos,
                    CHAN_AUTO,
                    cg_volume_effects.value,
                    ATTN_STATIC
                );
            }
        }
    }

    if (water) {
        LeadBubbleTrail(tr, wtr.endPos);
    }
}

/*
* CG_Fire_SunflowerPattern
*/
void Fire_SunflowerPattern(const Vec3 &in start, 
	const Vec3 &in fv, const Vec3 &in rv,  const Vec3 &in uv,
	int seed, int ignore, int count, int hspread, int vspread, int range) {
	Rand rnd = Rand(seed);

    for (int i = 0; i < count; i++) {
        double fi = float(i) * 2.4f; // magic value creating Fibonacci numbers
        double r = cos(float(seed) + fi) * hspread * sqrt(fi);
        double u = sin(float(seed) + fi) * vspread * sqrt(fi);

        Trace tr, wtr;
        bool water = GS::Weapons::TraceBullet4D(tr, wtr, start, fv, rv, uv, r, u, range, ignore, 0);
        if (water) {
            if (wtr.endPos != start) {
                LeadWaterSplash(wtr);
            }
        }

        if (tr.entNum != -1 && (tr.surfFlags & SURF_NOIMPACT) == 0) {
            BulletImpact(tr);
        }

        if (water) {
            LeadBubbleTrail(tr, wtr.endPos);
        }
    }
}

/*
* CG_Event_FireRiotgun
*/

void Event_FireRiotgun(const Vec3 &in origin, const Vec3 &in fv, const Vec3 &in rv, const Vec3 &in uv,
    int weapon, int fireMode, int seed, int owner) {
    Trace tr;
    Vec3 end;
    const GS::Weapons::WeaponDef@ weaponDef = GS::Weapons::getWeaponDef(weapon);
    const GS::Weapons::FireDef@ fireDef = (fireMode != 0) ? weaponDef.fireDef : weaponDef.fireDefWeak;

    Fire_SunflowerPattern(origin, fv, rv, uv, seed, owner, fireDef.projectileCount,
        fireDef.spread, fireDef.vSpread, fireDef.timeout);

    // spawn a single sound at the impact
    end = origin + fv * double(fireDef.timeout);
	tr.doTrace(origin, Vec3(0,0,0), Vec3(0,0,0), end, owner, MASK_SHOT);

    if (tr.entNum != -1 && (tr.surfFlags & SURF_NOIMPACT) == 0) {
        if (fireDef.fireMode == FIRE_MODE_STRONG) {
            CGame::Sound::StartFixedSound(@cgs.media.sfxRiotgunStrongHit, tr.endPos, CHAN_AUTO,
                cg_volume_effects.value, ATTN_IDLE
            );
        } else {
            CGame::Sound::StartFixedSound(@cgs.media.sfxRiotgunWeakHit, tr.endPos, CHAN_AUTO,
                cg_volume_effects.value, ATTN_IDLE
            );
        }
    }
}

void Event_WeaponBeam(const Vec3 &in origin, const Vec3 &in dir, int ownerNum, int weapon, int fireMode) {
    const GS::Weapons::WeaponDef@ weaponDef;
    int range;
    Vec3 end;
    Trace tr;
	int POVent = CGame::Camera::GetMainCamera().POVent;

    switch (weapon) {
        case WEAP_ELECTROBOLT:
            @weaponDef = GS::Weapons::getWeaponDef(WEAP_ELECTROBOLT);
            range = GS::Weapons::ELECTROBOLT_RANGE;
            break;
        case WEAP_INSTAGUN:
            @weaponDef = GS::Weapons::getWeaponDef(WEAP_INSTAGUN);
            range = weaponDef.fireDef.timeout;
            break;
        default:
            return;
    }

	Vec3 normDir = dir;
    normDir.normalize();
    end = origin + normDir * double(range);

    // retrace to spawn wall impact
    tr.doTrace(origin, Vec3(0,0,0), Vec3(0,0,0), end, POVent, MASK_SOLID);
    if (tr.entNum != -1) {
        if (weaponDef.weaponId == WEAP_ELECTROBOLT) {
            LE::BoltExplosionMode(tr.endPos, tr.planeNormal, FIRE_MODE_STRONG, tr.surfFlags);
        } else if (weaponDef.weaponId == WEAP_INSTAGUN) {
            LE::InstaExplosionMode(tr.endPos, tr.planeNormal, FIRE_MODE_STRONG, tr.surfFlags, ownerNum);
        }
    }

    // when it's predicted we have to delay the drawing until the view weapon is calculated
    cgEnts[ownerNum].localEffects[LEF_EV_WEAPONBEAM] = weapon;
    cgEnts[ownerNum].laserOrigin = origin;
    cgEnts[ownerNum].laserPoint = tr.endPos;
}

void Event_LaserBeam(int entNum, int weapon, int fireMode) {
    CEntity@ cent = @cgEnts[entNum];
    uint timeout;
    Vec3 f, r, u;

    if (!cg_predictLaserBeam.boolean) {
        return;
    }

    // lasergun's smooth refire
    if (fireMode == FIRE_MODE_STRONG) {
        cent.laserCurved = false;
        timeout = GS::Weapons::getWeaponDef(WEAP_LASERGUN).fireDef.reloadTime + 10;

        // find destiny point
        cent.laserOrigin = CGame::PredictedPlayerState.pmove.origin;
        cent.laserOrigin.z += CGame::PredictedPlayerState.viewHeight;
		CGame::PredictedPlayerState.viewAngles.angleVectors(f, r, u);
        cent.laserPoint = cent.laserOrigin + f * double(GS::Weapons::getWeaponDef(WEAP_LASERGUN).fireDef.timeout);
    } else {
        cent.laserCurved = true;
        timeout = GS::Weapons::getWeaponDef(WEAP_LASERGUN).fireDefWeak.reloadTime + 10;

        // find destiny point
        cent.laserOrigin = CGame::PredictedPlayerState.pmove.origin;
        cent.laserOrigin.z += CGame::PredictedPlayerState.viewHeight;

        if (!GS::Weapons::GetLaserbeamPoint(@cg.weaklaserTrail, cg.predictingTimeStamp, cent.laserPoint)) {
			f = CGame::PredictedPlayerState.viewAngles.anglesToForward();
            cent.laserPoint = cent.laserOrigin + f * double(GS::Weapons::getWeaponDef(WEAP_LASERGUN).fireDef.timeout);
        }
    }

    // it appears that 64ms is the maximum allowed time interval between prediction events on localhost
    if (timeout < 65) {
        timeout = 65;
    }

    cent.laserOriginOld = cent.laserOrigin;
    cent.laserPointOld = cent.laserPoint;
    cent.localEffects[LEF_LASERBEAM] = cg.time + timeout;
}

}
