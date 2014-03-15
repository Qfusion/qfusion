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

// cg_scoreboard.c -- scoreboard layouts for gametypes

#include "cg_local.h"

extern cvar_t *cg_scoreboardStats;
extern cvar_t *cg_scoreboardFontFamily;
extern cvar_t *cg_scoreboardFontSize;
extern cvar_t *cg_scoreboardWidthScale;

#define SCB_BACKGROUND_ALPHA 0.25f

#define SCB_TEAMNAME_PIXELWIDTH ( 260 * cg_scoreboardWidthScale->value )
#define SCB_SMALLFIELD_PIXELWIDTH ( 40 * cg_scoreboardWidthScale->value )
#define SCB_TINYFIELD_PIXELWIDTH ( 26 * cg_scoreboardWidthScale->value )

#define SCB_SCORENUMBER_SIZE 48
#define SCB_CENTERMARGIN 16

void CG_DrawHUDNumeric( int x, int y, int align, float *color, int charwidth, int charheight, int value );

/*
* CG_DrawAlignPic
*/
static void CG_DrawAlignPic( int x, int y, int width, int height, int align, const vec4_t color, struct shader_s *shader )
{
	x = CG_HorizontalAlignForWidth( x, align, width );
	y = CG_VerticalAlignForHeight( y, align, height );

	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, color, shader );
}

/*
* CG_PingColor
*/
static void CG_PingColor( int ping, vec4_t color )
{
	if( ping < 70 )
		Vector4Copy( colorGreen, color );
	else if( ping < 100 )
		Vector4Copy( colorYellow, color );
	else if( ping < 150 )
		Vector4Copy( colorOrange, color );
	else
		Vector4Copy( colorRed, color );
}

// ====================================================
// player stats
// ====================================================

static int scb_player_stats[2*( WEAP_TOTAL-WEAP_GUNBLADE )]; // weak strong

/*
* SCB_ParsePlayerStats
*/
static void SCB_ParsePlayerStats( const char **s )
{
	int i, j, weak, strong;
	int shot_weak, hit_weak, shot_strong, hit_strong, hit_total, shot_total;
	unsigned int playerNum;

	if( !s || !*s )
		return;

	playerNum = CG_ParseValue( s );
	if( cg.frame.playerState.POVnum != playerNum + 1 )
		return;

	memset( scb_player_stats, -1, sizeof( scb_player_stats ) );
	j = 0;

#define STATS_PERCENT(hit,total) ((hit) > 0 ? ((hit) == (total) ? 100 : (min( (int)( floor( ( 100.0f*(hit) ) / ( (float)(total) ) + 0.5f ) ), 99 ))) : -1)

	for( i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ )
	{
		weak = j++;
		strong = j++;

		// total
		shot_total = CG_ParseValue( s );
		if( shot_total == 0 )
			continue;
		hit_total = CG_ParseValue( s );

		shot_strong = shot_total;
		hit_strong = hit_total;
		if( i == WEAP_LASERGUN || i == WEAP_ELECTROBOLT )
		{	// strong
			shot_strong = CG_ParseValue( s );
			if( shot_strong != shot_total )
				hit_strong = CG_ParseValue( s );
		}

		// weak
		shot_weak = shot_total - shot_strong;
		hit_weak = hit_total - hit_strong;

		scb_player_stats[weak] = STATS_PERCENT(hit_weak,shot_weak);
		scb_player_stats[strong] = STATS_PERCENT(hit_strong,shot_strong);
	}

#undef STATS_PERCENT
}

