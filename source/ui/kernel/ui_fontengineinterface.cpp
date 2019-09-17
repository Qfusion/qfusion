/*
 * UI_FontEngineInterface.cpp
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_fontengineinterface.h"

namespace WSWUI
{

using namespace Rml::Core;

static UI_FontEngineInterface *instance = nullptr;

const std::string UI_FontEngineInterface::debugger_font_family = "rmlui-debugger-font";
const std::string UI_FontEngineInterface::debugger_font_family_alias = DEFAULT_SYSTEM_FONT_FAMILY;

UI_FontEngineInterface::UI_FontEngineInterface( RenderInterface *render_interface ) :
	render_interface( render_interface ), capture_shader_last( nullptr ), capture_geometry( nullptr ), capture_texture_last( nullptr ) {
	instance = this;
}

UI_FontEngineInterface::~UI_FontEngineInterface() {
	if( instance == this ) {
		instance = nullptr;
	}
}

FontFaceHandle UI_FontEngineInterface::GetFontFaceHandle( const String& family, Style::FontStyle style, Style::FontWeight weight, int size ) {
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

	const std::string *aliased_family = &family;
	if( family == debugger_font_family ) {
		aliased_family = &debugger_font_family_alias;
	}

	if( aliased_family->empty() ) {
		return FontFaceHandle( 0 );
	}
	return FontFaceHandle( trap::SCR_RegisterFont( aliased_family->c_str(), (qfontstyle_t)qstyle, (unsigned)size ) );
}

bool UI_FontEngineInterface::LoadFontFace( const byte* data, int data_size, const String& family, Style::FontStyle style, Style::FontWeight weight, bool fallback_face ) {
	if( family == debugger_font_family ) {
		return true;
	}
	return false;
}

int UI_FontEngineInterface::GetSize( FontFaceHandle handle ) {
	return trap::SCR_FontSize( (qfontface_s *)( handle ) );
}

int UI_FontEngineInterface::GetXHeight( FontFaceHandle handle ) {
	return trap::SCR_FontXHeight( (qfontface_s *)( handle ) );
}

int UI_FontEngineInterface::GetLineHeight( FontFaceHandle handle ) {
	return trap::SCR_FontHeight( (qfontface_s *)( handle ) );
}

int UI_FontEngineInterface::GetBaseline( FontFaceHandle handle ) {
	return trap::SCR_FontHeight( (qfontface_s *)( handle ) );
}

float UI_FontEngineInterface::GetUnderline( FontFaceHandle handle, float &thickness ) {
	int ithickness;
	float pos = -trap::SCR_FontUnderline( (qfontface_s *)( handle ), &ithickness );
	thickness = (float)ithickness;
	return pos;
}

int UI_FontEngineInterface::GetStringWidth( FontFaceHandle handle, const String& string, Rml::Core::Character prior_character) {
	return trap::SCR_strWidth( string.c_str(), (qfontface_s *)( handle ), 0 );
}

void UI_FontEngineInterface::DrawCharCallback( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader ) {
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
			t->Set( texture_name );
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

int UI_FontEngineInterface::GenerateString( FontFaceHandle handle, FontEffectsHandle font_effects_handle, const String& string, const Vector2f& position, const Colourb& colour, GeometryList& geometry ) {
	vec4_t colorf;

	if( instance == nullptr ) {
		return 0;
	}

	for( int i = 0; i < 4; i++ ) {
		colorf[i] = colour[i] * ( 1.0 / 255.0 );
	}

	instance->capture_geometry = &geometry;

	ui_fdrawchar_t pop = trap::SCR_SetDrawCharIntercept( (ui_fdrawchar_t)&UI_FontEngineInterface::DrawCharCallback );

	int string_width = trap::SCR_DrawString( position.x, position.y, 0, string.c_str(), (qfontface_s *)( handle ), colorf );

	trap::SCR_SetDrawCharIntercept( pop );

	return string_width;
}

}
