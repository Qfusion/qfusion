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

	void Init( int vidWidth, int vidHeight, float pixelRatio,
		int protocol, const char *demoExtension, const char *basePath )
	{
		// destructor doesnt throw
		if( ui_main ) {
			UI_Main::Destroy();
			ui_main = NULL;
		}

		// constructor may throw
		try
		{
			ui_main = UI_Main::Instance( vidWidth, vidHeight, pixelRatio,
				protocol, demoExtension, basePath );
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
		bool demoPlaying, const char *demoName, bool demoPaused, unsigned int demoTime, 
		bool backGround, bool showCursor )
	{
		if( ui_main ) {
			ui_main->refreshScreen( time, clientState, serverState, 
				demoPlaying == true, demoName ? demoName : "",
				demoPaused == true, demoTime, backGround == true, showCursor == true );
		}
	}

	void UpdateConnectScreen( const char *serverName, const char *rejectmessage, 
		int downloadType, const char *downloadfilename, float downloadPercent, int downloadSpeed, 
		int connectCount, bool backGround )
	{
		if( ui_main )
			ui_main->drawConnectScreen( serverName, rejectmessage, downloadType, downloadfilename, 
				downloadPercent, downloadSpeed, connectCount, (backGround == true) );
	}

	void Keydown( int context, int key )
	{
		if( ui_main ) {
			ui_main->keyEvent( context, key, true );
		}
	}

	void Keyup( int context, int key )
	{
		if( ui_main ) {
			ui_main->keyEvent( context, key, false );
		}
	}

	void CharEvent( int context, wchar_t key )
	{
		// Check if the character is printable.
		// Emitting textinput events for non-printable chars might cause 
		// surprising behavior (e.g. backspace key not working in librocket's
		// text input fields).
		if( ui_main ) {
			if(isprint(key)) ui_main->textInput( context, key );
		}
	}

	void MouseMove( int context, int dx, int dy )
	{
		if( ui_main ) {
			ui_main->mouseMove( context, dx, dy, false, true );
		}
	}

	void MouseSet( int context, int mx, int my, bool showCursor )
	{
		if( ui_main ) {
			ui_main->mouseMove( context, mx, my, true, showCursor );
		}
	}

	bool TouchEvent( int context, int id, touchevent_t type, int x, int y )
	{
		if( ui_main ) {
			return ui_main->touchEvent( context, id, type, x, y );
		}

		return false;
	}

	bool IsTouchDown( int context, int id )
	{
		if( ui_main ) {
			return ui_main->isTouchDown( context, id );
		}

		return false;
	}

	void CancelTouches( int context )
	{
		if( ui_main ) {
			ui_main->cancelTouches( context );
		}
	}

	void ForceMenuOff( void )
	{
		if( ui_main ) {
			ui_main->forceMenuOff();
		}
	}

	void ShowQuickMenu( bool show )
	{
		if( ui_main ) {
			ui_main->showQuickMenu( show );
		}
	}

	bool HaveQuickMenu( void )
	{
		if( ui_main ) {
			return ui_main->haveQuickMenu();
		}
		return false;
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
	globals.MouseSet = WSWUI::MouseSet;
	globals.TouchEvent = WSWUI::TouchEvent;
	globals.IsTouchDown = WSWUI::IsTouchDown;
	globals.CancelTouches = WSWUI::CancelTouches;

	globals.ForceMenuOff = WSWUI::ForceMenuOff;
	globals.ShowQuickMenu = WSWUI::ShowQuickMenu;
	globals.HaveQuickMenu = WSWUI::HaveQuickMenu;

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
int WINAPI DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