/*
* SCB_DrawPlayerStats
*/
static int SCB_DrawPlayerStats( int x, int y, struct qfontface_s *font )
{
	int xoffset, yoffset, lines;
	int i, j, num_weapons, weap, xpos, width, done;
	gsitem_t *it;
	char string[MAX_STRING_CHARS];
	vec4_t color = { 0.5, 0.5, 0.5, 0.5f };

	// don't display stats
	if( !cg_scoreboardStats->integer )
		return 0;

	// total number of weapon
	num_weapons = WEAP_TOTAL-WEAP_GUNBLADE;

	width = ( SCB_TINYFIELD_PIXELWIDTH + 2 * SCB_SMALLFIELD_PIXELWIDTH ) * 2 + SCB_SMALLFIELD_PIXELWIDTH;

	xpos = -8 * SCB_TINYFIELD_PIXELWIDTH/2;

	// Center the box
	xoffset = xpos;
	yoffset = trap_SCR_strHeight( font );

	// Room for header, it's actually written later if we have at least one stat
	yoffset += trap_SCR_strHeight( font );

	lines = 0;
	for( i = 0; i < num_weapons; )
	{
		xoffset = xpos;

		// two weapons per line
		for( j = 0, done = 0; done < 2 && i + j < num_weapons; j++ )
		{
			weap = WEAP_GUNBLADE + i + j;

			if( scb_player_stats[2*( i+j )] == -1 && scb_player_stats[2*( i+j )+1] == -1 )
				continue;

			it = GS_FindItemByTag( weap );

			// short name
			Q_snprintfz( string, sizeof( string ), "%s%2s", it->color, it->shortname );
			trap_SCR_DrawStringWidth( x + xoffset, y + yoffset, ALIGN_LEFT_TOP, string, SCB_TINYFIELD_PIXELWIDTH, font, colorWhite );
			xoffset += SCB_TINYFIELD_PIXELWIDTH;

			Q_snprintfz( string, sizeof( string ), "%2d%c", scb_player_stats[2*( i+j )+1], '%' );
			trap_SCR_DrawStringWidth( x + xoffset + SCB_SMALLFIELD_PIXELWIDTH, y + yoffset, ALIGN_CENTER_TOP, string, 2*SCB_SMALLFIELD_PIXELWIDTH, font, colorWhite );
			xoffset += 2*SCB_SMALLFIELD_PIXELWIDTH;

			// separator
			xoffset += SCB_SMALLFIELD_PIXELWIDTH;
			done++;
		}

		// next line
		if( done > 0 )
		{
			lines++;
			yoffset += trap_SCR_strHeight( font );
		}

		i += j;
	}

	if( lines )
	{
		// if we drew anything, draw header and box too
		xoffset = xpos;
		yoffset = trap_SCR_strHeight( font );

		// header
		trap_SCR_DrawStringWidth( x + xoffset, y + yoffset, ALIGN_LEFT_TOP, 
			CG_TranslateString( "Weapon stats" ), width, font, colorMdGrey );
		yoffset += trap_SCR_strHeight( font );

		// box
		trap_R_DrawStretchPic( x + xoffset - SCB_TINYFIELD_PIXELWIDTH/2, y + yoffset, width + SCB_TINYFIELD_PIXELWIDTH,
			lines * trap_SCR_strHeight( font ), 0, 0, 1, 1, color, cgs.shaderWhite );

		return ( trap_SCR_strHeight( font ) * ( 2+lines ) );
	}

	return 0;
}

// ====================================================
// player scoreboards
// ====================================================

static char scoreboardString[MAX_STRING_CHARS];

/*
* SCR_DrawChallengers
*/
static int SCR_DrawChallengers( const char **ptrptr, int x, int y, int panelWidth, struct qfontface_s *font )
{
	char *token;
	const char *oldptr;
	char string[MAX_STRING_CHARS];
	int yoffset = 0, xoffset = 0;
	int playerNum, ping;
	int height;

	assert( ptrptr && *ptrptr );

	height = trap_SCR_strHeight( font );

	// draw title
	yoffset = height;
	trap_SCR_DrawString( x + xoffset, y + yoffset, ALIGN_CENTER_TOP, 
		CG_TranslateString( "Challengers" ), font, colorCyan );
	yoffset += height;

	// draw challengers
	while( *ptrptr )
	{
		oldptr = *ptrptr;
		token = COM_ParseExt( ptrptr, qtrue );
		if( !token[0] )
			break;

		if( token[0] == '&' ) // it's a different command than 'challengers', so step back and return
		{
			*ptrptr = oldptr;
			break;
		}

		// first token is played id
		playerNum = atoi( token );
		if( playerNum < 0 || playerNum >= gs.maxclients )
			break;

		// get a second token
		oldptr = *ptrptr;
		token = COM_ParseExt( ptrptr, qtrue );
		if( !token[0] )
			break;

		if( token[0] == '&' ) // it's a different command than 'challengers', so step back and return
		{
			*ptrptr = oldptr;
			break;
		}

		// second token is ping
		ping = atoi( token );

		// draw the challenger
		if( ping < 0 )
			Q_snprintfz( string, sizeof( string ), "%s%s ...", cgs.clientInfo[playerNum].name, S_COLOR_WHITE );
		else
			Q_snprintfz( string, sizeof( string ), "%s%s %i", cgs.clientInfo[playerNum].name, S_COLOR_WHITE, ping );

		trap_SCR_DrawString( x + xoffset, y + yoffset, ALIGN_CENTER_TOP, string, font, colorWhite );
		yoffset += height;
	}

	yoffset += height;
	return yoffset;
}

