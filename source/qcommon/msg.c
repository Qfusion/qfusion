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
#if 0
	unsigned int i;
	for( i = 0; i < length; i++ )
		MSG_WriteUint8( msg, ( (uint8_t *)data )[i] );
#else
	MSG_CopyData( msg, data, length );
#endif
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

void MSG_WriteInt24( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 3 );
	buf[0] = ( uint8_t )( c & 0xff );
	buf[1] = ( uint8_t )( ( c >> 8 ) & 0xff );
	buf[2] = ( uint8_t )( ( c >> 16 ) & 0xff );
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

void MSG_WriteDir( msg_t *msg, vec3_t dir ) {
	MSG_WriteUint8( msg, dir ? DirToByte( dir ) : 0 );
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
		return -1;
	}

	return ( unsigned char )( msg->data[msg->readcount - 1] );
}

int MSG_ReadInt16( msg_t *msg ) {
	msg->readcount += 2;
	if( msg->readcount > msg->cursize ) {
		return -1;
	}

	return ( short )( msg->data[msg->readcount - 2] | ( msg->data[msg->readcount - 1] << 8 ) );
}

int MSG_ReadInt24( msg_t *msg ) {
	msg->readcount += 3;
	if( msg->readcount > msg->cursize ) {
		return -1;
	}

	return msg->data[msg->readcount - 3]
		   | ( msg->data[msg->readcount - 2] << 8 )
		   | ( msg->data[msg->readcount - 1] << 16 )
		   | ( ( msg->data[msg->readcount - 1] & 0x80 ) ? ~0xFFFFFF : 0 );
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

//==================================================
// ENCODED FIELDS
//==================================================

/*
* MSG_CompareField
*/
bool MSG_CompareField( const void *from, const void *to, const msg_field_t *field ) {
	int32_t itv, ifv;
	bool btv, bfv;
	int64_t bitv, bifv;
	float ftv, ffv;

	switch( field->bits ) {
		case 0:
			ftv = *((float *)( (uint8_t *)to + field->offset ));
			ffv = *((float *)( (uint8_t *)from + field->offset ));
			return ftv != ffv;
		case 1:
			btv = *((bool *)( (uint8_t *)to + field->offset ));
			bfv = *((bool *)( (uint8_t *)from + field->offset ));
			return btv != bfv;
		case 8:
			itv = *((int8_t *)( (uint8_t *)to + field->offset ));
			ifv = *((int8_t *)( (uint8_t *)from + field->offset ));
			return itv != ifv;
		case 16:
			itv = *((int16_t *)( (uint8_t *)to + field->offset ));
			ifv = *((int16_t *)( (uint8_t *)from + field->offset ));
			return itv != ifv;
		case 32:
			itv = *((int32_t *)( (uint8_t *)to + field->offset ));
			ifv = *((int32_t *)( (uint8_t *)from + field->offset ));
			return itv != ifv;
		case 64:
			bitv = *((int64_t *)( (uint8_t *)to + field->offset ));
			bifv = *((int64_t *)( (uint8_t *)from + field->offset ));
			return bitv != bifv;
		default:
			Com_Error( ERR_FATAL, "MSG_CompareField: unknown field bits value %i", field->bits );
	}

	return false;
}

/*
* MSG_CompareFields
*/
unsigned MSG_CompareFields( const void *from, const void *to, const msg_field_t *fields, size_t numFields, uint8_t *fieldMask, size_t maskSize ) {
	size_t i;
	unsigned byteMask;

	byteMask = 0;
	for( i = 0; i < numFields; i++ ) {
		size_t byte = i >> 3;

		if( byte > maskSize ) {
			Com_Error( ERR_FATAL, "MSG_CompareFields: byte > maskSize" );
		}
		if( byte > 32 ) {
			Com_Error( ERR_FATAL, "MSG_CompareFields: byte > 32" );
		}

		if( MSG_CompareField( from, to, &fields[i] ) ) {
			fieldMask[byte] |= (1 << (i & 7));
			byteMask |= (1 << (byte & 7));
		}
	}

	return byteMask;
}

/*
* MSG_WriteField
*/
void MSG_WriteField( msg_t *msg, const void *to, const msg_field_t *field ) {
	switch( field->encoding ) {
		case MSG_ENCTYPE_BOOL:
			break;
		case MSG_ENCTYPE_FIXEDINT8:
			MSG_WriteInt8( msg, *((int8_t *)( (uint8_t *)to + field->offset )) );
			break;
		case MSG_ENCTYPE_FIXEDINT16:
			MSG_WriteInt16( msg, *((int16_t *)( (uint8_t *)to + field->offset )) );
			break;
		case MSG_ENCTYPE_FIXEDINT32:
			MSG_WriteInt32( msg, *((int32_t *)( (uint8_t *)to + field->offset )) );
			break;
		case MSG_ENCTYPE_FIXEDINT64:
			MSG_WriteInt64( msg, *((int64_t *)( (uint8_t *)to + field->offset )) );
			break;
		case MSG_ENCTYPE_FLOAT:
			MSG_WriteFloat( msg, *((float *)( (uint8_t *)to + field->offset )) );
			break;
		case MSG_ENCTYPE_FLOAT88:
			MSG_WriteInt16( msg, (int)((*((float *)( (uint8_t *)to + field->offset ))) * 255.0f) );
			break;
		case MSG_ENCTYPE_COORD24:
			MSG_WriteCoord24( msg, *((float *)( (uint8_t *)to + field->offset )) );
			break;
		case MSG_ENCTYPE_ANGLE8:
			MSG_WriteAngle8( msg, *((float *)( (uint8_t *)to + field->offset )) );
			break;
		case MSG_ENCTYPE_ANGLE16:
			MSG_WriteAngle16( msg, *((float *)( (uint8_t *)to + field->offset )) );
			break;
		case MSG_ENCTYPE_BASE128:
			switch( field->bits ) {
			case 8:
				MSG_WriteInt8( msg, *((int8_t *)( (uint8_t *)to + field->offset )) );
				break;
			case 16:
				MSG_WriteIntBase128( msg, *((int16_t *)( (uint8_t *)to + field->offset )) );
				break;
			case 32:
				MSG_WriteIntBase128( msg, *((int32_t *)( (uint8_t *)to + field->offset )) );
				break;
			case 64:
				MSG_WriteIntBase128( msg, *((int64_t *)( (uint8_t *)to + field->offset )) );
				break;
			default:
				Com_Error( ERR_FATAL, "MSG_WriteField: unknown base128 field bits value %i", field->bits );
				break;
			}
			break;
		case MSG_ENCTYPE_UBASE128:
			switch( field->bits ) {
			case 8:
				MSG_WriteUint8( msg, *((uint8_t *)( (uint8_t *)to + field->offset )) );
				break;
			case 16:
				MSG_WriteUintBase128( msg, *((uint16_t *)( (uint8_t *)to + field->offset )) );
				break;
			case 32:
				MSG_WriteUintBase128( msg, *((uint32_t *)( (uint8_t *)to + field->offset )) );
				break;
			case 64:
				MSG_WriteUintBase128( msg, *((uint64_t *)( (uint8_t *)to + field->offset )) );
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
* MSG_WriteFields
*/
void MSG_WriteFields( msg_t *msg, const void *to, const msg_field_t *fields, size_t numFields, const uint8_t *fieldMask, unsigned byteMask ) {
	size_t b;

	b = 0;
	while( byteMask ) {
		if( byteMask & 1 ) {
			unsigned fm = fieldMask[b];
			size_t f = b << 3;

			while( fm ) {
				if( f >= numFields )
					return;
				if( fm & 1 ) {
					MSG_WriteField( msg, to, &fields[f] );
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
* MSG_WriteDeltaStruct
*/
void MSG_WriteDeltaStruct( msg_t *msg, const void *from, const void *to, const msg_field_t *fields, size_t numFields ) {
	unsigned byteMask;
	uint8_t fieldMask[32] = { 0 };

	assert( numFields < 256 );
	if( numFields > 256 ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaStruct: numFields == %i", numFields );
	}

	// send the movement message
	byteMask = MSG_CompareFields( from, to, fields, numFields, fieldMask, sizeof( fieldMask ) );

	MSG_WriteUintBase128( msg, byteMask );

	MSG_WriteFieldMask( msg, fieldMask, byteMask );

	MSG_WriteFields( msg, to, fields, numFields, fieldMask, byteMask );
}

/*
* MSG_ReadField
*/
void MSG_ReadField( msg_t *msg, const void *to, const msg_field_t *field ) {
	switch( field->encoding ) {
	case MSG_ENCTYPE_BOOL:
		*((bool *)( (uint8_t *)to + field->offset )) ^= true;
		break;
	case MSG_ENCTYPE_FIXEDINT8:
		*((int8_t *)( (uint8_t *)to + field->offset )) = MSG_ReadInt8( msg );
		break;
	case MSG_ENCTYPE_FIXEDINT16:
		*((int16_t *)( (uint8_t *)to + field->offset )) = MSG_ReadInt16( msg );
		break;
	case MSG_ENCTYPE_FIXEDINT32:
		*((int32_t *)( (uint8_t *)to + field->offset )) = MSG_ReadInt32( msg );
		break;
	case MSG_ENCTYPE_FIXEDINT64:
		*((int64_t *)( (uint8_t *)to + field->offset )) = MSG_ReadInt64( msg );
		break;
	case MSG_ENCTYPE_FLOAT:
		*((float *)( (uint8_t *)to + field->offset )) = MSG_ReadFloat( msg );
		break;
	case MSG_ENCTYPE_FLOAT88:
		*((float *)( (uint8_t *)to + field->offset )) = (float)MSG_ReadInt16( msg ) / 255.0f;
		break;
	case MSG_ENCTYPE_COORD24:
		*((float *)( (uint8_t *)to + field->offset )) = MSG_ReadCoord24( msg );
		break;
	case MSG_ENCTYPE_ANGLE8:
		*((float *)( (uint8_t *)to + field->offset )) = MSG_ReadAngle8( msg );
		break;
	case MSG_ENCTYPE_ANGLE16:
		*((float *)( (uint8_t *)to + field->offset )) = MSG_ReadAngle16( msg );
		break;
	case MSG_ENCTYPE_BASE128:
		switch( field->bits ) {
		case 8:
			*((int8_t *)( (uint8_t *)to + field->offset )) = MSG_ReadInt8( msg );
			break;
		case 16:
			*((int16_t *)( (uint8_t *)to + field->offset )) = MSG_ReadIntBase128( msg );
			break;
		case 32:
			*((int32_t *)( (uint8_t *)to + field->offset )) = MSG_ReadIntBase128( msg );
			break;
		case 64:
			*((int64_t *)( (uint8_t *)to + field->offset )) = MSG_ReadIntBase128( msg );
			break;
		default:
			Com_Error( ERR_FATAL, "MSG_WriteField: unknown base128 field bits value %i", field->bits );
			break;
		}
		break;
	case MSG_ENCTYPE_UBASE128:
		switch( field->bits ) {
		case 8:
			*((uint8_t *)( (uint8_t *)to + field->offset )) = MSG_ReadUint8( msg );
			break;
		case 16:
			*((uint16_t *)( (uint8_t *)to + field->offset )) = MSG_ReadUintBase128( msg );
			break;
		case 32:
			*((uint32_t *)( (uint8_t *)to + field->offset )) = MSG_ReadUintBase128( msg );
			break;
		case 64:
			*((uint64_t *)( (uint8_t *)to + field->offset )) = MSG_ReadUintBase128( msg );
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
* MSG_ReadFields
*/
void MSG_ReadFields( msg_t *msg, void *to, const msg_field_t *fields, size_t numFields, const uint8_t *fieldMask, size_t maskSize, unsigned byteMask ) {
	size_t b;

	b = 0;
	while( byteMask ) {
		if( byteMask & 1 ) {
			unsigned fm;
			size_t f = b << 3;

			if( b >= maskSize ) {
				Com_Error( ERR_FATAL, "MSG_ReadFields: b >= maxSize" );
			}
			
			fm = fieldMask[b];
			while( fm ) {
				if( f >= numFields ) {
					Com_Error( ERR_FATAL, "MSG_ReadFields: f >= numFields" );
				}

				if( fm & 1 ) {
					MSG_ReadField( msg, to, &fields[f] );
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
* MSG_ReadDeltaStruct
*/
void MSG_ReadDeltaStruct( msg_t *msg, const void *from, void *to, size_t size, const msg_field_t *fields, size_t numFields ) {
	unsigned byteMask;
	uint8_t fieldMask[32] = { 0 };

	assert( numFields < 256 );
	if( numFields > 256 ) {
		Com_Error( ERR_FATAL, "MSG_WriteDeltaStruct: numFields == %i", numFields );
	}

	// set everything to the state we are delta'ing from
	memcpy( to, from, size );

	byteMask = MSG_ReadUintBase128( msg );

	MSG_ReadFieldMask( msg, fieldMask, sizeof( fieldMask ), byteMask );

	MSG_ReadFields( msg, to, fields, numFields, fieldMask, sizeof( fieldMask ), byteMask );
}

//==================================================
// DELTA ENTITIES
//==================================================

#define ESOFS( x ) offsetof( entity_state_t,x )

static const msg_field_t ent_state_fields[] = {
	{ ESOFS( events[0] ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( eventParms[0] ), 32, MSG_ENCTYPE_BASE128 },

	{ ESOFS( origin[0] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( origin[1] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( origin[2] ), 0, MSG_ENCTYPE_COORD24 },

	{ ESOFS( angles[0] ), 0, MSG_ENCTYPE_ANGLE16 },
	{ ESOFS( angles[1] ), 0, MSG_ENCTYPE_ANGLE16 },

	{ ESOFS( teleported ), 1, MSG_ENCTYPE_BOOL },

	{ ESOFS( type ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( solid ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( frame ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( modelindex ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( svflags ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( skinnum ), 32, MSG_ENCTYPE_BASE128 },
	{ ESOFS( effects ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( ownerNum ), 32, MSG_ENCTYPE_BASE128 },
	{ ESOFS( targetNum ), 32, MSG_ENCTYPE_BASE128 },
	{ ESOFS( sound ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( modelindex2 ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( attenuation ), 0, MSG_ENCTYPE_FLOAT88 },
	{ ESOFS( counterNum ), 32, MSG_ENCTYPE_BASE128 },
	{ ESOFS( bodyOwner ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( channel ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( events[1] ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( eventParms[1] ), 32, MSG_ENCTYPE_BASE128 },
	{ ESOFS( weapon ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( firemode ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( damage ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( range ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( team ), 32, MSG_ENCTYPE_UBASE128 },

	{ ESOFS( origin2[0] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( origin2[1] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( origin2[2] ), 0, MSG_ENCTYPE_COORD24 },

	{ ESOFS( linearMovementTimeStamp ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( linearMovement ), 1, MSG_ENCTYPE_BOOL },
	{ ESOFS( linearMovementDuration ), 32, MSG_ENCTYPE_UBASE128 },
	{ ESOFS( linearMovementVelocity[0] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( linearMovementVelocity[1] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( linearMovementVelocity[2] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( linearMovementBegin[0] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( linearMovementBegin[1] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( linearMovementBegin[2] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( linearMovementEnd[0] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( linearMovementEnd[1] ), 0, MSG_ENCTYPE_COORD24 },
	{ ESOFS( linearMovementEnd[2] ), 0, MSG_ENCTYPE_COORD24 },

	{ ESOFS( itemNum ), 32, MSG_ENCTYPE_UBASE128 },

	{ ESOFS( angles[2] ), 0, MSG_ENCTYPE_ANGLE16 },

	{ ESOFS( colorRGBA ), 32, MSG_ENCTYPE_FIXEDINT32 },

	{ ESOFS( light ), 32, MSG_ENCTYPE_FIXEDINT32 },
};

/*
* MSG_WriteEntityNumber
*/
static void MSG_WriteEntityNumber( msg_t *msg, int number, bool remove, unsigned byteMask ) {
	MSG_WriteIntBase128( msg, (remove ? 1 : 0) | number << 1 );
	MSG_WriteUintBase128( msg, byteMask );
}

/*
* MSG_WriteDeltaEntity
*
* Writes part of a packetentities message.
* Can delta from either a baseline or a previous packet_entity
*/
void MSG_WriteDeltaEntity( entity_state_t *from, entity_state_t *to, msg_t *msg, bool force ) {
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

	byteMask = MSG_CompareFields( from, to, fields, numFields, fieldMask, sizeof( fieldMask ) );
	if( !byteMask && !force ) {
		// no changes
		return;
	}

	MSG_WriteEntityNumber( msg, number, false, byteMask );

	MSG_WriteFieldMask( msg, fieldMask, byteMask );

	MSG_WriteFields( msg, to, fields, numFields, fieldMask, byteMask );
}

/*
* MSG_ReadEntityNumber
*
* Returns the entity number and the remove bit
*/
int MSG_ReadEntityNumber( msg_t *msg, bool *remove, unsigned *byteMask ) {
	int number;

	number = (int)MSG_ReadIntBase128( msg );
	*remove = (number & 1 ? true : false);
	number = number >> 1;
	*byteMask = MSG_ReadUintBase128( msg );

	return number;
}

/*
* MSG_ReadDeltaEntity
*
* Can go from either a baseline or a previous packet_entity
*/
void MSG_ReadDeltaEntity( msg_t *msg, entity_state_t *from, entity_state_t *to, int number, unsigned byteMask ) {
	uint8_t fieldMask[32] = { 0 };
	const msg_field_t *fields = ent_state_fields;
	int numFields = sizeof( ent_state_fields ) / sizeof( ent_state_fields[0] );

	// set everything to the state we are delta'ing from
	*to = *from;
	to->number = number;
	
	MSG_ReadFieldMask( msg, fieldMask, sizeof( fieldMask ), byteMask );

	MSG_ReadFields( msg, to, fields, numFields, fieldMask, sizeof( fieldMask ), byteMask );
}

//==================================================
// DELTA USER CMDS
//==================================================

#define UCOFS( x ) offsetof( usercmd_t,x )

static const msg_field_t usercmd_fields[] = {
	{ UCOFS( angles[0] ), 16, MSG_ENCTYPE_FIXEDINT16 },
	{ UCOFS( angles[1] ), 16, MSG_ENCTYPE_FIXEDINT16 },
	{ UCOFS( angles[2] ), 16, MSG_ENCTYPE_FIXEDINT16 },

	{ UCOFS( forwardmove ), 0, MSG_ENCTYPE_FIXEDINT8 },
	{ UCOFS( sidemove ), 0, MSG_ENCTYPE_FIXEDINT8 },
	{ UCOFS( upmove ), 0, MSG_ENCTYPE_FIXEDINT8 },

	{ UCOFS( buttons ), 8, MSG_ENCTYPE_UBASE128 },
};

/*
* MSG_WriteDeltaUsercmd
*/
void MSG_WriteDeltaUsercmd( msg_t *msg, usercmd_t *from, usercmd_t *cmd ) {
	int numFields = sizeof( usercmd_fields ) / sizeof( usercmd_fields[0] );
	const msg_field_t *fields = usercmd_fields;

	MSG_WriteDeltaStruct( msg, from, cmd, fields, numFields );

	MSG_WriteInt32( msg, cmd->serverTimeStamp );
}

/*
* MSG_ReadDeltaUsercmd
*/
void MSG_ReadDeltaUsercmd( msg_t *msg, usercmd_t *from, usercmd_t *move ) {
	int numFields = sizeof( usercmd_fields ) / sizeof( usercmd_fields[0] );
	const msg_field_t *fields = usercmd_fields;

	MSG_ReadDeltaStruct( msg, from, move, sizeof( usercmd_t ), fields, numFields );

	move->serverTimeStamp = MSG_ReadInt32( msg );
}

