#ifndef QFUSION_AI_GAMETYPE_BRAIN_H
#define QFUSION_AI_GAMETYPE_BRAIN_H

#include "ai_frame_aware_updatable.h"
#include "ai_goal_entities.h"

class AiGametypeBrain: public AiFrameAwareUpdatable
{
protected:
    AiGametypeBrain() {};

    static AiGametypeBrain *instance;
    virtual void Frame() override;
public:
    virtual ~AiGametypeBrain() override {}

    // This call does actual (re-)initialization of this class instance and team AI's
    static void OnGametypeChanged(const char *gametype);
    // May return some of subtypes of this class depending on a gametype in future
    static inline AiGametypeBrain *Instance() { return instance; }
    void ClearGoals(const NavEntity *canceledGoal, const class Ai *goalGrabber);
};

#endif
