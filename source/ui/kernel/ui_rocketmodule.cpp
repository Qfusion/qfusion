#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_keyconverter.h"
#include "kernel/ui_rocketmodule.h"
#include "kernel/ui_eventlistener.h"

#include "widgets/ui_widgets.h"
#include "as/asui.h"

#include "decorators/ui_decorators.h"

#include "../gameshared/q_keycodes.h"

#include "parsers/ui_parsersound.h"

#include <RmlUi/Core/FontEffectInstancer.h>
#include <RmlUi/Controls.h>
#include <RmlUi/Debugger/Debugger.h>

namespace WSWUI
{

//==================================================

class MyEventInstancer : public Rml::Core::EventInstancer
{
	typedef Rml::Core::Event Event;
	typedef Rml::Core::Element Element;
	typedef Rml::Core::Dictionary Dictionary;

public:
	MyEventInstancer() : Rml::Core::EventInstancer() {}
	~MyEventInstancer() {}

	// rocket overrides
	virtual Rml::Core::EventPtr InstanceEvent( Element *target, Rml::Core::EventId id, const std::string & type, const Dictionary &parameters, bool interruptible ) override {
		return Rml::Core::EventPtr(__new__( Rml::Core::Event )( target, id, type, parameters, interruptible ));
	}

	virtual void ReleaseEvent( Event *event ) override {
		__delete__( event );
	}

	virtual void Release() override {
		__delete__( this );
	}
};

//==================================================

RocketModule::RocketModule( int vidWidth, int vidHeight, float pixelRatio )
	: rocketInitialized( false ),

