#pragma once

void AI_InitLevel();
void AI_Shutdown();
void AI_RemoveBots();

void AI_CommonFrame();

void AI_Respawn( edict_t * ent );

void AI_SpawnBot( const char * teamName );

void AI_RemoveBot( const char * name );

void AI_RegisterEvent( edict_t * ent, int event, int parm );

void AI_TouchedEntity( edict_t * self, edict_t * ent );

void AI_Think( edict_t * self );
