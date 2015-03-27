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
// cg_screen.c -- master status bar, crosshairs, hud, etc

/*

full screen console
put up loading plaque
blanked background with loading plaque
blanked background with menu
cinematics
full screen image for quit and victory

end of unit intermissions

*/

#include "cg_local.h"

vrect_t	scr_vrect;

cvar_t *cg_viewSize;
cvar_t *cg_centerTime;
cvar_t *cg_showFPS;
cvar_t *cg_showPointedPlayer;
cvar_t *cg_showHUD;
cvar_t *cg_draw2D;
cvar_t *cg_weaponlist;
cvar_t *cg_debugLoading;

cvar_t *cg_crosshair;
cvar_t *cg_crosshair_size;
cvar_t *cg_crosshair_color;
cvar_t *cg_crosshair_font;

cvar_t *cg_crosshair_strong;
cvar_t *cg_crosshair_strong_size;
cvar_t *cg_crosshair_strong_color;

cvar_t *cg_crosshair_damage_color;

cvar_t *cg_clientHUD;
cvar_t *cg_specHUD;
cvar_t *cg_debugHUD;
cvar_t *cg_showSpeed;
cvar_t *cg_showPickup;
cvar_t *cg_showTimer;
cvar_t *cg_showAwards;
cvar_t *cg_showZoomEffect;
cvar_t *cg_showCaptureAreas;

cvar_t *cg_showPlayerNames;
cvar_t *cg_showPlayerNames_alpha;
cvar_t *cg_showPlayerNames_zfar;
cvar_t *cg_showPlayerNames_barWidth;
cvar_t *cg_showTeamMates;

cvar_t *cg_showPressedKeys;

cvar_t *cg_scoreboardFontFamily;
cvar_t *cg_scoreboardMonoFontFamily;
cvar_t *cg_scoreboardTitleFontFamily;
cvar_t *cg_scoreboardFontSize;
cvar_t *cg_scoreboardTitleFontSize;
cvar_t *cg_scoreboardWidthScale;
cvar_t *cg_scoreboardStats;

cvar_t *cg_showTeamLocations;
cvar_t *cg_showViewBlends;

cvar_t *cg_touch_moveThres;
cvar_t *cg_touch_strafeThres;
cvar_t *cg_touch_lookThres;
cvar_t *cg_touch_lookSens;
cvar_t *cg_touch_lookInvert;
cvar_t *cg_touch_lookDecel;

float scr_damagetime_off;

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char scr_centerstring[1024];
float scr_centertime_start;   // for slow victory printing
float scr_centertime_off;
int scr_center_lines;
int scr_erase_center;

/*
* CG_CenterPrint
* 
* Called for important messages that should stay in the center of the screen
* for a few moments
*/
void CG_CenterPrint( const char *str )
{
	char c, *s;
	int colorindex = -1;
	const char *tmp;
	char l10n_buffer[sizeof(scr_centerstring)];
	const char *l10n = NULL;

	tmp = str;
	if( Q_GrabCharFromColorString( &tmp, &c, &colorindex ) == GRABCHAR_COLOR ) {
		// attempt to translate the remaining string
		l10n = trap_L10n_TranslateString( tmp );
	} else {
		l10n = trap_L10n_TranslateString( str );
	}

	if( l10n ) {
		if( colorindex > 0 ) {
			l10n_buffer[0] = '^';
			l10n_buffer[1] = '0' + colorindex;
			Q_strncpyz( &l10n_buffer[2], l10n, sizeof( l10n_buffer ) - 2 );
		}
		else {
			Q_strncpyz( l10n_buffer, l10n, sizeof( l10n_buffer ) );
		}
		str = l10n_buffer;
	}

	Q_strncpyz( scr_centerstring, str, sizeof( scr_centerstring ) );
	scr_centertime_off = cg_centerTime->value;
	scr_centertime_start = cg.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = scr_centerstring;
	while( *s )
		if( *s++ == '\n' )
			scr_center_lines++;
}

void CG_CenterPrintToUpper( const char *format, ... )
{
	char c, *s;
	int colorindex = -1;
	va_list	argptr;
	const char *tmp;
	const char *new_format = format;
	char l10n_format[sizeof(scr_centerstring)];
	const char *l10n = NULL;

	tmp = format;
	if( Q_GrabCharFromColorString( &tmp, &c, &colorindex ) == GRABCHAR_COLOR ) {
		// attempt to translate the remaining string
		l10n = trap_L10n_TranslateString( tmp );
	} else {
		l10n = trap_L10n_TranslateString( format );
	}

	if( l10n ) {
		if( colorindex > 0 ) {
			l10n_format[0] = '^';
			l10n_format[1] = '0' + colorindex;
			Q_strncpyz( &l10n_format[2], l10n, sizeof( l10n_format ) - 2 );
		}
		else {
			Q_strncpyz( l10n_format, l10n, sizeof( l10n_format ) );
		}
		new_format = l10n_format;
	}

	va_start( argptr, format );
	Q_vsnprintfz( scr_centerstring, sizeof( scr_centerstring ), new_format, argptr );
	va_end( argptr );

	scr_centertime_off = cg_centerTime->value;
	scr_centertime_start = cg.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = scr_centerstring;
	while( *s )
	{
		if( *s == '\n' )
		{
			scr_center_lines++;
		}
		else
		{
			*s = toupper( *s );
		}
		s++;
	}
}

static void CG_DrawCenterString( void )
{
	int y;
	struct qfontface_s *font = cgs.fontSystemMedium;
	char *helpmessage = scr_centerstring;
	int x = cgs.vidWidth / 2;
	int width = cgs.vidWidth / 2;
	size_t len;

	if( scr_center_lines <= 4 )
		y = cgs.vidHeight*0.35;
	else
		y = 48 * cgs.vidHeight / 600;

	if( width < 320 * cgs.vidHeight / 600 )
		width = 320 * cgs.vidHeight / 600;

	while( ( len = trap_SCR_DrawStringWidth( x, y, ALIGN_CENTER_TOP, helpmessage, width, font, colorWhite ) ) )
	{
		if( len && helpmessage[len-1] == '\n' )
		{
			y += trap_SCR_FontHeight( font );
		}
		helpmessage += len;
	}
}

//=============================================================================

static void CG_CheckDamageCrosshair( void )
{
	scr_damagetime_off -= cg.frameTime;
	if( scr_damagetime_off <= 0 ) 
	{
		if ( ! cg_crosshair_damage_color->modified )
			return;

		// Reset crosshair
		cg_crosshair_color->modified = true;
		cg_crosshair_strong_color->modified = true;
		cg_crosshair_damage_color->modified = false;
	}
}

void CG_ScreenCrosshairDamageUpdate( void )
{
	cg_crosshair_damage_color->modified = true;
}

//=============================================================================

/*
* CG_CalcVrect
* 
* Sets scr_vrect, the coordinates of the rendered window
*/
void CG_CalcVrect( void )
{
	int size;

	// bound viewsize
	if( cg_viewSize->integer < 40 )
		trap_Cvar_Set( cg_viewSize->name, "40" );
	else if( cg_viewSize->integer > 100 )
		trap_Cvar_Set( cg_viewSize->name, "100" );

	size = cg_viewSize->integer;

	scr_vrect.width = cgs.vidWidth*size/100;
	scr_vrect.width &= ~1;

	scr_vrect.height = cgs.vidHeight*size/100;
	scr_vrect.height &= ~1;

	scr_vrect.x = ( cgs.vidWidth - scr_vrect.width )/2;
	scr_vrect.y = ( cgs.vidHeight - scr_vrect.height )/2;
}

