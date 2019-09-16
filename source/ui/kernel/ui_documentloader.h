#pragma once
#ifndef __DOCUMENTLOADER_H__
#define __DOCUMENTLOADER_H__

namespace WSWUI
{

/*
    Document loading, TODO: (consider this as documentation now (DONE))

    Wrap up the document itself, i think we need to store
    the rocket document itself and then information about
    the angelscript code.
    All UI documents are cached until all of them are called for purging.
    In the future when we implement HUD's with this library, they'll
    be created with flag which tells that they have to be explicitly
    purged (they arent cached the same way).

    To enable caching we have to keep stuff in memory AFAIK, so we dont
    need to hassle with re-attaching events to elements and scriptsections
    etc.. If angelscript works out well in this case, we can compile each
    document and its scripts into unique angelscript module (named after the
    .rml document). Creating an unique context for each document would be
    overkill, although the next 'simple' option (The hard option would be
    to cache bytecode's of compile-units and relink everything upon
    invokation of cached document).

    We can separate DocumentLoader that is purely functional class that hides
    the intrinsicacies of loading rocket documents, compiling embedded scripts,
    etc.. It just returns pure Document which can be used by both the caching
    (UI) and non-caching (HUD) mechanisms.

    Then we have DocumentStack which also does the caching and it has a
    webbrowser-type back/forward document-stack.

    This allows us to separate the instance that HUD loading depends on.

    ---

    So rip off this DocumentLoader here and re-implement as above.

    Also, not related in here but also create RocketModule or smth that takes care
    of initializing and shutting down libRocket among with other such tasks.
*/

//==================================================

class NavigationStack;

// Document that stores references to Rocket's element and Angelscript info
class Document
{
public:
	Document( const std::string &name = "", NavigationStack *stack = NULL );
	~Document();

	const std::string &getName() const { return documentName; }
	// addref? nah.. make sure you dont leave a pointer hanging and also check for NULL
	void setRocketDocument( Rml::Core::ElementDocument *elem ) { rocketDocument = elem; }
	Rml::Core::ElementDocument *getRocketDocument() { return rocketDocument; }

	// other rocket wrappers
	void Show( bool modal = false, bool autofocus = false );
	void Hide();
	void SetViewed( void ) { viewed = true; }
	bool IsViewed( void ) const { return viewed; }
	bool IsModal( void );
	NavigationStack *getStack() const { return stack; }
	void setStack( NavigationStack *s ) { this->stack = s; }

private:
	// this will also be the name for the asmodule!
	std::string documentName;
	Rml::Core::ElementDocument *rocketDocument;
	NavigationStack *stack;
	bool viewed;
};

//==================================================

// DocumentLoader, functional class that implements document loading
class DocumentLoader
{
public:
	DocumentLoader( int contextId );
	~DocumentLoader();

	// cached?
	Document *loadDocument( const char *path, NavigationStack *stack = NULL );
	// TODO: redundant
	void closeDocument( Document* );

private:
	int contextId;

	// TODO: proper PostponedEvent that handles reference counting and event instancing!

	// mechanism that calls onload events after all of AS scripts are built
	typedef std::pair<Rml::Core::EventListener*, Rml::Core::Event*>
		PostponedEvent;
	typedef std::list<PostponedEvent> PostponedList;

	PostponedList onloads;
};

}

#endif
