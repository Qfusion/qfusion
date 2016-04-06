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

#include "bot.h"
#include "aas.h"
#include "../../gameshared/gs_public.h"

//===============================================================
//
//				BOT SPAWN
//
//===============================================================

// JALFIXME : we only have 1 skin now, so I use invalid names so the randomizer check works
// and force the default one later on
static const char * const LocalBotSkins[] =
{
	"bigvic/default",
	"bigvic/default2",
	"bigvic/default3",
	"bigvic/default4",
	"bigvic/default5",

	"monada/default",
	"monada/default1",
	"monada/default2",
	"monada/default3",
	"monada/default4",

	"silverclaw/default",
	"silverclaw/default1",
	"silverclaw/default2",
	"silverclaw/default3",
	"silverclaw/default4",

	"padpork/default",
	"padpork/default1",
	"padpork/default2",
	"padpork/default3",
	"padpork/default4",

	"bobot/default",
	"bobot/default1",
	"bobot/default2",
	"bobot/default3",
	"bobot/default4",

	NULL
};

static const char * const LocalBotNames[] =
{
	"Viciious",
	"Sid",
	"Pervert",
	"Sick",
	"Punk",

	"Black Sis",
	"Monada",
	"Afrodita",
	"Goddess",
	"Athena",

	"Silver",
	"Cathy",
	"MishiMishi",
	"Lobita",
	"SisterClaw",

	"Padpork",
	"Jason",
	"Lord Hog",
	"Porkalator",
	"Babe",

	"YYZ2112",
	"01011001",
	"Sector",
	"%APPDATA%",
	"P.E.#1",

	NULL
};

#define BOT_NUMCHARACTERS 13
//------------------------------------------------------------


typedef struct
{
	char bot_model[MAX_INFO_STRING];
	char bot_skin[MAX_INFO_STRING];
	char bot_name[MAX_NAME_BYTES];
} localbotskin_t;


//==========================================
// BOT_GetUnusedSkin
// Retrieve a random unused skin & name
//==========================================
static bool BOT_GetUnusedSkin( char *bot_model, char *bot_skin, char *bot_name )
{
	bool inuse;
	int skinnumber;
	char *model, *skin;
	char scratch[MAX_INFO_STRING];
	int i, freeskins;
	edict_t	*ent;
	localbotskin_t *botskins;
	localbotskin_t *localbotskin;

	//count the unused skins, and make sure there is at least 1 of them
	skinnumber = freeskins = 0;
	while( LocalBotSkins[skinnumber] != NULL )
	{
		inuse = false;
		for( i = 0, ent = game.edicts + 1; i < gs.maxclients; i++, ent++ )
		{
			if( !( ent->r.svflags & SVF_FAKECLIENT ) || !ent->r.client )
				continue;

			model = Info_ValueForKey( ent->r.client->userinfo, "model" );
			skin = Info_ValueForKey( ent->r.client->userinfo, "skinl" );

			if( model && skin )
			{
				Q_snprintfz( scratch, sizeof( scratch ), "%s/%s", model, skin );

				if( !Q_stricmp( scratch, LocalBotSkins[skinnumber] ) )
				{
					inuse = true;
					break;
				}
			}
		}
		if( inuse == false )
			freeskins++;

		skinnumber++;
	}

	//fallback to old method
	if( !freeskins )
		return false;

	//assign tmp memory for storing unused skins
	botskins = (localbotskin_t *)G_Malloc( sizeof( localbotskin_t ) * freeskins );

	//create a list of unused skins
	skinnumber = freeskins = 0;
	while( LocalBotSkins[skinnumber] != NULL )
	{
		inuse = false;
		for( i = 0, ent = game.edicts + 1; i < gs.maxclients; i++, ent++ )
		{
			if( !( ent->r.svflags & SVF_FAKECLIENT ) || !ent->r.client )
				continue;

			model = Info_ValueForKey( ent->r.client->userinfo, "model" );
			skin = Info_ValueForKey( ent->r.client->userinfo, "skinl" );

			if( model && skin )
			{
				Q_snprintfz( scratch, sizeof( scratch ), "%s/%s", model, skin );

				if( !Q_stricmp( scratch, LocalBotSkins[skinnumber] ) )
				{
					inuse = true;
					break;
				}
			}
		}
		//store and advance
		if( inuse == false )
		{
			const char *p;
			localbotskin = botskins + freeskins;

			p = strchr( LocalBotSkins[skinnumber], '/' );
			if( !strlen( p ) )
				continue;
			p++;

			Q_strncpyz( localbotskin->bot_model, LocalBotSkins[skinnumber], strlen( LocalBotSkins[skinnumber] ) - strlen( p ) );
			Q_strncpyz( localbotskin->bot_skin, p, sizeof( localbotskin->bot_skin ) );
			Q_strncpyz( localbotskin->bot_name, LocalBotNames[skinnumber], sizeof( localbotskin->bot_name ) );

			freeskins++;
		}

		skinnumber++;
	}

	//now get a random skin from the list
	skinnumber = (int)( random()*freeskins );
	localbotskin = botskins + skinnumber;
	Q_strncpyz( bot_model, localbotskin->bot_model, sizeof( localbotskin->bot_model ) );
	Q_strncpyz( bot_skin, localbotskin->bot_skin, sizeof( localbotskin->bot_skin ) );
	Q_strncpyz( bot_name, localbotskin->bot_name, sizeof( localbotskin->bot_name ) );

	G_Free( botskins );

	return true;
}


