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
#include "resource.h"

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL ( WM_MOUSELAST + 1 ) // message that will be supported by the OS
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
#define KF_UP           0x8000
#define LLKHF_UP        ( KF_UP >> 8 )

typedef struct {
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
HWND cl_hwnd;           // Main window handle for life of program

static HHOOK WinKeyHook;
static bool s_winkeys_hooked;
static bool s_alttab_disabled;
extern int64_t sys_msg_time;

static float vid_pixelRatio = 1.0f;

/*
** WinKeys system hook (taken from ezQuake)
*/
LRESULT CALLBACK LLWinKeyHook( int Code, WPARAM wParam, LPARAM lParam ) {
	PKBDLLHOOKSTRUCT p;

	p = (PKBDLLHOOKSTRUCT) lParam;

	if( ActiveApp ) {
		// we do not allow separate bindings for left and right keys yet
		switch( p->vkCode ) {
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
void VID_EnableAltTab( bool enable ) {
	if( enable ) {
		if( s_alttab_disabled ) {
			UnregisterHotKey( 0, 0 );
			UnregisterHotKey( 0, 1 );
			s_alttab_disabled = false;
		}
	} else {
		if( s_alttab_disabled ) {
			return;
		}

		RegisterHotKey( 0, 0, MOD_ALT, VK_TAB );
		RegisterHotKey( 0, 1, MOD_ALT, VK_RETURN );

		s_alttab_disabled = true;
	}
}

/*
* VID_EnableWinKeys
*/
void VID_EnableWinKeys( bool enable ) {
	if( enable ) {
		if( !s_winkeys_hooked ) {
			if( ( WinKeyHook = SetWindowsHookEx( 13, LLWinKeyHook, global_hInstance, 0 ) ) ) {
				s_winkeys_hooked = true;
			} else {
				Com_Printf( "Failed to install winkey hook.\n" );
			}
		}
	} else {
		if( s_winkeys_hooked ) {
			UnhookWindowsHookEx( WinKeyHook );
			s_winkeys_hooked = false;
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
	0, K_ESCAPE, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', K_BACKSPACE, 9, // 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', K_ENTER, K_LCTRL, 'a', 's',      // 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', K_LSHIFT, '\\', 'z', 'x', 'c', 'v',      // 2
	'b', 'n', 'm', ',', '.', '/', K_RSHIFT, '*',
	K_LALT, ' ', K_CAPSLOCK, K_F1, K_F2, K_F3, K_F4, K_F5,   // 3
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
int IN_MapKey( int key ) {
	int result;
	int modified;
	bool is_extended;

	//	Com_Printf( "0x%x\n", key);

	modified = ( key >> 16 ) & 255;

	if( modified > 127 ) {
		return 0;
	}

	if( key & ( 1 << 24 ) ) {
		is_extended = true;
	} else {
		is_extended = false;
	}

	result = s_scantokey[modified];

	if( !is_extended ) {
		switch( result ) {
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
	} else {
		switch( result ) {
			case K_PAUSE:
				return K_NUMLOCK;
			case 0x0D:
				return K_ENTER;
			case 0x2F:
				return KP_SLASH;
			case 0xAF:
				return KP_PLUS;
			case K_LALT:
				return K_RALT;
			case K_LCTRL:
				return K_RCTRL;
		}
		return result;
	}
}
// wsw : pb :  end of paste from Q3

static void AppActivate( BOOL fActive, BOOL minimize, BOOL destroy ) {
	int prevActiveApp;

	Minimized = minimize;
	if( destroy ) {
		fActive = minimize = FALSE;
	}

	CL_ClearInputState();

	// we don't want to act like we're active if we're minimized
	prevActiveApp = ActiveApp;
	if( fActive && !Minimized ) {
		ActiveApp = true;
	} else {
		ActiveApp = false;
	}

	// minimize/restore mouse-capture on demand
	IN_Activate( ActiveApp );

	if( prevActiveApp != ActiveApp ) {
		SCR_PauseCinematic( !ActiveApp );
		CL_SoundModule_Activate( ActiveApp );
	}

	if( win_noalttab->integer ) {
		VID_EnableAltTab( !ActiveApp );
	}
	if( win_nowinkeys->integer ) {
		VID_EnableWinKeys( !ActiveApp );
	}

	VID_AppActivate( fActive, minimize, destroy );
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
	LPARAM lParam ) {
	switch( uMsg ) {
		case WM_MOUSEWHEEL:
			/*
			** this chunk of code theoretically only works under NT4 and Win98
			** since this message doesn't exist under Win95
			*/
			if( mouse_wheel_type != MWHEEL_DINPUT ) {
				mouse_wheel_type = MWHEEL_WM;
				if( ( short ) HIWORD( wParam ) > 0 ) {
					Key_Event( K_MWHEELUP, true, sys_msg_time );
					Key_Event( K_MWHEELUP, false, sys_msg_time );
				} else {
					Key_Event( K_MWHEELDOWN, true, sys_msg_time );
					Key_Event( K_MWHEELDOWN, false, sys_msg_time );
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
			IN_WinIME_AssociateContext();
			AppActivate( TRUE, FALSE, FALSE );
			MSH_MOUSEWHEEL = RegisterWindowMessage( "MSWHEEL_ROLLMSG" );
			break;

		case WM_PAINT:
			break;

		case WM_DESTROY:
			// let sound and input know about this?
			cl_hwnd = NULL;
			IN_WinIME_AssociateContext();
			AppActivate( FALSE, FALSE, TRUE );
			break;

		case WM_ACTIVATE:
		{
			BOOL fActive, fMinimized;

			// KJB: Watch this for problems in fullscreen modes with Alt-tabbing.
			fActive = LOWORD( wParam ) != WA_INACTIVE;
			fMinimized = (BOOL) HIWORD( wParam );

			AppActivate( fActive, fMinimized, FALSE );

			if( fActive && !fMinimized ) {
				SetForegroundWindow( cl_hwnd );
				ShowWindow( cl_hwnd, SW_RESTORE );
			} else {
				if( vid_fullscreen->integer || fMinimized ) {
					ShowWindow( cl_hwnd, SW_MINIMIZE );
				}
			}
		}
		break;

		case WM_MOVE:
		{
			int xPos, yPos;

			if( !vid_fullscreen->integer ) {
				xPos = (short) LOWORD( lParam ); // horizontal position
				yPos = (short) HIWORD( lParam ); // vertical position

				Cvar_SetValue( "vid_xpos", xPos );
				Cvar_SetValue( "vid_ypos", yPos );

				vid_xpos->modified = false;
				vid_ypos->modified = false;
				if( ActiveApp ) {
					IN_Activate( true );
				}
			}
		}
		break;

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
				if( wParam & mbuttons[i] ) {
					temp |= ( 1 << i );
				}

			IN_MouseEvent( temp );
		}
		break;

		case WM_GETMINMAXINFO:
			// the default handler won't allow the window size to exceed desktop size
			// this also includes the title bar and edges, but we want the client area to
			// match the requested video mode, thus the handler override
			{
				int style;
				int x, y, w, h;
				MINMAXINFO *info = (MINMAXINFO *)lParam;
				RECT r;

				r.left = 0;
				r.top = 0;
				r.right = VID_GetWindowWidth();
				r.bottom = VID_GetWindowHeight();

				if( r.right == 0 || r.bottom == 0 ) {
					// dummy startup window
					break;
				}

				style = GetWindowLong( hWnd, GWL_STYLE );
				AdjustWindowRectEx( &r, style, FALSE, 0 );

				w = r.right - r.left;
				h = r.bottom - r.top;

				// get current window position
				GetWindowRect( hWnd, &r );
				x = r.left;
				y = r.top;

				info->ptMaxSize.x = w;
				info->ptMaxSize.y = h;
				info->ptMaxPosition.x = x;
				info->ptMaxPosition.y = y;
				info->ptMinTrackSize.x = w;
				info->ptMinTrackSize.y = h;
				info->ptMaxTrackSize.x = w;
				info->ptMaxTrackSize.y = h;
				return 0;
			}
			break;

		case WM_SYSCOMMAND:
			if( wParam == SC_SCREENSAVE ) {
				return 0;
			}
			break;

		case WM_SYSKEYDOWN:
			if( wParam == VK_RETURN ) {
				Cbuf_ExecuteText( EXEC_APPEND, "toggle vid_fullscreen\n" );
				return 0;
			}
			if( wParam == VK_F4 ) {
				Cbuf_ExecuteText( EXEC_NOW, "quit\n" );
				return 0;
			}
			if( wParam == VK_F10 ) {
				Key_Event( IN_MapKey( lParam ), true, sys_msg_time );
				return 0; // don't let the default handler activate the menu in windowed mode
			}
		// fall through
		case WM_KEYDOWN:
			if( wParam == VK_PROCESSKEY ) {
				return 0;
			}
			Key_Event( IN_MapKey( lParam ), true, sys_msg_time );
			break;

		case WM_SYSKEYUP:
			if( wParam == 18 ) { // ALT-key
				Key_Event( IN_MapKey( lParam ), false, sys_msg_time );
				return 0;
			}
		// fall through
		case WM_KEYUP:
			if( wParam == VK_PROCESSKEY ) {
				return 0;
			}
			Key_Event( IN_MapKey( lParam ), false, sys_msg_time );
			break;

		case WM_CLOSE:
			Cbuf_ExecuteText( EXEC_NOW, "quit" );
			break;

		case WM_KILLFOCUS:
			AppFocused = false;
			break;

		case WM_SETFOCUS:
			AppFocused = true;
			break;

		// wsw : pb : new keyboard code using WM_CHAR event
		case WM_CHAR:
		{
			int key = ( wParam <= 255 ) ? wParam : 0;
			Key_CharEvent( key, wParam );
		}
		break;

		case WM_ENTERSIZEMOVE:
			CL_SoundModule_Clear();
			break;

		case WM_IME_STARTCOMPOSITION:
			return 0;

		case WM_IME_SETCONTEXT:
			lParam &= ~ISC_SHOWUIALL;
			break;
	}

	if( uMsg == MSH_MOUSEWHEEL ) {
		if( mouse_wheel_type != MWHEEL_DINPUT ) {
			mouse_wheel_type = MWHEEL_WM;
			if( ( ( int ) wParam ) > 0 ) {
				Key_Event( K_MWHEELUP, true, sys_msg_time );
				Key_Event( K_MWHEELUP, false, sys_msg_time );
			} else {
				Key_Event( K_MWHEELDOWN, true, sys_msg_time );
				Key_Event( K_MWHEELDOWN, false, sys_msg_time );
			}
		}
	}

	/* return 0 if handled message, 1 if not */
	return DefWindowProcW( hWnd, uMsg, wParam, lParam );
}

/*
** VID_GetWindowHandle - The sound module may require the handle when using directsound
*/
void *VID_GetWindowHandle( void ) {
	return ( void * )cl_hwnd;
}

/*
** VID_Sys_Init
*/
rserr_t VID_Sys_Init( const char *applicationName, const char *screenshotsPrefix, int startupColor,
					  const int *iconXPM, void *parentWindow, bool verbose ) {
	return re.Init( applicationName, screenshotsPrefix, startupColor,
					IDI_APPICON_VALUE, iconXPM,
					global_hInstance, (void *)&MainWndProc, parentWindow,
					verbose );
}

/*
** VID_UpdateWindowPosAndSize
*/
void VID_UpdateWindowPosAndSize( int x, int y ) {
	RECT r;
	int style;
	int w, h;

	r.left   = 0;
	r.top    = 0;
	r.right  = VID_GetWindowWidth();
	r.bottom = VID_GetWindowHeight();

	style = GetWindowLong( cl_hwnd, GWL_STYLE );
	AdjustWindowRectEx( &r, style, FALSE, 0 );

	x = x + r.left;
	y = y + r.top;
	w = r.right - r.left;
	h = r.bottom - r.top;

	SetWindowPos( cl_hwnd, NULL, x, y, w, h, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSENDCHANGING );
}

/*
** VID_GetDefaultMode
*/
bool VID_GetDefaultMode( int *width, int *height ) {
	DEVMODE dm;

	memset( &dm, 0, sizeof( dm ) );
	dm.dmSize = sizeof( dm );

	EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &dm );

	if( !dm.dmPelsWidth || !dm.dmPelsHeight ) {
		return false;
	}

	*width = dm.dmPelsWidth;
	*height = dm.dmPelsHeight;

	return true;
}

/*
** VID_GetSysModes
*/
unsigned int VID_GetSysModes( vidmode_t *modes ) {
	DEVMODE dm;
	unsigned int i, count = 0, prevwidth = 0, prevheight = 0;

	memset( &dm, 0, sizeof( dm ) );
	dm.dmSize = sizeof( dm );

	for( i = 0; EnumDisplaySettings( NULL, i, &dm ); i++ ) {
		if( dm.dmBitsPerPel < 15 ) {
			continue;
		}

		if( ( dm.dmPelsWidth == prevwidth ) && ( dm.dmPelsHeight == prevheight ) ) {
			continue;
		}

		if( ChangeDisplaySettings( &dm, CDS_TEST | CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL ) {
			continue;
		}

		if( modes ) {
			modes[count].width = dm.dmPelsWidth;
			modes[count].height = dm.dmPelsHeight;
		}

		prevwidth = dm.dmPelsWidth;
		prevheight = dm.dmPelsHeight;

		count++;
	}

	return count;
}


/*
** VID_FlashWindow
*
* Sends a flash message to inactive window
*/
void VID_FlashWindow( int count ) {
	FLASHWINFO fwi;

	if( ActiveApp ) {
		return;
	}
	if( !count ) {
		return;
	}

	ZeroMemory( &fwi, sizeof( fwi ) );

	fwi.cbSize = sizeof( fwi );
	fwi.dwFlags = FLASHW_ALL;
	fwi.dwTimeout = 0;
	fwi.hwnd = cl_hwnd;
	fwi.uCount = count;

	FlashWindowEx( &fwi );
}

/*
** VID_SetProcessDPIAware
*
* Disables the automatic DPI scaling and gets the pixel ratio.
*/
void VID_SetProcessDPIAware( void ) {
	HINSTANCE user32Dll;
	HDC hdc;

	user32Dll = LoadLibrary( "user32.dll" );
	if( user32Dll ) {
		BOOL( WINAPI * pSetProcessDPIAware )( void ) = ( void * )GetProcAddress( user32Dll, "SetProcessDPIAware" );
		if( pSetProcessDPIAware ) {
			pSetProcessDPIAware();
		}
		FreeLibrary( user32Dll );
	}

	hdc = GetDC( NULL );
	if( hdc ) {
		vid_pixelRatio = ( float )GetDeviceCaps( hdc, LOGPIXELSY ) / 96.0f;
		ReleaseDC( NULL, hdc );
	}
}

/*
** VID_GetPixelRatio
*/
float VID_GetPixelRatio( void ) {
	return vid_pixelRatio;
}
