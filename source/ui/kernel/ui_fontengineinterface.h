/*
 * UI_FontEngineInterface.h
 *
 */
#pragma once
#ifndef UI_FontEngineInterface_H_
#define UI_FontEngineInterface_H_

#include <map>
#include <RmlUi/Core/FontEngineInterface.h>
#include "kernel/ui_polyallocator.h"

namespace WSWUI
{

class UI_FontEngineInterface : public Rml::Core::FontEngineInterface
{
public:
	UI_FontEngineInterface( Rml::Core::RenderInterface *render_interface );
	virtual ~UI_FontEngineInterface();

	virtual Rml::Core::FontFaceHandle GetFontFaceHandle(const Rml::Core::String& family, Rml::Core::Style::FontStyle style, Rml::Core::Style::FontWeight weight, int size) override;

	virtual bool LoadFontFace(const Rml::Core::byte* data, int data_size, const Rml::Core::String& family, Rml::Core::Style::FontStyle style, Rml::Core::Style::FontWeight weight, bool fallback_face) override;

	virtual int GetSize( Rml::Core::FontFaceHandle ) override;

	virtual int GetXHeight( Rml::Core::FontFaceHandle ) override;

	virtual int GetLineHeight( Rml::Core::FontFaceHandle ) override;

	virtual int GetBaseline( Rml::Core::FontFaceHandle ) override;

	virtual float GetUnderline( Rml::Core::FontFaceHandle, float &thickness ) override;

	virtual int GetStringWidth( Rml::Core::FontFaceHandle, const Rml::Core::String & string, Rml::Core::Character prior_character = Rml::Core::Character::Null ) override;

	virtual int GenerateString( Rml::Core::FontFaceHandle, Rml::Core::FontEffectsHandle , const Rml::Core::String & string, const Rml::Core::Vector2f & position, const Rml::Core::Colourb & colour, Rml::Core::GeometryList& geometry ) override;

	Rml::Core::RenderInterface *GetRenderInterface() { return render_interface; }

	static void DrawCharCallback( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader );

private:
	Rml::Core::RenderInterface *render_interface;

	typedef std::map< std::string, Rml::Core::Texture * > TextureMap;

	const struct shader_s *capture_shader_last;
	Rml::Core::GeometryList *capture_geometry;
	Rml::Core::Texture *capture_texture_last;

	static const std::string debugger_font_family, debugger_font_family_alias;

	TextureMap textures;
};

}

#endif /* UI_FontEngineInterface_H_ */
