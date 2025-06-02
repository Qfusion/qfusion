namespace GS {
    
namespace Weapons {

const float ELECTROBOLT_RANGE = 9001.0f;

const float BULLET_WATER_REFRACTION = 1.5f;

const int CURVELASERBEAM_SUBDIVISIONS = 40;
const int CURVELASERBEAM_BACKTIME = 60;

const int LASERGUN_WEAK_TRAIL_BACKUP = 32; // 0.5 second backup at 62 fps, which is the ucmd fps ratio
const int LASERGUN_WEAK_TRAIL_MASK = LASERGUN_WEAK_TRAIL_BACKUP - 1;

class LaserbeamTrail {
    array<Vec3> origins(LASERGUN_WEAK_TRAIL_BACKUP);
    array<int64> timeStamps(LASERGUN_WEAK_TRAIL_BACKUP);
    array<bool> teleported(LASERGUN_WEAK_TRAIL_BACKUP);
    int head = 0;
}

/*
* TraceBullet4D
*/
bool TraceBullet4D(Trace &out tr, Trace &out wtr, const Vec3 &in start, 
    const Vec3 &in fv, const Vec3 &in rv, const Vec3 &in uv,
    double r, double u, int range, int ignore, int timeDelta) {
    Vec3 end;
    bool water = false;
    int contentMask = MASK_SHOT | MASK_WATER;

    if ((GS::PointContents4D(start, timeDelta) & MASK_WATER) != 0) {
        water = true;
        contentMask &= ~MASK_WATER;

        // ok, this isn't randomized, but I think we can live with it
        // the effect on water has never been properly noticed anyway
        // r *= BULLET_WATER_REFRACTION;
        // u *= BULLET_WATER_REFRACTION;
    }

    end = start + fv * range + rv * r + uv * u;

    tr.doTrace4D(start, Vec3(0,0,0), Vec3(0,0,0), end, ignore, contentMask, timeDelta);

    // see if we hit water
    if ((tr.contents & MASK_WATER) != 0) {
        // re-trace ignoring water this time
        wtr.doTrace4D(tr.endPos, Vec3(0,0,0), Vec3(0,0,0), end, ignore, MASK_SHOT, timeDelta);
        return true;
    }

    if (water) {
        wtr.endPos = start;
        return true;
    }

    return false;
}

const int MAX_BEAM_HIT_ENTITIES = 16;

// Delegate interface for impact callback
interface ITraceImpact {
    void impact(Trace trace, Vec3 dir);
}

// Returns the last Trace result (by value)
Trace TraceLaserBeam(Vec3 origin, Vec3 dir, double range, int ignore, int timeDelta, ITraceImpact@ impactCb) {
    Vec3 from = origin;
    Vec3 end = origin + dir * range;
    int mask = MASK_SHOT;
    int passthrough = ignore;
    array<int> hits(MAX_BEAM_HIT_ENTITIES);
    int numhits = 0;

    Trace localTrace;

    while (true) {
        bool hitSomething = localTrace.doTrace(from, Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, 0.5f), end, passthrough, mask);
        int entNum = localTrace.entNum;

        if (!hitSomething || entNum == -1) {
            break;
        }

        // Prevent endless loops by checking whether we have already impacted this entity
        int j = 0;
        for (; j < numhits; j++) {
            if (entNum == hits[j]) {
                break;
            }
        }
        if (j < numhits) {
            break;
        }

        // callback impact
        if (impactCb !is null) {
            impactCb.impact(localTrace, dir);
        }

        EntityState@ hit = GS::GetEntityState(entNum, timeDelta);
        if (entNum == 0 || hit is null || hit.solid == SOLID_BMODEL) {
            break;
        }

        if (localTrace.fraction == 0.0f || localTrace.allSolid || localTrace.startSolid) {
            break;
        }

        if (numhits < MAX_BEAM_HIT_ENTITIES) {
            hits[numhits++] = entNum;
        } else {
            break;
        }

        passthrough = entNum;
        from = localTrace.endPos;
    }

    return localTrace;
}

Trace TraceCurveLaserBeam(Vec3 origin, Vec3 angles, Vec3 blendPoint, int ignore, int timeDelta, ITraceImpact@ impactCb) {
    double subdivisions = CURVELASERBEAM_SUBDIVISIONS;
    double range = double(getWeaponDef(WEAP_LASERGUN).fireDefWeak.timeout);
    Vec3 from = origin;
    Vec3 dir, end;
    Vec3 tmpAngles, blendAngles;

    blendAngles = (blendPoint - origin).toAngles();

    int passthrough = ignore;
    Trace lastTrace;

    for (int i = 1; i <= int(subdivisions); i++) {
        double frac = ((range / subdivisions) * double(i)) / range;

        for (int j = 0; j < 3; j++) {
            tmpAngles[j] = LerpAngle(angles[j], blendAngles[j], float(frac));
        }

        dir = tmpAngles.anglesToForward();
        end = origin + dir * (range * frac);

        Trace localTrace = TraceLaserBeam(from, dir, (from - end).length(), passthrough, timeDelta, impactCb);
        lastTrace = localTrace;
        if (localTrace.fraction != 1.0f) {
            break;
        }

        passthrough = localTrace.entNum;
        from = end;
    }

    return lastTrace;
}

void AddLaserbeamPoint(LaserbeamTrail@ trail, PlayerState@ playerState, int64 timeStamp) {
    if (timeStamp == 0 || playerState is null) {
        return;
    }

    Vec3 origin = playerState.pmove.origin;
    origin.z += playerState.viewHeight;
    Vec3 dir = playerState.viewAngles.anglesToForward();

    int idx = trail.head & LASERGUN_WEAK_TRAIL_MASK;
    trail.origins[idx] = origin + dir * double(getWeaponDef(WEAP_LASERGUN).fireDefWeak.timeout);
    trail.timeStamps[idx] = timeStamp;
    trail.teleported[idx] = (playerState.pmove.pm_flags & PMF_TIME_TELEPORT) != 0;
    trail.head++;
}

bool GetLaserbeamPoint(LaserbeamTrail@ trail, int64 curtime, Vec3 &out res) {
    if (curtime <= CURVELASERBEAM_BACKTIME) {
        return false;
    }

    int64 timeStamp = curtime - CURVELASERBEAM_BACKTIME;
    int current = trail.head - 1;

    if (trail.timeStamps[current & LASERGUN_WEAK_TRAIL_MASK] == 0) {
        return false;
    }

    if (timeStamp >= trail.timeStamps[current & LASERGUN_WEAK_TRAIL_MASK]) {
        timeStamp = trail.timeStamps[current & LASERGUN_WEAK_TRAIL_MASK];
    }

    int older = current;
    while (
        (older > 0) &&
        (trail.timeStamps[older & LASERGUN_WEAK_TRAIL_MASK] > timeStamp) &&
        (trail.timeStamps[(older - 1) & LASERGUN_WEAK_TRAIL_MASK] != 0) &&
        (!trail.teleported[older & LASERGUN_WEAK_TRAIL_MASK])
    ) {
        older--;
    }

    // todo: add interpolation?
    res = trail.origins[older & LASERGUN_WEAK_TRAIL_MASK];
    return true;
}

bool CheckBladeAutoAttack(PlayerState@ playerState, int timeDelta, bool allowTeamDamage) {
    if (playerState is null) {
        return false;
    }

    int povnum = playerState.POVnum;
    if (povnum <= 0 || povnum > GS::maxClients) {
        return false;
    }

    // Check for autoattack feature
    if ((playerState.pmove.stats[PM_STAT_FEATURES] & PMFEAT_GUNBLADEAUTOATTACK) == 0) {
        return false;
    }

    Vec3 origin = playerState.pmove.origin;
    origin.z += playerState.viewHeight;
    Vec3 dir = playerState.viewAngles.anglesToForward();
    double range = double(getWeaponDef(WEAP_GUNBLADE).fireDefWeak.timeout);
    Vec3 end = origin + dir * range;

    Trace trace;
    // Trace for a player to touch
    trace.doTrace(origin, Vec3(0,0,0), Vec3(0,0,0), end, povnum, CONTENTS_BODY);

    if (trace.entNum <= 0 || trace.entNum > GS::maxClients) {
        return false;
    }

    EntityState@ player = GS::GetEntityState(povnum, 0);
    EntityState@ targ = GS::GetEntityState(trace.entNum, 0);
    if (targ is null || player is null) {
        return false;
    }

    if ((targ.effects & EF_TAKEDAMAGE) == 0 || targ.type != ET_PLAYER) {
        return false;
    }

    if (!allowTeamDamage || targ.team == player.team){
        return false;
    }

    return true;
}

//============================================================
//
//		PLAYER WEAPON THINKING
//
//============================================================

const int NOAMMOCLICK_PENALTY = 100;
const int NOAMMOCLICK_AUTOSWITCH = 50;

/*
* SelectBestWeapon
*/
int SelectBestWeapon(PlayerState@ playerState) {
    int weap_chosen = WEAP_NONE;

    // Find with strong ammo
    for (int weap = WEAP_TOTAL - 1; weap > WEAP_GUNBLADE; weap--) {
        if (playerState.inventory[weap] == 0)
            continue;

        const WeaponDef@ weapondef = getWeaponDef(weap);

        if (weapondef.fireDef.usageCount == 0 ||
            playerState.inventory[weapondef.fireDef.ammoID] >= weapondef.fireDef.usageCount) {
            weap_chosen = weap;
            break;
        }
    }

    if (weap_chosen != WEAP_NONE)
        return weap_chosen;

    // Repeat find with weak ammo
    for (int weap = WEAP_TOTAL - 1; weap >= WEAP_NONE; weap--) {
        if (playerState.inventory[weap] == 0)
            continue;

        const WeaponDef@ weapondef = getWeaponDef(weap);

        if (weapondef.fireDefWeak.usageCount == 0 ||
            playerState.inventory[weapondef.fireDefWeak.ammoID] >= weapondef.fireDefWeak.usageCount) {
            weap_chosen = weap;
            break;
        }
    }

    return weap_chosen;
}

/*
* GS_FiredefForPlayerState
*/
FireDef@ FiredefForPlayerState(PlayerState@ playerState, int checkweapon) {
    WeaponDef@ weapondef = getWeaponDef(checkweapon);

    // Find out our current fire mode
    if (playerState.inventory[weapondef.fireDef.ammoID] >= weapondef.fireDef.usageCount) {
        return @weapondef.fireDef;
    }

    return @weapondef.fireDefWeak;
}

/*
* GS_CheckAmmoInWeapon
*/
bool CheckAmmoInWeapon(PlayerState@ playerState, int checkweapon) {
    const FireDef@ firedef = FiredefForPlayerState(playerState, checkweapon);

    if (checkweapon != WEAP_NONE && playerState.inventory[checkweapon] == 0) {
        return false;
    }

    if (firedef.usageCount == 0 || firedef.ammoID == AMMO_NONE) {
        return true;
    }

    return playerState.inventory[firedef.ammoID] >= firedef.usageCount;
}

/*
* ThinkPlayerWeapon
*/
int ThinkPlayerWeapon(PlayerState@ playerState, int buttons, int msecs, int timeDelta) {
    int64 serverTimestamp = 1; // weapons think on client side only if ucmdReady

    if (playerState is null)
        return WEAP_NONE;

    bool refire = false;

    int pendingWeapon = playerState.stats[STAT_PENDING_WEAPON];
    int currentWeapon = playerState.stats[STAT_WEAPON];

    if (pendingWeapon < 0 || pendingWeapon >= WEAP_TOTAL)
        return currentWeapon;

    if (GS::MatchPaused()) {
        return currentWeapon;
    }

    if (playerState.pmove.pm_type != PM_NORMAL) {
        playerState.weaponState = WEAPON_STATE_READY;
        playerState.stats[STAT_PENDING_WEAPON] = WEAP_NONE;
        playerState.stats[STAT_WEAPON] = WEAP_NONE;
        playerState.stats[STAT_WEAPON_TIME] = 0;
        return playerState.stats[STAT_WEAPON];
    }

    if (playerState.pmove.stats[PM_STAT_NOUSERCONTROL] > 0) {
        buttons = 0;
    }

    if (msecs == 0) {
        return playerState.stats[STAT_WEAPON];
    }

    if (playerState.stats[STAT_WEAPON_TIME] > 0) {
        playerState.stats[STAT_WEAPON_TIME] = playerState.stats[STAT_WEAPON_TIME] - msecs;
    } else {
        playerState.stats[STAT_WEAPON_TIME] = 0;
    }

    const FireDef@ firedef = FiredefForPlayerState(playerState, currentWeapon);

    // During cool-down time it can shoot again or go into reload time
    if (playerState.weaponState == WEAPON_STATE_REFIRE || playerState.weaponState == WEAPON_STATE_REFIRESTRONG) {
        if (playerState.stats[STAT_WEAPON_TIME] > 0) {
            return playerState.stats[STAT_WEAPON];
        }

        int last_firemode = (playerState.weaponState == WEAPON_STATE_REFIRESTRONG) ? FIRE_MODE_STRONG : FIRE_MODE_WEAK;
        if (last_firemode == firedef.fireMode) {
            refire = true;
        }

        playerState.weaponState = WEAPON_STATE_READY;
    }

    // Nothing can be done during reload time
    if (playerState.weaponState == WEAPON_STATE_RELOADING) {
        if (playerState.stats[STAT_WEAPON_TIME] > 0) {
            return playerState.stats[STAT_WEAPON];
        }
        playerState.weaponState = WEAPON_STATE_READY;
    }

    if (playerState.weaponState == WEAPON_STATE_NOAMMOCLICK) {
        if (playerState.stats[STAT_WEAPON_TIME] > 0) {
            return playerState.stats[STAT_WEAPON];
        }
        if (playerState.stats[STAT_WEAPON] != playerState.stats[STAT_PENDING_WEAPON]) {
            playerState.weaponState = WEAPON_STATE_READY;
        }
    }

    // There is a weapon to be changed
    if (playerState.stats[STAT_WEAPON] != playerState.stats[STAT_PENDING_WEAPON]) {
        if (playerState.weaponState == WEAPON_STATE_READY ||
            playerState.weaponState == WEAPON_STATE_DROPPING ||
            playerState.weaponState == WEAPON_STATE_ACTIVATING) {
            if (playerState.weaponState != WEAPON_STATE_DROPPING) {
                playerState.weaponState = WEAPON_STATE_DROPPING;
                playerState.stats[STAT_WEAPON_TIME] = playerState.stats[STAT_WEAPON_TIME] + firedef.weaponDownTime;

                if (firedef.weaponDownTime != 0) {
                    GS::PredictedEvent(playerState.POVnum, EV_WEAPONDROP, 0, serverTimestamp);
                }
            }
        }
    }

    // Do the change
    if (playerState.weaponState == WEAPON_STATE_DROPPING) {
        if (playerState.stats[STAT_WEAPON_TIME] > 0) {
            return playerState.stats[STAT_WEAPON];
        }

        playerState.stats[STAT_WEAPON] = playerState.stats[STAT_PENDING_WEAPON];

        // Update the firedef
        @firedef = @FiredefForPlayerState(playerState, playerState.stats[STAT_WEAPON]);
        playerState.weaponState = WEAPON_STATE_ACTIVATING;
        playerState.stats[STAT_WEAPON_TIME] = playerState.stats[STAT_WEAPON_TIME] + firedef.weaponUpTime;
        GS::PredictedEvent(playerState.POVnum, EV_WEAPONACTIVATE, playerState.stats[STAT_WEAPON] << 1, serverTimestamp);
    }

    if (playerState.weaponState == WEAPON_STATE_ACTIVATING) {
        if (playerState.stats[STAT_WEAPON_TIME] > 0) {
            return playerState.stats[STAT_WEAPON];
        }
        playerState.weaponState = WEAPON_STATE_READY;
    }

    if (playerState.weaponState == WEAPON_STATE_READY || playerState.weaponState == WEAPON_STATE_NOAMMOCLICK) {
        if (playerState.stats[STAT_WEAPON_TIME] > 0) {
            return playerState.stats[STAT_WEAPON];
        }

        if (!GS::ShootingDisabled()) {
            if ((buttons & BUTTON_ATTACK) != 0) {
                if (CheckAmmoInWeapon(@playerState, playerState.stats[STAT_WEAPON])) {
                    playerState.weaponState = WEAPON_STATE_FIRING;
                } else {
                    // Player has no ammo nor clips
                    if (playerState.weaponState == WEAPON_STATE_NOAMMOCLICK) {
                        playerState.weaponState = WEAPON_STATE_RELOADING;
                        playerState.stats[STAT_WEAPON_TIME] = playerState.stats[STAT_WEAPON_TIME] + NOAMMOCLICK_AUTOSWITCH;
                        if (playerState.stats[STAT_PENDING_WEAPON] == playerState.stats[STAT_WEAPON]) {
                            playerState.stats[STAT_PENDING_WEAPON] = SelectBestWeapon(playerState);
                        }
                    } else {
                        playerState.weaponState = WEAPON_STATE_NOAMMOCLICK;
                        playerState.stats[STAT_WEAPON_TIME] = playerState.stats[STAT_WEAPON_TIME] + NOAMMOCLICK_PENALTY;
                        GS::PredictedEvent(playerState.POVnum, EV_NOAMMOCLICK, 0, serverTimestamp);
                        return playerState.stats[STAT_WEAPON];
                    }
                }
            }
            // Gunblade auto attack is special
            else if (playerState.stats[STAT_WEAPON] == WEAP_GUNBLADE &&
                     playerState.pmove.stats[PM_STAT_NOUSERCONTROL] <= 0 &&
                     playerState.pmove.stats[PM_STAT_NOAUTOATTACK] <= 0 &&
                     CheckBladeAutoAttack(@playerState, timeDelta, !GS::TeamBasedGametype())) {
                @firedef = @getWeaponDef(WEAP_GUNBLADE).fireDefWeak;
                playerState.weaponState = WEAPON_STATE_FIRING;
            }
        }
    }

    if (playerState.weaponState == WEAPON_STATE_FIRING) {
        int parm = playerState.stats[STAT_WEAPON] << 1;
        if (firedef.fireMode == FIRE_MODE_STRONG) {
            parm |= 0x1;
        }

        playerState.stats[STAT_WEAPON_TIME] = playerState.stats[STAT_WEAPON_TIME] + firedef.reloadTime;
        if (firedef.fireMode == FIRE_MODE_STRONG) {
            playerState.weaponState = WEAPON_STATE_REFIRESTRONG;
        } else {
            playerState.weaponState = WEAPON_STATE_REFIRE;
        }

        if (refire && firedef.smoothRefire) {
            GS::PredictedEvent(playerState.POVnum, EV_SMOOTHREFIREWEAPON, parm, serverTimestamp);
        } else {
            GS::PredictedEvent(playerState.POVnum, EV_FIREWEAPON, parm, serverTimestamp);
        }

        // Waste ammo
        if (!GS::InfiniteAmmo() && playerState.stats[STAT_WEAPON] != WEAP_GUNBLADE) {
            if (firedef.ammoID != AMMO_NONE && firedef.usageCount != 0) {
                playerState.inventory[firedef.ammoID] = playerState.inventory[firedef.ammoID] - firedef.usageCount;
            }
        }
    }

    return playerState.stats[STAT_WEAPON];
}

}

}
