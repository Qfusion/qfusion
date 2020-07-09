/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// msg.c -- Message IO functions
#include "qcommon.h"
#include "../qalgo/half_float.h"

/*
==============================================================================

MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/
#define MAX_MSG_STRING_CHARS    2048

void MSG_Init( msg_t *msg, uint8_t *data, size_t length ) {
	memset( msg, 0, sizeof( *msg ) );
	msg->data = data;
	msg->maxsize = length;
	msg->cursize = 0;
	msg->compressed = false;
}

void MSG_Clear( msg_t *msg ) {
	msg->cursize = 0;
	msg->compressed = false;
}

void *MSG_GetSpace( msg_t *msg, size_t length ) {
	void *ptr;

	assert( msg->cursize + length <= msg->maxsize );
	if( msg->cursize + length > msg->maxsize ) {
		Com_Error( ERR_FATAL, "MSG_GetSpace: overflowed" );
	}

	ptr = msg->data + msg->cursize;
	msg->cursize += length;
	return ptr;
}

//==================================================
// WRITE FUNCTIONS
//==================================================


void MSG_WriteData( msg_t *msg, const void *data, size_t length ) {
	MSG_CopyData( msg, data, length );
}

void MSG_CopyData( msg_t *buf, const void *data, size_t length ) {
	memcpy( MSG_GetSpace( buf, length ), data, length );
}

void MSG_WriteInt8( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 1 );
	buf[0] = ( char )c;
}

void MSG_WriteUint8( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 1 );
	buf[0] = ( uint8_t )( c & 0xff );
}

void MSG_WriteInt16( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 2 );
	buf[0] = ( uint8_t )( c & 0xff );
	buf[1] = ( uint8_t )( ( c >> 8 ) & 0xff );
}

void MSG_WriteUint16( msg_t *msg, unsigned c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 2 );
	buf[0] = ( uint8_t )( c & 0xff );
	buf[1] = ( uint8_t )( ( c >> 8 ) & 0xff );
}

void MSG_WriteInt32( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 4 );
	buf[0] = ( uint8_t )( c & 0xff );
	buf[1] = ( uint8_t )( ( c >> 8 ) & 0xff );
	buf[2] = ( uint8_t )( ( c >> 16 ) & 0xff );
	buf[3] = ( uint8_t )( c >> 24 );
}

void MSG_WriteInt64( msg_t *msg, int64_t c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 8 );
	buf[0] = ( uint8_t )( c & 0xffL );
	buf[1] = ( uint8_t )( ( c >> 8L ) & 0xffL );
	buf[2] = ( uint8_t )( ( c >> 16L ) & 0xffL );
	buf[3] = ( uint8_t )( ( c >> 24L ) & 0xffL );
	buf[4] = ( uint8_t )( ( c >> 32L ) & 0xffL );
	buf[5] = ( uint8_t )( ( c >> 40L ) & 0xffL );
	buf[6] = ( uint8_t )( ( c >> 48L ) & 0xffL );
	buf[7] = ( uint8_t )( c >> 56L );
}

void MSG_WriteUintBase128( msg_t *msg, uint64_t c ) {
	uint8_t buf[10];
	size_t len = 0;

	do {
		buf[len] = c & 0x7fU;
		if ( c >>= 7 ) {
			buf[len] |= 0x80U;
		}
		len++;
	} while( c );

	MSG_WriteData( msg, buf, len );
}

void MSG_WriteIntBase128( msg_t *msg, int64_t c ) {
	// use Zig-zag encoding for signed integers for more efficient storage
	uint64_t cc = (uint64_t)(c << 1) ^ (uint64_t)(c >> 63);
	MSG_WriteUintBase128( msg, cc );
}

void MSG_WriteFloat( msg_t *msg, float f ) {
	union {
		float f;
		int l;
	} dat;

	dat.f = f;
	MSG_WriteInt32( msg, dat.l );
}

void MSG_WriteHalfFloat( msg_t *msg, float f ) {
	MSG_WriteUint16( msg, float_to_half( f ) );
}

void MSG_WriteString( msg_t *msg, const char *s ) {
	if( !s ) {
		MSG_WriteData( msg, "", 1 );
	} else {
		int l = strlen( s );
		if( l >= MAX_MSG_STRING_CHARS ) {
			Com_Printf( "MSG_WriteString: MAX_MSG_STRING_CHARS overflow" );
			MSG_WriteData( msg, "", 1 );
			return;
		}
		MSG_WriteData( msg, s, l + 1 );
	}
}

#define MSG_AreaBitsUTMSize( numareas ) ( ( ( ( numareas ) * ( (numareas)-1 ) / 2 ) + 7 ) / 8 )
#define MSG_AreaBitsRowSize( numareas ) ( ( numareas + 7 ) / 8 )

/*
 * MSG_WriteAreaBitsUTM
 */
int MSG_WriteAreaBitsUTM( msg_t *msg, int numareas, const uint8_t *areabits )
{
	int		 i, j, k;
	int		 bytes;
	int		 rowsize;
	uint8_t *out;

	// only send the upper triangle of the triangluar state matrix
	// ignore the main diagonal
	bytes = MSG_AreaBitsUTMSize( numareas );
	rowsize = MSG_AreaBitsRowSize( numareas );

	if( !bytes ) {
		return 0;
	}

	out = MSG_GetSpace( msg, bytes );
	memset( out, 0, bytes );

	k = 0;
	for( i = 0; i < numareas; i++ ) {
		const uint8_t *row = areabits + i * rowsize;
		for( j = i + 1; j < numareas; j++ ) {
			if( row[j >> 3] & ( 1 << ( j & 7 ) ) ) {
				out[k >> 3] |= ( 1 << ( k & 7 ) );
			}
			k++;
		}
	}

	return bytes;
}

