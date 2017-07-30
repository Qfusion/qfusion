#include "ai_base_team_brain.h"
#include "ai_squad_based_team_brain.h"
#include "ai_shutdown_hooks_holder.h"
#include "bot.h"

// May be instantiated dynamically in future by subclasses
static AiBaseTeamBrain *teamBrains[GS_MAX_TEAMS - 1];

AiBaseTeamBrain::AiBaseTeamBrain( int team_ )
	: team( team_ ) {
	svFps = -1;
	svSkill = -1;
	teamBrainAffinityModulo = -1;
	teamBrainAffinityOffset = -1;

	memset( botAffinityModulo, 0, sizeof( botAffinityModulo ) );
	memset( botAffinityOffsets, 0, sizeof( botAffinityOffsets ) );
}

void AiBaseTeamBrain::Debug( const char *format, ... ) {
	// Cut it early to help optimizer to eliminate AI_Debugv call
#ifdef _DEBUG
	va_list va;
	va_start( va, format );
	AI_Debugv( GS_TeamName( team ), format, va );
	va_end( va );
#endif
}

AiBaseTeamBrain *AiBaseTeamBrain::GetBrainForTeam( int team ) {
	if( team < TEAM_PLAYERS || team >= GS_MAX_TEAMS ) {
		AI_FailWith( "AiBaseTeamBrain", "GetBrainForTeam(): Illegal team %d\n", team );
	}
	if( !teamBrains[team - 1] ) {
		AI_FailWith( "AiBaseTeamBrain", "GetBrainForTeam(): Team brain for team %d is not instantiated atm\n", team );
	}
	return teamBrains[team - 1];
}

unsigned AiBaseTeamBrain::AffinityModulo() const {
	if( teamBrainAffinityModulo == -1 ) {
		InitTeamAffinity();
	}
	return (unsigned)teamBrainAffinityModulo;
}

unsigned AiBaseTeamBrain::TeamAffinityOffset() const {
	if( teamBrainAffinityOffset == -1 ) {
		InitTeamAffinity();
	}
	return (unsigned)teamBrainAffinityOffset;
}

void AiBaseTeamBrain::InitTeamAffinity() const {
	// We round frame time to integer milliseconds
	int frameTime = 1000 / ServerFps();
	// 4 for 60 fps or more, 1 for 16 fps or less
	teamBrainAffinityModulo = std::min( 4, std::max( 1, 64 / frameTime ) );
	if( team == TEAM_PLAYERS ) {
		teamBrainAffinityOffset = 0;
		const_cast<AiBaseTeamBrain*>( this )->SetFrameAffinity( teamBrainAffinityModulo, teamBrainAffinityOffset );
		return;
	}

	static_assert( TEAM_ALPHA == 2 && TEAM_BETA == 3, "Modify affinity offset computations" );
	switch( teamBrainAffinityModulo ) {
		// The Alpha team brain thinks on frame 0, the Beta team brain thinks on frame 2
		case 4: teamBrainAffinityOffset = ( team - 2 ) * 2; break;
		// Both Alpha and Beta team brains think on frame 0
		case 3: teamBrainAffinityOffset = 0; break;
		// The Alpha team brain thinks on frame 0, the Beta team brain thinks on frame 1
		case 2: teamBrainAffinityOffset = team - 2; break;
		// All brains think in the same frame
		case 1: teamBrainAffinityOffset = 0; break;
	}
	// Initialize superclass fields
	const_cast<AiBaseTeamBrain*>( this )->SetFrameAffinity( teamBrainAffinityModulo, teamBrainAffinityOffset );
}

void AiBaseTeamBrain::AddBot( Bot *bot ) {
	Debug( "new bot %s has been added\n", bot->Nick() );

	AcquireBotFrameAffinity( bot->EntNum() );
	// Call subtype method (if any)
	OnBotAdded( bot );
}

void AiBaseTeamBrain::RemoveBot( Bot *bot ) {
	Debug( "bot %s has been removed\n", bot->Nick() );

	ReleaseBotFrameAffinity( bot->EntNum() );
	// Call subtype method (if any)
	OnBotRemoved( bot );
}

