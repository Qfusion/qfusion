/*
Copyright (C) 2011 Victor Luchits

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

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_demoinfo.h"
#include "kernel/ui_downloadinfo.h"
#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI
{

// dummy cGame class, single referenced, sorta like 'window' JS
class Game
{
public:
	Game() {
	}
};

typedef WSWUI::DemoInfo DemoInfo;
typedef WSWUI::DownloadInfo DownloadInfo;

static Game dummyGame;

// =====================================================================================

void PrebindGame( ASInterface *as ) {
	ASBind::Class<Game, ASBind::class_singleref>( as->getEngine() );
}

static const DemoInfo & Game_GetDemoInfo( Game *game ) {
	return *UI_Main::Get()->getDemoInfo();
}

static asstring_t *Game_Name( Game *game ) {
	return ASSTR( trap::Cvar_String( "gamename" ) );
}

static asstring_t *Game_Version( Game *game ) {
	return ASSTR( trap::Cvar_String( "version" ) );
}

static asstring_t *Game_Revision( Game *game ) {
	return ASSTR( trap::Cvar_String( "revision" ) );
}

static asstring_t *Game_ServerName( Game *game ) {
	return ASSTR( UI_Main::Get()->getServerName() );
}

static asstring_t *Game_RejectMessage( Game *game ) {
	return ASSTR( UI_Main::Get()->getRejectMessage() );
}

static const DownloadInfo & Game_GetDownloadInfo( Game *game ) {
	return *UI_Main::Get()->getDownloadInfo();
}

static asstring_t *Game_ConfigString( Game *game, int cs ) {
	char configstring[MAX_CONFIGSTRING_CHARS];

	if( cs < 0 || cs >= MAX_CONFIGSTRINGS ) {
		Com_Printf( S_COLOR_RED "Game_ConfigString: bogus configstring index: %i", cs );
		return ASSTR( "" );
	}

	trap::GetConfigString( cs, configstring, sizeof( configstring ) );
	return ASSTR( configstring );
}

static int Game_ClientState( Game *game ) {
	return UI_Main::Get()->getRefreshState().clientState;
}

static int Game_ServerState( Game *game ) {
	return UI_Main::Get()->getRefreshState().serverState;
}

static void Game_Exec( Game *game, const asstring_t &cmd ) {
	trap::Cmd_ExecuteText( EXEC_NOW, cmd.buffer );
}

static void Game_ExecAppend( Game *game, const asstring_t &cmd ) {
	trap::Cmd_ExecuteText( EXEC_APPEND, cmd.buffer );
}

static void Game_ExecInsert( Game *game, const asstring_t &cmd ) {
	trap::Cmd_ExecuteText( EXEC_INSERT, cmd.buffer );
}

static int Game_PlayerNum( Game *game ) {
	return trap::CL_PlayerNum();
}

void BindGame( ASInterface *as ) {
	ASBind::Enum( as->getEngine(), "eConfigString" )
		( "CS_MODMANIFEST", CS_MODMANIFEST )
		( "CS_MESSAGE", CS_MESSAGE )
		( "CS_MAPNAME", CS_MAPNAME )
		( "CS_AUDIOTRACK", CS_AUDIOTRACK )
		( "CS_HOSTNAME", CS_HOSTNAME )
		( "CS_GAMETYPETITLE", CS_GAMETYPETITLE )
		( "CS_GAMETYPENAME", CS_GAMETYPENAME )
		( "CS_GAMETYPEVERSION", CS_GAMETYPEVERSION )
		( "CS_GAMETYPEAUTHOR", CS_GAMETYPEAUTHOR )
		( "CS_TEAM_ALPHA_NAME", CS_TEAM_ALPHA_NAME )
		( "CS_TEAM_BETA_NAME", CS_TEAM_BETA_NAME )
		( "CS_MATCHNAME", CS_MATCHNAME )
		( "CS_MATCHSCORE", CS_MATCHSCORE )
		( "CS_ACTIVE_CALLVOTE", CS_ACTIVE_CALLVOTE )
		( "CS_ACTIVE_CALLVOTE_VOTES", CS_ACTIVE_CALLVOTE_VOTES )
	;

	ASBind::Enum( as->getEngine(), "eClientState" )
		( "CA_UNITIALIZED", CA_UNINITIALIZED )
		( "CA_DISCONNECTED", CA_DISCONNECTED )
		( "CA_GETTING_TICKET", CA_GETTING_TICKET )
		( "CA_CONNECTING", CA_CONNECTING )
		( "CA_HANDSHAKE", CA_HANDSHAKE )
		( "CA_CONNECTED", CA_CONNECTED )
		( "CA_LOADING", CA_LOADING )
		( "CA_ACTIVE", CA_ACTIVE )
	;

	ASBind::Enum( as->getEngine(), "eDropReason" )
		( "DROP_REASON_CONNFAILED", DROP_REASON_CONNFAILED )
		( "DROP_REASON_CONNTERMINATED", DROP_REASON_CONNTERMINATED )
		( "DROP_REASON_CONNERROR", DROP_REASON_CONNERROR )
	;

	ASBind::Enum( as->getEngine(), "eDropType" )
		( "DROP_TYPE_GENERAL", DROP_TYPE_GENERAL )
		( "DROP_TYPE_PASSWORD", DROP_TYPE_PASSWORD )
		( "DROP_TYPE_NORECONNECT", DROP_TYPE_NORECONNECT )
		( "DROP_TYPE_TOTAL", DROP_TYPE_TOTAL )
	;

	ASBind::Enum( as->getEngine(), "eDownloadType" )
		( "DOWNLOADTYPE_NONE", DOWNLOADTYPE_NONE )
		( "DOWNLOADTYPE_SERVER", DOWNLOADTYPE_SERVER )
		( "DOWNLOADTYPE_WEB", DOWNLOADTYPE_WEB )
	;

	ASBind::GetClass<Game>( as->getEngine() )

	// gives access to properties and controls of the currently playing demo instance
	.constmethod( Game_GetDemoInfo, "get_demo", true )

	.constmethod( Game_Name, "get_name", true )
	.constmethod( Game_Version, "get_version", true )
	.constmethod( Game_Revision, "get_revision", true )

	.constmethod( Game_ConfigString, "configString", true )
	.constmethod( Game_ConfigString, "cs", true )

	.constmethod( Game_PlayerNum, "get_playerNum", true )

	.constmethod( Game_ClientState, "get_clientState", true )
	.constmethod( Game_ServerState, "get_serverState", true )

	.constmethod( Game_Exec, "exec", true )
	.constmethod( Game_ExecAppend, "execAppend", true )
	.constmethod( Game_ExecInsert, "execInsert", true )

	.constmethod( Game_ServerName, "get_serverName", true )
	.constmethod( Game_RejectMessage, "get_rejectMessage", true )
	.constmethod( Game_GetDownloadInfo, "get_download", true )
	;
}

void BindGameGlobal( ASInterface *as ) {
	ASBind::Global( as->getEngine() )

	// global variable
	.var( &dummyGame, "game" )
	;
}

}

ASBIND_TYPE( ASUI::Game, Game )
