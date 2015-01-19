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
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

#include "as/asui.h"
#include "as/asui_local.h"

namespace ASUI {

static const asstring_t *L10n_TranslateString( const asstring_t &input )
{
	const char *translation;

	translation = trap::L10n_TranslateString( input.buffer );
	if( !translation ) {
		translation = input.buffer;
	}
	return ASSTR( translation );
}

static const asstring_t *L10n_GetUserLanguage( void )
{
	return ASSTR( trap::L10n_GetUserLanguage() );
}

void PrebindL10n( ASInterface *as )
{
	(void)as;
}

void BindL10n( ASInterface *as )
{
	ASBind::Global( as->getEngine() )
		.function( &L10n_TranslateString, "TranslateString" )
		.function( &L10n_TranslateString, "_T" )
		.function( &L10n_GetUserLanguage, "GetUserLanguage" )
	;
}

}
