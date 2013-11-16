#ifndef __UI_ROCKETMODULE_H__
#define __UI_ROCKETMODULE_H__

#include "kernel/ui_systeminterface.h"
#include "kernel/ui_fileinterface.h"
#include "kernel/ui_renderinterface.h"
#include "kernel/ui_keyconverter.h"

namespace WSWUI
{
	// RocketModule contains details and interface to libRocket
	class RocketModule
	{
		// some typical Rocket shortcuts
		typedef Rocket::Core::Element Element;
		typedef Rocket::Core::Event Event;

	public:
		RocketModule( int vidWidth, int vidHeight );
		~RocketModule();

		// post-initialization
		void registerCustoms();

		// pre-shutdown
		void unregisterCustoms();

		// cursor functions
		void loadCursor( const String& rmlCursor );
		void showCursor( void );
		void hideCursor( void );

		// system events
		void mouseMove( int mousex, int mousey );
		void textInput( qwchar c );
		void keyEvent( int key, bool pressed );

		void update( void );
		void render( void );

		Rocket::Core::ElementDocument *loadDocument( const char *filename, bool show=false );
		void closeDocument( Rocket::Core::ElementDocument *doc );

		// called from ElementInstancer after it instances an element, set up default
		// attributes, properties, events etc..
		void registerElementDefaults( Rocket::Core::Element *);

		// GET/SET Submodules
		UI_SystemInterface *getSystemInterface() { return systemInterface; }
		UI_FileInterface *getFileInterface() { return fsInterface; }
		UI_RenderInterface *getRenderInterface() { return renderInterface; }

		// you shouldnt need to use this
		Rocket::Core::Context *getContext() { return context; }

		void clearShaderCache( void );
		void touchAllCachedShaders( void );

	private:
		void preloadFonts( const char *ext );

		void registerElement( const char *tag, Rocket::Core::ElementInstancer* );
		void registerFontEffect( const char *name, Rocket::Core::FontEffectInstancer *);
		void registerDecorator( const char *name, Rocket::Core::DecoratorInstancer *);
		void registerEventInstancer( Rocket::Core::EventInstancer *);
		void registerEventListener( Rocket::Core::EventListenerInstancer *);

		bool rocketInitialized;

		UI_SystemInterface *systemInterface;
		UI_FileInterface *fsInterface;
		UI_RenderInterface *renderInterface;

		Rocket::Core::Context *context;

		// hold this so we can unref these in the end
		std::list<Rocket::Core::ElementInstancer*> elementInstancers;
		Rocket::Core::EventListenerInstancer *scriptEventListenerInstancer;
	};
}

#endif // __UI_ROCKETMODULE_H__
