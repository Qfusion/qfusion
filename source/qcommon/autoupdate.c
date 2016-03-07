/*
Copyright (C) 2015 Victor Luchits

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

#include "qcommon.h"
#include "asyncstream.h"

#define AU_BASE_URL APP_UPDATE_URL APP_SERVER_UPDATE_DIRECTORY
#define AU_LIST_FILE APP_SERVER_UPDATE_FILE

#define AU_MemAlloc(s) _AU_MemAlloc(s,__FILE__,__LINE__)
#define AU_MemFree(f) _AU_MemFree(f,__FILE__,__LINE__)
#define AU_CopyString(s) ZoneCopyString(s)

typedef struct filedownload_s {
	char *url;
	char *filepath;
	char *temppath;
	char *writepath;
	int filenum;
	unsigned checksum;
	void (*done_cb)(struct filedownload_s *, int);
	struct filedownload_s *next;
} filedownload_t;

static async_stream_module_t *au_async_stream;

static bool au_safe_guard; // prevent multiple cancels/shutdowns

static bool au_check_only;
static char *au_remote_list;
static size_t au_remote_list_size;
static filedownload_t *au_download_head;
static int au_download_count;
static int au_download_errcount;
static void (*au_newfiles_callback)( void );

/*
* AU_MemAlloc
*/
static void *_AU_MemAlloc( size_t size, const char *filename, int fileline ) {
	return _Mem_Alloc( zoneMemPool, size, 0, 0, filename, fileline );
}

/*
* AU_MemFree
*/
static void _AU_MemFree( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, 0, 0, filename, fileline );
}

// ============================================================================

/*
* AU_FileDoneCb
*/
static void AU_FileDoneCb( int status, const char *contentType, void *privatep )
{
	filedownload_t *fd = privatep;
	FS_FCloseFile( fd->filenum );
	fd->done_cb( fd, status );
}

/*
* AU_FileReadCb
*/
static size_t AU_FileReadCb( const void *buf, size_t numb, float percentage, 
	int status, const char *contentType, void *privatep )
{
	filedownload_t *fd = privatep;
	if( status < 0 || status >= 300 ) {
		return 0;
	}
	return FS_Write( buf, numb, fd->filenum );
}

/*
* AU_FreeDownload
*/
static void AU_FreeDownload( filedownload_t *fd )
{
	AU_MemFree( fd->url );
	AU_MemFree( fd->filepath );
	AU_MemFree( fd->temppath );
	AU_MemFree( fd->writepath );
	AU_MemFree( fd );
}

/*
* AU_AllocDownload
*/
static filedownload_t *AU_AllocDownload( void )
{
	filedownload_t *fd = AU_MemAlloc( sizeof( *fd ) );
	return fd;
}

