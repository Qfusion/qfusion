/*
Copyright (C) 2011 Christian Holmberg

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
#include "../qalgo/base64.h"
#include "../qcommon/wswcurl.h"

cvar_t *mm_url;

static bool mm_initialized = false;

//==================================================



//==================================================

// returns static internal string
static const char *MM_PasswordFilename( const char *user ) {
	static char filename[MAX_STRING_CHARS];
	char *user64;

	user64 = (char*)base64_encode( (unsigned char*)user, strlen( user ), NULL );

	Q_snprintfz( filename, sizeof( filename ), "%s.profile", user64 );

	free( user64 );

	return filename;
}

// returns static string of md5 contents
const char *MM_PasswordRead( const char *user ) {
	static char buffer[MAX_STRING_CHARS];
	const char *filename;
	int filenum;
	size_t bytes;

	Com_DPrintf( "MM_PasswordRead %s\n", user );

	filename = MM_PasswordFilename( user );
	if( FS_FOpenFile( filename, &filenum, FS_READ ) == -1 ) {
		Com_Printf( "MM_PasswordRead: Couldnt open file %s\n", filename );
		return NULL;
	}

	bytes = FS_Read( buffer, sizeof( buffer ) - 1,  filenum );
	FS_FCloseFile( filenum );

	if( bytes == 0 || bytes >= sizeof( buffer ) - 1 ) {
		return NULL;
	}

	buffer[bytes] = '\0';

	return buffer;
}

void MM_PasswordWrite( const char *user, const char *password ) {
	const char *filename;
	int filenum;

	Com_DPrintf( "MM_PasswordWrite: %s %s\n", user, password );

	filename = MM_PasswordFilename( user );
	if( FS_FOpenFile( filename, &filenum, FS_WRITE ) == -1 ) {
		Com_Printf( "MM_PasswordWrite: Failed to open %s for writing\n", filename );
		return;
	}

	FS_Write( password, strlen( password ), filenum );
	FS_FCloseFile( filenum );

	Com_DPrintf( "MM_PasswordWrite: wrote %s\n", filename );
}

//==================================

// returns list of tokens and sets *argc into number of tokens
// you need to Mem_TempFree( ret[0] ) ; Mem_TempFree( ret )
char ** MM_ParseResponse( wswcurl_req *req, int *argc ) {
	size_t respSize;
	char *buffer, *realBuffer, *p, *end, **tokens;
	int numTokens, startOfs, endToken;

	if( argc ) {
		*argc = 0;
	}

	wswcurl_getsize( req, &respSize );
	if( !respSize ) {
		return 0;
	}

	// read the response string
	buffer = Mem_TempMalloc( respSize + 1 );
	respSize = wswcurl_read( req, buffer, respSize );
	buffer[respSize] = '\0';

	Com_DPrintf( "MM_ParseResponse: Parsing string\n%s\n", buffer );

	// count the number of tokens (could this be done in 1 pass?)
	p = buffer;
	end = buffer + respSize;
	numTokens = 0;
	startOfs = 0;
	while( p < end ) {
		// skip whitespace
		while( p < end && *p <= ' ' )
			p++;

		if( p >= end ) {
			break;
		}

		if( !numTokens ) {
			startOfs = p - buffer;
		}

		numTokens++;

		// skip the token
		while( p < end && *p > ' ' )
			p++;
	}

	// fail
	if( !numTokens ) {
		if( argc ) {
			*argc = 0;
		}
		Mem_TempFree( buffer );
		return NULL;
	}

	if( argc ) {
		*argc = numTokens;
	}
	tokens = Mem_TempMalloc( ( numTokens + 1 ) * sizeof( char* ) );

	// allocate the actual buffer that we are going to return
	if( startOfs > 0 ) {
		realBuffer = Mem_TempMalloc( ( respSize - startOfs ) + 1 );
		memcpy( realBuffer, buffer + startOfs, respSize - startOfs );
		Mem_TempFree( buffer );
		buffer = realBuffer;
		respSize -= startOfs;
		buffer[respSize] = '\0';
	}

	// 2nd pass, mark all tokens
	p = buffer;
	end = buffer + respSize;
	endToken = numTokens;
	numTokens = 0;
	while( p < end && numTokens < endToken ) {
		// we made the buffer point into the first character
		// so we can shuffle this loop a little
		tokens[numTokens++] = p;

		// skip the token
		while( p < end && *p > ' ' )
			p++;

		if( p >= end ) {
			break;
		}

		// skip whitespace
		while( p < end && *p <= ' ' )
			*p++ = '\0';

		Com_DPrintf( "MM_ParseResponse: parsed token %s\n", tokens[numTokens - 1] );
	}
	*p = '\0';  // we left room for 1 character
	tokens[numTokens] = 0;

	return tokens;
}

// free the result from mm_parseResponse2
void MM_FreeResponse( char **argv ) {
	if( argv ) {
		if( argv[0] ) {
			Mem_TempFree( argv[0] );
		}
		Mem_TempFree( argv );
	}
}

//==================================

void MM_Frame( const int realmsec ) {

}

void MM_Init( void ) {
	mm_initialized = false;

	mm_url = Cvar_Get( "mm_url", APP_MATCHMAKER_URL, CVAR_ARCHIVE | CVAR_NOSET );

	mm_initialized = true;
}

void MM_Shutdown( void ) {
	mm_url = NULL;
	mm_initialized = false;
}
