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

namespace WSWUI {

using namespace Rocket::Core;

class IFrameWidget : public Element, EventListener
{
public:
	IFrameWidget( const String &tag ) : Element( tag ), framed_document( NULL )
	{
		SetProperty( "display", "inline-block" );
		SetProperty( "overflow", "auto" );
	}

	virtual ~IFrameWidget()
	{
		DetachFromOwnerDocument();
	}
	
	// Called when attributes on the element are changed.
	void OnAttributeChange( const Rocket::Core::AttributeNameList& changed_attributes )
	{
		Element::OnAttributeChange(changed_attributes);

		AttributeNameList::const_iterator it;

		// Check for a changed 'src' attribute. If this changes, we need to reload
		// contents of the element.
		it = changed_attributes.find( "src" );
		if( it != changed_attributes.end() && GetOwnerDocument() != NULL ) {
			LoadSource();
		}
	}

	virtual void ProcessEvent( Event &ev )
	{
		if( framed_document != NULL ) {
			if( ev.GetTargetElement() == GetOwnerDocument() ) {
				if( ev.GetType() == "hide" ) {
					framed_document->Hide();
				}
				else if( ev.GetType() == "show" ) {
					framed_document->Show();
				}
			}
		}
	}

	virtual void OnChildAdd( Element *child )
	{
		if( this == child ) {
			LoadSource();
		}
	}

	virtual void OnChildRemove( Element *child )
	{
		if( this == child ) {
			DetachFromOwnerDocument();
		}
	}

private:
	void LoadSource()
	{
		String source = GetAttribute< String >("src", "");

		WSWUI::NavigationStack *stack = NULL;

		if( source.Empty() ) {
			SetInnerRML( "" );

			if( framed_document ) {
				stack = framed_document->getStack();
				//framed_document->RemoveReference();
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

		framed_document = stack->pushDocument( source.CString() );
		if( !framed_document ) {
			return;
		}

		ElementDocument *rocket_document = framed_document->getRocketDocument();

		AppendChild( rocket_document );
		rocket_document->SetProperty( "overflow", "auto" );
		rocket_document->PullToFront();

		AttachToOwnerDocument();
	}

	void AttachToOwnerDocument( void )
	{
		ElementDocument *owner_document = GetOwnerDocument();
		if( owner_document ) {
			owner_document->AddEventListener( "show", this );
			owner_document->AddEventListener( "hide", this );
		}
	}

	void DetachFromOwnerDocument( void )
	{
		ElementDocument *owner_document = GetOwnerDocument();
		if( owner_document ) {
			owner_document->RemoveEventListener( "show", this );
			owner_document->RemoveEventListener( "hide", this );
		}
	}

	WSWUI::Document *framed_document;
};

//==============================================================

ElementInstancer *GetIFrameWidgetInstancer( void )
{
	return __new__( GenericElementInstancer<IFrameWidget> )();
}

}
