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

#include "sdl/SDL.h"

#include "../qcommon/qcommon.h"
#include "../client/renderer/glad.h"
// #include "../client/renderer/r_local.h"
#include "../client/xpm.h"
const
#include "../../icons/forksow.xpm"

#include "sdl_window.h"

SDL_Window * sdl_window = NULL;

#define RESET "\x1b[0m"
#define RED "\x1b[1;31m"
#define YELLOW "\x1b[1;32m"
#define GREEN "\x1b[1;33m"

static const char * type_string( GLenum type ) {
        switch( type ) {
                case GL_DEBUG_TYPE_ERROR:
                case GL_DEBUG_CATEGORY_API_ERROR_AMD:
                        return "error";
                case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                case GL_DEBUG_CATEGORY_DEPRECATION_AMD:
                        return "deprecated";
                case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD:
                        return "undefined";
                case GL_DEBUG_TYPE_PORTABILITY:
                        return "nonportable";
                case GL_DEBUG_TYPE_PERFORMANCE:
                case GL_DEBUG_CATEGORY_PERFORMANCE_AMD:
                        return "performance";
                case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD:
                        return "window system";
                case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD:
                        return "shader compiler";
                case GL_DEBUG_CATEGORY_APPLICATION_AMD:
                        return "application";
                case GL_DEBUG_TYPE_OTHER:
                case GL_DEBUG_CATEGORY_OTHER_AMD:
                        return "other";
                default:
                        return "idk";
        }
}

static const char * severity_string( GLenum severity ) {
        switch( severity ) {
                case GL_DEBUG_SEVERITY_LOW:
                // case GL_DEBUG_SEVERITY_LOW_AMD:
                        return GREEN "low" RESET;
                case GL_DEBUG_SEVERITY_MEDIUM:
                // case GL_DEBUG_SEVERITY_MEDIUM_AMD:
                        return YELLOW "medium" RESET;
                case GL_DEBUG_SEVERITY_HIGH:
                // case GL_DEBUG_SEVERITY_HIGH_AMD:
                        return RED "high" RESET;
                case GL_DEBUG_SEVERITY_NOTIFICATION:
                        return "notice";
                default:
                        return "idk";
        }
}

static void gl_debug_output_callback(
        GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
        const GLchar * message, const void * _
) {
        if(
            source == 33352 || // shader compliation errors
            source == 131169 ||
            source == 131185 ||
            source == 131218 ||
            source == 131204
        ) {
                return;
        }

        if( severity == GL_DEBUG_SEVERITY_NOTIFICATION || severity == GL_DEBUG_SEVERITY_NOTIFICATION_KHR ) {
                return;
        }

	Com_Printf( "GL [%s - %s]: %s", type_string( type ), severity_string( severity ), message );
	size_t len = strlen( message );
	if( len == 0 || message[ len - 1 ] != '\n' )
		Com_Printf( "\n" );

        if( severity == GL_DEBUG_SEVERITY_HIGH ) {
		abort();
        }
}

static void gl_debug_output_callback_amd(
        GLuint id, GLenum type, GLenum severity, GLsizei length,
        const GLchar * message, const void * _
) {
        gl_debug_output_callback( GL_DONT_CARE, type, id, severity, length, message, _ );
}

static bool InitGL( int stencilbits ) {
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, max( 0, stencilbits ) );

	/* SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE ); */
	/* SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG ); */
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );
#if !PUBLIC_BUILD && 0
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );
#endif

	SDL_GLContext context = SDL_GL_CreateContext( sdl_window );
	if( context == NULL ) {
		Com_Printf( "SDL_GL_CreateContext failed: \"%s\"\n", SDL_GetError() );
		return false;
	}

	if( SDL_GL_MakeCurrent( sdl_window, context ) != 0 ) {
		Com_Printf( "SDL_GL_MakeCurrent failed: \"%s\"\n", SDL_GetError() );
		return false;
	}

	if( gladLoadGLLoader( SDL_GL_GetProcAddress ) != 1 ) {
		Com_Printf( "Error loading OpenGL\n" );
		return false;
	}

