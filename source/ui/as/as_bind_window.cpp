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
#include "as/asui_url.h"
#include "as/asui_scheduled.h"

namespace ASUI
{

typedef WSWUI::RefreshState RefreshState;

class ASWindow : public EventListener
{
public:
	ASWindow( ASInterface *asmodule ) :
		EventListener(),
		suspendedContext( NULL ),
		attachedModalDocument( NULL ),
		modalValue( 0 ), shuttingDown( false ),
		backgroundTrackPlaying( false ) {
		schedulers.clear();
	}

	~ASWindow() {
		shutdown();
	}

	void shutdown() {
		shuttingDown = true;

		// detatch itself from the possibly opened modal window
		detachAsEventListener();

		// remove schedulers for all documents we hold references to
		for( SchedulerMap::iterator it = schedulers.begin(); it != schedulers.end(); ++it ) {
			FunctionCallScheduler *scheduler = it->second;

			scheduler->shutdown();
			__delete__( scheduler );
		}
		schedulers.clear();
	}

	/// Loads document from the passed URL.
	ElementDocument *open( const asstring_t &location ) {
		WSWUI::NavigationStack *stack = GetCurrentUIStack();
		if( stack == NULL ) {
			return NULL;
		}

		// create new stack in the same context
		WSWUI::NavigationStack *new_stack = UI_Main::Get()->createStack( stack->getContextId() );
		if( new_stack == NULL ) {
			return NULL;
		}

		WSWUI::Document *ui_document = new_stack->pushDocument( location.buffer );
		if( !ui_document ) {
			return NULL;
		}
		return ui_document->getRocketDocument();
	}

	void preload( const asstring_t &location ) {
		if( !UI_Main::preloadEnabled() ) {
			return;
		}

		WSWUI::NavigationStack *stack = GetCurrentUIStack();
		if( stack == NULL ) {
			return;
		}
		stack->preloadDocument( location.buffer );
	}

	/// Loads modal document from the URL.
	/// FIXME: move to window.
	void modal( const asstring_t &location, int defaultCode = -1 ) {
		WSWUI::NavigationStack *stack = GetCurrentUIStack();

		// default return value when modal window is not closed via window.close()
		modalValue = defaultCode;
		if( stack == NULL || stack->isTopModal() ) {
			modalValue = defaultCode;
			return;
		}

		// suspend active context, we're going to resume it when
		// the modal dialog is closed
		suspendActiveContext();

		if( suspendedContext ) {
			// attach itself as a listener of hide event so the context
			// can be resumed after the modal document is hidden
			WSWUI::Document *doc = stack->pushDocument( location.buffer, true, true );
			if( doc ) {
				attachedModalDocument = doc->getRocketDocument();
				attachedModalDocument->AddEventListener( "hide", this );
			}
		}
	}

	/// Returns exit code of the last opened modal document.
	int getModalValue( void ) const {
		return modalValue;
	}

	/// Closes the current document if there's more than one on the stack.
	/// Stores exit code to be passed to suspended context if modal.
	void close( int code = 0 ) {
		WSWUI::NavigationStack *stack = GetCurrentUIStack();
		if( stack == NULL ) {
			return;
		}

		ElementDocument *document = GetCurrentUIDocument();
		bool isModal = document->IsModal();

		// can't close if there's a modal dialog on top
		if( !isModal && stack->isTopModal() ) {
			return;
		}

		// so it's a top modal dialog or there's no modal dialog on stack at all
		if( isModal ) {
			modalValue = code;
			stack->popDocument();
		} else if( stack->getContextId() == UI_CONTEXT_MAIN ) {
			// not really a modal window, clear the stack
			UI_Main::Get()->showUI( false );
		}
	}

	/// Run all currently active schedulers.
	/// If we're the only reference holder to a document, release the document and its scheduler
	void update( void ) {
		SchedulerMap::iterator it = schedulers.begin();
		while( it != schedulers.end() ) {
			FunctionCallScheduler *scheduler = it->second;
			scheduler->update();
			++it;
		}
	}

