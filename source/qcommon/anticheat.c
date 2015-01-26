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
#include "anticheat.h"

#ifdef ANTICHEAT_MODULE

typedef void ( *AC_Export_t )(const ac_import_t *import);

static void *ac_libhandle = NULL;

static AC_Export_t InitClient_f = NULL;
static AC_Export_t InitServer_f = NULL;

/*
* AC_InitImportStruct
*/
#define AC_StructEntry( entry ) entry = (entry)

#define AC_InitImportStruct( import ) \
( \
	import.AC_StructEntry( SV_SendMessageToClient ), \
	import.AC_StructEntry( SV_ParseClientMessage ), \
\
	import.AC_StructEntry( CL_ParseServerMessage ), \
	import.AC_StructEntry( CL_Netchan_Transmit ), \
\
	import.AC_StructEntry( MSG_Init ), \
	import.AC_StructEntry( MSG_Clear ), \
	import.AC_StructEntry( MSG_GetSpace ), \
	import.AC_StructEntry( MSG_WriteData ), \
	import.AC_StructEntry( MSG_CopyData ), \
\
	import.AC_StructEntry( MSG_WriteChar ), \
	import.AC_StructEntry( MSG_WriteByte ), \
	import.AC_StructEntry( MSG_WriteShort ), \
	import.AC_StructEntry( MSG_WriteInt3 ), \
	import.AC_StructEntry( MSG_WriteLong ), \
	import.AC_StructEntry( MSG_WriteFloat ), \
	import.AC_StructEntry( MSG_WriteString ), \
	import.AC_StructEntry( MSG_WriteDeltaUsercmd ), \
	import.AC_StructEntry( MSG_WriteDeltaEntity ), \
	import.AC_StructEntry( MSG_WriteDir ), \
\
	import.AC_StructEntry( MSG_BeginReading ), \
\
	import.AC_StructEntry( MSG_ReadChar ), \
	import.AC_StructEntry( MSG_ReadByte ), \
	import.AC_StructEntry( MSG_ReadShort ), \
	import.AC_StructEntry( MSG_ReadInt3 ), \
	import.AC_StructEntry( MSG_ReadLong ), \
	import.AC_StructEntry( MSG_ReadFloat ), \
	import.AC_StructEntry( MSG_ReadString ), \
	import.AC_StructEntry( MSG_ReadStringLine ), \
	import.AC_StructEntry( MSG_ReadDeltaUsercmd ), \
\
	import.AC_StructEntry( MSG_ReadDir ), \
	import.AC_StructEntry( MSG_ReadData ), \
	import.AC_StructEntry( MSG_SkipData ), \
\
	import.AC_StructEntry( imports ), \
	import.AC_StructEntry( exports ) \
)

/*
* AC_LoadLibrary
*/
bool AC_LoadLibrary( void *imports, void *exports, unsigned int flags )
{
	static ac_import_t import;
	dllfunc_t funcs[3];
	bool found = false;
	bool verbose = false;

	AC_InitImportStruct( import );

	// load dynamic library if it's not already loaded...
	if( !ac_libhandle )
	{
		if( verbose )
			Com_Printf( "Loading AC module... " );

		funcs[0].name = "InitServer";
		funcs[0].funcPointer = (void **) &InitServer_f;
		funcs[1].name = "InitClient";
		funcs[1].funcPointer = (void **) &InitClient_f;
		funcs[2].name = NULL;
		ac_libhandle = Com_LoadLibrary( LIB_DIRECTORY "/" LIB_PREFIX "ac_" ARCH LIB_SUFFIX, funcs );
	}

	// load succeeded or already loaded and exported functions retrieved
	if( ac_libhandle && InitServer_f && InitClient_f )
	{
		switch ( flags )
		{
		case ANTICHEAT_SERVER:
			InitServer_f( &import );
			found = true;
			break;

		case ANTICHEAT_CLIENT:
			InitClient_f( &import );
			found = true;
			break;

		default:
			break;
		}
	}

	if( verbose )
		Com_Printf( "%s.\n", found ? "Done" : "Not found" );

	return found;
}

#else

bool AC_LoadLibrary( void *imports, void *exports, unsigned int flags )
{
	return true;
}

#endif
