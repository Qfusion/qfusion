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

#include <time.h>       // just for dev

#include "server.h"
#include "../gameshared/q_shared.h"

#include "../matchmaker/mm_common.h"
#include "../matchmaker/mm_rating.h"
#include "../matchmaker/mm_query.h"

// interval between successive attempts to get match UUID from the mm
#define SV_MM_MATCH_UUID_FETCH_INTERVAL     20  // in seconds

/*
* private vars
*/
static bool sv_mm_initialized = false;
static int sv_mm_session;

// local session counter
static unsigned int sv_mm_localsession;
static int64_t sv_mm_last_heartbeat;
static bool sv_mm_logout_semaphore = false;

// flag for gamestate = game-on
static bool sv_mm_gameon = false;

static stat_query_t *sv_login_query = NULL;

static stat_query_api_t *sq_api = NULL;

static char sv_mm_match_uuid[37];
static unsigned sv_mm_next_match_uuid_fetch;
static stat_query_t *sv_mm_match_uuid_fetch_query;
static void (*sv_mm_match_uuid_callback_fn)( const char *uuid );

/*
* public vars
*/
cvar_t *sv_mm_authkey;
cvar_t *sv_mm_enable;
cvar_t *sv_mm_loginonly;
cvar_t *sv_mm_debug_reportbots;

/*
* prototypes
*/
static bool SV_MM_Login( void );
static void SV_MM_Logout( bool force );
static void SV_MM_GetMatchUUIDThink( void );

/*
* Utilities
*/
static client_t *SV_MM_ClientForSession( int session_id ) {
	int i;
	client_t *cl;

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
		// also ignore zombies?
		if( cl->state == CS_FREE ) {
			continue;
		}

		if( cl->mm_session == session_id ) {
			return cl;
		}
	}

	return NULL;
}

int SV_MM_GenerateLocalSession( void ) {
	unsigned int id;

	id = sv_mm_localsession;
	do {
		id = ( id + 1 ) & 0x7fffffff;
		if( !id ) {
			id = 1;
		}
	} while( SV_MM_ClientForSession( -(int)id ) != NULL );

	sv_mm_localsession = id;
	return -(int)sv_mm_localsession;
}

//======================================
//		HTTP REQUESTS
//======================================

struct stat_query_s *SV_MM_CreateQuery( const char *iface, const char *url, bool get ) {
	return sq_api->CreateQuery( sv_ip->string, url, false );
}

void SV_MM_SendQuery( struct stat_query_s *query ) {
	// add our session id
	sq_api->SetField( query, "ssession", va( "%d", sv_mm_session ) );
	sq_api->Send( query );
}

// TODO: instead of this, factor ClientDisconnect to game module which can flag
// the gamestate in that function
void SV_MM_GameState( bool gameon ) {
	sv_mm_gameon = gameon;
}

static void sv_mm_heartbeat_done( stat_query_t *query, bool success, void *customp ) {
}

void SV_MM_Heartbeat( void ) {
	stat_query_t *query;

	if( !sv_mm_initialized || !sv_mm_session ) {
		return;
	}

	// push a request
	query = sq_api->CreateQuery( sv_ip->string, "shb", false );
	if( query == NULL ) {
		return;
	}

	// servers own session (TODO: put this to a cookie or smth)
	sq_api->SetField( query, "ssession", va( "%d", sv_mm_session ) );

	// redundant atm
	sq_api->SetCallback( query, sv_mm_heartbeat_done, NULL );
	sq_api->Send( query );
}

/*
* sv_mm_clientdisconnect_done
* This only exists so that we are sure the message got through
*/
static void sv_mm_clientdisconnect_done( stat_query_t *query, bool success, void *customp ) {
	intptr_t p = (uintptr_t)customp;

	if( success == true ) {
		Com_Printf( "SV_MM_ClientDisconnect: Acknowledged %i\n", (int)p );
	} else {
		Com_Printf( "SV_MM_ClientDisconnect: Error\n" );
	}
}

void SV_MM_ClientDisconnect( client_t *client ) {
	stat_query_t *query;

	if( !sv_mm_initialized || !sv_mm_session ) {
		return;
	}

	// do we need to tell about anonymous clients?
	if( client->mm_session <= 0 ) {
		return;
	}

	// push a request
	query = sq_api->CreateQuery( sv_ip->string, "scd", false );
	if( query == NULL ) {
		return;
	}

	// servers own session (TODO: put this to a cookie or smth)
	sq_api->SetField( query, "ssession", va( "%d", sv_mm_session ) );

	// clients session
	sq_api->SetField( query, "csession", va( "%d", client->mm_session ) );
	sq_api->SetField( query, "gameon", sv_mm_gameon == true ? "1" : "0" );

	sq_api->SetCallback( query, sv_mm_clientdisconnect_done, (void *)( (intptr_t)client->mm_session ) );
	sq_api->Send( query );
}

