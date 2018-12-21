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

#ifndef ROCKETCOREFONTFACEHANDLE_H
#define ROCKETCOREFONTFACEHANDLE_H

// HACK: Include cstdint here, for some reasons it is enough for the
// whole librocket code.
#include <cstdint>

#include <Rocket/Core/ReferenceCountable.h>
#include <Rocket/Core/UnicodeRange.h>
#include <Rocket/Core/Font.h>
#include <Rocket/Core/FontEffect.h>
#include <Rocket/Core/Geometry.h>
#include <Rocket/Core/String.h>
#include <Rocket/Core/Texture.h>

namespace Rocket {
namespace Core {

/**
	@author Peter Curry
 */

class FontFaceHandle : public ReferenceCountable
{
public:
	FontFaceHandle();
	virtual ~FontFaceHandle();

	/// Initialises the handle so it is able to render text.
	/// @param[in] ft_face The font provider that this handle is rendering.
	/// @param[in] font_handle The handle coming from the font provider
	/// @param[in] charset The comma-separated list of unicode ranges this handle must support.
	/// @param[in] size The size, in points, of the face this handle should render at.
	/// @return True if the handle initialised successfully and is ready for rendering, false if an error occured.
	bool Initialise(FontProviderInterface *font_provider, FontHandle font_handle);

	/// Returns the average advance of all glyphs in this font face.
	/// @return An approximate width of the characters in this font face.
	int GetCharacterWidth() const;

	/// Returns the point size of this font face.
	/// @return The face's point size.
	int GetSize() const;
	/// Returns the pixel height of a lower-case x in this font face.
	/// @return The height of a lower-case x.
	int GetXHeight() const;
	/// Returns the default height between this font face's baselines.
	/// @return The default line height.
	int GetLineHeight() const;

	/// Returns the font's baseline, as a pixel offset from the bottom of the font.
	/// @return The font's baseline.
	int GetBaseline() const;

	/// Returns the width a string will take up if rendered with this handle.
	/// @param[in] string The string to measure.
	/// @param[in] prior_character The optionally-specified character that immediately precedes the string. This may have an impact on the string width due to kerning.
	/// @return The width, in pixels, this string will occupy if rendered with this handle.
	int GetStringWidth(const WString& string, word prior_character = 0);

	/// Generates, if required, the layer configuration for a given array of font effects.
	/// @param[in] font_effects The list of font effects to generate the configuration for.
	/// @return The index to use when generating geometry using this configuration.
	int GenerateLayerConfiguration(FontEffectMap& font_effects);

	/// Generates the geometry required to render a single line of text.
	/// @param[out] geometry An array of geometries to generate the geometry into.
	/// @param[in] string The string to render.
	/// @param[in] position The position of the baseline of the first character to render.
	/// @param[in] colour The colour to render the text.
	/// @return The width, in pixels, of the string geometry.
	int GenerateString(GeometryList& geometry, const WString& string, const Vector2f& position, const Colourb& colour, int layer_configuration = 0) const;
	/// Generates the geometry required to render a line above, below or through a line of text.
	/// @param[out] geometry The geometry to append the newly created geometry into.
	/// @param[in] position The position of the baseline of the lined text.
	/// @param[in] width The width of the string to line.
	/// @param[in] height The height to render the line at.
	/// @param[in] colour The colour to draw the line in.
	void GenerateLine(Geometry* geometry, const Vector2f& position, int width, Font::Line height, const Colourb& colour) const;

protected:
	/// Destroys the handle.
	virtual void OnReferenceDeactivate();

private:
	FontHandle font_handle;
	FontProviderInterface *font_provider;
};

}
}

#endif