/*
* CG_SizeUp_f
* 
* Keybinding command
*/
static void CG_SizeUp_f( void )
{
	trap_Cvar_SetValue( cg_viewSize->name, cg_viewSize->integer + 10 );
}

/*
* CG_SizeDown_f
* 
* Keybinding command
*/
static void CG_SizeDown_f( void )
{
	trap_Cvar_SetValue( cg_viewSize->name, cg_viewSize->integer - 10 );
}

//============================================================================

enum
{
	TOUCHAREA_SCREEN_CROSSHAIR = TOUCHAREA_SCREEN,
	TOUCHAREA_SCREEN_TIMER
};

/*
* CG_ScreenInit
*/
void CG_ScreenInit( void )
{
	cg_viewSize =		trap_Cvar_Get( "cg_viewSize", "100", CVAR_ARCHIVE );
	cg_showFPS =		trap_Cvar_Get( "cg_showFPS", "0", CVAR_ARCHIVE );
	cg_showHUD =		trap_Cvar_Get( "cg_showHUD", "1", CVAR_ARCHIVE );
	cg_draw2D =		trap_Cvar_Get( "cg_draw2D", "1", 0 );
	cg_centerTime =		trap_Cvar_Get( "cg_centerTime", "2.5", 0 );
	cg_debugLoading =	trap_Cvar_Get( "cg_debugLoading", "0", CVAR_ARCHIVE );
	cg_weaponlist =		trap_Cvar_Get( "cg_weaponlist", "1", CVAR_ARCHIVE );

	cg_crosshair =		trap_Cvar_Get( "cg_crosshair", "1", CVAR_ARCHIVE );
	cg_crosshair_size =	trap_Cvar_Get( "cg_crosshair_size", "24", CVAR_ARCHIVE );
	cg_crosshair_color =	trap_Cvar_Get( "cg_crosshair_color", "255 255 255", CVAR_ARCHIVE );
	cg_crosshair_font =		trap_Cvar_Get( "cg_crosshair_font", "Warsow Crosshairs", CVAR_ARCHIVE );
	cg_crosshair_damage_color =	trap_Cvar_Get( "cg_crosshair_damage_color", "255 0 0", CVAR_ARCHIVE );
	cg_crosshair_color->modified = true;
	cg_crosshair_damage_color->modified = false;

	cg_crosshair_strong =	    trap_Cvar_Get( "cg_crosshair_strong", "0", CVAR_ARCHIVE );
	cg_crosshair_strong_size =  trap_Cvar_Get( "cg_crosshair_strong_size", "24", CVAR_ARCHIVE );
	cg_crosshair_strong_color = trap_Cvar_Get( "cg_crosshair_strong_color", "255 255 255", CVAR_ARCHIVE );
	cg_crosshair_strong_color->modified = true;

	cg_clientHUD =		trap_Cvar_Get( "cg_clientHUD", "default", CVAR_ARCHIVE );
	cg_specHUD =		trap_Cvar_Get( "cg_specHUD", "default", CVAR_ARCHIVE );
	cg_showTimer =		trap_Cvar_Get( "cg_showTimer", "1", CVAR_ARCHIVE );
	cg_showSpeed =		trap_Cvar_Get( "cg_showSpeed", "0", CVAR_ARCHIVE );
	cg_showPickup =		trap_Cvar_Get( "cg_showPickup", "1", CVAR_ARCHIVE );
	cg_showPointedPlayer =	trap_Cvar_Get( "cg_showPointedPlayer", "1", CVAR_ARCHIVE );
	cg_showTeamLocations =	trap_Cvar_Get( "cg_showTeamLocations", "1", CVAR_ARCHIVE );
	cg_showViewBlends =	trap_Cvar_Get( "cg_showViewBlends", "1", CVAR_ARCHIVE );
	cg_showAwards =		trap_Cvar_Get( "cg_showAwards", "1", CVAR_ARCHIVE );
	cg_showZoomEffect =	trap_Cvar_Get( "cg_showZoomEffect", "1", CVAR_ARCHIVE );
	cg_showCaptureAreas = trap_Cvar_Get( "cg_showCaptureAreas", "1", CVAR_ARCHIVE );

	cg_showPlayerNames =	    trap_Cvar_Get( "cg_showPlayerNames", "1", CVAR_ARCHIVE );
	cg_showPlayerNames_alpha =  trap_Cvar_Get( "cg_showPlayerNames_alpha", "0.4", CVAR_ARCHIVE );
	cg_showPlayerNames_zfar =   trap_Cvar_Get( "cg_showPlayerNames_zfar", "1024", CVAR_ARCHIVE );
	cg_showPlayerNames_barWidth =   trap_Cvar_Get( "cg_showPlayerNames_barWidth", "8", CVAR_ARCHIVE );
	cg_showTeamMates =	    trap_Cvar_Get( "cg_showTeamMates", "1", CVAR_ARCHIVE );

	cg_showPressedKeys = trap_Cvar_Get( "cg_showPressedKeys", "0", CVAR_ARCHIVE );

	cg_scoreboardFontFamily = trap_Cvar_Get( "cg_scoreboardFontFamily", DEFAULT_SCOREBOARD_FONT_FAMILY, CVAR_ARCHIVE );
	cg_scoreboardMonoFontFamily = trap_Cvar_Get( "cg_scoreboardMonoFontFamily", DEFAULT_SCOREBOARD_MONO_FONT_FAMILY, CVAR_ARCHIVE );
	cg_scoreboardTitleFontFamily = trap_Cvar_Get( "cg_scoreboardTitleFontFamily", DEFAULT_SCOREBOARD_TITLE_FONT_FAMILY, CVAR_ARCHIVE );
	cg_scoreboardFontSize = trap_Cvar_Get( "cg_scoreboardFontSize", STR_TOSTR( DEFAULT_SCOREBOARD_FONT_SIZE ), CVAR_ARCHIVE );
	cg_scoreboardTitleFontSize = trap_Cvar_Get( "cg_scoreboardTitleFontSize", STR_TOSTR( DEFAULT_SCOREBOARD_TITLE_FONT_SIZE ), CVAR_ARCHIVE );
	cg_scoreboardWidthScale = trap_Cvar_Get( "cg_scoreboardWidthScale", "1.0", CVAR_ARCHIVE );
	cg_scoreboardStats =	trap_Cvar_Get( "cg_scoreboardStats", "1", CVAR_ARCHIVE );

	// wsw : hud debug prints
	cg_debugHUD =		    trap_Cvar_Get( "cg_debugHUD", "0", 0 );

	cg_touch_moveThres = trap_Cvar_Get( "cg_touch_moveThres", "4", CVAR_ARCHIVE );
	cg_touch_strafeThres = trap_Cvar_Get( "cg_touch_strafeThres", "8", CVAR_ARCHIVE );
	cg_touch_lookThres = trap_Cvar_Get( "cg_touch_lookThres", "4", CVAR_ARCHIVE );
	cg_touch_lookSens = trap_Cvar_Get( "cg_touch_lookSens", "3.25", CVAR_ARCHIVE );
	cg_touch_lookInvert = trap_Cvar_Get( "cg_touch_lookInvert", "0", CVAR_ARCHIVE );
	cg_touch_lookDecel = trap_Cvar_Get( "cg_touch_lookDecel", "0.7", CVAR_ARCHIVE );

	//
	// register our commands
	//
	trap_Cmd_AddCommand( "sizeup", CG_SizeUp_f );
	trap_Cmd_AddCommand( "sizedown", CG_SizeDown_f );
	trap_Cmd_AddCommand( "help_hud", Cmd_CG_PrintHudHelp_f );
	trap_Cmd_AddCommand( "gamemenu", CG_GameMenu_f );

	int i;
	for( i = 0; i < TOUCHPAD_COUNT; ++i )
		CG_SetTouchpad( i, -1 );
}