//==================================================
// READ FUNCTIONS
//==================================================

void MSG_BeginReading( msg_t *msg ) {
	msg->readcount = 0;
}

int MSG_ReadInt8( msg_t *msg ) {
	int i = (signed char)msg->data[msg->readcount++];
	if( msg->readcount > msg->cursize ) {
		i = -1;
	}
	return i;
}


int MSG_ReadUint8( msg_t *msg ) {
	msg->readcount++;
	if( msg->readcount > msg->cursize ) {
		return 0;
	}

	return ( unsigned char )( msg->data[msg->readcount - 1] );
}

int16_t MSG_ReadInt16( msg_t *msg ) {
	msg->readcount += 2;
	if( msg->readcount > msg->cursize ) {
		return -1;
	}
	return ( int16_t )( msg->data[msg->readcount - 2] | ( msg->data[msg->readcount - 1] << 8 ) );
}

uint16_t MSG_ReadUint16( msg_t *msg ) {
	msg->readcount += 2;
	if( msg->readcount > msg->cursize ) {
		return 0;
	}
	return ( uint16_t )( msg->data[msg->readcount - 2] | ( msg->data[msg->readcount - 1] << 8 ) );
}

int MSG_ReadInt32( msg_t *msg ) {
	msg->readcount += 4;
	if( msg->readcount > msg->cursize ) {
		return -1;
	}

	return msg->data[msg->readcount - 4]
		   | ( msg->data[msg->readcount - 3] << 8 )
		   | ( msg->data[msg->readcount - 2] << 16 )
		   | ( msg->data[msg->readcount - 1] << 24 );
}

int64_t MSG_ReadInt64( msg_t *msg ) {
	msg->readcount += 8;
	if( msg->readcount > msg->cursize ) {
		return -1;
	}

	return ( int64_t )msg->data[msg->readcount - 8]
		| ( ( int64_t )msg->data[msg->readcount - 7] << 8L )
		| ( ( int64_t )msg->data[msg->readcount - 6] << 16L )
		| ( ( int64_t )msg->data[msg->readcount - 5] << 24L )
		| ( ( int64_t )msg->data[msg->readcount - 4] << 32L )
		| ( ( int64_t )msg->data[msg->readcount - 3] << 40L )
		| ( ( int64_t )msg->data[msg->readcount - 2] << 48L )
		| ( ( int64_t )msg->data[msg->readcount - 1] << 56L );
}

uint64_t MSG_ReadUintBase128( msg_t *msg ) {
	size_t len = 0;
	uint64_t i = 0;

	while( len < 10 ) {
		uint8_t c = MSG_ReadUint8( msg );
		i |= (c & 0x7fLL) << (7 * len);
		len++;
		if( !(c & 0x80) ) {
			break;
		}
	}

	return i;
}

int64_t MSG_ReadIntBase128( msg_t *msg ) {
	// un-Zig-Zag our value back to a signed integer
	uint64_t c = MSG_ReadUintBase128( msg );
	return (int64_t)(c >> 1) ^ (-(int64_t)(c & 1));
}

float MSG_ReadFloat( msg_t *msg ) {
	union {
		float f;
		int l;
	} dat;

	dat.l = MSG_ReadInt32( msg );
	if( msg->readcount > msg->cursize ) {
		dat.f = -1;
	}
	return dat.f;
}

float MSG_ReadHalfFloat( msg_t *msg ) {
	return half_to_float( MSG_ReadUint16( msg ) );
}

void MSG_ReadDir( msg_t *msg, vec3_t dir ) {
	ByteToDir( MSG_ReadUint8( msg ), dir );
}

void MSG_ReadData( msg_t *msg, void *data, size_t length ) {
	unsigned int i;

	for( i = 0; i < length; i++ )
		( (uint8_t *)data )[i] = MSG_ReadUint8( msg );

}

int MSG_SkipData( msg_t *msg, size_t length ) {
	if( msg->readcount + length <= msg->cursize ) {
		msg->readcount += length;
		return 1;
	}
	return 0;
}

uint8_t *MSG_PeekData( msg_t *msg, size_t size )
{
	if( msg->readcount + size > msg->cursize ) {
		return NULL;
	}
	return &msg->data[msg->readcount];
}

static char *MSG_ReadString2( msg_t *msg, bool linebreak ) {
	int l, c;
	static char string[MAX_MSG_STRING_CHARS];

	l = 0;
	do {
		c = MSG_ReadUint8( msg );
		if( c == -1 || c == 0 || ( linebreak && c == '\n' ) ) {
			break;
		}

		string[l] = c;
		l++;
	} while( (unsigned int)l < sizeof( string ) - 1 );

	string[l] = 0;

	return string;
}

char *MSG_ReadString( msg_t *msg ) {
	return MSG_ReadString2( msg, false );
}

char *MSG_ReadStringLine( msg_t *msg ) {
	return MSG_ReadString2( msg, true );
}

/*
 * MSG_ReadAreaBitsUTM
 */
