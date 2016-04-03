#include "ai_local.h"

AiGametypeBrain AiGametypeBrain::instance;

void AiGametypeBrain::ClearGoals(NavEntity *canceledGoal, Ai *goalGrabber)
{
    if (!canceledGoal)
        return;

    // find all bots which have this node as goal and tell them their goal is reached
    for (edict_t *ent = game.edicts + 1; PLAYERNUM(ent) < gs.maxclients; ent++)
    {
        if (!ent->ai || ent->ai->type == AI_INACTIVE)
            continue;
        if (ent->ai->aiRef == goalGrabber)
            continue;

        if (ent->ai->aiRef->aiBaseBrain->longTermGoal == canceledGoal)
            ent->ai->aiRef->aiBaseBrain->ClearLongTermGoal();
        else if (ent->ai->aiRef->aiBaseBrain->shortTermGoal == canceledGoal)
            ent->ai->aiRef->aiBaseBrain->ClearShortTermGoal();
    }
}
