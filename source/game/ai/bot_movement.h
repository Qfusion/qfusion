#ifndef QFUSION_BOT_MOVEMENT_H
#define QFUSION_BOT_MOVEMENT_H

#include "ai_base_ai.h"

class alignas(2) AiCampingSpot
{
    // Fields of this class are packed to allow cheap copying of class instances in bot movement prediction code
    friend class Bot;
    friend class BotCampingSpotState;

    short origin[3];
    short lookAtPoint[3];
    unsigned char radius;
    unsigned char alertness: 7;
    AiCampingSpot() {}
public:
    bool hasLookAtPoint: 1;

    inline float Radius() const { return radius * 2; }
    inline float Alertness() const { return alertness / 128.0f; }
    inline Vec3 Origin() const { return GetUnpacked4uVec(origin); }
    inline Vec3 LookAtPoint() const { return GetUnpacked4uVec(lookAtPoint); }
    // Warning! This does not set hasLookAtPoint, only used to store a vector in (initially unsused) lookAtPoint field
    // This behaviour is used when lookAtPoint is controlled manually by an external code.
    inline void SetLookAtPoint(const Vec3 &lookAtPoint_) { SetPacked4uVec(lookAtPoint_, lookAtPoint); }

    AiCampingSpot(const Vec3 &origin_, float radius_, float alertness_ = 0.75f)
        : radius((decltype(radius))(radius_ / 2)), alertness((decltype(alertness))(alertness_ * 128)), hasLookAtPoint(false)
    {
        SetPacked4uVec(origin_, origin);
    }

    AiCampingSpot(const vec3_t &origin_, float radius_, float alertness_ = 0.75f)
        : radius((decltype(radius))(radius_ / 2)), alertness((decltype(alertness))(alertness_ * 128)), hasLookAtPoint(false)
    {
        SetPacked4uVec(origin_, origin);
    }

    AiCampingSpot(const vec3_t &origin_, const vec3_t &lookAtPoint_, float radius_, float alertness_ = 0.75f)
        : radius((decltype(radius))(radius_ / 2)), alertness((decltype(alertness))(alertness_ * 128)), hasLookAtPoint(true)
    {
        SetPacked4uVec(origin_, origin);
        SetPacked4uVec(lookAtPoint_, lookAtPoint);
    }

    AiCampingSpot(const Vec3 &origin_, const Vec3 &lookAtPoint_, float radius_, float alertness_ = 0.75f)
        : radius((decltype(radius))(radius_ / 2)), alertness((decltype(alertness))(alertness_ * 128)), hasLookAtPoint(true)
    {
        SetPacked4uVec(origin_, origin);
        SetPacked4uVec(lookAtPoint_, lookAtPoint);
    }
};

class alignas(2) AiPendingLookAtPoint
{
    // Fields of this class are packed to allow cheap copying of class instances in bot movement prediction code
    friend struct BotPendingLookAtPointState;

    short origin[3];
    // Floating point values greater than 1.0f are allowed (unless they are significantly greater than 1.0f);
    unsigned short turnSpeedMultiplier;

    AiPendingLookAtPoint() {}
public:
    inline Vec3 Origin() const { return GetUnpacked4uVec(origin); }
    inline float TurnSpeedMultiplier() const { return turnSpeedMultiplier / 16.0f; };

    AiPendingLookAtPoint(const vec3_t origin_, float turnSpeedMultiplier_)
        : turnSpeedMultiplier((decltype(turnSpeedMultiplier))std::min(255.0f, turnSpeedMultiplier_ * 16.0f))
    {
        SetPacked4uVec(origin_, origin);
    }

    AiPendingLookAtPoint(const Vec3 &origin_, float turnSpeedMultiplier_)
        : turnSpeedMultiplier((decltype(turnSpeedMultiplier))std::min(255.0f, turnSpeedMultiplier_ * 16.0f))
    {
        SetPacked4uVec(origin_, origin);
    }
};

class alignas(4) BotInput
{
    // Todo: Pack since it is required to be normalized now?
    Vec3 intendedLookDir;
    // A copy of self->s.angles for modification
    // We do not want to do deeply hidden angles update in the aiming functions,
    // the BotInput should be only mutable thing in the related code.
    // Should be copied back to self->s.angles if it has been modified when the BotInput gets applied.
    Vec3 alreadyComputedAngles;
    unsigned char turnSpeedMultiplier;
    signed ucmdForwardMove: 2;
    signed ucmdSideMove: 2;
    signed ucmdUpMove: 2;
    bool attackButton: 1;
    bool specialButton: 1;
    bool walkButton: 1;
public:
    bool fireScriptWeapon: 1;
    bool isUcmdSet: 1;
    bool isLookDirSet: 1;
    bool hasAlreadyComputedAngles: 1;
    bool canOverrideUcmd: 1;
    bool shouldOverrideUcmd: 1;
    bool canOverrideLookVec: 1;
    bool shouldOverrideLookVec: 1;
    bool canOverridePitch: 1;
    bool applyExtraViewPrecision: 1;

    inline BotInput()
        : intendedLookDir(NAN, NAN, NAN),
          alreadyComputedAngles(NAN, NAN, NAN),
          turnSpeedMultiplier(16)
    {
        Clear();
    }

    inline void Clear()
    {
        // Do not use memset() call (there is few bytes to clear)
        turnSpeedMultiplier = 16;
        char *ptr = reinterpret_cast<char *>(&turnSpeedMultiplier) + 1;
        char *end = reinterpret_cast<char *>(this) + sizeof(BotInput);
        for (; ptr != end; ++ptr)
            *ptr = 0;
    }

    // Button accessors are kept for backward compatibility with existing bot movement code
    inline void SetAttackButton(bool isSet) { attackButton = isSet; }
    inline void SetSpecialButton(bool isSet) { specialButton = isSet; }
    inline void SetWalkButton(bool isSet) { walkButton = isSet; }

    inline bool IsAttackButtonSet() const { return attackButton; }
    inline bool IsSpecialButtonSet() const { return specialButton; }
    inline bool IsWalkButtonSet() const { return walkButton; }

    inline int ForwardMovement() const { return ucmdForwardMove; }
    inline int RightMovement() const { return ucmdSideMove; }
    inline int UpMovement() const { return ucmdUpMove; }

    inline bool IsCrouching() const { return UpMovement() < 0; }

    inline void SetForwardMovement(int movement) { ucmdForwardMove = movement; }
    inline void SetRightMovement(int movement) { ucmdSideMove = movement; }
    inline void SetUpMovement(int movement) { ucmdUpMove = movement; }

    inline void ClearMovementDirections()
    {
        ucmdForwardMove = 0;
        ucmdSideMove = 0;
        ucmdUpMove = 0;
    }

    inline void ClearButtons()
    {
        attackButton = false;
        specialButton = false;
        walkButton = false;
    }

    inline float TurnSpeedMultiplier() const { return turnSpeedMultiplier / 16.0f; }
    inline void SetTurnSpeedMultiplier(float value)
    {
        turnSpeedMultiplier = (decltype(turnSpeedMultiplier))(value * 16.0f);
    }

    inline void CopyToUcmd(usercmd_t *ucmd) const
    {
        ucmd->forwardmove = ForwardMovement();
        ucmd->sidemove = RightMovement();
        ucmd->upmove = UpMovement();

        ucmd->buttons = 0;
        if (attackButton)
            ucmd->buttons |= BUTTON_ATTACK;
        if (specialButton)
            ucmd->buttons |= BUTTON_SPECIAL;
        if (walkButton)
            ucmd->buttons |= BUTTON_WALK;
    }

