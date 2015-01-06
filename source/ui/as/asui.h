#pragma once
#ifndef __ASUI_H__
#define __ASUI_H__

#include "as/asmodule.h"

namespace ASUI {

	// asui.cpp
	void BindAPI( ASInterface *as );
	void BindGlobals( ASInterface *as );
	void BindFrame( ASInterface *as );
	void BindShutdown( ASInterface *as );

	// asui_scriptdocument.cpp
	Rocket::Core::ElementInstancer *GetScriptDocumentInstancer( void );

	// asui_scriptevent.cpp
	Rocket::Core::EventListenerInstancer *GetScriptEventListenerInstancer( void );
	/// Releases Angelscript function pointers held by event listeners
	void ReleaseScriptEventListenersFunctions( Rocket::Core::EventListenerInstancer * );
	void GarbageCollectEventListenersFunctions( Rocket::Core::EventListenerInstancer *instancer );

	class UI_ScriptDocument : public Rocket::Core::ElementDocument
	{
		private:
			int numScriptsAdded;
			ASInterface *as;
			asIScriptModule *module;
			bool isLoading;
			unsigned numScripts;

			// TODO: proper PostponedEvent that handles reference counting and event instancing!

			// mechanism that calls onload events after all of AS scripts are built
			typedef std::list<Rocket::Core::Event*> PostponedList;
			PostponedList onloads;

			Rocket::Core::ScriptObject owner;

		public:
			UI_ScriptDocument( const Rocket::Core::String &tag = "body" );
			virtual ~UI_ScriptDocument( void );

			asIScriptModule *GetModule( void ) const;

			virtual void LoadScript( Rocket::Core::Stream *stream, const Rocket::Core::String &source_name );

			virtual void ProcessEvent( Rocket::Core::Event& event );

			virtual Rocket::Core::ScriptObject GetScriptObject( void ) const;

			bool IsLoading( void ) const { return isLoading; }
	};
}

#endif
