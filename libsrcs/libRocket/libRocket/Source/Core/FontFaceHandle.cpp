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
#include <Rocket/Core/FontFaceHandle.h>
#include "../../Include/Rocket/Core.h"

namespace Rocket {
namespace Core {

class FontEffectSort
{
public:
	bool operator()(const FontEffect* lhs, const FontEffect* rhs)
	{
		return lhs->GetZIndex() < rhs->GetZIndex();
	}
};

FontFaceHandle::FontFaceHandle()
{
	font_provider = NULL;
	font_handle = 0;
}

FontFaceHandle::~FontFaceHandle()
{
	if (font_provider)
		font_provider->RemoveReference();
	font_provider = NULL;
}

// Initialises the handle so it is able to render text.
bool FontFaceHandle::Initialise(FontProviderInterface *_font_provider, FontHandle _font_handle)
{
	if (font_provider)
		font_provider->RemoveReference();
	font_provider = _font_provider;
	font_handle = _font_handle;
	font_provider->AddReference();
	return true;
}

// Returns the point size of this font face.
int FontFaceHandle::GetSize() const
{
	return font_provider->GetSize(font_handle);
}

// Returns the average advance of all glyphs in this font face.
int FontFaceHandle::GetCharacterWidth() const
{
	return font_provider->GetCharacterWidth(font_handle);
}

// Returns the pixel height of a lower-case x in this font face.
int FontFaceHandle::GetXHeight() const
{
	return font_provider->GetXHeight(font_handle);
}

// Returns the default height between this font face's baselines.
int FontFaceHandle::GetLineHeight() const
{
	return font_provider->GetLineHeight(font_handle);
}

// Returns the font's baseline.
int FontFaceHandle::GetBaseline() const
{
	return font_provider->GetBaseline(font_handle);
}

// Returns the width a string will take up if rendered with this handle.
int FontFaceHandle::GetStringWidth(const WString& string, word prior_character)
{
	return font_provider->GetStringWidth(font_handle, string, prior_character);
}

// Generates, if required, the layer configuration for a given array of font effects.
int FontFaceHandle::GenerateLayerConfiguration(FontEffectMap& font_effects)
{
	if (font_effects.empty())
		return 0;
	return 0;
}

// Generates the geometry required to render a single line of text.
int FontFaceHandle::GenerateString(GeometryList& geometry, const WString& string, const Vector2f& position, const Colourb& colour, int ROCKET_UNUSED_PARAMETER(layer_configuration_index)) const
{
	ROCKET_UNUSED(layer_configuration_index);
	return font_provider->GenerateString(font_handle, geometry, string, position, colour);
}

// Generates the geometry required to render a line above, below or through a line of text.
void FontFaceHandle:: GenerateLine(Geometry* geometry, const Vector2f& position, int width, Font::Line height, const Colourb& colour) const
{
	int underline_position = 0;
	int underline_thickness = 0;

	underline_position = font_provider->GetUnderline(font_handle, &underline_thickness);

	std::vector< Vertex >& line_vertices = geometry->GetVertices();
	std::vector< int >& line_indices = geometry->GetIndices();

	float offset;
	switch (height)
	{
		case Font::UNDERLINE:			offset = (float)-underline_position; break;
		case Font::OVERLINE:			// where to place? offset = -line_height - underline_position; break;
		case Font::STRIKE_THROUGH:		// where to place? offset = -line_height * 0.5f; break;
		default:						return;
	}

	line_vertices.resize(line_vertices.size() + 4);
	line_indices.resize(line_indices.size() + 6);
	GeometryUtilities::GenerateQuad(&line_vertices[0] + (line_vertices.size() - 4), &line_indices[0] + (line_indices.size() - 6), Vector2f(position.x, position.y + offset), 
		Vector2f((float) width, (float)underline_thickness), colour, (int)line_vertices.size() - 4);
}

// Destroys the handle.
void FontFaceHandle::OnReferenceDeactivate()
{
	delete this;
}

}
}
