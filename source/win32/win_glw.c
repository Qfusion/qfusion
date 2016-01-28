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
#include "../ref_gl/r_local.h"
#include "win_glw.h"

#define WINDOW_STYLE	( WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE|WS_SYSMENU|WS_MINIMIZEBOX )

static int GLimp_InitGL( void );

glwstate_t glw_state;

/*
** GLimp_CreateWindow
*/
#define WITH_UTF8

#pragma warning( disable : 4055 )

static void GLimp_SetWindowSize( bool fullscreen )
{
	RECT r;
	int stylebits;
	int exstyle;
	int x = glw_state.win_x, y = glw_state.win_y;
	int width = glConfig.width, height = glConfig.height;
	HWND parentHWND = glw_state.parenthWnd;

	if( !glw_state.hWnd )
		return;

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

static void GLimp_CreateWindow( void )
{
	bool fullscreen = glConfig.fullScreen;
	HWND parentHWND = glw_state.parenthWnd;
#ifdef WITH_UTF8
	WNDCLASSW wc;
#else
	WNDCLASS  wc;
#endif

	Q_snprintfz( glw_state.windowClassName, sizeof( glw_state.windowClassName ), "%sWndClass", glw_state.applicationName );
#ifdef WITH_UTF8
	MultiByteToWideChar( CP_UTF8, 0, glw_state.windowClassName, -1, glw_state.windowClassNameW, sizeof( glw_state.windowClassNameW ) );
	glw_state.windowClassNameW[sizeof( glw_state.windowClassNameW )/sizeof( glw_state.windowClassNameW[0] ) - 1] = 0;
#endif

	/* Register the frame class */
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)glw_state.wndproc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = glw_state.hInstance;
	wc.hIcon         = LoadIcon( glw_state.hInstance, MAKEINTRESOURCE( glw_state.applicationIconResourceID ) );
	wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wc.lpszMenuName  = 0;
#ifdef WITH_UTF8
	wc.lpszClassName = (LPCWSTR)glw_state.windowClassNameW;
	if( !RegisterClassW( &wc ) )
#else
	wc.lpszClassName = (LPCSTR)glw_state.windowClassName;
	if( !RegisterClass( &wc ) )
#endif
		Sys_Error( "Couldn't register window class" );

	glw_state.hWnd =
#ifdef WITH_UTF8
		CreateWindowExW(
#else
		CreateWindowEx(
#endif
	        0,
#ifdef WITH_UTF8
	        glw_state.windowClassNameW,
	        glw_state.applicationNameW,
#else
	        glw_state.windowClassName,
	        glw_state.applicationName,
#endif
			0,
	        0, 0, 0, 0,
	        parentHWND,
	        NULL,
	        glw_state.hInstance,
	        NULL );

	if( !glw_state.hWnd )
		Sys_Error( "Couldn't create window" );

	GLimp_SetWindowSize( fullscreen );
}

/*
** GLimp_SetFullscreenMode
*/
rserr_t GLimp_SetFullscreenMode( int displayFrequency, bool fullscreen )
{
	glConfig.fullScreen = false;

	// do a CDS if needed
	if( fullscreen )
	{
		int a;
		DEVMODE dm;

		ri.Com_Printf( "...attempting fullscreen\n" );

		memset( &dm, 0, sizeof( dm ) );

		dm.dmSize = sizeof( dm );

		dm.dmPelsWidth  = glConfig.width;
		dm.dmPelsHeight = glConfig.height;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

		if( displayFrequency > 0 )
		{
			dm.dmFields |= DM_DISPLAYFREQUENCY;
			dm.dmDisplayFrequency = displayFrequency;
			ri.Com_Printf( "...using display frequency %i\n", dm.dmDisplayFrequency );
		}

		ri.Com_Printf( "...calling CDS: " );
		a = ChangeDisplaySettings( &dm, CDS_FULLSCREEN );
		if( a == DISP_CHANGE_SUCCESSFUL )
		{
			ri.Com_Printf( "ok\n" );
			glConfig.fullScreen = true;
			GLimp_SetWindowSize( true );
			return rserr_ok;
		}

		ri.Com_Printf( "failed: %x\n", a );
		return rserr_invalid_fullscreen;
	}

	ChangeDisplaySettings( 0, 0 );
	GLimp_SetWindowSize( false );

	return rserr_ok;
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int x, int y, int width, int height, int displayFrequency, bool fullscreen, bool stereo )
{
	const char *win_fs[] = { "W", "FS" };

	ri.Com_Printf( "Setting video mode:" );

	// disable fullscreen if rendering to a parent window
	if( glw_state.parenthWnd ) {
		RECT parentWindowRect;

		fullscreen = false;

		GetWindowRect( glw_state.parenthWnd, &parentWindowRect );
		width = parentWindowRect.right - parentWindowRect.left;
		height = parentWindowRect.bottom - parentWindowRect.top;
	}

	ri.Com_Printf( " %d %d %s\n", width, height, win_fs[fullscreen] );

	// destroy the existing window
	if( glw_state.hWnd )
	{
		GLimp_Shutdown();
	}

	glw_state.win_x = x;
	glw_state.win_y = y;

	glConfig.width = width;
	glConfig.height = height;
	glConfig.fullScreen = ( fullscreen ? GLimp_SetFullscreenMode( displayFrequency, fullscreen ) == rserr_ok : false );
	glConfig.stereoEnabled = stereo;

	GLimp_CreateWindow();

	// init all the gl stuff for the window
	if( !GLimp_InitGL() ) {
		ri.Com_Printf( "GLimp_CreateWindow() - GLimp_InitGL failed\n" );
		return false;
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
	if( qwglMakeCurrent && !qwglMakeCurrent( NULL, NULL ) )
		ri.Com_Printf( "ref_gl::R_Shutdown() - wglMakeCurrent failed\n" );
	if( glw_state.hGLRC )
	{
		if( qwglDeleteContext && !qwglDeleteContext( glw_state.hGLRC ) )
			ri.Com_Printf( "ref_gl::R_Shutdown() - wglDeleteContext failed\n" );
		glw_state.hGLRC = NULL;
	}
	if( glw_state.hDC )
	{
		if( !ReleaseDC( glw_state.hWnd, glw_state.hDC ) )
			ri.Com_Printf( "ref_gl::R_Shutdown() - ReleaseDC failed\n" );
		glw_state.hDC   = NULL;
	}
	if( glw_state.hWnd )
	{
		ShowWindow( glw_state.hWnd, SW_HIDE );
		DestroyWindow( glw_state.hWnd );
		glw_state.hWnd = NULL;
	}

#ifdef WITH_UTF8
	UnregisterClassW( glw_state.windowClassNameW, glw_state.hInstance );
#else
	UnregisterClass( glw_state.windowClassName, glw_state.hInstance );
#endif

	if( glConfig.fullScreen )
	{
		ChangeDisplaySettings( 0, 0 );
		glConfig.fullScreen = false;
	}

	if( glw_state.applicationName )
	{
		free( glw_state.applicationName );
		glw_state.applicationName = NULL;
	}

	if( glw_state.applicationNameW )
	{
		free( glw_state.applicationNameW );
		glw_state.applicationNameW = NULL;
	}

	glw_state.applicationIconResourceID = 0;

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
int GLimp_Init( const char *applicationName, void *hinstance, void *wndproc, void *parenthWnd, 
	int iconResource, const int *iconXPM )
{
	size_t applicationNameSize = strlen( applicationName ) + 1;
	// save off hInstance and wndproc
	glw_state.applicationName = malloc( applicationNameSize );
	memcpy( glw_state.applicationName, applicationName, applicationNameSize );
#ifdef WITH_UTF8
	glw_state.applicationNameW = malloc( applicationNameSize * sizeof( WCHAR ) ); // may be larger than needed, but not smaller
	MultiByteToWideChar( CP_UTF8, 0, applicationName, -1, glw_state.applicationNameW, applicationNameSize * sizeof( WCHAR ) );
	glw_state.applicationNameW[applicationNameSize - 1] = 0;
#endif
	glw_state.hInstance = ( HINSTANCE ) hinstance;
	glw_state.wndproc = wndproc;
	glw_state.parenthWnd = ( HWND )parenthWnd;
	glw_state.applicationIconResourceID = iconResource;

	return true;
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

	if( r_stencilbits->integer == 8 || r_stencilbits->integer == 16 )
		pfd.cStencilBits = r_stencilbits->integer;

	/*
	** set PFD_STEREO if necessary
	*/
	if( glConfig.stereoEnabled )
	{
		ri.Com_DPrintf( "...attempting to use stereo\n" );
		pfd.dwFlags |= PFD_STEREO;
	}

	/*
	** Get a DC for the specified window
	*/
	if( glw_state.hDC != NULL )
		ri.Com_Printf( "GLimp_Init() - non-NULL DC exists\n" );

	if( ( glw_state.hDC = GetDC( glw_state.hWnd ) ) == NULL )
	{
		ri.Com_Printf( "GLimp_Init() - GetDC failed\n" );
		return false;
	}

	if( ( pixelformat = ChoosePixelFormat( glw_state.hDC, &pfd ) ) == 0 )
	{
		ri.Com_Printf( "GLimp_Init() - ChoosePixelFormat failed\n" );
		return false;
	}
	if( SetPixelFormat( glw_state.hDC, pixelformat, &pfd ) == FALSE )
	{
		ri.Com_Printf( "GLimp_Init() - SetPixelFormat failed\n" );
		return false;
	}
	DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );

	glConfig.stencilBits = pfd.cStencilBits;

	/*
	** report if stereo is desired but unavailable
	*/
	if( !( pfd.dwFlags & PFD_STEREO ) && glConfig.stereoEnabled )
	{
		ri.Com_Printf( "...failed to select stereo pixel format\n" );
		glConfig.stereoEnabled = false;
	}

	/*
	** startup the OpenGL subsystem by creating a context and making
	** it current
	*/
	if( ( glw_state.hGLRC = qwglCreateContext( glw_state.hDC ) ) == 0 )
	{
		ri.Com_Printf( "GLimp_Init() - qwglCreateContext failed\n" );
		goto fail;
	}

	if( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) )
	{
		ri.Com_Printf( "GLimp_Init() - qwglMakeCurrent failed\n" );
		goto fail;
	}

	/*
	** print out PFD specifics
	*/
	ri.Com_Printf( "GL PFD: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", ( int ) pfd.cColorBits, ( int ) pfd.cDepthBits, ( int )pfd.cStencilBits );

	return true;

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
	return false;
}

/*
** GLimp_UpdateGammaRamp
*/
bool GLimp_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp )
{
	unsigned short ramp256[3*256];

	if( stride < 256 )
	{
		// only supports gamma ramps with 256 mappings per channel
		return false;
	}

	if( qwglGetDeviceGammaRamp3DFX )
	{
		if( qwglGetDeviceGammaRamp3DFX( glw_state.hDC, ramp256 ) )
		{
			*psize = 256;
			memcpy( ramp,          ramp256,       256*sizeof(*ramp) );
			memcpy( ramp+  stride, ramp256+  256, 256*sizeof(*ramp) );
			memcpy( ramp+2*stride, ramp256+2*256, 256*sizeof(*ramp) );
			return true;
		}
	}

	if( GetDeviceGammaRamp( glw_state.hDC, ramp256 ) )
	{
		*psize = 256;
		memcpy( ramp,          ramp256,       256*sizeof(*ramp) );
		memcpy( ramp+  stride, ramp256+  256, 256*sizeof(*ramp) );
		memcpy( ramp+2*stride, ramp256+2*256, 256*sizeof(*ramp) );
		return true;
	}

	return false;
}

