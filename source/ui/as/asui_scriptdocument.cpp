#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"
#include <RmlUi/Core/Stream.h>

namespace ASUI
{

using namespace Rml::Core;

UI_ScriptDocument::UI_ScriptDocument( const std::string &tag )
	: ElementDocument( tag ), as( NULL ), module( NULL ), isLoading( true ), numScripts( 0 ), script_object( NULL ) {
	SetProperty( PropertyId::Position, Property( Style::Position::Relative ) );
}

UI_ScriptDocument::~UI_ScriptDocument( void ) {
}

asIScriptModule *UI_ScriptDocument::GetModule( void ) const {
	return module;
}

void UI_ScriptDocument::LoadScript( Stream *stream, const std::string &source_name ) {
	String code;

	stream->Read( code, stream->Length() );

	as = UI_Main::Get()->getAS();

	if( !module ) {
		if( !as ) {
			return;
		}
		module = as->startBuilding( GetSourceURL().c_str() );
	}

	String script_name = source_name;
	if( script_name.empty() ) {
		Rml::Core::FormatString( script_name, 100, "_script_%d", numScripts );
	}
	numScripts++;

	if( module && !code.empty() ) {
		as->addScript( module, script_name.c_str(), code.c_str() );
	}
}

void UI_ScriptDocument::BuildScripts( void ) {
	if( module ) {
		as->finishBuilding( module );
		as->setModuleUserData( module, this );
	}
	isLoading = false;
}

void UI_ScriptDocument::DestroyScripts( void ) {
	if( module ) {
		// FIXME: destroy script event listeners here somehow!

		// destroy the AS module by name to prevent crashes in case multiple document instances share the same module pointer
		as->buildReset( GetSourceURL().c_str() );
		module = NULL;
		isLoading = true;
	}
}

//=========================================================

class UI_ScriptDocumentInstancer : public ElementInstancer
{
public:
	// UI_ScriptDocumentInstancer() {}
	virtual ElementPtr InstanceElement( Element *parent, const std::string &tag, const XMLAttributes &attr ) {
		return ElementPtr(__new__(UI_ScriptDocument)( tag ));
	}

	virtual void ReleaseElement( Element* element ) {
		__delete__(element);
	}
};

ElementInstancer *GetScriptDocumentInstancer( void ) {
	ElementInstancer *instancer = new UI_ScriptDocumentInstancer();
	return instancer;
}

}
