/*
Copyright (C) 2008 German Garcia

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
#include "addon_math.h"

/*************************************
* MATHS ADDON
**************************************/

static int asFunc_abs( int x ) {
	return abs( x );
}

static double asFunc_fabs( double x ) {
	return fabs( x );
}

static double asFunc_log( double x ) {
	return log( x );
}

static double asFunc_pow( double x, double y ) {
	return pow( x, y );
}

static double asFunc_cos( double x ) {
	return cos( x );
}

static double asFunc_sin( double x ) {
	return sin( x );
}

static double asFunc_tan( double x ) {
	return tan( x );
}

static double asFunc_acos( double x ) {
	return acos( x );
}

static double asFunc_asin( double x ) {
	return asin( x );
}

static double asFunc_atan( double x ) {
	return atan( x );
}

static double asFunc_atan2( double x, double y ) {
	return atan2( x, y );
}

static double asFunc_sqrt( double x ) {
	return sqrt( x );
}

static double asFunc_fmax( double x, double y ) {
	return x > y ? x : y;
}

static double asFunc_fmin( double x, double y ) {
	return x < y ? x : y;
}

static int64_t asFunc_imax( int64_t x, int64_t y ) {
	return x > y ? x : y;
}

static int64_t asFunc_imin( int64_t x, int64_t y ) {
	return x < y ? x : y;
}

static uint64_t asFunc_umax( uint64_t x, uint64_t y ) {
	return x > y ? x : y;
}

static uint64_t asFunc_umin( uint64_t x, uint64_t y ) {
	return x < y ? x : y;
}

static double asFunc_ceil( double x ) {
	return ceil( x );
}

static double asFunc_floor( double x ) {
	return floor( x );
}

static double asFunc_random( void ) {
	return random();
}

static double asFunc_brandom( double min, double max ) {
	return brandom( min, max );
}

static int asFunc_rand( void ) {
	return rand();
}

static double asFunc_deg2rad( double deg ) {
	return DEG2RAD( deg );
}

static double asFunc_rad2deg( double rad ) {
	return RAD2DEG( rad );
}

static asvec3_t asFunc_RotatePointAroundVector( const asvec3_t *dir, const asvec3_t *point, float degrees ) {
	asvec3_t dst;
	RotatePointAroundVector( dst.v, dir->v, point->v, degrees );
	return dst;
}

static asvec3_t asFunc_AnglesSubtract( asvec3_t *a1, asvec3_t *a2 ) {
	asvec3_t dst;
	AnglesSubtract( a1->v, a2->v, dst.v );
	return dst;
}

static asvec3_t asFunc_LerpAngles( asvec3_t *a1, asvec3_t *a2, float f )
{
	asvec3_t dst;
	for( int i = 0; i < 3; i++ )
		dst.v[i] = LerpAngle( a1->v[i], a2->v[i], f );
	return dst;
}

void PreRegisterMathAddon( asIScriptEngine *engine ) {
}

void RegisterMathAddon( asIScriptEngine *engine ) {
	const struct
	{
		const char *declaration;
		asSFuncPtr ptr;
	}
	math_asGlobFuncs[] =
	{
		{ "int abs( int x )", asFUNCTION( asFunc_abs ) },
		{ "double abs( double x )", asFUNCTION( asFunc_fabs ) },
		{ "double log( double x )", asFUNCTION( asFunc_log ) },
		{ "double pow( double x, double y )", asFUNCTION( asFunc_pow ) },
		{ "double cos( double x )", asFUNCTION( asFunc_cos ) },
		{ "double sin( double x )", asFUNCTION( asFunc_sin ) },
		{ "double tan( double x )", asFUNCTION( asFunc_tan ) },
		{ "double acos( double x )", asFUNCTION( asFunc_acos ) },
		{ "double asin( double x )", asFUNCTION( asFunc_asin ) },
		{ "double atan( double x )", asFUNCTION( asFunc_atan ) },
		{ "double atan2( double x, double y )", asFUNCTION( asFunc_atan2 ) },
		{ "double sqrt( double x )", asFUNCTION( asFunc_sqrt ) },
		{ "double ceil( double x )", asFUNCTION( asFunc_ceil ) },
		{ "double max( double x, double y )", asFUNCTION( asFunc_fmax ) },
		{ "double min( double x, double y )", asFUNCTION( asFunc_fmin ) },
		{ "double max( int64 x, int64 y )", asFUNCTION( asFunc_imax ) },
		{ "double min( int64 x, int64 y )", asFUNCTION( asFunc_imin ) },
		{ "double max( uint64 x, uint64 y )", asFUNCTION( asFunc_umax ) },
		{ "double min( uint64 x, uint64 y )", asFUNCTION( asFunc_umin ) },
		{ "double floor( double x )", asFUNCTION( asFunc_floor ) },
		{ "double random()", asFUNCTION( asFunc_random ) },
		{ "double brandom( double min, double max )", asFUNCTION( asFunc_brandom ) },
		{ "double deg2rad( double deg )", asFUNCTION( asFunc_deg2rad ) },
		{ "double rad2deg( double rad )", asFUNCTION( asFunc_rad2deg ) },
		{ "int rand()", asFUNCTION( asFunc_rand ) },
		{ "Vec3 RotatePointAroundVector( const Vec3 &in dir, const Vec3 &in point, float degrees )", asFUNCTION( asFunc_RotatePointAroundVector ) },
		{ "float AngleSubtract( float v1, float v2 )", asFUNCTION( AngleSubtract ) },
		{ "Vec3 AnglesSubtract( const Vec3 &in a1, const Vec3 &in a2 )", asFUNCTION( asFunc_AnglesSubtract ) },
		{ "float AngleNormalize360( float a )", asFUNCTION( AngleNormalize360 ) },
		{ "float AngleNormalize180( float a )", asFUNCTION( AngleNormalize180 ) },
		{ "float AngleDelta( float a )", asFUNCTION( AngleDelta ) },
		{ "float anglemod( float a )", asFUNCTION( anglemod ) },
		{ "float LerpAngle( float v1, float v2, float lerp )", asFUNCTION( LerpAngle ) },
		{ "Vec3 LerpAngles( const Vec3 &in a1, const Vec3 &in a2, float f )", asFUNCTION( asFunc_LerpAngles ) },

		{ NULL, asFUNCTION( 0 ) }
	}, *func;
	int r = 0;

	for( func = math_asGlobFuncs; func->declaration; func++ ) {
		r = engine->RegisterGlobalFunction( func->declaration, func->ptr, asCALL_CDECL ); assert( r >= 0 );
	}

	(void)sizeof( r ); // hush the compiler
}
