#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

/*
    SiPlus : Nine-patch decorator usage in CSS:

            ninep-decorator: ninepatch;					<-- keyword

            ninep-src: /ui/porkui/gfx/controls/button	<-- texture

            ninep-coords-top: 0.125|4px;				<-- offset to the center part of texture
                        -right: 0.125;					    from the corresponding edge
                        -bottom: 0.0625;
                        -left: 0.25;
            ninep-coords: 0.125 0.25 0.9375 0.875;		<-- shortcut

            ninep-size-top: 4px|auto;					<-- size of the border on the element (> 0 - from edges, < 0 - from centre)
                      -right: 4px;
                      -bottom: 2px;
                      -left: 8px;
            ninep-size: 4px 4px 2px 8px;				<-- shortcut
*/

namespace WSWUI
{
using namespace Rml::Core;

class NinePatchDecorator : public Decorator
{
	int texture_index;
	Vector2f coords[2];
	bool coords_absolute[2][2];
	bool size_auto[2][2];
	PropertyDictionary properties;

public:
	NinePatchDecorator() : Decorator(), texture_index( -1 ) {}

	static float ResolveProperty( const PropertyDictionary &properties, const std::string &name, float base_value ) {
#if 0
		const Property *property = properties.GetProperty( name );
		if( property == NULL ) {
			ROCKET_ERROR;
			return 0;
		}

		// Need to include em!
		if( property->unit & Property::RELATIVE_UNIT )
			return base_value * property->value.Get<float>() * 0.01f;

		if( property->unit & Property::ABSOLUTE_UNIT )
			return property->value.Get<float>();

		// Values based on pixels-per-inch.
		if( property->unit & Property::PPI_UNIT ) {
			Rml::Core::RenderInterface *renderInterface = GetRenderInterface();
			float inch = property->value.Get<float>() * renderInterface->GetPixelsPerInch();

			if( property->unit & Property::INCH ) // inch
				return inch;
			if( property->unit & Property::CM ) // centimeter
				return inch * ( 1.0f / 2.54f );
			if( property->unit & Property::MM ) // millimeter
				return inch * ( 1.0f / 25.4f );
			if( property->unit & Property::PT ) // point
				return inch * ( 1.0f / 72.0f );
			if( property->unit & Property::PC ) // pica
				return inch * ( 1.0f / 6.0f );
		}

		ROCKET_ERROR;
#endif
		return 0;
	}

	bool Initialise( const PropertyDictionary &_properties ) {
#if 0
		const Property *property = _properties.GetProperty(  "src" );
		texture_index = LoadTexture( property->Get< std::string >(), property->source );
		if( texture_index < 0 ) {
			return false;
		}

		properties = _properties;

		property = properties.GetProperty( "coords-left" );
		coords[0].x = Math::Max( 0.0f, property->Get< float >() );
		coords_absolute[0][0] = ( property->unit == Property::PX );
		property = properties.GetProperty( "coords-top" );
		coords[0].y = Math::Max( 0.0f, property->Get< float >() );
		coords_absolute[0][1] = ( property->unit == Property::PX );
		property = properties.GetProperty( "coords-right" );
		coords[1].x = Math::Max( 0.0f, property->Get< float >() );
		coords_absolute[1][0] = ( property->unit == Property::PX );
		property = properties.GetProperty( "coords-bottom" );
		coords[1].y = Math::Max( 0.0f, property->Get< float >() );
		coords_absolute[1][1] = ( property->unit == Property::PX );

		property = properties.GetProperty( "size-left" );
		size_auto[0][0] = property->unit == Property::KEYWORD;
		property = properties.GetProperty( "size-top" );
		size_auto[0][1] = property->unit == Property::KEYWORD;
		property = properties.GetProperty( "size-right" );
		size_auto[1][0] = property->unit == Property::KEYWORD;
		property = properties.GetProperty( "size-bottom" );
		size_auto[1][1] = property->unit == Property::KEYWORD;
#endif
		return true;
	}

