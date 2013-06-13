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
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
**
*/
#include <assert.h>
#include "winquake.h"
#include "../qcommon/qcommon.h"
#include "../ref_gl/r_glimp.h"
#include "win_glw.h"
#include "resource.h"

#define WINDOW_STYLE	( WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE|WS_SYSMENU )

static int GLimp_InitGL( void );

glwstate_t glw_state;

/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME   APPLICATION "WndClass"

#pragma warning( disable : 4055 )

static void VID_SetWindowSize( qboolean fullscreen )
{
	RECT r;
	int stylebits;
	int exstyle;
	int x = glw_state.win_x, y = glw_state.win_y;
	int width = glConfig.width, height = glConfig.height;
	HWND parentHWND = glw_state.parenthWnd;

	if( fullscreen )
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = ( WS_POPUP|WS_VISIBLE );
		parentHWND = NULL;
	}
	else if( parentHWND )
	{
		exstyle = 0;
		stylebits = WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		stylebits = WINDOW_STYLE;
	}

	r.left = 0;
	r.top = 0;
	r.right  = width;
	r.bottom = height;

	AdjustWindowRect( &r, stylebits, FALSE );

	width = r.right - r.left;
	height = r.bottom - r.top;

	if( fullscreen )
	{
		x = 0;
		y = 0;
	}
	else if( parentHWND )
	{
		RECT parentWindowRect;

		GetWindowRect( parentHWND, &parentWindowRect );

		// share centre with the parent window
		x = (parentWindowRect.right - parentWindowRect.left - width) / 2;
		y = (parentWindowRect.bottom - parentWindowRect.top - height) / 2;
	}

	SetActiveWindow( glw_state.hWnd );

	SetWindowLong( glw_state.hWnd, GWL_EXSTYLE, exstyle );
	SetWindowLong( glw_state.hWnd, GWL_STYLE, stylebits );

	SetWindowPos( glw_state.hWnd, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED );

	ShowWindow( glw_state.hWnd, SW_SHOW );
	UpdateWindow( glw_state.hWnd );

	SetForegroundWindow( glw_state.hWnd );
	SetFocus( glw_state.hWnd );
}

static qboolean VID_CreateWindow( void )
{
#define WITH_UTF8 // FIXME: turn non-Unicode code into a runtime fallback for Win9x
#ifndef WITH_UTF8
	WNDCLASS  wc;
#else
	WNDCLASSW wc;
	qboolean fullscreen = glConfig.fullScreen;
	HWND parentHWND = glw_state.parenthWnd;

	/* determine buffer size required for the unicode string */
	WCHAR windowClassName[sizeof(WINDOW_CLASS_NAME)] = L"";
	MultiByteToWideChar( CP_ACP, 0, WINDOW_CLASS_NAME, -1, windowClassName, sizeof( windowClassName )-1 );
#endif

	/* Register the frame class */
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)glw_state.wndproc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = glw_state.hInstance;
	wc.hIcon         = LoadIcon( glw_state.hInstance, MAKEINTRESOURCE( IDI_APPICON_VALUE ) );
	wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)GetStockObject( GRAY_BRUSH );
	wc.lpszMenuName  = 0;
#ifdef WITH_UTF8
	wc.lpszClassName = (LPCWSTR)windowClassName;
	if( !RegisterClassW( &wc ) )
#else
	wc.lpszClassName = (LPCSTR)WINDOW_CLASS_NAME;
	if( !RegisterClass( &wc ) )
#endif
		Com_Error( ERR_FATAL, "Couldn't register window class" );

	glw_state.hWnd = CreateWindowEx(
	        0,
	        WINDOW_CLASS_NAME,
	        APPLICATION,
			0,
	        0, 0, 0, 0,
	        parentHWND,
	        NULL,
	        glw_state.hInstance,
	        NULL );

	if( !glw_state.hWnd )
		Com_Error( ERR_FATAL, "Couldn't create window" );

	VID_SetWindowSize( fullscreen );

	// init all the gl stuff for the window
	if( !GLimp_InitGL() )
	{
		Com_Printf( "VID_CreateWindow() - GLimp_InitGL failed\n" );
		return qfalse;
	}

	if( glw_state.parenthWnd )
		PostMessage( glw_state.parenthWnd, UWM_APPACTIVE, WA_ACTIVE, 0 );

	return qtrue;
}