/*
* CG_ScreenShutdown
*/
void CG_ScreenShutdown( void )
{
	trap_Cmd_RemoveCommand( "gamemenu" );
	trap_Cmd_RemoveCommand( "sizeup" );
	trap_Cmd_RemoveCommand( "sizedown" );
	trap_Cmd_RemoveCommand( "help_hud" );
}


/*
* CG_ParseValue
*/
int CG_ParseValue( const char **s )
{
	int index;
	char *token;

	token = COM_Parse( s );
	if( !token[0] )
		return 0;
	if( token[0] != '%' )
		return atoi( token );

	index = atoi( token + 1 );
	if( index < 0 || index >= PS_MAX_STATS )
		CG_Error( "Bad stat index: %i", index );

	return cg.predictedPlayerState.stats[index];
}

/*
* CG_DrawNet
*/
void CG_DrawNet( int x, int y, int w, int h, int align, vec4_t color )
{
	int incomingAcknowledged, outgoingSequence;

	if( cgs.demoPlaying )
		return;

	trap_NET_GetCurrentState( &incomingAcknowledged, &outgoingSequence, NULL );
	if( outgoingSequence - incomingAcknowledged < CMD_BACKUP-1 )
		return;
	x = CG_HorizontalAlignForWidth( x, align, w );
	y = CG_VerticalAlignForHeight( y, align, h );
	trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, CG_MediaShader( cgs.media.shaderNet ) );
}

/*
* CG_CrosshairDimensions
*/
static int CG_CrosshairDimensions( int x, int y, int size, int align, int *sx, int *sy )
{
	size = ceilf( size * (float)( cgs.vidHeight / 600.0f ) );
	size += size & 1; // crosshairs are symmetric, so make their size even
	*sx = CG_HorizontalAlignForWidth( x, align, size );
	*sy = CG_VerticalAlignForHeight( y, align, size );
	return size;
}

/*
* CG_DrawCrosshairChar
*/
static void CG_DrawCrosshairChar( int x, int y, int size, int num, vec_t *color )
{
	struct qfontface_s *font = trap_SCR_RegisterSpecialFont( cg_crosshair_font->string, QFONT_STYLE_NONE, size );
	if( !font )
	{
		trap_Cvar_Set( cg_crosshair_font->name, cg_crosshair_font->dvalue );
		font = trap_SCR_RegisterSpecialFont( cg_crosshair_font->string, QFONT_STYLE_NONE, size );
	}

	wchar_t blackChar, colorChar;
	if( num )
	{
		blackChar = 'A' - 1 + num;
		colorChar = 'a' - 1 + num;
	}
	else
	{
		blackChar = '?';
		colorChar = '!';
	}

	trap_SCR_DrawRawChar( x, y, blackChar, font, colorBlack );
	trap_SCR_DrawRawChar( x, y, colorChar, font, color );
}

/*
* CG_DrawCrosshair
*/
void CG_DrawCrosshair( int x, int y, int align )
{
	static vec4_t chColor = { 255, 255, 255, 255 };
	static vec4_t chColorStrong = { 255, 255, 255, 255 };
	int rgbcolor;
	int sx, sy, size;

	if( cg_crosshair->modified )
	{
		if( cg_crosshair->integer > 26 || cg_crosshair->integer < 0 )
			trap_Cvar_Set( cg_crosshair->name, "0" );
		cg_crosshair->modified = false;
	}

	if( cg_crosshair_size->modified )
	{
		if( cg_crosshair_size->integer <= 0 || cg_crosshair_size->integer > 64 )
			trap_Cvar_Set( cg_crosshair_size->name, cg_crosshair_size->dvalue );
		cg_crosshair_size->modified = false;
	}

	if( cg_crosshair_color->modified || cg_crosshair_damage_color->modified )
	{
		if ( cg_crosshair_damage_color->modified ) 
		{
			if ( scr_damagetime_off <= 0 )
				scr_damagetime_off = 0.3;
			rgbcolor = COM_ReadColorRGBString( cg_crosshair_damage_color->string );
		} else {
			rgbcolor = COM_ReadColorRGBString( cg_crosshair_color->string );
		}
		if( rgbcolor != -1 )
		{
			Vector4Set( chColor, COLOR_R( rgbcolor ), COLOR_G( rgbcolor ), COLOR_B( rgbcolor ), 255 );
		}
		else
		{
			Vector4Set( chColor, 255, 255, 255, 255 );
		}
		cg_crosshair_color->modified = false;
	}

	if( cg_crosshair_strong->modified )
	{
		if( cg_crosshair_strong->integer > 26 || cg_crosshair_strong->integer < 0 )
			trap_Cvar_Set( cg_crosshair_strong->name, "0" );
		cg_crosshair_strong->modified = false;
	}

	if( cg_crosshair_strong_size->modified )
	{
		if( cg_crosshair_strong_size->integer <= 0 || cg_crosshair_strong_size->integer > 64 )
			trap_Cvar_Set( cg_crosshair_strong_size->name, cg_crosshair_strong_size->dvalue );
		cg_crosshair_strong_size->modified = false;
	}

	if( cg_crosshair_strong_color->modified || cg_crosshair_damage_color->modified )
	{
		if ( cg_crosshair_damage_color->modified ) 
		{
			rgbcolor = COM_ReadColorRGBString( cg_crosshair_damage_color->string );
		} else {
			rgbcolor = COM_ReadColorRGBString( cg_crosshair_strong_color->string );
		}
		if( rgbcolor != -1 )
		{
			Vector4Set( chColorStrong, COLOR_R( rgbcolor ), COLOR_G( rgbcolor ), COLOR_B( rgbcolor ), 255 );
		}
		else
		{
			Vector4Set( chColorStrong, 255, 255, 255, 255 );
		}
		cg_crosshair_strong_color->modified = false;
	}

	if( cg_crosshair_strong->integer )
	{
		firedef_t *firedef = GS_FiredefForPlayerState( &cg.predictedPlayerState, cg.predictedPlayerState.stats[STAT_WEAPON] );
		if( firedef && firedef->fire_mode == FIRE_MODE_STRONG ) // strong
		{
			size = CG_CrosshairDimensions( x, y, cg_crosshair_strong_size->integer, align, &sx, &sy );
			CG_DrawCrosshairChar( sx, sy, size, cg_crosshair_strong->integer, chColorStrong );
		}
	}

	if( cg_crosshair->integer && ( cg.predictedPlayerState.stats[STAT_WEAPON] != WEAP_NONE ) )
	{
		size = CG_CrosshairDimensions( x, y, cg_crosshair_size->integer, align, &sx, &sy );
		CG_DrawCrosshairChar( sx, sy, size, cg_crosshair->integer, chColor );
	}
}

