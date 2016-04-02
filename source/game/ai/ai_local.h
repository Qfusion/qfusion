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

#define AI_VERSION_STRING "A0059"

//bot debug_chase options
extern cvar_t *bot_showpath;
extern cvar_t *bot_showcombat;
extern cvar_t *bot_showsrgoal;
extern cvar_t *bot_showlrgoal;
extern cvar_t *bot_dummy;
extern cvar_t *sv_botpersonality;

//----------------------------------------------------------

#define AI_STATUS_TIMEOUT	150
#define AI_LONG_RANGE_GOAL_DELAY 2000
#define AI_SHORT_RANGE_GOAL_DELAY 250
#define AI_SHORT_RANGE_GOAL_DELAY_IDLE 25

#define AI_DEFAULT_YAW_SPEED	( self->ai->pers.cha.default_yaw_speed )
#define AI_REACTION_TIME	( self->ai->pers.cha.reaction_time )
#define AI_COMBATMOVE_TIMEOUT	( self->ai->pers.cha.combatmove_timeout )
#define AI_YAW_ACCEL		( self->ai->pers.cha.yaw_accel * FRAMETIME )
#define AI_CHAR_OFFENSIVNESS ( self->ai->pers.cha.offensiveness )
#define AI_CHAR_CAMPINESS ( self->ai->pers.cha.campiness )

// Platform states:
#define	STATE_TOP	    0
#define	STATE_BOTTOM	    1
#define STATE_UP	    2
#define STATE_DOWN	    3

#define BOT_MOVE_LEFT		0
#define BOT_MOVE_RIGHT		1
#define BOT_MOVE_FORWARD	2
#define BOT_MOVE_BACK		3

//=============================================================
//	NAVIGATION DATA
//=============================================================

#define MAX_GOALENTS 1024
#define MAX_NODES 2048        //jalToDo: needs dynamic alloc (big terrain maps)
#define NODE_INVALID  -1
#define NODE_DENSITY 128         // Density setting for nodes
#define NODE_TIMEOUT 1500 // (milli)seconds to reach the next node
#define NODE_REACH_RADIUS 36
#define NODE_WIDE_REACH_RADIUS 92
#define	NODES_MAX_PLINKS 16
#define	NAV_FILE_VERSION 10
#define NAV_FILE_EXTENSION "nav"
#define NAV_FILE_FOLDER "navigation"

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

#define MASK_NODESOLID      ( CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP )
#define MASK_AISOLID        ( CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY|CONTENTS_MONSTERCLIP )

// node flags
#define	NODEFLAGS_WATER 0x00000001
#define	NODEFLAGS_LADDER 0x00000002
#define NODEFLAGS_SERVERLINK 0x00000004  // plats, doors, teles. Only server can link 2 nodes with this flag
#define	NODEFLAGS_FLOAT 0x00000008  // don't drop node to floor ( air & water )
#define	NODEFLAGS_DONOTENTER 0x00000010
#define	NODEFLAGS_BOTROAM 0x00000020
#define NODEFLAGS_JUMPPAD 0x00000040
#define NODEFLAGS_JUMPPAD_LAND 0x00000080
#define	NODEFLAGS_PLATFORM 0x00000100
#define	NODEFLAGS_TELEPORTER_IN 0x00000200
#define NODEFLAGS_TELEPORTER_OUT 0x00000400
#define NODEFLAGS_REACHATTOUCH 0x00000800
#define NODEFLAGS_ENTITYREACH 0x00001000 // never reachs on it's own, the entity has to declare itself reached

#define NODE_ALL 0xFFFFFFFF

