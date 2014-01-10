/*
Copyright (C) 2014 Victor Luchits

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

// r_cmds.c
#include "r_local.h"
#include "../qalgo/glob.h"

/*
* R_ScreenShot_f
*/
void R_ScreenShot_f( void )
{
	R_ScreenShot( ri.Cmd_Argv( 1 ), 
		ri.Cmd_Argc() >= 3 && !Q_stricmp( ri.Cmd_Argv( 2 ), "silent" ) ? qtrue : qfalse );
}

/*
* R_GlobFilter
*/
static qboolean R_GlobFilter( const char *pattern, const char *value )
{
	if( *pattern && !glob_match( pattern, value, 0 ) ) {
		return qfalse;
	}
	return qtrue;
}

/*
* R_ImageList_f
*/
void R_ImageList_f( void )
{
	R_PrintImageList( ri.Cmd_Argv( 1 ), R_GlobFilter );
}

/*
* R_ShaderList_f
*/
void R_ShaderList_f( void )
{
	R_PrintShaderList( ri.Cmd_Argv( 1 ), R_GlobFilter );
}

/*
* R_ShaderDump_f
*/
void R_ShaderDump_f( void )
{
	const char *name;

	if( (ri.Cmd_Argc() < 2) && !rsc.debugSurface )
	{
		Com_Printf( "Usage: %s [name]\n", ri.Cmd_Argv(0) );
		return;
	}

	if( ri.Cmd_Argc() < 2 )
		name = rsc.debugSurface->shader->name;
	else
		name = ri.Cmd_Argv( 1 );

	R_PrintShaderCache( name );
}
