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
** GLW_IMP.C
**
** This file contains ALL Linux specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/

#include "../ref_gl/r_local.h"

#include "../qcommon/version.h"

#include "x11.h"

#include "unix_glw.h"

#define DISPLAY_MASK ( VisibilityChangeMask | StructureNotifyMask | ExposureMask | PropertyChangeMask )
#define INIT_MASK ( KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask | DISPLAY_MASK )

// use experimental Xrandr resolution?
#define _XRANDR_OVER_VIDMODE_

x11display_t x11display;
x11wndproc_t x11wndproc;

glwstate_t glw_state;

static int _vid_display_refresh_rate = 0;
static bool _xf86_vidmodes_supported = false;
static int default_dotclock, default_viewport[2];
static XF86VidModeModeLine default_modeline;
static XF86VidModeModeInfo **_xf86_vidmodes;
static int _xf86_vidmodes_num;
static bool _xf86_vidmodes_active = false;
static bool _xf86_xinerama_supported = false;

static void _xf86_VidmodesInit( void )
{
	int MajorVersion = 0, MinorVersion = 0;

	// Get video mode list
	if( XF86VidModeQueryVersion( x11display.dpy, &MajorVersion, &MinorVersion ) )
	{
		ri.Com_Printf( "..XFree86-VidMode Extension Version %d.%d\n", MajorVersion, MinorVersion );
		XF86VidModeGetViewPort( x11display.dpy, x11display.scr, &default_viewport[0], &default_viewport[1] );
		XF86VidModeGetModeLine( x11display.dpy, x11display.scr, &default_dotclock, &default_modeline );
		XF86VidModeGetAllModeLines( x11display.dpy, x11display.scr, &_xf86_vidmodes_num, &_xf86_vidmodes );
		_xf86_vidmodes_supported = true;
	}
	else
	{
		ri.Com_Printf( "..XFree86-VidMode Extension not available\n" );
		_xf86_vidmodes_supported = false;
	}
}

static void _xf86_VidmodesFree( void )
{
	if( _xf86_vidmodes_supported ) XFree( _xf86_vidmodes );

	_xf86_vidmodes_supported = false;
}

static void _xf86_XineramaInit( void )
{
	int MajorVersion = 0, MinorVersion = 0;

	if( ( XineramaQueryVersion( x11display.dpy, &MajorVersion, &MinorVersion ) ) && ( XineramaIsActive( x11display.dpy ) ) )
	{
		ri.Com_Printf( "..XFree86-Xinerama Extension Version %d.%d\n", MajorVersion, MinorVersion );
		_xf86_xinerama_supported = true;
	}
	else
	{
		ri.Com_Printf( "..XFree86-Xinerama Extension not available\n" );
		_xf86_xinerama_supported = false;
	}
}

static void _xf86_XineramaFree( void )
{
	_xf86_xinerama_supported = false;
}

static bool _xf86_XineramaFindBest( int *x, int *y, int *width, int *height, bool silent )
{
	int i, screens, head;
	int best_dist, dist;
	XineramaScreenInfo *xinerama;
	cvar_t *vid_multiscreen_head;

	assert( _xf86_xinerama_supported );

	vid_multiscreen_head = ri.Cvar_Get( "vid_multiscreen_head", "0", CVAR_ARCHIVE );
	vid_multiscreen_head->modified = false;

	if( vid_multiscreen_head->integer == 0 )
		return false;

	xinerama = XineramaQueryScreens( x11display.dpy, &screens );
	if( screens <= 1 )
		return false;

	head = -1;
	if( vid_multiscreen_head->integer > 0 )
	{
		for( i = 0; i < screens; i++ )
		{
			if( xinerama[i].screen_number == vid_multiscreen_head->integer - 1 )
			{
				head = i;
				break;
			}
		}
		if( head == -1 && !silent )
			ri.Com_Printf( "Xinerama: Head %i not found, using best fit\n", vid_multiscreen_head->integer );
		if( *width > xinerama[head].width || *height > xinerama[head].height )
		{
			head = -1;
			if( !silent )
				ri.Com_Printf( "Xinerama: Window doesn't fit into head %i, using best fit\n", vid_multiscreen_head->integer );
		}
	}

	if( head == -1 ) // find best fit
	{
		best_dist = 999999999;
		for( i = 0; i < screens; i++ )
		{
			if( *width <= xinerama[i].width && *height <= xinerama[i].height )
			{
				if( xinerama[i].width - *width > xinerama[i].height - *height )
				{
					dist = xinerama[i].height - *height;
				}
				else
				{
					dist = xinerama[i].width - *width;
				}

				if( dist < 0 )
					dist = -dist; // Only positive number please

				if( dist < best_dist )
				{
					best_dist = dist;
					head = i;
				}
			}
		}
		if( head < -1 )
		{
			if( !silent )
				ri.Com_Printf( "Xinerama: No fitting head found" );
			return false;
		}
	}

	*x = xinerama[head].x_org;
	*y = xinerama[head].y_org;
	*width = xinerama[head].width;
	*height = xinerama[head].height;

	if( !silent )
		ri.Com_Printf( "Xinerama: Using screen %d: %dx%d+%d+%d\n", xinerama[head].screen_number, xinerama[head].width, xinerama[head].height, xinerama[head].x_org, xinerama[head].y_org );

	return true;
}

