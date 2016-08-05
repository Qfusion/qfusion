#ifndef QFUSION_AI_BASE_BRAIN_H
#define QFUSION_AI_BASE_BRAIN_H

#include "ai_local.h"
#include "ai_goal_entities.h"
#include "ai_frame_aware_updatable.h"
#include "static_vector.h"
#include "ai_aas_route_cache.h"
#include "ai_base_ai.h"

class AiBaseBrain: public AiFrameAwareUpdatable
{
    friend class Ai;
    friend class AiGametypeBrain;
    friend class AiBaseTeamBrain;
protected:
    edict_t *self;

    Goal localLongTermGoal;
    Goal *longTermGoal;
    Goal localShortTermGoal;
    Goal *shortTermGoal;
    // A domain-specific goal that overrides regular goals.
    // By default is NULL. May be set by subclasses logic/team AI logic.
    Goal localSpecialGoal;
    Goal *specialGoal;

    unsigned longTermGoalSearchTimeout;
    unsigned shortTermGoalSearchTimeout;

    const unsigned longTermGoalSearchPeriod;
    const unsigned shortTermGoalSearchPeriod;

    unsigned longTermGoalReevaluationTimeout;
    unsigned shortTermGoalReevaluationTimeout;

    const unsigned longTermGoalReevaluationPeriod;
    const unsigned shortTermGoalReevaluationPeriod;

    int currAasAreaNum;
    int droppedToFloorAasAreaNum;
    Vec3 droppedToFloorOrigin;

    int preferredAasTravelFlags;
    int allowedAasTravelFlags;

    const AiAasWorld *AasWorld() const { return self->ai->aiRef->aasWorld; }
    AiAasRouteCache *RouteCache() { return self->ai->aiRef->routeCache; }
    const AiAasRouteCache *RouteCache() const { return self->ai->aiRef->routeCache; }

    // Weights computed by a bot
    float internalEntityWeights[MAX_EDICTS];
    // Weights set by external code.
    // These weights completely override internal weights
    // (If an external weight != 0, the external weight is used).
    // Weights may be negative (in this case an entity will be excluded from potential goals).
    float externalEntityWeights[MAX_EDICTS];

    AiBaseBrain(edict_t *self, int preferredAasTravelFlags, int allowedAasTravelFlags);

    int FindReachabilityToGoalArea(int goalAreaNum) const;
    int FindTravelTimeToGoalArea(int goalAreaNum) const;

    inline void ClearInternalEntityWeights()
    {
        memset(internalEntityWeights, 0, sizeof(internalEntityWeights));
    }
    void UpdateInternalWeights();
    virtual void UpdatePotentialGoalsWeights();
    float GetEntityWeight(int entNum) const;

    void CheckOrCancelGoal();
    bool ShouldCancelGoal(const Goal *goal);
    // To be overridden in subclass. Should check other reasons of goal rejection aside generic ones for all goals.
    virtual bool ShouldCancelSpecialGoalBySpecificReasons() { return false; }

    void PickLongTermGoal(const Goal *currLongTermGoal);
    void PickShortTermGoal(const Goal *currLongTermGoal);
    void ClearLongAndShortTermGoal(const Goal *pickedGoal);
    void SetShortTermGoal(NavEntity *navEntity);
    void SetLongTermGoal(NavEntity *navEntity);
    // Overriding method should call this one
    virtual void SetSpecialGoal(Goal *goal);
    virtual void OnGoalCleanedUp(const Goal *goal) {}

    // Returns a pair of AAS travel times to the target point and back
    std::pair<unsigned, unsigned> FindToAndBackTravelTimes(const Vec3 &targetPoint) const;

    bool IsCloseToGoal(const Goal *goal, float proximityThreshold) const;

    int GoalAasAreaNum() const;
    Vec3 CurrentGoalOrigin() const;

    virtual void PreThink() override;
    virtual void Think() override;

    // Used for additional potential goal rejection that does not reflected in entity weights.
    // Returns true if the goal entity is not feasible for some reasons.
    // Return result "false" does not means that goal is feasible though.
    // Should be overridden in subclasses to implement domain-specific behaviour.
    virtual bool MayNotBeFeasibleGoal(const Goal *goal) { return false; };
    virtual bool MayNotBeFeasibleGoal(const NavEntity *navEntity) { return false; }

    void OnLongTermGoalReached();
    void OnShortTermGoalReached();
    // To be overridden in subclasses
    virtual void OnSpecialGoalReached();
private:
    struct NavEntityAndWeight
    {
        NavEntity *goal;
        float weight;
        inline NavEntityAndWeight(NavEntity *goal, float weight): goal(goal), weight(weight) {}
        // For sorting in descending by weight order operator < is negated
        inline bool operator<(const NavEntityAndWeight &that) const { return weight > that.weight; }
    };
    typedef StaticVector<NavEntityAndWeight, MAX_NAVENTS> GoalCandidates;

    // Fills a result container and returns it sorted by weight in descending order.
    // Returns weight of current long-term goal (or zero).
    float SelectLongTermGoalCandidates(const Goal *currLongTermGoal, GoalCandidates &result);
    // (Same as SelectLongTermGoalCandidates applied to a short-term goal)
    float SelectShortTermGoalCandidates(const Goal *currShortTermGoal, GoalCandidates &result);
    // Filters candidates selected by SelectLongTermGoalCandidates() by short-term reachability
    // Returns true if current short-term goal (if any) is reachable
    bool SelectShortTermReachableGoals(const Goal *currShortTermGoal, const GoalCandidates &candidates,
                                       GoalCandidates &result);
public:
    virtual ~AiBaseBrain() override {}

    inline bool HasGoal() const
    {
        return longTermGoal || shortTermGoal || specialGoal;
    }

    void ClearExternalEntityWeights()
    {
        memset(externalEntityWeights, 0, sizeof(externalEntityWeights));
    }
    // Sets an external entity weight.
    // This weight overrides internal one computed by this brain itself.
    void SetExternalEntityWeight(const edict_t *ent, float weight)
    {
        externalEntityWeights[ENTNUM(const_cast<edict_t*>(ent))] = weight;
    }

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
