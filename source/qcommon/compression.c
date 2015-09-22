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
#include "compression.h"

static void *zLibrary;

#ifdef ZLIB_RUNTIME

int (ZEXPORT *qzcompress)(Bytef *dest,uLongf *destLen, const Bytef *source, uLong sourceLen);
int (ZEXPORT *qzcompress2)(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);
int (ZEXPORT *qzuncompress)(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);
int (ZEXPORT *qzinflateInit2_)(z_streamp strm, int  windowBits, const char *version, int stream_size);
int (ZEXPORT *qzinflate)(z_streamp strm, int flush);
int (ZEXPORT *qzinflateEnd)(z_streamp strm);
int (ZEXPORT *qzinflateReset)(z_streamp strm);
gzFile (ZEXPORT *qgzopen)(const char *, const char *);
z_off_t (ZEXPORT *qgzseek)(gzFile, z_off_t, int);
z_off_t (ZEXPORT *qgztell)(gzFile);
int (ZEXPORT *qgzread)(gzFile file, voidp buf, unsigned len);
int (ZEXPORT *qgzwrite)(gzFile file, voidpc buf, unsigned len);
int (ZEXPORT *qgzclose)(gzFile file);
int (ZEXPORT *qgzeof)(gzFile file);
int (ZEXPORT *qgzflush)(gzFile file, int flush);
int (ZEXPORT *qgzsetparams)(gzFile file, int level, int strategy);
int (ZEXPORT *qgzbuffer)(gzFile file, unsigned size);

static dllfunc_t zlibfuncs[] =
{
	{ "compress", ( void **)&qzcompress },
	{ "compress2", ( void **)&qzcompress2 },
	{ "uncompress", ( void **)&qzuncompress },
	{ "inflateInit2_", ( void **)&qzinflateInit2_ },
	{ "inflate", ( void **)&qzinflate },
	{ "inflateEnd", ( void **)&qzinflateEnd },
	{ "inflateReset", ( void **)&qzinflateReset },
	{ "gzopen", ( void **)&qgzopen },
	{ "gzseek", ( void **)&qgzseek },
	{ "gztell", ( void **)&qgztell },
	{ "gzread", ( void **)&qgzread },
	{ "gzwrite", ( void **)&qgzwrite },
	{ "gzclose", ( void **)&qgzclose },
	{ "gzeof", ( void **)&qgzeof },
	{ "gzflush", ( void **)&qgzflush },
	{ "gzsetparams", ( void **)&qgzsetparams },
	{ "gzbuffer", ( void **)&qgzbuffer },
	{ NULL, NULL },
};

#endif

/*
* ZLib_UnloadLibrary
*/
void ZLib_UnloadLibrary( void )
{
#ifdef ZLIB_RUNTIME
	if( zLibrary )
		Com_UnloadLibrary( &zLibrary );
#endif
	zLibrary = NULL;
}

/*
* ZLib_LoadLibrary
*/
void ZLib_LoadLibrary( void )
{
	ZLib_UnloadLibrary();

#ifdef ZLIB_RUNTIME
	zLibrary = Com_LoadSysLibrary( LIBZ_LIBNAME, zlibfuncs );
	if( !zLibrary )
		Com_Error( ERR_FATAL, "Failed to load %s", LIBZ_LIBNAME );
#else
	zLibrary = (void *)1;
#endif
}

/*
* Com_LoadCompressionLibraries
*/
void Com_LoadCompressionLibraries( void )
{
	ZLib_LoadLibrary();
}

/*
* Com_UnloadCompressionLibraries
*/
void Com_UnloadCompressionLibraries( void )
{
	ZLib_UnloadLibrary();
}
