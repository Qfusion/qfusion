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

//===============================================================
//
//				BOT SPAWN
//
//===============================================================

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

static constexpr int BOT_CHARACTERS_COUNT = sizeof(botCharacters) / sizeof(botCharacters[0]);

//==========================================
// BOT_CreateUserinfo
// Creates UserInfo string to connect with
//==========================================
static void BOT_CreateUserinfo( char *userinfo, size_t userinfo_size )
{
	// Try to avoid bad distribution, otherwise some bots are selected too often. Weights are prime numbers
	int characterIndex = ((int)(3 * random() + 11 * random() +  97 * random() + 997 * random())) % BOT_CHARACTERS_COUNT;

	memset(userinfo, 0, userinfo_size);

	Info_SetValueForKey(userinfo, "name", botCharacters[characterIndex].name);
	Info_SetValueForKey(userinfo, "model", botCharacters[characterIndex].model);
	Info_SetValueForKey(userinfo, "skin", "default");
	Info_SetValueForKey(userinfo, "hand", va( "%i", (int)( random()*2.5 ) ) );
	const char *color = va("%i %i %i", (uint8_t)(random()*255), (uint8_t)(random()*255), (uint8_t)(random()*255));
	Info_SetValueForKey(userinfo, "color", color);
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

	self->ai->botRef->OnRespawn();

	VectorClear( self->r.client->ps.pmove.delta_angles );
	self->r.client->level.last_activity = level.time;
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
// BOT_SpawnBot
// Used Spawn the bot
//==========================================
void BOT_SpawnBot( const char *team_name )
{
	if( level.spawnedTimeStamp + 5000 > game.realtime || !level.canSpawnEntities )
		return;

	if(!AAS_Initialized())
	{
		Com_Printf( "AI: Can't spawn bots without a valid navigation file\n" );
		if( g_numbots->integer ) 
			trap_Cvar_Set( "g_numbots", "0" );
		return;
	}

	char userinfo[MAX_INFO_STRING];
	static char fakeSocketType[] = "loopback";
	static char fakeIP[] = "127.0.0.1";
	BOT_CreateUserinfo( userinfo, sizeof( userinfo ) );

	int entNum = trap_FakeClientConnect( userinfo, fakeSocketType, fakeIP );
	if( entNum < 1 )
	{          // 0 is worldspawn, -1 is error
		Com_Printf( "AI: Can't spawn the fake client\n" );
		return;
	}

	edict_t *ent = &game.edicts[entNum];

	// We have to determine skill level early, since G_SpawnAI calls Bot constructor that requires it as a constant
	float skillLevel = MakeRandomBotSkillByServerSkillLevel();

	G_SpawnAI( ent, skillLevel );

	//init this bot
	ent->think = NULL;
	ent->nextThink = level.time + 1;
	ent->ai->type = AI_ISBOT;
	ent->classname = "bot";
	ent->yaw_speed = AI_DEFAULT_YAW_SPEED;
	ent->pain = BOT_pain;
	ent->die = player_die;
	ent->yaw_speed -= 20 * (1.0f - skillLevel);

	G_Printf("%s skill %i\n", ent->r.client->netname, (int) (skillLevel * 100));

	//set up for Spawn
	BOT_Respawn( ent );

	int team = GS_Teams_TeamFromName(team_name);
	if (team != -1 && team > TEAM_PLAYERS)
	{
		// Join specified team immediately
		G_Teams_JoinTeam(ent, team);
	}
	else
	{
		//stay as spectator, give random time for joining
		ent->nextThink = level.time + 500 + random() * 2000;
	}

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
