/*
Copyright (C) 2016 SiPlus, Warsow development team

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

#include "../qalgo/q_trie.h"
#include "../qcommon/qcommon.h"
#include "sys_vfs_zip.h"

#define VFS_ZIP_BUFSIZE				0x00004000

#define VFS_ZIP_BUFREADCOMMENT		0x00000400
#define VFS_ZIP_SIZELOCALHEADER		0x0000001e
#define VFS_ZIP_SIZECENTRALDIRITEM	0x0000002e

#define VFS_ZIP_LOCALHEADERMAGIC	0x04034b50
#define VFS_ZIP_CENTRALHEADERMAGIC	0x02014b50
#define VFS_ZIP_ENDHEADERMAGIC		0x06054b50

static mempool_t *sys_vfs_zip_mempool;

typedef struct
{
	char *name;
	bool isDirectory;
	int vfs;
	unsigned offset, size;
} sys_vfs_zip_file_t;

typedef struct
{
	const char *name;
	int numFiles;
	sys_vfs_zip_file_t *files;
	void *handle;
} sys_vfs_zip_vfs_t;

static int sys_vfs_zip_numvfs;
static sys_vfs_zip_vfs_t *sys_vfs_zip_files;
static trie_t *sys_vfs_zip_trie;

static inline unsigned int LittleLongRaw( const uint8_t *raw )
{
	return ( raw[3] << 24 ) | ( raw[2] << 16 ) | ( raw[1] << 8 ) | raw[0];
}

static inline unsigned short LittleShortRaw( const uint8_t *raw )
{
	return ( raw[1] << 8 ) | raw[0];
}

static unsigned Sys_VFS_Zip_SearchCentralDir( FILE *fin )
{
	unsigned fileSize, backRead;
	unsigned maxBack = 0xffff; // maximum size of global comment
	unsigned char buf[VFS_ZIP_BUFREADCOMMENT+4];

	if( fseek( fin, 0, SEEK_END ) )
		return 0;

	fileSize = ftell( fin );
	if( maxBack > fileSize )
		maxBack = fileSize;

	backRead = 4;
	while( backRead < maxBack )
	{
		unsigned i, readSize, readPos;

		if( backRead + VFS_ZIP_BUFREADCOMMENT > maxBack )
			backRead = maxBack;
		else
			backRead += VFS_ZIP_BUFREADCOMMENT;

		readPos = fileSize - backRead;
		readSize = min( VFS_ZIP_BUFREADCOMMENT + 4, backRead );
		if( readSize < 4 )
			continue;

		if( fseek( fin, readPos, SEEK_SET ) != 0 )
			break;
		if( !fread( buf, readSize, 1, fin ) )
			break;

		for( i = readSize - 3; i--; )
		{
			// check the magic
			if( LittleLongRaw( buf + i ) == VFS_ZIP_ENDHEADERMAGIC )
				return readPos + i;
		}
	}

	return 0;
}

static unsigned Sys_VFS_Zip_GetFileInfo( FILE *f, unsigned pos, unsigned byteBeforeTheZipFile,
	char *name, size_t *nameLen, unsigned *offset, unsigned *size )
{
	size_t sizeRead;
	unsigned char infoHeader[46], localHeader[31]; // we can't use a struct here because of packing
	unsigned localOffset;
	unsigned uncompressedSize;

	if( fseek( f, pos, SEEK_SET ) )
		return 0;
	if( !fread( infoHeader, sizeof( infoHeader ), 1, f ) )
		return 0;

	// check the magic
	if( LittleLongRaw( &infoHeader[0] ) != VFS_ZIP_CENTRALHEADERMAGIC )
		return 0;
	// must be uncompressed
	if( LittleShortRaw( &infoHeader[10] ) )
		return 0;

	sizeRead = ( size_t )LittleShortRaw( &infoHeader[28] );
	if( !sizeRead )
		return 0;
	if( nameLen )
		*nameLen = sizeRead;

	if( name )
	{
		if( !fread( name, sizeRead, 1, f ) )
			return 0;
		*( name + sizeRead ) = 0;
	}

	localOffset = LittleLongRaw( &infoHeader[42] ) + byteBeforeTheZipFile;
	if( fseek( f, localOffset, SEEK_SET ) )
		return 0;
	if( !fread( localHeader, sizeof( localHeader ), 1, f ) )
		return 0;

	// check the magic
	if( LittleLongRaw( &localHeader[0] ) != VFS_ZIP_LOCALHEADERMAGIC )
		return 0;
	// must be uncompressed
	if( LittleShortRaw( &localHeader[8] ) )
		return 0;
	uncompressedSize = LittleLongRaw( &localHeader[22] );
	if( uncompressedSize != LittleLongRaw( &infoHeader[24] ) )
		return 0;

	if( offset )
	{
		*offset = localOffset + VFS_ZIP_SIZELOCALHEADER + LittleShortRaw( &localHeader[26] ) +
			( unsigned )LittleShortRaw( &localHeader[28] );
	}
	if( size )
		*size = uncompressedSize;

	return VFS_ZIP_SIZECENTRALDIRITEM + ( unsigned )LittleShortRaw( &infoHeader[28] ) +
		( unsigned )LittleShortRaw( &infoHeader[30] ) + ( unsigned )LittleShortRaw( &infoHeader[32] );
}

static void Sys_VFS_Zip_LoadVFS( int idx, const char *filename )
{
	int i;
	int numFiles;
	size_t namesLen = 0, len;
	char *names;
	FILE *fin = NULL;
	unsigned char zipHeader[20]; // we can't use a struct here because of packing
	unsigned offset, centralPos, sizeCentralDir, offsetCentralDir, byteBeforeTheZipFile;
	void *handle = NULL;
	sys_vfs_zip_vfs_t *vfs;
	sys_vfs_zip_file_t *file, *trie_file;
	trie_error_t trie_err;

	handle = Sys_FS_LockFile( filename );
	if( !handle )
	{
		Com_Printf( "Error locking VFS zip file: %s\n", filename );
		goto end;
	}

	fin = fopen( filename, "rb" );
	if( !fin )
	{
		Com_Printf( "Error opening VFS zip file: %s\n", filename );
		goto end;
	}
	centralPos = Sys_VFS_Zip_SearchCentralDir( fin );
	if( !centralPos )
	{
		Com_Printf( "No central directory found for VFS zip file: %s\n", filename );
		goto end;
	}
	if( fseek( fin, centralPos, SEEK_SET ) )
	{
		Com_Printf( "Error seeking VFS zip file: %s\n", filename );
		goto end;
	}
	if( !fread( zipHeader, sizeof( zipHeader ), 1, fin ) )
	{
		Com_Printf( "Error reading VFS zip file: %s\n", filename );
		goto end;
	}

	numFiles = LittleShortRaw( &zipHeader[8] );
	if( !numFiles )
	{
		Com_Printf( "%s is not a valid VFS zip file\n", filename );
		goto end;
	}
	if( LittleShortRaw( &zipHeader[10] ) != numFiles || LittleShortRaw( &zipHeader[6] ) != 0
		|| LittleShortRaw( &zipHeader[4] ) != 0 )
	{
		Com_Printf( "%s is not a valid VFS zip file\n", filename );
		goto end;
	}

	sizeCentralDir = LittleLongRaw( &zipHeader[12] );
	offsetCentralDir = LittleLongRaw( &zipHeader[16] );
	if( centralPos < offsetCentralDir + sizeCentralDir )
	{
		Com_Printf( "%s is not a valid VFS zip file\n", filename );
		goto end;
	}
	byteBeforeTheZipFile = centralPos - offsetCentralDir - sizeCentralDir;

	for( i = 0, centralPos = offsetCentralDir + byteBeforeTheZipFile; i < numFiles; ++i, centralPos += offset )
	{
		offset = Sys_VFS_Zip_GetFileInfo( fin, centralPos, byteBeforeTheZipFile, NULL, &len, NULL, NULL );
		if( !offset )
		{
			Com_Printf( "%s is not a valid VFS zip file (may be compressed)\n", filename );
			goto end;
		}
		namesLen += len + 1;
	}

	vfs = &sys_vfs_zip_files[idx];
	vfs->name = Mem_CopyString( sys_vfs_zip_mempool, filename );
	vfs->numFiles = 0;
	vfs->files = Mem_Alloc( sys_vfs_zip_mempool, numFiles * sizeof( sys_vfs_zip_file_t ) + namesLen );
	file = vfs->files;
	names = ( char * )( vfs->files ) + numFiles * sizeof( sys_vfs_zip_file_t );
	for( i = 0, centralPos = offsetCentralDir + byteBeforeTheZipFile; i < numFiles; ++i, centralPos += offset )
	{
		offset = Sys_VFS_Zip_GetFileInfo( fin, centralPos, byteBeforeTheZipFile, names, &len, &file->offset, &file->size );
		if( !offset )
		{
			Com_Printf( "%s is not a valid VFS zip file (may be compressed)\n", filename );
			goto end;
		}
		if( names[0] )
		{
			file->name = names;
			file->vfs = idx;
			file->isDirectory = ( names[len - 1] == '/' );

			trie_file = NULL;
			trie_err = Trie_Replace( sys_vfs_zip_trie, names, file, ( void ** )&trie_file );
			if( trie_err == TRIE_KEY_NOT_FOUND )
				Trie_Insert( sys_vfs_zip_trie, names, file );
			else if( trie_file )
				trie_file->name = NULL; // make Sys_VFS_Zip_ListFiles skip already existing file if new vfs overrides it

			++vfs->numFiles;
			++file;
			names += len + 1;
		}
	}

	vfs->handle = handle;
	handle = NULL; // don't unlock now

end:
	if( fin )
		fclose( fin );
	if( handle )
		Sys_FS_UnlockFile( handle );
}

void Sys_VFS_Zip_Init( int numvfs, const char * const *vfsnames )
{
	int i;

	if( !numvfs )
		return;

	sys_vfs_zip_mempool = Mem_AllocPool( NULL, "Zip VFS" );
	sys_vfs_zip_numvfs = numvfs;
	sys_vfs_zip_files = Mem_Alloc( sys_vfs_zip_mempool, numvfs * sizeof( sys_vfs_zip_vfs_t ) );

	Trie_Create( TRIE_CASE_INSENSITIVE, &sys_vfs_zip_trie );
	for( i = 0; i < numvfs; ++i )
		Sys_VFS_Zip_LoadVFS( i, vfsnames[i] );
}

char **Sys_VFS_Zip_ListFiles( const char *pattern, const char *prependBasePath, int *numFiles, bool listFiles, bool listDirs )
{
	int i, j;
	sys_vfs_zip_vfs_t *vfs;
	int nFiles = 0;
	char **list;
	sys_vfs_zip_file_t *file;
	const char *name;
	size_t nameSize;
	size_t basePathLength = 0;

	for( i = 0, vfs = sys_vfs_zip_files; i < sys_vfs_zip_numvfs; ++i, ++vfs )
		nFiles += vfs->numFiles;

	if( !nFiles )
	{
		*numFiles = 0;
		return NULL;
	}

	list = Mem_ZoneMalloc( ( nFiles + 1 ) * sizeof( char * ) );

	if( prependBasePath )
		basePathLength = strlen( prependBasePath ) + 1;

	nFiles = 0;
	for( i = 0, vfs = sys_vfs_zip_files; i < sys_vfs_zip_numvfs; ++i, ++vfs )
	{
		for( j = vfs->numFiles, file = vfs->files; j-- > 0; ++file )
		{
			if( ( !file->isDirectory && !listFiles ) || ( file->isDirectory && !listDirs ) )
				continue;
			name = file->name;
			if( !name )
				continue; // overriden by another VFS later in the list
			if( !Com_GlobMatch( pattern, name, false ) )
				continue;
			nameSize = basePathLength + strlen( name ) + 1;
			list[nFiles] = Mem_ZoneMalloc( nameSize );
			if( prependBasePath )
				Q_snprintfz( list[nFiles], nameSize, "%s/%s", prependBasePath, name );
			else
				Q_strncpyz( list[nFiles], name, nameSize );
			nFiles++;
		}
	}

	list[nFiles] = NULL;
	*numFiles = nFiles;
	return list;
}

void *Sys_VFS_Zip_FindFile( const char *filename )
{
	sys_vfs_zip_file_t *file = NULL;

	if( !sys_vfs_zip_numvfs )
		return NULL;

	Trie_Find( sys_vfs_zip_trie, filename, TRIE_EXACT_MATCH, ( void ** )&file );
	if( file && file->isDirectory )
		return NULL;

	return file;
}

const char *Sys_VFS_Zip_VFSName( void *handle )
{
	if( !handle )
		return NULL;
	return sys_vfs_zip_files[( ( const sys_vfs_zip_file_t * )handle )->vfs].name;
}

unsigned Sys_VFS_Zip_FileOffset( void *handle )
{
	if( !handle )
		return 0;
	return ( ( const sys_vfs_zip_file_t * )handle )->offset;
}

unsigned Sys_VFS_Zip_FileSize( void *handle )
{
	if( !handle )
		return 0;
	return ( ( const sys_vfs_zip_file_t * )handle )->size;
}

void Sys_VFS_Zip_Shutdown( void )
{
	int i;
	sys_vfs_zip_vfs_t *vfs;

	if( !sys_vfs_zip_numvfs )
		return;

	for( i = 0, vfs = sys_vfs_zip_files; i < sys_vfs_zip_numvfs; ++i, ++vfs )
	{
		if( vfs->handle )
			Sys_FS_UnlockFile( vfs->handle );
	}

	Trie_Destroy( sys_vfs_zip_trie );
	Mem_FreePool( &sys_vfs_zip_mempool );

	sys_vfs_zip_numvfs = 0;
}
