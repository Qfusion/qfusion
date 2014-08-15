#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

/*
	SiPlus : Nine-patch decorator usage in CSS:

			ninep-decorator: ninepatch;					<-- keyword

			ninep-image: /ui/porkui/gfx/controls/button	<-- texture

			ninep-coord-top: 0.125;						<-- offset to the center part of texture
			           -left: 0.25;						    from the corresponding edge
			           -bottom: 0.9375;
			           -right: 0.875;
			ninep-coord: 0.125 0.25 0.9375 0.875;		<-- shortcut
			           -top-left: 0.125 0.25;
			           -bottom-right: 0.9375 0.875;

			ninep-size-top: 4px;						<-- size of the border on the element
			          -left: 8px;
			          -bottom: 2px;
			          -right: 4px;
			ninep-size: 4px 8px 2px 4px;				<-- shortcut
			          -top-left: 4px 8px;
			          -bottom-right: 2px 4px;
*/

namespace WSWUI
{
	using namespace Rocket::Core;

	class NinePatchDecorator : public Decorator
	{
		int texture_index;
		Vector2f coord[2];
		PropertyDictionary properties;

	public:
		bool Initialise( const PropertyDictionary &properties )
		{
			const Property *texture_property = properties.GetProperty( "image" );
			texture_index = LoadTexture( texture_property->Get< String >(), texture_property->source );
			if( texture_index < 0 )
				return false;

			coord[0].x = Math::Max( 0.0f, properties.GetProperty( "coord-left" )->Get< float >() );
			coord[0].y = Math::Max( 0.0f, properties.GetProperty( "coord-top" )->Get< float >() );
			coord[1].x = Math::Max( 0.0f, properties.GetProperty( "coord-right" )->Get< float >() );
			coord[1].y = Math::Max( 0.0f, properties.GetProperty( "coord-bottom" )->Get< float >() );

			this->properties = properties;

			return true;
		}

