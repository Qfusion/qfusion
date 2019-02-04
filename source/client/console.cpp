#include "client.h"
#include "qcommon/utf8.h"
#include "imgui/imgui.h"

// TODO: revamp key_dest garbage
// TODO: finish cleaning up old stuff
// TODO: check if mutex is really needed

static constexpr size_t CONSOLE_LOG_SIZE = 1000 * 1000; // 1MB
static constexpr size_t CONSOLE_INPUT_SIZE = 1024;

struct HistoryEntry {
	char cmd[ CONSOLE_INPUT_SIZE ];
};

struct Console {
	String< CONSOLE_LOG_SIZE > log;

	char input[ CONSOLE_INPUT_SIZE ];

	bool at_bottom;
	bool scroll_to_bottom;
	bool visible;

	HistoryEntry input_history[ 64 ];
	size_t history_head;
	size_t history_count;
	size_t history_idx;

	qmutex_t * mutex = NULL;
};

static Console console;

static void Con_ClearScrollback() {
	console.log.clear();
}

static void Con_ClearInput() {
	console.input[ 0 ] = '\0';
	console.history_idx = 0;
}

static void Con_MessageMode_f( void );
static void Con_MessageMode2_f( void );

void Con_Init() {
	Con_ClearScrollback();
	Con_ClearInput();

	console.at_bottom = true;
	console.scroll_to_bottom = false;
	console.visible = false;

	console.history_head = 0;
	console.history_count = 0;

	console.mutex = QMutex_Create();

	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole );
	Cmd_AddCommand( "messagemode", Con_MessageMode_f );
	Cmd_AddCommand( "messagemode2", Con_MessageMode2_f );
	Cmd_AddCommand( "clear", Con_ClearScrollback );
	// Cmd_AddCommand( "condump", Con_Dump );
}

void Con_Shutdown() {
	QMutex_Destroy( &console.mutex );

	Cmd_RemoveCommand( "toggleconsole" );
	Cmd_RemoveCommand( "messagemode" );
	Cmd_RemoveCommand( "messagemode2" );
	Cmd_RemoveCommand( "clear" );
	// Cmd_RemoveCommand( "condump" );
}

void Con_ToggleConsole() {
	if( cls.state == CA_GETTING_TICKET || cls.state == CA_CONNECTING || cls.state == CA_CONNECTED ) {
		return;
	}

	if( console.visible ) {
		CL_SetKeyDest( cls.old_key_dest );
	} else {
		CL_SetOldKeyDest( cls.key_dest );
		CL_SetKeyDest( key_console );
	}

	Con_ClearInput();

	console.scroll_to_bottom = true;
	console.visible = !console.visible;
}

bool Con_IsVisible() {
	return console.visible;
}

void Con_Close() {
	if( console.visible ) {
		CL_SetKeyDest( cls.old_key_dest );
		console.visible = false;
	}
}

static void Con_Append( const char * str, size_t len ) {
	// delete lines until we have enough space to add str
	size_t trim = 0;
	while( console.log.len() - trim + len >= CONSOLE_LOG_SIZE ) {
		const char * newline = StrChrUTF8( console.log.c_str() + trim, '\n' );
		if( newline == NULL ) {
			trim = console.log.len();
			break;
		}

		trim += newline - ( console.log.c_str() + trim ) + 1;
	}

	console.log.remove( 0, trim );
	console.log.append_raw( str, len );

	if( console.at_bottom )
		console.scroll_to_bottom = true;
}

static const char * FindNextColorToken( const char * str, char * token ) {
	const char * p = str;
	while( ( p = StrChrUTF8( p, Q_COLOR_ESCAPE ) ) != NULL ) {
		if( p[ 1 ] == Q_COLOR_ESCAPE || ( p[ 1 ] >= '0' && p[ 1 ] <= char( '0' + MAX_S_COLORS ) ) ) {
			*token = p[ 1 ];
			return p;
		}
	}
	return NULL;
}