/*
* sv_clientconnect_done
* callback for clientconnect POST request
*/
static void sv_mm_clientconnect_done( stat_query_t *query, bool success, void *customp ) {
	stat_query_section_t *root, *ratings_section;
	int session_id, isession_id;
	client_t *cl;
	edict_t *ent;
	bool userinfo_changed = false;

	/*
	 * ch : JSON API
	 * {
	 *		id: [int],	// 0 on error, > 0 on logged-in user, < 0 for "anonymous" user
	 *		login: [string],	// login-name for user on success
	 *		mminfo: [int]
	 *		ratings: [
	 *			{ gametype: [string], rating: [float]: deviation: [float] }
	 *			..
	 *		]
	 * }
	 */

	/*
	 * since we are now generating the local session in SV_MM_ClientConnect, we should
	 * check if session_id < 0 just in case so that we wont try to regenerate local
	 * session or do anything stupid like that
	 * (currently we dont even tell MM about these so ignore)
	 */

	session_id = (int)( (intptr_t )customp );
	isession_id = 0;
	cl = SV_MM_ClientForSession( session_id );

	if( cl == NULL ) {
		// or figure out the validation anyway from the received session-id?
		Com_Printf( "SV_MM_ClientConnect: Couldnt find client with session-id %d\n", session_id );
		return;
	}

	if( !success ) {
		Com_Printf( "SV_MM_ClientConnect: Error\n" );
	} else {
		root = sq_api->GetRoot( query );
		if( query == NULL ) {
			Com_Printf( "SV_MM_ParseResponse: Failed to parse data\n" );
		} else {
			int banned = (int)sq_api->GetNumber( root, "banned" );
			if( banned != 0 ) {
				const char *reason = sq_api->GetString( root, "reason" );
				if( !reason || *reason == '\0' ) {
					reason = "Your account at " APP_URL " has been banned.";
				}

				SV_DropClient( cl, DROP_TYPE_GENERAL, "Error: %s", reason );
				return;
			}

			isession_id = (int)sq_api->GetNumber( root, "id" );
			if( isession_id == 0 ) {
				Com_Printf( "SV_MM_ClientConnect: Client not logged in\n" );
			} else if( isession_id != session_id ) {
				Com_Printf( "SV_MM_ClientConnect: Session-id doesnt match %d -> %d\n", isession_id, session_id );
				isession_id = 0;
			} else {
				const char *login = sq_api->GetString( root, "login" );
				const char *mmflags = sq_api->GetString( root, "mmflags" );
				ratings_section = sq_api->GetSection( root, "ratings" );

				Q_strncpyz( cl->mm_login, login, sizeof( cl->mm_login ) );
				if( !Info_SetValueForKey( cl->userinfo, "cl_mm_login", login ) ) {
					Com_Printf( "Failed to set infokey 'cl_mm_login' for player %s\n", login );
				}

				if( mmflags && *mmflags ) {
					if( !Info_SetValueForKey( cl->userinfo, "mmflags", mmflags ) ) {
						Com_Printf( "Failed to set infokey 'mmflags' for player %s\n", login );
					}
				}

				userinfo_changed = true;

				if( ge != NULL && ratings_section != NULL ) {
					int idx = 0;
					stat_query_section_t *element = sq_api->GetArraySection( ratings_section, idx++ );
					ent = EDICT_NUM( ( cl - svs.clients ) + 1 );
					while( element != NULL ) {
						ge->AddRating( ent, sq_api->GetString( element, "gametype" ),
									   sq_api->GetNumber( element, "rating" ),
									   sq_api->GetNumber( element, "deviation" ) );
						element = sq_api->GetArraySection( ratings_section, idx++ );
					}
				}
			}
		}
	}

	// unable to validate client, either kick him out or force local session
	if( isession_id == 0 ) {
		if( sv_mm_loginonly->integer ) {
			SV_DropClient( cl, DROP_TYPE_GENERAL, "%s", "Error: This server requires login. Create account at " APP_URL );
			return;
		}

		// TODO: check that session_id >= 0
		isession_id = SV_MM_GenerateLocalSession();
		Com_Printf( "SV_MM_ClientConnect: Forcing local_session %d on client %s\n", isession_id, cl->name );
		cl->mm_session = isession_id;
		userinfo_changed = true;

		// We should also notify MM about the new local session id?
		// Or another option would be that MM doesnt track local sessions at all,
		// it just emits the results straight away.

		// resend scc query
		// cl->socket->address
	}

	if( userinfo_changed ) {
		SV_UserinfoChanged( cl );
	}

	Com_Printf( "SV_MM_ClientConnect: %s with session id %d\n", cl->name, cl->mm_session );
}