void MSG_ReadAreaBitsUTM( msg_t *msg, int numareas, uint8_t *out, size_t outsize )
{
	int		 i, j, k;
	int		 inbytes, outbytes;
	int		 rowsize;
	uint8_t *buffer;

	inbytes = MSG_AreaBitsUTMSize( numareas );
	rowsize = MSG_AreaBitsRowSize( numareas );
	if( msg->readcount + inbytes > msg->cursize ) {
		return;
	}

	outbytes = numareas * rowsize;
	if( (size_t)outbytes > outsize ) {
		Com_Error( ERR_DROP, "Invalid areabits size: %" PRIuPTR " > MAX_SNAPSHOT_AREABYTES", (uintptr_t)outbytes );
		return;
	}

	memset( out, 0, outbytes );
	buffer = &msg->data[msg->readcount];

	// read and mirror the upper triangluar matrix
	// set the main diagonal bits to 1
	k = 0;
	for( i = 0; i < numareas; i++ ) {
		uint8_t *row = out + i * rowsize;

		for( j = i + 1; j < numareas; j++ ) {
			if( buffer[k >> 8] & ( 1 << ( k & 7 ) ) ) {
				uint8_t *row2 = out + j * rowsize;

				row[j >> 3] |= ( 1 << ( j & 7 ) );
				row2[i >> 3] |= ( 1 << ( i & 7 ) );
			}
			k++;
		}

		row[i >> 3] |= ( 1 << ( i & 7 ) );
	}

	msg->readcount += inbytes;
}

//==================================================
// ENCODED FIELDS
//==================================================

/*
* MSG_FieldBytes
*/
static size_t MSG_FieldBytes( const msg_field_t *field ) {
	if( field->bits == 0 ) {
		return sizeof( float );
	}
	return field->bits >> 3;
}

/*
* MSG_CompareField
*/
static bool MSG_CompareField( const uint8_t *from, const uint8_t *to, const msg_field_t *field ) {
	int32_t itv, ifv;
	bool btv, bfv;
	int64_t bitv, bifv;
	float ftv, ffv;

	switch( field->bits ) {
		case 0:
			ftv = *((float *)( to + field->offset ));
			ffv = *((float *)( from + field->offset ));
			return ftv != ffv;
		case 1:
			btv = *((bool *)( to + field->offset ));
			bfv = *((bool *)( from + field->offset ));
			return btv != bfv;
		case 8:
			itv = *((int8_t *)( to + field->offset ));
			ifv = *((int8_t *)( from + field->offset ));
			return itv != ifv;
		case 16:
			itv = *((int16_t *)( to + field->offset ));
			ifv = *((int16_t *)( from + field->offset ));
			return itv != ifv;
		case 32:
			itv = *((int32_t *)( to + field->offset ));
			ifv = *((int32_t *)( from + field->offset ));
			return itv != ifv;
		case 64:
			bitv = *((int64_t *)( to + field->offset ));
			bifv = *((int64_t *)( from + field->offset ));
			return bitv != bifv;
		default:
			Com_Error( ERR_FATAL, "MSG_CompareField: unknown field bits value %i", field->bits );
	}

	return false;
}

/*
* MSG_WriteField
*/
static void MSG_WriteField( msg_t *msg, const uint8_t *to, const msg_field_t *field ) {
	switch( field->encoding ) {
	case WIRE_BOOL:
		break;
	case WIRE_FIXED_INT8:
		MSG_WriteInt8( msg, *((int8_t *)( to + field->offset )) );
		break;
	case WIRE_FIXED_INT16:
		MSG_WriteInt16( msg, *((int16_t *)( to + field->offset )) );
		break;
	case WIRE_FIXED_INT32:
		MSG_WriteInt32( msg, *((int32_t *)( to + field->offset )) );
		break;
	case WIRE_FIXED_INT64:
		MSG_WriteInt64( msg, *((int64_t *)( to + field->offset )) );
		break;
	case WIRE_FLOAT:
		MSG_WriteFloat( msg, *((float *)( to + field->offset )) );
		break;
	case WIRE_HALF_FLOAT:
		MSG_WriteHalfFloat( msg, (*((float *)( to + field->offset ))) );
		break;
	case WIRE_ANGLE:
		MSG_WriteHalfFloat( msg, anglemod( (*((float *)( to + field->offset ))) ) );
		break;
	case WIRE_BASE128:
		switch( field->bits ) {
		case 8:
			MSG_WriteInt8( msg, *((int8_t *)( to + field->offset )) );
			break;
		case 16:
			MSG_WriteIntBase128( msg, *((int16_t *)( to + field->offset )) );
			break;
		case 32:
			MSG_WriteIntBase128( msg, *((int32_t *)( to + field->offset )) );
			break;
		case 64:
			MSG_WriteIntBase128( msg, *((int64_t *)( to + field->offset )) );
			break;
		default:
			Com_Error( ERR_FATAL, "MSG_WriteField: unknown base128 field bits value %i", field->bits );
			break;
		}
		break;
	case WIRE_UBASE128:
		switch( field->bits ) {
		case 8:
			MSG_WriteUint8( msg, *((uint8_t *)( to + field->offset )) );
			break;
		case 16:
			MSG_WriteUintBase128( msg, *((uint16_t *)( to + field->offset )) );
			break;
		case 32:
			MSG_WriteUintBase128( msg, *((uint32_t *)( to + field->offset )) );
			break;
		case 64:
			MSG_WriteUintBase128( msg, *((uint64_t *)( to + field->offset )) );
			break;
		default:
			Com_Error( ERR_FATAL, "MSG_WriteField: unknown base128 field bits value %i", field->bits );
			break;
		}
		break;
	default:
		Com_Error( ERR_FATAL, "MSG_WriteField: unknown encoding type %i", field->encoding );
		break;
	}
}

