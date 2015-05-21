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
	const char *name;
	const char *extension;
	const char *mediadir;
	size_t path_size;
	char *path;
	char *checkname = NULL;
	size_t checkname_size = 0;
	int quality;

	if( !R_ScreenEnabled() )
		return;

	name = ri.Cmd_Argv( 1 );
	if( r_screenshot_jpeg->integer ) {
		extension = ".jpg";
		quality = r_screenshot_jpeg_quality->integer;
	}
	else {
		extension = ".tga";
		quality = 100;
	}

	mediadir = ri.FS_MediaDirectory( FS_MEDIA_IMAGES );
	if( mediadir )
	{
		path_size = strlen( mediadir ) + 1 + strlen( glConfig.applicationName ) + sizeof( " Screenshots/" );
		path = alloca( path_size );
		Q_snprintfz( path, path_size, "%s/%s Screenshots/", mediadir, glConfig.applicationName );
	}
	else
	{
		path_size = strlen( ri.FS_WriteDirectory() ) + 1 + strlen( ri.FS_GameDirectory() ) + sizeof( "/screenshots/" );
		path = alloca( path_size );
		Q_snprintfz( path, path_size, "%s/%s/screenshots/", ri.FS_WriteDirectory(), ri.FS_GameDirectory() );
	}

	if( name && name[0] && Q_stricmp(name, "*") )
	{
		if( !COM_ValidateRelativeFilename( name ) )
		{
			Com_Printf( "Invalid filename\n" );
			return;
		}

		checkname_size = ( path_size - 1 ) + strlen( name ) + strlen( extension ) + 1;
		checkname = alloca( checkname_size );
		Q_snprintfz( checkname, checkname_size, "%s%s", path, name );
		COM_DefaultExtension( checkname, extension, checkname_size );
	}

	//
	// find a file name to save it to
	//
	if( !checkname )
	{
		int i;
		const int maxFiles = 100000;
		static int lastIndex = 0;
		bool addIndex = false;
		time_t timestamp;
		char timestamp_str[MAX_QPATH];
		struct tm *timestampptr;

		timestamp = time( NULL );
		timestampptr = localtime( &timestamp );

		// validate timestamp string
		for( i = 0; i < 2; i++ )
		{
			strftime( timestamp_str, sizeof( timestamp_str ), r_screenshot_fmtstr->string, timestampptr );
			if( !COM_ValidateRelativeFilename( timestamp_str ) )
				ri.Cvar_ForceSet( r_screenshot_fmtstr->name, r_screenshot_fmtstr->dvalue );
			else
				break;
		}

		// hm... shouldn't really happen, but check anyway
		if( i == 2 )
		{
			Q_strncpyz( timestamp_str, glConfig.screenshotPrefix, sizeof( timestamp_str ) );
			ri.Cvar_ForceSet( r_screenshot_fmtstr->name, glConfig.screenshotPrefix );
		}

		checkname_size = ( path_size - 1 ) + strlen( timestamp_str ) + 5 + 1 + strlen( extension );
		checkname = alloca( checkname_size );

		// if the string format is a constant or file already exists then iterate
		if( !*timestamp_str || !strcmp( timestamp_str, r_screenshot_fmtstr->string ) )
		{
			addIndex = true;

			// force a rescan in case some vars have changed..
			if( r_screenshot_fmtstr->modified )
			{
				lastIndex = 0;
				r_screenshot_fmtstr->modified = true;
			}
			if( r_screenshot_jpeg->modified )
			{
				lastIndex = 0;
				r_screenshot_jpeg->modified = false;
			}
		}
		else
		{
			Q_snprintfz( checkname, checkname_size, "%s%s%s", path, timestamp_str, extension );
			if( ri.FS_FOpenAbsoluteFile( checkname, NULL, FS_READ ) != -1 )
			{
				lastIndex = 0;
				addIndex = true;
			}
		}

		for( ; addIndex && lastIndex < maxFiles; lastIndex++ )
		{
			Q_snprintfz( checkname, checkname_size, "%s%s%05i%s", path, timestamp_str, lastIndex, extension );
			if( ri.FS_FOpenAbsoluteFile( checkname, NULL, FS_READ ) == -1 )
				break; // file doesn't exist
		}

		if( lastIndex == maxFiles )
		{
			Com_Printf( "Couldn't create a file\n" );
			return;
		}

		lastIndex++;
	}

	R_ScreenShot( checkname, 
		0, 0, glConfig.width, glConfig.height, quality, 
		false, false, false, 
		ri.Cmd_Argc() >= 3 && !Q_stricmp( ri.Cmd_Argv( 2 ), "silent" ) ? true : false );

	ri.FS_AddFileToMedia( checkname );
}

