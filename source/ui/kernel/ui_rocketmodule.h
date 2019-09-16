#ifndef __UI_ROCKETMODULE_H__
#define __UI_ROCKETMODULE_H__

#include "kernel/ui_systeminterface.h"
#include "kernel/ui_fileinterface.h"
#include "kernel/ui_renderinterface.h"
#include "kernel/ui_keyconverter.h"
#include "kernel/ui_fontproviderinterface.h"

namespace WSWUI
{
// RocketModule contains details and interface to libRocket
class RocketModule : public Rml::Core::Plugin
{
	// some typical Rocket shortcuts
	typedef Rml::Core::Element Element;
	typedef Rml::Core::Event Event;

public:
	RocketModule( int vidWidth, int vidHeight, float pixelRatio );
	~RocketModule();

	// post-initialization
	void registerCustoms();

	// pre-shutdown
	void unregisterCustoms();

	// system events
	void mouseMove( int contextId, int mousex, int mousey );
	bool mouseHover( int contextId );
	void textInput( int contextId, wchar_t c );
	void keyEvent( int contextId, int key, bool pressed );
	bool touchEvent( int contextId, int id, touchevent_t type, int x, int y );
	bool isTouchDown( int contextId, int id );
	void cancelTouches( int contextId );

	void update( void );
	void render( int contextId );

	Rml::Core::ElementDocument *loadDocument( int contextId, const char *filename, void *script_object = NULL );
	void closeDocument( Rml::Core::ElementDocument *doc );

	// called from ElementInstancer after it instances an element, set up default
	// attributes, properties, events etc..
	void registerElementDefaults( Rml::Core::Element * );

	// GET/SET Submodules
	UI_SystemInterface *getSystemInterface() { return systemInterface; }
	UI_FileInterface *getFileInterface() { return fsInterface; }
	UI_RenderInterface *getRenderInterface() { return renderInterface; }

	void clearShaderCache( void );
	void touchAllCachedShaders( void );

	int idForContext( Rml::Core::Context *context );

	/// Called when the plugin is registered to determine
	/// which of the above event types the plugin is interested in
	virtual int GetEventClasses() override;
	/// Called when a document is successfully loaded from file or instanced, initialised and added
	/// to its context. This is called before the document's 'load' event.
	virtual void OnDocumentLoad( Rml::Core::ElementDocument *document ) override;
	/// Called when a document is unloaded from its context. This is called after the document's
	/// 'unload' event.
	virtual void OnDocumentUnload( Rml::Core::ElementDocument *document ) override;

   private:
	void registerElement( const char *tag, Rml::Core::ElementInstancer* );
	void registerFontEffect( const char *name, Rml::Core::FontEffectInstancer * );
	void registerDecorator( const char *name, Rml::Core::DecoratorInstancer * );
	void registerEventInstancer( Rml::Core::EventInstancer * );
	void registerEventListener( Rml::Core::EventListenerInstancer * );

	// translates UI_CONTEXT_ constants to rocket contexts and vise versa
	Rml::Core::Context *contextForId( int contextId );

	bool rocketInitialized;

	struct contextTouch {
		int id;
		Rml::Core::Vector2f origin;
		int y;
		bool scroll;
	};
	contextTouch contextsTouch[UI_NUM_CONTEXTS];

	UI_SystemInterface *systemInterface;
	UI_FileInterface *fsInterface;
	UI_RenderInterface *renderInterface;
	UI_FontProviderInterface *fontProviderInterface;

	Rml::Core::Context *contextMain;
	Rml::Core::Context *contextQuick;

	// hold this so we can unref these in the end
	std::list<Rml::Core::ElementInstancer*> elementInstancers;
	Rml::Core::EventListenerInstancer *scriptEventListenerInstancer;
};
}

#endif // __UI_ROCKETMODULE_H__
