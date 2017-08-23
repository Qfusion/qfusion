#include "ai_base_enemy_pool.h"
#include "bot.h"

constexpr float MAX_ENEMY_WEIGHT = 5.0f;

float DamageToKill( const edict_t *ent, float armorProtection, float armorDegradation ) {
	if( !ent || !ent->takedamage ) {
		return std::numeric_limits<float>::infinity();
	}

	if( !ent->r.client ) {
		return ent->health;
	}

	float health = ent->r.client->ps.stats[STAT_HEALTH];
	float armor = ent->r.client->ps.stats[STAT_ARMOR];

	return DamageToKill( health, armor, armorProtection, armorDegradation );
}

float DamageToKill( float health, float armor, float armorProtection, float armorDegradation ) {
	if( !armor ) {
		return health;
	}
	if( armorProtection == 1.0f ) {
		return std::numeric_limits<float>::infinity();
	}

	if( armorDegradation != 0 ) {
		float damageToWipeArmor = armor / armorDegradation;
		float healthDamageToWipeArmor = damageToWipeArmor * ( 1.0f - armorProtection );

		if( healthDamageToWipeArmor < health ) {
			return damageToWipeArmor + ( health - healthDamageToWipeArmor );
		}

		return health / ( 1.0f - armorProtection );
	}

	return health / ( 1.0f - armorProtection );
}

void Enemy::Clear() {
	ent = nullptr;
	weight = 0.0f;
	avgPositiveWeight = 0.0f;
	maxPositiveWeight = 0.0f;
	positiveWeightsCount = 0;
	registeredAt = 0;
	lastSeenSnapshots.clear();
	lastSeenAt = 0;
}

void Enemy::OnViewed( const float *specifiedOrigin ) {
	if( lastSeenSnapshots.size() == MAX_TRACKED_POSITIONS ) {
		lastSeenSnapshots.pop_front();
	}

	// Put the likely case first
	const float *origin = !specifiedOrigin ? ent->s.origin : specifiedOrigin;
	// Set members for faster access
	VectorCopy( origin, lastSeenPosition.Data() );
	VectorCopy( ent->velocity, lastSeenVelocity.Data() );
	lastSeenAt = level.time;
	// Store in a queue then for history
	lastSeenSnapshots.emplace_back( Snapshot( ent->s.origin, ent->velocity, level.time ) );
}

AiBaseEnemyPool::AiBaseEnemyPool( float avgSkill_ )
	: avgSkill( avgSkill_ ),
	decisionRandom( 0.5f ),
	decisionRandomUpdateAt( 0 ),
	trackedEnemiesCount( 0 ),
	maxTrackedEnemies( 3 + From0UpToMax( MAX_TRACKED_ENEMIES - 3, avgSkill_ ) ),
	maxTrackedAttackers( From1UpToMax( MAX_TRACKED_ATTACKERS, avgSkill_ ) ),
	maxTrackedTargets( From1UpToMax( MAX_TRACKED_TARGETS, avgSkill_ ) ),
	maxActiveEnemies( From1UpToMax( MAX_ACTIVE_ENEMIES, avgSkill_ ) ),
	reactionTime( 320 - From0UpToMax( 300, avgSkill_ ) ),
	prevThinkLevelTime( 0 ),
	hasQuad( false ),
	hasShell( false ),
	damageToBeKilled( 0.0f ) {
	unsigned maxEnemies = maxTrackedEnemies;
	// Ensure we always will have at least 2 free slots for new enemies
	// (quad/shell owners and carrier) FOR ANY SKILL (high skills will have at least 3)
	if( maxTrackedAttackers + 2 > maxEnemies ) {
		FailWith( "skill %f: maxTrackedAttackers %d + 2 > maxTrackedEnemies %d\n", AvgSkill(), maxTrackedAttackers, maxEnemies );
	}

	if( maxTrackedTargets + 2 > maxEnemies ) {
		FailWith( "skill %f: maxTrackedTargets %d + 2 > maxTrackedEnemies %d\n", AvgSkill(), maxTrackedTargets, maxEnemies );
	}

	// Initialize empty slots
	for( unsigned i = 0; i < maxTrackedEnemies; ++i )
		trackedEnemies[i].parent = this;

	for( unsigned i = 0; i < maxTrackedAttackers; ++i )
		attackers.push_back( AttackStats() );

	for( unsigned i = 0; i < maxTrackedTargets; ++i )
		targets.push_back( AttackStats() );
}

