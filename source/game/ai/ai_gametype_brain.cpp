#include "ai_gametype_brain.h"
#include "ai_base_brain.h"
#include "ai_base_team_brain.h"
#include "ai_base_ai.h"
#include "ai_shutdown_hooks_holder.h"

// Class static variable declaration
AiGametypeBrain *AiGametypeBrain::instance = nullptr;
// Actual instance location in memory
static StaticVector<AiGametypeBrain, 1> instanceHolder;

void AiGametypeBrain::OnGametypeChanged(const char *gametype)
{
    // Currently, gametype brain is shared for all gametypes
    // If gametype brain differs for different gametypes,
    // delete previous instance and create a suitable new instance.

    // This means that gametype has been set up for first time.
    if (instanceHolder.empty())
    {
        instanceHolder.emplace_back(AiGametypeBrain());
        AiShutdownHooksHolder::Instance()->RegisterHook([&] { instanceHolder.clear(); });
        instance = &instanceHolder.front();
    }

    AiBaseTeamBrain::OnGametypeChanged(gametype);
}

void AiGametypeBrain::ClearGoals(const NavEntity *canceledGoal, const Ai *goalGrabber)
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
            ent->ai->aiRef->aiBaseBrain->ClearLongAndShortTermGoal(canceledGoal);
        else if (ent->ai->aiRef->aiBaseBrain->shortTermGoal == canceledGoal)
            ent->ai->aiRef->aiBaseBrain->ClearLongAndShortTermGoal(canceledGoal);
    }
}

void AiGametypeBrain::Frame()
{
    if (!GS_TeamBasedGametype())
    {
        AiBaseTeamBrain::GetBrainForTeam(TEAM_PLAYERS)->Update();
        return;
    }

    for (int team = TEAM_ALPHA; team < GS_MAX_TEAMS; ++team)
    {
        AiBaseTeamBrain::GetBrainForTeam(team)->Update();
    }
}