int SV_MM_ClientConnect( const netadr_t *address, char *userinfo, unsigned int ticket_id, int session_id ) {
	/*
	* what crizis did.. push a query after checking that ticket id and session id
	* at least aren't null and if server expects login-only clients
	*
	* ahem, figure out how to handle anonymous players. currently this will bug out so that
	* session_id is 0 -> request receives zero id's and freaks out. generate a session_id
	* here and return it.
	*
	* ok done. so this function receives session_id = 0 if we are dealing with 'anonymous'
	* player and this here generates local session-id for the client
	*/
	stat_query_t *query;

	// return of -1 is not an error, it just marks a dummy local session
	if( !sv_mm_initialized || !sv_mm_session ) {
		return -1;
	}

	// accept only players that are logged in (session_id <= 0 ??)
	if( sv_mm_loginonly->integer && session_id == 0 ) {
		Com_Printf( "SV_MM_ClientConnect: Login-only\n" );
		return 0;
	}

	// expect a ticket for logged-in client (rly?) session_id > 0
	// we should force local session in here
	if( ticket_id == 0 && session_id != 0 ) {
		Com_Printf( "SV_MM_ClientConnect: Logged-in client didnt declare ticket, marking as anonymous\n" );
		session_id = 0;
	}

	if( session_id == 0 ) {
		// WMM doesnt care about anonymous players
		session_id = SV_MM_GenerateLocalSession();
		Com_Printf( "SV_MM_ClientConnect: Generated local session %d\n", session_id );
		return session_id;
	}

	// push a request
	query = sq_api->CreateQuery( sv_ip->string, "scc", false );
	if( query == NULL ) {
		return 0;
	}

	// servers own session (TODO: put this to a cookie or smth)
	sq_api->SetField( query, "ssession", va( "%d", sv_mm_session ) );

	// clients attributes (nickname here?)
	sq_api->SetField( query, "cticket", va( "%u", ticket_id ) );
	sq_api->SetField( query, "csession", va( "%d", session_id ) );
	sq_api->SetField( query, "cip", NET_AddressToString( address ) );

	sq_api->SetCallback( query, sv_mm_clientconnect_done, (void*)( (intptr_t)session_id ) );
	sq_api->Send( query );

	return session_id;
}

void SV_MM_Frame( void ) {
	int64_t time;

	if( sv_mm_enable->modified ) {
		if( sv_mm_enable->integer && !sv_mm_initialized ) {
			SV_MM_Login();
		} else if( !sv_mm_enable->integer && sv_mm_initialized ) {
			SV_MM_Logout( false );
		}

		sv_mm_enable->modified = false;
	}

	if( sv_mm_initialized ) {
		if( sv_mm_logout_semaphore ) {
			// logout process is finished so we can shutdown game
			SV_MM_Shutdown( false );
			sv_mm_logout_semaphore = false;
			return;
		}

		// heartbeat
		time = Sys_Milliseconds();
		if( ( sv_mm_last_heartbeat + MM_HEARTBEAT_INTERVAL ) < time ) {
			SV_MM_Heartbeat();
			sv_mm_last_heartbeat = time;
		}

		SV_MM_GetMatchUUIDThink();
	}
}

bool SV_MM_Initialized( void ) {
	return sv_mm_initialized;
}


static void sv_mm_logout_done( stat_query_t *query, bool success, void *customp ) {
	Com_Printf( "SV_MM_Logout: Loggin off..\n" );

	// ignore response-status and just mark us as logged-out
	sv_mm_logout_semaphore = true;
}

