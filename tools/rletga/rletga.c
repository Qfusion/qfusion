/*

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char qbyte;
typedef unsigned int qboolean;

int loadfile( const char *filename, void **buf )
{
	int pos, end;
	FILE *f;

	*buf = NULL;

	f = fopen( filename, "rb" );
	if( !f )
		return -1;

	pos = ftell( f );
	fseek( f, 0, SEEK_END );
	end = ftell( f );
	fseek( f, 0, SEEK_SET );

	*buf = malloc( end );
	fread( *buf, 1, end, f );

	fseek( f, pos, SEEK_SET );
	fclose( f );

	return end;
}

void writefile( const char *filename, void *buf, int size )
{
	FILE *f;

	f = fopen (filename, "wb");
	if( !f )
		return;

	fwrite (buf, 1, size, f);
	fclose (f);
}

void freefile( void *buf )
{
	if( buf )
		free( buf );
}

//=====================================================================================

#ifdef _WIN32
#define ENDIAN_LITTLE
#endif

#ifdef ENDIAN_LITTLE
// little endian
# define BigShort(l) ShortSwap(l)
# define LittleShort(l) (l)
# define BigLong(l) LongSwap(l)
# define LittleLong(l) (l)
# define BigFloat(l) FloatSwap(l)
# define LittleFloat(l) (l)
#elif defined(ENDIAN_BIG)
// big endian
# define BigShort(l) (l)
# define LittleShort(l) ShortSwap(l)
# define BigLong(l) (l)
# define LittleLong(l) LongSwap(l)
# define BigFloat(l) (l)
# define LittleFloat(l) FloatSwap(l)
#else
// figure it out at runtime
extern short (*BigShort) (short l);
extern short (*LittleShort) (short l);
extern int (*BigLong) (int l);
extern int (*LittleLong) (int l);
extern float (*BigFloat) (float l);
extern float (*LittleFloat) (float l);
#endif

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
short   (*BigShort) (short l);
short   (*LittleShort) (short l);
int     (*BigLong) (int l);
int     (*LittleLong) (int l);
float   (*BigFloat) (float l);
float   (*LittleFloat) (float l);
#endif

short   ShortSwap (short l)
{
	qbyte    b1, b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
short   ShortNoSwap (short l)
{
	return l;
}
#endif

int    LongSwap (int l)
{
	qbyte    b1, b2, b3, b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
int     LongNoSwap (int l)
{
	return l;
}
#endif

float FloatSwap (float f)
{
	union
	{
		float   f;
		qbyte   b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
float FloatNoSwap (float f)
{
	return f;
}
#endif

/*
================
Swap_Init
================
*/
void Swap_Init (void)
{
#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
	qbyte    swaptest[2] = {1,0};

// set the qbyte swapping variables in a portable manner
	if ( *(short *)swaptest == 1)
	{
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}
#endif
}

//=====================================================================================

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

