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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

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

qboolean scr_initialized;    // ready to draw

int scr_draw_loading;

static cvar_t *scr_consize;
static cvar_t *scr_conspeed;
static cvar_t *scr_netgraph;
static cvar_t *scr_timegraph;
static cvar_t *scr_debuggraph;
static cvar_t *scr_graphheight;
static cvar_t *scr_graphscale;
static cvar_t *scr_graphshift;
static cvar_t *scr_forceclear;

static cvar_t *con_fontSystemFamily;
static cvar_t *con_fontSystemSmallSize;
static cvar_t *con_fontSystemMediumSize;
static cvar_t *con_fontSystemBigSize;

/*
===============================================================================

MUFONT STRINGS

===============================================================================
*/


//
//	Variable width (proportional) fonts
//

//===============================================================================
//FONT LOADING
//===============================================================================

/*
* SCR_RegisterFont
*/
qfontface_t *SCR_RegisterFont( const char *family, qfontstyle_t style, unsigned int size )
{
	return FTLIB_RegisterFont( family, (qfontstyle_t)style, size );
}

/*
* SCR_RegisterSystemFonts
*/
static void SCR_RegisterSystemFonts( void )
{
	const char *con_fontSystemFamilyName;
	const qfontstyle_t con_fontSystemStyle = (qfontstyle_t)DEFAULT_SYSTEM_FONT_STYLE;

	// register system fonts
	con_fontSystemFamilyName = con_fontSystemFamily->string;
	if( !con_fontSystemSmallSize->integer ) {
		Cvar_ForceSet( con_fontSystemSmallSize->name, con_fontSystemSmallSize->dvalue );
	}
	if( !con_fontSystemMediumSize->integer ) {
		Cvar_ForceSet( con_fontSystemMediumSize->name, con_fontSystemMediumSize->dvalue );
	}
	if( !con_fontSystemBigSize->integer ) {
		Cvar_ForceSet( con_fontSystemBigSize->name, con_fontSystemBigSize->dvalue );
	}

	cls.fontSystemSmall = FTLIB_RegisterFont( con_fontSystemFamilyName, con_fontSystemStyle, con_fontSystemSmallSize->integer );
	if( !cls.fontSystemSmall )
	{
		Cvar_ForceSet( con_fontSystemFamily->name, con_fontSystemFamily->dvalue );
		con_fontSystemFamilyName = con_fontSystemFamily->dvalue;

		cls.fontSystemSmall = FTLIB_RegisterFont( con_fontSystemFamilyName, con_fontSystemStyle, DEFAULT_SYSTEM_FONT_SMALL_SIZE );
		if( !cls.fontSystemSmall )
			Com_Error( ERR_FATAL, "Couldn't load default font \"%s\"", con_fontSystemFamily->dvalue );
	}

	cls.fontSystemMedium = FTLIB_RegisterFont( con_fontSystemFamilyName, con_fontSystemStyle, con_fontSystemMediumSize->integer );
	if( !cls.fontSystemMedium )
		cls.fontSystemMedium = FTLIB_RegisterFont( con_fontSystemFamilyName, con_fontSystemStyle, DEFAULT_SYSTEM_FONT_MEDIUM_SIZE );

	cls.fontSystemBig = FTLIB_RegisterFont( con_fontSystemFamily->string, con_fontSystemStyle, con_fontSystemBigSize->integer );
	if( !cls.fontSystemBig )
		cls.fontSystemBig = FTLIB_RegisterFont( con_fontSystemFamilyName, con_fontSystemStyle, DEFAULT_SYSTEM_FONT_BIG_SIZE );
}

