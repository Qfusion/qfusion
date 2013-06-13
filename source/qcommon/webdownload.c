/*
Copyright (C) 2006 Pekka Lampila ("Medar"), Damien Deville ("Pb")
and German Garcia Fernandez ("Jal") for Chasseur de bots association.

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

#include "webdownload.h"
#include "qcommon.h"

#include <curl/curl.h>
#include <stdio.h>

static int Web_Init( void );
static void Web_Cleanup( void );

// to get the size and mime type before the download we can use that
// http://curl.haxx.se/mail/lib-2002-05/0036.html

static CURL *curl = NULL;
static char curl_err[1024];
static int ( *progress )(float)= NULL;

static size_t Write( void *ptr, size_t size, size_t nmemb, void *stream )
{
	int f;
	int written;

	if( FS_FOpenAbsoluteFile( ( const char * )stream, &f, FS_APPEND ) < 0 )
		return size*nmemb; // weird

	written = FS_Write( ptr, size * nmemb, f );
	FS_FCloseFile( f );

	return (size_t)written;
}

static int Progress( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
	// callback
	if( progress != NULL )
	{
		if( progress( dlnow/dltotal ) != 0 )
			return 1; // we abort
	}

	//Com_Printf("Progress: %2.2f\n",dlnow*100.0/dltotal);
	return 0;
}

static void Web_Cleanup( void )
{
	if( curl != NULL )
	{
		/* always cleanup */
		curl_easy_cleanup( curl );
		curl = NULL;

		// reset callback
		progress = NULL;
	}
}

static int Web_Init( void )
{
	CURLcode code;
	static char useragent[256];
	cvar_t *http_proxy = Cvar_Get( "http_proxy", "", CVAR_ARCHIVE );
	cvar_t *http_proxyuserpwd = Cvar_Get( "http_proxyuserpwd", "", CVAR_ARCHIVE );
	const char *proxy = http_proxy->string;
	const char *proxy_userpwd = http_proxyuserpwd->string;

	if( curl != NULL )
	{
		Web_Cleanup();
	}

	// reinit
	curl = curl_easy_init();
	// reset callback
	progress = NULL;

	// http://curl.haxx.se/libcurl/c/curl_easy_setopt.html
	/* init some options of curl */
	code = curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, curl_err );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set error buffer\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_NOPROGRESS, 0 );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set NoProgress\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1 );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set libcurl nosignal mode\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1 );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set FollowLocation\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_MAXREDIRS, 2 );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set Max Redirection\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, Write );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set writer callback function\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_PROGRESSFUNCTION, Progress );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set progress callback function\n" );
		return 0;
	}

	Q_strncpyz( useragent, APPLICATION, sizeof( useragent ) );

	// same as in cvar version, qcommon.c
	if( versioncvar && versioncvar->string && versioncvar->string[0] != '\0' )
		Q_strncatz( useragent, va( "  %s", versioncvar->string ), sizeof( useragent ) );
	if( revisioncvar && revisioncvar->string && revisioncvar->string[0] != '\0' )
		Q_strncatz( useragent, va( "  %s", revisioncvar->string ), sizeof( useragent ) );

	code = curl_easy_setopt( curl, CURLOPT_USERAGENT, useragent );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set UserAgent\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_FAILONERROR, 1 );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set fail on error\n" );
		return 0;
	}

	// HTTP proxy settings
	if( proxy && *proxy ) {
		code = curl_easy_setopt( curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP );
		if( code != CURLE_OK )
		{
			Com_Printf( "Failed to set proxy type to http\n" );
			return 0;
		}

		code = curl_easy_setopt( curl, CURLOPT_PROXY, proxy );
		if( code != CURLE_OK )
		{
			Com_Printf( "Failed to set proxy\n" );
			return 0;
		}

		if( proxy_userpwd && *proxy_userpwd ) {
			code = curl_easy_setopt( curl, CURLOPT_PROXY, proxy_userpwd );
			if( code != CURLE_OK )
			{
				Com_Printf( "Failed to set proxy password\n" );
				return 0;
			}
		}
	}

	return 1;
}

int Web_Get( const char *url, const char *referer, const char *name, int resume, int max_downloading_time, int timeout, int ( *_progress )(float), int noreuse )
{
	CURLcode code;
	int fsize;

	// init/reinit curl
	Web_Init();

	code = curl_easy_setopt( curl, CURLOPT_URL, url );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set url\n" );;
		return 0;
	}

	if( referer )
	{
		code = curl_easy_setopt( curl, CURLOPT_REFERER, referer );
		if( code != CURLE_OK )
		{
			Com_Printf( "Failed to set Referer\n" );
			return 0;
		}
	}

	// connection timeout
	code = curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT, timeout );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set libcurl connection timeout\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_TIMEOUT, max_downloading_time );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set libcurl global timeout\n" );
		return 0;
	}

	if( noreuse )
	{
		code = curl_easy_setopt( curl, CURLOPT_FORBID_REUSE, 1 );
		if( code != CURLE_OK )
		{
			Com_Printf( "Failed to forbid reuse\n" );
			return 0;
		}
	}

	if( resume == 1 )
	{
		// test if file exist
		fsize = FS_FOpenAbsoluteFile( name, NULL, FS_READ );
		if( fsize < 0 )
			goto new_file; // file does not exist

		code = curl_easy_setopt( curl, CURLOPT_RESUME_FROM, fsize );
		if( code != CURLE_OK )
		{
			Com_Printf( "Failed to set file resume from length\n" );
			return 0;
		}
	}
	else
	{
		// we will append to the file so if it already exist we will have twice the data
		// so delete the file if it exist
		FS_RemoveAbsoluteFile( name );
	}

new_file:

	code = curl_easy_setopt( curl, CURLOPT_WRITEDATA, name );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to set writer data\n" );
		return 0;
	}

	Com_DPrintf( "Downloading %s from %s\n", name, url );

	// set callback
	progress = _progress;
	code = curl_easy_perform( curl );
	if( code != CURLE_OK )
	{
		Com_Printf( "Failed to download %s from %s\n", name, url );
		Com_Printf( "Error: %s\n", curl_err );

		Web_Cleanup();
		return 0;
	}

	Web_Cleanup();
	return 1;
}
