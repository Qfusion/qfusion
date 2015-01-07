/*
Copyright (C) 2015 SiPlus, Chasseur de bots

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

#include "../ref_gl/r_local.h"
#include "android_glw.h"

glwstate_t glw_state;

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( void )
{
}

/*
** GLimp_EndFrame
*/
void GLimp_EndFrame( void )
{
	if( glw_state.surface != EGL_NO_SURFACE )
		qeglSwapBuffers( glw_state.display, glw_state.surface );
}

/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
int GLimp_Init( const char *applicationName, void *hinstance, void *wndproc, void *parenthWnd )
{
	glw_state.window = ( ANativeWindow * )parenthWnd;
	return qtrue;
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
	if( glw_state.context != EGL_NO_CONTEXT )
	{
		qeglMakeCurrent( glw_state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
		qeglDestroyContext( glw_state.display, glw_state.context );
		glw_state.context = EGL_NO_CONTEXT;
	}
	if( glw_state.surface != EGL_NO_SURFACE )
	{
		qeglDestroySurface( glw_state.display, glw_state.surface );
		glw_state.surface = EGL_NO_SURFACE;
	}
	if( glw_state.pbufferSurface != EGL_NO_SURFACE )
	{
		qeglDestroySurface( glw_state.display, glw_state.pbufferSurface );
		glw_state.surface = EGL_NO_SURFACE;
	}
	if( glw_state.display != EGL_NO_DISPLAY )
	{
		qeglTerminate( glw_state.display );
		glw_state.display = EGL_NO_DISPLAY;
	}
}

/*
** GLimp_Android_ChooseVisual
*/
static void GLimp_Android_ChooseVisual( int colorSize, int depthSize, int depthEncoding, int stencilSize, int minSwapInterval )
{
	int attribs[] =
	{
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
		EGL_RED_SIZE, colorSize,
		EGL_GREEN_SIZE, colorSize,
		EGL_BLUE_SIZE, colorSize,
		EGL_ALPHA_SIZE, colorSize,
		EGL_DEPTH_SIZE, depthSize,
		EGL_STENCIL_SIZE, stencilSize,
		EGL_MIN_SWAP_INTERVAL, minSwapInterval,
		EGL_SAMPLES, 0,
		EGL_SAMPLE_BUFFERS, 0,
		( depthEncoding != EGL_DONT_CARE ) ? EGL_DEPTH_ENCODING_NV : EGL_NONE, depthEncoding,
		EGL_NONE
	};
	int numConfigs = 0;
	qeglChooseConfig( glw_state.display, attribs, &glw_state.config, 1, &numConfigs );
}

/*
** GLimp_Android_ChooseConfig
*/
static void GLimp_Android_ChooseConfig( void )
{
	int colorSizes[] = { 8, 4 }, colorSize;
	int depthSizes[] = { 24, 16, 16 }, depthSize;
	qboolean depthEncodingSupported = qfalse;
	int depthEncodings[] = { EGL_DONT_CARE, EGL_DEPTH_ENCODING_NONLINEAR_NV, EGL_DONT_CARE }, depthEncoding;
	int maxStencilSize = ( ( r_stencilbits->integer >= 8 ) ? 8 : 0 ), stencilSize;
	int minSwapIntervals[] = { 0, EGL_DONT_CARE }, minSwapInterval;
	const char *extensions = qglGetGLWExtensionsString();
	int i, j, k;

	if( extensions && strstr( extensions, "EGL_NV_depth_nonlinear" ) )
		depthEncodingSupported = qtrue;

	for( i = 0; i < sizeof( colorSizes ) / sizeof( colorSizes[0] ); i++ )
	{
		colorSize = colorSizes[i];

		for( j = 0; j < sizeof( depthSizes ) / sizeof( depthSizes[0] ); j++ )
		{
			depthEncoding = depthEncodings[j];
			if( ( depthEncoding != EGL_DONT_CARE ) && !depthEncodingSupported )
				continue;

			depthSize = depthSizes[j];

			for( stencilSize = maxStencilSize; stencilSize >= 0; stencilSize -= 8 )
			{
				for( k = 0; k < sizeof( minSwapIntervals ) / sizeof( minSwapIntervals[0] ); k++ )
				{
					GLimp_Android_ChooseVisual( colorSize, depthSize, depthEncoding, stencilSize, minSwapIntervals[k] );

					if( glw_state.config )
					{
						qeglGetConfigAttrib( glw_state.display, glw_state.config, EGL_MIN_SWAP_INTERVAL, &minSwapInterval );
						ri.Com_Printf( "Got colorbits %i, depthbits %i%s, stencilbits %i, min swap interval %i\n",
							colorSize * 4, depthSize,
							( depthEncoding == EGL_DEPTH_ENCODING_NONLINEAR_NV ) ? " (non-linear)" : "",
							stencilSize, minSwapInterval );
						glConfig.stencilBits = stencilSize;
						if( minSwapInterval > 0 )
							ri.Cvar_ForceSet( "r_swapinterval", "1" );
						return;
					}
				}
			}
		}
	}
}

/*
** GLimp_Android_CreateWindowSurface
*/
static void GLimp_Android_CreateWindowSurface( void )
{
	const char *extensions = qglGetGLWExtensionsString();
	cvar_t *stereo = ri.Cvar_Get( "cl_stereo", "0", 0 );

	ANativeWindow_setBuffersGeometry( glw_state.window, glConfig.width, glConfig.height, glw_state.format );
	glw_state.surface = EGL_NO_SURFACE;
	if( stereo->integer && extensions && strstr( extensions, "EGL_EXT_multiview_window" ) )
	{
		int attribs[] = { EGL_MULTIVIEW_VIEW_COUNT_EXT, 2, EGL_NONE };
		glw_state.surface = qeglCreateWindowSurface( glw_state.display, glw_state.config, glw_state.window, attribs );
		glConfig.stereoEnabled = ( glw_state.surface != EGL_NO_SURFACE ) ? qtrue : qfalse;
	}
	if( glw_state.surface == EGL_NO_SURFACE )
		glw_state.surface = qeglCreateWindowSurface( glw_state.display, glw_state.config, glw_state.window, NULL );
}

/*
** GLimp_InitGL
*/
static qboolean GLimp_InitGL( void )
{
	int format;
	EGLConfig config;
	const int pbufferAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	const int contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

	if( !glw_state.window )
	{
		ri.Com_Printf( "GLimp_InitGL() - ANativeWindow not found\n" );
		return qfalse;
	}

	glw_state.display = qeglGetDisplay( EGL_DEFAULT_DISPLAY );
	if( glw_state.display == EGL_NO_DISPLAY )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglGetDisplay failed\n" );
		return qfalse;
	}
	if( !qeglInitialize( glw_state.display, NULL, NULL ) )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglInitialize failed\n" );
		return qfalse;
	}

	GLimp_Android_ChooseConfig();
	if( !glw_state.config )
	{
		ri.Com_Printf( "GLimp_InitGL() - GLimp_Android_ChooseConfig failed\n" );
		return qfalse;
	}
	if ( !qeglGetConfigAttrib( glw_state.display, glw_state.config, EGL_NATIVE_VISUAL_ID, &glw_state.format ) )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglGetConfigAttrib failed\n" );
		return qfalse;
	}

	glw_state.pbufferSurface = qeglCreatePbufferSurface( glw_state.display, glw_state.config, pbufferAttribs );
	if( glw_state.pbufferSurface == EGL_NO_SURFACE )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglCreatePbufferSurface failed\n" );
		return qfalse;
	}

	GLimp_Android_CreateWindowSurface();
	if( glw_state.surface == EGL_NO_SURFACE )
	{
		ri.Com_Printf( "GLimp_InitGL() - GLimp_Android_CreateWindowSurface failed\n" );
		return qfalse;
	}

	glw_state.context = qeglCreateContext( glw_state.display, glw_state.config, EGL_NO_CONTEXT, contextAttribs );
	if( glw_state.context == EGL_NO_CONTEXT )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglCreateContext failed\n" );
		return qfalse;
	}

	if( !qeglMakeCurrent( glw_state.display, glw_state.surface, glw_state.surface, glw_state.context ) )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglMakeCurrent failed\n" );
		return qfalse;
	}

	glw_state.swapInterval = 1;

	return qtrue;
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int x, int y, int width, int height, int displayFrequency, 
	qboolean fullscreen, qboolean wideScreen )
{
	ri.Com_Printf( "Initializing OpenGL display\n" );
	if( glw_state.context != EGL_NO_CONTEXT )
		GLimp_Shutdown();
	glConfig.width = width;
	glConfig.height = height;
	glConfig.wideScreen = wideScreen;
	glConfig.fullScreen = qtrue;
	if( !GLimp_InitGL() )
	{
		ri.Com_Printf( "GLimp_SetMode() - GLimp_InitGL failed\n" );
		GLimp_Shutdown();
		return rserr_invalid_mode;
	}
	return rserr_ok;
}

