#ifndef QFUSION_AI_BASE_AI_H
#define QFUSION_AI_BASE_AI_H

#include "edict_ref.h"
#include "ai_frame_aware_updatable.h"
#include "ai_goal_entities.h"
#include "ai_aas_world.h"
#include "ai_aas_route_cache.h"
#include "static_vector.h"

class alignas(4) AiEntityPhysicsState
{
    // Fields of this class are packed to allow cheap copying of class instances in bot movement prediction code
    friend class Ai;
    static constexpr float GROUND_TRACE_DEPTH = 128.0f;
    // These fields are accessed way too often, so packing benefits does not outweigh unpacking performance loss.
    vec3_t origin;
    vec3_t velocity;
public:
    // CONTENTS flags, cannot be compressed
    int waterType;
private:
    short angles[3];
    static_assert(MAX_EDICTS == (1 << 10), "Fields bits count assumes 1024 as game entities count limit");
    // Add an extra bit for -1 entity num sign
    short groundEntNum: 11;
    unsigned short selfEntNum: 10;
    // This needs some precision (can be used to restore trace fraction if needed), so its packed into 2 bytes
    unsigned short heightOverGround;

    inline void SetHeightOverGround(float heightOverGround_)
    {
        if (heightOverGround_ <= GROUND_TRACE_DEPTH)
            this->heightOverGround = (decltype(this->heightOverGround))(heightOverGround_ * 256);
        else
            this->heightOverGround = (decltype(this->heightOverGround))((GROUND_TRACE_DEPTH + 1) * 256 + 1);
    }

public:
    signed char droppedToFloorOriginOffset;
private:
    unsigned currAasAreaNum: 20;
    unsigned droppedToFloorAasAreaNum: 20;

    void UpdateAreaNums();
public:
    unsigned short waterLevel: 2;

    inline void UpdateFromEntity(const edict_t *ent)
    {
        VectorCopy(ent->s.origin, this->origin);
        VectorCopy(ent->velocity, this->velocity);
        this->waterType = ent->watertype;
        this->waterLevel = (decltype(this->waterLevel))ent->waterlevel;
        SetAngles(ent->s.angles);
        this->groundEntNum = -1;
        if (ent->groundentity)
            this->groundEntNum = (decltype(this->groundEntNum))(ENTNUM(const_cast<edict_t *>(ent->groundentity)));
        this->selfEntNum = (decltype(this->selfEntNum))ENTNUM(const_cast<edict_t *>(ent));

        UpdateAreaNums();
    }

    inline void UpdateFromPMove(const pmove_t *pmove)
    {
        VectorCopy(pmove->playerState->pmove.origin, this->origin);
        VectorCopy(pmove->playerState->pmove.velocity, this->velocity);
        this->waterType = pmove->watertype;
        this->waterLevel = (decltype(this->waterLevel))pmove->waterlevel;
        SetAngles(pmove->playerState->viewangles);
        this->groundEntNum = (decltype(this->groundEntNum))pmove->groundentity;
        this->selfEntNum = (decltype(this->selfEntNum))(pmove->playerState->playerNum + 1);

        UpdateAreaNums();
    }

    inline float HeightOverGround() const
    {
        if (heightOverGround <= GROUND_TRACE_DEPTH * 256)
            return heightOverGround / 256.0f;
        return std::numeric_limits<float>::infinity();
    }

    // If true, reachability checks do not make sense, wait for landing.
    inline bool IsHighAboveGround() const
    {
        return heightOverGround > GROUND_TRACE_DEPTH * 256;
    }

    inline const edict_t *GroundEntity() const
    {
        return groundEntNum >= 0 ? game.edicts + groundEntNum : nullptr;
    }
    inline const edict_t *Self() const { return game.edicts + selfEntNum; }

    inline Vec3 Angles() const
    {
        return Vec3((float)SHORT2ANGLE(angles[0]), (float)SHORT2ANGLE(angles[1]), (float)SHORT2ANGLE(angles[2]));
    }
    inline void SetAngles(const Vec3 &angles) { SetAngles(angles.Data()); }
    inline void SetAngles(const vec3_t angles_)
    {
        for (int i = 0; i < 3; ++i)
            this->angles[i] = (short)ANGLE2SHORT(angles_[i]);
    }

    int CurrAasAreaNum() const { return (int)currAasAreaNum; }
    int DroppedToFloorAasAreaNum() const { return (int)droppedToFloorAasAreaNum; }

    inline Vec3 DroppedToFloorOrigin() const
    {
        return Vec3(origin[0], origin[1], origin[2] + droppedToFloorOriginOffset);
    }

    // Do not expose origin/velocity directly.
    // These accessors help to trace access to origin, and packing is yet an open question.
    // A bug have already been spotted using this access tracing.

    inline const float *Origin() const { return origin; }
    inline void SetOrigin(const vec3_t origin_) { VectorCopy(origin_, this->origin); }
    inline void SetOrigin(const Vec3 &origin) { SetOrigin(origin.Data()); }