void CG_DrawKeyState( int x, int y, int w, int h, int align, const char *key )
{
	int i;
	uint8_t on = 0;
	usercmd_t cmd;
	vec4_t color;

	if( !cg_showPressedKeys->integer && !cgs.demoTutorial && !GS_TutorialGametype() )
		return;

	if( !key )
		return;

	for( i = 0; i < KEYICON_TOTAL; i++ )
		if( !Q_stricmp( key, gs_keyicon_names[i] ) )
			break;

	if( i == KEYICON_TOTAL )
		return;

	// now we have a valid key name so we draw it
	trap_NET_GetUserCmd( trap_NET_GetCurrentUserCmdNum() - 1, &cmd );

	if( cg.predictedPlayerState.plrkeys & ( 1 << i ) )
		on = 1;

	Vector4Copy( colorWhite, color );
	if( !on )
		color[3] = 0.5f;

	trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, CG_MediaShader( cgs.media.shaderKeyIcon[i] ) );
}

/*
* CG_ClockUpFunc
*/
void CG_ClockUpFunc( int id, unsigned int time )
{
	CG_ScoresOff_f();
}

/*
* CG_DrawClock
*/
void CG_DrawClock( int x, int y, int align, struct qfontface_s *font, vec4_t color, bool touch )
{
	unsigned int clocktime, startTime, duration, curtime;
	double seconds;
	int minutes;
	char string[12];

	if( !cg_showTimer->integer )
		return;

	if( GS_MatchState() > MATCH_STATE_PLAYTIME )
		return;

	if( GS_MatchClockOverride() )
	{
		clocktime = GS_MatchClockOverride();
	}
	else
	{
		curtime = GS_MatchPaused() ? cg.frame.serverTime : cg.time;
		duration = GS_MatchDuration();
		startTime = GS_MatchStartTime();

		// count downwards when having a duration
		if( duration && ( cg_showTimer->integer != 3 ) )
		{
			if( duration + startTime < curtime ) 
				duration = curtime - startTime; // avoid negative results

			clocktime = startTime + duration - curtime;
		}
		else
		{
			if( curtime >= startTime ) // avoid negative results
				clocktime = curtime - startTime;
			else
				clocktime = 0;
		}
	}

	seconds = (double)clocktime * 0.001;
	minutes = (int)( seconds / 60 );
	seconds -= minutes * 60;

	// fixme?: this could have its own HUD drawing, I guess.
	if( cg.predictedPlayerState.stats[STAT_NEXT_RESPAWN] )
	{
		int respawn = cg.predictedPlayerState.stats[STAT_NEXT_RESPAWN];
		Q_snprintfz( string, sizeof( string ), "%02i:%02i R:%02i", minutes, (int)seconds, respawn );
	}
	else
	{
		Q_snprintfz( string, sizeof( string ), "%02i:%02i", minutes, (int)seconds );
	}

	if( touch )
	{
		int w = trap_SCR_strWidth( string, font, 0 );
		int h = trap_SCR_FontHeight( font );
		if( CG_TouchArea( TOUCHAREA_SCREEN_TIMER,
			CG_HorizontalAlignForWidth( x, align, w ), CG_VerticalAlignForHeight( y, align, h ),
			w, h, CG_ClockUpFunc ) >= 0 )
		{
			CG_ScoresOn_f();
		}
	}
	else
	{
		trap_SCR_DrawString( x, y, align, string, font, color );
	}
}

static unsigned int point_remove_time;
static int pointed_health;
static int pointed_armor;

/*
* CG_UpdatePointedNum
*/
void CG_UpdatePointedNum( void )
{
	// disable cases
	if( ( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD )
		|| cg.view.thirdperson || cg.view.type != VIEWDEF_PLAYERVIEW || !cg_showPointedPlayer->integer )
	{
		cg.pointedNum = 0;
		return;
	}

	if( cg.predictedPlayerState.stats[STAT_POINTED_PLAYER] )
	{
		bool mega = false;

		cg.pointedNum = cg.predictedPlayerState.stats[STAT_POINTED_PLAYER];
		point_remove_time = cg.time + 150;

		pointed_health = 3.2 * ( cg.predictedPlayerState.stats[STAT_POINTED_TEAMPLAYER] &0x1F );
		mega = cg.predictedPlayerState.stats[STAT_POINTED_TEAMPLAYER]&0x20 ? true : false;
		pointed_armor = 5 * ( cg.predictedPlayerState.stats[STAT_POINTED_TEAMPLAYER]>>6 &0x3F );
		if( mega )
		{
			pointed_health += 100;
			if( pointed_health > 200 )
				pointed_health = 200;
		}
	}

	if( point_remove_time <= cg.time )
		cg.pointedNum = 0;

	if( cg.pointedNum && cg_showPointedPlayer->integer == 2 )
	{
		if( cg_entities[cg.pointedNum].current.team != cg.predictedPlayerState.stats[STAT_TEAM] )
			cg.pointedNum = 0;
	}
}

/*
* CG_DrawPlayerNames
*/
void CG_DrawPlayerNames( struct qfontface_s *font, vec4_t color )
{
	static vec4_t alphagreen = { 0, 1, 0, 0 }, alphared = { 1, 0, 0, 0 }, alphayellow = { 1, 1, 0, 0 }, alphamagenta = { 1, 0, 1, 1 }, alphagrey = { 0.85, 0.85, 0.85, 1 };
	centity_t *cent;
	vec4_t tmpcolor;
	vec3_t dir, drawOrigin;
	vec2_t coords;
	float dist, fadeFrac;
	trace_t trace;
	int i;

	if( !cg_showPlayerNames->integer && !cg_showPointedPlayer->integer )
		return;

	CG_UpdatePointedNum();

	// don't draw when scoreboard is up
	if( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD )
		return;

	for( i = 0; i < gs.maxclients; i++ )
	{
		if( !cgs.clientInfo[i].name[0] || ISVIEWERENTITY( i + 1 ) )
			continue;

		cent = &cg_entities[i + 1];
		if( cent->serverFrame != cg.frame.serverFrame )
			continue;

		if( cent->current.effects & EF_PLAYER_HIDENAME )
			continue;

		// only show the pointed player
		if( !cg_showPlayerNames->integer && ( cent->current.number != cg.pointedNum ) )
			continue;

		if( ( cg_showPlayerNames->integer == 2 ) && ( cent->current.team != cg.predictedPlayerState.stats[STAT_TEAM] ) )
			continue;

		if( !cent->current.modelindex || !cent->current.solid ||
			cent->current.solid == SOLID_BMODEL || cent->current.team == TEAM_SPECTATOR )
			continue;

		// Kill if behind the view
		VectorSubtract( cent->ent.origin, cg.view.origin, dir );
		dist = VectorNormalize( dir ) * cg.view.fracDistFOV;

		if( DotProduct( dir, &cg.view.axis[AXIS_FORWARD] ) < 0 )
			continue;

		Vector4Copy( color, tmpcolor );

		if( cent->current.number != cg.pointedNum )
		{
			if( dist > cg_showPlayerNames_zfar->value )
				continue;

			fadeFrac = ( cg_showPlayerNames_zfar->value - dist ) / ( cg_showPlayerNames_zfar->value * 0.25f );
			clamp( fadeFrac, 0.0f, 1.0f );

			tmpcolor[3] = cg_showPlayerNames_alpha->value * color[3] * fadeFrac;
		}
		else
		{
			fadeFrac = (float)( point_remove_time - cg.time ) / 150.0f;
			clamp( fadeFrac, 0.0f, 1.0f );

			tmpcolor[3] = color[3] * fadeFrac;
		}

		if( tmpcolor[3] <= 0.0f )
			continue;

		CG_Trace( &trace, cg.view.origin, vec3_origin, vec3_origin, cent->ent.origin, cg.predictedPlayerState.POVnum, MASK_OPAQUE );
		if( trace.fraction < 1.0f && trace.ent != cent->current.number )
			continue;

		VectorSet( drawOrigin, cent->ent.origin[0], cent->ent.origin[1], cent->ent.origin[2] + playerbox_stand_maxs[2] + 16 );

		// find the 3d point in 2d screen
		trap_R_TransformVectorToScreen( &cg.view.refdef, drawOrigin, coords );
		if( ( coords[0] < 0 || coords[0] > cgs.vidWidth ) || ( coords[1] < 0 || coords[1] > cgs.vidHeight ) )
			continue;

		trap_SCR_DrawString( coords[0], coords[1], ALIGN_CENTER_BOTTOM, cgs.clientInfo[i].name, font, tmpcolor );

		// if not the pointed player we are done
		if( cent->current.number != cg.pointedNum )
			continue;

		// pointed player hasn't a health value to be drawn, so skip adding the bars
		if( pointed_health && cg_showPlayerNames_barWidth->integer > 0 )
		{
			int x, y;
			int barwidth = trap_SCR_strWidth( "_", font, 0 ) * cg_showPlayerNames_barWidth->integer; // size of 8 characters
			int barheight = trap_SCR_FontHeight( font ) * 0.25; // quarter of a character height
			int barseparator = barheight * 0.333;

			alphagreen[3] = alphared[3] = alphayellow[3] = alphamagenta[3] = alphagrey[3] = tmpcolor[3];

			// soften the alpha of the box color
			tmpcolor[3] *= 0.4f;

			// we have to align first, then draw as left top, cause we want the bar to grow from left to right
			x = CG_HorizontalAlignForWidth( coords[0], ALIGN_CENTER_TOP, barwidth );
			y = CG_VerticalAlignForHeight( coords[1], ALIGN_CENTER_TOP, barheight );

			// draw the background box
			CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight * 3, 100, 100, tmpcolor, NULL );

			y += barseparator;

			if( pointed_health > 100 )
			{
				alphagreen[3] = alphamagenta[3] = 1.0f;
				CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, 100, 100, alphagreen, NULL );
				CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, pointed_health - 100, 100, alphamagenta, NULL );
				alphagreen[3] = alphamagenta[3] = alphared[3];
			}
			else
			{
				if( pointed_health <= 33 )
					CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, pointed_health, 100, alphared, NULL );
				else if( pointed_health <= 66 )
					CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, pointed_health, 100, alphayellow, NULL );
				else
					CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, pointed_health, 100, alphagreen, NULL );
			}

			if( pointed_armor )
			{
				y += barseparator + barheight;
				CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, pointed_armor, 150, alphagrey, NULL );
			}
		}
	}
}

