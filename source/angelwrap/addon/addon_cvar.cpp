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
#include "addon_string.h"
#include "addon_cvar.h"

//#define CVAR_FORCESET

// CLASS: Cvar
void objectCVar_Constructor( asstring_t *name, asstring_t *value, unsigned int flags, ascvar_t *self ) {
	self->cvar = trap_Cvar_Get( name->buffer, value->buffer, flags );
}

void objectCVar_CopyConstructor( ascvar_t *other, ascvar_t *self ) {
	self->cvar = other->cvar;
}

static void objectCVar_Reset( ascvar_t *self ) {
	if( !self->cvar ) {
		return;
	}

	trap_Cvar_Set( self->cvar->name, self->cvar->dvalue );
}

static void objectCVar_setS( asstring_t *str, ascvar_t *self ) {
	if( !str || !self->cvar ) {
		return;
	}

	trap_Cvar_Set( self->cvar->name, str->buffer );
}

static void objectCVar_setF( float value, ascvar_t *self ) {
	if( !self->cvar ) {
		return;
	}

	trap_Cvar_SetValue( self->cvar->name, value );
}

static void objectCVar_setI( int value, ascvar_t *self ) {
	objectCVar_setF( (float)value, self );
}

static void objectCVar_setD( double value, ascvar_t *self ) {
	objectCVar_setF( (float)value, self );
}

#ifdef CVAR_FORCESET
static void objectCVar_forceSetS( asstring_t *str, ascvar_t *self ) {
	if( !str || !self->cvar ) {
		return;
	}

	trap_Cvar_ForceSet( self->cvar->name, str->buffer );
}

static void objectCVar_forceSetF( float value, ascvar_t *self ) {
	if( !self->cvar ) {
		return;
	}

	trap_Cvar_ForceSet( self->cvar->name, va( "%f", value ) );
}

static void objectCVar_forceSetI( int value, ascvar_t *self ) {
	objectCVar_forceSetF( (float)value, self );
}

static void objectCVar_forceSetD( double value, ascvar_t *self ) {
	objectCVar_forceSetF( (float)value, self );
}
#endif

static bool objectCVar_getBool( ascvar_t *self ) {
	if( !self->cvar ) {
		return false;
	}

	return ( self->cvar->integer != 0 );
}

static bool objectCVar_getModified( ascvar_t *self ) {
	if( !self->cvar ) {
		return false;
	}

	return self->cvar->modified;
}

static int objectCVar_getInteger( ascvar_t *self ) {
	if( !self->cvar ) {
		return 0;
	}

	return self->cvar->integer;
}

static float objectCVar_getValue( ascvar_t *self ) {
	if( !self->cvar ) {
		return 0;
	}

	return self->cvar->value;
}

static void objectCVar_setModified( bool modified, ascvar_t *self ) {
	if( !self->cvar ) {
		return;
	}

	self->cvar->modified = modified;
}

static const asstring_t *objectCVar_getName( ascvar_t *self ) {
	if( !self->cvar || !self->cvar->name ) {
		return objectString_ConstFactoryBuffer( NULL, 0 );
	}

	return objectString_ConstFactoryBuffer( self->cvar->name, strlen( self->cvar->name ) );
}

static const asstring_t *objectCVar_getString( ascvar_t *self ) {
	if( !self->cvar || !self->cvar->string ) {
		return objectString_ConstFactoryBuffer( NULL, 0 );
	}

	return objectString_ConstFactoryBuffer( self->cvar->string, strlen( self->cvar->string ) );
}

static const asstring_t *objectCVar_getDefaultString( ascvar_t *self ) {
	if( !self->cvar || !self->cvar->dvalue ) {
		return objectString_ConstFactoryBuffer( NULL, 0 );
	}

	return objectString_ConstFactoryBuffer( self->cvar->dvalue, strlen( self->cvar->dvalue ) );
}