	// pointers
	systemInterface( nullptr ), fsInterface( nullptr ), renderInterface( nullptr ),
	contextMain( nullptr ), contextQuick( nullptr ), 
	scriptEventListenerInstancer( nullptr ) {
	Rml::Core::String contextName = trap::Cvar_String( "gamename" );

	renderInterface = __new__( UI_RenderInterface )( vidWidth, vidHeight, pixelRatio );
	Rml::Core::SetRenderInterface( renderInterface );
	systemInterface = __new__( UI_SystemInterface )();
	Rml::Core::SetSystemInterface( systemInterface );
	fsInterface = __new__( UI_FileInterface )();
	Rml::Core::SetFileInterface( fsInterface );

	fontProviderInterface = __new__( UI_FontProviderInterface )( renderInterface );
	Rml::Core::SetFontSubsystemInterface( fontProviderInterface );

	rocketInitialized = Rml::Core::Initialise();
	if( !rocketInitialized ) {
		throw std::runtime_error( "UI: Rml::Core::Initialise failed" );
	}

	Rml::Core::RegisterPlugin( this );

	// initialize the controls plugin
	Rml::Controls::Initialise();

	// Create our contexts
	contextMain = Rml::Core::CreateContext( contextName, Vector2i( vidWidth, vidHeight ) );
	contextMain->SetDensityIndependentPixelRatio( pixelRatio );

	contextQuick = Rml::Core::CreateContext( contextName + "_quick", Vector2i( vidWidth, vidHeight ) );
	contextQuick->SetDensityIndependentPixelRatio( pixelRatio );

	memset( contextsTouch, -1, sizeof( *contextsTouch ) * UI_NUM_CONTEXTS );

	Rml::Debugger::Initialise( contextMain );
}

RocketModule::~RocketModule() {
	contextMain = 0;
	contextQuick = 0;

	if( rocketInitialized ) {
		rocketInitialized = false;
		Rml::Core::Shutdown();
	}

	__SAFE_DELETE_NULLIFY( fontProviderInterface );
	__SAFE_DELETE_NULLIFY( fsInterface );
	__SAFE_DELETE_NULLIFY( systemInterface );
	__SAFE_DELETE_NULLIFY( renderInterface );
}

//==================================================

void RocketModule::mouseMove( int contextId, int mousex, int mousey ) {
	auto *context = contextForId( contextId );
	context->ProcessMouseMove( mousex, mousey, KeyConverter::getModifiers() );
}

bool RocketModule::mouseHover( int contextId ) {
	auto *context = contextForId( contextId );
	Element *element = context->GetHoverElement();
	if( !element || element->GetTagName() == "body" ) {
		return false;
	}
	return true;
}

void RocketModule::textInput( int contextId, wchar_t c ) {
	auto *context = contextForId( contextId );
	if( c >= ' ' ) {
		context->ProcessTextInput( c );
	}
}

void RocketModule::keyEvent( int contextId, int key, bool pressed ) {
	// DEBUG
#if 0
	if( key >= 32 && key <= 126 ) {
		Com_Printf( "**KEYEVENT CHAR %c\n", key & 0xff );
	} else {
		Com_Printf( "**KEYEVENT KEY %d\n", key );
	}
#endif

	if( key == K_MOUSE1DBLCLK ) {
		return; // Rocket handles double click internally

	}
	auto *context = contextForId( contextId );
	Element *element = context->GetFocusElement();

	int mod = KeyConverter::getModifiers();

	// send the blur event, to the current focused element, when ESC key is pressed
	if( ( key == K_ESCAPE ) && element ) {
		element->Blur();
	}

	if( element && ( element->GetTagName() == "keyselect" ) ) {
		if( pressed ) {
			Rml::Core::Dictionary parameters;
			parameters["key"] = key;
			element->DispatchEvent( "keyselect", parameters );
		}
	} else if( key >= K_MOUSE1 && key <= K_MOUSE8 ) {   // warsow sends mousebuttons as keys
		if( pressed ) {
			context->ProcessMouseButtonDown( key - K_MOUSE1, mod );
		} else {
			context->ProcessMouseButtonUp( key - K_MOUSE1, mod );
		}
	} else if( key == K_MWHEELDOWN ) {   // and ditto for wheel
		context->ProcessMouseWheel( 1, mod );
	} else if( key == K_MWHEELUP ) {
		context->ProcessMouseWheel( -1, mod );
	} else if( key == K_F11 ) {
		if( pressed ) {
			Rml::Debugger::SetVisible( !Rml::Debugger::IsVisible() );
		}
	} else {
		if( ( key == K_A_BUTTON ) || ( key == K_DPAD_CENTER ) ) {
			if( pressed ) {
				context->ProcessMouseButtonDown( 0, mod );
			} else {
				context->ProcessMouseButtonUp( 0, mod );
			}
		} else {
			int rkey = KeyConverter::toRocketKey( key );

			if( key == K_B_BUTTON ) {
				rkey = Rml::Core::Input::KI_ESCAPE;
				if( element ) {
					element->Blur();
				}
			}

			if( rkey != 0 ) {
				if( pressed ) {
					context->ProcessKeyDown( Rml::Core::Input::KeyIdentifier( rkey ), mod );
				} else {
					context->ProcessKeyUp( Rml::Core::Input::KeyIdentifier( rkey ), mod );
				}
			}
		}
	}
}

bool RocketModule::touchEvent( int contextId, int id, touchevent_t type, int x, int y ) {
#if 0
	auto &contextTouch = contextsTouch[contextId];
	auto *context = contextForId( contextId );

	if( ( type == TOUCH_DOWN ) && ( contextTouch.id < 0 ) ) {
		if( contextId == UI_CONTEXT_OVERLAY ) {
			Rml::Core::Vector2f position( ( float )x, ( float )y );
			Element *element = context->GetElementAtPoint( position );
			if( !element || element->GetTagName() == "body" ) {
				return false;
			}
		}

		contextTouch.id = id;
		contextTouch.origin.x = x;
		contextTouch.origin.y = y;
		contextTouch.y = y;
		contextTouch.scroll = false;
	}

	if( id != contextTouch.id ) {
		return false;
	}

	UI_Main::Get()->mouseMove( contextId, 0, x, y, true, false );

	if( type == TOUCH_DOWN ) {
		context->ProcessMouseButtonDown( 0, KeyConverter::getModifiers() );
	} else {
		int delta = contextTouch.y - y;
		if( delta ) {
			if( !contextTouch.scroll ) {
				int threshold = 32 * renderInterface->GetPixelRatio();
				if( abs( delta ) > threshold ) {
					contextTouch.scroll = true;
					contextTouch.y += ( ( delta < 0 ) ? threshold : -threshold );
					delta = contextTouch.y - y;
				}
			}

			if( contextTouch.scroll ) {
				Element *focusElement = context->GetFocusElement();
				if( !focusElement || ( focusElement->GetTagName() != "keyselect" ) ) {
					Element *element;
					for( element = context->GetElementAtPoint( contextTouch.origin ); element; element = element->GetParentNode() ) {
						if( element->GetTagName() == "scrollbarvertical" ) {
							break;
						}

						int overflow = element->GetProperty< int >( "overflow-y" );
						if( ( overflow != Rml::Core::OVERFLOW_AUTO ) && ( overflow != Rml::Core::OVERFLOW_SCROLL ) ) {
							continue;
						}

						int scrollTop = element->GetScrollTop();
						if( ( ( delta < 0 ) && ( scrollTop > 0 ) ) ||
							( ( delta > 0 ) && ( element->GetScrollHeight() > scrollTop + element->GetClientHeight() ) ) ) {
							element->SetScrollTop( element->GetScrollTop() + delta );
							break;
						}
					}
				}
				contextTouch.y = y;
			}
		}

		if( type == TOUCH_UP ) {
			cancelTouches( contextId );
		}
	}
#endif
	return true;
}

bool RocketModule::isTouchDown( int contextId, int id ) {
	return contextsTouch[contextId].id == id;
}

void RocketModule::cancelTouches( int contextId ) {
	auto &contextTouch = contextsTouch[contextId];

	if( contextTouch.id < 0 ) {
		return;
	}

	auto *context = contextForId( contextId );

	contextTouch.id = -1;
	context->ProcessMouseButtonUp( 0, KeyConverter::getModifiers() );
	UI_Main::Get()->mouseMove( contextId, 0, 0, 0, true, false );
}

//==================================================

Rml::Core::ElementDocument *RocketModule::loadDocument( int contextId, const char *filename, bool show, void *script_object ) {
	auto *context = contextForId( contextId );
	ASUI::UI_ScriptDocument *document = dynamic_cast<ASUI::UI_ScriptDocument *>( context->LoadDocument( filename ) );
	if( !document ) {
		return NULL;
	}

	document->SetScriptObject( script_object );

	if( show ) {
		// load documents with autofocus disabled
		document->Show( Rml::Core::FocusFlag::FocusDocument );

		// optional element specific eventlisteners here

		// only for UI documents! FIXME: we are already doing this in NavigationStack
		Rml::Core::EventListener *listener = UI_GetMainListener();
		document->AddEventListener( "keydown", listener );
		document->AddEventListener( "change", listener );
	}

	return document;
}

void RocketModule::closeDocument( Rml::Core::ElementDocument *doc ) {
	doc->Close();
}

int RocketModule::GetEventClasses() {
	return Rml::Core::Plugin::EVT_DOCUMENT;
}

void RocketModule::OnDocumentLoad( Rml::Core::ElementDocument *document ) {
	ASUI::UI_ScriptDocument *ui_document = dynamic_cast<ASUI::UI_ScriptDocument *>( document );
	if( ui_document != nullptr ) {
		ui_document->BuildScripts();
		ui_document->UpdateDocument();
	}
}

void RocketModule::OnDocumentUnload( Rml::Core::ElementDocument *document ) {
	ASUI::UI_ScriptDocument *ui_document = dynamic_cast<ASUI::UI_ScriptDocument *>( document );
	if( ui_document != nullptr ) {
		ui_document->DestroyScripts();
	}
}

//==================================================

void RocketModule::registerElementDefaults( Rml::Core::Element *element ) {
	// add these as they pile up in BaseEventListener
	element->AddEventListener( "mouseover", GetBaseEventListener() );
	element->AddEventListener( "click", GetBaseEventListener() );
}

void RocketModule::registerElement( const char *tag, Rml::Core::ElementInstancer *instancer ) {
	Rml::Core::Factory::RegisterElementInstancer( tag, instancer );
	elementInstancers.push_back( instancer );
}

void RocketModule::registerFontEffect( const char *name, Rml::Core::FontEffectInstancer *instancer ) {
	Rml::Core::Factory::RegisterFontEffectInstancer( name, instancer );
}

void RocketModule::registerDecorator( const char *name, Rml::Core::DecoratorInstancer *instancer ) {
	Rml::Core::Factory::RegisterDecoratorInstancer( name, instancer );
}

void RocketModule::registerEventInstancer( Rml::Core::EventInstancer *instancer ) {
	Rml::Core::Factory::RegisterEventInstancer( instancer );
}

void RocketModule::registerEventListener( Rml::Core::EventListenerInstancer *instancer ) {
	Rml::Core::Factory::RegisterEventListenerInstancer( instancer );
}

Rml::Core::Context *RocketModule::contextForId( int contextId ) {
	switch( contextId ) {
		case UI_CONTEXT_MAIN:
			return contextMain;
		case UI_CONTEXT_OVERLAY:
			return contextQuick;
		default:
			assert( contextId != UI_CONTEXT_MAIN && contextId != UI_CONTEXT_OVERLAY );
			return NULL;
	}
}

int RocketModule::idForContext( Rml::Core::Context *context ) {
	if( context == contextMain ) {
		return UI_CONTEXT_MAIN;
	}
	if( context == contextQuick ) {
		return UI_CONTEXT_OVERLAY;
	}
	return UI_NUM_CONTEXTS;
}

void RocketModule::update( void ) {
	ASUI::GarbageCollectEventListenersFunctions( scriptEventListenerInstancer );

	contextQuick->Update();
	contextMain->Update();
}

void RocketModule::render( int contextId ) {
	contextForId( contextId )->Render();
}

void RocketModule::registerCustoms() {
	//
	// ELEMENTS
	registerElement( "*", __new__( GenericElementInstancer<Element> )() );

	// Main document that implements <script> tags
	registerElement( "body", ASUI::GetScriptDocumentInstancer() );

	// Soft keyboard listener
	registerElement( "input",
					 __new__( GenericElementInstancerSoftKeyboard<Rml::Controls::ElementFormControlInput> )() );
	registerElement( "textarea",
					 __new__( GenericElementInstancerSoftKeyboard<Rml::Controls::ElementFormControlTextArea> )() );

	// other widgets
	registerElement( "keyselect", GetKeySelectInstancer() );
	registerElement( "a", GetAnchorWidgetInstancer() );
	registerElement( "optionsform", GetOptionsFormInstancer() );
	registerElement( "levelshot", GetLevelShotInstancer() );
	registerElement( "datagrid", GetSelectableDataGridInstancer() );
	registerElement( "dataspinner", GetDataSpinnerInstancer() );
	registerElement( "modelview", GetModelviewInstancer() );
	registerElement( "worldview", GetWorldviewInstancer() );
	registerElement( "colorselector", GetColorSelectorInstancer() );
	registerElement( "color", GetColorBlockInstancer() );
	registerElement( "idiv", GetInlineDivInstancer() );
	registerElement( "img", GetImageWidgetInstancer() );
	registerElement( "field", GetElementFieldInstancer() );
	registerElement( "video", GetVideoInstancer() );
	registerElement( "iframe", GetIFrameWidgetInstancer() );
	registerElement( "l10n", GetElementL10nInstancer() );
	registerElement( "blur", GetElementBlurInstancer() );

	//
	// EVENTS
	registerEventInstancer( __new__( MyEventInstancer )() );

	//
	// EVENT LISTENERS

	// inline script events
	scriptEventListenerInstancer = ASUI::GetScriptEventListenerInstancer();

	registerEventListener( scriptEventListenerInstancer );

	//
	// FONT EFFECTS

	//
	// DECORATORS
	registerDecorator( "gradient", GetGradientDecoratorInstancer() );
	registerDecorator( "svg", GetSVGDecoratorInstancer() );

	//
	// GLOBAL CUSTOM PROPERTIES

	Rml::Core::StyleSheetSpecification::RegisterProperty( "background-music", "", false ).AddParser( "string" );

	Rml::Core::StyleSheetSpecification::RegisterParser( "sound", new PropertyParserSound() );

	Rml::Core::StyleSheetSpecification::RegisterProperty( "sound-hover", "", false )
	.AddParser( "sound" );
	Rml::Core::StyleSheetSpecification::RegisterProperty( "sound-click", "", false )
	.AddParser( "sound" );
}

void RocketModule::unregisterCustoms() {
	if( scriptEventListenerInstancer ) {
		ASUI::ReleaseScriptEventListenersFunctions( scriptEventListenerInstancer );
		scriptEventListenerInstancer = NULL;
	}
}

//==================================================

void RocketModule::clearShaderCache( void ) {
	renderInterface->ClearShaderCache();
}

void RocketModule::touchAllCachedShaders( void ) {
	renderInterface->TouchAllCachedShaders();
}

}