    inline void SetAlreadyComputedAngles(const Vec3 &angles)
    {
        alreadyComputedAngles = angles;
        hasAlreadyComputedAngles = true;
    }

    inline const Vec3 &AlreadyComputedAngles() const
    {
#ifndef PUBLIC_BUILD
        if (!hasAlreadyComputedAngles)
            abort();
#endif
        return alreadyComputedAngles;
    }

    inline void SetIntendedLookDir(const Vec3 &intendedLookVec, bool alreadyNormalized = false)
    {
        SetIntendedLookDir(intendedLookVec.Data(), alreadyNormalized);
    }

    inline void SetIntendedLookDir(const vec3_t intendedLookVec, bool alreadyNormalized = false)
    {
        this->intendedLookDir.Set(intendedLookVec);
        if (!alreadyNormalized)
            this->intendedLookDir.NormalizeFast();
#ifndef PUBLIC_BUILD
        else if (fabsf(this->intendedLookDir.NormalizeFast() - 1.0f) > 0.1f)
            abort();
#endif
        this->isLookDirSet = true;
    }

    inline const Vec3 &IntendedLookDir() const
    {
#ifndef PUBLIC_BUILD
        if (isLookDirSet)
            abort();
#endif
        return intendedLookDir;
    }
};

struct BotAerialMovementState
{
protected:
    inline bool ShouldDeactivate(const edict_t *self, const class BotMovementPredictionContext *context = nullptr) const;
};

struct alignas(2) BotJumppadMovementState: protected BotAerialMovementState
{
    // Fields of this class are packed to allow cheap copying of class instances in bot movement prediction code
private:
    static_assert(MAX_EDICTS <= (1 << 10), "Cannot store jumppad entity number in 10 bits");
    unsigned short jumppadEntNum: 10;
public:
    // Should be set by Bot::TouchedJumppad() callback (its get called in ClientThink())
    // It gets processed by movement code in next frame
    bool hasTouchedJumppad: 1;
    // If this flag is set, bot is in "jumppad" movement state
    bool hasEnteredJumppad: 1;

    inline BotJumppadMovementState()
        : hasTouchedJumppad(false),
          hasEnteredJumppad(false) {}

    // Useless but kept for structural type conformance with other movement states
    inline void Frame(unsigned frameTime) {}

    inline bool IsActive() const
    {
        return (hasTouchedJumppad || hasEnteredJumppad);
    }

    inline void Deactivate()
    {
        hasTouchedJumppad = false;
        hasEnteredJumppad = false;
    }

    inline void Activate(const edict_t *triggerEnt)
    {
        hasTouchedJumppad = true;
        // Keep hasEnteredJumppad as-is (a jumppad might be touched again few millis later)
        jumppadEntNum = (decltype(jumppadEntNum))(ENTNUM(const_cast<edict_t *>(triggerEnt)));
    }

    inline void TryDeactivate(const edict_t *self, const BotMovementPredictionContext *context = nullptr)
    {
        if (ShouldDeactivate(self, context))
            Deactivate();
    }

    inline const edict_t *JumppadEntity() const { return game.edicts + jumppadEntNum; }
};

class alignas(2) BotWeaponJumpMovementState: protected BotAerialMovementState
{
    short jumpTarget[3];
    short fireTarget[3];
public:
    bool hasPendingWeaponJump: 1;
    bool hasTriggeredRocketJump: 1;
    bool hasCorrectedWeaponJump: 1;

    BotWeaponJumpMovementState()
        : hasPendingWeaponJump(false),
          hasTriggeredRocketJump(false),
          hasCorrectedWeaponJump(false) {}

    inline void Frame(unsigned frameTime) {}

    inline Vec3 JumpTarget() const { return GetUnpacked4uVec(jumpTarget); }
    inline Vec3 FireTarget() const { return GetUnpacked4uVec(fireTarget); }

    inline bool IsActive() const
    {
        return (hasPendingWeaponJump || hasTriggeredRocketJump || hasCorrectedWeaponJump);
    }

    inline void TryDeactivate(const edict_t *self, const class BotMovementPredictionContext *context = nullptr)
    {
        if (ShouldDeactivate(self, context))
            Deactivate();
    }

    inline void Deactivate()
    {
        hasPendingWeaponJump = false;
        hasTriggeredRocketJump = false;
        hasCorrectedWeaponJump = false;
    }

    inline void Activate(const Vec3 &jumpTarget_, const Vec3 &fireTarget_, unsigned timeoutPeriod, unsigned levelTime = level.time)
    {
        SetPacked4uVec(jumpTarget_, jumpTarget);
        SetPacked4uVec(fireTarget_, fireTarget);
        hasPendingWeaponJump = true;
        hasTriggeredRocketJump = false;
        hasCorrectedWeaponJump = false;
    }
};

class alignas(2) BotFlyUntilLandingMovementState: protected BotAerialMovementState
{
    short target[3];
    unsigned short landingDistanceThreshold: 14;
    bool isTriggered: 1;
    bool isLanding: 1;
public:
    inline BotFlyUntilLandingMovementState() : isTriggered(false), isLanding(false) {}

    inline void Frame(unsigned frameTime) {}

    inline bool ShouldBeLanding(const class BotMovementPredictionContext *context);

    inline void Activate(const vec3_t target_, float landingDistanceThreshold_)
    {
        SetPacked4uVec(target_, this->target);
        landingDistanceThreshold = (decltype(landingDistanceThreshold))(landingDistanceThreshold_);
        isTriggered = true;
        isLanding = false;
    }

    inline void Activate(const Vec3 &target_, float landingDistanceThreshold_)
    {
        Activate(target_.Data(), landingDistanceThreshold_);
    }

    inline bool IsActive() const { return isTriggered; }

    inline void Deactivate() { isTriggered = false; }

    inline void TryDeactivate(const edict_t *self, const class BotMovementPredictionContext *context = nullptr)
    {
        if (ShouldDeactivate(self, context))
            Deactivate();
    }
};

struct alignas(2) BotPendingLookAtPointState
{
    AiPendingLookAtPoint pendingLookAtPoint;
private:
    unsigned char timeLeft;
public:

    inline BotPendingLookAtPointState(): timeLeft(0) {}

    inline void Frame(unsigned frameTime)
    {
        timeLeft = (decltype(timeLeft))std::max(0, ((int)timeLeft * 4 - (int)frameTime) / 4);
    }

    inline bool IsActive() const { return timeLeft > 0; }

    // Timeout period is limited by 1000 millis
    inline void Activate(const AiPendingLookAtPoint &pendingLookAtPoint_, unsigned timeoutPeriod = 500U)
    {
        this->pendingLookAtPoint = pendingLookAtPoint_;
        this->timeLeft = (decltype(this->timeLeft))(std::min(1000U, timeoutPeriod) / 4);
    }

    inline void Deactivate() { timeLeft = 0; }

    inline void TryDeactivate(const edict_t *self, const class BotMovementPredictionContext *context = nullptr)
    {
        if (!IsActive())
            Deactivate();
    }
};

