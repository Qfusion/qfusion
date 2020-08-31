/*
Copyright (C) 2008 German Garcia
Copyright (C) 2011 Chasseur de bots
Copyright (C) 2017 Victor Luchits

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
#include "addon_vec4.h"
#include "addon_scriptarray.h"
#include "addon_string.h"

// CLASS: Vec4
void objectVec4_DefaultConstructor( asvec4_t *self ) {
	self->v[0] = self->v[1] = self->v[2] = self->v[3] = 0;
}

void objectVec4_Constructor3F( float x, float y, float z, float w, asvec4_t *self ) {
	self->v[0] = x;
	self->v[1] = y;
	self->v[2] = z;
	self->v[3] = w;
}

void objectVec4_CopyConstructor( asvec4_t *other, asvec4_t *self ) {
	self->v[0] = other->v[0];
	self->v[1] = other->v[1];
	self->v[2] = other->v[2];
	self->v[3] = other->v[3];
}

void objectVec4_ConstructorArray( CScriptArrayInterface &arr, asvec4_t *self )
{
	if( arr.GetSize() != 4 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Invalid array size" );
		}
		return;
	}

	for( unsigned int i = 0; i < 4; i++ ) {
		self->v[i] = *( (float *)arr.At( i ) );
	}
}

static asvec4_t *objectVec4_AssignBehaviour( asvec4_t *other, asvec4_t *self ) {
	Vector4Copy( other->v, self->v );
	return self;
}

static asvec4_t *objectVec4_AddAssignBehaviour( asvec4_t *other, asvec4_t *self ) {
	Vector4Add( self->v, other->v, self->v );
	return self;
}

static asvec4_t *objectVec4_SubAssignBehaviour( asvec4_t *other, asvec4_t *self ) {
	Vector4Subtract( self->v, other->v, self->v );
	return self;
}

static asvec4_t *objectVec4_MulAssignBehaviour( asvec4_t *other, asvec4_t *self ) {
	vec_t product = DotProduct( self->v, other->v );

	Vector4Scale( self->v, product, self->v );
	return self;
}

static asvec4_t *objectVec4_MulAssignBehaviourI( int other, asvec4_t *self ) {
	Vector4Scale( self->v, other, self->v );
	return self;
}

static asvec4_t *objectVec4_MulAssignBehaviourD( float other, asvec4_t *self ) {
	Vector4Scale( self->v, other, self->v );
	return self;
}

static asvec4_t objectVec4_AddBehaviour( asvec4_t *first, asvec4_t *second ) {
	asvec4_t vec;
	Vector4Add( first->v, second->v, vec.v );
	return vec;
}

static asvec4_t objectVec4_SubtractBehaviour( asvec4_t *first, asvec4_t *second ) {
	asvec4_t vec;
	Vector4Subtract( first->v, second->v, vec.v );
	return vec;
}

static float objectVec4_MultiplyBehaviour( asvec4_t *first, asvec4_t *second ) {
	return DotProduct( first->v, second->v );
}

static asvec4_t objectVec4_MultiplyBehaviourVD( asvec4_t *first, float second ) {
	asvec4_t vec;

	Vector4Scale( first->v, second, vec.v );
	return vec;
}

static asvec4_t objectVec4_MultiplyBehaviourDV( float first, asvec4_t *second ) {
	return objectVec4_MultiplyBehaviourVD( second, first );
}

static asvec4_t objectVec4_MultiplyBehaviourVI( asvec4_t *first, int second ) {
	asvec4_t vec;
	Vector4Scale( first->v, second, vec.v );
	return vec;
}

static asvec4_t objectVec4_MultiplyBehaviourIV( int first, asvec4_t *second ) {
	return objectVec4_MultiplyBehaviourVI( second, first );
}

static bool objectVec4_EqualBehaviour( asvec4_t *first, asvec4_t *second ) {
	return Vector4Compare( first->v, second->v );
}

static void objectVec4_Clear( asvec4_t *vec ) {
	Vector4Clear( vec->v );
}

static void objectVec4_Set( float x, float y, float z, float w, asvec4_t *vec ) {
	Vector4Set( vec->v, x, y, z, w );
}

static float objectVec4_Length( const asvec4_t *vec ) {
	return Vector4Length( vec->v );
}

static float objectVec4_Normalize( asvec4_t *vec ) {
	return Vector4Normalize( vec->v );
}

static float objectVec4_Distance( asvec4_t *other, asvec4_t *self ) {
	vec4_t d;
	Vector4Subtract( other->v, self->v, d );
	return Vector4Length( d );
}

static asvec3_t objectVec4_XYZ( const asvec4_t *self ) {
	asvec3_t v;
	VectorCopy( self->v, v.v );
	return v;
}

static float *objectVec4_Index( unsigned index, asvec4_t *self ) {
	if( index > 3 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Index out of bounds" );
		}
		return NULL;
	}
	return &self->v[index];
}

static CScriptArrayInterface *objectVec4_VecToArray( unsigned index, asvec4_t *self )
{
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	asIObjectType *ot = engine->GetObjectTypeById( engine->GetTypeIdByDecl( "array<float>" ) );
	CScriptArrayInterface *arr = QAS_NEW( CScriptArray )( 4, ot );

	for( int i = 0; i < 4; i++ ) {
		*( (float *)arr->At( i ) ) = self->v[i];
	}

	return arr;
}

// same as vtos
static asstring_t *objectVec4_VecToString( asvec4_t *self )
{
	char s[64];
	int len = Q_snprintfz( s, 32, "(%+6.3f %+6.3f %+6.3f %+6.3f)", self->v[0], self->v[1], self->v[2], self->v[3] );
	return objectString_FactoryBuffer( s, len );
}

void PreRegisterVec4Addon( asIScriptEngine *engine ) {
	int r;

	// register the vector type
	r = engine->RegisterObjectType( "Vec4", sizeof( asvec4_t ), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}

void RegisterVec4Addon( asIScriptEngine *engine ) {
	int r;

	// register object behaviours
	r = engine->RegisterObjectBehaviour( "Vec4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( objectVec4_DefaultConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec4", asBEHAVE_CONSTRUCT, "void f(float x, float y, float z, float w)", asFUNCTION( objectVec4_Constructor3F ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec4", asBEHAVE_CONSTRUCT, "void f(const Vec4 &in)", asFUNCTION( objectVec4_CopyConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec4", asBEHAVE_CONSTRUCT, "void f(const array<float> &)",
		asFUNCTION( objectVec4_ConstructorArray ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );

	// register object methods

	// assignments
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 &opAssign(Vec4 &in)", asFUNCTION( objectVec4_AssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 &opAddAssign(Vec4 &in)", asFUNCTION( objectVec4_AddAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 &opSubAssign(Vec4 &in)", asFUNCTION( objectVec4_SubAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 &opMulAssign(Vec4 &in)", asFUNCTION( objectVec4_MulAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 &opMulAssign(int)", asFUNCTION( objectVec4_MulAssignBehaviourI ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 &opMulAssign(float)", asFUNCTION( objectVec4_MulAssignBehaviourD ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec4", "Vec4 opAdd(Vec4 &in) const", asFUNCTION( objectVec4_AddBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 opSub(Vec4 &in) const", asFUNCTION( objectVec4_SubtractBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "float opMul(Vec4 &in) const", asFUNCTION( objectVec4_MultiplyBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 opMul(float) const", asFUNCTION( objectVec4_MultiplyBehaviourVD ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 opMul_r(float) const", asFUNCTION( objectVec4_MultiplyBehaviourDV ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 opMul(int) const", asFUNCTION( objectVec4_MultiplyBehaviourVI ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "Vec4 opMul_r(int) const", asFUNCTION( objectVec4_MultiplyBehaviourIV ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	// == !=
	r = engine->RegisterObjectMethod( "Vec4", "bool opEquals(const Vec4 &in) const", asFUNCTION( objectVec4_EqualBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec4", "void clear()", asFUNCTION( objectVec4_Clear ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "void set(float x, float y, float z, float w)", asFUNCTION( objectVec4_Set ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "float length() const", asFUNCTION( objectVec4_Length ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "float normalize()", asFUNCTION( objectVec4_Normalize ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "float distance(const Vec4 &in) const", asFUNCTION( objectVec4_Distance ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec4", "Vec3 xyz() const", asFUNCTION( objectVec4_XYZ ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "float &opIndex(uint)", asFUNCTION( objectVec4_Index ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec4", "const float &opIndex(uint) const", asFUNCTION( objectVec4_Index ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod(
		"Vec4", "array<float> @toArray() const", asFUNCTION( objectVec4_VecToArray ), asCALL_CDECL_OBJLAST );
	r = engine->RegisterObjectMethod( "Vec4", "String @toString() const", asFUNCTION( objectVec4_VecToString ), asCALL_CDECL_OBJLAST );

	// properties
	r = engine->RegisterObjectProperty( "Vec4", "float x", asOFFSET( asvec4_t, v[0] ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Vec4", "float y", asOFFSET( asvec4_t, v[1] ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Vec4", "float z", asOFFSET( asvec4_t, v[2] ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Vec4", "float w", asOFFSET( asvec4_t, v[3] ) ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}
