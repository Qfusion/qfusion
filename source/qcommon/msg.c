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
#define MAX_MSG_STRING_CHARS	2048

void MSG_Init( msg_t *msg, qbyte *data, size_t length )
{
	memset( msg, 0, sizeof( *msg ) );
	msg->data = data;
	msg->maxsize = length;
	msg->cursize = 0;
	msg->compressed = qfalse;
}

void MSG_Clear( msg_t *msg )
{
	msg->cursize = 0;
	msg->compressed = qfalse;
}

void *MSG_GetSpace( msg_t *msg, size_t length )
{
	void *ptr;

	assert( msg->cursize + length <= msg->maxsize );
	if( msg->cursize + length > msg->maxsize )
		Com_Error( ERR_FATAL, "MSG_GetSpace: overflowed" );

	ptr = msg->data + msg->cursize;
	msg->cursize += length;
	return ptr;
}

//==================================================
// WRITE FUNCTIONS
//==================================================


void MSG_WriteData( msg_t *msg, const void *data, size_t length )
{
#if 0
	unsigned int i;
	for( i = 0; i < length; i++ )
		MSG_WriteByte( msg, ( (qbyte *)data )[i] );
#else
	MSG_CopyData( msg, data, length );
#endif
}

void MSG_CopyData( msg_t *buf, const void *data, size_t length )
{
	memcpy( MSG_GetSpace( buf, length ), data, length );
}

void MSG_WriteChar( msg_t *msg, int c )
{
	qbyte *buf = ( qbyte* )MSG_GetSpace( msg, 1 );
	buf[0] = ( char )c;
}

void MSG_WriteByte( msg_t *msg, int c )
{
	qbyte *buf = ( qbyte* )MSG_GetSpace( msg, 1 );
	buf[0] = ( qbyte )( c&0xff );
}

void MSG_WriteShort( msg_t *msg, int c )
{
	unsigned short *sp = (unsigned short *)MSG_GetSpace( msg, 2 );
	*sp = LittleShort( c );
}

void MSG_WriteInt3( msg_t *msg, int c )
{
	qbyte *buf = ( qbyte* )MSG_GetSpace( msg, 3 );
	buf[0] = ( qbyte )( c&0xff );
	buf[1] = ( qbyte )( ( c>>8 )&0xff );
	buf[2] = ( qbyte )( ( c>>16 )&0xff );
}

void MSG_WriteLong( msg_t *msg, int c )
{
	unsigned int *ip = (unsigned int *)MSG_GetSpace( msg, 4 );
	*ip = LittleLong( c );
}

void MSG_WriteFloat( msg_t *msg, float f )
{
	union {
		float f;
		int l;
	} dat;

	dat.f = f;
	MSG_WriteLong( msg, dat.l );
}

void MSG_WriteDir( msg_t *msg, vec3_t dir )
{
	MSG_WriteByte( msg, dir ? DirToByte( dir ) : 0 );
}

void MSG_WriteString( msg_t *msg, const char *s )
{
	if( !s )
	{
		MSG_WriteData( msg, "", 1 );
	}
	else
	{
		int l = strlen( s );
		if( l >= MAX_MSG_STRING_CHARS )
		{
			Com_Printf( "MSG_WriteString: MAX_MSG_STRING_CHARS overflow" );
			MSG_WriteData( msg, "", 1 );
			return;
		}
		MSG_WriteData( msg, s, l+1 );
	}
}

//==================================================
// READ FUNCTIONS
//==================================================

void MSG_BeginReading( msg_t *msg )
{
	msg->readcount = 0;
}

int MSG_ReadChar( msg_t *msg )
{
	int i = (signed char)msg->data[msg->readcount++];
	if( msg->readcount > msg->cursize )
		i = -1;
	return i;
}


int MSG_ReadByte( msg_t *msg )
{
	int i = (unsigned char)msg->data[msg->readcount++];
	if( msg->readcount > msg->cursize )
		i = -1;
	return i;
}

int MSG_ReadShort( msg_t *msg )
{
	int i;
	short *sp = (short *)&msg->data[msg->readcount];
	i = LittleShort( *sp );
	msg->readcount += 2;
	if( msg->readcount > msg->cursize )
		i = -1;
	return i;
}

