/*
Copyright (C) 2020 Victor Luchits

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
#include "addon_scriptarray.h"
#include "addon_vec3.h"
#include "addon_mat3.h"

// CLASS: Mat3
void objectMat3_DefaultConstructor( asmat3_t *self )
{
	Matrix3_Identity( self->m );
}

void objectMat3_Constructor3V( asvec3_t *x, asvec3_t *y, asvec3_t *z, asmat3_t *self )
{
	VectorCopy( x->v, &self->m[0] );
	VectorCopy( y->v, &self->m[3] );
	VectorCopy( z->v, &self->m[6] );
}

void objectMat3_CopyConstructor( asmat3_t *other, asmat3_t *self )
{
	Matrix3_Copy( self->m, other->m );
}

void objectMat3_ConstructorArray( CScriptArrayInterface &arr, asmat3_t *self )
{
	if( arr.GetSize() != 9 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Invalid array size" );
		}
		return;
	}

	for( unsigned int i = 0; i < 9; i++ ) {
		self->m[i] = *( (float *)arr.At( i ) );
	}
}

static asmat3_t *objectMat3_AssignBehaviour( asmat3_t *other, asmat3_t *self )
{
	Matrix3_Copy( other->m, self->m );
	return self;
}

static asmat3_t objectMat3_MulBehaviour( asvec3_t *first, asvec3_t *second )
{
	asmat3_t res;
	Matrix3_Multiply( first->v, second->v, res.m );
	return res;
}

static asvec3_t objectMat3_MulBehaviourV( asvec3_t *v, asmat3_t *mat )
{
	asvec3_t res;
	Matrix3_TransformVector( mat->m, v->v, res.v );
	return res;
}

static bool objectMat3_EqualBehaviour( asmat3_t *first, asmat3_t *second )
{
	return Matrix3_Compare( first->m, second->m );
}

static void objectMat3_Normalize( asmat3_t *self )
{
	Matrix3_Normalize( self->m );
}

static void objectMat3_Transpose( asmat3_t *self )
{
	mat3_t tmp;
	Matrix3_Copy( self->m, tmp );
	Matrix3_Transpose( tmp, self->m );
}

void objectMat3_Identity( asmat3_t *self )
{
	Matrix3_Identity( self->m );
}

static void objectMat3_ToVectors( asvec3_t *x, asvec3_t *y, asvec3_t *z, asmat3_t *self )
{
	VectorCopy( &self->m[0], x->v );
	VectorCopy( &self->m[3], y->v );
	VectorCopy( &self->m[6], z->v );
}

static asvec3_t objectMat3_ToAngles( asmat3_t *self )
{
	asvec3_t angles;
	Matrix3_ToAngles( self->m, angles.v );
	return angles;
}

static float *objectMat3_Index( unsigned index, asmat3_t *self )
{
	if( index > 8 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Index out of bounds" );
		}
		return NULL;
	}
	return &self->m[index];
}

static CScriptArrayInterface *objectMat3_VecToArray( unsigned index, asmat3_t *self )
{
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	asIObjectType *ot = engine->GetObjectTypeById( engine->GetTypeIdByDecl( "array<float>" ) );
	CScriptArrayInterface *arr = QAS_NEW( CScriptArray )( 9, ot );

	for( int i = 0; i < 9; i++ ) {
		*( (float *)arr->At( i ) ) = self->m[i];
	}

	return arr;
}

void PreRegisterMat3Addon( asIScriptEngine *engine )
{
	int r;

	// register the vector type
	r = engine->RegisterObjectType(
		"Mat3", sizeof( asmat3_t ), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS );
	assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}

void RegisterMat3Addon( asIScriptEngine *engine )
{
	int r;

	// register object behaviours
	r = engine->RegisterObjectBehaviour(
		"Mat3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( objectMat3_DefaultConstructor ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Mat3", asBEHAVE_CONSTRUCT,
		"void f(const Vec3 &in, const Vec3 &in, const Vec3 &in)", asFUNCTION( objectMat3_Constructor3V ),
		asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Mat3", asBEHAVE_CONSTRUCT, "void f(const Mat3 &in)",
		asFUNCTION( objectMat3_CopyConstructor ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Mat3", asBEHAVE_CONSTRUCT, "void f(const array<float> &)",
		asFUNCTION( objectMat3_ConstructorArray ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );

	// register object methods

	// assignments
	r = engine->RegisterObjectMethod(
		"Mat3", "Vec3 &opAssign(Vec3 &in)", asFUNCTION( objectMat3_AssignBehaviour ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );

	r = engine->RegisterObjectMethod(
		"Mat3", "Mat3 opMul(Mat3 &in) const", asFUNCTION( objectMat3_MulBehaviour ), asCALL_CDECL_OBJFIRST );
	assert( r >= 0 );
	r = engine->RegisterObjectMethod(
		"Mat3", "Vec3 opMul(Vec3 &in) const", asFUNCTION( objectMat3_MulBehaviourV ), asCALL_CDECL_OBJFIRST );
	assert( r >= 0 );

	// == !=
	r = engine->RegisterObjectMethod(
		"Mat3", "bool opEquals(const Mat3 &in) const", asFUNCTION( objectMat3_EqualBehaviour ), asCALL_CDECL_OBJFIRST );
	assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Mat3", "void identity()", asFUNCTION( objectMat3_Identity ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectMethod(
		"Mat3", "void normalize()", asFUNCTION( objectMat3_Normalize ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Mat3", "void transpose()", asFUNCTION( objectMat3_Transpose ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Mat3", "void toVectors(Vec3 &out, Vec3 &out, Vec3 &out) const",
		asFUNCTION( objectMat3_ToVectors ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectMethod(
		"Mat3", "Vec3 toAngles() const", asFUNCTION( objectMat3_ToAngles ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectMethod(
		"Mat3", "float &opIndex(uint)", asFUNCTION( objectMat3_Index ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );
	r = engine->RegisterObjectMethod(
		"Mat3", "const float &opIndex(uint) const", asFUNCTION( objectMat3_Index ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );

	r = engine->RegisterObjectMethod(
		"Mat3", "array<float> @toArray()", asFUNCTION( objectMat3_VecToArray ), asCALL_CDECL_OBJLAST );
	assert( r >= 0 );

	// properties
	r = engine->RegisterObjectProperty( "Mat3", "Vec3 x", asOFFSET( asmat3_t, m[0] ) );
	assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Mat3", "Vec3 y", asOFFSET( asmat3_t, m[3] ) );
	assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Mat3", "Vec3 z", asOFFSET( asmat3_t, m[6] ) );
	assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}
