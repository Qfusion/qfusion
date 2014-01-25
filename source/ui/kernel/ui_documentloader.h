#pragma once
#ifndef __DOCUMENTLOADER_H__
#define __DOCUMENTLOADER_H__

#include <list>
#include <set>
#include <stack>

namespace WSWUI {

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

	// Document that stores references to Rocket's element and Angelscript info
	class Document
	{
	public:
		Document(const std::string &name="", Rocket::Core::ElementDocument *elem=0);
		~Document();

		const std::string &getName() const { return documentName; }
		// addref? nah.. make sure you dont leave a pointer hanging and also check for NULL
		Rocket::Core::ElementDocument *getRocketDocument() { return rocketDocument; }

		// refcount wrappers for rocket's element,
		// USE THESE! instead of direct Add/RemoveReference
		// these will return the refcount after the operation
		int addReference();
		int removeReference();
		int getReference();

		// other rocket wrappers
		void Show(bool show=true, bool modal=false);
		void Hide();
		void Focus();
		void SetViewed(void) { viewed = true; }
		bool IsViewed(void) { return viewed; }
		bool IsModal(void);

	private:
		// this will also be the name for the asmodule!
		std::string documentName;
		Rocket::Core::ElementDocument *rocketDocument;
		bool viewed;
	};

//==================================================

	// DocumentCache, storage for documents
	class DocumentCache
	{
	public:
		DocumentCache();
		~DocumentCache();

		// load or fetch document
		Document *getDocument( const std::string &name );
		// release document
		void purgeDocument( Document *doc );
		// release all documents
		void purgeAllDocuments();

		// full destroy, also destroys css cache
		// (maybe unsafe, invalidates all Document pointers dangling)
		void clearCaches();

		// touch or reset shaders, models, etc
		void invalidateAssets(void);

		// DEBUG
		void printCache();

	private:
		// few nice functions for std::set
		struct DocumentLess {
			bool operator()( const Document *lhs, const Document *rhs ) const {
				return lhs->getName() < rhs->getName();
			}
		};

		struct DocumentCompare {
			bool operator()( const Document *lhs, const Document *rhs ) const {
				return lhs->getName() == rhs->getName();
			}
		};

		// type for the actual storage element
		// we have to use pointers because Document's are explicitly
		// passed around as documents
		typedef std::set<Document*, DocumentLess> DocumentSet;
		// and the storage element itself
		DocumentSet documentSet;

		// release document
		DocumentSet::iterator purgeDocument( DocumentSet::iterator it );
	};

//==================================================

	// NavigationStack, forward/back (i.e push/pop)
	class NavigationStack
	{
	public:
		NavigationStack();
		~NavigationStack();

		// stack operations
		Document *pushDocument(const std::string &name, bool modal=false, bool show = true);
		void popDocument(void);
		void popAllDocuments(void);
		bool hasDocuments(void) const;
		bool hasOneDocument(void) const;
		bool hasAtLeastTwoDocuments(void) const;
		bool isTopModal(void) const { return modalTop; }
		void markTopAsViewed(void);
		DocumentCache *getCache(void);

		void setDefaultPath( const std::string &path );
		const std::string &getDefaultPath( void );

		// this is here for testing, in the future use proper mechanics outside
		// that will push/pop the stack when the UI is hidden
		void showStack(bool show=true);
		void hideStack();
		size_t getStackSize(void) const;

		void invalidateAssets(void);

		// DEBUG
		void printStack();

	private:
		void _popDocument(bool focusOnNext = true);
		void attachMainEventListenerToTop( Document *prev );

		std::string getFullpath( const std::string &name );

		// navstack owns the cache (TODO: or does it?)
		DocumentCache cache;

		// type for the document storage
		// use a list instead of stack with 'back' as top
		typedef std::list<Document*> DocumentStack;
		// actual storage object
		DocumentStack documentStack;

		bool modalTop;
		// locking the stack prevents documents from being pushed into the stack
		bool stackLocked;
		std::string defaultPath;
	};

//==================================================

	// DocumentLoader, functional class that implements document loading
	class DocumentLoader
	{
	public:
		DocumentLoader();
		DocumentLoader(const char *filename);

		~DocumentLoader();

		// cached?
		Rocket::Core::ElementDocument *loadDocument(const char *path);
		// get last document
		Rocket::Core::ElementDocument *getDocument();
		// TODO: redundant
		void closeDocument(Rocket::Core::ElementDocument*);

		// TODO: propagate these from UI_Main
		// set onload event to be called after document loading is done
		void postponeOnload(Rocket::Core::EventListener *listener, Rocket::Core::Event &event);

	private:

		// TODO: proper PostponedEvent that handles reference counting and event instancing!

		// mechanism that calls onload events after all of AS scripts are built
		typedef std::pair<Rocket::Core::EventListener*, Rocket::Core::Event*>
			PostponedEvent;
		typedef std::list<PostponedEvent> PostponedList;

		PostponedList onloads;

		bool isLoading;	// redundant here?
		Rocket::Core::String currentLoadPath;
		Rocket::Core::ElementDocument *loadedDocument;
	};

}

#endif
