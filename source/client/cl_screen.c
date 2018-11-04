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
// cl_screen.c -- master for refresh, status bar, console, chat, notify, etc

/*

full screen console
put up loading plaque
blanked background with loading plaque
blanked background with menu
cinematics
full screen image for quit and victory

end of unit intermissions

*/

#include "client.h"
#include "ftlib.h"

float scr_con_current;    // aproaches scr_conlines at scr_conspeed
float scr_con_previous;
float scr_conlines;       // 0.0 to 1.0 lines of console to display

static bool scr_initialized;    // ready to draw

static int scr_draw_loading;

static cvar_t *scr_consize;
static cvar_t *scr_conspeed;
static cvar_t *scr_netgraph;
static cvar_t *scr_timegraph;
static cvar_t *scr_debuggraph;
static cvar_t *scr_graphheight;
static cvar_t *scr_graphscale;
static cvar_t *scr_graphshift;

static cvar_t *con_fontSystemFamily;
static cvar_t *con_fontSystemFallbackFamily;
static cvar_t *con_fontSystemMonoFamily;
static cvar_t *con_fontSystemConsoleSize;

//
//	Variable width (proportional) fonts
//

//===============================================================================
//FONT LOADING
//===============================================================================

/*
* SCR_RegisterFont
*/
qfontface_t *SCR_RegisterFont( const char *family, int style, unsigned int size ) {
	return FTLIB_RegisterFont( family, con_fontSystemFallbackFamily->string, style, size );
}

/*
* SCR_RegisterSpecialFont
*/
qfontface_t *SCR_RegisterSpecialFont( const char *family, int style, unsigned int size ) {
	return FTLIB_RegisterFont( family, NULL, style, size );
}

/*
* SCR_RegisterConsoleFont
*/
static void SCR_RegisterConsoleFont( void ) {
	const char *con_fontSystemFamilyName;
	const int con_fontSystemStyle = DEFAULT_SYSTEM_FONT_STYLE;
	int size;
	float pixelRatio = Con_GetPixelRatio();

	// register system fonts
	con_fontSystemFamilyName = con_fontSystemMonoFamily->string;
	if( !con_fontSystemConsoleSize->integer ) {
		Cvar_SetValue( con_fontSystemConsoleSize->name, DEFAULT_SYSTEM_FONT_SMALL_SIZE );
	} else if( con_fontSystemConsoleSize->integer > DEFAULT_SYSTEM_FONT_SMALL_SIZE * 2 ) {
		Cvar_SetValue( con_fontSystemConsoleSize->name, DEFAULT_SYSTEM_FONT_SMALL_SIZE * 2 );
	} else if( con_fontSystemConsoleSize->integer < DEFAULT_SYSTEM_FONT_SMALL_SIZE / 2 ) {
		Cvar_SetValue( con_fontSystemConsoleSize->name, DEFAULT_SYSTEM_FONT_SMALL_SIZE / 2 );
	}

	size = ceil( con_fontSystemConsoleSize->integer * pixelRatio );
	cls.consoleFont = SCR_RegisterFont( con_fontSystemFamilyName, con_fontSystemStyle, size );
	if( !cls.consoleFont ) {
		Cvar_ForceSet( con_fontSystemMonoFamily->name, con_fontSystemMonoFamily->dvalue );
		con_fontSystemFamilyName = con_fontSystemMonoFamily->dvalue;

		size = DEFAULT_SYSTEM_FONT_SMALL_SIZE;
		cls.consoleFont = SCR_RegisterFont( con_fontSystemFamilyName, con_fontSystemStyle, size );
		if( !cls.consoleFont ) {
			Com_Error( ERR_FATAL, "Couldn't load default font \"%s\"", con_fontSystemMonoFamily->dvalue );
		}

		Con_CheckResize();
	}
}