/*
* AU_DownloadFile
*/
static filedownload_t *AU_DownloadFile( const char *baseUrl, const char *filepath, bool silent, 
	unsigned checksum, void (*done_cb)(struct filedownload_s *, int) )
{
	int fsize, fnum;
	int alloc_size;
	char *temppath, *writepath, *url;
	filedownload_t *fd;

	if( !baseUrl || !baseUrl[0] || !filepath )
		return NULL;

	if( !strrchr( baseUrl, '/' ) )
	{
		if( !silent )
			Com_Printf( "SV_WebDownload: Invalid URL\n" );
		return NULL;
	}

	if( filepath[0] == '/' ) // filepath should never begin with a slash
		filepath++;

	// full url (baseurl + path)
	alloc_size = strlen( baseUrl ) + 1 + strlen( filepath ) + 1;
	url = AU_MemAlloc( alloc_size );
	if( baseUrl[ strlen( baseUrl ) - 1 ] == '/' ) // url includes last slash
		Q_snprintfz( url, alloc_size, "%s%s", baseUrl, filepath );
	else
		Q_snprintfz( url, alloc_size, "%s/%s", baseUrl, filepath );

	// add .tmp (relative + .tmp)
	alloc_size = strlen( filepath ) + 128 + strlen( ".tmp" ) + 1;
	temppath = AU_MemAlloc( alloc_size );
	Q_snprintfz( temppath, alloc_size, "%s.%i.tmp", filepath, Sys_GetCurrentProcessId() );

	// full write path for curl
	alloc_size = strlen( FS_WriteDirectory() ) + 1 + strlen( temppath ) + 1;
	writepath = AU_MemAlloc( alloc_size );
	Q_snprintfz( writepath, alloc_size, "%s/%s", FS_WriteDirectory(), temppath );

	fsize = FS_FOpenAbsoluteFile( writepath, &fnum, FS_APPEND );

	fd = AU_AllocDownload();
	fd->url = url;
	fd->filenum = fnum;
	fd->filepath = AU_CopyString( filepath );
	fd->temppath = temppath;
	fd->writepath = writepath;
	fd->checksum = checksum;
	fd->done_cb = done_cb;

	// test if file exist
	if( !fnum )
	{
		if( !silent )
			Com_Printf( "Failed to open %s for writing\n", writepath );
		AU_FreeDownload( fd );
		return NULL;
	}

	Com_DPrintf( "Downloading %s from %s, pos %i\n", filepath, url, fsize );

	AsyncStream_PerformRequest( au_async_stream, url, 
		"GET", NULL, NULL, 60, fsize, AU_FileReadCb, AU_FileDoneCb, fd );

	return fd;
}

// ============================================================================

/*
* AU_ClearDownloads
*/
static void AU_ClearDownloads( void )
{
	au_download_head = NULL;
	au_download_count = 0;
	au_download_errcount = 0;
}

/*
* AU_FinishDownload
*/
static void AU_FinishDownload( filedownload_t *fd_, int status )
{
	filedownload_t *fd, *next;

	au_download_count--;
	if( status != 200 ) {
		au_download_errcount++;
		if( au_download_errcount == 1 ) {
			Com_Printf( "AU_FinishDownload: Failed to download %s, rolling back...\n", fd_->url );
			Com_Autoupdate_Cancel();
		}
		return;
	}

	if( au_download_count ) {
		return;
	}

	// done downloading all files

	if( !au_download_errcount ) {
		const char *temppath, *filepath;

		// rename downloaded files
		for( fd = au_download_head; fd; fd = fd->next ) {
			temppath = fd->temppath;
			filepath = fd->filepath;

			if( fd->checksum ) {
				unsigned checksum = FS_ChecksumBaseFile( temppath, true );
				if( checksum != fd->checksum ) {
					Com_Printf( "AU_FinishDownload: checksum mismatch for %s. Expected %u, got %u\n", temppath, fd->checksum, checksum );
					au_download_errcount++;
					break;
				}
			}

			if( FS_MoveBaseFile( temppath, filepath ) )
				continue;

			// check if it failed because there already exists a file with the same name
			// and in this case remove this file
			if( FS_FOpenBaseFile( filepath, NULL, FS_READ ) == -1 ) {
				Com_Printf( "AU_FinishDownload: failed to rename temporary file for unknown reason.\n" );
				au_download_errcount++;
			}
			else {
				char *backfile;
				size_t alloc_size;

				alloc_size = strlen( filepath ) + strlen( ".bak" ) + 1;
				backfile = Mem_TempMalloc( alloc_size );
				Q_snprintfz( backfile, alloc_size, "%s.bak", filepath );

				// if there is already a .bak file, destroy it
				if( FS_FOpenBaseFile( backfile, NULL, FS_READ ) != -1 )
					FS_RemoveBaseFile( backfile );

				// move the current file into .bak file
				if( !FS_MoveBaseFile( filepath, backfile ) ) {
					Com_Printf( "AU_FinishDownload: Failed to backup destination file.\n" );
					au_download_errcount++;
				}
				else {
					// now try renaming the downloaded file again
					if( !FS_MoveBaseFile( temppath, filepath ) )
					{
						// didn't work, so restore the backup file
						if( FS_MoveBaseFile( backfile, filepath ) )
							Com_Printf( "AU_FinishDownload: Failed to rename temporary file, restoring from backup.\n" );
						else
							Com_Printf( "AU_FinishDownload: failed to rename temporary file and restore from backup.\n" );
						au_download_errcount++;
					}
				}

				Mem_TempFree( backfile );
			}

			if( au_download_errcount )
				break;
		}
	}

	if( au_download_errcount ) {
		// error, remove all temp files
		for( fd = au_download_head; fd; fd = fd->next ) {
			FS_RemoveAbsoluteFile( fd->writepath );
			FS_RemoveBaseFile( fd->temppath );
		}
	}

	if( !au_download_errcount && au_newfiles_callback ) {
		au_newfiles_callback();
	}

	for( fd = au_download_head; fd; fd = next ) {
		next = fd->next;
		AU_FreeDownload( fd );
	}

	AU_ClearDownloads();

	Com_Printf( "========== Auto Update Finished ===========\n" );
}

