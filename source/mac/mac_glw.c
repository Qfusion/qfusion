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

#include <SDL2/SDL.h>
#include <OpenGL/OpenGL.h>

#include "../ref_gl/r_local.h"
#include "../client/client.h"
#include "mac_glw.h"

glwstate_t glw_state = { NULL, qfalse };
cvar_t *vid_fullscreen;

static int GLimp_InitGL( void );

static void VID_SetWindowSize( qboolean fullscreen )
{
	if (fullscreen)
	{
		SDL_SetWindowFullscreen(glw_state.sdl_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
	else
	{
		SDL_SetWindowSize(glw_state.sdl_window, glConfig.width, glConfig.height);
	}
}
    
static qboolean VID_CreateWindow( void )
{
	qboolean fullscreen = glConfig.fullScreen;
    
	glw_state.sdl_window = SDL_CreateWindow(glw_state.applicationName,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, SDL_WINDOW_OPENGL);
    
	if( !glw_state.sdl_window )
		Sys_Error( "Couldn't create window: \"%s\"", SDL_GetError() );
    
    VID_SetWindowSize( fullscreen );
    
	// init all the gl stuff for the window
	if( !GLimp_InitGL() )
	{
		ri.Com_Printf( "VID_CreateWindow() - GLimp_InitGL failed\n" );
		return qfalse;
	}
    
	return qtrue;
}

/**
 * Set video mode.
 * @param mode number of the mode to set
 * @param fullscreen <code>qtrue</code> for a fullscreen mode,
 *     <code>qfalse</code> otherwise
 */
rserr_t GLimp_SetMode( int x, int y, int width, int height, int displayFrequency,
	qboolean fullscreen, qboolean wideScreen )
{
    const char *win_fs[] = { "W", "FS" };
    
    ri.Com_Printf( "Initializing OpenGL display\n" );
	ri.Com_Printf( "...setting mode:" );
    ri.Com_Printf( " %d %d %s\n", width, height, win_fs[fullscreen] );
    
    // destroy the existing window
	if( glw_state.sdl_window )
	{
		GLimp_Shutdown();
	}
    
    glw_state.win_x = x;
	glw_state.win_y = y;
    
	glConfig.width = width;
	glConfig.height = height;
	glConfig.wideScreen = wideScreen;
    // TODO: SDL2
	glConfig.fullScreen = fullscreen;//VID_SetFullscreenMode( displayFrequency, fullscreen );
    
	if( !VID_CreateWindow() ) {
		return rserr_invalid_mode;
	}
    
    return ( fullscreen == glConfig.fullScreen ? rserr_ok : rserr_invalid_fullscreen );
}

/**
 * Shutdown GLimp sub system.
 */
void GLimp_Shutdown()
{
    SDL_DestroyWindow(glw_state.sdl_window);
    
    if( glConfig.fullScreen )
    {
        glConfig.fullScreen = qfalse;
    }
    
    if( glw_state.applicationName )
    {
        free( glw_state.applicationName );
        glw_state.applicationName = NULL;
    }
    
    glw_state.win_x = 0;
    glw_state.win_y = 0;
    
    glConfig.width = 0;
    glConfig.height = 0;
}

/**
 * Initialize GLimp sub system.
 * @param hinstance
 * @param wndproc
 */

int GLimp_Init( const char *applicationName, void *hinstance, void *wndproc, void *parenthWnd )
{
	glw_state.applicationName = (char*)malloc( strlen( applicationName ) + 1 );
	memcpy( glw_state.applicationName, applicationName, strlen( applicationName ) + 1 );
    
	return qtrue;
}

static int GLimp_InitGL( void )
{
    int colorBits, depthBits, stencilBits;
	
	cvar_t *stereo;
	stereo = ri.Cvar_Get( "cl_stereo", "0", 0 );
    
    glConfig.stencilBits = r_stencilbits->integer;
// TODO: SDL2
//	if( max( 0, r_stencilbits->integer ) != 0 )
//		glConfig.stencilEnabled = qtrue;
//	else
//		glConfig.stencilEnabled = qfalse;
    
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, max( 0, r_stencilbits->integer ));
    
	if( stereo->integer != 0)
	{
		ri.Com_DPrintf( "...attempting to use stereo\n" );
		SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
		glConfig.stereoEnabled = qtrue;
	}
	else
	{
		glConfig.stereoEnabled = qfalse;
	}
    
	glw_state.sdl_glcontext = SDL_GL_CreateContext(glw_state.sdl_window);
	if (glw_state.sdl_glcontext == 0) {
		ri.Com_Printf( "GLimp_Init() - SDL_GL_CreateContext failed: \"%s\"\n", SDL_GetError() );
		goto fail;
	}
    
	if (SDL_GL_MakeCurrent(glw_state.sdl_window, glw_state.sdl_glcontext)) {
		ri.Com_Printf( "GLimp_Init() - SDL_GL_MakeCurrent failed: \"%s\"\n", SDL_GetError() );
		goto fail;
	}
    
	/*
     ** print out PFD specifics
     */
	SDL_GL_GetAttribute(SDL_GL_BUFFER_SIZE, &colorBits);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthBits);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencilBits);
	
	ri.Com_Printf( "GL PFD: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", colorBits, depthBits, stencilBits);
    
	return qtrue;
    
