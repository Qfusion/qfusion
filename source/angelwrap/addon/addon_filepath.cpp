/*
Copyright (C) 2020 Victor Luchits

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

#include <stdarg.h>
#include "../qas_precompiled.h"
#include "addon_string.h"
#include "addon_filepath.h"

namespace FilePath
{

static asstring_t *QAS_StripExtension( asstring_t *fn )
{
	asstring_t *string = objectString_FactoryBuffer( fn->buffer, fn->len );
	COM_StripExtension( string->buffer );
	string->len = strlen( string->buffer );
	return string;
}

}

using namespace FilePath;

void PreRegisterFilePathAddon( asIScriptEngine *engine ) {
}

void RegisterFilePathAddon( asIScriptEngine *engine )
{
	int r;

	r = engine->SetDefaultNamespace( "FilePath" ); assert( r >= 0 );

	r = engine->RegisterGlobalFunction( "String @StripExtension( const String &in )", asFUNCTION( QAS_StripExtension ), asCALL_CDECL );
	assert( r >= 0 );

	r = engine->SetDefaultNamespace( "" ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}
