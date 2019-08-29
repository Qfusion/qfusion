#pragma once
#ifndef __ASUI_H__
#define __ASUI_H__

#include "as/asmodule.h"

namespace ASUI
{

// asui.cpp
void BindAPI( ASInterface *as );
void BindGlobals( ASInterface *as );
void BindFrame( ASInterface *as );
void BindShutdown( ASInterface *as );

// asui_scriptdocument.cpp
Rml::Core::ElementInstancer *GetScriptDocumentInstancer( void );

// asui_scriptevent.cpp
Rml::Core::EventListenerInstancer *GetScriptEventListenerInstancer( void );
/// Releases Angelscript function pointers held by event listeners
void ReleaseScriptEventListenersFunctions( Rml::Core::EventListenerInstancer * );
void GarbageCollectEventListenersFunctions( Rml::Core::EventListenerInstancer *instancer );

class UI_ScriptDocument : public Rml::Core::ElementDocument
{
private:
	ASInterface *as;
	asIScriptModule *module;
	bool isLoading;
	unsigned numScripts;
	Rml::Core::ScriptObject script_object;

public:
	UI_ScriptDocument( const std::string &tag );
	virtual ~UI_ScriptDocument( void );

	asIScriptModule *GetModule( void ) const;

	virtual void LoadScript( Rml::Core::Stream *stream, const std::string &source_name ) override;

	void SetScriptObject( Rml::Core::ScriptObject script_object_ ) { script_object = script_object_; }
	virtual Rml::Core::ScriptObject GetScriptObject( void ) const override { return script_object; }

	void BuildScripts( void );
	void DestroyScripts( void );

	bool IsLoading( void ) const { return isLoading; }
};
}

#endif