/*
* CG_DrawTeamMates
*/
void CG_DrawTeamMates( void )
{
	centity_t *cent;
	vec3_t dir, drawOrigin;
	vec2_t coords;
	trace_t trace;
	vec4_t color;
	int i;

	if( !cg_showTeamMates->integer )
		return;

	// don't draw when scoreboard is up
	if( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD )
		return;
	if(  cg.predictedPlayerState.stats[STAT_TEAM] < TEAM_ALPHA )
		return;

	for( i = 0; i < gs.maxclients; i++ )
	{
		if( !cgs.clientInfo[i].name[0] || ISVIEWERENTITY( i + 1 ) )
			continue;

		cent = &cg_entities[i + 1];
		if( cent->serverFrame != cg.frame.serverFrame )
			continue;

		if( cent->current.team != cg.predictedPlayerState.stats[STAT_TEAM] )
			continue;

		if( !cent->current.modelindex || !cent->current.solid ||
			cent->current.solid == SOLID_BMODEL || cent->current.team == TEAM_SPECTATOR )
			continue;

		// Kill if in the view
		VectorSubtract( cent->ent.origin, cg.view.origin, dir );
		if( DotProduct( dir, &cg.view.axis[AXIS_FORWARD] ) < 0 )
			continue;
			
		CG_Trace( &trace, cg.view.origin, vec3_origin, vec3_origin, cent->ent.origin, cg.predictedPlayerState.POVnum, MASK_OPAQUE );
		if( cg_showTeamMates->integer == 1 && trace.fraction == 1.0f )
			continue;

		VectorSet( drawOrigin, cent->ent.origin[0], cent->ent.origin[1], cent->ent.origin[2] + playerbox_stand_maxs[2] + 16 );

		// find the 3d point in 2d screen
		trap_R_TransformVectorToScreen( &cg.view.refdef, drawOrigin, coords );
		if( ( coords[0] < 0 || coords[0] > cgs.vidWidth ) || ( coords[1] < 0 || coords[1] > cgs.vidHeight ) )
			continue;
		
		CG_TeamColor( cg.predictedPlayerState.stats[STAT_TEAM], color );

		trap_R_DrawStretchPic( coords[0],
			coords[1],
			16, 16, 0, 0, 1, 1,
			color, CG_MediaShader( cgs.media.shaderTeamMateIndicator ) );
	}
}

/*
* CG_DrawTeamInfo
*/
void CG_DrawTeamInfo( int x, int y, int align, struct qfontface_s *font, vec4_t color )
{
	char string[128];
	int team;
	int teammate;
	char *ptr, *tok, *loc, *hp, *ap;
	int height, pixheight;
	int locationTag;
	int health, armor;
	centity_t *cent;

	if( !( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_TEAMTAB ) )
		return;

	// don't draw when scoreboard is up
	if( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD )
		return;

	if( cg.view.type != VIEWDEF_PLAYERVIEW || !cg_showTeamLocations->integer )
		return;

	team = cg.predictedPlayerState.stats[STAT_TEAM];
	if( team <= TEAM_PLAYERS || team >= GS_MAX_TEAMS
		|| !GS_TeamBasedGametype() || GS_InvidualGameType() )
		return;

	// time to parse the teaminfo string
	if( !cg.teaminfo || !strlen( cg.teaminfo ) )
		return;

	height = trap_SCR_FontHeight( font );

	// find longest line
	ptr = cg.teaminfo;
	pixheight = 0;
	while( ptr )
	{
		tok = COM_Parse( &ptr );
		if( !tok[0] )
			break;

		teammate = atoi( tok );
		if( teammate < 0 || teammate >= gs.maxclients )
			break;

		loc = COM_Parse( &ptr );
		if( !loc[0] )
			break;

		locationTag = atoi( loc );
		if( locationTag >= MAX_LOCATIONS )
			locationTag = 0;

		hp = COM_Parse( &ptr );
		if( !hp[0] )
			break;

		health = atoi( hp );
		if( health < 0 )
			health = 0;

		ap = COM_Parse( &ptr );
		if( !ap[0] )
			break;

		armor = atoi( ap );
		if( armor < 0 )
			armor = 0;

		// we don't display ourselves
		if( !ISVIEWERENTITY( teammate+1 ) )
			pixheight += height;
	}

	y = CG_VerticalAlignForHeight( y, align, pixheight );

	ptr = cg.teaminfo;
	while( ptr )
	{
		tok = COM_Parse( &ptr );
		if( !tok[0] )
			return;

		teammate = atoi( tok );
		if( teammate < 0 || teammate >= gs.maxclients )
			return;

		loc = COM_Parse( &ptr );
		if( !loc[0] )
			return;

		locationTag = atoi( loc );
		if( locationTag >= MAX_LOCATIONS )
			locationTag = 0;

		hp = COM_Parse( &ptr );
		if( !hp[0] )
			return;

		health = atoi( hp );
		if( health < 0 )
			health = 0;

		ap = COM_Parse( &ptr );
		if( !ap[0] )
			return;

		armor = atoi( ap );
		if( armor < 0 )
			armor = 0;

		// we don't display ourselves
		if( ISVIEWERENTITY( teammate+1 ) )
			continue;

		Q_snprintfz( string, sizeof( string ), "%s%s %s%s (%i/%i)%s", cgs.clientInfo[teammate].name, S_COLOR_WHITE,
			CG_TranslateString( cgs.configStrings[CS_LOCATIONS+locationTag] ), S_COLOR_WHITE,
			health, armor, S_COLOR_WHITE );

		// draw the head-icon in the case this player has one
		cent = &cg_entities[teammate+1];
		if( cent->localEffects[LOCALEFFECT_VSAY_HEADICON_TIMEOUT] > cg.time &&
			cent->localEffects[LOCALEFFECT_VSAY_HEADICON] > 0 && cent->localEffects[LOCALEFFECT_VSAY_HEADICON] < VSAY_TOTAL )
		{
			trap_R_DrawStretchPic( CG_HorizontalAlignForWidth( x, align, height ),
				CG_VerticalAlignForHeight( y, align, height ),
				height, height, 0, 0, 1, 1,
				color, CG_MediaShader( cgs.media.shaderVSayIcon[cent->localEffects[LOCALEFFECT_VSAY_HEADICON]] ) );
		}

		trap_SCR_DrawString( x + height * ( align % 3 == 0 ), y, align, string, font, color );

		y += height;
	}
}

