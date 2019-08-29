/*
Copyright (C) 2017 Victor Luchits

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
#include "widgets/ui_blur.h"

namespace WSWUI
{

using namespace Rml::Core;

ElementBlur::ElementBlur( const String &tag ) : Element( tag ) {
	SetProperty( "position", "fixed" );
}

void ElementBlur::OnRender( void ) {
	trap::R_BlurScreen();
}

//==============================================================

ElementInstancer *GetElementBlurInstancer( void ) {
	return __new__( GenericElementInstancer<ElementBlur> )();
}

}
