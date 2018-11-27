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
#include "qalgo/rng.h"

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

static double asFunc_ceil( double x ) {
	return ceil( x );
}

static double asFunc_floor( double x ) {
	return floor( x );
}

static PCG pcg;

static uint32_t asFunc_random_uint() {
	return random_u32( &pcg );
}

static int asFunc_random_uniform( int lo, int hi ) {
	return random_uniform( &pcg, lo, hi );
}

static float asFunc_random_float() {
	return random_float( &pcg );
}

void PreRegisterMathAddon( asIScriptEngine *engine ) {
	pcg = new_pcg();
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
		{ "double floor( double x )", asFUNCTION( asFunc_floor ) },
		{ "uint random_uint()", asFUNCTION( asFunc_random_uint ) },
		{ "int random_uniform( int lo, int hi )", asFUNCTION( asFunc_random_uniform ) },
		{ "float random_float()", asFUNCTION( asFunc_random_float ) },

		{ NULL, asFUNCTION( 0 ) }
	}, *func;
	int r = 0;

	for( func = math_asGlobFuncs; func->declaration; func++ ) {
		r = engine->RegisterGlobalFunction( func->declaration, func->ptr, asCALL_CDECL ); assert( r >= 0 );
	}

	(void)sizeof( r ); // hush the compiler
}