/*
* CG_DrawRSpeeds
*/
void CG_DrawRSpeeds( int x, int y, int align, struct qfontface_s *font, vec4_t color )
{
	char msg[1024];

	trap_R_SpeedsMessage( msg, sizeof( msg ) );

	if( msg[0] )
	{
		int height;
		const char *p, *start, *end;

		height = trap_SCR_FontHeight( font );

		p = start = msg;
		do
		{
			end = strchr( p, '\n' );
			if( end )
				msg[end-start] = '\0';

			trap_SCR_DrawString( x, y, align,
				p, font, color );
			y += height;

			if( end )
				p = end + 1;
			else
				break;
		} while( 1 );
	}
}

//=============================================================================

/*
* CG_InGameMenu
*/
static void CG_InGameMenu( void )
{
	static char menuparms[MAX_STRING_CHARS];
	int is_challenger = 0, needs_ready = 0, is_ready = 0;
	int realteam = cg.predictedPlayerState.stats[STAT_REALTEAM];

	if( GS_HasChallengers() && realteam == TEAM_SPECTATOR )
		is_challenger = ( ( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_CHALLENGER ) != 0 );

	if( GS_MatchState() <= MATCH_STATE_WARMUP && realteam != TEAM_SPECTATOR )
		needs_ready = !( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_READY );

	if( GS_MatchState() <= MATCH_STATE_WARMUP && realteam != TEAM_SPECTATOR )
		is_ready = ( ( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_READY ) != 0 );

	Q_snprintfz( menuparms, sizeof( menuparms ), 
		"menu_open game" 
			" is_teambased %i" 
			" team %i" 
			" queue %i" 
			" needs_ready %i" 
			" is_ready %i" 
			" gametype \"%s\""
			" has_gametypemenu %i"
			" team_spec %i"
			" team_list \"%i %i\"",

			GS_TeamBasedGametype(),
			realteam,
			( realteam == TEAM_SPECTATOR ) ? ( GS_HasChallengers() + is_challenger ) : 0,
			needs_ready,
			is_ready,
			gs.gametypeName,
			cgs.hasGametypeMenu,
			TEAM_SPECTATOR,
			TEAM_ALPHA,	TEAM_BETA
	);

	trap_Cmd_ExecuteText( EXEC_NOW, menuparms );
}

/*
* CG_GameMenu_f
*/
void CG_GameMenu_f( void )
{
	if( cgs.demoPlaying )
	{
		trap_Cmd_ExecuteText( EXEC_NOW, "menu_open demoplay\n" );
		return;
	}

	if( cgs.tv )
	{
		trap_Cmd_ExecuteText( EXEC_NOW, "menu_open tv\n" );
		return;
	}

	// if the menu is up, close it
	if( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD )
		trap_Cmd_ExecuteText( EXEC_NOW, "cmd putaway\n" );

	CG_InGameMenu();
}

/*
* CG_EscapeKey
*/
void CG_EscapeKey( void )
{
	CG_GameMenu_f();
}

//=============================================================================

/*
* CG_DrawLoading
*/
void CG_DrawLoading( void )
{
	if( !cgs.configStrings[CS_MAPNAME][0] )
		return;

	float scale = cgs.vidHeight / 1080.0f;

	const vec4_t color = { 22.0f / 255.0f, 20.0f / 255.0f, 28.0f / 255.0f, 1.0f };
	trap_R_DrawStretchPic( 0, 0, cgs.vidWidth, cgs.vidHeight, 0.0f, 0.0f, 1.0f, 1.0f, color, cgs.shaderWhite );
	trap_R_DrawStretchPic( cgs.vidWidth / 2 - ( int )( 256 * scale ), cgs.vidHeight / 2 - ( int )( 64 * scale ),
		512 * scale, 128 * scale, 0.0f, 0.0f, 1.0f, 1.0f, colorWhite, trap_R_RegisterPic( UI_SHADER_LOADINGLOGO ) );

	if( cg.precacheCount && cg.precacheTotal )
	{
		struct shader_s *shader = trap_R_RegisterPic( UI_SHADER_LOADINGBAR );
		int width = 480 * scale; 
		int height = 32 * scale;
		int barWidth = ( width - height ) * ( ( float )cg.precacheCount / ( float )cg.precacheTotal );
		int x = ( cgs.vidWidth - width ) / 2;
		int y = cgs.vidHeight / 2 + ( int )( 32 * scale );

		trap_R_DrawStretchPic( x, y, height, height, 0.0f, 0.0f, 0.5f, 0.5f, colorWhite, shader );
		trap_R_DrawStretchPic( x + height, y, width - height * 2, height, 0.5f, 0.0f, 0.5f, 0.5f, colorWhite, shader );
		trap_R_DrawStretchPic( x + width - height, y, height, height, 0.5f, 0.0f, 1.0f, 0.5f, colorWhite, shader );
		trap_R_DrawStretchPic( x + height / 2, y, barWidth, height, 0.25f, 0.5f, 0.25f, 1.0f, colorWhite, shader );
		trap_R_DrawStretchPic( x + barWidth, y, height, height, 0.5f, 0.5f, 1.0f, 1.0f, colorWhite, shader );
	}
}

/*
* CG_LoadingString
*/
void CG_LoadingString( const char *str )
{
	cg.checkname[0] = '\0';
	Q_strncpyz( cg.loadingstring, str, sizeof( cg.loadingstring ) );
	trap_R_UpdateScreen();
}

/*
* CG_LoadingItemName
*/
void CG_LoadingItemName( const char *str )
{
	cg.precacheCount++;	
}

/*
* CG_TileClearRect
* 
* This repeats tile graphic to fill the screen around a sized down
* refresh window.
*/
static void CG_TileClearRect( int x, int y, int w, int h, struct shader_s *shader )
{
	float iw, ih;

	iw = 1.0f / 64.0;
	ih = 1.0f / 64.0;

	trap_R_DrawStretchPic( x, y, w, h, x*iw, y*ih, ( x+w )*iw, ( y+h )*ih, colorWhite, shader );
}

