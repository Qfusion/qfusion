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

#ifndef __UI_DATETIME_FORMATTER_H__
#define __UI_DATETIME_FORMATTER_H__

#include <time.h>
#include <RmlUi/Controls/DataFormatter.h>

namespace WSWUI
{

class DatetimeFormatter : public Rml::Controls::DataFormatter
{
public:
	DatetimeFormatter() : Rml::Controls::DataFormatter( "datetime" ) {}

	// Expects unix time as input. Formats input as "YY/MM/DD hh:mm"
	void FormatData( std::string& formatted_data, const Rml::Core::StringList& raw_data ) {
		if( raw_data[0].empty() ) {
			formatted_data = "";
			return;
		}

		// convert unix time to human-readable date + time
		time_t time = ::atoi( raw_data[0].c_str() );
		struct tm *nt = ::localtime( &time );

		if( nt ) {
			formatted_data = Rml::Core::CreateString( 32,
												   "%02d/%02d/%02d %02d:%02d",
												   nt->tm_year + 1900, nt->tm_mon + 1, nt->tm_mday, nt->tm_hour, nt->tm_min
												   );
		} else {
			formatted_data = "##/##/## ##:##";
		}
	}
};

}

#endif // __UI_DATETIME_FORMATTER_H__
