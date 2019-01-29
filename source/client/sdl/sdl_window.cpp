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

#include "qcommon/qcommon.h"
#include "glad/glad.h"

#include "sdl_window.h"
#include "icon.h"

#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

#include "stb/stb_image.h"

SDL_Window * sdl_window = NULL;

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
                        return S_COLOR_GREEN "low" S_COLOR_WHITE;
                case GL_DEBUG_SEVERITY_MEDIUM:
                // case GL_DEBUG_SEVERITY_MEDIUM_AMD:
                        return S_COLOR_YELLOW "medium" S_COLOR_WHITE;
                case GL_DEBUG_SEVERITY_HIGH:
                // case GL_DEBUG_SEVERITY_HIGH_AMD:
                        return S_COLOR_RED "high" S_COLOR_WHITE;
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
            id == 131169 ||
            id == 131185 ||
            id == 131218 ||
            id == 131204
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

static bool InitGL() {
	int flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
#if PUBLIC_BUILD
	flags |= SDL_GL_CONTEXT_NO_ERROR;
#else
	flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
#endif

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, flags );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );

	SDL_GLContext context = SDL_GL_CreateContext( sdl_window );

	if( context == NULL ) {
		Com_Printf( "Couldn't create GL 3.3 context (%s), trying GL 2.1\n", SDL_GetError() );

		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );

		context = SDL_GL_CreateContext( sdl_window );

		if( context == NULL ) {
			Com_Printf( "SDL_GL_CreateContext failed: \"%s\"\n", SDL_GetError() );
			return false;
		}
	}

	if( SDL_GL_MakeCurrent( sdl_window, context ) != 0 ) {
		Com_Printf( "SDL_GL_MakeCurrent failed: \"%s\"\n", SDL_GetError() );
		return false;
	}

	if( gladLoadGLLoader( SDL_GL_GetProcAddress ) != 1 ) {
		Com_Printf( "Error loading OpenGL\n" );
		return false;
	}

#if !PUBLIC_BUILD
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

void VID_WindowInit( WindowMode mode ) {
	uint32_t flags = SDL_WINDOW_OPENGL;
	if( mode.fullscreen == FullScreenMode_Fullscreen )
		flags |= SDL_WINDOW_FULLSCREEN;
	if( mode.fullscreen == FullScreenMode_FullscreenBorderless )
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	if( mode.x == -1 && mode.y == -1 ) {
		mode.x = SDL_WINDOWPOS_CENTERED;
		mode.y = SDL_WINDOWPOS_CENTERED;
	}

	sdl_window = SDL_CreateWindow( "Cocaine Diesel", mode.x, mode.y, mode.video_mode.width, mode.video_mode.height, flags );
	if( sdl_window == NULL ) {
		Sys_Error( "Couldn't create window: \"%s\"", SDL_GetError() );
	}

	if( mode.fullscreen != FullScreenMode_Windowed ) {
		VID_SetVideoMode( mode.video_mode ); // also set frequency
	}

	int w, h;
	uint8_t * icon = stbi_load_from_memory( icon_png, icon_png_len, &w, &h, NULL, 4 );
	SDL_Surface * surface = SDL_CreateRGBSurfaceFrom( icon, w, h, 32, w * 4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 );
	SDL_SetWindowIcon( sdl_window, surface );
	SDL_FreeSurface( surface );
	free( icon );

	InitGL();
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

VideoMode VID_GetCurrentVideoMode() {
	SDL_DisplayMode sdl_mode;
	SDL_GetCurrentDisplayMode( 0, &sdl_mode );

	VideoMode mode;
	mode.width = sdl_mode.w;
	mode.height = sdl_mode.h;
	mode.frequency = sdl_mode.refresh_rate;

	return mode;
}

void VID_SetVideoMode( VideoMode mode ) {
	SDL_DisplayMode default_sdl_mode;
	SDL_GetDesktopDisplayMode( 0, &default_sdl_mode );

	SDL_DisplayMode sdl_mode;
	sdl_mode.w = mode.width;
	sdl_mode.h = mode.height;
	sdl_mode.refresh_rate = mode.frequency;
	sdl_mode.format = default_sdl_mode.format;
	sdl_mode.driverdata = NULL;

	SDL_SetWindowDisplayMode( sdl_window, &sdl_mode );
}

WindowMode VID_GetWindowMode() {
	WindowMode mode = { };

	uint32_t flags = SDL_GetWindowFlags( sdl_window );
	mode.fullscreen = FullScreenMode_Windowed;
	if( flags & SDL_WINDOW_FULLSCREEN )
		mode.fullscreen = FullScreenMode_Fullscreen;
	if( ( flags & SDL_WINDOW_FULLSCREEN_DESKTOP ) == SDL_WINDOW_FULLSCREEN_DESKTOP )
		mode.fullscreen = FullScreenMode_FullscreenBorderless;

	if( mode.fullscreen == FullScreenMode_Windowed ) {
		SDL_GetWindowSize( sdl_window, &mode.video_mode.width, &mode.video_mode.height );
		SDL_GetWindowPosition( sdl_window, &mode.x, &mode.y );
	}
	else {
		mode.video_mode = VID_GetCurrentVideoMode();
	}

	return mode;
}

void VID_SetWindowMode( WindowMode mode ) {
	uint32_t flags = 0;
	if( mode.fullscreen == FullScreenMode_Fullscreen )
		flags = SDL_WINDOW_FULLSCREEN;
	if( mode.fullscreen == FullScreenMode_FullscreenBorderless )
		flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
	SDL_SetWindowFullscreen( sdl_window, flags );

	if( mode.fullscreen == FullScreenMode_Windowed ) {
		SDL_SetWindowSize( sdl_window, mode.video_mode.width, mode.video_mode.height );
		if( mode.x == -1 && mode.y == -1 ) {
			mode.x = SDL_WINDOWPOS_CENTERED;
			mode.y = SDL_WINDOWPOS_CENTERED;
		}
		SDL_SetWindowPosition( sdl_window, mode.x, mode.y );
	}
	else {
		VID_SetVideoMode( mode.video_mode );
	}
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
	SDL_GetWindowWMInfo( sdl_window, &info );
	FlashWindow( info.info.win.window, TRUE );
#endif
}

void VID_Swap() {
	SDL_GL_SwapWindow( sdl_window );
}

void format( FormatBuffer * fb, VideoMode mode, const FormatOpts & opts ) {
	ggformat_impl( fb, "{}x{} {}Hz", mode.width, mode.height, mode.frequency );
}

void format( FormatBuffer * fb, WindowMode mode, const FormatOpts & opts ) {
	if( mode.fullscreen == FullScreenMode_Windowed ) {
		ggformat_impl( fb, "{}x{} {}x{}",
			mode.video_mode.width, mode.video_mode.height,
			mode.x, mode.y );
	}
	else {
		ggformat_impl( fb, "{}x{} {} {} {}Hz",
			mode.video_mode.width, mode.video_mode.height,
			mode.fullscreen == FullScreenMode_Fullscreen ? 'F' : 'B',
			mode.monitor, mode.video_mode.frequency );
	}
}
