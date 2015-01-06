#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI {

using namespace Rocket::Core;

UI_ScriptDocument::UI_ScriptDocument( const String &tag )
	: ElementDocument( tag ), numScriptsAdded( 0 ), as( NULL ), module( NULL ), isLoading( false ), numScripts( 0 ), owner( NULL )
{
	isLoading = true;
	onloads.clear();
}

UI_ScriptDocument::~UI_ScriptDocument( void )
{
}

asIScriptModule *UI_ScriptDocument::GetModule( void ) const
{
	return module;
}
	
void UI_ScriptDocument::LoadScript( Stream *stream, const String &source_name )
{
	String code;

	stream->Read( code, stream->Length() );

	if( !isLoading ) {
		return;
	}

	as = UI_Main::Get()->getAS();

	if( !module ) {
		if( !as ) {
			return;
		}
		module = as->startBuilding( GetSourceURL().CString() );
	}

	String script_name = source_name;
	if( script_name.Empty() ) {
		script_name.FormatString( 100, "_script_%d", numScripts );
	}
	numScripts++;

	if( module && !code.Empty() && isLoading ) {
		as->addScript( module, script_name.CString(), code.CString() );
		numScriptsAdded++;
	}
}

void UI_ScriptDocument::ProcessEvent( Rocket::Core::Event &event )
{
	if( event.GetType() == "afterLoad" && event.GetTargetElement() == this ) {
		if( module ) {
			owner = event.GetParameter<void *>( "owner", NULL );

			as->finishBuilding( module );
			as->setModuleUserData( module, owner );
		}

		isLoading = false;

		// handle postponed onload events (HOWTO handle these in cached documents?)
		for( PostponedList::iterator it = onloads.begin(); it != onloads.end(); ++it ) {
			Rocket::Core::Event *load = *it;
			this->DispatchEvent( load->GetType(), *(load->GetParameters()), true );
			load->RemoveReference();
		}

		// and clear the events
		onloads.clear();
		return;
	}

	if( event.GetType() == "beforeUnload" && event.GetTargetElement() == this ) {
		if( module ) {
			// FIXME: destroy script event listeners here somehow!

			// destroy the AS module by name to prevent crashes in case multiple document instances share the same module pointer
			as->buildReset( GetSourceURL().CString() );
			module = NULL;
		}
		return;
	}

	if( isLoading ) {
		Rocket::Core::Event *instanced = Rocket::Core::Factory::InstanceEvent( event.GetTargetElement(),
			event.GetType(), *event.GetParameters(), true );
		onloads.push_back( instanced );
		event.StopPropagation();
		return;
	}

	Rocket::Core::ElementDocument::ProcessEvent( event );
}

Rocket::Core::ScriptObject UI_ScriptDocument::GetScriptObject( void ) const
{
	return owner;
}

//=========================================================

class UI_ScriptDocumentInstancer : public ElementInstancer
{
public:
	// UI_ScriptDocumentInstancer() {}
	virtual Element *InstanceElement( Element *parent, const String &tag, const XMLAttributes &attr )
	{
		return new UI_ScriptDocument( tag );
	}

	virtual void ReleaseElement(Element* element)
	{
		//ElementDocument *doc = dynamic_cast<ElementDocument*>( element );
		//Com_Printf("ReleaseElement called %s\n", doc ? doc->GetSourceURL().CString() : "" );
		delete element;
	}

	virtual void Release() { delete this; }
};

ElementInstancer *GetScriptDocumentInstancer( void )
{
	ElementInstancer *instancer = new UI_ScriptDocumentInstancer();
	// instancer->RemoveReference();
	return instancer;
}

}