fail:
	return qfalse;
}

/**
 * TODO documentation
 */
void GLimp_BeginFrame( void )
{
}


/**
 * Swap the buffers and possibly do other stuff that yet needs to be
 * determined.
 */
void GLimp_EndFrame( void )
{
    SDL_GL_SwapWindow(glw_state.sdl_window);
}


/**
 * TODO documentation
 */
qboolean GLimp_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp )
{
	unsigned short ramp256[3*256];
	
	if( stride < 256 )
	{
        // SDL only supports gamma ramps with 256 mappings per channel
        return qfalse;
	}
	
	if( SDL_GetWindowGammaRamp( glw_state.sdl_window, ramp256, ramp256+256, ramp256+( 256<<1 ) ) != -1 )
	{
	 	*psize = 256;
	 	memcpy( ramp,          ramp256,       256*sizeof(*ramp) );
	 	memcpy( ramp+  stride, ramp256+  256, 256*sizeof(*ramp) );
	 	memcpy( ramp+2*stride, ramp256+2*256, 256*sizeof(*ramp) );
	}
	return qfalse;
}


/**
 * TODO documentation
 */
void GLimp_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp )
{
    unsigned short ramp256[3*256];
	
	if( size != 256 )
		return;
    
    memcpy( ramp256,       ramp         , size*sizeof(*ramp));
    memcpy( ramp256+  256, ramp+  stride, size*sizeof(*ramp));
    memcpy( ramp256+2*256, ramp+2*stride, size*sizeof(*ramp));
    if( SDL_SetWindowGammaRamp( glw_state.sdl_window, ramp256, ramp256+256, ramp256+( 256<<1 ) ) == -1 )
    {
        Com_Printf( "SDL_SetWindowGammaRamp(...) failed: ", SDL_GetError() );
    }
}


/**
 * TODO documentation
 */
void GLimp_AppActivate( qboolean active, qboolean destroy)
{
}

/*
** GLimp_SetWindow
*/
qboolean GLimp_SetWindow( void *hinstance, void *wndproc, void *parenthWnd )
{
	return qfalse; // surface cannot be lost
}

/*
** GLimp_ScreenEnabled
*/
qboolean GLimp_ScreenEnabled( void )
{
	return qtrue;
}

/*
** GLimp_SharedContext_Create
*/
qboolean GLimp_SharedContext_Create( void **context, void **surface )
{
	return qfalse;
}

/*
** GLimp_SharedContext_MakeCurrent
*/
qboolean GLimp_SharedContext_MakeCurrent( void *context, void *surface )
{
	return qfalse;
}

/*
** GLimp_SharedContext_Destroy
*/
void GLimp_SharedContext_Destroy( void *context, void *surface )
{
	(void)context;
}