static void _xf86_VidmodesSwitch( int mode )
{
	if( _xf86_vidmodes_supported )
	{
		XF86VidModeSwitchToMode( x11display.dpy, x11display.scr, _xf86_vidmodes[mode] );
		XF86VidModeSetViewPort( x11display.dpy, x11display.scr, 0, 0 );
	}

	_xf86_vidmodes_active = true;
}

static void _xf86_VidmodesSwitchBack( void )
{
	if( _xf86_vidmodes_supported && _xf86_vidmodes_active )
	{
		XF86VidModeModeInfo modeinfo;

		modeinfo.dotclock = default_dotclock;
		modeinfo.hdisplay = default_modeline.hdisplay;
		modeinfo.hsyncstart = default_modeline.hsyncstart;
		modeinfo.hsyncend = default_modeline.hsyncend;
		modeinfo.htotal = default_modeline.htotal;
		modeinfo.vdisplay = default_modeline.vdisplay;
		modeinfo.vsyncstart = default_modeline.vsyncstart;
		modeinfo.vsyncend = default_modeline.vsyncend;
		modeinfo.vtotal = default_modeline.vtotal;
		modeinfo.flags = default_modeline.flags;
		modeinfo.privsize = default_modeline.privsize;
		modeinfo.private = default_modeline.private;

		XF86VidModeSwitchToMode( x11display.dpy, x11display.scr, &modeinfo );
		XF86VidModeSetViewPort( x11display.dpy, x11display.scr, default_viewport[0], default_viewport[1] );
	}

	_xf86_vidmodes_active = false;
}

static void _xf86_VidmodesFindBest( int *mode, int *pwidth, int *pheight, bool silent )
{
	int i, best_fit, best_dist, dist, x, y;

	best_fit = -1;
	best_dist = 999999999;

	if( _xf86_vidmodes_supported )
	{
		for( i = 0; i < _xf86_vidmodes_num; i++ )
		{
			if( _xf86_vidmodes[i]->hdisplay < *pwidth || _xf86_vidmodes[i]->vdisplay < *pheight )
				continue;

			x = _xf86_vidmodes[i]->hdisplay - *pwidth;
			y = _xf86_vidmodes[i]->vdisplay - *pheight;

			if( x > y ) dist = y;
			else dist = x;

			if( dist < 0 ) dist = -dist; // Only positive number please

			if( dist < best_dist )
			{
				best_dist = dist;
				best_fit = i;
			}

			//if( !silent )
			//	ri.Com_Printf( "%ix%i -> %ix%i: %i\n", *pwidth, *pheight, _xf86_vidmodes[i]->hdisplay, _xf86_vidmodes[i]->vdisplay, dist );
		}

		if( best_fit >= 0 )
		{
			//if( !silent )
			//	ri.Com_Printf( "%ix%i selected\n", _xf86_vidmodes[best_fit]->hdisplay, _xf86_vidmodes[best_fit]->vdisplay );

			*pwidth = _xf86_vidmodes[best_fit]->hdisplay;
			*pheight = _xf86_vidmodes[best_fit]->vdisplay;
		}
	}

	*mode = best_fit;
}

#ifdef _XRANDR_OVER_VIDMODE_
// XRANDR
bool _xrandr_supported = false;
bool _xrandr_active = false;
int _xrandr_eventbase;
int _xrandr_errorbase;
// list of resolutions
XRRScreenConfiguration *_xrandr_config = 0;
XRRScreenSize *_xrandr_sizes = 0;
int _xrandr_numsizes = 0;
// original rate and resolution
short _xrandr_default_rate;
Rotation _xrandr_default_rotation;
SizeID _xrandr_default_size;

