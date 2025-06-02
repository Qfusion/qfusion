namespace CGame {

bool ucmdReady = false;

/*
 * PredictedEvent - shared code can fire events during prediction
 */
void PredictedEvent(int entNum, int ev, int parm) {
    if (ev >= PREDICTABLE_EVENTS_MAX) {
        return;
    }

    // ignore this action if it has already been predicted (the unclosed ucmd has timestamp zero)
    if (ucmdReady && cg.predictingTimeStamp > cg.predictedEventTimes[ev]) {
        // inhibit the fire event when there is a weapon change predicted
        if (ev == EV_FIREWEAPON) {
            if (cg.predictedWeaponSwitch != 0 &&
                cg.predictedWeaponSwitch != CGame::PredictedPlayerState.stats[STAT_PENDING_WEAPON]) {
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
    auto @cam = CGame::Camera::GetMainCamera();
    if (cam.playerPrediction) {
        cg.predictedWeaponSwitch = new_weapon;
    }
}

/*
 * BuildSolidList
 */
void BuildSolidList() {
    for (int i = 0; i < CGame::Snap.numEntities; i++) {
        auto @ent = @CGame::Snap.getEntityState(i);

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
                CGame::AddEntityToTriggerList(ent.number);
                break;

            default:
                CGame::AddEntityToSolidList(ent.number);
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
    int frame = ucmdExecuted & CMD_MASK;
	auto @ps = @CGame::PredictedPlayerState;

    ucmdReady = ( ucmd.serverTimeStamp != 0 );
    if( !ucmdReady ) {
        return;
    }

    cg.predictingTimeStamp = ucmd.serverTimeStamp;

    if( ucmdExecuted == ucmdHead - 1 ) {
        GS::Weapons::AddLaserbeamPoint( @cg.weaklaserTrail, @ps, ucmd.serverTimeStamp );

        // compensate for ground entity movement
        if (pm.groundEntity != -1) {
            auto @ent = @cgEnts[pm.groundEntity].current;
            if (ent.solid == SOLID_BMODEL && ent.linearMovement) {
                int64 serverTime = GS::MatchPaused() ? CGame::Snap.serverTime : cg.time + CGame::ExtrapolationTime;
                Vec3 move = GS::LinearMovementDelta(@ent, CGame::Snap.serverTime, serverTime);
                CGame::PredictedPlayerState.pmove.origin = CGame::PredictedPlayerState.pmove.origin + move;
            }
        }
    }

    CEntity @cent = @cgEnts[ps.POVnum];
    cent.current.weapon = GS::Weapons::ThinkPlayerWeapon( @ps, ucmd.buttons, ucmd.msec, 0 );
}

}