/*
** GLimp_SetGammaRamp
*/
void GLimp_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp )
{
	unsigned short ramp256[3*256];

	if( size != 256 )
		return;

	memcpy( ramp256,       ramp         , size*sizeof(*ramp));
	memcpy( ramp256+  256, ramp+  stride, size*sizeof(*ramp));
	memcpy( ramp256+2*256, ramp+2*stride, size*sizeof(*ramp));

	if( qwglGetDeviceGammaRamp3DFX )
		qwglSetDeviceGammaRamp3DFX( glw_state.hDC, ramp256 );
	else
		SetDeviceGammaRamp( glw_state.hDC, ramp256 );
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
	if( !qwglSwapBuffers( glw_state.hDC ) )
		Sys_Error( "GLimp_EndFrame() - SwapBuffers() failed!" );
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( bool active, bool destroy )
{
	if( active )
	{
		ri.Cvar_Set( "gl_drawbuffer", "GL_BACK" );
		SetForegroundWindow( glw_state.hWnd );
		ShowWindow( glw_state.hWnd, SW_RESTORE );
	}
	else
	{
		if( glConfig.fullScreen )
		{
			ri.Cvar_Set( "gl_drawbuffer", "GL_NONE" );
			ShowWindow( glw_state.hWnd, SW_MINIMIZE );
		}
		else
		{
			if( destroy )
				ri.Cvar_Set( "gl_drawbuffer", "GL_NONE" );
		}
	}
}

/*
** GLimp_SetWindow
*/
rserr_t GLimp_SetWindow( void *hinstance, void *wndproc, void *parenthWnd, bool *surfaceChangePending )
{
	if( surfaceChangePending )
		*surfaceChangePending = false;

	return rserr_ok; // surface cannot be lost
}

/*
** GLimp_RenderingEnabled
*/
bool GLimp_RenderingEnabled( void )
{
	return true;
}

/*
** GLimp_SetSwapInterval
*/
void GLimp_SetSwapInterval( int swapInterval )
{
	if( qwglSwapIntervalEXT )
		qwglSwapIntervalEXT( swapInterval );
}

/*
** GLimp_MakeCurrent
*/
bool GLimp_MakeCurrent( void *context, void *surface )
{
	if( qwglMakeCurrent && !qwglMakeCurrent( glw_state.hDC, context ) ) {
		return false;
	}
	return true;
}

/*
** GLimp_EnableMultithreadedRendering
*/
void GLimp_EnableMultithreadedRendering( bool enable )
{
}

/*
** GLimp_GetWindowSurface
*/
void *GLimp_GetWindowSurface( bool *renderable )
{
	if( renderable )
		*renderable = true;
	return NULL;
}

/*
** GLimp_UpdatePendingWindowSurface
*/
void GLimp_UpdatePendingWindowSurface( void )
{
}

/*
** GLimp_SharedContext_Create
*/
bool GLimp_SharedContext_Create( void **context, void **surface )
{
	HGLRC ctx = qwglCreateContext( glw_state.hDC );
	if( !ctx ) {
		return false;
	}

	qwglShareLists( glw_state.hGLRC, ctx );
	*context = ctx;
	if( surface )
		*surface = NULL;
	return true;
}

/*
** GLimp_SharedContext_Destroy
*/
void GLimp_SharedContext_Destroy( void *context, void *surface )
{
	if( qwglDeleteContext ) {
		qwglDeleteContext( context );
	}
}