	virtual DecoratorDataHandle GenerateElementData( Element *element ) const override {
		int i, j;
		const Texture *texture = GetTexture( texture_index );

		Vector2f padded_size = element->GetBox().GetSize( Box::PADDING );

		RenderInterface *render_interface = element->GetRenderInterface();
		Vector2i texture_dimensions = texture->GetDimensions( render_interface );

		Vector2f tex_coords[2];
		tex_coords[0] = coords[0];
		tex_coords[1] = coords[1];
		if( coords_absolute[0][0] ) {
			tex_coords[0].x /= texture_dimensions.x;
		}
		if( coords_absolute[0][1] ) {
			tex_coords[0].y /= texture_dimensions.y;
		}
		if( coords_absolute[1][0] ) {
			tex_coords[1].x /= texture_dimensions.x;
		}
		if( coords_absolute[1][1] ) {
			tex_coords[1].y /= texture_dimensions.y;
		}

		Vector2f dimensions[2];
		if( size_auto[0][0] ) {
			dimensions[0].x = ( float )texture_dimensions.x * tex_coords[0].x;
		} else {
			dimensions[0].x = ResolveProperty( properties, "size-left", padded_size.x );
		}
		if( size_auto[0][1] ) {
			dimensions[0].y = ( float )texture_dimensions.y * tex_coords[0].y;
		} else {
			dimensions[0].y = ResolveProperty( properties, "size-top", padded_size.y );
		}
		if( size_auto[1][0] ) {
			dimensions[1].x = ( float )texture_dimensions.x * tex_coords[1].x;
		} else {
			dimensions[1].x = ResolveProperty( properties, "size-right", padded_size.x );
		}
		if( size_auto[1][1] ) {
			dimensions[1].y = ( float )texture_dimensions.y * tex_coords[1].y;
		} else {
			dimensions[1].y = ResolveProperty( properties, "size-bottom", padded_size.y );
		}

		// Negative sizes make the dimensions be calculated from the centre, not the edges.
		if( dimensions[0].x < 0.0f ) {
			dimensions[0].x = Math::Max( 0.0f, padded_size.x * 0.5f + dimensions[0].x );
		}
		if( dimensions[0].y < 0.0f ) {
			dimensions[0].y = Math::Max( 0.0f, padded_size.y * 0.5f + dimensions[0].y );
		}
		if( dimensions[1].x < 0.0f ) {
			dimensions[1].x = Math::Max( 0.0f, padded_size.x * 0.5f + dimensions[1].x );
		}
		if( dimensions[1].y < 0.0f ) {
			dimensions[1].y = Math::Max( 0.0f, padded_size.y * 0.5f + dimensions[1].y );
		}

		// Shrink the sizes if necessary.
		Vector2f total_dimensions = dimensions[0] + dimensions[1];
		if( padded_size.x < total_dimensions.x ) {
			dimensions[0].x = padded_size.x * ( dimensions[0].x / total_dimensions.x );
			dimensions[1].x = padded_size.x * ( dimensions[1].x / total_dimensions.x );
		}
		if( padded_size.y < total_dimensions.y ) {
			dimensions[0].y = padded_size.y * ( dimensions[0].y / total_dimensions.y );
			dimensions[1].y = padded_size.y * ( dimensions[1].y / total_dimensions.y );
		}

		Vector2f centre_dimensions = padded_size - dimensions[0] - dimensions[1];

		// Generate the vertices.
		Vertex vertices[16];
		int num_vertices = 4;
		int indices[54];
		int num_indices = 0;

		int edge_indices[8] = { 0, 0, 1, 1, 2, 2, 3, 3 };     // Indices of the vertices on edges.
		int centre_indices[4] = { 0, 1, 2, 3 };     // Indices of the edges of the centre.

		Colourb colour( 255, 255, 255 );

		// Generate the corners.
		vertices[0].position = Vector2f( 0.0f, 0.0f );
		vertices[0].tex_coord = tex_coords[0];
		vertices[1].position = Vector2f( padded_size.x, 0.0f );
		vertices[1].tex_coord = Vector2f( 1.0f - tex_coords[1].x, tex_coords[0].y );
		vertices[2].position = Vector2f( 0.0f, padded_size.y );
		vertices[2].tex_coord = Vector2f( tex_coords[0].x, 1.0f - tex_coords[1].y );
		vertices[3].position = Vector2f( padded_size.x, padded_size.y );
		vertices[3].tex_coord = Vector2f( 1.0f - tex_coords[1].x, 1.0f - tex_coords[1].y );

		// Generate the edge vertices.
		for( i = 0; i < 2; i++ ) {
			Vector2f position = ( i ? ( padded_size - dimensions[1] ) : dimensions[0] );
			Vector2f tex_coord = ( i ? ( Vector2f( 1.0f, 1.0f ) - tex_coords[1] ) : tex_coords[0] );
			if( dimensions[i].x > 0.0f ) {
				for( j = 0; j < 2; j++ )
					vertices[j * 2 + i].tex_coord.x = ( float )i;
				vertices[num_vertices].position = Vector2f( position.x, 0.0f );
				vertices[num_vertices].tex_coord = Vector2f( tex_coord.x, 0.0f );
				edge_indices[i * 2] = centre_indices[i] = num_vertices++;
				vertices[num_vertices].position = Vector2f( position.x, padded_size.y );
				vertices[num_vertices].tex_coord = Vector2f( tex_coord.x, 1.0f );
				edge_indices[i * 2 + 4] = centre_indices[i + 2] = num_vertices++;
			}
			if( dimensions[i].y > 0.0f ) {
				for( j = 0; j < 2; j++ )
					vertices[i * 2 + j].tex_coord.y = ( float )i;
				vertices[num_vertices].position = Vector2f( 0.0f, position.y );
				vertices[num_vertices].tex_coord = Vector2f( 0.0f, tex_coord.y );
				edge_indices[i * 4 + 1] = centre_indices[i * 2] = num_vertices++;
				vertices[num_vertices].position = Vector2f( padded_size.x, position.y );
				vertices[num_vertices].tex_coord = Vector2f( 1.0f, tex_coord.y );
				edge_indices[i * 4 + 3] = centre_indices[i * 2 + 1] = num_vertices++;
			}
		}

		// Generate the corners.
		for( i = 0; i < 4; i++ ) {
			if( ( dimensions[i & 1].x > 0.0f ) && ( dimensions[i >> 1].y > 0.0f ) ) {
				vertices[num_vertices].position = Vector2f(
					( i & 1 ) ? ( padded_size.x - dimensions[1].x ) : dimensions[0].x,
					( i >> 1 ) ? ( padded_size.y - dimensions[1].y ) : dimensions[0].y );
				vertices[num_vertices].tex_coord = Vector2f(
					( i & 1 ) ? ( 1.0f - tex_coords[1].x ) : tex_coords[0].x,
					( i >> 1 ) ? ( 1.0f - tex_coords[1].y ) : tex_coords[0].y );
				centre_indices[i] = num_vertices++;

				int flip = ( ( ( i & 1 ) ^ ( i >> 1 ) ) ? 1 : -1 );
				indices[num_indices + 1 + flip] = i;
				indices[num_indices + 1] = edge_indices[i * 2];
				indices[num_indices + 1 - flip] = centre_indices[i];
				indices[num_indices + 4 + flip] = i;
				indices[num_indices + 4] = centre_indices[i];
				indices[num_indices + 4 - flip] = edge_indices[i * 2 + 1];
				num_indices += 6;
			}
		}

		// Generate the edges.
		if( centre_dimensions.x > 0.0f ) {
			for( i = 0; i < 2; i++ ) {
				if( dimensions[i].y > 0.0f ) {
					int flip = i * 2;
					indices[num_indices + flip] = edge_indices[i * 4];
					indices[num_indices + 1] = edge_indices[i * 4 + 2];
					indices[num_indices + 2 - flip] = centre_indices[i * 2 + 1];
					indices[num_indices + 3 + flip] = edge_indices[i * 4];
					indices[num_indices + 4] = centre_indices[i * 2 + 1];
					indices[num_indices + 5 - flip] = centre_indices[i * 2];
					num_indices += 6;
				}
			}
		}
		if( centre_dimensions.y > 0.0f ) {
			for( i = 0; i < 2; i++ ) {
				if( dimensions[i].x > 0.0f ) {
					int flip = i * 2;
					indices[num_indices + flip] = edge_indices[i * 2 + 1];
					indices[num_indices + 1] = centre_indices[i];
					indices[num_indices + 2 - flip] = centre_indices[i + 2];
					indices[num_indices + 3 + flip] = edge_indices[i * 2 + 1];
					indices[num_indices + 4] = centre_indices[i + 2];
					indices[num_indices + 5 - flip] = edge_indices[i * 2 + 5];
					num_indices += 6;
				}
			}
		}

		// Generate the centre.
		if( ( centre_dimensions.x > 0.0f ) && ( centre_dimensions.y > 0.0f ) ) {
			indices[num_indices++] = centre_indices[0];
			indices[num_indices++] = centre_indices[1];
			indices[num_indices++] = centre_indices[3];
			indices[num_indices++] = centre_indices[0];
			indices[num_indices++] = centre_indices[3];
			indices[num_indices++] = centre_indices[2];
		}

		Geometry *data = __new__( Geometry )( element );
		data->SetTexture( texture );

		std::vector< Vertex > &data_vertices = data->GetVertices();
		int old_num_vertices = data_vertices.size();
		data_vertices.resize( old_num_vertices + num_vertices );
		for( i = 0; i < num_vertices; i++ ) {
			vertices[i].colour = colour;
			data_vertices[old_num_vertices + i] = vertices[i];
		}

		std::vector< int > &data_indices = data->GetIndices();
		int old_num_indices = data_indices.size();
		data_indices.resize( old_num_indices + num_indices );
		for( i = 0; i < num_indices; i++ )
			data_indices[old_num_indices + i] = old_num_vertices + indices[i];

		return reinterpret_cast< DecoratorDataHandle >( data );
	}

