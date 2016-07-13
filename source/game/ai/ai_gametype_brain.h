#ifndef QFUSION_AI_GAMETYPE_BRAIN_H
#define QFUSION_AI_GAMETYPE_BRAIN_H

#include "ai_frame_aware_updatable.h"
#include "ai_goal_entities.h"

class AiGametypeBrain: public AiFrameAwareUpdatable
{
protected:
    AiGametypeBrain()
    {
        std::fill_n(teams, MAX_CLIENTS, TEAM_SPECTATOR);
    };

    int teams[MAX_CLIENTS];

    static AiGametypeBrain *instance;
    virtual void Frame() override;
public:
    virtual ~AiGametypeBrain() override {}

    // This call does actual (re-)initialization of this class instance and team AI's
    static void OnGametypeChanged(const char *gametype);
    // May return some of subtypes of this class depending on a gametype in future
    static inline AiGametypeBrain *Instance() { return instance; }
    void ClearGoals(const NavEntity *canceledGoal, const class Ai *goalGrabber);
    void OnBotJoinedTeam(edict_t *ent, int team);
    void OnBotDropped(edict_t *ent);
};

#endif
