/*
 * UI_RenderInterface.cpp
 *
 *  Created on: 25.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "../cgame/ref.h"
#include "kernel/ui_renderinterface.h"

namespace WSWUI
{

// shortcuts
typedef Rml::Core::TextureHandle TextureHandle;
typedef Rml::Core::Vertex Vertex;
typedef Rml::Core::Vector2f Vector2f;
typedef Rml::Core::Colourb Colourb;
typedef Rml::Core::CompiledGeometryHandle CompiledGeometryHandle;

typedef struct shader_s shader_t;

UI_RenderInterface::UI_RenderInterface( int vidWidth, int vidHeight, float pixelRatio )
	: vid_width( vidWidth ), vid_height( vidHeight ), pixelRatio( pixelRatio ), polyAlloc() {
	pixelsPerInch = basePixelsPerInch * pixelRatio;

	texCounter = 0;

	scissorEnabled = false;
	scissorX = 0;
	scissorY = 0;
	scissorWidth = vid_width;
	scissorHeight = vid_height;

	whiteShader = trap::R_RegisterPic( "$whiteimage" );
}

UI_RenderInterface::~UI_RenderInterface() {
}

Rml::Core::CompiledGeometryHandle UI_RenderInterface::CompileGeometry( Rml::Core::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rml::Core::TextureHandle texture ) {
	poly_t *poly;

	poly = RmlUiGeometry2Poly( false, vertices, num_vertices, indices, num_indices, texture );

	return Rml::Core::CompiledGeometryHandle( poly );
}

void UI_RenderInterface::ReleaseCompiledGeometry( Rml::Core::CompiledGeometryHandle geometry ) {
	if( geometry == 0 ) {
		return;
	}

	poly_t *poly = ( poly_t * )geometry;
	polyAlloc.free( poly );
}

void UI_RenderInterface::RenderCompiledGeometry( Rml::Core::CompiledGeometryHandle geometry, const Rml::Core::Vector2f & translation ) {
	if( geometry == 0 ) {
		return;
	}

	poly_t *poly = ( poly_t * )geometry;

	trap::R_DrawStretchPoly( poly, translation.x, translation.y );
}

void UI_RenderInterface::RenderGeometry( Rml::Core::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rml::Core::TextureHandle texture, const Rml::Core::Vector2f & translation ) {
	poly_t *poly;

	poly = RmlUiGeometry2Poly( true, vertices, num_vertices, indices, num_indices, texture );

	trap::R_DrawStretchPoly( poly, translation.x, translation.y );
}

void UI_RenderInterface::SetScissorRegion( int x, int y, int width, int height ) {
	scissorX = x;
	scissorY = y;
	scissorWidth = width;
	scissorHeight = height;

	if( scissorEnabled ) {
		trap::R_Scissor( x, y, width, height );
	}
}

void UI_RenderInterface::EnableScissorRegion( bool enable ) {
	if( enable ) {
		trap::R_Scissor( scissorX, scissorY, scissorWidth, scissorHeight );
	} else {
		trap::R_ResetScissor();
	}

	scissorEnabled = enable;
}

void UI_RenderInterface::ReleaseTexture( Rml::Core::TextureHandle texture_handle ) {

}

bool UI_RenderInterface::GenerateTexture( Rml::Core::TextureHandle & texture_handle, const Rml::Core::byte *source, const Rml::Core::Vector2i & source_dimensions/*, int source_samples*/ ) {
	shader_t *shader;
	auto name = Rml::Core::CreateString( MAX_QPATH, "ui_raw_%d", texCounter++ );

	shader = trap::R_RegisterRawPic( name.c_str(), source_dimensions.x, source_dimensions.y, (uint8_t*)source, /*source_samples*/4 );
	if( !shader ) {
		Com_Printf( S_COLOR_RED "Warning: RenderInterface couldnt register raw pic %s!\n", name.c_str() );
		return false;
	}

	AddShaderToCache( name );

	texture_handle = TextureHandle( shader );
	return true;
}

