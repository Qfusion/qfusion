/*
Copyright (C) 2013 Victor Luchits

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
#include "ui_parsersound.h"

PropertyParserSound::PropertyParserSound()
{
}

PropertyParserSound::~PropertyParserSound()
{
}

// Called to parse a RCSS string declaration.
bool PropertyParserSound::ParseValue(Rocket::Core::Property& property, 
	const Rocket::Core::String& value, 
	const Rocket::Core::ParameterMap& ROCKET_UNUSED_PARAMETER(parameters)) const
{
	property.value = Rocket::Core::Variant(value);
	property.unit = Rocket::Core::Property::STRING;

	if( !value.Empty() ) {
		// skip the '/' at the start of the path
		trap::S_RegisterSound( value.CString()+1 );
	}

	return true;
}

// Destroys the parser.
void PropertyParserSound::Release()
{
	delete this;
}