/*
* SV_MM_Logout
*/
static void SV_MM_Logout( bool force ) {
	stat_query_t *query;
	int64_t timeout;

	if( !sv_mm_initialized || !sv_mm_session ) {
		return;
	}

	query = sq_api->CreateQuery( sv_ip->string, "slogout", false );
	if( query == NULL ) {
		return;
	}

	sv_mm_logout_semaphore = false;

	// TODO: pull the authkey out of cvar into file
	sq_api->SetField( query, "ssession", va( "%d", sv_mm_session ) );
	sq_api->SetCallback( query, sv_mm_logout_done, NULL );
	sq_api->Send( query );

	if( force ) {
		timeout = Sys_Milliseconds();
		while( !sv_mm_logout_semaphore && Sys_Milliseconds() < ( timeout + MM_LOGOUT_TIMEOUT ) ) {
			sq_api->Poll();

			Sys_Sleep( 10 );
		}

		if( !sv_mm_logout_semaphore ) {
			Com_Printf( "SV_MM_Logout: Failed to force logout\n" );
		} else {
			Com_Printf( "SV_MM_Logout: force logout successful\n" );
		}

		sv_mm_logout_semaphore = false;

		// dont call this, we are coming from shutdown
		// SV_MM_Shutdown( false );
	}
}

/*
* sv_mm_login_done
* callback for login post request
*/
static void sv_mm_login_done( stat_query_t *query, bool success, void *customp ) {
	stat_query_section_t *root;

	sv_mm_initialized = false;
	sv_login_query = NULL;

	if( !success ) {
		Com_Printf( "SV_MM_Login_Done: Error\n" );
		Cvar_ForceSet( sv_mm_enable->name, "0" );
		return;
	}

	Com_DPrintf( "SV_MM_Login: %s\n", sq_api->GetRawResponse( query ) );

	/*
	 * ch : JSON API
	 * {
	 *		id: [int], // 0 on error, > 0 on success
	 * }
	 */
	root = sq_api->GetRoot( query );
	if( root == NULL ) {
		Com_Printf( "SV_MM_Login: Failed to parse data\n" );
	} else {
		sv_mm_session = (int)sq_api->GetNumber( root, "id" );
		sv_mm_initialized = ( sv_mm_session == 0 ? false : true );
	}

	if( sv_mm_initialized ) {
		Com_Printf( "SV_MM_Login: Success, session id %u\n", sv_mm_session );
	} else {
		Com_Printf( "SV_MM_Login: Failed, no session id\n" );
		Cvar_ForceSet( sv_mm_enable->name, "0" );
	}
}

/*
* SV_MM_Login
*/
static bool SV_MM_Login( void ) {
	stat_query_t *query;

	if( sv_login_query != NULL || sv_mm_initialized ) {
		return false;
	}
	if( sv_mm_authkey->string[0] == '\0' ) {
		Cvar_ForceSet( sv_mm_enable->name, "0" );
		return false;
	}

	Com_Printf( "SV_MM_Login: Creating query\n" );

	query = sq_api->CreateQuery( sv_ip->string, "slogin", false );
	if( query == NULL ) {
		return false;
	}

	sq_api->SetField( query, "authkey", sv_mm_authkey->string );
	sq_api->SetField( query, "port", va( "%d", sv_port->integer ) );
	sq_api->SetField( query, "hostname", sv.configstrings[CS_HOSTNAME] );
	sq_api->SetField( query, "demos_baseurl", sv_uploads_demos_baseurl->string );
	sq_api->SetCallback( query, sv_mm_login_done, NULL );
	sq_api->Send( query );

	sv_login_query = query;

	return true;
}

/*
* sv_mm_match_uuid_done
* callback for match uuid fetching
*/
static void sv_mm_match_uuid_done( stat_query_t *query, bool success, void *customp ) {
	stat_query_section_t *root;

	// set the repeat timer, which will be ignored in case we successfully parse the response
	sv_mm_next_match_uuid_fetch = Sys_Milliseconds() + SV_MM_MATCH_UUID_FETCH_INTERVAL * 1000;
	sv_mm_match_uuid_fetch_query = NULL;

	if( !success ) {
		return;
	}

	Com_DPrintf( "SV_MM_GetMatchUUID: %s\n", sq_api->GetRawResponse( query ) );

	/*
	 * JSON API
	 * {
	 *		uuid: [string]
	 * }
	 */
	root = sq_api->GetRoot( query );
	if( root == NULL ) {
		Com_Printf( "SV_MM_GetMatchUUID: Failed to parse data\n" );
	} else {
		Q_strncpyz( sv_mm_match_uuid, sq_api->GetString( root, "uuid" ), sizeof( sv_mm_match_uuid ) );
		if( sv_mm_match_uuid_callback_fn ) {
			// fire the callback function
			sv_mm_match_uuid_callback_fn( sv_mm_match_uuid );
		}
	}
}