void Con_Print( const char * str ) {
	if( console.mutex == NULL )
		return;

	QMutex_Lock( console.mutex );

	const char * p = str;
	const char * end = str + strlen( str );
	while( p < end ) {
		char token;
		const char * before = FindNextColorToken( p, &token );

		if( before == NULL ) {
			Con_Append( p, end - p );
			break;
		}

		Con_Append( p, before - p );

		if( token == '^' ) {
			Con_Append( "^", 1 );
		}
		else {
			const vec4_t & color = color_table[ token - '0' ];
			uint8_t r = max( 1, uint8_t( color[ 0 ] * 255.0f ) );
			uint8_t g = max( 1, uint8_t( color[ 1 ] * 255.0f ) );
			uint8_t b = max( 1, uint8_t( color[ 2 ] * 255.0f ) );
			uint8_t a = max( 1, uint8_t( color[ 3 ] * 255.0f ) );
			uint8_t escape[] = { 033, r, g, b, a };
			Con_Append( ( const char * ) escape, sizeof( escape ) );
		}

		p = before + 2;
	}

	uint8_t white[] = { 033, 255, 255, 255, 255 };
	Con_Append( ( const char * ) white, sizeof( white ) );

	QMutex_Unlock( console.mutex );
}

static void TabCompletion( char * buf );

static int InputCallback( ImGuiInputTextCallbackData * data ) {
	if( data->EventChar == 0 ) {
		bool dirty = false;

		if( data->EventKey == ImGuiKey_Tab ) {
			TabCompletion( data->Buf );
			dirty = true;
		}
		else if( data->EventKey == ImGuiKey_UpArrow || data->EventKey == ImGuiKey_DownArrow ) {
			if( data->EventKey == ImGuiKey_UpArrow && console.history_idx < console.history_count ) {
				console.history_idx++;
				dirty = true;
			}
			if( data->EventKey == ImGuiKey_DownArrow && console.history_idx > 0 ) {
				console.history_idx--;
				dirty = true;
			}
			if( dirty ) {
				if( console.history_idx == 0 ) {
					data->Buf[ 0 ] = '\0';
				}
				else {
					size_t idx = ( console.history_head + console.history_count - console.history_idx ) % ARRAY_COUNT( console.input_history );
					strcpy( data->Buf, console.input_history[ idx ].cmd );
				}
			}
		}

		if( dirty ) {
			data->BufDirty = true;
			data->BufTextLen = strlen( data->Buf );
			data->CursorPos = strlen( data->Buf );
		}
	}

	return 0;
}

static void Con_Execute() {
	bool chat = true;
	chat = chat && cls.state == CA_ACTIVE;
	chat = chat && console.input[ 0 ] != '/' && console.input[ 0 ] != '\\';
	chat = chat && !Cmd_CheckForCommand( console.input );

	if( chat ) {
		char * p = console.input;
		while( ( p = StrChrUTF8( p, '"' ) ) != NULL )
			*p = '\'';
		Cbuf_AddText( "say \"" );
		Cbuf_AddText( console.input );
		Cbuf_AddText( "\"\n" );
	}
	else {
		const char * cmd = console.input;
		if( cmd[ 0 ] == '/' || cmd[ 0 ] == '\\' )
			cmd++;
		Cbuf_AddText( cmd );
		Cbuf_AddText( "\n" );
	}

	Com_Printf( "> %s\n", console.input );

	if( strlen( console.input ) != 0 ) {
		const HistoryEntry * last = &console.input_history[ ( console.history_head + console.history_count - 1 ) % ARRAY_COUNT( console.input_history ) ];

		if( console.history_count == 0 || strcmp( last->cmd, console.input ) != 0 ) {
			HistoryEntry * entry = &console.input_history[ ( console.history_head + console.history_count ) % ARRAY_COUNT( console.input_history ) ];
			strcpy( entry->cmd, console.input );

			if( console.history_count == ARRAY_COUNT( console.input_history ) ) {
				console.history_head++;
			}
			else {
				console.history_count++;
			}
		}
	}

	Con_ClearInput();
}

// break str into small chunks so we can print them individually because the
// renderer doesn't like large dynamic meshes
const char * NextChunkEnd( const char * str ) {
	const char * p = str;
	while( true ) {
		p = strchr( p, '\n' );
		if( p == NULL )
			break;
		p++;
		if( p - str > 512 )
			return p;
	}
	return NULL;
}