void AiBaseEnemyPool::Frame() {

	const int64_t levelTime = level.time;

	for( AttackStats &attackerStats: attackers ) {
		attackerStats.Frame();
		if( attackerStats.LastActivityAt() + ATTACKER_TIMEOUT < levelTime ) {
			attackerStats.Clear();
		}
	}

	for( AttackStats &targetStats: targets ) {
		targetStats.Frame();
		if( targetStats.LastActivityAt() + TARGET_TIMEOUT < levelTime ) {
			targetStats.Clear();
		}
	}

	// If we could see enemy entering teleportation a last Think() frame, update its tracked origin by the actual one.
	for( unsigned i = 0; i < maxTrackedEnemies; ++i ) {
		Enemy *enemy = &trackedEnemies[i];
		const edict_t *ent = enemy->ent;
		if( !ent ) {
			continue;
		}
		// If the enemy cannot be longer valid
		if( G_ISGHOSTING( ent ) ) {
			continue;
		}
		if( !ent->s.teleported ) {
			continue;
		}
		if( levelTime - enemy->lastSeenAt >= 64 ) {
			continue;
		}
		enemy->OnViewed();
	}
}

void AiBaseEnemyPool::PreThink() {
	hasQuad = CheckHasQuad();
	hasShell = CheckHasShell();
	damageToBeKilled = ComputeDamageToBeKilled();
	if( decisionRandomUpdateAt <= level.time ) {
		decisionRandom = random();
		decisionRandomUpdateAt = level.time + 1500;
	}
}

void AiBaseEnemyPool::Think() {
	const int64_t levelTime = level.time;
	for( Enemy &enemy: trackedEnemies ) {
		// If enemy slot is free
		if( !enemy.ent ) {
			continue;
		}
		// Remove not seen yet enemies
		if( levelTime - enemy.LastSeenAt() > NOT_SEEN_TIMEOUT ) {
			Debug( "has not seen %s for %d ms, should forget this enemy\n", enemy.Nick(), NOT_SEEN_TIMEOUT );
			RemoveEnemy( enemy );
			continue;
		}
		if( G_ISGHOSTING( enemy.ent ) ) {
			Debug( "should forget %s (this enemy is ghosting)\n", enemy.Nick() );
			RemoveEnemy( enemy );
			continue;
		}
		// Do not forget, just skip
		if( enemy.ent->flags & ( FL_NOTARGET | FL_BUSY ) ) {
			continue;
		}
		// Skip during reaction time
		if( enemy.registeredAt + reactionTime > levelTime ) {
			continue;
		}

		UpdateEnemyWeight( enemy );
	}
}

float AiBaseEnemyPool::ComputeRawEnemyWeight( const edict_t *enemy ) {
	if( !enemy || G_ISGHOSTING( enemy ) ) {
		return 0.0;
	}

	float weight = 0.5f;
	if( !enemy->r.client ) {
		weight = enemy->aiIntrinsicEnemyWeight;
		if( weight <= 0.0f ) {
			return 0.0f;
		}
	}

	if( int64_t time = LastAttackedByTime( enemy ) ) {
		weight += 1.55f * ( 1.0f - BoundedFraction( level.time - time, ATTACKER_TIMEOUT ) );
		// TODO: Add weight for poor attackers (by total damage / attack attepts ratio)
	}

	if( int64_t time = LastTargetTime( enemy ) ) {
		weight += 1.55f * ( 1.0f - BoundedFraction( level.time - time, TARGET_TIMEOUT ) );
		// TODO: Add weight for targets that are well hit by bot
	}

	if( ::IsCarrier( enemy ) ) {
		weight += 2.0f;
	}

	constexpr float maxDamageToKill = 350.0f;

	float damageToKill = DamageToKill( enemy );
	if( hasQuad ) {
		damageToKill /= 4;
	}
	if( ::HasShell( enemy ) ) {
		damageToKill *= 4;
	}

	// abs(damageToBeKilled - damageToKill) / maxDamageToKill may be > 1
	weight += ( damageToBeKilled - damageToKill ) / maxDamageToKill;

	if( weight > 0 ) {
		if( hasQuad ) {
			weight *= 1.5f;
		}
		if( hasShell ) {
			weight += 0.5f;
		}
		if( hasQuad && hasShell ) {
			weight *= 1.5f;
		}
	}

	return std::min( std::max( 0.0f, weight ), MAX_ENEMY_WEIGHT );
}

