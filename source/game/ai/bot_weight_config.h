#ifndef QFUSION_BOT_WEIGHT_CONFIG_H
#define QFUSION_BOT_WEIGHT_CONFIG_H

#include "ai_weight_config.h"

// Var names might seem to be way too shortened, but its silly to replace each minor numeric constant by a 30 chars name

class BotWeightConfig : public AiWeightConfig
{
public:
	class NativeGoalsGroup : public AiNativeWeightConfigVarGroup
	{
public:
		struct GrabItemGroup : public AiNativeWeightConfigVarGroup {
			AiNativeWeightConfigVar baseWeight;
			AiNativeWeightConfigVar selectedGoalWeightScale;

			GrabItemGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "GrabItem" ),
				baseWeight( this, "BaseWeight", 0.00f, 0.50f, 0.00f ),
				selectedGoalWeightScale( this, "SelectedGoalWeightScale", 0.75f, 1.25f, 1.00f ) {}
		} grabItem;

		struct KillEnemyGroup : public AiNativeWeightConfigVarGroup {
			AiNativeWeightConfigVar baseWeight;
			AiNativeWeightConfigVar offCoeff;
			AiNativeWeightConfigVar nmyThreatCoeff;

			KillEnemyGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "KillEnemy" ),
				baseWeight( this, "BaseWeight", 0.00f, 0.50f, 0.00f ),
				offCoeff( this, "OffCoeff", 0.50f, 3.00f, 1.75f ),
				nmyThreatCoeff( this, "NmyThreatCoeff", 1.10f, 2.00f, 1.25f ) {}
		} killEnemy;

		struct RunAwayGroup : public AiNativeWeightConfigVarGroup {
			AiNativeWeightConfigVar baseWeight;
			AiNativeWeightConfigVar offCoeff;
			AiNativeWeightConfigVar nmyThreatCoeff;

			RunAwayGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "RunAway" ),
				baseWeight( this, "BaseWeight", 0.00f, 0.50f, 0.00f ),
				offCoeff( this, "OffCoeff", 1.00f, 3.00f, 1.75f ),
				nmyThreatCoeff( this, "NmyThreatCoeff", 1.10f, 3.00f, 1.50f ) {}
		} runAway;

		struct ReactToDangerGroup : public AiNativeWeightConfigVarGroup {
			AiNativeWeightConfigVar baseWeight;
			AiNativeWeightConfigVar dmgFracCoeff;
			AiNativeWeightConfigVar weightBound;

			ReactToDangerGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "ReactToDanger" ),
				baseWeight( this, "BaseWeight", 0.50f, 1.00f, 0.75f ),
				dmgFracCoeff( this, "DmgFracCoeff", 0.50f, 5.00f, 2.00f ),
				weightBound( this, "WeightBound", 1.00f, 5.00f, 2.00f ) {}
		} reactToDanger;

		struct ReactToThreatGroup : public AiNativeWeightConfigVarGroup {
			AiNativeWeightConfigVar baseWeight;
			AiNativeWeightConfigVar dmgFracCoeff;
			AiNativeWeightConfigVar weightBound;
			AiNativeWeightConfigVar offCoeff;

			ReactToThreatGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "ReactToThreat" ),
				baseWeight( this, "BaseWeight", 0.50f, 1.00f, 0.50f ),
				dmgFracCoeff( this, "DmgFracCoeff", 1.00f, 5.00f, 3.00f ),
				weightBound( this, "WeightBound", 1.00f, 3.00f, 1.75f ),
				offCoeff( this, "OffCoeff", 0.00f, 2.00f, 1.00f ) {}
		} reactToThreat;

		struct ReactToEnemyLostGroup : public AiNativeWeightConfigVarGroup {
			AiNativeWeightConfigVar baseWeight;
			AiNativeWeightConfigVar offCoeff;

			ReactToEnemyLostGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "ReactToEnemyLost" ),
				baseWeight( this, "BaseWeight", 0.50f, 1.00f, 0.50f ),
				offCoeff( this, "OffCoeff", 0.50f, 6.00f, 2.00f ) {}
		} reactToEnemyLost;

		struct AttackOutOfDespairGroup : public AiNativeWeightConfigVarGroup {
			AiNativeWeightConfigVar nmyFireDelayThreshold;
			AiNativeWeightConfigVar baseWeight;
			AiNativeWeightConfigVar nmyThreatExtraWeight;
			AiNativeWeightConfigVar dmgFracCoeff;
			AiNativeWeightConfigVar dmgUpperBound;

			AttackOutOfDespairGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "AttackOutOfDespair" ),
				nmyFireDelayThreshold( this, "NmyFireDelayThreshold", 200, 800, 600 ),
				baseWeight( this, "BaseWeight", 0.25f, 1.00f, 0.50f ),
				nmyThreatExtraWeight( this, "NmyThreatExtraWeight", 0.10f, 1.00f, 0.50f ),
				dmgFracCoeff( this, "DmgFracCoeff", 0.50f, 1.75f, 1.25f ),
				dmgUpperBound( this, "DmgUpperBound", 50.0f, 200.0f, 100.0f ) {}
		} attackOutOfDespair;

		NativeGoalsGroup( AiBaseWeightConfigVarGroup *parent )
			: AiNativeWeightConfigVarGroup( parent, "NativeGoals" ),
			grabItem( this ),
			killEnemy( this ),
			runAway( this ),
			reactToDanger( this ),
			reactToThreat( this ),
			reactToEnemyLost( this ),
			attackOutOfDespair( this ) {}

	} nativeGoals;

	class NativeActionsGroup : public AiNativeWeightConfigVarGroup
	{
public:
		struct AdvanceToGoodPositionGroup : public AiNativeWeightConfigVarGroup {
			struct SniperRangeGroup : public AiNativeWeightConfigVarGroup {
				struct HasThreateningEnemyGroup : public AiNativeWeightConfigVarGroup {
					AiNativeWeightConfigVar baseDmgToBeKilled;
					AiNativeWeightConfigVar baseDmgRatio;
					AiNativeWeightConfigVar baseOffCoeff;
					AiNativeWeightConfigVar goodNmyWeapMinusDmgRatio;
					AiNativeWeightConfigVar goodNmyWeapMinusOffCoeff;

					HasThreateningEnemyGroup( AiBaseWeightConfigVarGroup *parent )
						: AiNativeWeightConfigVarGroup( parent, "HasThreateningEnemy" ),
						baseDmgToBeKilled( this, "BaseDmgToBeKilled", 25, 100, 75 ),
						baseDmgRatio( this, "BaseDmgRatio", 0.70f, 1.00f, 0.80f ),
						baseOffCoeff( this, "BaseOffCoeff", 0.50f, 0.90f, 0.70f ),
						goodNmyWeapMinusDmgRatio( parent, "GoodNmyWeapMinusDmgRatio", 0.20f, 0.50f, 0.30f ),
						goodNmyWeapMinusOffCoeff( this, "GoodNmyWeapMinusOffCoeff", 0.00f, 0.50f, 0.20f ) {}
				} hasThreateningEnemy;

				struct NoThreateningEnemyGroup : public AiNativeWeightConfigVarGroup {
					AiNativeWeightConfigVar goodNmyWeapDmgRatio;
					AiNativeWeightConfigVar goodNmyWeapOffCoeff;
					AiNativeWeightConfigVar baseDmgToBeKilled;

					NoThreateningEnemyGroup( AiBaseWeightConfigVarGroup *parent )
						: AiNativeWeightConfigVarGroup( parent, "NoThreateningEnemy" ),
						goodNmyWeapDmgRatio( this, "GoodNmyWeapDmgRatio", 0.80f, 1.50f, 1.00f ),
						goodNmyWeapOffCoeff( this, "GoodNmyWeapOffCoeff", 0.00f, 0.60f, 0.40f ),
						baseDmgToBeKilled( this, "BaseDmgToBeKilled", 20, 60, 40 ) {}
				} noThreateningEnemy;

				SniperRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "SniperRange" ),
					hasThreateningEnemy( this ),
					noThreateningEnemy( this ) {}

			} sniperRange;

			struct FarRangeGroup : public AiNativeWeightConfigVarGroup {
				struct HasThreateningEnemy : public AiNativeWeightConfigVarGroup {
					AiNativeWeightConfigVar baseDmgToBeKilled;
					AiNativeWeightConfigVar goodNmyWeapBaseDmgRatio;
					AiNativeWeightConfigVar goodNmyWeapOffCoeff;

					HasThreateningEnemy( AiBaseWeightConfigVarGroup *parent )
						: AiNativeWeightConfigVarGroup( parent, "HasThreateningEnemy" ),
						baseDmgToBeKilled( this, "BaseDmgToBeKilled", 60, 180, 120 ),
						goodNmyWeapBaseDmgRatio( this, "GoodNmyWeapBaseDmgRatio", 0.90f, 1.40f, 1.00f ),
						goodNmyWeapOffCoeff( this, "GoodNmyWeapOffCoeff", 0.20f, 0.60f, 0.30f ) {}
				} hasThreateningEnemy;

				struct NoThreateningEnemy : public AiNativeWeightConfigVarGroup {
					AiNativeWeightConfigVar baseDmgToBeKilled;

					NoThreateningEnemy( AiBaseWeightConfigVarGroup *parent )
						: AiNativeWeightConfigVarGroup( parent, "NoThreateningEnemy" ),
						baseDmgToBeKilled( this, "BaseDmgToBeKilled", 50, 150, 100 ) {}
				} noThreateningEnemy;

				FarRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "FarRange" ),
					hasThreateningEnemy( this ),
					noThreateningEnemy( this ) {}
			} farRange;

			struct MiddleRangeGroup : public AiNativeWeightConfigVarGroup {
				struct HasThreateningEnemyGroup : public AiNativeWeightConfigVarGroup {
					AiNativeWeightConfigVar baseDmgToBeKilled;
					AiNativeWeightConfigVar offCoeff;
					AiNativeWeightConfigVar baseDmgRatio;
					AiNativeWeightConfigVar goodNmyWeapMinusDmgRatio;

					HasThreateningEnemyGroup( AiBaseWeightConfigVarGroup *parent )
						: AiNativeWeightConfigVarGroup( parent, "HasThreateningEnemy" ),
						baseDmgToBeKilled( this, "BaseDmgToBeKilled", 80, 200, 160 ),
						offCoeff( this, "OffCoeff", 0.20f, 0.80f, 0.50f ),
						baseDmgRatio( this, "BaseDmgRatio", 1.00f, 1.30f, 1.20f ),
						goodNmyWeapMinusDmgRatio( this, "GoodNmyWeapMinusDmgRatio", 0.2f, 0.6f, 0.4f ) {}
				} hasThreateningEnemy;

				struct NoThreateningEnemy : public AiNativeWeightConfigVarGroup {
					AiNativeWeightConfigVar offCoeff;
					AiNativeWeightConfigVar baseDmgRatio;
					AiNativeWeightConfigVar goodNmyWeapMinusDmgRatio;

					NoThreateningEnemy( AiBaseWeightConfigVarGroup *parent )
						: AiNativeWeightConfigVarGroup( parent, "NoThreateningEnemy" ),
						offCoeff( this, "OffCoeff", 0.20f, 0.80f, 0.50f ),
						baseDmgRatio( this, "BaseDmgRatio", 0.70f, 1.20f, 0.90f ),
						goodNmyWeapMinusDmgRatio( this, "GoodNmyWeapMinusDmgRatio", 0.20f, 0.50f, 0.50f ) {}
				} noThreateningEnemy;

				MiddleRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "MiddleRange" ),
					hasThreateningEnemy( this ),
					noThreateningEnemy( this ) {}
			} middleRange;

			AdvanceToGoodPositionGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "AdvanceToTacticalSpot" ),
				sniperRange( this ),
				farRange( this ),
				middleRange( this ) {}
		} advanceToGoodPosition;

		struct RetreatToGoodPositionGroup : public AiNativeWeightConfigVarGroup {
			struct FarRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar baseDmgRatio;
				AiNativeWeightConfigVar baseOffCoeff;
				AiNativeWeightConfigVar goodNmyWeapMinusDmgRatio;
				AiNativeWeightConfigVar goodNmyWeapMinusOffCoeff;

				FarRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "FarRange" ),
					baseDmgRatio( this, "BaseDmgRatio", 1.00f, 1.50f, 1.20f ),
					baseOffCoeff( this, "BaseOffCoeff", 0.50f, 0.90f, 0.75f ),
					goodNmyWeapMinusDmgRatio( this, "GoodNmyWeapMinusDmgRatio", 0.25f, 0.70f, 0.50f ),
					goodNmyWeapMinusOffCoeff( this, "GoodNmyWeapMinusOffCoeff", 0.25f, 0.50f, 0.35f ) {}
			} farRange;

			struct MiddleRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar baseDmgRatio;
				AiNativeWeightConfigVar baseOffCoeff;
				AiNativeWeightConfigVar goodNmyWeapMinusDmgRatio;
				AiNativeWeightConfigVar goodNmyWeapMinusOffCoeff;

				MiddleRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "MiddleRange" ),
					baseDmgRatio( this, "BaseDmgRatio", 1.00f, 1.70f, 1.50f ),
					baseOffCoeff( this, "BaseOffCoeff", 0.30f, 0.60f, 0.40f ),
					goodNmyWeapMinusDmgRatio( this, "GoodNmyWeapMinusDmgRatio", 0.50f, 0.80f, 0.70f ),
					goodNmyWeapMinusOffCoeff( this, "GoodNmyWeapMinusOffCoeff", 0.25f, 0.5f, 0.35f ) {}
			} middleRange;

			RetreatToGoodPositionGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "RetreatToTacticalSpot" ),
				farRange( this ),
				middleRange( this ) {}
		} retreatToGoodPosition;

		struct SteadyCombatGroup : public AiNativeWeightConfigVarGroup {
			struct SniperRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar dmgRatio;
				AiNativeWeightConfigVar offCoeff;

				SniperRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "SniperRange" ),
					dmgRatio( this, "DmgRatio", 0.80f, 1.40f, 1.00f ),
					offCoeff( this, "OffCoeff", 0.10f, 0.90f, 0.50f ) {}
			} sniperRange;

			struct FarRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar dmgRatio;
				AiNativeWeightConfigVar offCoeff;

				FarRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "FarRange" ),
					dmgRatio( this, "DmgRatio", 0.90f, 1.40f, 1.10f ),
					offCoeff( this, "OffCoeff", 0.20f, 0.80f, 0.40f ) {}
			} farRange;

			struct MiddleRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar dmgRatio;
				AiNativeWeightConfigVar offCoeff;

				MiddleRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "MiddleRange" ),
					dmgRatio( this, "DmgRatio", 1.00f, 1.40f, 1.20f ),
					offCoeff( this, "OffCoeff", 0.10f, 0.65f, 0.30f ) {}
			} middleRange;

			struct CloseRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar dmgRatio;
				AiNativeWeightConfigVar offCoeff;

				CloseRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "CloseRange" ),
					dmgRatio( this, "DmgRatio", 1.10f, 1.60f, 1.30f ),
					offCoeff( this, "OffCoeff", 0.05f, 0.50f, 0.20f ) {}
			} closeRange;

			SteadyCombatGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "SteadyCombat" ),
				sniperRange( this ),
				farRange( this ),
				middleRange( this ),
				closeRange( this ) {}
		} steadyCombat;

		struct GotoAvailableGoodPositionGroup : public AiNativeWeightConfigVarGroup {
			struct MiddleRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar baseDmgRatio;
				AiNativeWeightConfigVar baseOffCoeff;
				AiNativeWeightConfigVar goodNmyWeapMinusDmgRatio;
				AiNativeWeightConfigVar goodNmyWeapMinusOffCoeff;

				MiddleRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "MiddleRange" ),
					baseDmgRatio( this, "BaseDmgRatio", 0.90f, 1.40f, 1.20f ),
					baseOffCoeff( this, "BaseOffCoeff", 0.40f, 0.70f, 0.50f ),
					goodNmyWeapMinusDmgRatio( this, "GoodNmyWeapMinusDmgRatio", 0.30f, 0.70f, 0.40f ),
					goodNmyWeapMinusOffCoeff( this, "GoogNmyWeapMinusOffCoeff", 0.10f, 0.40f, 0.20f ) {}
			} middleRange;

			struct CloseRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar baseDmgRatio;
				AiNativeWeightConfigVar baseOffCoeff;
				AiNativeWeightConfigVar goodNmyWeapMinusDmgRatio;
				AiNativeWeightConfigVar goodNmyWeapMinusOffCoeff;
				CloseRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "CloseRange" ),
					baseDmgRatio( this, "BaseDmgRatio", 0.80f, 1.20f, 0.90f ),
					baseOffCoeff( this, "BaseOffCoeff", 0.30f, 0.50f, 0.40f ),
					goodNmyWeapMinusDmgRatio( this, "GoodNmyWeapMinusDmgRatio", 0.30f, 0.50f, 0.40f ),
					goodNmyWeapMinusOffCoeff( this, "GoogNmyWeapMinusOffCoeff", 0.10f, 0.30f, 0.20f ) {}
			} closeRange;

			GotoAvailableGoodPositionGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "GotoAvailableGoodPosition" ),
				middleRange( this ),
				closeRange( this ) {}
		} gotoAvailableGoodPosition;

		struct AttackFromCurrentPositionGroup : public AiNativeWeightConfigVarGroup {
			struct FarRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar dmgRatio;
				AiNativeWeightConfigVar offCoeff;

				FarRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "FarRange" ),
					dmgRatio( this, "DmgRatio", 0.70f, 1.40f, 0.90f ),
					offCoeff( this, "OffCoeff", 0.60f, 3.00f, 1.00f ) {}
			} farRange;

			struct MiddleRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar dmgRatio;
				AiNativeWeightConfigVar offCoeff;

				MiddleRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "MiddleRange" ),
					dmgRatio( this, "DmgRatio", 0.90f, 1.50f, 1.00f ),
					offCoeff( this, "OffCoeff", 0.60f, 2.50f, 1.50f ) {}
			} middleRange;

			struct CloseRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar baseDmgRatio;
				AiNativeWeightConfigVar baseOffCoeff;
				AiNativeWeightConfigVar goodNmyWeapMinusDmgRatio;
				AiNativeWeightConfigVar goodNmyWeapMinusOffCoeff;

				CloseRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "CloseRange" ),
					baseDmgRatio( this, "BaseDmgRatio", 0.80f, 1.40f, 1.00f ),
					baseOffCoeff( this, "BaseOffCoeff", 0.50f, 2.50f, 1.30f ),
					goodNmyWeapMinusDmgRatio( this, "GoodNmyWeapMinusDmgRatio", 0.50f, 1.00f, 0.60f ),
					goodNmyWeapMinusOffCoeff( this, "GoodNmyWeapMinusOffCoeff", 0.30f, 0.80f, 0.50f ) {}
			} closeRange;

			AttackFromCurrentPositionGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "AttackFromCurrentPosition" ),
				farRange( this ),
				middleRange( this ),
				closeRange( this ) {}
		} attackFromCurrentPosition;

		struct RunAwayGroup : public AiNativeWeightConfigVarGroup {
			struct MiddleRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar baseDmgToBeKilled;
				AiNativeWeightConfigVar baseDmgRatio;
				AiNativeWeightConfigVar goodWeapDmgToBeKilled;
				AiNativeWeightConfigVar goodWeapDmgRatio;

				MiddleRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "MiddleRange" ),
					baseDmgToBeKilled( this, "BaseDmgToBeKilled", 50, 125, 100 ),
					baseDmgRatio( this, "BaseDmgRatio", 0.50f, 1.00f, 0.75f ),
					goodWeapDmgToBeKilled( this, "GoodWeapDmgToBeKilled", 25, 50, 35 ),
					goodWeapDmgRatio( this, "GoodWeapDmgRatio", 0.80f, 1.20f, 0.90f ) {}
			} middleRange;

			struct CloseRangeGroup : public AiNativeWeightConfigVarGroup {
				AiNativeWeightConfigVar baseDmgRatio;
				AiNativeWeightConfigVar goodNmyWeapDmgRatio;

				CloseRangeGroup( AiBaseWeightConfigVarGroup *parent )
					: AiNativeWeightConfigVarGroup( parent, "CloseRange" ),
					baseDmgRatio( this, "BaseDmgRatio", 0.70f, 1.10f, 0.80f ),
					goodNmyWeapDmgRatio( this, "GoodNmyWeapDmgRatio", 0.40f, 1.10f, 1.00f ) {}
			} closeRange;

			RunAwayGroup( AiBaseWeightConfigVarGroup *parent )
				: AiNativeWeightConfigVarGroup( parent, "RunAway" ),
				middleRange( this ),
				closeRange( this ) {}
		} runAway;

		NativeActionsGroup( AiBaseWeightConfigVarGroup *parent )
			: AiNativeWeightConfigVarGroup( parent, "NativeActions" ),
			advanceToGoodPosition( this ),
			retreatToGoodPosition( this ),
			steadyCombat( this ),
			gotoAvailableGoodPosition( this ),
			attackFromCurrentPosition( this ),
			runAway( this ) {}
	} nativeActions;

	BotWeightConfig( const edict_t *owner )
		: AiWeightConfig( owner ),
		nativeGoals( Root() ),
		nativeActions( Root() ) {
		RegisterInScript();
	}
};

#endif
