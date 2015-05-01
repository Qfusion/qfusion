/*
Copyright (C) 2012 Victor Luchits

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

#ifndef __UI_SERVERFLAGS_FORMATTER_H__
#define __UI_SERVERFLAGS_FORMATTER_H__

#include <time.h>
#include <Rocket/Controls/DataFormatter.h>

namespace WSWUI
{

class ServerFlagsFormatter : public Rocket::Controls::DataFormatter
{
public:
	ServerFlagsFormatter() : Rocket::Controls::DataFormatter("serverflags") {}

	void FormatData( Rocket::Core::String& formatted_data, const Rocket::Core::StringList& raw_data )
	{
		formatted_data = "";

		const String &flags = raw_data[0];
		if( flags.Empty() ) {
			return;
		}

		size_t len = flags.Length();
		for( size_t i = 0; i < len; i++ ) {
			switch( flags[i] ) {
				case ' ':
					formatted_data += "&nbsp;";
					break;
				case 'P':
					formatted_data += "<span style=\"color: #FFFFFF;\">#</span>";
					break;
				case 'p':
					formatted_data += "<span style=\"color: #00000000;\">#</span>";
					break;
				case 'F':
					formatted_data += "<span style=\"color: #FAD85C;\">\xE2\x98\x85</span>";
					break;
				case 'f':
					formatted_data += "<span style=\"color: #FFFFFF;\">\xE2\x98\x85</span>";
					break;
				default:
					formatted_data += flags[i];
					break;
			}
		}
	}
};

}

#endif // __UI_SERVERFLAGS_FORMATTER_H__