static void _xf86_XrandrInit( void )
{
	int MajorVersion = 0, MinorVersion = 0;

	// Already initialized?
	if( _xrandr_supported )
		return;

	// Check extension
	if( XRRQueryExtension( x11display.dpy, &_xrandr_eventbase, &_xrandr_errorbase ) &&
		XRRQueryVersion( x11display.dpy, &MajorVersion, &MinorVersion ) )
	{
		ri.Com_Printf( "..Xrandr Extension Version %d.%d\n", MajorVersion, MinorVersion );

		// Get current resolution
		_xrandr_config = XRRGetScreenInfo( x11display.dpy, x11display.root );
		_xrandr_default_rate = XRRConfigCurrentRate( _xrandr_config );
		_xrandr_default_size = XRRConfigCurrentConfiguration( _xrandr_config, &_xrandr_default_rotation );

		// Get a list of resolutions (first here is actually the current resolution too ^^)
		_xrandr_sizes = XRRSizes( x11display.dpy, 0, &_xrandr_numsizes );
		_xrandr_supported = true;
	}
	else
	{
		ri.Com_Printf( "..Xrandr Extension not available\n" );
		_xrandr_supported = false;
	}
}

static void _xf86_XrandrFree( void )
{
	if( _xrandr_supported )
		XRRFreeScreenConfigInfo( _xrandr_config );

	_xrandr_config = 0;
	_xrandr_supported = false;
	_xrandr_active = false;
}

static short _xf86_XrandrClosestRate( int mode, short preferred_rate )
{
	short *rates, delta, min, best;
	int i, numrates;

	// fetch the rates for given resolution
	rates = XRRRates( x11display.dpy, x11display.scr, mode, &numrates );

	min = 0x7fff;
	best = 0;
	for( i = 0; i < numrates; i++ )
	{
		if( rates[i] > preferred_rate )
			continue;
		delta = preferred_rate - rates[i];
		if( delta < min )
		{
			best = rates[i];
			min = delta;
		}

		//ri.Com_Printf("  rate %i -> %i: %i\n", preferred_rate, rates[i], delta );
	}

	//ri.Com_Printf("_xf86_XrandrClosestRate found %i for %i\n", best, preferred_rate );
	return best;
}

static void _xf86_XrandrSwitch( int mode, int refresh_rate )
{
	if( _xrandr_supported )
	{
		short rate;

		// prefer user defined rate
		_vid_display_refresh_rate = refresh_rate;
		rate = _vid_display_refresh_rate ? _vid_display_refresh_rate : _xrandr_default_rate;

		// find the closest rate on this resolution
		rate = _xf86_XrandrClosestRate( mode, rate );

		/* CHANGE RESOLUTION */
		XRRSetScreenConfigAndRate( x11display.dpy, _xrandr_config, x11display.root, mode, RR_Rotate_0, rate, CurrentTime );

		// "Clients must call back into Xlib using XRRUpdateConfiguration when screen configuration change notify events are generated"
		// ??
	}

	_xrandr_active = true;
}

static void _xf86_XrandrSwitchBack( void )
{
	if( _xrandr_supported && _xrandr_active )
	{
		XRRSetScreenConfigAndRate( x11display.dpy, _xrandr_config, x11display.root, _xrandr_default_size, _xrandr_default_rotation, _xrandr_default_rate, CurrentTime );
	}

	_xrandr_active = false;
}

static void _xf86_XrandrFindBest( int *mode, int *pwidth, int *pheight, bool silent )
{
	int i, best_fit, best_dist, dist, x, y;

	best_fit = -1;
	best_dist = 999999999;

	if( _xrandr_supported )
	{
		for( i = 0; i < _xrandr_numsizes; i++ )
		{
			if( _xrandr_sizes[i].width < *pwidth || _xrandr_sizes[i].height < *pheight )
				continue;

			x = _xrandr_sizes[i].width - *pwidth;
			y = _xrandr_sizes[i].height - *pheight;

			if( x > y ) dist = y;
			else dist = x;

			if( dist < 0 ) dist = -dist; // Only positive number please

			if( dist < best_dist )
			{
				best_dist = dist;
				best_fit = i;
			}

			//if( !silent )
			//	ri.Com_Printf( "%ix%i -> %ix%i: %i\n", *pwidth, *pheight, _xrandr_sizes[i].width, _xrandr_sizes[i].height, dist );
		}

		if( best_fit >= 0 )
		{
			//if( !silent )
			//	ri.Com_Printf( "%ix%i selected\n", _xrandr_sizes[best_fit].width, _xrandr_sizes[best_fit].height );

			*pwidth = _xrandr_sizes[best_fit].width;
			*pheight = _xrandr_sizes[best_fit].height;
		}
	}

	*mode = best_fit;
}

#endif

