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
** QGL_LINUX.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Qfusion you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/

#include <dlfcn.h>

#include "../qcommon/qcommon.h"
#include "x11.h"
#include "unix_glw.h"

#define QGL_EXTERN

#define QGL_FUNC( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_EXT( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_WGL( type, name, params )
#define QGL_WGL_EXT( type, name, params )
#define QGL_GLX( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_GLX_EXT( type, name, params ) type( APIENTRY * q ## name ) params;

#include "../ref_gl/qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC

static const char *_qglGetGLWExtensionsString( void );
static const char *_qglGetGLWExtensionsStringInit( void );

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QGL_Shutdown( void )
{
	if( glw_state.OpenGLLib )
		dlclose( glw_state.OpenGLLib );
	glw_state.OpenGLLib = NULL;

	qglGetGLWExtensionsString = NULL;

#define QGL_FUNC( type, name, params ) ( q ## name ) = NULL;
#define QGL_EXT( type, name, params ) ( q ## name ) = NULL;
#define QGL_WGL( type, name, params )
#define QGL_WGL_EXT( type, name, params )
#define QGL_GLX( type, name, params ) ( q ## name ) = NULL;
#define QGL_GLX_EXT( type, name, params ) ( q ## name ) = NULL;

#include "../ref_gl/qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC
}

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
qboolean QGL_Init( const char *dllname )
{
	if( ( glw_state.OpenGLLib = dlopen( dllname, RTLD_LAZY|RTLD_GLOBAL ) ) == 0 )
	{
		Com_Printf( "%s\n", dlerror() );
		return qfalse;
	}
	else
	{
		Com_Printf( "Using %s for OpenGL...", dllname );
	}

#define QGL_FUNC( type, name, params ) ( q ## name ) = ( void * )qglGetProcAddress( (const GLubyte *)# name ); \
	if( !( q ## name ) ) { Com_Printf( "QGL_Init: Failed to get address for %s\n", # name ); return qfalse; }
#define QGL_EXT( type, name, params ) ( q ## name ) = NULL;
#define QGL_WGL( type, name, params )
#define QGL_WGL_EXT( type, name, params )
#define QGL_GLX( type, name, params ) ( q ## name ) = ( void * )dlsym( glw_state.OpenGLLib, # name ); \
	if( !( q ## name ) ) { Com_Printf( "QGL_Init: Failed to get address for %s\n", # name ); return qfalse; }
#define QGL_GLX_EXT( type, name, params ) ( q ## name ) = NULL;

#include "../ref_gl/qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC

	qglGetGLWExtensionsString = _qglGetGLWExtensionsStringInit;

	return qtrue;
}

/*
** qglGetProcAddress
*/
void *qglGetProcAddress( const GLubyte *procName )
{
#if 1
	if( qglXGetProcAddressARB )
		return qglXGetProcAddressARB( procName );
#endif
	if( glw_state.OpenGLLib )
		return (void *)dlsym( glw_state.OpenGLLib, (char *) procName );
	return NULL;
}

/*
** qglGetGLWExtensionsString
*/
static const char *_qglGetGLWExtensionsStringInit( void )
{
	int major = 0, minor = 0;

	if( !qglXQueryVersion || !qglXQueryVersion( x11display.dpy, &major, &minor ) || !( minor > 0 || major > 1 ) )
		qglXQueryExtensionsString = NULL;
	qglGetGLWExtensionsString = _qglGetGLWExtensionsString;

	return qglGetGLWExtensionsString();
}

static const char *_qglGetGLWExtensionsString( void )
{
	if( qglXQueryExtensionsString )
		return qglXQueryExtensionsString( x11display.dpy, x11display.scr );
	return NULL;
}
