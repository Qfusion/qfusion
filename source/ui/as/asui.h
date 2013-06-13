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
}

#endif
