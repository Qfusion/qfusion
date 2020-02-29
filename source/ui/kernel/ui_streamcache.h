/*
Copyright (C) 2011 Victor Luchits

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

#ifndef __UI_STREAMCACHE_H__
#define __UI_STREAMCACHE_H__

#include <map>
#include "kernel/ui_common.h"
#include "kernel/ui_utils.h"

#define WSW_UI_STREAMCACHE_EXT                  ".tmp"
#define WSW_UI_STREAMCACHE_DIR                  "cache/ui"
#define WSW_UI_STREAMCACHE_TIMEOUT              15                  // timeout in seconds
#define WSW_UI_STREAMCACHE_CACHE_TTL            60                  // TTL in minutes
#define WSW_UI_STREAMCACHE_CACHE_PURGE_INTERVAL 15                  // purge interval in days

namespace WSWUI
{
class AsyncStream;
class StreamCache;

typedef void (*stream_cache_cb)( const char *fileName, void *privatep );
typedef std::list<AsyncStream *> StreamList;
typedef std::map<std::string, StreamList> StreamMap;

// class repesenting a single async request
class AsyncStream
{
public:
	AsyncStream();

private:
	void *privatep;
	std::string key;
	StreamCache *parent;

	std::string url;
	std::string tmpFilename;
	int tmpFilenum;
	bool noCache;

	ui_async_stream_read_cb_t read_cb;
	ui_async_stream_done_cb_t done_cb;
	stream_cache_cb cache_cb;

	friend class StreamCache;
};

// class which is responsible for performing and caching of asyncrhonous HTTP requests
class StreamCache
{
public:
	StreamCache();

	// public streaming callbacks, privatep points to AsyncStream object
	static size_t StreamRead( const void *buf, size_t numb, float percentage, int status,
							  const char *contentType, void *privatep );
	static void StreamDone( int status, const char *contentType, void *privatep );

	// the entry function for all requests
	void PerformRequest( const char *url, const char *method, const char *data,
						 ui_async_stream_read_cb_t read_cb, ui_async_stream_done_cb_t done_cb, stream_cache_cb cache_cb,
						 void *privatep, int timeout = WSW_UI_STREAMCACHE_TIMEOUT, int cacheTTL = WSW_UI_STREAMCACHE_CACHE_TTL );

	// startup initialization
	void Init( void );

	// shutdown cleanup procedure
	void Shutdown( void );

private:
	// storage for caching streams
	StreamMap streams;

	// returns path to cache file for a given URL
	static std::string CacheFileForUrl( const std::string url, bool noCache );

	// returns path to cache file, given that we know its MIME type
	static std::string RealFileForCacheFile( const std::string cacheFile, const std::string contentType );

	// calls cache callback for all streams sharing the same key, then removes those
	void CallCacheCbByStreamKey( const std::string &key, const std::string &fileName, bool success );

	// remove all cached files
	void PurgeCache( void );

	friend class AsyncStream;

	cvar_t *ui_cachepurgedate;
};

}

#endif // __UI_STREAMCACHE_H__
