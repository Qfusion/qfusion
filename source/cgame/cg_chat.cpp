/*
Copyright (C) 2010 Victor Luchits

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

#include "cg_local.h"

cvar_t *con_chatCGame;

/*
** CG_InitChat
*/
void CG_InitChat( cg_gamechat_t *chat )
{
	con_chatCGame = trap_Cvar_Get( "con_chatCGame", "0", CVAR_READONLY );
	trap_Cvar_ForceSet( con_chatCGame->name, "0" );

	memset( chat, 0, sizeof( *chat ) );
}

/*
** CG_StackChatString
*/
void CG_StackChatString( cg_gamechat_t *chat, const char *str )
{
	chat->messages[chat->nextMsg].time = cg.realTime;
	Q_strncpyz( chat->messages[chat->nextMsg].text, str, sizeof( chat->messages[0].text ) );

	chat->lastMsgTime = cg.realTime;
	chat->nextMsg = (chat->nextMsg + 1) % GAMECHAT_STACK_SIZE;
}

#define WHITEPSACE_CHAR(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')

/*
** CG_ColorStrLastColor
*
* Copied from console.c
*/
static void CG_ColorStrLastColor( int *lastcolor, const char *s, int byteofs )
{
	char c;
	int colorindex;
	const char *end = s + byteofs;

	while( s < end )
	{
		int gc = Q_GrabCharFromColorString( &s, &c, &colorindex );
		if( gc == GRABCHAR_CHAR )
			;
		else if( gc == GRABCHAR_COLOR )
			*lastcolor = colorindex;
		else if( gc == GRABCHAR_END )
			break;
		else
			assert( 0 );
	}
}

/*
** CG_SetChatCvars
*/
static void CG_SetChatCvars( int x, int y, char *fontName, int font_height, int width, int height, int padding_x, int padding_y )
{
	char tstr[32];

	trap_Cvar_ForceSet( "con_chatCGame", "1" );
	trap_Cvar_ForceSet( "con_chatFont",  fontName );

	Q_snprintfz( tstr, sizeof( tstr ), "%i", x + padding_x );
	trap_Cvar_ForceSet( "con_chatX",  tstr );

	Q_snprintfz( tstr, sizeof( tstr ), "%i", y + height - padding_y - font_height );
	trap_Cvar_ForceSet( "con_chatY",  tstr );

	Q_snprintfz( tstr, sizeof( tstr ), "%i", width - padding_x );
	trap_Cvar_ForceSet( "con_chatWidth",  tstr );
}

#define GAMECHAT_NOTIFY_TIME		3000
#define GAMECHAT_WAIT_IN_TIME		0
#define GAMECHAT_FADE_IN_TIME		100
#define GAMECHAT_WAIT_OUT_TIME		2000
#define GAMECHAT_FADE_OUT_TIME		(GAMECHAT_NOTIFY_TIME-GAMECHAT_WAIT_OUT_TIME)