/*
* SCR_InitFonts
*/
static void SCR_InitFonts( qboolean verbose )
{
	const qfontstyle_t con_fontSystemStyle = (qfontstyle_t)DEFAULT_SYSTEM_FONT_STYLE;

	FTLIB_PrecacheFonts( verbose );
	
	// always register default fonts
	FTLIB_RegisterFont( DEFAULT_SYSTEM_FONT_FAMILY, con_fontSystemStyle, DEFAULT_SYSTEM_FONT_SMALL_SIZE );
	FTLIB_RegisterFont( DEFAULT_SYSTEM_FONT_FAMILY, con_fontSystemStyle, DEFAULT_SYSTEM_FONT_MEDIUM_SIZE );
	FTLIB_RegisterFont( DEFAULT_SYSTEM_FONT_FAMILY, con_fontSystemStyle, DEFAULT_SYSTEM_FONT_BIG_SIZE );

	con_fontSystemFamily = Cvar_Get( "con_fontSystemFamily", DEFAULT_SYSTEM_FONT_FAMILY, CVAR_ARCHIVE );
	con_fontSystemSmallSize = Cvar_Get( "con_fontSystemSmallSize", STR_TOSTR( DEFAULT_SYSTEM_FONT_SMALL_SIZE ), CVAR_ARCHIVE );
	con_fontSystemMediumSize = Cvar_Get( "con_fontSystemMediumSize", STR_TOSTR( DEFAULT_SYSTEM_FONT_MEDIUM_SIZE ), CVAR_ARCHIVE );
	con_fontSystemBigSize = Cvar_Get( "con_fontSystemBigSize", STR_TOSTR( DEFAULT_SYSTEM_FONT_BIG_SIZE ), CVAR_ARCHIVE );

	SCR_RegisterSystemFonts();
}

/*
* SCR_ShutdownFonts
*/
static void SCR_ShutdownFonts( qboolean verbose )
{
	FTLIB_FreeFonts( verbose );

	cls.fontSystemSmall = NULL;
	cls.fontSystemMedium = NULL;
	cls.fontSystemBig = NULL;

	con_fontSystemFamily = NULL;
	con_fontSystemSmallSize = con_fontSystemMediumSize = con_fontSystemBigSize = NULL;
}

/*
* SCR_CheckSystemFontsModified
*
* Reloads system fonts on demand
*/
static void SCR_CheckSystemFontsModified( void )
{
	if( !con_fontSystemFamily ) {
		return;
	}
	if( con_fontSystemFamily->modified 
		|| con_fontSystemSmallSize->modified 
		|| con_fontSystemMediumSize->modified 
		|| con_fontSystemBigSize->modified 
		) {
		SCR_RegisterSystemFonts();
		con_fontSystemFamily->modified = qfalse;
		con_fontSystemSmallSize->modified = qfalse;
		con_fontSystemMediumSize->modified = qfalse;
		con_fontSystemBigSize->modified = qfalse;
	}
}

/*
* SCR_ChangeSystemFontSmallSize
*/
void SCR_ChangeSystemFontSmallSize( int ch )
{
	if( !con_fontSystemSmallSize ) {
		return;
	}
	Cvar_ForceSet( con_fontSystemSmallSize->name, va( "%i", con_fontSystemSmallSize->integer + ch ) );
	SCR_CheckSystemFontsModified();
}


//===============================================================================
//STRINGS HELPERS
//===============================================================================


static int SCR_HorizontalAlignForString( const int x, int align, int width )
{
	int nx = x;

	if( align % 3 == 0 )  // left
		nx = x;
	if( align % 3 == 1 )  // center
		nx = x - width / 2;
	if( align % 3 == 2 )  // right
		nx = x - width;

	return nx;
}

static int SCR_VerticalAlignForString( const int y, int align, int height )
{
	int ny = y;

	if( align / 3 == 0 )  // top
		ny = y;
	else if( align / 3 == 1 )  // middle
		ny = y - height / 2;
	else if( align / 3 == 2 )  // bottom
		ny = y - height;

	return ny;
}

size_t SCR_strHeight( qfontface_t *font )
{
	return FTLIB_FontHeight( font );
}

size_t SCR_strWidth( const char *str, qfontface_t *font, size_t maxlen )
{
	return FTLIB_StringWidth( str, font, maxlen );
}