int MSG_ReadInt3( msg_t *msg )
{
	int i = msg->data[msg->readcount]
	| ( msg->data[msg->readcount+1]<<8 )
		| ( msg->data[msg->readcount+2]<<16 )
		| ( ( msg->data[msg->readcount+2] & 0x80 ) ? ~0xFFFFFF : 0 );
	msg->readcount += 3;
	if( msg->readcount > msg->cursize )
		i = -1;
	return i;
}

int MSG_ReadLong( msg_t *msg )
{
	int i;
	unsigned int *ip = (unsigned int *)&msg->data[msg->readcount];
	i = LittleLong( *ip );
	msg->readcount += 4;
	if( msg->readcount > msg->cursize )
		i = -1;
	return i;
}

float MSG_ReadFloat( msg_t *msg )
{
	union {


		float f;
		int l;
	} dat;

	dat.l = MSG_ReadLong( msg );
	if( msg->readcount > msg->cursize )
		dat.f = -1;
	return dat.f;
}

void MSG_ReadDir( msg_t *msg, vec3_t dir )
{
	ByteToDir( MSG_ReadByte( msg ), dir );
}

void MSG_ReadData( msg_t *msg, void *data, size_t length )
{
	unsigned int i;

	for( i = 0; i < length; i++ )
		( (qbyte *)data )[i] = MSG_ReadByte( msg );

}

int MSG_SkipData( msg_t *msg, size_t length )
{
	if( msg->readcount + length <= msg->cursize )
	{
		msg->readcount += length;
		return 1;
	}
	return 0;
}

static char *MSG_ReadString2( msg_t *msg, qboolean linebreak )
{
	int l, c;
	static char string[MAX_MSG_STRING_CHARS];

	l = 0;
	do
	{
		c = MSG_ReadByte( msg );
		if( c == -1 || c == 0 || ( linebreak && c == '\n' ) ) break;

		string[l] = c;
		l++;
	}
	while( (unsigned int)l < sizeof( string )-1 );

	string[l] = 0;

	return string;
}

char *MSG_ReadString( msg_t *msg )
{
	return MSG_ReadString2( msg, qfalse );
}

char *MSG_ReadStringLine( msg_t *msg )
{
	return MSG_ReadString2( msg, qtrue );
}

//==================================================
// SPECIAL CASES
//==================================================

