/*
Copyright (C) 2011 Victor Luchits

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
#include "widgets/ui_field.h"

#include <RmlUi/Controls.h>
#include <RmlUi/Controls/DataFormatter.h>

namespace WSWUI
{

using namespace Rml::Core;

ElementField::ElementField( const String &tag ) : Element( tag ), data_formatter( NULL ) {
}

// Called when attributes on the element are changed.
void ElementField::OnAttributeChange( const Rml::Core::ElementAttributes& changed_attributes ) {
	Element::OnAttributeChange( changed_attributes );

	bool formatterChanged = false;

	// Check for formatter change.
	auto it = changed_attributes.find( "formatter" );
	if( it != changed_attributes.end() ) {
		String formatter = GetAttribute< String >( "formatter", "" );

		if( formatter.empty() ) {
			data_formatter = NULL;
			formatterChanged = true;
		} else {
			data_formatter = Rml::Controls::DataFormatter::GetDataFormatter( formatter );
			if( !data_formatter ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: Unable to find data formatter named '%s', formatting skipped.", formatter.c_str() );
			} else {
				formatterChanged = true;
			}
		}
	}

	// Check for value change. Apply formatter.
	it = changed_attributes.find( "value" );
	if( it != changed_attributes.end() || formatterChanged ) {
		String value = GetAttribute< String >( "value", "" );

		if( data_formatter ) {
			String formatted = "";
			StringList raw_data;

			raw_data.push_back( value );

			data_formatter->FormatData( formatted, raw_data );

			SetInnerRML( formatted );
		} else {
			SetInnerRML( value );
		}
	}
}

//==============================================================

ElementInstancer *GetElementFieldInstancer( void ) {
	return __new__( GenericElementInstancer<ElementField> )();
}

}
