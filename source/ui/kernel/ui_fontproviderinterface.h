/*
 * ui_fontproviderinterface.h
 *
 */
#pragma once
#ifndef UI_FONTPROVIDERINTERFACE_H_
#define UI_FONTPROVIDERINTERFACE_H_

#include <Rocket/Core/FontProviderInterface.h>
#include "kernel/ui_polyallocator.h"

namespace WSWUI
{

class UI_FontProviderInterface : public Rocket::Core::FontProviderInterface
{
public:
	UI_FontProviderInterface(Rocket::Core::RenderInterface *render_interface);
	virtual ~UI_FontProviderInterface();

	virtual Rocket::Core::FontHandle GetFontFaceHandle(const String& family, const String& charset, Rocket::Core::Font::Style style, Rocket::Core::Font::Weight weight, int size);

	virtual int GetCharacterWidth(Rocket::Core::FontHandle) const;

	virtual int GetSize(Rocket::Core::FontHandle) const;

	virtual int GetXHeight(Rocket::Core::FontHandle) const;

	virtual int GetLineHeight(Rocket::Core::FontHandle) const;

	virtual int GetBaseline(Rocket::Core::FontHandle) const;

	virtual int GetUnderline(Rocket::Core::FontHandle, int *thickness) const;

	virtual int GetStringWidth(Rocket::Core::FontHandle, const Rocket::Core::WString& string, Rocket::Core::word prior_character = 0);

	virtual int GenerateString(Rocket::Core::FontHandle, Rocket::Core::GeometryList& geometry, const Rocket::Core::WString& string, const Rocket::Core::Vector2f& position, const Rocket::Core::Colourb& colour) const;

	Rocket::Core::RenderInterface *GetRenderInterface() { return render_interface; }

	static void DrawCharCallback( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader );

private:
	Rocket::Core::RenderInterface *render_interface;

	typedef std::map< Rocket::Core::String, Rocket::Core::Texture * > TextureMap;

	const struct shader_s *capture_shader_last;
	Rocket::Core::GeometryList *capture_geometry;
	Rocket::Core::Texture *capture_texture_last;

	TextureMap textures;
};

}

#endif /* UI_FONTPROVIDERINTERFACE_H_ */
