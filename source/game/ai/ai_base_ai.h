#ifndef QFUSION_AI_BASE_AI_H
#define QFUSION_AI_BASE_AI_H

#include "edict_ref.h"
#include "ai_frame_aware_updatable.h"
#include "ai_goal_entities.h"

struct MoveTestResult
{
    friend class Ai;
private:
    bool canWalk;
    bool canFall;
    bool canJump;
    float fallDepth;
    void Clear()
    {
        canWalk = canFall = canJump = false;
        fallDepth = 0;
    }
public:
    inline bool CanWalk() const { return canWalk; }
    inline bool CanFall() const { return canFall; }
    // TODO: Check exact falldamage condition
    inline bool CanWalkOrFallQuiteSafely() const { return canWalk || (canFall && fallDepth < 200); }
    inline bool CanJump() const { return canJump; }
    inline float PotentialFallDepth() const { return fallDepth; }
};

struct ClosePlaceProps
{
    MoveTestResult leftTest;
    MoveTestResult rightTest;
    MoveTestResult frontTest;
    MoveTestResult backTest;
};

#include "aas/aasfile.h"
#include "static_vector.h"

class Ai: public EdictRef, public AiFrameAwareUpdatable
{
    friend class AiGametypeBrain;
    friend class AiBaseTeamBrain;
    friend class AiBaseBrain;
protected:
    // Must be set in a subclass constructor. A subclass manages memory for its brain
    // (it either has it as an intrusive member of allocates it on heap)
    // and provides a reference to it to this base class via this pointer.
    class AiBaseBrain *aiBaseBrain;

    // Must be updated before brain thinks (set the brain currAasAreaNum to an updated value).
    int currAasAreaNum;
    // Must be updated after brain thinks (copy from a brain goal)
    int goalAasAreaNum;
    Vec3 goalTargetPoint;

    int allowedAasTravelFlags;
    int preferredAasTravelFlags;

    int currAasAreaTravelFlags;
    static constexpr unsigned MAX_REACH_CACHED = 24;
    StaticVector<aas_reachability_t, MAX_REACH_CACHED> nextReaches;

    float distanceToNextReachStart;

    inline bool IsCloseToReachStart() { return distanceToNextReachStart < 24.0f; };

    unsigned blockedTimeout;

    float aiYawSpeed, aiPitchSpeed;

    void SetFrameAffinity(unsigned modulo, unsigned offset) override;

    void ClearAllGoals();

    // Called by brain via self->ai->aiRef when long-term or short-term goals are set
    void OnGoalSet(Goal *goal);

    void UpdateReachCache(int reachedAreaNum);

    virtual void Frame() override;
    virtual void Think() override;
public:
    Ai(edict_t *self, int preferredAasTravelFlags, int allowedAasTravelFlags);
    virtual ~Ai() override {};

    inline bool IsGhosting() const { return G_ISGHOSTING(self); }

    inline int CurrAreaNum() const { return currAasAreaNum; }
    inline int GoalAreaNum() const { return goalAasAreaNum; }

    inline int PreferredTravelFlags() const { return preferredAasTravelFlags; }
    inline int AllowedTravelFlags() const { return allowedAasTravelFlags; }

    void ChangeAngle(const Vec3 &idealDirection, float angularSpeedMultiplier = 1.0f);
    static bool IsStep(edict_t *ent);
    // Accepts a touched entity and its old solid before touch
    void TouchedEntity(edict_t *ent);

    void ResetNavigation();
    void CategorizePosition();

    virtual void OnBlockedTimeout() {};

    static constexpr unsigned BLOCKED_TIMEOUT = 15000;
protected:
    void Debug(const char *format, ...) const;
    void FailWith(const char *format, ...) const;

    const char *Nick() const
    {
        return self->r.client ? self->r.client->netname : self->classname;
    }

    inline int FindAASReachabilityToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const
    {
        return ::FindAASReachabilityToGoalArea(fromAreaNum, origin, goalAreaNum, self,
                                               preferredAasTravelFlags, allowedAasTravelFlags);
    }
    inline int FindAASTravelTimeToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const
    {
        return ::FindAASTravelTimeToGoalArea(fromAreaNum, origin, goalAreaNum, self,
                                             preferredAasTravelFlags, allowedAasTravelFlags);
    }
    inline float FindSquareDistanceToGround(const vec3_t origin, float traceDepth = 999999.0f) const
    {
        return ::FindSquareDistanceToGround(origin, self, traceDepth);
    }
    inline float FindDistanceToGround(const vec3_t origin, float traceDepth = 999999.0f) const
    {
        return ::FindDistanceToGround(origin, self, traceDepth);
    }

    virtual void TouchedGoal(const edict_t *goalUnderlyingEntity) {};
    virtual void TouchedJumppad(const edict_t *jumppad) {};

    void CheckReachedArea();
    void ChangeAxisAngle(float currAngle, float idealAngle, float edictAngleSpeed, float *aiAngleSpeed, float *changedAngle);

    void TestClosePlace();
    ClosePlaceProps closeAreaProps;
private:
    void TestMove(MoveTestResult *moveTestResult, int currAasAreaNum, const vec3_t forward) const;
};

#endif