class alignas(2) BotCampingSpotState
{
    mutable AiCampingSpot campingSpot;
    // When to change chosen strafe dir
    mutable unsigned short moveDirsTimeLeft: 13;
    // When to change randomly chosen look-at-point (if the point is not initially specified)
    mutable unsigned short lookAtPointTimeLeft: 14;
    signed char forwardMove: 2;
    signed char rightMove: 2;
    bool isTriggered: 1;

    inline unsigned StrafeDirTimeout() const
    {
        return (unsigned)(400 + 100 * random() + 300 * (1.0f - campingSpot.Alertness()));
    }
    inline unsigned LookAtPointTimeout() const
    {
        return (unsigned)(800 + 200 * random() + 2000 * (1.0f - campingSpot.Alertness()));
    }
public:
    inline BotCampingSpotState()
        : moveDirsTimeLeft(0),
          lookAtPointTimeLeft(0),
          forwardMove(0),
          rightMove(0),
          isTriggered(false) {}

    inline void Frame(unsigned frameTime)
    {
        moveDirsTimeLeft = (decltype(moveDirsTimeLeft))std::max(0, (int)moveDirsTimeLeft - (int)frameTime);
        lookAtPointTimeLeft = (decltype(lookAtPointTimeLeft))std::max(0, (int)lookAtPointTimeLeft - (int)frameTime);
    }

    inline bool IsActive() const { return isTriggered; }

    inline void Activate(const AiCampingSpot &campingSpot_)
    {
        // Reset dir timers if and only if an actual origin has been significantly changed.
        // Otherwise this leads to "jitter" movement on the same point
        // when prediction errors prevent using a predicted action
        if (this->Origin().SquareDistance2DTo(campingSpot_.Origin()) > 16 * 16)
        {
            moveDirsTimeLeft = 0;
            lookAtPointTimeLeft = 0;
        }
        this->campingSpot = campingSpot_;
        this->isTriggered = true;
    }

    inline void Deactivate() { isTriggered = false; }

    inline void TryDeactivate(const edict_t *self, const class BotMovementPredictionContext *context = nullptr);

    inline Vec3 Origin() const { return campingSpot.Origin(); }
    inline float Radius() const { return campingSpot.Radius(); }

    inline AiPendingLookAtPoint GetOrUpdateRandomLookAtPoint() const
    {
        float turnSpeedMultiplier = 0.75f + 1.0f * campingSpot.Alertness();
        if (campingSpot.hasLookAtPoint)
            return AiPendingLookAtPoint(campingSpot.LookAtPoint(), turnSpeedMultiplier);
        if (lookAtPointTimeLeft)
            return AiPendingLookAtPoint(campingSpot.LookAtPoint(), turnSpeedMultiplier);

        Vec3 lookAtPoint(random(), random(), random());
        lookAtPoint.NormalizeFast();
        lookAtPoint += campingSpot.Origin();
        campingSpot.SetLookAtPoint(lookAtPoint);
        this->lookAtPointTimeLeft = (decltype(this->lookAtPointTimeLeft))LookAtPointTimeout();
        return AiPendingLookAtPoint(lookAtPoint, turnSpeedMultiplier);
    }

    inline float Alertness() const { return campingSpot.Alertness(); }

    inline int ForwardMove() const { return forwardMove; }
    inline int RightMove() const { return rightMove; }

    inline bool AreKeyMoveDirsValid() { return moveDirsTimeLeft > 0; }

    inline void SetKeyMoveDirs(int forwardMove_, int rightMove_)
    {
        this->forwardMove = (decltype(this->forwardMove))forwardMove_;
        this->rightMove = (decltype(this->rightMove))rightMove_;
    }
};

class alignas(2) BotCombatMoveDirsState
{
    // Can store up to 4096 millis without rounding.
    // Using a lower bits count does not make sence since either
    // the struct has a size of 2 bytes due to alignment
    // or there are only 4 bits left for this field what is not capable
    // to store the TIMEOUT_PERIOD with 16 millis rounding
    unsigned short timeLeft: 12;
    signed char forwardMove: 2;
    signed char rightMove: 2;
public:
    static constexpr unsigned short TIMEOUT_PERIOD = 512;

    inline void Frame(unsigned frameTime)
    {
        timeLeft = (decltype(timeLeft))std::max(0, ((int)timeLeft - (int)frameTime));
    }

    inline bool IsActive() const { return !timeLeft; }

    inline void TryDeactivate(const edict_t *self, const class BotMovementPredictionContext *context = nullptr) {}

    inline void Deactivate() { timeLeft = 0; }

    inline void Activate(int forwardMove_, int rightMove_)
    {
        this->forwardMove = (decltype(this->forwardMove))forwardMove_;
        this->rightMove = (decltype(this->rightMove))rightMove_;
        this->timeLeft = TIMEOUT_PERIOD / 8;
    }

    inline int ForwardMove() const { return forwardMove; }
    inline int RightMove() const { return rightMove; }
};

struct alignas(4) BotMovementState
{
    // We want to pack members tightly to reduce copying cost of this struct during the planning process
    static_assert(alignof(AiEntityPhysicsState) == 4, "Members order by alignment is broken");
    AiEntityPhysicsState entityPhysicsState;
    static_assert(alignof(BotCampingSpotState) == 2, "Members order by alignment is broken");
    BotCampingSpotState campingSpotState;
    static_assert(alignof(BotJumppadMovementState) == 2, "Members order by alignment is broken");
    BotJumppadMovementState jumppadMovementState;
    static_assert(alignof(BotWeaponJumpMovementState) == 2, "Members order by alignment is broken");
    BotWeaponJumpMovementState weaponJumpMovementState;
    static_assert(alignof(BotPendingLookAtPointState) == 2, "Members order by alignment is broken");
    BotPendingLookAtPointState pendingLookAtPointState;
    static_assert(alignof(BotFlyUntilLandingMovementState) == 2, "Members order by alignment is broken");
    BotFlyUntilLandingMovementState flyUntilLandingMovementState;
    static_assert(alignof(BotCombatMoveDirsState) == 2, "Members order by alignment is broken");
    BotCombatMoveDirsState combatMoveDirsState;

    inline void Frame(unsigned frameTime)
    {
        jumppadMovementState.Frame(frameTime);
        weaponJumpMovementState.Frame(frameTime);
        pendingLookAtPointState.Frame(frameTime);
        campingSpotState.Frame(frameTime);
        combatMoveDirsState.Frame(frameTime);
        flyUntilLandingMovementState.Frame(frameTime);
    }

    inline void TryDeactivateContainedStates(const edict_t *self, BotMovementPredictionContext *context)
    {
        jumppadMovementState.TryDeactivate(self, context);
        weaponJumpMovementState.TryDeactivate(self, context);
        pendingLookAtPointState.TryDeactivate(self, context);
        campingSpotState.TryDeactivate(self, context);
        combatMoveDirsState.TryDeactivate(self, context);
        flyUntilLandingMovementState.TryDeactivate(self, context);
    }

    inline void Reset()
    {
        jumppadMovementState.Deactivate();
        weaponJumpMovementState.Deactivate();
        pendingLookAtPointState.Deactivate();
        campingSpotState.Deactivate();
        combatMoveDirsState.Deactivate();
        flyUntilLandingMovementState.Deactivate();
    }

