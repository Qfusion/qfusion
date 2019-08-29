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

#ifndef __UI_CROSSHAIR_FORMATTER_H__
#define __UI_CROSSHAIR_FORMATTER_H__

#include <RmlUi/Controls/DataFormatter.h>

namespace WSWUI
{

class CrosshairFormatter : public Rml::Controls::DataFormatter
{
public:
	CrosshairFormatter() : Rml::Controls::DataFormatter( "crosshair" ) {}

	// CrosshairDataSource contains the reference to the .tga crosshair images
	// this formatter shows them into an <img> html tag
	void FormatData( String& formatted_data, const Rml::Core::StringList& raw_data ) {
		formatted_data = "";
		for( Rml::Core::StringList::const_iterator it = raw_data.begin(); it != raw_data.end(); ++it )
			formatted_data += " <img src=\"" + ( *it ) + "\" width=\"32\" height=\"32\" />";
	}
};

}

#endif // __UI_CROSSHAIR_FORMATTER_H__