/*
** VID_SetFullscreenMode
*/
qboolean VID_SetFullscreenMode( qboolean fullscreen )
{
	// do a CDS if needed
	if( fullscreen )
	{
		int a;
		DEVMODE dm;
		HDC hdc;
		int bitspixel;

		Com_DPrintf( "...attempting fullscreen\n" );

		memset( &dm, 0, sizeof( dm ) );

		dm.dmSize = sizeof( dm );

		dm.dmPelsWidth  = glConfig.width;
		dm.dmPelsHeight = glConfig.height;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

		hdc = GetDC( NULL );
		bitspixel = GetDeviceCaps( hdc, BITSPIXEL );

		Com_DPrintf( "...using desktop display depth of %d\n", bitspixel );

		ReleaseDC( 0, hdc );

		if( vid_displayfrequency->integer > 0 )
		{
			dm.dmFields |= DM_DISPLAYFREQUENCY;
			dm.dmDisplayFrequency = vid_displayfrequency->integer;
			Com_DPrintf( "...using display frequency %i\n", dm.dmDisplayFrequency );
		}

		Com_DPrintf( "...calling CDS: " );
		a = ChangeDisplaySettings( &dm, CDS_FULLSCREEN );
		if( a == DISP_CHANGE_SUCCESSFUL )
		{
			Com_DPrintf( "ok\n" );

			if( glw_state.hWnd ) {
				VID_SetWindowSize( qtrue );
			}
			return qtrue;
		}
		else
		{
			Com_DPrintf( "failed: %i\n", a );

			Com_DPrintf( "...calling CDS assuming dual monitors:" );

			dm.dmPelsWidth = glConfig.width * 2;
			dm.dmPelsHeight = glConfig.height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

			if( vid_displayfrequency->integer > 0 )
			{
				dm.dmFields |= DM_DISPLAYFREQUENCY;
				dm.dmDisplayFrequency = vid_displayfrequency->integer;
				Com_DPrintf( "...using display frequency %i\n", dm.dmDisplayFrequency );
			}

			/*
			** our first CDS failed, so maybe we're running on some weird dual monitor
			** system
			*/
			if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				Com_DPrintf( " failed\n" );

				Com_DPrintf( "...setting windowed mode\n" );

				ChangeDisplaySettings( 0, 0 );

				if( glw_state.hWnd ) {
					VID_SetWindowSize( qfalse );
				}
				return qfalse;
			}
			else
			{
				Com_DPrintf( " ok\n" );

				if( glw_state.hWnd ) {
					VID_SetWindowSize( qtrue );
				}
				return qtrue;
			}
		}
	}
	else
	{
		Com_DPrintf( "...setting windowed mode\n" );

		ChangeDisplaySettings( 0, 0 );

		if( glw_state.hWnd ) {
			VID_SetWindowSize( qfalse );
		}
	}

	return qfalse;
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int x, int y, int width, int height, qboolean fullscreen, qboolean wideScreen )
{
	const char *win_fs[] = { "W", "FS" };

	// check whether we can toggle fullscreen without a vid_restart
	if( glw_state.hWnd ) {
		if( glConfig.width == width && glConfig.height == height && fullscreen != glConfig.fullScreen ) {
			glConfig.fullScreen = VID_SetFullscreenMode( fullscreen );

			if( glConfig.fullScreen == fullscreen ) {
				VID_SetWindowSize( fullscreen );
				return rserr_ok;
			}

			return rserr_restart_required;
		}
	}

	Com_Printf( "Initializing OpenGL display\n" );

	Com_Printf( "...setting mode:" );

	// disable fullscreen if rendering to a parent window
	if( glw_state.parenthWnd ) {
		RECT parentWindowRect;

		fullscreen = qfalse;
		wideScreen = qfalse;

		GetWindowRect( glw_state.parenthWnd, &parentWindowRect );
		width = parentWindowRect.right - parentWindowRect.left;
		height = parentWindowRect.bottom - parentWindowRect.top;
	}

	Com_Printf( " %d %d %s\n", width, height, win_fs[fullscreen] );

	// destroy the existing window
	if( glw_state.hWnd )
	{
		GLimp_Shutdown();
	}

	glw_state.win_x = x;
	glw_state.win_y = y;

	glConfig.width = width;
	glConfig.height = height;
	glConfig.wideScreen = wideScreen;
	glConfig.fullScreen = VID_SetFullscreenMode( fullscreen );

	if( !VID_CreateWindow() ) {
		return rserr_invalid_mode;
	}

	return ( fullscreen == glConfig.fullScreen ? rserr_ok : rserr_invalid_fullscreen );
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown( void )
{
	if( glw_state.parenthWnd )
		PostMessage( glw_state.parenthWnd, UWM_APPACTIVE, WA_INACTIVE, 0 );

	if( qwglMakeCurrent && !qwglMakeCurrent( NULL, NULL ) )
		Com_Printf( "ref_gl::R_Shutdown() - wglMakeCurrent failed\n" );
	if( glw_state.hGLRC )
	{
		if( qwglDeleteContext && !qwglDeleteContext( glw_state.hGLRC ) )
			Com_Printf( "ref_gl::R_Shutdown() - wglDeleteContext failed\n" );
		glw_state.hGLRC = NULL;
	}
	if( glw_state.hDC )
	{
		if( !ReleaseDC( glw_state.hWnd, glw_state.hDC ) )
			Com_Printf( "ref_gl::R_Shutdown() - ReleaseDC failed\n" );
		glw_state.hDC   = NULL;
	}
	if( glw_state.hWnd )
	{
		ShowWindow( glw_state.hWnd, SW_HIDE );
		DestroyWindow( glw_state.hWnd );
		glw_state.hWnd = NULL;
	}

	UnregisterClass( WINDOW_CLASS_NAME, glw_state.hInstance );

	if( glConfig.fullScreen )
	{
		ChangeDisplaySettings( 0, 0 );
		glConfig.fullScreen = qfalse;
	}

	glw_state.win_x = 0;
	glw_state.win_y = 0;

	glConfig.width = 0;
	glConfig.height = 0;
}


/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
int GLimp_Init( void *hinstance, void *wndproc, void *parenthWnd )
{
	// save off hInstance and wndproc
	glw_state.hInstance = ( HINSTANCE ) hinstance;
	glw_state.wndproc = wndproc;
	glw_state.parenthWnd = ( HWND )parenthWnd;

	return qtrue;
}

static int GLimp_InitGL( void )
{
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof( PIXELFORMATDESCRIPTOR ), // size of this pfd
		1,                      // version number
		PFD_DRAW_TO_WINDOW |    // support window
		PFD_SUPPORT_OPENGL |    // support OpenGL
		PFD_DOUBLEBUFFER,       // double buffered
		PFD_TYPE_RGBA,          // RGBA type
		32,                     // 32-bit color depth
		0, 0, 0, 0, 0, 0,       // color bits ignored
		0,                      // no alpha buffer
		0,                      // shift bit ignored
		0,                      // no accumulation buffer
		0, 0, 0, 0,             // accum bits ignored
		24,                     // 32-bit z-buffer
		0,                      // no stencil buffer
		0,                      // no auxiliary buffer
		PFD_MAIN_PLANE,         // main layer
		0,                      // reserved
		0, 0, 0                 // layer masks ignored
	};
	int pixelformat;
	cvar_t *stereo;

	stereo = Cvar_Get( "cl_stereo", "0", 0 );

	pfd.cStencilBits = max( 0, r_stencilbits->integer );

	if( pfd.cStencilBits != 0 )
		glConfig.stencilEnabled = qtrue;
	else
		glConfig.stencilEnabled = qfalse;

	/*
	** set PFD_STEREO if necessary
	*/
	if( stereo->integer != 0 )
	{
		Com_DPrintf( "...attempting to use stereo\n" );
		pfd.dwFlags |= PFD_STEREO;
		glConfig.stereoEnabled = qtrue;
	}
	else
	{
		glConfig.stereoEnabled = qfalse;
	}

	/*
	** Get a DC for the specified window
	*/
	if( glw_state.hDC != NULL )
		Com_Printf( "GLimp_Init() - non-NULL DC exists\n" );

	if( ( glw_state.hDC = GetDC( glw_state.hWnd ) ) == NULL )
	{
		Com_Printf( "GLimp_Init() - GetDC failed\n" );
		return qfalse;
	}

	if( ( pixelformat = ChoosePixelFormat( glw_state.hDC, &pfd ) ) == 0 )
	{
		Com_Printf( "GLimp_Init() - ChoosePixelFormat failed\n" );
		return qfalse;
	}
	if( SetPixelFormat( glw_state.hDC, pixelformat, &pfd ) == FALSE )
	{
		Com_Printf( "GLimp_Init() - SetPixelFormat failed\n" );
		return qfalse;
	}
	DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );

	/*
	** report if stereo is desired but unavailable
	*/
	if( !( pfd.dwFlags & PFD_STEREO ) && ( stereo->integer != 0 ) )
	{
		Com_Printf( "...failed to select stereo pixel format\n" );
		Cvar_SetValue( "cl_stereo", 0 );
		glConfig.stereoEnabled = qfalse;
	}

	/*
	** startup the OpenGL subsystem by creating a context and making
	** it current
	*/
	if( ( glw_state.hGLRC = qwglCreateContext( glw_state.hDC ) ) == 0 )
	{
		Com_Printf( "GLimp_Init() - qwglCreateContext failed\n" );
		goto fail;
	}

	if( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) )
	{
		Com_Printf( "GLimp_Init() - qwglMakeCurrent failed\n" );
		goto fail;
	}

	/*
	** print out PFD specifics
	*/
	Com_Printf( "GL PFD: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", ( int ) pfd.cColorBits, ( int ) pfd.cDepthBits, ( int )pfd.cStencilBits );

	return qtrue;

