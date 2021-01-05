/*
Copyright (C) 2021 Victor Luchits

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
#include "addon_vec2.h"
#include "addon_scriptarray.h"
#include "addon_string.h"

// CLASS: Vec2
void objectVec2_DefaultConstructor( asvec2_t *self ) {
	self->v[0] = self->v[1] = 0;
}

void objectVec2_Constructor3F( float x, float y, asvec2_t *self ) {
	self->v[0] = x;
	self->v[1] = y;
}

void objectVec2_CopyConstructor( asvec2_t *other, asvec2_t *self ) {
	self->v[0] = other->v[0];
	self->v[1] = other->v[1];
}

void objectVec2_ConstructorArray( CScriptArrayInterface &arr, asvec2_t *self )
{
	if( arr.GetSize() != 2 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Invalid array size" );
		}
		return;
	}

	for( unsigned int i = 0; i < 2; i++ ) {
		self->v[i] = *( (float *)arr.At( i ) );
	}
}

static asvec2_t *objectVec2_AssignBehaviour( asvec2_t *other, asvec2_t *self ) {
	Vector2Copy( other->v, self->v );
	return self;
}

static asvec2_t *objectVec2_AddAssignBehaviour( asvec2_t *other, asvec2_t *self ) {
	Vector2Add( self->v, other->v, self->v );
	return self;
}

static asvec2_t *objectVec2_SubAssignBehaviour( asvec2_t *other, asvec2_t *self ) {
	Vector2Subtract( self->v, other->v, self->v );
	return self;
}

static asvec2_t *objectVec2_MulAssignBehaviour( asvec2_t *other, asvec2_t *self ) {
	vec_t product = DotProduct( self->v, other->v );

	Vector2Scale( self->v, product, self->v );
	return self;
}

static asvec2_t *objectVec2_MulAssignBehaviourI( int other, asvec2_t *self ) {
	Vector2Scale( self->v, other, self->v );
	return self;
}

static asvec2_t *objectVec2_MulAssignBehaviourD( float other, asvec2_t *self ) {
	Vector2Scale( self->v, other, self->v );
	return self;
}

static asvec2_t objectVec2_AddBehaviour( asvec2_t *first, asvec2_t *second ) {
	asvec2_t vec;
	Vector2Add( first->v, second->v, vec.v );
	return vec;
}

static asvec2_t objectVec2_SubtractBehaviour( asvec2_t *first, asvec2_t *second ) {
	asvec2_t vec;
	Vector2Subtract( first->v, second->v, vec.v );
	return vec;
}

static float objectVec2_MultiplyBehaviour( asvec2_t *first, asvec2_t *second ) {
	return DotProduct( first->v, second->v );
}

static asvec2_t objectVec2_MultiplyBehaviourVD( asvec2_t *first, float second ) {
	asvec2_t vec;

	Vector2Scale( first->v, second, vec.v );
	return vec;
}

static asvec2_t objectVec2_MultiplyBehaviourDV( float first, asvec2_t *second ) {
	return objectVec2_MultiplyBehaviourVD( second, first );
}

static asvec2_t objectVec2_MultiplyBehaviourVI( asvec2_t *first, int second ) {
	asvec2_t vec;
	Vector2Scale( first->v, second, vec.v );
	return vec;
}

static asvec2_t objectVec2_MultiplyBehaviourIV( int first, asvec2_t *second ) {
	return objectVec2_MultiplyBehaviourVI( second, first );
}

static bool objectVec2_EqualBehaviour( asvec2_t *first, asvec2_t *second ) {
	return Vector2Compare( first->v, second->v );
}

static void objectVec2_Clear( asvec2_t *vec ) {
	Vector2Clear( vec->v );
}

static void objectVec2_Set( float x, float y, float z, float w, asvec2_t *vec ) {
	Vector2Set( vec->v, x, y );
}

static float objectVec2_Length( const asvec2_t *vec ) {
	return Vector2Length( vec->v );
}

static float objectVec2_Normalize( asvec2_t *vec ) {
	return Vector2Normalize( vec->v );
}

static float objectVec2_Distance( asvec2_t *other, asvec2_t *self ) {
	vec2_t d;
	Vector2Subtract( other->v, self->v, d );
	return Vector2Length( d );
}

static float *objectVec2_Index( unsigned index, asvec2_t *self ) {
	if( index > 3 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Index out of bounds" );
		}
		return NULL;
	}
	return &self->v[index];
}

static CScriptArrayInterface *objectVec2_VecToArray( unsigned index, asvec2_t *self )
{
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	asITypeInfo *ot = engine->GetTypeInfoById( engine->GetTypeIdByDecl( "array<float>" ) );
	CScriptArrayInterface *arr = qasCreateArrayCpp( 2, ot );

	for( int i = 0; i < 2; i++ ) {
		*( (float *)arr->At( i ) ) = self->v[i];
	}

	return arr;
}

// same as vtos
static asstring_t *objectVec2_VecToString( asvec2_t *self )
{
	char s[64];
	int len = Q_snprintfz( s, 32, "(%+6.3f %+6.3f)", self->v[0], self->v[1] );
	return objectString_FactoryBuffer( s, len );
}

void PreRegisterVec2Addon( asIScriptEngine *engine ) {
	int r;

	// register the vector type
	r = engine->RegisterObjectType( "Vec2", sizeof( asvec2_t ), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}

void RegisterVec2Addon( asIScriptEngine *engine ) {
	int r;

	// register object behaviours
	r = engine->RegisterObjectBehaviour( "Vec2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( objectVec2_DefaultConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec2", asBEHAVE_CONSTRUCT, "void f(float x, float y)", asFUNCTION( objectVec2_Constructor3F ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec2", asBEHAVE_CONSTRUCT, "void f(const Vec2 &in)", asFUNCTION( objectVec2_CopyConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec2", asBEHAVE_CONSTRUCT, "void f(const array<float> &)",
		asFUNCTION( objectVec2_ConstructorArray ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );

	// register object methods

	// assignments
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 &opAssign(Vec2 &in)", asFUNCTION( objectVec2_AssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 &opAddAssign(Vec2 &in)", asFUNCTION( objectVec2_AddAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 &opSubAssign(Vec2 &in)", asFUNCTION( objectVec2_SubAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 &opMulAssign(Vec2 &in)", asFUNCTION( objectVec2_MulAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 &opMulAssign(int)", asFUNCTION( objectVec2_MulAssignBehaviourI ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 &opMulAssign(float)", asFUNCTION( objectVec2_MulAssignBehaviourD ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec2", "Vec2 opAdd(Vec2 &in) const", asFUNCTION( objectVec2_AddBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 opSub(Vec2 &in) const", asFUNCTION( objectVec2_SubtractBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "float opMul(Vec2 &in) const", asFUNCTION( objectVec2_MultiplyBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 opMul(float) const", asFUNCTION( objectVec2_MultiplyBehaviourVD ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 opMul_r(float) const", asFUNCTION( objectVec2_MultiplyBehaviourDV ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 opMul(int) const", asFUNCTION( objectVec2_MultiplyBehaviourVI ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "Vec2 opMul_r(int) const", asFUNCTION( objectVec2_MultiplyBehaviourIV ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	// == !=
	r = engine->RegisterObjectMethod( "Vec2", "bool opEquals(const Vec2 &in) const", asFUNCTION( objectVec2_EqualBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec2", "void clear()", asFUNCTION( objectVec2_Clear ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "void set(float x, float y)", asFUNCTION( objectVec2_Set ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "float length() const", asFUNCTION( objectVec2_Length ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "float normalize()", asFUNCTION( objectVec2_Normalize ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "float distance(const Vec2 &in) const", asFUNCTION( objectVec2_Distance ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec2", "float &opIndex(uint)", asFUNCTION( objectVec2_Index ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec2", "const float &opIndex(uint) const", asFUNCTION( objectVec2_Index ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod(
		"Vec2", "array<float> @toArray() const", asFUNCTION( objectVec2_VecToArray ), asCALL_CDECL_OBJLAST );
	r = engine->RegisterObjectMethod( "Vec2", "String @toString() const", asFUNCTION( objectVec2_VecToString ), asCALL_CDECL_OBJLAST );

	// properties
	r = engine->RegisterObjectProperty( "Vec2", "float x", asOFFSET( asvec2_t, v[0] ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Vec2", "float y", asOFFSET( asvec2_t, v[1] ) ); assert( r >= 0 );

	asITypeInfo *type = engine->GetTypeInfoByName( "Vec2" );
	type->SetUserData( &objectVec2_VecToString, 33 );

	(void)sizeof( r ); // hush the compiler
}
