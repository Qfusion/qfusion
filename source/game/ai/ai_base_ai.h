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

#include "ai_aas_world.h"
#include "ai_aas_route_cache.h"
#include "static_vector.h"

class Ai: public EdictRef, public AiFrameAwareUpdatable
{
    friend class AiManager;
    friend class AiBaseTeamBrain;
    friend class AiBaseBrain;
    friend class AiBaseAction;
    friend class AiBaseActionRecord;
    friend class AiBaseGoal;
protected:
    // Must be set in a subclass constructor. A subclass manages memory for its brain
    // (it either has it as an intrusive member of allocates it on heap)
    // and provides a reference to it to this base class via this pointer.
    class AiBaseBrain *aiBaseBrain;
    // Must be set in a subclass constructor.
    // A subclass should decide whether a shared or separated route cache should be used.
    // A subclass should destroy the cache instance if necessary.
    AiAasRouteCache *routeCache;
    // A cached reference to an AAS world, set by this class
    AiAasWorld *aasWorld;

    // Must be updated before brain thinks.
    int currAasAreaNum;
    // Must be updated before brain thinks
    // May match currAasAreaNum if the origin can't be dropped to floor
    // (An Ai is more than 96 units above a floor (if any))
    int droppedToFloorAasAreaNum;
    Vec3 droppedToFloorOrigin;

    int allowedAasTravelFlags;
    int preferredAasTravelFlags;

    static constexpr unsigned MAX_REACH_CACHED = 24;
    StaticVector<aas_reachability_t, MAX_REACH_CACHED> nextReaches;

    float distanceToNextReachStart;

    inline bool IsCloseToReachStart() const { return distanceToNextReachStart < 24.0f; };

    unsigned blockedTimeout;

    vec3_t angularViewSpeed;

    void SetFrameAffinity(unsigned modulo, unsigned offset) override;

    void OnNavTargetSet(NavTarget *navTarget);
    void OnNavTargetReset();

    void UpdateReachCache(int reachedAreaNum);

    virtual void Frame() override;
    virtual void Think() override;
public:
    Ai(edict_t *self_,
       AiBaseBrain *aiBaseBrain_,
       AiAasRouteCache *routeCache_,
       int preferredAasTravelFlags_,
       int allowedAasTravelFlags_,
       float yawSpeed = 180.0f,
       float pitchSpeed = 140.0f);

    virtual ~Ai() override {};

    inline bool IsGhosting() const { return G_ISGHOSTING(self); }

    inline int CurrAreaNum() const { return currAasAreaNum; }
    inline int DroppedToFloorAreaNum() const { return droppedToFloorAasAreaNum; }
    int NavTargetAasAreaNum() const;
    Vec3 NavTargetOrigin() const;

    inline int PreferredTravelFlags() const { return preferredAasTravelFlags; }
    inline int AllowedTravelFlags() const { return allowedAasTravelFlags; }

    static bool IsStep(edict_t *ent);
    // Accepts a touched entity and its old solid before touch
    void TouchedEntity(edict_t *ent);

    // TODO: Remove this, check item spawn time instead
    virtual void OnNavEntityReachedBy(const NavEntity *navEntity, const Ai *grabber) {}
    virtual void OnEntityReachedSignal(const edict_t *entity) {}

    void ResetNavigation();
    void CategorizePosition();

    virtual void OnBlockedTimeout() {};

    static constexpr unsigned BLOCKED_TIMEOUT = 15000;
protected:
    const char *Nick() const
    {
        return self->r.client ? self->r.client->netname : self->classname;
    }

    virtual void TouchedNavEntity(const edict_t *underlyingEntity) {};
    virtual void TouchedOtherEntity(const edict_t *entity) {}

    void CheckReachedArea();

    void TestClosePlace();
    ClosePlaceProps closeAreaProps;

    // This function produces very basic but reliable results.
    // Imitation of human-like aiming should be a burden of callers that prepare the desiredDirection.
    inline Vec3 GetNewViewAngles(const Vec3 &oldAngles, const Vec3 &desiredDirection,
                                 float angularSpeedMultiplier = 1.0f, bool extraPrecision = false)
    {
        return GetNewViewAngles(oldAngles.Data(), desiredDirection, angularSpeedMultiplier);
    }
    Vec3 GetNewViewAngles(const vec3_t oldAngles, const Vec3 &desiredDirection, float angularSpeedMultiplier = 1.0f);
private:
    float GetChangedAngle(float oldAngle, float desiredAngle, float angulasSpeedMultiplier, int angleIndex);

    void TestMove(MoveTestResult *moveTestResult, int currAasAreaNum_, const vec3_t forward) const;
};

#endif