    inline unsigned GetContainedStatesMask() const
    {
        unsigned result = 0;
        result |= ((unsigned)(jumppadMovementState.IsActive())) << 0;
        result |= ((unsigned)(weaponJumpMovementState.IsActive())) << 1;
        result |= ((unsigned)(pendingLookAtPointState.IsActive())) << 2;
        result |= ((unsigned)(campingSpotState.IsActive())) << 3;
        // Skip combatMoveDirsState.
        // It either should not affect movement at all if regular movement is chosen,
        // or should be handled solely by the combat movement code.
        result |= ((unsigned)(flyUntilLandingMovementState.IsActive())) << 4;
        return result;
    }

    bool TestActualStatesForExpectedMask(unsigned expectedStatesMask, const edict_t *owner = nullptr) const;
};

struct BotMovementActionRecord
{
    BotInput botInput;
private:
    signed short modifiedVelocity[3];
public:
    signed char pendingWeapon: 7;
    bool hasModifiedVelocity: 1;

    inline BotMovementActionRecord()
        : pendingWeapon(-1),
          hasModifiedVelocity(false) {}

    inline void Clear()
    {
        botInput.Clear();
        pendingWeapon = -1;
        hasModifiedVelocity = false;
    }

    inline void SetModifiedVelocity(const Vec3 &velocity)
    {
        SetModifiedVelocity(velocity.Data());
    }

    inline void SetModifiedVelocity(const vec3_t velocity)
    {
        for (int i = 0; i < 3; ++i)
        {
            int snappedVelocityComponent = (int)(velocity[i] * PM_VECTOR_SNAP);
            if (snappedVelocityComponent > std::numeric_limits<signed short>::max())
                snappedVelocityComponent = std::numeric_limits<signed short>::max();
            else if (snappedVelocityComponent < std::numeric_limits<signed short>::min())
                snappedVelocityComponent = std::numeric_limits<signed short>::min();
            modifiedVelocity[i] = (signed short)snappedVelocityComponent;
        }
        hasModifiedVelocity = true;
    }

    inline Vec3 ModifiedVelocity() const
    {
        assert(hasModifiedVelocity);
        float scale = 1.0f / PM_VECTOR_SNAP;
        return Vec3(scale * modifiedVelocity[0], scale * modifiedVelocity[1], scale * modifiedVelocity[2]);
    }
};

class BotEnvironmentTraceCache
{
public:
    static constexpr float TRACE_DEPTH = 32.0f;

    struct TraceResult
    {
        trace_t trace;
        vec3_t traceDir;

        inline bool IsEmpty() const { return trace.fraction == 1.0f; }
    };

    enum class ObstacleAvoidanceResult
    {
        NO_OBSTACLES,
        CORRECTED,
        KEPT_AS_IS
    };

    static const int sideDirXYMoves[8][2];
    static const float sideDirXYFractions[8][2];
private:
    TraceResult results[16];
    unsigned resultsMask;
    bool didAreaTest;

    template <typename T> static inline void Assert(T condition)
    {
        // There is a define in the source file, we do not want to either expose it to this header
        // or to move all inlines that use it to the source
#ifndef PUBLIC_BUILD
        if (!condition) abort();
#endif
    }

    // The resultFlag arg is supplied only for assertions check
    inline const TraceResult &ResultForIndex(unsigned resultFlag, int index) const
    {
        Assert(resultFlag == 1 << index);
        Assert(resultFlag & this->resultsMask);
        return results[index];
    }

    ObstacleAvoidanceResult TryAvoidObstacles(class BotMovementPredictionContext *context,
                                              Vec3 *intendedLookVec,
                                              float correctionFraction,
                                              unsigned sidesShift);

    inline static void MakeTraceDir(unsigned dirNum, const vec3_t front2DDir, const vec3_t right2DDir, vec3_t traceDir);
    inline static bool CanSkipTracingForAreaHeight(const vec3_t origin, const aas_area_t &area, float minZOffset);
    void SetFullHeightCachedTracesEmpty(const vec3_t front2DDir, const vec3_t right2DDir);
    void SetJumpableHeightCachedTracesEmpty(const vec3_t front2DDir, const vec3_t right2DDir);
    bool TrySkipTracingForCurrOrigin(class BotMovementPredictionContext *context,
                                     const vec3_t front2DDir, const vec3_t right2DDir);

    inline unsigned SelectNonBlockedDirs(class BotMovementPredictionContext *context, unsigned *nonBlockedDirIndices);
public:
    enum Side
    {
        FRONT = 1 << 0,
        FIRST_SIDE = FRONT,
        BACK = 1 << 1,
        LEFT = 1 << 2,
        RIGHT = 1 << 3,
        FRONT_LEFT = 1 << 4,
        FRONT_RIGHT = 1 << 5,
        BACK_LEFT = 1 << 6,
        BACK_RIGHT = 1 << 7,
        LAST_SIDE = BACK_RIGHT
    };

    static constexpr unsigned FULL_SIDES_MASK = (1 << 8) - 1;
    static constexpr unsigned ALL_SIDES_MASK = (1 << 16) - 1;
    static constexpr unsigned JUMPABLE_SIDES_MASK = ALL_SIDES_MASK & ~FULL_SIDES_MASK;

    inline unsigned FullHeightMask(unsigned sidesMask) const
    {
        Assert(sidesMask & FULL_SIDES_MASK);
        return sidesMask;
    }
    inline unsigned JumpableHeightMask(unsigned sidesMask) const
    {
        Assert(sidesMask & FULL_SIDES_MASK);
        return sidesMask << 8;
    }

    inline const TraceResult &FullHeightFrontTrace() const { return ResultForIndex(FullHeightMask(FRONT), 0); }
    inline const TraceResult &FullHeightBackTrace() const { return ResultForIndex(FullHeightMask(BACK), 1); }
    inline const TraceResult &FullHeightLeftTrace() const { return ResultForIndex(FullHeightMask(LEFT), 2); }
    inline const TraceResult &FullHeightRightTrace() const { return ResultForIndex(FullHeightMask(RIGHT), 3); }
    inline const TraceResult &FullHeightFrontLeftTrace() const { return ResultForIndex(FullHeightMask(FRONT_LEFT), 4); }
    inline const TraceResult &FullHeightFrontRightTrace() const { return ResultForIndex(FullHeightMask(FRONT_RIGHT), 5); }
    inline const TraceResult &FullHeightBackLeftTrace() const { return ResultForIndex(FullHeightMask(BACK_LEFT), 6); }
    inline const TraceResult &FullHeightBackRightTrace() const { return ResultForIndex(FullHeightMask(BACK_RIGHT), 7); }

    inline const TraceResult &JumpableHeightFrontTrace() const { return ResultForIndex(JumpableHeightMask(FRONT), 8); }
    inline const TraceResult &JumpableHeightBackTrace() const { return ResultForIndex(JumpableHeightMask(BACK), 9); }
    inline const TraceResult &JumpableHeightLeftTrace() const { return ResultForIndex(JumpableHeightMask(LEFT), 10); }
    inline const TraceResult &JumpableHeightRightTrace() const { return ResultForIndex(JumpableHeightMask(RIGHT), 11); }
    inline const TraceResult &JumpableHeightFrontLeftTrace() const
    {
        return ResultForIndex(JumpableHeightMask(FRONT_LEFT), 12);
    }
    inline const TraceResult &JumpableHeightFrontRightTrace() const
    {
        return ResultForIndex(JumpableHeightMask(FRONT_RIGHT), 13);
    }
    inline const TraceResult &JumpableHeightBackLeftTrace() const
    {
        return ResultForIndex(JumpableHeightMask(BACK_LEFT), 14);
    }
    inline const TraceResult &JumpableHeightBackRightTrace() const
    {
        return ResultForIndex(JumpableHeightMask(BACK_RIGHT), 15);
    }

