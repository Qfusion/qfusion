/*
Copyright (C) 2002-2007 Victor Luchits

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

#include "r_local.h"

typedef struct {
	char                *meshname;
	shader_t            *shader;
} mesh_shader_pair_t;

typedef struct skinfile_s {
	char                *name;
	int registrationSequence;

	mesh_shader_pair_t  *pairs;
	int numpairs;
} skinfile_t;

static int r_numskinfiles;
static skinfile_t r_skinfiles[MAX_SKINFILES];

/*
* R_InitSkinFiles
*/
void R_InitSkinFiles( void ) {
	memset( r_skinfiles, 0, sizeof( r_skinfiles ) );
}

/*
* SkinFile_FreeSkinFile
*/
static void SkinFile_FreeSkinFile( skinfile_t *skinfile ) {
	int i;

	if( skinfile->numpairs ) {
		for( i = 0; i < skinfile->numpairs; i++ )
			R_Free( skinfile->pairs[i].meshname );
		R_Free( skinfile->pairs );
	}

	R_Free( skinfile->name );

	memset( skinfile, 0, sizeof( skinfile_t ) );
}

/*
* R_FindShaderForSkinFile
*/
shader_t *R_FindShaderForSkinFile( const skinfile_t *skinfile, const char *meshname ) {
	int i;
	mesh_shader_pair_t *pair;

	if( !skinfile || !skinfile->numpairs ) {
		return NULL;
	}

	for( i = 0, pair = skinfile->pairs; i < skinfile->numpairs; i++, pair++ ) {
		if( !Q_stricmp( pair->meshname, meshname ) ) {
			return pair->shader;
		}
	}

	return NULL;
}

/*
* SkinFile_ParseBuffer
*/
static int SkinFile_ParseBuffer( char *buffer, mesh_shader_pair_t *pairs ) {
	int numpairs;
	char *ptr, *t, *token;

	ptr = buffer;
	numpairs = 0;

	while( ptr ) {
		token = COM_ParseExt( &ptr, false );
		if( !token[0] ) {
			continue;
		}

		t = strchr( token, ',' );
		if( !t ) {
			continue;
		}
		if( *( t + 1 ) == '\0' || *( t + 1 ) == '\n' ) {
			continue;
		}

		if( pairs ) {
			*t = 0;
			pairs[numpairs].meshname = R_CopyString( token );
			pairs[numpairs].shader = R_RegisterSkin( token + strlen( token ) + 1 );
		}

		numpairs++;
	}

	return numpairs;
}

/*
* R_SkinFileForName
*/
static skinfile_t *R_SkinFileForName( const char *name ) {
	char filename[MAX_QPATH];
	int i;
	char *buffer;
	skinfile_t *skinfile;

	Q_strncpyz( filename, name, sizeof( filename ) );
	COM_DefaultExtension( filename, ".skin", sizeof( filename ) );

	for( i = 0, skinfile = r_skinfiles; i < r_numskinfiles; i++, skinfile++ ) {
		if( !skinfile->name ) {
			break; // free slot
		}
		if( !Q_stricmp( skinfile->name, filename ) ) {
			return skinfile;
		}
	}

	if( i == MAX_SKINFILES ) {
		Com_Printf( S_COLOR_YELLOW "R_SkinFile_Load: Skin files limit exceeded\n" );
		return NULL;
	}

	if( R_LoadFile( filename, (void **)&buffer ) == -1 ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_SkinFile_Load: Failed to load %s\n", name );
		return NULL;
	}

	r_numskinfiles++;
	skinfile = &r_skinfiles[i];
	skinfile->name = R_CopyString( filename );

	skinfile->numpairs = SkinFile_ParseBuffer( buffer, NULL );
	if( skinfile->numpairs ) {
		skinfile->pairs = ( mesh_shader_pair_t * ) R_Malloc( skinfile->numpairs * sizeof( mesh_shader_pair_t ) );
		SkinFile_ParseBuffer( buffer, skinfile->pairs );
	} else {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_SkinFile_Load: no mesh/shader pairs in %s\n", name );
	}

	R_FreeFile( (void *)buffer );

	return skinfile;
}

/*
* R_RegisterSkinFile
*/
skinfile_t *R_RegisterSkinFile( const char *name ) {
	skinfile_t *skinfile;

	skinfile = R_SkinFileForName( name );
	if( skinfile && skinfile->registrationSequence != rsh.registrationSequence ) {
		int i;

		skinfile->registrationSequence = rsh.registrationSequence;
		for( i = 0; i < skinfile->numpairs; i++ ) {
			R_TouchShader( skinfile->pairs[i].shader );
		}
	}
	return skinfile;
}

/*
* R_FreeUnusedSkinFiles
*/
void R_FreeUnusedSkinFiles( void ) {
	int i;
	skinfile_t *skinfile;

	for( i = 0, skinfile = r_skinfiles; i < r_numskinfiles; i++, skinfile++ ) {
		if( skinfile->registrationSequence == rsh.registrationSequence ) {
			// we need this skin
			continue;
		}
		SkinFile_FreeSkinFile( skinfile );
	}
}

/*
* R_ShutdownSkinFiles
*/
void R_ShutdownSkinFiles( void ) {
	int i;
	skinfile_t *skinfile;

	for( i = 0, skinfile = r_skinfiles; i < r_numskinfiles; i++, skinfile++ ) {
		if( !skinfile->name ) {
			continue;
		}
		SkinFile_FreeSkinFile( skinfile );
	}

	r_numskinfiles = 0;
}
