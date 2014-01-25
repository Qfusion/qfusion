#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_eventlistener.h"
#include "kernel/ui_documentloader.h"

namespace WSWUI {

namespace Core = Rocket::Core;

//==========================================

// Document

Document::Document( const std::string &name, Core::ElementDocument *elem )
	: documentName( name ), rocketDocument( elem ), viewed( false )
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
		// here we have to handle releasing
		if( rocketDocument->GetReferenceCount() == 1 )
		{
			Core::ElementDocument *tempDoc = rocketDocument;
			rocketDocument = 0;
			tempDoc->RemoveReference();
			return 0;
		}
		// else
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
			rocketDocument->Focus();
			rocketDocument->Show( modal ? Rocket::Core::ElementDocument::MODAL : Rocket::Core::ElementDocument::FOCUS );
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

bool Document::IsModal()
{
	if( rocketDocument )
		return rocketDocument->IsModal();
	return false;
}

//==========================================

// DocumentCache

DocumentCache::DocumentCache()
{

}

DocumentCache::~DocumentCache()
{

}

// load or fetch document
Document *DocumentCache::getDocument( const std::string &name )
{
	Document *document = 0;

	// we need to use fake Document for comparison
	Document match( name );
	DocumentSet::iterator it = documentSet.find( &match );

	if( it == documentSet.end() )
	{
		// load it up, and keep the reference for the stack
		DocumentLoader loader( name.c_str() );

		if( !loader.getDocument() )
			return 0;

		document = __new__( Document )( name, loader.getDocument() );
		documentSet.insert( document );

		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "DocumentCache::getDocument, fully loaded document %s (refcount %d)\n", name.c_str(), document->getReference() );
		}
	}
	else
	{
		document = *it;
		document->addReference();

		// document has refcount of 1*cache + 1*previous owners + 1*caller
		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "DocumentCache::getDocument, found document %s from cache (refcount %d)\n", name.c_str(), document->getReference() );
		}
	}

	return document;
}

// release document
DocumentCache::DocumentSet::iterator DocumentCache::purgeDocument( DocumentSet::iterator it )
{
	Document *doc = *it;
	DocumentSet::iterator next = it;
	++next;

	// just trust the reference-counting
	doc->removeReference();

	// unload modal documents as we may actually have many of them
	// during the UI session, keeping all of them in memory seems rather wasteful
	// another reason is that modal document will keep stealing focus
	// from other librocket documents until it is unloaded or "shown" without
	// the modal flag set
	if( doc->IsModal() ) {
		DocumentLoader loader;
		loader.closeDocument( doc->getRocketDocument() );
		documentSet.erase( it );
	}
	
	return next;
}

// release document
void DocumentCache::purgeDocument( Document *doc )
{
	DocumentSet::iterator it = documentSet.find( doc );
	if( it == documentSet.end() )
	{
		Com_Printf("Warning: DocumentCache::purgeDocument couldn't find document %s\n", doc->getName().c_str() );
		return;
	}

	purgeDocument( it );
}

// release all documents
// TODO: should we clear the whole cache and just leave the reference'd
// ones to float around?
void DocumentCache::purgeAllDocuments()
{
	if( UI_Main::Get()->debugOn() ) {
		Com_Printf("DocumentCache::purgeAllDocument\n");
	}

	DocumentSet::iterator it = documentSet.begin();
	while( it != documentSet.end() )
	{
		it = purgeDocument( it );
	}

	// DEBUG
	if( UI_Main::Get()->debugOn() ) {
		if( documentSet.size() > 0 ) {
			Com_Printf("Warning: DocumentCache::purgeAllDocuments: still have %d documents in the cache\n", documentSet.size() );
			for( DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ++it )
				Com_Printf("    %s (refcount %d)\n", (*it)->getName().c_str(), (*it)->getReference() );
		}
	}
}

