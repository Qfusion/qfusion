#ifndef QFUSION_BOT_PERCEPTION_MANAGER_H
#define QFUSION_BOT_PERCEPTION_MANAGER_H

#include "ai_base_brain.h"

struct Danger : public PoolItem {
	static constexpr unsigned TIMEOUT = 400;

	Danger( PoolBase *pool_ )
        : PoolItem( pool_ ),
        hitPoint( 0, 0, 0 ),
        direction( 0, 0, 0 ),
        damage( 0 ),
        timeoutAt( 0 ),
        attacker( nullptr ),
        splash( false ) {}

	// Sorting by this operator is fast but should be used only
	// to prepare most dangerous entities of the same type.
	// Ai decisions should be made by more sophisticated code.
	bool operator<( const Danger &that ) const { return this->damage < that.damage; }

	bool IsValid() const { return timeoutAt > level.time; }

	Vec3 hitPoint;
	Vec3 direction;
	float damage;
	int64_t timeoutAt;
	const edict_t *attacker;
	bool splash;
};

class EntitiesDetector
{
	friend class BotPerceptionManager;

	struct EntAndDistance {
		int entNum;
		float distance;

		EntAndDistance( int entNum_, float distance_ ) : entNum( entNum_ ), distance( distance_ ) {}
		bool operator<( const EntAndDistance &that ) const { return distance < that.distance; }
	};

	static constexpr float DETECT_ROCKET_SQ_RADIUS = 300 * 300;
	static constexpr float DETECT_PLASMA_SQ_RADIUS = 400 * 400;
	static constexpr float DETECT_GB_BLAST_SQ_RADIUS = 400 * 400;
	static constexpr float DETECT_GRENADE_SQ_RADIUS = 300 * 300;
	static constexpr float DETECT_LG_BEAM_SQ_RADIUS = 1000 * 1000;

	// There is a way to compute it in compile-time but it looks ugly
	static constexpr float MAX_RADIUS = 1000.0f;
	static_assert( MAX_RADIUS * MAX_RADIUS >= DETECT_ROCKET_SQ_RADIUS, "" );
	static_assert( MAX_RADIUS * MAX_RADIUS >= DETECT_PLASMA_SQ_RADIUS, "" );
	static_assert( MAX_RADIUS * MAX_RADIUS >= DETECT_GB_BLAST_SQ_RADIUS, "" );
	static_assert( MAX_RADIUS * MAX_RADIUS >= DETECT_GRENADE_SQ_RADIUS, "" );
	static_assert( MAX_RADIUS * MAX_RADIUS >= DETECT_LG_BEAM_SQ_RADIUS, "" );

	void Clear();

	static const auto MAX_NONCLIENT_ENTITIES = MAX_EDICTS - MAX_CLIENTS;
	typedef StaticVector<EntAndDistance, MAX_NONCLIENT_ENTITIES> EntsAndDistancesVector;
	typedef StaticVector<uint16_t, MAX_NONCLIENT_ENTITIES> EntNumsVector;

	inline void TryAddEntity( const edict_t *ent,
							  float squareDistanceThreshold,
							  EntsAndDistancesVector &dangerousEntities,
							  EntsAndDistancesVector &otherEntities );
	inline void TryAddGrenade( const edict_t *ent,
							   EntsAndDistancesVector &dangerousEntities,
							   EntsAndDistancesVector &otherEntities );


	// Returns false if not all entities were tested (some entities have been rejected by the limit)
	template<unsigned N, unsigned M, typename PvsFunc, typename VisFunc>
	bool FilterRawEntitiesWithDistances( StaticVector<EntAndDistance, N> &rawEnts,
										 StaticVector<uint16_t, M> &filteredEnts,
										 unsigned visEntsLimit,
										 PvsFunc pvsFunc, VisFunc visFunc );

	const edict_t *const self;

