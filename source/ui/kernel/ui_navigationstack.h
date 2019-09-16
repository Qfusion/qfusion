#pragma once
#ifndef __NAVIGATIONSTACK_H__
#define __NAVIGATIONSTACK_H__

#include <list>
#include <set>
#include <string>

namespace WSWUI
{
class Document;

//==================================================

// DocumentCache, storage for documents
class DocumentCache
{
public:
	DocumentCache( int contextId );
	~DocumentCache();

	// load or fetch document
	Document *getDocument( const std::string &name, NavigationStack *stack = NULL );
	// release document
	void purgeDocument( Document *doc );
	// release all documents
	void purgeAllDocuments();

	// full destroy, also destroys css cache
	// (maybe unsafe, invalidates all Document pointers dangling)
	void clearCaches();

	// touch or reset shaders, models, etc
	void invalidateAssets( void );

	int getContextId( void ) const { return contextId; };

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

	int contextId;
	DocumentLoader loader;

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
	NavigationStack( int contextId );
	~NavigationStack();

	// stack operations
	Document *pushDocument( const std::string &name, bool modal = false, bool show = true );
	Document *preloadDocument( const std::string &name );
	void popDocument( void );
	void popAllDocuments( void );
	bool hasDocuments( void ) const;
	bool empty( void ) const { return !hasDocuments(); }
	bool hasAtLeastTwoDocuments( void ) const;
	bool isTopModal( void ) const { return modalTop; }
	void markTopAsViewed( void );
	DocumentCache *getCache( void );

	void setDefaultPath( const std::string &path );
	const std::string &getDefaultPath( void );

	void showStack( bool autofocus = false );
	void hideStack();
	size_t getStackSize( void ) const;

	void invalidateAssets( void );
	int getContextId( void ) const { return cache.getContextId(); };

	// DEBUG
	void printStack();

private:
	void _popDocument( bool focusOnNext = true );
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

}

#endif