/*
* AU_ParseUpdateList
*/
static void AU_ParseUpdateList( const char *data, bool checkOnly )
{
	const char *ptr = (const char *)data;
	unsigned int checksum, expected_checksum;
	const char *token;
	char path[MAX_TOKEN_CHARS];
	char newVersionTag[MAX_QPATH];
	bool newVersion = false;

	// first token is always the current release version
	token = COM_ParseExt( &ptr, true );
	if( !token[0] )
		return;

	// compare versions
	Q_strncpyz( newVersionTag, token, sizeof( newVersionTag ) );
	if( atof( newVersionTag ) > atof( va( "%4.3f", APP_VERSION ) ) )
		newVersion = true;

	AU_ClearDownloads();

	while( ptr )
	{
		filedownload_t *fd;

		// we got what should be a checksum
		token = COM_ParseExt( &ptr, true );
		if( !token[0] )
			return;

		expected_checksum = strtoul( token, NULL, 10 );

		// get filename
		token = COM_ParseExt( &ptr, false );
		if( !token[0] )
			return;

		// filename should never begin with a slash
		if( token[0] == '/' )
			token++;

		// we got what should be a file path
		if( !COM_ValidateRelativeFilename( token ) )
		{
			Com_Printf( "AU_ParseUpdateList: Invalid filename %s\n", token );
			goto skip_line;
		}

		if( !COM_FileExtension( token ) )
		{
			Com_Printf( "AU_ParseUpdateList: no file extension\n" );
			goto skip_line;
		}

		Q_strncpyz( path, token, sizeof( path ) );

		checksum = FS_ChecksumBaseFile( token, false );

		// if same checksum no need to update
		if( checksum == expected_checksum )
			goto skip_line;

		// if it's a pack file and the file exists it can't be replaced, so skip
		if( FS_CheckPakExtension( path ) && checksum )
		{
			Com_Printf( "WARNING: Purity check failed for: %s\n", path );
			Com_Printf( "WARNING: This file has been locally modified. It is highly \n" );
			Com_Printf( "WARNING: recommended to restore the original file.\n" );
			Com_Printf( "WARNING: Reinstalling \""APPLICATION"\" might be convenient.\n" );
			goto skip_line;
		}

		if( checkOnly )
		{
			Com_Printf( "File update available: %s\n", path );
			goto skip_line;
		}

		// check optional md5-digest checksum
		expected_checksum = 0;
		token = COM_ParseExt( &ptr, false );
		if( token[0] ) {
			expected_checksum = strtoul( token, NULL, 10 );
			if( expected_checksum == ULONG_MAX ) {
				expected_checksum = 0;
			}
		}

		if( developer->integer )
			Com_Printf( "Downloading update of %s (checksum %u local checksum %u)\n", path, expected_checksum, checksum );
		else
			Com_Printf( "Updating %s\n", path );

		fd = AU_DownloadFile( AU_BASE_URL, path, false, expected_checksum, &AU_FinishDownload );
		if( !fd )
		{
			Com_Printf( "Failed to update %s\n", path );
			return;
		}

		fd->next = au_download_head;
		au_download_head = fd;
		au_download_count++;

skip_line:
		while( token[0] ) {
			token = COM_ParseExt( &ptr, false );
		}
	}

	if( newVersion ) {
		Com_Printf( "****** Version %s of "APPLICATION" is available. ******\n", newVersionTag );
		Com_Printf( "****** Please download the new version at "APP_URL" ******\n" );
	}
}

