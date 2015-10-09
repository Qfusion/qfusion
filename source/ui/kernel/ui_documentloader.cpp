#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_eventlistener.h"
#include "kernel/ui_documentloader.h"

namespace WSWUI {

namespace Core = Rocket::Core;

//==========================================

// Document

Document::Document( const std::string &name, NavigationStack *stack )
	: documentName( name ), rocketDocument( NULL ), stack( stack ), viewed( false )
{}

Document::~Document()
{
	// well.. we dont remove references here or purge the document?
}

int Document::addReference()
{
	if( rocketDocument )
	{
		rocketDocument->AddReference();
		return rocketDocument->GetReferenceCount();
	}
	return 0;
}

int Document::removeReference()
{
	if( rocketDocument )
	{
		rocketDocument->RemoveReference();
		return rocketDocument->GetReferenceCount();
	}
	return 0;
}

int Document::getReference()
{
	if( rocketDocument )
		return rocketDocument->GetReferenceCount();
	return 0;
}

void Document::Show( bool show, bool modal )
{
	if( rocketDocument ) {
		if( show ) {
			rocketDocument->Show( modal ? Rocket::Core::ElementDocument::MODAL : 0 );
		}
		else {
			rocketDocument->Hide();
		}
	}
}

void Document::Hide()
{
	if( rocketDocument )
		rocketDocument->Hide();
}

void Document::Focus()
{
	if( rocketDocument )
		rocketDocument->Focus();
}

void Document::FocusFirstTabElement()
{
	if( rocketDocument ) {
		if (!rocketDocument->FocusFirstTabElement())
			rocketDocument->Focus();
	}
}

bool Document::IsModal()
{
	if( rocketDocument )
		return rocketDocument->IsModal();
	return false;
}

//==========================================

DocumentLoader::DocumentLoader(int contextId) : contextId(contextId)
{
	// register itself to UI_Main
}

DocumentLoader::~DocumentLoader()
{

}

Document *DocumentLoader::loadDocument(const char *path, NavigationStack *stack)
{
	UI_Main *ui = UI_Main::Get();
	RocketModule *rm = ui->getRocket();
	Document *loadedDocument;

	loadedDocument = __new__( Document )( path, stack );

	// load the .rml
	Rocket::Core::ElementDocument *rocketDocument = rm->loadDocument( contextId, path, /* true */ false, loadedDocument );
	loadedDocument->setRocketDocument( rocketDocument );

	if( !rocketDocument ) {
		Com_Printf( "DocumentLoader::loadDocument failed to load %s\n", path);
		__delete__( loadedDocument );
		return NULL;
	}

	// handle postponed onload events (HOWTO handle these in cached documents?)
	Rocket::Core::Dictionary ev_parms;
	ev_parms.Set( "owner", loadedDocument );
	rocketDocument->DispatchEvent( "afterLoad", ev_parms );

	return loadedDocument;
}

// TODO: redundant
void DocumentLoader::closeDocument(Document *document)
{
	UI_Main *ui = UI_Main::Get();
	RocketModule *rm = ui->getRocket();
	Rocket::Core::ElementDocument *rocketDocument = document->getRocketDocument();

	// handle postponed onload events (HOWTO handle these in cached documents?)
	Rocket::Core::Dictionary ev_parms;
	rocketDocument->DispatchEvent( "beforeUnload", ev_parms );

	rm->closeDocument(rocketDocument);
}


}
