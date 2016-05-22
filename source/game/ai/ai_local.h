/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
--------------------------------------------------------------
The ACE Bot is a product of Steve Yeager, and is available from
the ACE Bot homepage, at http://www.axionfx.com/ace.

This program is a modification of the ACE Bot, and is therefore
in NO WAY supported by Steve Yeager.
*/

//==========================================================
#ifndef AI_LOCAL_H
#define AI_LOCAL_H

#include "edict_ref.h"
#include "../../gameshared/q_collision.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include <algorithm>
#include <utility>
#include <stdarg.h>

#define AI_DEFAULT_YAW_SPEED	( 35 * 5 )
#define AI_COMBATMOVE_TIMEOUT	( 500 )
#define AI_YAW_ACCEL		( 95 * FRAMETIME )

// Platform states:
#define	STATE_TOP	    0
#define	STATE_BOTTOM	1
#define STATE_UP	    2
#define STATE_DOWN	    3

#define MAX_GOALENTS MAX_EDICTS

#define	AI_STEPSIZE	STEPSIZE    // 18
#define AI_JUMPABLE_HEIGHT		50
#define AI_JUMPABLE_DISTANCE	360
#define AI_WATERJUMP_HEIGHT		24
#define AI_MIN_RJ_HEIGHT		128
#define AI_MAX_RJ_HEIGHT		512
#define AI_GOAL_SR_RADIUS		200
#define AI_GOAL_SR_LR_RADIUS	600

constexpr int AI_GOAL_SR_MILLIS = 750;
constexpr int AI_GOAL_SR_LR_MILLIS = 1500;

#define MASK_AISOLID        ( CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY|CONTENTS_MONSTERCLIP )

enum class GoalFlags
{
	NONE = 0x0,
	REACH_AT_TOUCH = 0x1,
	REACH_ENTITY = 0x2,
	DROPPED_ENTITY = 0x4
};

inline GoalFlags operator|(const GoalFlags &lhs, const GoalFlags &rhs) { return (GoalFlags)((int)lhs | (int)rhs); }
inline GoalFlags operator&(const GoalFlags &lhs, const GoalFlags &rhs) { return (GoalFlags)((int)lhs & (int)rhs); }

class NavEntity
{
	friend class GoalEntitiesRegistry;
	int id;
	int aasAreaNum;
	GoalFlags goalFlags;
	edict_t *ent;
	NavEntity *prev, *next;
public:
	inline int Id() const { return id; }
	inline int AasAreaNum() const { return aasAreaNum; }
	inline Vec3 Origin() const { return Vec3(ent->s.origin); }
	inline const gsitem_t *Item() const { return ent->item; }
	inline const char *Name() const { return ent->classname; }
	inline bool IsEnabled() const { return ent && ent->r.inuse; }
	inline bool IsDisabled() const { return !ent || !ent->r.inuse; }
	inline bool IsBasedOnEntity(const edict_t *e) const { return e && this->ent == e; }
	inline bool IsBasedOnSomeEntity() const { return ent != nullptr; }
	inline bool IsClient() const { return ent->r.client != nullptr; }
	inline bool IsSpawnedAtm() const { return ent->r.solid != SOLID_NOT; }
	inline bool ToBeSpawnedLater() const { return ent->r.solid == SOLID_NOT; }
	inline bool IsDroppedEntity() const { return ent && GoalFlags::NONE != (goalFlags & GoalFlags::DROPPED_ENTITY); }
	inline unsigned DroppedEntityTimeout() const
	{
		if (!IsDroppedEntity())
			return std::numeric_limits<unsigned>::max();
		return ent->nextThink;
	}
	inline bool ShouldBeReachedAtTouch() const
	{
		return GoalFlags::NONE != (goalFlags & GoalFlags::REACH_AT_TOUCH);
	}
	// Returns true if it is enough to be close to the goal (not to pick up an item)
	// to reach it and the grabber is close enough to reach it atm.
	inline bool IsCloseEnoughToBeConsideredReached(const edict_t *grabber) const
	{
		return !ShouldBeReachedAtTouch() && (Origin() - grabber->s.origin).SquaredLength() < 32.0f * 32.0f;
	}

