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

static int asFunc_abs( int x )
{
	return abs( x );
}

static float asFunc_fabs( float x )
{
	return fabs( x );
}

static float asFunc_log( float x )
{
	return (float)log( x );
}

static float asFunc_pow( float x, float y )
{
	return (float)pow( x, y );
}

static float asFunc_cos( float x )
{
	return (float)cos( x );
}

static float asFunc_sin( float x )
{
	return (float)sin( x );
}

static float asFunc_tan( float x )
{
	return (float)tan( x );
}

static float asFunc_acos( float x )
{
	return (float)acos( x );
}

static float asFunc_asin( float x )
{
	return (float)asin( x );
}

static float asFunc_atan( float x )
{
	return (float)atan( x );
}

static float asFunc_atan2( float x, float y )
{
	return (float)atan2( x, y );
}

static float asFunc_sqrt( float x )
{
	return (float)sqrt( x );
}

static float asFunc_ceil( float x )
{
	return (float)ceil( x );
}

static float asFunc_floor( float x )
{
	return (float)floor( x );
}

void PreRegisterMathAddon( asIScriptEngine *engine )
{
}

void RegisterMathAddon( asIScriptEngine *engine )
{
	const struct
	{
		const char *declaration;
		void *pointer;
	}
	math_asGlobFuncs[] =
	{
		{ "int abs( int x )", (void *)asFunc_abs },
		{ "float abs( float x )", (void *)asFunc_fabs },
		{ "float log( float x )", (void *)asFunc_log },
		{ "float pow( float x, float y )", (void *)asFunc_pow },
		{ "float cos( float x )", (void *)asFunc_cos },
		{ "float sin( float x )", (void *)asFunc_sin },
		{ "float tan( float x )", (void *)asFunc_tan },
		{ "float acos( float x )", (void *)asFunc_acos },
		{ "float asin( float x )", (void *)asFunc_asin },
		{ "float atan( float x )", (void *)asFunc_atan },
		{ "float atan2( float x, float y )", (void *)asFunc_atan2 },
		{ "float sqrt( float x )", (void *)asFunc_sqrt },
		{ "float ceil( float x )", (void *)asFunc_ceil },
		{ "float floor( float x )", (void *)asFunc_floor },

		{ NULL, NULL }
	}, *func;
	int r;

	for( func = math_asGlobFuncs; func->declaration; func++ ) {
		r = engine->RegisterGlobalFunction( func->declaration, asFUNCTION( func->pointer ), asCALL_CDECL ); assert( r >= 0 );
	}
}