static void _x11_SetNoResize( Window w, int width, int height )
{
	XSizeHints *hints;

	if( x11display.dpy )
	{
		hints = XAllocSizeHints();

		if( hints )
		{
			hints->min_width = hints->max_width = width;
			hints->min_height = hints->max_height = height;

			hints->flags = PMaxSize | PMinSize;

			XSetWMNormalHints( x11display.dpy, w, hints );
			XFree( hints );
		}
	}
}

/*****************************************************************************/

/*
* _NET_WM_CHECK_SUPPORTED
*/
static bool _NET_WM_CHECK_SUPPORTED( Atom NET_ATOM )
{
	bool issupported = false;
	unsigned char *atomdata;
	Atom *atoms;
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left, i;
	int result = 1;
	Atom _NET_SUPPORTED;

	_NET_SUPPORTED = XInternAtom( x11display.dpy, "_NET_SUPPORTED", 0 );

	status = XGetWindowProperty( x11display.dpy, x11display.root, _NET_SUPPORTED,
		0L, 8192L, False, XA_ATOM, &real_type, &real_format,
		&items_read, &items_left, &atomdata );

	if( status != Success )
		return false;

	atoms = (Atom *)atomdata;
	for( i = 0; result && i < items_read; i++ )
	{
		if( atoms[i] == NET_ATOM )
		{
			issupported = true;
			break;
		}
	}

	XFree( atomdata );
	return issupported;
}

/*
* _NET_WM_STATE_FULLSCREEN_SUPPORTED
*/
static bool _NET_WM_STATE_FULLSCREEN_SUPPORTED( void )
{
	Atom _NET_WM_STATE_FULLSCREEN = XInternAtom( x11display.dpy, "_NET_WM_STATE_FULLSCREEN", 0 );
	return _NET_WM_CHECK_SUPPORTED( _NET_WM_STATE_FULLSCREEN );
}

/*
* _NETWM_CHECK_FULLSCREEN
*/
static bool _NETWM_CHECK_FULLSCREEN( void )
{
	bool isfullscreen = false;
	unsigned char *atomdata;
	Atom *atoms;
	int status, real_format;
	Atom real_type;
	unsigned long items_read, items_left, i;
	int result = 1;
	Atom _NET_WM_STATE;
	Atom _NET_WM_STATE_FULLSCREEN;
	cvar_t *vid_fullscreen;

	if( !x11display.features.wmStateFullscreen )
		return false;

	_NET_WM_STATE = XInternAtom( x11display.dpy, "_NET_WM_STATE", 0 );
	_NET_WM_STATE_FULLSCREEN = XInternAtom( x11display.dpy, "_NET_WM_STATE_FULLSCREEN", 0 );

	status = XGetWindowProperty( x11display.dpy, x11display.win, _NET_WM_STATE,
		0L, 8192L, False, XA_ATOM, &real_type, &real_format,
		&items_read, &items_left, &atomdata );

	if( status != Success )
		return false;

	atoms = (Atom *)atomdata;
	for( i = 0; result && i < items_read; i++ )
	{
		if( atoms[i] == _NET_WM_STATE_FULLSCREEN )
		{
			isfullscreen = true;
			break;
		}
	}

	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	glConfig.fullScreen = isfullscreen;
	ri.Cvar_SetValue( vid_fullscreen->name, isfullscreen ? 1 : 0 );
	vid_fullscreen->modified = false;

	XFree( atomdata );
	return isfullscreen;
}

/*
* _NETWM_SET_FULLSCREEN
*
* Tell Window-Manager to toggle fullscreen
*/
static void _NETWM_SET_FULLSCREEN( bool fullscreen )
{
	XEvent xev;
	Atom NET_WM_STATE;
	Atom NET_WM_STATE_FULLSCREEN;
	Atom NET_WM_BYPASS_COMPOSITOR;

	if( !x11display.features.wmStateFullscreen )
		return;

	NET_WM_BYPASS_COMPOSITOR = XInternAtom( x11display.dpy, "_NET_WM_BYPASS_COMPOSITOR", False );
	XChangeProperty( x11display.dpy, x11display.win, NET_WM_BYPASS_COMPOSITOR, XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&fullscreen, 1 );

	NET_WM_STATE = XInternAtom( x11display.dpy, "_NET_WM_STATE", False );
	NET_WM_STATE_FULLSCREEN = XInternAtom( x11display.dpy, "_NET_WM_STATE_FULLSCREEN", False );

	if( fullscreen ) {
		XChangeProperty( x11display.dpy, x11display.win, NET_WM_STATE, XA_ATOM, 32,
			PropModeReplace, (unsigned char *)&NET_WM_STATE_FULLSCREEN, 1 );
	}
	else {
        // Removing fullscreen property if it is present (we already set it).
		XDeleteProperty( x11display.dpy, x11display.win, NET_WM_STATE );
	}

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = x11display.win;
	xev.xclient.message_type = NET_WM_STATE;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = fullscreen ? 1 : 0;
	xev.xclient.data.l[1] = NET_WM_STATE_FULLSCREEN;
	xev.xclient.data.l[2] = 0;
 
	XMapWindow( x11display.dpy, x11display.win );

	XSendEvent( x11display.dpy, DefaultRootWindow( x11display.dpy ), False,
		SubstructureRedirectMask | SubstructureNotifyMask, &xev );
}