void AiBaseEnemyPool::OnPain( const edict_t *bot, const edict_t *enemy, float kick, int damage ) {
	int attackerSlot = EnqueueAttacker( enemy, damage );
	if( attackerSlot < 0 ) {
		return;
	}

	bool newThreat = true;
	if( bot->ai->botRef->IsPrimaryAimEnemy( enemy ) ) {
		newThreat = false;
		int currEnemySlot = -1;
		for( int i = 0, end = attackers.size(); i < end; ++i ) {
			if( bot->ai->botRef->IsPrimaryAimEnemy( attackers[i].ent ) ) {
				currEnemySlot = i;
				break;
			}
		}
		// If current enemy did not inflict any damage
		// or new attacker hits harder than current one, there is a new threat
		if( currEnemySlot < 0 || attackers[currEnemySlot].totalDamage < attackers[attackerSlot].totalDamage ) {
			newThreat = true;
		}
	}

	if( newThreat ) {
		OnNewThreat( enemy );
	}
}

int64_t AiBaseEnemyPool::LastAttackedByTime( const edict_t *ent ) const {
	for( const AttackStats &attackStats: attackers )
		if( ent && attackStats.ent == ent ) {
			return attackStats.LastActivityAt();
		}

	return 0;
}

int64_t AiBaseEnemyPool::LastTargetTime( const edict_t *ent ) const {
	for( const AttackStats &targetStats: targets )
		if( ent && targetStats.ent == ent ) {
			return targetStats.LastActivityAt();
		}

	return 0;
}

float AiBaseEnemyPool::TotalDamageInflictedBy( const edict_t *ent ) const {
	for( const AttackStats &attackStats: attackers )
		if( ent && attackStats.ent == ent ) {
			return attackStats.totalDamage;
		}

	return 0;
}

int AiBaseEnemyPool::EnqueueAttacker( const edict_t *attacker, int damage ) {
	if( !attacker ) {
		return -1;
	}

	int freeSlot = -1;
	for( unsigned i = 0; i < attackers.size(); ++i ) {
		if( attackers[i].ent == attacker ) {
			attackers[i].OnDamage( damage );
			return i;
		} else if( !attackers[i].ent && freeSlot < 0 ) {
			freeSlot = i;
		}
	}
	if( freeSlot >= 0 ) {
		attackers[freeSlot].Clear();
		attackers[freeSlot].ent = attacker;
		attackers[freeSlot].OnDamage( damage );
		return freeSlot;
	}
	float maxEvictionScore = 0.0f;
	for( unsigned i = 0; i < attackers.size(); ++i ) {
		float timeFactor = ( level.time - attackers[i].LastActivityAt() ) / (float)ATTACKER_TIMEOUT;
		float damageFactor = 1.0f - BoundedFraction( attackers[i].totalDamage, 500.0f );
		// Always > 0, so we always evict some attacker
		float evictionScore = 0.1f + timeFactor * damageFactor;
		if( maxEvictionScore < evictionScore ) {
			maxEvictionScore = evictionScore;
			freeSlot = i;
		}
	}
	attackers[freeSlot].Clear();
	attackers[freeSlot].ent = attacker;
	attackers[freeSlot].OnDamage( damage );
	return freeSlot;
}

