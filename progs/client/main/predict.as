namespace CGame {

/*
 * PredictedEvent - shared code can fire events during prediction
 */
void PredictedEvent(int entNum, int ev, int parm, int64 serverTimestamp) {
    if (ev >= PREDICTABLE_EVENTS_MAX) {
        return;
    }

    bool ucmdReady = ( serverTimestamp != 0 );
    if( !ucmdReady ) {
        return;
    }

    // ignore this action if it has already been predicted (the unclosed ucmd has timestamp zero)
    if (cg.predictingTimeStamp > cg.predictedEventTimes[ev]) {
        // inhibit the fire event when there is a weapon change predicted
        if (ev == EV_FIREWEAPON) {
            if (cg.predictedWeaponSwitch != 0 &&
                cg.predictedWeaponSwitch != PredictedPlayerState.stats[STAT_PENDING_WEAPON]) {
                return;
            }
        }

        cg.predictedEventTimes[ev] = cg.predictingTimeStamp;
        EntityEvent(@cgEnts[entNum].current, ev, parm, true);
    }
}

/*
 * Predict_ChangeWeapon
 */
void Predict_ChangeWeapon(int new_weapon) {
    auto @cam = Camera::GetMainCamera();
    if (cam.playerPrediction) {
        cg.predictedWeaponSwitch = new_weapon;
    }
}

/*
 * BuildSolidList
 */
void BuildSolidList() {
    for (int i = 0; i < Snap.numEntities; i++) {
        auto @ent = @Snap.getEntityState(i);

        switch (ent.type) {
            // the following entities can never be solid
            case ET_BEAM:
            case ET_PORTALSURFACE:
            case ET_BLASTER:
            case ET_ELECTRO_WEAK:
            case ET_ROCKET:
            case ET_GRENADE:
            case ET_PLASMA:
            case ET_LASERBEAM:
            case ET_CURVELASERBEAM:
            case ET_MINIMAP_ICON:
            case ET_DECAL:
            case ET_ITEM_TIMER:
            case ET_PARTICLES:
                break;

            case ET_PUSH_TRIGGER:
                AddEntityToTriggerList(ent.number);
                break;

            default:
                AddEntityToSolidList(ent.number);
                break;
        }
    }
}

/*
 * RunUserCmd
 *
 * Called after player movement simulation
 */
void RunUserCmd( PMove @pm, UserCmd ucmd, int ucmdHead, int ucmdExecuted )
{
	auto @ps = @PredictedPlayerState;
    bool lastUcmd = ( ucmdExecuted == ucmdHead );

    // if we are too far out of date, just freeze
    if( ucmdHead - ucmdExecuted >= CMD_BACKUP ) {
        cg.predictingTimeStamp = cg.time;
        return;
    }

    // compensate for ground entity movement
    if (lastUcmd && pm.groundEntity != -1) {
        auto @ent = @cgEnts[pm.groundEntity].current;
        if (ent.solid == SOLID_BMODEL && ent.linearMovement) {
            int64 serverTime = GS::MatchPaused() ? Snap.serverTime : cg.time + ExtrapolationTime;
            Vec3 move = GS::LinearMovementDelta(@ent, Snap.serverTime, serverTime);
            PredictedPlayerState.pmove.origin = PredictedPlayerState.pmove.origin + move;
        }
    }

    bool ucmdReady = ( ucmd.serverTimeStamp != 0 );
    if( !ucmdReady ) {
        return;
    }

    cg.predictingTimeStamp = ucmd.serverTimeStamp;

    if( ucmdExecuted == ucmdHead - 1 ) {
        // next to last ucmd
        GS::Weapons::AddLaserbeamPoint( @cg.weaklaserTrail, @ps, ucmd.serverTimeStamp );
    }

    CEntity @cent = @cgEnts[ps.POVnum];
    cent.current.weapon = GS::Weapons::ThinkPlayerWeapon( @ps, ucmd.buttons, ucmd.msec, 0 );
}

}
