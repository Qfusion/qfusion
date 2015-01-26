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

#include "tvm_main.h"

#include "tvm_client.h"

tv_module_locals_t tvm;

cvar_t *developer;
cvar_t *tv_chasemode;

//======================================================================

#ifndef TV_MODULE_HARD_LINKED
// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

void Com_Printf( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}
#endif

//===================================================================

/*
* TVM_Printf
*/
void TVM_Printf( const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}

/*
* TVM_Error
*/
void TVM_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

/*
* TVM_RelayError
*/
void TVM_RelayError( tvm_relay_t *relay, const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	assert( relay );

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_RelayError( relay, msg );
}

//===================================================================

/*
* TVM_API
*/
int TVM_API( void )
{
	return TV_MODULE_API_VERSION;
}

/*
* TVM_InitRelay
*/
tvm_relay_t *TVM_InitRelay( relay_t *relay_server, unsigned int snapFrameTime, int playernum )
{
	tvm_relay_t *relay;
	int i;

	assert( playernum >= -1 && playernum < MAX_CLIENTS );

	TVM_Printf( "==== TVM_InitRelay ====\n" );

	relay = TVM_Malloc( relay_server, sizeof( tvm_relay_t ) );


	relay->server = relay_server;
	relay->playernum = playernum;
	relay->snapFrameTime = snapFrameTime;

	// initialize all entities for this game
	relay->maxentities = MAX_EDICTS;
	relay->maxclients = MAX_CLIENTS;
	relay->edicts = TVM_Malloc( relay_server, relay->maxentities * sizeof( relay->edicts[0] ) );
	relay->clients = TVM_Malloc( relay_server, relay->maxclients * sizeof( relay->clients[0] ) );
	relay->numentities = 0;

	// set relay
	for( i = 0; i < relay->maxentities; i++ )
		relay->edicts[i].relay = relay;

	trap_LocateEntities( relay, relay->edicts, sizeof( relay->edicts[0] ), relay->numentities,
		relay->maxentities );

	// initialize local entities
	relay->local_maxentities = MAX_EDICTS;
	relay->local_maxclients = tvm.maxclients;
	relay->local_edicts = TVM_Malloc( relay_server, relay->local_maxentities * sizeof( relay->local_edicts[0] ) );
	relay->local_clients = TVM_Malloc( relay_server, relay->local_maxclients * sizeof( relay->local_clients[0] ) );
	relay->local_numentities = relay->local_maxclients;

	// set relay and local
	for( i = 0; i < relay->local_maxentities; i++ )
	{
		relay->local_edicts[i].local = true;
		relay->local_edicts[i].relay = relay;
	}

	// set client fields on player ents
	for( i = 0; i < relay->local_maxclients; i++ )
		relay->local_edicts[i].r.client = relay->local_clients + i;

	trap_LocateLocalEntities( relay, relay->local_edicts, sizeof( relay->local_edicts[0] ), relay->local_numentities,
		relay->local_maxclients );

	return relay;
}

/*
* TVM_ShutdownRelay
*/
void TVM_ShutdownRelay( tvm_relay_t *relay )
{
	assert( relay );

	TVM_Printf( "==== TVM_ShutdownRelay ====\n" );

	TVM_Free( relay );
}

/*
* TVM_Init
* 
* This will be called when the dll is first loaded
*/
void TVM_Init( const char *game, unsigned int maxclients )
{
	TVM_Printf( "==== TVM_Init ====\n" );

	developer = trap_Cvar_Get( "developer", "0", 0 );

	// chase cam mode for spawning specs, 'carriers' by default
	tv_chasemode = trap_Cvar_Get( "tv_chasemode", "6", 0 );

	memset( &tvm, 0, sizeof( tvm ) );
	tvm.maxclients = maxclients;
}

/*
* TVM_Shutdown
*/
void TVM_Shutdown( void )
{
	TVM_Printf( "==== TVM_Shutdown ====\n" );
}