/*
* MSG_WriteDeltaEntity
* 
* Writes part of a packetentities message.
* Can delta from either a baseline or a previous packet_entity
*/
void MSG_WriteDeltaEntity( entity_state_t *from, entity_state_t *to, msg_t *msg, qboolean force, qboolean updateOtherOrigin )
{
	int bits;

	if( !to->number )
		Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: Unset entity number" );
	else if( to->number >= MAX_EDICTS )
		Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: Entity number >= MAX_EDICTS" );
	else if( to->number < 0 )
		Com_Error( ERR_FATAL, "MSG_WriteDeltaEntity: Invalid Entity number" );

	// send an update
	bits = 0;

	if( to->number & 0xFF00 )
		bits |= U_NUMBER16; // number8 is implicit otherwise

	if( to->linearProjectile )
	{
		if( to->linearProjectileVelocity[0] != from->linearProjectileVelocity[0] )
			bits |= U_ORIGIN1;
		if( to->linearProjectileVelocity[1] != from->linearProjectileVelocity[1] )
			bits |= U_ORIGIN2;
		if( to->linearProjectileVelocity[2] != from->linearProjectileVelocity[2] )
			bits |= U_ORIGIN3;
	}
	else
	{
		if( to->origin[0] != from->origin[0] )
			bits |= U_ORIGIN1;
		if( to->origin[1] != from->origin[1] )
			bits |= U_ORIGIN2;
		if( to->origin[2] != from->origin[2] )
			bits |= U_ORIGIN3;
	}

	if( to->angles[0] != from->angles[0] )
		bits |= U_ANGLE1;
	if( to->angles[1] != from->angles[1] )
		bits |= U_ANGLE2;
	if( to->angles[2] != from->angles[2] )
		bits |= U_ANGLE3;

	if( to->skinnum != from->skinnum )
	{
		if( to->skinnum & 0xFFFF0000 )
			bits |= ( U_SKIN8|U_SKIN16 );
		else if( to->skinnum & 0xFF00 )
			bits |= U_SKIN16;
		else
			bits |= U_SKIN8;
	}

	if( to->frame != from->frame )
	{
		if( to->frame & 0xFF00 )
			bits |= U_FRAME16;
		else
			bits |= U_FRAME8;
	}

	if( to->effects != from->effects )
	{
		if( to->effects & 0xFFFF0000 )
			bits |= ( U_EFFECTS8|U_EFFECTS16 );
		else if( to->effects & 0xFF00 )
			bits |= U_EFFECTS16;
		else
			bits |= U_EFFECTS8;
	}

	if( to->solid != from->solid )
		bits |= U_SOLID;

	// events are not delta compressed, just 0 compressed
	if( to->events[0] )
		bits |= U_EVENT;
	if( to->events[1] )
		bits |= U_EVENT2;

	if( to->modelindex != from->modelindex )
		bits |= U_MODEL;
	if( to->modelindex2 != from->modelindex2 )
		bits |= U_MODEL2;

	if( ( to->type != from->type ) || ( to->linearProjectile != from->linearProjectile ) )
		bits |= U_TYPE;

	if( to->sound != from->sound )
		bits |= U_SOUND;

	if( updateOtherOrigin )
	{
		if( to->origin2[0] != from->origin2[0] || to->origin2[1] != from->origin2[1] || to->origin2[2] != from->origin2[2] )
			bits |= U_OTHERORIGIN;
	}

	if( to->weapon != from->weapon || to->teleported != from->teleported )
		bits |= U_WEAPON;

	if( to->svflags != from->svflags )
		bits |= U_SVFLAGS;

	if( to->light != from->light )
		bits |= U_LIGHT;

	if( to->team != from->team )
		bits |= U_TEAM;

	/*
	// small check for testing delta compression on linear projectiles
	if( to->linearProjectile ) 
	{
	if( bits & (U_ORIGIN1|U_ORIGIN2|U_ORIGIN3|U_OTHERORIGIN|U_WEAPON|U_LIGHT) )
	Com_Printf( "LINEAR PROJECTILE Delta compression test\n" );
	}

	if( bits & (U_OTHERORIGIN) )
	{
	if( to->type == 0 )
	Com_Printf( "ET_GENERIC: U_OTHERORIGIN: updated\n" );
	if( to->type == 1 )
	Com_Printf( "ET_PLAYER: U_OTHERORIGIN: updated\n" );
	if( to->type == 2 )
	Com_Printf( "ET_CORPSE: U_OTHERORIGIN: updated\n" );
	if( to->type == 6 )
	Com_Printf( "ET_PUSH_TRIGGER: U_OTHERORIGIN: updated\n" );
	if( to->type == 14 )
	Com_Printf( "ET_ITEM: U_OTHERORIGIN: updated\n" );
	}
	*/

	//
	// write the message
	//
	if( !bits && !force )
		return; // nothing to send!

	//----------

	if( bits & 0xff000000 )
		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if( bits & 0x00ff0000 )
		bits |= U_MOREBITS2 | U_MOREBITS1;
	else if( bits & 0x0000ff00 )
		bits |= U_MOREBITS1;

	MSG_WriteByte( msg, bits&255 );

	if( bits & 0xff000000 )
	{
		MSG_WriteByte( msg, ( bits>>8 )&255 );
		MSG_WriteByte( msg, ( bits>>16 )&255 );
		MSG_WriteByte( msg, ( bits>>24 )&255 );
	}
	else if( bits & 0x00ff0000 )
	{
		MSG_WriteByte( msg, ( bits>>8 )&255 );
		MSG_WriteByte( msg, ( bits>>16 )&255 );
	}
	else if( bits & 0x0000ff00 )
	{
		MSG_WriteByte( msg, ( bits>>8 )&255 );
	}

	//----------

	if( bits & U_NUMBER16 )
		MSG_WriteShort( msg, to->number );
	else
		MSG_WriteByte( msg, to->number );

	if( bits & U_TYPE )
	{
		qbyte ttype = 0;
		ttype = to->type & ~ET_INVERSE;
		if( to->linearProjectile )
			ttype |= ET_INVERSE;
		MSG_WriteByte( msg, ttype );
	}

	if( bits & U_SOLID )
		MSG_WriteShort( msg, to->solid );

	if( bits & U_MODEL )
		MSG_WriteByte( msg, to->modelindex );
	if( bits & U_MODEL2 )
		MSG_WriteByte( msg, to->modelindex2 );

	if( bits & U_FRAME8 )
		MSG_WriteByte( msg, to->frame );
	else if( bits & U_FRAME16 )
		MSG_WriteShort( msg, to->frame );

	if( ( bits & U_SKIN8 ) && ( bits & U_SKIN16 ) )  //used for laser colors
		MSG_WriteLong( msg, to->skinnum );
	else if( bits & U_SKIN8 )
		MSG_WriteByte( msg, to->skinnum );
	else if( bits & U_SKIN16 )
		MSG_WriteShort( msg, to->skinnum );


	if( ( bits & ( U_EFFECTS8|U_EFFECTS16 ) ) == ( U_EFFECTS8|U_EFFECTS16 ) )
		MSG_WriteLong( msg, to->effects );
	else if( bits & U_EFFECTS8 )
		MSG_WriteByte( msg, to->effects );
	else if( bits & U_EFFECTS16 )
		MSG_WriteShort( msg, to->effects );

	if( to->linearProjectile )
	{
		if( bits & U_ORIGIN1 )
			MSG_WriteCoord( msg, to->linearProjectileVelocity[0] );
		if( bits & U_ORIGIN2 )
			MSG_WriteCoord( msg, to->linearProjectileVelocity[1] );
		if( bits & U_ORIGIN3 )
			MSG_WriteCoord( msg, to->linearProjectileVelocity[2] );
	}
	else
	{
		if( bits & U_ORIGIN1 )
			MSG_WriteCoord( msg, to->origin[0] );
		if( bits & U_ORIGIN2 )
			MSG_WriteCoord( msg, to->origin[1] );
		if( bits & U_ORIGIN3 )
			MSG_WriteCoord( msg, to->origin[2] );
	}

	if( bits & U_ANGLE1 && ( to->solid == SOLID_BMODEL ) )
		MSG_WriteAngle16( msg, to->angles[0] );
	else if( bits & U_ANGLE1 )
		MSG_WriteAngle( msg, to->angles[0] );

	if( bits & U_ANGLE2 && ( to->solid == SOLID_BMODEL ) )
		MSG_WriteAngle16( msg, to->angles[1] );
	else if( bits & U_ANGLE2 )
		MSG_WriteAngle( msg, to->angles[1] );

	if( bits & U_ANGLE3 && ( to->solid == SOLID_BMODEL ) )
		MSG_WriteAngle16( msg, to->angles[2] );
	else if( bits & U_ANGLE3 )
		MSG_WriteAngle( msg, to->angles[2] );

	if( bits & U_OTHERORIGIN )
	{
		MSG_WriteCoord( msg, to->origin2[0] );
		MSG_WriteCoord( msg, to->origin2[1] );
		MSG_WriteCoord( msg, to->origin2[2] );
	}

	if( bits & U_SOUND )
		MSG_WriteByte( msg, (qbyte)to->sound );
	if( bits & U_EVENT )
	{
		if( !to->eventParms[0] )
		{
			MSG_WriteByte( msg, (qbyte)( to->events[0] & ~EV_INVERSE ) );
		}
		else
		{
			MSG_WriteByte( msg, (qbyte)( to->events[0] | EV_INVERSE ) );
			MSG_WriteByte( msg, (qbyte)to->eventParms[0] );
		}
	}
	if( bits & U_EVENT2 )
	{
		if( !to->eventParms[1] )
		{
			MSG_WriteByte( msg, (qbyte)( to->events[1] & ~EV_INVERSE ) );
		}
		else
		{
			MSG_WriteByte( msg, (qbyte)( to->events[1] | EV_INVERSE ) );
			MSG_WriteByte( msg, (qbyte)to->eventParms[1] );
		}
	}

	if( bits & U_WEAPON )
	{
		qbyte tweapon = 0;
		tweapon = to->weapon & ~ET_INVERSE;
		if( to->teleported )
			tweapon |= ET_INVERSE;
		MSG_WriteByte( msg, tweapon );
	}

	if( bits & U_SVFLAGS )
	{
		MSG_WriteShort( msg, to->svflags );
	}

	if( bits & U_LIGHT )
	{
		MSG_WriteLong( msg, to->light );
	}

	if( bits & U_TEAM )
	{
		MSG_WriteByte( msg, to->team );
	}
}