    inline const float *Velocity() const { return velocity; }
    inline void SetVelocity(const vec3_t velocity_) { VectorCopy(velocity_, this->velocity); }
    inline void SetVelocity(const Vec3 &velocity) { SetVelocity(velocity.Data()); }
};

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
    // Must be set in a subclass constructor. Can be arbitrary changed later.
    // Can point to external (predicted) entity physics state during movement planning.
    AiEntityPhysicsState *entityPhysicsState;

    int allowedAasTravelFlags;
    int preferredAasTravelFlags;

    unsigned blockedTimeout;

    vec3_t angularViewSpeed;

    void SetFrameAffinity(unsigned modulo, unsigned offset) override;

    void OnNavTargetSet(NavTarget *navTarget);
    void OnNavTargetReset();

    virtual void Frame() override;
    virtual void Think() override;
public:
    static constexpr unsigned MAX_REACH_CACHED = 20;
    struct alignas(2) ReachAndTravelTime
    {
    private:
        // Split an integer value in two parts to allow 2-byte alignment
        uint16_t reachNumHiPart;
        uint16_t reachNumLoPart;
    public:
        // AAS travel time to a nav target in centiseconds (seconds ^-2).
        // Do not confuse with travel time required to pass the reach. itself.
        // The intrinsic reach. travel time can be retrieved from the reach. properties addressed by the reachNum.
        short aasTravelTimeToTarget;

        ReachAndTravelTime(int reachNum_, short aasTravelTimeToTarget_)
        {
            this->reachNumHiPart = (uint16_t)((unsigned)reachNum_ >> 16);
            this->reachNumLoPart = (uint16_t)((unsigned)reachNum_ & 0xFFFF);
            this->aasTravelTimeToTarget = aasTravelTimeToTarget_;
        }

        int ReachNum() const { return (int)((reachNumHiPart << 16) | reachNumLoPart); }
    };
    static_assert(sizeof(ReachAndTravelTime) == 6, "");

    typedef StaticVector<ReachAndTravelTime, MAX_REACH_CACHED> ReachChainVector;

    Ai(edict_t *self_,
       AiBaseBrain *aiBaseBrain_,
       AiAasRouteCache *routeCache_,
       AiEntityPhysicsState *entityPhysicsState_,
       int preferredAasTravelFlags_,
       int allowedAasTravelFlags_,
       float yawSpeed = 180.0f,
       float pitchSpeed = 140.0f);

    virtual ~Ai() override {};

    inline bool IsGhosting() const { return G_ISGHOSTING(self); }

    inline int CurrAreaNum() const { return entityPhysicsState->currAasAreaNum; }
    inline int DroppedToFloorAreaNum() const { return entityPhysicsState->droppedToFloorAasAreaNum; }
    int NavTargetAasAreaNum() const;
    Vec3 NavTargetOrigin() const;
    float NavTargetRadius() const;
    bool IsNavTargetBasedOnEntity(const edict_t *ent) const;

    // Exposed for native and script actions
    int CheckTravelTimeMillis(const Vec3 &from, const Vec3 &to, bool allowUnreachable = true);

    inline int PreferredTravelFlags() const { return preferredAasTravelFlags; }
    inline int AllowedTravelFlags() const { return allowedAasTravelFlags; }

    // Accepts a touched entity and its old solid before touch
    void TouchedEntity(edict_t *ent);

    // TODO: Remove this, check item spawn time instead
    virtual void OnNavEntityReachedBy(const NavEntity *navEntity, const Ai *grabber) {}
    virtual void OnEntityReachedSignal(const edict_t *entity) {}

    void ResetNavigation();

    virtual void OnBlockedTimeout() {};

    static constexpr unsigned BLOCKED_TIMEOUT = 15000;
protected:
    const char *Nick() const
    {
        return self->r.client ? self->r.client->netname : self->classname;
    }

    virtual void TouchedOtherEntity(const edict_t *entity) {}

    // This function produces very basic but reliable results.
    // Imitation of human-like aiming should be a burden of callers that prepare the desiredDirection.
    inline Vec3 GetNewViewAngles(const Vec3 &oldAngles, const Vec3 &desiredDirection,
                                 unsigned frameTime, float angularSpeedMultiplier = 1.0f) const
    {
        return GetNewViewAngles(oldAngles.Data(), desiredDirection, frameTime, angularSpeedMultiplier);
    }
    Vec3 GetNewViewAngles(const vec3_t oldAngles, const Vec3 &desiredDirection,
                          unsigned frameTime, float angularSpeedMultiplier = 1.0f) const;

    void UpdateReachChain(const ReachChainVector &oldReachChain,
                          ReachChainVector *currReachChain,
                          const AiEntityPhysicsState &state) const;
private:
    float GetChangedAngle(float oldAngle, float desiredAngle, unsigned frameTime,
                          float angularSpeedMultiplier, int angleIndex) const;
};

#endif
