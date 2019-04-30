#ifndef QFUSION_BOT_BRAIN_H
#define QFUSION_BOT_BRAIN_H

#include <stdarg.h>
#include "ai_base_ai.h"
#include "ai_base_brain.h"
#include "ai_base_enemy_pool.h"
#include "bot_items_selector.h"
#include "bot_weapon_selector.h"
#include "bot_actions.h"
#include "bot_goals.h"

// This can be represented as an enum but feels better in the following form.
// Many values that affect bot behaviour already are not boolean
// (such as nav targets and special movement states like camping spots),
// and thus controlling a bot by a single flags field already is not possible.
// This struct is likely to be extended by non-boolean values later.
struct SelectedMiscTactics {
	bool willAdvance;
	bool willRetreat;

	bool shouldBeSilent;
	bool shouldMoveCarefully;

	bool shouldAttack;
	bool shouldKeepXhairOnEnemy;

	bool willAttackMelee;
	bool shouldRushHeadless;

	inline SelectedMiscTactics() { Clear(); };

	inline void Clear() {
		willAdvance = false;
		willRetreat = false;

		shouldBeSilent = false;
		shouldMoveCarefully = false;

		shouldAttack = false;
		shouldKeepXhairOnEnemy = false;

		willAttackMelee = false;
		shouldRushHeadless = false;
	}

	inline void PreferAttackRatherThanRun() {
		shouldAttack = true;
		shouldKeepXhairOnEnemy = true;
	}

	inline void PreferRunRatherThanAttack() {
		shouldAttack = true;
		shouldKeepXhairOnEnemy = false;
	}
};

class BotBrain : public AiBaseBrain
{
	friend class Bot;
	friend class BotItemsSelector;
	friend class BotBaseGoal;
	friend class BotGutsActionsAccessor;

	edict_t *bot;

	float baseOffensiveness;
	const float skillLevel;
	const unsigned reactionTime;

	int64_t nextTargetChoiceAt;
	const unsigned targetChoicePeriod;

	BotItemsSelector itemsSelector;

	StaticVector<BotScriptGoal, MAX_GOALS> scriptGoals;
	StaticVector<BotScriptAction, MAX_ACTIONS> scriptActions;

	BotBaseGoal *GetGoalByName( const char *name );
	BotBaseAction *GetActionByName( const char *name );

	inline BotScriptGoal *AllocScriptGoal() { return scriptGoals.unsafe_grow_back(); }
	inline BotScriptAction *AllocScriptAction() { return scriptActions.unsafe_grow_back(); }

	inline bool BotHasQuad() const { return ::HasQuad( bot ); }
	inline bool BotHasShell() const { return ::HasShell( bot ); }
	inline bool BotHasPowerups() const { return ::HasPowerups( bot ); }
	inline bool BotIsCarrier() const { return ::IsCarrier( bot ); }
	float BotSkill() const { return skillLevel; }

	inline const int *Inventory() const { return bot->r.client->ps.inventory; }

	template <int Weapon>
	inline int AmmoReadyToFireCount() const {
		if( !Inventory()[Weapon] ) {
			return 0;
		}
		return Inventory()[WeaponAmmo < Weapon > ::strongAmmoTag] + Inventory()[WeaponAmmo < Weapon > ::weakAmmoTag];
	}

