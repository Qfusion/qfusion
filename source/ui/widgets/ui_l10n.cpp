/*
Copyright (C) 2015 Victor Luchits

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
#include "widgets/ui_l10n.h"

#include <Rocket/Controls.h>
#include <Rocket/Controls/DataFormatter.h>

namespace WSWUI {

using namespace Rocket::Core;

ElementL10n::ElementL10n( const String &tag ) : Element(tag), data_formatter(NULL), num_args(0)
{
}

// Called when attributes on the element are changed.
void ElementL10n::OnAttributeChange( const Rocket::Core::AttributeNameList& changed_attributes )
{
	Element::OnAttributeChange(changed_attributes);

	AttributeNameList::const_iterator it;

	bool updateRML = false;

	// Check for formatter change.
	it = changed_attributes.find( "formatter" );
	if( it != changed_attributes.end() ) {
		String formatter = GetAttribute< String >("formatter", "");

		if( formatter.Empty() ) {
			data_formatter = NULL;
			updateRML = true;
		}
		else {
			data_formatter = Rocket::Controls::DataFormatter::GetDataFormatter( formatter );
			if( !data_formatter ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: Unable to find data formatter named '%s', formatting skipped.", formatter.CString() );
			}
			else {
				updateRML = true;
			}
		}
	}

	it = changed_attributes.find( "format" );
	if( it != changed_attributes.end() ) {
		num_args = 0;
		format = GetAttribute< String >("format", "");

		const char *l10n = trap::L10n_TranslateString( format.CString() );
		if( l10n ) {
			format = l10n;
		}
		
		String::size_type n = 0;
		while( true ) {
			n = format.Find( "%s", n );
			if( n == String::npos ) {
				break;
			}
			num_args++;
			n += 3;
		}

		updateRML = true;
	}

	for( unsigned i = 0; !updateRML && i < num_args; i++ ) {
		it = changed_attributes.find( String( 20, "arg%d", i+1 ) );
		if( it != changed_attributes.end() ) {
			updateRML = true;
		}
	}

	if( updateRML ) {
		String localized = format;

		switch( num_args ) {
			case 0:
				localized = format;
				break;
			case 1:
				localized.FormatString( 1024, format.CString(), GetAttribute< String >("arg1", "").CString() );
				break;
			case 2:
				localized.FormatString( 1024, format.CString(), GetAttribute< String >("arg1", "").CString(),
					GetAttribute< String >("arg2", "").CString() );
				break;
			case 3:
				localized.FormatString( 1024, format.CString(), GetAttribute< String >("arg1", "").CString(),
					GetAttribute< String >("arg2", "").CString(), GetAttribute< String >("arg3", "").CString() );
				break;
			case 4:
				localized.FormatString( 1024, format.CString(), GetAttribute< String >("arg1", "").CString(),
					GetAttribute< String >("arg2", "").CString(), GetAttribute< String >("arg3", "").CString(),
					GetAttribute< String >("arg4", "").CString());
				break;
			case 5:
				localized.FormatString( 1024, format.CString(), GetAttribute< String >("arg1", "").CString(),
					GetAttribute< String >("arg2", "").CString(), GetAttribute< String >("arg3", "").CString(),
					GetAttribute< String >("arg4", "").CString(), GetAttribute< String >("arg5", "").CString());
				break;
			case 6:
				localized.FormatString( 1024, format.CString(), GetAttribute< String >("arg1", "").CString(),
					GetAttribute< String >("arg2", "").CString(), GetAttribute< String >("arg3", "").CString(),
					GetAttribute< String >("arg4", "").CString(), GetAttribute< String >("arg5", "").CString(),
					GetAttribute< String >("arg6", "").CString());
				break;
			case 7:
				localized.FormatString( 1024, format.CString(), GetAttribute< String >("arg1", "").CString(),
					GetAttribute< String >("arg2", "").CString(), GetAttribute< String >("arg3", "").CString(),
					GetAttribute< String >("arg4", "").CString(), GetAttribute< String >("arg5", "").CString(),
					GetAttribute< String >("arg6", "").CString(), GetAttribute< String >("arg7", "").CString());
				break;
			default:
			case 8:
				localized.FormatString( 1024, format.CString(), GetAttribute< String >("arg1", "").CString(),
					GetAttribute< String >("arg2", "").CString(), GetAttribute< String >("arg3", "").CString(),
					GetAttribute< String >("arg4", "").CString(), GetAttribute< String >("arg5", "").CString(),
					GetAttribute< String >("arg6", "").CString(), GetAttribute< String >("arg7", "").CString(),
					GetAttribute< String >("arg8", "").CString());
				break;
		}

		if( data_formatter ) {
			String formatted = "";
			StringList raw_data;

			raw_data.push_back( localized );

			data_formatter->FormatData( formatted, raw_data );

			SetInnerRML( formatted );
		}
		else {
			SetInnerRML( localized );
		}
	}
}

//==============================================================

ElementInstancer *GetElementL10nInstancer( void )
{
	return __new__( GenericElementInstancer<ElementL10n> )();
}

}