void Con_Draw( int pressed_key ) {
	QMutex_Lock( console.mutex );

	ImGui::PushStyleColor( ImGuiCol_FrameBg, IM_COL32( 27, 24, 33, 224 ) );
	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0, 0, 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 8, 4 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( viddef.width, viddef.height ) );
	ImGui::Begin( "console", NULL, ImGuiWindowFlags_NoDecoration );
	{
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 27, 24, 33, 224 ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 4 ) );
		ImGui::BeginChild( "consoletext", ImVec2( 0, viddef.height * 0.4 - ImGui::GetFrameHeightWithSpacing() - 3 ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );
		{
			ImGui::PushTextWrapPos( 0 );
			const char * p = console.log.c_str();
			while( p != NULL ) {
				const char * end = NextChunkEnd( p );
				ImGui::TextUnformatted( p, end );
				p = end;
			}
			ImGui::PopTextWrapPos();

			if( console.scroll_to_bottom )
				ImGui::SetScrollHereY( 1.0f );
			console.scroll_to_bottom = false;

			if( pressed_key == K_PGUP || pressed_key == K_PGDN ) {
				float scroll = ImGui::GetScrollY();
				float page = ImGui::GetWindowSize().y - ImGui::GetTextLineHeight();
				scroll += page * ( pressed_key == K_PGUP ? -1 : 1 );
				scroll = bound( 0.0f, scroll, ImGui::GetScrollMaxY() );
				ImGui::SetScrollY( scroll );
			}

			console.at_bottom = ImGui::GetScrollY() == ImGui::GetScrollMaxY();
		}

		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();

		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 1 ) );
		ImGui::Separator();
		ImGui::PopStyleVar();

		ImGuiInputTextFlags input_flags = 0;
		input_flags |= ImGuiInputTextFlags_CallbackCharFilter;
		input_flags |= ImGuiInputTextFlags_CallbackCompletion;
		input_flags |= ImGuiInputTextFlags_CallbackHistory;
		input_flags |= ImGuiInputTextFlags_EnterReturnsTrue;

		ImGui::PushItemWidth( ImGui::GetWindowWidth() );
		bool enter = ImGui::InputText( "##consoleinput", console.input, sizeof( console.input ), input_flags, InputCallback );
		// can't drag the scrollbar without this
		if( !ImGui::IsAnyItemActive() )
			ImGui::SetKeyboardFocusHere();
		ImGui::PopItemWidth();

		if( enter ) {
			Con_Execute();
		}

		ImVec2 top_left = ImGui::GetCursorPos();
		top_left.y -= 1;
		ImVec2 bottom_right = top_left;
		bottom_right.x += ImGui::GetWindowWidth();
		bottom_right.y += 2;
		ImGui::GetWindowDrawList()->AddRectFilled( top_left, bottom_right, ImGui::GetColorU32( ImGuiCol_Separator ) );
	}

	ImGui::End();
	ImGui::PopStyleVar( 3 );
	ImGui::PopStyleColor( 2 );

	QMutex_Unlock( console.mutex );
}

static void Con_DisplayList( char ** list ) {
	for( int i = 0; list[ i ] != NULL; i++ ) {
		Com_Printf( "%s ", list[ i ] );
	}
	Com_Printf( "\n" );
}

static size_t CommonPrefixLength( const char * a, const char * b ) {
	size_t n = min( strlen( a ), strlen( b ) );
	size_t len = 0;
	for( size_t i = 0; i < n; i++ ) {
		if( a[ i ] != b[ i ] )
			break;
		len++;
	}
	return len;
}

