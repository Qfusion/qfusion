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
#include "addon_vec3.h"
#include "addon_mat3.h"
#include "addon_scriptarray.h"
#include "addon_string.h"

// CLASS: Vec3
void objectVec3_DefaultConstructor( asvec3_t *self ) {
	self->v[0] = self->v[1] = self->v[2] = 0;
}

void objectVec3_Constructor3F( float x, float y, float z, asvec3_t *self ) {
	self->v[0] = x;
	self->v[1] = y;
	self->v[2] = z;
}

void objectVec3_CopyConstructor( asvec3_t *other, asvec3_t *self ) {
	self->v[0] = other->v[0];
	self->v[1] = other->v[1];
	self->v[2] = other->v[2];
}

void objectVec3_ConstructorArray( CScriptArrayInterface &arr, asvec3_t *self )
{
	if( arr.GetSize() != 3 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Invalid array size" );
		}
		return;
	}

	for( unsigned int i = 0; i < 3; i++ ) {
		self->v[i] = *( (float *)arr.At( i ) );
	}
}

static asvec3_t *objectVec3_AssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	VectorCopy( other->v, self->v );
	return self;
}

static asvec3_t *objectVec3_AddAssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	VectorAdd( self->v, other->v, self->v );
	return self;
}

static asvec3_t *objectVec3_SubAssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	VectorSubtract( self->v, other->v, self->v );
	return self;
}

static asvec3_t *objectVec3_MulAssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	vec_t product = DotProduct( self->v, other->v );

	VectorScale( self->v, product, self->v );
	return self;
}

static asvec3_t *objectVec3_XORAssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	vec3_t product;

	CrossProduct( self->v, other->v, product );
	VectorCopy( product, self->v );
	return self;
}

static asvec3_t *objectVec3_MulAssignBehaviourI( int other, asvec3_t *self ) {
	VectorScale( self->v, other, self->v );
	return self;
}

static asvec3_t *objectVec3_MulAssignBehaviourD( float other, asvec3_t *self ) {
	VectorScale( self->v, other, self->v );
	return self;
}

static asvec3_t objectVec3_AddBehaviour( asvec3_t *first, asvec3_t *second ) {
	asvec3_t vec;

	VectorAdd( first->v, second->v, vec.v );
	return vec;
}

static asvec3_t objectVec3_SubtractBehaviour( asvec3_t *first, asvec3_t *second ) {
	asvec3_t vec;

	VectorSubtract( first->v, second->v, vec.v );
	return vec;
}

static float objectVec3_MultiplyBehaviour( asvec3_t *first, asvec3_t *second ) {
	return DotProduct( first->v, second->v );
}

static asvec3_t objectVec3_MultiplyBehaviourVD( asvec3_t *first, float second ) {
	asvec3_t vec;

	VectorScale( first->v, second, vec.v );
	return vec;
}

static asvec3_t objectVec3_MultiplyBehaviourDV( float first, asvec3_t *second ) {
	return objectVec3_MultiplyBehaviourVD( second, first );
}

static asvec3_t objectVec3_MultiplyBehaviourVI( asvec3_t *first, int second ) {
	asvec3_t vec;

	VectorScale( first->v, second, vec.v );
	return vec;
}

static asvec3_t objectVec3_MultiplyBehaviourIV( int first, asvec3_t *second ) {
	return objectVec3_MultiplyBehaviourVI( second, first );
}

static asvec3_t objectVec3_XORBehaviour( asvec3_t *first, asvec3_t *second ) {
	asvec3_t vec;

	CrossProduct( first->v, second->v, vec.v );
	return vec;
}

static bool objectVec3_EqualBehaviour( asvec3_t *first, asvec3_t *second ) {
	return VectorCompare( first->v, second->v );
}

static void objectVec3_Clear( asvec3_t *vec ) {
	VectorClear( vec->v );
}

static void objectVec3_Set( float x, float y, float z, asvec3_t *vec ) {
	VectorSet( vec->v, x, y, z );
}

static float objectVec3_Length( const asvec3_t *vec ) {
	return VectorLength( vec->v );
}

static float objectVec3_LengthSquared( const asvec3_t *vec ) {
	return DotProduct( vec->v, vec->v );
}

static float objectVec3_Normalize( asvec3_t *vec ) {
	return VectorNormalize( vec->v );
}

static float objectVec3_Distance( asvec3_t *other, asvec3_t *self ) {
	return Distance( self->v, other->v );
}

static void objectVec3_AngleVectors( asvec3_t *f, asvec3_t *r, asvec3_t *u, asvec3_t *self ) {
	AngleVectors( self->v, f->v, r->v, u->v );
}

static asvec3_t objectVec3_VecToAngles( asvec3_t *self ) {
	asvec3_t angles;

	VecToAngles( self->v, angles.v );
	return angles;
}

static asvec3_t objectVec3_Perpendicular( asvec3_t *self ) {
	asvec3_t dst;

	PerpendicularVector( dst.v, self->v );
	return dst;
}

static void objectVec3_MakeNormalVectors( asvec3_t *r, asvec3_t *u, asvec3_t *self ) {
	MakeNormalVectors( self->v, r->v, u->v );
}

static void objectVec3_AnglesToMatrix3( asmat3_t *res, asvec3_t *self )
{
	Matrix3_FromAngles( self->v, res->m );
}

