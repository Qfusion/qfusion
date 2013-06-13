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
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Qfusion you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/

#define GL_GLEXT_LEGACY

#include <windows.h>
#include <GL/gl.h>
#include "../qcommon/qcommon.h"
#include "win_glw.h"

#define QGL_EXTERN

#define QGL_FUNC( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_EXT( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_WGL( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_WGL_EXT( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_GLX( type, name, params )
#define QGL_GLX_EXT( type, name, params )

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
	if( glw_state.hinstOpenGL )
		FreeLibrary( glw_state.hinstOpenGL );
	glw_state.hinstOpenGL = NULL;

	qglGetGLWExtensionsString = NULL;

#define QGL_FUNC( type, name, params ) ( q ## name ) = NULL;
#define QGL_EXT( type, name, params ) ( q ## name ) = NULL;
#define QGL_WGL( type, name, params ) ( q ## name ) = NULL;
#define QGL_WGL_EXT( type, name, params ) ( q ## name ) = NULL;
#define QGL_GLX( type, name, params )
#define QGL_GLX_EXT( type, name, params )

#include "../ref_gl/qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC
}

#pragma warning( disable : 4113 4133 4047 )

/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to
** the appropriate GL stuff. In Windows this means doing a
** LoadLibrary and a bunch of calls to GetProcAddress. On other
** operating systems we need to do the right thing, whatever that
** might be.
**
*/
qboolean QGL_Init( const char *dllname )
{
	if( ( glw_state.hinstOpenGL = LoadLibrary( dllname ) ) == 0 )
	{
		char *buf;

		buf = NULL;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPTSTR) &buf, 0, NULL );
		Com_Printf( "%s\n", buf );
		MessageBox( NULL, buf, "Error", 0 /* MB_OK */ );
		return qfalse;
	}

#define QGL_FUNC( type, name, params ) ( q ## name ) = ( void * )GetProcAddress( glw_state.hinstOpenGL, # name ); \
	if( !( q ## name ) ) { Com_Printf( "QGL_Init: Failed to get address for %s\n", # name ); return qfalse; }
#define QGL_EXT( type, name, params ) ( q ## name ) = NULL;
#define QGL_WGL( type, name, params ) ( q ## name ) = ( void * )GetProcAddress( glw_state.hinstOpenGL, # name ); \
	if( !( q ## name ) ) { Com_Printf( "QGL_Init: Failed to get address for %s\n", # name ); return qfalse; }
#define QGL_WGL_EXT( type, name, params ) ( q ## name ) = NULL;
#define QGL_GLX( type, name, params )
#define QGL_GLX_EXT( type, name, params )

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
	return (void *)qwglGetProcAddress( (LPCSTR)procName );
}

/*
** qglGetGLWExtensionsString
*/
static const char *_qglGetGLWExtensionsStringInit( void )
{
	qwglGetExtensionsStringEXT = ( void * )qglGetProcAddress( (const GLubyte *)"wglGetExtensionsStringEXT" );
	qglGetGLWExtensionsString = _qglGetGLWExtensionsString;
	return qglGetGLWExtensionsString();
}

static const char *_qglGetGLWExtensionsString( void )
{
	if( qwglGetExtensionsStringEXT )
		return qwglGetExtensionsStringEXT();
	return NULL;
}