	// Returns level.time when the item is already spawned
	// Returns zero if spawn time is unknown
	// Returns spawn time when the item is not spawned and spawn time may be predicted
	unsigned SpawnTime() const;
};

class GoalEntitiesRegistry
{
	NavEntity goalEnts[MAX_GOALENTS];
	NavEntity *entGoals[MAX_EDICTS];
	NavEntity *goalEntsFree;
	NavEntity goalEntsHeadnode;

	static GoalEntitiesRegistry instance;

	NavEntity *AllocGoalEntity();
	void FreeGoalEntity(NavEntity *navEntity);
public:
	void Init();

	NavEntity *AddGoalEntity(edict_t *ent, int aasAreaNum, GoalFlags flags = GoalFlags::NONE);
	void RemoveGoalEntity(NavEntity *navEntity);

	inline NavEntity *GoalEntityForEntity(edict_t *ent)
	{
		if (!ent) return nullptr;
		return entGoals[ENTNUM(ent)];
	}

	inline edict_t *GetGoalEntity(int index) { return goalEnts[index].ent; }
	inline int GetNextGoalEnt(int index) { return goalEnts[index].prev->id; }

	class GoalEntitiesIterator
	{
		friend class GoalEntitiesRegistry;
		NavEntity *currEntity;
		inline GoalEntitiesIterator(NavEntity *currEntity): currEntity(currEntity) {}
	public:
		inline NavEntity *operator*() { return currEntity; }
		inline const NavEntity *operator*() const { return currEntity; }
		inline void operator++() { currEntity = currEntity->prev; }
		inline bool operator!=(const GoalEntitiesIterator &that) const { return currEntity != that.currEntity; }
	};
	inline GoalEntitiesIterator begin() { return GoalEntitiesIterator(goalEntsHeadnode.prev); }
	inline GoalEntitiesIterator end() { return GoalEntitiesIterator(&goalEntsHeadnode); }

	static inline GoalEntitiesRegistry *Instance() { return &instance; }
};

#define FOREACH_GOALENT(goalEnt) for (auto *goalEnt : *GoalEntitiesRegistry::Instance())

//=============================================================
//	WEAPON DECISSIONS DATA
//=============================================================

enum
{
	AI_AIMSTYLE_INSTANTHIT,
	AI_AIMSTYLE_PREDICTION,
	AI_AIMSTYLE_PREDICTION_EXPLOSIVE,
	AI_AIMSTYLE_DROP,

	AIWEAP_AIM_TYPES
};

enum
{
	AIWEAP_MELEE_RANGE,
	AIWEAP_SHORT_RANGE,
	AIWEAP_MEDIUM_RANGE,
	AIWEAP_LONG_RANGE,

	AIWEAP_RANGES
};

typedef struct
{
	int aimType;
	float RangeWeight[AIWEAP_RANGES];
} ai_weapon_t;

extern ai_weapon_t AIWeapons[WEAP_TOTAL];

//----------------------------------------------------------

#include "aas.h"

int FindAASReachabilityToGoalArea(
	int fromAreaNum, const vec3_t fromOrigin, int goalAreaNum, const edict_t *ignoreInTrace, int travelFlags);

int FindAASTravelTimeToGoalArea(
	int fromAreaNum, const vec3_t fromOrigin, int goalAreaNum, const edict_t *ignoreInTrace, int travelFlags);

float FindSquareDistanceToGround(const vec3_t origin, const edict_t *ignoreInTrace, float traceDepth = 999999.0f);
float FindDistanceToGround(const vec3_t origin, const edict_t *ignoreInTrace, float traceDepth = 999999.0f);

class AiFrameAwareUpdatable
{
protected:
	unsigned frameAffinityModulo;
	unsigned frameAffinityOffset;

	bool ShouldSkipThinkFrame()
	{
		// Check whether the modulo has not been set yet
		return frameAffinityModulo == 0 || level.framenum % frameAffinityModulo != frameAffinityOffset;
	}

	// This method group is called on each frame. See Update() for calls order.
	// It is not recommended (but not forbidden) to do CPU-intensive updates in these methods.
	virtual void Frame() {}
	virtual void PreFrame() {}
	virtual void PostFrame() {}