/*
* SV_MM_GetMatchUUIDThink
*
* Repeatedly query the matchmaker for match UUID until we get one.
*/
static void SV_MM_GetMatchUUIDThink( void ) {
	stat_query_t *query;

	if( !sv_mm_initialized || !sv_mm_session ) {
		return;
	}
	if( sv_mm_next_match_uuid_fetch > Sys_Milliseconds() ) {
		// not ready yet
		return;
	}
	if( sv_mm_match_uuid_fetch_query != NULL ) {
		// already in progress
		return;
	}
	if( sv_mm_match_uuid[0] != '\0' ) {
		// we have already queried the server
		return;
	}

	// ok, get it now!
	Com_DPrintf( "SV_MM_GetMatchUUIDThink: Creating query\n" );

	query = sq_api->CreateQuery( sv_ip->string, "smuuid", false );
	if( query == NULL ) {
		return;
	}

	sq_api->SetField( query, "ssession", va( "%d", sv_mm_session ) );
	sq_api->SetCallback( query, sv_mm_match_uuid_done, NULL );
	sq_api->Send( query );

	sv_mm_match_uuid_fetch_query = query;
}

/*
* SV_MM_GetMatchUUID
*
* Start querying the server for match UUID. Fire the callback function
* upon success.
*/
void SV_MM_GetMatchUUID( void ( *callback_fn )( const char *uuid ) ) {
	if( !sv_mm_initialized ) {
		return;
	}
	if( sv_mm_match_uuid_fetch_query != NULL ) {
		// already in progress
		return;
	}
	if( sv_mm_next_match_uuid_fetch > Sys_Milliseconds() ) {
		// not ready yet
		return;
	}

	sv_mm_match_uuid[0] = '\0';
	sv_mm_match_uuid_callback_fn = callback_fn;

	// think now!
	sv_mm_next_match_uuid_fetch = Sys_Milliseconds();
	SV_MM_GetMatchUUIDThink();
}

/*
* SV_MM_Init
*/
void SV_MM_Init( void ) {
	sv_mm_initialized = false;
	sv_mm_session = 0;
	sv_mm_localsession = 0;
	sv_mm_last_heartbeat = 0;
	sv_mm_logout_semaphore = false;

	sv_mm_gameon = false;

	sv_mm_match_uuid[0] = '\0';
	sv_mm_next_match_uuid_fetch = Sys_Milliseconds();
	sv_mm_match_uuid_fetch_query = NULL;
	sv_mm_match_uuid_callback_fn = NULL;

	StatQuery_Init();
	sq_api = StatQuery_GetAPI();

	/*
	* create cvars
	* ch : had to make sv_mm_enable to cmdline only, because of possible errors
	* if enabled while players on server
	*/
	sv_mm_enable = Cvar_Get( "sv_mm_enable", "0", CVAR_ARCHIVE | CVAR_NOSET | CVAR_SERVERINFO );
	sv_mm_loginonly = Cvar_Get( "sv_mm_loginonly", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_mm_debug_reportbots = Cvar_Get( "sv_mm_debug_reportbots", "0", CVAR_CHEAT );

	// this is used by game, but to pass it to client, we'll initialize it in sv
	Cvar_Get( "sv_skillRating", va( "%.0f", MM_RATING_DEFAULT ), CVAR_READONLY | CVAR_SERVERINFO );

	// TODO: remove as cvar
	sv_mm_authkey = Cvar_Get( "sv_mm_authkey", "", CVAR_ARCHIVE );

	/*
	* login
	*/
	sv_login_query = NULL;
	//if( sv_mm_enable->integer )
	//	SV_MM_Login();
	sv_mm_enable->modified = true;
}

void SV_MM_Shutdown( bool logout ) {
	if( !sv_mm_initialized ) {
		return;
	}

	Com_Printf( "SV_MM_Shutdown..\n" );

	if( logout ) {
		// logout is always force in here
		SV_MM_Logout( true );
	}

	Cvar_ForceSet( "sv_mm_enable", "0" );

	sv_mm_gameon = false;

	sv_mm_last_heartbeat = 0;
	sv_mm_logout_semaphore = false;

	sv_mm_initialized = false;
	sv_mm_session = 0;

	StatQuery_Shutdown();
	sq_api = NULL;
}
