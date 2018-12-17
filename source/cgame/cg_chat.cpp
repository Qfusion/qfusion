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

/*
** CG_InitChat
*/
void CG_InitChat( cg_gamechat_t *chat ) {
	memset( chat, 0, sizeof( *chat ) );
}

/*
** CG_StackChatString
*/
void CG_StackChatString( cg_gamechat_t *chat, const char *str ) {
	chat->messages[chat->nextMsg].time = cg.realTime;
	Q_strncpyz( chat->messages[chat->nextMsg].text, str, sizeof( chat->messages[0].text ) );

	chat->lastMsgTime = cg.realTime;
	chat->nextMsg = ( chat->nextMsg + 1 ) % GAMECHAT_STACK_SIZE;
}

#define GAMECHAT_NOTIFY_TIME        5000
#define GAMECHAT_WAIT_IN_TIME       0
#define GAMECHAT_FADE_IN_TIME       100
#define GAMECHAT_WAIT_OUT_TIME      4000
#define GAMECHAT_FADE_OUT_TIME      ( GAMECHAT_NOTIFY_TIME - GAMECHAT_WAIT_OUT_TIME )

/*
** CG_DrawChat
*/
void CG_DrawChat( cg_gamechat_t *chat, int x, int y, char *fontName, struct qfontface_s *font, int fontSize,
				  int width, int height, int padding_x, int padding_y, vec4_t backColor, struct shader_s *backShader ) {
	int i, j;
	int s, e, w;
	int utf_len;
	int l, total_lines, lines;
	int x_offset, y_offset;
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
	int corner_radius = 12 * cgs.vidHeight / 600;
	int background_y;

	font_height = trap_SCR_FontHeight( font );
	message_mode = (int)trap_Cvar_Value( "con_messageMode" );
	chat_active = ( chat->lastMsgTime + GAMECHAT_WAIT_IN_TIME + GAMECHAT_FADE_IN_TIME > cg.realTime || message_mode );
	lines = 0;
	total_lines = /*!message_mode ? 0 : */ 1;

	if( chat_active ) {
		wait_time = GAMECHAT_WAIT_IN_TIME;
		fade_time = GAMECHAT_FADE_IN_TIME;
	} else {
		wait_time = GAMECHAT_WAIT_OUT_TIME;
		fade_time = GAMECHAT_FADE_OUT_TIME;
	}

	if( chat_active != chat->lastActive ) {
		// smooth fade ins and fade outs
		chat->lastActiveChangeTime = cg.realTime - ( 1.0 - chat->activeFrac ) * ( wait_time + fade_time );
	}

	if( cg.realTime >= chat->lastActiveChangeTime + wait_time ) {
		int time_diff, time_interval;

		time_diff = cg.realTime - ( chat->lastActiveChangeTime + wait_time );
		time_interval = fade_time;

		if( time_diff <= time_interval ) {
			chat->activeFrac = (float)time_diff / time_interval;
		} else {
			chat->activeFrac = 1;
		}
	} else {
		chat->activeFrac = 0;
	}

	if( chat_active ) {
		backColor[3] *= chat->activeFrac;
	} else {
		backColor[3] *= ( 1.0 - chat->activeFrac );
	}

	for( i = 0; i < GAMECHAT_STACK_SIZE; i++ ) {
		bool old_msg;

		l = chat->nextMsg - 1 - i;
		if( l < 0 ) {
			l = GAMECHAT_STACK_SIZE + l;
		}

		msg = &chat->messages[l];
		text = msg->text;
		old_msg = !message_mode && ( cg.realTime > msg->time + GAMECHAT_NOTIFY_TIME );

		if( !background_drawn && backColor[3] ) {
			if( old_msg ) {
				// keep the box being drawn for a while to prevent it from flickering
				// upon arrival of the possibly entered chat message
				if( !( !chat_active && cg.realTime <= chat->lastActiveChangeTime + 200 ) ) {
					break;
				}
			}

			background_y = y;
			trap_R_DrawStretchPic( x, background_y, width, height - corner_radius,
								   0.0f, 0.0f, 1.0f, 0.5f, backColor, backShader );
			background_y += height - corner_radius;

			trap_R_DrawStretchPic( x, background_y, corner_radius, corner_radius,
								   0.0f, 0.5f, 0.5f, 1.0f, backColor, backShader );
			trap_R_DrawStretchPic( x + corner_radius, background_y, width - corner_radius * 2, corner_radius,
								   0.5f, 0.5f, 0.5f, 1.0f, backColor, backShader );
			trap_R_DrawStretchPic( x + width - corner_radius, background_y, corner_radius, corner_radius,
								   0.5f, 0.5f, 1.0f, 1.0f, backColor, backShader );

			background_drawn = true;
		}

		// unless user is typing something, only display recent messages
		if( old_msg ) {
			break;
		}

		pass = 0;
		lines = 0;
		lastcolor = ColorIndex( COLOR_WHITE );

parse_string:
		l = 1;
		s = e = 0;
		while( 1 ) {
			int len;

			memset( tstr, 0, sizeof( tstr ) );

			// skip whitespaces at start
			for( ; text[s] == '\n' || Q_IsBreakingSpace( text + s ); s = Q_Utf8SyncPos( text, s + 1, UTF8SYNC_RIGHT ) ) ;

			// empty string
			if( !text[s] ) {
				break;
			}

			w = -1;
			len = trap_SCR_StrlenForWidth( text + s, font, width - padding_x * 2 );
			clamp_low( len, 1 );

			for( j = s; ( j < ( s + len ) ) && text[j] != '\0'; j += utf_len ) {
				utf_len = Q_Utf8SyncPos( text + j, 1, UTF8SYNC_RIGHT );
				memcpy( tstr + j - s, text + j, utf_len );

				if( text[j] == '\n' || Q_IsBreakingSpace( text + j ) ) {
					w = j; // last whitespace
				}
				if( text[j] == '\n' ) {
					break;
				}
			}
			e = j; // end

			// try to word avoid splitting words, unless no other options
			if( text[j] != '\0' && w > 0 ) {
				// stop at the last encountered whitespace
				j = w;
			}

			tstr[j - s] = '\0';

			Vector4Copy( color_table[lastcolor], fontColor );
			fontColor[3] = chat_active ? chat->activeFrac : 1.0 - chat->activeFrac;

			if( pass ) {
				// now actually render the line
				x_offset = padding_x;
				y_offset = height - padding_y - font_height - ( total_lines + lines - l ) * ( font_height + 2 );
				if( y_offset < padding_y ) {
					break;
				}

				trap_SCR_DrawClampString( x + x_offset, y + y_offset, tstr,
										  x + padding_x, y + padding_y, x - padding_x + width, y - padding_y + height, font, fontColor );

				l++;
			} else {
				// increase the lines counter
				lines++;
			}

			if( !text[j] ) {
				// fast path: we don't need two passes in case of one-liners..
				if( lines == 1 ) {
					x_offset = padding_x;
					y_offset = height - font_height - total_lines * ( font_height + 2 );
					if( y_offset < padding_y ) {
						break;
					}

					trap_SCR_DrawClampString( x + x_offset, y + y_offset, tstr,
											  x + padding_x, y + padding_y, x - padding_x + width, y - padding_y + height, font, fontColor );

					total_lines++;
					pass++;
				}
				break;
			}

			if( pass ) {
				// grab the last color token to carry it over to the next line
				lastcolor = Q_ColorStrLastColor( lastcolor, tstr, j - s );
			}

			s = j;
		}

		if( !pass ) {
			pass++;
			goto parse_string;
		} else {
			total_lines += lines;
		}
	}

	// let the engine know where the input line should be drawn
	trap_SCR_DrawChat( x + padding_x, y + height - padding_y - font_height, width - padding_x, font );

	chat->lastActive = chat_active;
}