/*
* MSG_ReadEntityBits
* 
* Returns the entity number and the header bits
*/
int MSG_ReadEntityBits( msg_t *msg, unsigned *bits )
{
	unsigned b, total;
	int number;

	total = (qbyte)MSG_ReadByte( msg );
	if( total & U_MOREBITS1 )
	{
		b = (qbyte)MSG_ReadByte( msg );
		total |= ( b<<8 )&0x0000FF00;
	}
	if( total & U_MOREBITS2 )
	{
		b = (qbyte)MSG_ReadByte( msg );
		total |= ( b<<16 )&0x00FF0000;
	}
	if( total & U_MOREBITS3 )
	{
		b = (qbyte)MSG_ReadByte( msg );
		total |= ( b<<24 )&0xFF000000;
	}

	if( total & U_NUMBER16 )
		number = MSG_ReadShort( msg );
	else
		number = (qbyte)MSG_ReadByte( msg );

	*bits = total;

	return number;
}

/*
* MSG_ReadDeltaEntity
* 
* Can go from either a baseline or a previous packet_entity
*/
void MSG_ReadDeltaEntity( msg_t *msg, entity_state_t *from, entity_state_t *to, int number, unsigned bits )
{
	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = number;

	if( bits & U_TYPE )
	{
		qbyte ttype;
		ttype = (qbyte)MSG_ReadByte( msg );
		to->type = ttype & ~ET_INVERSE;
		to->linearProjectile = ( ttype & ET_INVERSE ) ? qtrue : qfalse;
	}

	if( bits & U_SOLID )
		to->solid = MSG_ReadShort( msg );

	if( bits & U_MODEL )
		to->modelindex = (qbyte)MSG_ReadByte( msg );
	if( bits & U_MODEL2 )
		to->modelindex2 = (qbyte)MSG_ReadByte( msg );

	if( bits & U_FRAME8 )
		to->frame = (qbyte)MSG_ReadByte( msg );
	if( bits & U_FRAME16 )
		to->frame = MSG_ReadShort( msg );

	if( ( bits & U_SKIN8 ) && ( bits & U_SKIN16 ) )  //used for laser colors
		to->skinnum = MSG_ReadLong( msg );
	else if( bits & U_SKIN8 )
		to->skinnum = MSG_ReadByte( msg );
	else if( bits & U_SKIN16 )
		to->skinnum = MSG_ReadShort( msg );

	if( ( bits & ( U_EFFECTS8|U_EFFECTS16 ) ) == ( U_EFFECTS8|U_EFFECTS16 ) )
		to->effects = MSG_ReadLong( msg );
	else if( bits & U_EFFECTS8 )
		to->effects = (qbyte)MSG_ReadByte( msg );
	else if( bits & U_EFFECTS16 )
		to->effects = MSG_ReadShort( msg );

	if( to->linearProjectile )
	{
		if( bits & U_ORIGIN1 )
			to->linearProjectileVelocity[0] = MSG_ReadCoord( msg );
		if( bits & U_ORIGIN2 )
			to->linearProjectileVelocity[1] = MSG_ReadCoord( msg );
		if( bits & U_ORIGIN3 )
			to->linearProjectileVelocity[2] = MSG_ReadCoord( msg );
	}
	else
	{
		if( bits & U_ORIGIN1 )
			to->origin[0] = MSG_ReadCoord( msg );
		if( bits & U_ORIGIN2 )
			to->origin[1] = MSG_ReadCoord( msg );
		if( bits & U_ORIGIN3 )
			to->origin[2] = MSG_ReadCoord( msg );
	}

	if( ( bits & U_ANGLE1 ) && ( to->solid == SOLID_BMODEL ) )
		to->angles[0] = MSG_ReadAngle16( msg );
	else if( bits & U_ANGLE1 )
		to->angles[0] = MSG_ReadAngle( msg );

	if( ( bits & U_ANGLE2 ) && ( to->solid == SOLID_BMODEL ) )
		to->angles[1] = MSG_ReadAngle16( msg );
	else if( bits & U_ANGLE2 )
		to->angles[1] = MSG_ReadAngle( msg );

	if( ( bits & U_ANGLE3 ) && ( to->solid == SOLID_BMODEL ) )
		to->angles[2] = MSG_ReadAngle16( msg );
	else if( bits & U_ANGLE3 )
		to->angles[2] = MSG_ReadAngle( msg );

	if( bits & U_OTHERORIGIN )
		MSG_ReadPos( msg, to->origin2 );

	if( bits & U_SOUND )
		to->sound = (qbyte)MSG_ReadByte( msg );

	if( bits & U_EVENT )
	{
		int event = (qbyte)MSG_ReadByte( msg );
		if( event & EV_INVERSE )
			to->eventParms[0] = (qbyte)MSG_ReadByte( msg );
		else
			to->eventParms[0] = 0;
		to->events[0] = ( event & ~EV_INVERSE );
	}
	else
	{
		to->events[0] = 0;
		to->eventParms[0] = 0;
	}

	if( bits & U_EVENT2 )
	{
		int event = (qbyte)MSG_ReadByte( msg );
		if( event & EV_INVERSE )
			to->eventParms[1] = (qbyte)MSG_ReadByte( msg );
		else
			to->eventParms[1] = 0;
		to->events[1] = ( event & ~EV_INVERSE );
	}
	else
	{
		to->events[1] = 0;
		to->eventParms[1] = 0;
	}

	if( bits & U_WEAPON )
	{
		qbyte tweapon;
		tweapon = (qbyte)MSG_ReadByte( msg );
		to->weapon = tweapon & ~ET_INVERSE;
		to->teleported = ( tweapon & ET_INVERSE ) ? qtrue : qfalse;
	}

	if( bits & U_SVFLAGS )
		to->svflags = MSG_ReadShort( msg );

	if( bits & U_LIGHT )
	{
		if( to->linearProjectile )
			to->linearProjectileTimeStamp = (unsigned int)MSG_ReadLong( msg );
		else
			to->light = MSG_ReadLong( msg );
	}

	if( bits & U_TEAM )
		to->team = (qbyte)MSG_ReadByte( msg );
}


