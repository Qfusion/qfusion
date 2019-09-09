/*
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

#include "cg_as_local.h"

//=======================================================================

static const gs_asEnumVal_t asTouchpadEnumVals[] =
{
	ASLIB_ENUM_VAL( TOUCHPAD_MOVE ),
	ASLIB_ENUM_VAL( TOUCHPAD_VIEW ),
	ASLIB_ENUM_VAL( TOUCHPAD_COUNT ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asTouchareaEnumVals[] =
{
	ASLIB_ENUM_VAL( TOUCHAREA_NONE ),
	ASLIB_ENUM_VAL( TOUCHAREA_HUD ),
	ASLIB_ENUM_VAL( TOUCHAREA_SUB_SHIFT ),
	ASLIB_ENUM_VAL( TOUCHAREA_MASK ),

	ASLIB_ENUM_VAL_NULL
};

const gs_asEnum_t asCGameInputEnums[] =
{
	{ "cg_touchpad_e", asTouchpadEnumVals },
	{ "cg_toucharea_e", asTouchareaEnumVals },

	ASLIB_ENUM_VAL_NULL
};

//=======================================================================

static const gs_asFuncdef_t astouch_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t astouch_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const gs_asMethod_t astouch_Methods[] =
{
	ASLIB_METHOD_NULL
};

static const gs_asProperty_t astouch_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( const bool, down ), ASLIB_FOFFSET( cg_touch_t, down ) },
	{ ASLIB_PROPERTY_DECL( const int, x ), ASLIB_FOFFSET( cg_touch_t, x ) },
	{ ASLIB_PROPERTY_DECL( const int, y ), ASLIB_FOFFSET( cg_touch_t, y ) },
	{ ASLIB_PROPERTY_DECL( const int64, time ), ASLIB_FOFFSET( cg_touch_t, time ) },
	{ ASLIB_PROPERTY_DECL( const int, area ), ASLIB_FOFFSET( cg_touch_t, area ) },
	{ ASLIB_PROPERTY_DECL( const bool, areaValid ), ASLIB_FOFFSET( cg_touch_t, area_valid ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asTouchClassDescriptor =
{
	"Touch",                    /* name */
	asOBJ_REF | asOBJ_NOCOUNT,  /* object type flags */
	sizeof( cg_touch_t ),       /* size */
	astouch_Funcdefs,           /* funcdefs */
	astouch_ObjectBehaviors,    /* object behaviors */
	astouch_Methods,            /* methods */
	astouch_Properties,         /* properties */
	NULL, NULL                  /* string factory hack */
};

//=======================================================================

static const gs_asFuncdef_t astouchpad_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t astouchpad_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const gs_asMethod_t astouchpad_Methods[] =
{
	ASLIB_METHOD_NULL
};

