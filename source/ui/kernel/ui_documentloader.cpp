#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_eventlistener.h"
#include "kernel/ui_documentloader.h"

namespace WSWUI
{

namespace Core = Rml::Core;

//==========================================

// Document

Document::Document( const std::string &name, NavigationStack *stack )
	: documentName( name ), rocketDocument(), stack( stack ), viewed( false )
{}

Document::~Document() {
	// well.. we dont remove references here or purge the document?
}

void Document::Show( bool modal, bool autofocus ) {
	auto *doc = rocketDocument;
	if( doc == nullptr ) {
		return;
	}

	Rml::Core::ModalFlag modal_flag = modal ? Rml::Core::ModalFlag::Modal : Rml::Core::ModalFlag::None;
	Rml::Core::FocusFlag focus_flag = autofocus ? Rml::Core::FocusFlag::Auto : Rml::Core::FocusFlag::Document;

	doc->Show( modal_flag, focus_flag );
}

void Document::Hide() {
	auto *doc = rocketDocument;
	if( doc == nullptr ) {
		return;
	}
	doc->Hide();
}

bool Document::IsModal() {
	auto *doc = rocketDocument;
	if( doc == nullptr ) {
		return false;
	}
	return doc->IsModal();
}

//==========================================

DocumentLoader::DocumentLoader( int contextId ) : contextId( contextId ) {
	// register itself to UI_Main
}

DocumentLoader::~DocumentLoader() {

}

Document *DocumentLoader::loadDocument( const char *path, NavigationStack *stack ) {
	UI_Main *ui = UI_Main::Get();
	RocketModule *rm = ui->getRocket();
	Document *loadedDocument;

	loadedDocument = __new__( Document )( path, stack );

	// load the .rml
	Rml::Core::ElementDocument *rocketDocument = rm->loadDocument( contextId, path, loadedDocument );
	loadedDocument->setRocketDocument( rocketDocument );

	if( !rocketDocument ) {
		Com_Printf( "DocumentLoader::loadDocument failed to load %s\n", path );
		__delete__( loadedDocument );
		return NULL;
	}

	return loadedDocument;
}

// TODO: redundant
void DocumentLoader::closeDocument( Document *document ) {
	UI_Main *ui = UI_Main::Get();
	RocketModule *rm = ui->getRocket();
	Rml::Core::ElementDocument *rocketDocument = document->getRocketDocument();

	rm->closeDocument( rocketDocument );
}


}