/*
* SCR_InitFonts
*/
static void SCR_InitFonts( void ) {
	con_fontSystemFamily = Cvar_Get( "con_fontSystemFamily", DEFAULT_SYSTEM_FONT_FAMILY, CVAR_ARCHIVE );
	con_fontSystemMonoFamily = Cvar_Get( "con_fontSystemMonoFamily", DEFAULT_SYSTEM_FONT_FAMILY_MONO, CVAR_ARCHIVE );
	con_fontSystemFallbackFamily = Cvar_Get( "con_fontSystemFallbackFamily", DEFAULT_SYSTEM_FONT_FAMILY_FALLBACK, CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	con_fontSystemConsoleSize = Cvar_Get( "con_fontSystemConsoleSize", STR_TOSTR( DEFAULT_SYSTEM_FONT_SMALL_SIZE ), CVAR_ARCHIVE );

	SCR_RegisterConsoleFont();
}

/*
* SCR_ShutdownFonts
*/
static void SCR_ShutdownFonts( void ) {
	cls.consoleFont = NULL;

	con_fontSystemFamily = NULL;
	con_fontSystemConsoleSize = NULL;
}

/*
* SCR_CheckSystemFontsModified
*
* Reloads system fonts on demand
*/
static void SCR_CheckSystemFontsModified( void ) {
	if( !con_fontSystemMonoFamily ) {
		return;
	}

	if( con_fontSystemMonoFamily->modified
		|| con_fontSystemConsoleSize->modified
		) {
		SCR_RegisterConsoleFont();
		con_fontSystemMonoFamily->modified = false;
		con_fontSystemConsoleSize->modified = false;
	}
}

/*
* SCR_ResetSystemFontConsoleSize
*/
void SCR_ResetSystemFontConsoleSize( void ) {
	if( !con_fontSystemConsoleSize ) {
		return;
	}
	Cvar_ForceSet( con_fontSystemConsoleSize->name, con_fontSystemConsoleSize->dvalue );
	SCR_CheckSystemFontsModified();
}

/*
* SCR_ChangeSystemFontConsoleSize
*/
void SCR_ChangeSystemFontConsoleSize( int ch ) {
	if( !con_fontSystemConsoleSize ) {
		return;
	}
	Cvar_ForceSet( con_fontSystemConsoleSize->name, va( "%i", con_fontSystemConsoleSize->integer + ch ) );
	SCR_CheckSystemFontsModified();
}


//===============================================================================
//STRINGS HELPERS
//===============================================================================


static int SCR_HorizontalAlignForString( const int x, int align, int width ) {
	int nx = x;

	if( align % 3 == 0 ) { // left
		nx = x;
	}
	if( align % 3 == 1 ) { // center
		nx = x - width / 2;
	}
	if( align % 3 == 2 ) { // right
		nx = x - width;
	}

	return nx;
}

static int SCR_VerticalAlignForString( const int y, int align, int height ) {
	int ny = y;

	if( align / 3 == 0 ) { // top
		ny = y;
	} else if( align / 3 == 1 ) { // middle
		ny = y - height / 2;
	} else if( align / 3 == 2 ) { // bottom
		ny = y - height;
	}

	return ny;
}

size_t SCR_FontSize( qfontface_t *font ) {
	return FTLIB_FontSize( font );
}

size_t SCR_FontHeight( qfontface_t *font ) {
	return FTLIB_FontHeight( font );
}

size_t SCR_strWidth( const char *str, qfontface_t *font, size_t maxlen, int flags ) {
	return FTLIB_StringWidth( str, font, maxlen, flags );
}

size_t SCR_StrlenForWidth( const char *str, qfontface_t *font, size_t maxwidth, int flags ) {
	return FTLIB_StrlenForWidth( str, font, maxwidth, flags );
}

int SCR_FontUnderline( qfontface_t *font, int *thickness ) {
	return FTLIB_FontUnderline( font, thickness );
}

size_t SCR_FontAdvance( qfontface_t *font ) {
	return FTLIB_FontAdvance( font );
}

size_t SCR_FontXHeight( qfontface_t *font ) {
	return FTLIB_FontXHeight( font );
}

fdrawchar_t SCR_SetDrawCharIntercept( fdrawchar_t intercept ) {
	return FTLIB_SetDrawCharIntercept( intercept );
}

//===============================================================================
//STRINGS DRAWING
//===============================================================================

void SCR_DrawRawChar( int x, int y, wchar_t num, qfontface_t *font, vec4_t color ) {
	FTLIB_DrawRawChar( x, y, num, font, color );
}

void SCR_DrawClampChar( int x, int y, wchar_t num, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color ) {
	FTLIB_DrawClampChar( x, y, num, xmin, ymin, xmax, ymax, font, color );
}

void SCR_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color, int flags ) {
	FTLIB_DrawClampString( x, y, str, xmin, ymin, xmax, ymax, font, color, flags );
}