/*
* MSG_ReadField
*/
static void MSG_ReadField( msg_t *msg, uint8_t *to, const msg_field_t *field ) {
	switch( field->encoding ) {
	case WIRE_BOOL:
		*((bool *)( to + field->offset )) ^= true;
		break;
	case WIRE_FIXED_INT8:
		*((int8_t *)( to + field->offset )) = MSG_ReadInt8( msg );
		break;
	case WIRE_FIXED_INT16:
		*((int16_t *)( to + field->offset )) = MSG_ReadInt16( msg );
		break;
	case WIRE_FIXED_INT32:
		*((int32_t *)( to + field->offset )) = MSG_ReadInt32( msg );
		break;
	case WIRE_FIXED_INT64:
		*((int64_t *)( to + field->offset )) = MSG_ReadInt64( msg );
		break;
	case WIRE_FLOAT:
		*((float *)( to + field->offset )) = MSG_ReadFloat( msg );
		break;
	case WIRE_HALF_FLOAT:
		*((float *)( to + field->offset )) = MSG_ReadHalfFloat( msg );
		break;
	case WIRE_ANGLE:
		*((float *)( to + field->offset )) = MSG_ReadHalfFloat( msg );
		break;
	case WIRE_BASE128:
		switch( field->bits ) {
		case 8:
			*((int8_t *)( to + field->offset )) = MSG_ReadInt8( msg );
			break;
		case 16:
			*((int16_t *)( to + field->offset )) = MSG_ReadIntBase128( msg );
			break;
		case 32:
			*((int32_t *)( to + field->offset )) = MSG_ReadIntBase128( msg );
			break;
		case 64:
			*((int64_t *)( to + field->offset )) = MSG_ReadIntBase128( msg );
			break;
		default:
			Com_Error( ERR_FATAL, "MSG_WriteField: unknown base128 field bits value %i", field->bits );
			break;
		}
		break;
	case WIRE_UBASE128:
		switch( field->bits ) {
		case 8:
			*((uint8_t *)( to + field->offset )) = MSG_ReadUint8( msg );
			break;
		case 16:
			*((uint16_t *)( to + field->offset )) = MSG_ReadUintBase128( msg );
			break;
		case 32:
			*((uint32_t *)( to + field->offset )) = MSG_ReadUintBase128( msg );
			break;
		case 64:
			*((uint64_t *)( to + field->offset )) = MSG_ReadUintBase128( msg );
			break;
		default:
			Com_Error( ERR_FATAL, "MSG_WriteField: unknown base128 field bits value %i", field->bits );
			break;
		}
		break;
	default:
		Com_Error( ERR_FATAL, "MSG_WriteField: unknown encoding type %i", field->encoding );
		break;
	}
}

/*
* MSG_WriteFieldMask
*/
void MSG_WriteFieldMask( msg_t *msg, const uint8_t *fieldMask, unsigned byteMask ) {
	size_t b;

	b = 0;
	while( byteMask ) {
		if( byteMask & 1 ) {
			MSG_WriteUint8( msg, fieldMask[b] );
		}
		b++;
		byteMask >>= 1;
	}
}

/*
* MSG_ReadFieldMask
*/
void MSG_ReadFieldMask( msg_t *msg, uint8_t *fieldMask, size_t maskSize, unsigned byteMask ) {
	size_t b;

	b = 0;
	while( byteMask ) {
		if( byteMask & 1 ) {
			if( b >= maskSize ) {
				Com_Error( ERR_FATAL, "MSG_ReadFieldMask: i >= maskSize" );
			}
			fieldMask[b] = MSG_ReadUint8( msg );
		}
		b++;
		byteMask >>= 1;
	}
}

/*
* MSG_CompareArrays
*/
static unsigned MSG_CompareArrays( const void *from, const void *to, const msg_field_t *field, uint8_t *elemMask, size_t maskSize, bool quick ) {
	size_t i;
	unsigned byteMask;
	const size_t bytes = MSG_FieldBytes( field );
	const size_t maxElems = field->count;
	const uint8_t *bfrom = from, *bto = to;

	byteMask = 0;
	for( i = 0; i < maxElems; i++ ) {
		size_t byte = i >> 3;

		if( elemMask != NULL && byte > maskSize ) {
			Com_Error( ERR_FATAL, "MSG_CompareArrays: byte > maskSize" );
		}
		if( byte > 32 ) {
			Com_Error( ERR_FATAL, "MSG_CompareArrays: byte > 32" );
		}

		if( MSG_CompareField( bfrom, bto, field ) ) {
			if( elemMask != NULL ) {
				elemMask[byte] |= (1 << (i & 7));
			}
			byteMask |= (1 << (byte & 7));
			if( quick ) {
				return byteMask;
			}
		}

		bfrom += bytes;
		bto += bytes;
	}

	return byteMask;
}

/*
* MSG_WriteArrayElems
*/
static void MSG_WriteArrayElems( msg_t *msg, const void *to, const msg_field_t *field, const uint8_t *elemMask, unsigned byteMask ) {
	size_t b;
	const size_t bytes = MSG_FieldBytes( field );
	const size_t maxElems = field->count;
	const uint8_t *bto = to;

	b = 0;
	while( byteMask ) {
		if( byteMask & 1 ) {
			unsigned fm = elemMask[b];
			size_t f = b << 3;

			while( fm ) {
				if( f >= maxElems )
					return;

				if( fm & 1 ) {
					MSG_WriteField( msg, bto + f * bytes, field );
				}
				f++;
				fm >>= 1;
			}
		}

		b++;
		byteMask >>= 1;
	}
}

