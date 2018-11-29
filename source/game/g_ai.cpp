#include "g_local.h"
#include "../gameshared/gs_public.h"

static struct {
	const char * name;
	const char * model;
} botCharacters[] = {
	{ "vic", "oldvic" },
	{ "crizis", "oldvic" },
	{ "jal", "oldvic" },

	{ "MWAGA", "bigvic" },

	{ "Triangel", "monada" },

	{ "Perrina", "silverclaw" },

	{ "__mute__", "padpork" },
	{ "Slice*>", "padpork" },
};

static constexpr int BOT_CHARACTERS_COUNT = sizeof( botCharacters ) / sizeof( botCharacters[0] );

static void CreateUserInfo( char * buffer, size_t bufferSize ) {
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

static edict_t * ConnectFakeClient() {
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

void AI_InitLevel() { }
void AI_Shutdown() { }
void AI_RemoveBots() { }

void AI_CommonFrame() { }

void AI_Respawn( edict_t * ent ) {
	VectorClear( ent->r.client->ps.pmove.delta_angles );
	ent->r.client->level.last_activity = level.time;
}

void AI_SpawnBot( const char * teamName ) {
	if( level.spawnedTimeStamp + 5000 > game.realtime || !level.canSpawnEntities ) {
		return;
	}

	if( edict_t * ent = ConnectFakeClient() ) {
		// init this bot
		ent->think = NULL;
		ent->nextThink = level.time + 1;
		ent->classname = "bot";
		ent->die = player_die;

		AI_Respawn( ent );

		int team = GS_Teams_TeamFromName( teamName );
		if( team != -1 && team > TEAM_PLAYERS ) {
			// Join specified team immediately
			G_Teams_JoinTeam( ent, team );
		} else {
			// stay as spectator, give random time for joining
			ent->nextThink = level.time + 500 + (unsigned)( random() * 2000 );
		}

		game.numBots++;
	}
}

void AI_RemoveBot( const char * name ) {
	// Do not iterate over the linked list of bots since it is implicitly modified by these calls
	for( edict_t * ent = game.edicts + gs.maxclients; PLAYERNUM( ent ) >= 0; ent-- ) {
		if( !Q_stricmp( ent->r.client->netname, name ) ) {
			trap_DropClient( ent, DROP_TYPE_GENERAL, nullptr );
			game.numBots--;
			return;
		}
	}
	G_Printf( "BOT: %s not found\n", name );
}

void AI_RegisterEvent( edict_t * ent, int event, int parm ) {
}

void AI_TouchedEntity( edict_t * self, edict_t * ent ) {
}

static void AI_SpecThink( edict_t * self ) {
	self->nextThink = level.time + 100;

	if( !level.canSpawnEntities )
		return;

	if( self->r.client->team == TEAM_SPECTATOR ) {
		// try to join a team
		// note that G_Teams_JoinAnyTeam is quite slow so only call it per frame
		if( !self->r.client->queueTimeStamp && self == level.think_client_entity ) {
			G_Teams_JoinAnyTeam( self, false );
		}

		if( self->r.client->team == TEAM_SPECTATOR ) { // couldn't join, delay the next think
			self->nextThink = level.time + 100;
		} else {
			self->nextThink = level.time + 1;
		}
		return;
	}

	usercmd_t ucmd;
        memset( &ucmd, 0, sizeof( usercmd_t ) );

        // set approximate ping and show values
        ucmd.serverTimeStamp = game.serverTime;
        ucmd.msec = (uint8_t)game.frametime;

        ClientThink( self, &ucmd, 0 );
}

static void AI_GameThink( edict_t * self ) {
	if( GS_MatchState() <= MATCH_STATE_WARMUP ) {
                G_Match_Ready( self );
        }

	usercmd_t ucmd;
	memset( &ucmd, 0, sizeof( usercmd_t ) );

	// set up for pmove
	for( int i = 0; i < 3; i++ )
		ucmd.angles[i] = (short)ANGLE2SHORT( self->s.angles[i] ) - self->r.client->ps.pmove.delta_angles[i];

	VectorSet( self->r.client->ps.pmove.delta_angles, 0, 0, 0 );

	// set approximate ping and show values
	ucmd.msec = (uint8_t)game.frametime;
	ucmd.serverTimeStamp = game.serverTime;

	ClientThink( self, &ucmd, 0 );
	self->nextThink = level.time + 1;
}

void AI_Think( edict_t * self ) {
	if( G_ISGHOSTING( self ) ) {
		AI_SpecThink( self );
	}
	else {
		AI_GameThink( self );
	}
}