int SCR_DrawMultilineString( int x, int y, const char *str, int halign, int maxwidth, int maxlines, qfontface_t *font, vec4_t color, int flags ) {
	return FTLIB_DrawMultilineString( x, y, str, halign, maxwidth, maxlines, font, color, flags );
}

/*
* SCR_DrawString
*/
int SCR_DrawString( int x, int y, int align, const char *str, qfontface_t *font, vec4_t color, int flags ) {
	int width;
	int fontHeight;

	if( !str ) {
		return 0;
	}

	if( !font ) {
		font = cls.consoleFont;
	}
	fontHeight = FTLIB_FontHeight( font );

	if( ( align % 3 ) != 0 ) { // not left - don't precalculate the width if not needed
		x = SCR_HorizontalAlignForString( x, align, FTLIB_StringWidth( str, font, 0, flags ) );
	}
	y = SCR_VerticalAlignForString( y, align, fontHeight );

	FTLIB_DrawRawString( x, y, str, 0, &width, font, color, flags );

	return width;
}

/*
* SCR_DrawStringWidth
*
* ClampS to width in pixels. Returns drawn len
*/
size_t SCR_DrawStringWidth( int x, int y, int align, const char *str, size_t maxwidth, qfontface_t *font, vec4_t color, int flags ) {
	size_t width;
	int fontHeight;

	if( !str ) {
		return 0;
	}

	if( !font ) {
		font = cls.consoleFont;
	}
	fontHeight = FTLIB_FontHeight( font );

	width = FTLIB_StringWidth( str, font, 0, flags );
	if( width ) {
		if( maxwidth && width > maxwidth ) {
			width = maxwidth;
		}

		x = SCR_HorizontalAlignForString( x, align, width );
		y = SCR_VerticalAlignForString( y, align, fontHeight );

		return FTLIB_DrawRawString( x, y, str, maxwidth, NULL, font, color, flags );
	}

	return 0;
}

//===============================================================================

/*
* SCR_RegisterPic
*/
struct shader_s *SCR_RegisterPic( const char *name ) {
	return re.RegisterPic( name );
}

/*
* SCR_DrawStretchPic
*/
void SCR_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const float *color, const struct shader_s *shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color, shader );
}

/*
* SCR_DrawFillRect
*
* Fills a box of pixels with a single color
*/
void SCR_DrawFillRect( int x, int y, int w, int h, vec4_t color ) {
	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, cls.whiteShader );
}

/*
* SCR_DrawClampFillRect
*
* Fills a scissored box of pixels with a single color
*/
void SCR_DrawClampFillRect( int x, int y, int w, int h, int xmin, int ymin, int xmax, int ymax, vec4_t color ) {
	int x2 = x + w;
	int y2 = y + h;

	if( ( xmax <= xmin ) || ( ymax <= ymin ) ) {
		return;
	}

	clamp_low( x, xmin );
	clamp_low( y, ymin );
	clamp_high( x2, xmax );
	clamp_high( y2, ymax );

	w = x2 - x;
	h = y2 - y;
	if( ( w <= 0 ) || ( h <= 0 ) ) {
		return;
	}

	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, cls.whiteShader );
}

/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
* CL_AddNetgraph
*
* A new packet was just parsed
*/
void CL_AddNetgraph( void ) {
	int i;
	int ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if( scr_timegraph->integer ) {
		return;
	}

	for( i = 0; i < cls.netchan.dropped; i++ )
		SCR_DebugGraph( 30.0f, 0.655f, 0.231f, 0.169f );

	for( i = 0; i < cl.suppressCount; i++ )
		SCR_DebugGraph( 30.0f, 0.0f, 1.0f, 0.0f );

	// see what the latency was on this packet
	ping = cls.realtime - cl.cmd_time[cls.ucmdAcknowledged & CMD_MASK];
	ping /= 30;
	if( ping > 30 ) {
		ping = 30;
	}
	SCR_DebugGraph( ping, 1.0f, 0.75f, 0.06f );
}