void AiBaseEnemyPool::EnqueueTarget( const edict_t *target ) {
	if( !target ) {
		return;
	}

	int freeSlot = -1;
	for( unsigned i = 0; i < targets.size(); ++i ) {
		if( targets[i].ent == target ) {
			targets[i].Touch();
			return;
		} else if( !targets[i].ent && freeSlot < 0 ) {
			freeSlot = i;
		}
	}
	if( freeSlot >= 0 ) {
		targets[freeSlot].Clear();
		targets[freeSlot].ent = target;
		targets[freeSlot].Touch();
		return;
	}
	float maxEvictionScore = 0.0f;
	for( unsigned i = 0; i < targets.size(); ++i ) {
		float timeFactor = ( level.time - targets[i].LastActivityAt() ) / (float)TARGET_TIMEOUT;
		// Do not evict enemies that bot hit hard
		float damageScale = this->HasQuad() ? 4.0f : 1.0f;
		float damageFactor = 1.0f - BoundedFraction( targets[i].totalDamage, 300.0f * damageScale );
		// Always > 0, so we always evict some target
		float evictionScore = 0.1f + timeFactor * damageFactor;
		if( maxEvictionScore < evictionScore ) {
			maxEvictionScore = evictionScore;
			freeSlot = i;
		}
	}
	targets[freeSlot].Clear();
	targets[freeSlot].ent = target;
	targets[freeSlot].Touch();
}

void AiBaseEnemyPool::OnEnemyDamaged( const edict_t *bot, const edict_t *target, int damage ) {
	if( !target ) {
		return;
	}

	for( unsigned i = 0; i < targets.size(); ++i ) {
		if( targets[i].ent == target ) {
			targets[i].OnDamage( damage );
			return;
		}
	}
}

bool AiBaseEnemyPool::WillAssignAimEnemy() const {
	for( const Enemy &enemy: trackedEnemies ) {
		if( !enemy.ent ) {
			continue;
		}
		if( enemy.LastSeenAt() == level.time ) {
			// Check whether we may react
			for( const auto &snapshot: enemy.lastSeenSnapshots ) {
				if( snapshot.timestamp + reactionTime <= level.time ) {
					return true;
				}
			}
		}
	}
	return false;
}

void AiBaseEnemyPool::UpdateEnemyWeight( Enemy &enemy ) {
	// Explicitly limit effective reaction time to a time quantum between Think() calls
	// This method gets called before all enemies are viewed.
	// For seen enemy registration actual weights of known enemies are mandatory
	// (enemies may get evicted based on their weights and weight of a just seen enemy).
	if( level.time - enemy.LastSeenAt() > std::max( 64u, reactionTime ) ) {
		enemy.weight = 0;
		return;
	}

	enemy.weight = ComputeRawEnemyWeight( enemy.ent );
	if( enemy.weight > enemy.maxPositiveWeight ) {
		enemy.maxPositiveWeight = enemy.weight;
	}
	if( enemy.weight > 0 ) {
		enemy.avgPositiveWeight = enemy.avgPositiveWeight * enemy.positiveWeightsCount + enemy.weight;
		enemy.positiveWeightsCount++;
		enemy.avgPositiveWeight /= enemy.positiveWeightsCount;
	}
}

struct EnemyAndScore
{
	Enemy *enemy;
	float score;
	EnemyAndScore( Enemy *enemy_, float score_ ) : enemy( enemy_ ), score( score_ ) {}
	bool operator<( const EnemyAndScore &that ) const { return score > that.score; }
};

