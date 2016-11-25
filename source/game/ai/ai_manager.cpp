#include "ai_manager.h"
#include "ai_base_brain.h"
#include "ai_base_team_brain.h"
#include "ai_shutdown_hooks_holder.h"
#include "bot.h"

// Class static variable declaration
AiManager *AiManager::instance = nullptr;

// Actual instance location in memory
static StaticVector<AiManager, 1> instanceHolder;

void AiManager::OnGametypeChanged( const char *gametype ) {
	// Currently, gametype brain is shared for all gametypes
	// If gametype brain differs for different gametypes,
	// delete previous instance and create a suitable new instance.

	// This means that gametype has been set up for first time.
	if( instanceHolder.empty() ) {
		instanceHolder.emplace_back( AiManager() );
		AiShutdownHooksHolder::Instance()->RegisterHook([&] { instanceHolder.clear(); } );
		instance = &instanceHolder.front();
	}

	AiBaseTeamBrain::OnGametypeChanged( gametype );
}

void AiManager::ClearGoals( const NavEntity *canceledGoal, const Ai *goalGrabber ) {
	if( !canceledGoal ) {
		return;
	}

	// find all bots which have this node as goal and tell them their goal is reached
	for( ai_handle_t *ai = last; ai; ai = ai->prev ) {
		if( ai->aiRef == goalGrabber ) {
			continue;
		}

		AiBaseBrain *aiBrain = ai->aiRef->aiBaseBrain;
		if( aiBrain->longTermGoal && aiBrain->longTermGoal->IsBasedOnNavEntity( canceledGoal ) ) {
			aiBrain->ClearAllGoals();
		} else if( aiBrain->shortTermGoal && aiBrain->shortTermGoal->IsBasedOnNavEntity( canceledGoal ) ) {
			aiBrain->ClearAllGoals();
		}
	}
}

void AiManager::ClearGoals( const Goal *canceledGoal, const Ai *goalGrabber ) {
	if( !canceledGoal ) {
		return;
	}

	// find all bots which have this node as goal and tell them their goal is reached
	for( ai_handle_t *ai = last; ai; ai = ai->prev ) {
		if( ai->type == AI_INACTIVE ) {
			continue;
		}

		if( ai->aiRef == goalGrabber ) {
			continue;
		}

		AiBaseBrain *aiBrain = ai->aiRef->aiBaseBrain;
		if( aiBrain->longTermGoal == canceledGoal ) {
			aiBrain->ClearAllGoals();
		} else if( aiBrain->shortTermGoal == canceledGoal ) {
			aiBrain->ClearAllGoals();
		}
	}
}

void AiManager::NavEntityReached( const edict_t *ent ) {
	if( !last ) {
		return;
	}

	// find all bots which have this node as goal and tell them their goal is reached
	for( ai_handle_t *ai = last; ai; ai = ai->prev ) {
		if( ai->type == AI_INACTIVE ) {
			continue;
		}

		AiBaseBrain *aiBrain = ai->aiRef->aiBaseBrain;
		if( aiBrain->specialGoal && aiBrain->specialGoal->IsBasedOnEntity( ent ) ) {
			aiBrain->OnSpecialGoalReached();
		} else if( aiBrain->longTermGoal && aiBrain->longTermGoal->IsBasedOnEntity( ent ) ) {
			aiBrain->OnLongTermGoalReached();
		} else if( aiBrain->shortTermGoal && aiBrain->shortTermGoal->IsBasedOnEntity( ent ) ) {
			aiBrain->OnShortTermGoalReached();
		}
	}
}

void AiManager::OnBotJoinedTeam( edict_t *ent, int team ) {
	const int entNum = ENTNUM( ent );
	const int oldTeam = teams[entNum];
	if( oldTeam != team ) {
		if( oldTeam != TEAM_SPECTATOR ) {
			AiBaseTeamBrain::GetBrainForTeam( oldTeam )->RemoveBot( ent->ai->botRef );
		}
		if( team != TEAM_SPECTATOR ) {
			AiBaseTeamBrain::GetBrainForTeam( team )->AddBot( ent->ai->botRef );
		}
		teams[entNum] = team;
	}
}