void MSG_WriteDeltaUsercmd( msg_t *buf, usercmd_t *from, usercmd_t *cmd )
{
	int bits;

	// send the movement message

	bits = 0;
	if( cmd->angles[0] != from->angles[0] )
		bits |= CM_ANGLE1;
	if( cmd->angles[1] != from->angles[1] )
		bits |= CM_ANGLE2;
	if( cmd->angles[2] != from->angles[2] )
		bits |= CM_ANGLE3;

	if( cmd->forwardfrac != from->forwardfrac )
		bits |= CM_FORWARD;
	if( cmd->sidefrac != from->sidefrac )
		bits |= CM_SIDE;
	if( cmd->upfrac != from->upfrac )
		bits |= CM_UP;

	if( cmd->buttons != from->buttons )
		bits |= CM_BUTTONS;

	MSG_WriteByte( buf, bits );

	if( bits & CM_ANGLE1 )
		MSG_WriteShort( buf, cmd->angles[0] );
	if( bits & CM_ANGLE2 )
		MSG_WriteShort( buf, cmd->angles[1] );
	if( bits & CM_ANGLE3 )
		MSG_WriteShort( buf, cmd->angles[2] );

	if( bits & CM_FORWARD )
		MSG_WriteChar( buf, (int)( cmd->forwardfrac * UCMD_PUSHFRAC_SNAPSIZE ) );
	if( bits & CM_SIDE )
		MSG_WriteChar( buf, (int)( cmd->sidefrac * UCMD_PUSHFRAC_SNAPSIZE ) );
	if( bits & CM_UP )
		MSG_WriteChar( buf, (int)( cmd->upfrac * UCMD_PUSHFRAC_SNAPSIZE ) );

	if( bits & CM_BUTTONS )
		MSG_WriteByte( buf, cmd->buttons );

	MSG_WriteLong( buf, cmd->serverTimeStamp );

}