/*
** CG_DrawChat
*/
void CG_DrawChat( cg_gamechat_t *chat, int x, int y, char *fontName, struct qfontface_s *font, 
				 int width, int height, int padding_x, int padding_y, vec4_t backColor, struct shader_s *backShader )
{
	int i, j;
	int s, e, w;
	int l, total_lines, lines;
	int x_offset, y_offset;
	int str_width;
	int font_height;
	int pass;
	int lastcolor;
	int message_mode;
	int wait_time, fade_time;
	const cg_gamemessage_t *msg;
	const char *text;
	char tstr[GAMECHAT_STRING_SIZE];
	vec4_t fontColor;
	bool chat_active = false;
	bool background_drawn = false;

	font_height = trap_SCR_strHeight( font );
	message_mode = (int)trap_Cvar_Value( "con_messageMode" );
	chat_active = ( chat->lastMsgTime + GAMECHAT_WAIT_IN_TIME + GAMECHAT_FADE_IN_TIME > cg.realTime || message_mode );
	lines = 0;
	total_lines = /*!message_mode ? 0 : */1;

	if( chat_active )
	{
		wait_time = GAMECHAT_WAIT_IN_TIME;
		fade_time = GAMECHAT_FADE_IN_TIME;
	}
	else
	{
		wait_time = GAMECHAT_WAIT_OUT_TIME;
		fade_time = GAMECHAT_FADE_OUT_TIME;
	}

	if( chat_active != chat->lastActive )
	{
		// smooth fade ins and fade outs
		chat->lastActiveChangeTime = cg.realTime - (1.0 - chat->activeFrac) * (wait_time + fade_time);
	}

	if( cg.realTime >= chat->lastActiveChangeTime + wait_time )
	{
		int time_diff, time_interval;

		time_diff = cg.realTime - (chat->lastActiveChangeTime + wait_time);
		time_interval = fade_time;

		if( time_diff <= time_interval )
			chat->activeFrac = (float)time_diff / time_interval;
		else
			chat->activeFrac = 1;
	}
	else
	{
		chat->activeFrac = 0;
	}

	if( chat_active )
		backColor[3] *= chat->activeFrac;
	else
		backColor[3] *= (1.0 - chat->activeFrac);

	// let the engine know where the input line should be drawn
	CG_SetChatCvars( x, y, fontName, font_height, width, height, padding_x, padding_y );

	for( i = 0; i < GAMECHAT_STACK_SIZE; i++ )
	{
		bool old_msg;

		l = chat->nextMsg - 1 - i;
		if( l < 0 )
			l = GAMECHAT_STACK_SIZE + l;

		msg = &chat->messages[l];
		text = msg->text;
		old_msg = !message_mode && ( cg.realTime > msg->time + GAMECHAT_NOTIFY_TIME );

		if( !background_drawn && backColor[3] )
		{
			if( old_msg )
			{
				// keep the box being drawn for a while to prevent it from flickering
				// upon arrival of the possibly entered chat message
				if( !(!chat_active && cg.realTime <= chat->lastActiveChangeTime + 200) )
					break;
			}

			trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, backColor, backShader );

			background_drawn = true;
		}

		// unless user is typing something, only display recent messages
		if( old_msg )
			break;

		pass = 0;
		lines = 0;
		lastcolor = ColorIndex( COLOR_WHITE );

parse_string:
		l = 1;
		s = e = 0;
		while( 1 )
		{
			memset( tstr, 0, sizeof( tstr ) );

			// skip whitespaces at start
			for( ; WHITEPSACE_CHAR( text[s] ); s++ );

			// empty string
			if( !text[s] )
				break;

			w = -1;
			j = s; // start
			for( ; text[j] != '\0'; j++ )
			{
				tstr[j-s] = text[j];
				str_width = trap_SCR_strWidth( tstr, font, 0 );

				if( WHITEPSACE_CHAR( text[j] ) )
					w = j; // last whitespace

				if( text[j] == '\n' || str_width > width - padding_x )
					break;
			}
			e = j; // end

			// try to word avoid splitting words, unless no other options
			if( text[j] != '\0' && w > 0 )
			{
				// stop at the last encountered whitespace
				j = w;
			}

			tstr[j-s] = '\0';

			Vector4Copy( color_table[lastcolor], fontColor );
			fontColor[3] = chat_active ? chat->activeFrac : 1.0 - chat->activeFrac;

			if( pass )
			{
				// now actually render the line
				x_offset = padding_x;
				y_offset = height - padding_y - font_height - (total_lines + lines - l) * (font_height + 2);
				if( y_offset <= -font_height )
					break;

				trap_SCR_DrawClampString( x + x_offset, y + y_offset, tstr,
					x + padding_x, y + padding_y, x - padding_x + width, y - padding_y + height, font, fontColor );

				l++;
			}
			else
			{
				// increase the lines counter
				lines++;
			}

			if( !text[j] )
			{
				// fast path: we don't need two passes in case of one-liners..
				if( lines == 1 )
				{
					x_offset = padding_x;
					y_offset = height - font_height - total_lines * (font_height + 2);
					if( y_offset <= -font_height )
						break;

					trap_SCR_DrawClampString( x + x_offset, y + y_offset, tstr,
						x + padding_x, y + padding_y, x - padding_x + width, y - padding_y + height, font, fontColor );

					total_lines++;
					pass++;
				}
				break;
			}

			if( pass )
			{
				// grab the last color token to carry it over to the next line
				CG_ColorStrLastColor( &lastcolor, tstr, j - s );
			}

			s = j;
		}

		if( !pass )
		{
			pass++;
			goto parse_string;
		}
		else
		{
			total_lines += lines;
		}
	}

	chat->lastActive = chat_active;
}