static const asstring_t *objectCVar_getLatchedString( ascvar_t *self ) {
	if( !self->cvar || !self->cvar->latched_string ) {
		return objectString_ConstFactoryBuffer( NULL, 0 );
	}

	return objectString_ConstFactoryBuffer( self->cvar->latched_string, strlen( self->cvar->latched_string ) );
}

// same as vtos
static asstring_t *objectCVar_ToString( ascvar_t *self )
{
	if( !self->cvar || !self->cvar->string ) {
		return objectString_FactoryBuffer( NULL, 0 );
	}
	return objectString_FactoryBuffer( self->cvar->string, strlen( self->cvar->string ) );
}

void PreRegisterCvarAddon( asIScriptEngine *engine ) {
	int r;

	// register the vector type
	r = engine->RegisterObjectType( "Cvar", sizeof( ascvar_t ), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C ); assert( r >= 0 );

	// register the cvar flags enum
	r = engine->RegisterEnum( "eCvarFlag" ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}

void RegisterCvarAddon( asIScriptEngine *engine ) {
	int r;

	// register object behaviours
	r = engine->RegisterObjectBehaviour( "Cvar", asBEHAVE_CONSTRUCT, "void f(const String &in, const String &in, const uint flags)", asFUNCTION( objectCVar_Constructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Cvar", asBEHAVE_CONSTRUCT, "void f(const Cvar &in)", asFUNCTION( objectCVar_CopyConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	// register object methods

	r = engine->RegisterObjectMethod( "Cvar", "void reset()", asFUNCTION( objectCVar_Reset ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "void set( const String &in )", asFUNCTION( objectCVar_setS ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "void set( float value )", asFUNCTION( objectCVar_setF ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "void set( int value )", asFUNCTION( objectCVar_setI ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "void set( double value )", asFUNCTION( objectCVar_setD ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
#ifdef CVAR_FORCESET
	r = engine->RegisterObjectMethod( "Cvar", "void forceSet( const String &in )", asFUNCTION( objectCVar_forceSetS ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "void forceSet( float value )", asFUNCTION( objectCVar_forceSetF ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "void forceSet( int value )", asFUNCTION( objectCVar_forceSetI ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "void forceSet( double value )", asFUNCTION( objectCVar_forceSetD ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
#endif
	r = engine->RegisterObjectMethod( "Cvar", "void set_modified( bool modified )", asFUNCTION( objectCVar_setModified ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Cvar", "bool get_modified() const", asFUNCTION( objectCVar_getModified ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "bool get_boolean() const", asFUNCTION( objectCVar_getBool ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "int get_integer() const", asFUNCTION( objectCVar_getInteger ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "float get_value() const", asFUNCTION( objectCVar_getValue ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "const String @ get_name() const", asFUNCTION( objectCVar_getName ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "const String @ get_string() const", asFUNCTION( objectCVar_getString ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "const String @ get_defaultString() const", asFUNCTION( objectCVar_getDefaultString ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Cvar", "const String @ get_latchedString() const", asFUNCTION( objectCVar_getLatchedString ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	asITypeInfo *type = engine->GetTypeInfoByName( "Cvar" );
	type->SetUserData( (void *)&objectCVar_ToString, 33 );

	// enums
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_ARCHIVE", CVAR_ARCHIVE ); assert( r >= 0 );
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_USERINFO", CVAR_USERINFO ); assert( r >= 0 );
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_SERVERINFO", CVAR_SERVERINFO ); assert( r >= 0 );
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_NOSET", CVAR_NOSET ); assert( r >= 0 );
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_LATCH", CVAR_LATCH ); assert( r >= 0 );
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_LATCH_VIDEO", CVAR_LATCH_VIDEO ); assert( r >= 0 );
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_LATCH_SOUND", CVAR_LATCH_SOUND ); assert( r >= 0 );
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_CHEAT", CVAR_CHEAT ); assert( r >= 0 );
	r = engine->RegisterEnumValue( "eCvarFlag", "CVAR_READONLY", CVAR_READONLY ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}