/*
** GLimp_SetWindow
*/
qboolean GLimp_SetWindow( void *hinstance, void *wndproc, void *parenthWnd )
{
	EGLDisplay dpy;

	if( glw_state.window == ( ANativeWindow * )parenthWnd )
		return qtrue;

	glw_state.window = ( ANativeWindow * )parenthWnd;

	if( !parenthWnd )
	{
		if( glw_state.surface != EGL_NO_SURFACE )
		{
			qeglDestroySurface( glw_state.display, glw_state.surface );
			glw_state.surface = EGL_NO_SURFACE;
			qeglMakeCurrent( glw_state.display, glw_state.pbufferSurface, glw_state.pbufferSurface, glw_state.context );
		}
		return qtrue;
	}

	GLimp_Android_CreateWindowSurface();
	if( glw_state.surface == EGL_NO_SURFACE )
	{
		ri.Com_Printf( "GLimp_SetWindow() - GLimp_Android_CreateWindowSurface failed\n" );
		return qfalse;
	}

	if( !qeglMakeCurrent( glw_state.display, glw_state.surface, glw_state.surface, glw_state.context ) )
	{
		ri.Com_Printf( "GLimp_SetWindow() - eglMakeCurrent failed\n" );
		qeglDestroySurface( glw_state.display, glw_state.surface );
		glw_state.surface = EGL_NO_SURFACE;
		return qfalse;
	}

	dpy = qeglGetCurrentDisplay();
	if( dpy != EGL_NO_DISPLAY )
		qeglSwapInterval( dpy, glw_state.swapInterval );

	return qtrue;
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( qboolean active, qboolean destroy )
{
}

/*
** GLimp_GetGammaRamp
*/
qboolean GLimp_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp )
{
	return qfalse;
}