void AiManager::OnBotDropped( edict_t *ent ) {
	const int entNum = ENTNUM( ent );
	const int oldTeam = teams[entNum];
	if( oldTeam != TEAM_SPECTATOR ) {
		AiBaseTeamBrain::GetBrainForTeam( oldTeam )->RemoveBot( ent->ai->botRef );
	}
	teams[entNum] = TEAM_SPECTATOR;
}

void AiManager::LinkAi( ai_handle_t *ai ) {
	ai->next = nullptr;
	if( last != nullptr ) {
		ai->prev = last;
		last->next = ai;
	} else {
		ai->prev = nullptr;
	}
	last = ai;
}

void AiManager::UnlinkAi( ai_handle_t *ai ) {
	ai_handle_t *next = ai->next;
	ai_handle_t *prev = ai->prev;

	ai->next = nullptr;
	ai->prev = nullptr;

	// If the cell is not the last in chain
	if( next ) {
		next->prev = prev;
		if( prev ) {
			prev->next = next;
		}
	} else {
		// A cell before the last in chain should become the last one
		if( prev ) {
			prev->next = nullptr;
			last = prev;
		} else {
			last = nullptr;
		}
	}
}

static struct { const char *name; const char *model; } botCharacters[] = {
	{ "Viciious",   "bigvic" },
	{ "Sid",        "bigvic" },
	{ "Pervert",    "bigvic" },
	{ "Sick",       "bigvic" },
	{ "Punk",       "bigvic" },

	{ "Black Sis",  "monada" },
	{ "Monada",     "monada" },
	{ "Afrodita",   "monada" },
	{ "Goddess",    "monada" },
	{ "Athena",     "monada" },

	{ "Silver",     "silverclas" },
	{ "Cathy",      "silverclaw" },
	{ "MishiMishi", "silverclaw" },
	{ "Lobita",     "silverclaw" },
	{ "SisterClaw", "silverclaw" },

	{ "Padpork",    "padpork" },
	{ "Jason",      "padpork" },
	{ "Lord Hog",   "padpork" },
	{ "Porkalator", "padpork" },
	{ "Babe",       "padpork" },

	{ "YYZ2112",    "bobot" },
	{ "01011001",   "bobot" },
	{ "Sector",     "bobot" },
	{ "%APPDATA%",  "bobot" },
	{ "P.E.#1",     "bobot" },
};

static constexpr int BOT_CHARACTERS_COUNT = sizeof( botCharacters ) / sizeof( botCharacters[0] );

void AiManager::CreateUserInfo( char *buffer, size_t bufferSize ) {
	// Try to avoid bad distribution, otherwise some bots are selected too often. Weights are prime numbers
	int characterIndex = ( (int)( 3 * random() + 11 * random() +  97 * random() + 997 * random() ) ) % BOT_CHARACTERS_COUNT;

	memset( buffer, 0, bufferSize );

	Info_SetValueForKey( buffer, "name", botCharacters[characterIndex].name );
	Info_SetValueForKey( buffer, "model", botCharacters[characterIndex].model );
	Info_SetValueForKey( buffer, "skin", "default" );
	Info_SetValueForKey( buffer, "hand", va( "%i", (int)( random() * 2.5 ) ) );
	const char *color = va( "%i %i %i", (uint8_t)( random() * 255 ), (uint8_t)( random() * 255 ), (uint8_t)( random() * 255 ) );
	Info_SetValueForKey( buffer, "color", color );
}

edict_t * AiManager::ConnectFakeClient() {
	char userInfo[MAX_INFO_STRING];
	static char fakeSocketType[] = "loopback";
	static char fakeIP[] = "127.0.0.1";
	CreateUserInfo( userInfo, sizeof( userInfo ) );
	int entNum = trap_FakeClientConnect( userInfo, fakeSocketType, fakeIP );
	if( entNum < 1 ) {
		Com_Printf( "AI: Can't spawn the fake client\n" );
		return nullptr;
	}
	return game.edicts + entNum;
}

void AiManager::RespawnBot( edict_t *self ) {
	if( AI_GetType( self->ai ) != AI_ISBOT ) {
		return;
	}

	self->ai->botRef->OnRespawn();

	VectorClear( self->r.client->ps.pmove.delta_angles );
	self->r.client->level.last_activity = level.time;
}

