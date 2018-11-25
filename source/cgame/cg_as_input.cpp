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

const gs_asglobfuncs_t asCGameInputGlobalFuncs[] =
{
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
void CG_asInputFrame( int frameTime ) {
	CG_asCallScriptFunc( cgs.asInput.frame, [frameTime](asIScriptContext *ctx)
		{
			ctx->SetArgQWord( 0, trap_Milliseconds() );
			ctx->SetArgDWord( 1, frameTime );
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
