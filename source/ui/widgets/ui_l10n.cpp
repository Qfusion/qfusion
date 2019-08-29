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

#include <RmlUi/Controls.h>
#include <RmlUi/Controls/DataFormatter.h>

namespace WSWUI
{

using namespace Rml::Core;

ElementL10n::ElementL10n( const String &tag ) : Element( tag ), data_formatter( NULL ), num_args( 0 ) {
}

// Called when attributes on the element are changed.
void ElementL10n::OnAttributeChange( const Rml::Core::ElementAttributes& changed_attributes ) {
	Element::OnAttributeChange( changed_attributes );

	bool updateRML = false;

	// Check for formatter change.
	auto it = changed_attributes.find( "formatter" );
	if( it != changed_attributes.end() ) {
		String formatter = GetAttribute< String >( "formatter", "" );

		if( formatter.empty() ) {
			data_formatter = NULL;
			updateRML = true;
		} else {
			data_formatter = Rml::Controls::DataFormatter::GetDataFormatter( formatter );
			if( !data_formatter ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: Unable to find data formatter named '%s', formatting skipped.", formatter.c_str() );
			} else {
				updateRML = true;
			}
		}
	}

	it = changed_attributes.find( "format" );
	if( it != changed_attributes.end() ) {
		num_args = 0;
		format = GetAttribute< String >( "format", "" );

		const char *l10n = trap::L10n_TranslateString( format.c_str() );
		if( l10n ) {
			format = l10n;
		}

		String::size_type n = 0;
		while( true ) {
			n = format.find( "%s", n );
			if( n == String::npos ) {
				break;
			}
			num_args++;
			n += 3;
		}

		updateRML = true;
	}

	std::string argN;
	for( unsigned i = 0; !updateRML && i < num_args; i++ ) {
		Rml::Core::FormatString( argN, 20, "arg%d", i + 1 );

		if( changed_attributes.find( argN ) != changed_attributes.end() ) {
			updateRML = true;
			break;
		}
	}

	if( updateRML ) {
		std::string localized = format;

		switch( num_args ) {
			case 0:
				localized = format;
				break;
			case 1:
				Rml::Core::FormatString( localized, 1024, format.c_str(), GetAttribute< String >( "arg1", "" ).c_str() );
				break;
			case 2:
				Rml::Core::FormatString( localized, 1024, format.c_str(), GetAttribute< String >( "arg1", "" ).c_str(),
										GetAttribute< String >( "arg2", "" ).c_str() );
				break;
			case 3:
				Rml::Core::FormatString( localized, 1024, format.c_str(), GetAttribute< String >( "arg1", "" ).c_str(),
										GetAttribute< String >( "arg2", "" ).c_str(), GetAttribute< String >( "arg3", "" ).c_str() );
				break;
			case 4:
				Rml::Core::FormatString( localized, 1024, format.c_str(), GetAttribute< String >( "arg1", "" ).c_str(),
										GetAttribute< String >( "arg2", "" ).c_str(), GetAttribute< String >( "arg3", "" ).c_str(),
										GetAttribute< String >( "arg4", "" ).c_str() );
				break;
			case 5:
				Rml::Core::FormatString( localized, 1024, format.c_str(), GetAttribute< String >( "arg1", "" ).c_str(),
										GetAttribute< String >( "arg2", "" ).c_str(), GetAttribute< String >( "arg3", "" ).c_str(),
										GetAttribute< String >( "arg4", "" ).c_str(), GetAttribute< String >( "arg5", "" ).c_str() );
				break;
			case 6:
				Rml::Core::FormatString( localized, 1024, format.c_str(), GetAttribute< String >( "arg1", "" ).c_str(),
										GetAttribute< String >( "arg2", "" ).c_str(), GetAttribute< String >( "arg3", "" ).c_str(),
										GetAttribute< String >( "arg4", "" ).c_str(), GetAttribute< String >( "arg5", "" ).c_str(),
										GetAttribute< String >( "arg6", "" ).c_str() );
				break;
			case 7:
				Rml::Core::FormatString( localized, 1024, format.c_str(), GetAttribute< String >( "arg1", "" ).c_str(),
										GetAttribute< String >( "arg2", "" ).c_str(), GetAttribute< String >( "arg3", "" ).c_str(),
										GetAttribute< String >( "arg4", "" ).c_str(), GetAttribute< String >( "arg5", "" ).c_str(),
										GetAttribute< String >( "arg6", "" ).c_str(), GetAttribute< String >( "arg7", "" ).c_str() );
				break;
			default:
			case 8:
				Rml::Core::FormatString( localized, 1024, format.c_str(), GetAttribute< String >( "arg1", "" ).c_str(),
										GetAttribute< String >( "arg2", "" ).c_str(), GetAttribute< String >( "arg3", "" ).c_str(),
										GetAttribute< String >( "arg4", "" ).c_str(), GetAttribute< String >( "arg5", "" ).c_str(),
										GetAttribute< String >( "arg6", "" ).c_str(), GetAttribute< String >( "arg7", "" ).c_str(),
										GetAttribute< String >( "arg8", "" ).c_str() );
				break;
		}

		if( data_formatter ) {
			String formatted = "";
			StringList raw_data;

			raw_data.push_back( localized );

			data_formatter->FormatData( formatted, raw_data );

			SetInnerRML( formatted );
		} else {
			SetInnerRML( localized );
		}
	}
}

//==============================================================

ElementInstancer *GetElementL10nInstancer( void ) {
	return __new__( GenericElementInstancer<ElementL10n> )();
}

}
