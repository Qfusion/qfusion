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

#ifndef __ANDROID__
#error You should not be including this file on this platform
#endif

#ifndef GLW_EGL_H
#define GLW_EGL_H

#include <EGL/egl.h>

typedef struct
{
	EGLDisplay display;
	EGLConfig config;
	int format;

	bool multithreadedRendering;

	EGLContext context; // Main thread context

	EGLSurface mainThreadPbuffer; // Bound to the main thread when the rendering thread is active

	// Window surface and fake minimized surface - used on the thread that is currently rendering
	EGLSurface surface;
	int swapInterval;
	EGLSurface noWindowPbuffer;

	// Window replacement in the rendering thread
	qmutex_t *windowMutex;
	volatile EGLNativeWindowType window; // Only main can change
	volatile bool windowChanged;

	void *EGLLib, *OpenGLLib;
} glwstate_t;

extern glwstate_t glw_state;

#endif // GLW_EGL_H