size_t SCR_StrlenForWidth( const char *str, qfontface_t *font, size_t maxwidth )
{
	return FTLIB_StrlenForWidth( str, font, maxwidth );
}

//===============================================================================
//STRINGS DRAWING
//===============================================================================

void SCR_DrawRawChar( int x, int y, qwchar num, qfontface_t *font, vec4_t color )
{
	FTLIB_DrawRawChar( x, y, num, font, color );
}

void SCR_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color )
{
	FTLIB_DrawClampString( x, y, str, xmin, ymin, xmax, ymax, font, color );
}

/*
* SCR_DrawString
*/
void SCR_DrawString( int x, int y, int align, const char *str, qfontface_t *font, vec4_t color )
{
	size_t width;
	int fontHeight;

	if( !str )
		return;

	if( !font )
		font = cls.fontSystemSmall;
	fontHeight = FTLIB_FontHeight( font );

	width = FTLIB_StringWidth( str, font, 0 );
	if( width )
	{
		x = SCR_HorizontalAlignForString( x, align, width );
		y = SCR_VerticalAlignForString( y, align, fontHeight );

		if( y <= -fontHeight || y >= (int)viddef.height )
			return; // totally off screen

		if( x + width <= 0 || x >= (int)viddef.width )
			return; // totally off screen

		FTLIB_DrawRawString( x, y, str, 0, font, color );
	}
}

/*
* SCR_DrawStringWidth - clamp to width in pixels. Returns drawn len
*/
size_t SCR_DrawStringWidth( int x, int y, int align, const char *str, size_t maxwidth, qfontface_t *font, vec4_t color )
{
	size_t width;
	int fontHeight;

	if( !str )
		return 0;

	if( !font )
		font = cls.fontSystemSmall;
	fontHeight = FTLIB_FontHeight( font );

	if( maxwidth < 0 )
		maxwidth = 0;

	width = FTLIB_StringWidth( str, font, 0 );
	if( width )
	{
		if( maxwidth && width > maxwidth )
			width = maxwidth;

		x = SCR_HorizontalAlignForString( x, align, width );
		y = SCR_VerticalAlignForString( y, align, fontHeight );

		return FTLIB_DrawRawString( x, y, str, maxwidth, font, color );
	}

	return 0;
}

/*
* SCR_DrawFillRect
* 
* Fills a box of pixels with a single color
*/
void SCR_DrawFillRect( int x, int y, int w, int h, vec4_t color )
{
	R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, cls.whiteShader );
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
void CL_AddNetgraph( void )
{
	int i;
	int ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if( scr_timegraph->integer )
		return;

	for( i = 0; i < cls.netchan.dropped; i++ )
		SCR_DebugGraph( 30.0f, 0.655f, 0.231f, 0.169f );

	for( i = 0; i < cl.suppressCount; i++ )
		SCR_DebugGraph( 30.0f, 0.0f, 1.0f, 0.0f );

	// see what the latency was on this packet
	ping = cls.realtime - cl.cmd_time[cls.ucmdAcknowledged & CMD_MASK];
	ping /= 30;
	if( ping > 30 )
		ping = 30;
	SCR_DebugGraph( ping, 1.0f, 0.75f, 0.06f );
}