/*
* AU_ListReadCb
*/
static size_t AU_ListReadCb( const void *buf, size_t numb, float percentage, 
	int status, const char *contentType, void *privatep )
{
	char *newbuf;

	if( status < 0 || status >= 300 ) {
		return 0;
	}

	newbuf = Mem_ZoneMalloc( au_remote_list_size + numb + 1 );
	memcpy( newbuf, au_remote_list, au_remote_list_size - 1 );
	memcpy( newbuf + au_remote_list_size - 1, buf, numb );
	newbuf[numb] = '\0'; // EOF

	Mem_Free( au_remote_list );
	au_remote_list = newbuf;
	au_remote_list_size = au_remote_list_size + numb + 1;

	return numb;
}

/*
* AU_ListDoneCb
*/
static void AU_ListDoneCb( int status, const char *contentType, void *privatep )
{
	if( status != 200 )
		goto done;

	if( au_remote_list )
		AU_ParseUpdateList( au_remote_list, au_check_only );
	
done:
	if( au_remote_list )
	{
		Mem_Free( au_remote_list );
		au_remote_list = NULL;
		au_remote_list_size = 0;
	}
}

/*
* AU_FetchUpdateList
*/
static void AU_FetchUpdateList( bool checkOnly, void (*newfiles_cb)(void) )
{
	if( au_remote_list )
		return;
	if( au_download_count )
		return;

	au_remote_list_size = 1;
	au_remote_list = Mem_ZoneMalloc( 1 );
	*au_remote_list = '\0';

	au_check_only = checkOnly;
	au_newfiles_callback = newfiles_cb;

	AsyncStream_PerformRequest( au_async_stream, AU_BASE_URL AU_LIST_FILE, 
		"GET", NULL, NULL, 60, 0, AU_ListReadCb, AU_ListDoneCb, false );
}

// ============================================================================

/*
* Com_Autoupdate_Init
*/
void Com_Autoupdate_Init( void )
{
	if( au_async_stream )
		return;
	if( au_safe_guard )
		return;

	au_async_stream = AsyncStream_InitModule( "Update", _AU_MemAlloc, _AU_MemFree );
	
	au_remote_list = NULL;
	au_remote_list_size = 0;

	AU_ClearDownloads();
}

/*
* Com_Autoupdate_Run
*/
void Com_Autoupdate_Run( bool checkOnly, void (*newfiles_cb)(void) )
{
	if( au_safe_guard )
		return;

	AU_FetchUpdateList( checkOnly, newfiles_cb );
}

/*
* Com_Autoupdate_Cancel
*/
void Com_Autoupdate_Cancel( void )
{
	if( au_safe_guard )
		return;

	Com_Autoupdate_Shutdown();
	Com_Autoupdate_Init();
}

/*
* Com_Autoupdate_Shutdown
*/
void Com_Autoupdate_Shutdown( void )
{
	if( !au_async_stream )
		return;

	au_safe_guard = true;

	AsyncStream_ShutdownModule( au_async_stream );

	au_safe_guard = false;

	au_async_stream = NULL;
}