#define NODE_MASK_SERVERFLAGS ( NODEFLAGS_SERVERLINK|NODEFLAGS_BOTROAM|NODEFLAGS_JUMPPAD|NODEFLAGS_JUMPPAD_LAND|NODEFLAGS_PLATFORM|NODEFLAGS_TELEPORTER_IN|NODEFLAGS_TELEPORTER_OUT|NODEFLAGS_REACHATTOUCH|NODEFLAGS_ENTITYREACH )
#define NODE_MASK_NOREUSE ( NODEFLAGS_LADDER|NODEFLAGS_JUMPPAD|NODEFLAGS_JUMPPAD_LAND|NODEFLAGS_PLATFORM|NODEFLAGS_TELEPORTER_IN|NODEFLAGS_TELEPORTER_OUT|NODEFLAGS_ENTITYREACH )

// links types (movetypes required to run node links)
#define	LINK_MOVE 0x00000001
#define	LINK_STAIRS 0x00000002
#define LINK_FALL 0x00000004
#define	LINK_CLIMB 0x00000008
#define	LINK_TELEPORT 0x00000010
#define	LINK_PLATFORM 0x00000020
#define LINK_JUMPPAD 0x00000040
#define LINK_WATER 0x00000080
#define	LINK_WATERJUMP 0x00000100
#define	LINK_LADDER	0x00000200
#define LINK_JUMP 0x00000400
#define LINK_CROUCH 0x00000800
#define LINK_ROCKETJUMP 0x00002000
#define LINK_DOOR 0x00004000

#define LINK_INVALID 0x00001000

class NavEntity
{
	friend class GoalEntitiesRegistry;
	int id;
	int aasAreaNum;
	int nodeFlags;
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
	inline bool IsBasedOnEntity(const edict_t *ent) const { return ent && this->ent == ent; }
	inline bool IsClient() const { return ent->r.client != nullptr; }
	inline bool IsSpawnedAtm() const { return ent->r.solid != SOLID_NOT; }
	inline bool ToBeSpawnedLater() const { return ent->r.solid == SOLID_NOT; }

	bool MayBeReachedNow(const edict_t *grabber);
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