/*****************************************************************************/

static void GLimp_SetXPMIcon( const int *xpm_icon )
{
	int width, height;
	size_t i, cardinalSize;
	long *cardinalData;
	Atom NET_WM_ICON;
	Atom CARDINAL;

	if( !xpm_icon ) {
		return;
	}

	// allocate memory for icon data: width + height + width * height pixels
	// note: sizeof(long) shall be used according to XChangeProperty() man page
	width = xpm_icon[0];
	height = xpm_icon[1];
	cardinalSize = width * height + 2;
	cardinalData = malloc( cardinalSize * sizeof( *cardinalData ) );
	for( i = 0; i < cardinalSize; i++ )
		cardinalData[i] = xpm_icon[i];

	NET_WM_ICON = XInternAtom( x11display.dpy, "_NET_WM_ICON", False );
	CARDINAL = XInternAtom( x11display.dpy, "CARDINAL", False );

	XChangeProperty( x11display.dpy, x11display.win, NET_WM_ICON, CARDINAL, 32,
		PropModeReplace, (unsigned char *)cardinalData, cardinalSize );

	free( cardinalData );
}

/*****************************************************************************/

/*
** GLimp_SetMode_Real
* Hack to get rid of the prints when toggling fullscreen
*/
static rserr_t GLimp_SetMode_Real( int width, int height, int displayFrequency, bool fullscreen, bool silent, bool force )
{
	int screen_x, screen_y, screen_width, screen_height, screen_mode;
	float ratio;
	XSetWindowAttributes wa;
	unsigned long mask;
	XClassHint *class_hint;

	if( x11display.dpy ) {
		if( !force && (glConfig.width == width) && (glConfig.height == height) && (glConfig.fullScreen != fullscreen) ) {
			// fullscreen toggle
			_NETWM_SET_FULLSCREEN( fullscreen );
			_NETWM_CHECK_FULLSCREEN();
			return glConfig.fullScreen == fullscreen ? rserr_ok : rserr_restart_required;
		}
	}

	screen_mode = -1;
	screen_x = screen_y = 0;
	screen_width = width;
	screen_height = height;

	x11display.old_win = x11display.win;

	if( fullscreen )
	{
		if( !_xf86_xinerama_supported ||
			!_xf86_XineramaFindBest( &screen_x, &screen_y, &screen_width, &screen_height, silent ) )
		{
#ifdef _XRANDR_OVER_VIDMODE_
			_xf86_XrandrFindBest( &screen_mode, &screen_width, &screen_height, silent );
#else
			_xf86_VidmodesFindBest( &screen_mode, &screen_width, &screen_height, silent );
#endif
			if( screen_mode < 0 )
			{
				if( !silent )
					ri.Com_Printf( " no mode found\n" );
				return rserr_invalid_fullscreen;
			}
		}

		if( screen_width < width || screen_height < height )
		{
			if( width > height )
			{
				ratio = width / height;
				height = height * ratio;
				width = screen_width;
			}
			else
			{
				ratio = height / width;
				width = width * ratio;
				height = screen_height;
			}
		}

		if( !silent )
			ri.Com_Printf( "...setting fullscreen mode %ix%i:\n", width, height );

		/* Create fulscreen window */
		wa.background_pixel = 0;
		wa.border_pixel = 0;
		wa.event_mask = INIT_MASK;
		wa.backing_store = NotUseful;
		wa.save_under = False;

		if( x11display.features.wmStateFullscreen )
		{
			wa.override_redirect = False;
			mask = CWBackPixel | CWBorderPixel | CWEventMask | CWSaveUnder | CWBackingStore;
		}
		else
		{
			wa.override_redirect = True;
			mask = CWBackPixel | CWBorderPixel | CWEventMask | CWSaveUnder | CWBackingStore | CWOverrideRedirect;
		}

		x11display.wa = wa;

		x11display.win = XCreateWindow( x11display.dpy, x11display.root, screen_x, screen_y, screen_width, screen_height,
			0, CopyFromParent, InputOutput, CopyFromParent, mask, &wa );

		XResizeWindow( x11display.dpy, x11display.gl_win, width, height );
		XReparentWindow( x11display.dpy, x11display.gl_win, x11display.win, ( screen_width/2 )-( width/2 ),
			( screen_height/2 )-( height/2 ) );

		x11display.modeset = true;

		XMapWindow( x11display.dpy, x11display.gl_win );
		XMapWindow( x11display.dpy, x11display.win );

		if ( !x11display.features.wmStateFullscreen )
			_x11_SetNoResize( x11display.win, width, height );
		else
			_NETWM_SET_FULLSCREEN( true );

		if( screen_mode != -1 )
		{
#ifdef _XRANDR_OVER_VIDMODE_
			_xf86_XrandrSwitch( screen_mode, displayFrequency );
#else
			_xf86_VidmodesSwitch( screen_mode );
#endif
		}
	}
	else
	{
		if( !silent )
			ri.Com_Printf( "...setting mode %ix%i:\n", width, height );

		/* Create managed window */
		wa.background_pixel = 0;
		wa.border_pixel = 0;
		wa.event_mask = INIT_MASK;
		mask = CWBackPixel | CWBorderPixel | CWEventMask;
		x11display.wa = wa;

		x11display.win = XCreateWindow( x11display.dpy, x11display.root, 0, 0, screen_width, screen_height,
			0, CopyFromParent, InputOutput, CopyFromParent, mask, &wa );
		x11display.wmDeleteWindow = XInternAtom( x11display.dpy, "WM_DELETE_WINDOW", False );
		XSetWMProtocols( x11display.dpy, x11display.win, &x11display.wmDeleteWindow, 1 );

		XResizeWindow( x11display.dpy, x11display.gl_win, width, height );
		XReparentWindow( x11display.dpy, x11display.gl_win, x11display.win, 0, 0 );

		x11display.modeset = true;

		XMapWindow( x11display.dpy, x11display.gl_win );
		XMapWindow( x11display.dpy, x11display.win );

		if( !x11display.features.wmStateFullscreen )
			_x11_SetNoResize( x11display.win, width, height );
		else
			_NETWM_SET_FULLSCREEN( false );

#ifdef _XRANDR_OVER_VIDMODE_
		_xf86_XrandrSwitchBack();
#else
		_xf86_VidmodesSwitchBack();
#endif
	}

	XSetStandardProperties( x11display.dpy, x11display.win, glw_state.applicationName, None, None, NULL, 0, NULL );

	GLimp_SetXPMIcon( glw_state.applicationIcon );

	XSetIconName( x11display.dpy, x11display.win, glw_state.applicationName );
	XStoreName( x11display.dpy, x11display.win, glw_state.applicationName );

	class_hint = XAllocClassHint();
	if( class_hint ) {
		class_hint->res_name = glw_state.applicationName;
		class_hint->res_class = glw_state.applicationName;
		XSetClassHint( x11display.dpy, x11display.win, class_hint );
		XFree( class_hint );
	}

	// save the parent window size for mouse use. this is not the gl context window
	x11display.win_width = width;
	x11display.win_height = height;

	if( x11display.old_win )
	{
		XDestroyWindow( x11display.dpy, x11display.old_win );
		x11display.old_win = 0;
	}

	XFlush( x11display.dpy );

	glConfig.width = width;
	glConfig.height = height;
	glConfig.fullScreen = fullscreen;

	_NETWM_CHECK_FULLSCREEN();

	if( x11wndproc ) {
		x11wndproc( &x11display, 0, 0, 0 );
	}

	return rserr_ok;
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int x, int y, int width, int height, int displayFrequency, bool fullscreen, bool stereo )
{
	return GLimp_SetMode_Real( width, height, displayFrequency, fullscreen, false, false );
}

