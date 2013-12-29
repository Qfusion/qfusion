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

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include <cctype>

namespace WSWUI
{
	UI_Main *ui_main = 0;
	ui_import_t UI_IMPORT;

	// if API is different, the dll cannot be used
	int API( void )
	{
		return UI_API_VERSION;
	}

	void Init( int vidWidth, int vidHeight, int protocol, const char *demoExtension )
	{
		// destructor doesnt throw
		if( ui_main ) {
			UI_Main::Destroy();
			ui_main = NULL;
		}

		// constructor may throw
		try
		{
			ui_main = UI_Main::Instance( vidWidth, vidHeight, protocol, demoExtension );
		}
		catch( std::runtime_error &err )
		{
			ui_main = NULL;
			Com_Printf(S_COLOR_RED"UI Init: %s\n", err.what() );
		}
	}

	void Shutdown( void )
	{
		// destructor doesnt throw
		if( ui_main )
			__delete__( ui_main );
		ui_main = 0;
	}

	void TouchAllAssets( void )
	{
		if( ui_main ) {
			ui_main->touchAllCachedShaders();
			ui_main->flushAjaxCache();
		}
	}

	void Refresh( unsigned int time, int clientState, int serverState, 
		qboolean demoPlaying, const char *demoName, qboolean demoPaused, unsigned int demoTime, 
		qboolean backGround, qboolean showCursor )
	{
		if( ui_main ) {
			ui_main->refreshScreen( time, clientState, serverState, 
				demoPlaying == qtrue, demoName ? demoName : "",
				demoPaused == qtrue, demoTime, backGround == qtrue, showCursor == qtrue );
		}
	}

	void UpdateConnectScreen( const char *serverName, const char *rejectmessage, 
		int downloadType, const char *downloadfilename, float downloadPercent, int downloadSpeed, 
		int connectCount, qboolean backGround )
	{
		if( ui_main )
			ui_main->drawConnectScreen( serverName, rejectmessage, downloadType, downloadfilename, 
				downloadPercent, downloadSpeed, connectCount, (backGround == qtrue) );
	}

	void Keydown( int key )
	{
		if( ui_main ) {
			ui_main->keyEvent( key, true );
		}
	}

	void Keyup( int key )
	{
		if( ui_main ) {
			ui_main->keyEvent( key, false );
		}
	}

	void CharEvent( qwchar key )
	{
		// Check if the character is printable.
		// Emitting textinput events for non-printable chars might cause 
		// surprising behavior (e.g. backspace key not working in librocket's
		// text input fields).
		if( ui_main ) {
			if(isprint(key)) ui_main->textInput( key );
		}
	}

	void MouseMove( int dx, int dy )
	{
		if( ui_main ) {
			ui_main->mouseMove( dx, dy );
		}
	}

	void ForceMenuOff( void )
	{
		if( ui_main ) {
			ui_main->forceMenuOff();
		}
	}

	void AddToServerList( const char *adr, const char *info )
	{
		if( ui_main ) {
			ui_main->addToServerList( adr, info );
		}
	}
}	// namespace

//=================================

ui_export_t *GetUIAPI( ui_import_t *import )
{
	static ui_export_t globals;

	// Trap::UI_IMPORT = *import;
	WSWUI::UI_IMPORT = *import;

	globals.API = WSWUI::API;

	globals.Init = WSWUI::Init;
	globals.Shutdown = WSWUI::Shutdown;

	globals.TouchAllAssets = WSWUI::TouchAllAssets;

	globals.Refresh = WSWUI::Refresh;
	globals.UpdateConnectScreen = WSWUI::UpdateConnectScreen;

	globals.Keydown = WSWUI::Keydown;
	globals.Keyup = WSWUI::Keyup;
	globals.CharEvent = WSWUI::CharEvent;
	globals.MouseMove = WSWUI::MouseMove;

	globals.ForceMenuOff = WSWUI::ForceMenuOff;

	globals.AddToServerList = WSWUI::AddToServerList;

	return &globals;
}

#ifndef UI_HARD_LINKED
#include <stdarg.h>

// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap::Error( msg );
}

void Com_Printf( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap::Print( msg );
}
#endif

#if defined(HAVE_DLLMAIN) && !defined(UI_HARD_LINKED)
int _stdcall DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