bool UI_RenderInterface::LoadTexture( Rml::Core::TextureHandle & texture_handle, Rml::Core::Vector2i & texture_dimensions, const Rml::Core::String & source ) {
	shader_t *shader = NULL;
	Rml::Core::String source2( source );

	if( source2[0] == '/' ) {
		source2.erase( 0, 1 );
	} else if( source2[0] == '?' ) {
		std::string protocol = source2.substr( 1, source2.find( "::" ) - 1 );
		if( protocol == "fonthandle" ) {
			if( sscanf( source2.c_str(), "?fonthandle::%p", (void **)&shader ) != 1 ) {
				Com_Printf( S_COLOR_RED "Warning: RenderInterface couldnt load pic %s!\n", source.c_str() );
				return false;
			}
		}
	}

	if( !shader ) {
		shader = trap::R_RegisterPic( source2.c_str() );
	}

	if( !shader ) {
		Com_Printf( S_COLOR_RED "Warning: RenderInterface couldnt load pic %s!\n", source.c_str() );
		return false;
	}

	trap::R_GetShaderDimensions( shader, &texture_dimensions.x, &texture_dimensions.y );

	if( source2[0] != '?' ) {
		AddShaderToCache( source2 );
	}

	texture_handle = TextureHandle( shader );

	// Com_Printf( "RenderInterface::LoadTexture %s successful (dimensions %dx%d\n", source.CString(), texture_dimensions.x, texture_dimensions.y );

	return true;
}

void UI_RenderInterface::SetTransform( const Rml::Core::Matrix4f* transform ) {
	if( transform == nullptr ) {
		trap::R_SetTransformMatrix( NULL );
		return;
	}

	if( std::is_same<Rml::Core::Matrix4f, Rml::Core::ColumnMajorMatrix4f>::value ) {
		trap::R_SetTransformMatrix( (const float *)transform->data() );
	} else if( std::is_same<Rml::Core::Matrix4f, Rml::Core::RowMajorMatrix4f>::value ) {
		trap::R_SetTransformMatrix( (const float *)transform->Transpose().data() );
	}
}

float UI_RenderInterface::GetPixelsPerInch( void ) {
	return this->pixelsPerInch;
}

poly_t *UI_RenderInterface::RmlUiGeometry2Poly( bool temp, Rml::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::Core::TextureHandle texture ) {
	poly_t *poly;
	int i;

	if( temp ) {
		poly = polyAlloc.get_temp( num_vertices, num_indices );
	} else {
		poly = polyAlloc.alloc( num_vertices, num_indices );
	}

	// copy stuff over
	for( i = 0; i < num_vertices; i++ ) {
		poly->verts[i][0] = vertices[i].position.x;
		poly->verts[i][1] = vertices[i].position.y;
		poly->verts[i][2] = 0;
		poly->verts[i][3] = 1;

		poly->normals[i][0] = 0;
		poly->normals[i][1] = 0;
		poly->normals[i][2] = 0;
		poly->normals[i][3] = 0;

		poly->stcoords[i][0] = vertices[i].tex_coord.x;
		poly->stcoords[i][1] = vertices[i].tex_coord.y;

		poly->colors[i][0] = vertices[i].colour.red;
		poly->colors[i][1] = vertices[i].colour.green;
		poly->colors[i][2] = vertices[i].colour.blue;
		poly->colors[i][3] = vertices[i].colour.alpha;
	}

	for( i = 0; i < num_indices; i++ )
		poly->elems[i] = indices[i];

	poly->shader = ( texture == 0 ? whiteShader : ( shader_t* )texture );

	return poly;
}

void UI_RenderInterface::AddShaderToCache( const Rml::Core::String &shader ) {
	ShaderMap::const_iterator it;

	it = shaderMap.find( shader );
	if( it == shaderMap.end() ) {
		shaderMap[shader] = 1;
	}
}

void UI_RenderInterface::ClearShaderCache( void ) {
	shaderMap.clear();
}

void UI_RenderInterface::TouchAllCachedShaders( void ) {
	ShaderMap::const_iterator it;

	for( it = shaderMap.begin(); it != shaderMap.end(); ++it ) {
		trap::R_RegisterPic( it->first.c_str() );
	}
}

}