static void TabCompletion( char * buf ) {
	char * input = buf;
	if( *input == '\\' || *input == '/' )
		input++;
	if( strlen( input ) == 0 )
		return;

	// Count number of possible matches
	int c = Cmd_CompleteCountPossible( input );
	int v = Cvar_CompleteCountPossible( input );
	int a = Cmd_CompleteAliasCountPossible( input );
	int ca = 0;

	char ** completion_lists[ 4 ] = { };

	const char * completion = NULL;

	if( !( c + v + a ) ) {
		// now see if there's any valid cmd in there, providing
		// a list of matching arguments
		completion_lists[3] = Cmd_CompleteBuildArgList( input );
		if( !completion_lists[3] ) {
			// No possible matches, let the user know they're insane
			Com_Printf( "\nNo matching aliases, commands, or cvars were found.\n" );
			return;
		}

		// count the number of matching arguments
		while( completion_lists[3][ca] != NULL )
			ca++;
		if( ca == 0 ) {
			// the list is empty, although non-NULL list pointer suggests that the command
			// exists, so clean up and exit without printing anything
			Mem_TempFree( completion_lists[3] );
			return;
		}
	}

	if( c != 0 ) {
		completion_lists[0] = Cmd_CompleteBuildList( input );
		completion = *completion_lists[0];
	}
	if( v != 0 ) {
		completion_lists[1] = Cvar_CompleteBuildList( input );
		completion = *completion_lists[1];
	}
	if( a != 0 ) {
		completion_lists[2] = Cmd_CompleteAliasBuildList( input );
		completion = *completion_lists[2];
	}
	if( ca != 0 ) {
		input = StrChrUTF8( input, ' ' ) + 1;
		completion = *completion_lists[3];
	}

	size_t common_prefix_len = SIZE_MAX;
	for( size_t i = 0; i < ARRAY_COUNT( completion_lists ); i++ ) {
		if( completion_lists[ i ] == NULL )
			continue;
		char ** c = &completion_lists[ i ][ 0 ];
		while( *c != NULL ) {
			common_prefix_len = min( common_prefix_len, CommonPrefixLength( completion, *c ) );
			c++;
		}
	}

	if( c + v + a + ca != 1 ) {
		if( c != 0 ) {
			Com_Printf( S_COLOR_RED "%i possible command%s%s\n", c, ( c > 1 ) ? "s: " : ":", S_COLOR_WHITE );
			Con_DisplayList( completion_lists[0] );
		}
		if( v != 0 ) {
			Com_Printf( S_COLOR_CYAN "%i possible variable%s%s\n", v, ( v > 1 ) ? "s: " : ":", S_COLOR_WHITE );
			Con_DisplayList( completion_lists[1] );
		}
		if( a != 0 ) {
			Com_Printf( S_COLOR_MAGENTA "%i possible alias%s%s\n", a, ( a > 1 ) ? "es: " : ":", S_COLOR_WHITE );
			Con_DisplayList( completion_lists[2] );
		}
		if( ca != 0 ) {
			Com_Printf( S_COLOR_GREEN "%i possible argument%s%s\n", ca, ( ca > 1 ) ? "s: " : ":", S_COLOR_WHITE );
			Con_DisplayList( completion_lists[3] );
		}
	}

	if( completion != NULL ) {
		size_t to_copy = min( common_prefix_len + 1, sizeof( console.input ) - ( input - console.input ) );
		Q_strncpyz( input, completion, to_copy );
	}

	for( size_t i = 0; i < ARRAY_COUNT( completion_lists ); i++ ) {
		Mem_TempFree( completion_lists[i] );
	}
}

/*
 * chat stuff
 */

// keep these around from previous Con_DrawChat call
static int con_chatX, con_chatY;
static int con_chatWidth;
static struct qfontface_s *con_chatFont;

// messagemode[2]
static bool chat_team;
#define     MAXCMDLINE  256
static char chat_buffer[MAXCMDLINE];
static int chat_prestep = 0;
static unsigned int chat_linepos = 0;
static unsigned int chat_bufferlen = 0;

#define ctrl_is_down ( Key_IsDown( K_LCTRL ) || Key_IsDown( K_RCTRL ) )

/*
* Con_SetMessageMode
*
* Called from CL_SetKeyDest
*/
void Con_SetMessageMode( void ) {
	bool message = ( cls.key_dest == key_message );

	if( message ) {
		Cvar_ForceSet( "con_messageMode", chat_team ? "2" : "1" );
	} else {
		Cvar_ForceSet( "con_messageMode", "0" );
	}
}