    inline BotEnvironmentTraceCache() : resultsMask(0), didAreaTest(false) {}

    void TestForResultsMask(class BotMovementPredictionContext *context, unsigned requiredResultsMask);

    inline ObstacleAvoidanceResult TryAvoidJumpableObstacles(class BotMovementPredictionContext *context,
                                                             Vec3 *intendedLookVec, float correctionFraction)
    {
        return TryAvoidObstacles(context, intendedLookVec, correctionFraction, 8);
    }
    inline ObstacleAvoidanceResult TryAvoidFullHeightObstacles(class BotMovementPredictionContext *context,
                                                               Vec3 *intendedLookVec, float correctionFraction)
    {
        return TryAvoidObstacles(context, intendedLookVec, correctionFraction, 0);
    }

    inline const TraceResult &FullHeightTraceForSideIndex(unsigned index) const
    {
        return ResultForIndex(1U << index, index);
    }
    inline const TraceResult &JumpableHeightTraceForSideIndex(unsigned index) const
    {
        return ResultForIndex(1U << (index + 8), index + 8);
    }

    void MakeRandomizedKeyMovesToTarget(BotMovementPredictionContext *context, const Vec3 &intendedMoveDir, int *keyMoves);
    void MakeKeyMovesToTarget(BotMovementPredictionContext *context, const Vec3 &intendedMoveDir, int *keyMoves);
    void MakeRandomKeyMoves(BotMovementPredictionContext *context, int *keyMoves);
};

class BotBaseMovementAction;

class BotMovementPredictionContext
{
    friend class BotTriggerPendingWeaponJumpMovementAction;
public:
    static constexpr unsigned MAX_PREDICTED_STATES = 48;
    static constexpr unsigned MAX_SAVED_LANDING_AREAS = 16;

    struct alignas(1) HitWhileRunningTestResult
    {
        bool canHitAsIs: 1;
        bool mayHitOverridingPitch: 1;

        inline HitWhileRunningTestResult()
        {
            static_assert(sizeof(*this) == 1, "");
            *((uint8_t *)(this)) = 0;
        }

        inline bool CanHit() const { return canHitAsIs || mayHitOverridingPitch; }

        // Use the method and not a static var (the method result should be inlined w/o any static memory access)
        static inline HitWhileRunningTestResult Failure() { return HitWhileRunningTestResult(); }
    };
private:
    struct PredictedMovementAction
    {
        AiEntityPhysicsState entityPhysicsState;
        BotMovementActionRecord record;
        BotBaseMovementAction *action;
        unsigned timestamp;
        unsigned stepMillis;
        unsigned movementStatesMask;
    };

    StaticVector<PredictedMovementAction, MAX_PREDICTED_STATES> predictedMovementActions;
    StaticVector<BotMovementState, MAX_PREDICTED_STATES> botMovementStatesStack;
    StaticVector<player_state_t, MAX_PREDICTED_STATES> playerStatesStack;
    StaticVector<signed char, MAX_PREDICTED_STATES> pendingWeaponsStack;

    edict_t *self;

    template <typename T, unsigned N>
    class CachesStack
    {
        static_assert(sizeof(uint64_t) * 8 >= N, "64-bit bitset capacity overflow");

        StaticVector<T, N> values;
        uint64_t isCachedBitset;

        inline void SetBit(unsigned bit) { isCachedBitset |= (1 << bit); }
        inline void ClearBit(unsigned bit) { isCachedBitset &= ~(1 << bit); }
    public:
        inline CachesStack(): isCachedBitset(0) {}

        inline void SetCachedValue(const T &value)
        {
            assert(values.size());
            SetBit(values.size() - 1);
            values.back() = value;
        }
        inline void SetCachedValue(T &&value)
        {
            assert(values.size());
            SetBit(values.size() - 1);
            values.back() = std::move(value);
        }
        // When cache stack growth for balancing is needed and no value exists for current stack pos, use this method
        inline void PushDummyNonCachedValue(T &&value = T())
        {
            ClearBit(values.size());
            values.emplace_back(std::move(value));
        }
        // Should be used when the cached type cannot be copied or moved (use this pointer to allocate a value in-place)
        inline T *UnsafeGrowForNonCachedValue()
        {
            ClearBit(values.size());
            return values.unsafe_grow_back();
        }
        inline T *GetUnsafeBufferForCachedValue()
        {
            SetBit(values.size() - 1);
            return &values[0] + (values.size() - 1);
        }
        inline const T *GetCached() const
        {
            assert(values.size());
            return (isCachedBitset & (1 << (values.size() - 1))) ? &values.back() : nullptr;
        }
        inline const T *GetCachedValueBelowTopOfStack() const
        {
            assert(values.size());
            if (values.size() == 1)
                return nullptr;
            return (isCachedBitset & (1 << (values.size() - 2))) ? &values[values.size() - 2] : nullptr;
        }

        inline unsigned Size() const { return values.size(); }
        // Use when cache stack is being rolled back
        inline void PopToSize(unsigned newSize)
        {
            assert(newSize <= values.size());
            values.truncate(newSize);
        }
    };

    CachesStack<Ai::ReachChainVector, MAX_PREDICTED_STATES> reachChainsCachesStack;
    CachesStack<BotInput, MAX_PREDICTED_STATES> defaultBotInputsCachesStack;
    CachesStack<HitWhileRunningTestResult, MAX_PREDICTED_STATES> mayHitWhileRunningCachesStack;
    StaticVector<BotEnvironmentTraceCache, MAX_PREDICTED_STATES> environmentTestResultsStack;
public:
    BotMovementState *movementState;
    BotMovementActionRecord *record;

    const player_state_t *oldPlayerState;
    player_state_t *currPlayerState;

    BotBaseMovementAction *actionSuggestedByAction;
    BotBaseMovementAction *activeAction;

    unsigned totalMillisAhead;
    unsigned predictionStepMillis;
    // Must be set to game.frameTime for the first step!
    unsigned oldStepMillis;

    unsigned topOfStackIndex;
    unsigned savepointTopOfStackIndex;

    bool isCompleted;
    bool cannotApplyAction;
    bool shouldRollback;

    struct FrameEvents
    {
        static constexpr auto MAX_TOUCHED_TRIGGERS = 32;
        int touchedTriggerEnts[MAX_TOUCHED_TRIGGERS];
        int numTouchedTriggers;

        bool hasJumped;
        bool hasDashed;
        bool hasWalljumped;
        bool hasTakenFallDamage;

        bool hasTouchedJumppad;
        bool hasTouchedTeleporter;
        bool hasTouchedPlatform;

        inline void Clear()
        {
            numTouchedTriggers = 0;
            hasJumped = false;
            hasDashed = false;
            hasWalljumped = false;
            hasTakenFallDamage = false;
            hasTouchedJumppad = false;
            hasTouchedTeleporter = false;
            hasTouchedPlatform = false;
        }
    };

    FrameEvents frameEvents;

    class BotBaseMovementAction *SuggestSuitableAction();
    inline class BotBaseMovementAction *SuggestAnyAction();