typedef struct {
	float value;
	vec4_t color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

/*
* SCR_DebugGraph
*/
void SCR_DebugGraph( float value, float r, float g, float b ) {
	values[current].value = value;
	values[current].color[0] = r;
	values[current].color[1] = g;
	values[current].color[2] = b;
	values[current].color[3] = 1.0f;

	current++;
	current &= 1023;
}

/*
* SCR_DrawDebugGraph
*/
static void SCR_DrawDebugGraph( void ) {
	int a, x, y, w, i, h, s;
	float v;

	//
	// draw the graph
	//
	w = viddef.width;
	x = 0;
	y = 0 + viddef.height;
	SCR_DrawFillRect( x, y - scr_graphheight->integer,
					  w, scr_graphheight->integer, colorBlack );

	s = ( w + 1024 - 1 ) / 1024; //scale for resolutions with width >1024

	for( a = 0; a < w; a++ ) {
		i = ( current - 1 - a + 1024 ) & 1023;
		v = values[i].value;
		v = v * scr_graphscale->integer + scr_graphshift->integer;

		if( v < 0 ) {
			v += scr_graphheight->integer * ( 1 + (int)( -v / scr_graphheight->integer ) );
		}
		h = (int)v % scr_graphheight->integer;
		SCR_DrawFillRect( x + w - 1 - a * s, y - h, s, h, values[i].color );
	}
}

//============================================================================

/*
* SCR_InitScreen
*/
void SCR_InitScreen( void ) {
	scr_consize = Cvar_Get( "scr_consize", "0.4", CVAR_ARCHIVE );
	scr_conspeed = Cvar_Get( "scr_conspeed", "3", CVAR_ARCHIVE );
	scr_netgraph = Cvar_Get( "netgraph", "0", 0 );
	scr_timegraph = Cvar_Get( "timegraph", "0", 0 );
	scr_debuggraph = Cvar_Get( "debuggraph", "0", 0 );
	scr_graphheight = Cvar_Get( "graphheight", "32", 0 );
	scr_graphscale = Cvar_Get( "graphscale", "1", 0 );
	scr_graphshift = Cvar_Get( "graphshift", "0", 0 );

	scr_initialized = true;
}

/*
* SCR_GetScreenWidth
*/
unsigned int SCR_GetScreenWidth( void ) {
	return VID_GetWindowWidth();
}

/*
* SCR_GetScreenHeight
*/
unsigned int SCR_GetScreenHeight( void ) {
	return VID_GetWindowHeight();
}

/*
* SCR_ShutdownScreen
*/
void SCR_ShutdownScreen( void ) {
	scr_initialized = false;
}

/*
* SCR_EnableOverlayMenu
*/
void SCR_EnableOverlayMenu( bool enable, bool showCursor ) {
	cls.overlayMenu = enable;
	cls.overlayMenuShowCursor = showCursor;
	CL_UIModule_ShowOverlayMenu( enable, showCursor );
}

/*
* SCR_IsOverlayMenuShown
*/
bool SCR_IsOverlayMenuShown( void ) {
	return cls.overlayMenu && CL_UIModule_HaveOverlayMenu();
}

/*
* SCR_IsOverlayMenuHover
*/
bool SCR_IsOverlayMenuHover( void ) {
	return SCR_IsOverlayMenuShown() && cls.overlayMenuShowCursor && CL_UIModule_MouseHover( false );
}

/*
* SCR_DrawConsole
*/
void SCR_DrawChat( int x, int y, int width, struct qfontface_s *font ) {
	Con_DrawChat( x, y, width, font );
}

//=============================================================================

/*
* SCR_RunConsole
*
* Scroll it up or down
*/
void SCR_RunConsole( int msec ) {
	// decide on the height of the console
	if( cls.key_dest == key_console ) {
		scr_conlines = bound( 0.1f, scr_consize->value, 1.0f );
	} else {
		scr_conlines = 0;
	}

	scr_con_previous = scr_con_current;
	if( scr_conlines < scr_con_current ) {
		scr_con_current -= scr_conspeed->value * msec * 0.001f;
		if( scr_conlines > scr_con_current ) {
			scr_con_current = scr_conlines;
		}

	} else if( scr_conlines > scr_con_current ) {
		scr_con_current += scr_conspeed->value * msec * 0.001f;
		if( scr_conlines < scr_con_current ) {
			scr_con_current = scr_conlines;
		}
	}
}

/*
* SCR_DrawConsole
*/
static void SCR_DrawConsole( void ) {
	if( scr_con_current ) {
		Con_DrawConsole();
		return;
	}
}

/*
* SCR_DrawNotify
*/
static void SCR_DrawNotify( void ) {
	Con_DrawNotify();
}

/*
* SCR_BeginLoadingPlaque
*/
void SCR_BeginLoadingPlaque( void ) {
	CL_UIModule_ForceMenuOff();

	CL_SoundModule_StopAllSounds( true, true );

	memset( cl.configstrings, 0, sizeof( cl.configstrings ) );

	scr_conlines = 0;       // none visible
	scr_draw_loading = 2;   // clear to black first

	SCR_UpdateScreen();
}

/*
* SCR_EndLoadingPlaque
*/
void SCR_EndLoadingPlaque( void ) {
	cls.disable_screen = 0;
	Con_ClearNotify();
}


//=======================================================

/*
* SCR_RegisterConsoleMedia
*/
void SCR_RegisterConsoleMedia() {
	cls.whiteShader = re.RegisterPic( "$whiteimage" );
	cls.consoleShader = re.RegisterPic( "gfx/ui/console" );

	SCR_InitFonts();
}

/*
* SCR_ShutDownConsoleMedia
*/
void SCR_ShutDownConsoleMedia( void ) {
	SCR_ShutdownFonts();
}

//============================================================================

/*
* SCR_RenderView
*/
static void SCR_RenderView( bool timedemo ) {
	if( timedemo ) {
		if( !cl.timedemo.startTime ) {
			cl.timedemo.startTime = Sys_Milliseconds();
		}
		cl.timedemo.frames++;
	}

	// frame is not valid until we load the CM data
	if( cl.cms != NULL ) {
		CL_GameModule_RenderView();
	}
}

//============================================================================

/*
* SCR_UpdateScreen
*
* This is called every frame, and can also be called explicitly to flush
* text to the screen.
*/
void SCR_UpdateScreen( void ) {
	bool cinematic;
	bool forceclear;
	bool timedemo;

	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if( cls.disable_screen ) {
		if( Sys_Milliseconds() - cls.disable_screen > 120000 ) {
			cls.disable_screen = 0;
			Com_Printf( "Loading plaque timed out.\n" );
		}
		return;
	}

	if( !scr_initialized || !con_initialized || !cls.mediaInitialized ) {
		return;     // not ready yet

	}
	Con_CheckResize();

	SCR_CheckSystemFontsModified();

	cinematic = cls.state == CA_CINEMATIC ? true : false;
	CL_ForceVsync( cinematic || ( cls.state == CA_DISCONNECTED && scr_con_current ) );
	forceclear = cinematic;
	timedemo = cl_timedemo->integer != 0 && cls.demo.playing;

	re.BeginFrame( forceclear, timedemo );

	if( scr_draw_loading == 2 ) {
		// loading plaque over APP_STARTUP_COLOR screen
		scr_draw_loading = 0;
		CL_UIModule_UpdateConnectScreen( true );
	}
	// if a cinematic is supposed to be running, handle menus
	// and console specially
	else if( cinematic ) {
		SCR_DrawCinematic();
		SCR_DrawConsole();
	} else if( cls.state == CA_DISCONNECTED ) {
		CL_UIModule_Refresh( true, true );
		SCR_DrawConsole();
	} else if( cls.state == CA_GETTING_TICKET || cls.state == CA_CONNECTING || cls.state == CA_HANDSHAKE ) {
		CL_UIModule_UpdateConnectScreen( true );
	} else if( cls.state == CA_CONNECTED ) {
		if( cls.cgameActive ) {
			CL_UIModule_UpdateConnectScreen( false );
			SCR_RenderView( timedemo );
		} else {
			CL_UIModule_UpdateConnectScreen( true );
		}
	} else if( cls.state == CA_ACTIVE ) {
		SCR_RenderView( timedemo );

		CL_UIModule_Refresh( false, true );

		if( scr_timegraph->integer ) {
			SCR_DebugGraph( cls.frametime * 0.3f, 1, 1, 1 );
		}

		if( scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer ) {
			SCR_DrawDebugGraph();
		}

		SCR_DrawConsole();
		SCR_DrawNotify();
	}

	re.EndFrame();
}
