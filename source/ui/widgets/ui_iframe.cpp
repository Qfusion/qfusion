/*
Copyright (C) 2014 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "widgets/ui_widgets.h"
#include "widgets/ui_idiv.h"
#include "formatters/ui_colorcode_formatter.h"

namespace WSWUI
{

using namespace Rml::Core;

class IFrameWidget : public Element, EventListener
{
public:
	IFrameWidget( const String &tag ) : Element( tag ), framed_document( NULL ) {
		SetProperty( "display", "inline-block" );
		SetProperty( "overflow", "auto" );
	}

	virtual ~IFrameWidget() {
		DetachFromOwnerDocument();
	}

	// Called when attributes on the element are changed.
	void OnAttributeChange( const Rml::Core::ElementAttributes& changed_attributes ) {
		Element::OnAttributeChange( changed_attributes );

		// Check for a changed 'src' attribute. If this changes, we need to reload
		// contents of the element.
		auto it = changed_attributes.find( "src" );
		if( it != changed_attributes.end() && GetOwnerDocument() != NULL ) {
			LoadSource();
		}
	}

	virtual void ProcessEvent( Event &ev ) {
		if( framed_document != NULL ) {
			if( ev.GetTargetElement() == GetOwnerDocument() ) {
				if( ev.GetType() == "hide" ) {
					framed_document->Hide();
				} else if( ev.GetType() == "show" ) {
					framed_document->Show();
				}
			}
		}
	}

	virtual void OnChildAdd( Element *child ) {
		if( this == child ) {
			LoadSource();
		}
	}

	virtual void OnChildRemove( Element *child ) {
		if( this == child ) {
			DetachFromOwnerDocument();
		}
	}

private:
	void LoadSource() {
		String source = GetAttribute< String >( "src", "" );

		WSWUI::NavigationStack *stack = NULL;

		if( source.empty() ) {
			SetInnerRML( "" );

			if( framed_document ) {
				stack = framed_document->getStack();
				if( stack ) {
					stack->popAllDocuments();
				}
				framed_document = NULL;
			}
			return;
		}

		RocketModule* rocketModule = UI_Main::Get()->getRocket();
		stack = UI_Main::Get()->createStack( rocketModule->idForContext( GetContext() ) );
		if( stack == NULL ) {
			return;
		}

		framed_document = stack->pushDocument( source.c_str() );
		if( !framed_document ) {
			return;
		}

		ElementDocument *rocket_document = framed_document->getRocketDocument();

		assert( rocket_document->GetParentNode() != nullptr );
		AppendChild( rocket_document->GetParentNode()->RemoveChild( rocket_document ) );

		AttachToOwnerDocument();
	}

	void AttachToOwnerDocument( void ) {
		ElementDocument *doc = GetOwnerDocument();
		if( doc ) {
			doc->AddEventListener( "show", this );
			doc->AddEventListener( "hide", this );
		}
	}

	void DetachFromOwnerDocument( void ) {
		ElementDocument *doc = GetOwnerDocument();
		if( doc ) {
			doc->RemoveEventListener( "show", this );
			doc->RemoveEventListener( "hide", this );
		}
	}

	WSWUI::Document *framed_document;
};

//==============================================================

ElementInstancer *GetIFrameWidgetInstancer( void ) {
	return __new__( GenericElementInstancer<IFrameWidget> )();
}

}