static float MakeRandomBotSkillByServerSkillLevel() {
	float skillLevel = trap_Cvar_Value( "sv_skilllevel" ); // 0 = easy, 2 = hard
	skillLevel += random(); // so we have a float between 0 and 3 meaning the server skill
	skillLevel /= 3.0f;
	if( skillLevel < 0.1f ) {
		skillLevel = 0.1f;
	} else if( skillLevel > 1.0f ) { // Won't happen?
		skillLevel = 1.0f;
	}
	return skillLevel;
}

static void BOT_JoinPlayers( edict_t *self ) {
	G_Teams_JoinAnyTeam( self, true );
	self->think = nullptr;
}

bool AiManager::CheckCanSpawnBots() {
	if( level.spawnedTimeStamp + 5000 > game.realtime || !level.canSpawnEntities ) {
		return false;
	}

	if( !AiAasWorld::Instance()->IsLoaded() ) {
		Com_Printf( "AI: Can't spawn bots without a valid navigation file\n" );
		if( g_numbots->integer ) {
			trap_Cvar_Set( "g_numbots", "0" );
		}

		return false;
	}

	return true;
}

void AiManager::SetupClientBot( edict_t *ent ) {
	// We have to determine skill level early, since G_SpawnAI calls Bot constructor that requires it as a constant
	float skillLevel = MakeRandomBotSkillByServerSkillLevel();

	// This also does an increment of game.numBots
	G_SpawnAI( ent, skillLevel );

	//init this bot
	ent->think = nullptr;
	ent->nextThink = level.time + 1;
	ent->ai->type = AI_ISBOT;
	ent->classname = "bot";
	ent->yaw_speed = AI_DEFAULT_YAW_SPEED;
	ent->die = player_die;
	ent->yaw_speed -= 20 * ( 1.0f - skillLevel );

	G_Printf( "%s skill %i\n", ent->r.client->netname, (int) ( skillLevel * 100 ) );
}

void AiManager::SetupBotTeam( edict_t *ent, const char *teamName ) {
	int team = GS_Teams_TeamFromName( teamName );
	if( team != -1 && team > TEAM_PLAYERS ) {
		// Join specified team immediately
		G_Teams_JoinTeam( ent, team );
	} else {
		//stay as spectator, give random time for joining
		ent->think = BOT_JoinPlayers;
		ent->nextThink = level.time + 500 + (unsigned)( random() * 2000 );
	}
}

void AiManager::SpawnBot( const char *teamName ) {
	if( CheckCanSpawnBots() ) {
		if( edict_t *ent = ConnectFakeClient() ) {
			SetupClientBot( ent );
			RespawnBot( ent );
			SetupBotTeam( ent, teamName );
			game.numBots++;
		}
	}
}

void AiManager::RemoveBot( const char *name ) {
	// Do not iterate over the linked list of bots since it is implicitly modified by these calls
	for( edict_t *ent = game.edicts + gs.maxclients; PLAYERNUM( ent ) >= 0; ent-- ) {
		if( !Q_stricmp( ent->r.client->netname, name ) ) {
			trap_DropClient( ent, DROP_TYPE_GENERAL, nullptr );
			OnBotDropped( ent );
			G_FreeAI( ent );
			game.numBots--;
			return;
		}
	}
	G_Printf( "BOT: %s not found\n", name );
}

void AiManager::RemoveBots() {
	// Do not iterate over the linked list of bots since it is implicitly modified by these calls
	for( edict_t *ent = game.edicts + gs.maxclients; PLAYERNUM( ent ) >= 0; ent-- ) {
		if( !ent->r.inuse || AI_GetType( ent->ai ) != AI_ISBOT ) {
			continue;
		}

		trap_DropClient( ent, DROP_TYPE_GENERAL, nullptr );
		OnBotDropped( ent );
		G_FreeAI( ent );
		game.numBots--;
	}
}

void AiManager::Frame() {
	if( !GS_TeamBasedGametype() ) {
		AiBaseTeamBrain::GetBrainForTeam( TEAM_PLAYERS )->Update();
		return;
	}

	for( int team = TEAM_ALPHA; team < GS_MAX_TEAMS; ++team ) {
		AiBaseTeamBrain::GetBrainForTeam( team )->Update();
	}
}
