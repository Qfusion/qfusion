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
#include "client.h"

/*

key up events are sent even if in console mode

*/

#define SEMICOLON_BINDNAME	"SEMICOLON"

int anykeydown;

static char *keybindings[256];
static qboolean consolekeys[256];   // if qtrue, can't be rebound while in console
static qboolean menubound[256];     // if qtrue, can't be rebound while in menu
static int key_repeats[256];   // if > 1, it is autorepeating
static qboolean keydown[256];

static qboolean	key_initialized = qfalse;

static char alt_color_escape = '$';
static dynvar_t *key_colorEscape = NULL;

static cvar_t *in_debug;

static dynvar_get_status_t Key_GetColorEscape_f( void **key )
{
	assert( key );
	*key = (void *) &alt_color_escape;
	return DYNVAR_GET_OK;
}

static dynvar_set_status_t Key_SetColorEscape_f( void *key )
{
	const char *const keyStr = (char *) key;
	assert( keyStr );
	if( strlen( keyStr ) == 1 )
	{
		const char keyChr = *keyStr;
		if( !isalnum( keyChr ) )
		{
			alt_color_escape = keyChr;
			return DYNVAR_SET_OK;
		}
	}
	return DYNVAR_SET_INVALID;
}

typedef struct
{
	const char *name;
	int keynum;
} keyname_t;

const keyname_t keynames[] =
{
	{ "TAB", K_TAB },
	{ "ENTER", K_ENTER },
	{ "ESCAPE", K_ESCAPE },
	{ "SPACE", K_SPACE },
	{ "CAPSLOCK", K_CAPSLOCK },
	{ "SCROLLLOCK", K_SCROLLLOCK },
	{ "SCROLLOCK", K_SCROLLLOCK },
	{ "NUMLOCK", K_NUMLOCK },
	{ "KP_NUMLOCK", K_NUMLOCK },
	{ "BACKSPACE", K_BACKSPACE },
	{ "UPARROW", K_UPARROW },
	{ "DOWNARROW", K_DOWNARROW },
	{ "LEFTARROW", K_LEFTARROW },
	{ "RIGHTARROW", K_RIGHTARROW },

	{ "ALT", K_ALT },
	{ "CTRL", K_CTRL },
	{ "SHIFT", K_SHIFT },

	{ "F1", K_F1 },
	{ "F2", K_F2 },
	{ "F3", K_F3 },
	{ "F4", K_F4 },
	{ "F5", K_F5 },
	{ "F6", K_F6 },
	{ "F7", K_F7 },
	{ "F8", K_F8 },
	{ "F9", K_F9 },
	{ "F10", K_F10 },
	{ "F11", K_F11 },
	{ "F12", K_F12 },
	{ "F13", K_F13 },
	{ "F14", K_F14 },
	{ "F15", K_F15 },

	{ "INS", K_INS },
	{ "DEL", K_DEL },
	{ "PGDN", K_PGDN },
	{ "PGUP", K_PGUP },
	{ "HOME", K_HOME },
	{ "END", K_END },

	{ "WINKEY", K_WIN },
	//	{"LWINKEY", K_LWIN},
	//	{"RWINKEY", K_RWIN},
	{ "POPUPMENU", K_MENU },

	{ "COMMAND", K_COMMAND },
	{ "OPTION", K_OPTION },

	{ "MOUSE1", K_MOUSE1 },
	{ "MOUSE2", K_MOUSE2 },
	{ "MOUSE3", K_MOUSE3 },
	{ "MOUSE4", K_MOUSE4 },
	{ "MOUSE5", K_MOUSE5 },
	{ "MOUSE6", K_MOUSE6 },
	{ "MOUSE7", K_MOUSE7 },
	{ "MOUSE8", K_MOUSE8 },

	{ "JOY1", K_JOY1 },
	{ "JOY2", K_JOY2 },
	{ "JOY3", K_JOY3 },
	{ "JOY4", K_JOY4 },

	{ "AUX1", K_AUX1 },
	{ "AUX2", K_AUX2 },
	{ "AUX3", K_AUX3 },
	{ "AUX4", K_AUX4 },
	{ "AUX5", K_AUX5 },
	{ "AUX6", K_AUX6 },
	{ "AUX7", K_AUX7 },
	{ "AUX8", K_AUX8 },
	{ "AUX9", K_AUX9 },
	{ "AUX10", K_AUX10 },
	{ "AUX11", K_AUX11 },
	{ "AUX12", K_AUX12 },
	{ "AUX13", K_AUX13 },
	{ "AUX14", K_AUX14 },
	{ "AUX15", K_AUX15 },
	{ "AUX16", K_AUX16 },
	{ "AUX17", K_AUX17 },
	{ "AUX18", K_AUX18 },
	{ "AUX19", K_AUX19 },
	{ "AUX20", K_AUX20 },
	{ "AUX21", K_AUX21 },
	{ "AUX22", K_AUX22 },
	{ "AUX23", K_AUX23 },
	{ "AUX24", K_AUX24 },
	{ "AUX25", K_AUX25 },
	{ "AUX26", K_AUX26 },
	{ "AUX27", K_AUX27 },
	{ "AUX28", K_AUX28 },
	{ "AUX29", K_AUX29 },
	{ "AUX30", K_AUX30 },
	{ "AUX31", K_AUX31 },
	{ "AUX32", K_AUX32 },

	{ "KP_HOME", KP_HOME },
	{ "KP_UPARROW",	KP_UPARROW },
	{ "KP_PGUP", KP_PGUP },
	{ "KP_LEFTARROW", KP_LEFTARROW },
	{ "KP_5", KP_5 },
	{ "KP_RIGHTARROW", KP_RIGHTARROW },
	{ "KP_END", KP_END },
	{ "KP_DOWNARROW", KP_DOWNARROW },
	{ "KP_PGDN", KP_PGDN },
	{ "KP_ENTER", KP_ENTER },
	{ "KP_INS", KP_INS },
	{ "KP_DEL", KP_DEL },
	{ "KP_STAR", KP_STAR },
	{ "KP_SLASH", KP_SLASH },
	{ "KP_MINUS", KP_MINUS },
	{ "KP_PLUS", KP_PLUS },

	{ "KP_MULT", KP_MULT },
	{ "KP_EQUAL", KP_EQUAL },

	{ "MWHEELUP", K_MWHEELUP },
	{ "MWHEELDOWN", K_MWHEELDOWN },

	{ "PAUSE", K_PAUSE },

	{ "SEMICOLON", ';' }, // because a raw semicolon separates commands

	{ NULL, 0 }
};