	inline void CheckIsInThinkFrame(const char *function)
	{
		if (ShouldSkipThinkFrame())
		{
			const char *format = "%s has been called not in think frame: frame#=%d, modulo=%d, offset=%d\n";
			G_Printf(format, function, frameAffinityModulo, frameAffinityOffset);
			printf(format, function, frameAffinityModulo, frameAffinityOffset);
			abort();
		}
	}

	// This method group is called only when frame affinity allows to do it. See Update() for calls order.
	// It is recommended (but not mandatory) to do all CPU-intensive updates in these methods instead of Frame();
	virtual void Think() {}
	virtual void PreThink() {}
	virtual void PostThink() {}

	// May be overridden if some actions should be performed when a frame affinity is set
	virtual void SetFrameAffinity(unsigned modulo, unsigned offset)
	{
		frameAffinityModulo = modulo;
		frameAffinityOffset = offset;
	}
public:
	AiFrameAwareUpdatable(): frameAffinityModulo(0), frameAffinityOffset(0) {}

	// Call this method to update an instance state.
	void Update()
	{
		PreFrame();
		Frame();
		if (!ShouldSkipThinkFrame())
		{
			PreThink();
			Think();
			PostThink();
		}
		PostFrame();
	}
};

class AiGametypeBrain: public AiFrameAwareUpdatable
{
protected:
	AiGametypeBrain() {};

	// May be instantiated dynamically with a some subtype of this class in future
	static AiGametypeBrain instance;
	virtual void Frame() override;
public:
	// May return some of subtypes of this class depending on a gametype in future
	static inline AiGametypeBrain *Instance() { return &instance; }
	void ClearGoals(NavEntity *canceledGoal, class Ai *goalGrabber);
};

class AiBaseTeamBrain: public AiFrameAwareUpdatable
{
	friend class Bot;  // Bots should be able to notify its team in destructor when they get dropped immediately
	friend class AiGametypeBrain;

	// We can't initialize these vars in constructor, because game exports may be not yet intialized.
	// These values are set to -1 in constructor and computed on demand
	mutable int svFps;
	mutable int svSkill;

	// These vars are used instead of AiFrameAwareUpdatable for lazy intiailization
	mutable int teamBrainAffinityModulo;
	mutable int teamBrainAffinityOffset;
	static constexpr int MAX_AFFINITY_OFFSET = 4;
	// This array contains count of bots that use corresponding offset for each possible affinity offset
	unsigned affinityOffsetsInUse[MAX_AFFINITY_OFFSET];

	// These arrays store copies of bot affinities to be able to access them even if the bot reference has been lost
	unsigned char botAffinityModulo[MAX_CLIENTS];
	unsigned char botAffinityOffsets[MAX_CLIENTS];

	unsigned AffinityModulo() const;
	unsigned TeamAffinityOffset() const;

	void InitTeamAffinity() const;  // Callers are const ones, and only mutable vars are modified

	// This function instantiates another brain instance and registers its shutdown hook
	static AiBaseTeamBrain *CreateTeamBrain(int team);
protected:
	AiBaseTeamBrain(int team);
	virtual ~AiBaseTeamBrain() {}

	const int team;
	int prevFrameBotsCount;
	int prevFrameBots[MAX_CLIENTS];

	int currBotsCount;
	int currBots[MAX_CLIENTS];

	void AddBot(int botEntNum);
	void RemoveBot(int botEntNum);
	virtual void OnBotAdded(int botEntNum) {};
	virtual void OnBotRemoved(int botEntNum) {};

	void AcquireBotFrameAffinity(int entNum);
	void ReleaseBotFrameAffinity(int entNum);
	void SetBotFrameAffinity(int bot, unsigned modulo, unsigned offset);

	void CheckTeamChanges();

	virtual void Frame() override;

	inline int GetCachedCVar(int *cached, const char *name) const
	{
		if (*cached == -1)
		{
			*cached = (int)trap_Cvar_Value(name);
		}
		return *cached;
	}

	inline int ServerFps() const { return GetCachedCVar(&svFps, "sv_fps"); }
	inline int ServerSkill() const { return GetCachedCVar(&svSkill, "sv_skilllevel"); }

	void Debug(const char *format, ...);
public:
	static AiBaseTeamBrain *GetBrainForTeam(int team);
};