/*
* Con_MessageMode_f
*/
static void Con_MessageMode_f( void ) {
	chat_team = false;
	if( cls.state == CA_ACTIVE && !cls.demo.playing ) {
		CL_SetKeyDest( key_message );
	}
}

/*
* Con_MessageMode2_f
*/
static void Con_MessageMode2_f( void ) {
	chat_team = Cmd_Exists( "say_team" ); // if not, make it a normal "say: "
	if( cls.state == CA_ACTIVE && !cls.demo.playing ) {
		CL_SetKeyDest( key_message );
	}
}

/*
* Q_ColorCharCount
*/
static int Q_ColorCharCount( const char *s, int byteofs ) {
	wchar_t c;
	const char *end = s + byteofs;
	int charcount = 0;

	while( s < end ) {
		int gc = Q_GrabWCharFromColorString( &s, &c, NULL );
		if( gc == GRABCHAR_CHAR ) {
			charcount++;
		} else if( gc == GRABCHAR_COLOR ) {
			;
		} else if( gc == GRABCHAR_END ) {
			break;
		} else {
			assert( 0 );
		}
	}

	return charcount;
}

/*
* Q_ColorCharOffset
*/
static int Q_ColorCharOffset( const char *s, int charcount ) {
	const char *start = s;
	wchar_t c;

	while( *s && charcount ) {
		int gc = Q_GrabWCharFromColorString( &s, &c, NULL );
		if( gc == GRABCHAR_CHAR ) {
			charcount--;
		} else if( gc == GRABCHAR_COLOR ) {
			;
		} else if( gc == GRABCHAR_END ) {
			break;
		} else {
			assert( 0 );
		}
	}

	return s - start;
}

/*
* Con_ChatPrompt
*
* Returns the prompt for the chat input
*/
static const char *Con_ChatPrompt( void ) {
	if( chat_team || ctrl_is_down ) {
		return "say (to team):";
	} else {
		return "say:";
	}
}

/*
* Con_DrawChat
*/
void Con_DrawChat( int x, int y, int width, struct qfontface_s *font ) {
	const char *say;
	char *s;
	int swidth, totalwidth, prewidth = 0;
	int promptwidth, spacewidth;
	int fontHeight;
	int underlineThickness;
	char oldchar;
	int cursorcolor = ColorIndex( COLOR_WHITE );

	if( cls.state != CA_ACTIVE || cls.key_dest != key_message ) {
		return;
	}

	if( !font ) {
		font = cls.consoleFont;
	}

	con_chatX = x;
	con_chatY = y;
	con_chatWidth = width;
	con_chatFont = font;

	fontHeight = SCR_FontHeight( font );

	// 48 is an arbitrary offset for not overlapping the FPS and clock prints
	width -= 48 * viddef.height / 600;

	say = Con_ChatPrompt();
	SCR_DrawString( x, y, ALIGN_LEFT_TOP, say, font, colorWhite, 0 );
	spacewidth = SCR_strWidth( " ", font, 0, 0 );
	promptwidth = SCR_strWidth( say, font, 0, 0 ) + spacewidth;
	x += promptwidth;
	width -= promptwidth;

	SCR_FontUnderline( font, &underlineThickness );
	width -= underlineThickness;

	s = chat_buffer;
	swidth = SCR_strWidth( s, font, 0, 0 );

	totalwidth = swidth;

	if( chat_linepos ) {
		if( chat_linepos == chat_bufferlen ) {
			prewidth += swidth;
		} else {
			prewidth += SCR_strWidth( s, font, chat_linepos, 0 );
		}
	}

	if( totalwidth > width ) {
		// don't let the cursor go beyond the left screen edge
		clamp_high( chat_prestep, prewidth );

		// don't let it go beyond the right screen edge
		clamp_low( chat_prestep, prewidth - width );

		// don't leave an empty space after the string when deleting a character
		if( ( totalwidth - chat_prestep ) < width ) {
			chat_prestep = totalwidth - width;
		}
	} else {
		chat_prestep = 0;
	}

	if( chat_linepos == chat_bufferlen ) {
		SCR_DrawClampString( x - chat_prestep, y, s, x, y,
							 x + width, y + fontHeight, font, colorWhite, 0 );
	}
	oldchar = s[chat_linepos];
	s[chat_linepos] = '\0';
	cursorcolor = Q_ColorStrLastColor( ColorIndex( COLOR_WHITE ), s, -1 );
	s[chat_linepos] = oldchar;

	if( (int)( cls.realtime >> 8 ) & 1 ) {
		SCR_DrawFillRect( x + prewidth - chat_prestep, y, underlineThickness, fontHeight, color_table[cursorcolor] );
	}
}