static void Key_DelegateCallKeyDel( int key );
static void Key_DelegateCallCharDel( qwchar key );

static int consolebinded = 0;

/*
* Key_StringToKeynum
* 
* Returns a key number to be used to index keybindings[] by looking at
* the given string.  Single ascii characters return themselves, while
* the K_* names are matched up.
*/
int Key_StringToKeynum( const char *str )
{
	const keyname_t *kn;

	if( !str || !str[0] )
		return -1;
	if( !str[1] )
		return (int)(unsigned char)str[0];

	for( kn = keynames; kn->name; kn++ )
	{
		if( !Q_stricmp( str, kn->name ) )
			return kn->keynum;
	}
	return -1;
}

/*
* Key_KeynumToString
* 
* Returns a string (either a single ascii char, or a K_* name) for the
* given keynum.
* FIXME: handle quote special (general escape sequence?)
*/
const char *Key_KeynumToString( int keynum )
{
	const keyname_t *kn;
	static char tinystr[2];

	if( keynum == -1 )
		return "<KEY NOT FOUND>";
	if( keynum > 32 && keynum < 127 )
	{ // printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for( kn = keynames; kn->name; kn++ )
		if( keynum == kn->keynum )
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}


/*
* Key_SetBinding
*/
void Key_SetBinding( int keynum, const char *binding )
{
	if( keynum == -1 )
		return;

	// free old bindings
	if( keybindings[keynum] )
	{
		if( !Q_stricmp( keybindings[keynum], "toggleconsole" ) )
			consolebinded--;

		Mem_ZoneFree( keybindings[keynum] );
		keybindings[keynum] = NULL;
	}

	if( !binding )
		return;

	// allocate memory for new binding
	keybindings[keynum] = ZoneCopyString( binding );

	if( !Q_stricmp( keybindings[keynum], "toggleconsole" ) )
		consolebinded++;
}

/*
* Key_Unbind_f
*/
static void Key_Unbind_f( void )
{
	int b;

	if( Cmd_Argc() != 2 )
	{
		Com_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}

	b = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if( b == -1 )
	{
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	Key_SetBinding( b, NULL );
}

static void Key_Unbindall( void )
{
	int i;

	for( i = 0; i < 256; i++ )
	{
		if( keybindings[i] )
			Key_SetBinding( i, NULL );
	}
}


/*
* Key_Bind_f
*/
static void Key_Bind_f( void )
{
	int i, c, b;
	char cmd[1024];

	c = Cmd_Argc();
	if( c < 2 )
	{
		Com_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	b = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if( b == -1 )
	{
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	if( c == 2 )
	{
		if( keybindings[b] )
			Com_Printf( "\"%s\" = \"%s\"\n", Cmd_Argv( 1 ), keybindings[b] );
		else
			Com_Printf( "\"%s\" is not bound\n", Cmd_Argv( 1 ) );
		return;
	}

	// copy the rest of the command line
	cmd[0] = 0; // start out with a null string
	for( i = 2; i < c; i++ )
	{
		Q_strncatz( cmd, Cmd_Argv( i ), sizeof( cmd ) );
		if( i != ( c-1 ) )
			Q_strncatz( cmd, " ", sizeof( cmd ) );
	}

	Key_SetBinding( b, cmd );
}

/*
* Key_WriteBindings
* 
* Writes lines containing "bind key value"
*/
void Key_WriteBindings( int file )
{
	int i;

	FS_Printf( file, "unbindall\r\n" );

	for( i = 0; i < 256; i++ )
		if( keybindings[i] && keybindings[i][0] )
			FS_Printf( file, "bind %s \"%s\"\r\n", (i == ';' ? SEMICOLON_BINDNAME : Key_KeynumToString( i )), keybindings[i] );
}


/*
* Key_Bindlist_f
*/
static void Key_Bindlist_f( void )
{
	int i;

	for( i = 0; i < 256; i++ )
		if( keybindings[i] && keybindings[i][0] )
			Com_Printf( "%s \"%s\"\n", Key_KeynumToString( i ), keybindings[i] );
}

/*
* Key_IsToggleConsole
* 
* If nothing is bound to toggleconsole, we use default key for it
* Also toggleconsole is specially handled, so it's never outputed to the console or so
*/
static qboolean Key_IsToggleConsole( int key )
{
	if( key == -1 )
		return qfalse;

	assert (key >= 0 && key <= 255);

	if( consolebinded > 0 )
	{
		if( keybindings[key] && !Q_stricmp( keybindings[key], "toggleconsole" ) )
			return qtrue;
		return qfalse;
	}
	else
	{
		if( key == '`' || key == '~' )
			return qtrue;
		return qfalse;
	}
}

/*
* Key_IsNonPrintable
* 
* Called by sys code to avoid garbage if the toggleconsole
* key happens to be a dead key (like in the German layout)
*/
qboolean Key_IsNonPrintable (int key)
{
	// This may be called before client is initialized. Shouldn't be a problem
	// for Key_IsToggleConsole, but double check just in case
	if (!key_initialized)
		return qfalse;

	return Key_IsToggleConsole(key);
}

/*
* Key_Init
*/
void Key_Init( void )
{
	int i;

	assert( !key_initialized );

	//
	// init ascii characters in console mode
	//
	for( i = 32; i < 128; i++ )
		consolekeys[i] = qtrue;
	consolekeys[K_ENTER] = qtrue;
	consolekeys[KP_ENTER] = qtrue;
	consolekeys[K_TAB] = qtrue;
	consolekeys[K_LEFTARROW] = qtrue;
	consolekeys[KP_LEFTARROW] = qtrue;
	consolekeys[K_RIGHTARROW] = qtrue;
	consolekeys[KP_RIGHTARROW] = qtrue;
	consolekeys[K_UPARROW] = qtrue;
	consolekeys[KP_UPARROW] = qtrue;
	consolekeys[K_DOWNARROW] = qtrue;
	consolekeys[KP_DOWNARROW] = qtrue;
	consolekeys[K_BACKSPACE] = qtrue;
	consolekeys[K_HOME] = qtrue;
	consolekeys[KP_HOME] = qtrue;
	consolekeys[K_END] = qtrue;
	consolekeys[KP_END] = qtrue;
	consolekeys[K_PGUP] = qtrue;
	consolekeys[KP_PGUP] = qtrue;
	consolekeys[K_PGDN] = qtrue;
	consolekeys[KP_PGDN] = qtrue;
	consolekeys[K_SHIFT] = qtrue;
	consolekeys[K_INS] = qtrue;
	consolekeys[K_DEL] = qtrue;
	consolekeys[KP_INS] = qtrue;
	consolekeys[KP_DEL] = qtrue;
	consolekeys[KP_SLASH] = qtrue;
	consolekeys[KP_PLUS] = qtrue;
	consolekeys[KP_MINUS] = qtrue;
	consolekeys[KP_5] = qtrue;

	consolekeys[K_WIN] = qtrue;
	//	consolekeys[K_LWIN] = qtrue;
	//	consolekeys[K_RWIN] = qtrue;
	consolekeys[K_MENU] = qtrue;

	consolekeys[K_CTRL] = qtrue; // wsw : pb : ctrl in console for ctrl-v
	consolekeys[K_ALT] = qtrue;

	consolekeys['`'] = qfalse;
	consolekeys['~'] = qfalse;

	// wsw : pb : support mwheel in console
	consolekeys[K_MWHEELDOWN] = qtrue;
	consolekeys[K_MWHEELUP] = qtrue;

	menubound[K_ESCAPE] = qtrue;
	// Vic: allow to bind F1-F12 from the menu
	//	for (i=0 ; i<12 ; i++)
	//		menubound[K_F1+i] = qtrue;

	//
	// register our functions
	//
	Cmd_AddCommand( "bind", Key_Bind_f );
	Cmd_AddCommand( "unbind", Key_Unbind_f );
	Cmd_AddCommand( "unbindall", Key_Unbindall );
	Cmd_AddCommand( "bindlist", Key_Bindlist_f );

	// wsw : aiwa : create dynvar for alternative color escape character
	key_colorEscape = Dynvar_Create( "key_colorEscape", qtrue, Key_GetColorEscape_f, Key_SetColorEscape_f );

	in_debug = Cvar_Get( "in_debug", "0", 0 );

	key_initialized = qtrue;
}

void Key_Shutdown( void )
{
	if( !key_initialized )
		return;

	Cmd_RemoveCommand( "bind" );
	Cmd_RemoveCommand( "unbind" );
	Cmd_RemoveCommand( "unbindall" );
	Cmd_RemoveCommand( "bindlist" );

	Key_Unbindall();
}

/*
* Key_CharEvent
* 
* Called by the system between frames for key down events for standard characters
* Should NOT be called during an interrupt!
*/
void Key_CharEvent( int key, qwchar charkey )
{
	if( Key_IsToggleConsole( key ) )
		return;

	if( charkey == ( qwchar )alt_color_escape )
		charkey = Q_COLOR_ESCAPE;

	switch( cls.key_dest )
	{
	case key_message:
		Con_MessageCharEvent( charkey );
		break;
	case key_menu:
		CL_UIModule_CharEvent( charkey );
		break;
	case key_game:
	case key_console:
		Con_CharEvent( charkey );
		break;
	case key_delegate:
		Key_DelegateCallCharDel( charkey );
		break;
	default:
		Com_Error( ERR_FATAL, "Bad cls.key_dest" );
	}
}

/*
* Key_MouseEvent
* 
* A wrapper around Key_Event to generate double click events
* A typical sequence of events will look like this:
* +MOUSE1 - user pressed button
* -MOUSE1 - user released button
* +MOUSE1 - user pressed button  (must be within 480 ms or so of the previous down event)
* +MOUSE1DBLCLK - inserted by Key_MouseEvent
* -MOUSE1DBLCLK - inserted by Key_MouseEvent
* -MOUSE1 - user released button
* (This order is not final! We might want to suppress the second pair of 
* mouse1 down/up events, or make +MOUSE1DBLCLK come before +MOUSE1)
*/
void Key_MouseEvent( int key, qboolean down, unsigned time )
{
	static unsigned int last_button1_click = 0;
	// use a lower delay than XP default (480 ms) because we don't support width/height yet
	const unsigned int doubleclick_time = 350;	// milliseconds
	//	static int last_button1_x, last_button1_y; // TODO
	//	const int doubleclick_width = 4;	// TODO
	//	const int doubleclick_height = 4;	// TODO

	if( key == K_MOUSE1 )
	{
		if( down )
		{
			if( last_button1_click && ( (time - last_button1_click) < doubleclick_time ) )
			{
				last_button1_click = 0;
				Key_Event( key, down, time );
				Key_Event( K_MOUSE1DBLCLK, qtrue, time );
				Key_Event( K_MOUSE1DBLCLK, qfalse, time );
				return;
			}
			else
			{
				last_button1_click = time;
			}
		}
	}
	else if( key == K_MOUSE2 || key == K_MOUSE3 )
	{
		last_button1_click = 0;
	}

	Key_Event( key, down, time );
}

/*
* Key_Event
* 
* Called by the system between frames for both key up and key down events
* Should NOT be called during an interrupt!
*/
void Key_Event( int key, qboolean down, unsigned time )
{
	char *kb;
	char cmd[1024];
	qboolean handled = qfalse;

	// update auto-repeat status
	if( down )
	{
		key_repeats[key]++;
		if( key_repeats[key] > 1 )
		{
			if( ( key != K_BACKSPACE && key != K_DEL
				&& key != K_LEFTARROW && key != K_RIGHTARROW
				&& key != K_UPARROW && key != K_DOWNARROW
				&& key != K_PGUP && key != K_PGDN && ( key < 32 || key > 126 || key == '`' ) )
				|| cls.key_dest == key_game )
				return;
		}
	}
	else
	{
		key_repeats[key] = 0;
	}

#ifndef WIN32
	// switch between fullscreen/windowed when ALT+ENTER is pressed
	if( key == K_ENTER && down && keydown[K_ALT] )
	{
		Cbuf_ExecuteText( EXEC_APPEND, "toggle vid_fullscreen\n" );
		return;
	}
#endif

#if defined ( __MACOSX__ )
	// quit the game when Control + q is pressed
	if( key == 'q' && down && keydown[K_COMMAND] )
	{
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
		return;
	}
#endif

	if( Key_IsToggleConsole( key ) )
	{
		if( !down )
			return;
		Con_ToggleConsole_f();
		return;
	}

	// menu key is hardcoded, so the user can never unbind it
	if( key == K_ESCAPE )
	{
		if( !down )
			return;

		if( cls.state != CA_ACTIVE )
		{
			if( cls.key_dest == key_game || cls.key_dest == key_menu )
			{
				if( cls.state != CA_DISCONNECTED )
					Cbuf_AddText( "disconnect\n" );
				else if( cls.key_dest == key_menu )
					CL_UIModule_Keydown( key );
				return;
			}
		}

		switch( cls.key_dest )
		{
		case key_message:
			Con_MessageKeyDown( key );
			break;
		case key_menu:
			CL_UIModule_Keydown( key );
			break;
		case key_game:
			CL_GameModule_EscapeKey();
			break;
		case key_console:
			Con_ToggleConsole_f();
			break;
		case key_delegate:
			Key_DelegateCallKeyDel( key );
			break;
		default:
			Com_Error( ERR_FATAL, "Bad cls.key_dest" );
		}
		return;
	}

	//
	// if not a consolekey, send to the interpreter no matter what mode is
	//
	if( ( cls.key_dest == key_menu && menubound[key] )
		|| ( cls.key_dest == key_console && !consolekeys[key] )
		|| ( cls.key_dest == key_game && ( cls.state == CA_ACTIVE || !consolekeys[key] ) )
		|| ( cls.key_dest == key_message && ( key >= K_F1 && key <= K_F15 ) ) )
	{
		kb = keybindings[key];

		if( kb )
		{
			if( in_debug && in_debug->integer ) {
				Com_Printf( "key:%i down:%i time:%i %s\n", key, down, time, kb );
			}

			if( kb[0] == '+' )
			{ // button commands add keynum and time as a parm
				if( down )
				{
					Q_snprintfz( cmd, sizeof( cmd ), "%s %i %u\n", kb, key, time );
					Cbuf_AddText( cmd );
				}
				else if( keydown[key] )
				{
					Q_snprintfz( cmd, sizeof( cmd ), "-%s %i %u\n", kb+1, key, time );
					Cbuf_AddText( cmd );
				}
			}
			else if( down )
			{
				Cbuf_AddText( kb );
				Cbuf_AddText( "\n" );
			}
		}
		handled = qtrue; // can't return here, because we want to track keydown & repeats
	}

	// track if any key is down for BUTTON_ANY
	keydown[key] = down;
	if( down )
	{
		if( key_repeats[key] == 1 )
			anykeydown++;
	}
	else
	{
		anykeydown--;
		if( anykeydown < 0 )
			anykeydown = 0;
	}

	if( cls.key_dest == key_menu )
	{
		if( down )
			CL_UIModule_Keydown( key );
		else
			CL_UIModule_Keyup( key );
		return;
	}

	if( handled || !down )
		return; // other systems only care about key down events

	switch( cls.key_dest )
	{
	case key_message:
		Con_MessageKeyDown( key );
		break;
	case key_game:
	case key_console:
		Con_KeyDown( key );
		break;
	case key_delegate:
		Key_DelegateCallKeyDel( key );
		break;
	default:
		Com_Error( ERR_FATAL, "Bad cls.key_dest" );
	}
}

/*
* Key_ClearStates
*/
void Key_ClearStates( void )
{
	int i;

	anykeydown = qfalse;

	for( i = 0; i < 256; i++ )
	{
		if( keydown[i] || key_repeats[i] )
			Key_Event( i, qfalse, 0 );
		keydown[i] = 0;
		key_repeats[i] = 0;
	}
}


/*
* Key_GetBindingBuf
*/
const char *Key_GetBindingBuf( int binding )
{
	return keybindings[binding];
}

/*
* Key_IsDown
*/
qboolean Key_IsDown( int keynum )
{
	if( keynum < 0 || keynum > 255 )
		return qfalse;
	return keydown[keynum];
}

typedef struct
{
	key_delegate_f key_del;
	key_char_delegate_f char_del;
} key_delegates_t;

static key_delegates_t key_delegate_stack[32];
static int key_delegate_stack_index = 0;

/*
* Key_DelegatePush
*/
keydest_t Key_DelegatePush( key_delegate_f key_del, key_char_delegate_f char_del )
{
	assert( key_delegate_stack_index < sizeof( key_delegate_stack ) / sizeof( key_delegate_f ) );
	key_delegate_stack[key_delegate_stack_index].key_del = key_del;
	key_delegate_stack[key_delegate_stack_index].char_del = char_del;
	++key_delegate_stack_index;
	if( key_delegate_stack_index == 1 )
	{
		CL_SetOldKeyDest( cls.key_dest );
		CL_SetKeyDest( key_delegate );
		return cls.old_key_dest;
	}
	else
		return key_delegate;
}

/*
* Key_DelegatePop
*/
void Key_DelegatePop( keydest_t next_dest )
{
	assert( key_delegate_stack_index > 0 );
	--key_delegate_stack_index;
	CL_SetKeyDest( next_dest );
}

/*
* Key_DelegateCallKeyDel
*/
static void Key_DelegateCallKeyDel( int key )
{
	assert( key_delegate_stack_index > 0 );
	key_delegate_stack[key_delegate_stack_index - 1].key_del( key, keydown );
}

/*
* Key_DelegateCallCharDel
*/
static void Key_DelegateCallCharDel( qwchar key )
{
	assert( key_delegate_stack_index > 0 );
	key_delegate_stack[key_delegate_stack_index - 1].char_del( key );
}
