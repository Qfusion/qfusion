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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake refresh engine.
#include <assert.h>
#include <float.h>
#include "../client/client.h"
#include "winquake.h"

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL ( WM_MOUSELAST+1 ) // message that will be supported by the OS
#endif

#ifndef MK_XBUTTON1
#define MK_XBUTTON1         0x0020
#define MK_XBUTTON2         0x0040
#endif

#ifndef MK_XBUTTON3
#define MK_XBUTTON3         0x0080
#define MK_XBUTTON4         0x0100
#endif

#ifndef MK_XBUTTON5
#define MK_XBUTTON5         0x0200
#endif

#ifndef WM_XBUTTONUP
#define WM_XBUTTONUP        0x020C
#define WM_XBUTTONDOWN      0x020B
#endif

#if ( _WIN32_WINNT < 0x0400 )
#define KF_UP		    0x8000
#define LLKHF_UP	    ( KF_UP >> 8 )

typedef struct
{
	DWORD vkCode;
	DWORD scanCode;
	DWORD flags;
	DWORD time;
	ULONG_PTR dwExtraInfo;
	//	ULONG dwExtraInfo;
} *PKBDLLHOOKSTRUCT;
#endif

static UINT MSH_MOUSEWHEEL;

// Console variables that we need to access from this module
extern cvar_t *vid_xpos;          // X coordinate of window position
extern cvar_t *vid_ypos;          // Y coordinate of window position
extern cvar_t *vid_fullscreen;
extern cvar_t *win_noalttab;
extern cvar_t *win_nowinkeys;

// Global variables used internally by this module
extern viddef_t	viddef;             // global video state; used by other modules

HWND cl_hwnd;           // Main window handle for life of program
HWND cl_parent_hwnd;	// pointer to parent window handle

static HHOOK WinKeyHook;
static qboolean s_winkeys_hooked;
static qboolean s_alttab_disabled;
extern qboolean	s_win95;
extern unsigned	sys_msg_time;

/*
** WinKeys system hook (taken from ezQuake)
*/
LRESULT CALLBACK LLWinKeyHook( int Code, WPARAM wParam, LPARAM lParam )
{
	PKBDLLHOOKSTRUCT p;

	p = (PKBDLLHOOKSTRUCT) lParam;

	if( ActiveApp )
	{
		// we do not allow separate bindings for left and right keys yet
		switch( p->vkCode )
		{
#if 0
		case VK_LWIN: Key_Event( K_LWIN, !( p->flags & LLKHF_UP ), sys_msg_time ); return 1;
		case VK_RWIN: Key_Event( K_RWIN, !( p->flags & LLKHF_UP ), sys_msg_time ); return 1;
#else
		case VK_LWIN: Key_Event( K_WIN, !( p->flags & LLKHF_UP ), sys_msg_time ); return 1;
		case VK_RWIN: Key_Event( K_WIN, !( p->flags & LLKHF_UP ), sys_msg_time ); return 1;
#endif
		default:
			break;
		}
	}

	return CallNextHookEx( NULL, Code, wParam, lParam );
}

/*
* VID_EnableAltTab
*/
void VID_EnableAltTab( qboolean enable )
{
	if( enable )
	{
		if( s_alttab_disabled )
		{
			if( s_win95 )
			{
				BOOL old;
				SystemParametersInfo( SPI_SETSCREENSAVERRUNNING, 0, &old, 0 );
			}
			else
			{
				UnregisterHotKey( 0, 0 );
				UnregisterHotKey( 0, 1 );
			}
			s_alttab_disabled = qfalse;
		}
	}
	else
	{
		if( s_alttab_disabled )
			return;

		if( s_win95 )
		{
			BOOL old;
			SystemParametersInfo( SPI_SETSCREENSAVERRUNNING, 1, &old, 0 );
		}
		else
		{
			RegisterHotKey( 0, 0, MOD_ALT, VK_TAB );
			RegisterHotKey( 0, 1, MOD_ALT, VK_RETURN );
		}
		s_alttab_disabled = qtrue;
	}
}

