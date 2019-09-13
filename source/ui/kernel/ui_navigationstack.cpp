#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_eventlistener.h"
#include "kernel/ui_documentloader.h"

namespace WSWUI
{

namespace Core = Rml::Core;

//==========================================

// DocumentCache

DocumentCache::DocumentCache( int contextId ) : contextId( contextId ), loader( contextId ) {

}

DocumentCache::~DocumentCache() {

}

// load or fetch document
Document *DocumentCache::getDocument( const std::string &name, NavigationStack *stack ) {
	Document *document = 0;

	// we need to use fake Document for comparison
	Document match( name );
	DocumentSet::iterator it = documentSet.find( &match );

	if( it == documentSet.end() ) {
		// load it up, and keep the reference for the stack
		document = loader.loadDocument( name.c_str(), stack );
		if( !document ) {
			return 0;
		}

		documentSet.insert( document );

		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "DocumentCache::getDocument, fully loaded document %s\n", name.c_str());
		}
	} else {
		document = *it;

		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "DocumentCache::getDocument, found document %s from cache\n", name.c_str());
		}
	}

	return document;
}

// release document
DocumentCache::DocumentSet::iterator DocumentCache::purgeDocument( DocumentSet::iterator it ) {
	Document *doc = *it;
	DocumentSet::iterator next = it;
	++next;

	// unload modal documents as we may actually have many of them
	// during the UI session, keeping all of them in memory seems rather wasteful
	// another reason is that modal document will keep stealing focus
	// from other librocket documents until it is unloaded or "shown" without
	// the modal flag set
	if( doc->IsModal() ) {
		loader.closeDocument( doc );
		documentSet.erase( it );
		doc->setRocketDocument( nullptr );
	}

	return next;
}

// release document
void DocumentCache::purgeDocument( Document *doc ) {
	DocumentSet::iterator it = documentSet.find( doc );
	if( it == documentSet.end() ) {
		Com_Printf( "Warning: DocumentCache::purgeDocument couldn't find document %s\n", doc->getName().c_str() );
		return;
	}

	purgeDocument( it );
}

// release all documents
// TODO: should we clear the whole cache and just leave the reference'd
// ones to float around?
void DocumentCache::purgeAllDocuments() {
	if( UI_Main::Get()->debugOn() ) {
		Com_Printf( "DocumentCache::purgeAllDocument\n" );
	}

	for( DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ) {
		it = purgeDocument( it );
	}

	// DEBUG
	if( UI_Main::Get()->debugOn() ) {
		if( !documentSet.empty() ) {
			Com_Printf( "Warning: DocumentCache::purgeAllDocuments: still have %d documents in the cache\n", (int)documentSet.size() );
			for( DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ++it )
				Com_Printf( "    %s\n", ( *it )->getName().c_str() );
		}
	}
}

// as above but will force destroy all documents
void DocumentCache::clearCaches() {
	if( UI_Main::Get()->debugOn() ) {
		Com_Printf( "DocumentCache::clearCaches\n" );
	}

	// force destroy all documents
	purgeAllDocuments();

	for( DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ++it ) {
		if( ( *it )->getRocketDocument() ) {
			loader.closeDocument( ( *it ) );
			( *it )->setRocketDocument( nullptr );
		}
	}

	documentSet.clear();

	// here we also do this
	Rml::Core::Factory::ClearStyleSheetCache();
	Rml::Core::Factory::ClearTemplateCache();

	// and if in the future Rocket offers more cache-cleaning functions, call 'em
}

// DEBUG
void DocumentCache::printCache() {
	for( DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ++it )
		Com_Printf( "  %s\n", ( *it )->getName().c_str() );
}

// send "invalidate" event to all documents
// elements that reference engine assets (models, shaders, sounds, etc)
// must attach themselves to the event as listeneres to either touch
// the assets or invalidate them
void DocumentCache::invalidateAssets( void ) {
	Rml::Core::Dictionary parameters;
	for( DocumentSet::iterator it = documentSet.begin(); it != documentSet.end(); ++it ) {
		( *it )->getRocketDocument()->DispatchEvent( "invalidate", parameters, true );
	}
}

//==========================================

// NavigationStack

NavigationStack::NavigationStack( int contextId ) : cache( contextId ), modalTop( false ), stackLocked( false ) {
	documentStack.clear();
}

NavigationStack::~NavigationStack() {

}

// stack operations
Document *NavigationStack::pushDocument( const std::string &name, bool modal, bool show ) {
	if( modalTop || !name.length() ) {
		return nullptr;
	}
	if( stackLocked ) {
		return nullptr;
	}

	std::string documentRealname = getFullpath( name );

	// TODO: keep a max size for the stack.. or search the stack for
	// previous version of the document and push that to top?
	// except stack doesnt have random access operations..

	// grab the previous top
	Document *top = !documentStack.empty() ? documentStack.back() : nullptr;
	if( top != nullptr && top->getName() == documentRealname ) {
		// same document, return
		top->setStack( this );
		return top;
	}

	// pop all unviewed documents off the stack
	if( top && !top->IsViewed() ) {
		_popDocument( false );
		top = !documentStack.empty() ? documentStack.back() : nullptr;
	} else {
		// if modal, dont hide previous, else hide it
		if( !modal && top ) {
			top->Hide();
		}
	}

	// cache has reserved a ref for us
	Document *doc = cache.getDocument( documentRealname, this );
	if( doc == nullptr || !doc->getRocketDocument() ) {
		return nullptr;
	}

	doc->setStack( this );

	// the loading document might have pushed another document onto the stack
	// in the onload event, pushing ourselves on top of it now is going to fuck up the order
	Document *new_top = !documentStack.empty() ? documentStack.back() : nullptr;
	if( top != new_top ) {
		// the stack has changed in the cache.getDocument call
		return doc;
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
	if( doc == documentStack.back() ) {
		doc->FocusFirstTabElement();

		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "NavigationStack::pushDocument returning %s\n",
						documentRealname.c_str() );
		}
	}

	return doc;
}