fail:
	if( glw_state.hGLRC )
	{
		qwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}

	if( glw_state.hDC )
	{
		ReleaseDC( glw_state.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}
	return qfalse;
}

/*
** GLimp_UpdateGammaRamp
*/
qboolean GLimp_GetGammaRamp( size_t stride, unsigned short *ramp )
{
	if( qwglGetDeviceGammaRamp3DFX )
	{
		if( qwglGetDeviceGammaRamp3DFX( glw_state.hDC, ramp ) )
			return qtrue;
	}

	if( GetDeviceGammaRamp( glw_state.hDC, ramp ) )
		return qtrue;

	return qfalse;
}

/*
** GLimp_SetGammaRamp
*/
void GLimp_SetGammaRamp( size_t stride, unsigned short *ramp )
{
	if( qwglGetDeviceGammaRamp3DFX )
		qwglSetDeviceGammaRamp3DFX( glw_state.hDC, ramp );
	else
		SetDeviceGammaRamp( glw_state.hDC, ramp );
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( void )
{

}

/*
** GLimp_EndFrame
**
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame( void )
{
	int err;

	err = qglGetError();
	assert( err == GL_NO_ERROR );

	if( Q_stricmp( gl_drawbuffer->string, "GL_BACK" ) == 0 )
	{
		if( !qwglSwapBuffers( glw_state.hDC ) )
			Com_Error( ERR_FATAL, "GLimp_EndFrame() - SwapBuffers() failed!" );
	}
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( qboolean active, qboolean destroy )
{
	if( active )
	{
		Cvar_Set( "gl_drawbuffer", "GL_BACK" );
		SetForegroundWindow( glw_state.hWnd );
		ShowWindow( glw_state.hWnd, SW_RESTORE );
	}
	else
	{
		if( glConfig.fullScreen )
		{
			Cvar_Set( "gl_drawbuffer", "GL_NONE" );
			ShowWindow( glw_state.hWnd, SW_MINIMIZE );
		}
		else
		{
			if( destroy )
				Cvar_Set( "gl_drawbuffer", "GL_NONE" );
		}
	}
}
