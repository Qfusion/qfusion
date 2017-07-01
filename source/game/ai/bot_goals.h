#ifndef QFUSION_BOT_GOALS_H
#define QFUSION_BOT_GOALS_H

#include "ai_base_brain.h"

class BotBaseGoal : public AiBaseGoal
{
	StaticVector<AiBaseAction *, AiBaseBrain::MAX_ACTIONS> extraApplicableActions;

public:
	BotBaseGoal( Ai *ai_, const char *name_, unsigned updatePeriod_ )
		: AiBaseGoal( ai_, name_, updatePeriod_ ) {}

	inline void AddExtraApplicableAction( AiBaseAction *action ) {
		extraApplicableActions.push_back( action );
	}

protected:
	inline PlannerNode *ApplyExtraActions( PlannerNode *firstTransition, const WorldState &worldState );

	inline const class SelectedNavEntity &SelectedNavEntity() const;
	inline const class SelectedEnemies &SelectedEnemies() const;
	inline const class BotWeightConfig &WeightConfig() const;
};

class BotGrabItemGoal : public BotBaseGoal
{
public:
	BotGrabItemGoal( Ai *ai_ ) : BotBaseGoal( ai_, "BotGrabItemGoal", 500 ) {}

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;
};

class BotKillEnemyGoal : public BotBaseGoal
{
public:
	BotKillEnemyGoal( Ai *ai_ ) : BotBaseGoal( ai_, "BotKillEnemyGoal", 400 ) {}

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;
};

class BotRunAwayGoal : public BotBaseGoal
{
public:
	BotRunAwayGoal( Ai *ai_ ) : BotBaseGoal( ai_, "BotRunAwayGoal", 550 ) {}

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;
};

class BotAttackOutOfDespairGoal : public BotBaseGoal
{
	float oldOffensiveness;

public:
	BotAttackOutOfDespairGoal( Ai *ai_ ) : BotBaseGoal( ai_, "BotAttackOutOfDespairGoal", 400 ) {}

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;

	void OnPlanBuildingStarted() override;
	void OnPlanBuildingCompleted( const AiBaseActionRecord *planHead ) override;
};

class BotReactToDangerGoal : public BotBaseGoal
{
public:
	BotReactToDangerGoal( Ai *ai_ ) : BotBaseGoal( ai_, "BotReactToDangerGoal", 350 ) {}

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;
};

class BotReactToThreatGoal : public BotBaseGoal
{
public:
	BotReactToThreatGoal( Ai *ai_ ) : BotBaseGoal( ai_, "BotReactToThreatGoal", 350 ) {}

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;
};

class BotReactToEnemyLostGoal : public BotBaseGoal
{
public:
	BotReactToEnemyLostGoal( Ai *ai_ ) : BotBaseGoal( ai_, "BotReactToEnemyLostGoal", 400 ) {}

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;
};

class BotRoamGoal : public BotBaseGoal
{
public:
	BotRoamGoal( Ai *ai_ ) : BotBaseGoal( ai_, "BotRoamGoal", 400 ) {}

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;
};

class BotScriptGoal : public BotBaseGoal
{
	void *scriptObject;

public:
	BotScriptGoal( Ai *ai_, const char *name_, unsigned updatePeriod_, void *scriptObject_ )
		: BotBaseGoal( ai_, name_, updatePeriod_ ),
		scriptObject( scriptObject_ ) {}

	// Exposed for script API
	inline edict_t *Self() { return self; }

	void UpdateWeight( const WorldState &currWorldState ) override;
	void GetDesiredWorldState( WorldState *worldState ) override;
	PlannerNode *GetWorldStateTransitions( const WorldState &worldState ) override;

	void OnPlanBuildingStarted() override;
	void OnPlanBuildingCompleted( const AiBaseActionRecord *planHead ) override;
};

#endif
