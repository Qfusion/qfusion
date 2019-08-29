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

namespace ASUI
{

// dummy funcdef
static void ASMatchMaker_EventListenerCallback( Event *event ) {
}

class ASMatchMaker
{
public:
	ASMatchMaker( ASInterface *asmodule ) : state( 0 ), asmodule( asmodule ) { }
	~ASMatchMaker() { clearEventListeners(); }

	bool login( const asstring_t &user, const asstring_t &password ) {
		return trap::MM_Login( ASSTR( user ), ASSTR( password ) ) == true;
	}

	bool logout( void ) {
		return trap::MM_Logout( false ) == true;
	}

	int getState( void ) const {
		return state;
	}

	asstring_t *getUser( void ) const {
		return ASSTR( trap::Cvar_String( "cl_mm_user" ) );
	}

	asstring_t *getProfileURL( bool rml ) const {
		char buffer[2048];

		trap::MM_GetProfileURL( buffer, sizeof( buffer ), rml ? true : false );
		return ASSTR( buffer );
	}

	asstring_t *getBaseWebURL() const {
		char buffer[2048];

		trap::MM_GetBaseWebURL( buffer, sizeof( buffer ) );
		return ASSTR( buffer );
	}

	asstring_t *getLastError( void ) const {
		char buffer[2048];

		trap::MM_GetLastErrorMessage( buffer, sizeof( buffer ) );
		return ASSTR( buffer );
	}

	void update( void ) {
		const int pstate = state;
		state = trap::MM_GetLoginState();

		Rml::Core::Dictionary ev_parms;

		if( pstate != state ) {
			ev_parms["state"] = pstate;
			ev_parms["old_state"] = pstate;
			dispatchEvent( "stateChange", ev_parms );
		}
	}

	void addEventListener( const asstring_t &event, asIScriptFunction *func ) {
		EventCallback cb;

		cb = ASBind::CreateFunctionPtr( func, cb );

		Listener l( ASSTR( event ), cb );
		listeners.push_back( l );
	}

	void removeEventListener( const asstring_t &event, asIScriptFunction *func ) {
		Listener l( ASSTR( event ), func );

		for( ListenersList::iterator it = listeners.begin(); it != listeners.end(); ++it ) {
			if( it->first == l.first && it->second.getPtr() == func ) {
				it->second.release();
				it = listeners.erase( it );
				break;
			}
		}

		func->Release();
	}

private:
	int state;
	ASInterface *asmodule;

	void dispatchEvent( const char *event, const Rml::Core::Dictionary &parms ) {
/*
 		const Rml::Core::EventSpecification& spec = Rml::Core::EventSpecification::GetOrInsert( event );
		Rml::Core::Event *ev = Rml::Core::Factory::InstanceEvent( NULL, spec.id, spec.type, parms, false );

		ev->SetPhase( Rml::Core::Event::PHASE_BUBBLE ); // FIXME?

		for( ListenersList::iterator it = listeners.begin(); it != listeners.end(); ) {
			EventCallback func = it->second;

			if( !func.isValid() || !func.getModule() ) {
erase:
				func.release();
				it = listeners.erase( it );
				continue;
			}

			if( it->first == event ) {
				ev->AddReference();

				try {
					func.setContext( asmodule->getContext() );
					func( ev );
				} catch( ASBind::Exception & ) {
					Com_Printf( S_COLOR_RED "ASMatchMaker: Failed to call function %s\n", func.getName() );
					goto erase;
				}
			}

			++it;
		}
*/
	}

	void clearEventListeners( void ) {
		for( ListenersList::iterator it = listeners.begin(); it != listeners.end(); ++it )
			it->second.release();
		listeners.clear();
	}

	typedef ASBind::FunctionPtr<void ( Rml::Core::Event* )> EventCallback;
	typedef std::pair<std::string, EventCallback> Listener;
	typedef std::vector<Listener> ListenersList;
	ListenersList listeners;
};

}
ASBIND_TYPE( ASUI::ASMatchMaker, Matchmaker );

namespace ASUI
{

// ====================================================================

static ASMatchMaker *asMM;

/// This makes AS aware of this class so other classes may reference
/// it in their properties and methods
void PrebindMatchMaker( ASInterface *as ) {
	ASBind::Class<ASMatchMaker, ASBind::class_singleref>( as->getEngine() );
}

void BindMatchMaker( ASInterface *as ) {
	ASBind::Global( as->getEngine() )

	// setTimeout and setInterval callback funcdefs
	.funcdef( &ASMatchMaker_EventListenerCallback, "MMEventListenerCallback" )
	;

	ASBind::Enum( as->getEngine(), "eMatchmakerState" )
		( "MM_LOGIN_STATE_LOGGED_OUT", MM_LOGIN_STATE_LOGGED_OUT )
		( "MM_LOGIN_STATE_IN_PROGRESS", MM_LOGIN_STATE_IN_PROGRESS )
		( "MM_LOGIN_STATE_LOGGED_IN", MM_LOGIN_STATE_LOGGED_IN )
	;

	ASBind::GetClass<ASMatchMaker>( as->getEngine() )
	.method( &ASMatchMaker::login, "login" )
	.method( &ASMatchMaker::logout, "logout" )
	.method( &ASMatchMaker::getState, "get_state" )
	.method( &ASMatchMaker::getLastError, "get_lastError" )
	.method( &ASMatchMaker::getUser, "get_user" )
	.method( &ASMatchMaker::getProfileURL, "profileURL" )
	.method( &ASMatchMaker::getBaseWebURL, "baseWebURL" )

	.method2( &ASMatchMaker::addEventListener, "void addEventListener( const String &event, MMEventListenerCallback @callback )" )
	.method2( &ASMatchMaker::removeEventListener, "void removeEventListener( const String &event, MMEventListenerCallback @callback )" )
	;
}

void BindMatchMakerGlobal( ASInterface *as ) {
	assert( asMM == NULL );

	// set the AS module for scheduler
	asMM = __new__( ASMatchMaker )( as );

	ASBind::Global( as->getEngine() )

	// global variable
	.var( asMM, "matchmaker" )
	;
}

void RunMatchMakerFrame( void ) {
	asMM->update();
}

void UnbindMatchMaker( void ) {
	__delete__( asMM );
	asMM = NULL;
}

}