/*
* R_EnvShot_f
*/
void R_EnvShot_f( void )
{
	int i;
	int size, maxSize;
	const char *writedir, *gamedir;
	int checkname_size;
	char *checkname;
	refdef_t fd;
	struct	cubemapSufAndFlip
	{
		char *suf; vec3_t angles; int flags;
	} cubemapShots[6] = {
		{ "px", { 0, 0, 0 }, IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL },
		{ "nx", { 0, 180, 0 }, IT_FLIPDIAGONAL },
		{ "py", { 0, 90, 0 }, IT_FLIPY },
		{ "ny", { 0, 270, 0 }, IT_FLIPX },
		{ "pz", { -90, 180, 0 }, IT_FLIPDIAGONAL },
		{ "nz", { 90, 180, 0 }, IT_FLIPDIAGONAL }
	};

	if( !R_ScreenEnabled() || !rsh.worldModel )
		return;

	if( ri.Cmd_Argc() != 3 )
	{
		Com_Printf( "usage: envshot <name> <size>\n" );
		return;
	}

	maxSize = min( min( glConfig.width, glConfig.height ), glConfig.maxTextureSize );
	if( maxSize > atoi( ri.Cmd_Argv( 2 ) ) )
		maxSize = atoi( ri.Cmd_Argv( 2 ) );

	for( size = 1; size < maxSize; size <<= 1 ) ;
	if( size > maxSize )
		size >>= 1;

	writedir = ri.FS_WriteDirectory();
	gamedir = ri.FS_GameDirectory();
	checkname_size = strlen( writedir ) + 1 + strlen( gamedir ) + strlen( "/env/" ) + 
		strlen( ri.Cmd_Argv( 1 ) ) + 1 + strlen( cubemapShots[0].suf ) + 4 + 1;
	checkname = alloca( checkname_size );

	fd = rsc.refdef;
	fd.time = 0;
	//fd.x = fd.y = 0;
	fd.width = fd.height = size;
	fd.fov_x = fd.fov_y = 90;

	rn.farClip = R_DefaultFarClip();

	// do not render non-bmodel entities
	rn.renderFlags |= RF_CUBEMAPVIEW;
	rn.clipFlags = 15;
	rn.shadowGroup = NULL;
	rn.fbColorAttachment = rn.fbDepthAttachment = NULL;

	Vector4Set( rn.viewport, fd.x, glConfig.height - size - fd.y, size, size );
	Vector4Set( rn.scissor, fd.x, glConfig.height - size - fd.y, size, size );

	for( i = 0; i < 6; i++ )
	{
		AnglesToAxis( cubemapShots[i].angles, fd.viewaxis );

		R_RenderView( &fd );

		Q_snprintfz( checkname, checkname_size, "%s/%s/env/%s_%s", writedir, gamedir, ri.Cmd_Argv( 1 ), cubemapShots[i].suf );
		COM_DefaultExtension( checkname, ".tga", checkname_size );

		R_ScreenShot( checkname, 0, 0, size, size, 100, 
			( cubemapShots[i].flags & IT_FLIPX ) ? true : false, 
			( cubemapShots[i].flags & IT_FLIPY ) ? true : false, 
			( cubemapShots[i].flags & IT_FLIPDIAGONAL ) ? true : false,
			false );
	}

	// render non-bmodel entities again
	rn.renderFlags &= ~RF_CUBEMAPVIEW;
}

/*
* R_GlobFilter
*/
static bool R_GlobFilter( const char *pattern, const char *value )
{
	if( *pattern && !glob_match( pattern, value, 0 ) ) {
		return false;
	}
	return true;
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