/*
* VID_EnableWinKeys
*/
void VID_EnableWinKeys( qboolean enable )
{
	if( s_win95 )
		return; // requires at least WinNT 4.0

	if( enable )
	{
		if( !s_winkeys_hooked )
		{
			if( ( WinKeyHook = SetWindowsHookEx( 13, LLWinKeyHook, global_hInstance, 0 ) ) )
				s_winkeys_hooked = qtrue;
			else
				Com_Printf( "Failed to install winkey hook.\n" );
		}
	}
	else
	{
		if( s_winkeys_hooked )
		{
			UnhookWindowsHookEx( WinKeyHook );
			s_winkeys_hooked = qfalse;
		}
	}
}

/*
==========================================================================

DLL GLUE

==========================================================================
*/

// wsw : pb :  start of paste from Q3
static byte s_scantokey[128] =
{
	//  0           1       2       3       4       5       6       7
	//  8           9       A       B       C       D       E       F
	0, 27, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', K_BACKSPACE, 9, // 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', 13, K_CTRL, 'a', 's',      // 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',      // 2
	'b', 'n', 'm', ',', '.', '/', K_SHIFT, '*',
	K_ALT, ' ', K_CAPSLOCK, K_F1, K_F2, K_F3, K_F4, K_F5,   // 3
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, 0, K_HOME,
	K_UPARROW, K_PGUP, KP_MINUS, K_LEFTARROW, KP_5, K_RIGHTARROW, KP_PLUS, K_END, //4
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, 0, K_F11,
	K_F12, 0, 0, K_WIN, K_WIN, K_MENU, 0, 0,        // 5
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,        // 6
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0         // 7
};

/*
* IN_MapKey
* 
* Map from windows to quake keynums
*/
int IN_MapKey( int key )
{
	int result;
	int modified;
	qboolean is_extended;

	//	Com_Printf( "0x%x\n", key);

	modified = ( key >> 16 ) & 255;

	if( modified > 127 )
		return 0;

	if( key & ( 1 << 24 ) )
	{
		is_extended = qtrue;
	}
	else
	{
		is_extended = qfalse;
	}

	result = s_scantokey[modified];

	if( !is_extended )
	{
		switch( result )
		{
		case K_HOME:
			return KP_HOME;
		case K_UPARROW:
			return KP_UPARROW;
		case K_PGUP:
			return KP_PGUP;
		case K_LEFTARROW:
			return KP_LEFTARROW;
		case K_RIGHTARROW:
			return KP_RIGHTARROW;
		case K_END:
			return KP_END;
		case K_DOWNARROW:
			return KP_DOWNARROW;
		case K_PGDN:
			return KP_PGDN;
		case K_INS:
			return KP_INS;
		case K_DEL:
			return KP_DEL;
		default:
			return result;
		}
	}
	else
	{
		switch( result )
		{
		case K_PAUSE:
			return K_NUMLOCK;
		case 0x0D:
			return K_ENTER;
		case 0x2F:
			return KP_SLASH;
		case 0xAF:
			return KP_PLUS;
		}
		return result;
	}
}
// wsw : pb :  end of paste from Q3

static void AppActivate( BOOL fActive, BOOL minimize, BOOL destroy )
{
	Minimized = minimize;
	if( destroy )
		fActive = minimize = FALSE;

	Key_ClearStates();

	// we don't want to act like we're active if we're minimized
	if( fActive && !Minimized )
		ActiveApp = qtrue;
	else
		ActiveApp = qfalse;

	// minimize/restore mouse-capture on demand
	IN_Activate( ActiveApp );

	CL_SoundModule_Activate( ActiveApp );
	if( win_noalttab->integer )
		VID_EnableAltTab( !ActiveApp );
	if( win_nowinkeys->integer )
		VID_EnableWinKeys( !ActiveApp );

	GLimp_AppActivate( fActive, destroy );
}