/*
** GLimp_Shutdown
*/
void GLimp_Shutdown( void )
{
	if( x11display.dpy )
	{
#ifdef _XRANDR_OVER_VIDMODE_
		_xf86_XrandrSwitchBack();
		_xf86_XrandrFree();
#else
		_xf86_VidmodesSwitchBack();
		_xf86_VidmodesFree();
#endif
		_xf86_XineramaFree();

		if( x11display.cmap ) XFreeColormap( x11display.dpy, x11display.cmap );
		if( x11display.ctx ) qglXDestroyContext( x11display.dpy, x11display.ctx );
		if( x11display.gl_win ) XDestroyWindow( x11display.dpy, x11display.gl_win );
		if( x11display.win ) XDestroyWindow( x11display.dpy, x11display.win );

		XCloseDisplay( x11display.dpy );
	}

	memset(&x11display.features, 0, sizeof(x11display.features));
	x11display.modeset = false;
	x11display.visinfo = NULL;
	x11display.cmap = 0;
	x11display.ctx = NULL;
	x11display.gl_win = 0;
	x11display.win = 0;
	x11display.dpy = NULL;

	if( x11wndproc ) {
		x11wndproc( &x11display, 0, 0, 0 );
	}

	free( glw_state.applicationName );
	free( glw_state.applicationIcon );

	glw_state.applicationName = NULL;
	glw_state.applicationIcon = NULL;
}

