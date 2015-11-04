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
int GLimp_Init( const char *applicationName, void *hinstance, void *wndproc, void *parenthWnd,
	int iconResource, const int *iconXPM )
{
	glw_state.window = ( ANativeWindow * )parenthWnd;
	return true;
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
static EGLConfig GLimp_Android_ChooseVisual( int colorSize, int depthSize, int depthEncoding, int stencilSize, int minSwapInterval )
{
	int attribs[] =
	{
		/*  0 */	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		/*  2 */	EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
		/*  4 */	EGL_RED_SIZE, colorSize,
		/*  6 */	EGL_GREEN_SIZE, colorSize,
		/*  8 */	EGL_BLUE_SIZE, colorSize,
		/* 10 */	EGL_ALPHA_SIZE, colorSize,
		/* 12 */	EGL_DEPTH_SIZE, depthSize,
		/* 14 */	EGL_STENCIL_SIZE, stencilSize,
		/* 16 */	EGL_SAMPLES, 0,
		/* 18 */	EGL_SAMPLE_BUFFERS, 0,
		/* 20 */	EGL_NONE, EGL_DONT_CARE,
					EGL_NONE, EGL_DONT_CARE,
					EGL_NONE
	};
	int addAttrib = 20;
	int numConfigs = 0;
	EGLConfig config = NULL;

	if( minSwapInterval != EGL_DONT_CARE )
	{
		attribs[addAttrib++] = EGL_MIN_SWAP_INTERVAL;
		attribs[addAttrib++] = minSwapInterval;
	}

	if( depthEncoding != EGL_DONT_CARE )
	{
		attribs[addAttrib++] = EGL_DEPTH_ENCODING_NV;
		attribs[addAttrib++] = depthEncoding;
	}

	if( qeglChooseConfig( glw_state.display, attribs, &config, 1, &numConfigs ) )
		return numConfigs ? config : 0;

	return NULL;
}

/*
** GLimp_Android_ChooseConfig
*/
static void GLimp_Android_ChooseConfig( void )
{
	int colorSizes[] = { 8, 4 }, colorSize;
	int depthSizes[] = { 24, 16, 16 }, depthSize, firstDepthSize = 0;
	bool depthEncodingSupported = false;
	int depthEncodings[] = { EGL_DONT_CARE, EGL_DEPTH_ENCODING_NONLINEAR_NV, EGL_DONT_CARE }, depthEncoding;
	int maxStencilSize = ( ( r_stencilbits->integer >= 8 ) ? 8 : 0 ), stencilSize;
	int minSwapIntervals[] = {
#ifndef PUBLIC_BUILD // Vsync cannot be normally turned off on Android, so the setting must not be available to the user.
		0,
#endif
		EGL_DONT_CARE
	};
	int minSwapInterval;
	const char *extensions = qglGetGLWExtensionsString();
	int i, j, k;

	if( !( ri.Cvar_Get( "gl_ext_depth24", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO )->integer ) )
		firstDepthSize = 1;

	if( extensions && strstr( extensions, "EGL_NV_depth_nonlinear" ) &&
		ri.Cvar_Get( "gl_ext_depth_nonlinear", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO )->integer )
	{
		depthEncodingSupported = true;
	}

	for( i = 0; i < sizeof( colorSizes ) / sizeof( colorSizes[0] ); i++ )
	{
		colorSize = colorSizes[i];

		for( j = firstDepthSize; j < sizeof( depthSizes ) / sizeof( depthSizes[0] ); j++ )
		{
			depthEncoding = depthEncodings[j];
			if( ( depthEncoding != EGL_DONT_CARE ) && !depthEncodingSupported )
				continue;

			depthSize = depthSizes[j];

			for( stencilSize = maxStencilSize; stencilSize >= 0; stencilSize -= 8 )
			{
				for( k = 0; k < sizeof( minSwapIntervals ) / sizeof( minSwapIntervals[0] ); k++ )
				{
					EGLConfig config = GLimp_Android_ChooseVisual( colorSize, depthSize, depthEncoding, stencilSize, minSwapIntervals[k] );

					if( config )
					{
						glw_state.config = config;

#ifdef PUBLIC_BUILD
						minSwapInterval = 1;
#else
						qeglGetConfigAttrib( glw_state.display, glw_state.config, EGL_MIN_SWAP_INTERVAL, &minSwapInterval );
#endif
						ri.Com_Printf( "Got colorbits %i, depthbits %i%s, stencilbits %i"
#ifndef PUBLIC_BUILD
							", min swap interval %i"
#endif
							"\n"
							, colorSize * 4, depthSize
							, ( depthEncoding == EGL_DEPTH_ENCODING_NONLINEAR_NV ) ? " (non-linear)" : ""
							, stencilSize
#ifndef PUBLIC_BUILD
							, minSwapInterval
#endif
						);
						glConfig.stencilBits = stencilSize;
						ri.Cvar_ForceSet( "r_swapinterval_min", ( minSwapInterval > 0 ) ? "1" : "0" );
						return;
					}
				}
			}
		}
	}
}

/**
 * Recreates and selects the surface for the newly created window, or selects a dummy buffer when there's no window.
 */
static void GLimp_Android_UpdateWindowSurface( void )
{
	ANativeWindow *window = glw_state.window;

	if( glw_state.surface != EGL_NO_SURFACE )
	{
		qeglDestroySurface( glw_state.display, glw_state.surface );
		glw_state.surface = EGL_NO_SURFACE;
	}

	if( !window )
	{
		qeglMakeCurrent( glw_state.display, glw_state.pbufferSurface, glw_state.pbufferSurface, glw_state.context );
		return;
	}

	ANativeWindow_setBuffersGeometry( window, glConfig.width, glConfig.height, glw_state.format );

	if( glConfig.stereoEnabled )
	{
		const char *extensions = qglGetGLWExtensionsString();
		if( extensions && strstr( extensions, "EGL_EXT_multiview_window" ) )
		{
			int attribs[] = { EGL_MULTIVIEW_VIEW_COUNT_EXT, 2, EGL_NONE };
			glw_state.surface = qeglCreateWindowSurface( glw_state.display, glw_state.config, glw_state.window, attribs );
		}
	}

	if( glw_state.surface == EGL_NO_SURFACE ) // Try to create a non-stereo surface.
	{
		glConfig.stereoEnabled = false;
		glw_state.surface = qeglCreateWindowSurface( glw_state.display, glw_state.config, glw_state.window, NULL );
	}

	if( glw_state.surface == EGL_NO_SURFACE )
	{
		ri.Com_Printf( "GLimp_Android_UpdateWindowSurface() - GLimp_Android_CreateWindowSurface failed\n" );
		return;
	}

	if( !qeglMakeCurrent( glw_state.display, glw_state.surface, glw_state.surface, glw_state.context ) )
	{
		ri.Com_Printf( "GLimp_Android_UpdateWindowSurface() - eglMakeCurrent failed\n" );
		qeglDestroySurface( glw_state.display, glw_state.surface );
		glw_state.surface = EGL_NO_SURFACE;
		return;
	}

	qeglSwapInterval( glw_state.display, glw_state.swapInterval );
}

/*
** GLimp_InitGL
*/
static bool GLimp_InitGL( void )
{
	int format;
	EGLConfig config;
	const int pbufferAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	const int contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLSurface surface;

	glw_state.display = qeglGetDisplay( EGL_DEFAULT_DISPLAY );
	if( glw_state.display == EGL_NO_DISPLAY )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglGetDisplay failed\n" );
		return false;
	}
	if( !qeglInitialize( glw_state.display, NULL, NULL ) )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglInitialize failed\n" );
		return false;
	}

	GLimp_Android_ChooseConfig();
	if( !glw_state.config )
	{
		ri.Com_Printf( "GLimp_InitGL() - GLimp_Android_ChooseConfig failed\n" );
		return false;
	}
	if ( !qeglGetConfigAttrib( glw_state.display, glw_state.config, EGL_NATIVE_VISUAL_ID, &glw_state.format ) )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglGetConfigAttrib failed\n" );
		return false;
	}

	glw_state.pbufferSurface = qeglCreatePbufferSurface( glw_state.display, glw_state.config, pbufferAttribs );
	if( glw_state.pbufferSurface == EGL_NO_SURFACE )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglCreatePbufferSurface failed\n" );
		return false;
	}

	glw_state.context = qeglCreateContext( glw_state.display, glw_state.config, EGL_NO_CONTEXT, contextAttribs );
	if( glw_state.context == EGL_NO_CONTEXT )
	{
		ri.Com_Printf( "GLimp_InitGL() - eglCreateContext failed\n" );
		return false;
	}

	glw_state.swapInterval = 1;

	GLimp_Android_UpdateWindowSurface();

	return true;
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int x, int y, int width, int height, int displayFrequency, bool fullscreen, bool stereo )
{
	if( width == glConfig.width && height == glConfig.height && glConfig.fullScreen != fullscreen )
	{
		// fullscreen does nothing on Android
		return rserr_ok;
	}

	ri.Com_Printf( "Initializing OpenGL display\n" );

	GLimp_Shutdown();

	glConfig.width = width;
	glConfig.height = height;
	glConfig.fullScreen = fullscreen;
	glConfig.stereoEnabled = stereo;

	if( !GLimp_InitGL() )
	{
		ri.Com_Printf( "GLimp_SetMode() - GLimp_InitGL failed\n" );
		GLimp_Shutdown();
		return rserr_unknown;
	}

	return rserr_ok;
}