// as above but will force destroy all documents
void DocumentCache::clearCaches()
{
	if( UI_Main::Get()->debugOn() ) {
		Com_Printf("DocumentCache::clearCaches\n");
	}

	// force destroy all documents
	// purgeAllDocuments();

	DocumentLoader loader;
	for( DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ++it ) {
		if( (*it)->getRocketDocument() ) {
			//(*it)->removeReference();
			loader.closeDocument( (*it)->getRocketDocument() );
		}
	}

	documentSet.clear();

	// here we also do this
	Rocket::Core::Factory::ClearStyleSheetCache();
	// and if in the future Rocket offers more cache-cleaning functions, call 'em
}

// DEBUG
void DocumentCache::printCache()
{
	for(DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ++it )
		Com_Printf("  %s (%d references)\n", (*it)->getName().c_str(), (*it)->getReference() );
}

// send "invalidate" event to all documents
// elements that reference engine assets (models, shaders, sounds, etc)
// must attach themselves to the event as listeneres to either touch
// the assets or invalidate them
void DocumentCache::invalidateAssets(void)
{
	Rocket::Core::Dictionary parameters;
	for( DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ++it ) {
		( *it )->getRocketDocument()->DispatchEvent( "invalidate", parameters, true );
	}
}

//==========================================

// NavigationStack

NavigationStack::NavigationStack() : modalTop( false ), stackLocked( false )
{

}

NavigationStack::~NavigationStack()
{

}

// stack operations
Document *NavigationStack::pushDocument(const std::string &name, bool modal, bool show)
{
	if( modalTop || !name.length() ) {
		return NULL;
	}
	if( stackLocked ) {
		return NULL;
	}

	std::string documentRealname = name[0] == '/' ? name : ( defaultPath.c_str() + name );

	// TODO: keep a max size for the stack.. or search the stack for
	// previous version of the document and push that to top?
	// except stack doesnt have random access operations..

	// grab the previous top
	Document *top = documentStack.size() > 0 ? documentStack.back() : NULL;
	if( top && top->getName() == documentRealname ) {
		// same document, return
		return top;
	}

	// pop all unviewed documents off the stack
	if( top && !top->IsViewed() ) {
		_popDocument( false );
		top = documentStack.size() > 0 ? documentStack.back() : NULL;
	}
	else {
		// if modal, dont hide previous, else hide it
		if( !modal && top )
			top->Hide();
	}

	// cache has reserved a ref for us
	Document *doc = cache.getDocument( documentRealname );
	if( !doc || !doc->getRocketDocument() )
		return NULL;

	// the loading document might have pushed another document onto the stack 
	// in the onload event, pushing ourselves on top of it now is going to fuck up the order
	Document *new_top = documentStack.size() > 0 ? documentStack.back() : 0;
	if( top != new_top ) {
		// the stack has changed in the cache.getDocument call
		return NULL;
	}

	documentStack.push_back( doc );
	modalTop = modal;

	attachMainEventListenerToTop( top );

	// show doc, do stuff.. install eventlisteners?
	if( show ) {
		showStack( true );
	}

	// now check whether we're still on top of stack after the 'show' event
	// as we could have been popped off the stack
	if( doc == new_top ) {
		if( UI_Main::Get()->debugOn() ) {
			Com_Printf("NavigationStack::pushDocument returning %s with refcount %d\n",
					documentRealname.c_str(), doc->getReference() );
		}
	}

	return doc;
}

void NavigationStack::_popDocument(bool focusOnNext)
{
	modalTop = false;

	Document *doc = documentStack.back();
	documentStack.pop_back();
	Document *top = hasDocuments() ? documentStack.back() : NULL;

	doc->Hide();

	if( UI_Main::Get()->debugOn() ) {
		Com_Printf("NavigationStack::popDocument popping %s with refcount %d\n",
				doc->getName().c_str(), doc->getReference() );
	}

	// attach to the next document on stack
	attachMainEventListenerToTop( doc );

	// cache will remove our reference
	cache.purgeDocument( doc );

	// focus on top document
	if( focusOnNext && hasDocuments() ) {
		if( top != documentStack.back() ) {
			// doc->Hide() might have pushed another document onto the top
			return;
		}

		while( top && !top->IsViewed() ) {
			documentStack.pop_back();
			top = documentStack.back();
		}
		if( !modalTop && top != NULL ) {
			top->Show();
		}
	}
}