/*
* MSG_ReadArrayElems
*/
static void MSG_ReadArrayElems( msg_t *msg, void *to, const msg_field_t *field, const uint8_t *elemMask, size_t maskSize, unsigned byteMask ) {
	size_t b;
	uint8_t *bto = to;
	const size_t bytes = MSG_FieldBytes( field );
	const size_t maxElems = field->count;

	b = 0;
	while( byteMask ) {
		if( byteMask & 1 ) {
			unsigned fm;
			size_t fn = b << 3;

			if( b >= maskSize ) {
				Com_Error( ERR_FATAL, "MSG_ReadArrayElems: b >= maxSize" );
			}

			fm = elemMask[b];
			while( fm ) {
				assert( fn < maxElems );
				if( fn >= maxElems ) {
					Com_Error( ERR_FATAL, "MSG_ReadArrayElems: fn >= maxElems" );
				}

				if( fm & 1 ) {
					MSG_ReadField( msg, bto + fn * bytes, field );
				}

				fn++;
				fm >>= 1;
			}
		}

		b++;
		byteMask >>= 1;
	}
}

/*
* MSG_WriteDeltaArray
*/
static void MSG_WriteDeltaArray( msg_t *msg, const void *from, const void *to, const msg_field_t *field ) {
	unsigned byteMask;
	uint8_t elemMask[32] = { 0 };
	const size_t numElems = field->count;

	assert( numElems < 256 );
	if( numElems > 256 ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaArray: numFields == %" PRIu32, (unsigned)numElems );
	}

	byteMask = MSG_CompareArrays( from, to, field, elemMask, sizeof( elemMask ), false );

	if( numElems <= 8 ) {
		// we don't need the byteMask in case all field bits fit a single byte
		byteMask = 1;
	} else {
		MSG_WriteUintBase128( msg, byteMask );
	}

	MSG_WriteFieldMask( msg, elemMask, byteMask );

	MSG_WriteArrayElems( msg, to, field, elemMask, byteMask );
}

/*
* MSG_ReadDeltaArray
*/
static void MSG_ReadDeltaArray( msg_t *msg, const void *from, void *to, const msg_field_t *field ) {
	unsigned byteMask;
	uint8_t elemMask[32] = { 0 };
	//const size_t bytes = MSG_FieldBytes( field );
	const size_t maxElems = field->count;

	assert( maxElems < 256 );
	if( maxElems > 256 ) {
		Com_Error( ERR_FATAL, "MSG_ReadDeltaArray: numFields == %" PRIu32, (unsigned)maxElems );
	}

	// set everything to the state we are delta'ing from
	// we actually do this in MSG_ReadDeltaStruct
	// memcpy( (uint8_t *)to + field->offset, (uint8_t *)from + field->offset, bytes * maxElems );

	if( maxElems <= 8 ) {
		// we don't need the byteMask in case all field bits fit a single byte
		byteMask = 1;
	} else {
		byteMask = MSG_ReadUintBase128( msg );
	}

	MSG_ReadFieldMask( msg, elemMask, sizeof( elemMask ), byteMask );

	MSG_ReadArrayElems( msg, to, field, elemMask, sizeof( elemMask ), byteMask );
}

/*
* MSG_CompareStructs
*/
static unsigned MSG_CompareStructs( const void *from, const void *to, const msg_field_t *fields, size_t numFields, uint8_t *fieldMask, size_t maskSize ) {
	size_t i;
	unsigned byteMask;

	byteMask = 0;
	for( i = 0; i < numFields; i++ ) {
		size_t byte = i >> 3;
		bool change;
		const msg_field_t *f = &fields[i];

		if( fieldMask != NULL && byte > maskSize ) {
			Com_Error( ERR_FATAL, "MSG_CompareStructs: byte > maskSize" );
		}
		if( byte > 32 ) {
			Com_Error( ERR_FATAL, "MSG_CompareStructs: byte > 32" );
		}

		if( f->count > 1 ) {
			change = MSG_CompareArrays( from, to, f, NULL, 0, true ) != 0;
		} else {
			change = MSG_CompareField( from, to, f );
		}

		if( change ) {
			if( fieldMask != NULL ) {
				fieldMask[byte] |= (1 << (i & 7));
			}
			byteMask |= (1 << (byte & 7));
		}
	}

	return byteMask;
}

/*
* MSG_WriteStructFields
*/
static void MSG_WriteStructFields( msg_t *msg, const void *from, const void *to, const msg_field_t *fields, size_t numFields, const uint8_t *fieldMask, unsigned byteMask ) {
	size_t b;

	b = 0;
	while( byteMask ) {
		if( byteMask & 1 ) {
			unsigned fm = fieldMask[b];
			size_t fn = b << 3;

			while( fm ) {
				if( fn >= numFields )
					return;
				
				if( fm & 1 ) {
					const msg_field_t *f = &fields[fn];

					if( f->count > 1 ) {
						MSG_WriteDeltaArray( msg, from, to, f );
					} else {
						MSG_WriteField( msg, to, f );
					}
				}
				fn++;
				fm >>= 1;
			}
		}

		b++;
		byteMask >>= 1;
	}
}