/*
* MainWndProc
* 
* main window procedure
*/
LONG WINAPI MainWndProc(
						HWND hWnd,
						UINT uMsg,
						WPARAM wParam,
						LPARAM lParam )
{
	if( uMsg == MSH_MOUSEWHEEL )
	{
		if( mouse_wheel_type != MWHEEL_DINPUT )
		{
			mouse_wheel_type = MWHEEL_WM;
			if( ( ( int ) wParam ) > 0 )
			{
				Key_Event( K_MWHEELUP, qtrue, sys_msg_time );
				Key_Event( K_MWHEELUP, qfalse, sys_msg_time );
			}
			else
			{
				Key_Event( K_MWHEELDOWN, qtrue, sys_msg_time );
				Key_Event( K_MWHEELDOWN, qfalse, sys_msg_time );
			}
		}
		return DefWindowProcW( hWnd, uMsg, wParam, lParam );
	}

	switch( uMsg )
	{
	case WM_MOUSEWHEEL:
		/*
		** this chunk of code theoretically only works under NT4 and Win98
		** since this message doesn't exist under Win95
		*/
		if( mouse_wheel_type != MWHEEL_DINPUT )
		{
			mouse_wheel_type = MWHEEL_WM;
			if( ( short ) HIWORD( wParam ) > 0 )
			{
				Key_Event( K_MWHEELUP, qtrue, sys_msg_time );
				Key_Event( K_MWHEELUP, qfalse, sys_msg_time );
			}
			else
			{
				Key_Event( K_MWHEELDOWN, qtrue, sys_msg_time );
				Key_Event( K_MWHEELDOWN, qfalse, sys_msg_time );
			}
		}
		break;

	case WM_HOTKEY:
		return 0;

	case WM_INPUT:
		IN_RawInput_MouseRead( (HANDLE)lParam );
		break;

	case WM_CREATE:
		cl_hwnd = hWnd;
		cl_parent_hwnd = GetParent( hWnd );
		AppActivate( TRUE, FALSE, FALSE );
		MSH_MOUSEWHEEL = RegisterWindowMessage( "MSWHEEL_ROLLMSG" );
		return DefWindowProcW( hWnd, uMsg, wParam, lParam );

	case WM_PAINT:
		return DefWindowProcW( hWnd, uMsg, wParam, lParam );

	case WM_DESTROY:
		// let sound and input know about this?
		cl_hwnd = NULL;
		cl_parent_hwnd = NULL;
		AppActivate( FALSE, FALSE, TRUE );
		return DefWindowProcW( hWnd, uMsg, wParam, lParam );

	case WM_ACTIVATE:
		{
			int fActive, fMinimized;

			// KJB: Watch this for problems in fullscreen modes with Alt-tabbing.
			fActive = LOWORD( wParam );
			fMinimized = (BOOL) HIWORD( wParam );

			AppActivate( fActive != WA_INACTIVE, fMinimized, FALSE );

			if( fActive != WA_INACTIVE )
			{
				SetForegroundWindow( cl_hwnd );
				ShowWindow( cl_hwnd, SW_RESTORE );
			}
			else
			{
				if( vid_fullscreen->integer )
					ShowWindow( cl_hwnd, SW_MINIMIZE );
			}
		}
		return DefWindowProcW( hWnd, uMsg, wParam, lParam );

	case WM_MOVE:
		{
			int xPos, yPos;
			RECT r;
			int style;

			if( !vid_fullscreen->integer )
			{
				xPos = (short) LOWORD( lParam ); // horizontal position
				yPos = (short) HIWORD( lParam ); // vertical position

				r.left   = 0;
				r.top    = 0;
				r.right  = 1;
				r.bottom = 1;

				style = GetWindowLong( hWnd, GWL_STYLE );
				AdjustWindowRect( &r, style, FALSE );

				Cvar_SetValue( "vid_xpos", xPos + r.left );
				Cvar_SetValue( "vid_ypos", yPos + r.top );
				vid_xpos->modified = qfalse;
				vid_ypos->modified = qfalse;
				if( ActiveApp )
					IN_Activate( qtrue );
			}
		}
		return DefWindowProcW( hWnd, uMsg, wParam, lParam );

		// this is complicated because Win32 seems to pack multiple mouse events into
		// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_XBUTTONUP:
	case WM_XBUTTONDOWN:
		{
			int i, temp = 0;
			int mbuttons[] = { MK_LBUTTON, MK_RBUTTON, MK_MBUTTON,
				MK_XBUTTON1, MK_XBUTTON2, MK_XBUTTON3, MK_XBUTTON4, MK_XBUTTON5 };

			for( i = 0; i < mouse_buttons; i++ )
				if( wParam & mbuttons[i] )
					temp |= ( 1<<i );

			IN_MouseEvent( temp );
		}
		break;

	case WM_SYSCOMMAND:
		if( wParam == SC_SCREENSAVE )
			return 0;
		return DefWindowProcW( hWnd, uMsg, wParam, lParam );
	case WM_SYSKEYDOWN:
		if( wParam == VK_RETURN ) {
			Cbuf_ExecuteText( EXEC_APPEND, "toggle vid_fullscreen\n" );
			return 0;
		}
		if( wParam == VK_F10)
		{
			Key_Event( IN_MapKey( lParam ), qtrue, sys_msg_time );
			return 0;	// don't let the default handler activate the menu in windowed mode
		}

		// fall through
	case WM_KEYDOWN:
		Key_Event( IN_MapKey( lParam ), qtrue, sys_msg_time );
		break;

	case WM_SYSKEYUP:
		if( wParam == 18 )
		{ // ALT-key
			Key_Event( IN_MapKey( lParam ), qfalse, sys_msg_time );
			return 0;
		}
		// fall through
	case WM_KEYUP:
		Key_Event( IN_MapKey( lParam ), qfalse, sys_msg_time );
		break;

	case WM_CLOSE:
		Cbuf_ExecuteText( EXEC_NOW, "quit" );
		break;

	case WM_KILLFOCUS:
		AppFocused = qfalse;
		break;

	case WM_SETFOCUS:
		AppFocused = qtrue;
		break;

	// wsw : pb : new keyboard code using WM_CHAR event
	case WM_CHAR:
		{
			int key = (wParam <= 255) ? wParam : 0;
			Key_CharEvent( key, wParam );
		}
		break;

	case WM_ENTERSIZEMOVE:
		CL_SoundModule_Clear();
		break;

	default: // pass all unhandled messages to DefWindowProc
		return DefWindowProcW( hWnd, uMsg, wParam, lParam );
	}

	/* return 0 if handled message, 1 if not */
	return DefWindowProcW( hWnd, uMsg, wParam, lParam );
}