/*
* SCR_DrawSpectators
*/
static int SCR_DrawSpectators( const char **ptrptr, int x, int y, int panelWidth, struct qfontface_s *font )
{
	char *token;
	const char *oldptr;
	char string[MAX_STRING_CHARS];
	int yoffset = 0, xoffset = 0;
	int playerNum, ping;
	int aligns[3], offsets[3];
	int colwidth, fullwidth, count = 0, height;

	fullwidth = panelWidth * 1.5;
	if( fullwidth > cgs.vidWidth * 0.7 )
		fullwidth = cgs.vidWidth * 0.7;
	colwidth = fullwidth / 3;

	aligns[0] = ALIGN_CENTER_TOP;
	aligns[1] = ALIGN_LEFT_TOP;
	aligns[2] = ALIGN_RIGHT_TOP;

	offsets[0] = 0;
	offsets[1] = -fullwidth * 0.5;
	offsets[2] = fullwidth * 0.5;

	assert( ptrptr && *ptrptr );

	height = trap_SCR_strHeight( font );

	// draw title
	yoffset = height;
	trap_SCR_DrawString( x + xoffset, y + yoffset, ALIGN_CENTER_TOP, 
		CG_TranslateString( "Spectators" ), font, colorYellow );
	yoffset += height;

	// draw spectators
	while( *ptrptr )
	{
		oldptr = *ptrptr;
		token = COM_ParseExt( ptrptr, qtrue );
		if( !token[0] )
			break;

		if( token[0] == '&' ) // it's a different command than 'spectators', so step back and return
		{
			*ptrptr = oldptr;
			break;
		}

		// first token is played id
		playerNum = atoi( token );
		if( playerNum < 0 || playerNum >= gs.maxclients )
			break;

		// get a second token
		oldptr = *ptrptr;
		token = COM_ParseExt( ptrptr, qtrue );
		if( !token[0] )
			break;

		if( token[0] == '&' ) // it's a different command than 'spectators', so step back and return
		{
			*ptrptr = oldptr;
			break;
		}

		// second token is ping
		ping = atoi( token );

		// draw the spectator
		if( ping < 0 )
			Q_snprintfz( string, sizeof( string ), "%s%s ...", cgs.clientInfo[playerNum].name, S_COLOR_WHITE );
		else
			Q_snprintfz( string, sizeof( string ), "%s%s %i", cgs.clientInfo[playerNum].name, S_COLOR_WHITE, ping );

		xoffset = offsets[count] + CG_HorizontalAlignForWidth( 0, aligns[count], trap_SCR_strWidth( string, font, 0 ) );

		// fixme: the boxes aren't actually correctly aligned
		trap_SCR_DrawClampString( x + xoffset, y + yoffset, string, x + xoffset, y + yoffset, x + xoffset + colwidth, y + yoffset + height, font, colorWhite );

		count++;
		if( count > 2 )
		{
			count = 0;
			yoffset += height;
		}
	}

	if( count )
		yoffset += height;
	return yoffset;
}