	EntsAndDistancesVector maybeDangerousRockets;
	EntNumsVector dangerousRockets;
	EntsAndDistancesVector maybeDangerousPlasmas;
	EntNumsVector dangerousPlasmas;
	EntsAndDistancesVector maybeDangerousBlasts;
	EntNumsVector dangerousBlasts;
	EntsAndDistancesVector maybeDangerousGrenades;
	EntNumsVector dangerousGrenades;
	StaticVector<EntAndDistance, MAX_NONCLIENT_ENTITIES> maybeDangerousLasers;
	StaticVector<uint16_t, MAX_NONCLIENT_ENTITIES> dangerousLasers;

	EntsAndDistancesVector maybeVisibleOtherRockets;
	EntNumsVector visibleOtherRockets;
	EntsAndDistancesVector maybeVisibleOtherPlasmas;
	EntNumsVector visibleOtherPlasmas;
	EntsAndDistancesVector maybeVisibleOtherBlasts;
	EntNumsVector visibleOtherBlasts;
	EntsAndDistancesVector maybeVisibleOtherGrenades;
	EntNumsVector visibleOtherGrenades;
	EntsAndDistancesVector maybeVisibleOtherLasers;
	EntNumsVector visibleOtherLasers;

	EntitiesDetector( const edict_t *self_ ) : self( self_ ) {}

	void Run();
};

class BotPerceptionManager
{
	friend class PlasmaBeamsBuilder;
	friend class EntitiesDetector;

	EntitiesDetector entitiesDetector;

	edict_t *const self;

	// Currently there is no more than a single active danger. It might be changed in future.
	static constexpr auto MAX_CLASS_DANGERS = 1;
	typedef Pool<Danger, MAX_CLASS_DANGERS> DangersPool;

	DangersPool rocketDangersPool;
	DangersPool plasmaBeamDangersPool;
	DangersPool grenadeDangersPool;
	DangersPool blastDangersPool;
	DangersPool laserBeamsPool;

	Danger *primaryDanger;

	float viewDirDotTeammateDir[MAX_CLIENTS];
	float distancesToTeammates[MAX_CLIENTS];
	uint8_t testedTeammatePlayerNums[MAX_CLIENTS];
	int8_t teammatesVisStatus[MAX_CLIENTS];
	unsigned numTestedTeamMates;
	bool hasComputedTeammatesVisData;
	bool areAllTeammatesInFov;

	static const auto MAX_NONCLIENT_ENTITIES = EntitiesDetector::MAX_NONCLIENT_ENTITIES;
	typedef EntitiesDetector::EntAndDistance EntAndDistance;

	void ClearDangers();

	bool TryAddDanger( float damageScore, const vec3_t hitPoint, const vec3_t direction,
					   const edict_t *owner, bool splash = false );

	typedef StaticVector<uint16_t, MAX_NONCLIENT_ENTITIES> EntNumsVector;
	void FindProjectileDangers( const EntNumsVector &entNums, float dangerRadius, float damageScale );

	void FindPlasmaDangers( const EntNumsVector &entNums );
	void FindLaserDangers( const EntNumsVector &entNums );

	// The failure chance is specified mainly to throttle excessive plasma spam
	void TryGuessingBeamOwnersOrigins( const EntNumsVector &dangerousEntsNums, float failureChance );
	void TryGuessingProjectileOwnersOrigins( const EntNumsVector &dangerousEntNums, float failureChance );

	void ResetTeammatesVisData();
	void ComputeTeammatesVisData( const vec3_t forwardDir, float fovDotFactor );
	bool CanDistinguishEnemyShotsFromTeammates( const vec_t *guessedEnemyOrigin );

	void RegisterVisibleEnemies();
public:
	BotPerceptionManager( edict_t *self_ )
		: entitiesDetector( self_ ),
		self( self_ ),
		rocketDangersPool( "rocket dangers pool" ),
		plasmaBeamDangersPool( "plasma beam dangers pool" ),
		grenadeDangersPool( "grenade dangers pool" ),
		blastDangersPool( "blast dangers pool" ),
		laserBeamsPool( "laser beams pool" ),
		primaryDanger( nullptr ) {}

	const Danger *PrimaryDanger() const { return primaryDanger; }

	void Frame();
};

#endif