Document *NavigationStack::preloadDocument( const std::string &name ) {
	std::string documentRealname = getFullpath( name );

	Document *doc = cache.getDocument( documentRealname );
	if( doc == nullptr || !doc->getRocketDocument() ) {
		return nullptr;
	}

	return doc;
}

void NavigationStack::_popDocument( bool focusOnNext ) {
	modalTop = false;

	Document *doc = documentStack.back();
	documentStack.pop_back();
	doc->setStack( nullptr );
	Document *top = hasDocuments() ? documentStack.back() : nullptr;

	doc->Hide();

	if( UI_Main::Get()->debugOn() ) {
		Com_Printf( "NavigationStack::popDocument popping %s\n",
					doc->getName().c_str() );
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
			top->setStack( nullptr );
			documentStack.pop_back();
			top = documentStack.back();
		}
		if( !modalTop && top != nullptr ) {
			top->Show();
		}
	}
}

void NavigationStack::popDocument() {
	_popDocument( true );
}

void NavigationStack::popAllDocuments( void ) {
	// ensure no documents cripple in, say, onshow even
	// otherwise we can actually loop endlessly
	stackLocked = true;

	while( !documentStack.empty() ) {
		_popDocument( false );
	}
	documentStack.clear();

	stackLocked = false;
}

void NavigationStack::invalidateAssets( void ) {
	cache.invalidateAssets();
}

void NavigationStack::attachMainEventListenerToTop( Document *prev ) {
	if( !hasDocuments() ) {
		return;
	}

	Document *top = documentStack.back();
	if( top == nullptr ) {
		return;
	}

	// global event listeners, TODO: if we ever change eventlistener to be
	// dynamically instanced, then we cant call GetMainListener every time?
	// only for UI documents!
	Rml::Core::EventListener *listener = UI_GetMainListener();

	if( prev && prev->getRocketDocument() ) {
		top->getRocketDocument()->RemoveEventListener( "keydown", listener );
		top->getRocketDocument()->RemoveEventListener( "change", listener );
	}

	if( top->getRocketDocument() ) {
		top->getRocketDocument()->AddEventListener( "keydown", listener );
		top->getRocketDocument()->AddEventListener( "change", listener );
	}
}

void NavigationStack::markTopAsViewed( void ) {
	Document *modal = nullptr;

	// mark the top document as viewed
	// if the top document is modal, temporarily pop if off the stack
	// and then push back

	if( documentStack.empty() ) {
		return;
	}

	Document *top = documentStack.back();
	if( modalTop ) {
		modal = top;

		documentStack.pop_back();
		top = !documentStack.empty() ? documentStack.back() : nullptr;
	}

	if( top ) {
		top->SetViewed();
	}

	if( modalTop ) {
		documentStack.push_back( modal );
	}
}

bool NavigationStack::hasDocuments( void ) const {
	return !documentStack.empty();
}

bool NavigationStack::hasAtLeastTwoDocuments( void ) const {
	return documentStack.size() >= 2;
}

size_t NavigationStack::getStackSize( void ) const {
	return documentStack.size();
}

DocumentCache *NavigationStack::getCache( void ) {
	return &cache;
}

void NavigationStack::setDefaultPath( const std::string &path ) {
	// ensure path begins and ends with slash
	if( !path.length() ) {
		defaultPath = '/';
	} else if( path[0] != '/' ) {
		defaultPath = '/' + path;
	} else {
		defaultPath = path;
	}

	if( defaultPath[defaultPath.length() - 1] != '/' ) {
		defaultPath += '/';
	}
}

const std::string &NavigationStack::getDefaultPath( void ) {
	return defaultPath;
}

std::string NavigationStack::getFullpath( const std::string &name ) {
	// if name is absolute, return name
	if( name.empty() || name[0] == '/' ) {
		return name;
	}

	// prepend with default path and return
	return defaultPath + name;
}

// TEMP TEMP
void NavigationStack::showStack( bool show ) {
	if( documentStack.empty() ) {
		return;
	}

#if 0
	if( modalTop ) {
		// also show the one below the top
		Document *top = documentStack.back();
		documentStack.pop_back();
		if( !documentStack.empty() ) {
			documentStack.back()->Show( show );
		}
		documentStack.push_back( top );
	}
#endif

	documentStack.back()->Show( modalTop );
}

// TEMP TEMP
void NavigationStack::hideStack() {
	showStack( false );
}

// DEBUG
void NavigationStack::printStack() {
	for( DocumentStack::iterator it = documentStack.begin(); it != documentStack.end(); ++it )
		Com_Printf( "  %d %s\n", (int)std::distance( documentStack.begin(), it ), ( *it )->getName().c_str() );
}

}
