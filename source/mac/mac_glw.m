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

#import <Cocoa/Cocoa.h>

#include <SDL/SDL.h>
#include <OpenGL/OpenGL.h>

#include "../ref_gl/r_local.h"
#include "../client/client.h"
#include "mac_glw.h"

glwstate_t glw_state = { NULL, qfalse };
cvar_t *vid_fullscreen;

/**
 * Set video mode.
 * @param mode number of the mode to set
 * @param fullscreen <code>qtrue</code> for a fullscreen mode,
 *     <code>qfalse</code> otherwise
 */
rserr_t GLimp_SetMode( int x, int y, int width, int height, int displayFrequency,
	qboolean fullscreen, qboolean wideScreen )
{
	int colorbits = 0;

#ifdef VIDEOMODE_HACK
	/*
	   SDL Hack
	    We cant switch from one OpenGL video mode to another.
	    Thus we first switch to some stupid 2D mode and then back to OpenGL.
	 */
	if( !glw_state.videonotthefirsttime )
		SDL_SetVideoMode( 0, 0, 0, 0 );
	glw_state.videonotthefirsttime = qtrue;
#endif

	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 4 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
  
  if (r_swapinterval->integer == 0) {
    SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 0 );
  }
  
	if( SDL_SetVideoMode( width, height, colorbits,
	                     SDL_OPENGL | ( fullscreen == qtrue ? SDL_FULLSCREEN : 0 ) ) == NULL )
	{
		Com_Printf( " setting the video mode failed: %s", SDL_GetError() );
		return rserr_invalid_mode;
	}

	glConfig.width = width;
	glConfig.height = height;
	glConfig.fullScreen = fullscreen;
	glConfig.wideScreen = wideScreen;

#if 0
	//Threaded OpenGL, untested, appears to mess up colors on the UI
	CGLContextObj ctx = CGLGetCurrentContext();
	const CGLError err = CGLEnable( ctx, kCGLCEMPEngine );
	if( err == kCGLNoError )
		Com_Printf( "Enabled threaded GL engine" );
	else
		Com_Printf( "Couldn't enable threaded GL engine: %d\n", (int) err );
#endif

	// Restart Input ...
	SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
	SDL_ShowCursor( SDL_DISABLE );
	SDL_EnableUNICODE( SDL_ENABLE );
	SDL_WM_GrabInput( SDL_GRAB_ON );
	SDL_SetCursor( NULL );

	return rserr_ok;
}

/**
 * Shutdown GLimp sub system.
 */
void GLimp_Shutdown()
{

}

/**
 * Initialize GLimp sub system.
 * @param hinstance
 * @param wndproc
 */

int GLimp_Init( const char *applicationName, void *hinstance, void *wndproc, void *parenthWnd )
{
	hinstance = NULL;
	wndproc = NULL;
	parenthWnd = NULL;

	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	Com_Printf( "Display initialization\n" );

	const SDL_VideoInfo *info = NULL;
	info = SDL_GetVideoInfo();
	if( !info )
	{
		Com_Printf( "Video query failed: %s\n", SDL_GetError() );
		return 0;
	}

	SDL_WM_SetCaption( APPLICATION_UTF8, NULL );
	// Restart Input ...
	SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
	SDL_ShowCursor( SDL_DISABLE );
	SDL_EnableUNICODE( SDL_ENABLE );
	SDL_WM_GrabInput( SDL_GRAB_ON );
	SDL_SetCursor( NULL );
	
	return 1;
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
	SDL_GL_SwapBuffers();
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
	
	if( SDL_GetGammaRamp( ramp256, ramp256+256, ramp256+( 256<<1 ) ) != -1 )
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
	if( SDL_SetGammaRamp( ramp256, ramp256+256, ramp256+( 256<<1 ) ) == -1 )
	{
		Com_Printf( "SDL_SetGammaRamp(...) failed: ", SDL_GetError() );
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

/*****************************************************************************/

/*
* Sys_GetClipboardData
*
* Orginally from EzQuake
* There should be a smarter place to put this
*/
char *Sys_GetClipboardData( qboolean primary )
{
	char* clipboard = NULL;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSPasteboard	*myPasteboard = NULL;
    NSArray 		*myPasteboardTypes = NULL;
	
    myPasteboard = [NSPasteboard generalPasteboard];
    myPasteboardTypes = [myPasteboard types];
    if ([myPasteboardTypes containsObject: NSStringPboardType])
    {
        NSString	*myClipboardString;
		
        myClipboardString = [myPasteboard stringForType: NSStringPboardType];
        if (myClipboardString != NULL && [myClipboardString length] > 0)
        {
			int bytes = [myClipboardString length];
			clipboard = malloc( bytes + 1 );
			Q_strncpyz( clipboard, (char *)[myClipboardString UTF8String], bytes + 1 );
        }
    }
	[pool release];
    return (clipboard);
}

/*
* Sys_SetClipboardData
*/
qboolean Sys_SetClipboardData( char *data )
{
	return qtrue;
}

/*
* Sys_FreeClipboardData
*/
void Sys_FreeClipboardData( char *data )
{
	free( data );
}

/*
* Sys_OpenURLInBrowser
*/
void Sys_OpenURLInBrowser( const char *url )
{
  NSString *string_url = [NSString stringWithUTF8String:url];
  NSURL *ns_url = [NSURL URLWithString:string_url];
  [[NSWorkspace sharedWorkspace] openURL:ns_url];
}