static int gotstencil = 0; // evil hack!
static bool ChooseVisual( int colorbits, int stencilbits )
{
	int colorsize;
	int depthbits = colorbits;

	if( colorbits == 32 ) colorsize = 8;
	else colorsize = 4;

	{
		int attributes[] = {
			GLX_RGBA,
			GLX_DOUBLEBUFFER,
			GLX_RED_SIZE, colorsize,
			GLX_GREEN_SIZE, colorsize,
			GLX_BLUE_SIZE, colorsize,
			GLX_ALPHA_SIZE, colorsize,
			GLX_DEPTH_SIZE, depthbits,
			GLX_STENCIL_SIZE, stencilbits,
			None
		};

		x11display.visinfo = qglXChooseVisual( x11display.dpy, x11display.scr, attributes );
		if( !x11display.visinfo )
		{
			ri.Com_Printf( "..Failed to get colorbits %i, depthbits %i, stencilbits %i\n", colorbits, depthbits, stencilbits );
			return false;
		}
		else
		{
			ri.Com_Printf( "..Got colorbits %i, depthbits %i, stencilbits %i\n", colorbits, depthbits, stencilbits );
			gotstencil = stencilbits;
			return true;
		}
	}
}

/*
** GLimp_Init
*/
int GLimp_Init( const char *applicationName, void *hinstance, void *wndproc, void *parenthWnd,
	int iconResource, const int *iconXPM )
{
	int colorbits, stencilbits;
	XSetWindowAttributes attr;
	unsigned long mask;

	glw_state.applicationName = strdup( applicationName );
	glw_state.applicationIcon = NULL;

	if( iconXPM )
	{
		size_t icon_memsize = iconXPM[0] * iconXPM[1] * sizeof( int );
		glw_state.applicationIcon = malloc( icon_memsize );
		memcpy( glw_state.applicationIcon, iconXPM, icon_memsize );
	}
	
	hinstance = NULL;
	x11wndproc = (x11wndproc_t )wndproc;

	if( x11display.dpy )
		GLimp_Shutdown();

	ri.Com_Printf( "Display initialization\n" );

	x11display.dpy = XOpenDisplay( NULL );
	if( !x11display.dpy )
	{
		ri.Com_Printf( "..Error couldn't open the X display\n" );
		return 0;
	}

	x11display.scr = DefaultScreen( x11display.dpy );
	if( parenthWnd )
		x11display.root = (Window )parenthWnd;
	else
		x11display.root = RootWindow( x11display.dpy, x11display.scr );

	x11display.wmState = XInternAtom( x11display.dpy, "WM_STATE", False );
	x11display.features.wmStateFullscreen = _NET_WM_STATE_FULLSCREEN_SUPPORTED();

#ifdef _XRANDR_OVER_VIDMODE_
	_xf86_XrandrInit();
#else
	_xf86_VidmodesInit();
#endif
	_xf86_XineramaInit();

	colorbits = 0;

	if( r_stencilbits->integer == 8 || r_stencilbits->integer == 16 ) stencilbits = r_stencilbits->integer;
	else stencilbits = 0;

	gotstencil = 0;
	if( colorbits > 0 )
	{
		ChooseVisual( colorbits, stencilbits );
		if( !x11display.visinfo && stencilbits > 8 ) ChooseVisual( colorbits, 8 );
		if( !x11display.visinfo && stencilbits > 0 ) ChooseVisual( colorbits, 0 );
		if( !x11display.visinfo && colorbits > 16 ) ChooseVisual( 16, 0 );
	}
	else
	{
		ChooseVisual( 32, stencilbits );
		if( !x11display.visinfo ) ChooseVisual( 16, stencilbits );
		if( !x11display.visinfo && stencilbits > 8 ) ChooseVisual( 32, 8 );
		if( !x11display.visinfo && stencilbits > 8 ) ChooseVisual( 16, 8 );
		if( !x11display.visinfo && stencilbits > 0 ) ChooseVisual( 32, 0 );
		if( !x11display.visinfo && stencilbits > 0 ) ChooseVisual( 16, 0 );
	}

	if( !x11display.visinfo )
	{
		GLimp_Shutdown(); // hope this doesn't do anything evil when we don't have window etc.
		ri.Com_Printf( "..Error couldn't set GLX visual\n" );
		return 0;
	}

	glConfig.stencilBits = gotstencil;

	x11display.ctx = qglXCreateContext( x11display.dpy, x11display.visinfo, NULL, True );
	x11display.cmap = XCreateColormap( x11display.dpy, x11display.root, x11display.visinfo->visual, AllocNone );

	attr.colormap = x11display.cmap;
	attr.border_pixel = 0;
	attr.event_mask = DISPLAY_MASK;
	attr.override_redirect = False;
	mask = CWBorderPixel | CWColormap | ExposureMask;

	x11display.gl_win = XCreateWindow( x11display.dpy, x11display.root, 0, 0, 1, 1, 0,
		x11display.visinfo->depth, InputOutput, x11display.visinfo->visual, mask, &attr );
	qglXMakeCurrent( x11display.dpy, x11display.gl_win, x11display.ctx );

	XSync( x11display.dpy, False );
	
	if( x11wndproc ) {
		x11wndproc( &x11display, 0, 0, 0 );
	}

	return 1;
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( void )
{
}

/*
** GLimp_EndFrame
**
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame( void )
{
	qglXSwapBuffers( x11display.dpy, x11display.gl_win );

	if( glConfig.fullScreen )
	{
		cvar_t *vid_multiscreen_head = ri.Cvar_Get( "vid_multiscreen_head", "0", CVAR_ARCHIVE );
		
		if( vid_multiscreen_head->modified ) {
			GLimp_SetMode_Real( glConfig.width, glConfig.height, _vid_display_refresh_rate, true, true, true );
			vid_multiscreen_head->modified = false;
		}
	}
}

/*
** GLimp_GetGammaRamp
*/
bool GLimp_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp )
{
	int size;
#ifndef X_XF86VidModeGetGammaRampSize
	if( XF86VidModeGetGammaRampSize( x11display.dpy, x11display.scr,
		&size ) == 0 )
		return false;
#else
	size = 256;
#endif
	if( size > stride )
		return false;
	*psize = size;
	if( XF86VidModeGetGammaRamp( x11display.dpy, x11display.scr,
		size, ramp, ramp + stride, ramp + ( stride << 1 ) ) != 0 )
		return true;
	return false;
}