const Enemy *AiBaseEnemyPool::ChooseVisibleEnemy( const edict_t *challenger ) {
	Vec3 botOrigin( challenger->s.origin );
	vec3_t forward;
	AngleVectors( challenger->s.angles, forward, nullptr, nullptr );

	// Until these bounds distance factor scales linearly
	constexpr float distanceBounds = 3500.0f;

	StaticVector<EnemyAndScore, MAX_TRACKED_ENEMIES> candidates;

	for( unsigned i = 0; i < maxTrackedEnemies; ++i ) {
		Enemy &enemy = trackedEnemies[i];
		if( !enemy.ent ) {
			continue;
		}
		// Not seen in this frame enemies have zero weight;
		if( !enemy.weight ) {
			continue;
		}

		Vec3 botToEnemy = botOrigin - enemy.ent->s.origin;
		float distance = botToEnemy.LengthFast();
		botToEnemy *= 1.0f / distance;
		// For far enemies distance factor is lower
		float distanceFactor = 1.0f - 0.7f * BoundedFraction( distance, distanceBounds );
		// Should affect the score only a bit (otherwise bot will miss a dangerous enemy that he is not looking at).
		float directionFactor = 0.7f + 0.3f * botToEnemy.Dot( forward );

		float weight = enemy.weight + GetAdditionalEnemyWeight( challenger, enemy.ent );
		float currScore = weight * distanceFactor * directionFactor;
		candidates.push_back( EnemyAndScore( &enemy, currScore ) );
	}

	if( candidates.empty() ) {
		OnBotEnemyAssigned( challenger, nullptr );
		return nullptr;
	}

	// Its better to sort once instead of pushing into a heap inside the loop above
	std::sort( candidates.begin(), candidates.end() );

	// Now candidates should be merged in a list of active enemies
	StaticVector<EnemyAndScore, MAX_ACTIVE_ENEMIES * 2> mergedActiveEnemies;
	// Best candidates are first (EnemyAndScore::operator<() yields this result)
	// Choose not more than maxActiveEnemies candidates
	// that have a score not than twice less than the current best one
	float bestCurrentActiveScore = candidates.front().score;
	for( unsigned i = 0, end = std::min( candidates.size(), maxActiveEnemies ); i < end; ++i ) {
		if( candidates[i].score < 0.5f * bestCurrentActiveScore ) {
			break;
		}
		mergedActiveEnemies.push_back( candidates[i] );
	}
	// Add current active enemies to merged ones
	for( unsigned i = 0, end = activeEnemies.size(); i < end; ++i )
		mergedActiveEnemies.push_back( EnemyAndScore( activeEnemies[i], activeEnemiesScores[i] ) );

	// Sort merged enemies
	std::sort( mergedActiveEnemies.begin(), mergedActiveEnemies.end() );

	// Select not more than maxActiveEnemies mergedActiveEnemies as a current activeEnemies
	activeEnemies.clear();
	activeEnemiesScores.clear();
	for( unsigned i = 0, end = std::min( mergedActiveEnemies.size(), maxActiveEnemies ); i < end; ++i ) {
		activeEnemies.push_back( mergedActiveEnemies[i].enemy );
		activeEnemiesScores.push_back( mergedActiveEnemies[i].score );
	}

	OnBotEnemyAssigned( challenger, candidates.front().enemy );
	// (We operate on pointers to enemies which are allocated in the enemy pool)
	return candidates.front().enemy;
}

const Enemy *AiBaseEnemyPool::ChooseLostOrHiddenEnemy( const edict_t *challenger, unsigned timeout ) {
	if( AvgSkill() < 0.33f ) {
		return nullptr;
	}

	vec3_t forward;
	AngleVectors( challenger->s.angles, forward, nullptr, nullptr );

	// ChooseLostOrHiddenEnemy(const edict_t *challenger, unsigned timeout = (unsigned)-1)
	if( timeout > NOT_SEEN_TIMEOUT ) {
		timeout = (unsigned)( ( NOT_SEEN_TIMEOUT - 1000 ) * AvgSkill() );
	}

	float bestScore = 0.0f;
	const Enemy *bestEnemy = nullptr;

	for( unsigned i = 0; i < maxTrackedEnemies; ++i ) {
		const Enemy &enemy = trackedEnemies[i];
		if( !enemy.ent ) {
			continue;
		}
		if( enemy.weight ) {
			continue;
		}

		Vec3 botToSpotDirection = enemy.LastSeenPosition() - challenger->s.origin;
		float directionFactor = 0.5f;
		float distanceFactor = 1.0f;
		float squareDistance = botToSpotDirection.SquaredLength();
		if( squareDistance > 1 ) {
			float distance = 1.0f / Q_RSqrt( squareDistance );
			botToSpotDirection *= 1.0f / distance;
			directionFactor = 0.3f + 0.7f * botToSpotDirection.Dot( forward );
			distanceFactor = 1.0f - 0.9f * BoundedFraction( distance, 2000.0f );
		}
		float timeFactor = 1.0f - BoundedFraction( level.time - enemy.LastSeenAt(), timeout );

		float currScore = ( 0.5f * ( enemy.maxPositiveWeight + enemy.avgPositiveWeight ) );
		currScore *= directionFactor * distanceFactor * timeFactor;
		if( currScore > bestScore ) {
			bestScore = currScore;
			bestEnemy = &enemy;
		}
	}

	return bestEnemy;
}