	virtual void ReleaseElementData( DecoratorDataHandle element_data ) const override {
		__delete__( reinterpret_cast< Geometry * >( element_data ) );
	}

	virtual void RenderElement( Element *element, DecoratorDataHandle element_data ) const override {
		reinterpret_cast< Geometry * >( element_data )->Render( element->GetAbsoluteOffset( Box::PADDING ) );
	}
};

//=======================================================

class NinePatchDecoratorInstancer : public DecoratorInstancer
{
public:
	NinePatchDecoratorInstancer( void ) {
#if 0
		RegisterProperty( "src", "" ).AddParser( "string" );

		RegisterProperty( "coords-top", "0" ).AddParser( "number" );
		RegisterProperty( "coords-left", "0" ).AddParser( "number" );
		RegisterProperty( "coords-bottom", "0" ).AddParser( "number" );
		RegisterProperty( "coords-right", "0" ).AddParser( "number" );
		RegisterShorthand( "coords", "coords-top, coords-right, coords-bottom, coords-left" );

		RegisterProperty( "size-top", "auto" )
		.AddParser( "keyword", "auto" )
		.AddParser( "number" );
		RegisterProperty( "size-left", "auto" )
		.AddParser( "keyword", "auto" )
		.AddParser( "number" );
		RegisterProperty( "size-bottom", "auto" )
		.AddParser( "keyword", "auto" )
		.AddParser( "number" );
		RegisterProperty( "size-right", "auto" )
		.AddParser( "keyword", "auto" )
		.AddParser( "number" );
		RegisterShorthand( "size", "size-top, size-right, size-bottom, size-left" );
#endif
	}

	std::shared_ptr<Rml::Core::Decorator> InstanceDecorator( const String &name, const PropertyDictionary &_properties, const Rml::Core::DecoratorInstancerInterface& interface ) {
		auto decorator = std::make_shared<NinePatchDecorator>();
		if( decorator->Initialise( _properties ) ) {
			return decorator;
		}

		return nullptr;
	}
};

DecoratorInstancer *GetNinePatchDecoratorInstancer( void ) {
	return __new__( NinePatchDecoratorInstancer );
}
}
