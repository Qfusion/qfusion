/*
Copyright (C) 2016 SiPlus, Warsow Development Team

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
#include "egl_glw.h"

#if defined( __ANDROID__ ) && defined( PUBLIC_BUILD )
// Vsync cannot be normally turned off on Android, so the setting must not be available to the user
#define GLIMP_EGL_FORCE_VSYNC
#endif

glwstate_t glw_state;

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( void ) {
}

/*
** GLimp_EndFrame
*/
void GLimp_EndFrame( void ) {
	if( glw_state.surface != EGL_NO_SURFACE ) {
		qeglSwapBuffers( glw_state.display, glw_state.surface );
	}
}

/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.
*/
bool GLimp_Init( const char *applicationName, void *hinstance, void *wndproc, void *parenthWnd,
				 int iconResource, const int *iconXPM ) {
	glw_state.window = ( EGLNativeWindowType )parenthWnd;
	return true;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem. The state structure is also nulled out.
**
*/
void GLimp_Shutdown( void ) {
	if( glw_state.windowMutex ) {
		ri.Mutex_Destroy( &glw_state.windowMutex );
	}
	if( glw_state.context != EGL_NO_CONTEXT ) {
		qeglMakeCurrent( glw_state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
		qeglDestroyContext( glw_state.display, glw_state.context );
		glw_state.context = EGL_NO_CONTEXT;
	}
	if( glw_state.surface != EGL_NO_SURFACE ) {
		qeglDestroySurface( glw_state.display, glw_state.surface );
		glw_state.surface = EGL_NO_SURFACE;
	}
	if( glw_state.noWindowPbuffer != EGL_NO_SURFACE ) {
		qeglDestroySurface( glw_state.display, glw_state.noWindowPbuffer );
		glw_state.noWindowPbuffer = EGL_NO_SURFACE;
	}
	if( glw_state.mainThreadPbuffer != EGL_NO_SURFACE ) {
		qeglDestroySurface( glw_state.display, glw_state.mainThreadPbuffer );
		glw_state.mainThreadPbuffer = EGL_NO_SURFACE;
	}
	if( glw_state.display != EGL_NO_DISPLAY ) {
		qeglTerminate( glw_state.display );
		glw_state.display = EGL_NO_DISPLAY;
	}
}

/*
** GLimp_EGL_ChooseVisual
*/
static EGLConfig GLimp_EGL_ChooseVisual( int colorSize, int depthSize, int depthEncoding, int stencilSize, int minSwapInterval ) {
	int attribs[] =
	{
		/*  0 */ EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		/*  2 */ EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
		/*  4 */ EGL_RED_SIZE, colorSize,
		/*  6 */ EGL_GREEN_SIZE, colorSize,
		/*  8 */ EGL_BLUE_SIZE, colorSize,
		/* 10 */ EGL_ALPHA_SIZE, colorSize,
		/* 12 */ EGL_DEPTH_SIZE, depthSize,
		/* 14 */ EGL_STENCIL_SIZE, stencilSize,
		/* 16 */ EGL_SAMPLES, 0,
		/* 18 */ EGL_SAMPLE_BUFFERS, 0,
		/* 20 */ EGL_NONE, EGL_DONT_CARE,
		EGL_NONE, EGL_DONT_CARE,
		EGL_NONE
	};
	int addAttrib = 20;
	int numConfigs = 0;
	EGLConfig config = NULL;

	if( minSwapInterval != EGL_DONT_CARE ) {
		attribs[addAttrib++] = EGL_MIN_SWAP_INTERVAL;
		attribs[addAttrib++] = minSwapInterval;
	}

	if( depthEncoding != EGL_DONT_CARE ) {
		attribs[addAttrib++] = EGL_DEPTH_ENCODING_NV;
		attribs[addAttrib++] = depthEncoding;
	}

	if( qeglChooseConfig( glw_state.display, attribs, &config, 1, &numConfigs ) ) {
		return numConfigs ? config : 0;
	}

	return NULL;
}

/*
** GLimp_EGL_ChooseConfig
*/
static void GLimp_EGL_ChooseConfig( void ) {
	int colorSizes[] = { 8, 4 }, colorSize;
	int depthSizes[] = { 24, 16, 16 }, depthSize, firstDepthSize = 0;
	bool depthEncodingSupported = false;
	int depthEncodings[] = { EGL_DONT_CARE, EGL_DEPTH_ENCODING_NONLINEAR_NV, EGL_DONT_CARE }, depthEncoding;
	int maxStencilSize = ( ( r_stencilbits->integer >= 8 ) ? 8 : 0 ), stencilSize;
	int minSwapIntervals[] = {
#ifndef GLIMP_EGL_FORCE_VSYNC
		0,
#endif
		EGL_DONT_CARE
	};
	int minSwapInterval;
	const char *extensions = qglGetGLWExtensionsString();
	int i, j, k;

	if( !( ri.Cvar_Get( "gl_ext_depth24", "1", CVAR_ARCHIVE | CVAR_LATCH_VIDEO )->integer ) ) {
		firstDepthSize = 1;
	}

	if( extensions && strstr( extensions, "EGL_NV_depth_nonlinear" ) &&
		ri.Cvar_Get( "gl_ext_depth_nonlinear", "1", CVAR_ARCHIVE | CVAR_LATCH_VIDEO )->integer ) {
		depthEncodingSupported = true;
	}

	for( i = 0; i < sizeof( colorSizes ) / sizeof( colorSizes[0] ); i++ ) {
		colorSize = colorSizes[i];

		for( j = firstDepthSize; j < sizeof( depthSizes ) / sizeof( depthSizes[0] ); j++ ) {
			depthEncoding = depthEncodings[j];
			if( ( depthEncoding != EGL_DONT_CARE ) && !depthEncodingSupported ) {
				continue;
			}

			depthSize = depthSizes[j];

			for( stencilSize = maxStencilSize; stencilSize >= 0; stencilSize -= 8 ) {
				for( k = 0; k < sizeof( minSwapIntervals ) / sizeof( minSwapIntervals[0] ); k++ ) {
					EGLConfig config = GLimp_EGL_ChooseVisual( colorSize, depthSize, depthEncoding, stencilSize, minSwapIntervals[k] );

					if( config ) {
						glw_state.config = config;

#ifdef GLIMP_EGL_FORCE_VSYNC
						minSwapInterval = 1;
#else
						qeglGetConfigAttrib( glw_state.display, glw_state.config, EGL_MIN_SWAP_INTERVAL, &minSwapInterval );
#endif
						ri.Com_Printf( "Got colorbits %i, depthbits %i%s, stencilbits %i"
#ifndef GLIMP_EGL_FORCE_VSYNC
									   ", min swap interval %i"
#endif
									   "\n"
									   , colorSize * 4, depthSize
									   , ( depthEncoding == EGL_DEPTH_ENCODING_NONLINEAR_NV ) ? " (non-linear)" : ""
									   , stencilSize
#ifndef GLIMP_EGL_FORCE_VSYNC
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
 * Creates a 1x1 pbuffer surface for contexts that don't use the main surface.
 *
 * @return 1x1 pbuffer surface
 */
static EGLSurface GLimp_EGL_CreatePbufferSurface( void ) {
	const int pbufferAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	return qeglCreatePbufferSurface( glw_state.display, glw_state.config, pbufferAttribs );
}

/**
 * Recreates and selects the surface for the newly created window, or selects a dummy buffer when there's no window.
 */
static void GLimp_EGL_UpdateWindowSurface( void ) {
	EGLNativeWindowType window = glw_state.window;
	EGLContext context = qeglGetCurrentContext();

	if( context == EGL_NO_CONTEXT ) {
		return;
	}

	if( glw_state.surface != EGL_NO_SURFACE ) {
		qeglMakeCurrent( glw_state.display, glw_state.noWindowPbuffer, glw_state.noWindowPbuffer, context );
		qeglDestroySurface( glw_state.display, glw_state.surface );
		glw_state.surface = EGL_NO_SURFACE;
	}

	if( !window ) {
		return;
	}

#ifdef __ANDROID__
	ANativeWindow_setBuffersGeometry( window, glConfig.width, glConfig.height, glw_state.format );
#endif

	glw_state.surface = qeglCreateWindowSurface( glw_state.display, glw_state.config, window, NULL );
	if( glw_state.surface == EGL_NO_SURFACE ) {
		ri.Com_Printf( "GLimp_EGL_UpdateWindowSurface() - GLimp_EGL_CreateWindowSurface failed\n" );
		return;
	}

	qeglMakeCurrent( glw_state.display, glw_state.surface, glw_state.surface, context );
	qeglSwapInterval( glw_state.display, glw_state.swapInterval );
}

/*
** GLimp_InitGL
*/
static bool GLimp_InitGL( void ) {
	int format;
	EGLConfig config;
	const int contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLSurface surface;

	glw_state.display = qeglGetDisplay( EGL_DEFAULT_DISPLAY );
	if( glw_state.display == EGL_NO_DISPLAY ) {
		ri.Com_Printf( "GLimp_InitGL() - eglGetDisplay failed\n" );
		return false;
	}
	if( !qeglInitialize( glw_state.display, NULL, NULL ) ) {
		ri.Com_Printf( "GLimp_InitGL() - eglInitialize failed\n" );
		return false;
	}

	GLimp_EGL_ChooseConfig();
	if( !glw_state.config ) {
		ri.Com_Printf( "GLimp_InitGL() - GLimp_EGL_ChooseConfig failed\n" );
		return false;
	}
	if( !qeglGetConfigAttrib( glw_state.display, glw_state.config, EGL_NATIVE_VISUAL_ID, &glw_state.format ) ) {
		ri.Com_Printf( "GLimp_InitGL() - eglGetConfigAttrib failed\n" );
		return false;
	}

	glw_state.mainThreadPbuffer = GLimp_EGL_CreatePbufferSurface();
	if( glw_state.mainThreadPbuffer == EGL_NO_SURFACE ) {
		ri.Com_Printf( "GLimp_InitGL() - GLimp_EGL_CreatePbufferSurface for mainThreadPbuffer failed\n" );
		return false;
	}

	glw_state.noWindowPbuffer = GLimp_EGL_CreatePbufferSurface();
	if( glw_state.noWindowPbuffer == EGL_NO_SURFACE ) {
		ri.Com_Printf( "GLimp_InitGL() - GLimp_EGL_CreatePbufferSurface for noWindowPbuffer failed\n" );
		return false;
	}

	glw_state.context = qeglCreateContext( glw_state.display, glw_state.config, EGL_NO_CONTEXT, contextAttribs );
	if( glw_state.context == EGL_NO_CONTEXT ) {
		ri.Com_Printf( "GLimp_InitGL() - eglCreateContext failed\n" );
		return false;
	}

	glw_state.windowMutex = ri.Mutex_Create();

	glw_state.swapInterval = 1; // Default swap interval for new surfaces

	// GLimp_EGL_UpdateWindowSurface attaches the surface to the current context, so make one current to initialize
	qeglMakeCurrent( glw_state.display, glw_state.noWindowPbuffer, glw_state.noWindowPbuffer, glw_state.context );
	GLimp_EGL_UpdateWindowSurface();

	return true;
}

/*
** GLimp_SetFullscreen
*/
rserr_t GLimp_SetFullscreen( bool fullscreen, int xpos, int ypos ) {
	glConfig.fullScreen = fullscreen;
	return rserr_ok;
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int x, int y, int width, int height, bool fullscreen, bool stereo, bool borderless ) {
	if( width == glConfig.width && height == glConfig.height && glConfig.fullScreen != fullscreen ) {
#ifdef __ANDROID__
		return rserr_ok; // The window is always fullscreen on Android
#endif
	}

	ri.Com_Printf( "Initializing OpenGL display\n" );

	GLimp_Shutdown();

	glConfig.width = width;
	glConfig.height = height;
	glConfig.fullScreen = fullscreen;
	glConfig.borderless = borderless;

	if( !GLimp_InitGL() ) {
		ri.Com_Printf( "GLimp_SetMode() - GLimp_InitGL failed\n" );
		GLimp_Shutdown();
		return rserr_unknown;
	}

	return rserr_ok;
}

/*
** GLimp_SetWindow
*/
rserr_t GLimp_SetWindow( void *hinstance, void *wndproc, void *parenthWnd, bool *surfaceChangePending ) {
	EGLNativeWindowType window = ( EGLNativeWindowType )parenthWnd;

	if( surfaceChangePending ) {
		*surfaceChangePending = false;
	}

	if( glw_state.context == EGL_NO_CONTEXT ) { // Not initialized yet
		glw_state.window = window;
		return rserr_ok;
	}

	if( glw_state.multithreadedRendering ) {
		ri.Mutex_Lock( glw_state.windowMutex );
	}

	if( glw_state.window != window ) {
		glw_state.window = window;
		if( glw_state.multithreadedRendering ) {
			glw_state.windowChanged = true;
			if( surfaceChangePending ) {
				*surfaceChangePending = true;
			}
		} else {
			GLimp_EGL_UpdateWindowSurface();
		}
	}

	if( glw_state.multithreadedRendering ) {
		ri.Mutex_Unlock( glw_state.windowMutex );
	}

	return rserr_ok;
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( bool active, bool minimize, bool destroy ) {
}

/*
** GLimp_GetGammaRamp
*/
bool GLimp_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp ) {
	return false;
}

/*
** GLimp_SetGammaRamp
*/
void GLimp_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp ) {
}

/*
** GLimp_RenderingEnabled
*/
bool GLimp_RenderingEnabled( void ) {
	return glw_state.window != NULL;
}

/*
** GLimp_SetSwapInterval
*/
void GLimp_SetSwapInterval( int swapInterval ) {
	if( glw_state.swapInterval != swapInterval ) {
		glw_state.swapInterval = swapInterval;
		qeglSwapInterval( glw_state.display, swapInterval );
	}
}

/*
** GLimp_EnableMultithreadedRendering
*/
void GLimp_EnableMultithreadedRendering( bool enable ) {
	EGLSurface surface;

	if( glw_state.multithreadedRendering == enable ) {
		return;
	}

	glw_state.multithreadedRendering = enable;

	if( enable ) {
		surface = glw_state.mainThreadPbuffer;
	} else {
		if( glw_state.windowChanged ) {
			glw_state.windowChanged = false;
			GLimp_EGL_UpdateWindowSurface();
		}
		surface = ( glw_state.surface != EGL_NO_SURFACE ? glw_state.surface : glw_state.noWindowPbuffer );
	}

	qeglMakeCurrent( glw_state.display, surface, surface, glw_state.context );
}

/*
** GLimp_GetWindowSurface
*/
void *GLimp_GetWindowSurface( bool *renderable ) {
	if( renderable ) {
		*renderable = ( glw_state.surface != EGL_NO_SURFACE );
	}
	return ( glw_state.surface != EGL_NO_SURFACE ? glw_state.surface : glw_state.noWindowPbuffer );
}

/*
** GLimp_UpdatePendingWindowSurface
*/
void GLimp_UpdatePendingWindowSurface( void ) {
	ri.Mutex_Lock( glw_state.windowMutex );

	if( glw_state.windowChanged ) {
		glw_state.windowChanged = false;
		GLimp_EGL_UpdateWindowSurface();
	}

	ri.Mutex_Unlock( glw_state.windowMutex );
}

/*
** GLimp_MakeCurrent
*/
bool GLimp_MakeCurrent( void *context, void *surface ) {
	return qeglMakeCurrent( glw_state.display, surface, surface, context ) ? true : false;
}

/*
** GLimp_SharedContext_Create
*/
bool GLimp_SharedContext_Create( void **context, void **surface ) {
	const int contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLSurface pbuffer = EGL_NO_SURFACE;
	EGLContext ctx;

	if( surface ) {
		pbuffer = GLimp_EGL_CreatePbufferSurface();
		if( pbuffer == EGL_NO_SURFACE ) {
			return false;
		}
	}

	ctx = qeglCreateContext( glw_state.display, glw_state.config, glw_state.context, contextAttribs );
	if( !ctx ) {
		if( pbuffer != EGL_NO_SURFACE ) {
			qeglDestroySurface( glw_state.display, pbuffer );
		}
		return false;
	}

	*context = ctx;
	if( surface ) {
		*surface = pbuffer;
	}
	return true;
}

/*
** GLimp_SharedContext_Destroy
*/
void GLimp_SharedContext_Destroy( void *context, void *surface ) {
	qeglDestroyContext( glw_state.display, context );
	if( surface ) {
		qeglDestroySurface( glw_state.display, surface );
	}
}