void AiBaseEnemyPool::OnEnemyViewed( const edict_t *enemy ) {
	if( !enemy ) {
		return;
	}

	int freeSlot = -1;
	for( unsigned i = 0; i < maxTrackedEnemies; ++i ) {
		// Use first free slot for faster access and to avoid confusion
		if( !trackedEnemies[i].ent && freeSlot < 0 ) {
			freeSlot = i;
		} else if( trackedEnemies[i].ent == enemy ) {
			trackedEnemies[i].OnViewed();
			return;
		}
	}

	if( freeSlot >= 0 ) {
		Debug( "has viewed a new enemy %s, uses free slot #%d to remember it\n", Nick( enemy ), freeSlot );
		EmplaceEnemy( enemy, freeSlot );
		trackedEnemiesCount++;
	} else {
		Debug( "has viewed a new enemy %s, all slots are used. Should try evict some slot\n", Nick( enemy ) );
		TryPushNewEnemy( enemy, nullptr );
	}
}

void AiBaseEnemyPool::OnEnemyOriginGuessed( const edict_t *enemy,
											unsigned minMillisSinceLastSeen,
											const float *guessedOrigin ) {
	if( !enemy ) {
		return;
	}

	const int64_t levelTime = level.time;
	int freeSlot = -1;
	for( unsigned i = 0; i < maxTrackedEnemies; ++i ) {
		if( !trackedEnemies[i].ent ) {
			freeSlot = i;
			continue;
		}
		if( trackedEnemies[i].ent != enemy ) {
			continue;
		}
		// If there is already an Enemy record containing an entity,
		// check whether this record timed out enough to be overwritten.
		// This code prevents overwriting
		if( trackedEnemies[i].lastSeenAt + minMillisSinceLastSeen > levelTime ) {
			continue;
		}
		trackedEnemies[i].OnViewed();
		return;
	}

	if( freeSlot > 0 ) {
		Debug( "has guessed a new origin for not seen enemy %s, uses a free slot to remember it\n", Nick( enemy ) );
		EmplaceEnemy( enemy, freeSlot, guessedOrigin );
		trackedEnemiesCount++;
	} else {
		Debug( "has guessed a new enemy %s, all slots are used. Should try to evict some slot\n", Nick( enemy ) );
		TryPushNewEnemy( enemy, guessedOrigin );
	}
}

void AiBaseEnemyPool::Forget( const edict_t *enemy ) {
	if( !enemy ) {
		return;
	}

	for( unsigned i = 0; i < maxTrackedEnemies; ++i ) {
		if( trackedEnemies[i].ent == enemy ) {
			RemoveEnemy( trackedEnemies[i] );
			return;
		}
	}
}

void AiBaseEnemyPool::RemoveEnemy( Enemy &enemy ) {
	// Call overridden method that should contain domain-specific logic
	OnEnemyRemoved( &enemy );

	enemy.Clear();
	--trackedEnemiesCount;
}

void AiBaseEnemyPool::EmplaceEnemy( const edict_t *enemy, int slot, const float *specifiedOrigin ) {
	Enemy &slotEnemy = trackedEnemies[slot];
	slotEnemy.ent = enemy;
	slotEnemy.registeredAt = level.time;
	slotEnemy.weight = 0.0f;
	slotEnemy.avgPositiveWeight = 0.0f;
	slotEnemy.maxPositiveWeight = 0.0f;
	slotEnemy.positiveWeightsCount = 0;
	slotEnemy.OnViewed( specifiedOrigin );
	Debug( "has stored enemy %s in slot %d\n", slotEnemy.Nick(), slot );
}