void MSG_ReadDeltaUsercmd( msg_t *msg_read, usercmd_t *from, usercmd_t *move )
{
	int bits;

	memcpy( move, from, sizeof( *move ) );

	bits = MSG_ReadByte( msg_read );

	// read current angles
	if( bits & CM_ANGLE1 )
		move->angles[0] = MSG_ReadShort( msg_read );
	if( bits & CM_ANGLE2 )
		move->angles[1] = MSG_ReadShort( msg_read );
	if( bits & CM_ANGLE3 )
		move->angles[2] = MSG_ReadShort( msg_read );

	// read movement
	if( bits & CM_FORWARD )
		move->forwardfrac = (float)MSG_ReadChar( msg_read )/UCMD_PUSHFRAC_SNAPSIZE;
	if( bits & CM_SIDE )
		move->sidefrac = (float)MSG_ReadChar( msg_read )/UCMD_PUSHFRAC_SNAPSIZE;
	if( bits & CM_UP )
		move->upfrac = (float)MSG_ReadChar( msg_read )/UCMD_PUSHFRAC_SNAPSIZE;

	// read buttons
	if( bits & CM_BUTTONS )
		move->buttons = MSG_ReadByte( msg_read );

	move->serverTimeStamp = MSG_ReadLong( msg_read );
}