/*
* Con_SendChatMessage
*/
static void Con_SendChatMessage( const char *text, bool team ) {
	const char *cmd;
	char buf[MAX_CHAT_BYTES], *p;

	// convert double quotes to single quotes
	Q_strncpyz( buf, text, sizeof( buf ) );
	for( p = buf; *p; p++ )
		if( *p == '"' ) {
			*p = '\'';
		}

	if( team && Cmd_Exists( "say_team" ) ) {
		cmd = "say_team";
	} else if( Cmd_Exists( "say" ) ) {
		cmd = "say";
	} else {
		cmd = "cmd say";
	}

	Cbuf_AddText( va( "%s \"%s\"\n", cmd, buf ) );
}

/*
* Con_MessageKeyPaste
*/
static void Con_MessageKeyPaste( void ) {
	char *cbd;
	char *tok;
	size_t i, next;

	cbd = CL_GetClipboardData();
	if( cbd ) {
		tok = strtok( cbd, "\n\r\b" );

		// only allow pasting of one line for malicious reasons
		if( tok != NULL ) {
			i = 0;
			while( tok[i] ) {
				next = Q_Utf8SyncPos( tok, i + 1, UTF8SYNC_RIGHT );
				if( next + chat_bufferlen >= MAX_CHAT_BYTES ) {
					break;
				}
				i = next;
			}

			if( i ) {
				memmove( chat_buffer + chat_linepos + i, chat_buffer + chat_linepos, chat_bufferlen - chat_linepos + 1 );
				memcpy( chat_buffer + chat_linepos, tok, i );
				chat_linepos += i;
				chat_bufferlen += i;
			}

			tok = strtok( NULL, "\n\r\b" );

			if( tok != NULL ) {
				Con_SendChatMessage( chat_buffer, chat_team );
				chat_bufferlen = 0;
				chat_linepos = 0;
				chat_buffer[0] = 0;
				CL_SetKeyDest( key_game );
			}
		}

		CL_FreeClipboardData( cbd );
	}
}

/*
* Con_NumPadValue
*/
static int Con_NumPadValue( int key ) {
	switch( key ) {
		case KP_SLASH:
			return '/';

		case KP_STAR:
			return '*';

		case KP_MINUS:
			return '-';

		case KP_PLUS:
			return '+';

		case KP_HOME:
			return '7';

		case KP_UPARROW:
			return '8';

		case KP_PGUP:
			return '9';

		case KP_LEFTARROW:
			return '4';

		case KP_5:
			return '5';

		case KP_RIGHTARROW:
			return '6';

		case KP_END:
			return '1';

		case KP_DOWNARROW:
			return '2';

		case KP_PGDN:
			return '3';

		case KP_INS:
			return '0';

		case KP_DEL:
			return '.';
	}

	return key;
}