/*
* SCR_GetNextColumnLayout
*/
static const char *SCR_GetNextColumnLayout( const char **ptrlay, const char **ptrtitle, char *type, int *width )
{
	static const char *empty = "";
	const char *token;

	assert( ptrlay && *ptrlay );

	// get the token type from the layout
	token = COM_ParseExt( ptrlay, qtrue );
	if( !token[0] )
		return NULL;

	if( token[0] != '%' )
		CG_Error( "SCR_GetNextColumnLayout: Invalid player tab layout (expecting token type. found '%s')\n", token );

	if( type )
		*type = token[1];

	// get the column width from the layout
	token = COM_ParseExt( ptrlay, qtrue );
	if( !token[0] || token[0] == '%' )
		CG_Error( "SCR_GetNextColumnLayout: Invalid player tab layout (expecting token width. found '%s')\n", token );

	if( width )
	{
		*width = ( atoi( token ) * cg_scoreboardWidthScale->value );

		if( *width < 0 )
			*width = 0;
	}

	if( ptrtitle && *ptrtitle )
	{
		// get the column title token from the layout
		token = COM_ParseExt( ptrtitle, qtrue );
		if( !token[0] )
			CG_Error( "SCR_GetNextColumnLayout: Invalid player tab layout (expecting token tittle. found '%s')\n", token );
	}
	else
	{
		token = empty;
	}

	return token;
}

/*
* SCR_DrawTeamTab
*/
static int SCR_DrawTeamTab( const char **ptrptr, int *curteam, int x, int y, int panelWidth, struct qfontface_s *font )
{
	const char *token;
	const char *layout, *titles;
	int team, team_score, team_ping;
	int yoffset = 0, xoffset = 0;
	int dir = 0, align, width, height;
	vec4_t teamcolor, pingcolor;

	// team tab is always the same. Sets the current team and draws its score

	if( !(*ptrptr) || !(*ptrptr[0]) || *ptrptr[0] == '&' )
		return yoffset;

	team = CG_ParseValue( ptrptr );
	if( team < TEAM_PLAYERS || team > TEAM_BETA )
		CG_Error( "SCR_ParseTeamTab: Invalid team value\n" );

	*curteam = team;

	if( *ptrptr[0] == '&' )
		return yoffset;

	team_score = CG_ParseValue( ptrptr );

	if( *ptrptr[0] == '&' )
		return yoffset;

	team_ping = CG_ParseValue( ptrptr );

	CG_TeamColor( team, teamcolor );
	teamcolor[3] = SCB_BACKGROUND_ALPHA; // make transparent

	if( GS_TeamBasedGametype() ) // we only draw the team tabs in team based gametypes
	{
		dir = ( team == TEAM_ALPHA ) ? -1 : 1;
		align = ( team == TEAM_ALPHA ) ? ALIGN_RIGHT_TOP : ALIGN_LEFT_TOP;

		// draw the tab

		xoffset = ( SCB_CENTERMARGIN * dir );

		width = ( cgs.vidWidth * 0.5 ) - SCB_CENTERMARGIN;
		height = trap_SCR_strHeight( cgs.fontSystemBig ) + 2;

		CG_DrawAlignPic( x + xoffset, y + yoffset + SCB_SCORENUMBER_SIZE - height,
			width, height, align, teamcolor, cgs.shaderWhite );

		xoffset += ( 16 * dir );
		CG_DrawHUDNumeric( x + xoffset, y + yoffset, align, colorWhite, 
			SCB_SCORENUMBER_SIZE, SCB_SCORENUMBER_SIZE, team_score );

		xoffset += ( ( SCB_SCORENUMBER_SIZE * strlen(va("%i", team_score)) + 16 ) * dir );
		CG_PingColor( team_ping, pingcolor );
		trap_SCR_DrawStringWidth( x + xoffset, y + yoffset + SCB_SCORENUMBER_SIZE - ( trap_SCR_strHeight( font ) + 1 ),
			align, va( "%i", team_ping ), SCB_TINYFIELD_PIXELWIDTH, font, pingcolor );

		xoffset += ( ( SCB_TINYFIELD_PIXELWIDTH + 16 ) * dir );
		trap_SCR_DrawStringWidth( x + xoffset, y + yoffset + SCB_SCORENUMBER_SIZE - ( trap_SCR_strHeight( cgs.fontSystemBig ) + 1 ),
			align, GS_TeamName( team ), SCB_TEAMNAME_PIXELWIDTH, cgs.fontSystemBig, colorWhite );

		yoffset += SCB_SCORENUMBER_SIZE;
	}
	else
	{
		dir = 0;
		align = ALIGN_CENTER_TOP;
	}

	// draw the player tab column titles
	layout = cgs.configStrings[CS_SCB_PLAYERTAB_LAYOUT];
	titles = cgs.configStrings[CS_SCB_PLAYERTAB_TITLES];

	height = trap_SCR_strHeight( font );

	// start from the center again
	xoffset = CG_HorizontalAlignForWidth( 0, align, panelWidth );
	xoffset += ( SCB_CENTERMARGIN * dir );

	while( ( token = SCR_GetNextColumnLayout( &layout, &titles, NULL, &width ) ) != NULL )
	{
		if( width )
		{
			trap_SCR_DrawClampString( x + xoffset, y + yoffset, CG_TranslateString( token ),
				x + xoffset, y + yoffset, x + xoffset + width, y + yoffset + height, font, colorWhite );
			xoffset += width;
		}
	}

	yoffset += trap_SCR_strHeight( font );

	return yoffset;
}