/*
** GLimp_SetGammaRamp
*/
void GLimp_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp )
{
}

/*
** GLimp_ScreenEnabled
*/
qboolean GLimp_ScreenEnabled( void )
{
	return ( glw_state.surface != EGL_NO_SURFACE ) ? qtrue : qfalse;
}

/*
** GLimp_SharedContext_Create
*/
qboolean GLimp_SharedContext_Create( void **context, void **surface )
{
	const int pbufferAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	const int contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLSurface pbuffer;
	EGLContext ctx;

	pbuffer = qeglCreatePbufferSurface( glw_state.display, glw_state.config, pbufferAttribs );
	if( pbuffer == EGL_NO_SURFACE )
		return qfalse;

	ctx = qeglCreateContext( glw_state.display, glw_state.config, glw_state.context, contextAttribs );
	if( !ctx )
	{
		qeglDestroySurface( glw_state.display, pbuffer );
		return qfalse;
	}

	*context = ctx;
	*surface = pbuffer;
	return qtrue;
}

/*
** GLimp_SharedContext_MakeCurrent
*/
qboolean GLimp_SharedContext_MakeCurrent( void *context, void *surface )
{
	return qeglMakeCurrent( glw_state.display, surface, surface, context ) ? qtrue : qfalse;
}

/*
** GLimp_SharedContext_Destroy
*/
void GLimp_SharedContext_Destroy( void *context, void *surface )
{
	qeglDestroyContext( glw_state.display, context );
	qeglDestroySurface( glw_state.display, surface );
}