//==========================================
// BOT_CreateUserinfo
// Creates UserInfo string to connect with
//==========================================
static void BOT_CreateUserinfo( char *userinfo, size_t userinfo_size )
{
	char bot_skin[MAX_INFO_STRING];
	char bot_name[MAX_NAME_BYTES];
	char bot_model[MAX_INFO_STRING];

	//jalfixme: we have only one skin yet

	//GetUnusedSkin doesn't repeat already used skins/names
	if( !BOT_GetUnusedSkin( bot_model, bot_skin, bot_name ) )
	{
		float r;
		int i, botcount = 0;
		edict_t	*ent;

		//count spawned bots for the names
		for( i = 0, ent = game.edicts + 1; i < gs.maxclients; i++, ent++ )
		{
			if( !ent->r.inuse || !ent->ai ) continue;
			if( ent->r.svflags & SVF_FAKECLIENT && AI_GetType( ent->ai ) == AI_ISBOT )
				botcount++;
		}

		// Set the name for the bot.
		Q_snprintfz( bot_name, sizeof( bot_name ), "Bot%d", botcount+1 );

		// randomly choose skin
		r = random();
		if( r > 0.8f )
			Q_snprintfz( bot_model, sizeof( bot_model ), "bigvic" );
		else if( r > 0.6f )
			Q_snprintfz( bot_model, sizeof( bot_model ), "padpork" );
		else if( r > 0.4f )
			Q_snprintfz( bot_model, sizeof( bot_model ), "silverclaw" );
		else if( r > 0.2f )
			Q_snprintfz( bot_model, sizeof( bot_model ), "bobot" );
		else
			Q_snprintfz( bot_model, sizeof( bot_model ), "monada" );

		Q_snprintfz( bot_skin, sizeof( bot_skin ), "default" );
	}

	//Q_strncpyz( bot_name, bot_personalities[bot_pers].name, sizeof( bot_name ) );

	// initialize userinfo
	memset( userinfo, 0, userinfo_size );

	// add bot's name/skin/hand to userinfo
	Info_SetValueForKey( userinfo, "name", bot_name );
	Info_SetValueForKey( userinfo, "model", bot_model );
	//Info_SetValueForKey( userinfo, "skin", bot_skin );
	Info_SetValueForKey( userinfo, "skin", "default" ); // JALFIXME
	Info_SetValueForKey( userinfo, "hand", va( "%i", (int)( random()*2.5 ) ) );
	Info_SetValueForKey( userinfo, "color", va( "%i %i %i", (uint8_t)( random()*255 ), (uint8_t)( random()*255 ), (uint8_t)( random()*255 ) ) );
}

static void BOT_pain( edict_t *self, edict_t *other, float kick, int damage )
{
	if( other->r.client )
	{
		self->ai->botRef->Pain(other, kick, damage);
	}
}

//==========================================
// BOT_Respawn
// Set up bot for Spawn. Called at first spawn & each respawn
//==========================================
void BOT_Respawn( edict_t *self )
{
	if( AI_GetType( self->ai ) != AI_ISBOT )
		return;

	self->pain = BOT_pain;

	self->ai->botRef->OnRespawn();

	VectorClear( self->r.client->ps.pmove.delta_angles );
	self->r.client->level.last_activity = level.time;

	self->ai->aiRef->ResetNavigation();
}

