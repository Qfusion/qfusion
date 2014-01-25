/*
 * UI_EventListener.cpp
 *
 *  Created on: 27.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

#include <string>
#include <sstream>

namespace ASUI
{

using namespace Rocket::Core;

//===================================================

// This is listener for inline script events, such as
// <div onclick="runScriptFunction();">
// this is done by creating a proxy function for each of these in format of __eventfunc_XXXX(...)
// with the inline code embedded in the function
// TODO: event strings starting with $ are function names that you can fetch directly
// TODO: event strings that start with # are internal warsow commands
class ScriptEventListener : public EventListener
{
	ASInterface *asmodule;
	// We have to make Event const, cause we cant use &inout with value-types
	ASBind::FunctionPtr<void( Rocket::Core::Element*, Rocket::Core::Event* )> funcPtr;
	String funcName;
	String script;
	bool loaded;
	bool released;
	int uniqueId;

	/** DAMN MIXTURE OF Rocket::String, std::string and std::ostringstream!! **/

	String createFunctionName( int uniqueId )
	{
		std::ostringstream os;
		os << "__eventfunc_" << uniqueId;
		return String( os.str().c_str() );
	}

	String createFunctionCode( int uniqueId, const String &code )
	{
		std::ostringstream os;
		// TODO: grab the typenames from ASBind::TypeString
		os << "void __eventfunc_" << uniqueId << "( Element @self, Event @event){" << code.CString() << "}";
		return String( os.str().c_str() );
	}

	void fetchFunctionPtr( const String &document_name )
	{
		if( loaded ) {
			return;
		}

		loaded = true;

		asIScriptModule *module = asmodule->getModule( document_name.CString() );

		assert( module != NULL );
		if( module == NULL ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: ScriptEventListener unable to find module %s\n", 
				document_name.CString() );
		}

		// check direct function-name
		if( script[0] == '$' )
			funcName = script.Substring( 1 );
		else
		{
			// compile inline code
			funcName = createFunctionName( uniqueId );
			String funcCode = createFunctionCode( uniqueId, script );
			script = funcCode;
			asIScriptFunction *scriptFunc = NULL;

			if( !asmodule->addFunction( module, funcName.CString(), funcCode.CString(), &scriptFunc ) ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: ScriptEventListener addFunction failed with %s %s\n", 
					funcName.CString(), funcCode.CString() );
			} else if( scriptFunc ) {
				// I think we only hit this scenario when we do smth like
				// elem.setInnerRML( '<button onclick="window.close();" />' );
				funcPtr = ASBind::CreateFunctionPtr( scriptFunc, funcPtr );
				funcPtr.addref();
				scriptFunc->Release();
			}
			return;
		}

		funcPtr = ASBind::CreateFunctionPtr( funcName.CString(), module, funcPtr );
		if( !funcPtr.isValid() ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: ScriptEventListener::fetchFunctionPtr failed with %s\n", funcName.CString() );
			return;
		}
		funcPtr.addref();
	}

public:

	ScriptEventListener( const String &s, int uniqueId ) : script( s ), 
		loaded( false ), released( false ),  uniqueId( uniqueId )
	{
		asmodule = UI_Main::Get()->getAS();
	}

	virtual ~ScriptEventListener() {
		releaseFunctionPtr();
	}

	virtual void ProcessEvent( Event &event )
	{
		if( released ) {
			// the function pointer has been released, but
			// we're hanging around, waiting for shutdown or GC
			return;
		}

		// onloads cant be called within building process
		if( /* event.GetType() == "load" && */ asmodule->isBuilding() )
		{
			// TODO: we should eliminate chain to DocumentLoader like this, so move
			// event postponing somewhere else
			UI_Main::Get()->getDocumentLoader()->postponeOnload( this, event );
			return;
		}

		Element *elem = event.GetTargetElement();

		fetchFunctionPtr( elem->GetOwnerDocument()->GetSourceURL() );

		// push elem and event as parameters to the internal function
		// and call it

		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "ScriptEventListener: Event %s, target %s, script %s\n",
				event.GetType().CString(),
				event.GetTargetElement()->GetTagName().CString(),
				script.CString() );
		}

		if( funcPtr.isValid() ) {
			elem->AddReference();
			event.AddReference();
			try {
				asIScriptContext *context = asmodule->getContext();

				// the context may actually be NULL after AS shutdown
				if( context ) {
					funcPtr.setContext( context );
					funcPtr( elem, &event );
				}
			} catch( ASBind::Exception & ) {
				Com_Printf( S_COLOR_RED "ScriptEventListener: Failed to call function %s %s\n", funcName.CString(), script.CString() );
			}
		}
		else {
			Com_Printf( S_COLOR_RED "ScriptEventListener: Not gonna call invalid function %s %s\n", funcName.CString(), script.CString() );
		}
	}

	void releaseFunctionPtr()
	{
		released = true;
		funcPtr.release();
	}
};