/*
* CG_TileClear
* 
* Clear any parts of the tiled background that were drawn on last frame
*/
void CG_TileClear( void )
{
	int w, h;
	int top, bottom, left, right;
	struct shader_s *backTile;

	if( cg_viewSize->integer == 100 )
		return; // full screen rendering

	w = cgs.vidWidth;
	h = cgs.vidHeight;

	top = scr_vrect.y;
	bottom = top + scr_vrect.height-1;
	left = scr_vrect.x;
	right = left + scr_vrect.width-1;

	backTile = CG_MediaShader( cgs.media.shaderBackTile );

	// clear above view screen
	CG_TileClearRect( 0, 0, w, top, backTile );

	// clear below view screen
	CG_TileClearRect( 0, bottom, w, h - bottom, backTile );

	// clear left of view screen
	CG_TileClearRect( 0, top, left, bottom - top + 1, backTile );

	// clear left of view screen
	CG_TileClearRect( right, top, w - right, bottom - top + 1, backTile );
}

//===============================================================

/*
* CG_AddBlend - wsw
*/
static void CG_AddBlend( float r, float g, float b, float a, float *v_blend )
{
	float a2, a3;

	if( a <= 0 )
		return;
	a2 = v_blend[3] + ( 1-v_blend[3] )*a; // new total alpha
	a3 = v_blend[3]/a2; // fraction of color from old

	v_blend[0] = v_blend[0]*a3 + r*( 1-a3 );
	v_blend[1] = v_blend[1]*a3 + g*( 1-a3 );
	v_blend[2] = v_blend[2]*a3 + b*( 1-a3 );
	v_blend[3] = a2;
}

/*
* CG_CalcColorBlend - wsw
*/
static void CG_CalcColorBlend( float *color )
{
	float time;
	float uptime;
	float delta;
	int i, contents;

	//clear old values
	for( i = 0; i < 4; i++ )
		color[i] = 0.0f;

	// Add colorblend based on world position
	contents = CG_PointContents( cg.view.origin );
	if( contents & CONTENTS_WATER )
		CG_AddBlend( 0.0f, 0.1f, 8.0f, 0.2f, color );
	if( contents & CONTENTS_LAVA )
		CG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, color );
	if( contents & CONTENTS_SLIME )
		CG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, color );

	// Add colorblends from sfx
	for( i = 0; i < MAX_COLORBLENDS; i++ )
	{
		if( cg.time > cg.colorblends[i].timestamp + cg.colorblends[i].blendtime )
			continue;

		time = (float)( ( cg.colorblends[i].timestamp + cg.colorblends[i].blendtime ) - cg.time );
		uptime = ( (float)cg.colorblends[i].blendtime ) * 0.5f;
		delta = 1.0f - ( abs( time - uptime ) / uptime );
		if( delta <= 0.0f )
			continue;
		if( delta > 1.0f )
			delta = 1.0f;

		CG_AddBlend( cg.colorblends[i].blend[0],
			cg.colorblends[i].blend[1],
			cg.colorblends[i].blend[2],
			cg.colorblends[i].blend[3] * delta,
			color );
	}
}

/*
* CG_SCRDrawViewBlend
*/
static void CG_SCRDrawViewBlend( void )
{
	vec4_t colorblend;

	if( !cg_showViewBlends->integer )
		return;

	CG_CalcColorBlend( colorblend );
	if( colorblend[3] < 0.01f )
		return;

	trap_R_DrawStretchPic( 0, 0, cgs.vidWidth, cgs.vidHeight, 0, 0, 1, 1, colorblend, cgs.shaderWhite );
}


//=======================================================

/*
* CG_CheckHUDChanges
*/
static void CG_CheckHUDChanges( void )
{
	// if changed from or to spec, reload the HUD
	if (cg.specStateChanged) {
		cg_specHUD->modified = cg_clientHUD->modified = true;
		cg.specStateChanged = false;
	}

	cvar_t *hud = ISREALSPECTATOR() ? cg_specHUD : cg_clientHUD;
	if( hud->modified )
	{
		CG_LoadStatusBar();
		hud->modified = false;
	}
}

/*
* CG_Draw2DView
*/
void CG_Draw2DView( void )
{
	bool drawScoreboard;

	if( !cg.view.draw2D )
		return;

	CG_SCRDrawViewBlend();

	// show when we are in "dead" chasecam
	if( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SPECDEAD )
	{
		int barheight = cgs.vidHeight * 0.08;
		trap_R_DrawStretchPic( 0, 0, cgs.vidWidth, barheight, 0, 0, 1, 1, colorBlack, cgs.shaderWhite );
		trap_R_DrawStretchPic( 0, cgs.vidHeight - barheight, cgs.vidWidth, barheight, 0, 0, 1, 1, colorBlack, cgs.shaderWhite );
	}

	if( cg.motd && ( cg.time > cg.motd_time ) )
	{
		CG_Free( cg.motd );
		cg.motd = NULL;
	}

	drawScoreboard = false;
	if( cgs.demoPlaying || cg.frame.multipov || cgs.tv )
	{
		if( cg.showScoreboard || GS_MatchState() > MATCH_STATE_PLAYTIME )
			drawScoreboard = true;
	}
	else
	{
		if( cg.frame.playerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD )
			drawScoreboard = true;
	}

	if( cg_showHUD->integer )
	{
		CG_CheckHUDChanges();
		CG_ExecuteLayoutProgram( cg.statusBar, false );
	}

	CG_UpdateHUDPostDraw();

	CG_CheckDamageCrosshair();

	if( drawScoreboard )
		CG_DrawScoreboard();

	scr_centertime_off -= cg.frameTime;
	if( !drawScoreboard && ( scr_centertime_off > 0 ) )
		CG_DrawCenterString();

	CG_DrawRSpeeds( cgs.vidWidth, cgs.vidHeight/2 + 8*cgs.vidHeight/600,
		ALIGN_RIGHT_TOP, cgs.fontSystemSmall, colorWhite );
}

/*
* CG_Draw2D
*/
void CG_Draw2D( void )
{
	if( !cg_draw2D->integer )
		return;

	CG_Draw2DView();
	CG_DrawDemocam2D();
}

/*
===============================================================================

TOUCH INPUT

===============================================================================
*/

cg_touch_t cg_touches[CG_MAX_TOUCHES];

typedef struct {
	int touch;
	int x, y;			// center position, may be modified
	int startx, starty;	// original center position
} cg_touchpad_t;

static cg_touchpad_t cg_touchpads[TOUCHPAD_COUNT];