		virtual DecoratorDataHandle GenerateElementData( Element *element )
		{
			int i;

			Vector2f padded_size = element->GetBox().GetSize( Box::PADDING );

			Vector2f dimensions[2];
			dimensions[0].x = Math::Max( 0.0f, ResolveProperty( properties, "size-left", padded_size.x ) );
			dimensions[0].y = Math::Max( 0.0f, ResolveProperty( properties, "size-top", padded_size.y ) );
			dimensions[1].x = Math::Max( 0.0f, ResolveProperty( properties, "size-right", padded_size.x ) );
			dimensions[1].y = Math::Max( 0.0f, ResolveProperty( properties, "size-bottom", padded_size.y ) );

			// Shrink the sizes if necessary.
			Vector2f total_dimensions = dimensions[0] + dimensions[1];
			if( padded_size.x < total_dimensions.x )
			{
				dimensions[0].x = padded_size.x * ( dimensions[0].x / total_dimensions.x );
				dimensions[1].x = padded_size.x * ( dimensions[1].x / total_dimensions.x );
			}
			if( padded_size.y < total_dimensions.y )
			{
				dimensions[0].y = padded_size.y * ( dimensions[0].y / total_dimensions.y );
				dimensions[1].y = padded_size.y * ( dimensions[1].y / total_dimensions.y );
			}

			Vector2f centre_dimensions = padded_size - dimensions[0] - dimensions[1];

			// Generate the vertices.
			Vertex vertices[16];
			int num_vertices = 4;
			int indices[54];
			int num_indices = 0;

			int edge_indices[8] = { 0, 0, 1, 1, 2, 2, 3, 3 }; // Indices of the vertices on edges.
			int centre_indices[4] = { 0, 1, 2, 3 }; // Indices of the edges of the centre.
			
			Colourb colour( 255, 255, 255 );

			// Generate the corners.
			vertices[0].position = Vector2f( 0.0f, 0.0f );
			vertices[0].tex_coord = Vector2f( 0.0f, 0.0f );
			vertices[0].colour = colour;
			vertices[1].position = Vector2f( padded_size.x, 0.0f );
			vertices[1].tex_coord = Vector2f( 1.0f, 0.0f );
			vertices[1].colour = colour;
			vertices[2].position = Vector2f( 0.0f, padded_size.y );
			vertices[2].tex_coord = Vector2f( 0.0f, 1.0f );
			vertices[2].colour = colour;
			vertices[3].position = Vector2f( padded_size.x, padded_size.y );
			vertices[3].tex_coord = Vector2f( 1.0f, 1.0f );
			vertices[3].colour = colour;

			// Generate the edge vertices.
			for( i = 0; i < 2; i++ )
			{
				Vector2f position = ( i ? ( padded_size - dimensions[1] ) : dimensions[0] );
				Vector2f tex_coord = ( i ? ( Vector2f( 1.0f, 1.0f ) - coord[1] ) : coord[0] );
				if( dimensions[i].x > 0.0f )
				{
					vertices[num_vertices].position = Vector2f( position.x, 0.0f );
					vertices[num_vertices].tex_coord = Vector2f( tex_coord.x, 0.0f );
					vertices[num_vertices].colour = colour;
					edge_indices[i * 2] = centre_indices[i] = num_vertices++;
					vertices[num_vertices].position = Vector2f( position.x, padded_size.y );
					vertices[num_vertices].tex_coord = Vector2f( tex_coord.x, 1.0f );
					vertices[num_vertices].colour = colour;
					edge_indices[i * 2 + 4] = centre_indices[i + 2] = num_vertices++;
				}
				if( dimensions[i].y > 0.0f )
				{
					vertices[num_vertices].position = Vector2f( 0.0f, position.y );
					vertices[num_vertices].tex_coord = Vector2f( 0.0f, tex_coord.y );
					vertices[num_vertices].colour = colour;
					edge_indices[i * 4 + 1] = centre_indices[i * 2] = num_vertices++;
					vertices[num_vertices].position = Vector2f( padded_size.x, position.y );
					vertices[num_vertices].tex_coord = Vector2f( 1.0f, tex_coord.y );
					vertices[num_vertices].colour = colour;
					edge_indices[i * 4 + 3] = centre_indices[i * 2 + 1] = num_vertices++;
				}
			}

			// Generate the corners.
			for( i = 0; i < 4; i++ )
			{
				if( ( dimensions[i & 1].x > 0.0f ) && ( dimensions[i >> 1].y > 0.0f ) )
				{
					vertices[num_vertices].position = Vector2f(
						( i & 1 ) ? ( padded_size.x - dimensions[1].x ) : dimensions[0].x,
						( i >> 1 ) ? ( padded_size.y - dimensions[1].y ) : dimensions[0].y );
					vertices[num_vertices].tex_coord = Vector2f(
						( i & 1 ) ? ( 1.0f - coord[1].x ) : coord[0].x,
						( i >> 1 ) ? ( 1.0f - coord[1].y ) : coord[0].y );
					vertices[num_vertices].colour = colour;
					centre_indices[i] = num_vertices++;

					indices[num_indices++] = i;
					indices[num_indices++] = edge_indices[i * 2];
					indices[num_indices++] = centre_indices[i];
					indices[num_indices++] = i;
					indices[num_indices++] = centre_indices[i];
					indices[num_indices++] = edge_indices[i * 2 + 1];
				}
			}

			// Generate the edges.
			if( centre_dimensions.x > 0.0f )
			{
				for( i = 0; i < 2; i++ )
				{
					if( dimensions[i].y > 0.0f )
					{
						indices[num_indices++] = edge_indices[i * 4];
						indices[num_indices++] = edge_indices[i * 4 + 2];
						indices[num_indices++] = centre_indices[i * 2 + 1];
						indices[num_indices++] = edge_indices[i * 4];
						indices[num_indices++] = centre_indices[i * 2 + 1];
						indices[num_indices++] = centre_indices[i * 2];
					}
				}
			}
			if( centre_dimensions.y > 0.0f )
			{
				for( i = 0; i < 2; i++ )
				{
					if( dimensions[i].x > 0.0f )
					{
						indices[num_indices++] = edge_indices[i * 2 + 1];
						indices[num_indices++] = centre_indices[i];
						indices[num_indices++] = centre_indices[i + 2];
						indices[num_indices++] = edge_indices[i * 2 + 1];
						indices[num_indices++] = centre_indices[i + 2];
						indices[num_indices++] = edge_indices[i * 2 + 5];
					}
				}
			}

			// Generate the centre.
			if( ( centre_dimensions.x > 0.0f ) && ( centre_dimensions.y > 0.0f ) )
			{
				indices[num_indices++] = centre_indices[0];
				indices[num_indices++] = centre_indices[1];
				indices[num_indices++] = centre_indices[3];
				indices[num_indices++] = centre_indices[0];
				indices[num_indices++] = centre_indices[3];
				indices[num_indices++] = centre_indices[2];
			}

			Geometry *data = __new__( Geometry )( element );
			data->SetTexture( GetTexture( texture_index ) );

			std::vector< Vertex > &data_vertices = data->GetVertices();
			int old_num_vertices = data_vertices.size();
			data_vertices.resize( old_num_vertices + num_vertices );
			memcpy( &data_vertices[old_num_vertices], vertices, sizeof( Vertex ) * num_vertices );

			std::vector< int > &data_indices = data->GetIndices();
			int old_num_indices = data_indices.size();
			data_indices.resize( old_num_indices + num_indices );
			for( i = 0; i < num_indices; i++ )
				data_indices[old_num_indices + i] = old_num_vertices + indices[i];

			return reinterpret_cast< DecoratorDataHandle >( data );
		}