	inline int ShellsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_RIOTGUN>(); }
	inline int GrenadesReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_GRENADELAUNCHER>(); }
	inline int RocketsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_ROCKETLAUNCHER>(); }
	inline int PlasmasReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_PLASMAGUN>(); }
	inline int BulletsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_MACHINEGUN>(); }
	inline int LasersReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_LASERGUN>(); }
	inline int BoltsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_ELECTROBOLT>(); }
	inline int InstasReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_INSTAGUN>(); }

	SelectedMiscTactics selectedTactics;

	inline bool WillAdvance() const { return selectedTactics.willAdvance; }
	inline bool WillRetreat() const { return selectedTactics.willRetreat; }

	inline bool ShouldBeSilent() const { return selectedTactics.shouldBeSilent; }
	inline bool ShouldMoveCarefully() const { return selectedTactics.shouldMoveCarefully; }

	inline bool ShouldAttack() const { return selectedTactics.shouldAttack; }
	inline bool ShouldKeepXhairOnEnemy() const { return selectedTactics.shouldKeepXhairOnEnemy; }

	inline bool WillAttackMelee() const { return selectedTactics.willAttackMelee; }
	inline bool ShouldRushHeadless() const { return selectedTactics.shouldRushHeadless; }

	SelectedNavEntity selectedNavEntity;
	// For tracking picked up items
	const NavEntity *prevSelectedNavEntity;

	const SelectedNavEntity &GetOrUpdateSelectedNavEntity();
	void ForceSetNavEntity( const SelectedNavEntity &selectedNavEntity_ );

	inline bool HasJustPickedGoalItem() const {
		if( lastNavTargetReachedAt < prevThinkAt ) {
			return false;
		}
		if( !lastReachedNavTarget ) {
			return false;
		}
		if( !lastReachedNavTarget->IsBasedOnNavEntity( prevSelectedNavEntity ) ) {
			return false;
		}
		return true;
	}

	float ComputeEnemyAreasBlockingFactor( const Enemy *enemy, float damageToKillBot, int botBestWeaponTier );

	void UpdateBlockedAreasStatus();

	bool FindDodgeDangerSpot( const Danger &danger, vec3_t spotOrigin );

	void CheckNewActiveDanger();

	Danger triggeredPlanningDanger;
	Danger actualDanger;

	struct Threat {
		const edict_t *inflictor;
		Vec3 possibleOrigin;
		float totalDamage;
		unsigned lastHitTimestamp;

		// Initialize the inflictor by the world entity (it is never valid as one).
		// This helps to avoid extra branching from testing for nullity.
		Threat() : inflictor( world ), possibleOrigin( NAN, NAN, NAN ), totalDamage( 0.0f ), lastHitTimestamp( 0 ) {}

		bool IsValidFor( const edict_t *self ) const;
	};

	Threat activeThreat;

	void PrepareCurrWorldState( WorldState *worldState ) override;

	bool ShouldSkipPlanning() const override;

	BotBrain() = delete;
	// Disable copying and moving
	BotBrain( BotBrain &&that ) = delete;

	class EnemyPool : public AiBaseEnemyPool
	{
		friend class BotBrain;
		edict_t *bot;
		BotBrain *botBrain;

protected:
		void OnNewThreat( const edict_t *newThreat ) override;
		bool CheckHasQuad() const override { return ::HasQuad( bot ); }
		bool CheckHasShell() const override { return ::HasShell( bot ); }
		float ComputeDamageToBeKilled() const override { return DamageToKill( bot ); }
		void OnEnemyRemoved( const Enemy *enemy ) override;
		void SetBotRoleWeight( const edict_t *bot_, float weight ) override {}
		float GetAdditionalEnemyWeight( const edict_t *bot_, const edict_t *enemy ) const override { return 0; }
		void OnBotEnemyAssigned( const edict_t *bot_, const Enemy *enemy ) override {}

public:
		EnemyPool( edict_t *bot_, BotBrain *botBrain_, float skill_ )
			: AiBaseEnemyPool( skill_ ), bot( bot_ ), botBrain( botBrain_ ) {
			SetTag( va( "BotBrain(%s)::EnemyPool", bot->r.client->netname ) );
		}
		virtual ~EnemyPool() override {}
	};

	class AiSquad *squad;
	EnemyPool botEnemyPool;
	AiBaseEnemyPool *activeEnemyPool;

protected:
	virtual void SetFrameAffinity( unsigned modulo, unsigned offset ) override {
		// Call super method first
		AiBaseBrain::SetFrameAffinity( modulo, offset );
		// Allow bot's own enemy pool to think
		botEnemyPool.SetFrameAffinity( modulo, offset );
	}
	virtual void OnAttitudeChanged( const edict_t *ent, int oldAttitude_, int newAttitude_ ) override;

public:
	SelectedEnemies &selectedEnemies;
	SelectedEnemies lostEnemies;
	SelectedWeapons &selectedWeapons;

	// A WorldState cached from the moment of last world state update
	WorldState cachedWorldState;

	// Note: saving references to Bot members is the only valid access kind to Bot in this call
	BotBrain( class Bot *bot, float skillLevel_ );

	virtual void Frame() override;
	virtual void Think() override;

	void OnAttachedToSquad( AiSquad *squad_ );
	void OnDetachedFromSquad( AiSquad *squad_ );

	void OnNewThreat( const edict_t *newThreat, const AiFrameAwareUpdatable *threatDetector );
	void OnEnemyRemoved( const Enemy *enemy );

	void OnEnemyViewed( const edict_t *enemy );
	void OnEnemyOriginGuessed( const edict_t *enemy, unsigned minMillisSinceLastSeen, const float *guessedOrigin = nullptr );

	void AfterAllEnemiesViewed() {}
	void UpdateSelectedEnemies();

	void OnPain( const edict_t *enemy, float kick, int damage );
	void OnEnemyDamaged( const edict_t *target, int damage );

	// In these calls use not active but bot's own enemy pool
	// (this behaviour is expected by callers, otherwise referring to a squad enemy pool is enough)
	inline int64_t LastAttackedByTime( const edict_t *attacker ) const {
		return botEnemyPool.LastAttackedByTime( attacker );
	}
	inline int64_t LastTargetTime( const edict_t *target ) const {
		return botEnemyPool.LastTargetTime( target );
	}

	inline bool IsPrimaryAimEnemy( const edict_t *enemy ) const {
		return selectedEnemies.IsPrimaryEnemy( enemy );
	}

	inline float GetBaseOffensiveness() const { return baseOffensiveness; }

	float GetEffectiveOffensiveness() const;

	inline void SetBaseOffensiveness( float baseOffensiveness_ ) {
		this->baseOffensiveness = Clamp( baseOffensiveness_, 0.0f, 1.0f );
	}

	inline void ClearOverriddenEntityWeights() {
		itemsSelector.ClearOverriddenEntityWeights();
	}

	inline void OverrideEntityWeight( const edict_t *ent, float weight ) {
		itemsSelector.OverrideEntityWeight( ent, weight );
	}
};

#endif
