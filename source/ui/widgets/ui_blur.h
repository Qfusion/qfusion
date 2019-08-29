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

#ifndef __UI_BLUR_H__
#define __UI_BLUR_H__

#include "kernel/ui_utils.h"
#include "kernel/ui_common.h"
#include <RmlUi/Core/Element.h>

namespace WSWUI
{
class ElementBlur : public Rml::Core::Element
{
public:
	ElementBlur( const Rml::Core::String& tag );
	virtual ~ElementBlur() {}
	virtual void OnRender();
};
}

#endif // __UI_BLUR_H__
