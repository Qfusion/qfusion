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

#include <zlib.h>

#ifdef ZLIB_RUNTIME

#define qzinflateInit2(strm, windowBits) \
        qzinflateInit2_((strm), (windowBits), ZLIB_VERSION, \
                      (int)sizeof(z_stream))

extern int (ZEXPORT *qzcompress)(Bytef *dest,   uLongf *destLen, const Bytef *source, uLong sourceLen);
extern int (ZEXPORT *qzcompress2)(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);
extern int (ZEXPORT *qzuncompress)(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);
extern int (ZEXPORT *qzinflateInit2_)(z_streamp strm, int  windowBits, const char *version, int stream_size);
extern int (ZEXPORT *qzinflate)(z_streamp strm, int flush);
extern int (ZEXPORT *qzinflateEnd)(z_streamp strm);
extern int (ZEXPORT *qzinflateReset)(z_streamp strm);
extern gzFile (ZEXPORT *qgzopen)(const char *file, const char *mode);
extern z_off_t (ZEXPORT *qgzseek)(gzFile, z_off_t, int);
extern z_off_t (ZEXPORT *qgztell)(gzFile);
extern int (ZEXPORT *qgzread)(gzFile file, voidp buf, unsigned len);
extern int (ZEXPORT *qgzwrite)(gzFile file, voidpc buf, unsigned len);
extern int (ZEXPORT *qgzclose)(gzFile file);
extern int (ZEXPORT *qgzeof)(gzFile file);
extern int (ZEXPORT *qgzflush)(gzFile file, int flush);
extern int (ZEXPORT *qgzsetparams)(gzFile file, int level, int strategy);
extern int (ZEXPORT *qgzbuffer)(gzFile file, unsigned size);

#else

#define qzcompress compress
#define qzcompress2 compress2
#define qzuncompress uncompress
#define qzinflateInit2 inflateInit2
#define qzinflate inflate
#define qzinflateEnd inflateEnd
#define qzinflateReset inflateReset
#define qgzopen gzopen
#define qgzseek gzseek
#define qgztell gztell
#define qgzread gzread
#define qgzwrite gzwrite
#define qgzclose gzclose
#define qgzeof gzeof
#define qgzflush gzflush
#define qgzsetparams gzsetparams
#define qgzbuffer gzbuffer

#endif

void Com_LoadCompressionLibraries( void );
void Com_UnloadCompressionLibraries( void );