	NavEntity *AddGoalEntity(edict_t *ent, int aasAreaNum, int nodeFlags = 0);
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

typedef struct
{
	unsigned int moveTypesMask; // moves the bot can perform at this moment
	float entityWeights[MAX_GOALENTS];
} ai_status_t;

typedef struct
{
	const char *name;
	float default_yaw_speed;
	float reaction_time;		
	float combatmove_timeout;
	float yaw_accel;
	float offensiveness;
	float campiness;
	float firerate;
	float armor_grabber;
	float health_grabber;

	float weapon_affinity[WEAP_TOTAL];
} ai_character;

typedef struct
{
	const char *netname;
	float skillLevel;       // Affects AIM and fire rate (fraction of 1)
	unsigned int moveTypesMask;      // bot can perform these moves, to check against required moves for a given path
	float inventoryWeights[MAX_ITEMS];

	//class based functions
	void ( *UpdateStatus )( edict_t *ent );
	void ( *RunFrame )( edict_t *ent );
	void ( *blockedTimeout )( edict_t *ent );

	ai_character cha;
} ai_pers_t;

typedef struct ai_handle_s
{
	ai_pers_t pers;         // persistant definition (class?)
	ai_status_t status;     //player (bot, NPC) status for AI frame

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
void	    AITools_DrawPath( edict_t *self, int node_to );
void	    AITools_DrawLine( vec3_t origin, vec3_t dest );
void	    AITools_DrawColorLine( vec3_t origin, vec3_t dest, int color, int parm );
void	    AITools_InitEditnodes( void );
void	    AITools_InitMakenodes( void );

// ai_links.c
//----------------------------------------------------------

//bot_classes
//----------------------------------------------------------
void	    BOT_DMclass_InitPersistant( edict_t *self );

struct MoveTestResult
{
	trace_t forwardGroundTrace;
	trace_t forwardPitTrace;
	trace_t wallFullHeightTrace;
	trace_t wallStepHeightTrace;
	trace_t wallZeroHeightTrace;

	const edict_t *self;

	bool CanWalk() const;
	bool CanWalkOrFallQuiteSafely() const;
	bool CanJump() const;
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

class Ai: public EdictRef
{
protected:
	NavEntity *longTermGoal;
	NavEntity *shortTermGoal;

	int currAasAreaNum;
	int goalAasAreaNum;
	Vec3 goalTargetPoint;

	int allowedAasTravelFlags;
	int preferredAasTravelFlags;

	int currAasAreaTravelFlags;
	static constexpr unsigned MAX_REACH_CACHED = 8;
	StaticVector<aas_reachability_t, MAX_REACH_CACHED> nextReaches;

	float distanceToNextReachStart;
	float distanceToNextReachEnd;

	inline bool IsCloseToReachStart() { return distanceToNextReachStart < 24.0f; };
	inline bool IsCloseToReachEnd() { return distanceToNextReachEnd < 36.0f; }

	unsigned statusUpdateTimeout;
	unsigned blockedTimeout;

	unsigned stateCombatTimeout;
	unsigned longTermGoalTimeout;
	unsigned shortTermGoalTimeout;

	float aiYawSpeed, aiPitchSpeed;

	void UpdateReachCache(int reachedAreaNum);
public:
	Ai(edict_t *self);

	inline bool IsGhosting() const { return G_ISGHOSTING(self); }

	void Think();

	bool IsShortRangeReachable(const Vec3 &targetOrigin) const;

	bool IsVisible(edict_t *other) const;
	bool IsInFront(edict_t *other) const;
	bool IsInFront2D(vec3_t lookDir, vec3_t origin, vec3_t point, float accuracy) const;

	void ChangeAngle(const Vec3 &idealDirection, float angularSpeedMultiplier = 1.0f);
	static bool IsStep(edict_t *ent);

	int FindCurrAASAreaNum();

	inline bool HasLongTermGoal() { return longTermGoal != nullptr; }
	inline bool HasShortTermGoal() { return shortTermGoal != nullptr; }
	void ClearLongTermGoal();
	void ClearShortTermGoal();
	void SetLongTermGoal(NavEntity *goalEnt);
	void SetShortTermGoal(NavEntity *goalEnt);
	void OnLongTermGoalReached();
	void OnShortTermGoalReached();
	void TouchedEntity(edict_t *ent);

	static NavEntity *GetGoalentForEnt(edict_t *target);
	void PickLongTermGoal();
	void PickShortTermGoal();
	// Looks like it is unused since is not implemented in original code
	void Frame(usercmd_t *ucmd);
	void ResetNavigation();
	void CategorizePosition();
	void UpdateStatus();

	static constexpr unsigned BLOCKED_TIMEOUT = 15000;
protected:
	void Debug(const char *format, ...) const;
	void FailWith(const char *format, ...) const;

	const char *Nick() const
	{
		return self->r.client ? self->r.client->netname : self->classname;
	}

	int FindAASReachabilityToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const;
	int FindAASTravelTimeToGoalArea(int fromAreaNum, const vec3_t origin, int goalAreaNum) const;
	float FindSquareDistanceToGround(const vec3_t origin, float traceDepth = 999999.0f) const;
	inline float FindDistanceToGround(const vec3_t origin, float traceDepth = 999999.0f) const;

	void CheckReachedArea();

	void ChangeAxisAngle(float currAngle, float idealAngle, float edictAngleSpeed, float *aiAngleSpeed, float *changedAngle);

	void TestClosePlace();
	ClosePlaceProps closeAreaProps;
private:
	template <typename AASFn>
	int FindAASParamToGoalArea(AASFn fn, int fromAreaNum, const vec3_t origin, int goalAreaNum) const;
	void CancelOtherAisGoals(NavEntity *canceledGoal);
	void TestMove(MoveTestResult *moveTestResult, int direction) const;
};

inline float BoundedFraction(float value, float bound)
{
	return std::min(value, bound) / bound;
}

#endif