//==============================================================

// TODO: migrate with ScriptEventListener
// When scripts call AddEventListener, one of these is created holding
// the function given. This is then registered for that event and when that
// event happens this gets called which in turn calls the script function
// TODO: make it possible to input inline scripts as event callbacks!
class ScriptEventCaller : public EventListener
{
	ASInterface *asmodule;
	// We have to make Event const, cause we cant use &inout with value-types
	ASBind::FunctionPtr<void( Element*, Event* )> funcPtr;

public:
	ScriptEventCaller( ASInterface *as, asIScriptFunction *func ) : asmodule( as )
	{
		funcPtr = ASBind::CreateFunctionPtr( func, funcPtr );
		if( !funcPtr.isValid() ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: ScriptEventCaller::CreateFunctionPtr failed with %s\n", func ? func->GetDeclaration() : "NULL" );
			return;
		}
	}

	virtual ~ScriptEventCaller() 
	{
		funcPtr.release();
	}

	// Perform the cleanup
	virtual void OnDetach( Element* element )
	{
		__delete__( this );
	}

	virtual void ProcessEvent( Event &event )
	{
		// onloads cant be called within building process
		if( /* event.GetType() == "load" && */ asmodule->isBuilding() )
		{
			UI_Main::Get()->getDocumentLoader()->postponeOnload( this, event );
			return;
		}

		Element *elem = event.GetTargetElement();

		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "ScriptEventCaller: Event %s, target %s, func %s\n",
				event.GetType().CString(),
				event.GetTargetElement()->GetTagName().CString(),
				funcPtr.getName() );
		}

		if( funcPtr.isValid() ) {
			elem->AddReference();
			event.AddReference();
			try {
				asIScriptContext *context = asmodule->getContext();

				// the context may actually be NULL after AS shutdown
				if( context ) {
					funcPtr.setContext( context );
					funcPtr( elem, &event );
				}
			} catch( ASBind::Exception & ) {
				Com_Printf( S_COLOR_RED "ScriptEventListener: Failed to call function %s\n", funcPtr.getName() );
			}
		}
		else {
			Com_Printf( S_COLOR_RED "ScriptEventListener: Not gonna call invalid function %s\n", funcPtr.getName() );
		}
	}
};

EventListener *CreateScriptEventCaller( ASInterface *as, asIScriptFunction *func )
{
	return __new__(ScriptEventCaller)( as, func );
}

//===================================================

// This instancer creates EventListeners for inline script-events
// such as <div onclick="runScriptFunction();">

class ScriptEventListenerInstancer : public EventListenerInstancer
{
	typedef std::vector<ScriptEventListener*> listenerList;

	listenerList listeners;
	int idCounter;

public:
	ScriptEventListenerInstancer() : idCounter(0)
	{
	}

	virtual ~ScriptEventListenerInstancer()
	{
	}

	virtual Rocket::Core::EventListener* InstanceEventListener( const String& value, Element *elem )
	{
		if( !value.Length() )
			return 0;

		ScriptEventListener *listener = __new__( ScriptEventListener )( value, idCounter++ );
		listeners.push_back( listener );
		return listener;
	}

	/// Releases pointers to AS functions held by allocated listeners
	void ReleaseListnersFunctions()
	{
		listenerList::iterator it;
		for( it = listeners.begin(); it != listeners.end(); ++it ) {
			(*it)->releaseFunctionPtr();
		}
	}

	/// Releases all allocated listeners
	void ReleaseListners()
	{
		listenerList::iterator it;
		for( it = listeners.begin(); it != listeners.end(); ++it ) {
			__delete__( *it );
		}
		listeners.clear();
	}

	void Release()
	{
		ReleaseListners();
		__delete__( this );
	}
};

EventListenerInstancer *GetScriptEventListenerInstancer( void )
{
	EventListenerInstancer *instancer = __new__( ScriptEventListenerInstancer )();
	// instancer->RemoveReference();
	return instancer;
}

void ReleaseScriptEventListenersFunctions( EventListenerInstancer *instancer )
{
	ScriptEventListenerInstancer *scriptInstancer = static_cast<ScriptEventListenerInstancer *>( instancer );
	if( scriptInstancer ) {
		scriptInstancer->ReleaseListnersFunctions();
	}
}

}
