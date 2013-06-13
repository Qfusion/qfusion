/*
Copyright (C) 2011 Victor Luchits

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

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI {

class ASMatchMaker
{
public:
	ASMatchMaker() { }

	bool login( const asstring_t &user, const asstring_t &password )
	{
		return trap::MM_Login( ASSTR( user ), ASSTR( password ) ) == qtrue;
	}

	bool logout( void )
	{
		return trap::MM_Logout( qfalse ) == qtrue;
	}

	int state( void ) const
	{
		return trap::MM_GetLoginState();
	}

	asstring_t *user( void ) const
	{
		return ASSTR( trap::Cvar_String( "cl_mm_user" ) );
	}

	asstring_t *profileURL( bool rml ) const
	{
		char buffer[2048];

		trap::MM_GetProfileURL( buffer, sizeof( buffer ), rml ? qtrue : qfalse );
		return ASSTR( buffer );
	}

	asstring_t *baseWebURL() const
	{
		char buffer[2048];

		trap::MM_GetBaseWebURL( buffer, sizeof( buffer ) );
		return ASSTR( buffer );
	}

	asstring_t *lastError( void ) const
	{
		char buffer[2048];

		trap::MM_GetLastErrorMessage( buffer, sizeof( buffer ) );
		return ASSTR( buffer );
	}
};

// ====================================================================

static ASMatchMaker asMM;

/// This makes AS aware of this class so other classes may reference
/// it in their properties and methods
void PrebindMatchMaker( ASInterface *as )
{
	ASBind::Class<ASMatchMaker, ASBind::class_singleref>( as->getEngine() );
}

void BindMatchMaker( ASInterface *as )
{
	ASBind::Enum( as->getEngine(), "eMatchmakerState" )
		( "MM_LOGIN_STATE_LOGGED_OUT", MM_LOGIN_STATE_LOGGED_OUT )
		( "MM_LOGIN_STATE_IN_PROGRESS", MM_LOGIN_STATE_IN_PROGRESS )
		( "MM_LOGIN_STATE_LOGGED_IN", MM_LOGIN_STATE_LOGGED_IN )
	;

	ASBind::GetClass<ASMatchMaker>( as->getEngine() )
		.method( &ASMatchMaker::login, "login" )
		.method( &ASMatchMaker::logout, "logout" )
		.method( &ASMatchMaker::state, "get_state" )
		.method( &ASMatchMaker::lastError, "get_lastError" )
		.method( &ASMatchMaker::user, "get_user" )
		.method( &ASMatchMaker::profileURL, "profileURL" )
		.method( &ASMatchMaker::baseWebURL, "baseWebURL" )
	;
}

void BindMatchMakerGlobal( ASInterface *as )
{
	ASBind::Global( as->getEngine() )
		// global variable
		.var( &asMM, "matchmaker" )
	;
}

}

ASBIND_TYPE( ASUI::ASMatchMaker, Matchmaker );
