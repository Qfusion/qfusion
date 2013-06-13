/*
Copyright (C) 2008 Chasseur de bots

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
#include "../qcommon/qcommon.h"
#include "../irc/irc_interface.h"

static irc_export_t *irc_export;
static void *irc_libhandle = NULL;

static cvar_t *irc_server;
static dynvar_t *irc_connected;

static mempool_t *irc_pool;
static qboolean connected_b = qfalse;
static qboolean irc_initialized = qfalse;

static void Irc_LoadLibrary( void );
static void Irc_UnloadLibrary( void );

// diverse declarations of functions we need to export but whose headers conflict
extern struct mufont_s *SCR_RegisterFont( const char *name );
extern void SCR_DrawString( int x, int y, int align, const char *str, struct mufont_s *font, vec4_t color );
extern int SCR_DrawStringWidth( int x, int y, int align, const char *str, int maxwidth, struct mufont_s *font, vec4_t color );
extern void SCR_DrawRawChar( int x, int y, qwchar num, struct mufont_s *font, vec4_t color );
extern size_t SCR_strHeight( struct mufont_s *font );
extern size_t SCR_strWidth( const char *str, struct mufont_s *font, int maxlen );
extern size_t SCR_StrlenForWidth( const char *str, struct mufont_s *font, size_t maxwidth );
extern int CL_GetKeyDest( void );
extern connstate_t CL_GetClientState( void );
extern struct shader_s *R_RegisterPic( const char *name );
extern void R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *color, struct shader_s *shader );
extern void R_DrawStretchPoly( const struct poly_s *poly, float x_offset, float y_offset );

static void Irc_Print( const char *msg )
{
	Com_Printf( "%s", msg );
}

dynvar_get_status_t Irc_GetConnected_f( void **connected )
{
	*connected = &connected_b;
	return DYNVAR_GET_OK;
}

dynvar_set_status_t Irc_SetConnected_f( void *connected )
{
	connected_b = *(qboolean *) connected;
	return DYNVAR_SET_OK;
}

static void Irc_ConnectedListener_f( void *connected )
{
	if( *(qboolean *) connected )
	{
		assert( irc_server );
	}
	else
	{
		assert( irc_server );
		Dynvar_RemoveListener( irc_connected, Irc_ConnectedListener_f );
	}
}

static void Irc_Quit_f( void *nothing )
{
	if( irc_initialized )
	{
		qboolean *c, b;
		Dynvar_GetValue( irc_connected, (void **) &c );
		b = *c;
		Irc_UnloadLibrary();
		if( b )
		{
			// was connected before unload, sleep a little for sockets to close properly
			Sys_Sleep( 1000 );
		}
	}
}

static void *Irc_MemAlloc( int size, const char *filename, int fileline )
{
	return _Mem_Alloc( irc_pool, size, 0, 0, filename, fileline );
}

static void Irc_MemFree( void *data, const char *filename, int fileline )
{
	_Mem_Free( data, 0, 0, filename, fileline );
}

static mempool_t *Irc_MemAllocPool( const char *name, const char *filename, int fileline )
{
	return _Mem_AllocPool( irc_pool, name, 0, filename, fileline );
}

static void Irc_MemFreePool( const char *filename, int fileline )
{
	_Mem_FreePool( &irc_pool, 0, 0, filename, fileline );
}

static void Irc_MemEmptyPool( const char *filename, int fileline )
{
	_Mem_EmptyPool( irc_pool, 0, 0, filename, fileline );
}

static void Irc_LoadLibrary( void )
{
	static irc_import_t import;
	dllfunc_t funcs[2];
	GetIrcAPI_t GetIrcAPI_f;

	assert( !irc_libhandle );

	import.Printf = Irc_Print;
	import.CL_GetKeyDest = CL_GetKeyDest;
	import.CL_GetClientState = CL_GetClientState;
	import.Key_DelegatePush = Key_DelegatePush;
	import.Key_DelegatePop = Key_DelegatePop;
	import.SCR_RegisterFont = SCR_RegisterFont;
	import.SCR_DrawString = SCR_DrawString;
	import.SCR_DrawStringWidth = SCR_DrawStringWidth;
	import.SCR_DrawRawChar = SCR_DrawRawChar;
	import.SCR_strHeight = SCR_strHeight;
	import.SCR_strWidth = SCR_strWidth;
	import.SCR_StrlenForWidth = SCR_StrlenForWidth;
	import.R_RegisterPic = R_RegisterPic;
	import.R_DrawStretchPic = R_DrawStretchPic;
	import.R_DrawStretchPoly = R_DrawStretchPoly;
	import.viddef = &viddef;
	import.Milliseconds = Sys_Milliseconds;
	import.Microseconds = Sys_Microseconds;
	import.Mem_AllocPool = Irc_MemAllocPool;
	import.Mem_Alloc = Irc_MemAlloc;
	import.Mem_Free = Irc_MemFree;
	import.Mem_FreePool = Irc_MemFreePool;
	import.Mem_EmptyPool = Irc_MemEmptyPool;
	import.Dynvar_Create = Dynvar_Create;
	import.Dynvar_Destroy = Dynvar_Destroy;
	import.Dynvar_Lookup = Dynvar_Lookup;
	import.Dynvar_GetName = Dynvar_GetName;
	import.Dynvar_GetValue = Dynvar_GetValue;
	import.Dynvar_SetValue = Dynvar_SetValue;
	import.Dynvar_CallListeners = Dynvar_CallListeners;
	import.Dynvar_AddListener = Dynvar_AddListener;
	import.Dynvar_RemoveListener = Dynvar_RemoveListener;
	import.DYNVAR_WRITEONLY = DYNVAR_WRITEONLY;
	import.DYNVAR_READONLY = DYNVAR_READONLY;
	import.Cvar_Get = Cvar_Get;
	import.Cvar_Set = Cvar_Set;
	import.Cvar_SetValue = Cvar_SetValue;
	import.Cvar_ForceSet = Cvar_ForceSet;
	import.Cvar_Integer = Cvar_Integer;
	import.Cvar_Value = Cvar_Value;
	import.Cvar_String = Cvar_String;
	import.Cmd_Argc = Cmd_Argc;
	import.Cmd_Argv = Cmd_Argv;
	import.Cmd_Args = Cmd_Args;
	import.Cmd_AddCommand = Cmd_AddCommand;
	import.Cmd_RemoveCommand = Cmd_RemoveCommand;
	import.Cmd_ExecuteString = Cmd_ExecuteString;
	import.Com_BeginRedirect = Com_BeginRedirect;
	import.Com_EndRedirect = Com_EndRedirect;
	import.Cmd_SetCompletionFunc = Cmd_SetCompletionFunc;
	import.Cbuf_AddText = Cbuf_AddText;
	import.Trie_Create = Trie_Create;
	import.Trie_Destroy = Trie_Destroy;
	import.Trie_Clear = Trie_Clear;
	import.Trie_GetSize = Trie_GetSize;
	import.Trie_Insert = Trie_Insert;
	import.Trie_Remove = Trie_Remove;
	import.Trie_Replace = Trie_Replace;
	import.Trie_Find = Trie_Find;
	import.Trie_FindIf = Trie_FindIf;
	import.Trie_NoOfMatches = Trie_NoOfMatches;
	import.Trie_NoOfMatchesIf = Trie_NoOfMatchesIf;
	import.Trie_Dump = Trie_Dump;
	import.Trie_DumpIf = Trie_DumpIf;
	import.Trie_FreeDump = Trie_FreeDump;

	// load dynamic library
	Com_Printf( "Loading IRC module... " );
	funcs[0].name = "GetIrcAPI";
	funcs[0].funcPointer = (void **) &GetIrcAPI_f;
	funcs[1].name = NULL;
	irc_libhandle = Com_LoadLibrary( LIB_DIRECTORY "/irc_" ARCH LIB_SUFFIX, funcs );

	if( irc_libhandle )
	{
		// load succeeded
		int api_version;
		irc_export = GetIrcAPI_f( &import );
		irc_pool = Mem_AllocPool( NULL, "IRC Module" );
		api_version = irc_export->API();
		if( api_version == IRC_API_VERSION )
		{
			if( irc_export->Init() )
			{
				dynvar_t *const quit = Dynvar_Lookup( "quit" );
				if( quit )
					Dynvar_AddListener( quit, Irc_Quit_f );
				irc_initialized = qtrue;
				Cmd_AddCommand( "irc_unload", Irc_UnloadLibrary );
				Com_Printf( "Success.\n" );
			}
			else
			{
				// initialization failed
				Mem_FreePool( &irc_pool );
				Com_Printf( "Initialization failed.\n" );
				Irc_UnloadLibrary();
			}
		}
		else
		{
			// wrong version
			Mem_FreePool( &irc_pool );
			Com_Printf( "Wrong version: %i, not %i.\n", api_version, IRC_API_VERSION );
			Irc_UnloadLibrary();
		}
	}
	else
	{
		Com_Printf( "Not found.\n" );
	}

	Mem_CheckSentinelsGlobal();
}

static void Irc_UnloadLibrary( void )
{
	assert( irc_libhandle );
	if( irc_initialized )
	{
		dynvar_t *const quit = Dynvar_Lookup( "quit" );
		qboolean *c;
		if( !irc_connected )
			irc_connected = Dynvar_Lookup( "irc_connected" );
		Dynvar_GetValue( irc_connected, (void **) &c );
		if( *c )
			irc_export->Disconnect();
		irc_export->Shutdown();
		Cmd_RemoveCommand( "irc_unload" );
		if( quit )
			Dynvar_RemoveListener( quit, Irc_Quit_f );
		irc_initialized = qfalse;
	}
	Com_UnloadLibrary( &irc_libhandle );
	assert( !irc_libhandle );
	Com_Printf( "IRC module unloaded.\n" );
}

void Irc_Connect_f( void )
{
	const int argc = Cmd_Argc();
	if( argc <= 3 )
	{
		if( !irc_libhandle )
			Irc_LoadLibrary(); // load IRC library if not already loaded
		if( irc_libhandle )
		{
			// library loaded, check for connection status
			qboolean *c;
			if( !irc_server )
				irc_server = Cvar_Get( "irc_server", "irc.quakenet.org", CVAR_ARCHIVE );
			if( !irc_connected )
				irc_connected = Dynvar_Lookup( "irc_connected" );
			assert( irc_server );
			assert( irc_connected );
			Dynvar_GetValue( irc_connected, (void **) &c );
			if( !*c )
			{
				// not connected yet
				if( argc >= 2 )
					Cvar_Set( "irc_server", Cmd_Argv( 1 ) );
				if( argc >= 3 )
					Cvar_Set( "irc_port", Cmd_Argv( 2 ) );
				Dynvar_AddListener( irc_connected, Irc_ConnectedListener_f );
				irc_export->Connect();
				Dynvar_GetValue( irc_connected, (void **) &c );
				if( !*c )
				{
					// connect failed
					Com_Printf( "Could not connect to %s (%s).\n", Cvar_GetStringValue( irc_server ), irc_export->ERROR_MSG );
					Dynvar_RemoveListener( irc_connected, Irc_ConnectedListener_f );
				}
			}
			else
				Com_Printf( "Already connected.\n" );
		}
	}
	else
		Com_Printf( "usage: irc_connect [<server>] [<port>]" );
}

void Irc_Disconnect_f( void )
{
	if( irc_libhandle )
	{
		qboolean *c;
		if( !irc_server )
			irc_server = Cvar_Get( "irc_server", "", 0 );
		if( !irc_connected )
			irc_connected = Dynvar_Lookup( "irc_connected" );
		assert( irc_connected );
		assert( irc_server );
		Dynvar_GetValue( irc_connected, (void **) &c );
		if( *c )
		{
			// still connected, proceed
			irc_export->Disconnect();
			Dynvar_RemoveListener( irc_connected, Irc_ConnectedListener_f );
		}
		else
			Com_Printf( "Not connected.\n" );
	}
	else
		Com_Printf( "IRC module not loaded. Connect first.\n" );
}

qboolean Irc_IsConnected( void )
{
	if( irc_libhandle ) {
		qboolean *c;

		if( !irc_connected )
			irc_connected = Dynvar_Lookup( "irc_connected" );
		assert( irc_connected );
		
		Dynvar_GetValue( irc_connected, (void **) &c );
		if( *c ) {
			return qtrue;
		}
	}
	return qfalse;
}

size_t Irc_HistorySize( void )
{
	return irc_libhandle ? irc_export->HistorySize() : 0;
}

size_t Irc_HistoryTotalSize( void )
{
	return irc_libhandle ? irc_export->HistoryTotalSize() : 0;
}

// history is in reverse order (newest line first)
const struct irc_chat_history_node_s *Irc_GetHistoryHeadNode(void)
{
	return irc_libhandle ? irc_export->GetHistoryHeadNode() : NULL;
}

const struct irc_chat_history_node_s *Irc_GetNextHistoryNode(const struct irc_chat_history_node_s *n)
{
	return irc_libhandle ? irc_export->GetNextHistoryNode(n) : NULL;
}

const struct irc_chat_history_node_s *Irc_GetPrevHistoryNode(const struct irc_chat_history_node_s *n)
{
	return irc_libhandle ? irc_export->GetPrevHistoryNode(n) : NULL;
}

const char *Irc_GetHistoryNodeLine(const struct irc_chat_history_node_s *n)
{
	return irc_libhandle ? irc_export->GetHistoryNodeLine(n) : NULL;
}