	virtual void OnDetach( Element *element ) {
		if( shuttingDown ) {
			return;
		}

		ElementDocument *doc = dynamic_cast<ElementDocument *>( element );
		SchedulerMap::iterator it = schedulers.find( doc );
		if( it == schedulers.end() ) {
			// FIXME
			return;
		}

		FunctionCallScheduler *scheduler = it->second;
		scheduler->shutdown();
		__delete__( scheduler );

		schedulers.erase( it );
	}

	ElementDocument *getDocument( void ) const {
		ElementDocument *document = GetCurrentUIDocument();
		assert( document != NULL );
		return document;
	}

	asstring_t *getLocation( void ) const {
		ElementDocument *document = GetCurrentUIDocument();
		assert( document != NULL );
		return ASSTR( document->GetSourceURL().c_str() );
	}

	void setLocation( const asstring_t &location ) {
		WSWUI::NavigationStack *stack = GetCurrentUIStack();
		if( stack == NULL ) {
			return;
		}
		stack->pushDocument( location.buffer );
	}

	int64_t getTime( void ) const {
		const RefreshState &state = UI_Main::Get()->getRefreshState();
		return state.time;
	}

	bool getDrawBackground( void ) const {
		const RefreshState &state = UI_Main::Get()->getRefreshState();
		return state.drawBackground;
	}

	int getWidth( void ) const {
		const RefreshState &state = UI_Main::Get()->getRefreshState();
		return state.width;
	}

	int getHeight( void ) const {
		const RefreshState &state = UI_Main::Get()->getRefreshState();
		return state.height;
	}

	float getPixelRatio( void ) const {
		const RefreshState &state = UI_Main::Get()->getRefreshState();
		return state.pixelRatio;
	}

	unsigned int historySize( void ) const {
		WSWUI::NavigationStack *stack = GetCurrentUIStack();
		if( stack != NULL ) {
			return stack->getStackSize();
		}
		return 0;
	}

	void historyBack( void ) const {
		WSWUI::NavigationStack *stack = GetCurrentUIStack();
		if( stack != NULL && stack->hasAtLeastTwoDocuments() && !stack->isTopModal() ) {
			stack->popDocument();
		}
	}

	int setTimeout( asIScriptFunction *func, unsigned int ms ) {
		return getSchedulerForCurrentUIDocument()->setTimeout( func, ms );
	}

	int setTimeout( asIScriptFunction *func, unsigned int ms, CScriptAnyInterface &any ) {
		return getSchedulerForCurrentUIDocument()->setTimeout( func, ms, any );
	}

	int setInterval( asIScriptFunction *func, unsigned int ms ) {
		return getSchedulerForCurrentUIDocument()->setInterval( func, ms );
	}

	int setInterval( asIScriptFunction *func, unsigned int ms, CScriptAnyInterface &any ) {
		return getSchedulerForCurrentUIDocument()->setInterval( func, ms, any );
	}

	void clearTimeout( int id ) {
		getSchedulerForCurrentUIDocument()->clearTimeout( id );
	}

	void clearInterval( int id ) {
		getSchedulerForCurrentUIDocument()->clearInterval( id );
	}

	void ProcessEvent( Event &event ) {
		if( suspendedContext && event.GetTargetElement() == attachedModalDocument ) {
			detachAsEventListener();
			resumeSuspendedContext();
		}
	}

	void startLocalSound( const asstring_t &s, float volume ) {
		trap::S_StartLocalSound( trap::S_RegisterSound( s.buffer ), 0, volume );
	}

	void startBackgroundTrack( const asstring_t &intro, const asstring_t &loop, bool stopIfPlaying ) {
		if( stopIfPlaying || !backgroundTrackPlaying ) {
			trap::S_StartBackgroundTrack( intro.buffer, loop.buffer, 3 );
			backgroundTrackPlaying = true;
		}
	}