class AiBaseBrain: public AiFrameAwareUpdatable
{
	friend class Ai;
	friend class AiGametypeBrain;
	friend class AiBaseTeamBrain;
protected:
	edict_t *self;

	NavEntity *longTermGoal;
	NavEntity *shortTermGoal;

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

	int allowedAasTravelFlags;
	int preferredAasTravelFlags;

	float entityWeights[MAX_GOALENTS];

	AiBaseBrain(edict_t *self, int allowedAasTravelFlags, int preferredAasTravelFlags);

	void ClearWeights();
	void UpdateWeights();
	virtual void UpdatePotentialGoalsWeights();

	void CheckOrCancelGoal();
	bool ShouldCancelGoal(const NavEntity *goalEnt);

	void PickLongTermGoal(const NavEntity *currLongTermGoalEnt);
	void PickShortTermGoal(const NavEntity *currLongTermGoalEnt);
	void ClearLongTermGoal();
	void ClearShortTermGoal();
	void SetShortTermGoal(NavEntity *goalEnt);
	void SetLongTermGoal(NavEntity *goalEnt);
	virtual void OnGoalCleanedUp(const NavEntity *goalEnt) {}

	// Returns a pair of AAS travel times to the target point and back
	std::pair<unsigned, unsigned> FindToAndBackTravelTimes(const Vec3 &targetPoint) const;

	inline int FindAASReachabilityToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const
	{
		return ::FindAASReachabilityToGoalArea(fromAreaNum, origin, goalAreaNum, self, allowedAasTravelFlags);
	}
	inline int FindAASTravelTimeToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const
	{
		return ::FindAASTravelTimeToGoalArea(fromAreaNum, origin, goalAreaNum, self, allowedAasTravelFlags);
	}
	inline bool IsCloseToGoal(const NavEntity *goalEnt, float proximityThreshold)
	{
		if (!goalEnt)
			return false;
		return (goalEnt->Origin() - self->s.origin).SquaredLength() <= proximityThreshold * proximityThreshold;
	}
	inline bool IsCloseEnoughToConsiderGoalReached(const NavEntity *goalEnt)
	{
		if (!goalEnt)
			return false;
		return goalEnt->IsCloseEnoughToBeConsideredReached(self);
	}

	inline int GoalAasAreaNum()
	{
		if (shortTermGoal)
			return shortTermGoal->AasAreaNum();
		if (longTermGoal)
			return longTermGoal->AasAreaNum();
		return 0;
	}

	void Debug(const char *format, ...);

	virtual void Think() override;

	// Used for additional potential goal rejection that does not reflected in entity weights.
	// Returns true if the goal entity is not feasible for some reasons.
	// Return result "false" does not means that goal is feasible though.
	// Should be overridden in subclasses to implement domain-specific behaviour.
	virtual bool MayNotBeFeasibleGoal(const NavEntity *goalEnt) { return false; };
public:
	// Returns true if the LTG does not require being touched and it is close enough to consider it reached
	inline bool IsCloseEnoughToConsiderLongTermGoalReached() { return IsCloseEnoughToConsiderGoalReached(longTermGoal); }
	// Returns true if the STG does not require being touched and it is close enough to consider it reached
	inline bool IsCloseEnoughToConsiderShortTermGoalReached() { return IsCloseEnoughToConsiderGoalReached(shortTermGoal); }

	bool ShouldWaitForLongTermGoal()
	{
		if (!longTermGoal)
			return false;
		unsigned spawnTime = longTermGoal->SpawnTime();
		if (!spawnTime)
			return false;
		return spawnTime > level.time;
	}

	inline Vec3 LongTermGoalOrigin() { return longTermGoal->Origin(); }

	inline bool IsCloseToLongTermGoal(float proximityThreshold = 128.0f)
	{
		return IsCloseToGoal(longTermGoal, proximityThreshold);
	}
	inline bool IsCloseToShortTermGoal(float proximityThreshold = 128.0f)
	{
		return IsCloseToGoal(shortTermGoal, proximityThreshold);
	}

	inline bool UnderliesLongTermGoal(const edict_t *ent) const
	{
		return longTermGoal && longTermGoal->IsBasedOnEntity(ent);
	}

	inline bool UnderliesShortTermGoal(const edict_t *ent) const
	{
		return shortTermGoal && shortTermGoal->IsBasedOnEntity(ent);
	}

