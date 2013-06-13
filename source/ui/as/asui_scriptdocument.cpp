#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI {

using namespace Rocket::Core;

class UI_ScriptDocument : public ElementDocument
{
	int numScriptsAdded;
	ASInterface *asmodule;

public:
	UI_ScriptDocument( const String &tag = "body" )
		: ElementDocument( tag ), numScriptsAdded(0)
	{
		asmodule = UI_Main::Get()->getAS();

		// establish our relationship with the building module
		asIScriptModule *scriptModule = asmodule->getModule();
		if( scriptModule ) {
			scriptModule->SetUserData( static_cast<void *>(this) );
		}
	}

	virtual ~UI_ScriptDocument(void)
	{
	}

	virtual void LoadScript( Stream *stream, const String &source_name)
	{
		String code;

		stream->Read( code, stream->Length() );

		if( asmodule && code.Length() > 0 && asmodule->isBuilding() ) {
			asmodule->addScript( asmodule->getModule(), source_name.CString(), code.CString() );
			//Com_Printf( "UI_ScriptDocument: Added <script> from %s\n", source_name.CString() );
			numScriptsAdded++;
		}
	}
};

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