void AiBaseEnemyPool::TryPushEnemyOfSingleBot( const edict_t *bot, const edict_t *enemy, const float *specifiedOrigin ) {
	// Try to find a free slot. For each used and not reserved slot compute eviction score relative to new enemy
	int candidateSlot = -1;

	// Floating point computations for zero cases from pure math point of view may yield a non-zero result,
	// so use some positive value that is greater that possible computation zero epsilon.
	float maxEvictionScore = 0.001f;
	// Significantly increases chances to get a slot, but not guarantees it.
	bool isNewEnemyAttacker = LastAttackedByTime( enemy ) > 0;
	// It will be useful inside the loop, so it needs to be precomputed
	float distanceToNewEnemy;
	if( !specifiedOrigin ) {
		distanceToNewEnemy = ( Vec3( bot->s.origin ) - enemy->s.origin ).LengthFast();
	} else {
		distanceToNewEnemy = DistanceFast( bot->s.origin, specifiedOrigin );
	}
	float newEnemyWeight = ComputeRawEnemyWeight( enemy );

	for( unsigned i = 0; i < maxTrackedEnemies; ++i ) {
		Enemy &slotEnemy = trackedEnemies[i];
		// Skip last attackers
		if( bot->ai->botRef->LastAttackedByTime( slotEnemy.ent ) > 0 ) {
			continue;
		}
		// Skip last targets
		if( bot->ai->botRef->LastTargetTime( slotEnemy.ent ) > 0 ) {
			continue;
		}

		// Never evict powerup owners or item carriers
		if( slotEnemy.HasPowerups() || slotEnemy.IsCarrier() ) {
			continue;
		}

		float currEvictionScore = 0.0f;
		if( isNewEnemyAttacker ) {
			currEvictionScore += 0.5f;
		}

		float absWeightDiff = slotEnemy.weight - newEnemyWeight;
		if( newEnemyWeight > slotEnemy.weight ) {
			currEvictionScore += newEnemyWeight - slotEnemy.weight;
		} else {
			if( AvgSkill() < 0.66f ) {
				if( decisionRandom > AvgSkill() ) {
					currEvictionScore += ( 1.0 - AvgSkill() ) * expf( -absWeightDiff );
				}
			}
		}

		// Forget far and not seen enemies
		if( slotEnemy.LastSeenAt() < prevThinkLevelTime ) {
			float absTimeDiff = prevThinkLevelTime - slotEnemy.LastSeenAt();
			// 0..1
			float timeFactor = std::min( absTimeDiff, (float)NOT_SEEN_TIMEOUT ) / NOT_SEEN_TIMEOUT;

			float distanceToSlotEnemy = ( slotEnemy.LastSeenPosition() - bot->s.origin ).LengthFast();
			constexpr float maxDistanceDiff = 2500.0f;
			float nonNegDistDiff = std::max( 0.0f, distanceToSlotEnemy - distanceToNewEnemy );
			// 0..1
			float distanceFactor = std::min( maxDistanceDiff, nonNegDistDiff ) / maxDistanceDiff;

			// += 0..1,  Increase eviction score linearly for far enemies
			currEvictionScore += 1.0f - distanceFactor;
			// += 2..0, Increase eviction score non-linearly for non-seen enemies (forget far enemies faster)
			currEvictionScore += 2.0f - timeFactor * ( 1.0f + distanceFactor );
		}

		if( currEvictionScore > maxEvictionScore ) {
			maxEvictionScore = currEvictionScore;
			candidateSlot = i;
		}
	}

	if( candidateSlot != -1 ) {
		Debug( "will evict %s to make a free slot, new enemy have higher priority atm\n", Nick( enemy ) );
		EmplaceEnemy( enemy, candidateSlot, specifiedOrigin );
	} else {
		Debug( "can't find free slot for %s, all current enemies have higher priority\n", Nick( enemy ) );
	}
}
