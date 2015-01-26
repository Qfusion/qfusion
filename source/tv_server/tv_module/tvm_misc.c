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

*/

#include "tvm_local.h"

#include "tvm_misc.h"

/*
* TVM_FindLocal
* 
* Searches all active entities for the next one that holds
* the matching string at fieldofs (use the FOFS() macro) in the structure.
* 
* Searches beginning at the edict after from, or the beginning if NULL
* NULL will be returned if the end of the list is reached.
*/
edict_t *TVM_FindLocal( tvm_relay_t *relay, const edict_t *start, size_t fieldofs, const char *match )
{
	char *s;
	edict_t	*from;

	if( !start )
		from = &relay->local_edicts[0];
	else
		from = &relay->local_edicts[ENTNUM( start ) + 1];

	for(; from < &relay->local_edicts[relay->local_numentities]; from++ )
	{
		if( !from->r.inuse )
			continue;
		s = *(char **) ( (uint8_t *)from + fieldofs );
		if( !s )
			continue;
		if( !Q_stricmp( s, match ) )
			return from;
	}

	return NULL;
}

/*
* TVM_AllowDownload
*/
bool TVM_AllowDownload( tvm_relay_t *relay, edict_t *ent, const char *requestname, const char *uploadname )
{
	return false;
}

/*
* TVM_UpdateServerSettings
*/
static void TVM_UpdateServerSettings( tvm_relay_t *relay )
{
	relay->configStringsOverwritten[CS_TVSERVER] = true;
	trap_ConfigString( relay, CS_TVSERVER, "1" );
}

/*
* TVM_UpdateStatNums
*/
static void TVM_UpdateStatNums( tvm_relay_t *relay )
{
	const char *stats;

	stats = relay->configStrings[CS_STATNUMS];
	relay->stats.frags = atoi( COM_Parse( &stats ) );
	relay->stats.health = atoi( COM_Parse( &stats ) );
	relay->stats.last_killer = atoi( COM_Parse( &stats ) );

	clamp( relay->stats.frags, 0, PS_MAX_STATS-1 );
	clamp( relay->stats.health, 0, PS_MAX_STATS-1 );
	clamp( relay->stats.last_killer, 0, PS_MAX_STATS-1 );
}


/*
* TVM_UpdatePowerupFXNums
*/
static void TVM_UpdatePowerupFXNums( tvm_relay_t *relay )
{
	const char *effects;

	effects = relay->configStrings[CS_POWERUPEFFECTS];
	relay->effects.quad = atoi( COM_Parse( &effects ) );
	relay->effects.shell = atoi( COM_Parse( &effects ) );
	relay->effects.enemy_flag = atoi( COM_Parse( &effects ) );
	relay->effects.regen = atoi( COM_Parse( &effects ) );
}

/*
* TVM_PrintMsg_Template
* 
* NULL sends to all the message to all clients
*/
static void TVM_PrintMsg_Template( tvm_relay_t *relay, edict_t *ent, const char *tmplt, char *msg )
{
	char *p;

	assert( !ent || ( ent->local && ent->r.client ) );
	assert( msg );

	// double quotes are bad
	while( ( p = strchr( msg, '\"' ) ) != NULL )
		*p = '\'';

	trap_GameCmd( relay, ent ? PLAYERNUM( ent ) : -1, va( tmplt, msg ) );
}

/*
* TVM_PrintMsg
* 
* NULL sends to all the message to all clients
*/
void TVM_PrintMsg( tvm_relay_t *relay, edict_t *ent, const char *format, ... )
{
	char msg[1024];
	va_list	argptr;

	assert( !ent || ( ent->local && ent->r.client ) );
	assert( format );

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	TVM_PrintMsg_Template( relay, ent, "pr \"%s\"", msg );
}

/*
* TVM_CenterPrintMsg
* 
* NULL sends to all the message to all clients
*/
// MOVEME
void TVM_CenterPrintMsg( tvm_relay_t *relay, edict_t *ent, const char *format, ... )
{
	char msg[1024];
	va_list	argptr;

	assert( !ent || ( ent->local && ent->r.client ) );
	assert( format );

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	TVM_PrintMsg_Template( relay, ent, "cp \"%s\"", msg );
}

/*
* TVM_ConfigString
*/
bool TVM_ConfigString( tvm_relay_t *relay, int number, const char *value )
{
	assert( number >= 0 && number < MAX_CONFIGSTRINGS );
	assert( value && strlen( value ) < MAX_CONFIGSTRING_CHARS );

	if( number < 0 || number >= MAX_CONFIGSTRINGS )
		TVM_RelayError( relay, "TVM_ConfigString: Invalid number" );

	Q_strncpyz( relay->configStrings[number], value, sizeof( relay->configStrings[number] ) );

	switch( number )
	{
	case CS_TVSERVER:
		TVM_UpdateServerSettings( relay );
		break;
	case CS_STATNUMS:
		TVM_UpdateStatNums( relay );
		break;
	case CS_POWERUPEFFECTS:
		TVM_UpdatePowerupFXNums( relay );
		break;
	default:
		break;
	}

	return ( !relay->configStringsOverwritten[number] );
}

/*
* TVM_SetAudoTrack
*/
void TVM_SetAudoTrack( tvm_relay_t *relay, const char *track )
{
	if( !track ) {
		relay->configStringsOverwritten[CS_AUDIOTRACK] = false;
		track = relay->configStrings[CS_AUDIOTRACK];
	} else {
		relay->configStringsOverwritten[CS_AUDIOTRACK] = true;
	}

	trap_ConfigString( relay, CS_AUDIOTRACK, track ? track : "" );
}
