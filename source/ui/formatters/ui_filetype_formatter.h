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

#ifndef __UI_FILETYPE_FORMATTER_H__
#define __UI_FILETYPE_FORMATTER_H__

#include <Rocket/Controls/DataFormatter.h>

namespace WSWUI
{

class FiletypeFormatter : public Rocket::Controls::DataFormatter
{
public:
	FiletypeFormatter() : Rocket::Controls::DataFormatter("filetype") {}

	// Encloses filename into a <filetype> tag and sets its class to file extension (e.g. "mp3") .
	// Directories have their own tag <dirtype> with the only possible class "back" for ".." directories.
	void FormatData( Rocket::Core::String& formatted_data, const Rocket::Core::StringList& raw_data )
	{
		const Rocket::Core::String &name = raw_data[0];
		if( name == ".." ) {
			formatted_data = "<dirtype class=\"back\">..</dirtype>";
		}
		else {
			Rocket::Core::String::size_type nameLength = name.Length();
			Rocket::Core::String::size_type delimPos = name.RFind( "/" );
			if ( delimPos != name.npos && delimPos + 1 == nameLength ) {
				formatted_data = String("<dirtype>") + name.Substring( 0, nameLength - 1 ) + "</dirtype>";
			}
			else {
				delimPos = name.RFind( "." );
				if( delimPos != name.npos ) {
					formatted_data = String("<filetype class=\"") + name.Substring( delimPos + 1 ) + "\">" + name + "</filetype>";
				}
				else {
					formatted_data = String("<filetype>") + name + "</filetype>";
				}
			}
		}
	}
};

}

#endif // __UI_FILETYPE_FORMATTER_H__