/*
** VID_GetWindowHandle - The sound module may require the handle when using directsound
*/
void *VID_GetWindowHandle( void )
{
	return ( void * )cl_hwnd;
}

/*
** VID_Sys_Init
*/
rserr_t VID_Sys_Init( int x, int y, int width, int height, qboolean fullScreen, qboolean wideScreen, qboolean verbose )
{
	extern cvar_t *vid_parentwid;
	return R_Init( global_hInstance, MainWndProc, STR_TO_POINTER( vid_parentwid->string ), 
		x, y, width, height, fullScreen, wideScreen, verbose );
}

/*
** VID_UpdateWindowPosAndSize
*/
void VID_UpdateWindowPosAndSize( int x, int y )
{
	RECT r;
	int style;
	int w, h;

	r.left   = 0;
	r.top    = 0;
	r.right  = viddef.width;
	r.bottom = viddef.height;

	style = GetWindowLong( cl_hwnd, GWL_STYLE );
	AdjustWindowRect( &r, style, FALSE );

	w = r.right - r.left;
	h = r.bottom - r.top;

	MoveWindow( cl_hwnd, x, y, w, h, TRUE );
}

/*
** VID_Front_f
*/
void VID_Front_f( void )
{
	SetWindowLong( cl_hwnd, GWL_EXSTYLE, WS_EX_TOPMOST );
	SetForegroundWindow( cl_hwnd );
}

/*
** VID_GetScreenSize
*/
qboolean VID_GetScreenSize( int *width, int *height )
{
	DEVMODE dm;
		
	memset( &dm, 0, sizeof( dm ) );
	dm.dmSize = sizeof( dm );

	EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &dm );

	*width = dm.dmPelsWidth;
	*height = dm.dmPelsHeight;

	return qtrue;
}

/*
** VID_FlashWindow
*
* Sends a flash message to inactive window
*/
void VID_FlashWindow( int count )
{
	FLASHWINFO fwi;

	if( ActiveApp )
		return;
	if( !count )
		return;

	ZeroMemory(&fwi, sizeof(fwi));

	fwi.cbSize = sizeof(fwi);
	fwi.dwFlags = FLASHW_ALL;
	fwi.dwTimeout = 0;
	fwi.hwnd = cl_hwnd;
	fwi.uCount = count;

	FlashWindowEx(&fwi);
}