		virtual void ReleaseElementData( DecoratorDataHandle element_data )
		{
			delete reinterpret_cast< Geometry * >( element_data );
		}

		virtual void RenderElement( Element *element, DecoratorDataHandle element_data )
		{
			reinterpret_cast< Geometry * >( element_data )->Render( element->GetAbsoluteOffset( Box::PADDING ) );
		}
	};

	//=======================================================

	class NinePatchDecoratorInstancer : public DecoratorInstancer
	{
	public:
		NinePatchDecoratorInstancer()
		{
			RegisterProperty( "image", "" ).AddParser( "string" );

			RegisterProperty( "coord-top", "0" ).AddParser( "number" );
			RegisterProperty( "coord-left", "0" ).AddParser( "number" );
			RegisterShorthand( "coord-top-left", "coord-top, coord-left" );
			RegisterProperty( "coord-bottom", "0" ).AddParser( "number" );
			RegisterProperty( "coord-right", "0" ).AddParser( "number" );
			RegisterShorthand( "coord-bottom-right", "coord-bottom, coord-right" );
			RegisterShorthand( "coord", "coord-top, coord-left, coord-bottom, coord-right" );

			RegisterProperty( "size-top", "0" ).AddParser( "number" );
			RegisterProperty( "size-left", "0" ).AddParser( "number" );
			RegisterShorthand( "size-top-left", "size-top, size-left" );
			RegisterProperty( "size-bottom", "0" ).AddParser( "number" );
			RegisterProperty( "size-right", "0" ).AddParser( "number" );
			RegisterShorthand( "size-bottom-right", "size-bottom, size-right" );
			RegisterShorthand( "size", "size-top, size-left, size-bottom, size-right" );
		}

		virtual Decorator *InstanceDecorator(const String &name, const PropertyDictionary &properties)
		{
			NinePatchDecorator *decorator = __new__( NinePatchDecorator );
			if( decorator->Initialise( properties ) )
				return decorator;

			decorator->RemoveReference();
			ReleaseDecorator( decorator );
			return NULL;
		}

		virtual void ReleaseDecorator( Decorator *decorator )
		{
			__delete__( decorator );
		}

		virtual void Release()
		{
			__delete__( this );
		}
	};

	DecoratorInstancer *GetNinePatchDecoratorInstancer( void )
	{
		return __new__( NinePatchDecoratorInstancer );
	}
}