void NavigationStack::popDocument()
{
	_popDocument( true );
}

void NavigationStack::popAllDocuments(void)
{
	// ensure no documents cripple in, say, onshow even
	// otherwise we can actually loop endlessly
	stackLocked = true;

	while( !documentStack.empty() ) {
		_popDocument( false );
	}
	documentStack.clear();

	stackLocked = false;
}

void NavigationStack::invalidateAssets(void)
{
	cache.invalidateAssets();
}

void NavigationStack::attachMainEventListenerToTop( Document *prev )
{
	if( !hasDocuments() ) {
		return;
	}

	Document *top = documentStack.back();

	// global event listeners, TODO: if we ever change eventlistener to be
	// dynamically instanced, then we cant call GetMainListener every time?
	// only for UI documents!
	Rocket::Core::EventListener *listener = UI_GetMainListener();
	if( prev && prev->getRocketDocument() ) {
		top->getRocketDocument()->RemoveEventListener( "keydown", listener );
		top->getRocketDocument()->RemoveEventListener( "change", listener );
	}

	if( top && top->getRocketDocument() ) {
		top->getRocketDocument()->AddEventListener( "keydown", listener );
		top->getRocketDocument()->AddEventListener( "change", listener );
	}
}

void NavigationStack::markTopAsViewed(void)
{
	Document *modal = NULL;

	// mark the top document as viewed
	// if the top document is modal, temporarily pop if off the stack
	// and then push back

	Document *top = documentStack.back();
	if( modalTop ) {
		modal = top;

		documentStack.pop_back();
		top = documentStack.size() > 0 ? documentStack.back() : NULL;
	}

	if( top ) {
		top->SetViewed();
	}

	if( modalTop ) {
		documentStack.push_back( modal );
	}
}

bool NavigationStack::hasDocuments(void) const
{
	return !documentStack.empty();
}

bool NavigationStack::hasOneDocument(void) const
{
	return documentStack.size() == 1;
}

bool NavigationStack::hasAtLeastTwoDocuments(void) const
{
	return documentStack.size() >= 2;
}

size_t NavigationStack::getStackSize(void) const
{
	return documentStack.size();
}

DocumentCache *NavigationStack::getCache(void)
{
	return &cache;
}

void NavigationStack::setDefaultPath( const std::string &path )
{
	// ensure path begins and ends with slash
	if( !path.length() )
		defaultPath = '/';
	else if( path[0] != '/' )
		defaultPath = '/' + path;
	else
		defaultPath = path;

	if( defaultPath[defaultPath.length()-1] != '/' )
		defaultPath += '/';
}

const std::string &NavigationStack::getDefaultPath( void )
{
	return defaultPath;
}

std::string NavigationStack::getFullpath( const std::string &name )
{
	// if name is absolute, return name
	if( !name.length() || name[0] == '/' )
		return name;

	// prepend with stacks top if any and return
	if( documentStack.size() ) {
		const std::string &s = documentStack.back()->getName();

		// figure out if theres a path element here and prepend name with that
		size_t slash = s.rfind( '/' );
		if( slash != std::string::npos )
			return s.substr( 0, slash ) + name;
	}

	// prepend with default path and return
	return defaultPath + name;
}