	void stopBackgroundTrack( void ) {
		trap::S_StopBackgroundTrack();
		backgroundTrackPlaying = false;
	}

	void flash( unsigned int count ) {
		trap::VID_FlashWindow( count );
	}

	int getConnectCount( void ) const {
		return UI_Main::Get()->getConnectCount();
	}

	unsigned int getSupportedInputDevices( void ) const {
		return trap::IN_SupportedDevices();
	}

	void showSoftKeyboard( bool show ) {
		trap::IN_ShowSoftKeyboard( show ? true : false );
	}

	bool isBrowserAvailable( void ) const {
		return trap::CL_IsBrowserAvailable();
	}

	asstring_t *getOSName( void ) const {
		return ASSTR( OSNAME );
	}

private:
	typedef std::map<ElementDocument *, FunctionCallScheduler *>  SchedulerMap;
	SchedulerMap schedulers;

	/// Suspend active Angelscript execution context
	void suspendActiveContext( void ) {
		suspendedContext = UI_Main::Get()->getAS()->getActiveContext();
		suspendedContext->Suspend();
	}

	/// Resume previously suspended AngelScript execution context
	void resumeSuspendedContext( void ) {
		suspendedContext->Execute();
		suspendedContext = NULL;
	}

	static ElementDocument *GetCurrentUIDocument( void ) {
		// note that this method can be called outside the AS execution context!
		asIScriptModule *m = UI_Main::Get()->getAS()->getActiveModule();
		if( !m ) {
			return NULL;
		}
		UI_ScriptDocument *ui_document = static_cast<UI_ScriptDocument *>( m->GetUserData() );
		return ui_document;
	}

	static WSWUI::NavigationStack *GetCurrentUIStack( void ) {
		// note that this method can be called outside the AS execution context!
		asIScriptModule *m = UI_Main::Get()->getAS()->getActiveModule();
		if( !m ) {
			return NULL;
		}
		UI_ScriptDocument *ui_document = static_cast<UI_ScriptDocument *>( m->GetUserData() );
		WSWUI::Document *wsw_document = static_cast<WSWUI::Document *>( ui_document->GetScriptObject() );
		return wsw_document ? wsw_document->getStack() : NULL;
	}

	void detachAsEventListener( void ) {
		if( attachedModalDocument ) {
			attachedModalDocument->RemoveEventListener( "hide", this );
			attachedModalDocument = NULL;
		}
	}

	/// finds or creates new scheduler for the document currently on AS-stack
	FunctionCallScheduler *getSchedulerForCurrentUIDocument( void ) {
		ElementDocument *doc = GetCurrentUIDocument();

		assert( doc != NULL );

		SchedulerMap::iterator it = schedulers.find( doc );

		FunctionCallScheduler *scheduler;
		if( it == schedulers.end() ) {
			scheduler = __new__( FunctionCallScheduler )();
			scheduler->init( UI_Main::Get()->getAS() );
			schedulers[doc] = scheduler;
		} else {
			scheduler = it->second;
		}
		return scheduler;
	}

	// context we've suspended to popup the modal document
	// we're going to resume it as soon as the document
	// is closed with document.close call in the script
	asIScriptContext *suspendedContext;

	// modal document we've attached to
	ElementDocument *attachedModalDocument;

	// exit code passed via document.close() of the modal document
	int modalValue;
	bool shuttingDown;