    inline Vec3 NavTargetOrigin() const;
    inline float NavTargetRadius() const;
    inline bool IsCloseToNavTarget() const;
    inline int CurrAasAreaNum() const;
    inline int NavTargetAasAreaNum() const;
    inline bool IsInNavTargetArea() const;

    const Ai::ReachChainVector &NextReachChain();
    inline BotEnvironmentTraceCache &EnvironmentTraceCache();
    inline BotEnvironmentTraceCache::ObstacleAvoidanceResult TryAvoidFullHeightObstacles(float correctionFraction);
    inline BotEnvironmentTraceCache::ObstacleAvoidanceResult TryAvoidJumpableObstacles(float correctionFraction);

    // Do not return boolean value, avoid extra branching. Checking results if necessary is enough.
    void NextReachNumAndTravelTimeToNavTarget(int *reachNum, int *travelTimeToNavTarget);

    inline int NextReachNum()
    {
        int results[2];
        NextReachNumAndTravelTimeToNavTarget(results, results + 1);
        return results[0];
    }
    inline int TravelTimeToNavTarget()
    {
        int results[2];
        NextReachNumAndTravelTimeToNavTarget(results, results + 1);
        return results[1];
    }

    inline BotMovementPredictionContext(edict_t *self_): self(self_) {}

    HitWhileRunningTestResult MayHitWhileRunning();

    void BuildPlan();
    bool NextPredictionStep();
    void SetupStackForStep();

    void NextMovementStep();

    inline const AiEntityPhysicsState &PhysicsStateBeforeStep() const
    {
        return predictedMovementActions[topOfStackIndex].entityPhysicsState;
    }
    inline bool CanGrowStackForNextStep() const
    {
        // Note: topOfStackIndex is an array index, MAX_PREDICTED_STATES is an array size
        return this->topOfStackIndex + 1 < MAX_PREDICTED_STATES;
    }
    inline void SaveActionOnStack(BotBaseMovementAction *action);

    // Frame index is restricted to topOfStack or topOfStack + 1
    inline void MarkSavepoint(BotBaseMovementAction *markedBy, unsigned frameIndex);
    inline void SetPendingRollback()
    {
        this->cannotApplyAction = true;
        this->shouldRollback = true;
    }
    inline void RollbackToSavepoint();
    inline void SetPendingWeapon(int weapon);
    inline void SaveSuggestedActionForNextFrame(BotBaseMovementAction *action);
    inline unsigned MillisAheadForFrameStart(unsigned frameIndex) const;

    class BotBaseMovementAction *GetCachedActionAndRecordForCurrTime(BotMovementActionRecord *record);

    void SetDefaultBotInput();

    void Debug(const char *format, ...) const;
    // We want to have a full control over movement code assertions, so use custom ones for this class
    inline void Assert(bool condition) const;
    template <typename T> inline void Assert(T conditionLikeValue) const
    {
        Assert(conditionLikeValue != 0);
    }

    inline float GetRunSpeed() const;
    inline float GetJumpSpeed() const;
    inline float GetDashSpeed() const;

    void OnInterceptedPredictedEvent(int ev, int parm);
    void OnInterceptedPMoveTouchTriggers(pmove_t *pm, vec3_t const previousOrigin);

    class BotBaseMovementAction *GetActionAndRecordForCurrTime(BotMovementActionRecord *record);

    // Might be called for failed attempts too
    void ShowBuiltPlanPath() const;
};

inline bool BotAerialMovementState::ShouldDeactivate(const edict_t *self, const BotMovementPredictionContext *context) const
{
    const edict_t *groundEntity;
    if (context)
        groundEntity = context->movementState->entityPhysicsState.GroundEntity();
    else
        groundEntity = self->groundentity;

    if (!groundEntity)
        return false;

    // TODO: Discover why general solid checks fail for world entity
    if (groundEntity == world)
        return true;

    if (groundEntity->s.solid == SOLID_YES || groundEntity->s.solid == SOLID_BMODEL)
        return true;

    return false;
}

inline bool BotFlyUntilLandingMovementState::ShouldBeLanding(const class BotMovementPredictionContext *context)
{
    if (isLanding)
        return true;
    const float *botOrigin = context->movementState->entityPhysicsState.Origin();
    Vec3 unpackedTarget(GetUnpacked4uVec(target));
    if (unpackedTarget.Z() > botOrigin[2] + 12.0f)
        return false;
    const float distanceThreshold = this->landingDistanceThreshold * 4.0f;
    if (unpackedTarget.SquareDistanceTo(botOrigin) > distanceThreshold * distanceThreshold)
        return false;

    isLanding = true;
    return true;
}

inline void BotCampingSpotState::TryDeactivate(const edict_t *self, const class BotMovementPredictionContext *context)
{
    const float *botOrigin = context ? context->movementState->entityPhysicsState.Origin() : self->s.origin;
    const float distanceThreshold = 1.5f * campingSpot.Radius();
    if (this->Origin().SquareDistance2DTo(botOrigin) > distanceThreshold * distanceThreshold)
        Deactivate();
}

class Bot;

class BotBaseMovementAction
{
    friend class BotMovementPredictionContext;
    Bot *bot;
    void RegisterSelf();
    const char *name;
protected:
    edict_t *self;
    int debugColor;

    // Used to establish a direct mapping between integers and actions.
    // It is very useful for algorithms that involve lookup tables addressed by this field.
    unsigned actionNum;

    Vec3 originAtSequenceStart;

    unsigned sequenceStartFrameIndex;
    unsigned sequenceEndFrameIndex;

    // Has the action been completely disabled in current planning session for further planning
    bool isDisabledForPlanning;
    // These flags are used by default CheckPredictionStepResults() implementation.
    // Set these flags in child class to tweak the mentioned method behaviour.
    bool stopPredictionOnTouchingJumppad;
    bool stopPredictionOnTouchingTeleporter;
    bool stopPredictionOnTouchingPlatform;
    bool stopPredictionOnTouchingNavEntity;
    bool stopPredictionOnEnteringWater;

    inline BotBaseMovementAction &DummyAction();
    inline BotBaseMovementAction &DefaultWalkAction();
    inline BotBaseMovementAction &DefaultBunnyAction();
    inline BotBaseMovementAction &FallbackBunnyAction();
    inline class BotFlyUntilLandingMovementAction &FlyUntilLandingAction();
    inline class BotLandOnSavedAreasMovementAction &LandOnSavedAreasAction();

    void Debug(const char *format, ...) const;
    // We want to have a full control over movement code assertions, so use custom ones for this class
    inline void Assert(bool condition) const;
    template <typename T> inline void Assert(T conditionLikeValue) const
    {
        Assert(conditionLikeValue != 0);
    }

    inline bool GenericCheckIsActionEnabled(BotMovementPredictionContext *context,
                                            BotBaseMovementAction *suggestedAction = nullptr) const
    {
        // Put likely case first
        if (!isDisabledForPlanning)
            return true;

        context->cannotApplyAction = true;
        context->actionSuggestedByAction = suggestedAction;
        Debug("The action has been completely disabled for further planning\n");
        return false;
    }