/*
* MSG_ReadStructFields
*/
static void MSG_ReadStructFields( msg_t *msg, const void *from, void *to, const msg_field_t *fields, size_t numFields, const uint8_t *fieldMask, size_t maskSize, unsigned byteMask ) {
	size_t b;

	b = 0;
	while( byteMask ) {
		if( byteMask & 1 ) {
			unsigned fm;
			size_t fn = b << 3;

			if( b >= maskSize ) {
				Com_Error( ERR_FATAL, "MSG_ReadStructFields: b >= maxSize" );
			}

			fm = fieldMask[b];
			while( fm ) {
				assert( fn < numFields );
				if( fn >= numFields ) {
					Com_Error( ERR_FATAL, "MSG_ReadStructFields: f >= numFields" );
				}

				if( fm & 1 ) {
					const msg_field_t *f = &fields[fn];
					if( f->count > 1 ) {
						MSG_ReadDeltaArray( msg, from, to, f );
					} else {
						MSG_ReadField( msg, to, f );
					}
				}

				fn++;
				fm >>= 1;
			}
		}

		b++;
		byteMask >>= 1;
	}
}

/*
* MSG_WriteDeltaStruct
*/
void MSG_WriteDeltaStruct( msg_t *msg, const void *from, const void *to, const msg_field_t *fields, size_t numFields ) {
	unsigned byteMask;
	uint8_t fieldMask[32] = { 0 };

	assert( numFields < 256 );
	if( numFields > 256 ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaStruct: numFields == %" PRIu32, (unsigned)numFields );
	}

	byteMask = MSG_CompareStructs( from, to, fields, numFields, fieldMask, sizeof( fieldMask ) );

	if( numFields <= 8 ) {
		// we don't need the byteMask in case all field bits fit a single byte
		byteMask = 1;
	} else {
		MSG_WriteUintBase128( msg, byteMask );
	}

	MSG_WriteFieldMask( msg, fieldMask, byteMask );

	MSG_WriteStructFields( msg, from, to, fields, numFields, fieldMask, byteMask );
}

/*
* MSG_ReadDeltaStruct
*/
void MSG_ReadDeltaStruct( msg_t *msg, const void *from, void *to, size_t size, const msg_field_t *fields, size_t numFields ) {
	unsigned byteMask;
	uint8_t fieldMask[32] = { 0 };

	assert( numFields < 256 );
	if( numFields > 256 ) {
		Com_Error( ERR_FATAL, "MSG_ReadDeltaStruct: numFields == %" PRIu32, (unsigned)numFields );
	}

	// set everything to the state we are delta'ing from
	memcpy( to, from, size );

	if( numFields <= 8 ) {
		// we don't need the byteMask in case all field bits fit a single byte
		byteMask = 1;
	} else {
		byteMask = MSG_ReadUintBase128( msg );
	}

	MSG_ReadFieldMask( msg, fieldMask, sizeof( fieldMask ), byteMask );

	MSG_ReadStructFields( msg, from, to, fields, numFields, fieldMask, sizeof( fieldMask ), byteMask );
}

//==================================================
// DELTA ENTITIES
//==================================================

#define ESOFS( x ) offsetof( entity_state_t,x )