static void objectVec3_AnglesToAxis( asmat3_t *res, asvec3_t *self )
{
	AnglesToAxis( self->v, res->m );
}

static float *objectVec3_Index( unsigned index, asvec3_t *self ) {
	if( index > 2 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Index out of bounds" );
		}
		return NULL;
	}
	return &self->v[index];
}

static CScriptArrayInterface *objectVec3_VecToArray( unsigned index, asvec3_t *self )
{
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	asIObjectType *ot = engine->GetObjectTypeById( engine->GetTypeIdByDecl( "array<float>" ) );
	CScriptArrayInterface *arr = QAS_NEW( CScriptArray )( 3, ot );

	for( int i = 0; i < 3; i++ ){
		*( (float *)arr->At( i ) ) = self->v[i];
	}

	return arr;
}

// same as vtos
static asstring_t *objectVec3_VecToString( asvec3_t *self )
{
	char s[32];
	int len = Q_snprintfz( s, 32, "(%+6.3f %+6.3f %+6.3f)", self->v[0], self->v[1], self->v[2] );
	return objectString_FactoryBuffer( s, len );
}

void PreRegisterVec3Addon( asIScriptEngine *engine ) {
	int r;

	// register the vector type
	r = engine->RegisterObjectType( "Vec3", sizeof( asvec3_t ), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}

void RegisterVec3Addon( asIScriptEngine *engine ) {
	int r;

	// register object behaviours
	r = engine->RegisterObjectBehaviour( "Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( objectVec3_DefaultConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec3", asBEHAVE_CONSTRUCT, "void f(float x, float y, float z)", asFUNCTION( objectVec3_Constructor3F ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)", asFUNCTION( objectVec3_CopyConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec3", asBEHAVE_CONSTRUCT, "void f(const array<float> &)", asFUNCTION( objectVec3_ConstructorArray ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	// register object methods

	// assignments
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opAssign(Vec3 &in)", asFUNCTION( objectVec3_AssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opAddAssign(Vec3 &in)", asFUNCTION( objectVec3_AddAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opSubAssign(Vec3 &in)", asFUNCTION( objectVec3_SubAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opMulAssign(Vec3 &in)", asFUNCTION( objectVec3_MulAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opXorAssign(Vec3 &in)", asFUNCTION( objectVec3_XORAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opMulAssign(int)", asFUNCTION( objectVec3_MulAssignBehaviourI ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opMulAssign(float)", asFUNCTION( objectVec3_MulAssignBehaviourD ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opAdd(Vec3 &in) const", asFUNCTION( objectVec3_AddBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opSub(Vec3 &in) const", asFUNCTION( objectVec3_SubtractBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float opMul(Vec3 &in) const", asFUNCTION( objectVec3_MultiplyBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opMul(float) const", asFUNCTION( objectVec3_MultiplyBehaviourVD ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opMul_r(float) const", asFUNCTION( objectVec3_MultiplyBehaviourDV ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opMul(int) const", asFUNCTION( objectVec3_MultiplyBehaviourVI ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opMul_r(int) const", asFUNCTION( objectVec3_MultiplyBehaviourIV ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opXor(const Vec3 &in) const", asFUNCTION( objectVec3_XORBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );

	// == !=
	r = engine->RegisterObjectMethod( "Vec3", "bool opEquals(const Vec3 &in) const", asFUNCTION( objectVec3_EqualBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec3", "void clear()", asFUNCTION( objectVec3_Clear ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "void set(float x, float y, float z)", asFUNCTION( objectVec3_Set ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float length() const", asFUNCTION( objectVec3_Length ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float lengthSquared() const", asFUNCTION( objectVec3_LengthSquared ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float normalize()", asFUNCTION( objectVec3_Normalize ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float distance(const Vec3 &in) const", asFUNCTION( objectVec3_Distance ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "void angleVectors(Vec3 &out, Vec3 &out, Vec3 &out) const", asFUNCTION( objectVec3_AngleVectors ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 toAngles() const", asFUNCTION( objectVec3_VecToAngles ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 perpendicular() const", asFUNCTION( objectVec3_Perpendicular ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "void makeNormalVectors(Vec3 &out, Vec3 &out) const", asFUNCTION( objectVec3_MakeNormalVectors ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "void anglesToMarix(Mat3 &out) const", asFUNCTION( objectVec3_AnglesToMatrix3 ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "void anglesToAxis(Mat3 &out) const", asFUNCTION( objectVec3_AnglesToAxis ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float &opIndex(uint)", asFUNCTION( objectVec3_Index ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "const float &opIndex(uint) const", asFUNCTION( objectVec3_Index ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec3", "array<float> @toArray() const", asFUNCTION( objectVec3_VecToArray ), asCALL_CDECL_OBJLAST );
	r = engine->RegisterObjectMethod( "Vec3", "String @toString() const", asFUNCTION( objectVec3_VecToString ), asCALL_CDECL_OBJLAST );

	assert( r >= 0 );

	// properties
	r = engine->RegisterObjectProperty( "Vec3", "float x", asOFFSET( asvec3_t, v[0] ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Vec3", "float y", asOFFSET( asvec3_t, v[1] ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Vec3", "float z", asOFFSET( asvec3_t, v[2] ) ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}