/*
** GLimp_SetWindow
*/
rserr_t GLimp_SetWindow( void *hinstance, void *wndproc, void *parenthWnd )
{
	ANativeWindow *window = ( ANativeWindow * )parenthWnd;

	if( glw_state.window == window )
		return rserr_ok;

	glw_state.window = window;
	GLimp_Android_UpdateWindowSurface();
	return rserr_ok;
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( bool active, bool destroy )
{
}

/*
** GLimp_GetGammaRamp
*/
bool GLimp_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp )
{
	return false;
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
bool GLimp_ScreenEnabled( void )
{
	return ( glw_state.surface != EGL_NO_SURFACE ) ? true : false;
}

/*
** GLimp_SetSwapInterval
*/
void GLimp_SetSwapInterval( int swapInterval )
{
	glw_state.swapInterval = swapInterval;
	if( glw_state.surface != EGL_NO_SURFACE )
		qeglSwapInterval( glw_state.display, swapInterval );
}

/*
** GLimp_SharedContext_Create
*/
bool GLimp_SharedContext_Create( void **context, void **surface )
{
	const int pbufferAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	const int contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLSurface pbuffer;
	EGLContext ctx;

	pbuffer = qeglCreatePbufferSurface( glw_state.display, glw_state.config, pbufferAttribs );
	if( pbuffer == EGL_NO_SURFACE )
		return false;

	ctx = qeglCreateContext( glw_state.display, glw_state.config, glw_state.context, contextAttribs );
	if( !ctx )
	{
		qeglDestroySurface( glw_state.display, pbuffer );
		return false;
	}

	*context = ctx;
	*surface = pbuffer;
	return true;
}

/*
** GLimp_SharedContext_MakeCurrent
*/
bool GLimp_SharedContext_MakeCurrent( void *context, void *surface )
{
	return qeglMakeCurrent( glw_state.display, surface, surface, context ) ? true : false;
}

/*
** GLimp_SharedContext_Destroy
*/
void GLimp_SharedContext_Destroy( void *context, void *surface )
{
	qeglDestroyContext( glw_state.display, context );
	qeglDestroySurface( glw_state.display, surface );
}