/*
=============
LoadTGA
=============
*/
static int LoadTGA( const char *name, qbyte **pic, int *width, int *height, int *size )
{
	int		i, columns, rows, row_inc, row, col;
	qbyte	*buf_p, *buffer, *pixbuf, *targa_rgba;
	int		length, samples, readpixelcount, pixelcount;
	qbyte	palette[256][4], red = 0, green = 0, blue = 0, alpha = 0;
	qboolean compressed;
	TargaHeader	targa_header;

	*pic = NULL;

	//
	// load the file
	//
	length = loadfile (name, (void **)&buffer);
	if( size )
		*size = length;
	if( !buffer )
		return 0;

	buf_p = buffer;
	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = buf_p[0] + buf_p[1] * 256;
	buf_p+=2;
	targa_header.colormap_length = buf_p[0] + buf_p[1] * 256;
	buf_p+=2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.y_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.width = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.height = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;
	if( targa_header.id_length != 0 )
		buf_p += targa_header.id_length;  // skip TARGA image comment

	if( targa_header.image_type == 1 || targa_header.image_type == 9 ) {
#if 1
		freefile( buffer );
		return 0;
#endif
		// uncompressed colormapped image
		if( targa_header.pixel_size != 8 ) {
			freefile( buffer );
			return 0;
		}
		if( targa_header.colormap_length != 256 ) {
			freefile( buffer );
			return 0;
		}
		if( targa_header.colormap_index ) {
			freefile( buffer );
			return 0;
		}
		if( targa_header.colormap_size == 24 ) {
			for( i = 0; i < targa_header.colormap_length; i++ ) {
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = 255;
			}
		} else if( targa_header.colormap_size == 32 ) {
			for( i = 0; i < targa_header.colormap_length; i++ ) {
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
		} else {
			freefile( buffer );
			return 0;
		}
	} else if( targa_header.image_type == 2 || targa_header.image_type == 10 ) {
#if 1
		if( targa_header.image_type == 10  )
		{
			freefile( buffer );
			return 0;
		}
#endif
		// uncompressed or RLE compressed RGB
		if( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) {
			freefile( buffer );
			return 0;
		}
	} else if( targa_header.image_type == 3 || targa_header.image_type == 11 ) {
#if 1
		freefile( buffer );
		return 0;
#endif
		// uncompressed greyscale
		if( targa_header.pixel_size != 8 ) {
			freefile( buffer );
			return 0;
		}
	}

	columns = targa_header.width;
	if( width )
		*width = columns;

	rows = targa_header.height;
	if( height )
		*height = rows;

	targa_rgba = malloc( columns * rows * 4 );
	*pic = targa_rgba;

	// if bit 5 of attributes isn't set, the image has been stored from bottom to top
	if( targa_header.attributes & 0x20 ) {
		pixbuf = targa_rgba;
		row_inc = 0;
	} else {
		pixbuf = targa_rgba + (rows - 1) * columns * 4;
		row_inc = -columns * 4 * 2;
	}

	compressed = ( targa_header.image_type == 9 || targa_header.image_type == 10 || targa_header.image_type == 11 );
	for( row = col = 0, samples = 3; row < rows; ) {
		pixelcount = 0x10000;
		readpixelcount = 0x10000;

		if( compressed ) {
			pixelcount = *buf_p++;
			if( pixelcount & 0x80 )	// run-length packet
				readpixelcount = 1;
			pixelcount = 1 + (pixelcount & 0x7f);
		}

		while( pixelcount-- && (row < rows) ) {
			if( readpixelcount-- > 0 ) {
				switch( targa_header.image_type ) {
				case 1:
				case 9:
					// colormapped image
					blue = *buf_p++;
					red = palette[blue][0];
					green = palette[blue][1];
					alpha = palette[blue][3];
					blue = palette[blue][2];
					if( alpha != 255 )
						samples = 4;
					break;
				case 2:
				case 10:
					// 24 or 32 bit image
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alpha = 255;
					if( targa_header.pixel_size == 32 ) {
						alpha = *buf_p++;
						if( alpha != 255 )
							samples = 4;
					}
					break;
				case 3:
				case 11:
					// greyscale image
					blue = green = red = *buf_p++;
					alpha = 255;
					break;
				}
			}

			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;
			if( ++col == columns ) { // run spans across rows
				row++;
				col = 0;
				pixbuf += row_inc;
			}
		}
	}

	freefile( buffer );

	return samples;
}

// original code taken from quake3mme (quake3 movie maker edition or something)

static int SaveTGA_RLEPacket(qbyte *out, qbyte* pixel, int pixelsize, int count) {
	qbyte rlepacketheader;
	int i;

	if (count < 1) {
		printf("WARNING: SaveTGA_RLEWrite() count is zero\n");
		return 0;	// Nothing to write,
	}
	if (count > 128) {
		printf("WARNING: SaveTGA_RLEWrite() count is > 128\n");
		return 0;
	}
	if ((out == NULL) || (pixel == NULL)) {
		printf("WARNING: SaveTGA_RLEWrite() has been passed NULL\n");
		return 0;	// Nothing to write,
	}

	rlepacketheader = (count-1) & 0x7F;
	rlepacketheader |= 0x80;

	*out++ = rlepacketheader;
	for (i = 0; i < pixelsize; i++) {
		*out++ = pixel[i];
	}

	return pixelsize+1;
}

static int SaveTGA_RLEPacketRaw(qbyte *out, qbyte* data, int pixelsize, int count) {
	qbyte rlepacketheader;
	int i;

	if (count < 1) {
		printf("WARNING: SaveTGA_RLEWriteRaw() count is zero\n");
		return 0;	// Nothing to write,
	}
	if (count > 128) {
		printf("WARNING: SaveTGA_RLEWriteRaw() count is > 128\n");
		return 0;
	}
	if (out == NULL) {
		printf("WARNING: SaveTGA_RLEWriteRaw() has been passed NULL\n");
		return 0;	// Nothing to write,
	}

	rlepacketheader = (count-1) & 0x7F;

	*out++ = rlepacketheader;
	for (i = 0; i < count; i++) {
		*out++ = *data++;
		*out++ = *data++;
		*out++ = *data++;
		if (pixelsize > 3) *out++ = *data++;
		else data++;
	}
	
	return (count*pixelsize)+1;
}

#define PIXEL_COMPARE(pixel1,pixel2) memcmp(pixel1, pixel2, pixelsize)
#define PIXEL_READ(pixel) memcpy(pixel, inbuffer, pixelsize);inbuffer+=4;read_pos++;
#define PIXEL_COPY(pixel1,pixel2) memcpy(pixel1, pixel2, pixelsize);

static int SaveTGA_CountDiff(const void* in, const int pixelsize, const int num) {
	int n = 0;
	int read_pos = 0;
	qbyte pixel[4];
	qbyte pixelnext[4];
	qbyte *inbuffer = (qbyte*)in;

	if (num == 1) return num;

	PIXEL_READ(pixel);

	while (read_pos < num) {
		PIXEL_READ(pixelnext);
		if (!PIXEL_COMPARE(pixelnext,pixel)) {
			break;
		}
		PIXEL_COPY(pixel, pixelnext);
		n++;
	}

	if (!PIXEL_COMPARE(pixelnext,pixel)) {
		return n;
	}

	return n + 1;
}

static int SaveTGA_CountSame(const void* in, const int pixelsize, const int num) {
	int n = 1;
	int read_pos = 0;
	qbyte pixel[4];
	qbyte pixelnext[4];
	qbyte *inbuffer = (qbyte*)in;

	PIXEL_READ(pixel);

	while (read_pos <= num) {
		PIXEL_READ(pixelnext);
		if (PIXEL_COMPARE(pixelnext,pixel)) {
			break;
		}
		n++;
	}
	return n;
}

static int SaveTGA_RLEEncode(qbyte *out, const int image_width, const int image_height, const void* image_buffer, const int image_buffer_size, const int image_hasalpha) {
	int pixelsize = (image_hasalpha) ? 4 : 3;
	qbyte *inbuffer = (qbyte*)image_buffer;

	int datasize = 0;

	int diffcount;
	int samecount;

	int i;

	for (i = 0; i < image_height; i++) {
		int column = 0;
		while (column < image_width) {
			int read_offset = (column + (i * image_width)) * 4;
			samecount = SaveTGA_CountSame(inbuffer + read_offset, pixelsize, 128);
			if (samecount > 1) {
				if (samecount > 128) samecount = 128;
				if (samecount > image_width-column) {
					samecount = image_width - column;
				}
				datasize += SaveTGA_RLEPacket(out+datasize, inbuffer+read_offset, pixelsize, samecount);
				column += samecount;
				continue;
			}

			diffcount = SaveTGA_CountDiff(inbuffer + read_offset, pixelsize, 128);
			if (diffcount > 0) {
				if (diffcount > 128) diffcount = 128;
				if (diffcount > image_width - column) {
					diffcount = image_width - column;
				}
				datasize += SaveTGA_RLEPacketRaw(out+datasize, inbuffer+read_offset, pixelsize, diffcount);
				column += diffcount;
			}
		}
	}
	return datasize;
}

/*
===============
SaveTGA
===============
*/
int SaveTGA(const char * filename, const int image_compressed, const int image_width, const int image_height, qbyte *image_buffer, const int image_buffer_size, const int image_samples) {
	int i, temp;
	qbyte *out;
	int outbuffersize = (int)((float)image_buffer_size * 1.4f); // Just in case RLE results in larger file
	const int image_hasalpha = (image_samples == 4) ? 1 : 0;

	int filesize = 18;	// header is here by default

	out = malloc(outbuffersize); 
	if (!out) {
		printf("WARNING: Not enough memory for TGA saving");		// yellow
		return 0;
	}
	memset(out, 0, outbuffersize);

	// Fill in the header
	out[2] = (image_compressed) ? 10 : 2;
	out[12] = image_width & 255;
	out[13] = image_width >> 8;
	out[14] = image_height & 255;
	out[15] = image_height >> 8;
	out[16] = (image_hasalpha) ? 32 : 24;
	out[17] = 0x20;

	// Swap RGB to BGR
	for (i = 0; i < image_buffer_size; i += 4) {
		temp = image_buffer[i];
		image_buffer[i] = image_buffer[i+2];
		image_buffer[i+2] = temp;
	}

	// Fill output buffer
	if (!image_compressed) { // Plain memcpy
		if (image_hasalpha) {
			memcpy(out+filesize, image_buffer, image_buffer_size);
			filesize += image_buffer_size;
		} else {
			qbyte *buftemp = out+filesize;
			for (i = 0; i < image_buffer_size; i += 4) {
				*buftemp++ = image_buffer[i];
				*buftemp++ = image_buffer[i+1];
				*buftemp++ = image_buffer[i+2];
			}
			filesize += image_width*image_height*3;
		}
	} else {
		filesize += SaveTGA_RLEEncode(out+filesize, image_width, image_height, image_buffer, image_buffer_size, image_hasalpha);
	}

	writefile(filename, out, filesize);

	free(out);
	return filesize;
}

void RLETGA (const char *filename)
{
	int width, height, samples, size, newsize, diff;
	qbyte *buf;

	printf ("%s... ", filename);
	samples = LoadTGA(filename, &buf, &width, &height, &size );
	if (samples == 0)
	{
		printf ("skipped\n");
		return;
	}

	newsize = SaveTGA(filename, 1, width, height, buf, width * height * 4, samples);
	if (!newsize)
	{
		printf ("skipped\n");
		return;
	}

	diff = newsize - size;
	printf( " %.1f kb (%.1f%%)\n", (diff+1023)/1024.0, (float)diff/(float)size*100.0f );
}

int main( int argc, char **argv )
{
	if (argc < 2)
	{
		printf( "usage: rletga filename\n" );
		return 0;
	}

	Swap_Init ();

	RLETGA (argv[1]);

	return 0;
}
