/*
 * UI_EventListener.cpp
 *
 *  Created on: 27.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_eventlistener.h"
#include <RmlUi/Core/Input.h>
#include <RmlUi/Controls.h>

namespace WSWUI
{

static const std::string SOUND_HOVER = "sound-hover";
static const std::string SOUND_CLICK = "sound-click";

using namespace Rml::Core;

BaseEventListener::BaseEventListener() {
}

BaseEventListener::~BaseEventListener() {
}

void BaseEventListener::ProcessEvent( Event &event ) {
	if( event.GetPhase() != Rml::Core::EventPhase::Target ) {
		return;
	}

	Element *target = event.GetTargetElement();

	/* ch : CSS sound properties are handled here */
	if( event.GetType() == "mouseover" ) {
		StartTargetPropertySound( target, SOUND_HOVER );
	} else if( event.GetType() == "click" ) {
		StartTargetPropertySound( target, SOUND_CLICK );
	}
}

void BaseEventListener::StartTargetPropertySound( Element *target, const String &property ) {
	String sound = target->GetProperty<String>( property );
	if( sound.empty() ) {
		return;
	}

	// check relative url, and add documents path
	if( sound[0] != '/' ) {
		ElementDocument *doc = target->GetOwnerDocument();
		if( doc ) {
			URL documentURL( doc->GetSourceURL() );

			URL soundURL( sound );
			soundURL.PrefixPath( documentURL.GetPath() );

			sound = soundURL.GetPathedFileName();
		}
	}

	trap::S_StartLocalSound( trap::S_RegisterSound( sound.c_str() + 1 ), 0, 1.0 );
}

static BaseEventListener ui_baseEventListener;

Rml::Core::EventListener * GetBaseEventListener( void ) {
	return &ui_baseEventListener;
}

//===================================================

class UI_MainListener : public EventListener
{
public:
	virtual void ProcessEvent( Event &event ) {
		if( UI_Main::Get()->debugOn() ) {
			Com_Printf( "EventListener: Event %s, target %s, phase %i\n",
						event.GetType().c_str(),
						event.GetTargetElement()->GetTagName().c_str(),
						event.GetPhase() );
		}

		if( event.GetType() == "keydown" &&
			( event.GetPhase() ==Rml::Core::EventPhase::Target || event.GetPhase() == Rml::Core::EventPhase::Bubble ) ) {
			int key = event.GetParameter<int>( "key_identifier", 0 );
			ElementDocument *document = event.GetTargetElement()->GetOwnerDocument();
			WSWUI::Document *ui_document = static_cast<WSWUI::Document *>( document->GetScriptObject() );
			WSWUI::NavigationStack *stack = ui_document ? ui_document->getStack() : NULL;

			if( key == Input::KI_ESCAPE ) {
				if( stack ) {
					if( stack->isTopModal() ) {
						// pop the top document
						stack->popDocument();
					} else if( stack->getContextId() == UI_CONTEXT_MAIN ) {
						// hide all documents
						UI_Main::Get()->showUI( false );
					}
				}
				event.StopPropagation();
			} else if( key == Rml::Core::Input::KI_BROWSER_BACK || key == Rml::Core::Input::KI_BACK ) {
				// act as history.back()
				if( stack && stack->hasAtLeastTwoDocuments() ) {
					stack->popDocument();
					event.StopPropagation();
				}
			}
		} else if( event.GetType() == "change" && ( event.GetPhase() == Rml::Core::EventPhase::Bubble ) ) {
			bool linebreak = event.GetParameter<int>( "linebreak", 0 ) != 0;
			if( linebreak ) {
				// form submission
				String inputType;
				Element *target = event.GetTargetElement();
				Rml::Controls::ElementFormControl *input = dynamic_cast<Rml::Controls::ElementFormControl *>( target );

				if( event.GetPhase() != Rml::Core::EventPhase::Bubble ) {
					return;
				}
				if( input == NULL ) {
					// not a form control
					return;
				}
				if( input->IsDisabled() ) {
					// skip disabled inputs
					return;
				}
				if( !input->IsSubmitted() ) {
					// this input field isn't submitted with the form
					return;
				}

				inputType = input->GetAttribute<String>( "type", "" );
				if( inputType != "text" && inputType != "password" ) {
					// not a text field
					return;
				}

				// find the parent form element
				Element *parent = target->GetParentNode();
				Rml::Controls::ElementForm *form = NULL;
				while( parent ) {
					form = dynamic_cast<Rml::Controls::ElementForm *>( parent );
					if( form != NULL ) {
						// not a form, go up the tree
						break;
					}
					parent = parent->GetParentNode();
				}

				if( form == NULL ) {
					if( UI_Main::Get()->debugOn() ) {
						Com_Printf( "EventListener: input element outside the form, what should I do?\n" );
					}
					return;
				}

				// find the submit element
				Element *submit = NULL;

				ElementList controls;
				parent->GetElementsByTagName( controls, "input" );
				for( size_t i = 0; i < controls.size(); i++ ) {
					Rml::Controls::ElementFormControl *control =
						dynamic_cast< Rml::Controls::ElementFormControl* >( controls[i] );

					if( !control ) {
						continue;
					}
					if( control->IsDisabled() ) {
						// skip disabled inputs
						continue;
					}

					String controlType = control->GetAttribute<String>( "type", "" );
					if( controlType != "submit" ) {
						// not a text field
						continue;
					}

					submit = controls[i];
					break;
				}

				if( submit == NULL ) {
					if( UI_Main::Get()->debugOn() ) {
						Com_Printf( "EventListener: form with no submit element, what should I do?\n" );
					}
					return;
				}

				// finally submit the form
				form->Submit( submit->GetAttribute< Rml::Core::String >( "name", "" ), submit->GetAttribute< Rml::Core::String >( "value", "" ) );
			}
		}
	}
};

static UI_MainListener ui_mainListener;

EventListener *UI_GetMainListener( void ) {
	return &ui_mainListener;
}

//===================================================

class UI_SoftKeyboardListener : public EventListener
{
public:
	virtual void ProcessEvent( Event &event ) {
		if( event.GetPhase() != Rml::Core::EventPhase::Target ) {
			return;
		}

		Rml::Controls::ElementFormControl *input =
			dynamic_cast< Rml::Controls::ElementFormControl * >( event.GetTargetElement() );
		if( !input || input->IsDisabled() ) {
			return;
		}

		String inputType = input->GetAttribute< String >( "type", "" );
		if( ( inputType != "text" ) && ( inputType != "password" ) &&
			!dynamic_cast< Rml::Controls::ElementFormControlTextArea * >( input ) ) {
			return;
		}

		trap::IN_ShowSoftKeyboard( ( event.GetType() == "click" ) ? true : false );
	}
};


static UI_SoftKeyboardListener ui_softKeyboardListener;

EventListener *UI_GetSoftKeyboardListener( void ) {
	return &ui_softKeyboardListener;
}

}
