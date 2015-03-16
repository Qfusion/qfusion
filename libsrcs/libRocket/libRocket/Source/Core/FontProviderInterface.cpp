/*
 * This source file is part of libRocket, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://www.librocket.com
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "precompiled.h"

namespace Rocket {
namespace Core {

FontProviderInterface::FontProviderInterface() : ReferenceCountable(0)
{
}

FontProviderInterface::~FontProviderInterface()
{
}

// Called when this interface is released.
void FontProviderInterface::Release()
{
}

void FontProviderInterface::OnReferenceDeactivate()
{
	Release();
}

FontHandle FontProviderInterface::GetFontFaceHandle(const String& ROCKET_UNUSED_PARAMETER(family), 
	const String& ROCKET_UNUSED_PARAMETER(charset), Font::Style ROCKET_UNUSED_PARAMETER(style), 
	Font::Weight ROCKET_UNUSED_PARAMETER(weight), int ROCKET_UNUSED_PARAMETER(size))
{
	ROCKET_UNUSED(family);
	ROCKET_UNUSED(charset);
	ROCKET_UNUSED(style);
	ROCKET_UNUSED(weight);
	ROCKET_UNUSED(size);
	return 0;
}

int FontProviderInterface::GetCharacterWidth(FontHandle) const
{
	return 0;
}

int FontProviderInterface::GetSize(FontHandle) const
{
	return 0;
}

int FontProviderInterface::GetXHeight(FontHandle) const
{
	return 0;
}

int FontProviderInterface::GetLineHeight(FontHandle) const
{
	return 0;
}

int FontProviderInterface::GetBaseline(FontHandle) const
{
	return 0;
}

int FontProviderInterface::GetUnderline(FontHandle, int *) const
{
	return 0;
}

int FontProviderInterface::GetStringWidth(FontHandle, const WString& ROCKET_UNUSED_PARAMETER(string), word ROCKET_UNUSED_PARAMETER(prior_character))
{
	ROCKET_UNUSED(string);
	ROCKET_UNUSED(prior_character);
	return 0;
}

int FontProviderInterface::GenerateString(FontHandle, GeometryList& ROCKET_UNUSED_PARAMETER(geometry), const WString& ROCKET_UNUSED_PARAMETER(string), 
	const Vector2f& ROCKET_UNUSED_PARAMETER(position), const Colourb& ROCKET_UNUSED_PARAMETER(colour)) const
{
	ROCKET_UNUSED(geometry);
	ROCKET_UNUSED(string);
	ROCKET_UNUSED(position);
	ROCKET_UNUSED(colour);
	return 0;
}

}
}
