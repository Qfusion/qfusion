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
typedef Rocket::Core::TextureHandle TextureHandle;
typedef Rocket::Core::Vertex Vertex;
typedef Rocket::Core::Vector2f Vector2f;
typedef Rocket::Core::Colourb Colourb;
typedef Rocket::Core::CompiledGeometryHandle CompiledGeometryHandle;

typedef struct shader_s shader_t;

UI_RenderInterface::UI_RenderInterface( int vidWidth, int vidHeight, float pixelRatio )
	: vid_width( vidWidth ), vid_height( vidHeight ), polyAlloc(), pixelRatio( pixelRatio ) {
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
	this->RemoveReference();
}

Rocket::Core::CompiledGeometryHandle UI_RenderInterface::CompileGeometry( Rocket::Core::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rocket::Core::TextureHandle texture ) {
	poly_t *poly;

	poly = RocketGeometry2Poly( false, vertices, num_vertices, indices, num_indices, texture );

	return Rocket::Core::CompiledGeometryHandle( poly );
}

void UI_RenderInterface::ReleaseCompiledGeometry( Rocket::Core::CompiledGeometryHandle geometry ) {
	if( geometry == 0 ) {
		return;
	}

	poly_t *poly = ( poly_t * )geometry;
	polyAlloc.free( poly );
}

void UI_RenderInterface::RenderCompiledGeometry( Rocket::Core::CompiledGeometryHandle geometry, const Rocket::Core::Vector2f & translation ) {
	if( geometry == 0 ) {
		return;
	}

	poly_t *poly = ( poly_t * )geometry;

	trap::R_DrawStretchPoly( poly, translation.x, translation.y );
}

void UI_RenderInterface::RenderGeometry( Rocket::Core::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rocket::Core::TextureHandle texture, const Rocket::Core::Vector2f & translation ) {
	poly_t *poly;

	poly = RocketGeometry2Poly( true, vertices, num_vertices, indices, num_indices, texture );

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

void UI_RenderInterface::ReleaseTexture( Rocket::Core::TextureHandle texture_handle ) {

}

bool UI_RenderInterface::GenerateTexture( Rocket::Core::TextureHandle & texture_handle, const Rocket::Core::byte *source, const Rocket::Core::Vector2i & source_dimensions, int source_samples ) {
	shader_t *shader;
	Rocket::Core::String name( MAX_QPATH, "ui_raw_%d", texCounter++ );

	// Com_Printf("RenderInterface::GenerateTexture: going to register %s %dx%d\n", name.CString(), source_dimensions.x, source_dimensions.y );
	shader = trap::R_RegisterRawPic( name.CString(), source_dimensions.x, source_dimensions.y, (uint8_t*)source, source_samples );
	if( !shader ) {
		Com_Printf( S_COLOR_RED "Warning: RenderInterface couldnt register raw pic %s!\n", name.CString() );
		return false;
	}

	// Com_Printf( "RenderInterface::GenerateTexture %s successful\n", name.CString() );

	AddShaderToCache( name );

	texture_handle = TextureHandle( shader );
	return true;
}

bool UI_RenderInterface::LoadTexture( Rocket::Core::TextureHandle & texture_handle, Rocket::Core::Vector2i & texture_dimensions, const Rocket::Core::String & source ) {
	shader_t *shader = NULL;
	Rocket::Core::String source2( source );

	if( source2[0] == '/' ) {
		source2.Erase( 0, 1 );
	} else if( source2[0] == '?' ) {
		String protocol = source2.Substring( 1, source2.Find( "::" ) - 1 );
		if( protocol == "fonthandle" ) {
			if( sscanf( source2.CString(), "?fonthandle::%p", (void **)&shader ) != 1 ) {
				Com_Printf( S_COLOR_RED "Warning: RenderInterface couldnt load pic %s!\n", source.CString() );
				return false;
			}
		}
	}

	if( !shader ) {
		shader = trap::R_RegisterPic( source2.CString() );
	}

	if( !shader ) {
		Com_Printf( S_COLOR_RED "Warning: RenderInterface couldnt load pic %s!\n", source.CString() );
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

void UI_RenderInterface::PushTransform( bool projection, const Rocket::Core::RowMajorMatrix4f& transform ) {
	trap::R_PushTransformMatrix( projection, (const float *)transform.Transpose().data() );
}

void UI_RenderInterface::PushTransform( bool projection, const Rocket::Core::ColumnMajorMatrix4f& transform ) {
	trap::R_PushTransformMatrix( projection, (const float *)transform.data() );
}

void UI_RenderInterface::PopTransform( bool projection, const Rocket::Core::Matrix4f& ROCKET_UNUSED_PARAMETER( transform ) ) {
	ROCKET_UNUSED( transform );
	trap::R_PopTransformMatrix( projection );
}

float UI_RenderInterface::GetPixelsPerInch( void ) {
	return this->pixelsPerInch;
}

poly_t *UI_RenderInterface::RocketGeometry2Poly( bool temp, Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture ) {
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

void UI_RenderInterface::AddShaderToCache( const Rocket::Core::String &shader ) {
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
		trap::R_RegisterPic( it->first.CString() );
	}
}

}
