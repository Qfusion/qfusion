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
#include "r_frontend.h"
#include "../../qalgo/glob.h"

/*
 * R_Localtime
 */
static struct tm *R_Localtime( const time_t time, struct tm* _tm ) {
#ifdef _WIN32
	struct tm* __tm = localtime( &time );
	*_tm = *__tm;
#else
	localtime_r( &time, _tm );
#endif
	return _tm;
}

/*
* R_TakeScreenShot
*/
void R_TakeScreenShot( const char *path, const char *name, const char *fmtString, int x, int y, int w, int h, bool silent ) {
	const char *extension = ".png";
	size_t path_size = strlen( path ) + 1;
	char *checkname = NULL;
	size_t checkname_size = 0;

	if( name && name[0] && Q_stricmp( name, "*" ) ) {
		if( !COM_ValidateRelativeFilename( name ) ) {
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
	if( !checkname ) {
		const int maxFiles = 100000;
		static int lastIndex = 0;
		bool addIndex = false;
		char timestampString[MAX_QPATH];
		static char lastFmtString[MAX_QPATH];
		struct tm newtime;

		R_Localtime( time( NULL ), &newtime );
		strftime( timestampString, sizeof( timestampString ), fmtString, &newtime );

		checkname_size = ( path_size - 1 ) + strlen( timestampString ) + 5 + 1 + strlen( extension );
		checkname = alloca( checkname_size );

		// if the string format is a constant or file already exists then iterate
		if( !*fmtString || !strcmp( timestampString, fmtString ) ) {
			addIndex = true;

			// force a rescan in case some vars have changed..
			if( strcmp( lastFmtString, fmtString ) ) {
				lastIndex = 0;
				Q_strncpyz( lastFmtString, fmtString, sizeof( lastFmtString ) );
				r_screenshot_fmtstr->modified = false;
			}
		} else {
			Q_snprintfz( checkname, checkname_size, "%s%s%s", path, timestampString, extension );
			if( ri.FS_FOpenAbsoluteFile( checkname, NULL, FS_READ ) != -1 ) {
				lastIndex = 0;
				addIndex = true;
			}
		}

		for( ; addIndex && lastIndex < maxFiles; lastIndex++ ) {
			Q_snprintfz( checkname, checkname_size, "%s%s%05i%s", path, timestampString, lastIndex, extension );
			if( ri.FS_FOpenAbsoluteFile( checkname, NULL, FS_READ ) == -1 ) {
				break; // file doesn't exist
			}
		}

		if( lastIndex == maxFiles ) {
			Com_Printf( "Couldn't create a file\n" );
			return;
		}

		lastIndex++;
	}

	// flip Y because 0,0 is bottom left in OpenGL
	R_ScreenShot( checkname, x, y, w, h, false, true, false, silent );
}

/*
* R_ScreenShot_f
*/
void R_ScreenShot_f( void ) {
	int i;
	const char *name;
	size_t path_size;
	char *path;
	char timestamp_str[MAX_QPATH];
	struct tm newtime;

	R_Localtime( time( NULL ), &newtime );

	name = ri.Cmd_Argv( 1 );

	path_size = strlen( ri.FS_WriteDirectory() ) + 1 /* '/' */ + strlen( ri.FS_GameDirectory() ) + strlen( "/screenshots/" ) + 1;
	path = alloca( path_size );
	Q_snprintfz( path, path_size, "%s/%s/screenshots/", ri.FS_WriteDirectory(), ri.FS_GameDirectory() );

	// validate timestamp string
	for( i = 0; i < 2; i++ ) {
		strftime( timestamp_str, sizeof( timestamp_str ), r_screenshot_fmtstr->string, &newtime );
		if( !COM_ValidateRelativeFilename( timestamp_str ) ) {
			ri.Cvar_ForceSet( r_screenshot_fmtstr->name, r_screenshot_fmtstr->dvalue );
		} else {
			break;
		}
	}

	// hm... shouldn't really happen, but check anyway
	if( i == 2 ) {
		ri.Cvar_ForceSet( r_screenshot_fmtstr->name, APP_SCREENSHOTS_PREFIX );
	}

	RF_ScreenShot( path, name, r_screenshot_fmtstr->string,
				   ri.Cmd_Argc() >= 3 && !Q_stricmp( ri.Cmd_Argv( 2 ), "silent" ) ? true : false );
}

/*
 * R_TakeEnvShot
 */
void R_TakeEnvShot( const char *path, const char *name, unsigned maxPixels ) {
	int i;
	unsigned size, maxSize;
	int checkname_size;
	char *checkname;
	refdef_t fd;
	struct  cubemapSufAndFlip {
		char *suf; vec3_t angles; int flags;
	} cubemapShots[6] = {
		{ "px", { 0, 0, 0 }, IT_FLIPX | IT_FLIPY | IT_FLIPDIAGONAL },
		{ "nx", { 0, 180, 0 }, IT_FLIPDIAGONAL },
		{ "py", { 0, 90, 0 }, IT_FLIPY },
		{ "ny", { 0, 270, 0 }, IT_FLIPX },
		{ "pz", { -90, 180, 0 }, IT_FLIPDIAGONAL },
		{ "nz", { 90, 180, 0 }, IT_FLIPDIAGONAL }
	};

	if( !rsh.worldModel ) {
		return;
	}

	maxSize = min( min( glConfig.width, glConfig.height ), glConfig.maxTextureSize );
	if( maxSize > maxPixels ) {
		maxSize = maxPixels;
	}

	for( size = 1; size < maxSize; size <<= 1 ) ;
	if( size > maxSize ) {
		size >>= 1;
	}

	checkname_size = strlen( path ) + strlen( name ) + 1 + strlen( cubemapShots[0].suf ) + 4 + 1;
	checkname = alloca( checkname_size );

	fd = rsc.refdef;
	fd.time = 0;
	//fd.x = fd.y = 0;
	fd.width = fd.height = size;
	fd.fov_x = fd.fov_y = 90;

	rn.nearClip = Z_NEAR;
	rn.farClip = R_DefaultFarClip();

	rn.polygonFactor = POLYOFFSET_FACTOR;
	rn.polygonUnits = POLYOFFSET_UNITS;

	// do not render non-bmodel entities
	rn.renderFlags |= RF_CUBEMAPVIEW;
	rn.clipFlags = 15;
	rn.renderTarget = 0;

	Vector4Set( rn.viewport, fd.x, glConfig.height - size - fd.y, size, size );
	Vector4Set( rn.scissor, fd.x, glConfig.height - size - fd.y, size, size );

	for( i = 0; i < 6; i++ ) {
		AnglesToAxis( cubemapShots[i].angles, fd.viewaxis );

		R_SetupViewMatrices( &fd );

		R_SetupFrustum( &fd, rn.nearClip, rn.farClip, rn.frustum, rn.frustumCorners );

		R_SetupPVS( &fd );

		R_RenderView( &fd );

		Q_snprintfz( checkname, checkname_size, "%s%s_%s", path, name, cubemapShots[i].suf );
		COM_DefaultExtension( checkname, ".tga", checkname_size );

		R_ScreenShot( checkname, 0, 0, size, size,
					  ( cubemapShots[i].flags & IT_FLIPX ) ? true : false,
					  ( cubemapShots[i].flags & IT_FLIPY ) ? true : false,
					  ( cubemapShots[i].flags & IT_FLIPDIAGONAL ) ? true : false,
					  false );
	}

	// render non-bmodel entities again
	rn.renderFlags &= ~RF_CUBEMAPVIEW;
}

/*
* R_EnvShot_f
*/
void R_EnvShot_f( void ) {
	const char *writedir, *gamedir;
	int path_size;
	char *path;

	if( !rsh.worldModel ) {
		return;
	}

	if( ri.Cmd_Argc() != 3 ) {
		Com_Printf( "usage: envshot <name> <size>\n" );
		return;
	}

	writedir = ri.FS_WriteDirectory();
	gamedir = ri.FS_GameDirectory();
	path_size = strlen( writedir ) + 1 + strlen( gamedir ) + 1 + strlen( "env/" ) + 1;
	path = alloca( path_size );
	Q_snprintfz( path, path_size, "%s/%s/env/", writedir, gamedir );

	RF_EnvShot( path, ri.Cmd_Argv( 1 ), atoi( ri.Cmd_Argv( 2 ) ) );
}

/*
* R_GlobFilter
*/
static bool R_GlobFilter( const char *pattern, const char *value ) {
	if( *pattern && !glob_match( pattern, value, 0 ) ) {
		return false;
	}
	return true;
}

/*
* R_ImageList_f
*/
void R_ImageList_f( void ) {
	R_PrintImageList( ri.Cmd_Argv( 1 ), R_GlobFilter );
}

/*
* R_ShaderList_f
*/
void R_ShaderList_f( void ) {
	R_PrintShaderList( ri.Cmd_Argv( 1 ), R_GlobFilter );
}

/*
* R_ShaderDump_f
*/
void R_ShaderDump_f( void ) {
	const char *name;
	const msurface_t *debugSurface;

	debugSurface = R_GetDebugSurface();

	if( ( ri.Cmd_Argc() < 2 ) && !debugSurface ) {
		Com_Printf( "Usage: %s [name]\n", ri.Cmd_Argv( 0 ) );
		return;
	}

	if( ri.Cmd_Argc() < 2 ) {
		name = debugSurface->shader->name;
	} else {
		name = ri.Cmd_Argv( 1 );
	}

	R_PrintShaderCache( name );
}
