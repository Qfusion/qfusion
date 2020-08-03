/*
Copyright (C) 2008 German Garcia
Copyright (C) 2011 Chasseur de bots

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

#include "../qas_precompiled.h"
#include "addon_time.h"

// CLASS: Time
void objectTime_DefaultConstructor( astime_t *self ) {
	self->time = 0;
}

void objectTime_ConstructorInt64( int64_t time, astime_t *self ) {
	self->time = time;

	if( time ) {
		struct tm *tm;
		time_t time_ = time;
		tm = localtime( &time_ );
		self->localtime = *tm;
	}
}

void objectTime_CopyConstructor( astime_t *other, astime_t *self ) {
	self->time = other->time;
	self->localtime = other->localtime;
}

static astime_t *objectTime_AssignBehaviour( astime_t *other, astime_t *self ) {
	self->time = other->time;
	memcpy( &( self->localtime ), &( other->localtime ), sizeof( struct tm ) );
	return self;
}

static bool objectTime_EqualBehaviour( astime_t *first, astime_t *second ) {
	return ( first->time == second->time );
}

void PreRegisterTimeAddon( asIScriptEngine *engine ) {
	int r;

	// register the time type
	r = engine->RegisterObjectType( "Time", sizeof( astime_t ), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLINTS ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}

void RegisterTimeAddon( asIScriptEngine *engine ) {
	int r;

	// register object behaviours
	r = engine->RegisterObjectBehaviour( "Time", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( objectTime_DefaultConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Time", asBEHAVE_CONSTRUCT, "void f(int64 t)", asFUNCTION( objectTime_ConstructorInt64 ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Time", asBEHAVE_CONSTRUCT, "void f(const Time &in)", asFUNCTION( objectTime_CopyConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	// register object methods

	// assignments
	r = engine->RegisterObjectMethod( "Time", "Time &opAssign(const Time &in)", asFUNCTION( objectTime_AssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	// == !=
	r = engine->RegisterObjectMethod( "Time", "bool opEquals(const Time &in)", asFUNCTION( objectTime_EqualBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );

	// properties
	r = engine->RegisterObjectProperty( "Time", "const int64 time", asOFFSET( astime_t, time ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int sec", asOFFSET( astime_t, localtime.tm_sec ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int min", asOFFSET( astime_t, localtime.tm_min ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int hour", asOFFSET( astime_t, localtime.tm_hour ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int mday", asOFFSET( astime_t, localtime.tm_mday ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int mon", asOFFSET( astime_t, localtime.tm_mon ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int year", asOFFSET( astime_t, localtime.tm_year ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int wday", asOFFSET( astime_t, localtime.tm_wday ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int yday", asOFFSET( astime_t, localtime.tm_yday ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Time", "const int isdst", asOFFSET( astime_t, localtime.tm_isdst ) ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}