// TEMP TEMP
void NavigationStack::showStack(bool show)
{
	if( documentStack.empty() )
		return;		// Warn?

#if 0
	if( modalTop )
	{
		// also show the one below the top
		Document *top = documentStack.back();
		documentStack.pop_back();
		if( documentStack.size() )
			documentStack.back()->Show( show );
		documentStack.push_back( top );
	}
#endif

	documentStack.back()->Show( show, modalTop );
}

// TEMP TEMP
void NavigationStack::hideStack()
{
	showStack(false);
}

// DEBUG
void NavigationStack::printStack()
{
	for(DocumentStack::iterator it = documentStack.begin(); it != documentStack.end(); ++it )
		Com_Printf("  %d %s\n", std::distance( documentStack.begin(), it ), (*it)->getName().c_str() );
}

//==========================================

DocumentLoader::DocumentLoader()
	: isLoading(false), loadedDocument(0)
{
	// register itself to UI_Main

}

DocumentLoader::DocumentLoader(const char *filename)
	: isLoading(false), loadedDocument(0)
{
	// this will set loadedDocument if succeeded
	loadDocument( filename );
}

DocumentLoader::~DocumentLoader()
{

}

// TODO: return UI_Document
Core::ElementDocument *DocumentLoader::loadDocument(const char *path)
{
	// FIXME: use RocketModule
	UI_Main *ui = UI_Main::Get();
	RocketModule *rm = ui->getRocket();
	ASUI::ASInterface *as = ui->getAS();

	isLoading = true;
	currentLoadPath = path;

	// backwards development compatibility TODO: eliminate this system
	ui->setDocumentLoader( this );

	// clear the postponed onload events
	onloads.clear();

	// tell angelscript to start building a new module
	as->startBuilding(path);

	// load the .rml
	loadedDocument = rm->loadDocument(path, /* true */ false);
	if( !loadedDocument )
	{
		// TODO: failsafe instead of this
		// trap::Error( va("DocumentLoader::loadDocument failed to load %s", filename) );
		Com_Printf( "DocumentLoader::loadDocument failed to load %s\n", path);
	}

	// tell angelscript it can compile and link the module
	// has to be called even if document loading fails!
	// also ensure the 1-to-1 match between the build and document names
	as->finishBuilding( loadedDocument ? loadedDocument->GetSourceURL().CString() : NULL );

	// handle postponed onload events (HOWTO handle these in cached documents?)
	if( loadedDocument )
	{
		for( PostponedList::iterator it = onloads.begin(); it != onloads.end(); ++it )
		{
			it->first->ProcessEvent( *it->second );
			it->second->RemoveReference();
		}
	}

	// and clear the events
	onloads.clear();
	isLoading = false;

	// backwards development compatibility TODO: eliminate this system
	ui->setDocumentLoader( 0 );

	return loadedDocument;
}

// get last document
Rocket::Core::ElementDocument *DocumentLoader::getDocument()
{
	return loadedDocument;
}

// TODO: redundant
void DocumentLoader::closeDocument(Rocket::Core::ElementDocument *document)
{
	UI_Main *ui = UI_Main::Get();
	ASUI::ASInterface *as = ui ? ui->getAS() : 0;

	if( as ) {
		// we need to release AS handles referencing libRocket resources
		as->buildReset( document->GetSourceURL().CString() );
	}

	ui->getRocketContext()->UnloadDocument(document);
}

void DocumentLoader::postponeOnload(Rocket::Core::EventListener *listener, Rocket::Core::Event &event)
{
	// use InstanceEvent here, I have no clue what to use as interruptible here
	Rocket::Core::Event *instanced = Rocket::Core::Factory::InstanceEvent( event.GetTargetElement(),
														event.GetType(), *event.GetParameters(), true );
	instanced->SetPhase( event.GetPhase() );

	// DEBUG
	if( UI_Main::Get()->debugOn() ) {
		Com_Printf("Reference count of instanced event %d\n", instanced->GetReferenceCount() );
	}

	onloads.push_back( PostponedEvent( listener, instanced ) );
}

}
