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
int API( void ) {
	return UI_API_VERSION;
}

void Init( int vidWidth, int vidHeight, float pixelRatio,
		   int protocol, const char *demoExtension, const char *basePath ) {
	// destructor doesnt throw
	if( ui_main ) {
		UI_Main::Destroy();
		ui_main = NULL;
	}

	// constructor may throw
	try{
		ui_main = UI_Main::Instance( vidWidth, vidHeight, pixelRatio,
									 protocol, demoExtension, basePath );
	}catch( std::runtime_error &err ) {
		ui_main = NULL;
		Com_Printf( S_COLOR_RED "UI Init: %s\n", err.what() );
	}
}

void Shutdown( void ) {
	// destructor doesnt throw
	if( ui_main ) {
		__delete__( ui_main );
	}
	ui_main = 0;
}

void TouchAllAssets( void ) {
	if( ui_main ) {
		ui_main->touchAllCachedShaders();
		ui_main->flushAjaxCache();
	}
}

void Refresh( int64_t time, int clientState, int serverState,
			  bool demoPlaying, const char *demoName, bool demoPaused, unsigned int demoTime,
			  bool backGround, bool showCursor ) {
	if( ui_main ) {
		ui_main->refreshScreen( time, clientState, serverState,
								demoPlaying == true, demoName ? demoName : "",
								demoPaused == true, demoTime, backGround == true, showCursor == true );
	}
}

void UpdateConnectScreen( const char *serverName, const char *rejectmessage,
						  int downloadType, const char *downloadfilename, float downloadPercent, int downloadSpeed,
						  int connectCount, bool backGround ) {
	if( ui_main ) {
		ui_main->drawConnectScreen( serverName, rejectmessage, downloadType, downloadfilename,
									downloadPercent, downloadSpeed, connectCount, ( backGround == true ) );
	}
}

void KeyEvent( int context, int key, bool down ) {
	if( ui_main ) {
		ui_main->keyEvent( context, key, down );
	}
}

void CharEvent( int context, wchar_t key ) {
	// Check if the character is printable.
	// Emitting textinput events for non-printable chars might cause
	// surprising behavior (e.g. backspace key not working in librocket's
	// text input fields).
	if( ui_main ) {
		if( isprint( key ) ) {
			ui_main->textInput( context, key );
		}
	}
}

void MouseMove( int context, int frameTime, int dx, int dy ) {
	if( ui_main ) {
		ui_main->mouseMove( context, frameTime, dx, dy, false, true );
	}
}

bool MouseHover( int context ) {
	if( ui_main ) {
		return ui_main->mouseHover( context );
	}
	return false;
}

void MouseSet( int context, int mx, int my, bool showCursor ) {
	if( ui_main ) {
		ui_main->mouseMove( context, 0, mx, my, true, showCursor );
	}
}

void ForceMenuOff( void ) {
	if( ui_main ) {
		ui_main->forceMenuOff();
	}
}

void ShowOverlayMenu( bool show, bool showCursor ) {
	if( ui_main ) {
		ui_main->showOverlayMenu( show, showCursor );
	}
}

bool HaveOverlayMenu( void ) {
	if( ui_main ) {
		return ui_main->haveOverlayMenu();
	}
	return false;
}

void AddToServerList( const char *adr, const char *info ) {
	if( ui_main ) {
		ui_main->addToServerList( adr, info );
	}
}
}   // namespace

//=================================

ui_export_t *GetUIAPI( ui_import_t *import ) {
	static ui_export_t globals;

	// Trap::UI_IMPORT = *import;
	WSWUI::UI_IMPORT = *import;

	globals.API = WSWUI::API;

	globals.Init = WSWUI::Init;
	globals.Shutdown = WSWUI::Shutdown;

	globals.TouchAllAssets = WSWUI::TouchAllAssets;

	globals.Refresh = WSWUI::Refresh;
	globals.UpdateConnectScreen = WSWUI::UpdateConnectScreen;

	globals.KeyEvent = WSWUI::KeyEvent;
	globals.CharEvent = WSWUI::CharEvent;
	globals.MouseMove = WSWUI::MouseMove;
	globals.MouseHover = WSWUI::MouseHover;
	globals.MouseSet = WSWUI::MouseSet;

	globals.ForceMenuOff = WSWUI::ForceMenuOff;
	globals.ShowOverlayMenu = WSWUI::ShowOverlayMenu;
	globals.HaveOverlayMenu = WSWUI::HaveOverlayMenu;

	globals.AddToServerList = WSWUI::AddToServerList;

	return &globals;
}

#ifndef UI_HARD_LINKED
#include <stdarg.h>

// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap::Error( msg );
}

void Com_Printf( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap::Print( msg );
}
#endif

#if defined( HAVE_DLLMAIN ) && !defined( UI_HARD_LINKED )
int WINAPI DLLMain( void *hinstDll, unsigned long dwReason, void *reserved ) {
	return 1;
}
#endif