/*
** GLimp_SetGammaRamp
*/
void GLimp_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp )
{
	XF86VidModeSetGammaRamp( x11display.dpy, x11display.scr,
		size, ramp, ramp + stride, ramp + ( stride << 1 ) );
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( bool active, bool destroy )
{
}

/*
** GLimp_SetWindow
*/
rserr_t GLimp_SetWindow( void *hinstance, void *wndproc, void *parenthWnd, bool *surfaceChangePending )
{
	if( surfaceChangePending )
		*surfaceChangePending = false;

	return rserr_ok; // surface cannot be lost
}

/*
** GLimp_RenderingEnabled
*/
bool GLimp_RenderingEnabled( void )
{
	return true;
}

/*
** GLimp_SetSwapInterval
*/
void GLimp_SetSwapInterval( int swapInterval )
{
	if( qglXSwapIntervalSGI )
		qglXSwapIntervalSGI( swapInterval );
}

/*
** GLimp_EnableMultithreadedRendering
*/
void GLimp_EnableMultithreadedRendering( bool enable )
{
}

/*
** GLimp_GetWindowSurface
*/
void *GLimp_GetWindowSurface( bool *renderable )
{
	if( renderable )
		*renderable = true;
	return ( void * )x11dispay.gl_win;
}

/*
** GLimp_UpdatePendingWindowSurface
*/
void GLimp_UpdatePendingWindowSurface( void )
{
}

/*
** GLimp_SharedContext_Create
*/
bool GLimp_SharedContext_Create( void **context, void **surface )
{
	GLXContext ctx = qglXCreateContext( x11display.dpy, x11display.visinfo, x11display.ctx, True );
	if( !ctx )
		return false;

	*context = (void *)ctx;
	if( surface )
		*surface = (void *)x11display.gl_win;
	return true;
}

/*
** GLimp_SharedContext_MakeCurrent
*/
bool GLimp_SharedContext_MakeCurrent( void *context, void *surface )
{
	return qglXMakeCurrent( x11display.dpy, (GLXDrawable)surface, (GLXContext)context ) == True ? true : false;
}

/*
** GLimp_SharedContext_Destroy
*/
void GLimp_SharedContext_Destroy( void *context, void *surface )
{
	qglXDestroyContext( x11display.dpy, context );
}