/*
* Con_MessageCharEvent
*/
void Con_MessageCharEvent( wchar_t key ) {
	key = Con_NumPadValue( key );

	switch( key ) {
		case 12:
			// CTRL - L : clear
			chat_bufferlen = 0;
			chat_linepos = 0;
			chat_buffer[0] = '\0';
			return;
		case 1: // CTRL+A: jump to beginning of line (same as HOME)
			chat_linepos = 0;
			return;
		case 5: // CTRL+E: jump to end of line (same as END)
			chat_linepos = chat_bufferlen;
			return;
		case 22: // CTRL - V : paste
			Con_MessageKeyPaste();
			return;
	}

	if( key < 32 || key > 0xFFFF ) {
		return; // non-printable

	}
	if( chat_linepos < MAX_CHAT_BYTES - 1 ) {
		const char *utf = Q_WCharToUtf8Char( key );
		size_t utflen = strlen( utf );

		if( chat_bufferlen + utflen >= MAX_CHAT_BYTES ) {
			return;     // won't fit

		}
		// move remainder to the right
		memmove( chat_buffer + chat_linepos + utflen,
				 chat_buffer + chat_linepos,
				 strlen( chat_buffer + chat_linepos ) + 1 ); // +1 for trailing 0

		// insert the char sequence
		memcpy( chat_buffer + chat_linepos, utf, utflen );
		chat_bufferlen += utflen;
		chat_linepos += utflen;
	}
}

/*
* Con_MessageCompletion
*/
static void Con_MessageCompletion( const char *partial, bool teamonly ) {
	char comp[256];
	size_t comp_len;
	size_t partial_len;
	char **args;
	const char *p;

	// only complete at the end of the line
	if( chat_linepos != chat_bufferlen ) {
		return;
	}

	p = strrchr( chat_buffer, ' ' );
	if( p && *( p + 1 ) ) {
		partial = p + 1;
	} else {
		partial = chat_buffer;
	}

	comp[0] = '\0';

	args = Cmd_CompleteBuildArgListExt( teamonly ? "say_team" : "say", partial );
	if( args ) {
		int i;

		// check for single match
		if( args[0] && !args[1] ) {
			Q_strncpyz( comp, args[0], sizeof( comp ) );
		} else if( args[0] ) {
			char ch;
			size_t start_pos, pos;

			start_pos = strlen( partial );
			for( pos = start_pos; pos + 1 < sizeof( comp ); pos++ ) {
				ch = args[0][pos];
				if( !ch ) {
					break;
				}
				for( i = 1; args[i] && args[i][pos] == ch; i++ ) ;
				if( args[i] ) {
					break;
				}
			}
			Q_strncpyz( comp, args[0], sizeof( comp ) );
			comp[pos] = '\0';
		}

		Mem_Free( args );
	}

	comp_len = 0;
	partial_len = strlen( partial );

	if( comp[0] != '\0' ) {
		comp_len = strlen( comp );

		// add ', ' to string if completing at the beginning of the string
		if( comp[0] && ( chat_linepos == partial_len ) && ( chat_bufferlen + comp_len + 2 < MAX_CHAT_BYTES - 1 ) ) {
			Q_strncatz( comp, ", ", sizeof( comp ) );
			comp_len += 2;
		}
	} else {
		int c, v, a, t;

		c = Cmd_CompleteCountPossible( partial );
		v = Cvar_CompleteCountPossible( partial );
		a = Cmd_CompleteAliasCountPossible( partial );
		t = c + v + a;

		if( t > 0 ) {
			int i;
			char **list[5] = { 0, 0, 0, 0, 0 };
			const char *cmd = NULL;

			if( c ) {
				cmd = *( list[0] = Cmd_CompleteBuildList( partial ) );
			}
			if( v ) {
				cmd = *( list[1] = Cvar_CompleteBuildList( partial ) );
			}
			if( a ) {
				cmd = *( list[2] = Cmd_CompleteAliasBuildList( partial ) );
			}

			if( t == 1 ) {
				comp_len = strlen( cmd );
			} else {
				comp_len = partial_len;
				do {
					for( i = 0; i < 4; i++ ) {
						char ch = cmd[comp_len];
						char **l = list[i];
						if( l ) {
							while( *l && ( *l )[comp_len] == ch )
								l++;
							if( *l ) {
								break;
							}
						}
					}
					if( i == 4 ) {
						comp_len++;
					}
				} while( i == 4 );
			}

			if( comp_len >= sizeof( comp ) - 1 ) {
				comp_len = sizeof( comp ) - 1;
			}

			if( comp_len > partial_len ) {
				memcpy( comp, cmd, comp_len );
				comp[comp_len] = '\0';
			}

			for( i = 0; i < 4; ++i ) {
				if( list[i] ) {
					Mem_TempFree( list[i] );
				}
			}

			if( t == 1 && comp_len < sizeof( comp ) - 1 ) {
				Q_strncatz( comp, " ", sizeof( comp ) );
				comp_len++;
			}
		}
	}

	if( comp_len == 0 || comp_len == partial_len || chat_bufferlen + comp_len >= MAX_CHAT_BYTES - 1 ) {
		return;     // won't fit
	}

	chat_linepos -= partial_len;
	chat_bufferlen -= partial_len;
	memcpy( chat_buffer + chat_linepos, comp, comp_len + 1 );
	chat_bufferlen += comp_len;
	chat_linepos += comp_len;
}