	bool backgroundTrackPlaying;
};

// ====================================================================

static ASWindow *asWindow;

/// This makes AS aware of this class so other classes may reference
/// it in their properties and methods
void PrebindWindow( ASInterface *as ) {
	ASBind::Class<ASWindow, ASBind::class_singleref>( as->getEngine() );
}

void BindWindow( ASInterface *as ) {
	ASBind::Global( as->getEngine() )

	// setTimeout and setInterval callback funcdefs
	.funcdef( &FunctionCallScheduler::ASFuncdef, "TimerCallback" )
	.funcdef( &FunctionCallScheduler::ASFuncdef2, "TimerCallback2" )
	;

	ASBind::Enum( as->getEngine(), "eInputDeviceMask" )
		( "IN_DEVICE_KEYBOARD", IN_DEVICE_KEYBOARD )
		( "IN_DEVICE_MOUSE", IN_DEVICE_MOUSE )
		( "IN_DEVICE_JOYSTICK", IN_DEVICE_JOYSTICK )
		( "IN_DEVICE_TOUCHSCREEN", IN_DEVICE_TOUCHSCREEN )
		( "IN_DEVICE_SOFTKEYBOARD", IN_DEVICE_SOFTKEYBOARD )
	;

	ASBind::GetClass<ASWindow>( as->getEngine() )
	.method( &ASWindow::open, "open" )
	.method2( &ASWindow::close, "void close( int code = 0 )" )
	.method2( &ASWindow::modal, "void modal( const String &location, int defaultCode = -1 )" )
	.method( &ASWindow::getModalValue, "getModalValue" )
	.method( &ASWindow::preload, "preload" )

	.method( &ASWindow::getDocument, "get_document" )

	.method( &ASWindow::getLocation, "get_location" )
	.method( &ASWindow::setLocation, "set_location" )

	.method( &ASWindow::getTime, "get_time" )
	.method( &ASWindow::getDrawBackground, "get_drawBackground" )
	.method( &ASWindow::getWidth, "get_width" )
	.method( &ASWindow::getHeight, "get_height" )
	.method( &ASWindow::getPixelRatio, "get_pixelRatio" )

	.method( &ASWindow::historySize, "history_size" )
	.method( &ASWindow::historyBack, "history_back" )

	.method2( &ASWindow::startLocalSound, "void startLocalSound( String &in sound, float volume = 1.0 ) const" )
	.method2( &ASWindow::startBackgroundTrack, "void startBackgroundTrack( String &in intro, String &in loop, bool stopIfPlaying = true ) const" )
	.constmethod( &ASWindow::stopBackgroundTrack, "stopBackgroundTrack" )

	.method2<int ( ASWindow::* )( asIScriptFunction *, unsigned int )>
		( &ASWindow::setTimeout, "int setTimeout (TimerCallback @, uint)" )
	.method2<int ( ASWindow::* )( asIScriptFunction *, unsigned int )>
		( &ASWindow::setInterval, "int setInterval (TimerCallback @, uint)" )

	.method2<int ( ASWindow::* )( asIScriptFunction *, unsigned int, CScriptAnyInterface & )>
		( &ASWindow::setTimeout, "int setTimeout (TimerCallback2 @, uint, any &in)" )
	.method2<int ( ASWindow::* )( asIScriptFunction *, unsigned int, CScriptAnyInterface & )>
		( &ASWindow::setInterval, "int setInterval (TimerCallback2 @, uint, any &in)" )

	.method( &ASWindow::clearTimeout, "clearTimeout" )
	.method( &ASWindow::clearInterval, "clearInterval" )

	.method( &ASWindow::flash, "flash" )

	.method( &ASWindow::getConnectCount, "get_connectCount" )

	.method( &ASWindow::getSupportedInputDevices, "get_supportedInputDevices" )

	.method( &ASWindow::showSoftKeyboard, "showSoftKeyboard" )

	.method( &ASWindow::isBrowserAvailable, "get_browserAvailable" )

	.method( &ASWindow::getOSName, "get_osName" )
	;
}

void BindWindowGlobal( ASInterface *as ) {
	assert( asWindow == NULL );

	// set the AS module for scheduler
	asWindow = __new__( ASWindow )( as );

	ASBind::Global( as->getEngine() )

	// global variable
	.var( asWindow, "window" )
	;
}

void RunWindowFrame( void ) {
	assert( asWindow != NULL );

	asWindow->update();
}

void UnbindWindow( void ) {
	if( asWindow ) {
		__delete__( asWindow );
	}
	asWindow = NULL;
}

}

ASBIND_TYPE( ASUI::ASWindow, Window );