	void OnLongTermGoalReached();
	void OnShortTermGoalReached();
};

typedef struct ai_handle_s
{
	ai_type	type;

	int asFactored, asRefCount;

	class Ai *aiRef;
	class Bot *botRef;
} ai_handle_t;

void AI_Debug(const char *nick, const char *format, ...);
void AI_Debugv(const char *nick, const char *format, va_list va);

//----------------------------------------------------------

//game
//----------------------------------------------------------
void	    Use_Plat( edict_t *ent, edict_t *other, edict_t *activator );

// ai_nodes.c
//----------------------------------------------------------

// ai_tools.c
//----------------------------------------------------------
void	    AITools_DrawLine( vec3_t origin, vec3_t dest );
void	    AITools_DrawColorLine( vec3_t origin, vec3_t dest, int color, int parm );

// ai_links.c
//----------------------------------------------------------

//bot_classes
//----------------------------------------------------------

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
	AiBaseBrain *aiBaseBrain;

	// Must be updated before brain thinks (set the brain currAasAreaNum to an updated value).
	int currAasAreaNum;
	// Must be updated after brain thinks (copy from a brain goal)
	int goalAasAreaNum;
	Vec3 goalTargetPoint;

	int allowedAasTravelFlags;
	int preferredAasTravelFlags;

	int currAasAreaTravelFlags;
	static constexpr unsigned MAX_REACH_CACHED = 8;
	StaticVector<aas_reachability_t, MAX_REACH_CACHED> nextReaches;

	float distanceToNextReachStart;

	inline bool IsCloseToReachStart() { return distanceToNextReachStart < 24.0f; };

	unsigned blockedTimeout;

	float aiYawSpeed, aiPitchSpeed;

	void SetFrameAffinity(unsigned modulo, unsigned offset) override
	{
		frameAffinityModulo = modulo;
		frameAffinityOffset = offset;
		aiBaseBrain->SetFrameAffinity(modulo, offset);
	}

	void ClearAllGoals();

	// Called by brain via self->ai->aiRef when long-term or short-term goals are set
	void OnGoalSet(NavEntity *goalEnt);

	void UpdateReachCache(int reachedAreaNum);

	virtual void Frame() override;
	virtual void Think() override;
public:
	Ai(edict_t *self, int preferredAasTravelFlags = TFL_DEFAULT, int allowedAasTravelFlags = TFL_DEFAULT);

	inline bool IsGhosting() const { return G_ISGHOSTING(self); }

	void ChangeAngle(const Vec3 &idealDirection, float angularSpeedMultiplier = 1.0f);
	static bool IsStep(edict_t *ent);
	int FindCurrAASAreaNum();
	// Accepts a touched entity and its old solid before touch
	void TouchedEntity(edict_t *ent, int oldSolid);

	static NavEntity *GetGoalentForEnt(edict_t *target);

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
		return ::FindAASReachabilityToGoalArea(fromAreaNum, origin, goalAreaNum, self, allowedAasTravelFlags);
	}
	inline int FindAASTravelTimeToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const
	{
		return ::FindAASTravelTimeToGoalArea(fromAreaNum, origin, goalAreaNum, self, allowedAasTravelFlags);
	}
	inline float FindSquareDistanceToGround(const vec3_t origin, float traceDepth = 999999.0f) const
	{
		return ::FindSquareDistanceToGround(origin, self, traceDepth);
	}
	inline float FindDistanceToGround(const vec3_t origin, float traceDepth = 999999.0f) const
	{
		return ::FindDistanceToGround(origin, self, traceDepth);
	}

	virtual void TouchedGoal(const edict_t *goalUnderlyingEntity, int goalOldSolid) {};
	virtual void TouchedJumppad(const edict_t *jumppad) {};

	void CheckReachedArea();
	void ChangeAxisAngle(float currAngle, float idealAngle, float edictAngleSpeed, float *aiAngleSpeed, float *changedAngle);

	void TestClosePlace();
	ClosePlaceProps closeAreaProps;
private:
	void TestMove(MoveTestResult *moveTestResult, int currAasAreaNum, const vec3_t forward) const;
};

inline float BoundedFraction(float value, float bound)
{
	return std::min(value, bound) / bound;
}

#endif