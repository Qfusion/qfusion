/*
Copyright (C) 2014 SiPlus, Chasseur de bots

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

#include <dlfcn.h>
#include "../qcommon/qcommon.h"
#include "egl_glw.h"

#define QGL_EXTERN

#define QGL_FUNC( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_FUNC_OPT( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_EXT( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_WGL( type, name, params )
#define QGL_WGL_EXT( type, name, params )
#define QGL_GLX( type, name, params )
#define QGL_GLX_EXT( type, name, params )
#define QGL_EGL( type, name, params ) type( APIENTRY * q ## name ) params;
#define QGL_EGL_EXT( type, name, params ) type( APIENTRY * q ## name ) params;

#include "../ref_gl/qgl.h"

#undef QGL_EGL_EXT
#undef QGL_EGL
#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC_OPT
#undef QGL_FUNC

#ifdef __ANDROID__
#define QGL_EGL_LIBNAME "libEGL.so"
#endif

static const char *_qglGetGLWExtensionsString( void );

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QGL_Shutdown( void )
{
	if( glw_state.OpenGLLib )
	{
		dlclose( glw_state.OpenGLLib );
		glw_state.OpenGLLib = NULL;
	}
	if( glw_state.EGLLib )
	{
		dlclose( glw_state.EGLLib );
		glw_state.EGLLib = NULL;
	}
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
qgl_initerr_t QGL_Init( const char *dllname )
{
	if( !( glw_state.EGLLib = dlopen( QGL_EGL_LIBNAME, RTLD_LAZY | RTLD_GLOBAL ) ) )
	{
		Com_Printf( "%s\n", dlerror() );
		return qgl_initerr_unknown;
	}

	if( !( glw_state.OpenGLLib = dlopen( dllname, RTLD_LAZY | RTLD_GLOBAL ) ) )
	{
		Com_Printf( "%s\n", dlerror() );
		return qgl_initerr_invalid_driver;
	}
	else
	{
		Com_Printf( "Using %s for OpenGL...\n", dllname );
	}

#define QGL_FUNC( type, name, params ) ( q ## name ) = ( void * )dlsym( glw_state.OpenGLLib, # name ); \
	if( !( q ## name ) ) { Com_Printf( "QGL_Init: Failed to get address for %s\n", # name ); return qgl_initerr_invalid_driver; }
#define QGL_FUNC_OPT( type, name, params ) ( q ## name ) = ( void * )dlsym( glw_state.OpenGLLib, # name );
#define QGL_EXT( type, name, params ) ( q ## name ) = NULL;
#define QGL_WGL( type, name, params )
#define QGL_WGL_EXT( type, name, params )
#define QGL_GLX( type, name, params )
#define QGL_GLX_EXT( type, name, params )
#define QGL_EGL( type, name, params ) ( q ## name ) = ( void * )dlsym( glw_state.EGLLib, # name ); \
	if( !( q ## name ) ) { Com_Printf( "QGL_Init: Failed to get address for %s\n", # name ); return qgl_initerr_unknown; }
#define QGL_EGL_EXT( type, name, params ) ( q ## name ) = NULL;

#include "../ref_gl/qgl.h"

#undef QGL_EGL_EXT
#undef QGL_EGL
#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC_OPT
#undef QGL_FUNC

	qglGetGLWExtensionsString = _qglGetGLWExtensionsString;

	return qgl_initerr_ok;
}

/*
** QGL_GetDriverInfo
**
** Returns information about the GL DLL.
*/
const qgl_driverinfo_t *QGL_GetDriverInfo( void )
{
	// libGLESv2 exposes GLES2 and above, libGLESv3 is a symlink to it
	static const qgl_driverinfo_t driver =
	{
#ifdef __ANDROID__
		"libGLESv2.so",
		"gl_driver_android"
#endif
	};
	return &driver;
}


/*
** qglGetProcAddress
*/
void *qglGetProcAddress( const GLubyte *procName )
{
	return (void *)qeglGetProcAddress( (const char *)procName );
}

/*
** _qglGetGLWExtensionsString
*/
static const char *_qglGetGLWExtensionsString( void )
{
	EGLDisplay dpy = qeglGetDisplay( EGL_DEFAULT_DISPLAY );
	if( dpy == EGL_NO_DISPLAY )
		return NULL;
	return qeglQueryString( dpy, EGL_EXTENSIONS );
}