typedef struct
{
	float value;
	vec4_t color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

/*
* SCR_DebugGraph
*/
void SCR_DebugGraph( float value, float r, float g, float b )
{
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
static void SCR_DrawDebugGraph( void )
{
	int a, x, y, w, i, h;
	float v;

	//
	// draw the graph
	//
	w = viddef.width;
	x = 0;
	y = 0+viddef.height;
	SCR_DrawFillRect( x, y-scr_graphheight->integer,
		w, scr_graphheight->integer, colorBlack );

	for( a = 0; a < w; a++ )
	{
		i = ( current-1-a+1024 ) & 1023;
		v = values[i].value;
		v = v*scr_graphscale->integer + scr_graphshift->integer;

		if( v < 0 )
			v += scr_graphheight->integer * ( 1+(int)( -v/scr_graphheight->integer ) );
		h = (int)v % scr_graphheight->integer;
		SCR_DrawFillRect( x+w-1-a, y - h, 1, h, values[i].color );
	}
}

//============================================================================

/*
* SCR_InitScreen
*/
void SCR_InitScreen( void )
{
	scr_consize = Cvar_Get( "scr_consize", "0.5", CVAR_ARCHIVE );
	scr_conspeed = Cvar_Get( "scr_conspeed", "3", CVAR_ARCHIVE );
	scr_netgraph = Cvar_Get( "netgraph", "0", 0 );
	scr_timegraph = Cvar_Get( "timegraph", "0", 0 );
	scr_debuggraph = Cvar_Get( "debuggraph", "0", 0 );
	scr_graphheight = Cvar_Get( "graphheight", "32", 0 );
	scr_graphscale = Cvar_Get( "graphscale", "1", 0 );
	scr_graphshift = Cvar_Get( "graphshift", "0", 0 );
	scr_forceclear = Cvar_Get( "scr_forceclear", "0", CVAR_READONLY );

	FTLIB_LoadLibrary( qtrue );

	scr_initialized = qtrue;
}

//=============================================================================

/*
* SCR_RunConsole
* 
* Scroll it up or down
*/
void SCR_RunConsole( int msec )
{
	// decide on the height of the console
	if( cls.key_dest == key_console )
		scr_conlines = bound( 0.1f, scr_consize->value, 1.0f );
	else
		scr_conlines = 0;

	scr_con_previous = scr_con_current;
	if( scr_conlines < scr_con_current )
	{
		scr_con_current -= scr_conspeed->value * msec * 0.001f;
		if( scr_conlines > scr_con_current )
			scr_con_current = scr_conlines;

	}
	else if( scr_conlines > scr_con_current )
	{
		scr_con_current += scr_conspeed->value * msec * 0.001f;
		if( scr_conlines < scr_con_current )
			scr_con_current = scr_conlines;
	}
}

/*
* SCR_DrawConsole
*/
static void SCR_DrawConsole( void )
{
	if( scr_con_current )
	{
		Con_DrawConsole( scr_con_current );
		return;
	}

	if( cls.state == CA_ACTIVE && ( cls.key_dest == key_game || cls.key_dest == key_message ) )
	{
		Con_DrawNotify(); // only draw notify in game
	}
}

/*
* SCR_BeginLoadingPlaque
*/
void SCR_BeginLoadingPlaque( void )
{
	CL_SoundModule_StopAllSounds();

	memset( cl.configstrings, 0, sizeof( cl.configstrings ) );

	scr_conlines = 0;       // none visible
	scr_draw_loading = 2;   // clear to black first
	SCR_UpdateScreen();

	CL_ShutdownMedia( qfalse );
}

/*
* SCR_EndLoadingPlaque
*/
void SCR_EndLoadingPlaque( void )
{
	cls.disable_screen = 0;
	Con_ClearNotify();
	CL_InitMedia( qfalse );
}


//=======================================================

/*
* SCR_RegisterConsoleMedia
*/
void SCR_RegisterConsoleMedia( qboolean verbose )
{
	cls.whiteShader = R_RegisterPic( "$whiteimage" );
	cls.consoleShader = R_RegisterPic( "gfx/ui/console" );

	SCR_InitFonts( verbose );
}

/*
* SCR_ShutDownConsoleMedia
*/
void SCR_ShutDownConsoleMedia( qboolean verbose )
{
	SCR_ShutdownFonts( verbose );
}

//============================================================================

/*
* SCR_RenderView
*/
static void SCR_RenderView( float stereo_separation )
{
	if( cls.demo.playing )
	{
		if( cl_timedemo->integer )
		{
			if( !cl.timedemo.start )
				cl.timedemo.start = Sys_Milliseconds();
			cl.timedemo.frames++;
		}
	}

	// frame is not valid until we load the CM data
	if( CM_ClientLoad( cl.cms ) )
		CL_GameModule_RenderView( stereo_separation );
}

//============================================================================

/*
* SCR_UpdateScreen
* 
* This is called every frame, and can also be called explicitly to flush
* text to the screen.
*/
void SCR_UpdateScreen( void )
{
	static dynvar_t *updatescreen = NULL;
	int numframes;
	int i;
	float separation[2];
	qboolean scr_cinematic;

	if( !updatescreen )
		updatescreen = Dynvar_Create( "updatescreen", qfalse, DYNVAR_WRITEONLY, DYNVAR_READONLY );

	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if( cls.disable_screen )
	{
		if( Sys_Milliseconds() - cls.disable_screen > 120000 )
		{
			cls.disable_screen = 0;
			Com_Printf( "Loading plaque timed out.\n" );
		}
		return;
	}

	if( !scr_initialized || !con_initialized || !cls.mediaInitialized )
		return;     // not initialized yet

	Con_CheckResize();

	SCR_CheckSystemFontsModified();

	/*
	** range check cl_camera_separation so we don't inadvertently fry someone's
	** brain
	*/
	if( cl_stereo_separation->value > 1.0 )
		Cvar_SetValue( "cl_stereo_separation", 1.0 );
	else if( cl_stereo_separation->value < 0 )
		Cvar_SetValue( "cl_stereo_separation", 0.0 );

	if( cl_stereo->integer )
	{
		numframes = 2;
		separation[0] = -cl_stereo_separation->value / 2;
		separation[1] =  cl_stereo_separation->value / 2;
	}
	else
	{
		separation[0] = 0;
		separation[1] = 0;
		numframes = 1;
	}

	// avoid redrawing fullscreen cinematics unless damaged by console drawing
	scr_cinematic = cls.state == CA_CINEMATIC ? qtrue : qfalse;
//	if( scr_cinematic && !cl.cin.redraw && !scr_con_current && !scr_con_previous ) {
//		return;
//	}

	if( cls.cgameActive && cls.state < CA_LOADING ) {
		// this is when we've finished loading cgame media and are waiting
		// for the first valid snapshot to arrive. keep the loading screen untouched
		return;
	}

	for( i = 0; i < numframes; i++ )
	{
		R_BeginFrame( separation[i], scr_cinematic || scr_forceclear->integer ? qtrue : qfalse );

		if( scr_draw_loading == 2 )
		{ 
			// loading plaque over black screen
			scr_draw_loading = 0;
			CL_UIModule_UpdateConnectScreen( qtrue );
		}
		// if a cinematic is supposed to be running, handle menus
		// and console specially
		else if( scr_cinematic )
		{
			SCR_DrawCinematic();
			SCR_DrawConsole();
		}
		else if( cls.state == CA_DISCONNECTED )
		{
			CL_UIModule_Refresh( qtrue, qtrue );
			SCR_DrawConsole();
		}
		else if( cls.state == CA_GETTING_TICKET || cls.state == CA_CONNECTING || cls.state == CA_CONNECTED || cls.state == CA_HANDSHAKE )
		{
			CL_UIModule_UpdateConnectScreen( qtrue );
		}
		else if( cls.state == CA_LOADING )
		{
			SCR_RenderView( separation[i] );
			CL_UIModule_UpdateConnectScreen( qfalse );
		}
		else if( cls.state == CA_ACTIVE )
		{
			SCR_RenderView( separation[i] );

			CL_UIModule_Refresh( qfalse, qtrue );

			if( scr_timegraph->integer )
				SCR_DebugGraph( cls.frametime*300, 1, 1, 1 );

			if( scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer )
				SCR_DrawDebugGraph();

			SCR_DrawConsole();
		}

		// wsw : aiwa : call any listeners so they can draw their stuff
		Dynvar_CallListeners( updatescreen, NULL );

		R_EndFrame();
	}
}