void AiBaseTeamBrain::AcquireBotFrameAffinity( int entNum ) {
	// Always specify offset as zero for affinity module that = 1 (any % 1 == 0)
	if( AffinityModulo() == 1 ) {
		SetBotFrameAffinity( entNum, AffinityModulo(), 0 );
		return;
	}

	if( GS_TeamBasedGametype() ) {
		unsigned modulo = AffinityModulo(); // Precompute
		static_assert( MAX_AFFINITY_OFFSET == 4, "Only two teams are supported" );
		switch( modulo ) {
			case 4:
				// If the think cycle consist of 4 frames:
				// the Alpha team brain thinks on frame 0, bots of team Alpha think on frame 1,
				// the Beta team brain thinks on frame 2, bots of team Beta think on frame 3
				SetBotFrameAffinity( entNum, modulo, 2 * ( (unsigned)team - TEAM_ALPHA ) + 1 );
				break;
			case 3:
				// If the think cycle consist of 3 frames:
				// team brains think on frame 0, bots of team Alpha think on frame 1, bots of team Beta think on frame 2
				SetBotFrameAffinity( entNum, modulo, 1 + (unsigned)team - TEAM_ALPHA );
				break;
			case 2:
				// If the think cycle consist of 2 frames:
				// the Alpha team brain and team Alpha bot brains think on frame 0,
				// the Beta team brain an team Beta bot brains think on frame 1
				SetBotFrameAffinity( entNum, modulo, (unsigned)team - TEAM_ALPHA );
				break;
		}
	} else {
		// Select less used offset (thus, less loaded frames)
		unsigned chosenOffset = 0;
		for( unsigned i = 1; i < MAX_AFFINITY_OFFSET; ++i ) {
			if( affinityOffsetsInUse[chosenOffset] > affinityOffsetsInUse[i] ) {
				chosenOffset = i;
			}
		}
		affinityOffsetsInUse[chosenOffset]++;
		SetBotFrameAffinity( entNum, AffinityModulo(), chosenOffset );
	}
}

void AiBaseTeamBrain::ReleaseBotFrameAffinity( int entNum ) {
	unsigned offset = botAffinityOffsets[entNum];
	botAffinityOffsets[entNum] = 0;
	botAffinityModulo[entNum] = 0;
	affinityOffsetsInUse[offset]--;
}

void AiBaseTeamBrain::SetBotFrameAffinity( int entNum, unsigned modulo, unsigned offset ) {
	botAffinityModulo[entNum] = (unsigned char)modulo;
	botAffinityOffsets[entNum] = (unsigned char)offset;
	game.edicts[entNum].ai->botRef->SetFrameAffinity( modulo, offset );
}

void AiBaseTeamBrain::OnGametypeChanged( const char *gametype ) {
	// First, unregister all current brains (if any)
	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; ++team )
		UnregisterTeamBrain( team );

	if( GS_TeamBasedGametype() ) {
		for( int team = TEAM_ALPHA; team < GS_MAX_TEAMS; ++team ) {
			RegisterTeamBrain( team, InstantiateTeamBrain( team, gametype ) );
		}
	} else {
		RegisterTeamBrain( TEAM_PLAYERS, InstantiateTeamBrain( TEAM_PLAYERS, gametype ) );
	}
}

void AiBaseTeamBrain::RegisterTeamBrain( int team, AiBaseTeamBrain *brain ) {
	if( teamBrains[team - 1] ) {
		UnregisterTeamBrain( team );
	}
	// Set team brain pointer
	teamBrains[team - 1] = brain;
	// Use address of a static array cell for the brain pointer as a tag
	uint64_t tag = (uint64_t)( teamBrains + team - 1 );
	// Capture brain pointer (a stack variable) by value!
	AiShutdownHooksHolder::Instance()->RegisterHook( tag, [ = ]
	{
		brain->~AiBaseTeamBrain();
		G_Free( brain );
	} );
}

void AiBaseTeamBrain::UnregisterTeamBrain( int team ) {
	if( !teamBrains[team - 1] ) {
		return;
	}

	AiBaseTeamBrain *brainToDelete = teamBrains[team - 1];
	// Reset team brain pointer
	teamBrains[team - 1] = nullptr;
	// Destruct the brain
	brainToDelete->~AiBaseTeamBrain();
	// Free brain memory
	G_Free( brainToDelete );
	// Use address of a static array cell for the brain pointer as a tag
	uint64_t tag = (uint64_t)( teamBrains + team - 1 );
	AiShutdownHooksHolder::Instance()->UnregisterHook( tag );
}

AiBaseTeamBrain *AiBaseTeamBrain::InstantiateTeamBrain( int team, const char *gametype ) {
	// Delegate construction to AiSquadBasedTeamBrain
	if( GS_TeamBasedGametype() && !GS_InvidualGameType() ) {
		return AiSquadBasedTeamBrain::InstantiateTeamBrain( team, gametype );
	}

	void *mem = G_Malloc( sizeof( AiBaseTeamBrain ) );
	return new(mem)AiBaseTeamBrain( team );
}
