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
This code is part of DynGL, a method of dynamically loading an OpenGL
library without much pain designed by Joseph Carter and is based
loosely on previous work done both by Zephaniah E. Hull and Joseph.

Both contributors have decided to disclaim all Copyright to this work.
It is released to the Public Domain WITHOUT ANY WARRANTY whatsoever,
express or implied, in the hopes that others will use it instead of
other less-evolved hacks which usually don't work right.  ;)
*/

/*
The following code is loosely based on DynGL code by Joseph Carter
and Zephaniah E. Hull. Adapted by Victor Luchits for qfusion project.
*/

/*
** SDL_QGL.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Qfusion you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/

#include <SDL.h>

#include "../qcommon/qcommon.h"
#include "sdl_glw.h"
#include "../ref_gl/qgl.h"
#include "../ref_gl/glad.h"

/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to
** the appropriate GL stuff.  In Windows this means doing a
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
**
*/
qgl_initerr_t QGL_Init() {
	if( SDL_InitSubSystem( SDL_INIT_VIDEO ) < 0 ) {
		Com_Printf( "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s", SDL_GetError() );
		return qgl_initerr_unknown;
	}

	if( gladLoadGLLoader( SDL_GL_GetProcAddress ) != 1 ) {
		Com_Printf( "Error loading OpenGL\n" );
		return qgl_initerr_invalid_driver;
	}

	return qgl_initerr_ok;
}

void QGL_Shutdown( void ) {
	SDL_QuitSubSystem( SDL_INIT_VIDEO );
}
