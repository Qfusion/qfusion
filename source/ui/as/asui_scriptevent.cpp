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

using namespace Rml::Core;

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
	ASBind::FunctionPtr<void( Rml::Core::Element*, Rml::Core::Event* )> funcPtr;
	String funcName;
	String script;
	bool loaded;
	bool released;
	int uniqueId;
	Rml::Core::Element *target;

	/** DAMN MIXTURE OF Rml::String, std::string and std::ostringstream!! **/

	String createFunctionName( void ) {
		std::ostringstream os;
		os << "__eventfunc_" << uniqueId;
		return String( os.str().c_str() );
	}

	String createFunctionCode( const String &code ) {
		std::ostringstream os;

		// TODO: grab the typenames from ASBind::TypeString
		os << "void __eventfunc_" << uniqueId << "( Element @self, Event @event){" << code.c_str() << "}";
		return String( os.str().c_str() );
	}

	void fetchFunctionPtr( asIScriptModule *module ) {
		if( loaded ) {
			return;
		}

		loaded = true;

		assert( module != NULL );
		if( module == NULL ) {
			return;
		}

		// check direct function-name
		if( script[0] == '$' ) {
			funcName = script.substr( 1 );
		} else {
			// compile inline code
			funcName = createFunctionName();
			String funcCode = createFunctionCode( script );
			script = funcCode;
			asIScriptFunction *scriptFunc = NULL;

			if( !asmodule->addFunction( module, funcName.c_str(), funcCode.c_str(), &scriptFunc ) ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: ScriptEventListener addFunction failed with %s %s\n",
							funcName.c_str(), funcCode.c_str() );
			} else if( scriptFunc ) {
				// I think we only hit this scenario when we do smth like
				// elem.setInnerRML( '<button onclick="window.close();" />' );
				funcPtr = ASBind::CreateFunctionPtr( scriptFunc, funcPtr );
				funcPtr.addref();
				scriptFunc->Release();
			}
			return;
		}

		funcPtr = ASBind::CreateFunctionPtr( funcName.c_str(), module, funcPtr );
		if( !funcPtr.isValid() ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: ScriptEventListener::fetchFunctionPtr failed with %s\n", funcName.c_str() );
			return;
		}
		funcPtr.addref();
	}

public:
	ScriptEventListener( const String &s, int uniqueId, Element *target ) : script( s ),
		loaded( false ), released( false ),  uniqueId( uniqueId ), target( target ) {
		asmodule = UI_Main::Get()->getAS();
		if( target ) {
			//target->AddReference();
		}
	}

	virtual ~ScriptEventListener() {
		releaseFunctionPtr();
	}

	virtual void OnDetach( Element *element ) {
		releaseFunctionPtr();
	}

	virtual void ProcessEvent( Event &event ) {
		if( !target ) {
			return;
		}
		if( released ) {
			// the function pointer has been released, but
			// we're hanging around, waiting for shutdown or GC
			return;
		}

		Element *elem = event.GetTargetElement();

		if( ( elem->GetOwnerDocument() != target->GetOwnerDocument() ) && ( elem->GetOwnerDocument()->GetParentNode() != target ) ) {
			// make sure the event originated from the same document as the original target
			// but leak event to the parent iframe
			return;
		}

		UI_ScriptDocument *document = dynamic_cast<UI_ScriptDocument *>( elem->GetOwnerDocument() );
		if( !document || document->IsLoading() ) {
			return;
		}

		fetchFunctionPtr( document->GetModule() );

		// push elem and event as parameters to the internal function
		// and call it

		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "ScriptEventListener: Event %s, target %s, script %s\n",
						event.GetType().c_str(),
						event.GetTargetElement()->GetTagName().c_str(),
						script.c_str() );
		}

		if( funcPtr.isValid() ) {
			try {
				asIScriptContext *context = asmodule->getContext();

				// the context may actually be NULL after AS shutdown
				if( context ) {
					funcPtr.setContext( context );
					funcPtr( target, &event );
				}
			} catch( ASBind::Exception & ) {
				Com_Printf( S_COLOR_RED "ScriptEventListener: Failed to call function %s %s\n", funcName.c_str(), script.c_str() );
			}
		} else {
			Com_Printf( S_COLOR_RED "ScriptEventListener: Not gonna call invalid function %s %s\n", funcName.c_str(), script.c_str() );
		}
	}

	void releaseFunctionPtr() {
		if( released ) {
			return;
		}

		released = true;
		funcPtr.release();

		if( target ) {
			//target->RemoveReference();
			target = NULL;
		}
	}

	bool isValid() {
		if( released ) {
			return false;
		}
		return true;
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
	ASInterface *as;

	// We have to make Event const, cause we cant use &inout with value-types
	ASBind::FunctionPtr<void( Element*, Event* )> funcPtr;

public:
	ScriptEventCaller( ASInterface *as, asIScriptFunction *func ) : as( as ) {
		funcPtr = ASBind::CreateFunctionPtr( func, funcPtr );
		if( !funcPtr.isValid() ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: ScriptEventCaller::CreateFunctionPtr failed with %s\n", func ? func->GetDeclaration() : "NULL" );
			return;
		}
	}

	virtual ~ScriptEventCaller() {
		funcPtr.release();
	}

	// Perform the cleanup
	virtual void OnDetach( Element* element ) {
		__delete__( this );
	}

	virtual void ProcessEvent( Event &event ) {
		Element *elem = event.GetTargetElement();

		UI_ScriptDocument *document = dynamic_cast<UI_ScriptDocument *>( elem->GetOwnerDocument() );
		if( !document ) {
			return;
		}

		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "ScriptEventCaller: Event %s, target %s, func %s\n",
						event.GetType().c_str(),
						event.GetTargetElement()->GetTagName().c_str(),
						funcPtr.getName() );
		}

		if( funcPtr.isValid() ) {
			//event.AddReference();
			try {
				asIScriptContext *context = as->getContext();

				// the context may actually be NULL after AS shutdown
				if( context ) {
					funcPtr.setContext( context );
					funcPtr( NULL, &event );
				}
			} catch( ASBind::Exception & ) {
				Com_Printf( S_COLOR_RED "ScriptEventListener: Failed to call function %s\n", funcPtr.getName() );
			}
		} else {
			Com_Printf( S_COLOR_RED "ScriptEventListener: Not gonna call invalid function %s\n", funcPtr.getName() );
		}
	}
};