    struct AreaAndScore
    {
        int areaNum;
        float score;
        AreaAndScore(int areaNum_, float score_): areaNum(areaNum_), score(score_) {}
        bool operator<(const AreaAndScore &that) const { return score > that.score; }
    };
public:
    inline BotBaseMovementAction(class Bot *bot_, const char *name_, int debugColor_ = 0)
        : bot(bot_),
          name(name_),
          debugColor(debugColor_),
          originAtSequenceStart(0, 0, 0),
          stopPredictionOnTouchingJumppad(true),
          stopPredictionOnTouchingTeleporter(true),
          stopPredictionOnTouchingPlatform(true),
          stopPredictionOnTouchingNavEntity(true),
          stopPredictionOnEnteringWater(true)
    {
        RegisterSelf();
    }
    virtual void PlanPredictionStep(BotMovementPredictionContext *context) = 0;
    virtual void ExecActionRecord(const BotMovementActionRecord *record,
                                  BotInput *inputWillBeUsed,
                                  BotMovementPredictionContext *context = nullptr);

    virtual void CheckPredictionStepResults(BotMovementPredictionContext *context);

    virtual void BeforePlanning();
    virtual void AfterPlanning() {}

    // If an action has been applied consequently in N frames, these frames are called an application sequence.
    // Usually an action is valid and can be applied in all application sequence frames except these cases:
    // N = 1 and the first (and the last) action application is invalid
    // N > 1 and the last action application is invalid
    // The first callback is very useful for saving some initial state
    // related to the frame for further checks during the entire application sequence.
    // The second callback is provided for symmetry reasons
    // (e.g. any resources that are allocated in the first callback might need cleanup).
    virtual void OnApplicationSequenceStarted(BotMovementPredictionContext *context);
    enum SequenceStopReason
    {
        SUCCEEDED,
        SWITCHED,
        FAILED
    };

    // Might be called in a next frame, thats what stoppedAtFrameIndex is.
    // If application sequence has failed, stoppedAtFrameIndex is ignored.
    virtual void OnApplicationSequenceStopped(BotMovementPredictionContext *context,
                                              SequenceStopReason reason,
                                              unsigned stoppedAtFrameIndex);

    inline unsigned SequenceDuration(const BotMovementPredictionContext *context) const;

    inline const char *Name() const { return name; }
    inline int DebugColor() const { return debugColor; }
    inline unsigned ActionNum() const { return actionNum; }
    inline bool IsDisabledForPlanning() const { return isDisabledForPlanning; }
};

#define DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(name, debugColor_) \
    name(class Bot *bot_): BotBaseMovementAction(bot_, #name, debugColor_) {}

class BotDummyMovementAction: public BotBaseMovementAction
{
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotDummyMovementAction, COLOR_RGB(0, 0, 0));
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
    void CheckPredictionStepResults(BotMovementPredictionContext *context) override
    {
        AI_FailWith(__FUNCTION__, "This method should never get called (PlanMovmementStep() should stop planning)\n");
    }
};

class BotHandleTriggeredJumppadMovementAction: public BotBaseMovementAction
{
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotHandleTriggeredJumppadMovementAction, COLOR_RGB(0, 128, 128));
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
};

class BotLandOnSavedAreasMovementAction: public BotBaseMovementAction
{
    friend class BotHandleTriggeredJumppadMovementAction;
    friend class BotTryWeaponJumpShortcutMovementAction;

    StaticVector<int, BotMovementPredictionContext::MAX_SAVED_LANDING_AREAS> savedLandingAreas;

    int currAreaIndex;
    unsigned totalTestedAreas;

    static constexpr auto MAX_BBOX_AREAS = BotMovementPredictionContext::MAX_SAVED_LANDING_AREAS * 2;
    typedef StaticVector<AreaAndScore, MAX_BBOX_AREAS> FilteredRawAreas;
    typedef StaticVector<AreaAndScore, BotMovementPredictionContext::MAX_SAVED_LANDING_AREAS> ReachCheckedAreas;

    float SuggestInitialBBoxSide(const vec3_t origin, const vec3_t target);
    static constexpr auto SIDE_STEP_MULTIPLIER = 1.35f;
    inline void MakeBBoxDimensions(const vec3_t target, float side, vec3_t mins, vec3_t maxs);

    void FilterRawAreas(const vec3_t start, const vec3_t target, int *rawAreas, int numRawAreas, FilteredRawAreas &result);
    void CheckAreasNavTargetReachability(FilteredRawAreas &rawAreas, int navTargetAreaNum, ReachCheckedAreas &result);

    inline void SaveLandingAreas(const vec3_t target, BotMovementPredictionContext *context);
    // For each candidate area checks reachability from the area to the nav target area
    void SaveLandingAreasForDefinedNavTarget(const vec3_t target, int navTargetAreaNum, BotMovementPredictionContext *context);
    // Generic version that does not perform reachability checks
    // (Sometimes this movement action gets triggered when there is (temporarily) no nav target area)
    void SaveLandingAreasForUndefinedNavTarget(const vec3_t target, BotMovementPredictionContext *context);
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotLandOnSavedAreasMovementAction, COLOR_RGB(255, 0, 255));
    bool TryLandingStepOnArea(int areaNum, BotMovementPredictionContext *context);
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
    void CheckPredictionStepResults(BotMovementPredictionContext *context) override;
    void BeforePlanning() override;
    void AfterPlanning() override;
};

class BotRidePlatformMovementAction: public BotBaseMovementAction
{
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotRidePlatformMovementAction, COLOR_RGB(128, 128, 0));
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
};

class BotSwimMovementAction: public BotBaseMovementAction
{
public:
    BotSwimMovementAction(class Bot *bot_)
        : BotBaseMovementAction(bot_, "BotSwimMovementAction", COLOR_RGB(0, 0, 255))
    {
        this->stopPredictionOnEnteringWater = false;
    }
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
    void CheckPredictionStepResults(BotMovementPredictionContext *context) override;
};

class BotFlyUntilLandingMovementAction: public BotBaseMovementAction
{
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotFlyUntilLandingMovementAction, COLOR_RGB(0, 255, 0));
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
};

class BotCampASpotMovementAction: public BotBaseMovementAction
{
    unsigned disabledForApplicationFrameIndex;
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotCampASpotMovementAction, COLOR_RGB(128, 0, 128));
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
    void CheckPredictionStepResults(BotMovementPredictionContext *context) override;
    void OnApplicationSequenceStopped(BotMovementPredictionContext *context,
                                      SequenceStopReason stopReason,
                                      unsigned stoppedAtFrameIndex) override;
    void BeforePlanning() override
    {
        BotBaseMovementAction::BeforePlanning();
        disabledForApplicationFrameIndex = std::numeric_limits<unsigned>::max();
    }
};

class BotWalkCarefullyMovementAction: public BotBaseMovementAction
{
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotWalkCarefullyMovementAction, COLOR_RGB(128, 0, 255));
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
};

class BotGenericRunBunnyingMovementAction: public BotBaseMovementAction
{
protected:
    Vec3 savedIntendedLookVec;
    bool hasSavedIntendedLookVec;

    int minTravelTimeToNavTargetSoFar;

    // A fraction of speed gain per frame time.
    // Might be negative, in this case it limits allowed speed loss
    float minDesiredSpeedGainPerSecond;
    unsigned currentSpeedLossSequentialMillis;
    unsigned tolerableSpeedLossSequentialMillis;

    // When bot bunnies over a gap, its target either becomes unreachable
    // or travel time is calculated from the bottom of the pit.
    // These timers allow to temporarily skip targer reachability/travel time tests.
    unsigned currentUnreachableTargetSequentialMillis;
    unsigned tolerableUnreachableTargetSequentialMillis;
    unsigned currentGreaterTravelTimeSequentialMillis;
    unsigned tolerableGreaterTravelTimeSequentialMillis;