#if !PUBLIC_BUILD && 0
	if( GLAD_GL_KHR_debug != 0 ) {
		GLint context_flags;
		glGetIntegerv( GL_CONTEXT_FLAGS, &context_flags );
		if( context_flags & GL_CONTEXT_FLAG_DEBUG_BIT ) {
			Com_Printf( "Initialising debug output\n" );

			glEnable( GL_DEBUG_OUTPUT );
			glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
			glDebugMessageCallback( ( GLDEBUGPROC ) gl_debug_output_callback, NULL );
			glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE );
		}
	}
	else if( GLAD_GL_AMD_debug_output != 0 ) {
		Com_Printf( "Initialising AMD debug output\n" );

		glDebugMessageCallbackAMD( ( GLDEBUGPROCAMD ) gl_debug_output_callback_amd, NULL );
		glDebugMessageEnableAMD( 0, 0, 0, NULL, GL_TRUE );
	}
#endif

	return true;
}

void VID_WindowInit( VideoMode mode, int stencilbits ) {
	uint32_t flags = SDL_WINDOW_OPENGL;
	if( mode.fullscreen == FullScreenMode_Fullscreen )
		flags |= SDL_WINDOW_FULLSCREEN;
	if( mode.fullscreen == FullScreenMode_FullscreenBorderless )
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	sdl_window = SDL_CreateWindow( "Cocaine Diesel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, mode.width, mode.height, flags );

	if( sdl_window == NULL ) {
		Sys_Error( "Couldn't create window: \"%s\"", SDL_GetError() );
	}

#ifndef __APPLE__
	uint32_t r = 0x00ff0000;
	uint32_t g = 0x0000ff00;
	uint32_t b = 0x000000ff;
	uint32_t a = 0xff000000;

#ifndef ENDIAN_LITTLE
	r = 0x000000ff;
	g = 0x0000ff00;
	b = 0x00ff0000;
	a = 0xff000000;
#endif

	int * xpm = XPM_ParseIcon( sizeof( ahacheers_xpm ) / sizeof( ahacheers_xpm[ 0 ] ), ahacheers_xpm );
	SDL_Surface * surface = SDL_CreateRGBSurfaceFrom( ( xpm + 2 ), xpm[ 0 ], xpm[ 1 ], 32, xpm[ 0 ] * 4, r, g, b, a );
	free( xpm );

	SDL_SetWindowIcon( sdl_window, surface );
	SDL_FreeSurface( surface );
#endif

	InitGL( stencilbits );
}

void VID_WindowShutdown() {
	SDL_DestroyWindow( sdl_window );
}

void VID_EnableVsync( VsyncEnabled enabled ) {
	SDL_GL_SetSwapInterval( enabled == VsyncEnabled_Enabled ? 1 : 0 );
}

int VID_GetNumVideoModes() {
	return SDL_GetNumDisplayModes( 0 );
}

VideoMode VID_GetVideoMode( int i ) {
	VideoMode mode;
	memset( &mode, 0, sizeof( mode ) );
	SDL_DisplayMode sdl_mode;

	int ok = SDL_GetDisplayMode( 0, i, &sdl_mode );
	if( ok != 0 )
		return mode;

	mode.width = sdl_mode.w;
	mode.height = sdl_mode.h;
	mode.frequency = sdl_mode.refresh_rate;

	return mode;
}

static VideoMode sdl_to_videomode( SDL_DisplayMode sdl_mode ) {
	VideoMode mode;
	memset( &mode, 0, sizeof( mode ) );
	mode.width = sdl_mode.w;
	mode.height = sdl_mode.h;
	mode.frequency = sdl_mode.refresh_rate;

	return mode;
}

VideoMode VID_GetDefaultVideoMode() {
	SDL_DisplayMode sdl_mode;
	SDL_GetDesktopDisplayMode( 0, &sdl_mode );

	VideoMode mode = sdl_to_videomode( sdl_mode );

	uint32_t flags = SDL_GetWindowFlags( sdl_window );
	mode.fullscreen = FullScreenMode_Windowed;
	if( flags & SDL_WINDOW_FULLSCREEN )
		mode.fullscreen = FullScreenMode_Fullscreen;
	if( flags & SDL_WINDOW_FULLSCREEN_DESKTOP )
		mode.fullscreen = FullScreenMode_FullscreenBorderless;

	return mode;
}

bool VID_SetVideoMode( VideoMode mode ) {
	if( SDL_SetWindowFullscreen( sdl_window, 0 ) != 0 )
		return false;
	SDL_SetWindowSize( sdl_window, mode.width, mode.height );

	if( mode.fullscreen != FullScreenMode_Windowed ) {
		SDL_DisplayMode default_sdl_mode;
		if( SDL_GetDesktopDisplayMode( 0, &default_sdl_mode ) != 0 )
			return false;

		SDL_DisplayMode sdl_mode;
		memset( &sdl_mode, 0, sizeof( sdl_mode ) );
		sdl_mode.w = mode.width;
		sdl_mode.h = mode.height;
		sdl_mode.refresh_rate = mode.frequency;
		sdl_mode.format = default_sdl_mode.format;

		if( SDL_SetWindowDisplayMode( sdl_window, &sdl_mode ) != 0 )
			return false;

		uint32_t flags = SDL_WINDOW_FULLSCREEN;
		if( mode.fullscreen == FullScreenMode_FullscreenBorderless )
			flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
		if( SDL_SetWindowFullscreen( sdl_window, flags ) != 0 )
			return false;
	}

	return true;
}

VideoMode VID_GetCurrentVideoMode() {
	SDL_DisplayMode sdl_mode;
	SDL_GetCurrentDisplayMode( 0, &sdl_mode );
	return sdl_to_videomode( sdl_mode );
}

bool VID_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp ) {
	return false;
	/* unsigned short ramp256[3 * 256]; */
	/* assert( stride < 256 ); */
        /*  */
	/* if( SDL_GetWindowGammaRamp( sdl_window, ramp256, ramp256 + 256, ramp256 + ( 256 << 1 ) ) != -1 ) { */
	/* 	*psize = 256; */
	/* 	memcpy( ramp, ramp256, 256 * sizeof( *ramp ) ); */
	/* 	memcpy( ramp + stride, ramp256 + 256, 256 * sizeof( *ramp ) ); */
	/* 	memcpy( ramp + 2 * stride, ramp256 + 2 * 256, 256 * sizeof( *ramp ) ); */
	/* 	return true; */
	/* } */
	/* return false; */
}

void VID_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp ) {
	// TODO: add a gamma ramp struct
	unsigned short ramp256[3 * 256];
	assert( size == 256 );

	memcpy( ramp256, ramp, size * sizeof( *ramp ) );
	memcpy( ramp256 + 256, ramp + stride, size * sizeof( *ramp ) );
	memcpy( ramp256 + 2 * 256, ramp + 2 * stride, size * sizeof( *ramp ) );
	if( SDL_SetWindowGammaRamp( sdl_window, ramp256, ramp256 + 256, ramp256 + ( 256 << 1 ) ) == -1 ) {
		Com_Printf( "SDL_SetWindowGammaRamp() failed: \"%s\"\n", SDL_GetError() );
	}
}

void VID_FlashWindow() {
#if _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION( &info.version );
	SDL_GetWindowWMInfo( window, &info );
	FlashWindow( info.info.win.window, TRUE );
#endif
}

void VID_Swap() {
	SDL_GL_SwapWindow( sdl_window );
}
