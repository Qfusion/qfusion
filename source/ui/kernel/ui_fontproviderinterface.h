/*
 * ui_fontproviderinterface.h
 *
 */
#pragma once
#ifndef UI_FONTPROVIDERINTERFACE_H_
#define UI_FONTPROVIDERINTERFACE_H_

#include <RmlUi/Core/FontSubsystemInterface.h>
#include "kernel/ui_polyallocator.h"

namespace WSWUI
{

class UI_FontProviderInterface : public Rml::Core::FontSubsystemInterface
{
public:
	UI_FontProviderInterface( Rml::Core::RenderInterface *render_interface );
	virtual ~UI_FontProviderInterface();

	virtual Rml::Core::FontFaceHandle GetFontFaceHandle( const std::string& family, const std::string& charset, Rml::Core::Style::FontStyle style, Rml::Core::Style::FontWeight weight, int size ) override;

	virtual int GetCharacterWidth( Rml::Core::FontFaceHandle ) const override;

	virtual int GetSize( Rml::Core::FontFaceHandle ) const override;

	virtual int GetXHeight( Rml::Core::FontFaceHandle ) const override;

	virtual int GetLineHeight( Rml::Core::FontFaceHandle ) const override;

	virtual int GetBaseline( Rml::Core::FontFaceHandle ) const override;

	virtual float GetUnderline( Rml::Core::FontFaceHandle, float *thickness ) const override;

	virtual int GetStringWidth( Rml::Core::FontFaceHandle, const Rml::Core::WString & string, Rml::Core::word prior_character = 0 ) override;

	virtual int GenerateString( Rml::Core::FontFaceHandle, Rml::Core::GeometryList & geometry, const Rml::Core::WString & string, const Rml::Core::Vector2f & position, const Rml::Core::Colourb & colour, int layer_configuration ) const override;

	Rml::Core::RenderInterface *GetRenderInterface() { return render_interface; }

	static void DrawCharCallback( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader );

private:
	Rml::Core::RenderInterface *render_interface;

	typedef std::map< std::string, Rml::Core::Texture * > TextureMap;

	const struct shader_s *capture_shader_last;
	Rml::Core::GeometryList *capture_geometry;
	Rml::Core::Texture *capture_texture_last;

	TextureMap textures;
};

}

#endif /* UI_FONTPROVIDERINTERFACE_H_ */