/*
* Con_MessageKeyDown
*/
void Con_MessageKeyDown( int key ) {
	key = Con_NumPadValue( key );

	if( key == K_ENTER || key == KP_ENTER ) {
		if( chat_bufferlen > 0 ) {
			Con_SendChatMessage( chat_buffer, chat_team || ctrl_is_down );
			chat_bufferlen = 0;
			chat_linepos = 0;
			chat_buffer[0] = 0;
		}

		CL_SetKeyDest( key_game );
		return;
	}

	if( key == K_HOME || key == KP_HOME ) {
		if( !ctrl_is_down ) {
			chat_linepos = 0;
		}
		return;
	}

	if( key == K_END || key == KP_END ) {
		if( !ctrl_is_down ) {
			chat_linepos = chat_bufferlen;
		}
		return;
	}

	if( ( key == K_INS || key == KP_INS ) && ( Key_IsDown( K_LSHIFT ) || Key_IsDown( K_RSHIFT ) ) ) {
		Con_MessageKeyPaste();
		return;
	}

	if( key == K_TAB ) {
		Con_MessageCompletion( chat_buffer, chat_team || ctrl_is_down );
		return;
	}

	if( key == K_LEFTARROW || key == KP_LEFTARROW ) {
		if( chat_linepos > 0 ) {
			int charcount;

			// jump over invisible color sequences
			charcount = Q_ColorCharCount( chat_buffer, chat_linepos );
			chat_linepos = Q_ColorCharOffset( chat_buffer, charcount - 1 );
		}
		return;
	}

	if( key == K_RIGHTARROW || key == KP_RIGHTARROW ) {
		if( chat_linepos < chat_bufferlen ) {
			int charcount;

			// jump over invisible color sequences
			charcount = Q_ColorCharCount( chat_buffer, chat_linepos );
			chat_linepos = Q_ColorCharOffset( chat_buffer, charcount + 1 );
		}
		return;
	}

	if( key == K_DEL ) {
		char *s = chat_buffer + chat_linepos;
		int wc = Q_GrabWCharFromUtf8String( ( const char ** )&s );
		if( wc ) {
			memmove( chat_buffer + chat_linepos, s, strlen( s ) + 1 );
			chat_bufferlen -= ( s - ( chat_buffer + chat_linepos ) );
		}
		return;
	}

	if( key == K_BACKSPACE ) {
		if( chat_linepos ) {
			// skip to the end of color sequence
			while( 1 ) {
				char *tmp = chat_buffer + chat_linepos;
				wchar_t c;
				if( Q_GrabWCharFromColorString( ( const char ** )&tmp, &c, NULL ) == GRABCHAR_COLOR ) {
					chat_linepos = tmp - chat_buffer; // advance, try again
				} else {                                                                             // GRABCHAR_CHAR or GRABCHAR_END
					break;
				}
			}

			{
				int oldpos = chat_linepos;
				chat_linepos = Q_Utf8SyncPos( chat_buffer, chat_linepos - 1, UTF8SYNC_LEFT );
				memmove( chat_buffer + chat_linepos, chat_buffer + oldpos, strlen( chat_buffer + oldpos ) + 1 );
				chat_bufferlen -= ( oldpos - chat_linepos );
			}
		}
		return;
	}

	if( key == K_ESCAPE ) {
		CL_SetKeyDest( key_game );
		chat_bufferlen = 0;
		chat_linepos = 0;
		chat_buffer[0] = 0;
		return;
	}
}