static float MakeRandomBotSkillByServerSkillLevel()
{
	float skillLevel = trap_Cvar_Value("sv_skilllevel"); // 0 = easy, 2 = hard
	skillLevel += random(); // so we have a float between 0 and 3 meaning the server skill
	skillLevel /= 3.0f;
	if (skillLevel < 0.1f)
		skillLevel = 0.1f;
	else if (skillLevel > 1.0f) // Won't happen?
		skillLevel = 1.0f;
	return skillLevel;
}

//==========================================
// BOT_DoSpawnBot
// Spawn the bot
//==========================================
static void BOT_DoSpawnBot( void )
{
	char userinfo[MAX_INFO_STRING];
	int entNum;
	edict_t	*ent;
	static char fakeSocketType[] = "loopback";
	static char fakeIP[] = "127.0.0.1";

	if (!AAS_Initialized())
	{
		Com_Printf( "AI: Can't spawn bots without a valid navigation file\n" );
		if( g_numbots->integer ) 
			trap_Cvar_Set( "g_numbots", "0" );
		return;
	}

	BOT_CreateUserinfo( userinfo, sizeof( userinfo ) );

	entNum = trap_FakeClientConnect( userinfo, fakeSocketType, fakeIP );
	if( entNum < 1 )
	{          // 0 is worldspawn, -1 is error
		Com_Printf( "AI: Can't spawn the fake client\n" );
		return;
	}

	ent = &game.edicts[entNum];

	// We have to determine skill level early, since G_SpawnAI calls Bot constructor that requires it as a constant
	float skillLevel = MakeRandomBotSkillByServerSkillLevel();

	G_SpawnAI( ent, skillLevel );

	//init this bot
	ent->think = NULL;
	ent->nextThink = level.time + 1;
	ent->ai->type = AI_ISBOT;
	ent->classname = "bot";
	ent->yaw_speed = AI_DEFAULT_YAW_SPEED;
	ent->die = player_die;
	ent->yaw_speed -= 20 * (1.0f - skillLevel);

	G_Printf("%s skill %i\n", ent->r.client->netname, (int) (skillLevel * 100));

	//set up for Spawn
	BOT_Respawn( ent );

	//stay as spectator, give random time for joining
	ent->nextThink = level.time + random() * 8000;
}

//==========================================
// BOT_SpawnerThink
// Call the real bot spawning function
//==========================================
static void BOT_SpawnerThink( edict_t *spawner )
{
	BOT_DoSpawnBot();
	G_FreeEdict( spawner );
}

//==========================================
// BOT_SpawnBot
// Used Spawn the bot
//==========================================
void BOT_SpawnBot( const char *team_name )
{
	edict_t *spawner;
	int team;

	if( level.spawnedTimeStamp + 5000 > game.realtime || !level.canSpawnEntities )
		return;

	if(!AAS_Initialized())
	{
		Com_Printf( "AI: Can't spawn bots without a valid navigation file\n" );
		if( g_numbots->integer ) 
			trap_Cvar_Set( "g_numbots", "0" );
		return;
	}

	// create a entity which will call the bot spawn
	spawner = G_Spawn();
	spawner->think = BOT_SpawnerThink;

	team = GS_Teams_TeamFromName( team_name );
	if( team != -1 )
		spawner->s.team = team;

	spawner->nextThink = level.time + random() * 3000;
	spawner->movetype = MOVETYPE_NONE;
	spawner->r.solid = SOLID_NOT;
	spawner->r.svflags |= SVF_NOCLIENT;
	GClip_LinkEntity( spawner );

	game.numBots++;
}

//==========================================
//	BOT_RemoveBot
//	Remove a bot by name or all bots
//==========================================
void BOT_RemoveBot( const char *name )
{
	int i;
	edict_t *ent;
	// If a named bot should be removed
	if (Q_stricmp(name, "all"))
	{
		for (i = 0, ent = game.edicts + 1; i < gs.maxclients; i++, ent++)
		{
			if (!Q_stricmp(ent->r.client->netname, name))
			{
				trap_DropClient(ent, DROP_TYPE_GENERAL, nullptr);
				break;
			}
		}
		if (i == gs.maxclients)
			G_Printf("BOT: %s not found\n", name);
	}
	else
	{
		for (i = 0, ent = game.edicts + 1; i < gs.maxclients; i++, ent++)
		{
			if (!ent->r.inuse || AI_GetType(ent->ai) != AI_ISBOT)
				continue;

			trap_DropClient(ent, DROP_TYPE_GENERAL, nullptr);
		}
	}
}