    // There is a mechanism for completely disabling an action for further planning by setting isDisabledForPlanning flag.
    // However we need a more flexible way of disabling an action after an failed application sequence.
    // A sequence started from different frame that the failed one might succeed.
    // An application sequence will not start at the frame indexed by this value.
    unsigned disabledForApplicationFrameIndex;

    bool supportsObstacleAvoidance;
    bool shouldTryObstacleAvoidance;
    bool isTryingObstacleAvoidance;

    inline void ResetObstacleAvoidanceState()
    {
        shouldTryObstacleAvoidance = false;
        isTryingObstacleAvoidance = false;
    }

    enum class WalljumpingMode: char
    {
        NEVER,
        ALWAYS,
        TRY_FIRST
    };
    WalljumpingMode walljumpingMode;
    // Ignored if an action does not support walljumping
    bool isTryingWalljumping;
    bool hasTestedWalljumping;
    bool mightHasFailedWalljumping;

    inline void ResetWalljumpingState()
    {
        isTryingWalljumping = false;
        hasTestedWalljumping = false;
        mightHasFailedWalljumping = false;
    }

    void SetupCommonBunnyingInput(BotMovementPredictionContext *context);
    void CheatingAccelerate(BotMovementPredictionContext *context, float frac) const;
    void CheatingCorrectVelocity(BotMovementPredictionContext *context, float velocity2DDirDotToTarget2DDir, Vec3 toTargetDir2D) const;
    // TODO: Mark as virtual in base class and mark as final here to avoid a warning about hiding parent member?
    bool GenericCheckIsActionEnabled(BotMovementPredictionContext *context, BotBaseMovementAction *suggestedAction);
    bool CheckCommonBunnyingActionPreconditions(BotMovementPredictionContext *context);
    bool SetupBunnying(const Vec3 &intendedLookVec, BotMovementPredictionContext *context);
    bool CanFlyAboveGroundRelaxed(const BotMovementPredictionContext *context) const;
    bool CanSetWalljump(BotMovementPredictionContext *context) const;
    inline void TrySetWalljump(BotMovementPredictionContext *context);

    // Can be overridden for finer control over tests
    virtual bool CheckStepSpeedGainOrLoss(BotMovementPredictionContext *context);
    bool IsMovingIntoNavEntity(BotMovementPredictionContext *context) const;
public:
    BotGenericRunBunnyingMovementAction(class Bot *bot_, const char *name_, int debugColor_ = 0)
        : BotBaseMovementAction(bot_, name_, debugColor_),
          savedIntendedLookVec(0, 0, 0),
          minDesiredSpeedGainPerSecond(0.0f),
          tolerableSpeedLossSequentialMillis(300),
          tolerableUnreachableTargetSequentialMillis(700),
          tolerableGreaterTravelTimeSequentialMillis(350),
          supportsObstacleAvoidance(false),
          walljumpingMode(WalljumpingMode::NEVER)
    {
        ResetObstacleAvoidanceState();
        ResetWalljumpingState();
    }

    void CheckPredictionStepResults(BotMovementPredictionContext *context) override;
    void OnApplicationSequenceStarted(BotMovementPredictionContext *context) override;
    void OnApplicationSequenceStopped(BotMovementPredictionContext *context,
                                      SequenceStopReason reason,
                                      unsigned stoppedAtFrameIndex) override;
    void BeforePlanning() override;
};

#define DECLARE_BUNNYING_MOVEMENT_ACTION_CONSTRUCTOR(name, debugColor_) \
    name(class Bot *bot_): BotGenericRunBunnyingMovementAction(bot_, #name, debugColor_)

class BotBunnyStraighteningReachChainMovementAction: public BotGenericRunBunnyingMovementAction
{
    bool TryStraightenLookVec(Vec3 *intendedLookVec, BotMovementPredictionContext *context);
public:
    DECLARE_BUNNYING_MOVEMENT_ACTION_CONSTRUCTOR(BotBunnyStraighteningReachChainMovementAction, COLOR_RGB(0, 192, 192))
    {
        supportsObstacleAvoidance = true;
        walljumpingMode = WalljumpingMode::TRY_FIRST;
    }
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
};

class BotBunnyToBestShortcutAreaMovementAction: public BotGenericRunBunnyingMovementAction
{
    int FindBestShortcutArea(BotMovementPredictionContext *context) const;
public:
    DECLARE_BUNNYING_MOVEMENT_ACTION_CONSTRUCTOR(BotBunnyToBestShortcutAreaMovementAction, COLOR_RGB(255, 64, 0))
    {
        supportsObstacleAvoidance = false;
        walljumpingMode = WalljumpingMode::TRY_FIRST;
    }
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
};

class BotBunnyInVelocityDirectionMovementAction: public BotGenericRunBunnyingMovementAction
{
public:
    DECLARE_BUNNYING_MOVEMENT_ACTION_CONSTRUCTOR(BotBunnyInVelocityDirectionMovementAction, COLOR_RGB(192, 0, 0))
    {
        supportsObstacleAvoidance = false;
        walljumpingMode = WalljumpingMode::ALWAYS;
    }
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
};

class BotWalkToBestNearbyTacticalSpotMovementAction: public BotBaseMovementAction
{
    inline void SetupMovementToTacticalSpot(BotMovementPredictionContext *context, const vec3_t spotOrigin);
    inline void SetupMovementInTargetArea(BotMovementPredictionContext *context);
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotWalkToBestNearbyTacticalSpotMovementAction, COLOR_RGB(0, 72, 128));
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
    void CheckPredictionStepResults(BotMovementPredictionContext *context) override;
};

class BotCombatDodgeSemiRandomlyToTargetMovementAction: public BotBaseMovementAction
{
    int minTravelTimeToTarget;
    float totalCovered2DDistance;

    unsigned maxAttempts;
    unsigned attemptNum;

    inline bool ShouldTryRandomness() { return attemptNum < maxAttempts / 2; }
    inline bool ShouldTrySpecialMovement()
    {
        // HACK for easy bots to disable special movement in combat
        // (maxAttempts is 2 for easy bots and is 4 otherwise)
        // This approach seems more cache friendly than self->ai->botRef... chasing
        // not to mention we cannot access Bot members in this header due to its incomplete definition
        return maxAttempts > 2 && !(attemptNum & 1);
    }

    void UpdateKeyMoveDirs(BotMovementPredictionContext *context);
public:
    DECLARE_MOVEMENT_ACTION_CONSTRUCTOR(BotCombatDodgeSemiRandomlyToTargetMovementAction, COLOR_RGB(192, 192, 192));
    void PlanPredictionStep(BotMovementPredictionContext *context) override;
    void CheckPredictionStepResults(BotMovementPredictionContext *context) override;
    void OnApplicationSequenceStarted(BotMovementPredictionContext *context) override;
    void OnApplicationSequenceStopped(BotMovementPredictionContext *context,
                                      SequenceStopReason stopReason,
                                      unsigned stoppedAtFrameIndex) override;
    void BeforePlanning() override;
};

#undef DECLARE_BUNNYING_MOVEMENT_ACTION_CONSTRUCTOR

#undef DECLARE_MOVEMENT_ACTION_CONSTRUCTOR

#endif