/*
* CG_TouchArea
*
* Touches a rectangle. Returns touch id if it's a new touch.
*/
int CG_TouchArea( int area, int x, int y, int w, int h, void ( *upfunc )( int id, unsigned int time ) )
{
	if( ( w <= 0 ) || ( h <= 0 ) )
		return -1;

	int i;
	int x2 = x + w, y2 = y + h;

	// first check if already touched
	for( i = 0; i < CG_MAX_TOUCHES; ++i )
	{
		cg_touch_t &touch = cg_touches[i];

		if( touch.down && ( ( touch.area & TOUCHAREA_MASK ) == ( area & TOUCHAREA_MASK ) ) )
		{
			touch.area_valid = true;
			if( ( ( touch.area >> TOUCHAREA_SUB_SHIFT ) != ( area >> TOUCHAREA_SUB_SHIFT ) ) &&
				( touch.x >= x ) && ( touch.y >= y ) && ( touch.x < x2 ) && ( touch.y < y2 ) )
			{
				if( touch.upfunc )
					touch.upfunc( i, 0 );
				touch.area = area;
				return i;
			}
			return -1;
		}
	}

	// now add a new touch
	for( i = 0; i < CG_MAX_TOUCHES; ++i )
	{
		cg_touch_t &touch = cg_touches[i];

		if( touch.down && ( touch.area == TOUCHAREA_NONE ) &&
			( touch.x >= x ) && ( touch.y >= y ) && ( touch.x < x2 ) && ( touch.y < y2 ) )
		{
			touch.area = area;
			touch.area_valid = true;
			touch.upfunc = upfunc;
			return i;
		}
	}

	return -1;
}

/*
* CG_TouchEvent
*/
void CG_TouchEvent( int id, touchevent_t type, int x, int y, unsigned int time )
{
	if( id >= CG_MAX_TOUCHES )
		return;

	cg_touch_t &touch = cg_touches[id];

	switch( type )
	{
	case TOUCH_DOWN:
	case TOUCH_MOVE:
		touch.x = x;
		touch.y = y;
		if( !touch.down )
		{
			touch.down = true;
			touch.time = time;
			touch.area = TOUCHAREA_NONE;
		}
		break;

	case TOUCH_UP:
		if( touch.down )
		{
			touch.down = false;
			if( ( touch.area != TOUCHAREA_NONE ) && touch.upfunc )
				touch.upfunc( id, time );
		}
		break;
	}
}

/*
* CG_TouchFrame
*/
void CG_TouchFrame( void )
{
	int i;
	bool touching = false;

	for( i = 0; i < CG_MAX_TOUCHES; ++i )
	{
		cg_touches[i].area_valid = false;
		if( cg_touches[i].down )
			touching = true;
	}

	if( touching )
	{
		if( cg_showHUD->integer )
		{
			CG_CheckHUDChanges();
			CG_ExecuteLayoutProgram( cg.statusBar, true );
		}

		// cancel non-existent areas
		for( i = 0; i < CG_MAX_TOUCHES; ++i )
		{
			cg_touch_t &touch = cg_touches[i];
			if( touch.down )
			{
				if( ( touch.area != TOUCHAREA_NONE ) && !touch.area_valid )
				{
					if( touch.upfunc )
						touch.upfunc( i, 0 );
					touch.area = TOUCHAREA_NONE;
				}
			}
		}
	}

	CG_UpdateHUDPostTouch();
}

/*
* CG_TouchMove
*/
void CG_TouchMove( usercmd_t *cmd, vec3_t viewangles, int frametime )
{
	int buttons, upmove;
	CG_GetHUDTouchButtons( buttons, upmove );

	cmd->buttons |= buttons;

	if( frametime )
	{
		float scale = 600.0f / ( float )cgs.vidHeight;

		cg_touchpad_t &movepad = cg_touchpads[TOUCHPAD_MOVE];
		if( movepad.touch >= 0 )
		{
			if( cg_touch_moveThres->modified )
			{
				if( cg_touch_moveThres->value < 0.0f )
					trap_Cvar_Set( cg_touch_moveThres->name, cg_touch_moveThres->dvalue );
				cg_touch_moveThres->modified = false;
			}
			if( cg_touch_strafeThres->modified )
			{
				if( cg_touch_strafeThres->value < 0.0f )
					trap_Cvar_Set( cg_touch_strafeThres->name, cg_touch_strafeThres->dvalue );
				cg_touch_strafeThres->modified = false;
			}

			cg_touch_t &touch = cg_touches[movepad.touch];

			float move = movepad.y - ( float )touch.y;
			if( fabsf( move * scale ) > cg_touch_moveThres->value )
				cmd->forwardmove += ( move < 0 ) ? -frametime : frametime;

			move = ( float )touch.x - movepad.x;
			if( fabsf( move * scale ) > cg_touch_strafeThres->value )
				cmd->sidemove += ( move < 0 ) ? -frametime : frametime;
		}

		cg_touchpad_t &viewpad = cg_touchpads[TOUCHPAD_VIEW];
		if( viewpad.touch >= 0 )
		{
			if( cg_touch_lookThres->modified )
			{
				if( cg_touch_lookThres->value < 0.0f )
					trap_Cvar_Set( cg_touch_lookThres->name, cg_touch_lookThres->dvalue );
				cg_touch_lookThres->modified = false;
			}

			cg_touch_t &touch = cg_touches[viewpad.touch];

			float speed = cg_touch_lookSens->value * ( float )frametime * 0.001f;
			if( !cgs.demoPlaying && ( cg.predictedPlayerState.pmove.stats[PM_STAT_ZOOMTIME] > 0 ) )
				speed *= cg.predictedPlayerState.fov / cgs.clientInfo[cgs.playerNum].fov;

			float decel = cg_touch_lookDecel->value * ( float )frametime * 0.01f;

			float angle = ( ( float )touch.y - viewpad.y ) * scale;
			if( cg_touch_lookInvert->integer )
				angle = -angle;
			float dir = ( ( angle < 0.0f ) ? -1.0f : 1.0f );
			angle = fabsf( angle ) - cg_touch_lookThres->value;
			if( angle > 0.0f )
				viewangles[PITCH] += angle * dir * speed;
			viewpad.y += ( ( float )touch.y - viewpad.y ) * decel;

			angle = ( viewpad.x - ( float )touch.x ) * scale;
			dir = ( ( angle < 0.0f ) ? -1.0f : 1.0f );
			angle = fabsf( angle ) - cg_touch_lookThres->value;
			if( angle > 0.0f )
				viewangles[YAW] += angle * dir * speed;
			viewpad.x += ( ( float )touch.x - viewpad.x ) * decel;
		}

		cmd->upmove += upmove * frametime;
	}
}

/*
* CG_CancelTouches
*/
void CG_CancelTouches( void )
{
	int i;

	for( i = 0; i < CG_MAX_TOUCHES; ++i )
	{
		cg_touch_t &touch = cg_touches[i];
		if( touch.down )
		{
			if( touch.area != TOUCHAREA_NONE )
			{
				if( touch.upfunc )
					touch.upfunc( i, 0 );
				touch.area = TOUCHAREA_NONE;
			}
			touch.down = false;
		}
	}
}

/*
* CG_GetTouchpadOffset
*/
bool CG_GetTouchpadOffset( int padID, float &x, float &y, bool fromStart )
{
	cg_touchpad_t &pad = cg_touchpads[padID];
	if( pad.touch < 0 )
		return false;

	cg_touch_t &touch = cg_touches[pad.touch];
	float scale = 600.0f / ( float )cgs.vidHeight;
	if( fromStart )
	{
		x = ( touch.x - pad.startx ) * scale;
		y = ( touch.y - pad.starty ) * scale;
	}
	else
	{
		x = ( touch.x - pad.x ) * scale;
		y = ( touch.y - pad.y ) * scale;
	}
	return true;
}

/*
* CG_SetTouchpad
*/
void CG_SetTouchpad( int padID, int touchID )
{
	cg_touchpad_t &pad = cg_touchpads[padID];

	pad.touch = touchID;

	if( touchID >= 0 )
	{
		cg_touch_t &touch = cg_touches[touchID];
		pad.x = pad.startx = touch.x;
		pad.y = pad.starty = touch.y;
	}
}
