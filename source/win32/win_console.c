#include "../qcommon/qcommon.h"
#include "winquake.h"

HANDLE hinput = NULL;
HANDLE houtput = NULL;

#define MAX_CONSOLETEXT 256
static char console_text[MAX_CONSOLETEXT];
static int console_textlen;

static char *OEM_to_utf8( const char *str ) {
	WCHAR wstr[MAX_CONSOLETEXT];
	static char utf8str[MAX_CONSOLETEXT * 4]; /* longest valid utf8 sequence is 4 bytes */

	MultiByteToWideChar( CP_OEMCP, 0, str, -1, wstr, sizeof( wstr ) / sizeof( WCHAR ) );
	wstr[sizeof( wstr ) / sizeof( wstr[0] ) - 1] = 0;
	WideCharToMultiByte( CP_UTF8, 0, wstr, -1, utf8str, sizeof( utf8str ), NULL, NULL );
	utf8str[sizeof( utf8str ) - 1] = 0;

	return utf8str;
}

static char *utf8_to_OEM( const char *utf8str ) {
	WCHAR wstr[MAX_PRINTMSG];
	static char oemstr[MAX_PRINTMSG];

	MultiByteToWideChar( CP_UTF8, 0, utf8str, -1, wstr, sizeof( wstr ) / sizeof( WCHAR ) );
	wstr[sizeof( wstr ) / sizeof( wstr[0] ) - 1] = 0;
	WideCharToMultiByte( CP_OEMCP, 0, wstr, -1, oemstr, sizeof( oemstr ), "?", NULL );
	oemstr[sizeof( oemstr ) - 1] = 0;

	return oemstr;
}

/*
* Sys_ConsoleInput
*/
char *Sys_ConsoleInput( void ) {
	INPUT_RECORD rec;
	int ch;
	DWORD dummy;
	DWORD numread, numevents;

	if( !dedicated || !dedicated->integer ) {
		return NULL;
	}

	if( !hinput ) {
		hinput = GetStdHandle( STD_INPUT_HANDLE );
	}
	if( !houtput ) {
		houtput = GetStdHandle( STD_OUTPUT_HANDLE );
	}

	for(;; ) {
		if( !GetNumberOfConsoleInputEvents( hinput, &numevents ) ) {
			Sys_Error( "Error getting # of console events" );
		}

		if( numevents <= 0 ) {
			break;
		}

		if( !ReadConsoleInput( hinput, &rec, 1, &numread ) ) {
			Sys_Error( "Error reading console input" );
		}

		if( numread != 1 ) {
			Sys_Error( "Couldn't read console input" );
		}

		if( rec.EventType == KEY_EVENT ) {
			if( !rec.Event.KeyEvent.bKeyDown ) {
				ch = rec.Event.KeyEvent.uChar.AsciiChar;

				switch( ch ) {
					case '\r':
						WriteFile( houtput, "\r\n", 2, &dummy, NULL );

						if( console_textlen ) {
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return OEM_to_utf8( console_text );
						}
						break;

					case '\b':
						if( console_textlen ) {
							console_textlen--;
							WriteFile( houtput, "\b \b", 3, &dummy, NULL );
						}
						break;

					default:
						if( ( unsigned char )ch >= ' ' ) {
							if( console_textlen < sizeof( console_text ) - 2 ) {
								WriteFile( houtput, &ch, 1, &dummy, NULL );
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}
						break;
				}
			}
		}
	}

	return NULL;
}

static void PrintColoredText( const char *s ) {
	char c;
	int colorindex;
	DWORD dummy;

	while( *s ) {
		int gc = Q_GrabCharFromColorString( &s, &c, &colorindex );
		if( gc == GRABCHAR_CHAR ) {
			if( c == '\n' ) {
				SetConsoleTextAttribute( houtput, 7 );
			}
			// I hope it's not too slow to output char by char
			WriteFile( houtput, &c, 1, &dummy, NULL );
		} else if( gc == GRABCHAR_COLOR ) {
			switch( colorindex ) {
				case 0: colorindex = 3; break; // dark cyan instead of black to keep it visible
				case 1: colorindex = 12; break;
				case 2: colorindex = 10; break;
				case 3: colorindex = 14; break;
				case 4: colorindex = 9; break;
				case 5: colorindex = 11; break; // note that cyan and magenta are
				case 6: colorindex = 13; break; // not where one might expect
				case 8: colorindex = 6; break;
				case 9: colorindex = 8; break;
				default:
				case 7: colorindex = 7; break; // 15 would be bright white
			}
			;
			SetConsoleTextAttribute( houtput, colorindex );
		} else if( gc == GRABCHAR_END ) {
			break;
		} else {
			assert( 0 );
		}
	}
}

/*
* Sys_ConsoleOutput
*
* Print text to the dedicated console
*/
void Sys_ConsoleOutput( char *string ) {
	DWORD dummy;
	char text[MAX_CONSOLETEXT + 2];   /* need 2 chars for the \r's */

	if( !dedicated || !dedicated->integer ) {
		return;
	}

	if( !houtput ) {
		houtput = GetStdHandle( STD_OUTPUT_HANDLE );
	}

	if( console_textlen ) {
		text[0] = '\r';
		memset( &text[1], ' ', console_textlen );
		text[console_textlen + 1] = '\r';
		text[console_textlen + 2] = 0;
		WriteFile( houtput, text, console_textlen + 2, &dummy, NULL );
	}

	string = utf8_to_OEM( string );

#if 0
	WriteFile( houtput, string, (unsigned)strlen( string ), &dummy, NULL );
#else
	PrintColoredText( string );
#endif

	if( console_textlen ) {
		WriteFile( houtput, console_text, console_textlen, &dummy, NULL );
	}
}