/*
* SCR_DrawPlayerTab
*/
static int SCR_DrawPlayerTab( const char **ptrptr, int team, int x, int y, int panelWidth, struct qfontface_s *font )
{
	int dir, align, i, columncount;
	char type, string[MAX_STRING_CHARS];
	const char *oldptr;
	char *token, *layout;
	int height, width, xoffset, yoffset;
	vec4_t teamcolor, color;
	struct shader_s *shader;
	bool highlight = false, trans = false;

	if( GS_TeamBasedGametype() )
	{
		dir = ( team == TEAM_ALPHA ) ? -1 : 1;
		align = ( team == TEAM_ALPHA ) ? ALIGN_RIGHT_TOP : ALIGN_LEFT_TOP;
	}
	else
	{
		dir = 0;
		align = ALIGN_CENTER_TOP;
	}

	xoffset = 0;
	yoffset = 0;

	height = trap_SCR_strHeight( font );

	// start from the center again
	xoffset = CG_HorizontalAlignForWidth( 0, align, panelWidth );
	xoffset += ( SCB_CENTERMARGIN * dir );

	// draw the background
	columncount = 0;
	CG_TeamColor( team, teamcolor );

	// draw the player tab column titles
	layout = cgs.configStrings[CS_SCB_PLAYERTAB_LAYOUT];

	while( SCR_GetNextColumnLayout( (const char **)&layout, NULL, &type, &width ) != NULL )
	{
		// grab the actual scoreboard data

		oldptr = *ptrptr; // in case we need to revert
		token = COM_ParseExt( ptrptr, qtrue );
		if( token[0] == '&' )
		{
			*ptrptr = oldptr; // failed, but revert so it can continue with the next player
			break;
		}

		if( !token[0] )
			break;

		Vector4Copy( colorWhite, color ); // reset to white after each column
		shader = NULL;
		string[0] = 0;

		// interpret the data based on the type defined in the layout
		switch( type )
		{
		default:
			CG_Error( "SCR_DrawPlayerTab: Invalid player tab layout\n" );
			break;

		case 's': // is a string
			Q_strncpyz( string, token, sizeof( string ) );
			break;

		case 'n': // is a player name indicated by player number
			i = atoi( token );

			if( i < 0 ) // negative numbers toggle transparency on
			{
				trans = true;
				i = abs( i ) - 1;
			}

			if( i < 0 || i >= gs.maxclients )
				Q_strncpyz( string, "invalid", sizeof( string ) );
			else
				Q_strncpyz( string, cgs.clientInfo[i].name, sizeof( string ) );

			if( ISVIEWERENTITY( i + 1 ) ) // highlight if it's our own player
				highlight = true;

			break;

		case 'i': // is a integer (negatives are colored in red)
			i = atoi( token );
			Q_snprintfz( string, sizeof( string ), "%i", i );
			VectorCopy( i >= 0 ? colorWhite : colorRed, color );
			break;

		case 'f': // is a float
			Q_snprintfz( string, sizeof( string ), "%.2f", atof( token ) );
			break;

		case 'l': // p is an integer colored in latency style
			i = atoi( token );
			Q_snprintfz( string, sizeof( string ), "%i", i );
			CG_PingColor( i, color );
			break;

		case 'b': // is a Y/N boolean
			i = atoi( token );
			Q_snprintfz( string, sizeof( string ), "%s", CG_TranslateString( ( i != 0 ) ? "Yes" : "No" ) );
			VectorCopy( i ? colorGreen : colorRed, color );
			break;

		case 'p': // is a picture. It uses height for width to get a square
			i = atoi( token );
			if( i )
				shader = cgs.imagePrecache[i];
			break;

		case 't': // is a race time. Convert time into MM:SS:mm
			{
				unsigned int milli, min, sec;

				milli = (unsigned int)( atoi( token ) );
				if( !milli )
					Q_snprintfz( string, sizeof( string ), CG_TranslateString( "no time" ) );
				else
				{
					min = milli / 60000;
					milli -= min * 60000;
					sec = milli / 1000;
					milli -= sec * 1000;
					Q_snprintfz( string, sizeof( string ), va( "%02i:%02i.%03i", min, sec, milli ) );
				}
			}
			break;
		}

		if( !width )
			continue;

		// draw the column background
		teamcolor[3] = SCB_BACKGROUND_ALPHA;
		if( columncount & 1 )
			teamcolor[3] -= 0.15;

		if( highlight )
			teamcolor[3] += 0.3;

		if( trans )
			color[3] = 0.3;

		trap_R_DrawStretchPic( x + xoffset, y + yoffset, width, height, 0, 0, 1, 1, teamcolor, cgs.shaderWhite );

		// draw the column value
		if( string[0] )
			trap_SCR_DrawClampString( x + xoffset, y + yoffset, string,
			x + xoffset, y + yoffset, x + xoffset + width, y + yoffset + height, font, color );

		if( shader )
			trap_R_DrawStretchPic( x + xoffset, y + yoffset, height, height, 0, 0, 1, 1, color, shader );

		columncount++;

		xoffset += width;
	}

	yoffset += height;
	return yoffset;
}

