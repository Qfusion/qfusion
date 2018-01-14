/*
Copyright (C) 2018 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "ui_svg_tesselator.h"
#include "ui_svg_rasterizer.h"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

namespace WSWUI
{
using namespace Rocket::Core;

class SVGDecorator : public Decorator
{
	String path;
	bool tesselate;
	NSVGimage *image;
	SVGTesselator *tesselator;
	SVGRasterizer *rasterizer;

public:
	SVGDecorator() : 
		Decorator(), tesselate( false ), image( NULL ), tesselator( NULL ), rasterizer( NULL ) {
	}

	~SVGDecorator() {
		if( image != NULL ) {
			nsvgDelete( image );
		}
	}

	bool Initialise( const PropertyDictionary &_properties ) {
		char *buffer = NULL;
		size_t buffer_size = 0;

		const Property *property = _properties.GetProperty( "src" );
		if( !property ) {
			return false;
		}

		const String src = property->Get< String >();
		const String &directory = property->source;

		if( src.Substring(0, 1) == "?" ) {
			path = src;
		} else {
			GetSystemInterface()->JoinPath( path, directory.Replace( "|", ":" ), src );
		}

		FileHandle handle = GetFileInterface()->Open( path.CString() );
		if( handle ) {
			size_t length = GetFileInterface()->Length( handle );
			if( length > 0 ) {
				buffer_size = length + 1;
				buffer = new char[buffer_size];

				// read the whole file contents
				GetFileInterface()->Read( buffer, length, handle );
				buffer[length] = '\0';
			}

			GetFileInterface()->Close( handle );
		}

		if( !buffer ) {
			return false;
		}

		image = nsvgParse( buffer, "px", GetRenderInterface()->GetPixelsPerInch() );
		delete[] buffer;

		if( image == NULL ) {
			return false;
		}

		if( image->width <= 0 || image->height <= 0 ) {
			nsvgDelete( image );
			image = NULL;
			return false;
		}

		property = _properties.GetProperty( "tesselate" );
		tesselate = property && property->Get< int >() != 0;

		tesselator = SVGTesselator::GetInstance();
		tesselator->Initialise();
		
		rasterizer = SVGRasterizer::GetInstance();
		rasterizer->Initialise();

		return true;
	}

	virtual DecoratorDataHandle GenerateElementData( Element *element ) {
		Geometry *geom;

		const Property* width_property, *height_property;
		element->GetDimensionProperties( &width_property, &height_property );

		float scale_x = 1.0;
		float scale_y = 1.0;
		bool auto_width = width_property->unit == Property::KEYWORD;
		bool auto_height = height_property->unit == Property::KEYWORD;

		Vector2f size = element->GetBox().GetSize( Box::CONTENT );
		scale_x = size[0] / image->width;
		scale_y = size[1] / image->height;

		if( auto_width ^ auto_height ) {
			if( auto_width ) {
				scale_x = scale_y;
			}
			if( auto_height ) {
				scale_y = scale_x;
			}
		} else {
			if( auto_width ) {
				scale_x = 1.0;
				scale_y = 1.0;
			}
		}

		size.x = (int)(image->width * scale_x + 0.5f);
		size.y = (int)(image->height * scale_y + 0.5f);
		int width = size.x;
		int height = size.y;

		if( tesselate ) {
			geom = tesselator->Tesselate( image, width, height );
			return reinterpret_cast< DecoratorDataHandle >( geom );
		}

		String rasterPath( path );
		rasterizer->Rasterize( rasterPath, image, std::min( scale_x, scale_y ), width, height );

		int textureIndex = LoadTexture( rasterPath, "" );

		geom = __new__( Geometry )();
		geom->SetTexture( GetTexture( textureIndex ) );

		std::vector< Vertex > &geom_vertices = geom->GetVertices();
		geom_vertices.resize( 4 );

		std::vector< int > &geom_indices = geom->GetIndices();
		geom_indices.resize( 6 );

		GeometryUtilities::GenerateQuad( &geom_vertices[0], &geom_indices[0], Rocket::Core::Vector2f( 0, 0 ), 
			size, Colourb( 255, 255, 255, 255 ), 0 );

		return reinterpret_cast< DecoratorDataHandle >( geom );
	}

	virtual void ReleaseElementData( DecoratorDataHandle element_data ) {
		__delete__( reinterpret_cast< Geometry * >( element_data ) );
	}

	virtual void RenderElement( Element *element, DecoratorDataHandle element_data ) {
		reinterpret_cast< Geometry * >( element_data )->Render( element->GetAbsoluteOffset( Box::PADDING ) );
	}
};

//=======================================================

class SVGDecoratorInstancer : public DecoratorInstancer
{
public:
	SVGDecoratorInstancer( void ) {
		RegisterProperty( "src", "" ).AddParser( "string" );
	}

	virtual Decorator *InstanceDecorator( const String &name, const PropertyDictionary &_properties ) {
		SVGDecorator *decorator = __new__( SVGDecorator );
		if( decorator->Initialise( _properties ) ) {
			return decorator;
		}

		decorator->RemoveReference();
		ReleaseDecorator( decorator );
		return NULL;
	}

	virtual void ReleaseDecorator( Decorator *decorator ) {
		__delete__( decorator );
	}

	virtual void Release( void ) {
		__delete__( this );
	}
};

DecoratorInstancer *GetSVGDecoratorInstancer( void ) {
	return __new__( SVGDecoratorInstancer );
}
}