static const msg_field_t ent_state_fields[] = {
	{ ESOFS( events[0] ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( eventParms[0] ), 32, 1, WIRE_BASE128 },

	{ ESOFS( origin[0] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( origin[1] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( origin[2] ), 0, 1, WIRE_FLOAT },

	{ ESOFS( angles[0] ), 0, 1, WIRE_ANGLE },
	{ ESOFS( angles[1] ), 0, 1, WIRE_ANGLE },

	{ ESOFS( teleported ), 1, 1, WIRE_BOOL },

	{ ESOFS( type ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( solid ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( frame ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( modelindex ), 32, 1, WIRE_FIXED_INT8 },
	{ ESOFS( svflags ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( skinnum ), 32, 1, WIRE_BASE128 },
	{ ESOFS( effects ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( ownerNum ), 32, 1, WIRE_BASE128 },
	{ ESOFS( targetNum ), 32, 1, WIRE_BASE128 },
	{ ESOFS( sound ), 32, 1, WIRE_FIXED_INT8 },
	{ ESOFS( modelindex2 ), 32, 1, WIRE_FIXED_INT8 },
	{ ESOFS( attenuation ), 0, 1, WIRE_HALF_FLOAT },
	{ ESOFS( counterNum ), 32, 1, WIRE_BASE128 },
	{ ESOFS( bodyOwner ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( channel ), 32, 1, WIRE_FIXED_INT8 },
	{ ESOFS( events[1] ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( eventParms[1] ), 32, 1, WIRE_BASE128 },
	{ ESOFS( weapon ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( firemode ), 32, 1, WIRE_FIXED_INT8 },
	{ ESOFS( damage ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( range ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( team ), 32, 1, WIRE_FIXED_INT8 },

	{ ESOFS( origin2[0] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( origin2[1] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( origin2[2] ), 0, 1, WIRE_FLOAT },

	{ ESOFS( origin3[0] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( origin3[1] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( origin3[2] ), 0, 1, WIRE_FLOAT },

	{ ESOFS( linearMovementTimeStamp ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( linearMovement ), 1, 1, WIRE_BOOL },
	{ ESOFS( linearMovementDuration ), 32, 1, WIRE_UBASE128 },
	{ ESOFS( linearMovementVelocity[0] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( linearMovementVelocity[1] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( linearMovementVelocity[2] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( linearMovementBegin[0] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( linearMovementBegin[1] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( linearMovementBegin[2] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( linearMovementEnd[0] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( linearMovementEnd[1] ), 0, 1, WIRE_FLOAT },
	{ ESOFS( linearMovementEnd[2] ), 0, 1, WIRE_FLOAT },

	{ ESOFS( itemNum ), 32, 1, WIRE_UBASE128 },

	{ ESOFS( angles[2] ), 0, 1, WIRE_ANGLE },

	{ ESOFS( colorRGBA ), 32, 1, WIRE_FIXED_INT32 },

	{ ESOFS( light ), 32, 1, WIRE_FIXED_INT32 },
};

/*
* MSG_WriteEntityNumber
*/
void MSG_WriteEntityNumber( msg_t *msg, int number, bool remove, unsigned byteMask ) {
	MSG_WriteIntBase128( msg, number * (remove ? -1 : 1) );
	MSG_WriteUintBase128( msg, byteMask );
}

/*
* MSG_WriteDeltaEntity
*
* Writes part of a packetentities message.
* Can delta from either a baseline or a previous packet_entity
*/
void MSG_WriteDeltaEntity( msg_t *msg, const entity_state_t *from, const entity_state_t *to, bool force ) {
	int number;
	unsigned byteMask;
	uint8_t fieldMask[32] = { 0 };
	const msg_field_t *fields = ent_state_fields;
	int numFields = sizeof( ent_state_fields ) / sizeof( ent_state_fields[0] );

	assert( numFields < 256 );
	if( numFields > 256 ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: numFields == %i", numFields );
	}

	if( !to ) {
		if( !from )
			Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: Unset base state" );
		number = from->number;
	} else {
		number = to->number;
	}

	if( !number ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: Unset entity number" );
	} else if( number >= MAX_EDICTS ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: Entity number >= MAX_EDICTS" );
	} else if( number < 0 ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: Invalid Entity number" );
	}

	if( !to ) {
		// remove
		MSG_WriteEntityNumber( msg, number, true, 0 );
		return;
	}

	byteMask = MSG_CompareStructs( from, to, fields, numFields, fieldMask, sizeof( fieldMask ) );
	if( !byteMask && !force ) {
		// no changes
		return;
	}

	MSG_WriteEntityNumber( msg, number, false, byteMask );

	MSG_WriteFieldMask( msg, fieldMask, byteMask );

	MSG_WriteStructFields( msg, from, to, fields, numFields, fieldMask, byteMask );
}

/*
* MSG_ReadEntityNumber
*
* Returns the entity number and the remove bit
*/
int MSG_ReadEntityNumber( msg_t *msg, bool *remove, unsigned *byteMask ) {
	int number;

	*remove = false;
	number = (int)MSG_ReadIntBase128( msg );
	*byteMask = MSG_ReadUintBase128( msg );

	if( number < 0 ) {
		number *= -1;
		*remove = true;
	}

	return number;
}

/*
* MSG_ReadDeltaEntity
*
* Can go from either a baseline or a previous packet_entity
*/
void MSG_ReadDeltaEntity( msg_t *msg, const entity_state_t *from, entity_state_t *to, int number, unsigned byteMask ) {
	uint8_t fieldMask[32] = { 0 };
	const msg_field_t *fields = ent_state_fields;
	int numFields = sizeof( ent_state_fields ) / sizeof( ent_state_fields[0] );

	// set everything to the state we are delta'ing from
	*to = *from;
	to->number = number;
	
	MSG_ReadFieldMask( msg, fieldMask, sizeof( fieldMask ), byteMask );

	MSG_ReadStructFields( msg, from, to, fields, numFields, fieldMask, sizeof( fieldMask ), byteMask );
}

//==================================================
// DELTA USER CMDS
//==================================================

#define UCOFS( x ) offsetof( usercmd_t,x )

static const msg_field_t usercmd_fields[] = {
	{ UCOFS( angles[0] ), 16, 1, WIRE_FIXED_INT16 },
	{ UCOFS( angles[1] ), 16, 1, WIRE_FIXED_INT16 },
	{ UCOFS( angles[2] ), 16, 1, WIRE_FIXED_INT16 },

	{ UCOFS( forwardmove ), 8, 1, WIRE_FIXED_INT8 },
	{ UCOFS( sidemove ), 8, 1, WIRE_FIXED_INT8 },
	{ UCOFS( upmove ), 8, 1, WIRE_FIXED_INT8 },

	{ UCOFS( buttons ), 32, 1, WIRE_UBASE128 },
};

/*
* MSG_WriteDeltaUsercmd
*/
void MSG_WriteDeltaUsercmd( msg_t *msg, const usercmd_t *from, usercmd_t *cmd ) {
	int numFields = sizeof( usercmd_fields ) / sizeof( usercmd_fields[0] );
	const msg_field_t *fields = usercmd_fields;

	MSG_WriteDeltaStruct( msg, from, cmd, fields, numFields );

	MSG_WriteIntBase128( msg, cmd->serverTimeStamp );
}

/*
* MSG_ReadDeltaUsercmd
*/
void MSG_ReadDeltaUsercmd( msg_t *msg, const usercmd_t *from, usercmd_t *move ) {
	int numFields = sizeof( usercmd_fields ) / sizeof( usercmd_fields[0] );
	const msg_field_t *fields = usercmd_fields;

	MSG_ReadDeltaStruct( msg, from, move, sizeof( usercmd_t ), fields, numFields );

	move->serverTimeStamp = MSG_ReadIntBase128( msg );
}

//==================================================
// DELTA PLAYER STATES
//==================================================

#define PSOFS( x ) offsetof( player_state_t,x )

static const msg_field_t player_state_msg_fields[] = {
	{ PSOFS( pmove.pm_type ), 32, 1, WIRE_UBASE128 },

	{ PSOFS( pmove.origin[0] ), 0, 1, WIRE_FLOAT },
	{ PSOFS( pmove.origin[1] ), 0, 1, WIRE_FLOAT },
	{ PSOFS( pmove.origin[2] ), 0, 1, WIRE_FLOAT },

	{ PSOFS( pmove.velocity[0] ), 0, 1, WIRE_FLOAT },
	{ PSOFS( pmove.velocity[1] ), 0, 1, WIRE_FLOAT },
	{ PSOFS( pmove.velocity[2] ), 0, 1, WIRE_FLOAT },

	{ PSOFS( pmove.pm_time ), 32, 1, WIRE_UBASE128 },

	{ PSOFS( pmove.pm_flags ), 32, 1, WIRE_UBASE128 },

	{ PSOFS( pmove.delta_angles[0] ), 16, 1, WIRE_FIXED_INT16 },
	{ PSOFS( pmove.delta_angles[1] ), 16, 1, WIRE_FIXED_INT16 },
	{ PSOFS( pmove.delta_angles[2] ), 16, 1, WIRE_FIXED_INT16 },

	{ PSOFS( event[0] ), 32, 1, WIRE_UBASE128 },
	{ PSOFS( eventParm[0] ), 32, 1, WIRE_UBASE128 },

	{ PSOFS( event[1] ), 32, 1, WIRE_UBASE128 },
	{ PSOFS( eventParm[1] ), 32, 1, WIRE_UBASE128 },

	{ PSOFS( viewangles[0] ), 0, 1, WIRE_ANGLE },
	{ PSOFS( viewangles[1] ), 0, 1, WIRE_ANGLE },
	{ PSOFS( viewangles[2] ), 0, 1, WIRE_ANGLE },

	{ PSOFS( pmove.gravity ), 32, 1, WIRE_UBASE128 },

	{ PSOFS( weaponState ), 8, 1, WIRE_FIXED_INT8 },

	{ PSOFS( fov ), 0, 1, WIRE_HALF_FLOAT },

	{ PSOFS( POVnum ), 32, 1, WIRE_UBASE128 },
	{ PSOFS( playerNum ), 32, 1, WIRE_UBASE128 },

	{ PSOFS( viewheight ), 32, 1, WIRE_HALF_FLOAT },

	{ PSOFS( plrkeys ), 32, 1, WIRE_UBASE128 },

	{ PSOFS( stats ), 16, PS_MAX_STATS, WIRE_BASE128 },

	{ PSOFS( pmove.stats ), 16, PM_STAT_SIZE, WIRE_BASE128 },
	{ PSOFS( inventory ), 32, MAX_ITEMS, WIRE_UBASE128 },
};

/*
* MSG_WriteDeltaPlayerstate
*/
void MSG_WriteDeltaPlayerState( msg_t *msg, const player_state_t *ops, const player_state_t *ps ) {
	int numFields = sizeof( player_state_msg_fields ) / sizeof( player_state_msg_fields[0] );
	const msg_field_t *fields = player_state_msg_fields;
	static player_state_t dummy;

	if( !ops ) {
		ops = &dummy;
	}

	MSG_WriteDeltaStruct( msg, ops, ps, fields, numFields );
}

/*
* MSG_ReadDeltaPlayerstate
*/
void MSG_ReadDeltaPlayerState( msg_t *msg, const player_state_t *ops, player_state_t *ps ) {
	int numFields = sizeof( player_state_msg_fields ) / sizeof( player_state_msg_fields[0] );
	const msg_field_t *fields = player_state_msg_fields;
	static player_state_t dummy;

	if( !ops ) {
		ops = &dummy;
	}
	memcpy( ps, ops, sizeof( player_state_t ) );

	MSG_ReadDeltaStruct( msg, ops, ps, sizeof( player_state_t ), fields, numFields );
}

//==================================================
// DELTA GAME STATES
//==================================================

#define GSOFS( x ) offsetof( game_state_t,x )

static const msg_field_t game_state_msg_fields[] = {
	{ GSOFS( stats ), 64, MAX_GAME_STATS, WIRE_BASE128 },
};

/*
* MSG_WriteDeltaGameState
*/
void MSG_WriteDeltaGameState( msg_t *msg, const game_state_t *from, const game_state_t *to ) {
	int numFields = sizeof( game_state_msg_fields ) / sizeof( game_state_msg_fields[0] );
	const msg_field_t *fields = game_state_msg_fields;
	static game_state_t dummy;

	if( !from ) {
		from = &dummy;
	}

	MSG_WriteDeltaStruct( msg, from, to, fields, numFields );
}

/*
* MSG_ReadDeltaGameState
*/
void MSG_ReadDeltaGameState( msg_t *msg, const game_state_t *from, game_state_t *to ) {
	int numFields = sizeof( game_state_msg_fields ) / sizeof( game_state_msg_fields[0] );
	const msg_field_t *fields = game_state_msg_fields;
	static game_state_t dummy;

	if( !from ) {
		from = &dummy;
	}

	MSG_ReadDeltaStruct( msg, from, to, sizeof( game_state_t ), fields, numFields );
}