static const gs_asProperty_t astouchpad_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( int, x ), ASLIB_FOFFSET( cg_touchpad_t, x ) },
	{ ASLIB_PROPERTY_DECL( int, y ), ASLIB_FOFFSET( cg_touch_t, y ) },
	{ ASLIB_PROPERTY_DECL( const int, touch ), ASLIB_FOFFSET( cg_touchpad_t, touch ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asTouchpadClassDescriptor =
{
	"Touchpad",                 /* name */
	asOBJ_REF | asOBJ_NOCOUNT,  /* object type flags */
	sizeof( cg_touchpad_t ),    /* size */
	astouchpad_Funcdefs,        /* funcdefs */
	astouchpad_ObjectBehaviors, /* object behaviors */
	astouchpad_Methods,         /* methods */
	astouchpad_Properties,      /* properties */
	NULL, NULL                  /* string factory hack */
};

//=======================================================================

static asvec4_t CG_asInputGetThumbsticks( void ) {
	asvec4_t sticks;
	trap_IN_GetThumbsticks( sticks.v );
	return sticks;
}

const gs_asClassDescriptor_t * const asCGameInputClassesDescriptors[] =
{
	&asTouchClassDescriptor,
	&asTouchpadClassDescriptor,

	NULL
};

const gs_asglobfuncs_t asCGameInputGlobalFuncs[] =
{
	{ "Touch @GetTouch( int id )", asFUNCTION( CG_GetTouch ), NULL },
	{ "Touchpad @GetTouchpad( int id )", asFUNCTION( CG_GetTouchpad ), NULL },
	{ "Vec4 GetThumbsticks()", asFUNCTION( CG_asInputGetThumbsticks ), NULL },
	{ "float GetSensitivityScale( float sens, float zoomSens )", asFUNCTION( CG_GetSensitivityScale ), NULL },

	{ NULL }
};

//======================================================================

/*
* CG_asInputInit
*/
void CG_asInputInit( void ) {
	CG_asCallScriptFunc( cgs.asInput.init, cg_empty_as_cb, cg_empty_as_cb );
}

/*
* CG_asInputShutdown
*/
void CG_asInputShutdown( void ) {
	CG_asCallScriptFunc( cgs.asInput.shutdown, cg_empty_as_cb, cg_empty_as_cb );
}

/*
* CG_asInputFrame
*/
void CG_asInputFrame( int64_t inputTime ) {
	CG_asCallScriptFunc( cgs.asInput.frame, [inputTime](asIScriptContext *ctx)
		{
			ctx->SetArgQWord( 0, inputTime );
		},
		cg_empty_as_cb
	);
}

/*
* CG_asInputClearState
*/
void CG_asInputClearState( void ) {
	CG_asCallScriptFunc( cgs.asInput.clearState, cg_empty_as_cb, cg_empty_as_cb );
}

/*
* CG_asInputKeyEvent
*/
bool CG_asInputKeyEvent( int key, bool down ) {
	uint8_t res = 0;

	if( !cgs.asInput.keyEvent ) {
		return false;
	}

	CG_asCallScriptFunc( cgs.asInput.keyEvent, [key, down](asIScriptContext *ctx)
		{
			ctx->SetArgDWord( 0, key );
			ctx->SetArgByte( 1, down );
		},
		[&res](asIScriptContext *ctx)
		{
			res = ctx->GetReturnByte();
		}
	);

	return res == 0 ? false : true;
}

/*
* CG_asInputMouseMove
*/
void CG_asInputMouseMove( int mx, int my ) {
	CG_asCallScriptFunc( cgs.asInput.mouseMove, [mx, my](asIScriptContext *ctx)
		{
			ctx->SetArgDWord( 0, mx );
			ctx->SetArgDWord( 1, my );
		},
		cg_empty_as_cb
	);
}

/*
* CG_asGetButtonBits
*/
unsigned CG_asGetButtonBits( void ) {
	unsigned res = 0;

	CG_asCallScriptFunc( cgs.asInput.getButtonBits, cg_empty_as_cb,
		[&res](asIScriptContext *ctx)
		{
			res = ctx->GetReturnDWord();
		}
	);

	return res;
}

/*
* CG_asGetAngularMovement
*/
void CG_asGetAngularMovement( vec3_t viewAngles ) {
	CG_asCallScriptFunc( cgs.asInput.getAngularMovement,
		cg_empty_as_cb,
		[viewAngles](asIScriptContext *ctx)
		{
			const asvec3_t *va = ( const asvec3_t * )ctx->GetReturnAddress();
			VectorCopy( va->v, viewAngles );
		}
	);
}

/*
* CG_asGetMovement
*/
void CG_asGetMovement( vec3_t movement ) {
	CG_asCallScriptFunc( cgs.asInput.getMovement,
		cg_empty_as_cb,
		[movement](asIScriptContext *ctx)
		{
			const asvec3_t *mv = ( const asvec3_t * )ctx->GetReturnAddress();
			VectorCopy( mv->v, movement );
		}
	);
}
