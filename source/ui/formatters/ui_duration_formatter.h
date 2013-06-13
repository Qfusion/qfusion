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

#ifndef __UI_DURATION_FORMATTER_H__
#define __UI_DURATION_FORMATTER_H__

#include <time.h>
#include <Rocket/Controls/DataFormatter.h>

namespace WSWUI
{

class DurationFormatter : public Rocket::Controls::DataFormatter
{
public:
	DurationFormatter() : Rocket::Controls::DataFormatter("duration") {}

	// Expects number of seconds as input. Formats input as "hh:mm:ss"
	void FormatData( Rocket::Core::String& formatted_data, const Rocket::Core::StringList& raw_data )
	{
		if( raw_data[0].Empty() ) {
			formatted_data = "";
			return;
		}

		int i_value = ::atoi( raw_data[0].CString() );
		formatted_data = Rocket::Core::String( 32,
			"%02d:%02d:%02d", i_value/3600, i_value/60, i_value%60
		);
	}
};

}

#endif // __UI_DURATION_FORMATTER_H__