/*
* CG_ScoreboardFont
*/
struct qfontface_s *CG_ScoreboardFont( cvar_t *familyCvar )
{
	struct qfontface_s *font;

	font = trap_SCR_RegisterFont( familyCvar->string, QFONT_STYLE_NONE, cg_scoreboardFontSize->integer );
	if( !font )
	{
		CG_Printf( "%sWarning: Invalid font in '%s'. Reseting to default\n", familyCvar->name, S_COLOR_YELLOW );
		trap_Cvar_Set( familyCvar->name, familyCvar->dvalue );
		trap_Cvar_Set( cg_scoreboardFontSize->name, cg_scoreboardFontSize->dvalue );
		font = trap_SCR_RegisterFont( familyCvar->string, QFONT_STYLE_NONE, cg_scoreboardFontSize->integer );

		if( !font )
			CG_Error( "Couldn't load default scoreboard font \"%s\"", familyCvar->value );
	}
	return font;
}

/*
* CG_DrawScoreboard
*/
void CG_DrawScoreboard( void )
{
	char *ptr, *token, *layout, title[MAX_STRING_CHARS];
	int team = TEAM_PLAYERS;
	int xpos;
	int ypos, yoffset, maxyoffset;
	struct qfontface_s *font;
	struct qfontface_s *monofont;
	int width, panelWidth;
	vec4_t whiteTransparent = { 1.0f, 1.0f, 1.0f, 0.5f };

	// no layout defined
	if( !cgs.configStrings[CS_SCB_PLAYERTAB_LAYOUT][0] )
		return;

	if( scoreboardString[0] != '&' ) // nothing to draw
		return;

	font = CG_ScoreboardFont( cg_scoreboardFontFamily );
	monofont = CG_ScoreboardFont( cg_scoreboardMonoFontFamily );

	xpos = (int)( cgs.vidWidth * 0.5 );
	ypos = (int)( cgs.vidHeight * 0.25 ) - 24;

	// draw title
	Q_snprintfz( title, sizeof( title ), va( "%s %s", trap_Cvar_String( "gamename" ), gs.gametypeName ) );
	Q_strupr( title );

	trap_SCR_DrawString( xpos, ypos, ALIGN_CENTER_TOP, title, cgs.fontSystemBig, whiteTransparent );
	ypos += trap_SCR_strHeight( cgs.fontSystemBig );
	trap_SCR_DrawStringWidth( xpos, ypos, ALIGN_CENTER_TOP, cgs.configStrings[CS_HOSTNAME], cgs.vidWidth*0.75, cgs.fontSystemSmall, whiteTransparent );
	ypos += trap_SCR_strHeight( cgs.fontSystemSmall );

	// calculate the panel width from the layout
	panelWidth = 0;
	layout = cgs.configStrings[CS_SCB_PLAYERTAB_LAYOUT];
	while( SCR_GetNextColumnLayout( (const char **)&layout, NULL, NULL, &width ) != NULL )
		panelWidth += width;

	// parse and draw the scoreboard message
	yoffset = 0;
	maxyoffset = 0;
	ptr = scoreboardString;
	while( ptr )
	{
		token = COM_ParseExt( &ptr, qtrue );
		if( token[0] != '&' )
			break;

		if( !Q_stricmp( token, "&t" ) ) // team tab
		{
			yoffset = 0;
			yoffset += SCR_DrawTeamTab( (const char **)&ptr, &team, xpos, ypos + yoffset, panelWidth, font );
		}
		else if( !Q_stricmp( token, "&p" ) ) // player tab
		{
			yoffset += SCR_DrawPlayerTab( (const char **)&ptr, team, xpos, ypos + yoffset, panelWidth, font );
		}
		else if( !Q_stricmp( token, "&w" ) ) // list of challengers
		{
			if( yoffset < maxyoffset )
				yoffset = maxyoffset;

			maxyoffset += SCR_DrawChallengers( (const char **)&ptr, xpos, ypos + yoffset, panelWidth, font );
		}
		else if( !Q_stricmp( token, "&s" ) ) // list of spectators
		{
			if( yoffset < maxyoffset )
				yoffset = maxyoffset;

			maxyoffset += SCR_DrawSpectators( (const char **)&ptr, xpos, ypos + yoffset, panelWidth, font );
		}

		if( yoffset > maxyoffset )
			maxyoffset = yoffset;
	}

	// add the player stats
	yoffset = maxyoffset + trap_SCR_strHeight( font );
	yoffset += SCB_DrawPlayerStats( xpos, ypos + yoffset, monofont );
}



/*
* SCR_UpdateScoreboardMessage
*/
void SCR_UpdateScoreboardMessage( const char *string )
{
	Q_strncpyz( scoreboardString, string, sizeof( scoreboardString ) );
}

/*
* SCR_UpdatePlayerStatsMessage
*/
void SCR_UpdatePlayerStatsMessage( const char *string )
{
	SCB_ParsePlayerStats( &string );
}

/*
* CG_ToggleScores_f
*/
void CG_ToggleScores_f( void )
{
	if( cgs.demoPlaying || cg.frame.multipov || cgs.tv )
		cg.showScoreboard = !cg.showScoreboard;
	else
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore" );
}

/*
* CG_ScoresOn_f
*/
void CG_ScoresOn_f( void )
{
	if( cgs.demoPlaying || cg.frame.multipov || cgs.tv )
		cg.showScoreboard = true;
	else
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore 1" );
}

/*
* CG_ScoresOff_f
*/
void CG_ScoresOff_f( void )
{
	if( cgs.demoPlaying || cg.frame.multipov || cgs.tv )
		cg.showScoreboard = false;
	else
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore 0" );
}
