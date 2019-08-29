/*
 * UI_FontProviderInterface.cpp
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_fontproviderinterface.h"

namespace WSWUI
{

using namespace Rml::Core;

static UI_FontProviderInterface *instance = nullptr;

UI_FontProviderInterface::UI_FontProviderInterface( RenderInterface *render_interface ) :
	render_interface( render_interface ), capture_shader_last( nullptr ), capture_geometry( nullptr ), capture_texture_last( nullptr ) {
	instance = this;
}

UI_FontProviderInterface::~UI_FontProviderInterface() {
	if( instance == this ) {
		instance = nullptr;
	}
}

FontFaceHandle UI_FontProviderInterface::GetFontFaceHandle( const String& family, const String& charset, Style::FontStyle style, Style::FontWeight weight, int size ) {
	int qstyle = QFONT_STYLE_NONE;

	switch( style ) {
		case Style::FontStyle::Italic:
			qstyle |= QFONT_STYLE_ITALIC;
			break;
		default:
			break;
	}

	switch( weight ) {
		case Style::FontWeight::Bold:
			qstyle |= QFONT_STYLE_BOLD;
			break;
		default:
			break;
	}

	if( family.empty() ) {
		return FontFaceHandle( 0 );
	}
	return FontFaceHandle( trap::SCR_RegisterFont( family.c_str(), (qfontstyle_t)qstyle, (unsigned)size ) );
}

int UI_FontProviderInterface::GetCharacterWidth( FontFaceHandle handle ) const {
	return trap::SCR_FontAdvance( (qfontface_s *)( handle ) );
}

int UI_FontProviderInterface::GetSize( FontFaceHandle handle ) const {
	return trap::SCR_FontSize( (qfontface_s *)( handle ) );
}

int UI_FontProviderInterface::GetXHeight( FontFaceHandle handle ) const {
	return trap::SCR_FontXHeight( (qfontface_s *)( handle ) );
}

int UI_FontProviderInterface::GetLineHeight( FontFaceHandle handle ) const {
	return trap::SCR_FontHeight( (qfontface_s *)( handle ) );
}

int UI_FontProviderInterface::GetBaseline( FontFaceHandle handle ) const {
	return trap::SCR_FontHeight( (qfontface_s *)( handle ) );
}

float UI_FontProviderInterface::GetUnderline( FontFaceHandle handle, float *thickness ) const {
	int ithickness;
	float pos = -trap::SCR_FontUnderline( (qfontface_s *)( handle ), &ithickness );
	if (thickness != nullptr)
		*thickness = (float)ithickness;
	return pos;
}

int UI_FontProviderInterface::GetStringWidth( FontFaceHandle handle, const WString& string, word prior_character ) {
	String utf8str = Rml::Core::StringUtilities::ToUTF8( string );
	return trap::SCR_strWidth( utf8str.c_str(), (qfontface_s *)( handle ), 0 );
}

void UI_FontProviderInterface::DrawCharCallback( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader ) {
	if( instance == nullptr ) {
		return;
	}

	GeometryList& geometry = *instance->capture_geometry;
	auto *render_interface = instance->GetRenderInterface();
	bool newgeom = geometry.empty();
	Geometry *g = nullptr;

	if( render_interface == nullptr ) {
		return;
	}

	if( shader != instance->capture_shader_last ) {
		Texture *t;
		std::string key = Rml::Core::CreateString( 64, "%p", shader );

		auto it = instance->textures.find( key );
		if( it != instance->textures.end() ) {
			t = it->second;
		} else {
			String texture_name = "?fonthandle::" + key;

			t = new Texture();
			t->Load( texture_name );
			t->GetDimensions( render_interface );
			instance->textures[key] = t;
		}
		instance->capture_shader_last = shader;
		instance->capture_texture_last = t;
	}

	Texture *texture = instance->capture_texture_last;

	if( !geometry.empty() ) {
		g = &geometry.back();
		if( g->GetVertices().size() >= 1024 || g->GetIndices().size() >= 1536 ) {
			// keep geometry at reasonable size
			g = nullptr;
		} else if( g->GetTexture() != instance->capture_texture_last ) {
			g = nullptr;
			for( auto it = geometry.begin(); it != geometry.end(); ++it ) {
				if( it->GetTexture() == texture ) {
					g = &( *it );
					break;
				}
			}
		}
	}

	if( newgeom || g == nullptr ) {
		geometry.resize( geometry.size() + 1 );
		g = &geometry.back();
	}

	if( g != NULL ) {
		g->SetTexture( texture );

		// Generate the geometry for the character.
		std::vector< Vertex >& character_vertices = g->GetVertices();
		std::vector< int >& character_indices = g->GetIndices();

		character_vertices.resize( character_vertices.size() + 4 );
		character_indices.resize( character_indices.size() + 6 );

		GeometryUtilities::GenerateQuad( &character_vertices[0] + ( character_vertices.size() - 4 ), &character_indices[0] + ( character_indices.size() - 6 ),
										 Vector2f( x, y ), Vector2f( w, h ), Colourb( color[0] * 255, color[1] * 255, color[2] * 255, color[3] * 255 ), Vector2f( s1, t1 ), Vector2f( s2, t2 ),
										 (int)character_vertices.size() - 4 );
	}
}

int UI_FontProviderInterface::GenerateString( FontFaceHandle handle, GeometryList& geometry, const WString& string, const Vector2f& position, const Colourb& colour, int layer_configuration ) const {
	vec4_t colorf;

	if( instance == nullptr ) {
		return 0;
	}

	for( int i = 0; i < 4; i++ ) {
		colorf[i] = colour[i] * ( 1.0 / 255.0 );
	}

	String utf8str = Rml::Core::StringUtilities::ToUTF8( string );

	instance->capture_geometry = &geometry;

	ui_fdrawchar_t pop = trap::SCR_SetDrawCharIntercept( (ui_fdrawchar_t)&UI_FontProviderInterface::DrawCharCallback );

	int string_width = trap::SCR_DrawString( position.x, position.y, 0, utf8str.c_str(), (qfontface_s *)( handle ), colorf );

	trap::SCR_SetDrawCharIntercept( pop );

	return string_width;
}

}
