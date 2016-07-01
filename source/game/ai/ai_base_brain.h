#ifndef QFUSION_AI_BASE_BRAIN_H
#define QFUSION_AI_BASE_BRAIN_H

#include "ai_local.h"
#include "ai_goal_entities.h"
#include "ai_frame_aware_updatable.h"

class AiBaseBrain: public AiFrameAwareUpdatable
{
    friend class Ai;
    friend class AiGametypeBrain;
    friend class AiBaseTeamBrain;
protected:
    edict_t *self;

    NavEntity *longTermGoal;
    NavEntity *shortTermGoal;
    // A domain-specific goal that overrides regular goals.
    // By default is NULL. May be set by subclasses logic/team AI logic.
    NavEntity *specialGoal;

    unsigned longTermGoalSearchTimeout;
    unsigned shortTermGoalSearchTimeout;

    const unsigned longTermGoalSearchPeriod;
    const unsigned shortTermGoalSearchPeriod;

    unsigned longTermGoalReevaluationTimeout;
    unsigned shortTermGoalReevaluationTimeout;

    const unsigned longTermGoalReevaluationPeriod;
    const unsigned shortTermGoalReevaluationPeriod;

    unsigned weightsUpdateTimeout;

    int currAasAreaNum;

    int preferredAasTravelFlags;
    int allowedAasTravelFlags;

    float entityWeights[MAX_GOALENTS];

    AiBaseBrain(edict_t *self, int preferredAasTravelFlags, int allowedAasTravelFlags);

    void ClearWeights();
    void UpdateWeights();
    virtual void UpdatePotentialGoalsWeights();

    void CheckOrCancelGoal();
    bool ShouldCancelGoal(const NavEntity *goalEnt);
    // To be overridden in subclass. Should check other reasons of goal rejection aside generic ones for all goals.
    virtual bool ShouldCancelSpecialGoalBySpecificReasons() { return false; }

    void PickLongTermGoal(const NavEntity *currLongTermGoalEnt);
    void PickShortTermGoal(const NavEntity *currLongTermGoalEnt);
    void ClearLongAndShortTermGoal(const NavEntity *pickedGoal);
    void SetShortTermGoal(NavEntity *goalEnt);
    void SetLongTermGoal(NavEntity *goalEnt);
    // Overriding method should call this one
    virtual void SetSpecialGoal(NavEntity *goalEnt);
    virtual void OnGoalCleanedUp(const NavEntity *goalEnt) {}

    // Returns a pair of AAS travel times to the target point and back
    std::pair<unsigned, unsigned> FindToAndBackTravelTimes(const Vec3 &targetPoint) const;

    int FindAASReachabilityToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const;
    int FindAASTravelTimeToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const;
    bool IsCloseToGoal(const NavEntity *goalEnt, float proximityThreshold) const;

    int GoalAasAreaNum() const;

    void Debug(const char *format, ...) const;

    virtual void Think() override;

    // Used for additional potential goal rejection that does not reflected in entity weights.
    // Returns true if the goal entity is not feasible for some reasons.
    // Return result "false" does not means that goal is feasible though.
    // Should be overridden in subclasses to implement domain-specific behaviour.
    virtual bool MayNotBeFeasibleGoal(const NavEntity *goalEnt) { return false; };

    void OnLongTermGoalReached();
    void OnShortTermGoalReached();
    // To be overridden in subclasses
    virtual void OnSpecialGoalReached();
public:
    void ClearAllGoals();
    // May be overridden in subclasses
    virtual void OnClearSpecialGoalRequested();

    // Should return true if entity touch has been handled
    bool HandleGoalTouch(const edict_t *ent);
    virtual bool HandleSpecialGoalTouch(const edict_t *ent);
    bool IsCloseToAnyGoal() const;
    bool TryReachGoalByProximity();
    // To be overridden in subclasses
    virtual bool TryReachSpecialGoalByProximity();
    bool ShouldWaitForGoal() const;
    // To be overridden in subclasses
    virtual bool ShouldWaitForSpecialGoal() const;
    Vec3 ClosestGoalOrigin() const;
};

#endif