EventListener *CreateScriptEventCaller( ASInterface *as, asIScriptFunction *func ) {
	return __new__( ScriptEventCaller )( as, func );
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
	ScriptEventListenerInstancer() : idCounter( 0 ) {
	}

	virtual ~ScriptEventListenerInstancer() {
	}

	virtual Rml::Core::EventListener* InstanceEventListener( const String& value, Element *elem ) {
		if( value.empty() ) {
			return 0;
		}

		ScriptEventListener *listener = __new__( ScriptEventListener )( value, idCounter++, elem );
		listeners.push_back( listener );
		return listener;
	}

	/// Releases pointers to AS functions held by allocated listeners
	void ReleaseListenersFunctions() {
		for( listenerList::iterator it = listeners.begin(); it != listeners.end(); ++it ) {
			( *it )->releaseFunctionPtr();
		}
	}

	/// Releases all allocated listeners
	void ReleaseListeners() {
		for( listenerList::iterator it = listeners.begin(); it != listeners.end(); ++it ) {
			__delete__( *it );
		}
		listeners.clear();
	}

	void Release() {
		ReleaseListeners();
		__delete__( this );
	}

	void GarbageCollect( void ) {
		for( listenerList::iterator it = listeners.begin(); it != listeners.end(); ) {
			ScriptEventListener *listener = *it;
			if( !listener->isValid() ) {
				it = listeners.erase( it );
				__delete__( listener );
				continue;
			}
			++it;
		}
	}
};

EventListenerInstancer *GetScriptEventListenerInstancer( void ) {
	EventListenerInstancer *instancer = __new__( ScriptEventListenerInstancer )();

	// instancer->RemoveReference();
	return instancer;
}

void ReleaseScriptEventListenersFunctions( EventListenerInstancer *instancer ) {
	ScriptEventListenerInstancer *scriptInstancer = static_cast<ScriptEventListenerInstancer *>( instancer );
	if( scriptInstancer ) {
		scriptInstancer->ReleaseListenersFunctions();
	}
}

void GarbageCollectEventListenersFunctions( EventListenerInstancer *instancer ) {
	ScriptEventListenerInstancer *scriptInstancer = static_cast<ScriptEventListenerInstancer *>( instancer );
	if( scriptInstancer ) {
		scriptInstancer->GarbageCollect();
	}
}

}
